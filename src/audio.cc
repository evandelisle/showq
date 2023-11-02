/*
 * Show Q
 * Copyright (c) 2007-2008 Errol van de l'Isle
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

#include "audio.h"

#include <glib/gi18n.h>
#include <glibmm.h>

#include <jack/jack.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <vector>

const int sample_size = sizeof(jack_default_audio_sample_t);

static int sample_rate;

AudioFile::AudioFile(const char *f)
    : fades(new Fades),
      SRC_state(nullptr),
      SRC_input_buffer(nullptr),
      SRC_input_buffer_size(0),
      input_buffer(nullptr),
      input_buffer_size(0)
{
    sfinfo.format = 0;
    codec = NoCodec;

    sf = sf_open(f, SFM_READ, &sfinfo);
    if (sf) {
        codec = SndFile;
        num_channels = sfinfo.channels;
        mTotalTime = double(sfinfo.frames) / sfinfo.samplerate;
    }

    if (codec == NoCodec) {
        eof = true;
        eob = true;
        srate = 0;
        num_channels = 0;
        return;
    }
    eof = false;
    eob = false;
    srate = sfinfo.samplerate;

    // The ring buffer is filled every 100ms by the disc thread. The size needs
    // to be able to store more that this to avoid it becoming empty.
    for (int i = 0; i < num_channels; ++i) {
        rbs.push_back(jack_ringbuffer_create(sample_size * sample_rate / 6));
    }

    if (srate != sample_rate) {
        int error;
        SRC_state =
            src_callback_new(src_callback, SRC_SINC_MEDIUM_QUALITY, num_channels, &error, this);
        if (!SRC_state)
            std::cerr << "Error opening resampler\n";
    }

    if (SRC_state) {
        SRC_input_buffer = new float[4096];
        SRC_input_buffer_size = 4096;
    }
    input_buffer_size = 4096;
    input_buffer = new float[input_buffer_size * 2];
}

AudioFile::~AudioFile()
{
    for (auto &rb : rbs)
        if (rb)
            jack_ringbuffer_free(rb);

    if (SRC_state)
        src_delete(SRC_state);

    if (codec == SndFile && sf)
        sf_close(sf);

    delete[] SRC_input_buffer;
    delete[] input_buffer;
}

long AudioFile::src_callback(void *cb_data, float **data)
{
    auto *af = static_cast<AudioFile *>(cb_data);
    *data = af->SRC_input_buffer;

    return sf_readf_float(
        af->sf, af->SRC_input_buffer, af->SRC_input_buffer_size / af->num_channels);
}

size_t AudioFile::read_cb()
{
    if (do_seek) {
        switch (codec) {
        case SndFile:
            sf_seek(sf, static_cast<long>(seek_pos * sfinfo.samplerate), SEEK_SET);
            break;
        default:
            break;
        }
        read_frame = size_t(seek_pos * srate);
        do_seek = false;
        eof = false;

        const std::lock_guard<std::mutex> lock(buffer_lock);
        for (auto &rb : rbs)
            jack_ringbuffer_reset(rb);
        cur_frame = size_t(seek_pos * sample_rate);
    }

    if (eof)
        return 0;
    size_t n = jack_ringbuffer_write_space(rbs[0]) / sample_size;
    const double factor = double(sample_rate) / double(srate);

    const size_t max_buffer_frames = input_buffer_size / num_channels;

    while (!eof && n > 0) {
        const int read_length = std::min(n, max_buffer_frames);
        sf_count_t frames_read;

        if (SRC_state) {
            frames_read = src_callback_read(SRC_state, factor, read_length, input_buffer);
        } else {
            frames_read = sf_readf_float(sf, input_buffer, read_length);
        }
        n -= frames_read;
        if (frames_read == 0) {
            eof = true;
            break;
        }
        for (int channel = 0; channel < num_channels; ++channel) {
            for (sf_count_t i = 0, j = channel; i < frames_read; ++i, j += num_channels) {
                input_buffer[i + input_buffer_size] = input_buffer[j];
            }
            jack_ringbuffer_write(rbs[channel],
                                  (char *)(&input_buffer[input_buffer_size]),
                                  frames_read * sample_size);
        }
    }

    return 1;
}

std::string AudioFile::get_info_str()
{
    SF_FORMAT_INFO format_info;
    switch (codec) {
    case SndFile:
        format_info.format = sfinfo.format;
        sf_command(sf, SFC_GET_FORMAT_INFO, &format_info, sizeof(format_info));

        return Glib::ustring::compose(
            _("%1 %2 channels, sample rate %3"), format_info.name, num_channels, srate);
    default:
        return "";
    }
}

double AudioFile::total_time()
{
    return mTotalTime;
}

void AudioFile::stop()
{
    status = Done;
    const std::shared_ptr<AudioFile::Fades> fad = fades.reader();

    for (auto &f : *fad) {
        AudioFile::fade_ *fa = f.get();
        fa->nframes = 0;
    }
}

void AudioFile::seek(double x)
{
    seek_pos = x;
    do_seek = true;
    audio->do_disc_thread();
}

double AudioFile::get_pos()
{
    return double(cur_frame) / sample_rate;
}

void AudioFile::add_fade(std::shared_ptr<fade_> f)
{
    RCUWriter<Fades> writer(fades);
    const std::shared_ptr<Fades> p = writer.get_copy();
    auto i = p->begin();
    while (i != p->end()) {
        if ((*i)->nframes <= 0)
            i = p->erase(i);
        else
            ++i;
    }

    p->push_front(f);
}

Audio::Audio()
    : afs(new Afs)
{
    client = jack_client_open("ShowQ", JackUseExactName, nullptr);

    if (!client)
        throw NoAudio();

    for (int i = 0; i < 8; ++i) {
        ports[i] = jack_port_register(client,
                                      Glib::ustring::compose("out_%1", i + 1).c_str(),
                                      JACK_DEFAULT_AUDIO_TYPE,
                                      JackPortIsOutput | JackPortIsTerminal,
                                      0);
    }

    jack_set_process_callback(client, audio_callback, this);
    jack_on_shutdown(client, sdown_callback, this);
    jack_set_sample_rate_callback(client, srate_callback, this);

    running = true;
    disc_thread_p = std::thread(&Audio::disc_thread, this);

    jack_activate(client);

    setup_ports();
}

Audio::~Audio()
{
    jack_deactivate(client);
    jack_client_close(client);

    running = false;
    disc_thread_p.join();
}

void Audio::setup_ports()
{
    const char **o_ports =
        jack_get_ports(client, nullptr, JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput);

    for (int i = 0; i < 8 && o_ports[i]; ++i) {
        if (ports[i]) {
            std::cerr << jack_port_name(ports[i]) << " # " << o_ports[i] << '\n';
            jack_connect(client, jack_port_name(ports[i]), o_ports[i]);
        }
    }

    jack_free(o_ports);
}

void Audio::add_af(std::shared_ptr<AudioFile> a)
{
    RCUWriter<Afs> writer(afs);
    const std::shared_ptr<Afs> p = writer.get_copy();

    for (auto i = p->begin(); i != p->end();) {
        if ((*i)->status == Done) {
            i = p->erase(i);
        } else
            ++i;
    }

    if (a->get_codec() != NoCodec)
        p->push_front(a);
    else
        a->status = Done;
}

void Audio::do_disc_thread()
{
    if (disc_thread_lock.try_lock()) { // Kick the disc thread
        disc_thread_cond.notify_all();
        disc_thread_lock.unlock();
    }
}

// Note jack_port_set_name is deprecated, but the new solution is not
// yet common enough to use
int Audio::port_set_name(int port, const Glib::ustring &name)
{
    if (!ports[port])
        return -1;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    return jack_port_set_name(ports[port], name.c_str());
#pragma GCC diagnostic pop
}

void Audio::disconnect_all()
{
    for (auto &port : ports) {
        if (port) {
            jack_port_disconnect(client, port);
        }
    }
}

int Audio::connect(int port, const Glib::ustring &name)
{
    if (!ports[port])
        return -1;
    return jack_connect(client, jack_port_name(ports[port]), name.c_str());
}

float Audio::get_cpu_load()
{
    return jack_cpu_load(client);
}

long Audio::get_sample_rate()
{
    return sample_rate;
}

void Audio::disc_thread()
{
    using namespace std::chrono_literals;

    std::unique_lock<std::mutex> lock(disc_thread_lock);

    while (running) {
        {
            const std::shared_ptr<Afs> pr = afs.reader();

            for (const auto &l : *pr) {
                if (l->status != Done)
                    l->read_cb();
            }
        }
        disc_thread_cond.wait_for(lock, 100ms);
    }
}

void Audio::sdown_callback(void *arg) noexcept
{
    static_cast<Audio *>(arg)->signal_jack_disconnect.emit();
}

int Audio::srate_callback(jack_nframes_t nframes, void *arg) noexcept
{
    static_cast<Audio *>(arg)->m_samplerate = nframes;
    sample_rate = nframes;
    return 0;
}

int Audio::audio_callback(jack_nframes_t nframes, void *arg) noexcept
{
    return static_cast<Audio *>(arg)->audio_callback0(nframes);
}

int Audio::audio_callback0(jack_nframes_t nframes) noexcept
{
    jack_default_audio_sample_t *buffers[8];
    float deltas[8];
    long fade_len[8];

    // Get and clear output buffers
    std::fill_n(buffers, 8, nullptr);
    for (int i = 0; i < 8; ++i) {
        if (ports[i]) {
            buffers[i] = (jack_default_audio_sample_t *)jack_port_get_buffer(ports[i], nframes);
            std::fill_n(buffers[i], nframes, 0.0);
        }
    }

    const std::shared_ptr<Afs> pr = afs.reader();
    auto i = pr->begin();

    for (; i != pr->end(); ++i) {
        AudioFile *l = i->get();

        if (l->status != Play || l->eob)
            continue;

        const std::unique_lock<std::mutex> lock(l->buffer_lock, std::try_to_lock);

        if (!lock.owns_lock())
            continue;

        size_t n = jack_ringbuffer_read_space(l->rbs[l->rbs.size() - 1]) / sample_size;
        if (n == 0) {
            if (l->eof) {
                l->eob = true;
                l->stop();
            }
            continue;
        }

        if (n > nframes)
            n = nframes;

        std::fill_n(deltas, 8, 0.0);
        std::fill_n(fade_len, 8, 0);

        const std::shared_ptr<AudioFile::Fades> fades = l->fades.reader();

        for (auto &f : *fades) {
            AudioFile::fade_ *fa = f.get();
            if (fa->status != Play)
                continue;
            if (fa->nframes <= 0)
                fa->nframes = 1;
            for (size_t j = 0; j < fa->on.size(); ++j) {
                if (fa->on[j]) {
                    deltas[j] = (l->vol[j] - fa->vol[j]) / fa->nframes;
                    fade_len[j] = (unsigned int)fa->nframes > n ? n : fa->nframes;
                }
            }
            fa->nframes -= n;
            if (fa->nframes <= 0) {
                fa->status = Done;
                if (fa->stop_on_complete)
                    l->stop();
                if (fa->pause_on_complete)
                    l->pause();
            }
        }

        for (size_t p = 0; p < l->patch.size(); ++p) {
            if (l->patch[p].src >= l->rbs.size())
                continue;

            jack_ringbuffer_data_t rdata[2];
            jack_ringbuffer_get_read_vector(l->rbs[l->patch[p].src], rdata);

            auto *s1 = (jack_default_audio_sample_t *)(rdata[0].buf);
            auto *s2 = (jack_default_audio_sample_t *)(rdata[1].buf);
            jack_default_audio_sample_t *dest = buffers[l->patch[p].dest];
            if (!dest)
                continue;

            float vol = l->vol[l->patch[p].dest];
            const float delta = deltas[l->patch[p].dest];
            size_t len1 = rdata[0].len / sample_size;
            size_t len2 = rdata[1].len / sample_size;
            size_t ftgo = fade_len[l->patch[p].dest];
            size_t ntgo = n - ftgo;

            for (; ftgo && len1; --ftgo, --len1) {
                *dest++ += *s1++ * vol;
                vol -= delta;
            }
            for (; ftgo && len2; --ftgo, --len2) {
                *dest++ += *s2++ * vol;
                vol -= delta;
            }
            for (; ntgo && len1; --ntgo, --len1) {
                *dest++ += *s1++ * vol;
            }
            for (; ntgo && len2; --ntgo, --len2) {
                *dest++ += *s2++ * vol;
            }
        }

        for (int j = 0; j < 8; ++j) {
            l->vol[j] -= deltas[j] * fade_len[j];
        }
        for (auto &rb : l->rbs)
            jack_ringbuffer_read_advance(rb, n * sample_size);
        l->cur_frame += nframes;
    }

    return 0;
}

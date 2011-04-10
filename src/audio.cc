/*
 * Show Q
 * Copyright (c) 2007-2008 Errol van-de-l'Isle
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

#include <pthread.h>
#include "audio.h"

#include <sstream>
#include <iostream>
#include <vector>

#include <cmath>
#include <cstdio>

const int samplesize = sizeof(jack_default_audio_sample_t);

static jack_default_audio_sample_t cb_rbuf[32000];
static jack_default_audio_sample_t cb_buf[32000];
static jack_default_audio_sample_t cb_rsbuf[32000 * 12];

static int samplerate;

AudioFile::AudioFile(const char *f)
    : status(Pause), eof(false), eob(false), fades(new Fades),
    cur_frame(0), srate(0), do_seek(false), read_frame(0), num_channels(0)
{
    sfinfo.format = 0;
    codec = NoCodec;
    FILE *fp;

    if ((sf = sf_open(f, SFM_READ, &sfinfo))) {
	codec = SndFile;
	num_channels = sfinfo.channels;
	mTotalTime = double(sfinfo.frames) / sfinfo.samplerate;
    } else if ((fp = fopen(f, "rb"))) {
	if (ov_open(fp, &vf, 0, 0))
	    fclose(fp);
	else {
	    codec = OggVorbis;
	    vi = ov_info(&vf, -1);
	    num_channels = vi->channels;
	    mTotalTime =  ov_time_total(&vf, -1);
	}
    }

    if (codec == NoCodec) {
	eof = true;
	eob = true;
	return;
    }
    srate = (codec == OggVorbis) ? vi->rate : sfinfo.samplerate;

    for (int i = 0; i < num_channels; ++i)
	rbs.push_back(jack_ringbuffer_create(samplesize * 192000));

    if (srate != samplerate) {
	for (int i = 0; i < num_channels; ++i) {
	    int error;
	    SRC_STATE *p = src_new(2, 1, &error);
	    if (!p)
		std::cerr << "Error opening resampler\n";
	    src.push_back(p);
	}
    }
}

AudioFile::~AudioFile()
{
    size_t i;
    for (i = 0; i < rbs.size(); ++i)
	if (rbs[i])
	    jack_ringbuffer_free(rbs[i]);

    for (i = 0; i < src.size(); ++i)
	if (src[i])
	    src_delete(src[i]);

    if (codec == SndFile && sf) sf_close(sf);
    if (codec == OggVorbis) ov_clear(&vf);
}

size_t AudioFile::sf_read(float *** vbuf, size_t n)
{
    static float * ibuf[16];
    size_t i;
    if (n > (32000 / rbs.size())) i = (32000/rbs.size()); else i = n;

    i = sf_readf_float(sf, cb_rbuf, i);

    for (size_t c = 0; c < rbs.size(); ++c) {
	ibuf[c] = &cb_buf[(32000 / rbs.size()) * c];
    }

    float *s, *d;

    for (size_t c = 0; c < rbs.size(); ++c) {
	d = ibuf[c];
	s = &cb_rbuf[c];
	for (size_t j = 0; j < i; ++j) {
	    *d++ = *s;
	    s += rbs.size();
	}
    }

    *vbuf = ibuf;
    return i;
}

size_t AudioFile::read_cb()
{
    if (do_seek) {
	switch (codec) {
	case SndFile:
	    sf_seek(sf, static_cast<long>(seek_pos * sfinfo.samplerate), SEEK_SET);
	    break;
	case OggVorbis:
	    ov_time_seek(&vf, seek_pos);
	    break;
	default:
	    break;
	}
	read_frame = size_t(seek_pos * srate);
	do_seek = false;
	eof = false;

	Glib::Mutex::Lock lock(buffer_lock);
	for (size_t i = 0; i < rbs.size(); ++i)
	    jack_ringbuffer_reset(rbs[i]);
	cur_frame = size_t(seek_pos * samplerate);
    }

    if (eof) return 0;
    size_t n = jack_ringbuffer_write_space(rbs[0]) / samplesize;
    double factor = double(samplerate) / double(srate);
    float **vbuf;

    while (!eof && n > 100) {
	long j = 0;
	long want_read = long(floor(double(n) / factor));
	if (codec == OggVorbis && (j = ov_read_float(&vf, &vbuf, want_read, 0)) <= 0) {
	    eof = true;
	    break;
	}
	if (codec == SndFile && (j = sf_read(&vbuf, want_read)) == 0) {
	    eof = true;
	    break;
	}

	read_frame += j;

	if (srate == samplerate) {
	    for (size_t c = 0; c < rbs.size(); ++c)
		jack_ringbuffer_write(rbs[c], (const char *) vbuf[c], j * samplesize);
	    n -= j;
	} else {
	    long d = 0;
	    for (size_t c = 0; c < rbs.size(); ++c) {
		SRC_DATA s;
		s.data_in = vbuf[c];
		s.input_frames = j;
		s.data_out = cb_rsbuf;
		s.output_frames = 32000 * 12 * 2;
		s.src_ratio = factor;
		s.end_of_input = 0;

		src_process(src[c], &s);
		jack_ringbuffer_write(rbs[c], (const char *) cb_rsbuf, s.output_frames_gen * samplesize);
		d = s.output_frames_gen;
	    }
	    n -= d;
	}
    }

    return (1);
}

std::string AudioFile::get_info_str()
{
    std::ostringstream s;
    SF_FORMAT_INFO format_info;
    switch (codec) {
    case SndFile:
	format_info.format = sfinfo.format;
	sf_command(sf, SFC_GET_FORMAT_INFO, &format_info, sizeof(format_info));
	s << format_info.name << " " << num_channels << " channels, samplerate "  << srate;
	return s.str();
    case OggVorbis:
	s << "OggVorbis " << num_channels << " channels, samplerate " << srate ;
	return s.str();
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
    boost::shared_ptr<AudioFile::Fades> fad = fades.reader();
    AudioFile::Fades::const_iterator f = fad->begin();
    for ( ; f != fad->end(); ++f) {
	AudioFile::fade_ * fa = f->get();
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
    return double(cur_frame)/samplerate;

    size_t offset = jack_ringbuffer_read_space(rbs[0]) / samplesize;
    double factor = double(samplerate) / double(srate);
    offset = size_t(offset / factor);

    return double(read_frame - offset) / srate;
}

void AudioFile::add_fade(boost::shared_ptr<fade_> f)
{
    RCUWriter<Fades> writer(fades);
    boost::shared_ptr<Fades> p = writer.get_copy();
    Fades::iterator i = p->begin();
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
    client = jack_client_open("ShowQ", JackUseExactName, 0);

    if (!client) throw NoAudio();

    for (int i = 0; i < 8; ++i) {
	std::ostringstream s;
	s << "out_" << i + 1;
	ports[i] = jack_port_register(client, s.str().c_str(),
	    JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput | JackPortIsTerminal, 0);
    }

    jack_set_process_callback(client, audio_callback, this);
    jack_on_shutdown(client, sdown_callback, this);
    jack_set_sample_rate_callback(client, srate_callback, this);

    g_atomic_int_set(&running, 1);
    disc_thread_p = Glib::Thread::create(sigc::mem_fun(*this, &Audio::disc_thread), true);

    jack_activate(client);

    setup_ports();
}

Audio::~Audio()
{
    jack_deactivate(client);
    jack_client_close(client);

    g_atomic_int_set(&running, 0);
    disc_thread_p->join();
}

void Audio::setup_ports()
{
    const char **oports
	= jack_get_ports(client, 0, 0, JackPortIsInput);

    for (int i = 0; oports[i] && i < 8; ++i) {
	std::cerr << jack_port_name(ports[i]) << " # " << oports[i] << '\n';
	jack_connect(client, jack_port_name(ports[i]), oports[i]);
    }

    free(oports);
}

void Audio::add_af(boost::shared_ptr<AudioFile> a)
{
    {
	RCUWriter<Afs> writer(afs);
	boost::shared_ptr<Afs> p = writer.get_copy();
	Afs::iterator i = p->begin();
	for ( ; i != p->end();) {
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
    do_disc_thread();
}

void Audio::do_disc_thread()
{
    if (disc_thread_lock.trylock()) {	// Kick the disc thread
	disc_thread_cond.signal();
	disc_thread_lock.unlock();
    }
}

int Audio::port_set_name(int port, const Glib::ustring & name)
{
    if (!ports[port]) return -1;
    return jack_port_set_name(ports[port], name.c_str());
}

void Audio::disconnect_all()
{
    for (int i = 0; i < 8; ++i) {
	jack_port_disconnect(client, ports[i]);
    }
}

int Audio::connect(int port, const Glib::ustring & name)
{
    if (!ports[port]) return -1;
    return jack_connect(client, jack_port_name(ports[port]), name.c_str());
}

float Audio::get_cpu_load()
{
    return jack_cpu_load(client);
}

long Audio::get_sample_rate()
{
    return samplerate;
}

void Audio::disc_thread()
{
    Glib::Mutex::Lock lock(disc_thread_lock);
    Glib::TimeVal t;

    while (g_atomic_int_get(&running)) {
	t.assign_current_time();
	t.add_milliseconds(500);

	{
	    boost::shared_ptr<Afs> pr = afs.reader();
	    Afs::iterator i = pr->begin();
	    for ( ; i != pr->end(); ++i) {
		boost::shared_ptr<AudioFile> l = *i;
		if (l->status != Done) l->read_cb();
	    }
	}
	disc_thread_cond.timed_wait(disc_thread_lock, t);
    }
}

void Audio::sdown_callback(void *arg) throw()
{
    static_cast<Audio *>(arg)->signal_jack_disconnect.emit();
}

int Audio::srate_callback(jack_nframes_t nframes, void *arg) throw()
{
    static_cast<Audio *>(arg)->m_samplerate = nframes;
    samplerate = nframes;
    return 0;
}

int Audio::audio_callback(jack_nframes_t nframes, void *arg) throw()
{
    return static_cast<Audio *>(arg)->audio_callback0(nframes);
}

int Audio::audio_callback0(jack_nframes_t nframes) throw()
{
    jack_default_audio_sample_t *buffers[8];
    float deltas[8];
    long fade_len[8];

    // Get and clear output buffers
    for (int i = 0; i < 8; ++i) {
	buffers[i] = (jack_default_audio_sample_t *)
			jack_port_get_buffer(ports[i], nframes);
	memset(buffers[i], 0, nframes * samplesize);
    }

    boost::shared_ptr<Afs> pr = afs.reader();
    Afs::const_iterator i = pr->begin();

    for ( ; i != pr->end(); ++i) {
	AudioFile * l = i->get();

	if (l->status != Play || l->eob) continue;
	Glib::Mutex::Lock lock(l->buffer_lock, Glib::TRY_LOCK);
	if (!lock.locked()) continue;

	size_t n = jack_ringbuffer_read_space(l->rbs[l->rbs.size() - 1]) / samplesize;
	if (n == 0) {
	    if (l->eof) {
		l->eob = true;
		l->stop();
	    }
	    continue;
	}

	if (n > nframes) n = nframes;

	for (int j = 0; j < 8; ++j) {
	    deltas[j] = 0.0;
	    fade_len[j] = 0;
	}
	boost::shared_ptr<AudioFile::Fades> fades = l->fades.reader();
	AudioFile::Fades::const_iterator f = fades->begin();
	for ( ; f != fades->end(); ++f) {
	    AudioFile::fade_ * fa = f->get();
	    if (fa->status != Play) continue;
            if (fa->nframes <= 0) fa->nframes = 1;
	    for (size_t j = 0; j < fa->on.size(); ++j) {
		if (fa->on[j]) {
		    deltas[j] = (l->vol[j] - fa->vol[j]) / fa->nframes;
		    fade_len[j] = fa->nframes > n ? n : fa->nframes;
		}
	    }
            fa->nframes -= n;
	    if (fa->nframes <= 0) {
                fa->status = Done;
	        if (fa->stop_on_complete) l->stop();
		if (fa->pause_on_complete) l->pause();
	    }
	}

	for (size_t p = 0; p < l->patch.size(); ++p) {
	    if (l->patch[p].src >= l->rbs.size())
		continue;

	    jack_ringbuffer_data_t rdata[2];
	    jack_ringbuffer_get_read_vector(l->rbs[l->patch[p].src], rdata);

	    jack_default_audio_sample_t * s1 =
		(jack_default_audio_sample_t *)(rdata[0].buf);
	    jack_default_audio_sample_t * s2 =
		(jack_default_audio_sample_t *)(rdata[1].buf);
	    jack_default_audio_sample_t * dest = buffers[l->patch[p].dest];
	    float vol = l->vol[l->patch[p].dest];
	    float delta = deltas[l->patch[p].dest];
	    size_t len1 = rdata[0].len / samplesize;
	    size_t len2 = rdata[1].len / samplesize;
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
	for (size_t c = 0; c < l->rbs.size(); ++c)
	    jack_ringbuffer_read_advance(l->rbs[c], n * samplesize);
	l->cur_frame += nframes;
    }

    return 0;
}

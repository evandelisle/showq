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

#ifndef AUDIO_H__
#define AUDIO_H__

#include "rcu.h"

#include <list>
#include <boost/shared_ptr.hpp>

#include <alsa/asoundlib.h>
#include <jack/jack.h>
#include <jack/ringbuffer.h>

#include <sndfile.h>

#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>

#include <glibmm.h>

#include <samplerate.h>
#include <libxml++/libxml++.h>

enum {
    Play, Pause, Stop, Done
};
enum {NoCodec, SndFile, OggVorbis};

struct patch_ {
    unsigned int src;
    unsigned int dest;
};

class AudioFile {
public:
    AudioFile(const char *);
    ~AudioFile();
    class fade_ {
    public:
        fade_() : status(Play) {}

	void play() { status = Play; }
	void pause() { status = Pause; }
	void stop() { status = Stop; }
	std::vector<float> vol;
	std::vector<bool> on;
	long nframes;
	long tframes;
	int status;
	bool stop_on_complete;
	bool pause_on_complete;
    };
    size_t read_cb();
    size_t sf_read(float *** vbuf, size_t n);

    std::string get_info_str();
    double total_time();

    void play() { status = Play; }
    void pause() { status = Pause; }
    void stop();
    void seek(double);
    double get_pos();
    void add_fade(boost::shared_ptr<fade_> f);
    int get_codec() { return codec;};

    int status;
    bool eof;
    bool eob;

    typedef std::list<boost::shared_ptr<fade_> > Fades;
    SerializedRCUManager<Fades> fades;
    std::vector<SRC_STATE *> src;
    Glib::Mutex buffer_lock;
    std::vector<jack_ringbuffer_t *> rbs;
    std::vector<float> vol;
    std::vector<patch_> patch;

    size_t cur_frame;
private:
    int srate;
    int codec;
    OggVorbis_File vf;
    vorbis_info * vi;
    SNDFILE *sf;
    SF_INFO sfinfo;

    bool do_seek;
    double seek_pos;
    double mTotalTime;
    size_t read_frame;
    int num_channels;
};

class Audio {
public:
    Audio();
    ~Audio();

    class NoAudio {};

    void add_af(boost::shared_ptr<AudioFile>);
    void do_disc_thread();

    int port_set_name(int port, const Glib::ustring & name);
    void disconnect_all();
    int connect(int port, const Glib::ustring & name);

    float get_cpu_load();
    long get_sample_rate();

    void serialize(xmlpp::Element * el);

    Glib::Dispatcher signal_jack_disconnect;

    long m_samplerate;
protected:
private:
    void setup_ports();
    static void sdown_callback(void *arg) throw();
    static int srate_callback(jack_nframes_t nframes, void *arg) throw();
    static int audio_callback(jack_nframes_t nframes, void *ar) throw();
    int audio_callback0(jack_nframes_t nframes) throw();
    void disc_thread();

    Glib::Mutex disc_thread_lock;
    Glib::Cond disc_thread_cond;

    Glib::Thread *disc_thread_p;
    gint running;

    jack_client_t *client;
    jack_port_t *ports[8];

    typedef std::list< boost::shared_ptr<AudioFile> > Afs;
    SerializedRCUManager<Afs> afs;
};

extern Audio * audio;

#endif

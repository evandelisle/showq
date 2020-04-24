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

#ifndef AUDIO_H_
#define AUDIO_H_

#include <stddef.h>

#include <atomic>
#include <condition_variable>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <alsa/asoundlib.h>
#include <jack/ringbuffer.h>
#include <jack/types.h>

#include <sndfile.h>

#include <glibmm/dispatcher.h>
#include <glibmm/ustring.h>

#include <samplerate.h>
#include "rcu.h"
namespace xmlpp { class Element; }

enum {
  Play, Pause, Stop, Done
};
enum {NoCodec, SndFile};

struct patch_ {
  unsigned int src;
  unsigned int dest;
};

class AudioFile {
public:
  explicit AudioFile(const char *);
  AudioFile(const AudioFile &) = delete;
  AudioFile & operator=(const AudioFile &) = delete;
  ~AudioFile();

  class fade_ {
  public:
    fade_() : nframes(0), tframes(0), status(Play),
      stop_on_complete(false), pause_on_complete(false) {}

    void play()
    {
      status = Play;
    }
    void pause()
    {
      status = Pause;
    }
    void stop()
    {
      status = Stop;
    }
    std::vector<float> vol;
    std::vector<bool> on;
    long nframes;
    long tframes;
    int status;
    bool stop_on_complete;
    bool pause_on_complete;
  };

  size_t read_cb();
  std::string get_info_str();
  double total_time();

  void play()
  {
    status = Play;
  }
  void pause()
  {
    status = Pause;
  }
  void stop();
  void seek(double);
  double get_pos();
  void add_fade(std::shared_ptr<fade_> f);
  int get_codec()
  {
    return codec;
  }

  int status;
  bool eof;
  bool eob;

  typedef std::list<std::shared_ptr<fade_> > Fades;
  SerializedRCUManager<Fades> fades;
  SRC_STATE * SRC_state;
  std::mutex buffer_lock;
  std::vector<jack_ringbuffer_t *> rbs;
  std::vector<float> vol;
  std::vector<patch_> patch;

  size_t cur_frame;

  static long src_callback(void *cb_data, float **data);

private:
  int srate;
  int codec;
  SNDFILE *sf;
  SF_INFO sfinfo;

  bool do_seek;
  double seek_pos;
  double mTotalTime;
  size_t read_frame;
  int num_channels;

  float *SRC_input_buffer;
  size_t SRC_input_buffer_size;
  float *input_buffer;
  size_t input_buffer_size;
};

class Audio {
public:
  Audio();
  ~Audio();

  class NoAudio {};

  void add_af(std::shared_ptr<AudioFile>);
  void do_disc_thread();

  int port_set_name(int port, const Glib::ustring &name);
  void disconnect_all();
  int connect(int port, const Glib::ustring &name);

  float get_cpu_load();
  long get_sample_rate();

  void serialize(xmlpp::Element *el);

  Glib::Dispatcher signal_jack_disconnect;

  long m_samplerate;

private:
  void setup_ports();
  static void sdown_callback(void *arg) noexcept;
  static int srate_callback(jack_nframes_t nframes, void *arg) noexcept;
  static int audio_callback(jack_nframes_t nframes, void *ar) noexcept;
  int audio_callback0(jack_nframes_t nframes) noexcept;
  void disc_thread();

  std::mutex disc_thread_lock;
  std::condition_variable disc_thread_cond;

  std::thread disc_thread_p;
  std::atomic<bool> running;

  jack_client_t *client;
  jack_port_t *ports[8];

  typedef std::list<std::shared_ptr<AudioFile> > Afs;
  SerializedRCUManager<Afs> afs;
};

extern Audio *audio;

#endif

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

#ifndef CUE_H_
#define CUE_H_

#include <string>
#include <map>
#include <vector>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <libxml++/libxml++.h>
#pragma GCC diagnostic pop
#include <gtkmm.h>

#include "audio.h"
#include "uuid_cpp.h"

class Cue {
public:
  Cue() : cue_id_no(0), delay(0.0), autocont(false), target(0), keyval(0) {}
  virtual ~Cue() {}

  enum {Base, MIDI, Wave, Stop, Fade, Group, Pause, Start};

  virtual std::string cue_type_text()
  {
    return "";
  }
  virtual int cue_type()
  {
    return Base;
  }
  virtual bool run(Gtk::TreeModel::iterator )
  {
    return true;
  }
  virtual void serialize(xmlpp::Element *cel);
  virtual bool validate()
  {
    return true;
  }
  std::string validate_reason()
  {
    return v_reason;
  }

  std::string cue_id;  // Visable cue number or string eg 1, 2a or 3.4
  std::string text;
  std::string note;
  uuid::uuid cue_id_no;      // Unique cue identifier
  double delay;
  bool autocont;

  uuid::uuid target;
// Hot key
  Gdk::ModifierType state;
  guint keyval;

protected:
  std::string v_reason;
};

class Group_Cue : public Cue {
public:
  std::string cue_type_text() override
  {
    return "Group";
  }
  int cue_type() override
  {
    return Group;
  }
  bool run(Gtk::TreeModel::iterator r) override;
  void serialize(xmlpp::Element *cel) override;

  int mode;
};

class MIDI_Cue : public Cue {
public:
  std::string cue_type_text() override
  {
    return "MIDI";
  }
  int cue_type() override
  {
    return MIDI;
  }
  bool run(Gtk::TreeModel::iterator r) override;
  void serialize(xmlpp::Element *cel) override;

  class msg {
  public:
    int port;
    std::vector<unsigned char> midi_data;
  };

  std::vector<msg> msgs;
};

class Wave_Cue : public Cue {
public:
  Wave_Cue() : start_time(0.0)
  {
    vol.resize(8);
  }

  std::string cue_type_text() override
  {
    return "Wave";
  }
  int cue_type() override
  {
    return Wave;
  }
  bool run(Gtk::TreeModel::iterator r) override;
  void serialize(xmlpp::Element *cel) override;

  std::string file;

  double start_time;

  std::vector<float> vol;
  std::vector<patch_> patch;
};

class Stop_Cue : public Cue {
public:
  std::string cue_type_text() override
  {
    return "Stop";
  }
  int cue_type() override
  {
    return Stop;
  }
  bool run(Gtk::TreeModel::iterator r) override;
  void serialize(xmlpp::Element *cel) override;
};

class Pause_Cue : public Cue {
public:
  std::string cue_type_text() override
  {
    return "Pause";
  }
  int cue_type() override
  {
    return Pause;
  }
  bool run(Gtk::TreeModel::iterator r) override;
  void serialize(xmlpp::Element *cel) override;
};

class Start_Cue : public Cue {
public:
  std::string cue_type_text() override
  {
    return "Start";
  }
  int cue_type() override
  {
    return Start;
  }
  bool run(Gtk::TreeModel::iterator r) override;
  void serialize(xmlpp::Element *cel) override;
};

class FadeStop_Cue : public Cue {
public:
  FadeStop_Cue()
    : stop_on_complete(false), pause_on_complete(false), fade_time(0.0)
  {
    tvol.resize(8, 0.0);
    on.resize(8, false);
  }

  std::string cue_type_text() override
  {
    return "Fade";
  }
  int cue_type() override
  {
    return Fade;
  }
  bool run(Gtk::TreeModel::iterator r) override;
  void serialize(xmlpp::Element *cel) override;

  bool stop_on_complete;
  bool pause_on_complete;
  double fade_time;
  std::vector<float> tvol;
  std::vector<bool> on;
};

#endif // CUE_H_

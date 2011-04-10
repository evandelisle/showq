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

#ifndef CUE_H__
#define CUE_H__

#include <string>
#include <map>
#include <vector>
#include <libxml++/libxml++.h>
#include <boost/shared_ptr.hpp>
#include <gtkmm.h>

#include "audio.h"

class Cue {
public:
    Cue() : delay(0.0), autocont(false), target(0), keyval(0) {}
    virtual ~Cue() {}

    enum {Base, MIDI, Wave, Stop, Fade, Group, Pause, Start};

    virtual std::string cue_type_text() { return "";}
    virtual int cue_type() { return Base; }
    virtual bool run(Gtk::TreeModel::iterator r) { return true;}
    virtual void serialize(xmlpp::Element *cel);
    virtual bool validate() { return true;}
    std::string validate_reason() { return v_reason;}

    std::string cue_id;
    std::string text;
    std::string note;
    long cue_id_no;
    double delay;
    bool autocont;

    long target;
// Hot key
    Gdk::ModifierType state;
    guint keyval;
protected:
    std::string v_reason;
};

class Group_Cue : public Cue {
public:
    virtual std::string cue_type_text() { return "Group"; }
    virtual int cue_type() { return Group; }
    virtual bool run(Gtk::TreeModel::iterator r);
    virtual void serialize(xmlpp::Element *cel);

    int mode;
};

class MIDI_Cue : public Cue {
public:
    virtual std::string cue_type_text() { return "MIDI"; }
    virtual int cue_type() { return MIDI; }
    virtual bool run(Gtk::TreeModel::iterator r);
    virtual void serialize(xmlpp::Element *cel);

    class msg {
    public:
	int port;
	std::vector<unsigned char> midi_data;
    };

    std::vector<msg> msgs;
};

class Wave_Cue : public Cue {
public:
    Wave_Cue() : start_time(0.0) { vol.resize(8); }
    virtual std::string cue_type_text() { return "Wave"; }
    virtual int cue_type() { return Wave; }
    virtual bool run(Gtk::TreeModel::iterator r);
    virtual void serialize(xmlpp::Element *cel);

    std::string file;

    double start_time;

    std::vector<float> vol;
    std::vector<patch_> patch;
};

class Stop_Cue : public Cue {
public:
    virtual std::string cue_type_text() { return "Stop"; }
    virtual int cue_type() { return Stop; }
    virtual bool run(Gtk::TreeModel::iterator r);
    virtual void serialize(xmlpp::Element *cel);
};

class Pause_Cue : public Cue {
public:
    virtual std::string cue_type_text() { return "Pause"; }
    virtual int cue_type() { return Pause; }
    virtual bool run(Gtk::TreeModel::iterator r);
    virtual void serialize(xmlpp::Element *cel);
};

class Start_Cue : public Cue {
public:
    virtual std::string cue_type_text() { return "Start"; }
    virtual int cue_type() { return Start; }
    virtual bool run(Gtk::TreeModel::iterator r);
    virtual void serialize(xmlpp::Element *cel);
};

class FadeStop_Cue : public Cue {
public:
    FadeStop_Cue() : stop_on_complete(false), pause_on_complete(false), fade_time(0.0) {}

    virtual std::string cue_type_text() { return "Fade"; }
    virtual int cue_type() { return Fade; }
    virtual bool run(Gtk::TreeModel::iterator r);
    virtual void serialize(xmlpp::Element *cel);

    bool stop_on_complete;
    bool pause_on_complete;
    double fade_time;
    std::vector<float> tvol;
    std::vector<bool> on;
};

#endif /* CUE_H__ */

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

#include "app.h"
#include "editcue.h"
#include "utils.h"
#include "main.h"

#include <cmath>
#include <sstream>
#include <iostream>

// Fader

Fader::Fader(BaseObjectType *cobject, const Glib::RefPtr<Gtk::Builder>& refXml)
   : Gtk::VScale(cobject)
{
}

Glib::ustring Fader::on_format_value(double v)
{
    double gain = slider_to_gain(v);
    double db = gain_to_db(gain);
    std::ostringstream s;
    s.setf(std::ios::fixed, std::ios::floatfield);
    s.precision(1);
    s << db;
    return s.str();
}

double Fader::get_gain()
{
    return slider_to_gain(get_value());
}

void Fader::set_gain(double gain)
{
    set_value(gain_to_slider(gain));
}

// Wave cue editing

EditCueWave::EditCueWave(Gtk::Notebook *p)
{
    refXML_wave = Gtk::Builder::create_from_file(showq_ui+"editwave.ui");
    refXML_patch = Gtk::Builder::create_from_file(showq_ui+"epatch.ui");
    refXML_faders = Gtk::Builder::create_from_file(showq_ui+"efaders.ui");

    refXML_wave->get_widget("ed_wave_file", m_wave_fentry);
    m_wave_fentry->signal_selection_changed().
	connect(sigc::mem_fun(*this, &EditCueWave::wave_on_file_activate));
    refXML_wave->get_widget("ed_wave_start", m_wave_start);

    for (int i = 0; i < 8; ++i) {
	std::ostringstream s;
	s << "ed_wave_f" << (i + 1);
	Fader *vs;

	refXML_faders->get_widget_derived(s.str().c_str(), vs);

	vs->signal_value_changed().connect(
	    sigc::bind<int>(sigc::mem_fun(*this, &EditCueWave::wave_on_value_change), i));

	m_wave_faders.push_back(vs);
    }

    connect_clicked(refXML_wave, "ed_wave_play", sigc::mem_fun(*this, &EditCueWave::wave_on_play));
    connect_clicked(refXML_wave, "ed_wave_pause", sigc::mem_fun(*this, &EditCueWave::wave_on_pause));
    connect_clicked(refXML_wave, "ed_wave_stop", sigc::mem_fun(*this, &EditCueWave::wave_on_stop));

    refXML_wave->get_widget("ed_wave_slide", m_wave_tslide);

    m_wave_tslide->signal_button_press_event().connect(
        sigc::mem_fun(*this, &EditCueWave::sl_button_press_event), false);
    m_wave_tslide->signal_button_release_event().connect(
        sigc::mem_fun(*this, &EditCueWave::sl_button_release_event), false);
    m_wave_tslide->signal_change_value().connect(
        sigc::mem_fun(*this, &EditCueWave::sl_change_value));
    m_wave_tslide->signal_format_value().connect(
        sigc::mem_fun(*this, &EditCueWave::sl_format_value));
    update_hs_ok = true;

    dis_timer = Glib::signal_timeout().connect(sigc::mem_fun(*this, &EditCueWave::dis_update), 100);

    Gtk::Widget *wave_tab;
    Gtk::Widget *patch_tab;
    Gtk::Widget *level_tab;
    refXML_wave->get_widget("edit_wave_vbox", wave_tab);
    refXML_patch->get_widget("edit_patch_vbox", patch_tab);
    refXML_faders->get_widget("edit_faders_vbox", level_tab);
    p->append_page(*wave_tab, "Wave");
    p->append_page(*patch_tab, "Patch");
    p->append_page(*level_tab, "Levels");
}

EditCueWave::~EditCueWave()
{
    if (m_af) {
	m_af->stop();
	m_af.reset();
    }
}

void EditCueWave::set(boost::shared_ptr<Cue> & c)
{
    boost::shared_ptr<Wave_Cue> q = boost::dynamic_pointer_cast<Wave_Cue>(c);

    m_wave_fentry->set_filename(q->file);
    m_wave_start->set_value(q->start_time);

    std::vector<Fader *>::const_iterator i = m_wave_faders.begin();
    std::vector<float>::const_iterator j = q->vol.begin();
    for ( ; i != m_wave_faders.end() && j != q->vol.end(); ++i, ++j) {
	(*i)->set_gain(*j);
    }

    std::vector<patch_>::const_iterator k = q->patch.begin();
    for ( ; k != q->patch.end(); ++k) {
	std::ostringstream s;
	s << "ed_wave_p" << k->src << '_' << k->dest;
	Gtk::ToggleButton *tb;
	refXML_patch->get_widget(s.str(), tb);
	tb->set_active();
    }
}

void EditCueWave::get(boost::shared_ptr<Cue> & c)
{
    boost::shared_ptr<Wave_Cue> q = boost::dynamic_pointer_cast<Wave_Cue>(c);

    q->file = m_wave_fentry->get_filename();
    q->start_time = m_wave_start->get_value();

    q->vol.clear();
    std::vector<Fader *>::const_iterator i = m_wave_faders.begin();
    for ( ; i != m_wave_faders.end(); ++i) {
	q->vol.push_back((*i)->get_gain());
    }

    for (int src = 0; src < 8; ++src) {
	for (int dest = 0; dest < 8; ++dest) {
	    std::ostringstream s;
	    s << "ed_wave_p" << src << '_' << dest;
	    Gtk::ToggleButton *tb;
	    refXML_patch->get_widget(s.str(), tb);
	    if (tb->get_active()) {
		struct patch_ p;
		p.src = src;
		p.dest = dest;
		q->patch.push_back(p);
	    }
	}
    }
}

void EditCueWave::wave_on_file_activate()
{
    std::string file = m_wave_fentry->get_filename();
    if (file.size() == 0) return;

    Gtk::Label * p_len;
    refXML_wave->get_widget("ed_wave_llength", p_len);
    Gtk::Label * p_info;
    refXML_wave->get_widget("ed_wave_ltype", p_info);

    AudioFile af(file.c_str());
    if (af.get_codec() == NoCodec) {
        p_len->set_text("");
        p_info->set_text("");
        return;
    }

    double length = af.total_time();

    m_wave_start->set_range(0.0, length);

    p_len->set_text(dtoasctime(length));
    p_info->set_text(af.get_info_str());
    m_wave_tslide->set_range(0.0, length);
}

void EditCueWave::wave_on_value_change(int fader)
{
    if (m_af && m_af->status == Play) {
	m_af->vol[fader] = m_wave_faders[fader]->get_gain();
    }
}

void EditCueWave::wave_on_play()
{
    if (m_af && m_af->status != Done) {
	m_af->play();
    } else {
	std::string file = m_wave_fentry->get_filename();
	m_af = boost::shared_ptr<AudioFile>(new AudioFile(file.c_str()));

	boost::shared_ptr<Wave_Cue> mcue = boost::shared_ptr<Wave_Cue>(new Wave_Cue);
	boost::shared_ptr<Cue> q = boost::dynamic_pointer_cast<Cue>(mcue);
	get(q);

	m_af->play();
	if (mcue->start_time > 0.00001)
	    m_af->seek(mcue->start_time);
	m_af->vol = mcue->vol;
	m_af->patch = mcue->patch;
	update_hs_ok = true;
	audio->add_af(m_af);
    }
}

void EditCueWave::wave_on_pause()
{
    if (m_af) {
	if (m_af->status == Pause) {
	    m_af->play();
	    Gtk::ToggleToolButton * tb;
	    refXML_wave->get_widget("ed_wave_play", tb);
	    tb->set_active();
	} else
	    m_af->pause();
    } else {
	Gtk::ToggleToolButton * tb;
	refXML_wave->get_widget("ed_wave_stop", tb);
	tb->set_active();
    }
}

void EditCueWave::wave_on_stop()
{
    if (m_af) {
	m_af->stop();
	m_af.reset();
    }
}

bool EditCueWave::sl_change_value(Gtk::ScrollType , double v)
{
    if (m_af && update_hs_ok && !seek_lock) {
    	m_af->seek(v);
    }
    return false;
}

bool EditCueWave::sl_button_press_event(GdkEventButton *)
{
    seek_lock = true;
    return false;
}

bool EditCueWave::sl_button_release_event(GdkEventButton *)
{
    seek_lock = false;
    return false;
}

Glib::ustring EditCueWave::sl_format_value(double v)
{
    return dtoasctime(v);
}

// Update the display

bool EditCueWave::dis_update()
{
    if (m_af) {
	double t = m_af->get_pos();

	update_hs_ok = false;
	if (!seek_lock) {
	    m_wave_tslide->set_value(t);
	    m_wave_tslide->queue_draw();
	}
	update_hs_ok = true;

	if (m_af->status == Done) {
	    Gtk::RadioToolButton * pWidget = 0;
	    refXML_wave->get_widget("ed_wave_stop", pWidget);
	    pWidget->set_active();
	    m_af.reset();
	}
    }

    return true;
}

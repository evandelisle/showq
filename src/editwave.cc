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
#include "cue.h"
#include "editcue.h"
#include "utils.h"

#include <gdk/gdk.h>
#include <glibmm.h>
#include <gtkmm.h>

#include <iomanip>
#include <memory>
#include <string>
#include <vector>

// Fader

Fader::Fader(BaseObjectType *c_object, const Glib::RefPtr<Gtk::Builder> &)
    : Gtk::Scale(c_object)
{
}

Glib::ustring Fader::on_format_value(double v)
{
    return Glib::ustring::format(std::setprecision(1), std::fixed, gain_to_db(slider_to_gain(v)));
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
    : refXML_wave(Gtk::Builder::create_from_resource("/org/evandel/showq/ui/editwave.ui")),
      refXML_patch(Gtk::Builder::create_from_resource("/org/evandel/showq/ui/epatch.ui")),
      refXML_faders(Gtk::Builder::create_from_resource("/org/evandel/showq/ui/efaders.ui"))
{
    refXML_wave->get_widget("ed_wave_file", m_wave_fentry);
    m_wave_fentry->signal_selection_changed().connect(
        sigc::mem_fun(*this, &EditCueWave::wave_on_file_activate));
    refXML_wave->get_widget("ed_wave_start", m_wave_start);

    for (int i = 0; i < 8; ++i) {
        Fader *vs;

        refXML_faders->get_widget_derived(Glib::ustring::compose("ed_wave_f%1", i + 1), vs);

        vs->signal_value_changed().connect(
            sigc::bind<int>(sigc::mem_fun(*this, &EditCueWave::wave_on_value_change), i));

        m_wave_faders.push_back(vs);
    }

    connect_clicked(refXML_wave, "ed_wave_play", sigc::mem_fun(*this, &EditCueWave::wave_on_play));
    connect_clicked(
        refXML_wave, "ed_wave_pause", sigc::mem_fun(*this, &EditCueWave::wave_on_pause));
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

void EditCueWave::set(std::shared_ptr<Cue> &c)
{
    const std::shared_ptr<Wave_Cue> q = std::dynamic_pointer_cast<Wave_Cue>(c);

    m_wave_fentry->set_filename(q->file);
    m_wave_start->set_value(q->start_time);

    auto i = m_wave_faders.begin();
    auto j = q->vol.begin();
    for (; i != m_wave_faders.end() && j != q->vol.end(); ++i, ++j) {
        (*i)->set_gain(*j);
    }

    for (auto &k : q->patch) {
        Gtk::ToggleButton *tb;
        refXML_patch->get_widget(Glib::ustring::compose("ed_wave_p%1_%2", k.src, k.dest), tb);
        tb->set_active();
    }
}

void EditCueWave::get(std::shared_ptr<Cue> &c)
{
    const std::shared_ptr<Wave_Cue> q = std::dynamic_pointer_cast<Wave_Cue>(c);

    q->file = m_wave_fentry->get_filename();
    q->start_time = m_wave_start->get_value();

    q->vol.clear();

    for (auto &m_wave_fader : m_wave_faders) {
        q->vol.push_back(m_wave_fader->get_gain());
    }

    for (int src = 0; src < 8; ++src) {
        for (int dest = 0; dest < 8; ++dest) {
            Gtk::ToggleButton *tb;
            refXML_patch->get_widget(Glib::ustring::compose("ed_wave_p%1_%2", src, dest), tb);
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
    const std::string file = m_wave_fentry->get_filename();
    if (file.empty())
        return;

    Gtk::Label *p_len;
    refXML_wave->get_widget("ed_wave_llength", p_len);
    Gtk::Label *p_info;
    refXML_wave->get_widget("ed_wave_ltype", p_info);

    AudioFile af(file.c_str());
    if (af.get_codec() == NoCodec) {
        p_len->set_text("");
        p_info->set_text("");
        return;
    }

    const double length = af.total_time();

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
        const std::string file = m_wave_fentry->get_filename();
        m_af = std::make_shared<AudioFile>(file.c_str());

        const std::shared_ptr<Wave_Cue> mcue = std::make_shared<Wave_Cue>();
        std::shared_ptr<Cue> q = std::dynamic_pointer_cast<Cue>(mcue);
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
            Gtk::ToggleToolButton *tb;
            refXML_wave->get_widget("ed_wave_play", tb);
            tb->set_active();
        } else
            m_af->pause();
    } else {
        Gtk::ToggleToolButton *tb;
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

bool EditCueWave::sl_change_value(Gtk::ScrollType, double v)
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
        const double t = m_af->get_pos();

        update_hs_ok = false;
        if (!seek_lock) {
            m_wave_tslide->set_value(t);
            m_wave_tslide->queue_draw();
        }
        update_hs_ok = true;

        if (m_af->status == Done) {
            Gtk::RadioToolButton *pWidget = nullptr;
            refXML_wave->get_widget("ed_wave_stop", pWidget);
            pWidget->set_active();
            m_af.reset();
        }
    }

    return true;
}

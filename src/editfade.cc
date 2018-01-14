/*
 * Show Q
 * Copyright (c) 2007-2008 Errol van de l'Isle
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

EditCueFade::EditCueFade(Gtk::Notebook *p)
{
  gsize r_size;
  refXML_fade = Gtk::Builder::create();
  refXML_fade->add_from_string(
      (const char *) Gio::Resource::lookup_data_global("/org/evandel/showq/ui/editfade.ui")->get_data(r_size)
      , -1);

  refXML_faders = Gtk::Builder::create();
  refXML_faders->add_from_string(
      (const char *) Gio::Resource::lookup_data_global("/org/evandel/showq/ui/efaders.ui")->get_data(r_size)
      , -1);

  Gtk::Widget *fade_tab;
  Gtk::Widget *level_tab;
  refXML_fade->get_widget("edit_fade_vbox", fade_tab);
  refXML_faders->get_widget("edit_faders_vbox", level_tab);
  p->append_page(*fade_tab, "Fade");
  p->append_page(*level_tab, "Levels");

  for (int i = 0; i < 8; ++i) {
    auto s1 = Glib::ustring::compose("ed_wave_f%1", (i + 1));
    auto s2 = Glib::ustring::compose("ed_wave_%1", (i + 1));

    Fader *vs;
    Gtk::ToggleButton *tb;

    refXML_faders->get_widget_derived(s1, vs);
    refXML_faders->get_widget(s2, tb);

    tb->signal_toggled().connect(
      sigc::bind<int>(sigc::mem_fun(*this, &EditCueFade::wave_on_toggle), i));

    m_wave_faders.push_back(vs);
    m_wave_but.push_back(tb);

    vs->set_sensitive(false);
  }

  refXML_fade->get_widget("ed_fade_time", m_fade_time);
  refXML_fade->get_widget("ed_fade_defaultcomplete", m_fade_defaultcomplete);
  refXML_fade->get_widget("ed_fade_stopcomplete", m_fade_stopcomplete);
  refXML_fade->get_widget("ed_fade_pausecomplete", m_fade_pausecomplete);
}

EditCueFade::~EditCueFade()
{
}

void EditCueFade::get(std::shared_ptr<Cue> &p)
{
  auto q = std::dynamic_pointer_cast<FadeStop_Cue>(p);
  q->tvol.clear();
  q->on.clear();

  for (const auto &i : m_wave_faders) {
    q->tvol.push_back((*i).get_gain());
  }
  for (const auto &i : m_wave_but) {
    q->on.push_back((*i).get_active());
  }

  q->stop_on_complete = m_fade_stopcomplete->get_active();
  q->pause_on_complete = m_fade_pausecomplete->get_active();
  q->fade_time = m_fade_time->get_value();
}

void EditCueFade::set(std::shared_ptr<Cue> &p)
{
  std::shared_ptr<FadeStop_Cue> q = std::dynamic_pointer_cast<FadeStop_Cue>(p);

  std::vector<Fader *>::const_iterator i = m_wave_faders.begin();
  std::vector<float>::const_iterator j = q->tvol.begin();
  std::vector<Gtk::ToggleButton *>::const_iterator k = m_wave_but.begin();
  std::vector<bool>::const_iterator l = q->on.begin();

  for (; i != m_wave_faders.end() && j != q->tvol.end(); ++i, ++j, ++k, ++l) {
    (*i)->set_gain(*j);
    (*k)->set_active(*l);
  }
  m_fade_defaultcomplete->set_active();
  m_fade_pausecomplete->set_active(q->pause_on_complete);
  m_fade_stopcomplete->set_active(q->stop_on_complete);
  m_fade_time->set_value(q->fade_time);
}

void EditCueFade::wave_on_toggle(int fader)
{
  bool state = m_wave_but[fader]->get_active();

  m_wave_faders[fader]->set_sensitive(state);
}

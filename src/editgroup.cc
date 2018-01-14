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

#include "editcue.h"
#include "app.h"
#include "main.h"

EditCueGroup::EditCueGroup(Gtk::Notebook *p)
{
  gsize r_size;
  refXml = Gtk::Builder::create();
  refXml->add_from_string(
      (const char *) Gio::Resource::lookup_data_global("/org/evandel/showq/ui/editgroup.ui")->get_data(r_size)
      , -1);

  Gtk::Widget *vbox;
  refXml->get_widget("edit_group_box", vbox);

  p->append_page(*vbox, "Group mode");
}

EditCueGroup::~EditCueGroup()
{
}

void EditCueGroup::set(std::shared_ptr<Cue> &q)
{
  std::shared_ptr<Group_Cue> pg = std::dynamic_pointer_cast<Group_Cue>(q);

  Gtk::RadioButton *button = 0;
  switch (pg->mode) {
  case 0:
    refXml->get_widget("ed_grp_all", button);
    break;
  case 1:
    refXml->get_widget("ed_grp_first", button);
    break;
  }
  if (button) button->set_active();
}

void EditCueGroup::get(std::shared_ptr<Cue> &q)
{
  std::shared_ptr<Group_Cue> pg = std::dynamic_pointer_cast<Group_Cue>(q);

  Gtk::RadioButton *button;
  refXml->get_widget("ed_grp_all", button);
  if (button->get_active()) pg->mode = 0;
  refXml->get_widget("ed_grp_first", button);
  if (button->get_active()) pg->mode = 1;
}

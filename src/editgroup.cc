/*
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

EditCueGroup::EditCueGroup(Gtk::Notebook * p)
{
    refXml = Gtk::Builder::create_from_file(showq_ui+"editgroup.ui");

    Gtk::Widget *vbox;
    refXml->get_widget("edit_group_box", vbox);

    p->append_page(*vbox, "Group mode");
}

EditCueGroup::~EditCueGroup()
{
}

void EditCueGroup::set(boost::shared_ptr<Cue> & q)
{
    boost::shared_ptr<Group_Cue> pg = boost::dynamic_pointer_cast<Group_Cue>(q);

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

void EditCueGroup::get(boost::shared_ptr<Cue> & q)
{
    boost::shared_ptr<Group_Cue> pg = boost::dynamic_pointer_cast<Group_Cue>(q);

    Gtk::RadioButton *button;
    refXml->get_widget("ed_grp_all", button);
    if (button->get_active()) pg->mode = 0;
    refXml->get_widget("ed_grp_first", button);
    if (button->get_active()) pg->mode = 1;
}

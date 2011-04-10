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

#include <iostream>
#include <string>
#include "app.h"
#include "pref.h"
#include "main.h"

Properties::Properties(BaseObjectType *cobject, const Glib::RefPtr<Gtk::Builder>& refXml)
    : Gtk::Dialog(cobject), m_refXml(refXml)
{
    Gtk::Entry * name = 0;
    m_refXml->get_widget("prop_name", name);
    Gtk::TextView * note = 0;
    m_refXml->get_widget("prop_note", note);

    old_name = app->title;
    old_note = app->note;

    name->set_text(app->title);
    note->get_buffer()->set_text(app->note);
}

void Properties::on_response(int r)
{
    Gtk::Entry * name = 0;
    Gtk::TextView * note = 0;

    switch (r) {
    case 0:
        return;
    case Gtk::RESPONSE_CANCEL:
        app->title = old_name;
        app->set_title(old_note + " - Show Q");
        app->note = old_note;
        break;
    case Gtk::RESPONSE_APPLY:
    case Gtk::RESPONSE_OK:
	m_refXml->get_widget("prop_name", name);
	m_refXml->get_widget("prop_note", note);

	app->title = name->get_text();
	app->set_title(app->title + " - Show Q");

	app->note = note->get_buffer()->get_text();

        if (r == Gtk::RESPONSE_APPLY) return;
	break;
    default:
	break;
    }
    hide();
}

std::auto_ptr<Properties> Properties::create()
{
    Properties * dialog;
    Glib::RefPtr <Gtk::Builder> refXml
	= Gtk::Builder::create_from_file(showq_ui + "prop.ui");
    refXml->get_widget_derived("Properties", dialog);

    return std::auto_ptr<Properties>(dialog);
}

Preferences::Preferences(BaseObjectType *cobject, const Glib::RefPtr<Gtk::Builder>& refXml)
    : Gtk::Dialog(cobject), m_refXml(refXml)
{
    Gtk::CheckButton * p_su;
    Gtk::CheckButton * p_disable_move_next;
    m_refXml->get_widget("Pref_load_su", p_su);
    m_refXml->get_widget("Pref_disable_move_next", p_disable_move_next);
    try {
        p_su->set_active(keyfile.get_boolean("main", "LoadLast"));
        p_disable_move_next->set_active(keyfile.get_boolean("main", "DisableMoveNext"));
    }
    catch (...) {
    }
    show();
}

std::auto_ptr<Preferences> Preferences::create()
{
    Preferences * dialog;
    Glib::RefPtr <Gtk::Builder> refXml
	= Gtk::Builder::create_from_file(showq_ui+"pref.ui");
    refXml->get_widget_derived("Preferences", dialog);

    return std::auto_ptr<Preferences>(dialog);
}

void Preferences::on_response(int r)
{
    switch (r) {
    case 0:
        return;
    case Gtk::RESPONSE_OK:
        Gtk::CheckButton * p_su;
        Gtk::CheckButton * p_disable_move_next;
        m_refXml->get_widget("Pref_load_su", p_su);
        m_refXml->get_widget("Pref_disable_move_next", p_disable_move_next);
        keyfile.set_boolean("main", "LoadLast", p_su->get_active());
        keyfile.set_boolean("main", "DisableMoveNext", p_disable_move_next->get_active());
        break;
    }
    hide();
}

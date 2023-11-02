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

#ifndef PREF_H
#define PREF_H

#include <gtkmm.h>
#include <memory>

class Properties : public Gtk::Dialog {
public:
    Properties(BaseObjectType *c_object, const Glib::RefPtr<Gtk::Builder> &refXml);
    ~Properties() override = default;

    static std::unique_ptr<Properties> create();

protected:
    void on_response(int) override;

private:
    Glib::ustring old_name;
    Glib::ustring old_note;
    Glib::RefPtr<Gtk::Builder> m_refXml;
};

class Preferences : public Gtk::Dialog {
public:
    Preferences(BaseObjectType *c_object, const Glib::RefPtr<Gtk::Builder> &refXml);
    ~Preferences() override = default;

    static std::unique_ptr<Preferences> create();

protected:
    void on_response(int) override;
    Glib::RefPtr<Gtk::Builder> m_refXml;
};

#endif // PREF_H

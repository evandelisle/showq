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

#ifndef _PREF_H
#define _PREF_H

#include <memory>
#include <gtkmm.h>

class Properties : public Gtk::Dialog {
public:
    Properties(BaseObjectType *cobject, const Glib::RefPtr<Gtk::Builder>& refXml);
    virtual ~Properties() {}

    static std::auto_ptr<Properties> create();
protected:
    virtual void on_response(int);
private:
    Glib::ustring old_name;
    Glib::ustring old_note;
    Glib::RefPtr<Gtk::Builder> m_refXml;
};

class Preferences : public Gtk::Dialog {
public:
    Preferences(BaseObjectType *cobject, const Glib::RefPtr<Gtk::Builder>& refXml);
    virtual ~Preferences() {}

    static std::auto_ptr<Preferences> create();
protected:
    virtual void on_response(int);
    Glib::RefPtr<Gtk::Builder> m_refXml;
};

#endif /* _PREF_H */

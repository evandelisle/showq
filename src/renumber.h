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

#ifndef RENUMBER_H_
#define RENUMBER_H_

#include <gtkmm.h>
#include <memory>

class Renumber : public Gtk::Dialog {
public:
    Renumber(BaseObjectType *c_object, const Glib::RefPtr<Gtk::Builder> &refXml);
    ~Renumber() override = default;

    static std::unique_ptr<Renumber> create();

    double step{0.0};
    double cue_no{0.0};
    bool skip_autocont{false};

    sigc::signal<void> signal_ok() { return m_signal_ok; }

private:
    void on_response(int) override;

    sigc::signal<void> m_signal_ok;
    Glib::RefPtr<Gtk::Builder> m_refXml;
};

#endif // RENUMBER_H_

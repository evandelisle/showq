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

#include "utils.h"
#include <gtkmm.h>

#include <iostream>
#include <sstream>

void connect_clicked(Glib::RefPtr<Gtk::Builder> m_refXml,
    const Glib::ustring& widget_name, const sigc::slot<void>& slot_ )
{
    Gtk::Widget * pWidget = 0;
    m_refXml->get_widget(widget_name, pWidget);

    Gtk::ToolButton * pToolButton = dynamic_cast<Gtk::ToolButton*>(pWidget);
    Gtk::Button * pButton = dynamic_cast<Gtk::Button*>(pWidget);

    if (pToolButton)
	pToolButton->signal_clicked().connect(slot_);
    if (pButton)
	pButton->signal_clicked().connect(slot_);
}

std::string dtoasctime(double x)
{
    std::stringstream s;

    int hours = int(x / (60 * 60));
    int minutes = int(x / 60) - (hours * 60);
    double seconds = x - (minutes * 60) - (hours * 60 * 60);

    s.setf(std::ios::fixed, std::ios::floatfield);
    s.precision(1);
    s.width(2);
    if (seconds < 10.0)
	s << hours << ':' << minutes << ":0" << seconds;
    else
	s << hours << ':' << minutes << ':' << seconds;

    return s.str();
}

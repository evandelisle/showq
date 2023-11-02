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

#include "utils.h"
#include <gtkmm.h>

#include <iomanip>

void connect_clicked(const Glib::RefPtr<Gtk::Builder> &m_refXml, const Glib::ustring &widget_name,
                     const sigc::slot<void> &slot_)
{
    Gtk::Widget *pWidget = nullptr;
    m_refXml->get_widget(widget_name, pWidget);

    auto *pToolButton = dynamic_cast<Gtk::ToolButton *>(pWidget);

    if (pToolButton) {
        pToolButton->signal_clicked().connect(slot_);
        return;
    }

    auto *pButton = dynamic_cast<Gtk::Button *>(pWidget);
    if (pButton)
        pButton->signal_clicked().connect(slot_);
}

void connect_menu_item(const Glib::RefPtr<Gtk::Builder> &m_refXml, const Glib::ustring &widget_name,
                       const sigc::slot<void> &slot_)
{
    Gtk::Widget *widget;
    m_refXml->get_widget(widget_name, widget);

    auto *pMenuItem = dynamic_cast<Gtk::MenuItem *>(widget);

    if (pMenuItem)
        pMenuItem->signal_activate().connect(slot_);
}

Glib::ustring dtoasctime(double x)
{
    const int hours = int(x / (60 * 60));
    const int minutes = int(x / 60) - (hours * 60);
    const double seconds = x - (minutes * 60) - (hours * 60 * 60);

    return Glib::ustring::compose(
        "%1:%2:%3",
        hours,
        Glib::ustring::format(std::setw(2), std::setfill(L'0'), minutes),
        Glib::ustring::format(
            std::setw(4), std::setfill(L'0'), std::setprecision(1), std::fixed, seconds));
}

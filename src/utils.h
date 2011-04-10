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

#ifndef UTILS_H__
#define UTILS_H__

#include <cmath>
#include <gtkmm.h>

void connect_clicked(Glib::RefPtr<Gtk::Builder> m_refXml,
    const Glib::ustring& widget_name, const sigc::slot<void>& slot_ );
std::string dtoasctime(double x);

static inline double gain_to_slider(double g)
{
    if (g == 0.0) return 0.0;
    return pow((6.0*log(g)/log(2.0)+192.0)/198.0, 8.0);
}

static inline double slider_to_gain(double pos)
{
    if (pos == 0.0) return 0.0;
    return pow(2.0, (sqrt(sqrt(sqrt(pos)))*198.0-192.0)/6.0);
}

static inline double db_to_gain(double db)
{
    return db > -318.8 ? pow(10.0, db * 0.05) : 0.0;
}

static inline double gain_to_db(double g)
{
    return 20.0 * log10(g);
}

#endif /* UTILS_H__ */

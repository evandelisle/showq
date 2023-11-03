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

#include "../config.h"

#include "about.h"

#include <gtkmm.h>

About::About()
{
    set_authors({"Errol van de l'Isle"});
    set_version(VERSION);
    set_license_type(Gtk::LICENSE_GPL_2_0);
    set_website("https://github.com/evandelisle/showq");
    set_website_label("https://github.com/evandelisle/showq");
}

void About::on_response(int)
{
    hide();
}

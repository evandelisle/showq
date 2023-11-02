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

#include "renumber.h"

#include <gtkmm.h>

Renumber::Renumber(BaseObjectType *c_object, const Glib::RefPtr<Gtk::Builder> &refXml)
    : Gtk::Dialog(c_object),
      m_refXml(refXml)
{
}

std::unique_ptr<Renumber> Renumber::create()
{
    Renumber *dialog;
    auto refXml = Gtk::Builder::create_from_resource("/org/evandel/showq/ui/renumber.ui");
    refXml->get_widget_derived("renumber", dialog);

    return std::unique_ptr<Renumber>(dialog);
}

void Renumber::on_response(int r)
{
    switch (r) {
    case 0:
        return;
    case Gtk::RESPONSE_OK:
        Gtk::SpinButton *p_start;
        m_refXml->get_widget("re_start", p_start);
        cue_no = p_start->get_value();

        Gtk::SpinButton *p_step;
        m_refXml->get_widget("re_step", p_step);
        step = p_step->get_value();

        Gtk::CheckButton *p_skip_ac;
        m_refXml->get_widget("re_skip_acont", p_skip_ac);
        skip_autocont = p_skip_ac->get_active();
        m_signal_ok.emit();
        break;
    default:
        break;
    }
    hide();
}

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

#include "editcue.h"
#include "app.h"
#include "utils.h"
#include "main.h"

#include <glibmm/i18n.h>
#include <gdk/gdkkeysyms.h>
#include <iostream>

EditCueBase::EditCueBase()
{
}

EditCueBase::~EditCueBase()
{
}

EditCue::EditCue(BaseObjectType *cobject, const Glib::RefPtr<Gtk::Builder>& refXml)
    : Gtk::Window(cobject), m_refXml(refXml), cue_id_no(0), keyval(0)
{
    m_refXml->get_widget("ed_notebook", m_notebook);

    connect_clicked(m_refXml, "ed_ok", sigc::mem_fun(*this, &EditCue::ok_activate));
    connect_clicked(m_refXml, "ed_apply", sigc::mem_fun(*this, &EditCue::apply_activate));
    connect_clicked(m_refXml, "ed_cancel", sigc::mem_fun(*this, &EditCue::cancel_activate));

    m_refXml->get_widget("ed_trigger_key", m_key_but);
    m_key_but->signal_clicked().connect(sigc::mem_fun(*this, &EditCue::trigger_key));

    m_refXml->get_widget("ed_info_cue_id", m_info_cueid);
    m_refXml->get_widget("ed_info_text", m_info_text);
    m_refXml->get_widget("ed_info_wait", m_info_wait);
    m_refXml->get_widget("ed_info_acont", m_info_autocont);
    m_refXml->get_widget("ed_info_note", m_info_note);
    m_refXml->get_widget("ed_info_target", m_info_target);

    try {
        std::vector<int> win_size = keyfile.get_integer_list("editcue", "geometry");
        if (win_size.size() == 2)
            resize(win_size[0], win_size[1]);
    }
    catch (...) {
    }
}

EditCue::~EditCue()
{
    try {
        std::vector<int> win_size(2);
        get_size(win_size[0], win_size[1]);

        keyfile.set_integer_list("editcue", "geometry", win_size);
    }
    catch (...) {
    }
}

EditCue * EditCue::show(int type)
{
    std::list<boost::shared_ptr<EditCue > >::iterator i = app->p_edit.begin();
    while (i != app->p_edit.end()){
	if ((* i)->is_visible())
	    ++i;
	else
	    i = app->p_edit.erase(i);
    }
    EditCue * p = 0;
    Glib::RefPtr <Gtk::Builder> refXml
	= Gtk::Builder::create_from_file(showq_ui+"editcue.ui");
    refXml->get_widget_derived("editcue", p);

    app->p_edit.push_back(boost::shared_ptr<EditCue>(p));

    p->m_type = type;
    p->cue_id_no = 0;

    if (type == Cue::MIDI || type == Cue::Wave || type == Cue::Group) {
        p->m_info_target->hide();
        Gtk::Widget * tl;
        refXml->get_widget("ed_info_target_label", tl);
        tl->hide();
    }

    switch (type) {
    case Cue::MIDI:
	p->m_tabs = boost::shared_ptr<EditCueBase>(new EditCueMidi(p->m_notebook));
	break;
    case Cue::Wave:
	p->m_tabs = boost::shared_ptr<EditCueBase>(new EditCueWave(p->m_notebook));
	break;
    case Cue::Stop:
	p->m_tabs = boost::shared_ptr<EditCueBase>(new EditCueStop(p->m_notebook));
	break;
    case Cue::Fade:
	p->m_tabs = boost::shared_ptr<EditCueBase>(new EditCueFade(p->m_notebook));
        break;
    case Cue::Group:
        p->m_tabs = boost::shared_ptr<EditCueBase>(new EditCueGroup(p->m_notebook));
        break;
    case Cue::Pause:
        p->m_tabs = boost::shared_ptr<EditCueBase>(new EditCuePause(p->m_notebook));
        break;
    case Cue::Start:
        p->m_tabs = boost::shared_ptr<EditCueBase>(new EditCueStart(p->m_notebook));
        break;
    }

    p->signal_hide().connect(sigc::ptr_fun(&EditCue::show_on_hide));

    p->Gtk::Window::show();

    return p;
}

// TODO : check that we are not editing the selected cue already

void EditCue::show(boost::shared_ptr<Cue> q, Gtk::TreeRowReference & r)
{
    EditCue * p = show(q->cue_type());

    p->m_tabs->set(q);

    p->m_info_cueid->set_text(q->cue_id);
    p->m_info_text->set_text(q->text);
    p->m_info_note->get_buffer()->set_text(q->note);
    p->m_info_wait->set_value(q->delay);
    p->m_info_autocont->set_active(q->autocont);
    p->cue_id_no = q->cue_id_no;
    p->keyval = q->keyval;
    p->state = q->state;

    p->target = q->target;
    app->m_refTreeModel->foreach_iter(sigc::mem_fun(p, &EditCue::get_target));

    p->m_path = r;
}

bool EditCue::get_target(const Gtk::TreeModel::iterator & i)
{
    boost::shared_ptr<Cue> tq = (*i)[app->m_refTreeModel->Col.cue];
    if (tq->cue_id_no == target) {
        m_info_target->set_text(tq->cue_id);
        return true;
    }
    return false;
}

void EditCue::ok_activate()
{
    boost::shared_ptr<Cue> cue;

    switch (m_type) {
    case Cue::MIDI:
        cue = boost::shared_ptr<MIDI_Cue>(new MIDI_Cue);
        break;
    case Cue::Wave:
        cue = boost::shared_ptr<Wave_Cue>(new Wave_Cue);
        break;
    case Cue::Stop:
        cue = boost::shared_ptr<Stop_Cue>(new Stop_Cue);
        break;
    case Cue::Fade:
        cue = boost::shared_ptr<FadeStop_Cue>(new FadeStop_Cue);
        break;
    case Cue::Group:
        cue = boost::shared_ptr<Group_Cue>(new Group_Cue);
        break;
    case Cue::Pause:
        cue = boost::shared_ptr<Pause_Cue>(new Pause_Cue);
        break;
    case Cue::Start:
        cue = boost::shared_ptr<Start_Cue>(new Start_Cue);
        break;
    default:
	hide();
	return;
    }
    m_tabs->get(cue);
    cue->cue_id = m_info_cueid->get_text();
    cue->text = m_info_text->get_text();
    cue->note = m_info_note->get_buffer()->get_text();
    cue->delay = m_info_wait->get_value();
    cue->autocont = m_info_autocont->get_active();
    cue->cue_id_no = cue_id_no;
    cue->keyval = keyval;
    cue->state = state;

    target = 0;
    app->m_refTreeModel->foreach_iter(sigc::mem_fun(*this, &EditCue::check_key));
    cue->target = target;

    Gtk::TreeModel::iterator iter;
    if (cue->cue_id_no) {
	iter = app->replace_cue(cue, m_path);
    } else {
	cue->cue_id_no = app->next_id;
	iter = app->insert_cue(cue);
    }
    if (iter && iter != app->m_refTreeModel->children().end()) {
	app->m_treeview->get_selection()->select(iter);
	app->m_treeview->set_cursor(Gtk::TreeModel::Path(iter));
        app->m_treeview->scroll_to_row(Gtk::TreePath(iter));
    } else {
    	app->m_treeview->get_selection()->unselect_all();
    }
    hide();
}

bool EditCue::check_key(const Gtk::TreeModel::iterator & i)
{
    boost::shared_ptr<Cue> q = (*i)[app->m_refTreeModel->Col.cue];
    if (m_info_target->get_text() == q->cue_id) {
        target = q->cue_id_no;
        return true;
    }
    return false;
}

void EditCue::apply_activate()
{
}

void EditCue::cancel_activate()
{
    hide();
}

void EditCue::show_on_hide()
{
    std::list<boost::shared_ptr<EditCue > >::iterator i = app->p_edit.begin();
    while (i != app->p_edit.end()){
	if ((* i)->is_visible())
	    ++i;
	else {
	    i = app->p_edit.erase(i);
	}
    }
}

bool EditCue::on_key_press_event(GdkEventKey *event)
{
    return Window::on_key_press_event(event);
}

bool EditCue::on_key_release_event(GdkEventKey *event)
{
    if (m_key_but->get_active()) {
        if (!Gtk::AccelGroup::valid(event->keyval, Gdk::ModifierType(event->state)))
            return true;
        m_key_but->set_active(false);
        keyval = event->keyval;
        state = Gdk::ModifierType(event->state);
        if (keyval == GDK_BackSpace) keyval = 0;
        if (keyval)
            m_key_but->set_label(Gtk::AccelGroup::name(keyval, state));
        else
            m_key_but->set_label(_("Disabled"));
        return true;
    }
    return Window::on_key_release_event(event);
}

void EditCue::trigger_key()
{
    if (m_key_but->get_active())
        m_key_but->set_label(_("New Hot Key"));
    else {
        if (keyval)
            m_key_but->set_label(Gtk::AccelGroup::name(keyval, state));
        else
            m_key_but->set_label(_("Disabled"));
    }
}

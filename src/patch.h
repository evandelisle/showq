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

#ifndef PATCH_H__
#define PATCH_H__

#include <gtkmm.h>
#include <alsa/asoundlib.h>

#include <memory>
#include <vector>

class Patch : public Gtk::Dialog {
public:
    Patch(BaseObjectType *cobject, const Glib::RefPtr<Gtk::Builder>& refXml);
    virtual ~Patch();

    static std::auto_ptr<Patch> create();
private:
    virtual void on_response(int);
    void refresh();
    void row_change1(const Gtk::TreeModel::Path&,const Gtk::TreeModel::iterator&);
    void row_change2(const Gtk::TreeModel::Path&,const Gtk::TreeModel::iterator&);

    class ModelColumns : public Gtk::TreeModel::ColumnRecord {
    public:
	ModelColumns()
	{ add(m_col_text); add(m_col_enable); add(m_col_port); add(m_col_client);}

	Gtk::TreeModelColumn<Glib::ustring> m_col_text;
	Gtk::TreeModelColumn<bool> m_col_enable;
	Gtk::TreeModelColumn<int> m_col_port;
	Gtk::TreeModelColumn<int> m_col_client;
    };

    Gtk::ComboBox * p_func;

    Gtk::TreeView * m_treeview;
    ModelColumns m_Columns;
    Glib::RefPtr<Gtk::ListStore> m_refTreeModel;

    Gtk::TreeView * m_oports;
    Glib::RefPtr<Gtk::ListStore> m_refOports;
    std::vector<snd_seq_addr> m_oconnections[8];

    bool in_refresh;

    Glib::RefPtr<Gtk::Builder> m_refXml;
};

#endif

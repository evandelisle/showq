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

#include "patch.h"
#include "main.h"

#include <alsa/asoundlib.h>

extern snd_seq_t * oseq;

Patch::Patch(BaseObjectType *cobject, const Glib::RefPtr<Gtk::Builder>& refXml)
    : Gtk::Dialog(cobject), m_refXml(refXml)
{
    try {
        std::vector<int> win_size = keyfile.get_integer_list("patch", "geometry");
        if (win_size.size() == 2)
            resize(win_size[0], win_size[1]);
    }
    catch (...) {
    }

    m_refXml->get_widget("Pa_MIDI_tree", m_treeview);
    m_refTreeModel = Gtk::ListStore::create(m_Columns);
    m_treeview->set_model(m_refTreeModel);
    m_treeview->append_column_editable("Connect", m_Columns.m_col_enable);
    m_treeview->append_column("Port", m_Columns.m_col_text);

    m_refXml->get_widget("Pa_MIDI_oports", m_oports);
    m_refOports = Gtk::ListStore::create(m_Columns);
    m_oports->set_model(m_refOports);
    m_oports->append_column("Port", m_Columns.m_col_port);
    m_oports->append_column_editable("Enable", m_Columns.m_col_enable);
    m_oports->append_column_editable("Name", m_Columns.m_col_text);

    snd_seq_port_info_t *info;
    snd_seq_port_info_alloca(&info);

    for (int i = 1; i < 9; ++i) {
	Gtk::TreeModel::iterator iter = m_refOports->append();
	Gtk::TreeModel::Row row = *iter;

	row[m_Columns.m_col_port] = i;

	if (snd_seq_get_port_info(oseq, i, info) < 0) {
	    row[m_Columns.m_col_enable] = false;
	} else {
	    row[m_Columns.m_col_enable] = true;
	    row[m_Columns.m_col_text] = snd_seq_port_info_get_name(info);
	}
    }

    m_refTreeModel->signal_row_changed().connect(sigc::mem_fun(*this, &Patch::row_change2));
    m_refOports->signal_row_changed().connect(sigc::mem_fun(*this, &Patch::row_change1));
    m_oports->get_selection()->
	signal_changed().connect(sigc::mem_fun(*this, &Patch::refresh));

    Dialog::show();
}

Patch::~Patch()
{
    try {
        std::vector<int> win_size(2);
        get_size(win_size[0], win_size[1]);

        keyfile.set_integer_list("patch", "geometry", win_size);
    }
    catch (...) {
    }
}

void Patch::row_change1(const Gtk::TreeModel::Path& path,const Gtk::TreeModel::iterator& iter)
{
    if (in_refresh) return;

    snd_seq_port_info_t *info;
    snd_seq_port_info_alloca(&info);

    Gtk::TreeModel::Row row = *iter;

    int port = row[m_Columns.m_col_port];
    bool enable = row[m_Columns.m_col_enable];
    Glib::ustring name = row[m_Columns.m_col_text];

    int i = snd_seq_get_port_info(oseq, port, info);

    if (i >= 0) {
	if (enable == false) {
	    snd_seq_delete_port(oseq, port);
	} else {
	    snd_seq_port_info_set_name(info, name.c_str());
	    snd_seq_set_port_info(oseq, port, info);
	}
    } else {
	if (enable == true) {
	    snd_seq_port_info_set_capability(info, SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ);
	    snd_seq_port_info_set_port_specified(info, 1);
	    snd_seq_port_info_set_port(info, port);
	    snd_seq_port_info_set_type(info, SND_SEQ_PORT_TYPE_MIDI_GENERIC|SND_SEQ_PORT_TYPE_APPLICATION);
	    snd_seq_port_info_set_name(info, name.c_str());
	    snd_seq_create_port(oseq, info);
	}
    }
}

void Patch::row_change2(const Gtk::TreeModel::Path& path,const Gtk::TreeModel::iterator& iter)
{
    if (in_refresh) return;

    Gtk::TreeModel::iterator j = m_oports->get_selection()->get_selected();
    if (!j || !(*j)[m_Columns.m_col_enable])
	return;

    int midi_port = (*j)[m_Columns.m_col_port];

    int client = (*iter)[m_Columns.m_col_client];
    int port = (*iter)[m_Columns.m_col_port];

    if ((*iter)[m_Columns.m_col_enable]) {
	snd_seq_connect_to(oseq, midi_port, client, port);
    } else {
	snd_seq_disconnect_to(oseq, midi_port, client, port);
    }
}

void Patch::refresh()
{
    in_refresh = true;

    Gtk::TreeModel::iterator j = m_oports->get_selection()->get_selected();

    snd_seq_port_info_t *info;
    snd_seq_client_info_t *cinfo;

    snd_seq_port_info_alloca(&info);
    snd_seq_client_info_alloca(&cinfo);

    snd_seq_query_subscribe_t *pAlsaSubs;
    snd_seq_query_subscribe_alloca(&pAlsaSubs);

    snd_seq_client_info_set_client(cinfo, -1);

    m_refTreeModel->clear();
    while (snd_seq_query_next_client(oseq, cinfo) >= 0) {
	int client = snd_seq_client_info_get_client(cinfo);
	snd_seq_port_info_set_client(info, client);
	snd_seq_port_info_set_port(info, -1);
	while (snd_seq_query_next_port(oseq, info) >= 0) {
	    int caps = snd_seq_port_info_get_capability(info);
	    int type = snd_seq_port_info_get_type(info);

	    if (caps & SND_SEQ_PORT_CAP_NO_EXPORT)
		continue;
	    else if ( !((caps & SND_SEQ_PORT_CAP_WRITE)
		|| (caps & SND_SEQ_PORT_CAP_DUPLEX)))
		continue;
	    else if (type == SND_SEQ_PORT_SYSTEM_TIMER)
		continue;
	    else if (type == SND_SEQ_PORT_SYSTEM_ANNOUNCE)
		continue;

	    Gtk::TreeModel::iterator iter = m_refTreeModel->append();
	    Gtk::TreeModel::Row row = *iter;
	    row[m_Columns.m_col_text] = snd_seq_port_info_get_name(info);
	    row[m_Columns.m_col_client] = snd_seq_port_info_get_client(info);
	    row[m_Columns.m_col_port] = snd_seq_port_info_get_port(info);
	}
    }

    snd_seq_addr_t addr;
    addr.client = snd_seq_client_id(oseq);
    addr.port = (*j)[m_Columns.m_col_port];
    snd_seq_query_subscribe_set_root(pAlsaSubs, &addr);
    snd_seq_query_subscribe_set_index(pAlsaSubs, 0);

    while(!snd_seq_query_port_subscribers(oseq, pAlsaSubs)) {
	const snd_seq_addr_t* connected_addr = snd_seq_query_subscribe_get_addr(pAlsaSubs);

	snd_seq_query_subscribe_set_index(pAlsaSubs, snd_seq_query_subscribe_get_index(pAlsaSubs) + 1);

	Gtk::TreeModel::Children c = m_refTreeModel->children();
	Gtk::TreeModel::Children::iterator iter = c.begin();
	for (; iter != c.end(); ++iter) {
	    if (connected_addr->client == (*iter)[m_Columns.m_col_client]
		&& connected_addr->port == (*iter)[m_Columns.m_col_port]) {
		    (*iter)[m_Columns.m_col_enable] = true;
	    }

	}
    }

    in_refresh = false;
}

void Patch::on_response(int r)
{
    switch (r) {
    case Gtk::RESPONSE_OK:
	break;
    default:
	break;
    }
    hide();
}

std::auto_ptr<Patch> Patch::create()
{
    Patch * p;
    Glib::RefPtr <Gtk::Builder> refXml
	= Gtk::Builder::create_from_file(showq_ui+"patch.ui");
    refXml->get_widget_derived("Patch", p);
    return std::auto_ptr<Patch>(p);
}

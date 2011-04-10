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
#include "main.h"
#include "utils.h"

#include <cctype>
#include <cstdlib>
#include <ios>
#include <sstream>

const struct msc_cmd_fmts {
    int cmd;
    const char *name;
} msc_cmd_fmt[] = {
    {0x01, "Lighting (General)"},
    {0x02, "Moving Lights"},
    {0x03, "Colour Changers"},
    {0x04, "Strobes"},
    {0x05, "Lasers"},
    {0x06, "Chasers"},

    {0x10, "Sound (General)"},
    {0x11, "Music"},
    {0x12, "CD Players"},
    {0x13, "EPROM Playback"},
    {0x14, "Audio Tape Machines"},
    {0x15, "Intercoms"},
    {0x16, "Amplifiers"},
    {0x17, "Audio Effects Devices"},
    {0x18, "Equalisers"},

    {0x20, "Machinery (General)"},
    {0x21, "Rigging"},
    {0x22, "Flys"},
    {0x23, "Lifts"},
    {0x24, "Turntables"},
    {0x25, "Trusses"},
    {0x26, "Robots"},
    {0x27, "Animation"},
    {0x28, "Floats"},
    {0x29, "Breakaways"},
    {0x2A, "Barges"},

    {0x30, "Video (General)"},
    {0x31, "Video Tape Machines"},
    {0x32, "Video Cassette Machines"},
    {0x33, "Video Disc Players"},
    {0x34, "Video Switchers"},
    {0x35, "Video Effects"},
    {0x36, "Video Character Generators"},
    {0x37, "Video Still Stores"},
    {0x38, "Video Monitors"},

    {0x40, "Projection (General)"},
    {0x41, "Film Projectors"},
    {0x42, "Slide Projectors"},
    {0x43, "Video Projectors"},
    {0x44, "Dissolvers"},
    {0x45, "Shutter Controls"},

    {0x50, "Process Control (General)"},
    {0x51, "Hydraulic Oil"},
    {0x52, "H20"},
    {0x53, "CO2"},
    {0x54, "Compressed Air"},
    {0x55, "Natural Gas"},
    {0x56, "Fog"},
    {0x57, "Smoke"},
    {0x58, "Cracked Haze"},

    {0x60, "Pyro (General)"},
    {0x61, "Fireworks"},
    {0x62, "Explosions"},
    {0x63, "Flame"},
    {0x64, "Smoke pots"},

    {0x7F, "All-types"},
};

const struct msc_cmds {
    int cmd;
    const char *name;
} msc_cmd[] = {
    {0x01, "Go"},
    {0x02, "Stop"},
    {0x03, "Resume"},
    {0x04, "Timed Go"},
    {0x05, "Load"},
    {0x06, "Set"},
    {0x07, "Fire"},
    {0x08, "All Off"},
    {0x09, "Restore"},
    {0x0A, "Reset"},
    {0x0B, "Go Off"},
    {0x10, "Go/Jam Clock"},
    {0x11, "Standby +"},
    {0x12, "Standby -"},
    {0x13, "Sequence +"},
    {0x14, "Sequence -"},
    {0x15, "Start Clock"},
    {0x16, "Stop Clock"},
    {0x17, "Zero Clock"},
    {0x18, "Set Clock"},
    {0x19, "MTC Chase On"},
    {0x1A, "MTC Chase Off"},
    {0x1B, "Open Cue List"},
    {0x1C, "Close Cue List"},
    {0x1D, "Open Cue Path"},
    {0x1E, "Close Cue Path"}
};

// MIDI cue editing

EditCueMidi::EditCueMidi(Gtk::Notebook *p)
{
    m_refXml = Gtk::Builder::create_from_file(showq_ui+"editmidi.ui");

    // Set up MIDI view
    connect_clicked(m_refXml, "ed_midi_delete",
	sigc::mem_fun(*this, &EditCueMidi::on_delete_clicked));
    connect_clicked(m_refXml, "ed_midi_add",
	sigc::mem_fun(*this, &EditCueMidi::on_add_clicked));
    connect_clicked(m_refXml, "ed_midi_modify",
	sigc::mem_fun(*this, &EditCueMidi::on_modify_clicked));

    Gtk::ComboBox * p_func;
    m_refXml->get_widget("ed_midi_func", p_func);
    p_func->signal_changed().
	connect(sigc::mem_fun(*this, &EditCueMidi::on_combo_changed));

    Gtk::ComboBox * p_cmd;
    m_refXml->get_widget("ed_midi_msc_cmd", p_cmd);
    p_cmd->signal_changed().
	connect(sigc::mem_fun(*this, &EditCueMidi::on_msc_combo_changed));

    Gtk::Notebook *pnote;
    m_refXml->get_widget("ed_midi_pad1", pnote);
    pnote->set_show_tabs(false);
    m_refXml->get_widget("ed_midi_pad2", pnote);
    pnote->set_show_tabs(false);

    // Set up the MIDI list view
    m_refXml->get_widget("ed_midi_tree", m_MIDItree);
    m_refTreeModel = Gtk::ListStore::create(m_Columns);
    m_MIDItree->set_model(m_refTreeModel);
    m_MIDItree->append_column("Description", m_Columns.m_text);
    m_MIDItree->get_selection()->signal_changed()
	.connect(sigc::mem_fun(*this, &EditCueMidi::on_selection_change));

    on_selection_change();

    Gtk::VBox *vbox;
    m_refXml->get_widget("edit_midi_vbox", vbox);
    p->append_page(*vbox, "MIDI");

    // Set up the MSC
    Gtk::Entry * p_msc_num;
    m_refXml->get_widget("ed_midi_msc_qnum", p_msc_num);
    p_msc_num->signal_insert_text()
	.connect(sigc::bind<Gtk::Entry *>(sigc::mem_fun(*this, &EditCueMidi::validate_qlist), p_msc_num), false);
    Gtk::Entry * p_msc_list;
    m_refXml->get_widget("ed_midi_msc_qlist", p_msc_list);
    p_msc_list->signal_insert_text()
	.connect(sigc::bind<Gtk::Entry *>(sigc::mem_fun(*this, &EditCueMidi::validate_qlist), p_msc_list), false);
    Gtk::Entry * p_msc_path;
    m_refXml->get_widget("ed_midi_msc_qpath", p_msc_path);
    p_msc_path->signal_insert_text()
	.connect(sigc::bind<Gtk::Entry *>(sigc::mem_fun(*this, &EditCueMidi::validate_qlist), p_msc_path), false);

    p_msc_num->signal_changed().connect(sigc::mem_fun(*this, &EditCueMidi::validate_fields));
    p_msc_list->signal_changed().connect(sigc::mem_fun(*this, &EditCueMidi::validate_fields));
    p_msc_path->signal_changed().connect(sigc::mem_fun(*this, &EditCueMidi::validate_fields));

    // Set up SysEx
    Gtk::Entry * p_sysex;
    m_refXml->get_widget("ed_midi_sysex", p_sysex);
    p_sysex->signal_insert_text()
        .connect(sigc::bind<Gtk::Entry *>(sigc::mem_fun(*this, &EditCueMidi::validate_sysex_entry)
        , p_sysex), false);
}

EditCueMidi::~EditCueMidi()
{
}

void EditCueMidi::set(boost::shared_ptr<Cue> & q)
{
    boost::shared_ptr<MIDI_Cue> pm = boost::dynamic_pointer_cast<MIDI_Cue>(q);

    std::vector<MIDI_Cue::msg>::const_iterator k = pm->msgs.begin();
    for (; k != pm->msgs.end(); k++) {
	Gtk::TreeModel::Row row = *(m_refTreeModel->append());
	row[m_Columns.m_text] = get_line(k->midi_data);
	row[m_Columns.m_port] = k->port;
	row[m_Columns.m_raw_data] = k->midi_data;
    }
}

void EditCueMidi::get(boost::shared_ptr<Cue> & q)
{
    boost::shared_ptr<MIDI_Cue> pm = boost::dynamic_pointer_cast<MIDI_Cue>(q);
    Gtk::TreeModel::Children children = m_refTreeModel->children();
    Gtk::TreeModel::Children::iterator iter = children.begin();

    for (; iter != children.end(); ++iter) {
	Gtk::TreeModel::Row row = *iter;
	MIDI_Cue::msg m;
	m.port = row[m_Columns.m_port];
	m.midi_data = row[m_Columns.m_raw_data];
	pm->msgs.push_back(m);
    }
}

Glib::ustring EditCueMidi::get_line(const std::vector<unsigned char> & raw)
{
    std::ostringstream line;

    switch (raw[0] & 0xF0) {
    case 0x80:
	line << "Note on channel " << (raw[0] & 0x0f) + 1
	    << " note " << int(raw[1]) << " velocity " << int(raw[2]);
	break;
    case 0x90:
	line << "Note off channel " << (raw[0] & 0x0f) + 1
	    << " note " << int(raw[1]) << " velocity " << int(raw[2]);
	break;
    case 0xa0:
	line << "Aftertouch channel " << (raw[0] & 0x0f) + 1
	    << " note " << int(raw[1]) << " value " << int(raw[2]);
	break;
    case 0xb0:
	line << "Controller change - Channel " << (raw[0] & 0x0f) + 1
	    << " control number " << int(raw[1]) << " value " << int(raw[2]);
	break;
    case 0xc0:
	line << "Program change channel " << (raw[0] & 0x0f) + 1
	    << " program " << int(raw[1]);
	break;
    case 0xd0:
	line << "Channel aftertouch channel " << (raw[0] & 0x0f) + 1
	    << " pressure " << int(raw[1]);
	break;
    case 0xe0:
	line << "Pitch change channel " << (raw[0] & 0x0f) + 1
	    << " LSB " << int(raw[1]) << " MSB " << int(raw[2]);
	break;
    case 0xf0:
	if (raw[0] == 0xf0 && raw[1] == 0x7f && raw[3] == 0x02) {
	    line << "MSC - Device " << int(raw[2]);

	    for (size_t i = 0; i < (sizeof(msc_cmd_fmt) / sizeof(struct msc_cmd_fmts)); ++i) {
		if (msc_cmd_fmt[i].cmd == raw[4]) {
		    line << " " << msc_cmd_fmt[i].name;
		    break;
		}
	    }
	    for (size_t i = 0; i < (sizeof(msc_cmd) / sizeof(struct msc_cmds)); ++i) {
		if (msc_cmd[i].cmd == raw[5]) {
		    line << " command "<< msc_cmd[i].name;
		    break;
		}
	    }

	    break;
	}
	line << "SYSEX";
	break;
    }

    return line.str();
}

void EditCueMidi::on_selection_change()
{
    Gtk::TreeModel::iterator iter = m_MIDItree->get_selection()->get_selected();
    Gtk::Button * p_midi_remove;
    Gtk::Button * p_midi_modify;
    m_refXml->get_widget("ed_midi_delete", p_midi_remove);
    m_refXml->get_widget("ed_midi_modify", p_midi_modify);
    if (!iter) {
	p_midi_remove->set_sensitive(false);
	p_midi_modify->set_sensitive(false);
	return;
    }
    p_midi_remove->set_sensitive();
    p_midi_modify->set_sensitive();

    Gtk::TreeModel::Row row = *iter;

    Gtk::ComboBox * p_midi_patch;
    m_refXml->get_widget("ed_midi_patch", p_midi_patch);

    p_midi_patch->set_active(row[m_Columns.m_port] - 1);

    std::vector<unsigned char> raw = row[m_Columns.m_raw_data];

    Gtk::ComboBox * p_midi_func;
    m_refXml->get_widget("ed_midi_func", p_midi_func);

    if (raw[0] >= 0xf0 && raw[1] == 0x7f && raw[3] == 0x02) {
	p_midi_func->set_active(7);
	Gtk::SpinButton * p_msc_dev;
	Gtk::ComboBox * p_msc_cmd_fmt;
	Gtk::ComboBox * p_msc_cmd;

	m_refXml->get_widget("ed_midi_msc_dev", p_msc_dev);
	m_refXml->get_widget("ed_midi_msc_cmd_fmt", p_msc_cmd_fmt);
	m_refXml->get_widget("ed_midi_msc_cmd", p_msc_cmd);

	p_msc_dev->set_value(raw[2]);
	for (size_t i = 0; i < (sizeof(msc_cmd_fmt) / sizeof(struct msc_cmd_fmts)); ++i) {
	    if (msc_cmd_fmt[i].cmd == raw[4]) {
		p_msc_cmd_fmt->set_active(i);
		break;
	    }
	}
	for (size_t i = 0; i < (sizeof(msc_cmd) / sizeof(struct msc_cmds)); ++i) {
	    if (msc_cmd[i].cmd == raw[5]) {
		p_msc_cmd->set_active(i);
		break;
	    }
	}

	Gtk::SpinButton * p_msc_macro;
	Gtk::SpinButton * p_msc_cnum;
	Gtk::SpinButton * p_msc_cval;
	Gtk::Entry * p_msc_qnum;
	Gtk::Entry * p_msc_qlist;
	Gtk::Entry * p_msc_qpath;

	m_refXml->get_widget("ed_midi_msc_macro", p_msc_macro);
	m_refXml->get_widget("ed_midi_msc_cnum", p_msc_cnum);
	m_refXml->get_widget("ed_midi_msc_cval", p_msc_cval);
	m_refXml->get_widget("ed_midi_msc_qnum", p_msc_qnum);
	m_refXml->get_widget("ed_midi_msc_qlist", p_msc_qlist);
	m_refXml->get_widget("ed_midi_msc_qpath", p_msc_qpath);

	std::vector<unsigned char>::const_iterator r = raw.begin() + 6;
	switch (raw[5]) {
	case 0x04:
	    set_msc_timecode(r);
	    r += 5;
	case 0x01:
	case 0x02:
	case 0x03:
	case 0x05:
	case 0x0b:
	case 0x10:
	    {
		Glib::ustring s;
		for (;*r && *r != 0xf7; ++r)
		    s += char(*r);
		p_msc_qnum->set_text(s);
		if (*r != 0) {
		    p_msc_qlist->set_text("");
		    p_msc_qpath->set_text("");
		    break;
		}

		for (s = "", ++r; *r && *r != 0xf7; ++r)
		    s += char(*r);
		p_msc_qlist->set_text(s);
		if (*r != 0) {
		    p_msc_qpath->set_text("");
		    break;
		}

		for (s = "", ++r; *r && *r != 0xf7; ++r)
		    s += char(*r);
		p_msc_qpath->set_text(s);
	    }
	    break;
	case 0x18:
	    set_msc_timecode(r);
	    r += 5;
	case 0x11:
	case 0x12:
	case 0x13:
	case 0x14:
	case 0x15:
	case 0x16:
	case 0x17:
	case 0x19:
	case 0x1a:
	case 0x1b:
	case 0x1c:
	    {
		Glib::ustring s;
		for (; *r && *r != 0xf7; ++r)
		    s += char(*r);
		p_msc_qlist->set_text(s);
		p_msc_qnum->set_text("");
		p_msc_qpath->set_text("");
	    }
	    break;
	case 0x1d:
	case 0x1e:
	    {
		Glib::ustring s;
		for (; *r && *r != 0xf7; ++r)
		    s += char(*r);
		p_msc_qpath->set_text(s);
		p_msc_qnum->set_text("");
		p_msc_qlist->set_text("");
	    }
	    break;
	case 0x06:
	    p_msc_cnum->set_value(raw[6] + raw[7] * 128);
	    p_msc_cval->set_value(raw[8] + raw[9] * 128);
	    set_msc_timecode(r + 4);
	    break;
	case 0x07:
	    p_msc_macro->set_value(raw[6]);
	    break;
	}
    } else if (raw[0] >= 0xf0) {
        p_midi_func->set_active(8);
        Gtk::Entry * p_sysex;
        m_refXml->get_widget("ed_midi_sysex", p_sysex);
        std::ostringstream s;
        s.setf(std::ios::hex, std::ios::basefield);
        for (size_t i = 1;i < (raw.size() - 1); ++i) {
            s << int(raw[i]) << ' ';
        }
        p_sysex->set_text(s.str());
    } else {
	Gtk::SpinButton * p_midi_channel;
	Gtk::SpinButton * p_midi_data1;
	Gtk::SpinButton * p_midi_data2;

	m_refXml->get_widget("ed_midi_channel", p_midi_channel);
	m_refXml->get_widget("ed_midi_data1", p_midi_data1);
	m_refXml->get_widget("ed_midi_data2", p_midi_data2);

	p_midi_channel->set_value((raw[0] & 0x0f) + 1);
	switch (raw[0] & 0xf0) {
	case 0x80:
	    p_midi_func->set_active(0);
	    p_midi_data1->set_value(raw[1]);
	    p_midi_data2->set_value(raw[2]);
	    break;
	case 0x90:
	    p_midi_func->set_active(1);
	    p_midi_data1->set_value(raw[1]);
	    p_midi_data2->set_value(raw[2]);
	    break;
	case 0xa0:
	    p_midi_func->set_active(4);
	    p_midi_data1->set_value(raw[1]);
	    p_midi_data2->set_value(raw[2]);
	    break;
	case 0xb0:
	    p_midi_func->set_active(3);
	    p_midi_data1->set_value(raw[1]);
	    p_midi_data2->set_value(raw[2]);
	    break;
	case 0xc0:
	    p_midi_func->set_active(2);
	    p_midi_data1->set_value(raw[1]);
	    break;
	case 0xd0:
	    p_midi_func->set_active(5);
	    p_midi_data1->set_value(raw[1]);
	    break;
	case 0xe0:
	    p_midi_func->set_active(6);
	    p_midi_data1->set_value(raw[1]);
	    p_midi_data2->set_value(raw[2]);
	    break;
	}
    }
}

std::vector<unsigned char> EditCueMidi::get_raw()
{
    Gtk::ComboBox * p_func;
    m_refXml->get_widget("ed_midi_func", p_func);

    Gtk::Label * p_label_data1;
    m_refXml->get_widget("ed_midi_label1", p_label_data1);

    Gtk::Label * p_label_data2;
    m_refXml->get_widget("ed_midi_label2", p_label_data2);

    Gtk::SpinButton * p_channel;
    m_refXml->get_widget("ed_midi_channel", p_channel);

    Gtk::SpinButton * p_data1;
    m_refXml->get_widget("ed_midi_data1", p_data1);
    p_data1->show();

    Gtk::SpinButton * p_data2;
    m_refXml->get_widget("ed_midi_data2", p_data2);

    Gtk::SpinButton * p_msc_dev;
    m_refXml->get_widget("ed_midi_msc_dev", p_msc_dev);

    Gtk::ComboBox * p_msc_cmd_fmt;
    m_refXml->get_widget("ed_midi_msc_cmd_fmt", p_msc_cmd_fmt);

    Gtk::ComboBox * p_msc_cmd;
    m_refXml->get_widget("ed_midi_msc_cmd", p_msc_cmd);

    Gtk::SpinButton * p_msc_macro;
    m_refXml->get_widget("ed_midi_msc_macro", p_msc_macro);

    Gtk::Entry * p_msc_num;
    m_refXml->get_widget("ed_midi_msc_qnum", p_msc_num);

    Gtk::Entry * p_msc_list;
    m_refXml->get_widget("ed_midi_msc_qlist", p_msc_list);

    Gtk::Entry * p_msc_path;
    m_refXml->get_widget("ed_midi_msc_qpath", p_msc_path);

    Gtk::SpinButton * p_msc_cnum;
    m_refXml->get_widget("ed_midi_msc_cnum", p_msc_cnum);

    Gtk::SpinButton * p_msc_cval;
    m_refXml->get_widget("ed_midi_msc_cval", p_msc_cval);

    unsigned int status, data1, data2;
    status = p_channel->get_value_as_int() - 1;
    data1 = p_data1->get_value_as_int();
    data2 = p_data2->get_value_as_int();

    std::vector<unsigned char> raw;
    Glib::ustring tstring;

    switch (p_func->get_active_row_number()) {
    case 0:
	raw.push_back(status | 0x90);
	raw.push_back(data1);
	raw.push_back(data2);
	break;
    case 1:
	raw.push_back(status | 0x80);
	raw.push_back(data1);
	raw.push_back(data2);
	break;
    case 2:
	raw.push_back(status | 0xc0);
	raw.push_back(data1);
	break;
    case 3:
	raw.push_back(status | 0xb0);
	raw.push_back(data1);
	raw.push_back(data2);
	break;
    case 4:
	raw.push_back(status | 0xa0);
	raw.push_back(data1);
	raw.push_back(data2);
	break;
    case 5:
	raw.push_back(status | 0xd0);
	raw.push_back(data1);
	break;
    case 6:
	raw.push_back(status | 0xe0);
	raw.push_back(data1);
	raw.push_back(data2);
	break;
    case 7:
	raw.push_back(0xf0);
	raw.push_back(0x7f);
	raw.push_back(p_msc_dev->get_value_as_int());
	raw.push_back(0x02);
	raw.push_back(msc_cmd_fmt[p_msc_cmd_fmt->get_active_row_number()].cmd);
	raw.push_back(msc_cmd[p_msc_cmd->get_active_row_number()].cmd);
	switch (p_msc_cmd->get_active_row_number()) {
	case 3:		// TIMED_GO
	    build_msc_timecode(raw);
	case 0:		// GO
	case 1:		// STOP
	case 2:		// RESUME
	case 4:		// LOAD - q number required
	case 10:	// GO_OFF
	case 11:	// GO/JAM_CLOCK
	    tstring = p_msc_num->get_text();
	    {
		Glib::ustring::iterator p = tstring.begin();
		for (; p != tstring.end(); ++p)
		    raw.push_back(*p);
	    }
	    tstring = p_msc_list->get_text();
	    if (tstring == "") break;
	    raw.push_back('\0');
	    {
		Glib::ustring::iterator p = tstring.begin();
		for (; p != tstring.end(); ++p)
		    raw.push_back(*p);
	    }
	    tstring = p_msc_path->get_text();
	    if (tstring == "") break;
	    raw.push_back('\0');
	    {
		Glib::ustring::iterator p = tstring.begin();
		for (; p != tstring.end(); ++p)
		    raw.push_back(*p);
	    }
	    break;
	case 5:		// SET
	    {
		int c = p_msc_cnum->get_value_as_int();
		int v = p_msc_cval->get_value_as_int();
		raw.push_back(c % 128);
		raw.push_back(c / 128);
		raw.push_back(v % 128);
		raw.push_back(v / 128);
		build_msc_timecode(raw);
	    }
	    break;
	case 6:		// FIRE
	    raw.push_back(p_msc_macro->get_value_as_int());
	    break;
	case 19:	// SET_CLOCK
	    build_msc_timecode(raw);
	case 12:	// STANDBY_+
	case 13:	// STANDBY_-
	case 14:	// SEQUENCE_+
	case 15:	// SEQUENCE_-
	case 16:	// START_CLOCK
	case 17:	// STOP_CLOCK
	case 18:	// ZERO_CLOCK
	case 20:	// MTC_CHASE_ON
	case 21:	// MTC_CHASE_OFF
	case 22:	// OPEN_CUE_LIST - list required
	case 23:	// CLOSE_CUE_LIST - list required
	    tstring = p_msc_list->get_text();
	    {
		Glib::ustring::iterator p = tstring.begin();
		for (; p != tstring.end(); ++p)
		    raw.push_back(*p);
	    }
	    break;
	case 24:	// OPEN_CUE_PATH
	case 25:	// CLOSE_CUE_PATH
	    tstring = p_msc_path->get_text();
	    {
		Glib::ustring::iterator p = tstring.begin();
		for (; p != tstring.end(); ++p)
		    raw.push_back(*p);
	    }
	    break;
	default:
	    break;
	}
	raw.push_back(0xf7);
	break;
    case 8:
        {
            Gtk::Entry * p_sysex;
            m_refXml->get_widget("ed_midi_sysex", p_sysex);
            raw.push_back(0xf0);
            //TODO read in the hexadecimal line
            Glib::ustring s = p_sysex->get_text();
            Glib::ustring val;
            size_t i = 0;
            for (; i < s.size(); ++i) {
                if (isspace(s[i])) continue;
                val = s[i];
                ++i;
                if (isspace(s[i])) {
                    raw.push_back(strtol(val.c_str(), 0, 16));
                    continue;
                }
                ++i;
                val += s[i];
                raw.push_back(strtol(val.c_str(), 0, 16));
            }
            raw.push_back(0xf7);
        }
        break;
    default:
	return raw;
    }
    return raw;
}

void EditCueMidi::on_modify_clicked()
{
    Gtk::TreeModel::iterator iter =
	m_MIDItree->get_selection()->get_selected();
    if (!iter) return;
    Gtk::TreeModel::Row row = (*iter);

    Gtk::ComboBox * p_midi_patch;
    m_refXml->get_widget("ed_midi_patch", p_midi_patch);

    std::vector<unsigned char> raw = get_raw();
    row[m_Columns.m_text] = get_line(raw);
    row[m_Columns.m_raw_data] = raw;
    row[m_Columns.m_port] = p_midi_patch->get_active_row_number() + 1;
}

void EditCueMidi::on_add_clicked()
{
    Gtk::TreeModel::Row row = *(m_refTreeModel->append());

    Gtk::ComboBox * p_midi_patch;
    m_refXml->get_widget("ed_midi_patch", p_midi_patch);

    std::vector<unsigned char> raw = get_raw();
    row[m_Columns.m_text] = get_line(raw);
    row[m_Columns.m_raw_data] = raw;
    row[m_Columns.m_port] = p_midi_patch->get_active_row_number() + 1;
}

void EditCueMidi::set_msc_timecode(std::vector<unsigned char>::const_iterator raw)
{
    Gtk::SpinButton * p_msc_hour;
    Gtk::SpinButton * p_msc_minute;
    Gtk::SpinButton * p_msc_second;
    Gtk::SpinButton * p_msc_frame;
    Gtk::SpinButton * p_msc_subframe;
    Gtk::RadioButton * p_msc_24;
    Gtk::RadioButton * p_msc_25;
    Gtk::RadioButton * p_msc_30;
    Gtk::RadioButton * p_msc_30df;

    m_refXml->get_widget("ed_midi_msc_hour", p_msc_hour);
    m_refXml->get_widget("ed_midi_msc_minute", p_msc_minute);
    m_refXml->get_widget("ed_midi_msc_second", p_msc_second);
    m_refXml->get_widget("ed_midi_msc_frame", p_msc_frame);
    m_refXml->get_widget("ed_midi_msc_sframe", p_msc_subframe);
    m_refXml->get_widget("ed_midi_msc_24", p_msc_24);
    m_refXml->get_widget("ed_midi_msc_25", p_msc_25);
    m_refXml->get_widget("ed_midi_msc_30", p_msc_30);
    m_refXml->get_widget("ed_midi_msc_30df", p_msc_30df);

    switch (raw[0] & 0x60) {
    case 0x00:
	p_msc_24->set_active();
	break;
    case 0x20:
	p_msc_25->set_active();
	break;
    case 0x40:
	p_msc_30->set_active();
	break;
    case 0x60:
	p_msc_30df->set_active();
	break;
    }
    p_msc_hour->set_value(raw[0] & 0x1f);
    p_msc_minute->set_value(raw[1] & 0x3f);
    p_msc_second->set_value(raw[2] & 0x3f);
    p_msc_frame->set_value(raw[3] & 0x1f);
    p_msc_subframe->set_value(raw[4]);
}

void EditCueMidi::build_msc_timecode(std::vector<unsigned char> & raw)
{
    Gtk::SpinButton * p_msc_hour;
    Gtk::SpinButton * p_msc_minute;
    Gtk::SpinButton * p_msc_second;
    Gtk::SpinButton * p_msc_frame;
    Gtk::SpinButton * p_msc_subframe;
    Gtk::RadioButton * p_msc_24;
    Gtk::RadioButton * p_msc_25;
    Gtk::RadioButton * p_msc_30;
    Gtk::RadioButton * p_msc_30df;

    m_refXml->get_widget("ed_midi_msc_hour", p_msc_hour);
    m_refXml->get_widget("ed_midi_msc_minute", p_msc_minute);
    m_refXml->get_widget("ed_midi_msc_second", p_msc_second);
    m_refXml->get_widget("ed_midi_msc_frame", p_msc_frame);
    m_refXml->get_widget("ed_midi_msc_sframe", p_msc_subframe);
    m_refXml->get_widget("ed_midi_msc_24", p_msc_24);
    m_refXml->get_widget("ed_midi_msc_25", p_msc_25);
    m_refXml->get_widget("ed_midi_msc_30", p_msc_30);
    m_refXml->get_widget("ed_midi_msc_30df", p_msc_30df);

    int hour = p_msc_hour->get_value_as_int();
    if (p_msc_25->get_active())
	hour |= 0x20;
    else if (p_msc_30->get_active())
	hour |= 0x40;
    else if (p_msc_30df->get_active())
	hour |= 0x60;
    raw.push_back(hour);

    raw.push_back(p_msc_minute->get_value_as_int());
    raw.push_back(p_msc_second->get_value_as_int());
    raw.push_back(p_msc_frame->get_value_as_int());
    raw.push_back(p_msc_subframe->get_value_as_int());
}

void EditCueMidi::on_delete_clicked()
{
    Gtk::TreeModel::iterator iter =
	m_MIDItree->get_selection()->get_selected();

    if (iter)
	m_refTreeModel->erase(iter);
}

void EditCueMidi::on_combo_changed()
{
    Gtk::ComboBox * p_func;
    m_refXml->get_widget("ed_midi_func", p_func);

    Gtk::Label * p_label_data1;
    m_refXml->get_widget("ed_midi_label1", p_label_data1);

    Gtk::Label * p_label_data2;
    m_refXml->get_widget("ed_midi_label2", p_label_data2);

    Gtk::SpinButton * p_data1;
    m_refXml->get_widget("ed_midi_data1", p_data1);
    p_data1->show();

    Gtk::SpinButton * p_data2;
    m_refXml->get_widget("ed_midi_data2", p_data2);

    Gtk::Notebook * p_pad1;
    m_refXml->get_widget("ed_midi_pad1", p_pad1);

    int page1 = 0;
    switch (p_func->get_active_row_number()) {
    case 0:
    case 1:
	p_label_data1->set_text("Note : ");
	p_label_data2->set_text("Velocity : ");
	p_data2->show();
	break;
    case 2:
	p_label_data1->set_text("Program number : ");
	p_label_data2->set_text("");
	p_data2->hide();
	break;
    case 3:
	p_label_data1->set_text("Controller : ");
	p_label_data2->set_text("Value : ");
	p_data2->show();
	break;
    case 4:
	p_label_data1->set_text("Note : ");
	p_label_data2->set_text("Value : ");
	p_data2->show();
	break;
    case 5:
	p_label_data1->set_text("Value : ");
	p_label_data2->set_text("");
	p_data2->hide();
	break;
    case 6:
	p_label_data1->set_text("LSB : ");
	p_label_data2->set_text("MSB : ");
	p_data2->show();
	break;
    case 7:
	page1 = 1;
	break;
    case 8:
        page1 = 2;
    default:
	p_label_data1->set_text("");
	p_label_data2->set_text("");
	break;
    }

    p_pad1->set_current_page(page1);
}

void EditCueMidi::on_msc_combo_changed()
{
    Gtk::ComboBox * p_func;
    m_refXml->get_widget("ed_midi_msc_cmd", p_func);

    Gtk::Notebook * p_pad2;
    m_refXml->get_widget("ed_midi_pad2", p_pad2);

    switch (p_func->get_active_row_number()) {
    case 5:
	p_pad2->set_current_page(3);
	break;
    case 6:
	p_pad2->set_current_page(2);
	break;
    case -1:
    case 7:
    case 8:
    case 9:
	p_pad2->set_current_page(0);
	break;
    default:
	p_pad2->set_current_page(1);
    }

    Gtk::Frame * p_time;
    Gtk::Frame * p_number;
    Gtk::Frame * p_list;
    Gtk::Frame * p_path;

    m_refXml->get_widget("ed_midi_msc_tframe", p_time);
    m_refXml->get_widget("ed_midi_msc_nframe", p_number);
    m_refXml->get_widget("ed_midi_msc_lframe", p_list);
    m_refXml->get_widget("ed_midi_msc_pframe", p_path);

    switch (p_func->get_active_row_number()) {
    case 3:
    case 5:
    case 19:
	p_time->show();
	break;
    default:
	p_time->hide();
    }
    switch (p_func->get_active_row_number()) {
    case -1:
	break;
    case 11:	// go/jam clock
    case 12:	// standby +
    case 13:	// standby -
    case 14:	// sequence +
    case 15:	// sequence -
    case 16:	// start clock
    case 17:	// stop clock
    case 18:	// zero clock
    case 19:	// set clock
    case 20:	// MTC chase on
    case 21:	// MTC chase off
    case 22:	// open cue list
    case 23:	// close cue list
	p_number->hide();
	p_list->show();
	p_path->hide();
	break;
    case 24:	// open cue path
    case 25:	// close cue path
	p_number->hide();
	p_list->hide();
	p_path->show();
	break;
    default:
	p_number->show();
	p_list->show();
	p_path->show();
	break;
    }
    validate_fields();
}

void EditCueMidi::validate_qlist(const Glib::ustring & text, int *, Gtk::Entry *e)
{
    for (Glib::ustring::const_iterator p = text.begin(); p != text.end(); ++p) {
	if (isdigit(*p) || *p == '.') continue;
	e->signal_insert_text().emission_stop();
    }
}

void EditCueMidi::validate_fields()
{
    Gtk::Entry * p_msc_qnum;
    Gtk::Entry * p_msc_qlist;
    Gtk::Entry * p_msc_qpath;
    Gtk::Frame * p_msc_nframe;
    Gtk::Frame * p_msc_lframe;
    Gtk::Frame * p_msc_pframe;

    m_refXml->get_widget("ed_midi_msc_qnum", p_msc_qnum);
    m_refXml->get_widget("ed_midi_msc_qlist", p_msc_qlist);
    m_refXml->get_widget("ed_midi_msc_qpath", p_msc_qpath);

    m_refXml->get_widget("ed_midi_msc_nframe", p_msc_nframe);
    m_refXml->get_widget("ed_midi_msc_lframe", p_msc_lframe);
    m_refXml->get_widget("ed_midi_msc_pframe", p_msc_pframe);

    if (p_msc_nframe->is_visible()) {
	if (p_msc_qnum->get_text_length() == 0) {
	    p_msc_qlist->set_sensitive(false);
	    p_msc_qpath->set_sensitive(false);
	    return;
	}
    }
    p_msc_qlist->set_sensitive();
    if (p_msc_lframe->is_visible()) {
	if (p_msc_qlist->get_text_length() == 0) {
	    p_msc_qpath->set_sensitive(false);
	    return;
	}
    }
    p_msc_qpath->set_sensitive();
}

void EditCueMidi::validate_sysex_entry(const Glib::ustring & text, int *, Gtk::Entry *e)
{
    for (Glib::ustring::const_iterator p = text.begin(); p != text.end(); ++p) {
	if (isxdigit(*p) || isspace(*p)) continue;
	e->signal_insert_text().emission_stop();
    }
}

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

#include "cue.h"
#include "editcue.h"
#include "utils.h"

#include <glib/gi18n.h>
#include <gtkmm.h>

#include <cstdlib>
#include <ios>
#include <memory>
#include <sstream>
#include <vector>

const struct msc_cmd_fmts
{
    int cmd;
    const char *name;
} msc_cmd_fmt[] = {
    {0x01, N_("Lighting (General)")},
    {0x02, N_("Moving Lights")},
    {0x03, N_("Colour Changers")},
    {0x04, N_("Strobes")},
    {0x05, N_("Lasers")},
    {0x06, N_("Chasers")},

    {0x10, N_("Sound (General)")},
    {0x11, N_("Music")},
    {0x12, N_("CD Players")},
    {0x13, N_("EPROM Playback")},
    {0x14, N_("Audio Tape Machines")},
    {0x15, N_("Intercoms")},
    {0x16, N_("Amplifiers")},
    {0x17, N_("Audio Effects Devices")},
    {0x18, N_("Equalisers")},

    {0x20, N_("Machinery (General)")},
    {0x21, N_("Rigging")},
    {0x22, N_("Flys")},
    {0x23, N_("Lifts")},
    {0x24, N_("Turntables")},
    {0x25, N_("Trusses")},
    {0x26, N_("Robots")},
    {0x27, N_("Animation")},
    {0x28, N_("Floats")},
    {0x29, N_("Breakaways")},
    {0x2A, N_("Barges")},

    {0x30, N_("Video (General)")},
    {0x31, N_("Video Tape Machines")},
    {0x32, N_("Video Cassette Machines")},
    {0x33, N_("Video Disc Players")},
    {0x34, N_("Video Switchers")},
    {0x35, N_("Video Effects")},
    {0x36, N_("Video Character Generators")},
    {0x37, N_("Video Still Stores")},
    {0x38, N_("Video Monitors")},

    {0x40, N_("Projection (General)")},
    {0x41, N_("Film Projectors")},
    {0x42, N_("Slide Projectors")},
    {0x43, N_("Video Projectors")},
    {0x44, N_("Dissolvers")},
    {0x45, N_("Shutter Controls")},

    {0x50, N_("Process Control (General)")},
    {0x51, N_("Hydraulic Oil")},
    {0x52, N_("H20")},
    {0x53, N_("CO2")},
    {0x54, N_("Compressed Air")},
    {0x55, N_("Natural Gas")},
    {0x56, N_("Fog")},
    {0x57, N_("Smoke")},
    {0x58, N_("Cracked Haze")},

    {0x60, N_("Pyro (General)")},
    {0x61, N_("Fireworks")},
    {0x62, N_("Explosions")},
    {0x63, N_("Flame")},
    {0x64, N_("Smoke pots")},

    {0x7F, N_("All-types")},
};

const struct msc_cmds
{
    int cmd;
    const char *name;
} msc_cmd[] = {{0x01, N_("Go")},
               {0x02, N_("Stop")},
               {0x03, N_("Resume")},
               {0x04, N_("Timed Go")},
               {0x05, N_("Load")},
               {0x06, N_("Set")},
               {0x07, N_("Fire")},
               {0x08, N_("All Off")},
               {0x09, N_("Restore")},
               {0x0A, N_("Reset")},
               {0x0B, N_("Go Off")},
               {0x10, N_("Go/Jam Clock")},
               {0x11, N_("Standby +")},
               {0x12, N_("Standby -")},
               {0x13, N_("Sequence +")},
               {0x14, N_("Sequence -")},
               {0x15, N_("Start Clock")},
               {0x16, N_("Stop Clock")},
               {0x17, N_("Zero Clock")},
               {0x18, N_("Set Clock")},
               {0x19, N_("MTC Chase On")},
               {0x1A, N_("MTC Chase Off")},
               {0x1B, N_("Open Cue List")},
               {0x1C, N_("Close Cue List")},
               {0x1D, N_("Open Cue Path")},
               {0x1E, N_("Close Cue Path")}};

// MIDI cue editing

EditCueMidi::EditCueMidi(Gtk::Notebook *p)
    : m_refXml(Gtk::Builder::create_from_resource("/org/evandel/showq/ui/editmidi.ui"))
{
    // Set up MIDI view
    connect_clicked(
        m_refXml, "ed_midi_delete", sigc::mem_fun(*this, &EditCueMidi::on_delete_clicked));
    connect_clicked(m_refXml, "ed_midi_add", sigc::mem_fun(*this, &EditCueMidi::on_add_clicked));
    connect_clicked(
        m_refXml, "ed_midi_modify", sigc::mem_fun(*this, &EditCueMidi::on_modify_clicked));

    Gtk::ComboBox *p_func;
    m_refXml->get_widget("ed_midi_func", p_func);
    p_func->signal_changed().connect(sigc::mem_fun(*this, &EditCueMidi::on_combo_changed));

    Gtk::ComboBox *p_cmd;
    m_refXml->get_widget("ed_midi_msc_cmd", p_cmd);
    p_cmd->signal_changed().connect(sigc::mem_fun(*this, &EditCueMidi::on_msc_combo_changed));

    Gtk::Notebook *p_note;
    m_refXml->get_widget("ed_midi_pad1", p_note);
    p_note->set_show_tabs(false);
    m_refXml->get_widget("ed_midi_pad2", p_note);
    p_note->set_show_tabs(false);

    // Set up the MIDI list view
    m_refXml->get_widget("ed_midi_tree", m_MIDItree);
    m_refTreeModel = Gtk::ListStore::create(m_Columns);
    m_MIDItree->set_model(m_refTreeModel);
    m_MIDItree->append_column("Description", m_Columns.m_text);
    m_MIDItree->get_selection()->signal_changed().connect(
        sigc::mem_fun(*this, &EditCueMidi::on_selection_change));

    on_selection_change();

    Gtk::Widget *vbox;
    m_refXml->get_widget("edit_midi_vbox", vbox);
    p->append_page(*vbox, "MIDI");

    // Set up the MSC
    Gtk::Entry *p_msc_num;
    m_refXml->get_widget("ed_midi_msc_qnum", p_msc_num);
    p_msc_num->signal_insert_text().connect(
        sigc::bind<Gtk::Entry *>(sigc::mem_fun(*this, &EditCueMidi::validate_qlist), p_msc_num),
        false);
    Gtk::Entry *p_msc_list;
    m_refXml->get_widget("ed_midi_msc_qlist", p_msc_list);
    p_msc_list->signal_insert_text().connect(
        sigc::bind<Gtk::Entry *>(sigc::mem_fun(*this, &EditCueMidi::validate_qlist), p_msc_list),
        false);
    Gtk::Entry *p_msc_path;
    m_refXml->get_widget("ed_midi_msc_qpath", p_msc_path);
    p_msc_path->signal_insert_text().connect(
        sigc::bind<Gtk::Entry *>(sigc::mem_fun(*this, &EditCueMidi::validate_qlist), p_msc_path),
        false);

    p_msc_num->signal_changed().connect(sigc::mem_fun(*this, &EditCueMidi::validate_fields));
    p_msc_list->signal_changed().connect(sigc::mem_fun(*this, &EditCueMidi::validate_fields));
    p_msc_path->signal_changed().connect(sigc::mem_fun(*this, &EditCueMidi::validate_fields));

    // Set up SysEx
    Gtk::Entry *p_sysex;
    m_refXml->get_widget("ed_midi_sysex", p_sysex);
    p_sysex->signal_insert_text().connect(
        sigc::bind<Gtk::Entry *>(sigc::mem_fun(*this, &EditCueMidi::validate_sysex_entry), p_sysex),
        false);
}

EditCueMidi::~EditCueMidi() = default;

void EditCueMidi::set(std::shared_ptr<Cue> &q)
{
    const std::shared_ptr<MIDI_Cue> pm = std::dynamic_pointer_cast<MIDI_Cue>(q);

    for (auto &msg : pm->msgs) {
        const Gtk::TreeModel::Row row = *(m_refTreeModel->append());
        row[m_Columns.m_text] = get_line(msg.midi_data);
        row[m_Columns.m_port] = msg.port;
        row[m_Columns.m_raw_data] = msg.midi_data;
    }
}

void EditCueMidi::get(std::shared_ptr<Cue> &q)
{
    const std::shared_ptr<MIDI_Cue> pm = std::dynamic_pointer_cast<MIDI_Cue>(q);
    Gtk::TreeModel::Children children = m_refTreeModel->children();
    Gtk::TreeModel::Children::iterator iter = children.begin();

    for (; iter != children.end(); ++iter) {
        const Gtk::TreeModel::Row &row = *iter;
        MIDI_Cue::msg m;
        m.port = row[m_Columns.m_port];
        m.midi_data = row[m_Columns.m_raw_data];
        pm->msgs.push_back(m);
    }
}

Glib::ustring EditCueMidi::get_line(const std::vector<unsigned char> &raw)
{
    switch (raw[0] & 0xF0) {
    case 0x80:
        return Glib::ustring::compose(_("Note on channel %1 note %2 velocity %3"),
                                      (raw[0] & 0x0f) + 1,
                                      int(raw[1]),
                                      int(raw[2]));
    case 0x90:
        return Glib::ustring::compose(_("Note off channel %1 note %2 velocity %3"),
                                      (raw[0] & 0x0f) + 1,
                                      int(raw[1]),
                                      int(raw[2]));
    case 0xa0:
        return Glib::ustring::compose(_("Aftertouch channel %1 note %2 value %3"),
                                      (raw[0] & 0x0f) + 1,
                                      int(raw[1]),
                                      int(raw[2]));
    case 0xb0:
        return Glib::ustring::compose(
            _("Controller change - Channel %1 control number %2 value %3"),
            (raw[0] & 0x0f) + 1,
            int(raw[1]),
            int(raw[2]));
    case 0xc0:
        return Glib::ustring::compose(
            _("Program change channel %1 program %2"), (raw[0] & 0x0f) + 1, int(raw[1]));
    case 0xd0:
        return Glib::ustring::compose(
            _("Channel aftertouch channel %1 pressure %2"), (raw[0] & 0x0f) + 1, int(raw[1]));
    case 0xe0:
        return Glib::ustring::compose(_("Pitch change channel %1 LSB %2 MSB %3"),
                                      (raw[0] & 0x0f) + 1,
                                      int(raw[1]),
                                      int(raw[2]));
    case 0xf0:
        if (raw[0] == 0xf0 && raw[1] == 0x7f && raw[3] == 0x02) {
            const char *format = "";
            const char *command = "";

            for (auto i : msc_cmd_fmt) {
                if (i.cmd == raw[4]) {
                    format = i.name;
                    break;
                }
            }
            for (auto i : msc_cmd) {
                if (i.cmd == raw[5]) {
                    command = i.name;
                    break;
                }
            }

            return Glib::ustring::compose(
                _("MSC - Device %1 %2 command %3"), int(raw[2]), gettext(format), gettext(command));
        }
        return _("SYSEX");
    }

    return "";
}

void EditCueMidi::on_selection_change()
{
    const Gtk::TreeModel::iterator iter = m_MIDItree->get_selection()->get_selected();
    Gtk::Button *p_midi_remove;
    Gtk::Button *p_midi_modify;
    m_refXml->get_widget("ed_midi_delete", p_midi_remove);
    m_refXml->get_widget("ed_midi_modify", p_midi_modify);
    if (!iter) {
        p_midi_remove->set_sensitive(false);
        p_midi_modify->set_sensitive(false);
        return;
    }
    p_midi_remove->set_sensitive();
    p_midi_modify->set_sensitive();

    const Gtk::TreeModel::Row &row = *iter;

    Gtk::ComboBox *p_midi_patch;
    m_refXml->get_widget("ed_midi_patch", p_midi_patch);

    p_midi_patch->set_active(row[m_Columns.m_port] - 1);

    std::vector<unsigned char> raw = row[m_Columns.m_raw_data];

    Gtk::ComboBox *p_midi_func;
    m_refXml->get_widget("ed_midi_func", p_midi_func);

    if (raw[0] >= 0xf0 && raw[1] == 0x7f && raw[3] == 0x02) {
        p_midi_func->set_active(7);
        Gtk::SpinButton *p_msc_dev;
        Gtk::ComboBox *p_msc_cmd_fmt;
        Gtk::ComboBox *p_msc_cmd;

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

        Gtk::SpinButton *p_msc_macro;
        Gtk::SpinButton *p_msc_cnum;
        Gtk::SpinButton *p_msc_cval;
        Gtk::Entry *p_msc_qnum;
        Gtk::Entry *p_msc_qlist;
        Gtk::Entry *p_msc_qpath;

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
                for (; *r && *r != 0xf7; ++r)
                    s += char(*r);
                p_msc_qnum->set_text(s);
                if (*r != 0) {
                    p_msc_qlist->set_text("");
                    p_msc_qpath->set_text("");
                    break;
                }

                for (s.clear(), ++r; *r && *r != 0xf7; ++r)
                    s += char(*r);
                p_msc_qlist->set_text(s);
                if (*r != 0) {
                    p_msc_qpath->set_text("");
                    break;
                }

                for (s.clear(), ++r; *r && *r != 0xf7; ++r)
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
        Gtk::Entry *p_sysex;
        m_refXml->get_widget("ed_midi_sysex", p_sysex);
        std::ostringstream s;
        s.setf(std::ios::hex, std::ios::basefield);
        for (size_t i = 1; i < (raw.size() - 1); ++i) {
            s << int(raw[i]) << ' ';
        }
        p_sysex->set_text(s.str());
    } else {
        Gtk::SpinButton *p_midi_channel;
        Gtk::SpinButton *p_midi_data1;
        Gtk::SpinButton *p_midi_data2;

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
    Gtk::ComboBox *p_func;
    m_refXml->get_widget("ed_midi_func", p_func);

    Gtk::Label *p_label_data1;
    m_refXml->get_widget("ed_midi_label1", p_label_data1);

    Gtk::Label *p_label_data2;
    m_refXml->get_widget("ed_midi_label2", p_label_data2);

    Gtk::SpinButton *p_channel;
    m_refXml->get_widget("ed_midi_channel", p_channel);

    Gtk::SpinButton *p_data1;
    m_refXml->get_widget("ed_midi_data1", p_data1);
    p_data1->show();

    Gtk::SpinButton *p_data2;
    m_refXml->get_widget("ed_midi_data2", p_data2);

    Gtk::SpinButton *p_msc_dev;
    m_refXml->get_widget("ed_midi_msc_dev", p_msc_dev);

    Gtk::ComboBox *p_msc_cmd_fmt;
    m_refXml->get_widget("ed_midi_msc_cmd_fmt", p_msc_cmd_fmt);

    Gtk::ComboBox *p_msc_cmd;
    m_refXml->get_widget("ed_midi_msc_cmd", p_msc_cmd);

    Gtk::SpinButton *p_msc_macro;
    m_refXml->get_widget("ed_midi_msc_macro", p_msc_macro);

    Gtk::Entry *p_msc_num;
    m_refXml->get_widget("ed_midi_msc_qnum", p_msc_num);

    Gtk::Entry *p_msc_list;
    m_refXml->get_widget("ed_midi_msc_qlist", p_msc_list);

    Gtk::Entry *p_msc_path;
    m_refXml->get_widget("ed_midi_msc_qpath", p_msc_path);

    Gtk::SpinButton *p_msc_cnum;
    m_refXml->get_widget("ed_midi_msc_cnum", p_msc_cnum);

    Gtk::SpinButton *p_msc_cval;
    m_refXml->get_widget("ed_midi_msc_cval", p_msc_cval);

    const unsigned int status = p_channel->get_value_as_int() - 1;
    unsigned int data1 = p_data1->get_value_as_int();
    unsigned int data2 = p_data2->get_value_as_int();

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
        case 3: // TIMED_GO
            build_msc_timecode(raw);
        case 0:  // GO
        case 1:  // STOP
        case 2:  // RESUME
        case 4:  // LOAD - q number required
        case 10: // GO_OFF
        case 11: // GO/JAM_CLOCK
            tstring = p_msc_num->get_text();
            for (Glib::ustring::iterator p = tstring.begin(); p != tstring.end(); ++p)
                raw.push_back(*p);
            tstring = p_msc_list->get_text();
            if (tstring.empty())
                break;
            raw.push_back('\0');
            for (Glib::ustring::iterator p = tstring.begin(); p != tstring.end(); ++p)
                raw.push_back(*p);
            tstring = p_msc_path->get_text();
            if (tstring.empty())
                break;
            raw.push_back('\0');
            for (Glib::ustring::iterator p = tstring.begin(); p != tstring.end(); ++p)
                raw.push_back(*p);
            break;
        case 5:
            { // SET
                const int c = p_msc_cnum->get_value_as_int();
                const int v = p_msc_cval->get_value_as_int();
                raw.push_back(c % 128);
                raw.push_back(c / 128);
                raw.push_back(v % 128);
                raw.push_back(v / 128);
                build_msc_timecode(raw);
            }
            break;
        case 6: // FIRE
            raw.push_back(p_msc_macro->get_value_as_int());
            break;
        case 19: // SET_CLOCK
            build_msc_timecode(raw);
        case 12: // STANDBY_+
        case 13: // STANDBY_-
        case 14: // SEQUENCE_+
        case 15: // SEQUENCE_-
        case 16: // START_CLOCK
        case 17: // STOP_CLOCK
        case 18: // ZERO_CLOCK
        case 20: // MTC_CHASE_ON
        case 21: // MTC_CHASE_OFF
        case 22: // OPEN_CUE_LIST - list required
        case 23: // CLOSE_CUE_LIST - list required
            tstring = p_msc_list->get_text();
            for (Glib::ustring::iterator p = tstring.begin(); p != tstring.end(); ++p)
                raw.push_back(*p);
            break;
        case 24: // OPEN_CUE_PATH
        case 25: // CLOSE_CUE_PATH
            tstring = p_msc_path->get_text();
            for (Glib::ustring::iterator p = tstring.begin(); p != tstring.end(); ++p)
                raw.push_back(*p);
            break;
        default:
            break;
        }
        raw.push_back(0xf7);
        break;
    case 8:
        {
            Gtk::Entry *p_sysex;
            m_refXml->get_widget("ed_midi_sysex", p_sysex);
            raw.push_back(0xf0);
            // TODO read in the hexadecimal line
            const Glib::ustring s = p_sysex->get_text();
            Glib::ustring val;
            for (size_t i = 0; i < s.size(); ++i) {
                if (Glib::Unicode::isspace(s[i]))
                    continue;
                val = s[i];
                ++i;
                if (Glib::Unicode::isspace(s[i])) {
                    raw.push_back(std::strtol(val.c_str(), nullptr, 16));
                    continue;
                }
                ++i;
                val += s[i];
                raw.push_back(std::strtol(val.c_str(), nullptr, 16));
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
    const Gtk::TreeModel::iterator iter = m_MIDItree->get_selection()->get_selected();
    if (!iter)
        return;
    const Gtk::TreeModel::Row &row = *iter;

    Gtk::ComboBox *p_midi_patch;
    m_refXml->get_widget("ed_midi_patch", p_midi_patch);

    const std::vector<unsigned char> raw = get_raw();
    row[m_Columns.m_text] = get_line(raw);
    row[m_Columns.m_raw_data] = raw;
    row[m_Columns.m_port] = p_midi_patch->get_active_row_number() + 1;
}

void EditCueMidi::on_add_clicked()
{
    const Gtk::TreeModel::Row row = *(m_refTreeModel->append());

    Gtk::ComboBox *p_midi_patch;
    m_refXml->get_widget("ed_midi_patch", p_midi_patch);

    const std::vector<unsigned char> raw = get_raw();
    row[m_Columns.m_text] = get_line(raw);
    row[m_Columns.m_raw_data] = raw;
    row[m_Columns.m_port] = p_midi_patch->get_active_row_number() + 1;
}

void EditCueMidi::set_msc_timecode(std::vector<unsigned char>::const_iterator raw)
{
    Gtk::SpinButton *p_msc_hour;
    Gtk::SpinButton *p_msc_minute;
    Gtk::SpinButton *p_msc_second;
    Gtk::SpinButton *p_msc_frame;
    Gtk::SpinButton *p_msc_sub_frame;
    Gtk::RadioButton *p_msc_24;
    Gtk::RadioButton *p_msc_25;
    Gtk::RadioButton *p_msc_30;
    Gtk::RadioButton *p_msc_30df;

    m_refXml->get_widget("ed_midi_msc_hour", p_msc_hour);
    m_refXml->get_widget("ed_midi_msc_minute", p_msc_minute);
    m_refXml->get_widget("ed_midi_msc_second", p_msc_second);
    m_refXml->get_widget("ed_midi_msc_frame", p_msc_frame);
    m_refXml->get_widget("ed_midi_msc_sframe", p_msc_sub_frame);
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
    p_msc_sub_frame->set_value(raw[4]);
}

void EditCueMidi::build_msc_timecode(std::vector<unsigned char> &raw)
{
    Gtk::SpinButton *p_msc_hour;
    Gtk::SpinButton *p_msc_minute;
    Gtk::SpinButton *p_msc_second;
    Gtk::SpinButton *p_msc_frame;
    Gtk::SpinButton *p_msc_sub_frame;
    Gtk::RadioButton *p_msc_24;
    Gtk::RadioButton *p_msc_25;
    Gtk::RadioButton *p_msc_30;
    Gtk::RadioButton *p_msc_30df;

    m_refXml->get_widget("ed_midi_msc_hour", p_msc_hour);
    m_refXml->get_widget("ed_midi_msc_minute", p_msc_minute);
    m_refXml->get_widget("ed_midi_msc_second", p_msc_second);
    m_refXml->get_widget("ed_midi_msc_frame", p_msc_frame);
    m_refXml->get_widget("ed_midi_msc_sframe", p_msc_sub_frame);
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
    raw.push_back(p_msc_sub_frame->get_value_as_int());
}

void EditCueMidi::on_delete_clicked()
{
    const Gtk::TreeModel::iterator iter = m_MIDItree->get_selection()->get_selected();

    if (iter)
        m_refTreeModel->erase(iter);
}

void EditCueMidi::on_combo_changed()
{
    Gtk::ComboBox *p_func;
    m_refXml->get_widget("ed_midi_func", p_func);

    Gtk::Label *p_label_data1;
    m_refXml->get_widget("ed_midi_label1", p_label_data1);

    Gtk::Label *p_label_data2;
    m_refXml->get_widget("ed_midi_label2", p_label_data2);

    Gtk::SpinButton *p_data1;
    m_refXml->get_widget("ed_midi_data1", p_data1);
    p_data1->show();

    Gtk::SpinButton *p_data2;
    m_refXml->get_widget("ed_midi_data2", p_data2);

    Gtk::Notebook *p_pad1;
    m_refXml->get_widget("ed_midi_pad1", p_pad1);

    int page1 = 0;
    switch (p_func->get_active_row_number()) {
    case 0:
    case 1:
        p_label_data1->set_text(_("Note"));
        p_label_data2->set_text(_("Velocity"));
        p_data2->show();
        break;
    case 2:
        p_label_data1->set_text(_("Program number"));
        p_label_data2->set_text("");
        p_data2->hide();
        break;
    case 3:
        p_label_data1->set_text(_("Controller"));
        p_label_data2->set_text(_("Value"));
        p_data2->show();
        break;
    case 4:
        p_label_data1->set_text(_("Note"));
        p_label_data2->set_text(_("Value"));
        p_data2->show();
        break;
    case 5:
        p_label_data1->set_text(_("Value"));
        p_label_data2->set_text("");
        p_data2->hide();
        break;
    case 6:
        p_label_data1->set_text(_("LSB"));
        p_label_data2->set_text(_("MSB"));
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
    Gtk::ComboBox *p_func;
    m_refXml->get_widget("ed_midi_msc_cmd", p_func);

    Gtk::Notebook *p_pad2;
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

    Gtk::Frame *p_time;
    Gtk::Frame *p_number;
    Gtk::Frame *p_list;
    Gtk::Frame *p_path;

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
    case 11: // go/jam clock
    case 12: // standby +
    case 13: // standby -
    case 14: // sequence +
    case 15: // sequence -
    case 16: // start clock
    case 17: // stop clock
    case 18: // zero clock
    case 19: // set clock
    case 20: // MTC chase on
    case 21: // MTC chase off
    case 22: // open cue list
    case 23: // close cue list
        p_number->hide();
        p_list->show();
        p_path->hide();
        break;
    case 24: // open cue path
    case 25: // close cue path
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

void EditCueMidi::validate_qlist(const Glib::ustring &text, int *, Gtk::Entry *e)
{
    for (auto p : text) {
        if (Glib::Unicode::isdigit(p) || p == '.')
            continue;
        e->signal_insert_text().emission_stop();
    }
}

void EditCueMidi::validate_fields()
{
    Gtk::Entry *p_msc_qnum;
    Gtk::Entry *p_msc_qlist;
    Gtk::Entry *p_msc_qpath;
    Gtk::Frame *p_msc_n_frame;
    Gtk::Frame *p_msc_l_frame;
    Gtk::Frame *p_msc_p_frame;

    m_refXml->get_widget("ed_midi_msc_qnum", p_msc_qnum);
    m_refXml->get_widget("ed_midi_msc_qlist", p_msc_qlist);
    m_refXml->get_widget("ed_midi_msc_qpath", p_msc_qpath);

    m_refXml->get_widget("ed_midi_msc_nframe", p_msc_n_frame);
    m_refXml->get_widget("ed_midi_msc_lframe", p_msc_l_frame);
    m_refXml->get_widget("ed_midi_msc_pframe", p_msc_p_frame);

    if (p_msc_n_frame->is_visible() && p_msc_qnum->get_text_length() == 0) {
        p_msc_qlist->set_sensitive(false);
        p_msc_qpath->set_sensitive(false);
        return;
    }
    p_msc_qlist->set_sensitive();

    if (p_msc_l_frame->is_visible() && p_msc_qlist->get_text_length() == 0) {
        p_msc_qpath->set_sensitive(false);
        return;
    }
    p_msc_qpath->set_sensitive();
}

void EditCueMidi::validate_sysex_entry(const Glib::ustring &text, int *, Gtk::Entry *e)
{
    for (auto p : text) {
        if (Glib::Unicode::isxdigit(p) || Glib::Unicode::isspace(p))
            continue;
        e->signal_insert_text().emission_stop();
    }
}

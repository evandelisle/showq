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

#include "app.h"
#include "audio.h"
#include "cue.h"
#include "editcue.h"
#include "main.h"
#include "patch.h"
#include "pref.h"
#include "renumber.h"
#include "utils.h"

#include <alsa/asoundlib.h>

#include <glib/gi18n.h>
#include <gtkmm.h>

#include <cmath>
#include <iomanip>
#include <sstream>

snd_seq_t *oseq;

About::About(BaseObjectType *c_object, const Glib::RefPtr<Gtk::Builder> &refXml)
    : Gtk::AboutDialog(c_object),
      m_refXml(refXml)
{
    set_version(VERSION);
}

void About::on_response(int)
{
    hide();
}

std::unique_ptr<About> About::create()
{
    About *dialog;
    auto refXml = Gtk::Builder::create_from_resource("/org/evandel/showq/ui/about.ui");
    refXml->get_widget_derived("about", dialog);

    return std::unique_ptr<About>(dialog);
}

MIDIengine::MIDIengine()
{
    m_seq = oseq; // for now

    snd_seq_port_info_t *port_info;

    snd_seq_port_info_alloca(&port_info);
    snd_seq_port_info_set_name(port_info, "System Announcement Receiver");
    snd_seq_port_info_set_capability(port_info,
                                     SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE |
                                         SND_SEQ_PORT_CAP_NO_EXPORT);
    snd_seq_port_info_set_port_specified(port_info, 1);
    snd_seq_port_info_set_port(port_info, 100);
    snd_seq_port_info_set_type(port_info, SND_SEQ_PORT_TYPE_APPLICATION);

    snd_seq_create_port(m_seq, port_info);
    snd_seq_connect_from(m_seq,
                         snd_seq_port_info_get_port(port_info),
                         SND_SEQ_CLIENT_SYSTEM,
                         SND_SEQ_PORT_SYSTEM_ANNOUNCE);

    snd_seq_port_info_t *info;
    snd_seq_port_info_alloca(&info);

    snd_seq_port_info_set_capability(info, SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE);
    snd_seq_port_info_set_port_specified(info, 1);
    snd_seq_port_info_set_port(info, 0);
    snd_seq_port_info_set_type(info,
                               SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
    snd_seq_port_info_set_name(info, "input");
    snd_seq_create_port(m_seq, info);

    running = true;
    midi_thread_p = std::thread(&MIDIengine::midi_main, this);
}

MIDIengine::~MIDIengine()
{
    running = false;
    midi_thread_p.join();
}

void MIDIengine::midi_main()
{
    const int nfds = snd_seq_poll_descriptors_count(m_seq, POLLIN);
    auto *pfds = new struct pollfd[nfds];
    auto *revents = new unsigned short[nfds];
    snd_midi_event_t *dev;
    unsigned char buf[256];

    snd_midi_event_new(sizeof(buf) - 1, &dev);

    snd_seq_poll_descriptors(m_seq, pfds, nfds, POLLIN);

    while (running) {
        poll(pfds, nfds, 1000);
        snd_seq_poll_descriptors_revents(m_seq, pfds, nfds, revents);

        for (int i = 0; i < nfds; ++i) {
            if (revents[i] > 0) {
                snd_seq_event_t *ev;
                snd_seq_event_input(m_seq, &ev);

                switch (ev->type) {
                case SND_SEQ_EVENT_PORT_START:
                case SND_SEQ_EVENT_PORT_EXIT:
                case SND_SEQ_EVENT_PORT_CHANGE:
                case SND_SEQ_EVENT_PORT_SUBSCRIBED:
                case SND_SEQ_EVENT_PORT_UNSUBSCRIBED:
                    signal_port_change();
                    break;
                default:
                    snd_midi_event_decode(dev, buf, sizeof(buf) - 1, ev);
                    if (use_msc && buf[0] == 0xf0 && buf[1] == 0x7f && buf[2] == msc_id &&
                        buf[3] == 0x02 && buf[4] == 0x10 && buf[5] == 0x01)
                        signal_go();
                    break;
                }
            }
        }
    }

    snd_midi_event_free(dev);
    delete[] revents;
    delete[] pfds;
}

App::App(BaseObjectType *c_object, const Glib::RefPtr<Gtk::Builder> &refXml)
    : Gtk::Window(c_object),
      m_refXml(refXml),
      recent_filter(Gtk::RecentFilter::create())
{
    resize(640, 400);
    try {
        std::vector<int> win_size = keyfile.get_integer_list("main", "geometry");
        if (win_size.size() == 2)
            resize(win_size[0], win_size[1]);
    }
    catch (...) {
    }

    // Get The app bar
    m_refXml->get_widget("appbar1", p_appbar);

    // Connect toolbar
    connect_clicked(m_refXml, "tb_new", sigc::mem_fun(*this, &App::on_new_activate));
    connect_clicked(m_refXml, "tb_open", sigc::mem_fun(*this, &App::on_open_activate));
    connect_clicked(m_refXml, "tb_save", sigc::mem_fun(*this, &App::on_save_activate));
    connect_clicked(m_refXml, "tb_allstop", sigc::mem_fun(*this, &App::on_all_stop_activate));

    // Connect menu items

    connect_menu_item(m_refXml, "m_new", sigc::mem_fun(*this, &App::on_new_activate));
    connect_menu_item(m_refXml, "m_open", sigc::mem_fun(*this, &App::on_open_activate));
    connect_menu_item(m_refXml, "m_save", sigc::mem_fun(*this, &App::on_save_activate));
    connect_menu_item(m_refXml, "m_saveas", sigc::mem_fun(*this, &App::on_saveas_activate));
    connect_menu_item(m_refXml, "m_quit", sigc::mem_fun(*this, &App::hide));

    connect_menu_item(m_refXml, "m_new_midi_cue", []() { EditCue::show(Cue::MIDI); });
    connect_menu_item(m_refXml, "m_new_wave_cue", []() { EditCue::show(Cue::Wave); });
    connect_menu_item(m_refXml, "m_new_stop_cue", []() { EditCue::show(Cue::Stop); });
    connect_menu_item(m_refXml, "m_new_fade_cue", []() { EditCue::show(Cue::Fade); });
    connect_menu_item(m_refXml, "m_new_group_cue", []() { EditCue::show(Cue::Group); });
    connect_menu_item(m_refXml, "m_new_pause_cue", []() { EditCue::show(Cue::Pause); });
    connect_menu_item(m_refXml, "m_new_start_cue", []() { EditCue::show(Cue::Start); });

    connect_menu_item(m_refXml, "m_edit_cue", sigc::mem_fun(*this, &App::on_edit_cue_activate));
    connect_menu_item(m_refXml, "m_renumber", sigc::mem_fun(*this, &App::on_renumber_activate));
    connect_menu_item(m_refXml, "m_cut_cue", sigc::mem_fun(*this, &App::on_cut_cue_activate));
    connect_menu_item(m_refXml, "m_lock", sigc::mem_fun(*this, &App::on_lock_activate));
    connect_menu_item(m_refXml, "m_properties", sigc::mem_fun(*this, &App::on_properties_activate));
    connect_menu_item(m_refXml, "m_patch", sigc::mem_fun(*this, &App::on_patch_activate));
    connect_menu_item(
        m_refXml, "m_preferences", sigc::mem_fun(*this, &App::on_preferences_activate));

    connect_menu_item(m_refXml, "m_go", sigc::mem_fun(*this, &App::on_go_activate));
    connect_menu_item(m_refXml, "m_previous", sigc::mem_fun(*this, &App::on_previous_activate));
    connect_menu_item(m_refXml, "m_next", sigc::mem_fun(*this, &App::on_next_activate));
    connect_menu_item(m_refXml, "m_load", sigc::mem_fun(*this, &App::on_load_activate));
    connect_menu_item(m_refXml, "m_all_stop", sigc::mem_fun(*this, &App::on_all_stop_activate));
    Gtk::MenuItem *esc_menu_item;
    m_refXml->get_widget("m_all_stop", esc_menu_item);
    auto win_accel = get_accel_group();
    esc_menu_item->add_accelerator(
        "activate", win_accel, GDK_KEY_Escape, Gdk::ModifierType(0), Gtk::ACCEL_VISIBLE);

    connect_menu_item(m_refXml,
                      "m_view_1",
                      sigc::bind<int, Glib::ustring>(
                          sigc::mem_fun(*this, &App::on_view_item_activate), 1, "m_view_1"));
    connect_menu_item(m_refXml,
                      "m_view_2",
                      sigc::bind<int, Glib::ustring>(
                          sigc::mem_fun(*this, &App::on_view_item_activate), 2, "m_view_2"));
    connect_menu_item(m_refXml,
                      "m_view_3",
                      sigc::bind<int, Glib::ustring>(
                          sigc::mem_fun(*this, &App::on_view_item_activate), 3, "m_view_3"));
    connect_menu_item(m_refXml,
                      "m_view_4",
                      sigc::bind<int, Glib::ustring>(
                          sigc::mem_fun(*this, &App::on_view_item_activate), 4, "m_view_4"));
    connect_menu_item(m_refXml,
                      "m_view_5",
                      sigc::bind<int, Glib::ustring>(
                          sigc::mem_fun(*this, &App::on_view_item_activate), 5, "m_view_5"));
    connect_menu_item(m_refXml,
                      "m_view_6",
                      sigc::bind<int, Glib::ustring>(
                          sigc::mem_fun(*this, &App::on_view_item_activate), 6, "m_view_6"));

    connect_menu_item(m_refXml, "m_about", sigc::mem_fun(*this, &App::on_about_activate));

    recent_filter->add_application(Glib::get_prgname());
    recent_menu_item.add_filter(recent_filter);
    recent_menu_item.signal_item_activated().connect(
        sigc::mem_fun(*this, &App::on_recent_activate));
    Gtk::MenuItem *recent_parent_menu_item;
    m_refXml->get_widget("m_recent", recent_parent_menu_item);
    recent_parent_menu_item->set_submenu(recent_menu_item);

    // Set up the tree view
    m_refXml->get_widget_derived("ap_treeview", m_treeview);
    m_refTreeModel = CueTreeStore::create();
    m_treeview->set_model(m_refTreeModel);

    Gtk::CellRendererText *cr;
    Gtk::CellRendererProgress *crp;
    Gtk::TreeViewColumn *tc;

    // Column 0
    tc = Gtk::manage(new Gtk::TreeViewColumn("", m_refTreeModel->Col.pos_img));
    tc->pack_end(m_refTreeModel->Col.image);
    mCols.push_back(tc);

    // Column 1
    cr = Gtk::manage(new Gtk::CellRendererText());
    cr->property_editable() = true;
    cr->signal_edited().connect(sigc::mem_fun(*this, &App::cell_cue));
    cr->signal_editing_canceled().connect(sigc::mem_fun(*this, &App::enable_hotkeys));
    cr->signal_editing_started().connect(sigc::mem_fun(*this, &App::disable_hotkeys));
    tc = Gtk::manage(new Gtk::TreeViewColumn(_("Cue No."), *cr));
    tc->set_cell_data_func(*cr, sigc::mem_fun(*this, &App::cell_data_func_cue));
    mCols.push_back(tc);

    // Column 2
    cr = Gtk::manage(new Gtk::CellRendererText());
    cr->property_editable() = true;
    cr->signal_edited().connect(sigc::mem_fun(*this, &App::cell_text));
    cr->signal_editing_canceled().connect(sigc::mem_fun(*this, &App::enable_hotkeys));
    cr->signal_editing_started().connect(sigc::mem_fun(*this, &App::disable_hotkeys));
    tc = Gtk::manage(new Gtk::TreeViewColumn(_("Description"), *cr));
    tc->set_expand();
    tc->set_resizable();
    tc->set_cell_data_func(*cr, sigc::mem_fun(*this, &App::cell_data_func_text));
    mCols.push_back(tc);

    // Column 3
    crp = Gtk::manage(new Gtk::CellRendererProgress());
    tc = Gtk::manage(new Gtk::TreeViewColumn(_("Wait"), *crp));
    tc->set_resizable();
    tc->set_cell_data_func(*crp, sigc::mem_fun(*this, &App::cell_data_func_wait));
    mCols.push_back(tc);

    // Column 4
    cr = Gtk::manage(new Gtk::CellRendererText());
    tc = Gtk::manage(new Gtk::TreeViewColumn(_("Type"), *cr));
    tc->set_resizable(false);
    tc->set_cell_data_func(*cr, sigc::mem_fun(*this, &App::cell_data_func_type));
    mCols.push_back(tc);

    // Column 5
    crp = Gtk::manage(new Gtk::CellRendererProgress());
    tc = Gtk::manage(new Gtk::TreeViewColumn(_("Q Elapsed"), *crp));
    tc->set_resizable();
    tc->set_cell_data_func(*crp, sigc::mem_fun(*this, &App::cell_data_func_elapsed));
    mCols.push_back(tc);

    // Column 6
    Gtk::CellRendererToggle *crt = Gtk::manage(new Gtk::CellRendererToggle());
    crt->property_activatable() = true;
    crt->signal_toggled().connect(sigc::mem_fun(*this, &App::cell_autoc));
    tc = Gtk::manage(new Gtk::TreeViewColumn(_("Auto cont"), *crt));
    tc->set_resizable(false);
    tc->set_cell_data_func(*crt, sigc::mem_fun(*this, &App::cell_data_func_autoc));
    mCols.push_back(tc);

    for (auto &column : mCols) {
        column->set_reorderable();
    }
    mCols[0]->set_reorderable(false);

    {
        std::string str("0 1 4 3 5 6 2 ");
        try {
            str = keyfile.get_string("main", "cueview");
        }
        catch (...) {
        }
        std::istringstream s(str);
        std::string l;
        std::vector<int> view_list;
        while (s >> l) {
            bool vis = true;
            if (l[0] == '*') {
                vis = false;
                l[0] = ' ';
                Gtk::CheckMenuItem *m;
                m_refXml->get_widget("m_view_" + l.substr(1), m);
                m->set_active(false);
            }
            const int i = std::atoi(l.c_str());
            view_list.push_back(i);
            mCols[i]->set_visible(vis);
            m_treeview->append_column(*mCols[i]);
        }
        for (int i = 0; i < 7; ++i) {
            if (find(view_list.begin(), view_list.end(), i) == view_list.end())
                m_treeview->append_column(*mCols[i]);
        }
    }
    m_treeview->set_expander_column(*mCols[2]);

    m_treeview->set_headers_clickable();
    m_treeview->signal_edit().connect(sigc::mem_fun(*this, &App::edit_cue));
    m_treeview->signal_pause().connect(sigc::mem_fun(*this, &App::on_pause));
    m_treeview->signal_stop().connect(sigc::mem_fun(*this, &App::on_stop));
    m_treeview->signal_sneak_out().connect(sigc::mem_fun(*this, &App::on_sneak_out));

    Pix_play = render_icon_pixbuf(Gtk::Stock::MEDIA_PLAY, Gtk::IconSize(Gtk::ICON_SIZE_MENU));
    Pix_pause = render_icon_pixbuf(Gtk::Stock::MEDIA_PAUSE, Gtk::IconSize(Gtk::ICON_SIZE_MENU));
    Pix_PBpos = render_icon_pixbuf(Gtk::Stock::YES, Gtk::IconSize(Gtk::ICON_SIZE_MENU));

    // Set up MIDI output
    const int result = snd_seq_open(&oseq, "default", SND_SEQ_OPEN_DUPLEX, 0);
    if (result < 0)
        std::exit(1);

    snd_seq_set_client_name(oseq, "ShowQ");
    midi_engine = new MIDIengine;
    // Set a timer to update the display
    dis_timer = Glib::signal_timeout().connect(sigc::mem_fun(*this, &App::dis_update), 100);
    Glib::signal_timeout().connect(sigc::mem_fun(*this, &App::update_status_bar), 1000);

    pb_pos_sel = true;

    Window::show();
}

App::~App()
{
    dis_timer.disconnect();

    try {
        std::vector<int> win_size(2);
        get_size(win_size[0], win_size[1]);
        keyfile.set_integer_list("main", "geometry", win_size);
        keyfile.set_string("main", "LastFile", file);

        std::ostringstream s;
        for (int i = 0; Gtk::TreeViewColumn *c = m_treeview->get_column(i); ++i) {
            if (!c->get_visible())
                s << '*';
            for (size_t j = 0; j < mCols.size(); ++j) {
                if (c == mCols[j])
                    s << j;
            }
            s << ' ';
        }
        keyfile.set_string("main", "cueview", s.str());
    }
    catch (...) {
    }
    delete midi_engine;
    // Close MIDI ports
    snd_seq_close(oseq);
}

void App::cell_data_func_type(Gtk::CellRenderer *cell, const Gtk::TreeModel::iterator &iter)
{
    auto &renderer = dynamic_cast<Gtk::CellRendererText &>(*cell);
    const std::shared_ptr<Cue> q = (*iter)[m_refTreeModel->Col.cue];
    renderer.property_text() = q->cue_type_text();
}

void App::cell_data_func_wait(Gtk::CellRenderer *cell, const Gtk::TreeModel::iterator &iter)
{
    auto &renderer = dynamic_cast<Gtk::CellRendererProgress &>(*cell);
    renderer.property_text() = (*iter)[m_refTreeModel->Col.delay];
    renderer.property_value() = (*iter)[m_refTreeModel->Col.delay_percent];
}

void App::cell_data_func_elapsed(Gtk::CellRenderer *cell, const Gtk::TreeModel::iterator &iter)
{
    auto &renderer = dynamic_cast<Gtk::CellRendererProgress &>(*cell);
    renderer.property_text() = (*iter)[m_refTreeModel->Col.qelapsed];
    renderer.property_value() = (*iter)[m_refTreeModel->Col.qelapsed_percent];
}

void App::cell_data_func_text(Gtk::CellRenderer *cell, const Gtk::TreeModel::iterator &iter)
{
    auto &renderer = dynamic_cast<Gtk::CellRendererText &>(*cell);
    const std::shared_ptr<Cue> q = (*iter)[m_refTreeModel->Col.cue];
    renderer.property_text() = q->text;
}

void App::cell_data_func_cue(Gtk::CellRenderer *cell, const Gtk::TreeModel::iterator &iter)
{
    auto &renderer = dynamic_cast<Gtk::CellRendererText &>(*cell);
    const std::shared_ptr<Cue> q = (*iter)[m_refTreeModel->Col.cue];
    renderer.property_text() = q->cue_id;
}

void App::cell_data_func_autoc(Gtk::CellRenderer *cell, const Gtk::TreeModel::iterator &iter)
{
    auto &renderer = dynamic_cast<Gtk::CellRendererToggle &>(*cell);
    const std::shared_ptr<Cue> q = (*iter)[m_refTreeModel->Col.cue];
    renderer.set_active(q->autocont);
}

void App::cell_cue(const Glib::ustring &path, const Glib::ustring &text)
{
    const Gtk::TreeModel::iterator iter = m_refTreeModel->get_iter(path);
    const std::shared_ptr<Cue> q = (*iter)[m_refTreeModel->Col.cue];
    q->cue_id = text;
    enable_hotkeys();
}

void App::cell_text(const Glib::ustring &path, const Glib::ustring &text)
{
    const Gtk::TreeModel::iterator iter = m_refTreeModel->get_iter(path);
    const std::shared_ptr<Cue> q = (*iter)[m_refTreeModel->Col.cue];
    q->text = text;
    enable_hotkeys();
}

void App::cell_autoc(const Glib::ustring &path)
{
    const Gtk::TreeModel::iterator iter = m_refTreeModel->get_iter(path);
    const std::shared_ptr<Cue> q = (*iter)[m_refTreeModel->Col.cue];
    q->autocont ^= true;
}

void App::disable_hotkeys(Gtk::CellEditable *, const Glib::ustring &)
{
    Gtk::Widget *widget;
    m_refXml->get_widget("m_go", widget);

    widget->set_sensitive(false);
}

void App::enable_hotkeys()
{
    Gtk::Widget *widget;
    m_refXml->get_widget("m_go", widget);
    widget->set_sensitive(true);
}

// Hot keys

bool App::on_key_press_event(GdkEventKey *event)
{
    Gtk::Widget *widget;
    m_refXml->get_widget("m_go", widget);

    if (widget->get_sensitive() &&
        Gtk::AccelGroup::valid(event->keyval, Gdk::ModifierType(event->state))) {
        keyval = event->keyval;
        state = Gdk::ModifierType(event->state);
        m_refTreeModel->foreach_iter(sigc::mem_fun(*this, &App::check_key));
    }

    return Window::on_key_press_event(event);
}

bool App::check_key(const Gtk::TreeModel::iterator &iter)
{
    const std::shared_ptr<Cue> q = (*iter)[m_refTreeModel->Col.cue];
    if (q->keyval == keyval && q->state == state)
        go_cue(iter);

    return false;
}

bool App::dis_update()
{
    auto i = waiting_cue.begin();
    Glib::TimeVal cur_time;
    cur_time.assign_current_time();

    while (i != waiting_cue.end()) {
        const Gtk::TreeModel::iterator iter = m_refTreeModel->get_iter(i->w_cue.get_path());
        if (!iter)
            continue;
        if (i->done) {
            (*iter)[m_refTreeModel->Col.image] = Pix_blank;
            (*iter)[m_refTreeModel->Col.delay] = "";
            (*iter)[m_refTreeModel->Col.delay_percent] = 0;
            i = waiting_cue.erase(i);
        } else if (i->active) {
            const double t = (cur_time - i->start).as_double();
            (*iter)[m_refTreeModel->Col.delay] = dtoasctime(t);
            (*iter)[m_refTreeModel->Col.delay_percent] =
                (t * 100) / (i->end - i->start).as_double();
            ++i;
        }
        if (i->update_icon) {
            i->update_icon = false;
            if (i->active)
                (*iter)[m_refTreeModel->Col.image] = Pix_pause;
            else
                (*iter)[m_refTreeModel->Col.image] = Pix_play;
        }
    }

    for (auto i = running_cue.begin(); i != running_cue.end();) {
        auto k = i++;
        const Gtk::TreeModel::iterator iter = m_refTreeModel->get_iter(k->r_cue.get_path());
        if (!iter)
            continue;

        if (k->update_icon) {
            // TODO: make my own icons
            const int s = k->Get_status();
            if (s == Play)
                (*iter)[m_refTreeModel->Col.image] = Pix_play;
            else if (s == Pause)
                (*iter)[m_refTreeModel->Col.image] = Pix_pause;
            k->update_icon = false;
        }

        if (k->fade) {
            if (k->fade->nframes <= 0) {
                (*iter)[m_refTreeModel->Col.image] = Pix_blank;
                (*iter)[m_refTreeModel->Col.qelapsed] = "";
                (*iter)[m_refTreeModel->Col.qelapsed_percent] = 0;
            } else {
                const long d = k->fade->tframes - k->fade->nframes;
                const double t = double(d) / audio->get_sample_rate();
                (*iter)[m_refTreeModel->Col.qelapsed] = dtoasctime(t);
                (*iter)[m_refTreeModel->Col.qelapsed_percent] = int((100 * (d)) / k->fade->tframes);
            }
            if (k->fade->status == Done)
                running_cue.erase(k);
        } else {
            const double t = k->af->get_pos();
            (*iter)[m_refTreeModel->Col.qelapsed] = dtoasctime(t);
            (*iter)[m_refTreeModel->Col.qelapsed_percent] = int(100 * t / k->af->total_time());
            if (k->af->status == Done) {
                (*iter)[m_refTreeModel->Col.image] = Pix_blank;
                (*iter)[m_refTreeModel->Col.qelapsed] = "";
                (*iter)[m_refTreeModel->Col.qelapsed_percent] = 0;
                running_cue.erase(k);
            }
        }
    }

    return true;
}

bool App::update_status_bar()
{
    auto str = Glib::ustring::compose(
        _("DSP load %1%%"), Glib::ustring::format(std::setprecision(2), audio->get_cpu_load()));
    p_appbar->pop();
    p_appbar->push(str);

    return true;
}

void App::on_view_item_activate(int i, Glib::ustring s)
{
    Gtk::CheckMenuItem *w;
    m_refXml->get_widget(s, w);

    mCols[i]->set_visible(w->get_active());
}

void App::on_new_activate()
{
    // TODO should be able to have multiple documents?

    Gtk::MessageDialog dialog(
        _("Clear the current show?"), false, Gtk::MESSAGE_WARNING, Gtk::BUTTONS_NONE, true);
    dialog.add_button(_("_OK"), Gtk::RESPONSE_OK);
    dialog.add_button(_("_Cancel"), Gtk::RESPONSE_CANCEL);
    if (dialog.run() == Gtk::RESPONSE_OK) {
        title.clear();
        file.clear();
        note.clear();
        set_title("Show Q");
        m_refTreeModel->clear();
    }
}

void App::on_open_activate()
{
    if (!title.empty() || m_refTreeModel->children().begin() != m_refTreeModel->children().end()) {
        Gtk::MessageDialog dialog(_("You are about to load a show over this one.\nContinue ?"),
                                  false,
                                  Gtk::MESSAGE_WARNING,
                                  Gtk::BUTTONS_NONE,
                                  true);
        dialog.add_button(_("_Cancel"), Gtk::RESPONSE_CANCEL);
        dialog.add_button(_("_OK"), Gtk::RESPONSE_OK);

        if (dialog.run() != Gtk::RESPONSE_OK)
            return;
    }

    Gtk::FileChooserDialog sel(*this, _("Load Show"), Gtk::FILE_CHOOSER_ACTION_OPEN);
    sel.add_button(_("_Cancel"), Gtk::RESPONSE_CANCEL);
    sel.add_button(_("_OK"), Gtk::RESPONSE_OK);
    if (sel.run() == Gtk::RESPONSE_OK) {
        do_load(sel.get_filename());
    }
}

void App::on_recent_activate()
{
    if (!title.empty() || m_refTreeModel->children().begin() != m_refTreeModel->children().end()) {
        Gtk::MessageDialog dialog(_("You are about to load a show over this one.\nContinue ?"),
                                  false,
                                  Gtk::MESSAGE_WARNING,
                                  Gtk::BUTTONS_NONE,
                                  true);
        dialog.add_button(_("_Cancel"), Gtk::RESPONSE_CANCEL);
        dialog.add_button(_("_OK"), Gtk::RESPONSE_OK);

        if (dialog.run() != Gtk::RESPONSE_OK)
            return;
    }

    do_load(Glib::filename_from_uri(recent_menu_item.get_current_uri()));
}

void App::on_save_activate()
{
    if (file.empty()) {
        on_saveas_activate();
        return;
    }

    do_save();
}

void App::on_saveas_activate()
{
    Gtk::FileChooserDialog sel(*this, _("Save Show As"), Gtk::FILE_CHOOSER_ACTION_SAVE);
    sel.add_button(_("_Cancel"), Gtk::RESPONSE_CANCEL);
    sel.add_button(_("_OK"), Gtk::RESPONSE_OK);
    if (sel.run() == Gtk::RESPONSE_OK) {
        file = sel.get_filename();
        do_save();
        const Glib::RefPtr<Gtk::RecentManager> pRM = Gtk::RecentManager::get_default();
        pRM->add_item(Glib::filename_to_uri(file));
    }
}

void App::on_edit_cue_activate()
{
    const Gtk::TreeModel::iterator iter = m_treeview->get_selection()->get_selected();

    if (iter) {
        Gtk::TreeRowReference p(m_refTreeModel, Gtk::TreePath(iter));
        EditCue::show((*iter)[m_refTreeModel->Col.cue], p);
    }
}

void App::on_renumber_activate()
{
    if (p_renumber)
        p_renumber->present();
    else {
        p_renumber = Renumber::create();
        p_renumber->signal_ok().connect(sigc::mem_fun(*this, &App::renumber));
        p_renumber->signal_hide().connect(sigc::mem_fun(*this, &App::on_renumber_hide));
    }
}

void App::on_renumber_hide()
{
    p_renumber.reset();
}

void App::renumber()
{
    double cue_no = p_renumber->cue_no;
    const double step = p_renumber->step;
    const bool skip_auto_cont = p_renumber->skip_autocont;
    bool prev_auto_cont = false;

    Gtk::TreeModel::Children children = m_refTreeModel->children();

    for (Gtk::TreeModel::iterator iter = children.begin(); iter != children.end(); ++iter) {
        const std::shared_ptr<Cue> cue = (*iter)[m_refTreeModel->Col.cue];

        if (skip_auto_cont && prev_auto_cont) {
            cue->cue_id.clear();
        } else {
            cue->cue_id = Glib::ustring::compose("%1", cue_no);
            cue_no += step;
        }
        m_refTreeModel->row_changed(m_refTreeModel->get_path(iter), iter);
        prev_auto_cont = cue->autocont;
    }
}

void App::on_lock_activate()
{
    const Glib::RefPtr<Gtk::ToggleAction> p =
        Glib::RefPtr<Gtk::ToggleAction>::cast_static(m_refXml->get_object("m_lock"));

    if (p->get_active()) {
        m_treeview->set_reorderable(false);
        (mCols[1])->get_first_cell()->property_mode() = Gtk::CELL_RENDERER_MODE_INERT;
        (mCols[2])->get_first_cell()->property_mode() = Gtk::CELL_RENDERER_MODE_INERT;
        (mCols[6])->get_first_cell()->property_mode() = Gtk::CELL_RENDERER_MODE_INERT;
    } else {
        m_treeview->set_reorderable();
        (mCols[1])->get_first_cell()->property_mode() = Gtk::CELL_RENDERER_MODE_EDITABLE;
        (mCols[2])->get_first_cell()->property_mode() = Gtk::CELL_RENDERER_MODE_EDITABLE;
        (mCols[6])->get_first_cell()->property_mode() = Gtk::CELL_RENDERER_MODE_ACTIVATABLE;
    }
}

void App::on_cut_cue_activate()
{
    const Gtk::TreeModel::iterator iter = m_treeview->get_selection()->get_selected();

    if (iter) {
        // TODO Should have an undo function
        const std::shared_ptr<Cue> q = (*iter)[m_refTreeModel->Col.cue];
        Gtk::MessageDialog dialog(Glib::ustring::compose(_("Delete sound cue %1 ?"), q->cue_id),
                                  false,
                                  Gtk::MESSAGE_WARNING,
                                  Gtk::BUTTONS_NONE,
                                  true);
        dialog.add_button(_("_Cancel"), Gtk::RESPONSE_CANCEL);
        dialog.add_button(_("_OK"), Gtk::RESPONSE_OK);

        if (dialog.run() == Gtk::RESPONSE_OK)
            m_refTreeModel->erase(iter);
    }
}

Gtk::TreeModel::iterator App::replace_cue(std::shared_ptr<Cue> &q, Gtk::TreeRowReference &r)
{
    Gtk::TreeModel::iterator iter = m_refTreeModel->get_iter(r.get_path());
    const Gtk::TreeModel::Row row = *iter;

    row[m_refTreeModel->Col.cue] = q;
    row[m_refTreeModel->Col.delay] = "";
    row[m_refTreeModel->Col.delay_percent] = 0;

    return iter;
}

Gtk::TreeModel::iterator App::append_cue(std::shared_ptr<Cue> &q)
{
    Gtk::TreeModel::iterator iter = m_refTreeModel->append();
    const Gtk::TreeModel::Row row = *iter;

    row[m_refTreeModel->Col.cue] = q;
    row[m_refTreeModel->Col.delay] = "";
    row[m_refTreeModel->Col.delay_percent] = 0;

    return iter;
}

Gtk::TreeModel::iterator App::append_cue(std::shared_ptr<Cue> &q, const Gtk::TreeModel::iterator &i)
{
    Gtk::TreeModel::iterator iter = m_refTreeModel->append(i->children());
    const Gtk::TreeModel::Row row = *iter;

    row[m_refTreeModel->Col.cue] = q;
    row[m_refTreeModel->Col.delay] = "";
    row[m_refTreeModel->Col.delay_percent] = 0;

    return iter;
}

Gtk::TreeModel::iterator App::insert_cue(std::shared_ptr<Cue> &q)
{
    const Gtk::TreeModel::iterator i = m_treeview->get_selection()->get_selected();
    Gtk::TreeModel::iterator iter = i ? m_refTreeModel->insert_after(i) : m_refTreeModel->append();
    const Gtk::TreeModel::Row row = *iter;

    row[m_refTreeModel->Col.cue] = q;
    row[m_refTreeModel->Col.delay] = "";
    row[m_refTreeModel->Col.delay_percent] = 0;

    return iter;
}

Gtk::TreeModel::iterator App::insert_cue(const Gtk::TreeModel::iterator &i, std::shared_ptr<Cue> &q)
{
    Gtk::TreeModel::iterator iter = i ? m_refTreeModel->insert(i) : m_refTreeModel->append();
    const Gtk::TreeModel::Row row = *iter;

    row[m_refTreeModel->Col.cue] = q;
    row[m_refTreeModel->Col.delay] = "";
    row[m_refTreeModel->Col.delay_percent] = 0;

    return iter;
}

void App::on_pause()
{
    const Gtk::TreeModel::iterator iter = m_treeview->get_selection()->get_selected();
    if (!iter)
        return;
    const std::shared_ptr<Cue> q = (*iter)[m_refTreeModel->Col.cue];

    for (auto &&cue : waiting_cue) {
        if (cue.cue_id_no == q->cue_id_no) {
            cue.Pause();
            break;
        }
    }

    for (auto &&cue : running_cue) {
        if (cue.cue_id_no == q->cue_id_no) {
            cue.Pause();
            break;
        }
    }
}

void App::on_stop()
{
    const Gtk::TreeModel::iterator iter = m_treeview->get_selection()->get_selected();
    if (!iter)
        return;
    const std::shared_ptr<Cue> q = (*iter)[m_refTreeModel->Col.cue];

    for (auto &&cue : waiting_cue) {
        if (cue.cue_id_no == q->cue_id_no) {
            cue.Stop();
            break;
        }
    }

    for (auto &&cue : running_cue) {
        if (cue.cue_id_no == q->cue_id_no) {
            cue.Stop();
            break;
        }
    }
}

void App::on_sneak_out()
{
    const Gtk::TreeModel::iterator iter = m_treeview->get_selection()->get_selected();
    if (!iter)
        return;
    const std::shared_ptr<Cue> q = (*iter)[m_refTreeModel->Col.cue];

    for (auto &&cue : waiting_cue) {
        if (cue.cue_id_no == q->cue_id_no) {
            cue.Stop();
            break;
        }
    }

    for (auto &&cue : running_cue) {
        if (cue.cue_id_no == q->cue_id_no) {
            auto fade = std::make_shared<AudioFile::fade_>();
            fade->vol = std::vector<float>(8, 0.0);
            fade->on = std::vector<bool>(8, true);
            fade->stop_on_complete = true;
            fade->tframes = fade->nframes = long(5 * audio->get_sample_rate());
            cue.af->add_fade(fade);
            break;
        }
    }
}

void App::on_go_activate()
{
    Gtk::TreeModel::iterator s_iter;

    if (pb_pos_sel)
        s_iter = m_treeview->get_selection()->get_selected();
    else if (m_CurrentCue)
        s_iter = m_refTreeModel->get_iter(m_CurrentCue.get_path());

    if (!s_iter)
        return;

    Gtk::TreeModel::iterator iter = go_cue(s_iter);

    try {
        if (keyfile.get_boolean("main", "DisableMoveNext"))
            return;
    }
    catch (...) {
    }

    if (iter)
        ++iter;
    if (!iter) {
        Gtk::TreeModel::Path p = Gtk::TreeModel::Path(s_iter);
        if (p.size() > 1 && p.up()) {
            iter = m_refTreeModel->get_iter(p);
            ++iter;
        }
    }

    if (iter && iter != m_refTreeModel->children().end()) {
        if (pb_pos_sel) {
            m_treeview->get_selection()->select(iter);
            m_treeview->set_cursor(Gtk::TreeModel::Path(iter));
        } else {
            (*s_iter)[m_refTreeModel->Col.pos_img] = Pix_blank;
            (*iter)[m_refTreeModel->Col.pos_img] = Pix_PBpos;
            m_CurrentCue = Gtk::TreeRowReference(m_refTreeModel, Gtk::TreePath(iter));
        }
        m_treeview->scroll_to_row(Gtk::TreePath(iter));
    } else {
        if (pb_pos_sel)
            m_treeview->get_selection()->unselect_all();
        else {
            (*s_iter)[m_refTreeModel->Col.pos_img] = Pix_blank;
            m_CurrentCue = Gtk::TreeRowReference();
        }
    }
}

// Runs the cues starting at iter
// run_all - if true starts all cues (useful for group cues)
// returns iterator to last cue started

Gtk::TreeModel::iterator App::go_cue(Gtk::TreeModel::iterator iter, bool run_all)
{
    Glib::TimeVal cur_time;

    cur_time.assign_current_time();

    for (; iter; ++iter) {
        const std::shared_ptr<Cue> q = (*iter)[m_refTreeModel->Col.cue];

        // Check that the cue is not running already
        auto i = waiting_cue.begin();
        for (; i != waiting_cue.end(); ++i)
            if (i->cue_id_no == q->cue_id_no) {
                if (!i->active)
                    i->Continue();
                break;
            }
        auto j = running_cue.begin();
        for (; j != running_cue.end(); ++j)
            if (j->cue_id_no == q->cue_id_no) {
                if (j->Get_status() != Play)
                    j->Continue();
                break;
            }

        if (i != waiting_cue.end() || j != running_cue.end()) {
        } else if (q->delay > 0.0001) {
            WaitingCue wq;
            double x;

            wq.cue_id_no = q->cue_id_no;
            wq.w_cue = Gtk::TreeRowReference(m_refTreeModel, Gtk::TreePath(iter));
            wq.start = cur_time;
            wq.end = cur_time;
            wq.end.add_milliseconds(long(std::modf(q->delay, &x) * 1000));
            wq.end += long(x);
            waiting_cue.push_back(wq);
            Glib::signal_timeout().connect(sigc::mem_fun(*this, &App::wait_timeout),
                                           long(q->delay * 1000));

            (*iter)[m_refTreeModel->Col.image] = Pix_play;
        } else {
            Gtk::TreeModel::Children children = iter->children();
            Gtk::TreeModel::iterator citer = children.begin();
            if (citer) {
                const std::shared_ptr<Group_Cue> pg = std::dynamic_pointer_cast<Group_Cue>(q);
                citer = go_cue(citer, pg->mode == 0);
                if (citer)
                    iter = citer;
            }
            q->run(iter);
        }

        if (!run_all && !q->autocont)
            break;
    }

    return iter;
}

bool App::wait_timeout()
{
    Glib::TimeVal next_time;
    Glib::TimeVal curr_time;

    curr_time.assign_current_time();
    next_time = curr_time + 60;

    std::list<Gtk::TreeRowReference> to_run;

    for (auto &&cue : waiting_cue) {
        if (curr_time >= cue.end && !cue.done) {
            to_run.push_back(cue.w_cue);
            cue.Stop();
        } else if (!cue.done && next_time > cue.end) {
            next_time = cue.end;
        }
    }

    for (auto &&j : to_run) {
        Gtk::TreeModel::iterator iter = m_refTreeModel->get_iter(j.get_path());
        if (!iter)
            continue;

        const std::shared_ptr<Cue> cue = (*iter)[m_refTreeModel->Col.cue];
        Gtk::TreeModel::Children children = iter->children();
        Gtk::TreeModel::iterator citer = children.begin();
        if (citer) {
            const std::shared_ptr<Group_Cue> pg = std::dynamic_pointer_cast<Group_Cue>(cue);
            citer = go_cue(citer, pg->mode == 0);
            if (citer)
                iter = citer;
        }
        cue->run(iter);
    }

    if (waiting_cue.empty())
        return false;

    curr_time.assign_current_time();
    Glib::signal_timeout().connect(sigc::mem_fun(*this, &App::wait_timeout),
                                   (unsigned int)((next_time - curr_time).as_double() * 1000));
    return false;
}

bool Pause_Cue::run(Gtk::TreeModel::iterator)
{
    for (auto &&i : app->waiting_cue) {
        if (target == i.cue_id_no) {
            i.Pause();
            break;
        }
    }

    for (auto &&j : app->running_cue) {
        if (target == j.cue_id_no) {
            j.Pause();
            break;
        }
    }
    return false;
}

bool Start_Cue::run(Gtk::TreeModel::iterator)
{
    const Gtk::TreeModel::iterator iter = app->m_refTreeModel->get_iter_from_id(target);
    if (iter)
        app->go_cue(iter);
    return false;
}

bool Stop_Cue::run(Gtk::TreeModel::iterator)
{
    for (auto &&i : app->waiting_cue) {
        if (target == i.cue_id_no) {
            i.Stop();
            break;
        }
    }

    for (auto &&j : app->running_cue) {
        if (target == j.cue_id_no) {
            j.Stop();
            break;
        }
    }
    return false;
}

bool Wave_Cue::run(Gtk::TreeModel::iterator r)
{
    RunningCue rq;

    auto af = std::make_shared<AudioFile>(file.c_str());

    af->play();
    if (start_time > 0.00001)
        af->seek(start_time);
    af->vol = vol;
    af->patch = patch;

    audio->add_af(af);
    rq.cue_id_no = cue_id_no;
    rq.r_cue = Gtk::TreeRowReference(app->m_refTreeModel, Gtk::TreePath(r));
    rq.af = af;
    app->running_cue.push_back(rq);
    return true;
}

bool MIDI_Cue::run(Gtk::TreeModel::iterator)
{
    snd_midi_event_t *dev;
    snd_midi_event_new(100, &dev);

    snd_seq_event_t ev;
    snd_seq_ev_clear(&ev);
    snd_seq_ev_set_subs(&ev);
    snd_seq_ev_set_direct(&ev);

    for (auto &&midi_msg : msgs) {
        snd_seq_ev_set_source(&ev, midi_msg.port);
        snd_midi_event_reset_encode(dev);
        for (const auto data_byte : midi_msg.midi_data) {
            snd_midi_event_encode_byte(dev, data_byte, &ev);
        }

        snd_seq_event_output(oseq, &ev);
    }
    snd_seq_drain_output(oseq);

    snd_midi_event_free(dev);

    return false;
}

bool FadeStop_Cue::run(Gtk::TreeModel::iterator r)
{
    RunningCue rq;

    for (auto &&j : app->running_cue) {
        if (target == j.cue_id_no) {
            rq.af = j.af;

            rq.cue_id_no = cue_id_no;
            rq.r_cue = Gtk::TreeRowReference(app->m_refTreeModel, Gtk::TreePath(r));
            rq.fade = std::make_shared<AudioFile::fade_>();
            rq.fade->vol = tvol;
            rq.fade->on = on;
            rq.fade->stop_on_complete = stop_on_complete;
            rq.fade->pause_on_complete = pause_on_complete;
            rq.fade->tframes = rq.fade->nframes = long(fade_time * audio->get_sample_rate());
            rq.af->add_fade(rq.fade);
            app->running_cue.push_back(rq);
            return true;
        }
    }
    return false;
}

bool Group_Cue::run(Gtk::TreeModel::iterator)
{
    return true;
}

void App::on_previous_activate()
{
    Gtk::TreeModel::iterator iter = m_treeview->get_selection()->get_selected();

    if (!iter) {
        iter = m_refTreeModel->children().begin();
        if (iter)
            m_treeview->get_selection()->select(iter);
    } else {
        if (iter != m_refTreeModel->children().begin()) {
            --iter;
            m_treeview->get_selection()->select(iter);
        }
    }
}

void App::on_next_activate()
{
    Gtk::TreeModel::iterator iter = m_treeview->get_selection()->get_selected();

    if (!iter) {
        iter = m_refTreeModel->children().begin();
        if (iter) {
            m_treeview->get_selection()->select(iter);
            m_treeview->set_cursor(Gtk::TreeModel::Path(iter));
        }
    } else {
        ++iter;
        if (iter != m_refTreeModel->children().end()) {
            m_treeview->get_selection()->select(iter);
            m_treeview->set_cursor(Gtk::TreeModel::Path(iter));
        } else
            m_treeview->get_selection()->unselect_all();
    }
}

void App::on_load_activate()
{
    const Gtk::TreeModel::iterator iter = m_treeview->get_selection()->get_selected();
    if (iter) {
        if (m_CurrentCue) {
            const Gtk::TreeModel::iterator o_iter =
                m_refTreeModel->get_iter(m_CurrentCue.get_path());
            (*o_iter)[m_refTreeModel->Col.pos_img] = Pix_blank;
        }

        (*iter)[m_refTreeModel->Col.pos_img] = Pix_PBpos;
        m_CurrentCue = Gtk::TreeRowReference(m_refTreeModel, Gtk::TreePath(iter));
    }
}

void App::on_all_stop_activate()
{
    for (auto &&cue : waiting_cue) {
        cue.Stop();
    }
    for (auto &&cue : running_cue) {
        cue.Stop();
    }
}

void App::on_properties_activate()
{
    if (!p_properties) {
        p_properties = Properties::create();
        p_properties->signal_hide().connect([this]() { p_properties.reset(); });
    } else
        p_properties->present();
}

void App::on_preferences_activate()
{
    if (!p_preferences) {
        p_preferences = Preferences::create();
        p_preferences->signal_hide().connect([this]() { p_preferences.reset(); });
    } else
        p_preferences->present();
}

void App::on_patch_activate()
{
    if (!p_patch) {
        p_patch = Patch::create();
        p_patch->signal_hide().connect([this]() { p_patch.reset(); });
    } else
        p_patch->present();
}

void App::on_about_activate()
{
    if (!p_about) {
        p_about = About::create();
        p_about->signal_hide().connect([this]() { p_about.reset(); });
    } else
        p_about->present();
}

CueTreeView::CueTreeView(BaseObjectType *c_object, const Glib::RefPtr<Gtk::Builder> &refXml)
    : Gtk::TreeView(c_object),
      m_refXml(refXml)
{
    set_column_drag_function(
        [](Gtk::TreeView *, Gtk::TreeViewColumn *, Gtk::TreeViewColumn *b, Gtk::TreeViewColumn *) {
            return b != nullptr;
        });
    auto refPopupXml = Gtk::Builder::create_from_resource("/org/evandel/showq/ui/popupmenu.ui");
    refPopupXml->get_widget("PopupMenu", m_MenuPopup);

    Glib::RefPtr<Gtk::Action>::cast_static(refPopupXml->get_object("menuitem1"))
        ->signal_activate()
        .connect(sigc::mem_fun(*this, &CueTreeView::on_edit));
    Glib::RefPtr<Gtk::Action>::cast_static(refPopupXml->get_object("menuitem2"))
        ->signal_activate()
        .connect(sigc::mem_fun(*this, &CueTreeView::on_stop));
    Glib::RefPtr<Gtk::Action>::cast_static(refPopupXml->get_object("menuitem3"))
        ->signal_activate()
        .connect(sigc::mem_fun(*this, &CueTreeView::on_pause));
    Glib::RefPtr<Gtk::Action>::cast_static(refPopupXml->get_object("menuitem4"))
        ->signal_activate()
        .connect(sigc::mem_fun(*this, &CueTreeView::on_sneak_out));

    // Targets:
    const std::vector<Gtk::TargetEntry> listTargets = {Gtk::TargetEntry("text/uri-list"),
                                                       Gtk::TargetEntry("GTK_TREE_MODEL_ROW")};

    enable_model_drag_dest(listTargets);
}

CueTreeView::~CueTreeView() = default;

bool CueTreeView::on_button_press_event(GdkEventButton *event)
{
    const bool return_value = Gtk::TreeView::on_button_press_event(event);

    if (event->type == GDK_2BUTTON_PRESS && event->button == 1) {
        m_signal_edit.emit();
    }

    // Then do our custom stuff:
    if (event->type == GDK_BUTTON_PRESS && event->button == 3) {
        const Gtk::TreeModel::iterator iter = get_selection()->get_selected();
        if (iter)
            m_MenuPopup->popup(event->button, event->time);
    }

    return return_value;
}

void CueTreeView::on_drag_data_received(const Glib::RefPtr<Gdk::DragContext> &context, int x, int y,
                                        const Gtk::SelectionData &selection_data, guint info,
                                        guint time)
{
    TreeView::on_drag_data_received(context, x, y, selection_data, info, time);

    const std::vector<Glib::ustring> uris = selection_data.get_uris();

    if (uris.empty())
        return;

    Gtk::TreeModel::Path path;
    Gtk::TreeViewDropPosition pos;

    get_dest_row_at_pos(x, y, path, pos);
    Gtk::TreeModel::iterator iter = app->m_refTreeModel->get_iter(path);

    if (pos == Gtk::TREE_VIEW_DROP_AFTER)
        ++iter;

    bool success = false;

    for (const auto &i : uris) {
        AudioFile af(Glib::filename_from_uri(i).c_str());
        if (af.get_codec() == NoCodec)
            continue;

        success = true;
        std::shared_ptr<Cue> cue = std::make_shared<Wave_Cue>();
        const std::shared_ptr<Wave_Cue> q = std::dynamic_pointer_cast<Wave_Cue>(cue);
        q->file = Glib::filename_from_uri(i);
        q->text = Glib::filename_to_utf8(Glib::path_get_basename(q->file));
        q->cue_id_no = uuid::uuid();
        iter = app->insert_cue(iter, cue);
        ++iter;
    }

    context->drag_finish(success, false, time);
}

void CueTreeView::on_edit()
{
    m_signal_edit.emit();
}

void CueTreeView::on_pause()
{
    m_signal_pause.emit();
}

void CueTreeView::on_stop()
{
    m_signal_stop.emit();
}

void CueTreeView::on_sneak_out()
{
    m_signal_sneak_out.emit();
}

CueTreeStore::CueTreeStore()
{
    set_column_types(Col);
}

Gtk::TreeModel::iterator CueTreeStore::get_iter_from_id(uuid::uuid id)
{
    m_id = id;
    m_id_iter = Gtk::TreeIter();
    foreach_iter(sigc::mem_fun(*this, &CueTreeStore::is_id));
    return m_id_iter;
}

bool CueTreeStore::is_id(const TreeModel::iterator &i)
{
    const std::shared_ptr<Cue> q = (*i)[Col.cue];
    if (m_id == q->cue_id_no) {
        m_id_iter = i;
        return true;
    }
    return false;
}

Glib::RefPtr<CueTreeStore> CueTreeStore::create()
{
    return Glib::RefPtr<CueTreeStore>(new CueTreeStore());
}

bool CueTreeStore::row_drop_possible_vfunc(const Gtk::TreeModel::Path &dest,
                                           const Gtk::SelectionData &selection_data) const
{
    Gtk::TreeModel::Path dest_parent = dest;
    const bool dest_is_not_top_level = dest_parent.up();

    if (!dest_is_not_top_level || dest_parent.empty()) {
        // The user wants to move something to the top-level.
        // Let's always allow that.
    } else {
        const iterator iter = (const_cast<CueTreeStore *>(this))->get_iter(dest_parent);
        if (iter) {
            const std::shared_ptr<Cue> q = (*iter)[Col.cue];
            return q->cue_type() == Cue::Group;
        }
    }

    return Gtk::TreeStore::row_drop_possible_vfunc(dest, selection_data);
}

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

#ifndef APP_H_
#define APP_H_

#include <gtkmm.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <libxml++/libxml++.h>
#pragma GCC diagnostic pop

#include <atomic>
#include <map>
#include <memory>
#include <thread>
#include <vector>
#include <alsa/asoundlib.h>

#include "cue.h"
#include "audio.h"
#include "pref.h"
#include "renumber.h"
#include "editcue.h"
#include "patch.h"
#include "uuid_cpp.h"

class MIDIengine {
public:
  MIDIengine();
  ~MIDIengine();

  Glib::Dispatcher signal_port_change;
  Glib::Dispatcher signal_go;

private:
  void midi_main();

  snd_seq_t *m_seq;
  int input_port;
  std::atomic<bool> running;

// MSC data
  bool use_msc;
  int msc_id;

  std::thread midi_thread_p;
};

//
// BIG TODO: merge running and waiting cues together to avoid copying
//

class RunningCue {
public:
  RunningCue() : cue_id_no(0), update_icon(true) {}
  void Pause()
  {
    if (fade) fade->pause();
    else if (af) af->pause();
    update_icon = true;
  }
  void Continue()
  {
    if (fade) fade->play();
    else if (af) af->play();
    update_icon = true;
  }
  void Stop()
  {
    if (fade) fade->stop();
    else if (af) af->stop();
  }
  int Get_status()
  {
    if (fade) return fade->status;
    else if (af) return af->status;
    else return Play;
  }

  uuid::uuid cue_id_no;
  Gtk::TreeRowReference r_cue;
  std::shared_ptr<AudioFile> af;
  std::shared_ptr<AudioFile::fade_> fade;
  bool update_icon;
};

class WaitingCue {
public:
  WaitingCue() : cue_id_no(0), done(false), active(true), update_icon(false) {}
  void Pause()
  {
    ptime.assign_current_time();
    active = false;
    update_icon = true;
  }
  void Continue()
  {
    Glib::TimeVal e = ptime - start;
    start += e;
    end += e;
    active = true;
    update_icon = true;
  }
  void Stop()
  {
    done = true;
  }

  Glib::TimeVal start;
  Glib::TimeVal end;
  Glib::TimeVal ptime;
  uuid::uuid cue_id_no;
  Gtk::TreeRowReference w_cue;
  bool done;
  bool active;
  bool update_icon;
};

class CueTreeStore : public Gtk::TreeStore {
private:
  CueTreeStore();
public:
  class ModelColumns : public Gtk::TreeModel::ColumnRecord {
  public:
    ModelColumns()
    {
      add(cue);
      add(image);
      add(pos_img);
      add(delay);
      add(delay_percent);
      add(qelapsed);
      add(qelapsed_percent);
    }

    Gtk::TreeModelColumn<std::shared_ptr<Cue> > cue;
    Gtk::TreeModelColumn<Glib::RefPtr<Gdk::Pixbuf> > pos_img;
    Gtk::TreeModelColumn<Glib::RefPtr<Gdk::Pixbuf> > image;
    Gtk::TreeModelColumn<Glib::ustring> delay;
    Gtk::TreeModelColumn<int> delay_percent;
    Gtk::TreeModelColumn<Glib::ustring> qelapsed;
    Gtk::TreeModelColumn<int> qelapsed_percent;
  };
  ModelColumns Col;

  Gtk::TreeModel::iterator get_iter_from_id(uuid::uuid id);

  static Glib::RefPtr<CueTreeStore> create();
protected:
  virtual bool row_drop_possible_vfunc(const Gtk::TreeModel::Path &dest, const Gtk::SelectionData &selection_data) const;
private:
  bool is_id(const TreeModel::iterator &i);
  uuid::uuid m_id;
  Gtk::TreeModel::iterator m_id_iter;
};

class CueTreeView : public Gtk::TreeView {
public:
  CueTreeView(BaseObjectType *cobject, const Glib::RefPtr<Gtk::Builder> &refXml);
  virtual ~CueTreeView();

  sigc::signal<void> signal_edit()
  {
    return m_signal_edit;
  }
  sigc::signal<void> signal_stop()
  {
    return m_signal_stop;
  }
  sigc::signal<void> signal_pause()
  {
    return m_signal_pause;
  }
  sigc::signal<void> signal_sneak_out()
  {
    return m_signal_sneak_out;
  }
protected:
  virtual bool on_button_press_event(GdkEventButton *event);
  virtual void on_drag_data_received(const Glib::RefPtr<Gdk::DragContext> &context,
                                     int x, int y, const Gtk::SelectionData &selection_data, guint info, guint time);

  void on_edit();
  void on_pause();
  void on_stop();
  void on_sneak_out();
private:
  bool on_col_drag(Gtk::TreeView *, Gtk::TreeViewColumn *, Gtk::TreeViewColumn *, Gtk::TreeViewColumn *);
  Glib::RefPtr<Gtk::Builder> m_refXml;
  Gtk::Menu *m_MenuPopup;
  Glib::RefPtr<Gtk::Builder> refPopupXml;

  sigc::signal<void> m_signal_edit;
  sigc::signal<void> m_signal_stop;
  sigc::signal<void> m_signal_pause;
  sigc::signal<void> m_signal_sneak_out;
};

class About : public Gtk::AboutDialog {
public:
  About(BaseObjectType *cobject, const Glib::RefPtr<Gtk::Builder> &refXml);
  virtual ~About() {}
  static std::unique_ptr<About> create();
protected:
  virtual void on_response(int);
  Glib::RefPtr<Gtk::Builder> m_refXml;
};

class App : public Gtk::Window {
public:
  App(BaseObjectType *cobject, const Glib::RefPtr<Gtk::Builder> &refXml);
  virtual ~App();

  void edit_cue()
  {
    on_edit_cue_activate();
  }
  Gtk::TreeModel::iterator replace_cue(std::shared_ptr<Cue> &q, Gtk::TreeRowReference &r);
  Gtk::TreeModel::iterator append_cue(std::shared_ptr<Cue> &q);
  Gtk::TreeModel::iterator append_cue(std::shared_ptr<Cue> &q, Gtk::TreeModel::iterator i);
  Gtk::TreeModel::iterator insert_cue(std::shared_ptr<Cue> &q);
  Gtk::TreeModel::iterator insert_cue(Gtk::TreeModel::iterator i, std::shared_ptr<Cue> &q);

  void do_load(const Glib::ustring &filename);
  Gtk::TreeModel::iterator go_cue(Gtk::TreeModel::iterator iter, bool run_all = false);

  MIDIengine *midi_engine;
protected:
  bool dis_update();
  bool update_status_bar();

  void on_view_item_activate(int, Glib::ustring);
  void on_properties_activate();
  void on_properties_hide();
  void on_preferences_activate();
  void on_preferences_hide();
  void on_patch_activate();
  void on_patch_hide();
  void on_new_activate();
  void on_open_activate();
  void on_recent_activate();
  void on_save_activate();
  void on_saveas_activate();
  void do_save();
  void do_save_cues(Gtk::TreeModel::Children , xmlpp::Element *);

  void on_new_cue_activate(int);
  void on_edit_cue_activate();
  void on_renumber_activate();
  void on_renumber_hide();
  void renumber();
  void on_lock_activate();
  void on_cut_cue_activate();

  void on_pause();
  void on_stop();
  void on_sneak_out();
  void on_go_activate();
  bool wait_timeout();

  void on_previous_activate();
  void on_next_activate();
  void on_load_activate();
  void on_all_stop_activate();

  void on_about_activate();
  void on_about_hide();

  virtual bool on_key_press_event(GdkEventKey *event);
public:
  std::string file;
  std::string title;
  Glib::ustring note;
  bool pb_pos_sel;

  Glib::RefPtr<CueTreeStore> m_refTreeModel;
  Gtk::TreeRowReference m_CurrentCue;

  std::list<RunningCue> running_cue;
  std::list<WaitingCue> waiting_cue;

  CueTreeView *m_treeview;

  std::list <std::shared_ptr<EditCue > > p_edit;

  Glib::RefPtr<Gtk::Builder> m_refXml;
private:
  void cell_data_func_type(Gtk::CellRenderer *cell, const Gtk::TreeModel::iterator &iter);
  void cell_data_func_wait(Gtk::CellRenderer *cell, const Gtk::TreeModel::iterator &iter);
  void cell_data_func_elapsed(Gtk::CellRenderer *cell, const Gtk::TreeModel::iterator &iter);
  void cell_data_func_text(Gtk::CellRenderer *cell, const Gtk::TreeModel::iterator &iter);
  void cell_data_func_cue(Gtk::CellRenderer *cell, const Gtk::TreeModel::iterator &iter);
  void cell_data_func_autoc(Gtk::CellRenderer *cell, const Gtk::TreeModel::iterator &iter);
  void cell_cue(const Glib::ustring &path, const Glib::ustring &text);
  void cell_text(const Glib::ustring &path, const Glib::ustring &text);
  void cell_autoc(const Glib::ustring &path);
  void disable_hotkeys(Gtk::CellEditable *, const Glib::ustring &);
  void enable_hotkeys();
  bool check_key(const Gtk::TreeModel::iterator &iter);

  std::unique_ptr<About> p_about;
  std::unique_ptr<Renumber> p_renumber;
  std::unique_ptr<Properties> p_properties;
  std::unique_ptr<Preferences> p_preferences;
  std::unique_ptr<Patch> p_patch;

  Glib::RefPtr<Gdk::Pixbuf> Pix_blank;
  Glib::RefPtr<Gdk::Pixbuf> Pix_play;
  Glib::RefPtr<Gdk::Pixbuf> Pix_pause;
  Glib::RefPtr<Gdk::Pixbuf> Pix_PBpos;

  Gtk::RecentFilter recent_filter;
  Gtk::RecentChooserMenu recent_menu_item;

  std::vector<Gtk::TreeViewColumn * > mCols;

  Gtk::Statusbar *p_appbar;
  sigc::connection dis_timer;

  Gdk::ModifierType state;
  guint keyval;
};

extern App *app;

#endif // APP_H_

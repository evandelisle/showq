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

#ifndef EDITCUE_H_
#define EDITCUE_H_

#include <memory>
#include <gtkmm.h>

#include "cue.h"
#include "audio.h"
#include "uuid_cpp.h"

class Fader : public Gtk::VScale {
public:
  Fader(BaseObjectType *cobject, const Glib::RefPtr<Gtk::Builder> &refXml);
  virtual ~Fader() {}

  double get_gain();
  void set_gain(double gain);

protected:
  virtual Glib::ustring on_format_value(double v);
};

class EditCueBase : public sigc::trackable {
public:
  EditCueBase();
  explicit EditCueBase(Gtk::Notebook *);
  virtual ~EditCueBase();

  virtual void get(std::shared_ptr<Cue> &p) = 0;
  virtual void set(std::shared_ptr<Cue> &p) = 0;
};

class EditCueFade : public EditCueBase {
public:
  EditCueFade();
  explicit EditCueFade(Gtk::Notebook *);
  virtual ~EditCueFade();

  void get(std::shared_ptr<Cue> &p) override;
  void set(std::shared_ptr<Cue> &p) override;

private:
  void wave_on_toggle(int fader);

  std::vector<Fader *> m_wave_faders;
  std::vector<Gtk::ToggleButton *> m_wave_but;

  Gtk::SpinButton *m_fade_time;
  Gtk::RadioButton *m_fade_defaultcomplete;
  Gtk::RadioButton *m_fade_stopcomplete;
  Gtk::RadioButton *m_fade_pausecomplete;

  Glib::RefPtr<Gtk::Builder> refXML_fade;
  Glib::RefPtr<Gtk::Builder> refXML_faders;
};

class EditCueWave : public EditCueBase {
public:
  explicit EditCueWave(Gtk::Notebook *);
  virtual ~EditCueWave();

  void get(std::shared_ptr<Cue> &p) override;
  void set(std::shared_ptr<Cue> &p) override;

private:
  bool dis_update();
  void wave_on_file_activate();
  void wave_on_value_change(int);
  void wave_on_play();
  void wave_on_pause();
  void wave_on_stop();
  bool sl_button_press_event(GdkEventButton *);
  bool sl_button_release_event(GdkEventButton *);
  bool sl_change_value(Gtk::ScrollType t, double v);
  Glib::ustring sl_format_value(double v);

  sigc::connection dis_timer;
  bool update_hs_ok;
  bool seek_lock;

  Gtk::FileChooserButton *m_wave_fentry;
  Gtk::SpinButton *m_wave_start;
  Gtk::HScale *m_wave_tslide;

  std::vector<Fader *> m_wave_faders;
  std::shared_ptr<AudioFile> m_af;

  Glib::RefPtr<Gtk::Builder> refXML_wave;
  Glib::RefPtr<Gtk::Builder> refXML_patch;
  Glib::RefPtr<Gtk::Builder> refXML_faders;
};

class EditCueMidi : public EditCueBase {
public:
  explicit EditCueMidi(Gtk::Notebook *);
  virtual ~EditCueMidi();

  void get(std::shared_ptr<Cue> &p) override;
  void set(std::shared_ptr<Cue> &p) override;

private:
  void on_add_clicked();
  void on_delete_clicked();
  void on_modify_clicked();
  void on_combo_changed();
  void on_msc_combo_changed();
  Glib::ustring get_line(const std::vector<unsigned char> &raw);
  void validate_qlist(const Glib::ustring &text, int *pos, Gtk::Entry *e);
  void validate_fields();
  void build_msc_timecode(std::vector<unsigned char> &raw);
  void on_selection_change();
  void set_msc_timecode(std::vector<unsigned char>::const_iterator raw);
  std::vector<unsigned char> get_raw();
  void validate_sysex_entry(const Glib::ustring &text, int *, Gtk::Entry *e);

  class ModelColumns : public Gtk::TreeModel::ColumnRecord {
  public:
    ModelColumns()
    {
      add(m_text);
      add(m_raw_data);
      add(m_port);
    }

    Gtk::TreeModelColumn<Glib::ustring> m_text;
    Gtk::TreeModelColumn<int> m_port;
    Gtk::TreeModelColumn<std::vector<unsigned char> > m_raw_data;
  };

  ModelColumns m_Columns;
  Glib::RefPtr<Gtk::ListStore> m_refTreeModel;
  Gtk::TreeView *m_MIDItree;

  Glib::RefPtr<Gtk::Builder> m_refXml;
};

class EditCueStop : public EditCueBase {
public:
  explicit EditCueStop(Gtk::Notebook *);
  virtual ~EditCueStop();

  void get(std::shared_ptr<Cue> &p) override;
  void set(std::shared_ptr<Cue> &p) override;
};

class EditCuePause : public EditCueBase {
public:
  explicit EditCuePause(Gtk::Notebook *);
  virtual ~EditCuePause();

  void get(std::shared_ptr<Cue> &p) override;
  void set(std::shared_ptr<Cue> &p) override;
};

class EditCueStart : public EditCueBase {
public:
  explicit EditCueStart(Gtk::Notebook *);
  virtual ~EditCueStart();

  void get(std::shared_ptr<Cue> &p) override;
  void set(std::shared_ptr<Cue> &p) override;
};

class EditCueGroup : public EditCueBase {
public:
  explicit EditCueGroup(Gtk::Notebook *);
  virtual ~EditCueGroup();

  void get(std::shared_ptr<Cue> &p) override;
  void set(std::shared_ptr<Cue> &p) override;

private:
  Glib::RefPtr<Gtk::Builder> refXml;
};

class EditCue : public Gtk::Window {
public:
  EditCue(BaseObjectType *cobject, const Glib::RefPtr<Gtk::Builder> &refXml);
  virtual ~EditCue();

  static EditCue *show(int type);
  static void show(std::shared_ptr<Cue> q, Gtk::TreeRowReference &r);
  static void show_on_hide();

protected:
  virtual bool on_key_press_event(GdkEventKey *);
  virtual bool on_key_release_event(GdkEventKey *event);

private:
  void ok_activate();
  void apply_activate();
  void cancel_activate();
  void trigger_key();
  bool get_target(const Gtk::TreeModel::iterator &i);
  bool check_key(const Gtk::TreeModel::iterator &i);

  Gtk::Entry *m_info_cueid;
  Gtk::Entry *m_info_text;
  Gtk::SpinButton *m_info_wait;
  Gtk::CheckButton *m_info_autocont;
  Gtk::TextView *m_info_note;
  Gtk::ToggleButton *m_key_but;
  Gtk::Entry *m_info_target;

  Gtk::Notebook *m_notebook;
  Glib::RefPtr<Gtk::Builder> m_refXml;

  Gtk::TreeRowReference m_path;
  uuid::uuid cue_id_no;
  uuid::uuid target;
//    std::shared_ptr<Cue> m_cue;
  int m_type;

  guint keyval;
  Gdk::ModifierType state;

  std::shared_ptr<EditCueBase> m_tabs;
};

#endif

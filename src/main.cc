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

#include "app.h"
#include "audio.h"
#include <glibmm/i18n.h>
#include <fstream>
#include <exception>
#include <iostream>

Glib::ustring showq_ui;
App * app;
Glib::KeyFile keyfile;

Audio * audio;

const char * glade_list[] = {
    "./ui/",
    "/home/errol/Projects/showq/src/ui/",
    "/usr/share/showq/ui/",
    "/usr/local/share/showq/ui/",
    0
};

static void on_activate_url_link(Gtk::AboutDialog& ,const Glib::ustring& link)
{
    std::string command = "xdg-open ";
    command += link;
    Glib::spawn_command_line_async(command);
}

int main(int argc, char *argv[])
{
    bindtextdomain(GETTEXT_PACKAGE, PROGRAMNAME_LOCALEDIR);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
    textdomain(GETTEXT_PACKAGE);

    Gtk::Main kit(argc, argv);

    if (!Glib::thread_supported()) Glib::thread_init();

    for (const char **p = glade_list; *p; ++p) {
	if (Glib::file_test(*p + std::string("app.ui"), Glib::FILE_TEST_EXISTS)) {
	    showq_ui = *p;
	}
    }

    try {
        keyfile.load_from_file(Glib::get_home_dir() + "/ShowQ.conf");
    }
    catch (...) {
    }

    Glib::RefPtr <Gtk::Builder> refXml;

    try {
	audio = new Audio;
	refXml = Gtk::Builder::create_from_file(showq_ui+"app.ui");
	refXml->get_widget_derived("app", app);
	if (app) {
	    try {
		bool load_last = keyfile.get_boolean("main", "LoadLast");
		if (load_last) {
		    Glib::ustring file = keyfile.get_string("main", "LastFile");
		    if (file != "") {
		        app->do_load(file);
		    }
		}
	    }
	    catch (...) {
	    }
	    Gtk::AboutDialog::set_url_hook(sigc::ptr_fun(&on_activate_url_link));
	    kit.run(*app);
	}
	delete audio;
	delete app;
    }
    catch (Gtk::BuilderError) {
        return 1;
    }
    catch (Audio::NoAudio) {
        Gtk::MessageDialog d(_("Show Q could not connect to JACK."), false, Gtk::MESSAGE_ERROR);
        d.set_secondary_text(_("(re)start JACK and try again"));
        d.run();
	return 1;
    }
    catch (std::exception & e) {
        std::cerr << e.what();
    }
    catch (...) {
        return 1;
    }

    try {
        Glib::ustring data = keyfile.to_data();
        std::ofstream fd((Glib::get_home_dir() + "/ShowQ.conf").c_str());
        fd << data;
    }
    catch (...) {
    }

    return (0);
}

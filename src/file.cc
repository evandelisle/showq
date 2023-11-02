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

#include "app.h"
#include "audio.h"
#include "cue.h"
#include "utils.h"
#include "uuid_cpp.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <libxml++/libxml++.h>
#pragma GCC diagnostic pop
#include <alsa/asoundlib.h>

#include <glib.h>
#include <glibmm.h>
#include <gtkmm.h>

#include <jack/jack.h>
#include <jack/types.h>

#include <algorithm>
#include <exception>
#include <istream>
#include <list>
#include <memory>
#include <cstdlib>
#include <string>
#include <vector>

extern snd_seq_t *oseq;

namespace
{
Glib::ustring to_ascii_string(long value, int base = 10)
{
    std::string buf;

    // check that the base if valid
    if (base < 2 || base > 16)
        return buf;

    buf.reserve(35);
    long quotient = value;

    // Translating number to string with base:
    do {
        buf += "0123456789abcdef"[std::abs(quotient % base)];
        quotient /= base;
    } while (quotient);

    // Append the negative sign
    if (value < 0)
        buf += '-';

    std::reverse(buf.begin(), buf.end());
    return buf;
}
} // namespace

void App::do_save()
{
    xmlpp::Document xml;

    xmlpp::Element *el = xml.create_root_node("SQproject");
    if (!title.empty())
        el->set_attribute("projname", title);
    el->set_attribute("sqversion", "0.1");

    if (!note.empty()) {
        xmlpp::Element *cel = el->add_child("note");
        cel->set_child_text(note);
    }

    audio->serialize(el);

    xmlpp::Element *MIDIpatch = el->add_child("MIDIpatch");
    snd_seq_port_info_t *info;
    snd_seq_port_info_alloca(&info);
    snd_seq_query_subscribe_t *pAlsaSubs;
    snd_seq_query_subscribe_alloca(&pAlsaSubs);

    for (int i = 1; i < 9; ++i) {
        if (snd_seq_get_port_info(oseq, i, info) == 0) {

            xmlpp::Element *cel = MIDIpatch->add_child("MIDIport");
            cel->set_attribute("port", to_ascii_string(i));
            cel->set_attribute("name", snd_seq_port_info_get_name(info));

            snd_seq_addr_t addr;
            addr.client = snd_seq_client_id(oseq);
            addr.port = i;
            snd_seq_query_subscribe_set_root(pAlsaSubs, &addr);
            snd_seq_query_subscribe_set_index(pAlsaSubs, 0);

            while (!snd_seq_query_port_subscribers(oseq, pAlsaSubs)) {
                const snd_seq_addr_t *connected_addr = snd_seq_query_subscribe_get_addr(pAlsaSubs);

                xmlpp::Element *con_el = cel->add_child("MIDIdest");

                con_el->set_attribute("client", to_ascii_string(connected_addr->client));
                con_el->set_attribute("port", to_ascii_string(connected_addr->port));

                snd_seq_query_subscribe_set_index(pAlsaSubs,
                                                  snd_seq_query_subscribe_get_index(pAlsaSubs) + 1);
            }
        }
    }

    const Gtk::TreeModel::Children children = m_refTreeModel->children();
    xmlpp::Element *cue_list = el->add_child("CueList");
    do_save_cues(children, cue_list);

    xml.write_to_file_formatted(file);

    set_title("ShowQ : " + Glib::filename_to_utf8(file));
}

void App::do_save_cues(Gtk::TreeModel::Children children, xmlpp::Element *cue_list)
{
    for (const auto &iter : children) {
        const std::shared_ptr<Cue> cue = iter[m_refTreeModel->Col.cue];

        xmlpp::Element *cel = cue_list->add_child("cue");

        cel->set_attribute("type", cue->cue_type_text());
        cel->set_attribute("id", cue->cue_id_no.unparse());

        if (!cue->cue_id.empty()) {
            xmlpp::Element *c = cel->add_child("cue_id");
            c->set_child_text(cue->cue_id);
        }
        if (!cue->text.empty()) {
            xmlpp::Element *c = cel->add_child("text");
            c->set_child_text(cue->text);
        }
        if (!cue->note.empty()) {
            xmlpp::Element *c = cel->add_child("note");
            c->set_child_text(cue->note);
        }
        if (cue->delay > 0.0001) {
            xmlpp::Element *c = cel->add_child("delay");
            c->set_child_text(Glib::Ascii::dtostr(cue->delay));
        }
        if (cue->autocont) {
            xmlpp::Element *c = cel->add_child("autocontinue");
            c->set_child_text("true");
        }

        if (cue->keyval) {
            xmlpp::Element *c = cel->add_child("Trigger");
            if (cue->keyval) {
                xmlpp::Element *t = c->add_child("Hotkey");
                t->set_child_text(Gtk::AccelGroup::name(cue->keyval, cue->state));
            }
        }

        cue->serialize(cel);

        const Gtk::TreeModel::Children &ch = iter.children();
        if (ch)
            do_save_cues(ch, cel);
    }
}

void Audio::serialize(xmlpp::Element *el)
{
    xmlpp::Element *Audiopatch = el->add_child("AudioPatch");
    for (int i = 0; i < 8; ++i) {
        xmlpp::Element *p = Audiopatch->add_child("AudioPort");

        p->set_attribute("port", to_ascii_string(i + 1));
        p->set_attribute("name", jack_port_short_name(ports[i]));

        const char **l = jack_port_get_connections(ports[i]);
        if (!l)
            continue;
        for (int j = 0; l[j]; ++j) {
            xmlpp::Element *c = p->add_child("AudioDest");
            c->set_attribute("name", l[j]);
        }
        free(l);
    }
}

void Cue::serialize(xmlpp::Element *)
{
}

void Group_Cue::serialize(xmlpp::Element *cel)
{
    xmlpp::Element *p = cel->add_child("Group");

    p->set_attribute("mode", to_ascii_string(mode));
}

void FadeStop_Cue::serialize(xmlpp::Element *cel)
{
    xmlpp::Element *p = cel->add_child("Fade");

    if (!target.is_null()) {
        p->set_attribute("target", target.unparse());
    }

    p->set_attribute("FadeTime", Glib::Ascii::dtostr(fade_time));

    if (stop_on_complete) {
        xmlpp::Element *c = p->add_child("StopOnComplete");
        c->set_child_text("true");
    }
    if (pause_on_complete) {
        xmlpp::Element *c = p->add_child("PauseOnComplete");
        c->set_child_text("true");
    }

    for (size_t j = 0; j < tvol.size(); ++j) {
        if (!on[j])
            continue;
        xmlpp::Element *c = p->add_child("level");

        Glib::ustring s2;

        if (gain_to_db(tvol[j]) < -120)
            s2 = "-Inf";
        else
            s2 = Glib::Ascii::dtostr(gain_to_db(tvol[j]));
        c->set_attribute("ch", to_ascii_string(j));
        c->set_attribute("gain", s2);
    }
}

void Stop_Cue::serialize(xmlpp::Element *cel)
{
    xmlpp::Element *c = cel->add_child("Stop");
    if (!target.is_null()) {
        c->set_attribute("target", target.unparse());
    }
}

void Pause_Cue::serialize(xmlpp::Element *cel)
{
    xmlpp::Element *c = cel->add_child("Pause");
    if (!target.is_null()) {
        c->set_attribute("target", target.unparse());
    }
}

void Start_Cue::serialize(xmlpp::Element *cel)
{
    xmlpp::Element *c = cel->add_child("Start");
    if (!target.is_null()) {
        c->set_attribute("target", target.unparse());
    }
}

void Wave_Cue::serialize(xmlpp::Element *cel)
{
    xmlpp::Element *p = cel->add_child("Wave");
    if (!file.empty()) {
        xmlpp::Element *c = p->add_child("filename");
        c->set_child_text(file);
    }
    if (start_time > 0.0001) {
        xmlpp::Element *c = p->add_child("start");
        c->set_child_text(Glib::Ascii::dtostr(start_time));
    }

    for (auto &i : patch) {
        xmlpp::Element *c = p->add_child("patch");

        c->set_attribute("src", to_ascii_string(i.src));
        c->set_attribute("dest", to_ascii_string(i.dest));
    }

    for (size_t j = 0; j < vol.size(); ++j) {
        if (gain_to_db(vol[j]) < -120)
            continue;

        xmlpp::Element *c = p->add_child("level");

        c->set_attribute("ch", to_ascii_string(j));
        c->set_attribute("gain", Glib::Ascii::dtostr(gain_to_db(vol[j])));
    }
}

void MIDI_Cue::serialize(xmlpp::Element *cel)
{
    for (auto &k : msgs) {
        xmlpp::Element *p = cel->add_child("MIDI_data");
        p->set_attribute("port", to_ascii_string(k.port));
        Glib::ustring s;
        for (auto &i : k.midi_data) {
            s += to_ascii_string(int(i)) + ' ';
        }
        p->set_child_text(s);
    }
}

// Parse the show XML file

class Deserialize {
public:
    void parse(const xmlpp::DomParser &parser);

private:
    void audio_patch(const xmlpp::Node *node);
    void midi_patch(const xmlpp::Element *node);
    void note(const xmlpp::Element *node);
    void common_cue_items(const xmlpp::Element *el, std::shared_ptr<Cue> cue);
    void midi(const xmlpp::Element *el, std::shared_ptr<MIDI_Cue> cue);
    void fade(const xmlpp::Element *el, std::shared_ptr<FadeStop_Cue> cue);
    void wave(const xmlpp::Element *el, std::shared_ptr<Wave_Cue> cue);
    void stop(const xmlpp::Element *el, std::shared_ptr<Stop_Cue> cue);
    void pause(const xmlpp::Element *el, std::shared_ptr<Pause_Cue> cue);
    void start(const xmlpp::Element *el, std::shared_ptr<Start_Cue> cue);
    void group(const xmlpp::Element *el, std::shared_ptr<Group_Cue> cue);
    void cue_list(const xmlpp::Node *node);

    std::vector<Gtk::TreeModel::iterator> m_iters;
};

void Deserialize::audio_patch(const xmlpp::Node *node)
{
    audio->disconnect_all();

    for (const auto &child : node->get_children()) {
        const auto *element = dynamic_cast<const xmlpp::Element *>(child);
        if (element && element->get_name() == "AudioPort") {
            int port = -1;
            Glib::ustring name;

            for (const auto &attribute : element->get_attributes()) {
                if (attribute->get_name() == "port") {
                    port = g_ascii_strtoll(attribute->get_value().c_str(), nullptr, 0) - 1;
                } else if (attribute->get_name() == "name") {
                    name = attribute->get_value();
                }
                if (port >= 0)
                    audio->port_set_name(port, name);
            }
            const auto *dest_node = child->get_first_child("AudioDest");
            const auto *dest_element = dynamic_cast<const xmlpp::Element *>(dest_node);
            if (dest_element) {
                const auto *dest_attribute = dest_element->get_attribute("name");
                if (port >= 0 && dest_attribute)
                    audio->connect(port, dest_attribute->get_value());
            }
        }
    }
}

void Deserialize::midi_patch(const xmlpp::Element *node)
{
    const auto port_str = node->get_attribute_value("port");
    const auto name_str = node->get_attribute_value("name");

    if (port_str.empty() || name_str.empty())
        return;

    const int port = g_ascii_strtoll(port_str.c_str(), nullptr, 0);
    snd_seq_port_info_t *info;
    snd_seq_port_info_alloca(&info);
    snd_seq_port_info_set_capability(info, SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ);
    snd_seq_port_info_set_port_specified(info, 1);
    snd_seq_port_info_set_port(info, port);
    snd_seq_port_info_set_type(info, SND_SEQ_PORT_TYPE_MIDI_GENERIC);
    snd_seq_port_info_set_name(info, name_str.c_str());
    snd_seq_create_port(oseq, info);

    for (const auto &child : node->get_children()) {
        const auto *element = dynamic_cast<const xmlpp::Element *>(child);
        if (element && element->get_name() == "MIDIdest") {
            const auto client_str = element->get_attribute_value("client");
            const auto port_str = element->get_attribute_value("client");
            if (client_str.empty() || port_str.empty())
                continue;

            snd_seq_connect_to(oseq,
                               port,
                               g_ascii_strtoll(client_str.c_str(), nullptr, 0),
                               g_ascii_strtoll(port_str.c_str(), nullptr, 0));
        }
    }
}

void Deserialize::note(const xmlpp::Element *node)
{
    if (!node->has_child_text())
        return;
    const auto *child_text_node = node->get_child_text();
    app->note = child_text_node->get_content();
}

void Deserialize::common_cue_items(const xmlpp::Element *el, std::shared_ptr<Cue> cue)
{
    for (const auto &child_node : el->get_children()) {
        const auto &child_element = dynamic_cast<const xmlpp::Element *>(child_node);
        if (!child_element)
            continue;

        const auto name = child_element->get_name();

        if (name == "text") {
            if (child_element->has_child_text())
                cue->text = child_element->get_child_text()->get_content();
        } else if (name == "cue_id") {
            if (child_element->has_child_text())
                cue->cue_id = child_element->get_child_text()->get_content();
        } else if (name == "note") {
            if (child_element->has_child_text())
                cue->note = child_element->get_child_text()->get_content();
        } else if (name == "delay") {
            if (child_element->has_child_text()) {
                const auto text = child_element->get_child_text()->get_content();
                cue->delay = Glib::Ascii::strtod(text);
            }
        } else if (name == "autocontinue") {
            if (child_element->has_child_text()) {
                if (child_element->get_child_text()->get_content() == "true")
                    cue->autocont = true;
            }
        } else if (name == "Trigger") {
            const auto *trigger_node = child_node->get_first_child("Hotkey");
            const auto *trigger_element = dynamic_cast<const xmlpp::Element *>(trigger_node);
            if (trigger_element && trigger_element->has_child_text()) {
                const auto text = trigger_element->get_child_text()->get_content();
                Gtk::AccelGroup::parse(text, cue->keyval, cue->state);
            }
        }
    }
}

void Deserialize::midi(const xmlpp::Element *el, std::shared_ptr<MIDI_Cue> cue)
{
    for (const auto &child_node : el->get_children()) {
        const auto &child_element = dynamic_cast<const xmlpp::Element *>(child_node);
        if (!child_element)
            continue;

        if (child_element->get_name() == "MIDI_data") {
            const auto port = child_element->get_attribute_value("port");
            const auto data = child_element->get_child_text()->get_content();
            MIDI_Cue::msg m;
            std::stringstream s(data);
            std::string i;
            while (s >> i) {
                m.midi_data.push_back(g_ascii_strtoll(i.c_str(), nullptr, 0));
            }
            m.port = g_ascii_strtoll(port.c_str(), nullptr, 0);
            if (!m.midi_data.empty())
                cue->msgs.push_back(m);
        }
    }
}

void Deserialize::fade(const xmlpp::Element *el, std::shared_ptr<FadeStop_Cue> cue)
{
    const auto *fade_node = el->get_first_child("Fade");
    if (!fade_node)
        return;

    const auto *fade_node_element = dynamic_cast<const xmlpp::Element *>(fade_node);
    const auto target = fade_node_element->get_attribute_value("target");
    const auto fade_time = fade_node_element->get_attribute_value("FadeTime");

    cue->target = uuid::uuid(target);
    cue->fade_time = Glib::Ascii::strtod(fade_time);

    for (const auto &child_node : fade_node->get_children()) {
        const auto *child_element = dynamic_cast<const xmlpp::Element *>(child_node);
        if (!child_element)
            continue;

        const auto name = child_element->get_name();
        if (name == "StopOnComplete") {
            if (child_element->has_child_text()) {
                const auto text = child_element->get_child_text()->get_content();
                if (text == "true")
                    cue->stop_on_complete = true;
            }
        } else if (name == "PauseOnComplete") {
            if (child_element->has_child_text()) {
                const auto text = child_element->get_child_text()->get_content();
                if (text == "true")
                    cue->pause_on_complete = true;
            }
        } else if (name == "level") {
            const auto ch = child_element->get_attribute_value("ch");
            const auto gain = child_element->get_attribute_value("gain");

            if (ch.empty() || gain.empty())
                continue;

            const int channel = g_ascii_strtoll(ch.c_str(), nullptr, 10);
            if (channel < 0 || channel > 8)
                continue;

            cue->on[channel] = true;
            cue->tvol[channel] = gain == "-Inf" ? 0.0 : db_to_gain(Glib::Ascii::strtod(gain));
        }
    }
}

void Deserialize::wave(const xmlpp::Element *el, std::shared_ptr<Wave_Cue> cue)
{
    const auto *wave_node = el->get_first_child("Wave");
    if (!wave_node)
        return;

    for (const auto &child_node : wave_node->get_children()) {
        const auto &child_element = dynamic_cast<const xmlpp::Element *>(child_node);
        if (!child_element)
            continue;

        const auto name = child_element->get_name();

        if (name == "patch") {
            const auto src = child_element->get_attribute_value("src");
            const auto dest = child_element->get_attribute_value("dest");
            struct patch_ p;
            if (src.empty() || dest.empty())
                continue;
            p.src = g_ascii_strtoll(src.c_str(), nullptr, 10);
            p.dest = g_ascii_strtoll(dest.c_str(), nullptr, 10);
            cue->patch.push_back(p);
        } else if (name == "level") {
            const auto ch = child_element->get_attribute_value("ch");
            const auto gain = child_element->get_attribute_value("gain");
            if (ch.empty() || gain.empty())
                continue;
            const int channel = g_ascii_strtoll(ch.c_str(), nullptr, 10);
            if (channel < 0 || channel > 8)
                continue;
            cue->vol[channel] = db_to_gain(Glib::Ascii::strtod(gain));
        } else if (name == "filename") {
            if (child_element->has_child_text())
                cue->file = child_element->get_child_text()->get_content();
        } else if (name == "start") {
            if (child_element->has_child_text()) {
                const auto text = child_element->get_child_text()->get_content();
                cue->start_time = Glib::Ascii::strtod(text);
            }
        }
    }
}

void Deserialize::stop(const xmlpp::Element *el, std::shared_ptr<Stop_Cue> cue)
{
    const auto *node = el->get_first_child("Stop");
    if (!node)
        return;
    const auto *element = dynamic_cast<const xmlpp::Element *>(node);
    if (element) {
        const auto text = element->get_attribute_value("target");
        cue->target = uuid::uuid(text);
    }
}

void Deserialize::pause(const xmlpp::Element *el, std::shared_ptr<Pause_Cue> cue)
{
    const auto *node = el->get_first_child("Pause");
    if (!node)
        return;
    const auto *element = dynamic_cast<const xmlpp::Element *>(node);
    if (element) {
        const auto text = element->get_attribute_value("target");
        cue->target = uuid::uuid(text);
    }
}

void Deserialize::start(const xmlpp::Element *el, std::shared_ptr<Start_Cue> cue)
{
    const auto *node = el->get_first_child("Start");
    if (!node)
        return;
    const auto* element = dynamic_cast<const xmlpp::Element *>(node);
    if (element) {
        const auto text = element->get_attribute_value("target");
        cue->target = uuid::uuid(text);
    }
}

void Deserialize::group(const xmlpp::Element *el, std::shared_ptr<Group_Cue> cue)
{
    const auto* node = el->get_first_child("Group");
    if (!node)
        return;
    const auto* element = dynamic_cast<const xmlpp::Element *>(node);
    if (element) {
        const auto text = element->get_attribute_value("mode");
        cue->mode = g_ascii_strtoll(text.c_str(), nullptr, 10);
    }
}

void Deserialize::cue_list(const xmlpp::Node *node)
{
    for (const auto &child : node->get_children()) {
        const auto *cue_element = dynamic_cast<const xmlpp::Element *>(child);
        if (!cue_element || cue_element->get_name() != "cue")
            continue;

        const auto type = cue_element->get_attribute_value("type");
        const auto id = cue_element->get_attribute_value("id");
        if (type.empty() || id.empty())
            continue;

        std::shared_ptr<Cue> cue;
        if (type == "MIDI") {
            cue = std::make_shared<MIDI_Cue>();
            midi(cue_element, std::static_pointer_cast<MIDI_Cue>(cue));
        } else if (type == "Wave") {
            cue = std::make_shared<Wave_Cue>();
            wave(cue_element, std::static_pointer_cast<Wave_Cue>(cue));
        } else if (type == "Stop") {
            cue = std::make_shared<Stop_Cue>();
            stop(cue_element, std::static_pointer_cast<Stop_Cue>(cue));
        } else if (type == "Fade") {
            cue = std::make_shared<FadeStop_Cue>();
            fade(cue_element, std::static_pointer_cast<FadeStop_Cue>(cue));
        } else if (type == "Group") {
            cue = std::make_shared<Group_Cue>();
            group(cue_element, std::static_pointer_cast<Group_Cue>(cue));
        } else if (type == "Pause") {
            cue = std::make_shared<Pause_Cue>();
            pause(cue_element, std::static_pointer_cast<Pause_Cue>(cue));
        } else if (type == "Start") {
            cue = std::make_shared<Start_Cue>();
            start(cue_element, std::static_pointer_cast<Start_Cue>(cue));
        }

        cue->cue_id_no = uuid::uuid(id);
        common_cue_items(cue_element, cue);

        Gtk::TreeModel::iterator tree_iter;
        if (m_iters.empty()) {
            tree_iter = app->append_cue(cue);
        } else {
            tree_iter = app->append_cue(cue, m_iters.back());
        }

        if (type == "Group") {
            m_iters.push_back(tree_iter);
            cue_list(child);
        }
    }
}

void Deserialize::parse(const xmlpp::DomParser &parser)
{
    const auto *node = parser.get_document()->get_root_node();
    if (node->get_name() != "SQproject")
        return;

    const auto *node_element = dynamic_cast<const xmlpp::Element *>(node);
    if (node_element) {
        const auto *name_attribute = node_element->get_attribute("projname");
        if (name_attribute)
            app->title = name_attribute->get_value();
    }

    if (node) {
        for (const auto &child : node->get_children()) {
            const auto *element = dynamic_cast<const xmlpp::Element *>(child);
            if (!element)
                continue;

            const auto name = child->get_name();
            if (name == "AudioPatch") {
                audio_patch(element);
            } else if (name == "MIDIpatch") {
                midi_patch(element);
            } else if (name == "note") {
                note(element);
            } else if (name == "CueList") {
                cue_list(element);
            }
        }
    }
}

void App::do_load(const Glib::ustring &filename)
{
    try {
        // Clear previous
        file = filename;
        title.clear();
        note.clear();
        m_refTreeModel->clear();

        xmlpp::DomParser parser;
        parser.set_throw_messages(true);
        parser.parse_file(filename);

        if (parser) {
            Deserialize show_file;
            show_file.parse(parser);
        }

        // Show what we have loaded
        set_title("ShowQ : " + Glib::filename_to_utf8(file));

        const Glib::RefPtr<Gtk::RecentManager> pRM = Gtk::RecentManager::get_default();
        pRM->add_item(Glib::filename_to_uri(filename));
    }
    catch (const std::exception &ex) {
        // Unable to load a show file
    }
}

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

#include <sstream>
#include <iostream>
#include <libxml++/libxml++.h>
#include <alsa/asoundlib.h>

#include "app.h"
#include "cue.h"
#include "utils.h"
#include "audio.h"

extern snd_seq_t * oseq;

void App::do_save()
{
    xmlpp::Document xml;

    xmlpp::Element *el = xml.create_root_node("SQproject");
    if (title.size())
        el->set_attribute("projname", title);
    el->set_attribute("sqversion", "0.1");

    if (note.size()) {
        xmlpp::Element * cel = el->add_child("note");
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
	    std::stringstream s;
	    s << i;
	    xmlpp::Element * cel = MIDIpatch->add_child("MIDIport");
	    cel->set_attribute("port", s.str());
	    cel->set_attribute("name", snd_seq_port_info_get_name(info));

	    snd_seq_addr_t addr;
	    addr.client = snd_seq_client_id(oseq);
	    addr.port = i;
	    snd_seq_query_subscribe_set_root(pAlsaSubs, &addr);
	    snd_seq_query_subscribe_set_index(pAlsaSubs, 0);

	    while(!snd_seq_query_port_subscribers(oseq, pAlsaSubs)) {
		const snd_seq_addr_t* connected_addr
		    = snd_seq_query_subscribe_get_addr(pAlsaSubs);

		xmlpp::Element * conel = cel->add_child("MIDIdest");
		std::stringstream a;
		std::stringstream b;

		a << int (connected_addr->client);
		b << int (connected_addr->port);
		conel->set_attribute("client", a.str());
		conel->set_attribute("port", b.str());

		snd_seq_query_subscribe_set_index(pAlsaSubs,
		    snd_seq_query_subscribe_get_index(pAlsaSubs) + 1);
	    }
	}
    }

    Gtk::TreeModel::Children children = m_refTreeModel->children();
    xmlpp::Element * cuelist = el->add_child("CueList");
    do_save_cues(children, cuelist);

    xml.write_to_file_formatted(file);
}

void App::do_save_cues(Gtk::TreeModel::Children children, xmlpp::Element * cuelist)
{
    for (Gtk::TreeModel::iterator iter = children.begin(); iter != children.end(); ++iter){
	boost::shared_ptr<Cue> cue = (*iter)[m_refTreeModel->Col.cue];

	xmlpp::Element *cel = cuelist->add_child("cue");
	std::ostringstream s;

	s << cue->cue_id_no;
	cel->set_attribute("type", cue->cue_type_text());
	cel->set_attribute("id", s.str());

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
	    std::stringstream s;
	    s << cue->delay;
	    c->set_child_text(s.str());
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

        Gtk::TreeModel::Children ch = iter->children();
        if (ch) do_save_cues(ch, cel);
    }
}

void Audio::serialize(xmlpp::Element * el)
{
    xmlpp::Element *Audiopatch = el->add_child("AudioPatch");
    for (int i = 0; i < 8; ++i) {
        xmlpp::Element *p = Audiopatch->add_child("AudioPort");
	std::stringstream s;
	s << (i + 1);
	p->set_attribute("port", s.str());
	p->set_attribute("name", jack_port_short_name(ports[i]));

	const char** l = jack_port_get_connections(ports[i]);
	if (!l) continue;
	for (int j = 0; l[j]; ++j) {
	    xmlpp::Element * c = p->add_child("AudioDest");
	    c->set_attribute("name", l[j]);
	}
	free(l);
    }
}

void Cue::serialize(xmlpp::Element *)
{
}

void Group_Cue::serialize(xmlpp::Element * cel)
{
    xmlpp::Element *p = cel->add_child("Group");
    std::stringstream s;
    s << mode;
    p->set_attribute("mode", s.str());
}

void FadeStop_Cue::serialize(xmlpp::Element *cel)
{
    xmlpp::Element *p = cel->add_child("Fade");

    if (target) {
        std::stringstream s;
        s << target;
        p->set_attribute("target", s.str());
    }
    std::stringstream s;
    s << fade_time;
    p->set_attribute("FadeTime", s.str());

    if (stop_on_complete) {
        xmlpp::Element *c = p->add_child("StopOnComplete");
        c->set_child_text("true");
    }
    if (pause_on_complete) {
        xmlpp::Element *c = p->add_child("PauseOnComplete");
        c->set_child_text("true");
    }

    for (size_t j = 0; j < tvol.size(); ++j) {
	if (on[j] == false) continue;
	xmlpp::Element *c = p->add_child("level");
	std::stringstream s1;
	std::stringstream s2;
	s1 << j;
	if (gain_to_db(tvol[j]) < -120)
	    s2 << "-Inf";
	else
        s2 << gain_to_db(tvol[j]);
        c->set_attribute("ch", s1.str());
        c->set_attribute("gain", s2.str());
    }
}

void Stop_Cue::serialize(xmlpp::Element *cel)
{
    long i = target;
    if (i) {
        xmlpp::Element *c = cel->add_child("Stop");
        std::stringstream s;
        s << i;
        c->set_attribute("target", s.str());
    }
}

void Pause_Cue::serialize(xmlpp::Element *cel)
{
    long i = target;
    if (i) {
        xmlpp::Element *c = cel->add_child("Pause");
        std::stringstream s;
        s << i;
        c->set_attribute("target", s.str());
    }
}

void Start_Cue::serialize(xmlpp::Element *cel)
{
    long i = target;
    if (i) {
        xmlpp::Element *c = cel->add_child("Start");
        std::stringstream s;
        s << i;
        c->set_attribute("target", s.str());
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
        std::stringstream s;
        s << start_time;
        c->set_child_text(s.str());
    }

    std::vector<patch_>::const_iterator i = patch.begin();
    for ( ; i != patch.end(); ++i) {
        xmlpp::Element *c = p->add_child("patch");
        std::stringstream s1;
        std::stringstream s2;
        s1 << i->src;
        s2 << i->dest;
        c->set_attribute("src", s1.str());
        c->set_attribute("dest", s2.str());
    }

    for (size_t j = 0; j < vol.size(); ++j) {
        if (gain_to_db(vol[j]) < -120) continue;

        xmlpp::Element *c = p->add_child("level");
        std::stringstream s1;
        std::stringstream s2;
        s1 << j;
        s2 << gain_to_db(vol[j]);
        c->set_attribute("ch", s1.str());
        c->set_attribute("gain", s2.str());
    }
}

void MIDI_Cue::serialize(xmlpp::Element *cel)
{
    std::vector<msg>::const_iterator k = msgs.begin();
    for ( ; k != msgs.end(); ++k) {
        xmlpp::Element *p = cel->add_child("MIDI_data");
        std::stringstream s1;
	s1 << k->port;
	p->set_attribute("port", s1.str());
	std::stringstream s;
	std::vector<unsigned char>::const_iterator i = k->midi_data.begin();
	for ( ; i != k->midi_data.end(); ++i) {
	    s << int(*i) << ' ';
	}
	p->set_child_text(s.str());
    }
}

void OpenParser::on_start_document()
{
}

void OpenParser::on_end_document()
{
}

void OpenParser::on_start_element(const Glib::ustring &name, const AttributeList &attributes)
{
    if (name == "SQproject") {
	AttributeList::const_iterator iter = attributes.begin();
	for (; iter != attributes.end(); ++iter) {
	    if (iter->name == "projname")
		app->title = iter->value;
	}
	return;
    }

    if (name == "AudioPatch") {
        audio->disconnect_all();
    }
    if (name == "AudioPort") {
        AttributeList::const_iterator iter = attributes.begin();
	Glib::ustring name;
	for (; iter != attributes.end(); ++iter) {
	    if (iter->name == "port")
	        port = strtol(iter->value.c_str(), 0, 0) - 1;
	    if (iter->name == "name")
	        name = iter->value;
	}
        audio->port_set_name(port, name);
    }

    if (name == "AudioDest") {
	AttributeList::const_iterator iter = attributes.begin();
	Glib::ustring name;
	for (; iter != attributes.end(); ++iter) {
	    if (iter->name == "name")
		name = iter->value;
	}
	audio->connect(port, name);
    }

    if (name == "MIDIport") {
	AttributeList::const_iterator iter = attributes.begin();
	Glib::ustring name;
	for (; iter != attributes.end(); ++iter) {
	    if (iter->name == "port")
		port = strtol(iter->value.c_str(), 0, 0);
	    if (iter->name == "name")
		name = iter->value;
	}
	snd_seq_port_info_t *info;
	snd_seq_port_info_alloca(&info);
	snd_seq_port_info_set_capability(info, SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ);
	snd_seq_port_info_set_port_specified(info, 1);
	snd_seq_port_info_set_port(info, port);
	snd_seq_port_info_set_type(info, SND_SEQ_PORT_TYPE_MIDI_GENERIC);
	snd_seq_port_info_set_name(info, name.c_str());
	snd_seq_create_port(oseq, info);
	return;
    }

    if (name == "MIDIdest") {
	AttributeList::const_iterator iter = attributes.begin();
	int d_client = -1;
	int d_port = -1;
	for (; iter != attributes.end(); ++iter) {
	    if (iter->name == "client")
		d_client = strtol(iter->value.c_str(), 0, 0);
	    if (iter->name == "port")
		d_port = strtol(iter->value.c_str(), 0, 0);
	}
	if (d_client != -1 && d_port != -1)
	    snd_seq_connect_to(oseq, port, d_client, d_port);
	return;
    }

    if (name == "cue") {
	long cue_id_no = 0;
	boost::shared_ptr<Cue> cue;

	AttributeList::const_iterator iter = attributes.begin();
	for (; iter != attributes.end(); ++iter) {
	    if (iter->name == "type") {
		if (iter->value == "MIDI")
		    cue = boost::shared_ptr<MIDI_Cue>(new MIDI_Cue);
		else if (iter->value == "Wave")
		    cue = boost::shared_ptr<Wave_Cue>(new Wave_Cue);
		else if (iter->value == "Stop")
		    cue = boost::shared_ptr<Stop_Cue>(new Stop_Cue);
		else if (iter->value == "Fade")
		    cue = boost::shared_ptr<FadeStop_Cue>(new FadeStop_Cue);
		else if (iter->value == "Group")
		    cue = boost::shared_ptr<Group_Cue>(new Group_Cue);
                else if (iter->value == "Pause")
                    cue = boost::shared_ptr<Pause_Cue>(new Pause_Cue);
                else if (iter->value == "Start")
                    cue = boost::shared_ptr<Start_Cue>(new Start_Cue);
	    }
	    if (iter->name == "id")
		cue_id_no = atol(iter->value.c_str());
	}
        if (cue) {
            cue->cue_id_no = cue_id_no;
            m_cues.push_back(cue);
            if (m_iters.size() == 0)
                m_iters.push_back(app->append_cue(cue));
            else
                m_iters.push_back(app->append_cue(cue, m_iters[m_iters.size() - 1]));
        }

        return;
    }

    if (name == "cue_id" || name == "text" || name == "note" || name == "delay"
        || name == "autocontinue") {
        m_text.clear();
        return;
    }

    if (m_cues.size() == 0) return;
    boost::shared_ptr<Cue> cue = m_cues[m_cues.size() - 1];

    boost::shared_ptr<Group_Cue> pg = boost::dynamic_pointer_cast<Group_Cue>(cue);
    if (pg && name == "Group") {
        AttributeList::const_iterator iter = attributes.begin();
	for (; iter != attributes.end(); ++iter) {
	    if (iter->name == "mode")
	        pg->mode = atol(iter->value.c_str());
	}
	return;
    }

    boost::shared_ptr<Start_Cue> pst = boost::dynamic_pointer_cast<Start_Cue>(cue);
    if (pst && name == "Start") {
	AttributeList::const_iterator iter = attributes.begin();
	for (; iter != attributes.end(); ++iter) {
	    if (iter->name == "target")
		pst->target = atol(iter->value.c_str());
	}
	return;
    }

    boost::shared_ptr<Pause_Cue> pp = boost::dynamic_pointer_cast<Pause_Cue>(cue);
    if (pp && name == "Pause") {
	AttributeList::const_iterator iter = attributes.begin();
	for (; iter != attributes.end(); ++iter) {
	    if (iter->name == "target")
		pp->target = atol(iter->value.c_str());
	}
	return;
    }

    boost::shared_ptr<Stop_Cue> ps = boost::dynamic_pointer_cast<Stop_Cue>(cue);
    if (ps && name == "Stop") {
	AttributeList::const_iterator iter = attributes.begin();
	for (; iter != attributes.end(); ++iter) {
	    if (iter->name == "target")
		ps->target = atol(iter->value.c_str());
	}
	return;
    }

    boost::shared_ptr<Wave_Cue> pw = boost::dynamic_pointer_cast<Wave_Cue>(cue);
    if (pw){
	if (name == "patch") {
	    AttributeList::const_iterator iter = attributes.begin();
	    struct patch_ p;
	    for (; iter != attributes.end(); ++iter) {
		if (iter->name == "src")
		    p.src = atol(iter->value.c_str());
		if (iter->name == "dest")
		    p.dest = atol(iter->value.c_str());
	    }
	    pw->patch.push_back(p);
	}
	if (name == "level") {
	    AttributeList::const_iterator iter = attributes.begin();
	    int ch = -1;
	    double gain = 0.0;
	    for (; iter != attributes.end(); ++iter) {
		if (iter->name == "ch")
		    ch = atol(iter->value.c_str());
		if (iter->name == "gain")
		    gain = strtod(iter->value.c_str(), 0);
	    }
	    if (ch != -1)
	        pw->vol[ch] = db_to_gain(gain);
	}
	if (name == "filename" || name == "start")
	    m_text.clear();
    }

    boost::shared_ptr<MIDI_Cue> pm = boost::dynamic_pointer_cast<MIDI_Cue>(cue);
    if (pm) {
	if (name == "MIDI_data") {
	    AttributeList::const_iterator iter = attributes.begin();
	    for (; iter != attributes.end(); ++iter) {
	        if (iter->name == "port")
	            midi_port = atol(iter->value.c_str());
	    }
	    m_text.clear();
	}
    }

    boost::shared_ptr<FadeStop_Cue> pf = boost::dynamic_pointer_cast<FadeStop_Cue>(cue);
    if (pf) {
	if (name == "Fade") {
	    AttributeList::const_iterator iter = attributes.begin();
	    for (; iter != attributes.end(); ++iter) {
		if (iter->name == "target")
		    pf->target = atol(iter->value.c_str());
		if (iter->name == "FadeTime")
		    pf->fade_time = strtod(iter->value.c_str(), 0);
	    }
	}
	if (name == "level") {
	    AttributeList::const_iterator iter = attributes.begin();
	    int ch = -1;
	    double gain = 0.0;
	    for (; iter != attributes.end(); ++iter) {
		if (iter->name == "ch")
		    ch = atol(iter->value.c_str());
		if (iter->name == "gain") {
		    if (iter->value == "-Inf")
			gain = -500;
		    else
			gain = strtod(iter->value.c_str(), 0);
		}
	    }
	    if (pf->tvol.size() < 8) pf->tvol.resize(8);
	    if (pf->on.size() < 8) pf->on.resize(8);
	    if (ch != -1) {
	        pf->on[ch] = true;
	        pf->tvol[ch] = gain < -200 ? 0.0 : db_to_gain(gain);
	    }
	}
	if (name == "StopOnComplete" || name == "PauseOnComplete")
	    m_text.clear();
    }
}

void OpenParser::on_end_element(const Glib::ustring &name)
{
    if (m_cues.size() == 0) {
	if (name == "note")
	    app->note = m_text;
	return;
    }

    boost::shared_ptr<Cue> cue = m_cues[m_cues.size() - 1];
    if (name == "cue") {
	m_cues.pop_back();
	m_iters.pop_back();
	return;
    }

    if (name == "cue_id") cue->cue_id = m_text;
    if (name == "text") cue->text = m_text;
    if (name == "note") cue->note = m_text;
    if (name == "delay") cue->delay = atof(m_text.c_str());
    if (name == "autocontinue" && m_text == "true") cue->autocont = true;
    if (name == "Hotkey") Gtk::AccelGroup::parse(m_text, cue->keyval, cue->state);

    boost::shared_ptr<Wave_Cue> pw = boost::dynamic_pointer_cast<Wave_Cue>(cue);
    if (pw) {
	if (name == "filename") pw->file = m_text;
	if (name == "start") pw->start_time = atof(m_text.c_str());
	return;
    }

    boost::shared_ptr<MIDI_Cue> pm = boost::dynamic_pointer_cast<MIDI_Cue>(cue);
    if (pm) {

	if (name == "MIDI_data") {
	    MIDI_Cue::msg m;
	    std::stringstream s(m_text);
	    std::string i;
	    while (s >> i) {
		m.midi_data.push_back(strtol(i.c_str(), 0, 0));
	    }
	    m.port = midi_port;
	    if (m.midi_data.size())
		pm->msgs.push_back(m);
	}
    }

    boost::shared_ptr<FadeStop_Cue> pf = boost::dynamic_pointer_cast<FadeStop_Cue>(cue);
    if (pf) {
	if (name == "StopOnComplete" && m_text == "true") {
	    pf->stop_on_complete = true;
	}
	if (name == "PauseOnComplete" && m_text == "true") {
	    pf->pause_on_complete = true;
	}
    }
}

void OpenParser::on_comment(const Glib::ustring &)
{
}

void OpenParser::on_cdata_block(const Glib::ustring &text)
{
    // Nasty bug in Debian Sarge release of libxml++
    // We do not use CDATA so just pass on to on_characters
    on_characters(text);
}

void OpenParser::on_characters(const Glib::ustring &characters)
{
    m_text += characters;
}

void OpenParser::on_internal_subset(const Glib::ustring &name,
    const Glib::ustring &publicId, const Glib::ustring &systemId)
{
    std::cerr << name << ',' << publicId << ',' << systemId << '\n';
}

void OpenParser::on_warning(const Glib::ustring& text)
{
    std::cerr << text << '\n';
}

void OpenParser::on_error(const Glib::ustring& text)
{
    std::cerr << text << '\n';
}

void OpenParser::on_fatal_error(const Glib::ustring& text)
{
    std::cerr << text << '\n';
}

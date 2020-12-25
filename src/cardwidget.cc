/***
  This file is part of pavucontrol.

  Copyright 2006-2008 Lennart Poettering
  Copyright 2009 Colin Guthrie

  pavucontrol is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  pavucontrol is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with pavucontrol. If not, see <http://www.gnu.org/licenses/>.
***/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "cardwidget.h"

#include "i18n.h"

/*** CardWidget ***/
CardWidget::CardWidget(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& x) :
    Gtk::VBox(cobject) {

    x->get_widget("cardNameLabel", nameLabel);
    x->get_widget("profileList", profileList);
    x->get_widget("cardIconImage", iconImage);
    x->get_widget("codecBox", codecBox);
    x->get_widget("codecList", codecList);
    x->get_widget("profileLockToggleButton", profileLockToggleButton);

    profileListStore = Gtk::ListStore::create(profileModel);
    profileList->set_model(profileListStore);
    profileList->pack_start(profileModel.desc);

    profileList->signal_changed().connect( sigc::mem_fun(*this, &CardWidget::onProfileChange));

    codecBox->hide();

    codecListStore = Gtk::ListStore::create(codecModel);
    codecList->set_model(codecListStore);
    codecList->pack_start(codecModel.desc);

    codecList->signal_changed().connect( sigc::mem_fun(*this, &CardWidget::onCodecChange));

    hasProfileLock = false;

    profileLockToggleButton->signal_clicked().connect(sigc::mem_fun(*this, &CardWidget::onProfileLockToggleButton));
    profileLockToggleButton->set_sensitive(true);
    profileLockToggleButton->set_visible(hasProfileLock);
}

CardWidget* CardWidget::create() {
    CardWidget* w;
    Glib::RefPtr<Gtk::Builder> x = Gtk::Builder::create_from_file(GLADE_FILE, "cardWidget");
    x->get_widget_derived("cardWidget", w);
    w->reference();
    return w;
}


void CardWidget::prepareMenu() {
    int active_idx;

    profileListStore->clear();
    active_idx = -1;
    /* Fill the ComboBox's Tree Model */
    for (uint32_t i = 0; i < profiles.size(); ++i) {
        Gtk::TreeModel::Row row = *(profileListStore->append());
        row[profileModel.name] = profiles[i].first;
        row[profileModel.desc] = profiles[i].second;
        if (profiles[i].first == activeProfile)
          active_idx = i;
    }

    if (active_idx >= 0)
        profileList->set_active(active_idx);

    codecListStore->clear();
    active_idx = -1;
    /* Fill the ComboBox's Tree Model */
    for (uint32_t i = 0; i < codecs.size(); ++i) {
        Gtk::TreeModel::Row row = *(codecListStore->append());
        row[codecModel.name] = codecs[i].first;
        row[codecModel.desc] = codecs[i].second;
        if (codecs[i].first == activeCodec)
          active_idx = i;
    }

    if (active_idx >= 0)
        codecList->set_active(active_idx);

    /* unhide codec box */
    if (codecs.size())
        codecBox->show();
    else
        codecBox->hide();

    profileLockToggleButton->set_visible(hasProfileLock);
}

void CardWidget::onProfileChange() {
    Gtk::TreeModel::iterator iter;

    if (updating)
        return;

    iter = profileList->get_active();
    if (iter)
    {
        Gtk::TreeModel::Row row = *iter;
        if (row)
        {
          pa_operation* o;
          Glib::ustring profile = row[profileModel.name];

          if (!(o = pa_context_set_card_profile_by_index(get_context(), index, profile.c_str(), NULL, NULL))) {
              show_error(_("pa_context_set_card_profile_by_index() failed"));
              return;
          }

          pa_operation_unref(o);
        }
    }
}

void CardWidget::onCodecChange() {
    if (updating)
        return;

#ifdef HAVE_PULSE_MESSAGING_API
    Gtk::TreeModel::iterator iter = codecList->get_active();
    if (iter)
    {
        Gtk::TreeModel::Row row = *iter;
        if (row)
        {
          pa_operation* o;
          Glib::ustring codec_id = row[codecModel.name];

          std::string codec_message = "\"" + std::string(codec_id) + "\"";

          if (!(o = pa_context_send_message_to_object(get_context(), card_bluez_message_handler_path(pulse_card_name).c_str(),
              "switch-codec", codec_message.c_str(), NULL, NULL))) {
              g_debug(_("pa_context_send_message_to_object() failed: %s"), pa_strerror(pa_context_errno(get_context())));
              return;
          }

          pa_operation_unref(o);
        }
    }
#endif
}

void CardWidget::onProfileLockToggleButton() {
    if (updating)
        return;

#ifdef HAVE_PULSE_MESSAGING_API
    Gtk::TreeModel::iterator iter = profileList->get_active();
    if (iter)
    {
        Gtk::TreeModel::Row row = *iter;
        if (row)
        {
          pa_operation* o;
          Glib::ustring profile = row[profileModel.name];

          bool profileIsLocked = profileLockToggleButton->get_active();

          if (!(o = pa_context_send_message_to_object(get_context(), card_message_handler_path(pulse_card_name).c_str(),
              "set-profile-sticky", profileIsLocked ? "true" : "false", NULL, NULL))) {
              g_debug(_("pa_context_send_message_to_object() failed: %s"), pa_strerror(pa_context_errno(get_context())));
              return;
          }

          pa_operation_unref(o);
        }
    }
#endif
}

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

    treeModel = Gtk::ListStore::create(profileModel);
    profileList->set_model(treeModel);
    profileList->pack_start(profileModel.desc);

    profileList->signal_changed().connect( sigc::mem_fun(*this, &CardWidget::onProfileChange));
}

CardWidget* CardWidget::create() {
    CardWidget* w;
    Glib::RefPtr<Gtk::Builder> x = Gtk::Builder::create_from_file(GLADE_FILE, "cardWidget");
    x->get_widget_derived("cardWidget", w);
    w->reference();
    return w;
}


void CardWidget::prepareMenu() {
    int idx = 0;
    int active_idx = -1;

    treeModel->clear();
    /* Fill the ComboBox's Tree Model */
    for (uint32_t i = 0; i < profiles.size(); ++i) {
        Gtk::TreeModel::Row row = *(treeModel->append());
        row[profileModel.name] = profiles[i].first;
        row[profileModel.desc] = profiles[i].second;
        if (profiles[i].first == activeProfile)
          active_idx = idx;
        idx++;
    }

    if (active_idx >= 0)
        profileList->set_active(active_idx);
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

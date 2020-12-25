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

#ifndef cardwidget_h
#define cardwidget_h

#include "pavucontrol.h"

class PortInfo {
public:
      Glib::ustring name;
      Glib::ustring description;
      uint32_t priority;
      int available;
      int direction;
      int64_t latency_offset;
      std::vector<Glib::ustring> profiles;
};

class CardWidget : public Gtk::VBox {
public:
    CardWidget(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& x);
    static CardWidget* create();

    Gtk::Label *nameLabel;
    Gtk::Menu menu;
    Gtk::Image *iconImage;
    Glib::ustring name;
    std::string pulse_card_name;
    Gtk::Box *codecBox;
    Gtk::ToggleButton *profileLockToggleButton;
    uint32_t index;
    bool updating;

    // each entry in profiles is a pair of profile name and profile description
    std::vector<std::pair<Glib::ustring, Glib::ustring>> profiles;
    std::map<Glib::ustring, PortInfo> ports;
    Glib::ustring activeProfile;
    bool hasSinks;
    bool hasSources;

    // each entry in codecs is a pair of codec name and codec description
    std::vector<std::pair<Glib::ustring, Glib::ustring>> codecs;
    Glib::ustring activeCodec;

    bool hasProfileLock;

    void prepareMenu();

protected:
  virtual void onProfileChange();
  virtual void onCodecChange();
  virtual void onProfileLockToggleButton();

  /* Tree model columns */
  class ModelColumns : public Gtk::TreeModel::ColumnRecord
  {
  public:

    ModelColumns()
    { add(name); add(desc); }

    Gtk::TreeModelColumn<Glib::ustring> name;
    Gtk::TreeModelColumn<Glib::ustring> desc;
  };

  ModelColumns profileModel;

  Gtk::ComboBox *profileList;
  Glib::RefPtr<Gtk::ListStore> profileListStore;

  ModelColumns codecModel;

  Gtk::ComboBox *codecList;
  Glib::RefPtr<Gtk::ListStore> codecListStore;
};

#endif

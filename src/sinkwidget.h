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

#ifndef sinkwidget_h
#define sinkwidget_h

#include "pavucontrol.h"
#include "devicewidget.h"

#if HAVE_EXT_DEVICE_RESTORE_API
#  include <pulse/format.h>

#  define PAVU_NUM_ENCODINGS 6

typedef struct {
    pa_encoding encoding;
    Gtk::CheckButton *widget;
} encodingList;
#endif

class SinkWidget : public DeviceWidget {
public:
    SinkWidget(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& x);
    static SinkWidget* create(MainWindow* mainWindow);

    SinkType type;
    uint32_t monitor_index;
    bool can_decibel;

#if HAVE_EXT_DEVICE_RESTORE_API
    encodingList encodings[PAVU_NUM_ENCODINGS];
    Gtk::Grid *encodingSelect;
#endif

    virtual void onMuteToggleButton();
    virtual void executeVolumeUpdate();
    virtual void onDefaultToggleButton();
    void setDigital(bool);

protected:
    virtual void onPortChange();
    virtual void onEncodingsChange();
};

#endif

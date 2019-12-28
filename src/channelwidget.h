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

#ifndef channelwidget_h
#define channelwidget_h

#include "pavucontrol.h"

class MinimalStreamWidget;

class ChannelWidget : public Gtk::EventBox {
public:
    ChannelWidget(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& x);

    /* This creates multiple ChannelWidgets based on the given channel map. The
     * widgets are stored in the caller-provided array. */
    static void create(MinimalStreamWidget *owner, const pa_channel_map &m, bool can_decibel,
                       ChannelWidget *widgets[PA_CHANNELS_MAX]);

    void setVolume(pa_volume_t volume);

    Gtk::Label *channelLabel;
    Gtk::Label *volumeLabel;
    Gtk::Scale *volumeScale;

    int channel;
    MinimalStreamWidget *minimalStreamWidget;

    void onVolumeScaleValueChanged();

    bool can_decibel;
    bool volumeScaleEnabled;
    bool last;

    virtual void set_sensitive(bool enabled);
    virtual void setBaseVolume(pa_volume_t);

private:
    static ChannelWidget *createOne(MinimalStreamWidget *owner, int channelIndex, pa_channel_position channelPosition,
                                    bool can_decibel);
};

#endif

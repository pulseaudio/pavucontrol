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

#include "streamwidget.h"
#include "mainwindow.h"
#include "channelwidget.h"

#include "i18n.h"

/*** StreamWidget ***/
StreamWidget::StreamWidget(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& x) :
    MinimalStreamWidget(cobject),
    mpMainWindow(NULL) {

    /* MinimalStreamWidget member variables. */
    x->get_widget("streamChannelsVBox", channelsVBox);
    x->get_widget("streamNameLabel", nameLabel);
    x->get_widget("streamBoldNameLabel", boldNameLabel);
    x->get_widget("streamIconImage", iconImage);

    x->get_widget("streamLockToggleButton", lockToggleButton);
    x->get_widget("streamMuteToggleButton", muteToggleButton);
    x->get_widget("directionLabel", directionLabel);
    x->get_widget("deviceComboBox", deviceComboBox);

    this->signal_button_press_event().connect(sigc::mem_fun(*this, &StreamWidget::onContextTriggerEvent));
    muteToggleButton->signal_clicked().connect(sigc::mem_fun(*this, &StreamWidget::onMuteToggleButton));
    lockToggleButton->signal_clicked().connect(sigc::mem_fun(*this, &StreamWidget::onLockToggleButton));
    deviceComboBox->signal_changed().connect(sigc::mem_fun(*this, &StreamWidget::onDeviceComboBoxChanged));

    terminate.set_label(_("Terminate"));
    terminate.signal_activate().connect(sigc::mem_fun(*this, &StreamWidget::onKill));
    contextMenu.append(terminate);
    contextMenu.show_all();

    for (unsigned i = 0; i < PA_CHANNELS_MAX; i++)
        channelWidgets[i] = NULL;
}

void StreamWidget::init(MainWindow* mainWindow) {
    mpMainWindow = mainWindow;

    MinimalStreamWidget::init();
}

bool StreamWidget::onContextTriggerEvent(GdkEventButton* event) {
    if (GDK_BUTTON_PRESS == event->type && 3 == event->button) {
        contextMenu.popup_at_pointer((GdkEvent*)event);
        return true;
    }
    return false;
}

void StreamWidget::setChannelMap(const pa_channel_map &m, bool can_decibel) {
    channelMap = m;

    ChannelWidget::create(this, m, can_decibel, channelWidgets);

    for (int i = 0; i < m.channels; i++) {
        ChannelWidget *cw = channelWidgets[i];
        channelsVBox->pack_start(*cw, false, false, 0);
        cw->unreference();
    }

    channelWidgets[m.channels-1]->setBaseVolume(PA_VOLUME_NORM);

    lockToggleButton->set_sensitive(m.channels > 1);
    hideLockedChannels(lockToggleButton->get_active());
}

void StreamWidget::setVolume(const pa_cvolume &v, bool force) {
    g_assert(v.channels == channelMap.channels);

    volume = v;

    if (timeoutConnection.empty() || force) { /* do not update the volume when a volume change is still in flux */
        for (int i = 0; i < volume.channels; i++)
            channelWidgets[i]->setVolume(volume.values[i]);
    }
}

void StreamWidget::updateChannelVolume(int channel, pa_volume_t v) {
    pa_cvolume n;
    g_assert(channel < volume.channels);

    n = volume;
    if (lockToggleButton->get_active()) {
        for (int i = 0; i < n.channels; i++)
            n.values[i] = v;
    } else
        n.values[channel] = v;

    setVolume(n, true);

    if (timeoutConnection.empty())
        timeoutConnection = Glib::signal_timeout().connect(sigc::mem_fun(*this, &StreamWidget::timeoutEvent), 100);
}

void StreamWidget::hideLockedChannels(bool hide) {
    for (int i = 0; i < channelMap.channels - 1; i++)
        channelWidgets[i]->set_visible(!hide);

    channelWidgets[channelMap.channels - 1]->channelLabel->set_visible(!hide);
}

void StreamWidget::onMuteToggleButton() {

    lockToggleButton->set_sensitive(!muteToggleButton->get_active());

    for (int i = 0; i < channelMap.channels; i++)
        channelWidgets[i]->set_sensitive(!muteToggleButton->get_active());
}

void StreamWidget::onLockToggleButton() {
    hideLockedChannels(lockToggleButton->get_active());
}

bool StreamWidget::timeoutEvent() {
    executeVolumeUpdate();
    return false;
}

void StreamWidget::executeVolumeUpdate() {
}

void StreamWidget::onKill() {
}

void StreamWidget::onDeviceComboBoxChanged() {
}

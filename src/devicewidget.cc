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

#include <pulse/ext-device-manager.h>

#include "mainwindow.h"
#include "devicewidget.h"
#include "channelwidget.h"

#include "i18n.h"

/*** DeviceWidget ***/
DeviceWidget::DeviceWidget(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& x) :
    MinimalStreamWidget(cobject),
    offsetButtonEnabled(false),
    mDigital(false) {

    /* MinimalStreamWidget member variables. */
    x->get_widget("deviceChannelsVBox", channelsVBox);
    x->get_widget("deviceNameLabel", nameLabel);
    x->get_widget("deviceBoldNameLabel", boldNameLabel);
    x->get_widget("deviceIconImage", iconImage);

    x->get_widget("deviceLockToggleButton", lockToggleButton);
    x->get_widget("deviceMuteToggleButton", muteToggleButton);
    x->get_widget("defaultToggleButton", defaultToggleButton);
    x->get_widget("portSelect", portSelect);
    x->get_widget("portList", portList);
    x->get_widget("advancedOptions", advancedOptions);
    x->get_widget("offsetSelect", offsetSelect);
    x->get_widget("offsetButton", offsetButton);

    this->signal_button_press_event().connect(sigc::mem_fun(*this, &DeviceWidget::onContextTriggerEvent));
    muteToggleButton->signal_clicked().connect(sigc::mem_fun(*this, &DeviceWidget::onMuteToggleButton));
    lockToggleButton->signal_clicked().connect(sigc::mem_fun(*this, &DeviceWidget::onLockToggleButton));
    defaultToggleButton->signal_clicked().connect(sigc::mem_fun(*this, &DeviceWidget::onDefaultToggleButton));

    rename.set_label(_("Rename Device..."));
    rename.signal_activate().connect(sigc::mem_fun(*this, &DeviceWidget::renamePopup));
    contextMenu.append(rename);
    contextMenu.show_all();

    treeModel = Gtk::ListStore::create(portModel);
    portList->set_model(treeModel);
    portList->pack_start(portModel.desc);

    portList->signal_changed().connect(sigc::mem_fun(*this, &DeviceWidget::onPortChange));
    offsetButton->signal_value_changed().connect(sigc::mem_fun(*this, &DeviceWidget::onOffsetChange));

    for (unsigned i = 0; i < PA_CHANNELS_MAX; i++)
        channelWidgets[i] = NULL;

    offsetAdjustment = Gtk::Adjustment::create(0.0, -2000.0, 5000.0, 10.0, 50.0, 0.0);
    offsetButton->configure(offsetAdjustment, 0, 2);
}

void DeviceWidget::init(MainWindow* mainWindow, Glib::ustring deviceType) {
    mpMainWindow = mainWindow;
    mDeviceType = deviceType;

    MinimalStreamWidget::init();
}

void DeviceWidget::setChannelMap(const pa_channel_map &m, bool can_decibel) {
    channelMap = m;
    ChannelWidget::create(this, m, can_decibel, channelWidgets);

    for (int i = 0; i < m.channels; i++) {
        ChannelWidget *cw = channelWidgets[i];
        channelsVBox->pack_start(*cw, false, false, 0);
        cw->unreference();
    }

    lockToggleButton->set_sensitive(m.channels > 1);
    hideLockedChannels(lockToggleButton->get_active());
}

void DeviceWidget::setVolume(const pa_cvolume &v, bool force) {
    g_assert(v.channels == channelMap.channels);

    volume = v;

    if (timeoutConnection.empty() || force) { /* do not update the volume when a volume change is still in flux */
        for (int i = 0; i < volume.channels; i++)
            channelWidgets[i]->setVolume(volume.values[i]);
    }
}

void DeviceWidget::updateChannelVolume(int channel, pa_volume_t v) {
    pa_cvolume n;
    g_assert(channel < volume.channels);

    n = volume;
    if (lockToggleButton->get_active())
        pa_cvolume_set(&n, n.channels, v);
    else
        n.values[channel] = v;

    setVolume(n, true);

    if (timeoutConnection.empty())
        timeoutConnection = Glib::signal_timeout().connect(sigc::mem_fun(*this, &DeviceWidget::timeoutEvent), 100);
}

void DeviceWidget::hideLockedChannels(bool hide) {
    for (int i = 0; i < channelMap.channels - 1; i++)
        channelWidgets[i]->set_visible(!hide);

    channelWidgets[channelMap.channels - 1]->channelLabel->set_visible(!hide);
}

void DeviceWidget::onMuteToggleButton() {

    lockToggleButton->set_sensitive(!muteToggleButton->get_active());

    for (int i = 0; i < channelMap.channels; i++)
        channelWidgets[i]->set_sensitive(!muteToggleButton->get_active());
}

void DeviceWidget::onLockToggleButton() {
    hideLockedChannels(lockToggleButton->get_active());
}

void DeviceWidget::onDefaultToggleButton() {
}

void DeviceWidget::onOffsetChange() {
    pa_operation *o;
    int64_t offset;
    std::ostringstream card_stream;
    Glib::ustring card_name;

    if (!offsetButtonEnabled)
        return;

    offset = offsetButton->get_value() * 1000.0;
    card_stream << card_index;
    card_name = card_stream.str();

    if (!(o = pa_context_set_port_latency_offset(get_context(),
            card_name.c_str(), activePort.c_str(), offset, NULL, NULL))) {
        show_error(_("pa_context_set_port_latency_offset() failed"));
        return;
    }
    pa_operation_unref(o);
}

void DeviceWidget::setDefault(bool isDefault) {
    defaultToggleButton->set_active(isDefault);
    /*defaultToggleButton->set_sensitive(!isDefault);*/
}

bool DeviceWidget::timeoutEvent() {
    executeVolumeUpdate();
    return false;
}

void DeviceWidget::executeVolumeUpdate() {
}

void DeviceWidget::setLatencyOffset(int64_t offset) {
    offsetButtonEnabled = false;
    offsetButton->set_value(offset / 1000.0);
    offsetButtonEnabled = true;
}

void DeviceWidget::setBaseVolume(pa_volume_t v) {

    for (int i = 0; i < channelMap.channels; i++)
        channelWidgets[i]->setBaseVolume(v);
}

void DeviceWidget::prepareMenu() {
    int idx = 0;
    int active_idx = -1;

    treeModel->clear();
    /* Fill the ComboBox's Tree Model */
    for (uint32_t i = 0; i < ports.size(); ++i) {
        Gtk::TreeModel::Row row = *(treeModel->append());
        row[portModel.name] = ports[i].first;
        row[portModel.desc] = ports[i].second;
        if (ports[i].first == activePort)
            active_idx = idx;
        idx++;
    }

    if (active_idx >= 0)
        portList->set_active(active_idx);

    if (ports.size() > 0) {
        portSelect->show();

        if (pa_context_get_server_protocol_version(get_context()) >= 27)
            offsetSelect->show();
        else
            offsetSelect->hide();

    } else {
        portSelect->hide();
        offsetSelect->hide();
    }

    updateAdvancedOptionsVisibility();
}

bool DeviceWidget::onContextTriggerEvent(GdkEventButton* event) {
    if (GDK_BUTTON_PRESS == event->type && 3 == event->button) {
        contextMenu.popup_at_pointer((GdkEvent*)event);
        return true;
    }

    return false;
}

void DeviceWidget::renamePopup() {
    if (updating)
        return;

    if (!mpMainWindow->canRenameDevices) {
        Gtk::MessageDialog dialog(
            *mpMainWindow,
            _("Sorry, but device renaming is not supported."),
            false,
            Gtk::MESSAGE_WARNING,
            Gtk::BUTTONS_OK,
            true);
        dialog.set_secondary_text(_("You need to load module-device-manager in the PulseAudio server in order to rename devices"));
        dialog.run();
        return;
    }

    Gtk::Dialog* dialog;
    Gtk::Entry* renameText;

    Glib::RefPtr<Gtk::Builder> x = Gtk::Builder::create_from_file(GLADE_FILE, "renameDialog");
    x->get_widget("renameDialog", dialog);
    x->get_widget("renameText", renameText);

    renameText->set_text(description);
    dialog->add_button(_("_Cancel"), Gtk::RESPONSE_CANCEL);
    dialog->add_button(_("_OK"), Gtk::RESPONSE_OK);
    dialog->set_default_response(Gtk::RESPONSE_OK);
    if (Gtk::RESPONSE_OK == dialog->run()) {
        pa_operation* o;
        gchar *key = g_markup_printf_escaped("%s:%s", mDeviceType.c_str(), name.c_str());

        if (!(o = pa_ext_device_manager_set_device_description(get_context(), key, renameText->get_text().c_str(), NULL, NULL))) {
            show_error(_("pa_ext_device_manager_write() failed"));
            return;
        }
        pa_operation_unref(o);
        g_free(key);
    }
    delete dialog;
}

void DeviceWidget::updateAdvancedOptionsVisibility() {
    bool visible = false;

    if (mDigital) {
        /* We need to show the format configuration options. */
        visible = true;
    }

    if (ports.size() > 0) {
        /* We need to show the latency offset spin button. */
        visible = true;
    }

    if (visible)
        advancedOptions->show();
    else
        advancedOptions->hide();
}

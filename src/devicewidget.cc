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
    channelsVBox = x->get_widget<Gtk::Box>("deviceChannelsVBox");
    nameLabel = x->get_widget<Gtk::Label>("deviceNameLabel");
    boldNameLabel = x->get_widget<Gtk::Label>("deviceBoldNameLabel");
    iconImage= x->get_widget<Gtk::Image>("deviceIconImage");

    lockToggleButton = x->get_widget<Gtk::ToggleButton>("deviceLockToggleButton");
    muteToggleButton = x->get_widget<Gtk::ToggleButton>("deviceMuteToggleButton");
    defaultToggleButton= x->get_widget<Gtk::ToggleButton>("defaultToggleButton");
    portSelect = x->get_widget<Gtk::Box>("portSelect");
    portList = x->get_widget<Gtk::ComboBox>("portList");
    advancedOptions = x->get_widget<Gtk::Expander>("advancedOptions");
    offsetSelect = x->get_widget<Gtk::Box>("offsetSelect");
    offsetButton = x->get_widget<Gtk::SpinButton>("offsetButton");

    muteToggleButton->signal_clicked().connect(sigc::mem_fun(*this, &DeviceWidget::onMuteToggleButton));
    lockToggleButton->signal_clicked().connect(sigc::mem_fun(*this, &DeviceWidget::onLockToggleButton));
    defaultToggleButton->signal_clicked().connect(sigc::mem_fun(*this, &DeviceWidget::onDefaultToggleButton));

    auto gesture = Gtk::GestureClick::create();
    gesture->set_button(3);
    gesture->set_exclusive(true);
    gesture->signal_pressed().connect(sigc::mem_fun(*this, &DeviceWidget::onContextTriggerEvent));
    this->add_controller(gesture);

    const std::string actionName = "rename", groupName="devicewidget";
    auto action = Gio::SimpleAction::create(actionName);
    action->set_enabled(true);
    action->signal_activate().connect(sigc::mem_fun(*this, &DeviceWidget::openRenamePopup));

    auto group = Gio::SimpleActionGroup::create();
    group->add_action(action);

    insert_action_group(groupName, group);

    auto menuModel = Gio::Menu::create();
    menuModel->append(_("Rename Device..."), groupName + "." + actionName);
    contextMenu.set_menu_model(menuModel);
    contextMenu.set_parent(*this);


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
        channelsVBox->prepend(*cw);
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
        show_error(this, _("pa_context_set_port_latency_offset() failed"));
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


void DeviceWidget::onContextTriggerEvent(gint n_press, gdouble x, gdouble y) {
    if (n_press == 1) {
        contextMenu.set_pointing_to(Gdk::Rectangle {(int) x, (int) y, 0 , 0});
        contextMenu.popup();
    }
}

void DeviceWidget::openRenamePopup(const Glib::VariantBase& parameter) {
    if (updating)
        return;

    if (!mpMainWindow->canRenameDevices) {
        auto dialog = Gtk::AlertDialog::create(_("Sorry, but device renaming is not supported."));
        dialog->set_modal(true);
        dialog->set_detail(_("You need to load module-device-manager in the PulseAudio server in order to rename devices"));
        dialog->show(*mpMainWindow);
        return;
    }

    Glib::RefPtr<Gtk::Builder> x = Gtk::Builder::create_from_resource("/org/pulseaudio/pavucontrol/ui/pavucontrol.glade", "renameDialog");
    gchar *key = g_markup_printf_escaped("%s:%s", mDeviceType.c_str(), name.c_str());
    RenameWindow* renameDialog = Gtk::Builder::get_widget_derived<RenameWindow>(x, "renameDialog", description.c_str(), key);
    renameDialog->set_transient_for(*mpMainWindow);

    renameDialog->present();
}

RenameWindow::RenameWindow(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& x, const gchar* name, const gchar* key) :
    Gtk::ApplicationWindow(cobject),
    deviceKey(key) {

    renameText = x->get_widget<Gtk::Entry>("renameText");
    renameText->set_text(name);

    Gtk::Button* renameButton = x->get_widget<Gtk::Button>("renameButton");
    set_default_widget(*renameButton);

    auto renameAction = Gio::SimpleAction::create("rename");
    renameAction->set_enabled(true);
    renameAction->signal_activate().connect(sigc::mem_fun(*this, &RenameWindow::doRename));

    add_action(renameAction);
}

void RenameWindow::doRename(const Glib::VariantBase& parameter) {
    pa_operation* o;
    auto name = renameText->get_text();

    if (!(o = pa_ext_device_manager_set_device_description(get_context(), deviceKey, name.c_str(), NULL, NULL))) {
        show_error(this, _("pa_ext_device_manager_write() failed"));
        return;
    }
    pa_operation_unref(o);
    g_free((char*)deviceKey);
    delete this;
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

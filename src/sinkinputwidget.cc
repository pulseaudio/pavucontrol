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

#include "sinkinputwidget.h"
#include "mainwindow.h"
#include "sinkwidget.h"

#include "i18n.h"

SinkInputWidget::SinkInputWidget(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& x) :
    StreamWidget(cobject, x) {

    gchar *txt;
    directionLabel->set_label(txt = g_markup_printf_escaped("<i>%s</i>", _("on")));
    g_free(txt);

    terminate.set_label(_("Terminate Playback"));
}

SinkInputWidget* SinkInputWidget::create(MainWindow* mainWindow) {
    SinkInputWidget* w;
    Glib::RefPtr<Gtk::Builder> x = Gtk::Builder::create_from_file(GLADE_FILE, "streamWidget");
    x->get_widget_derived("streamWidget", w);
    w->init(mainWindow);
    w->reference();

    return w;
}

SinkInputWidget::~SinkInputWidget(void) {
}

void SinkInputWidget::setSinkIndex(uint32_t idx) {
    mSinkIndex = idx;
    updateDeviceComboBox();
}

uint32_t SinkInputWidget::sinkIndex() {
    return mSinkIndex;
}

void SinkInputWidget::updateDeviceComboBox() {
    Glib::ustring currentSinkName = UNKNOWN_DEVICE_NAME;

    deviceComboBox->remove_all();

    for (auto i = mpMainWindow->sinkWidgets.begin(); i != mpMainWindow->sinkWidgets.end(); i++) {
        SinkWidget *sink = i->second;

        deviceComboBox->append(sink->name, sink->description);

        if (sink->index == mSinkIndex)
            currentSinkName = sink->name;
    }

    if (currentSinkName == UNKNOWN_DEVICE_NAME)
        deviceComboBox->append(UNKNOWN_DEVICE_NAME, _("Unknown output"));

    deviceComboBox->set_active_id(currentSinkName);
}

void SinkInputWidget::executeVolumeUpdate() {
    pa_operation* o;

    if (!(o = pa_context_set_sink_input_volume(get_context(), index, &volume, NULL, NULL))) {
        show_error(_("pa_context_set_sink_input_volume() failed"));
        return;
    }

    pa_operation_unref(o);
}

void SinkInputWidget::onMuteToggleButton() {
    StreamWidget::onMuteToggleButton();

    if (updating)
        return;

    pa_operation* o;
    if (!(o = pa_context_set_sink_input_mute(get_context(), index, muteToggleButton->get_active(), NULL, NULL))) {
        show_error(_("pa_context_set_sink_input_mute() failed"));
        return;
    }

    pa_operation_unref(o);
}

void SinkInputWidget::onKill() {
    pa_operation* o;
    if (!(o = pa_context_kill_sink_input(get_context(), index, NULL, NULL))) {
        show_error(_("pa_context_kill_sink_input() failed"));
        return;
    }

    pa_operation_unref(o);
}

void SinkInputWidget::onDeviceComboBoxChanged() {
    if (updating)
        return;

    Glib::ustring sinkName = deviceComboBox->get_active_id();

    pa_operation *o = pa_context_move_sink_input_by_name(get_context(), index, sinkName.c_str(), NULL, NULL);
    if (o)
        pa_operation_unref(o);
}

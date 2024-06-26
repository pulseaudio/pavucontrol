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

#include "sinkwidget.h"

#ifdef HAVE_LIBCANBERRA
#include <canberra.h>
#endif

#if HAVE_EXT_DEVICE_RESTORE_API
#  include <pulse/format.h>
#  include <pulse/ext-device-restore.h>
#endif

#include "i18n.h"

SinkWidget::SinkWidget(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& x) :
    DeviceWidget(cobject, x) {
#if HAVE_EXT_DEVICE_RESTORE_API
    uint8_t i = 0;

    encodingSelect = x->get_widget<Gtk::Grid>("encodingSelect");

    encodings[i].encoding = PA_ENCODING_PCM;
    encodings[i].widget = x->get_widget<Gtk::CheckButton>("encodingFormatPCM");
    encodings[i].widget->signal_toggled().connect(sigc::mem_fun(*this, &SinkWidget::onEncodingsChange));

    ++i;
    encodings[i].encoding = PA_ENCODING_AC3_IEC61937;
    encodings[i].widget = x->get_widget<Gtk::CheckButton>("encodingFormatAC3");
    encodings[i].widget->signal_toggled().connect(sigc::mem_fun(*this, &SinkWidget::onEncodingsChange));

    ++i;
    encodings[i].encoding = PA_ENCODING_EAC3_IEC61937;
    encodings[i].widget = x->get_widget<Gtk::CheckButton>("encodingFormatEAC3");
    encodings[i].widget->signal_toggled().connect(sigc::mem_fun(*this, &SinkWidget::onEncodingsChange));

    ++i;
    encodings[i].encoding = PA_ENCODING_MPEG_IEC61937;
    encodings[i].widget = x->get_widget<Gtk::CheckButton>("encodingFormatMPEG");
    encodings[i].widget->signal_toggled().connect(sigc::mem_fun(*this, &SinkWidget::onEncodingsChange));

    ++i;
    encodings[i].encoding = PA_ENCODING_DTS_IEC61937;
    encodings[i].widget = x->get_widget<Gtk::CheckButton>("encodingFormatDTS");
    encodings[i].widget->signal_toggled().connect(sigc::mem_fun(*this, &SinkWidget::onEncodingsChange));

    ++i;
    encodings[i].encoding = PA_ENCODING_INVALID;
    encodings[i].widget = x->get_widget<Gtk::CheckButton>("encodingFormatAAC");
    encodings[i].widget->set_sensitive(false);
#ifdef PA_ENCODING_MPEG2_AAC_IEC61937
    if (pa_context_get_server_protocol_version(get_context()) >= 28) {
        encodings[i].encoding = PA_ENCODING_MPEG2_AAC_IEC61937;
        encodings[i].widget->signal_toggled().connect(sigc::mem_fun(*this, &SinkWidget::onEncodingsChange));
        encodings[i].widget->set_sensitive(true);
    }
#endif
    ++i;
    encodings[i].encoding = PA_ENCODING_INVALID;
    encodings[i].widget = x->get_widget<Gtk::CheckButton>("encodingFormatTRUEHD");
    encodings[i].widget->set_sensitive(false);
#ifdef PA_ENCODING_TRUEHD_IEC61937
    if (pa_context_get_server_protocol_version(get_context()) >= 33) {
        encodings[i].encoding = PA_ENCODING_TRUEHD_IEC61937;
        encodings[i].widget->signal_toggled().connect(sigc::mem_fun(*this, &SinkWidget::onEncodingsChange));
        encodings[i].widget->set_sensitive(true);
    }
#endif
    ++i;
    encodings[i].encoding = PA_ENCODING_INVALID;
    encodings[i].widget = x->get_widget<Gtk::CheckButton>("encodingFormatDTSHD");
    encodings[i].widget->set_sensitive(false);
#ifdef PA_ENCODING_DTSHD_IEC61937
    if (pa_context_get_server_protocol_version(get_context()) >= 33) {
        encodings[i].encoding = PA_ENCODING_DTSHD_IEC61937;
        encodings[i].widget->signal_toggled().connect(sigc::mem_fun(*this, &SinkWidget::onEncodingsChange));
        encodings[i].widget->set_sensitive(true);
    }
#endif
#endif
}

SinkWidget* SinkWidget::create(MainWindow* mainWindow) {
    SinkWidget* w;
    Glib::RefPtr<Gtk::Builder> x = Gtk::Builder::create_from_resource("/org/pulseaudio/pavucontrol/ui/devicewidget.ui", "deviceWidget");
    w = Gtk::Builder::get_widget_derived<SinkWidget>(x, "deviceWidget");
    w->init(mainWindow, "sink");
    w->reference();
    return w;
}

void SinkWidget::executeVolumeUpdate() {
    pa_operation* o;
    char dev[64];

    if (!(o = pa_context_set_sink_volume_by_index(get_context(), index, &volume, NULL, NULL))) {
        show_error(this, _("pa_context_set_sink_volume_by_index() failed"));
        return;
    }

    pa_operation_unref(o);

    snprintf(dev, sizeof(dev), "%lu", (unsigned long) index);

#ifdef HAVE_LIBCANBERRA
    int playing = 0;
    ca_context_playing(mpMainWindow->canberraContext, 2, &playing);
    if (playing)
        return;

    ca_context_change_device(mpMainWindow->canberraContext, dev);

    ca_context_play(mpMainWindow->canberraContext,
                     2,
                     CA_PROP_EVENT_DESCRIPTION, _("Volume Control Feedback Sound"),
                     CA_PROP_EVENT_ID, "audio-volume-change",
                     CA_PROP_CANBERRA_CACHE_CONTROL, "permanent",
                     CA_PROP_CANBERRA_ENABLE, "1",
                     NULL);

    ca_context_change_device(mpMainWindow->canberraContext, NULL);
#endif
}

void SinkWidget::onMuteToggleButton() {
    DeviceWidget::onMuteToggleButton();

    if (updating)
        return;

    pa_operation* o;
    if (!(o = pa_context_set_sink_mute_by_index(get_context(), index, muteToggleButton->get_active(), NULL, NULL))) {
        show_error(this, _("pa_context_set_sink_mute_by_index() failed"));
        return;
    }

    pa_operation_unref(o);
}

void SinkWidget::onDefaultToggleButton() {
    pa_operation* o;

    if (updating)
        return;

    if (!(o = pa_context_set_default_sink(get_context(), name.c_str(), NULL, NULL))) {
        show_error(this, _("pa_context_set_default_sink() failed"));
        return;
    }
    pa_operation_unref(o);
}

void SinkWidget::onPortChange() {
    Gtk::TreeModel::iterator iter;

    if (updating)
        return;

    iter = portList->get_active();
    if (iter) {
        Gtk::TreeModel::Row row = *iter;
        if (row) {
            pa_operation* o;
            Glib::ustring port = row[portModel.name];

            if (!(o = pa_context_set_sink_port_by_index(get_context(), index, port.c_str(), NULL, NULL))) {
                show_error(this, _("pa_context_set_sink_port_by_index() failed"));
                return;
            }

            pa_operation_unref(o);
        }
    }
}

void SinkWidget::setDigital(bool digital) {
    mDigital = digital;

#if HAVE_EXT_DEVICE_RESTORE_API
    if (digital)
        encodingSelect->show();
    else
        encodingSelect->hide();

    updateAdvancedOptionsVisibility();
#endif
}

void SinkWidget::onEncodingsChange() {
#if HAVE_EXT_DEVICE_RESTORE_API
    pa_operation* o;
    uint8_t n_formats = 0;
    pa_format_info **formats;

    if (updating)
        return;

    formats = (pa_format_info**)malloc(sizeof(pa_format_info*) * PAVU_NUM_ENCODINGS);

    for (int i = 0; i < PAVU_NUM_ENCODINGS; ++i) {
        if (encodings[i].widget->get_active()) {
            formats[n_formats] = pa_format_info_new();
            formats[n_formats]->encoding = encodings[i].encoding;
            ++n_formats;
        }
    }

    if (!(o = pa_ext_device_restore_save_formats(get_context(), PA_DEVICE_TYPE_SINK, index, n_formats, formats, NULL, NULL))) {
        show_error(this, _("pa_ext_device_restore_save_sink_formats() failed"));
        free(formats);
        return;
    }

    free(formats);
    pa_operation_unref(o);
#endif
}

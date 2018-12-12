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

#include "channelwidget.h"
#include "minimalstreamwidget.h"

#include "i18n.h"

/*** ChannelWidget ***/

ChannelWidget::ChannelWidget(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& x) :
    Gtk::EventBox(cobject),
    can_decibel(false),
    volumeScaleEnabled(true),
    last(false) {

    x->get_widget("channelLabel", channelLabel);
    x->get_widget("volumeLabel", volumeLabel);
    x->get_widget("volumeScale", volumeScale);

    volumeScale->set_range((double)PA_VOLUME_MUTED, (double)PA_VOLUME_UI_MAX);
    volumeScale->set_value((double)PA_VOLUME_NORM);
    volumeScale->set_increments(((double)PA_VOLUME_NORM)/100.0, ((double)PA_VOLUME_NORM)/20.0);
    setBaseVolume(PA_VOLUME_NORM);

    volumeScale->signal_value_changed().connect(sigc::mem_fun(*this, &ChannelWidget::onVolumeScaleValueChanged));
}

ChannelWidget* ChannelWidget::createOne(MinimalStreamWidget *owner, int channelIndex, pa_channel_position channelPosition, bool can_decibel) {
    ChannelWidget* w;
    Glib::RefPtr<Gtk::Builder> x = Gtk::Builder::create();
    x->add_from_file(GLADE_FILE, "adjustment1");
    x->add_from_file(GLADE_FILE, "channelWidget");
    x->get_widget_derived("channelWidget", w);
    w->reference();

    w->channel = channelIndex;
    w->can_decibel = can_decibel;
    w->minimalStreamWidget = owner;

    char text[64];
    snprintf(text, sizeof(text), "<b>%s</b>", pa_channel_position_to_pretty_string(channelPosition));
    w->channelLabel->set_markup(text);

    return w;
}

void ChannelWidget::create(MinimalStreamWidget *owner, const pa_channel_map &m, bool can_decibel, ChannelWidget *widgets[PA_CHANNELS_MAX]) {
    int maxLabelWidth = 0;

    for (int i = 0; i < m.channels; i++) {
        widgets[i] = ChannelWidget::createOne(owner, i, m.map[i], can_decibel);

        Gtk::Requisition minimumSize;
        Gtk::Requisition naturalSize;
        widgets[i]->channelLabel->get_preferred_size(minimumSize, naturalSize);
        if (naturalSize.width > maxLabelWidth)
            maxLabelWidth = naturalSize.width;
    }

    widgets[m.channels - 1]->last = true;

    /* The channel labels have different widths by default, which makes the
     * volume slider widths different too. The volume sliders must have the
     * same width, otherwise it's very hard to see how the volumes of different
     * channels relate to each other, so we have to change all channel labels
     * to have the same width. */
    for (int i = 0; i < m.channels; i++)
        widgets[i]->channelLabel->set_size_request(maxLabelWidth, -1);
}

void ChannelWidget::setVolume(pa_volume_t volume) {
    double v;
    char txt[64];

    v = ((gdouble) volume * 100) / PA_VOLUME_NORM;
    if (can_decibel) {
        double dB = pa_sw_volume_to_dB(volume);
        if (dB > PA_DECIBEL_MININFTY)
            snprintf(txt, sizeof(txt), _("<small>%0.0f%% (%0.2f dB)</small>"), v, dB);
        else
            snprintf(txt, sizeof(txt), _("<small>%0.0f%% (-&#8734; dB)</small>"), v);
    }
    else
        snprintf(txt, sizeof(txt), _("%0.0f%%"), v);
    volumeLabel->set_markup(txt);

    volumeScaleEnabled = false;
    volumeScale->set_value(volume > PA_VOLUME_UI_MAX ? PA_VOLUME_UI_MAX : volume);
    volumeScaleEnabled = true;
}

void ChannelWidget::onVolumeScaleValueChanged() {

    if (!volumeScaleEnabled)
        return;

    if (minimalStreamWidget->updating)
        return;

    pa_volume_t volume = (pa_volume_t) volumeScale->get_value();
    minimalStreamWidget->updateChannelVolume(channel, volume);
}

void ChannelWidget::set_sensitive(bool enabled) {
    Gtk::EventBox::set_sensitive(enabled);

    channelLabel->set_sensitive(enabled);
    volumeLabel->set_sensitive(enabled);
    volumeScale->set_sensitive(enabled);
}

void ChannelWidget::setBaseVolume(pa_volume_t v) {

    gtk_scale_clear_marks(GTK_SCALE(volumeScale->gobj()));

    gtk_scale_add_mark(GTK_SCALE(volumeScale->gobj()), (double)PA_VOLUME_MUTED, (GtkPositionType) GTK_POS_BOTTOM,
                       last ? (can_decibel ? _("<small>Silence</small>") : _("<small>Min</small>")) : NULL);
    gtk_scale_add_mark(GTK_SCALE(volumeScale->gobj()), (double)PA_VOLUME_NORM, (GtkPositionType) GTK_POS_BOTTOM,
                       last ? _("<small>100% (0 dB)</small>") : NULL);

    if (v > PA_VOLUME_MUTED && v < PA_VOLUME_NORM) {
        gtk_scale_add_mark(GTK_SCALE(volumeScale->gobj()), (double)v, (GtkPositionType) GTK_POS_BOTTOM,
                           last ? _("<small><i>Base</i></small>") : NULL);
    }

}

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

#include "minimalstreamwidget.h"

/*** MinimalStreamWidget ***/
MinimalStreamWidget::MinimalStreamWidget(BaseObjectType* cobject) :
    Gtk::VBox(cobject),
    channelsVBox(NULL),
    nameLabel(NULL),
    boldNameLabel(NULL),
    iconImage(NULL),
    peakProgressBar(),
    lastPeak(0),
    peak(NULL),
    updating(false),
    volumeMeterEnabled(false),
    volumeMeterVisible(true) {
}

MinimalStreamWidget::~MinimalStreamWidget() {
    if (peak) {
        pa_stream_disconnect(peak);
        pa_stream_unref(peak);
        peak = NULL;
    }
}

void MinimalStreamWidget::init() {
    /* Set up the peak meter. This is not done in the constructor, because
     * channelsVBox is initialized by the subclasses, so it's not yet available
     * in the constructor. */

    peakProgressBar.set_size_request(-1, 10);
    channelsVBox->pack_end(peakProgressBar, false, false);

    /* XXX: Why is the peak meter hidden by default? Maybe the idea is that if
     * setting up the monitoring stream fails for whatever reason, then we
     * shouldn't show the peak meter at all. */
    peakProgressBar.hide();
}

#define DECAY_STEP .04

void MinimalStreamWidget::updatePeak(double v) {

    if (lastPeak >= DECAY_STEP)
        if (v < lastPeak - DECAY_STEP)
            v = lastPeak - DECAY_STEP;

    lastPeak = v;

    if (v >= 0) {
        peakProgressBar.set_sensitive(TRUE);
        peakProgressBar.set_fraction(v);
    } else {
        peakProgressBar.set_sensitive(FALSE);
        peakProgressBar.set_fraction(0);
    }

    enableVolumeMeter();
}

void MinimalStreamWidget::enableVolumeMeter() {
    if (volumeMeterEnabled)
        return;

    volumeMeterEnabled = true;
    if (volumeMeterVisible) {
        peakProgressBar.show();
    }
}

void MinimalStreamWidget::setVolumeMeterVisible(bool v) {
    volumeMeterVisible = v;
    if (v) {
        if (volumeMeterEnabled) {
            peakProgressBar.show();
        }
    } else {
        peakProgressBar.hide();
    }
}

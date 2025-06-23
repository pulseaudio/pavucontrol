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
    Gtk::Box(cobject),
    channelsVBox(NULL),
    nameLabel(NULL),
    boldNameLabel(NULL),
    iconImage(NULL),
    peakProgressBar(),
    lastPeak(0),
    peak(NULL),
    updating(false),
    volumeMeterEnabled(false),
    volumeMeterVisible(true),
    decayTickId(0),
    decayLastFrameTime(-1) {
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
    channelsVBox->append(peakProgressBar);

    /* XXX: Why is the peak meter hidden by default? Maybe the idea is that if
     * setting up the monitoring stream fails for whatever reason, then we
     * shouldn't show the peak meter at all. */
    peakProgressBar.hide();
}

void MinimalStreamWidget::stopDecay() {
    if (decayTickId) {
        remove_tick_callback(decayTickId);
        decayTickId = 0;
    }
}

void MinimalStreamWidget::updatePeak(double v, double decayStep) {
    if (lastPeak >= decayStep)
        if (v < lastPeak - decayStep)
            v = lastPeak - decayStep;

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
        stopDecay();
        peakProgressBar.hide();
    }
}

bool MinimalStreamWidget::decayOnTick(const Glib::RefPtr<Gdk::FrameClock>& frameClock) {
    auto frameTime = frameClock->get_frame_time();

    if (lastPeak == 0) {
        decayTickId = 0;
        return false;
    }

    // Scale elapsed time (µs) so we decay in at most 1 second
    if (frameTime != decayLastFrameTime)
        updatePeak(0, (frameTime - decayLastFrameTime) / 1000000.0);

    decayLastFrameTime = frameTime;

    return true;
}

void MinimalStreamWidget::decayToZero() {
    if (decayTickId)
        stopDecay();

    auto frameClock = get_frame_clock();

    if (!frameClock) {
        /* Widget isn't visible, set all the way to 0 */
        updatePeak(0, 1.0);
        return;
    }

    decayLastFrameTime = frameClock->get_frame_time();
    decayTickId = add_tick_callback(sigc::mem_fun(*this, &MinimalStreamWidget::decayOnTick));
}

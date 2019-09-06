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

#include "rolewidget.h"

#include <pulse/ext-stream-restore.h>

#include "i18n.h"

RoleWidget::RoleWidget(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& x) :
    StreamWidget(cobject, x) {

    lockToggleButton->hide();
    directionLabel->hide();
    deviceComboBox->hide();
}

RoleWidget* RoleWidget::create() {
    RoleWidget* w;
    Glib::RefPtr<Gtk::Builder> x = Gtk::Builder::create_from_file(GLADE_FILE, "streamWidget");
    x->get_widget_derived("streamWidget", w);
    w->reference();
    return w;
}

bool RoleWidget::onContextTriggerEvent(GdkEventButton*) {
    return false;
}

void RoleWidget::onMuteToggleButton() {
    StreamWidget::onMuteToggleButton();

    executeVolumeUpdate();
}

void RoleWidget::executeVolumeUpdate() {
    pa_ext_stream_restore_info info;

    if (updating)
        return;

    info.name = role.c_str();
    info.channel_map.channels = 1;
    info.channel_map.map[0] = PA_CHANNEL_POSITION_MONO;
    info.volume = volume;
    info.device = device == "" ? NULL : device.c_str();
    info.mute = muteToggleButton->get_active();

    pa_operation* o;
    if (!(o = pa_ext_stream_restore_write(get_context(), PA_UPDATE_REPLACE, &info, 1, TRUE, NULL, NULL))) {
        show_error(_("pa_ext_stream_restore_write() failed"));
        return;
    }

    pa_operation_unref(o);
}


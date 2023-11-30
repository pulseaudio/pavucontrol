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

#include "sourcewidget.h"

#include "i18n.h"

SourceWidget::SourceWidget(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& x) :
    DeviceWidget(cobject, x) {
}

SourceWidget* SourceWidget::create(MainWindow* mainWindow) {
    SourceWidget* w;
    Glib::RefPtr<Gtk::Builder> x = Gtk::Builder::create_from_file(GLADE_FILE, "deviceWidget");
    w = Gtk::Builder::get_widget_derived<SourceWidget>(x, "deviceWidget");
    w->init(mainWindow, "source");
    w->reference();
    return w;
}

void SourceWidget::executeVolumeUpdate() {
    pa_operation* o;

    if (!(o = pa_context_set_source_volume_by_index(get_context(), index, &volume, NULL, NULL))) {
        show_error(this, _("pa_context_set_source_volume_by_index() failed"));
        return;
    }

    pa_operation_unref(o);
}

void SourceWidget::onMuteToggleButton() {
    DeviceWidget::onMuteToggleButton();

    if (updating)
        return;

    pa_operation* o;
    if (!(o = pa_context_set_source_mute_by_index(get_context(), index, muteToggleButton->get_active(), NULL, NULL))) {
        show_error(this, _("pa_context_set_source_mute_by_index() failed"));
        return;
    }

    pa_operation_unref(o);
}

void SourceWidget::onDefaultToggleButton() {
    pa_operation* o;

    if (updating)
        return;

    if (!(o = pa_context_set_default_source(get_context(), name.c_str(), NULL, NULL))) {
        show_error(this, _("pa_context_set_default_source() failed"));
        return;
    }
    pa_operation_unref(o);
}

void SourceWidget::onPortChange() {
  Gtk::TreeModel::iterator iter;

  if (updating)
    return;

  iter = portList->get_active();
  if (iter)
  {
    Gtk::TreeModel::Row row = *iter;
    if (row)
    {
      pa_operation* o;
      Glib::ustring port = row[portModel.name];

      if (!(o = pa_context_set_source_port_by_index(get_context(), index, port.c_str(), NULL, NULL))) {
        show_error(this, _("pa_context_set_source_port_by_index() failed"));
        return;
      }

      pa_operation_unref(o);
    }
  }
}

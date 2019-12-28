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

#ifndef pavuapplication_h
#define pavuapplication_h

#include "pavucontrol.h"
#include "mainwindow.h"

class PavuApplication : public Gtk::Application {
public:
    PavuApplication();

    /* Main window */
    MainWindow *mainWindow;

    /* options */
    bool retry;
    bool maximize;
    gint32 tab;
    bool version;

    static PavuApplication& get_instance();

protected:
    // Override default signal handlers:
    void on_activate() override;

private:
    MainWindow* create_window();
    void on_hide_window(Gtk::Window* window);

    pa_glib_mainloop *m;
};


#endif

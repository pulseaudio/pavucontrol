/***
  This file is part of pavucontrol.

  Copyright 2006-2008 Lennart Poettering
  Copyright 2008 Sjoerd Simons <sjoerd@luon.net>

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

#include "i18n.h"

#include <canberra-gtk.h>

#include "pavuapplication.h"
#include "pavucontrol.h"
#include "mainwindow.h"

static PavuApplication globalInstance;

PavuApplication&
PavuApplication::get_instance()
{
    return globalInstance;
}

PavuApplication::PavuApplication() :
    Gtk::Application("org.pulseaudio.pavucontrol", Gio::ApplicationFlags::APPLICATION_HANDLES_COMMAND_LINE),
    mainWindow(NULL),
    retry(false),
    maximize(false),
    tab(0),
    version(false),
    m(NULL) {
}

/*
 * Create the main window and connect its "on_hide_window" signal to our cleanup
 * function
 */
MainWindow* PavuApplication::create_window()
{
    m = pa_glib_mainloop_new(g_main_context_default());
    g_assert(m);

    MainWindow* pavucontrol_window = pavucontrol_get_window(m, maximize, retry, tab);

    pavucontrol_window->signal_hide().connect(
                     sigc::bind<Gtk::Window*>(sigc::mem_fun(*this,
                     &PavuApplication::on_hide_window), pavucontrol_window));

    return pavucontrol_window;
}

/* "on_activate" signal handler
 * This is fired via the Gtk::Application framework either directly from the
 * run() function, or manually from the on_command_line signal handler.
 * This is always executed in the first-running process.
 */
void PavuApplication::on_activate()
{
    /* See if we are already running */
    if (mainWindow == NULL) {
        /* We aren't. Create the main window */
        mainWindow = create_window();

        /* and register it in the Gtk::Application */
        add_window(*mainWindow);
    } else if (tab != -1) {
        /* We are, and a specific tab has been requested. Select it. */
        mainWindow->selectTab(tab);
    }

    /* Present the main window. */
    mainWindow->present();
}

/* "on_hide_window" signal handler
 * This is executed in the first-running process and performs cleanup before
 * exiting : when the last registered window of Gtk::Application is closed,
 * the application's run() function returns.
 */
void PavuApplication::on_hide_window(Gtk::Window* window)
{
    delete window;
    mainWindow = NULL;

    if (get_context()) {
        pa_context_unref(get_context());
    }
    pa_glib_mainloop_free(m);
    m = NULL;
}

template <typename T_ArgType>
static bool get_arg_value(const Glib::RefPtr<Glib::VariantDict>& options, const Glib::ustring& arg_name, T_ArgType& arg_value)
{
    arg_value = T_ArgType();
    if (options->lookup_value(arg_name, arg_value)) {
        return true;
    }

    return false;
}

/*
 * "on_command_line" signal handler
 * This is executed in the first-running process.
 */
int on_command_line(const Glib::RefPtr<Gio::ApplicationCommandLine>& command_line,
                    PavuApplication *app)
{
    const auto options = command_line->get_options_dict();

    get_arg_value(options, "tab", app->tab);
    get_arg_value(options, "retry", app->retry);
    get_arg_value(options, "maximize", app->maximize);
    get_arg_value(options, "version", app->version);

    if (app->version) {
        printf("%s\n", PACKAGE_STRING);
        return 0;
    }

    app->activate();

    return 0;
}

int main(int argc, char *argv[]) {

    /* Initialize the i18n stuff */
    bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
    textdomain(GETTEXT_PACKAGE);

    signal(SIGPIPE, SIG_IGN);

    /* Create the application */
    globalInstance = PavuApplication();

    /* Add command-line options */
    globalInstance.add_main_option_entry(
        Gio::Application::OptionType::OPTION_TYPE_INT,
        "tab", 't',
        _("Select a specific tab on load."),
        _("number"));

    globalInstance.add_main_option_entry(
        Gio::Application::OptionType::OPTION_TYPE_BOOL,
        "retry", 'r',
        _("Retry forever if pa quits (every 5 seconds)."));

    globalInstance.add_main_option_entry(
        Gio::Application::OptionType::OPTION_TYPE_BOOL,
        "maximize", 'm',
        _("Maximize the window."));

    globalInstance.add_main_option_entry(
        Gio::Application::OptionType::OPTION_TYPE_BOOL,
        "version", 'v',
        _("Show version."));

    /* Connect to the "on_command_line" signal which is fired
     * when the application receives command-line arguments
     */
    globalInstance.signal_command_line().connect(sigc::bind(sigc::ptr_fun(&on_command_line), &globalInstance), false);

    /* Run the application.
     * In the first launched instance, this will return when its window is
     * closed. In subsequently launches instances, this will only signal the
     * first instance to handle a new request, and exit immediately.
     * Handling a new request consists of presenting the existing window (and
     * optionally, select a tab).
     */
    return globalInstance.run(argc, argv);
}

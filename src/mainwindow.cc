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

#include <set>

#include "mainwindow.h"

#include "cardwidget.h"
#include "sinkwidget.h"
#include "sourcewidget.h"
#include "sinkinputwidget.h"
#include "sourceoutputwidget.h"
#include "rolewidget.h"

#include "i18n.h"

/* Used for profile sorting */
struct profile_prio_compare {
    bool operator() (const pa_card_profile_info2& lhs, const pa_card_profile_info2& rhs) const {

        if (lhs.priority == rhs.priority)
            return strcmp(lhs.name, rhs.name) > 0;

        return lhs.priority > rhs.priority;
    }
};

struct sink_port_prio_compare {
    bool operator() (const pa_sink_port_info& lhs, const pa_sink_port_info& rhs) const {

        if (lhs.priority == rhs.priority)
            return strcmp(lhs.name, rhs.name) > 0;

        return lhs.priority > rhs.priority;
    }
};

struct source_port_prio_compare {
    bool operator() (const pa_source_port_info& lhs, const pa_source_port_info& rhs) const {

        if (lhs.priority == rhs.priority)
            return strcmp(lhs.name, rhs.name) > 0;

        return lhs.priority > rhs.priority;
    }
};

MainWindow::MainWindow(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& x) :
    Gtk::Window(cobject),
    showSinkInputType(SINK_INPUT_CLIENT),
    showSinkType(SINK_ALL),
    showSourceOutputType(SOURCE_OUTPUT_CLIENT),
    showSourceType(SOURCE_NO_MONITOR),
    eventRoleWidget(NULL),
    canRenameDevices(false),
    m_connected(false),
    m_config_filename(NULL) {
#ifdef HAVE_LIBCANBERRA
    ca_context_create(&canberraContext);
    ca_context_set_driver(canberraContext, "pulse");
#endif
    cardsVBox = x->get_widget<Gtk::Box>("cardsVBox");
    streamsVBox = x->get_widget<Gtk::Box>("streamsVBox");
    recsVBox = x->get_widget<Gtk::Box>("recsVBox");
    sinksVBox = x->get_widget<Gtk::Box>("sinksVBox");
    sourcesVBox = x->get_widget<Gtk::Box>("sourcesVBox");
    noCardsLabel = x->get_widget<Gtk::Label>("noCardsLabel");
    noStreamsLabel = x->get_widget<Gtk::Label>("noStreamsLabel");
    noRecsLabel = x->get_widget<Gtk::Label>("noRecsLabel");
    noSinksLabel = x->get_widget<Gtk::Label>("noSinksLabel");
    noSourcesLabel = x->get_widget<Gtk::Label>("noSourcesLabel");
    connectingLabel = x->get_widget<Gtk::Label>("connectingLabel");
    sinkInputTypeComboBox = x->get_widget<Gtk::ComboBox>("sinkInputTypeComboBox");
    sourceOutputTypeComboBox = x->get_widget<Gtk::ComboBox>("sourceOutputTypeComboBox");
    sinkTypeComboBox = x->get_widget<Gtk::ComboBox>("sinkTypeComboBox");
    sourceTypeComboBox = x->get_widget<Gtk::ComboBox>("sourceTypeComboBox");
    notebook = x->get_widget<Gtk::Notebook>("notebook");
    showVolumeMetersCheckButton = x->get_widget<Gtk::CheckButton>("showVolumeMetersCheckButton");
    hideUnavailableCardProfilesCheckButton = x->get_widget<Gtk::CheckButton>("hideUnavailableCardProfilesCheckButton");

    sinkInputTypeComboBox->set_active((int) showSinkInputType);
    sourceOutputTypeComboBox->set_active((int) showSourceOutputType);
    sinkTypeComboBox->set_active((int) showSinkType);
    sourceTypeComboBox->set_active((int) showSourceType);

    sinkInputTypeComboBox->signal_changed().connect(sigc::mem_fun(*this, &MainWindow::onSinkInputTypeComboBoxChanged));
    sourceOutputTypeComboBox->signal_changed().connect(sigc::mem_fun(*this, &MainWindow::onSourceOutputTypeComboBoxChanged));
    sinkTypeComboBox->signal_changed().connect(sigc::mem_fun(*this, &MainWindow::onSinkTypeComboBoxChanged));
    sourceTypeComboBox->signal_changed().connect(sigc::mem_fun(*this, &MainWindow::onSourceTypeComboBoxChanged));
    showVolumeMetersCheckButton->signal_toggled().connect(sigc::mem_fun(*this, &MainWindow::onShowVolumeMetersCheckButtonToggled));
    hideUnavailableCardProfilesCheckButton->signal_toggled().connect(sigc::mem_fun(*this, &MainWindow::onHideUnavailableCardProfilesCheckButtonToggled));

    auto event_controller_key = Gtk::EventControllerKey::create();
    event_controller_key->signal_key_pressed().connect(sigc::mem_fun(*this, &MainWindow::on_key_press_event), false);
    this->add_controller(event_controller_key);

    GKeyFile* config = g_key_file_new();
    g_assert(config);
    GKeyFileFlags flags = (GKeyFileFlags)( G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS);
    GError *err = NULL;
    m_config_filename = g_strconcat(g_get_user_config_dir(), "/pavucontrol.ini", NULL);

    /* Load the GKeyFile from keyfile.conf or return. */
    if (g_key_file_load_from_file (config, m_config_filename, flags, &err)) {
        int width  = g_key_file_get_integer(config, "window", "width", NULL);
        int height = g_key_file_get_integer(config, "window", "height", NULL);

        /* When upgrading from a previous version, set showVolumeMeters to TRUE
         * (default from glade file), so users don't complain about missing
         * volume meters. */
        if (g_key_file_has_key(config, "window", "showVolumeMeters", NULL)) {
            showVolumeMetersCheckButton->set_active(g_key_file_get_boolean(config, "window", "showVolumeMeters", NULL));
        }
        if (g_key_file_has_key(config, "window", "hideUnavailableCardProfiles", NULL)) {
            hideUnavailableCardProfilesCheckButton->set_active(g_key_file_get_boolean(config, "window", "hideUnavailableCardProfiles", NULL));
        }

        int default_width, default_height;
        get_default_size(default_width, default_height);
        if (width >= default_width && height >= default_height)
            set_default_size(width, height);

        int sinkInputTypeSelection = g_key_file_get_integer(config, "window", "sinkInputType", &err);
        if (err == NULL)
            sinkInputTypeComboBox->set_active(sinkInputTypeSelection);
        else {
            g_error_free(err);
            err = NULL;
        }

        int sourceOutputTypeSelection = g_key_file_get_integer(config, "window", "sourceOutputType", &err);
        if (err == NULL)
            sourceOutputTypeComboBox->set_active(sourceOutputTypeSelection);
        else {
            g_error_free(err);
            err = NULL;
        }

        int sinkTypeSelection = g_key_file_get_integer(config, "window", "sinkType", &err);
        if (err == NULL)
            sinkTypeComboBox->set_active(sinkTypeSelection);
        else {
            g_error_free(err);
            err = NULL;
        }

        int sourceTypeSelection = g_key_file_get_integer(config, "window", "sourceType", &err);
        if (err == NULL)
            sourceTypeComboBox->set_active(sourceTypeSelection);
        else {
            g_error_free(err);
            err = NULL;
        }
    } else {
        g_debug(_("Error reading config file %s: %s"), m_config_filename, err->message);
        g_error_free(err);
    }
    g_key_file_free(config);


    /* Hide first and show when we're connected */
    notebook->hide();
    connectingLabel->show();
}

MainWindow* MainWindow::create(bool maximize) {
    MainWindow* w;
    Glib::RefPtr<Gtk::Builder> x = Gtk::Builder::create_from_resource("/org/pulseaudio/pavucontrol/ui/mainwindow.ui", "mainWindow");
    w = Gtk::Builder::get_widget_derived<MainWindow>(x, "mainWindow");
    w->get_style_context()->add_class("pavucontrol-window");
    if (w && maximize)
        w->maximize();
    return w;
}

void MainWindow::on_realize() {
    Gtk::Window::on_realize();

    set_cursor(Gdk::Cursor::create("wait"));
}

bool MainWindow::on_key_press_event(guint keyval, guint keycode, Gdk::ModifierType state) {

    if ((state & Gdk::ModifierType::CONTROL_MASK) == Gdk::ModifierType::CONTROL_MASK) {
        switch (keyval) {
            case GDK_KEY_KP_1:
            case GDK_KEY_KP_2:
            case GDK_KEY_KP_3:
            case GDK_KEY_KP_4:
            case GDK_KEY_KP_5:
                notebook->set_current_page(keyval - GDK_KEY_KP_1);
                return true;
            case GDK_KEY_1:
            case GDK_KEY_2:
            case GDK_KEY_3:
            case GDK_KEY_4:
            case GDK_KEY_5:
                notebook->set_current_page(keyval - GDK_KEY_1);
                return true;
            case GDK_KEY_W:
            case GDK_KEY_Q:
            case GDK_KEY_w:
            case GDK_KEY_q:
                close();
                return true;
        }
    }
    return false;
}

MainWindow::~MainWindow() {
    GKeyFile* config = g_key_file_new();
    g_assert(config);

    int width, height;
    get_default_size(width, height);
    g_key_file_set_integer(config, "window", "width", width);
    g_key_file_set_integer(config, "window", "height", height);
    g_key_file_set_integer(config, "window", "sinkInputType", sinkInputTypeComboBox->get_active_row_number());
    g_key_file_set_integer(config, "window", "sourceOutputType", sourceOutputTypeComboBox->get_active_row_number());
    g_key_file_set_integer(config, "window", "sinkType", sinkTypeComboBox->get_active_row_number());
    g_key_file_set_integer(config, "window", "sourceType", sourceTypeComboBox->get_active_row_number());
    g_key_file_set_integer(config, "window", "showVolumeMeters", showVolumeMetersCheckButton->get_active());
    g_key_file_set_integer(config, "window", "hideUnavailableCardProfiles", hideUnavailableCardProfilesCheckButton->get_active());

    gsize filelen;
    GError *err = NULL;
    gchar *filedata = g_key_file_to_data(config, &filelen, &err);
    if (err) {
        show_error(this, _("Error saving preferences"));
        g_error_free(err);
        goto finish;
    }

    g_file_set_contents(m_config_filename, filedata, filelen, &err);
    g_free(filedata);
    if (err) {
        gchar* msg = g_strconcat(_("Error writing config file %s"), m_config_filename, NULL);
        show_error(this, msg);
        g_free(msg);
        g_error_free(err);
        goto finish;
    }

finish:

    g_key_file_free(config);
    g_free(m_config_filename);

    while (!clientNames.empty()) {
        std::map<uint32_t, char*>::iterator i = clientNames.begin();
        g_free(i->second);
        clientNames.erase(i);
    }
#ifdef HAVE_LIBCANBERRA
    ca_context_destroy(canberraContext);
#endif
}

static void set_icon_name_default(Gtk::Image *i, const char *name, const char *fallback) {
    /* We emulate the behavior of the GTK_ICON_LOOKUP_GENERIC_FALLBACK flag from Gtk3 */
    Glib::RefPtr<Gtk::IconTheme> theme;
    theme = Gtk::IconTheme::get_for_display(Gdk::Display::get_default());
    std::string iconName(name ? name : fallback);

    while (!theme->has_icon(iconName.c_str())) {
        /* Fallback to a symbolic icon, if it exists */
        if (theme->has_icon((iconName + "-symbolic").c_str())) {
            iconName = iconName + "-symbolic";
            break;
        }

        size_t lastDashIndex = iconName.find_last_of("-");
        if (lastDashIndex == std::string::npos) {
            iconName = fallback;
            break;
        }
        iconName = iconName.substr(0, lastDashIndex);
    }

    i->set_from_icon_name(iconName.c_str());
}

static void updatePorts(DeviceWidget *w, std::map<Glib::ustring, PortInfo> &ports) {
    std::map<Glib::ustring, PortInfo>::iterator it;
    PortInfo p;

    for (uint32_t i = 0; i < w->ports.size(); i++) {
        Glib::ustring desc;
        it = ports.find(w->ports[i].first);

        if (it == ports.end())
            continue;

        p = it->second;
        desc = p.description;

        if (p.available == PA_PORT_AVAILABLE_YES)
            desc +=  _(" (plugged in)");
        else if (p.available == PA_PORT_AVAILABLE_NO) {
            if (p.name == "analog-output-speaker" ||
                p.name == "analog-input-microphone-internal")
                desc += _(" (unavailable)");
            else
                desc += _(" (unplugged)");
        }

        w->ports[i].second = desc;
    }

    it = ports.find(w->activePort);

    if (it != ports.end()) {
        p = it->second;
        w->setLatencyOffset(p.latency_offset);
    }
}

void MainWindow::selectBestTab() {
    if (sinkInputWidgets.size() > 0)
        notebook->set_current_page(0);
    else if (sourceOutputWidgets.size() > 0)
        notebook->set_current_page(1);
    else if (sourceWidgets.size() > 0 && sinkWidgets.size() == 0)
        notebook->set_current_page(3);
    else
        notebook->set_current_page(2);
}

void MainWindow::selectTab(int tab_number) {
    if (tab_number > 0 && tab_number <= notebook->get_n_pages())
        notebook->set_current_page(tab_number - 1);
}

void MainWindow::updateCard(const pa_card_info &info) {
    CardWidget *w;
    bool is_new = false;
    const char *description, *icon;
    std::set<pa_card_profile_info2,profile_prio_compare> profile_priorities;

    if (cardWidgets.count(info.index))
        w = cardWidgets[info.index];
    else {
        cardWidgets[info.index] = w = CardWidget::create();
        cardsVBox->append(*w);
        w->unreference();
        w->index = info.index;
        w->hideUnavailableProfiles = hideUnavailableCardProfilesCheckButton->get_active();
        w->prepareMenu();
        is_new = true;
    }

    w->updating = true;

    w->pulse_card_name = info.name;

    description = pa_proplist_gets(info.proplist, PA_PROP_DEVICE_DESCRIPTION);
    w->name = description ? description : info.name;
    gchar *txt;
    w->nameLabel->set_markup(txt = g_markup_printf_escaped("%s", w->name.c_str()));
    g_free(txt);

    icon = pa_proplist_gets(info.proplist, PA_PROP_DEVICE_ICON_NAME);
    set_icon_name_default(w->iconImage, icon, "audio-card");

    w->hasSinks = w->hasSources = false;
    profile_priorities.clear();
    for (uint32_t i=0; i<info.n_profiles; ++i) {
        w->hasSinks = w->hasSinks || (info.profiles[i].n_sinks > 0);
        w->hasSources = w->hasSources || (info.profiles[i].n_sources > 0);
        profile_priorities.insert(*info.profiles2[i]);
    }

    w->ports.clear();
    for (uint32_t i = 0; i < info.n_ports; ++i) {
        PortInfo p;

        p.name = info.ports[i]->name;
        p.description = info.ports[i]->description;
        p.priority = info.ports[i]->priority;
        p.available = info.ports[i]->available;
        p.direction = info.ports[i]->direction;
        p.latency_offset = info.ports[i]->latency_offset;
        for (uint32_t j = 0; j < info.ports[i]->n_profiles; j++)
            p.profiles.push_back(info.ports[i]->profiles[j]->name);

        w->ports[p.name] = p;
    }

    w->profiles.clear();
    for (std::set<pa_card_profile_info2>::iterator profileIt = profile_priorities.begin(); profileIt != profile_priorities.end(); ++profileIt) {
        bool hasNo = false, hasOther = false, available = true;
        std::map<Glib::ustring, PortInfo>::iterator portIt;
        Glib::ustring desc = profileIt->description;

        for (portIt = w->ports.begin(); portIt != w->ports.end(); portIt++) {
            PortInfo port = portIt->second;

            if (std::find(port.profiles.begin(), port.profiles.end(), profileIt->name) == port.profiles.end())
                continue;

            if (port.available == PA_PORT_AVAILABLE_NO)
                hasNo = true;
            else {
                hasOther = true;
                break;
            }
        }
        if (hasNo && !hasOther) {
            desc += _(" (unplugged)");
            available = false;
        }

        if (!profileIt->available) {
            desc += _(" (unavailable)");
            available = false;
        }

        w->profiles.push_back(std::pair<Glib::ustring, Glib::ustring>(profileIt->name, desc));
        w->availableProfiles[profileIt->name] = available;
    }

    w->activeProfile = info.active_profile ? info.active_profile->name : "";

    /* Because the port info for sinks and sources is discontinued we need
     * to update the port info for them here. */

    if (w->hasSinks) {
        std::map<uint32_t, SinkWidget*>::iterator it;

        for (it = sinkWidgets.begin() ; it != sinkWidgets.end(); it++) {
            SinkWidget *sw = it->second;

            if (sw->card_index == w->index) {
                sw->updating = true;
                updatePorts(sw, w->ports);
                sw->updating = false;
            }
        }
    }

    if (w->hasSources) {
        std::map<uint32_t, SourceWidget*>::iterator it;

        for (it = sourceWidgets.begin() ; it != sourceWidgets.end(); it++) {
            SourceWidget *sw = it->second;

            if (sw->card_index == w->index) {
                sw->updating = true;
                updatePorts(sw, w->ports);
                sw->updating = false;
            }
        }
    }

    w->prepareMenu();

    if (is_new)
        updateDeviceVisibility();

    w->updating = false;
}

void MainWindow::updateCardCodecs(const std::string& card_name, const std::unordered_map<std::string, std::string>& codecs) {
    CardWidget *w = NULL;

    for (auto c : cardWidgets) {
        if (card_name.compare(c.second->pulse_card_name) == 0)
            w = c.second;
    }

    if (!w)
        return;

    w->updating = true;

    w->codecs.clear();

    /* insert profiles */
    for (auto e : codecs) {
        w->codecs.push_back(std::pair<Glib::ustring, Glib::ustring>(e.first, e.second));
    }

    w->prepareMenu();

    w->updating = false;
}

void MainWindow::setActiveCodec(const std::string& card_name, const std::string& codec) {
    CardWidget *w = NULL;

    for (auto c : cardWidgets) {
        if (card_name.compare(c.second->pulse_card_name) == 0)
            w = c.second;
    }

    if (!w)
        return;

    w->updating = true;

    w->activeCodec = codec;

    w->prepareMenu();

    w->updating = false;
}

void MainWindow::setCardProfileIsSticky(const std::string& card_name, gboolean profile_is_sticky) {
    CardWidget *w = NULL;

    for (auto c : cardWidgets) {
        if (card_name.compare(c.second->pulse_card_name) == 0)
            w = c.second;
    }

    if (!w)
        return;

    w->updating = true;

    /* make sure that profile lock toggle button is visible */
    w->hasProfileLock = true;
    w->profileLockToggleButton->set_active(profile_is_sticky);

    w->prepareMenu();

    w->updating = false;
}

bool MainWindow::updateSink(const pa_sink_info &info) {
    SinkWidget *w;
    bool is_new = false;
    const char *icon;
    std::map<uint32_t, CardWidget*>::iterator cw;
    std::set<pa_sink_port_info,sink_port_prio_compare> port_priorities;

    if (sinkWidgets.count(info.index))
        w = sinkWidgets[info.index];
    else {
        sinkWidgets[info.index] = w = SinkWidget::create(this);
        w->setChannelMap(info.channel_map, !!(info.flags & PA_SINK_DECIBEL_VOLUME));
        sinksVBox->append(*w);
        w->unreference();
        w->index = info.index;
        w->monitor_index = info.monitor_source;
        is_new = true;

        w->setBaseVolume(info.base_volume);
        w->setVolumeMeterVisible(showVolumeMetersCheckButton->get_active());
    }

    w->updating = true;

    w->card_index = info.card;
    w->name = info.name;
    w->description = info.description;
    w->type = info.flags & PA_SINK_HARDWARE ? SINK_HARDWARE : SINK_VIRTUAL;

    w->boldNameLabel->set_text("");
    gchar *txt;
    w->nameLabel->set_markup(txt = g_markup_printf_escaped("%s", info.description));
    w->nameLabel->set_tooltip_text(info.description);
    g_free(txt);

    icon = pa_proplist_gets(info.proplist, PA_PROP_DEVICE_ICON_NAME);
    set_icon_name_default(w->iconImage, icon, "audio-card");

    w->setVolume(info.volume);
    w->muteToggleButton->set_active(info.mute);

    w->setDefault(w->name == defaultSinkName);

    port_priorities.clear();
    for (uint32_t i=0; i<info.n_ports; ++i) {
        port_priorities.insert(*info.ports[i]);
    }

    w->ports.clear();
    for (std::set<pa_sink_port_info>::iterator i = port_priorities.begin(); i != port_priorities.end(); ++i)
        w->ports.push_back(std::pair<Glib::ustring, Glib::ustring>(i->name, i->description));

    w->activePort = info.active_port ? info.active_port->name : "";

    cw = cardWidgets.find(info.card);

    if (cw != cardWidgets.end())
        updatePorts(w, cw->second->ports);

#ifdef PA_SINK_SET_FORMATS
    w->setDigital(info.flags & PA_SINK_SET_FORMATS);
#endif

    w->prepareMenu();

    if (is_new)
        updateDeviceVisibility();

    w->updating = false;

    return is_new;
}

static void suspended_callback(pa_stream *s, void *userdata) {
    MainWindow *w = static_cast<MainWindow*>(userdata);

    if (pa_stream_is_suspended(s))
        w->updateVolumeMeter(pa_stream_get_device_index(s), PA_INVALID_INDEX, -1);
}

static void read_callback(pa_stream *s, size_t length, void *userdata) {
    MainWindow *w = static_cast<MainWindow*>(userdata);
    const void *data;
    double v;

    if (pa_stream_peek(s, &data, &length) < 0) {
        show_error(w, _("Failed to read data from stream"));
        return;
    }

    if (!data) {
        /* NULL data means either a hole or empty buffer.
         * Only drop the stream when there is a hole (length > 0) */
        if (length)
            pa_stream_drop(s);
        return;
    }

    assert(length > 0);
    assert(length % sizeof(float) == 0);

    v = ((const float*) data)[length / sizeof(float) -1];

    pa_stream_drop(s);

    if (v < 0)
        v = 0;
    if (v > 1)
        v = 1;

    w->updateVolumeMeter(pa_stream_get_device_index(s), pa_stream_get_monitor_stream(s), v);
}

pa_stream* MainWindow::createMonitorStreamForSource(uint32_t source_idx, uint32_t stream_idx = -1, bool suspend = false) {
    pa_stream *s;
    char t[16];
    pa_buffer_attr attr;
    pa_sample_spec ss;
    pa_stream_flags_t flags;

    ss.channels = 1;
    ss.format = PA_SAMPLE_FLOAT32;
    ss.rate = PEAKS_RATE;

    memset(&attr, 0, sizeof(attr));
    attr.fragsize = sizeof(float);
    attr.maxlength = (uint32_t) -1;

    snprintf(t, sizeof(t), "%u", source_idx);

    if (!(s = pa_stream_new(get_context(), _("Peak detect"), &ss, NULL))) {
        show_error(this, _("Failed to create monitoring stream"));
        return NULL;
    }

    if (stream_idx != (uint32_t) -1)
        pa_stream_set_monitor_stream(s, stream_idx);

    pa_stream_set_read_callback(s, read_callback, this);
    pa_stream_set_suspended_callback(s, suspended_callback, this);

    flags = (pa_stream_flags_t) (PA_STREAM_DONT_MOVE | PA_STREAM_PEAK_DETECT | PA_STREAM_ADJUST_LATENCY |
                                 (suspend ? PA_STREAM_DONT_INHIBIT_AUTO_SUSPEND : PA_STREAM_NOFLAGS) |
                                 (!showVolumeMetersCheckButton->get_active() ? PA_STREAM_START_CORKED : PA_STREAM_NOFLAGS));

    if (pa_stream_connect_record(s, t, &attr, flags) < 0) {
        show_error(this, _("Failed to connect monitoring stream"));
        pa_stream_unref(s);
        return NULL;
    }
    return s;
}

void MainWindow::createMonitorStreamForSinkInput(SinkInputWidget* w, uint32_t sink_idx) {
    if (!sinkWidgets.count(sink_idx))
        return;

    if (w->peak) {
        pa_stream_disconnect(w->peak);
        pa_stream_unref(w->peak);
        w->peak = NULL;
    }

    w->peak = createMonitorStreamForSource(sinkWidgets[sink_idx]->monitor_index, w->index);
}

void MainWindow::updateSource(const pa_source_info &info) {
    SourceWidget *w;
    bool is_new = false;
    const char *icon;
    std::map<uint32_t, CardWidget*>::iterator cw;
    std::set<pa_source_port_info,source_port_prio_compare> port_priorities;
    SourceType type = info.monitor_of_sink != PA_INVALID_INDEX ? SOURCE_MONITOR : (info.flags & PA_SOURCE_HARDWARE ? SOURCE_HARDWARE : SOURCE_VIRTUAL);

    if (sourceWidgets.count(info.index))
        w = sourceWidgets[info.index];
    else {
        sourceWidgets[info.index] = w = SourceWidget::create(this);
        w->setChannelMap(info.channel_map, !!(info.flags & PA_SOURCE_DECIBEL_VOLUME));
        sourcesVBox->append(*w);
        w->unreference();
        w->index = info.index;
        is_new = true;

        w->setBaseVolume(info.base_volume);
        w->setVolumeMeterVisible(showVolumeMetersCheckButton->get_active());

        if (pa_context_get_server_protocol_version(get_context()) >= 13)
            w->peak = createMonitorStreamForSource(info.index, -1, !!(info.flags & PA_SOURCE_NETWORK) || type == SOURCE_MONITOR);
    }

    w->updating = true;

    w->card_index = info.card;
    w->name = info.name;
    w->description = info.description;
    w->type = type;

    w->boldNameLabel->set_text("");
    gchar *txt;
    w->nameLabel->set_markup(txt = g_markup_printf_escaped("%s", info.description));
    w->nameLabel->set_tooltip_text(info.description);
    g_free(txt);

    icon = pa_proplist_gets(info.proplist, PA_PROP_DEVICE_ICON_NAME);
    set_icon_name_default(w->iconImage, icon, "audio-input-microphone");

    w->setVolume(info.volume);
    w->muteToggleButton->set_active(info.mute);

    w->setDefault(w->name == defaultSourceName);

    port_priorities.clear();
    for (uint32_t i=0; i<info.n_ports; ++i) {
        port_priorities.insert(*info.ports[i]);
    }

    w->ports.clear();
    for (std::set<pa_source_port_info>::iterator i = port_priorities.begin(); i != port_priorities.end(); ++i)
        w->ports.push_back(std::pair<Glib::ustring, Glib::ustring>(i->name, i->description));

    w->activePort = info.active_port ? info.active_port->name : "";

    cw = cardWidgets.find(info.card);

    if (cw != cardWidgets.end())
        updatePorts(w, cw->second->ports);

    w->prepareMenu();

    if (is_new)
        updateDeviceVisibility();

    w->updating = false;
}

void MainWindow::setIconFromProplist(Gtk::Image *icon, pa_proplist *l, const char *def) {
    const char *t;

    if ((t = pa_proplist_gets(l, PA_PROP_MEDIA_ICON_NAME)))
        goto finish;

    if ((t = pa_proplist_gets(l, PA_PROP_WINDOW_ICON_NAME)))
        goto finish;

    if ((t = pa_proplist_gets(l, PA_PROP_APPLICATION_ICON_NAME)))
        goto finish;

    if ((t = pa_proplist_gets(l, PA_PROP_MEDIA_ROLE))) {

        if (strcmp(t, "video") == 0 ||
            strcmp(t, "phone") == 0)
            goto finish;

        if (strcmp(t, "music") == 0) {
            t = "audio";
            goto finish;
        }

        if (strcmp(t, "game") == 0) {
            t = "applications-games";
            goto finish;
        }

        if (strcmp(t, "event") == 0) {
            t = "dialog-information";
            goto finish;
        }
    }

finish:
    set_icon_name_default(icon, t, def);
}

void MainWindow::updateSinkInput(const pa_sink_input_info &info) {
    const char *t;
    SinkInputWidget *w;
    bool is_new = false;

    if ((t = pa_proplist_gets(info.proplist, "module-stream-restore.id"))) {
        if (strcmp(t, "sink-input-by-media-role:event") == 0) {
            g_debug(_("Ignoring sink-input due to it being designated as an event and thus handled by the Event widget"));
            return;
        }
    }

    if (sinkInputWidgets.count(info.index)) {
        w = sinkInputWidgets[info.index];
        if (pa_context_get_server_protocol_version(get_context()) >= 13)
            if (w->sinkIndex() != info.sink)
                createMonitorStreamForSinkInput(w, info.sink);
    } else {
        sinkInputWidgets[info.index] = w = SinkInputWidget::create(this);
        w->setChannelMap(info.channel_map, true);
        streamsVBox->append(*w);
        w->unreference();
        w->index = info.index;
        w->clientIndex = info.client;
        is_new = true;
        w->setVolumeMeterVisible(showVolumeMetersCheckButton->get_active());

        if (pa_context_get_server_protocol_version(get_context()) >= 13)
            createMonitorStreamForSinkInput(w, info.sink);
    }

    w->updating = true;

    w->type = info.client != PA_INVALID_INDEX ? SINK_INPUT_CLIENT : SINK_INPUT_VIRTUAL;

    w->setSinkIndex(info.sink);

    char *txt;
    if (clientNames.count(info.client)) {
        w->boldNameLabel->set_markup(txt = g_markup_printf_escaped("<b>%s</b>", clientNames[info.client]));
        g_free(txt);
        w->nameLabel->set_markup(txt = g_markup_printf_escaped(": %s", info.name));
        g_free(txt);
    } else {
        w->boldNameLabel->set_text("");
        w->nameLabel->set_label(info.name);
    }

    w->nameLabel->set_tooltip_text(info.name);

    setIconFromProplist(w->iconImage, info.proplist, "application-x-executable");

    w->setVolume(info.volume);
    w->muteToggleButton->set_active(info.mute);

    w->updating = false;

    if (is_new)
        updateDeviceVisibility();
}

void MainWindow::updateSourceOutput(const pa_source_output_info &info) {
    SourceOutputWidget *w;
    const char *app;
    bool is_new = false;

    if ((app = pa_proplist_gets(info.proplist, PA_PROP_APPLICATION_ID)))
        if (strcmp(app, "org.PulseAudio.pavucontrol") == 0
            || strcmp(app, "org.gnome.VolumeControl") == 0
            || strcmp(app, "org.kde.kmixd") == 0)
            return;

    if (sourceOutputWidgets.count(info.index))
        w = sourceOutputWidgets[info.index];
    else {
        sourceOutputWidgets[info.index] = w = SourceOutputWidget::create(this);
#if HAVE_SOURCE_OUTPUT_VOLUMES
        w->setChannelMap(info.channel_map, true);
#endif
        recsVBox->append(*w);
        w->unreference();
        w->index = info.index;
        w->clientIndex = info.client;
        is_new = true;
        w->setVolumeMeterVisible(showVolumeMetersCheckButton->get_active());
    }

    w->updating = true;

    w->type = info.client != PA_INVALID_INDEX ? SOURCE_OUTPUT_CLIENT : SOURCE_OUTPUT_VIRTUAL;

    w->setSourceIndex(info.source);

    char *txt;
    if (clientNames.count(info.client)) {
        w->boldNameLabel->set_markup(txt = g_markup_printf_escaped("<b>%s</b>", clientNames[info.client]));
        g_free(txt);
        w->nameLabel->set_markup(txt = g_markup_printf_escaped(": %s", info.name));
        g_free(txt);
    } else {
        w->boldNameLabel->set_text("");
        w->nameLabel->set_label(info.name);
    }

    w->nameLabel->set_tooltip_text(info.name);

    setIconFromProplist(w->iconImage, info.proplist, "application-x-executable");

#if HAVE_SOURCE_OUTPUT_VOLUMES
    w->setVolume(info.volume);
    w->muteToggleButton->set_active(info.mute);
#endif

    w->updating = false;

    if (is_new)
        updateDeviceVisibility();
}

void MainWindow::updateClient(const pa_client_info &info) {

    g_free(clientNames[info.index]);
    clientNames[info.index] = g_strdup(info.name);

    for (std::map<uint32_t, SinkInputWidget*>::iterator i = sinkInputWidgets.begin(); i != sinkInputWidgets.end(); ++i) {
        SinkInputWidget *w = i->second;

        if (!w)
            continue;

        if (w->clientIndex == info.index) {
            gchar *txt;
            w->boldNameLabel->set_markup(txt = g_markup_printf_escaped("<b>%s</b>", info.name));
            g_free(txt);
        }
    }
}

void MainWindow::updateServer(const pa_server_info &info) {

    defaultSourceName = info.default_source_name ? info.default_source_name : "";
    defaultSinkName = info.default_sink_name ? info.default_sink_name : "";

    for (std::map<uint32_t, SinkWidget*>::iterator i = sinkWidgets.begin(); i != sinkWidgets.end(); ++i) {
        SinkWidget *w = i->second;

        if (!w)
            continue;

        w->updating = true;
        w->setDefault(w->name == defaultSinkName);

        w->updating = false;
    }

    for (std::map<uint32_t, SourceWidget*>::iterator i = sourceWidgets.begin(); i != sourceWidgets.end(); ++i) {
        SourceWidget *w = i->second;

        if (!w)
            continue;

        w->updating = true;
        w->setDefault(w->name == defaultSourceName);
        w->updating = false;
    }
}

bool MainWindow::createEventRoleWidget() {
    if (eventRoleWidget)
        return FALSE;

    pa_channel_map cm = {
        1, { PA_CHANNEL_POSITION_MONO }
    };

    eventRoleWidget = RoleWidget::create();
    streamsVBox->append(*eventRoleWidget);
    eventRoleWidget->unreference();
    eventRoleWidget->role = "sink-input-by-media-role:event";
    eventRoleWidget->setChannelMap(cm, true);

    eventRoleWidget->boldNameLabel->set_text("");
    eventRoleWidget->nameLabel->set_label(_("System Sounds"));

    eventRoleWidget->iconImage->set_from_icon_name("multimedia-volume-control");

    eventRoleWidget->device = "";

    eventRoleWidget->updating = true;

    pa_cvolume volume;
    volume.channels = 1;
    volume.values[0] = PA_VOLUME_NORM;

    eventRoleWidget->setVolume(volume);
    eventRoleWidget->muteToggleButton->set_active(false);

    eventRoleWidget->updating = false;

    return TRUE;
}

void MainWindow::deleteEventRoleWidget() {

    if (eventRoleWidget)
        delete eventRoleWidget;

    eventRoleWidget = NULL;
}

void MainWindow::updateRole(const pa_ext_stream_restore_info &info) {
    pa_cvolume volume;
    bool is_new = false;

    if (strcmp(info.name, "sink-input-by-media-role:event") != 0)
        return;

    is_new = createEventRoleWidget();

    eventRoleWidget->updating = true;

    eventRoleWidget->device = info.device ? info.device : "";

    volume.channels = 1;
    volume.values[0] = pa_cvolume_max(&info.volume);

    eventRoleWidget->setVolume(volume);
    eventRoleWidget->muteToggleButton->set_active(info.mute);

    eventRoleWidget->updating = false;

    if (is_new)
        updateDeviceVisibility();
}

#if HAVE_EXT_DEVICE_RESTORE_API
void MainWindow::updateDeviceInfo(const pa_ext_device_restore_info &info) {

    if (sinkWidgets.count(info.index)) {
        SinkWidget *w;
        pa_format_info *format;

        w = sinkWidgets[info.index];

        w->updating = true;

        /* Unselect everything */
        for (int j = 1; j < PAVU_NUM_ENCODINGS; ++j)
            w->encodings[j].widget->set_active(false);


        for (uint8_t i = 0; i < info.n_formats; ++i) {
            format = info.formats[i];
            for (int j = 1; j < PAVU_NUM_ENCODINGS; ++j) {
                if (format->encoding == w->encodings[j].encoding) {
                    w->encodings[j].widget->set_active(true);
                    break;
                }
            }
        }

        w->updating = false;
    }
}
#endif


void MainWindow::updateVolumeMeter(uint32_t source_index, uint32_t sink_input_idx, double v) {

    if (sink_input_idx != PA_INVALID_INDEX) {
        SinkInputWidget *w;

        if (sinkInputWidgets.count(sink_input_idx)) {
            w = sinkInputWidgets[sink_input_idx];
            w->updatePeak(v);
        }

    } else {

        for (std::map<uint32_t, SinkWidget*>::iterator i = sinkWidgets.begin(); i != sinkWidgets.end(); ++i) {
            SinkWidget* w = i->second;

            if (w->monitor_index == source_index)
                w->updatePeak(v);
        }

        for (std::map<uint32_t, SourceWidget*>::iterator i = sourceWidgets.begin(); i != sourceWidgets.end(); ++i) {
            SourceWidget* w = i->second;

            if (w->index == source_index)
                w->updatePeak(v);
        }

        for (std::map<uint32_t, SourceOutputWidget*>::iterator i = sourceOutputWidgets.begin(); i != sourceOutputWidgets.end(); ++i) {
            SourceOutputWidget* w = i->second;

            if (w->sourceIndex() == source_index)
                w->updatePeak(v);
        }
    }
}

static guint idle_source = 0;

gboolean idle_cb(gpointer data) {
    ((MainWindow*) data)->reallyUpdateDeviceVisibility();
    idle_source = 0;
    return FALSE;
}

void MainWindow::setConnectionState(gboolean connected) {
    if (m_connected != connected) {
        m_connected = connected;
        if (m_connected) {
            connectingLabel->hide();
            notebook->show();
        } else {
            notebook->hide();
            connectingLabel->show();
        }
    }
}

void MainWindow::updateDeviceVisibility() {

    if (idle_source)
        return;

    idle_source = g_idle_add(idle_cb, this);
}

void MainWindow::reallyUpdateDeviceVisibility() {
    bool is_empty = true;

    for (std::map<uint32_t, SinkInputWidget*>::iterator i = sinkInputWidgets.begin(); i != sinkInputWidgets.end(); ++i) {
        SinkInputWidget* w = i->second;

        w->updating = true;
        w->updateDeviceComboBox();

        if (sinkWidgets.size() > 1) {
            w->directionLabel->show();
            w->deviceComboBox->show();
        } else {
            w->directionLabel->hide();
            w->deviceComboBox->hide();
        }

        if (showSinkInputType == SINK_INPUT_ALL || w->type == showSinkInputType) {
            w->show();
            is_empty = false;
        } else
            w->hide();

        w->updating = false;
    }

    if (eventRoleWidget)
        is_empty = false;

    if (is_empty)
        noStreamsLabel->show();
    else
        noStreamsLabel->hide();

    is_empty = true;

    for (std::map<uint32_t, SourceOutputWidget*>::iterator i = sourceOutputWidgets.begin(); i != sourceOutputWidgets.end(); ++i) {
        SourceOutputWidget* w = i->second;

        w->updating = true;
        w->updateDeviceComboBox();

        if (sourceWidgets.size() > 1) {
            w->directionLabel->show();
            w->deviceComboBox->show();
        } else {
            w->directionLabel->hide();
            w->deviceComboBox->hide();
        }

        if (showSourceOutputType == SOURCE_OUTPUT_ALL || w->type == showSourceOutputType) {
            w->show();
            is_empty = false;
        } else
            w->hide();

        w->updating = false;
    }

    if (is_empty)
        noRecsLabel->show();
    else
        noRecsLabel->hide();

    is_empty = true;

    for (std::map<uint32_t, SinkWidget*>::iterator i = sinkWidgets.begin(); i != sinkWidgets.end(); ++i) {
        SinkWidget* w = i->second;

        if (showSinkType == SINK_ALL || w->type == showSinkType) {
            w->show();
            is_empty = false;
        } else
            w->hide();
    }

    if (is_empty)
        noSinksLabel->show();
    else
        noSinksLabel->hide();

    is_empty = true;

    for (std::map<uint32_t, CardWidget*>::iterator i = cardWidgets.begin(); i != cardWidgets.end(); ++i) {
        CardWidget* w = i->second;

        w->show();
        is_empty = false;
    }

    if (is_empty)
        noCardsLabel->show();
    else
        noCardsLabel->hide();

    is_empty = true;

    for (std::map<uint32_t, SourceWidget*>::iterator i = sourceWidgets.begin(); i != sourceWidgets.end(); ++i) {
        SourceWidget* w = i->second;

        if (showSourceType == SOURCE_ALL ||
            w->type == showSourceType ||
            (showSourceType == SOURCE_NO_MONITOR && w->type != SOURCE_MONITOR)) {
            w->show();
            is_empty = false;
        } else
            w->hide();
    }

    if (is_empty)
        noSourcesLabel->show();
    else
        noSourcesLabel->hide();

    /* Hmm, if I don't call hide()/show() here some widgets will never
     * get their proper space allocated */
    sinksVBox->hide();
    sinksVBox->show();
    sourcesVBox->hide();
    sourcesVBox->show();
    streamsVBox->hide();
    streamsVBox->show();
    recsVBox->hide();
    recsVBox->show();
    cardsVBox->hide();
    cardsVBox->show();
}

void MainWindow::removeCard(uint32_t index) {
    if (!cardWidgets.count(index))
        return;

    cardsVBox->remove(*cardWidgets[index]);
    cardWidgets.erase(index);
    updateDeviceVisibility();
}

void MainWindow::removeSink(uint32_t index) {
    if (!sinkWidgets.count(index))
        return;

    sinksVBox->remove(*sinkWidgets[index]);
    sinkWidgets.erase(index);
    updateDeviceVisibility();
}

void MainWindow::removeSource(uint32_t index) {
    if (!sourceWidgets.count(index))
        return;

    sourcesVBox->remove(*sourceWidgets[index]);
    sourceWidgets.erase(index);
    updateDeviceVisibility();
}

void MainWindow::removeSinkInput(uint32_t index) {
    if (!sinkInputWidgets.count(index))
        return;

    streamsVBox->remove(*sinkInputWidgets[index]);
    sinkInputWidgets.erase(index);
    updateDeviceVisibility();
}

void MainWindow::removeSourceOutput(uint32_t index) {
    if (!sourceOutputWidgets.count(index))
        return;

    recsVBox->remove(*sourceOutputWidgets[index]);
    sourceOutputWidgets.erase(index);
    updateDeviceVisibility();
}

void MainWindow::removeClient(uint32_t index) {
    g_free(clientNames[index]);
    clientNames.erase(index);
}

void MainWindow::removeAllWidgets() {
    while (!sinkInputWidgets.empty())
        removeSinkInput(sinkInputWidgets.begin()->first);
    while (!sourceOutputWidgets.empty())
        removeSourceOutput(sourceOutputWidgets.begin()->first);
    while (!sinkWidgets.empty())
        removeSink(sinkWidgets.begin()->first);
    while (!sourceWidgets.empty())
        removeSource(sourceWidgets.begin()->first);
    while (!cardWidgets.empty())
        removeCard(cardWidgets.begin()->first);
    while (!clientNames.empty())
        removeClient(clientNames.begin()->first);
    deleteEventRoleWidget();
}

void MainWindow::setConnectingMessage(const char *string) {
    Glib::ustring markup = "<i>";
    if (!string)
        markup += _("Establishing connection to PulseAudio. Please wait...");
    else
        markup += string;
    markup += "</i>";
    connectingLabel->set_markup(markup);
}

void MainWindow::onSinkTypeComboBoxChanged() {
    showSinkType = (SinkType) sinkTypeComboBox->get_active_row_number();

    if (showSinkType == (SinkType) -1)
        sinkTypeComboBox->set_active((int) SINK_ALL);

    updateDeviceVisibility();
}

void MainWindow::onSourceTypeComboBoxChanged() {
    showSourceType = (SourceType) sourceTypeComboBox->get_active_row_number();

    if (showSourceType == (SourceType) -1)
        sourceTypeComboBox->set_active((int) SOURCE_NO_MONITOR);

    updateDeviceVisibility();
}

void MainWindow::onSinkInputTypeComboBoxChanged() {
    showSinkInputType = (SinkInputType) sinkInputTypeComboBox->get_active_row_number();

    if (showSinkInputType == (SinkInputType) -1)
        sinkInputTypeComboBox->set_active((int) SINK_INPUT_CLIENT);

    updateDeviceVisibility();
}

void MainWindow::onSourceOutputTypeComboBoxChanged() {
    showSourceOutputType = (SourceOutputType) sourceOutputTypeComboBox->get_active_row_number();

    if (showSourceOutputType == (SourceOutputType) -1)
        sourceOutputTypeComboBox->set_active((int) SOURCE_OUTPUT_CLIENT);

    updateDeviceVisibility();
}


void MainWindow::onShowVolumeMetersCheckButtonToggled() {
    bool state = showVolumeMetersCheckButton->get_active();
    pa_operation *o;

    for (std::map<uint32_t, SinkWidget*>::iterator it = sinkWidgets.begin() ; it != sinkWidgets.end(); it++) {
        SinkWidget *sw = it->second;
        if (sw->peak) {
            o = pa_stream_cork(sw->peak, (int)!state, NULL, NULL);
            if (o)
                pa_operation_unref(o);
        }
        sw->setVolumeMeterVisible(state);
    }
    for (std::map<uint32_t, SourceWidget*>::iterator it = sourceWidgets.begin() ; it != sourceWidgets.end(); it++) {
        SourceWidget *sw = it->second;
        if (sw->peak) {
            o = pa_stream_cork(sw->peak, (int)!state, NULL, NULL);
            if (o)
                pa_operation_unref(o);
        }
        sw->setVolumeMeterVisible(state);
    }
    for (std::map<uint32_t, SinkInputWidget*>::iterator it = sinkInputWidgets.begin() ; it != sinkInputWidgets.end(); it++) {
        SinkInputWidget *sw = it->second;
        if (sw->peak) {
            o = pa_stream_cork(sw->peak, (int)!state, NULL, NULL);
            if (o)
                pa_operation_unref(o);
        }
        sw->setVolumeMeterVisible(state);
    }
    for (std::map<uint32_t, SourceOutputWidget*>::iterator it = sourceOutputWidgets.begin() ; it != sourceOutputWidgets.end(); it++) {
        SourceOutputWidget *sw = it->second;
        if (sw->peak) {
            o = pa_stream_cork(sw->peak, (int)!state, NULL, NULL);
            if (o)
                pa_operation_unref(o);
        }
        sw->setVolumeMeterVisible(state);
    }
}


void MainWindow::onHideUnavailableCardProfilesCheckButtonToggled() {
    bool state = hideUnavailableCardProfilesCheckButton->get_active();

    for (std::map<uint32_t, CardWidget*>::iterator it = cardWidgets.begin() ; it != cardWidgets.end(); it++) {
        CardWidget *cw = it->second;
        cw->hideUnavailableProfiles = state;
        cw->prepareMenu();
    }
}

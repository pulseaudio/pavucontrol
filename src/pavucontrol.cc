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

#include <pulse/pulseaudio.h>
#include <pulse/ext-stream-restore.h>
#include <pulse/ext-device-manager.h>
#ifdef HAVE_PULSE_MESSAGING_API
#include <json-glib/json-glib.h>
#endif

#include <canberra-gtk.h>

#include "pavucontrol.h"
#include "i18n.h"
#include "minimalstreamwidget.h"
#include "channelwidget.h"
#include "streamwidget.h"
#include "cardwidget.h"
#include "sinkwidget.h"
#include "sourcewidget.h"
#include "sinkinputwidget.h"
#include "sourceoutputwidget.h"
#include "rolewidget.h"
#include "mainwindow.h"
#include "pavuapplication.h"

#include <unordered_map>
#include <utility>
#include <memory>

using WindowAndCardName = std::pair<MainWindow*, std::string>;

static pa_context* context = NULL;
static pa_mainloop_api* api = NULL;
static int n_outstanding = 0;
static int tab_number = 0;
static bool retry = false;
static int reconnect_timeout = 1;

void show_error(const char *txt) {
    char buf[256];

    snprintf(buf, sizeof(buf), "%s: %s", txt, pa_strerror(pa_context_errno(context)));

    Gtk::MessageDialog dialog(buf, false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_CLOSE, true);
    dialog.run();

    PavuApplication::get_instance().quit();
}

static void dec_outstanding(MainWindow *w) {
    if (n_outstanding <= 0)
        return;

    if (--n_outstanding <= 0) {
        w->get_window()->set_cursor();
        w->setConnectionState(true);
    }
}

#ifdef HAVE_PULSE_MESSAGING_API

std::string card_message_handler_path(const std::string& name) {
    return "/card/" + name;
}

std::string card_bluez_message_handler_path(const std::string& name) {
    return "/card/" + name + "/bluez";
}

static void context_bluetooth_card_codec_list_cb(pa_context *c, int success, char *response, void *userdata) {
    auto u = std::unique_ptr<WindowAndCardName>(reinterpret_cast<WindowAndCardName*>(userdata));

    if (!success)
        return;

    int err = 0;

    GError *gerror = NULL;
    JsonParser *parser = json_parser_new();

    if (!json_parser_load_from_data(parser, response, strlen(response), &gerror)) {
        g_debug(_("could not read JSON from list-codecs message response: %s"), gerror->message);
        g_error_free(gerror);
        g_object_unref(parser);
        return;
    }

    JsonNode *root = json_parser_get_root(parser);

    if (!root || JSON_NODE_TYPE(root) != JSON_NODE_ARRAY) {
        g_debug(_("list-codecs message response is not a JSON array"));
        g_object_unref(parser);
        return;
    }

    JsonArray *array = json_node_get_array(root);

    std::unordered_map<std::string, std::string> codecs;

    for (guint i = 0; i < json_array_get_length(array); ++i) {
        const char *path;
        const char *description;
        JsonNode *v;

        JsonObject *object = json_array_get_object_element(array, i);
        if (!object) {
            err = -1;
            break;
        }

        v = json_object_get_member(object, "name");
        if (!v) {
            err = -1;
            break;
        }

        path = json_node_get_string(v);
        if (!path) {
            err = -1;
            break;
        }

        v = json_object_get_member(object, "description");
        if (!v) {
            err = -1;
            break;
        }

        description = json_node_get_string(v);
        if (!description)
            description = "";

        codecs[path] = description;
    }

    g_object_unref(parser);

    if (err < 0) {
        g_debug(_("list-codecs message response could not be parsed correctly"));
        codecs.clear();
        return;
    }

    u->first->updateCardCodecs(u->second, codecs);
}

static void context_bluetooth_card_active_codec_cb(pa_context *c, int success, char *response, void *userdata) {
    auto u = std::unique_ptr<WindowAndCardName>(reinterpret_cast<WindowAndCardName*>(userdata));

    if (!success)
        return;

    const char *name;
    GError *gerror = NULL;

    JsonParser *parser = json_parser_new();

    if (!json_parser_load_from_data(parser, response, strlen(response), &gerror)) {
        g_debug(_("could not read JSON from get-codec message response: %s"), gerror->message);
        g_error_free(gerror);
        g_object_unref(parser);
        return;
    }

    JsonNode *root = json_parser_get_root(parser);

    if (!root || JSON_NODE_TYPE(root) != JSON_NODE_VALUE) {
        g_debug(_("get-codec message response is not a JSON value"));
        g_object_unref(parser);
        return;
    }

    name = json_node_get_string(root);

    if (!name) {
        g_debug(_("could not get codec name from get-codec message response"));
        g_object_unref(parser);
        return;
    }

    u->first->setActiveCodec(u->second, name);

    g_object_unref(parser);
}

static void context_card_profile_is_sticky_cb(pa_context *c, int success, char *response, void *userdata) {
    auto u = std::unique_ptr<WindowAndCardName>(reinterpret_cast<WindowAndCardName*>(userdata));

    if (!success)
        return;

    gboolean profile_is_sticky;
    GError *gerror = NULL;

    JsonParser *parser = json_parser_new();

    if (!json_parser_load_from_data(parser, response, strlen(response), &gerror)) {
        g_debug(_("could not read JSON from get-profile-sticky message response: %s"), gerror->message);
        g_error_free(gerror);
        g_object_unref(parser);
        return;
    }

    JsonNode *root = json_parser_get_root(parser);

    if (!root || JSON_NODE_TYPE(root) != JSON_NODE_VALUE) {
        g_debug(_("get-profile-sticky message response is not a JSON value"));
        g_object_unref(parser);
        return;
    }

    profile_is_sticky = json_node_get_boolean(root);

    u->first->setCardProfileIsSticky(u->second, profile_is_sticky);

    g_object_unref(parser);
}

template<typename U> void send_message(pa_context *c, const char *target, const char *request, pa_context_string_cb_t cb, const U& u)
{
    auto send_message_userdata = new U(u);

    pa_operation *o = pa_context_send_message_to_object(c, target, request, NULL, cb, send_message_userdata);

    if (!o) {
        delete send_message_userdata;
        g_debug(_("pa_context_send_message_to_object() failed: %s"), pa_strerror(pa_context_errno(context)));
        return;
    }
    pa_operation_unref(o);
}

static void context_message_handlers_cb(pa_context *c, int success, char *response, void *userdata) {
    auto u = std::unique_ptr<WindowAndCardName>(reinterpret_cast<WindowAndCardName*>(userdata));

    if (!success)
        return;

    int err = 0;

    GError *gerror = NULL;
    JsonParser *parser = json_parser_new();

    if (!json_parser_load_from_data(parser, response, strlen(response), &gerror)) {
        g_debug(_("could not read JSON from list-handlers message response: %s"), gerror->message);
        g_error_free(gerror);
        g_object_unref(parser);
        return;
    }

    JsonNode *root = json_parser_get_root(parser);

    if (!root || JSON_NODE_TYPE(root) != JSON_NODE_ARRAY) {
        g_debug(_("list-handlers message response is not a JSON array"));
        g_object_unref(parser);
        return;
    }

    JsonArray *array = json_node_get_array(root);

    std::unordered_map<std::string, std::string> message_handler_map;

    for (guint i = 0; i < json_array_get_length(array); ++i) {
        const char *path;
        const char *description;
        JsonNode *v;

        JsonObject *object = json_array_get_object_element(array, i);
        if (!object) {
            err = -1;
            break;
        }

        v = json_object_get_member(object, "name");
        if (!v) {
            err = -1;
            break;
        }

        path = json_node_get_string(v);
        if (!path) {
            err = -1;
            break;
        }

        v = json_object_get_member(object, "description");
        if (!v) {
            err = -1;
            break;
        }

        description = json_node_get_string(v);
        if (!description)
            description = "";

        message_handler_map[path] = description;
    }

    g_object_unref(parser);

    if (err < 0) {
        g_debug(_("list-handlers message response could not be parsed correctly"));
        message_handler_map.clear();
        return;
    }

    /* only send requests if card bluez message handler is registered */
    auto e = message_handler_map.find(card_bluez_message_handler_path(u->second));

    if (e != message_handler_map.end()) {

        /* get-codec: retrieve active codec name */
        send_message(c, e->first.c_str(), "get-codec", context_bluetooth_card_active_codec_cb, *u);

        /* list-codecs: retrieve list of codecs */
        send_message(c, e->first.c_str(), "list-codecs", context_bluetooth_card_codec_list_cb, *u);
    }

    /* send requests to card if card message handler is registered */
    e = message_handler_map.find(card_message_handler_path(u->second));

    if (e != message_handler_map.end()) {
        /* get-profile-sticky: retrieve active codec name */
        send_message(c, e->first.c_str(), "get-profile-sticky", context_card_profile_is_sticky_cb, *u);
    }
}
#endif

void card_cb(pa_context *c, const pa_card_info *i, int eol, void *userdata) {
    MainWindow *w = static_cast<MainWindow*>(userdata);

    if (eol < 0) {
        if (pa_context_errno(context) == PA_ERR_NOENTITY)
            return;

        show_error(_("Card callback failure"));
        return;
    }

    if (eol > 0) {
        dec_outstanding(w);
        return;
    }

    w->updateCard(*i);

#ifdef HAVE_PULSE_MESSAGING_API
    /* initiate requesting bluetooth codec list */
    send_message(c, "/core", "list-handlers", context_message_handlers_cb, WindowAndCardName(w, i->name));
#endif
}

#if HAVE_EXT_DEVICE_RESTORE_API
static void ext_device_restore_subscribe_cb(pa_context *c, pa_device_type_t type, uint32_t idx, void *userdata);
#endif

void sink_cb(pa_context *c, const pa_sink_info *i, int eol, void *userdata) {
    MainWindow *w = static_cast<MainWindow*>(userdata);

    if (eol < 0) {
        if (pa_context_errno(context) == PA_ERR_NOENTITY)
            return;

        show_error(_("Sink callback failure"));
        return;
    }

    if (eol > 0) {
        dec_outstanding(w);
        return;
    }

#if HAVE_EXT_DEVICE_RESTORE_API
    if (w->updateSink(*i))
        ext_device_restore_subscribe_cb(c, PA_DEVICE_TYPE_SINK, i->index, w);
#else
    w->updateSink(*i);
#endif
}

void source_cb(pa_context *, const pa_source_info *i, int eol, void *userdata) {
    MainWindow *w = static_cast<MainWindow*>(userdata);

    if (eol < 0) {
        if (pa_context_errno(context) == PA_ERR_NOENTITY)
            return;

        show_error(_("Source callback failure"));
        return;
    }

    if (eol > 0) {
        dec_outstanding(w);
        return;
    }

    w->updateSource(*i);
}

void sink_input_cb(pa_context *, const pa_sink_input_info *i, int eol, void *userdata) {
    MainWindow *w = static_cast<MainWindow*>(userdata);

    if (eol < 0) {
        if (pa_context_errno(context) == PA_ERR_NOENTITY)
            return;

        show_error(_("Sink input callback failure"));
        return;
    }

    if (eol > 0) {
        dec_outstanding(w);
        return;
    }

    w->updateSinkInput(*i);
}

void source_output_cb(pa_context *, const pa_source_output_info *i, int eol, void *userdata) {
    MainWindow *w = static_cast<MainWindow*>(userdata);

    if (eol < 0) {
        if (pa_context_errno(context) == PA_ERR_NOENTITY)
            return;

        show_error(_("Source output callback failure"));
        return;
    }

    if (eol > 0)  {

        if (n_outstanding > 0) {
            /* At this point all notebook pages have been populated, so
             * let's open one that isn't empty */
            if (tab_number != 0) {
                w->selectTab(tab_number);
            } else {
                w->selectBestTab();
            }
        }

        dec_outstanding(w);
        return;
    }

    w->updateSourceOutput(*i);
}

void client_cb(pa_context *, const pa_client_info *i, int eol, void *userdata) {
    MainWindow *w = static_cast<MainWindow*>(userdata);

    if (eol < 0) {
        if (pa_context_errno(context) == PA_ERR_NOENTITY)
            return;

        show_error(_("Client callback failure"));
        return;
    }

    if (eol > 0) {
        dec_outstanding(w);
        return;
    }

    w->updateClient(*i);
}

void server_info_cb(pa_context *, const pa_server_info *i, void *userdata) {
    MainWindow *w = static_cast<MainWindow*>(userdata);

    if (!i) {
        show_error(_("Server info callback failure"));
        return;
    }

    w->updateServer(*i);
    dec_outstanding(w);
}

void ext_stream_restore_read_cb(
        pa_context *,
        const pa_ext_stream_restore_info *i,
        int eol,
        void *userdata) {

    MainWindow *w = static_cast<MainWindow*>(userdata);

    if (eol < 0) {
        dec_outstanding(w);
        g_debug(_("Failed to initialize stream_restore extension: %s"), pa_strerror(pa_context_errno(context)));
        w->deleteEventRoleWidget();
        return;
    }

    if (eol > 0) {
        dec_outstanding(w);
        return;
    }

    w->updateRole(*i);
}

static void ext_stream_restore_subscribe_cb(pa_context *c, void *userdata) {
    MainWindow *w = static_cast<MainWindow*>(userdata);
    pa_operation *o;

    if (!(o = pa_ext_stream_restore_read(c, ext_stream_restore_read_cb, w))) {
        show_error(_("pa_ext_stream_restore_read() failed"));
        return;
    }

    pa_operation_unref(o);
}

#if HAVE_EXT_DEVICE_RESTORE_API
void ext_device_restore_read_cb(
        pa_context *,
        const pa_ext_device_restore_info *i,
        int eol,
        void *userdata) {

    MainWindow *w = static_cast<MainWindow*>(userdata);

    if (eol < 0) {
        dec_outstanding(w);
        g_debug(_("Failed to initialize device restore extension: %s"), pa_strerror(pa_context_errno(context)));
        return;
    }

    if (eol > 0) {
        dec_outstanding(w);
        return;
    }

    /* Do something with a widget when this part is written */
    w->updateDeviceInfo(*i);
}

static void ext_device_restore_subscribe_cb(pa_context *c, pa_device_type_t type, uint32_t idx, void *userdata) {
    MainWindow *w = static_cast<MainWindow*>(userdata);
    pa_operation *o;

    if (type != PA_DEVICE_TYPE_SINK)
        return;

    if (!(o = pa_ext_device_restore_read_formats(c, type, idx, ext_device_restore_read_cb, w))) {
        show_error(_("pa_ext_device_restore_read_sink_formats() failed"));
        return;
    }

    pa_operation_unref(o);
}
#endif

void ext_device_manager_read_cb(
        pa_context *,
        const pa_ext_device_manager_info *,
        int eol,
        void *userdata) {

    MainWindow *w = static_cast<MainWindow*>(userdata);

    if (eol < 0) {
        dec_outstanding(w);
        g_debug(_("Failed to initialize device manager extension: %s"), pa_strerror(pa_context_errno(context)));
        return;
    }

    w->canRenameDevices = true;

    if (eol > 0) {
        dec_outstanding(w);
        return;
    }

    /* Do something with a widget when this part is written */
}

static void ext_device_manager_subscribe_cb(pa_context *c, void *userdata) {
    MainWindow *w = static_cast<MainWindow*>(userdata);
    pa_operation *o;

    if (!(o = pa_ext_device_manager_read(c, ext_device_manager_read_cb, w))) {
        show_error(_("pa_ext_device_manager_read() failed"));
        return;
    }

    pa_operation_unref(o);
}

void subscribe_cb(pa_context *c, pa_subscription_event_type_t t, uint32_t index, void *userdata) {
    MainWindow *w = static_cast<MainWindow*>(userdata);

    switch (t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) {
        case PA_SUBSCRIPTION_EVENT_SINK:
            if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_REMOVE)
                w->removeSink(index);
            else {
                pa_operation *o;
                if (!(o = pa_context_get_sink_info_by_index(c, index, sink_cb, w))) {
                    show_error(_("pa_context_get_sink_info_by_index() failed"));
                    return;
                }
                pa_operation_unref(o);
            }
            break;

        case PA_SUBSCRIPTION_EVENT_SOURCE:
            if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_REMOVE)
                w->removeSource(index);
            else {
                pa_operation *o;
                if (!(o = pa_context_get_source_info_by_index(c, index, source_cb, w))) {
                    show_error(_("pa_context_get_source_info_by_index() failed"));
                    return;
                }
                pa_operation_unref(o);
            }
            break;

        case PA_SUBSCRIPTION_EVENT_SINK_INPUT:
            if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_REMOVE)
                w->removeSinkInput(index);
            else {
                pa_operation *o;
                if (!(o = pa_context_get_sink_input_info(c, index, sink_input_cb, w))) {
                    show_error(_("pa_context_get_sink_input_info() failed"));
                    return;
                }
                pa_operation_unref(o);
            }
            break;

        case PA_SUBSCRIPTION_EVENT_SOURCE_OUTPUT:
            if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_REMOVE)
                w->removeSourceOutput(index);
            else {
                pa_operation *o;
                if (!(o = pa_context_get_source_output_info(c, index, source_output_cb, w))) {
                    show_error(_("pa_context_get_sink_input_info() failed"));
                    return;
                }
                pa_operation_unref(o);
            }
            break;

        case PA_SUBSCRIPTION_EVENT_CLIENT:
            if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_REMOVE)
                w->removeClient(index);
            else {
                pa_operation *o;
                if (!(o = pa_context_get_client_info(c, index, client_cb, w))) {
                    show_error(_("pa_context_get_client_info() failed"));
                    return;
                }
                pa_operation_unref(o);
            }
            break;

        case PA_SUBSCRIPTION_EVENT_SERVER: {
                pa_operation *o;
                if (!(o = pa_context_get_server_info(c, server_info_cb, w))) {
                    show_error(_("pa_context_get_server_info() failed"));
                    return;
                }
                pa_operation_unref(o);
            }
            break;

        case PA_SUBSCRIPTION_EVENT_CARD:
            if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_REMOVE)
                w->removeCard(index);
            else {
                pa_operation *o;
                if (!(o = pa_context_get_card_info_by_index(c, index, card_cb, w))) {
                    show_error(_("pa_context_get_card_info_by_index() failed"));
                    return;
                }
                pa_operation_unref(o);
            }
            break;

    }
}

/* Forward Declaration */
gboolean connect_to_pulse(gpointer userdata);

void context_state_callback(pa_context *c, void *userdata) {
    MainWindow *w = static_cast<MainWindow*>(userdata);

    g_assert(c);

    switch (pa_context_get_state(c)) {
        case PA_CONTEXT_UNCONNECTED:
        case PA_CONTEXT_CONNECTING:
        case PA_CONTEXT_AUTHORIZING:
        case PA_CONTEXT_SETTING_NAME:
            break;

        case PA_CONTEXT_READY: {
            pa_operation *o;

            reconnect_timeout = 1;

            /* Create event widget immediately so it's first in the list */
            w->createEventRoleWidget();

            pa_context_set_subscribe_callback(c, subscribe_cb, w);

            if (!(o = pa_context_subscribe(c, (pa_subscription_mask_t)
                                           (PA_SUBSCRIPTION_MASK_SINK|
                                            PA_SUBSCRIPTION_MASK_SOURCE|
                                            PA_SUBSCRIPTION_MASK_SINK_INPUT|
                                            PA_SUBSCRIPTION_MASK_SOURCE_OUTPUT|
                                            PA_SUBSCRIPTION_MASK_CLIENT|
                                            PA_SUBSCRIPTION_MASK_SERVER|
                                            PA_SUBSCRIPTION_MASK_CARD), NULL, NULL))) {
                show_error(_("pa_context_subscribe() failed"));
                return;
            }
            pa_operation_unref(o);

            /* Keep track of the outstanding callbacks for UI tweaks */
            n_outstanding = 0;

            if (!(o = pa_context_get_server_info(c, server_info_cb, w))) {
                show_error(_("pa_context_get_server_info() failed"));
                return;
            }
            pa_operation_unref(o);
            n_outstanding++;

            if (!(o = pa_context_get_client_info_list(c, client_cb, w))) {
                show_error(_("pa_context_client_info_list() failed"));
                return;
            }
            pa_operation_unref(o);
            n_outstanding++;

            if (!(o = pa_context_get_card_info_list(c, card_cb, w))) {
                show_error(_("pa_context_get_card_info_list() failed"));
                return;
            }
            pa_operation_unref(o);
            n_outstanding++;

            if (!(o = pa_context_get_sink_info_list(c, sink_cb, w))) {
                show_error(_("pa_context_get_sink_info_list() failed"));
                return;
            }
            pa_operation_unref(o);
            n_outstanding++;

            if (!(o = pa_context_get_source_info_list(c, source_cb, w))) {
                show_error(_("pa_context_get_source_info_list() failed"));
                return;
            }
            pa_operation_unref(o);
            n_outstanding++;

            if (!(o = pa_context_get_sink_input_info_list(c, sink_input_cb, w))) {
                show_error(_("pa_context_get_sink_input_info_list() failed"));
                return;
            }
            pa_operation_unref(o);
            n_outstanding++;

            if (!(o = pa_context_get_source_output_info_list(c, source_output_cb, w))) {
                show_error(_("pa_context_get_source_output_info_list() failed"));
                return;
            }
            pa_operation_unref(o);
            n_outstanding++;

            /* These calls are not always supported */
            if ((o = pa_ext_stream_restore_read(c, ext_stream_restore_read_cb, w))) {
                pa_operation_unref(o);
                n_outstanding++;

                pa_ext_stream_restore_set_subscribe_cb(c, ext_stream_restore_subscribe_cb, w);

                if ((o = pa_ext_stream_restore_subscribe(c, 1, NULL, NULL)))
                    pa_operation_unref(o);

            } else
                g_debug(_("Failed to initialize stream_restore extension: %s"), pa_strerror(pa_context_errno(context)));

#if HAVE_EXT_DEVICE_RESTORE_API
            /* TODO Change this to just the test function */
            if ((o = pa_ext_device_restore_read_formats_all(c, ext_device_restore_read_cb, w))) {
                pa_operation_unref(o);
                n_outstanding++;

                pa_ext_device_restore_set_subscribe_cb(c, ext_device_restore_subscribe_cb, w);

                if ((o = pa_ext_device_restore_subscribe(c, 1, NULL, NULL)))
                    pa_operation_unref(o);

            } else
                g_debug(_("Failed to initialize device restore extension: %s"), pa_strerror(pa_context_errno(context)));
#endif

            if ((o = pa_ext_device_manager_read(c, ext_device_manager_read_cb, w))) {
                pa_operation_unref(o);
                n_outstanding++;

                pa_ext_device_manager_set_subscribe_cb(c, ext_device_manager_subscribe_cb, w);

                if ((o = pa_ext_device_manager_subscribe(c, 1, NULL, NULL)))
                    pa_operation_unref(o);

            } else
                g_debug(_("Failed to initialize device manager extension: %s"), pa_strerror(pa_context_errno(context)));


            break;
        }

        case PA_CONTEXT_FAILED:
            w->setConnectionState(false);

            w->removeAllWidgets();
            w->updateDeviceVisibility();
            pa_context_unref(context);
            context = NULL;

            if (reconnect_timeout > 0) {
                g_debug(_("Connection failed, attempting reconnect"));
                g_timeout_add_seconds(reconnect_timeout, connect_to_pulse, w);
            }
            return;

        case PA_CONTEXT_TERMINATED:
        default:
            w->get_application()->quit();
            return;
    }
}

pa_context* get_context(void) {
  return context;
}

gboolean connect_to_pulse(gpointer userdata) {
    MainWindow *w = static_cast<MainWindow*>(userdata);

    if (context)
        return false;

    pa_proplist *proplist = pa_proplist_new();
    pa_proplist_sets(proplist, PA_PROP_APPLICATION_NAME, _("PulseAudio Volume Control"));
    pa_proplist_sets(proplist, PA_PROP_APPLICATION_ID, "org.PulseAudio.pavucontrol");
    pa_proplist_sets(proplist, PA_PROP_APPLICATION_ICON_NAME, "audio-card");
    pa_proplist_sets(proplist, PA_PROP_APPLICATION_VERSION, PACKAGE_VERSION);

    context = pa_context_new_with_proplist(api, NULL, proplist);
    g_assert(context);

    pa_proplist_free(proplist);

    pa_context_set_state_callback(context, context_state_callback, w);

    w->setConnectingMessage();
    if (pa_context_connect(context, NULL, PA_CONTEXT_NOFAIL, NULL) < 0) {
        if (pa_context_errno(context) == PA_ERR_INVALID) {
            w->setConnectingMessage(_("Connection to PulseAudio failed. Automatic retry in 5s\n\n"
                "In this case this is likely because PULSE_SERVER in the Environment/X11 Root Window Properties\n"
                "or default-server in client.conf is misconfigured.\n"
                "This situation can also arise when PulseAudio crashed and left stale details in the X11 Root Window.\n"
                "If this is the case, then PulseAudio should autospawn again, or if this is not configured you should\n"
                "run start-pulseaudio-x11 manually."));
            reconnect_timeout = 5;
        }
        else {
            if(!retry) {
                reconnect_timeout = -1;
                w->get_application()->quit();
            } else {
                g_debug(_("Connection failed, attempting reconnect"));
                reconnect_timeout = 5;
                g_timeout_add_seconds(reconnect_timeout, connect_to_pulse, w);
            }
        }
    }

    return false;
}

MainWindow* pavucontrol_get_window(pa_glib_mainloop *m, bool maximize, bool _retry, int _tab_number) {

    MainWindow* mainWindow = NULL;

    tab_number = _tab_number;
    retry = _retry;

    ca_context_set_driver(ca_gtk_context_get(), "pulse");

    mainWindow = MainWindow::create(maximize);

    api = pa_glib_mainloop_get_api(m);
    g_assert(api);

    connect_to_pulse(mainWindow);

    return mainWindow;
}

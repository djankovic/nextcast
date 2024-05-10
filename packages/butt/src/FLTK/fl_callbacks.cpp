// FLTK callback functions for butt
//
// Copyright 2007-2018 by Daniel Noethen.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or (at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>

#ifdef WIN32
#define usleep(us) Sleep(us / 1000)
#include "tray_agent.h"
#else
#include <sys/wait.h>
#endif

#include <FL/fl_ask.H>
#include <FL/Fl_Color_Chooser.H>
#include <FL/Fl_Browser.H>

#include <samplerate.h>

#include "gettext.h"
#include "config.h"

#include "FL/Fl_My_Native_File_Chooser.H"
// #include <FL/Fl_Native_File_Chooser.H>
// #define Fl_My_Native_File_Chooser Fl_Native_File_Chooser

#include "cfg.h"
#include "butt.h"
#include "port_audio.h"
#include "port_midi.h"
#include "timer.h"
#include "shoutcast.h"
#include "icecast.h"
#include "lame_encode.h"
#include "opus_encode.h"
#include "fl_callbacks.h"
#include "strfuncs.h"
#include "flgui.h"
#include "util.h"
#include "fl_timer_funcs.h"
#include "fl_funcs.h"
#include "update.h"
#include "command.h"
#include "url.h"
#ifdef WITH_RADIOCO
#include "radioco.h"
#include "oauth.h"
#endif

flgui *fl_g;
int display_info = STREAM_TIME;

pthread_t pt_connect_detached;

int ask_user = 0;
void *connect_thread(void *data)
{
    int ret;
    int (*xc_connect)() = NULL;

    if (cfg.srv[cfg.selected_srv]->type == ICECAST) {
        xc_connect = &ic_connect;
    }
    else { //(cfg.srv[cfg.selected_srv]->type == SHOUTCAST)
        xc_connect = &sc_connect;
    }

    // try to connect as long as xc_connect returns non-zero and try_to_connect == 1
    while (((ret = xc_connect()) != IC_OK) && (try_to_connect == 1)) // xc_connect returns 0 when connected
    {
        // Stop connecting in case of a fatal error
        if (ret == IC_ABORT) {
            try_to_connect = 0;
            break;
        }
        if (ret == IC_ASK) {
            ask_user = 1;
            while (ask_user_get_has_clicked() != 1) {
                usleep(100 * 1000); // 100 ms
            }

            if (ask_user_get_answer() == IC_ABORT) {
                try_to_connect = 0;
                ask_user_reset();
                break;
            }
            ask_user_reset();
        }

        usleep(100 * 1000); // 100ms
    }

    // Reset display
    if (recording == 0 && try_to_connect == 0) {
        // Make sure this code runs on main thread (async)
        Fl::awake([](void *) -> void {
            fl_g->lcd->clear();
            fl_g->lcd->print((const uchar *)("idle"), strlen(_("idle")));
            fl_g->radio_co_logo->show();
            fl_g->radio_co_logo->redraw();
        });
    }
    else {
        try_to_connect = 0;
    }

    // Detach thread (free ressources) because no one will call pthread_join() on it
    pthread_detach(pthread_self());

    return NULL;
}

// Print "Connecting..." on the LCD as long as we are trying to connect
void print_connecting_timeout(void *)
{
    static int dummy = 0;

    if (try_to_connect == 1) {
        if (dummy == 0) {
            print_lcd(_("connecting"), strlen(_("connecting")), 0, 1);
            dummy++;
        }
        else if (dummy <= 3) {
            print_lcd(".", 1, 0, 0);
            dummy++;
        }
        else if (dummy > 3) {
            dummy = 0;
        }
        else {
            dummy++;
        }

        Fl::repeat_timeout(0.25, &print_connecting_timeout);
    }
    else {
        dummy = 0;
    }
}

void show_vu_tabs(void)
{
    fl_g->vu_tabs->show();
    fl_g->label_volume->hide();
}
void hide_vu_tabs(void)
{
    fl_g->vu_tabs->hide();
    fl_g->label_volume->show();
}

void button_connect_cb(void)
{
    char text_buf[256];

    if (connected) {
        return;
    }

    if (try_to_connect) {
        return;
    }

    if (cfg.main.num_of_srv < 1) {
        print_info(_("Error: No server entry found.\nPlease add a server in the settings-window."), 1);
        return;
    }

    if (!strcmp(cfg.audio.codec, "ogg") && (cfg.audio.bitrate < 48)) {
        print_info(_("Error: ogg vorbis encoder doesn't support bitrates\n"
                     "lower than 48kbit"),
                   1);
        return;
    }

    if (cfg.srv[cfg.selected_srv]->type == SHOUTCAST) {
        if ((!strcmp(cfg.audio.codec, "ogg")) || (!strcmp(cfg.audio.codec, "opus"))) {
            snprintf(text_buf, sizeof(text_buf), _("Warning: %s is not supported by every Shoutcast version"), cfg.audio.codec);
            print_info(text_buf, 1);
        }
        if (!strcmp(cfg.audio.codec, "flac")) {
            print_info(_("Error: FLAC is not supported by ShoutCast"), 1);
            return;
        }
    }

    if (cfg.srv[cfg.selected_srv]->type == RADIOCO) {
        if ((strcmp(cfg.audio.codec, "mp3")) && (strcmp(cfg.audio.codec, "aac"))) {
            snprintf(text_buf, sizeof(text_buf), _("Error: Radio.co supports only mp3 and aac"));
            print_info(text_buf, 1);
            return;
        }
    }

    if (cfg.srv[cfg.selected_srv]->type == ICECAST) {
        snprintf(text_buf, sizeof(text_buf), _("Connecting to %s:%u ..."), cfg.srv[cfg.selected_srv]->addr, cfg.srv[cfg.selected_srv]->port);
    }
    else {
        snprintf(text_buf, sizeof(text_buf), _("Connecting to %s:%u (%u) ..."), cfg.srv[cfg.selected_srv]->addr, cfg.srv[cfg.selected_srv]->port + 1,
                 cfg.srv[cfg.selected_srv]->port);
    }

    print_info(text_buf, 0);

    // Clear libsamplerate state
    snd_reset_samplerate_conv(SND_STREAM);

    try_to_connect = 1;
    if (pthread_create(&pt_connect_detached, NULL, connect_thread, NULL) != 0) {
        print_info("Fatal error: Could not launch connect thread. Please restart BUTT", 1);
        try_to_connect = 1;
        return;
    }
    Fl::add_timeout(0, &print_connecting_timeout);

    while (try_to_connect) {
        if (ask_user == 1) {
            ask_user_ask();
            ask_user = 0;
        }

        usleep(10000); // 10 ms
        Fl::wait(0);   // update gui and parse user events
    }

    if (!connected) {
        return;
    }

    int codec_samplerate = 0;
    // we have to make sure that the first audio data
    // the server sees are the ogg headers
    if (!strcmp(cfg.audio.codec, "ogg")) {
        vorbis_enc_write_header(&vorbis_stream);
        codec_samplerate = vorbis_enc_get_samplerate(&vorbis_stream);
    }

    if (!strcmp(cfg.audio.codec, "opus")) {
        opus_enc_write_header(&opus_stream);
        codec_samplerate = opus_enc_get_samplerate(&opus_stream);
    }

    // Reset the internal flac frame counter to zero to
    // make sure that the header is sent to the server upon next connect
    if (!strcmp(cfg.audio.codec, "flac")) {
        flac_enc_reinit(&flac_stream);
        codec_samplerate = flac_enc_get_samplerate(&flac_stream);
    }

    if (!strcmp(cfg.audio.codec, "mp3")) {
        codec_samplerate = lame_enc_get_samplerate(&lame_stream);
    }

#ifdef HAVE_LIBFDK_AAC
    if (!strcmp(cfg.audio.codec, "aac")) {
        codec_samplerate = aac_enc_get_samplerate(&aac_stream);
    }
#endif

    char bitrate_str[32];
    if (strcmp(cfg.audio.codec, "flac") == 0) {
        snprintf(bitrate_str, sizeof(bitrate_str), "-");
    }
    else {
        snprintf(bitrate_str, sizeof(bitrate_str), "%dkbps", cfg.audio.bitrate);
    }

    char server_type[32];
    switch (cfg.srv[cfg.selected_srv]->type) {
    case ICECAST:
        snprintf(server_type, sizeof(server_type), "Icecast");
        break;
    case SHOUTCAST:
        snprintf(server_type, sizeof(server_type), "Shoutcast");
        break;
    case RADIOCO:
        snprintf(server_type, sizeof(server_type), "Radio.co");
        break;
    default:
        snprintf(server_type, sizeof(server_type), "Unknown");
        break;
    }

    char samplerate_text[32];
    if (cfg.audio.samplerate == codec_samplerate) {
        snprintf(samplerate_text, sizeof(samplerate_text), "%dHz", cfg.audio.samplerate);
    }
    else {
        snprintf(samplerate_text, sizeof(samplerate_text), "%dHz -> %dHz", cfg.audio.samplerate, codec_samplerate);
    }

    print_info(_("Connection established"), 0);
    snprintf(text_buf, sizeof(text_buf),
             "Settings:\n"
             "Type:\t\t%s\n"
             "Codec:\t\t%s\n"
             "Bitrate:\t%s\n"
             "Samplerate:\t%s\n",
             server_type, cfg.audio.codec, bitrate_str, samplerate_text);

    if (cfg.srv[cfg.selected_srv]->type == ICECAST) {
        snprintf(text_buf + strlen(text_buf), sizeof(text_buf) - strlen(text_buf),
                 "Mountpoint:\t%s\n"
                 "User:\t\t%s\n"
                 "SSL/TLS:\t%s\n",
                 cfg.srv[cfg.selected_srv]->mount, cfg.srv[cfg.selected_srv]->usr, cfg.srv[cfg.selected_srv]->tls == 0 ? _("no") : _("yes"));
    }
    snprintf(text_buf + strlen(text_buf), sizeof(text_buf) - strlen(text_buf), _("Device 1:\t%s\n"), cfg.audio.dev_name);
    snprintf(text_buf + strlen(text_buf), sizeof(text_buf) - strlen(text_buf), _("Device 2:\t%s\n"), cfg.audio.dev2_name);
    print_info(text_buf, 0);

    static int called_from_connect_cb = 1;
    if (!cfg.main.song_update && !cfg.main.app_update) {
        Fl::add_timeout(cfg.main.song_delay, &update_song, &called_from_connect_cb);
    }

    deactivate_stream_ui_elements();

    pa_new_frames = 0;

    // Just in case the record routine started a check_time timeout
    // already
    Fl::remove_timeout(&display_info_timer);

    static int reset = 1;
    Fl::add_timeout(0.1, &display_info_timer);
    Fl::add_timeout(0.1, &is_connected_timer);

    if (cfg.gui.show_listeners) {
        Fl::remove_timeout(&request_listener_count_timer);
        Fl::add_timeout(0.5, &request_listener_count_timer, &reset);
    }

    if (cfg.main.song_update) {
        Fl::remove_timeout(&songfile_timer);
        Fl::add_timeout(0.5, &songfile_timer, &reset);
    }

    if (cfg.main.app_update) {
        current_track_app = getCurrentTrackFunctionFromId(cfg.main.app_update_service);
        Fl::remove_timeout(&app_timer);
        Fl::add_timeout(0.5, &app_timer, &reset);
    }

    if (cfg.main.silence_detection == 1 && cfg.main.silence_threshold > 0) {
        Fl::remove_timeout(&stream_silence_timer);
        Fl::add_timeout(1, &stream_silence_timer);
    }

    snd_start_streaming_thread();

    if (cfg.rec.start_rec && !recording) {
        button_record_cb(false);
    }

    if (strlen(cfg.gui.window_title) == 0) {
        snprintf(text_buf, sizeof(text_buf), _("Connected to: %s"), cfg.srv[cfg.selected_srv]->name);
        fl_g->window_main->label(text_buf);
    }

    fl_g->button_connect->color(fl_rgb_color(12, 179, 0));
    fl_g->button_connect->redraw();

    display_info = STREAM_TIME;

#ifdef WIN32
    if (tray_agent_is_running(NULL) == 1) {
        tray_agent_send_cmd(TA_CONNECT_STATE);
    }
#endif
}

void button_cfg_cb(void)
{
    if (fl_g->window_cfg->shown()) {
        fl_g->window_cfg->hide();
        Fl::remove_timeout(&cfg_win_pos_timer);
    }
    else {
#ifdef WIN32
        fl_g->window_cfg->position(fl_g->window_main->x() + fl_g->window_main->w() + 0, fl_g->window_main->y());
#else
        fl_g->window_cfg->position(fl_g->window_main->x() + fl_g->window_main->w(), fl_g->window_main->y());
#endif
        fl_g->window_cfg->show();
        fill_cfg_widgets();

        if (cfg.gui.attach) {
            Fl::add_timeout(0.1, &cfg_win_pos_timer);
        }
    }
}

void button_mixer_cb(void)
{
    if (fl_g->window_mixer->shown()) {
        fl_g->window_mixer->hide();
    }
    else {
#ifdef WIN32
        fl_g->window_mixer->position(fl_g->window_main->x() - fl_g->window_mixer->w() + 0, fl_g->window_main->y());
#else
        fl_g->window_mixer->position(fl_g->window_main->x() - fl_g->window_mixer->w(), fl_g->window_main->y());
#endif
        fl_g->window_mixer->show();
    }
}

// add server
void button_add_srv_add_cb(void)
{
    int i, j, k;
    int server_exists;

#ifdef WITH_RADIOCO
    if (fl_g->radio_add_srv_radioco->value()) {
        if (fl_g->browser_add_srv_station_list->nchecked() == 0) {
            return;
        }

        radioco_stations_t stations;
        radioco_get_station_list(&stations);
        for (i = 0; i < stations.num_of_stations; i++) {
            if (fl_g->browser_add_srv_station_list->checked(i + 1) == 0) {
                continue;
            }

            server_exists = 0;
            // check if the name already exists
            for (j = 0; j < cfg.main.num_of_srv; j++) {
                if (!strcmp(stations.name[i], cfg.srv[j]->name)) {
                    server_exists = 1;
                    break;
                }
            }
            if (server_exists == 1) {
                continue;
            }

            k = cfg.main.num_of_srv;
            cfg.main.num_of_srv++;

            cfg.srv = (server_t **)realloc(cfg.srv, cfg.main.num_of_srv * sizeof(server_t *));
            cfg.srv[k] = (server_t *)malloc(sizeof(server_t));

            cfg.srv[k]->name = (char *)malloc(strlen(stations.name[i]) + 1);
            strcpy(cfg.srv[k]->name, stations.name[i]);
            strrpl(&cfg.srv[k]->name, (char *)"[", (char *)"", MODE_ALL);
            strrpl(&cfg.srv[k]->name, (char *)"]", (char *)"", MODE_ALL);
            strrpl(&cfg.srv[k]->name, (char *)"/", (char *)"", MODE_ALL);
            strrpl(&cfg.srv[k]->name, (char *)"\\", (char *)"", MODE_ALL);
            strrpl(&cfg.srv[k]->name, (char *)";", (char *)"", MODE_ALL);

            cfg.srv[k]->addr = (char *)malloc(strlen(stations.host[i]) + 1);
            strcpy(cfg.srv[k]->addr, stations.host[i]);

            cfg.srv[k]->pwd = (char *)malloc(strlen(stations.password[i]) + 1);
            strcpy(cfg.srv[k]->pwd, stations.password[i]);

            cfg.srv[k]->port = stations.port[i];

            cfg.srv[k]->cert_hash = NULL;
            cfg.srv[k]->mount = NULL;
            cfg.srv[k]->usr = NULL;
            cfg.srv[k]->type = RADIOCO;
            cfg.srv[k]->tls = 0;

            if (cfg.main.num_of_srv > 1) {
                cfg.main.srv_ent = (char *)realloc(cfg.main.srv_ent, strlen(cfg.main.srv_ent) + strlen(cfg.srv[k]->name) + 2);
                sprintf(cfg.main.srv_ent + strlen(cfg.main.srv_ent), ";%s", cfg.srv[k]->name);
                cfg.main.srv = (char *)realloc(cfg.main.srv, strlen(cfg.srv[k]->name) + 1);
            }
            else {
                cfg.main.srv_ent = (char *)malloc(strlen(cfg.srv[k]->name) + 1);
                sprintf(cfg.main.srv_ent, "%s", cfg.srv[k]->name);
                cfg.main.srv = (char *)malloc(strlen(cfg.srv[k]->name) + 1);
            }

            fl_g->choice_cfg_act_srv->add(cfg.srv[k]->name);
            fl_g->choice_cfg_act_srv->redraw();
            strcpy(cfg.main.srv, cfg.srv[k]->name);
            fl_g->choice_cfg_act_srv->value(k);
        }

        // Activate del and edit buttons
        fl_g->button_cfg_edit_srv->activate();
        fl_g->button_cfg_del_srv->activate();

        fl_g->choice_cfg_act_srv->activate();

        // make last added server the active server
        choice_cfg_act_srv_cb();

        fl_g->browser_add_srv_station_list->clear();

        fl_g->window_add_srv->hide();

        return;
    }
#endif

    // error checking
    if ((fl_g->radio_add_srv_icecast->value()) && (strlen(fl_g->input_add_srv_mount->value()) == 0)) {
        fl_alert(_("No mountpoint specified\nSetting mountpoint to \"stream\""));
        fl_g->input_add_srv_mount->value("stream");
    }
    if ((fl_g->radio_add_srv_icecast->value()) && (strlen(fl_g->input_add_srv_usr->value()) == 0)) {
        fl_alert(_("No user specified\nSetting user to \"source\""));
        fl_g->input_add_srv_usr->value("source");
    }
    if (strlen(fl_g->input_add_srv_name->value()) == 0) {
        fl_alert(_("No name specified"));
        return;
    }
    if (cfg.main.srv_ent != NULL) {
        if (strlen(fl_g->input_add_srv_name->value()) + strlen(cfg.main.srv_ent) > 1000) {
            fl_alert(_("The number of characters of all your server names exeeds 1000\n"
                       "Please reduce the number of characters of each server name"));
            return;
        }
    }
    if (strpbrk(fl_g->input_add_srv_name->value(), "[];\\/\n\r") != NULL) {
        fl_alert(_("Newline characters and [];/\\ are not allowed within the server name"));
        return;
    }
    if (strlen(fl_g->input_add_srv_addr->value()) == 0) {
        fl_alert(_("No address specified"));
        return;
    }
    if (strlen(fl_g->input_add_srv_pwd->value()) == 0) {
        fl_alert(_("No password specified"));
        return;
    }
    if (strlen(fl_g->input_add_srv_port->value()) == 0) {
        fl_alert(_("No port specified"));
        return;
    }
    else if ((atoi(fl_g->input_add_srv_port->value()) > 65535) || (atoi(fl_g->input_add_srv_port->value()) < 1)) {
        fl_alert(_("Invalid port number\nThe port number must be between 1 and 65535"));
        return;
    }

    // check if the name already exists
    for (i = 0; i < cfg.main.num_of_srv; i++) {
        if (!strcmp(fl_g->input_add_srv_name->value(), cfg.srv[i]->name)) {
            fl_alert(_("Server name already exist!"));
            return;
        }
    }

    i = cfg.main.num_of_srv;
    cfg.main.num_of_srv++;

    cfg.srv = (server_t **)realloc(cfg.srv, cfg.main.num_of_srv * sizeof(server_t *));
    cfg.srv[i] = (server_t *)malloc(sizeof(server_t));

    cfg.srv[i]->name = (char *)malloc(strlen(fl_g->input_add_srv_name->value()) + 1);
    strcpy(cfg.srv[i]->name, fl_g->input_add_srv_name->value());

    cfg.srv[i]->addr = (char *)malloc(strlen(fl_g->input_add_srv_addr->value()) + 1);
    strcpy(cfg.srv[i]->addr, fl_g->input_add_srv_addr->value());

    cfg.srv[i]->cert_hash = NULL;

    // strip leading http:// from addr
    strrpl(&cfg.srv[i]->addr, (char *)"http://", (char *)"", MODE_ALL);
    strrpl(&cfg.srv[i]->addr, (char *)"https://", (char *)"", MODE_ALL);

    cfg.srv[i]->pwd = (char *)malloc(strlen(fl_g->input_add_srv_pwd->value()) + 1);
    strcpy(cfg.srv[i]->pwd, fl_g->input_add_srv_pwd->value());

    cfg.srv[i]->port = atoi(fl_g->input_add_srv_port->value());

    if (fl_g->radio_add_srv_icecast->value()) {
        cfg.srv[i]->mount = (char *)malloc(strlen(fl_g->input_add_srv_mount->value()) + 1);
        strcpy(cfg.srv[i]->mount, fl_g->input_add_srv_mount->value());

        cfg.srv[i]->usr = (char *)malloc(strlen(fl_g->input_add_srv_usr->value()) + 1);
        strcpy(cfg.srv[i]->usr, fl_g->input_add_srv_usr->value());

        cfg.srv[i]->type = ICECAST;
        cfg.srv[i]->icecast_protocol = fl_g->check_add_srv_protocol->value();
    }
    else {
        cfg.srv[i]->mount = NULL;
        cfg.srv[i]->usr = NULL;
        cfg.srv[i]->type = SHOUTCAST;
    }

    cfg.srv[i]->tls = fl_g->check_add_srv_tls->value();

    if (cfg.main.num_of_srv > 1) {
        cfg.main.srv_ent = (char *)realloc(cfg.main.srv_ent, strlen(cfg.main.srv_ent) + strlen(cfg.srv[i]->name) + 2);
        sprintf(cfg.main.srv_ent + strlen(cfg.main.srv_ent), ";%s", cfg.srv[i]->name);
        cfg.main.srv = (char *)realloc(cfg.main.srv, strlen(cfg.srv[i]->name) + 1);
    }
    else {
        cfg.main.srv_ent = (char *)malloc(strlen(cfg.srv[i]->name) + 1);
        sprintf(cfg.main.srv_ent, "%s", cfg.srv[i]->name);
        cfg.main.srv = (char *)malloc(strlen(cfg.srv[i]->name) + 1);
    }
    strcpy(cfg.main.srv, cfg.srv[i]->name);

    // reset the input fields and hide the window
    fl_g->input_add_srv_name->value("");
    fl_g->input_add_srv_addr->value("");
    fl_g->input_add_srv_port->value("");
    fl_g->input_add_srv_pwd->value("");
    fl_g->input_add_srv_mount->value("");
    fl_g->input_add_srv_usr->value("");
    fl_g->window_add_srv->hide();
    fl_g->check_add_srv_tls->value(0);
    fl_g->check_add_srv_protocol->value(ICECAST_PROTOCOL_PUT);

    fl_g->choice_cfg_act_srv->add(cfg.srv[i]->name);
    fl_g->choice_cfg_act_srv->redraw();

    // Activate del and edit buttons
    fl_g->button_cfg_edit_srv->activate();
    fl_g->button_cfg_del_srv->activate();

    fl_g->choice_cfg_act_srv->activate();

    // make added server the active server
    fl_g->choice_cfg_act_srv->value(i);
    choice_cfg_act_srv_cb();
}

void button_cfg_del_srv_cb(void)
{
    int i;
    int diff;

    if (cfg.main.num_of_srv == 0) {
        return;
    }

    if (cfg.srv[cfg.selected_srv]->name != NULL) {
        free(cfg.srv[cfg.selected_srv]->name);
    }

    if (cfg.srv[cfg.selected_srv]->addr != NULL) {
        free(cfg.srv[cfg.selected_srv]->addr);
    }

    diff = cfg.main.num_of_srv - 1 - cfg.selected_srv;
    for (i = 0; i < diff; i++) {
        memcpy(cfg.srv[cfg.selected_srv + i], cfg.srv[cfg.selected_srv + i + 1], sizeof(server_t));
    }

    free(cfg.srv[cfg.main.num_of_srv - 1]);

    cfg.main.num_of_srv--;

    // rearrange the string that contains all server names
    memset(cfg.main.srv_ent, 0, strlen(cfg.main.srv_ent));
    for (i = 0; i < (int)cfg.main.num_of_srv; i++) {
        strcat(cfg.main.srv_ent, cfg.srv[i]->name);

        // the last entry doesn't have a trailing seperator ";"
        if (i < (int)cfg.main.num_of_srv - 1) {
            strcat(cfg.main.srv_ent, ";");
        }
    }

    fl_g->choice_cfg_act_srv->remove(cfg.selected_srv);
    fl_g->choice_cfg_act_srv->redraw(); // Yes we need this :-(

    if (cfg.main.num_of_srv == 0) {
        fl_g->button_cfg_edit_srv->deactivate();
        fl_g->button_cfg_del_srv->deactivate();
        fl_g->choice_cfg_act_srv->deactivate();
        free(cfg.main.srv);
    }

    if (cfg.selected_srv > 0) {
        cfg.selected_srv--;
    }
    else {
        cfg.selected_srv = 0;
    }

    if (cfg.main.num_of_srv > 0) {
        fl_g->choice_cfg_act_srv->value(cfg.selected_srv);
        choice_cfg_act_srv_cb();
    }
}

void button_cfg_del_icy_cb(void)
{
    int i;
    int diff;

    if (cfg.main.num_of_icy == 0) {
        return;
    }

    if (cfg.icy[cfg.selected_icy]->name != NULL) {
        free(cfg.icy[cfg.selected_icy]->name);
    }

    if (cfg.icy[cfg.selected_icy]->genre != NULL) {
        free(cfg.icy[cfg.selected_icy]->genre);
    }

    if (cfg.icy[cfg.selected_icy]->url != NULL) {
        free(cfg.icy[cfg.selected_icy]->url);
    }

    if (cfg.icy[cfg.selected_icy]->irc != NULL) {
        free(cfg.icy[cfg.selected_icy]->irc);
    }

    if (cfg.icy[cfg.selected_icy]->icq != NULL) {
        free(cfg.icy[cfg.selected_icy]->icq);
    }

    if (cfg.icy[cfg.selected_icy]->aim != NULL) {
        free(cfg.icy[cfg.selected_icy]->aim);
    }

    if (cfg.icy[cfg.selected_icy]->pub != NULL) {
        free(cfg.icy[cfg.selected_icy]->pub);
    }

    diff = cfg.main.num_of_icy - 1 - cfg.selected_icy;
    for (i = 0; i < diff; i++) {
        memcpy(cfg.icy[cfg.selected_icy + i], cfg.icy[cfg.selected_icy + i + 1], sizeof(icy_t));
    }

    free(cfg.icy[cfg.main.num_of_icy - 1]);

    cfg.main.num_of_icy--;

    // recreate the string that contains all ICY names
    memset(cfg.main.icy_ent, 0, strlen(cfg.main.icy_ent));
    for (i = 0; i < (int)cfg.main.num_of_icy; i++) {
        strcat(cfg.main.icy_ent, cfg.icy[i]->name);

        // do not add a trailing seperator ";" to the last entry
        if (i < (int)cfg.main.num_of_icy - 1) {
            strcat(cfg.main.icy_ent, ";");
        }
    }

    fl_g->choice_cfg_act_icy->remove(cfg.selected_icy);
    fl_g->choice_cfg_act_icy->redraw();

    if (cfg.main.num_of_icy == 0) {
        fl_g->button_cfg_edit_icy->deactivate();
        fl_g->button_cfg_del_icy->deactivate();
        fl_g->choice_cfg_act_icy->deactivate();
        free(cfg.main.icy);
    }

    if (cfg.selected_icy > 0) {
        cfg.selected_icy--;
    }
    else {
        cfg.selected_icy = 0;
    }

    if (cfg.main.num_of_icy > 0) {
        fl_g->choice_cfg_act_icy->value(cfg.selected_icy);
        choice_cfg_act_icy_cb();
    }
}

void button_disconnect_cb(bool ask)
{
    fl_g->label_current_listeners->hide();

    if (!connected && recording) {
        stop_recording(ask); // true = ask user if recording shall be stopped
    }

    if (connected && recording && cfg.rec.stop_rec) {
        stop_recording(false); // false = do not ask user
    }

    if (!recording) {
        fl_g->lcd->clear();
        fl_g->lcd->print((const uchar *)_("idle"), strlen(_("idle")));
        fl_g->radio_co_logo->show();
        fl_g->radio_co_logo->redraw();
    }

    // We are not trying to connect anymore
    try_to_connect = 0;

    if (cfg.main.signal_detection == 1 && cfg.main.signal_threshold > 0) {
        Fl::remove_timeout(&stream_signal_timer);
        Fl::add_timeout(1, &stream_signal_timer);
    }

    if (!connected) {
        return;
    }

    activate_stream_ui_elements();

    if (!recording) {
        Fl::remove_timeout(&display_info_timer);
    }
    else {
        display_info = REC_TIME;
    }

    Fl::remove_timeout(&songfile_timer);
    Fl::remove_timeout(&app_timer);

    if (connected) {
        Fl::remove_timeout(&is_connected_timer);
        Fl::remove_timeout(&stream_silence_timer);
        snd_stop_streaming_thread();

        if (cfg.srv[cfg.selected_srv]->type == ICECAST) {
            ic_disconnect();
        }
        else {
            sc_disconnect();
        }

        if (strlen(cfg.gui.window_title) == 0) {
            fl_g->window_main->label(PACKAGE_STRING);
        }

        fl_g->button_connect->color(FL_BACKGROUND_COLOR);
        fl_g->button_connect->redraw();

#ifdef WIN32
        if (tray_agent_is_running(NULL) == 1) {
            tray_agent_send_cmd(TA_CONNECT_STATE);
        }
#endif
    }
    else {
        print_info("Connecting canceled\n", 0);
    }
}

bool stop_recording(bool ask)
{
    if (!recording) {
        return false;
    }

    if (ask == true) {
        int rc = 0;
        rc = fl_choice(_("stop recording?"), _("No"), _("Yes"), NULL);
        if (rc == 0) // if NO pressed
            return false;
    }

    Fl::remove_timeout(&record_silence_timer);
    snd_stop_recording_thread();

    activate_rec_ui_elements();
    if (!connected) {
        fl_g->lcd->clear();
        fl_g->lcd->print((const uchar *)_("idle"), strlen(_("idle")));
        fl_g->radio_co_logo->show();
        fl_g->radio_co_logo->redraw();
        Fl::remove_timeout(&display_info_timer);
    }
    else {
        display_info = STREAM_TIME;
    }

    if (cfg.rec.signal_detection == 1 && cfg.rec.signal_threshold > 0) {
        Fl::remove_timeout(&record_signal_timer);
        Fl::add_timeout(1, &record_signal_timer);
    }

    fl_g->button_record->color(FL_BACKGROUND_COLOR);
    fl_g->button_record->redraw();

    return true;
}

void button_record_cb(bool ask)
{
    char mode[3];

    if (recording) {
        stop_recording(true);
        return;
    }

    if (strlen(cfg.rec.filename) == 0) {
        fl_alert(_("No recording filename specified"));
        return;
    }

    if (eval_record_path(0) != 0) {
        return;
    }

    // check if the file already exists
    if ((ask == true) && (fl_access(cfg.rec.path, F_OK)) == 0) {
        int rc = fl_choice(_("%s already exists!"), _("cancel"), _("overwrite"), _("append"), cfg.rec.path);
        switch (rc) {
        case 0: // cancel pressed
            return;
            break;
        case 1: // overwrite pressed
            strcpy(mode, "wb");
            break;
        case 2: // append pressed
            strcpy(mode, "ab");
        }
    }
    else { // selected file doesn't exist yet
        strcpy(mode, "wb");
    }

    if ((cfg.rec.fd = fl_fopen(cfg.rec.path, mode)) == NULL) {
        fl_alert(_("Could not open:\n%s"), cfg.rec.path);
        return;
    }

    timer_init(&rec_timer, 1.0);
    timer_start(&rec_timer);

    // Clear libsamplerate state
    snd_reset_samplerate_conv(SND_REC);

    // Allow the flac codec to access the file pointed to by cfg.rec.fd
    if (!strcmp(cfg.rec.codec, "flac")) {
        flac_enc_init(&flac_rec);
        flac_enc_init_FILE(&flac_rec, cfg.rec.fd);
    }

    // User may not change any record related audio settings while recording
    deactivate_rec_ui_elements();

    snd_start_recording_thread();

    if (cfg.rec.split_time > 0) {
        split_recording_file_timer();
    }

    if (cfg.rec.silence_detection == 1 && cfg.rec.silence_threshold > 0) {
        Fl::remove_timeout(&record_silence_timer);
        Fl::add_timeout(1, &record_silence_timer);
    }

    Fl::remove_timeout(&record_signal_timer);

    if (!connected) {
        display_info = REC_TIME;
        Fl::add_timeout(0.1, &display_info_timer);
    }

    fl_g->button_record->color(fl_rgb_color(255, 75, 75));
    fl_g->button_record->redraw();
}

void button_info_cb()
{
    if (!fl_g->info_visible) {
        // Show info output...

        fl_g->window_main->size_range(fl_g->window_main->w(), 276, fl_g->window_main->w(), 0);

        if (cfg.gui.window_height > fl_g->info_output->y() + 10) {
            fl_g->window_main->resize(fl_g->window_main->x(), fl_g->window_main->y(), fl_g->window_main->w(), cfg.gui.window_height);
        }
        else {
            fl_g->window_main->resize(fl_g->window_main->x(), fl_g->window_main->y(), fl_g->window_main->w(), fl_g->info_output->y() + 190);
        }
        fl_g->info_output->show();
        fl_g->button_info->label(_("Hide log"));
        fl_g->info_visible = 1;
    }
    else {
        // Hide info output...
        fl_g->window_main->size_range(fl_g->window_main->w(), fl_g->info_output->y() - 25, fl_g->window_main->w(), fl_g->info_output->y() - 25);
        fl_g->info_output->hide();
        fl_g->window_main->resize(fl_g->window_main->x(), fl_g->window_main->y(), fl_g->window_main->w(), fl_g->info_output->y() - 25);
        fl_g->button_info->label(_("Show log"));
        fl_g->info_visible = 0;
    }
}

void choice_cfg_act_srv_cb(void)
{
    cfg.selected_srv = fl_g->choice_cfg_act_srv->value();

    cfg.main.srv = (char *)realloc(cfg.main.srv, strlen(cfg.srv[cfg.selected_srv]->name) + 1);

    strcpy(cfg.main.srv, cfg.srv[cfg.selected_srv]->name);
}

void choice_cfg_act_icy_cb(void)
{
    cfg.selected_icy = fl_g->choice_cfg_act_icy->value();

    cfg.main.icy = (char *)realloc(cfg.main.icy, strlen(cfg.icy[cfg.selected_icy]->name) + 1);

    strcpy(cfg.main.icy, cfg.icy[cfg.selected_icy]->name);
}

void button_cfg_add_srv_cb(void)
{
    fl_g->window_add_srv->label(_("Add Server"));
    fl_g->radio_add_srv_shoutcast->setonly();
    fl_g->input_add_srv_mount->deactivate();
    fl_g->input_add_srv_usr->deactivate();

    fl_g->check_add_srv_tls->deactivate();
    fl_g->frame_add_srv_tls->deactivate();
    fl_g->button_add_srv_revoke_cert->deactivate();
    fl_g->check_add_srv_protocol->deactivate();

    fl_g->input_add_srv_pwd->input_type(FL_SECRET_INPUT);
    fl_g->input_add_srv_pwd->redraw();
    fl_g->button_cfg_show_pw->label(_("Show"));

    fl_g->button_add_srv_save->hide();
    fl_g->button_add_srv_add->show();

    fl_g->window_add_srv->position(fl_g->window_cfg->x(), fl_g->window_cfg->y());
    fl_g->input_add_srv_name->take_focus();
    fl_g->window_add_srv->show();

    fl_g->radio_add_srv_radioco->activate();

    fl_g->input_add_srv_pwd->show();
    fl_g->input_add_srv_port->show();
    fl_g->input_add_srv_addr->show();
    fl_g->button_cfg_show_pw->show();
    fl_g->input_add_srv_mount->show();
    fl_g->input_add_srv_usr->show();

    fl_g->browser_add_srv_station_list->hide();
    fl_g->button_add_srv_get_stations->hide();
    fl_g->button_add_srv_select_all->hide();
    fl_g->button_add_srv_deselect_all->hide();
}

void button_cfg_edit_srv_cb(void)
{
    char dummy[10];
    int srv;

    if (cfg.main.num_of_srv < 1) {
        return;
    }

    fl_g->window_add_srv->label(_("Edit Server"));

    srv = fl_g->choice_cfg_act_srv->value();

    if (cfg.srv[srv]->type == RADIOCO) {
        fl_message(_("Radio.co stations cannot be edited."));
        return;
    }

    fl_g->input_add_srv_name->value(cfg.srv[srv]->name);
    fl_g->input_add_srv_addr->value(cfg.srv[srv]->addr);

    snprintf(dummy, 6, "%u", cfg.srv[srv]->port);
    fl_g->input_add_srv_port->value(dummy);
    fl_g->input_add_srv_pwd->value(cfg.srv[srv]->pwd);

    fl_g->input_add_srv_pwd->input_type(FL_SECRET_INPUT);
    fl_g->input_add_srv_pwd->redraw();
    fl_g->button_cfg_show_pw->label(_("Show"));

    fl_g->radio_add_srv_radioco->deactivate();

    fl_g->input_add_srv_pwd->show();
    fl_g->input_add_srv_port->show();
    fl_g->input_add_srv_addr->show();
    fl_g->button_cfg_show_pw->show();
    fl_g->input_add_srv_mount->show();
    fl_g->input_add_srv_usr->show();

    fl_g->browser_add_srv_station_list->hide();
    fl_g->button_add_srv_get_stations->hide();
    fl_g->button_add_srv_select_all->hide();
    fl_g->button_add_srv_deselect_all->hide();

    if (cfg.srv[srv]->type == SHOUTCAST) {
        fl_g->input_add_srv_mount->value("");
        fl_g->input_add_srv_mount->deactivate();
        fl_g->input_add_srv_usr->value("");
        fl_g->input_add_srv_usr->deactivate();
        fl_g->check_add_srv_tls->deactivate();
        fl_g->frame_add_srv_tls->deactivate();
        fl_g->button_add_srv_revoke_cert->deactivate();
        fl_g->check_add_srv_protocol->deactivate();
        fl_g->radio_add_srv_shoutcast->setonly();
    }
    else { // if(cfg.srv[srv]->type == ICECAST)
        fl_g->input_add_srv_mount->value(cfg.srv[srv]->mount);
        fl_g->input_add_srv_mount->activate();
        fl_g->input_add_srv_usr->value(cfg.srv[srv]->usr);
        fl_g->input_add_srv_usr->activate();
        fl_g->radio_add_srv_icecast->setonly();
#ifdef HAVE_LIBSSL
        fl_g->check_add_srv_tls->activate();
        fl_g->frame_add_srv_tls->activate();
#else
        fl_g->check_add_srv_tls->deactivate();
        fl_g->frame_add_srv_tls->deactivate();
#endif
        fl_g->check_add_srv_protocol->activate();

        if ((cfg.srv[srv]->cert_hash != NULL) && (strlen(cfg.srv[srv]->cert_hash) == 64)) {
            fl_g->button_add_srv_revoke_cert->activate();
        }
        else {
            fl_g->button_add_srv_revoke_cert->deactivate();
        }
    }

    fl_g->check_add_srv_tls->value(cfg.srv[srv]->tls);
    fl_g->check_add_srv_protocol->value(cfg.srv[srv]->icecast_protocol);

    fl_g->input_add_srv_name->take_focus();

    fl_g->button_add_srv_add->hide();
    fl_g->button_add_srv_save->show();

    fl_g->window_add_srv->position(fl_g->window_cfg->x(), fl_g->window_cfg->y());
    fl_g->window_add_srv->show();
}

void button_cfg_add_icy_cb(void)
{
    fl_g->window_add_icy->label(_("Add Server Infos"));

    fl_g->button_add_icy_save->hide();
    fl_g->button_add_icy_add->show();
    fl_g->window_add_icy->position(fl_g->window_cfg->x(), fl_g->window_cfg->y());

    // give the "name" input field the input focus
    fl_g->input_add_icy_name->take_focus();

    fl_g->window_add_icy->show();
}

void update_song(void *user_data)
{
    static int is_updating = 0;

    int prefix_len = 0;
    int suffix_len = 0;
    char text_buf[512];
    char song_buf[512];
    song_buf[0] = '\0';

    int (*xc_update_song)(char *song_name) = NULL;

    int called_from_connect_cb;
    if (user_data != NULL) {
        called_from_connect_cb = *((int *)user_data);
    }
    else {
        called_from_connect_cb = 0;
    }

    if (!connected || cfg.main.song == NULL) {
        return;
    }

    // Make sure this function is not executed from different places at the same time
    if (is_updating == 1) {
        return;
    }

    is_updating = 1;

    if (cfg.main.song_prefix != NULL) {
        prefix_len = strlen(cfg.main.song_prefix);
        strncat(song_buf, cfg.main.song_prefix, sizeof(song_buf) - 1);
    }

    strncat(song_buf, cfg.main.song, sizeof(song_buf) - 1 - prefix_len);

    if (cfg.main.song_suffix != NULL) {
        suffix_len = strlen(cfg.main.song_suffix);
        strncat(song_buf, cfg.main.song_suffix, sizeof(song_buf) - 1 - prefix_len - suffix_len);
    }

    if (!strcmp(cfg.audio.codec, "flac")) {
        if (called_from_connect_cb == 0) {
            flac_update_song_title(&flac_stream, song_buf);
        }
        else {
            flac_set_initial_song_title(&flac_stream, song_buf);
        }
    }

    if (!strcmp(cfg.audio.codec, "opus")) {
        opus_update_song_title(&opus_stream, song_buf);
    }

    if (cfg.srv[cfg.selected_srv]->type == ICECAST) {
        xc_update_song = &ic_update_song;
    }
    else // if(cfg.srv[cfg.selected_srv]->type == SHOUTCAST)
        xc_update_song = &sc_update_song;

    if (xc_update_song(song_buf) == 0) {
        snprintf(text_buf, sizeof(text_buf), _("Updated songname to:\n%s\n"), song_buf);

        print_info(text_buf, 0);
#ifdef WIN32
        tray_agent_set_song(song_buf);
        tray_agent_send_cmd(TA_SONG_UPDATE);
#endif
    }
    else {
        print_info(_("Updating songname failed"), 1);
    }

    is_updating = 0;
}

void button_cfg_song_go_cb(void)
{
    cfg.main.song = (char *)realloc(cfg.main.song, strlen(fl_g->input_cfg_song->value()) + 1);
    strcpy(cfg.main.song, fl_g->input_cfg_song->value());

    Fl::add_timeout(cfg.main.song_delay, &update_song);

    // Set focus on the song input field and mark the whole text
    fl_g->input_cfg_song->take_focus();
    fl_g->input_cfg_song->position(0);
    fl_g->input_cfg_song->mark(fl_g->input_cfg_song->maximum_size());
}

void input_cfg_song_cb(void)
{
    if (strlen(fl_g->input_cfg_song->value()) == 0) {
        if (cfg.main.song != NULL) {
            free(cfg.main.song);
        }

        cfg.main.song = NULL;
    }
}

void input_cfg_song_prefix_cb(void)
{
    if (strlen(fl_g->input_cfg_song_prefix->value()) == 0) {
        if (cfg.main.song_prefix != NULL) {
            free(cfg.main.song_prefix);
        }

        cfg.main.song_prefix = NULL;
    }
    else {
        cfg.main.song_prefix = (char *)realloc(cfg.main.song_prefix, strlen(fl_g->input_cfg_song_prefix->value()) + 1);
        strcpy(cfg.main.song_prefix, fl_g->input_cfg_song_prefix->value());
    }
}
void input_cfg_song_suffix_cb(void)
{
    if (strlen(fl_g->input_cfg_song_suffix->value()) == 0) {
        if (cfg.main.song_suffix != NULL) {
            free(cfg.main.song_suffix);
        }

        cfg.main.song_suffix = NULL;
    }
    else {
        cfg.main.song_suffix = (char *)realloc(cfg.main.song_suffix, strlen(fl_g->input_cfg_song_suffix->value()) + 1);
        strcpy(cfg.main.song_suffix, fl_g->input_cfg_song_suffix->value());
    }
}

void input_cfg_buffer_cb(bool print_message)
{
    int ms;
    char text_buf[256];

    ms = fl_g->input_cfg_buffer->value();

    if (ms < 1) {
        return;
    }

    cfg.audio.buffer_ms = ms;
    snd_reopen_streams();

    if (print_message) {
        snprintf(text_buf, sizeof(text_buf), _("Audio buffer has been set to %d ms"), ms);
        print_info(text_buf, 0);
    }
}

void choice_cfg_resample_mode_cb(void)
{
    cfg.audio.resample_mode = fl_g->choice_cfg_resample_mode->value();
    snd_reopen_streams();
    switch (cfg.audio.resample_mode) {
    case SRC_SINC_BEST_QUALITY:
        print_info("Changed resample quality to SINC_BEST_QUALITY", 0);
        break;
    case SRC_SINC_MEDIUM_QUALITY:
        print_info("Changed resample quality to SINC_MEDIUM_QUALITY", 0);
        break;
    case SRC_SINC_FASTEST:
        print_info("Changed resample quality to SINC_FASTEST", 0);
        break;
    case SRC_ZERO_ORDER_HOLD:
        print_info("Changed resample quality to ZERO_ORDER_HOLD", 0);
        break;
    case SRC_LINEAR:
        print_info("Changed resample quality to LINEAR", 0);
        break;
    default:
        break;
    }
}

void radio_add_srv_shoutcast_cb(void)
{
    fl_g->input_add_srv_name->activate();

    fl_g->input_add_srv_pwd->show();
    fl_g->input_add_srv_port->show();
    fl_g->input_add_srv_addr->show();
    fl_g->button_cfg_show_pw->show();

    fl_g->input_add_srv_mount->show();
    fl_g->input_add_srv_usr->show();
    fl_g->input_add_srv_mount->deactivate();
    fl_g->input_add_srv_usr->deactivate();
    fl_g->check_add_srv_tls->deactivate();
    fl_g->frame_add_srv_tls->deactivate();
    fl_g->check_add_srv_protocol->deactivate();

    fl_g->browser_add_srv_station_list->hide();
    fl_g->button_add_srv_get_stations->hide();
    fl_g->button_add_srv_select_all->hide();
    fl_g->button_add_srv_deselect_all->hide();
}

void radio_add_srv_icecast_cb(void)
{
    fl_g->input_add_srv_name->activate();

    fl_g->input_add_srv_pwd->show();
    fl_g->input_add_srv_port->show();
    fl_g->input_add_srv_addr->show();
    fl_g->button_cfg_show_pw->show();
    fl_g->input_add_srv_mount->show();
    fl_g->input_add_srv_usr->show();

    fl_g->input_add_srv_mount->activate();
    fl_g->input_add_srv_usr->activate();

    fl_g->input_add_srv_mount->value("stream");
    fl_g->input_add_srv_usr->value("source");

#ifdef HAVE_LIBSSL
    fl_g->check_add_srv_tls->activate();
    fl_g->frame_add_srv_tls->activate();
#else
    fl_g->check_add_srv_tls->deactivate();
    fl_g->frame_add_srv_tls->deactivate();
#endif

    fl_g->check_add_srv_protocol->activate();

    fl_g->browser_add_srv_station_list->hide();
    fl_g->button_add_srv_get_stations->hide();
    fl_g->button_add_srv_select_all->hide();
    fl_g->button_add_srv_deselect_all->hide();
}

void radio_add_srv_radioco_cb(void)
{
    fl_g->input_add_srv_pwd->hide();
    fl_g->input_add_srv_port->hide();
    fl_g->input_add_srv_addr->hide();

    fl_g->input_add_srv_mount->hide();
    fl_g->input_add_srv_usr->hide();
    fl_g->button_cfg_show_pw->hide();

    fl_g->input_add_srv_name->deactivate();

    fl_g->check_add_srv_tls->deactivate();
    fl_g->frame_add_srv_tls->deactivate();

    fl_g->check_add_srv_protocol->deactivate();

    fl_g->browser_add_srv_station_list->show();
    fl_g->button_add_srv_get_stations->show();
    fl_g->button_add_srv_select_all->show();
    fl_g->button_add_srv_deselect_all->show();
}

void button_add_srv_get_stations_cb(void)
{
#ifdef WITH_RADIOCO
    if (fl_choice(_("butt will open Radio.co in a new browser window.\n\n"
                    "Login to Radio.co and allow butt access to your account."),
                  _("Cancel"), _("OK"), NULL) == 0) { // Cancel
        return;
    }

    radioco_request_access();

    Fl::add_timeout(1, &wait_for_radioco_timer);
#endif
}

void button_add_srv_select_all_cb(void)
{
    fl_g->browser_add_srv_station_list->check_all();
}
void button_add_srv_deselect_all_cb(void)
{
    fl_g->browser_add_srv_station_list->check_none();
}

void button_add_srv_show_pwd_cb(void)
{
    if (fl_g->input_add_srv_pwd->input_type() == FL_SECRET_INPUT) {
        fl_g->input_add_srv_pwd->input_type(FL_NORMAL_INPUT);
        fl_g->input_add_srv_pwd->redraw();
        fl_g->button_cfg_show_pw->label(_("Hide"));
    }
    else {
        fl_g->input_add_srv_pwd->input_type(FL_SECRET_INPUT);
        fl_g->input_add_srv_pwd->redraw();
        fl_g->button_cfg_show_pw->label(_("Show"));
    }
}

void button_add_srv_revoke_cert_cb(void)
{
    int srv;
    srv = fl_g->choice_cfg_act_srv->value();

    if (cfg.srv[srv]->cert_hash != NULL) {
        free(cfg.srv[srv]->cert_hash);
        cfg.srv[srv]->cert_hash = NULL;
        fl_g->button_add_srv_revoke_cert->deactivate();
    }
    else {
        fl_alert(_("Could not revoke trust for certificate"));
    }
}

// edit server
void button_add_srv_save_cb(void)
{
    int i;

    if (cfg.main.num_of_srv < 1) {
        return;
    }

    int srv_num = fl_g->choice_cfg_act_srv->value();
    int len = 0;

    // error checking
    if ((fl_g->radio_add_srv_icecast->value()) && (strlen(fl_g->input_add_srv_mount->value()) == 0)) {
        fl_alert(_("No mountpoint specified\nSetting mountpoint to \"stream\""));
        fl_g->input_add_srv_mount->value("stream");
    }
    if ((fl_g->radio_add_srv_icecast->value()) && (strlen(fl_g->input_add_srv_usr->value()) == 0)) {
        fl_alert(_("No user specified\nSetting user to \"source\""));
        fl_g->input_add_srv_usr->value("source");
    }
    if (strlen(fl_g->input_add_srv_name->value()) == 0) {
        fl_alert(_("No name specified"));
        return;
    }
    if (cfg.main.srv_ent != NULL) {
        if (strlen(fl_g->input_add_srv_name->value()) + strlen(cfg.main.srv_ent) > 1000) {
            fl_alert(_("The number of characters of all your server names exeeds 1000\n"
                       "Please reduce the number of characters of each server name"));
            return;
        }
    }
    if (strpbrk(fl_g->input_add_srv_name->value(), "[];\\/\n\r") != NULL) {
        fl_alert(_("Newline characters and [];/\\ are not allowed within the server name"));
        return;
    }
    if (strlen(fl_g->input_add_srv_addr->value()) == 0) {
        fl_alert(_("No address specified"));
        return;
    }
    if (strlen(fl_g->input_add_srv_pwd->value()) == 0) {
        fl_alert(_("No password specified"));
        return;
    }
    if (strlen(fl_g->input_add_srv_port->value()) == 0) {
        fl_alert(_("No port specified"));
        return;
    }
    else if ((atoi(fl_g->input_add_srv_port->value()) > 65535) || (atoi(fl_g->input_add_srv_port->value()) < 1)) {
        fl_alert(_("Invalid port number\nThe port number must be between 1 and 65535"));
        return;
    }

    // check if the name already exists
    for (i = 0; i < cfg.main.num_of_srv; i++) {
        if (i == srv_num) // don't check name against it self
            continue;
        if (!strcmp(fl_g->input_add_srv_name->value(), cfg.srv[i]->name)) {
            fl_alert(_("Server name already exist!"));
            return;
        }
    }

    // update current server name
    cfg.srv[srv_num]->name = (char *)realloc(cfg.srv[srv_num]->name, sizeof(char) * strlen(fl_g->input_add_srv_name->value()) + 1);

    strcpy(cfg.srv[srv_num]->name, fl_g->input_add_srv_name->value());

    // rewrite the string that contains all server names
    // first get the needed memory space
    for (int i = 0; i < cfg.main.num_of_srv; i++)
        len += strlen(cfg.srv[i]->name) + 1;
    // allocate enough memory
    cfg.main.srv_ent = (char *)realloc(cfg.main.srv_ent, sizeof(char) * len + 1);

    memset(cfg.main.srv_ent, 0, len);
    // now append the server strings
    for (int i = 0; i < cfg.main.num_of_srv; i++) {
        strcat(cfg.main.srv_ent, cfg.srv[i]->name);
        if (i < cfg.main.num_of_srv - 1) {
            strcat(cfg.main.srv_ent, ";");
        }
    }

    // update current server address
    cfg.srv[srv_num]->addr = (char *)realloc(cfg.srv[srv_num]->addr, sizeof(char) * strlen(fl_g->input_add_srv_addr->value()) + 1);

    strcpy(cfg.srv[srv_num]->addr, fl_g->input_add_srv_addr->value());

    // strip leading http:// from addr
    strrpl(&cfg.srv[srv_num]->addr, (char *)"http://", (char *)"", MODE_ALL);
    strrpl(&cfg.srv[srv_num]->addr, (char *)"https://", (char *)"", MODE_ALL);

    // update current server port
    cfg.srv[srv_num]->port = (unsigned int)atoi(fl_g->input_add_srv_port->value());

    // update current server password
    cfg.srv[srv_num]->pwd = (char *)realloc(cfg.srv[srv_num]->pwd, strlen(fl_g->input_add_srv_pwd->value()) + 1);

    strcpy(cfg.srv[srv_num]->pwd, fl_g->input_add_srv_pwd->value());

    // update current server type
    if (fl_g->radio_add_srv_shoutcast->value()) {
        cfg.srv[srv_num]->type = SHOUTCAST;
    }
    if (fl_g->radio_add_srv_icecast->value()) {
        cfg.srv[srv_num]->type = ICECAST;
    }

    // update current server mountpoint and user
    if (cfg.srv[srv_num]->type == ICECAST) {
        cfg.srv[srv_num]->mount = (char *)realloc(cfg.srv[srv_num]->mount, sizeof(char) * strlen(fl_g->input_add_srv_mount->value()) + 1);
        strcpy(cfg.srv[srv_num]->mount, fl_g->input_add_srv_mount->value());

        cfg.srv[srv_num]->usr = (char *)realloc(cfg.srv[srv_num]->usr, sizeof(char) * strlen(fl_g->input_add_srv_usr->value()) + 1);
        strcpy(cfg.srv[srv_num]->usr, fl_g->input_add_srv_usr->value());

        cfg.srv[srv_num]->icecast_protocol = fl_g->check_add_srv_protocol->value();
    }

    cfg.srv[srv_num]->tls = fl_g->check_add_srv_tls->value();

    fl_g->choice_cfg_act_srv->replace(srv_num, cfg.srv[srv_num]->name);
    fl_g->choice_cfg_act_srv->redraw();

    // reset the input fields and hide the window
    fl_g->input_add_srv_name->value("");
    fl_g->input_add_srv_addr->value("");
    fl_g->input_add_srv_port->value("");
    fl_g->input_add_srv_pwd->value("");
    fl_g->input_add_srv_mount->value("");
    fl_g->input_add_srv_usr->value("");
    fl_g->check_add_srv_tls->value(0);
    fl_g->check_add_srv_protocol->value(ICECAST_PROTOCOL_PUT);

    fl_g->window_add_srv->hide();

    choice_cfg_act_srv_cb();
}

void button_add_icy_save_cb(void)
{
    int i;

    if (cfg.main.num_of_icy < 1) {
        return;
    }

    int icy_num = fl_g->choice_cfg_act_icy->value();
    int len = 0;

    if (strlen(fl_g->input_add_icy_name->value()) == 0) {
        fl_alert(_("No name specified"));
        return;
    }
    if (cfg.main.icy_ent != NULL) {
        if (strlen(fl_g->input_add_icy_name->value()) + strlen(cfg.main.icy_ent) > 1000) {
            fl_alert(_("The number of characters of all your icy names exeeds 1000\n"
                       "Please reduce the count of characters of each icy name"));
            return;
        }
    }
    if (strpbrk(fl_g->input_add_icy_name->value(), "[];\\/\n\r") != NULL) {
        fl_alert(_("Newline characters and [];/\\ are not allowed within the icy name"));
        return;
    }

    // check if the name already exists
    for (i = 0; i < cfg.main.num_of_icy; i++) {
        if (i == icy_num) // don't check name against it self
            continue;
        if (!strcmp(fl_g->input_add_icy_name->value(), cfg.icy[i]->name)) {
            fl_alert(_("Icy name already exist!"));
            return;
        }
    }

    // update current icy name
    cfg.icy[icy_num]->name = (char *)realloc(cfg.icy[icy_num]->name, sizeof(char) * strlen(fl_g->input_add_icy_name->value()) + 1);

    strcpy(cfg.icy[icy_num]->name, fl_g->input_add_icy_name->value());

    // rewrite the string that contains all server names
    // first get the needed memory space
    for (int i = 0; i < cfg.main.num_of_icy; i++)
        len += strlen(cfg.icy[i]->name) + 1;
    // reserve enough memory
    cfg.main.icy_ent = (char *)realloc(cfg.main.icy_ent, sizeof(char) * len + 1);

    memset(cfg.main.icy_ent, 0, len);
    // now append the server strings
    for (int i = 0; i < cfg.main.num_of_icy; i++) {
        strcat(cfg.main.icy_ent, cfg.icy[i]->name);
        if (i < cfg.main.num_of_icy - 1) {
            strcat(cfg.main.icy_ent, ";");
        }
    }

    cfg.icy[icy_num]->desc = (char *)realloc(cfg.icy[icy_num]->desc, strlen(fl_g->input_add_icy_desc->value()) + 1);
    strcpy(cfg.icy[icy_num]->desc, fl_g->input_add_icy_desc->value());

    cfg.icy[icy_num]->genre = (char *)realloc(cfg.icy[icy_num]->genre, strlen(fl_g->input_add_icy_genre->value()) + 1);
    strcpy(cfg.icy[icy_num]->genre, fl_g->input_add_icy_genre->value());

    cfg.icy[icy_num]->url = (char *)realloc(cfg.icy[icy_num]->url, strlen(fl_g->input_add_icy_url->value()) + 1);
    strcpy(cfg.icy[icy_num]->url, fl_g->input_add_icy_url->value());

    cfg.icy[icy_num]->icq = (char *)realloc(cfg.icy[icy_num]->icq, strlen(fl_g->input_add_icy_icq->value()) + 1);
    strcpy(cfg.icy[icy_num]->icq, fl_g->input_add_icy_icq->value());

    cfg.icy[icy_num]->irc = (char *)realloc(cfg.icy[icy_num]->irc, strlen(fl_g->input_add_icy_irc->value()) + 1);
    strcpy(cfg.icy[icy_num]->irc, fl_g->input_add_icy_irc->value());

    cfg.icy[icy_num]->aim = (char *)realloc(cfg.icy[icy_num]->aim, strlen(fl_g->input_add_icy_aim->value()) + 1);
    strcpy(cfg.icy[icy_num]->aim, fl_g->input_add_icy_aim->value());

    sprintf(cfg.icy[icy_num]->pub, "%d", fl_g->check_add_icy_pub->value());

    cfg.icy[icy_num]->expand_variables = fl_g->check_expand_variables->value();

    fl_g->input_add_icy_name->value("");
    fl_g->input_add_icy_desc->value("");
    fl_g->input_add_icy_url->value("");
    fl_g->input_add_icy_genre->value("");
    fl_g->input_add_icy_irc->value("");
    fl_g->input_add_icy_icq->value("");
    fl_g->input_add_icy_aim->value("");
    fl_g->check_add_icy_pub->value(0);
    fl_g->check_expand_variables->value(0);

    fl_g->window_add_icy->hide();

    fl_g->choice_cfg_act_icy->replace(icy_num, cfg.icy[icy_num]->name);
    fl_g->choice_cfg_act_icy->redraw();
    choice_cfg_act_icy_cb();
}

/*
void choice_cfg_edit_srv_cb(void)
{
    char dummy[10];
    int server = fl_g->choice_cfg_edit_srv->value();

    fl_g->input_cfg_addr->value(cfg.srv[server]->addr);

    snprintf(dummy, 6, "%u", cfg.srv[server]->port);
    fl_g->input_cfg_port->value(dummy);
    fl_g->input_cfg_passwd->value(cfg.srv[server]->pwd);

    if(cfg.srv[server]->type == SHOUTCAST) {
        fl_g->input_cfg_mount->value("");
        fl_g->input_cfg_mount->deactivate();
        fl_g->radio_cfg_shoutcast->value(1);
        fl_g->radio_cfg_icecast->value(0);
    }
    else //if(cfg.srv[server]->type == ICECAST)
    {
        fl_g->input_cfg_mount->value(cfg.srv[server]->mount);
        fl_g->input_cfg_mount->activate();
        fl_g->radio_cfg_icecast->value(1);
        fl_g->radio_cfg_shoutcast->value(0);
    }
}
*/

void choice_cfg_bitrate_cb(void)
{
    int rc;
    int old_br;
    int sel_br;
    int codec_idx;
    int num_of_bitrates;
    int *br_list;
    char text_buf[256];

    codec_idx = fl_g->choice_cfg_codec->value();
    num_of_bitrates = get_bitrate_list_for_codec(codec_idx, &br_list);

    old_br = cfg.audio.bitrate;
    for (int i = 0; i < num_of_bitrates; i++) {
        if (br_list[i] == cfg.audio.bitrate) {
            old_br = i;
        }
    }

    sel_br = fl_g->choice_cfg_bitrate->value();
    cfg.audio.bitrate = br_list[sel_br];
    lame_stream.bitrate = br_list[sel_br];
    vorbis_stream.bitrate = br_list[sel_br];
#ifdef HAVE_LIBFDK_AAC
    aac_stream.bitrate = br_list[sel_br];
#endif
    opus_stream.bitrate = br_list[sel_br] * 1000;

    if (codec_idx == CHOICE_MP3) {
        rc = lame_enc_reinit(&lame_stream);
        if (rc != 0) {
            print_info(_("Warning:\nThe stream Sample-/Bitrate combination is invalid"), 1);
            fl_g->choice_cfg_bitrate->value(old_br);
            fl_g->choice_cfg_bitrate->redraw();
            cfg.audio.bitrate = br_list[old_br];
            lame_stream.bitrate = br_list[old_br];
            lame_enc_reinit(&lame_stream);
            print_info(_("The previous values have been set\n"), 1);
            return;
        }
    }
    if (codec_idx == CHOICE_OGG) {
        rc = vorbis_enc_reinit(&vorbis_stream);
        if (rc != 0) {
            print_info(_("Warning:\nThe stream Sample-/Bitrate combination is invalid"), 1);
            cfg.audio.bitrate = br_list[old_br];
            vorbis_stream.bitrate = br_list[old_br];
            fl_g->choice_cfg_bitrate->value(old_br);
            fl_g->choice_cfg_bitrate->redraw();
            vorbis_enc_reinit(&vorbis_stream);
            print_info(_("The previous values have been set\n"), 1);
            return;
        }
    }

    if (codec_idx == CHOICE_OPUS) {
        rc = opus_enc_reinit(&opus_stream);
        if (rc != 0) {
            print_info(_("Warning:\nThe stream Sample-/Bitrate combination is invalid"), 1);
            cfg.audio.bitrate = br_list[old_br];
            opus_stream.bitrate = br_list[old_br] * 1000;
            fl_g->choice_cfg_bitrate->value(old_br);
            fl_g->choice_cfg_bitrate->redraw();
            opus_enc_reinit(&opus_stream);
            print_info(_("The previous values have been set\n"), 1);
            return;
        }
    }

#ifdef HAVE_LIBFDK_AAC
    if (codec_idx == CHOICE_AAC) {
        rc = aac_enc_reinit(&aac_stream);
        if (rc != 0) {
            print_info(_("Warning:\nThe stream Sample-/Bitrate combination is invalid"), 1);
            cfg.audio.bitrate = br_list[old_br];
            aac_stream.bitrate = br_list[old_br];
            fl_g->choice_cfg_bitrate->value(old_br);
            fl_g->choice_cfg_bitrate->redraw();
            aac_enc_reinit(&aac_stream);
            print_info(_("The previous values have been set\n"), 1);
            return;
        }
    }
#endif

    snprintf(text_buf, sizeof(text_buf), _("Stream bitrate set to: %dk"), cfg.audio.bitrate);
    print_info(text_buf, 0);
}

void button_stream_codec_settings_cb(void)
{
    fl_g->window_stream_codec_settings->position(fl_g->window_cfg->x(), fl_g->window_cfg->y());
    fl_g->window_stream_codec_settings->show();
}

void button_rec_codec_settings_cb(void)
{
    fl_g->window_rec_codec_settings->position(fl_g->window_cfg->x(), fl_g->window_cfg->y());
    fl_g->window_rec_codec_settings->show();
}

void choice_rec_bitrate_cb(void)
{
    int rc;
    int old_br;
    int sel_br;
    int codec_idx;
    int num_of_bitrates;
    int *br_list;
    char text_buf[256];

    codec_idx = fl_g->choice_rec_codec->value();
    num_of_bitrates = get_bitrate_list_for_codec(codec_idx, &br_list);

    old_br = cfg.rec.bitrate;
    for (int i = 0; i < num_of_bitrates; i++) {
        if (br_list[i] == cfg.rec.bitrate) {
            old_br = i;
        }
    }

    sel_br = fl_g->choice_rec_bitrate->value();
    cfg.rec.bitrate = br_list[sel_br];
    lame_rec.bitrate = br_list[sel_br];
    vorbis_rec.bitrate = br_list[sel_br];
    opus_rec.bitrate = br_list[sel_br] * 1000;
#ifdef HAVE_LIBFDK_AAC
    aac_rec.bitrate = br_list[sel_br];
#endif

    if (fl_g->choice_rec_codec->value() == CHOICE_MP3) {
        rc = lame_enc_reinit(&lame_rec);
        if (rc != 0) {
            print_info(_("Warning:\nThe record Sample-/Bitrate combination is invalid"), 1);
            cfg.rec.bitrate = br_list[old_br];
            fl_g->choice_rec_bitrate->value(old_br);
            fl_g->choice_rec_bitrate->redraw();
            lame_rec.bitrate = br_list[old_br];
            lame_enc_reinit(&lame_rec);
            print_info(_("The previous values have been set"), 1);
            return;
        }
    }
    if (fl_g->choice_rec_codec->value() == CHOICE_OGG) {
        rc = vorbis_enc_reinit(&vorbis_rec);
        if (rc != 0) {
            print_info(_("Warning:\nThe record Sample-/Bitrate combination is invalid"), 1);
            cfg.rec.bitrate = br_list[old_br];
            vorbis_rec.bitrate = br_list[old_br];
            fl_g->choice_rec_bitrate->value(old_br);
            fl_g->choice_rec_bitrate->redraw();
            vorbis_enc_reinit(&vorbis_rec);
            print_info(_("The previous values have been set"), 1);
            return;
        }
    }

    if (fl_g->choice_rec_codec->value() == CHOICE_OPUS) {
        rc = opus_enc_reinit(&opus_rec);
        if (rc != 0) {
            print_info(_("Warning:\nThe record Sample-/Bitrate combination is invalid"), 1);
            cfg.rec.bitrate = br_list[old_br];
            opus_rec.bitrate = br_list[old_br] * 1000;
            fl_g->choice_rec_bitrate->value(old_br);
            fl_g->choice_rec_bitrate->redraw();
            opus_enc_reinit(&opus_rec);
            print_info(_("The previous values have been set"), 1);
            return;
        }
    }

#ifdef HAVE_LIBFDK_AAC
    if (fl_g->choice_rec_codec->value() == CHOICE_AAC) {
        rc = aac_enc_reinit(&aac_rec);
        if (rc != 0) {
            print_info(_("Warning:\nThe record Sample-/Bitrate combination is invalid"), 1);
            cfg.rec.bitrate = br_list[old_br];
            aac_rec.bitrate = br_list[old_br];
            fl_g->choice_rec_bitrate->value(old_br);
            fl_g->choice_rec_bitrate->redraw();
            aac_enc_reinit(&aac_rec);
            print_info(_("The previous values have been set"), 1);
            return;
        }
    }
#endif

    snprintf(text_buf, sizeof(text_buf), _("Record bitrate set to: %dk"), cfg.rec.bitrate);
    print_info(text_buf, 0);
}

void choice_cfg_samplerate_cb()
{
    int rc;
    int old_sr;
    int sel_sr;
    int *sr_list;
    char text_buf[256];

    sr_list = cfg.audio.pcm_list[cfg.audio.dev_num]->sr_list;
    old_sr = cfg.audio.samplerate;

    for (int i = 0; sr_list[i] != 0; i++) {
        if (sr_list[i] == cfg.audio.samplerate) {
            old_sr = i;
        }
    }

    sel_sr = fl_g->choice_cfg_samplerate->value();

    cfg.audio.samplerate = sr_list[sel_sr];

    // Reinit streaming codecs
    lame_stream.samplerate = sr_list[sel_sr];
    vorbis_stream.samplerate = sr_list[sel_sr];
    opus_stream.samplerate = sr_list[sel_sr];
#ifdef HAVE_LIBFDK_AAC
    aac_stream.samplerate = sr_list[sel_sr];
#endif
    flac_stream.samplerate = sr_list[sel_sr];

    if (fl_g->choice_cfg_codec->value() == CHOICE_MP3) {
        rc = lame_enc_reinit(&lame_stream);
        if (rc != 0) {
            print_info(_("Warning:\nThe stream Sample-/Bitrate combination is invalid"), 1);
            fl_g->choice_cfg_samplerate->value(old_sr);
            fl_g->choice_cfg_samplerate->redraw();
            cfg.audio.samplerate = sr_list[old_sr];
            lame_stream.samplerate = sr_list[old_sr];
            lame_enc_reinit(&lame_stream);
            print_info(_("The previous values have been set"), 1);
            return;
        }
    }
    if (fl_g->choice_cfg_codec->value() == CHOICE_OGG) {
        rc = vorbis_enc_reinit(&vorbis_stream);
        if (rc != 0) {
            print_info(_("Warning:\nThe stream Sample-/Bitrate combination is invalid"), 1);
            cfg.audio.samplerate = sr_list[old_sr];
            vorbis_stream.samplerate = sr_list[old_sr];
            fl_g->choice_cfg_samplerate->value(old_sr);
            fl_g->choice_cfg_samplerate->redraw();
            vorbis_enc_reinit(&vorbis_stream);
            print_info(_("The previous values have been set"), 1);
            return;
        }
    }

    if (fl_g->choice_cfg_codec->value() == CHOICE_OPUS) {
        rc = opus_enc_reinit(&opus_stream);
        if (rc != 0) {
            print_info(_("Warning:\nThe stream Sample-/Bitrate combination is invalid"), 1);
            cfg.audio.samplerate = sr_list[old_sr];
            opus_stream.samplerate = sr_list[old_sr];
            fl_g->choice_cfg_samplerate->value(old_sr);
            fl_g->choice_cfg_samplerate->redraw();
            opus_enc_reinit(&opus_stream);
            print_info(_("The previous values have been set"), 1);
            return;
        }
    }

#ifdef HAVE_LIBFDK_AAC
    if (fl_g->choice_cfg_codec->value() == CHOICE_AAC) {
        rc = aac_enc_reinit(&aac_stream);
        if (rc != 0) {
            print_info(_("Warning:\nThe stream Sample-/Bitrate combination is invalid"), 1);
            cfg.audio.samplerate = sr_list[old_sr];
            aac_stream.samplerate = sr_list[old_sr];
            fl_g->choice_cfg_samplerate->value(old_sr);
            fl_g->choice_cfg_samplerate->redraw();
            aac_enc_reinit(&aac_stream);
            print_info(_("The previous values have been set"), 1);
            return;
        }
    }
#endif

    if (fl_g->choice_cfg_codec->value() == CHOICE_FLAC) {
        rc = flac_enc_reinit(&flac_stream);
        if (rc != 0) {
            print_info(_("Warning:\nThe stream Sample-/Bitrate combination is invalid"), 1);
            cfg.audio.samplerate = sr_list[old_sr];
            flac_stream.samplerate = sr_list[old_sr];
            fl_g->choice_cfg_samplerate->value(old_sr);
            fl_g->choice_cfg_samplerate->redraw();
            flac_enc_reinit(&flac_stream);
            print_info(_("The previous values have been set"), 1);
            return;
        }
    }

    // Reinit record codecs
    lame_rec.samplerate = sr_list[sel_sr];
    vorbis_rec.samplerate = sr_list[sel_sr];
    opus_rec.samplerate = sr_list[sel_sr];
#ifdef HAVE_LIBFDK_AAC
    aac_rec.samplerate = sr_list[sel_sr];
#endif
    flac_rec.samplerate = sr_list[sel_sr];

    if (fl_g->choice_rec_codec->value() == CHOICE_MP3) {
        rc = lame_enc_reinit(&lame_rec);
        if (rc != 0) {
            print_info(_("Warning:\nThe record Sample-/Bitrate combination is invalid"), 1);
            cfg.audio.samplerate = sr_list[old_sr];
            lame_rec.samplerate = sr_list[old_sr];
            fl_g->choice_cfg_samplerate->value(old_sr);
            fl_g->choice_cfg_samplerate->redraw();
            lame_enc_reinit(&lame_rec);
            print_info(_("The previous values have been set"), 1);
            return;
        }
    }
    if (fl_g->choice_rec_codec->value() == CHOICE_OGG) {
        rc = vorbis_enc_reinit(&vorbis_rec);
        if (rc != 0) {
            print_info(_("Warning:\nThe record Sample-/Bitrate combination is invalid"), 1);
            cfg.audio.samplerate = sr_list[old_sr];
            vorbis_rec.samplerate = sr_list[old_sr];
            fl_g->choice_cfg_samplerate->value(old_sr);
            fl_g->choice_cfg_samplerate->redraw();
            vorbis_enc_reinit(&vorbis_rec);
            print_info(_("The previous values have been set"), 1);
            return;
        }
    }

    if (fl_g->choice_rec_codec->value() == CHOICE_OPUS) {
        rc = opus_enc_reinit(&opus_rec);
        if (rc != 0) {
            print_info(_("Warning:\nThe record Sample-/Bitrate combination is invalid"), 1);
            cfg.audio.samplerate = sr_list[old_sr];
            opus_rec.samplerate = sr_list[old_sr];
            fl_g->choice_cfg_samplerate->value(old_sr);
            fl_g->choice_cfg_samplerate->redraw();
            opus_enc_reinit(&opus_rec);
            print_info(_("The previous values have been set"), 1);
            return;
        }
    }

#ifdef HAVE_LIBFDK_AAC
    if (fl_g->choice_rec_codec->value() == CHOICE_AAC) {
        rc = aac_enc_reinit(&aac_rec);
        if (rc != 0) {
            print_info(_("Warning:\nThe record Sample-/Bitrate combination is invalid"), 1);
            cfg.audio.samplerate = sr_list[old_sr];
            aac_rec.samplerate = sr_list[old_sr];
            fl_g->choice_cfg_samplerate->value(old_sr);
            fl_g->choice_cfg_samplerate->redraw();
            aac_enc_reinit(&aac_rec);
            print_info(_("The previous values have been set"), 1);
            return;
        }
    }
#endif
    if (fl_g->choice_rec_codec->value() == CHOICE_FLAC) {
        rc = flac_enc_reinit(&flac_rec);
        if (rc != 0) {
            print_info(_("Warning:\nThe record Sample-/Bitrate combination is invalid"), 1);
            cfg.audio.samplerate = sr_list[old_sr];
            flac_rec.samplerate = sr_list[old_sr];
            fl_g->choice_cfg_samplerate->value(old_sr);
            fl_g->choice_cfg_samplerate->redraw();
            flac_enc_reinit(&flac_rec);
            print_info(_("The previous values have been set"), 1);
            return;
        }
    }

    // The buffer size is dependand on the samplerate
    input_cfg_buffer_cb(0);

    snprintf(text_buf, sizeof(text_buf), _("Samplerate set to: %dHz"), cfg.audio.samplerate);
    print_info(text_buf, 0);

    snd_reopen_streams();
}

void choice_cfg_channel_stereo_cb(void)
{
    cfg.audio.channel = 2;

    // Reinit streaming codecs
    lame_stream.channel = 2;
    vorbis_stream.channel = 2;
    opus_stream.channel = 2;
#ifdef HAVE_LIBFDK_AAC
    aac_stream.channel = 2;
#endif
    flac_stream.channel = 2;

    if (fl_g->choice_cfg_codec->value() == CHOICE_MP3) {
        lame_enc_reinit(&lame_stream);
    }
    if (fl_g->choice_cfg_codec->value() == CHOICE_OGG) {
        vorbis_enc_reinit(&vorbis_stream);
    }
    if (fl_g->choice_cfg_codec->value() == CHOICE_OPUS) {
        opus_enc_reinit(&opus_stream);
    }
#ifdef HAVE_LIBFDK_AAC
    if (fl_g->choice_cfg_codec->value() == CHOICE_AAC) {
        aac_enc_reinit(&aac_stream);
    }
#endif
    if (fl_g->choice_cfg_codec->value() == CHOICE_FLAC) {
        flac_enc_reinit(&flac_stream);
    }

    // Reinit recording codecs
    lame_rec.channel = 2;
    vorbis_rec.channel = 2;
    opus_rec.channel = 2;
#ifdef HAVE_LIBFDK_AAC
    aac_rec.channel = 2;
#endif
    flac_rec.channel = 2;

    if (fl_g->choice_rec_codec->value() == CHOICE_MP3) {
        lame_enc_reinit(&lame_rec);
    }
    if (fl_g->choice_rec_codec->value() == CHOICE_OGG) {
        vorbis_enc_reinit(&vorbis_rec);
    }
    if (fl_g->choice_rec_codec->value() == CHOICE_OPUS) {
        opus_enc_reinit(&opus_rec);
    }
#ifdef HAVE_LIBFDK_AAC
    if (fl_g->choice_rec_codec->value() == CHOICE_AAC) {
        aac_enc_reinit(&aac_rec);
    }
#endif
    if (fl_g->choice_rec_codec->value() == CHOICE_FLAC) {
        flac_enc_reinit(&flac_rec);
    }

    snd_reopen_streams();

    print_info(_("Channels set to: stereo"), 0);
}

void choice_cfg_channel_mono_cb(void)
{
    cfg.audio.channel = 1;

    // Reinit streaming codecs
    lame_stream.channel = 1;
    vorbis_stream.channel = 1;
    opus_stream.channel = 1;
#ifdef HAVE_LIBFDK_AAC
    aac_stream.channel = 1;
#endif
    flac_stream.channel = 1;

    if (fl_g->choice_cfg_codec->value() == CHOICE_MP3) {
        lame_enc_reinit(&lame_stream);
    }
    if (fl_g->choice_cfg_codec->value() == CHOICE_OGG) {
        vorbis_enc_reinit(&vorbis_stream);
    }
    if (fl_g->choice_cfg_codec->value() == CHOICE_OPUS) {
        opus_enc_reinit(&opus_stream);
    }
#ifdef HAVE_LIBFDK_AAC
    if (fl_g->choice_cfg_codec->value() == CHOICE_AAC) {
        aac_enc_reinit(&aac_stream);
    }
#endif
    if (fl_g->choice_cfg_codec->value() == CHOICE_FLAC) {
        flac_enc_reinit(&flac_stream);
    }

    // Reinit recording codecs
    lame_rec.channel = 1;
    vorbis_rec.channel = 1;
    opus_rec.channel = 1;
#ifdef HAVE_LIBFDK_AAC
    aac_rec.channel = 1;
#endif
    flac_rec.channel = 1;

    if (fl_g->choice_rec_codec->value() == CHOICE_MP3) {
        lame_enc_reinit(&lame_rec);
    }
    if (fl_g->choice_rec_codec->value() == CHOICE_OGG) {
        vorbis_enc_reinit(&vorbis_rec);
    }
    if (fl_g->choice_rec_codec->value() == CHOICE_OPUS) {
        opus_enc_reinit(&opus_rec);
    }
#ifdef HAVE_LIBFDK_AAC
    if (fl_g->choice_rec_codec->value() == CHOICE_AAC) {
        aac_enc_reinit(&aac_rec);
    }
#endif
    if (fl_g->choice_rec_codec->value() == CHOICE_FLAC) {
        flac_enc_reinit(&flac_rec);
    }

    snd_reopen_streams();

    print_info(_("Channels set to: mono"), 0);
}

void button_add_srv_cancel_cb(void)
{
#ifdef WITH_RADIOCO
    if (radioco_get_state() == RADIOCO_STATE_WAITING) {
        radioco_cancel();
    }
#endif

    fl_g->input_add_srv_name->value("");
    fl_g->input_add_srv_addr->value("");
    fl_g->input_add_srv_port->value("");
    fl_g->input_add_srv_pwd->value("");
    fl_g->input_add_srv_mount->value("");
    fl_g->input_add_srv_usr->value("");
    fl_g->check_add_srv_tls->value(0);
    fl_g->check_add_srv_protocol->value(ICECAST_PROTOCOL_PUT);
    fl_g->browser_add_srv_station_list->clear();
    fl_g->window_add_srv->hide();
}

void button_add_icy_add_cb(void)
{
    int i;
    // error checking
    if (strlen(fl_g->input_add_icy_name->value()) == 0) {
        fl_alert(_("No name specified"));
        return;
    }

    if (cfg.main.icy_ent != NULL) {
        if (strlen(fl_g->input_add_icy_name->value()) + strlen(cfg.main.icy_ent) > 1000) {
            fl_alert(_("The number of characters of all your icy names exeeds 1000\n"
                       "Please reduce the number of characters of each icy name"));
            return;
        }
    }
    if (strpbrk(fl_g->input_add_icy_name->value(), "[];\\/\n\r") != NULL) {
        fl_alert(_("Newline characters and [];/\\ are not allowed within the icy name"));
        return;
    }

    // check if the name already exists
    for (i = 0; i < cfg.main.num_of_icy; i++) {
        if (!strcmp(fl_g->input_add_icy_name->value(), cfg.icy[i]->name)) {
            fl_alert(_("Server name already exist!"));
            return;
        }
    }

    i = cfg.main.num_of_icy;
    cfg.main.num_of_icy++;

    cfg.icy = (icy_t **)realloc(cfg.icy, cfg.main.num_of_icy * sizeof(icy_t *));
    cfg.icy[i] = (icy_t *)malloc(sizeof(icy_t));

    cfg.icy[i]->name = (char *)malloc(strlen(fl_g->input_add_icy_name->value()) + 1);
    strcpy(cfg.icy[i]->name, fl_g->input_add_icy_name->value());

    cfg.icy[i]->desc = (char *)malloc(strlen(fl_g->input_add_icy_desc->value()) + 1);
    strcpy(cfg.icy[i]->desc, fl_g->input_add_icy_desc->value());

    cfg.icy[i]->url = (char *)malloc(strlen(fl_g->input_add_icy_url->value()) + 1);
    strcpy(cfg.icy[i]->url, fl_g->input_add_icy_url->value());

    cfg.icy[i]->genre = (char *)malloc(strlen(fl_g->input_add_icy_genre->value()) + 1);
    strcpy(cfg.icy[i]->genre, fl_g->input_add_icy_genre->value());

    cfg.icy[i]->irc = (char *)malloc(strlen(fl_g->input_add_icy_irc->value()) + 1);
    strcpy(cfg.icy[i]->irc, fl_g->input_add_icy_irc->value());

    cfg.icy[i]->icq = (char *)malloc(strlen(fl_g->input_add_icy_icq->value()) + 1);
    strcpy(cfg.icy[i]->icq, fl_g->input_add_icy_icq->value());

    cfg.icy[i]->aim = (char *)malloc(strlen(fl_g->input_add_icy_aim->value()) + 1);
    strcpy(cfg.icy[i]->aim, fl_g->input_add_icy_aim->value());

    cfg.icy[i]->pub = (char *)malloc(16 * sizeof(char));
    snprintf(cfg.icy[i]->pub, 15, "%d", fl_g->check_add_icy_pub->value());

    if (cfg.main.num_of_icy > 1) {
        cfg.main.icy_ent = (char *)realloc(cfg.main.icy_ent, strlen(cfg.main.icy_ent) + strlen(cfg.icy[i]->name) + 2);
        sprintf(cfg.main.icy_ent + strlen(cfg.main.icy_ent), ";%s", cfg.icy[i]->name);
        cfg.main.icy = (char *)realloc(cfg.main.icy, strlen(cfg.icy[i]->name) + 1);
    }
    else {
        cfg.main.icy_ent = (char *)malloc(strlen(cfg.icy[i]->name) + 1);
        sprintf(cfg.main.icy_ent, "%s", cfg.icy[i]->name);
        cfg.main.icy = (char *)malloc(strlen(cfg.icy[i]->name) + 1);
    }
    strcpy(cfg.main.icy, cfg.icy[i]->name);

    fl_g->input_add_icy_name->value("");
    fl_g->input_add_icy_desc->value("");
    fl_g->input_add_icy_url->value("");
    fl_g->input_add_icy_genre->value("");
    fl_g->input_add_icy_irc->value("");
    fl_g->input_add_icy_icq->value("");
    fl_g->input_add_icy_aim->value("");
    fl_g->check_add_icy_pub->value(0);
    fl_g->check_expand_variables->value(0);

    fl_g->window_add_icy->hide();

    fl_g->choice_cfg_act_icy->add(cfg.icy[i]->name);
    fl_g->choice_cfg_act_icy->redraw();

    fl_g->button_cfg_edit_icy->activate();
    fl_g->button_cfg_del_icy->activate();

    fl_g->choice_cfg_act_icy->activate();

    // make added icy data the active icy entry
    fl_g->choice_cfg_act_icy->value(i);
    choice_cfg_act_icy_cb();
}

void button_add_icy_cancel_cb(void)
{
    fl_g->input_add_icy_name->value("");
    fl_g->input_add_icy_desc->value("");
    fl_g->input_add_icy_url->value("");
    fl_g->input_add_icy_genre->value("");
    fl_g->input_add_icy_irc->value("");
    fl_g->input_add_icy_icq->value("");
    fl_g->input_add_icy_aim->value("");
    fl_g->check_add_icy_pub->value(0);
    fl_g->window_add_icy->hide();
}

void button_cfg_edit_icy_cb(void)
{
    if (cfg.main.num_of_icy < 1) {
        return;
    }

    int icy = fl_g->choice_cfg_act_icy->value();

    fl_g->window_add_icy->label(_("Edit Server Infos"));

    fl_g->button_add_icy_add->hide();
    fl_g->button_add_icy_save->show();

    fl_g->input_add_icy_name->value(cfg.icy[icy]->name);
    fl_g->input_add_icy_desc->value(cfg.icy[icy]->desc);
    fl_g->input_add_icy_genre->value(cfg.icy[icy]->genre);
    fl_g->input_add_icy_url->value(cfg.icy[icy]->url);
    fl_g->input_add_icy_irc->value(cfg.icy[icy]->irc);
    fl_g->input_add_icy_icq->value(cfg.icy[icy]->icq);
    fl_g->input_add_icy_aim->value(cfg.icy[icy]->aim);

    if (!strcmp(cfg.icy[icy]->pub, "1")) {
        fl_g->check_add_icy_pub->value(1);
    }
    else {
        fl_g->check_add_icy_pub->value(0);
    }

    fl_g->check_expand_variables->value(cfg.icy[icy]->expand_variables);

    fl_g->window_add_icy->position(fl_g->window_cfg->x(), fl_g->window_cfg->y());

    // give the "name" input field the input focus
    fl_g->input_add_icy_name->take_focus();
    fl_g->window_add_icy->show();
}

void choice_cfg_dev_cb(void)
{
    char info_buf[256];
    int new_dev_num = fl_g->choice_cfg_dev->value();
    int prev_dev_num = cfg.audio.dev_num;

    if (cfg.audio.dev2_num >= 0 && cfg.audio.pcm_list[cfg.audio.dev2_num]->is_asio && cfg.audio.pcm_list[new_dev_num]->is_asio) {
        fl_alert(_(
            "Primary and secondary audio device are both ASIO devices.\nYou can not use two ASIO devices at the same time.\nPlease select a different device.\n"));
        // Fall back to previous selected secondary device
        fl_g->choice_cfg_dev->value(prev_dev_num);
        return;
    }

    // save current device name to config struct
    cfg.audio.dev_name = (char *)realloc(cfg.audio.dev_name, strlen(cfg.audio.pcm_list[new_dev_num]->name) + 1);
    strcpy(cfg.audio.dev_name, cfg.audio.pcm_list[new_dev_num]->name);

    // In case the previous selected audio device had only 1 input channel but the new selected audio device has more than 1,
    // select channel 1 and 2 by default for the new device instead of 1 and 1 to prevent that the user unintentionally streams the
    // left channel in both stereo channels
    if ((cfg.audio.pcm_list[new_dev_num]->num_of_channels > 1) && (cfg.audio.left_ch == 1) && cfg.audio.right_ch == 1) {
        cfg.audio.left_ch = 1;
        cfg.audio.right_ch = 2;
    }

    cfg.audio.dev_num = new_dev_num;
    update_samplerates_list();
    if (snd_reopen_streams() != 0) {
        fl_alert(_("butt could not open selected audio device.\nPlease try another device.\n"));

        // Fall back to previous selected primary device
        fl_g->choice_cfg_dev2->value(prev_dev_num);
        cfg.audio.dev_num = prev_dev_num;
        cfg.audio.dev_name = (char *)realloc(cfg.audio.dev_name, strlen(fl_g->choice_cfg_dev->text()) + 1);
        strcpy(cfg.audio.dev_name, fl_g->choice_cfg_dev->text());
        update_samplerates_list();
        if (snd_reopen_streams() != 0) {
            return;
        }
    }

    update_channel_lists();

    snprintf(info_buf, sizeof(info_buf), _("Primary device:\n%s\n"), cfg.audio.dev_name);
    print_info(info_buf, 1);
}

void choice_cfg_dev2_cb(void)
{
    char info_buf[256];
    int new_dev2_num = fl_g->choice_cfg_dev2->value() - 1;
    int prev_dev2_num = cfg.audio.dev2_num;

    if (new_dev2_num == -1) {
        cfg.audio.dev2_name = (char *)realloc(cfg.audio.dev2_name, strlen(fl_g->choice_cfg_dev2->text()) + 1);
        strcpy(cfg.audio.dev2_name, fl_g->choice_cfg_dev2->text());
    }
    else {
        if (cfg.audio.pcm_list[new_dev2_num]->is_asio && cfg.audio.pcm_list[cfg.audio.dev_num]->is_asio) {
            fl_alert(_(
                "Primary and secondary audio device are both ASIO devices.\nYou can not use two ASIO devices at the same time.\nPlease select a different device.\n"));
            // Fall back to previous selected secondary device
            fl_g->choice_cfg_dev2->value(prev_dev2_num + 1);
            return;
        }

        cfg.audio.dev2_name = (char *)realloc(cfg.audio.dev2_name, strlen(cfg.audio.pcm_list[new_dev2_num]->name) + 1);
        strcpy(cfg.audio.dev2_name, cfg.audio.pcm_list[new_dev2_num]->name);

        // In case the previous selected audio device had only 1 input channel but the new selected audio device has more than 1,
        // select channel 1 and 2 by default for the new device instead of 1 and 1 to prevent that the user unintentionally streams the
        // left channel in both stereo channels
        if ((cfg.audio.pcm_list[new_dev2_num]->num_of_channels > 1) && (cfg.audio.left_ch2 == 1) && cfg.audio.right_ch2 == 1) {
            cfg.audio.left_ch2 = 1;
            cfg.audio.right_ch2 = 2;
        }
    }

    cfg.audio.dev2_num = new_dev2_num;
    if (snd_reopen_streams() != 0) {
        fl_alert(_("butt could not open secondary audio device.\nPlease try another device.\n"));

        // Fall back to previous selected secondary device
        fl_g->choice_cfg_dev2->value(prev_dev2_num + 1);
        cfg.audio.dev2_num = prev_dev2_num;
        cfg.audio.dev2_name = (char *)realloc(cfg.audio.dev2_name, strlen(fl_g->choice_cfg_dev2->text()) + 1);
        strcpy(cfg.audio.dev2_name, fl_g->choice_cfg_dev2->text());
        if (snd_reopen_streams() != 0) {
            return;
        }
    }

    update_channel_lists();

    snprintf(info_buf, sizeof(info_buf), _("Secondary device:\n%s\n"), cfg.audio.dev2_name);
    print_info(info_buf, 1);
}

void button_cfg_rescan_devices_cb(void)
{
    if (connected || recording) {
        return;
    }

    fl_g->choice_cfg_dev->deactivate();
    fl_g->choice_cfg_dev2->deactivate();
    fl_g->button_cfg_rescan_devices->deactivate();
    Fl::check(); // Force UI update

    int dev_count;
    char *current_device = strdup(cfg.audio.pcm_list[cfg.audio.dev_num]->name);
    char *current_device2 = NULL;
    if (cfg.audio.dev2_num >= 0) {
        current_device2 = strdup(cfg.audio.pcm_list[cfg.audio.dev2_num]->name);
    }

    snd_close_streams();

    snd_free_device_list(cfg.audio.pcm_list, cfg.audio.dev_count);
    cfg.audio.pcm_list = snd_get_devices(&dev_count);
    cfg.audio.dev_count = dev_count;

    fl_g->choice_cfg_dev->clear();
    fl_g->choice_cfg_dev2->clear();
    fl_g->choice_cfg_dev->textsize(14);
    fl_g->choice_cfg_dev2->textsize(14);

    fl_g->choice_cfg_dev2->add(_("None"));
    for (int i = 0; i < cfg.audio.dev_count; i++) {
        unsigned long dev_name_len = strlen(cfg.audio.pcm_list[i]->name) + 10;
        char *dev_name = (char *)malloc(dev_name_len);

        snprintf(dev_name, dev_name_len, "%d: %s", i, cfg.audio.pcm_list[i]->name);
        fl_g->choice_cfg_dev->add(dev_name);
        fl_g->choice_cfg_dev2->add(dev_name);
        free(dev_name);
    }

    cfg.audio.dev_num = snd_get_dev_num_by_name(current_device);
    free(current_device);

    if (current_device2 != NULL) {
        cfg.audio.dev2_num = snd_get_dev_num_by_name(current_device2);
        free(current_device2);
    }

    fl_g->choice_cfg_dev->value(cfg.audio.dev_num);
    fl_g->choice_cfg_dev2->value(cfg.audio.dev2_num + 1);
    fl_g->choice_cfg_dev->take_focus();

    snd_open_streams();

    fl_g->choice_cfg_dev->activate();
    fl_g->choice_cfg_dev2->activate();
    fl_g->button_cfg_rescan_devices->activate();
}

void radio_cfg_ID_cb(void)
{
    cfg.audio.dev_remember = REMEMBER_BY_ID;
}

void radio_cfg_name_cb(void)
{
    cfg.audio.dev_remember = REMEMBER_BY_NAME;
    // save current device names to config struct
    cfg.audio.dev_name = (char *)realloc(cfg.audio.dev_name, strlen(cfg.audio.pcm_list[cfg.audio.dev_num]->name) + 1);
    strcpy(cfg.audio.dev_name, cfg.audio.pcm_list[cfg.audio.dev_num]->name);

    if (cfg.audio.dev2_num >= 0) {
        cfg.audio.dev2_name = (char *)realloc(cfg.audio.dev2_name, strlen(cfg.audio.pcm_list[cfg.audio.dev2_num]->name) + 1);
        strcpy(cfg.audio.dev2_name, cfg.audio.pcm_list[cfg.audio.dev2_num]->name);
    }
}

void choice_cfg_left_channel_cb(void)
{
    cfg.audio.left_ch = fl_g->choice_cfg_left_channel->value() + 1;
}

void choice_cfg_right_channel_cb(void)
{
    cfg.audio.right_ch = fl_g->choice_cfg_right_channel->value() + 1;
}

void choice_cfg_left_channel2_cb(void)
{
    cfg.audio.left_ch2 = fl_g->choice_cfg_left_channel2->value() + 1;
}

void choice_cfg_right_channel2_cb(void)
{
    cfg.audio.right_ch2 = fl_g->choice_cfg_right_channel2->value() + 1;
}

void choice_cfg_codec_mp3_cb(void)
{
    update_stream_bitrate_list(CHOICE_MP3);
    lame_stream.bitrate = cfg.audio.bitrate;

    if (lame_enc_reinit(&lame_stream) != 0) {
        print_info(_("MP3 encoder doesn't support current\n"
                     "Sample-/Bitrate combination"),
                   1);

        if (!strcmp(cfg.audio.codec, "ogg")) {
            fl_g->choice_cfg_codec->value(CHOICE_OGG);
        }
        else if (!strcmp(cfg.audio.codec, "opus")) {
            fl_g->choice_cfg_codec->value(CHOICE_OPUS);
        }
        else if (!strcmp(cfg.audio.codec, "aac")) {
            fl_g->choice_cfg_codec->value(CHOICE_AAC);
        }
        else if (!strcmp(cfg.audio.codec, "flac")) {
            fl_g->choice_cfg_codec->value(CHOICE_FLAC);
        }

        return;
    }

    strcpy(cfg.audio.codec, "mp3");
    print_info(_("Stream codec set to mp3"), 0);

    fl_g->choice_cfg_bitrate->activate();
    fl_g->choice_cfg_bitrate->show();
}

void choice_cfg_codec_ogg_cb(void)
{
    update_stream_bitrate_list(CHOICE_OGG);
    vorbis_stream.bitrate = cfg.audio.bitrate;

    if (vorbis_enc_reinit(&vorbis_stream) != 0) {
        print_info(_("OGG Vorbis encoder doesn't support current\n"
                     "Sample-/Bitrate combination"),
                   1);

        if (!strcmp(cfg.audio.codec, "mp3")) {
            fl_g->choice_cfg_codec->value(CHOICE_MP3);
        }
        else if (!strcmp(cfg.audio.codec, "opus")) {
            fl_g->choice_cfg_codec->value(CHOICE_OPUS);
        }
        else if (!strcmp(cfg.audio.codec, "aac")) {
            fl_g->choice_cfg_codec->value(CHOICE_AAC);
        }
        else if (!strcmp(cfg.audio.codec, "flac")) {
            fl_g->choice_cfg_codec->value(CHOICE_FLAC);
        }

        return;
    }

    strcpy(cfg.audio.codec, "ogg");
    print_info(_("Stream codec set to ogg/vorbis"), 0);
    fl_g->choice_cfg_bitrate->activate();
    fl_g->choice_cfg_bitrate->show();
}

void choice_cfg_codec_opus_cb(void)
{
    update_stream_bitrate_list(CHOICE_OPUS);
    printf("cfg.audio.bitrate: %d\n", cfg.audio.bitrate);
    opus_stream.bitrate = cfg.audio.bitrate * 1000;

    if (opus_enc_reinit(&opus_stream) != 0) {
        print_info(_("Opus encoder doesn't support current\n"
                     "Sample-/Bitrate combination"),
                   1);

        if (!strcmp(cfg.audio.codec, "mp3")) {
            fl_g->choice_cfg_codec->value(CHOICE_MP3);
        }
        else if (!strcmp(cfg.audio.codec, "ogg")) {
            fl_g->choice_cfg_codec->value(CHOICE_OGG);
        }
        else if (!strcmp(cfg.audio.codec, "aac")) {
            fl_g->choice_cfg_codec->value(CHOICE_AAC);
        }
        else if (!strcmp(cfg.audio.codec, "flac")) {
            fl_g->choice_cfg_codec->value(CHOICE_FLAC);
        }

        return;
    }

    print_info(_("Stream codec set to opus"), 0);
    strcpy(cfg.audio.codec, "opus");
    fl_g->choice_cfg_bitrate->activate();
    fl_g->choice_cfg_bitrate->show();
}

void choice_cfg_codec_aac_cb(void)
{
#ifdef HAVE_LIBFDK_AAC
    if (g_aac_lib_available == 0) {
        fl_alert(_("Could not find aac library.\nPlease follow the instructions in the manual for adding aac support."));
        if (!strcmp(cfg.audio.codec, "ogg")) {
            fl_g->choice_cfg_codec->value(CHOICE_OGG);
        }
        else if (!strcmp(cfg.audio.codec, "opus")) {
            fl_g->choice_cfg_codec->value(CHOICE_OPUS);
        }
        else if (!strcmp(cfg.audio.codec, "mp3")) {
            fl_g->choice_cfg_codec->value(CHOICE_MP3);
        }
        else if (!strcmp(cfg.audio.codec, "flac")) {
            fl_g->choice_cfg_codec->value(CHOICE_FLAC);
        }

        return;
    }

    update_stream_bitrate_list(CHOICE_AAC);
    aac_stream.bitrate = cfg.audio.bitrate;

    if (aac_enc_reinit(&aac_stream) != 0) {
        print_info(_("AAC encoder doesn't support current\n"
                     "Sample-/Bitrate combination"),
                   1);

        if (!strcmp(cfg.audio.codec, "ogg")) {
            fl_g->choice_cfg_codec->value(CHOICE_OGG);
        }
        else if (!strcmp(cfg.audio.codec, "opus")) {
            fl_g->choice_cfg_codec->value(CHOICE_OPUS);
        }
        else if (!strcmp(cfg.audio.codec, "mp3")) {
            fl_g->choice_cfg_codec->value(CHOICE_MP3);
        }
        else if (!strcmp(cfg.audio.codec, "flac")) {
            fl_g->choice_cfg_codec->value(CHOICE_FLAC);
        }

        return;
    }

    strcpy(cfg.audio.codec, "aac");
    print_info(_("Stream codec set to aac"), 0);
    fl_g->choice_cfg_bitrate->activate();
    fl_g->choice_cfg_bitrate->show();

#endif
}

void choice_cfg_codec_flac_cb(void)
{
    if (flac_enc_reinit(&flac_stream) != 0) {
        print_info(_("ERROR: While initializing flac settings"), 1);

        if (!strcmp(cfg.audio.codec, "mp3")) {
            fl_g->choice_rec_codec->value(CHOICE_MP3);
        }
        else if (!strcmp(cfg.audio.codec, "ogg")) {
            fl_g->choice_rec_codec->value(CHOICE_OGG);
        }
        else if (!strcmp(cfg.audio.codec, "opus")) {
            fl_g->choice_rec_codec->value(CHOICE_OPUS);
        }
        else if (!strcmp(cfg.audio.codec, "aac")) {
            fl_g->choice_rec_codec->value(CHOICE_AAC);
        }

        return;
    }
    strcpy(cfg.audio.codec, "flac");
    print_info(_("Stream codec set to flac"), 0);

    fl_g->choice_cfg_bitrate->hide();
    fl_g->window_cfg->redraw();
}

void choice_rec_codec_mp3_cb(void)
{
    update_rec_bitrate_list(CHOICE_MP3);
    lame_rec.bitrate = cfg.rec.bitrate;

    if (lame_enc_reinit(&lame_rec) != 0) {
        print_info(_("MP3 encoder doesn't support current\n"
                     "Sample-/Bitrate combination"),
                   1);

        // fall back to old rec codec
        if (!strcmp(cfg.rec.codec, "ogg")) {
            fl_g->choice_rec_codec->value(CHOICE_OGG);
        }
        else if (!strcmp(cfg.rec.codec, "wav")) {
            fl_g->choice_rec_codec->value(CHOICE_WAV);
        }
        else if (!strcmp(cfg.rec.codec, "opus")) {
            fl_g->choice_rec_codec->value(CHOICE_OPUS);
        }
        else if (!strcmp(cfg.rec.codec, "aac")) {
            fl_g->choice_rec_codec->value(CHOICE_AAC);
        }
        else if (!strcmp(cfg.rec.codec, "flac")) {
            fl_g->choice_rec_codec->value(CHOICE_FLAC);
        }

        return;
    }
    strcpy(cfg.rec.codec, "mp3");

    // check if the extension of the filename matches
    // the current selected codec
    test_file_extension();

    print_info(_("Record codec set to mp3"), 0);
    fl_g->choice_rec_bitrate->activate();
    fl_g->choice_rec_bitrate->show();
}

void choice_rec_codec_ogg_cb(void)
{
    update_rec_bitrate_list(CHOICE_OGG);
    vorbis_rec.bitrate = cfg.rec.bitrate;

    if (vorbis_enc_reinit(&vorbis_rec) != 0) {
        print_info(_("OGG Vorbis encoder doesn't support current\n"
                     "Sample-/Bitrate combination"),
                   1);

        if (!strcmp(cfg.rec.codec, "mp3")) {
            fl_g->choice_rec_codec->value(CHOICE_MP3);
        }
        else if (!strcmp(cfg.rec.codec, "wav")) {
            fl_g->choice_rec_codec->value(CHOICE_WAV);
        }
        else if (!strcmp(cfg.rec.codec, "opus")) {
            fl_g->choice_rec_codec->value(CHOICE_OPUS);
        }
        else if (!strcmp(cfg.rec.codec, "aac")) {
            fl_g->choice_rec_codec->value(CHOICE_AAC);
        }
        else if (!strcmp(cfg.rec.codec, "flac")) {
            fl_g->choice_rec_codec->value(CHOICE_FLAC);
        }

        return;
    }
    strcpy(cfg.rec.codec, "ogg");

    // check if the extension of the filename matches
    // the current selected codec
    test_file_extension();

    print_info(_("Record codec set to ogg/vorbis"), 0);
    fl_g->choice_rec_bitrate->activate();
    fl_g->choice_rec_bitrate->show();
}

void choice_rec_codec_opus_cb(void)
{
    update_rec_bitrate_list(CHOICE_OPUS);
    opus_rec.bitrate = cfg.rec.bitrate * 1000;

    if (opus_enc_reinit(&opus_rec) != 0) {
        print_info(_("Opus encoder doesn't support current\n"
                     "Sample-/Bitrate combination"),
                   1);

        if (!strcmp(cfg.rec.codec, "mp3")) {
            fl_g->choice_rec_codec->value(CHOICE_MP3);
        }
        else if (!strcmp(cfg.rec.codec, "wav")) {
            fl_g->choice_rec_codec->value(CHOICE_WAV);
        }
        else if (!strcmp(cfg.rec.codec, "ogg")) {
            fl_g->choice_rec_codec->value(CHOICE_OGG);
        }
        else if (!strcmp(cfg.rec.codec, "aac")) {
            fl_g->choice_rec_codec->value(CHOICE_AAC);
        }
        else if (!strcmp(cfg.rec.codec, "flac")) {
            fl_g->choice_rec_codec->value(CHOICE_FLAC);
        }

        return;
    }
    strcpy(cfg.rec.codec, "opus");

    // check if the extension of the filename matches
    // the current selected codec
    test_file_extension();

    print_info(_("Record codec set to opus"), 0);
    fl_g->choice_rec_bitrate->activate();
    fl_g->choice_rec_bitrate->show();
}

void choice_rec_codec_aac_cb(void)
{
#ifdef HAVE_LIBFDK_AAC
    if (g_aac_lib_available == 0) {
        fl_alert(_("Could not find aac library.\nPlease follow the instructions in the manual for adding aac support."));
        if (!strcmp(cfg.audio.codec, "ogg")) {
            fl_g->choice_cfg_codec->value(CHOICE_OGG);
        }
        else if (!strcmp(cfg.audio.codec, "opus")) {
            fl_g->choice_cfg_codec->value(CHOICE_OPUS);
        }
        else if (!strcmp(cfg.audio.codec, "mp3")) {
            fl_g->choice_cfg_codec->value(CHOICE_MP3);
        }
        else if (!strcmp(cfg.audio.codec, "flac")) {
            fl_g->choice_cfg_codec->value(CHOICE_FLAC);
        }

        return;
    }

    update_rec_bitrate_list(CHOICE_AAC);
    aac_rec.bitrate = cfg.rec.bitrate;
    if (aac_enc_reinit(&aac_rec) != 0) {
        print_info(_("AAC encoder doesn't support current\n"
                     "Sample-/Bitrate combination"),
                   1);

        // fall back to old rec codec
        if (!strcmp(cfg.rec.codec, "ogg")) {
            fl_g->choice_rec_codec->value(CHOICE_OGG);
        }
        else if (!strcmp(cfg.rec.codec, "wav")) {
            fl_g->choice_rec_codec->value(CHOICE_WAV);
        }
        else if (!strcmp(cfg.rec.codec, "opus")) {
            fl_g->choice_rec_codec->value(CHOICE_OPUS);
        }
        else if (!strcmp(cfg.rec.codec, "flac")) {
            fl_g->choice_rec_codec->value(CHOICE_FLAC);
        }
        else if (!strcmp(cfg.rec.codec, "mp3")) {
            fl_g->choice_rec_codec->value(CHOICE_MP3);
        }

        return;
    }
    strcpy(cfg.rec.codec, "aac");

    // check if the extension of the filename matches
    // the current selected codec
    test_file_extension();

    print_info(_("Record codec set to aac"), 0);
    fl_g->choice_rec_bitrate->activate();
    fl_g->choice_rec_bitrate->show();

#endif
}

void choice_rec_codec_flac_cb(void)
{
    if (flac_enc_reinit(&flac_rec) != 0) {
        print_info(_("ERROR: While initializing flac settings"), 1);

        if (!strcmp(cfg.rec.codec, "mp3")) {
            fl_g->choice_rec_codec->value(CHOICE_MP3);
        }
        else if (!strcmp(cfg.rec.codec, "ogg")) {
            fl_g->choice_rec_codec->value(CHOICE_OGG);
        }
        else if (!strcmp(cfg.rec.codec, "opus")) {
            fl_g->choice_rec_codec->value(CHOICE_OPUS);
        }
        else if (!strcmp(cfg.rec.codec, "wav")) {
            fl_g->choice_rec_codec->value(CHOICE_WAV);
        }
        else if (!strcmp(cfg.rec.codec, "aac")) {
            fl_g->choice_rec_codec->value(CHOICE_AAC);
        }

        return;
    }
    strcpy(cfg.rec.codec, "flac");

    // check if the extension of the filename matches
    // the current selected codec
    test_file_extension();

    print_info(_("Record codec set to flac"), 0);
    fl_g->choice_rec_bitrate->hide();
    fl_g->window_cfg->redraw();
}

void choice_rec_codec_wav_cb(void)
{
    fl_g->choice_rec_bitrate->hide();
    fl_g->window_cfg->redraw();

    strcpy(cfg.rec.codec, "wav");

    // check if the extension of the filename matches
    // the current selected codec
    test_file_extension();

    print_info(_("Record codec set to wav"), 0);
}

void input_tls_cert_file_cb(void)
{
    cfg.tls.cert_file = (char *)realloc(cfg.tls.cert_file, strlen(fl_g->input_tls_cert_file->value()) + 1);

    strcpy(cfg.tls.cert_file, fl_g->input_tls_cert_file->value());
    fl_g->input_tls_cert_file->tooltip(cfg.tls.cert_file);
}

void input_tls_cert_dir_cb(void)
{
    int len = strlen(fl_g->input_tls_cert_dir->value());

    cfg.tls.cert_dir = (char *)realloc(cfg.tls.cert_dir, len + 2);

    strcpy(cfg.tls.cert_dir, fl_g->input_tls_cert_dir->value());

#ifdef WIN32 // Replace all "Windows slashes" with "unix slashes"
    strrpl(&cfg.tls.cert_dir, (char *)"\\", (char *)"/", MODE_ALL);
#endif

    // Append an '/' if there isn't one
    if ((len > 0) && (cfg.tls.cert_dir[len - 1] != '/')) {
        strcat(cfg.tls.cert_dir, "/");
    }

    fl_g->input_tls_cert_dir->value(cfg.tls.cert_dir);
    fl_g->input_tls_cert_dir->tooltip(cfg.tls.cert_dir);
}

void button_tls_browse_file_cb(void)
{
    Fl_My_Native_File_Chooser nfc;
    nfc.title(_("Select certificate file..."));
    nfc.type(Fl_My_Native_File_Chooser::BROWSE_FILE);

    switch (nfc.show()) {
    case -1:
        fl_alert(_("ERROR: %s"), nfc.errmsg());
        break;
    case 1:
        break; // cancel pressed
    default:
        fl_g->input_tls_cert_file->value(nfc.filename());
        input_tls_cert_file_cb();
    }
}

void button_tls_browse_dir_cb(void)
{
    Fl_My_Native_File_Chooser nfc;
    nfc.title(_("Select certificate directory..."));
    nfc.type(Fl_My_Native_File_Chooser::BROWSE_DIRECTORY);
    nfc.options(Fl_My_Native_File_Chooser::NEW_FOLDER);

    nfc.directory(fl_g->input_tls_cert_dir->value());

    switch (nfc.show()) {
    case -1:
        fl_alert(_("ERROR: %s"), nfc.errmsg()); // error
        break;
    case 1:
        break; // cancel pressed
    default:
        fl_g->input_tls_cert_dir->value(nfc.filename());
        input_tls_cert_dir_cb();
        break;
    }
}

void ILM216_cb(void)
{
    if (Fl::event_button() == 1) // left mouse button
    {
        // change the display mode only when connected or recording
        // this will prevent confusing the user
        if (!connected && !recording) {
            return;
        }

        switch (display_info) {
        case STREAM_TIME:
            if (recording) {
                display_info = REC_TIME;
            }
            else {
                display_info = SENT_DATA;
            }
            break;

        case REC_TIME:
            if (connected) {
                display_info = SENT_DATA;
            }
            else {
                display_info = REC_DATA;
            }
            break;

        case SENT_DATA:
            if (recording) {
                display_info = REC_DATA;
            }
            else {
                display_info = STREAM_TIME;
            }
            break;

        case REC_DATA:
            if (connected) {
                display_info = STREAM_TIME;
            }
            else {
                display_info = REC_TIME;
            }
        }
    }
    /* if(Fl::event_button() == 3) //right mouse button
     {
         uchar r, g, b;

         Fl_Color bg, txt;
         bg  = (Fl_Color)cfg.main.bg_color;
         txt = (Fl_Color)cfg.main.txt_color;

         //Set the r g b values the color_chooser should start with
         r = (bg & 0xFF000000) >> 24;
         g = (bg & 0x00FF0000) >> 16;
         b = (bg & 0x0000FF00) >>  8;

         fl_color_chooser((const char*)"select background color", r, g, b);

         //The color_chooser changes the r, g, b, values to selected color
         cfg.main.bg_color = fl_rgb_color(r, g, b);

         fl_g->lcd->redraw();

         r = (txt & 0xFF000000) >> 24;
         g = (txt & 0x00FF0000) >> 16;
         b = (txt & 0x0000FF00) >>  8;

         fl_color_chooser((const char*)"select text color", r, g, b);
         cfg.main.txt_color = fl_rgb_color(r, g, b);

         fl_g->lcd->redraw();


     }*/
}

void button_rec_browse_cb(void)
{
    Fl_My_Native_File_Chooser nfc;
    nfc.title(_("Record to..."));
    nfc.type(Fl_My_Native_File_Chooser::BROWSE_DIRECTORY);
    nfc.options(Fl_My_Native_File_Chooser::NEW_FOLDER);

    nfc.directory(fl_g->input_rec_folder->value());

    switch (nfc.show()) {
    case -1:
        fl_alert(_("ERROR: %s"), nfc.errmsg()); // error
        break;
    case 1:
        break; // cancel
    default:
        fl_g->input_rec_folder->value(nfc.filename());
        input_rec_folder_cb();

        break;
    }
}
void button_rec_split_now_cb(void)
{
    if (recording) {
        split_recording_file();
    }
    else {
        fl_alert(_("File splitting only works if recording is active."));
    }
}

void input_rec_filename_cb(void)
{
    char *tooltip;

    cfg.rec.filename = (char *)realloc(cfg.rec.filename, strlen(fl_g->input_rec_filename->value()) + 1);

    strcpy(cfg.rec.filename, fl_g->input_rec_filename->value());

    // check if the extension of the filename matches
    // the current selected codec
    test_file_extension();

    tooltip = strdup(cfg.rec.filename);

    expand_string(&tooltip);

    fl_g->input_rec_filename->copy_tooltip(tooltip);

    free(tooltip);
}

void input_rec_folder_cb(void)
{
    char *tooltip;
    int len = strlen(fl_g->input_rec_folder->value());

    cfg.rec.folder = (char *)realloc(cfg.rec.folder, len + 2);

    strcpy(cfg.rec.folder, fl_g->input_rec_folder->value());

#ifdef WIN32 // Replace all "Windows slashes" with "unix slashes"
    char *p;
    p = cfg.rec.folder;
    while (*p != '\0') {
        if (*p == '\\') {
            *p = '/';
        }
        p++;
    }
#endif

    // Append an '/' if there isn't one
    if (cfg.rec.folder[len - 1] != '/') {
        strcat(cfg.rec.folder, "/");
    }

    fl_g->input_rec_folder->value(cfg.rec.folder);

    tooltip = strdup(cfg.rec.folder);
    expand_string(&tooltip);
    fl_g->input_rec_folder->copy_tooltip(tooltip);
    free(tooltip);
}

void input_log_filename_cb(void)
{
    cfg.main.log_file = (char *)realloc(cfg.main.log_file, strlen(fl_g->input_log_filename->value()) + 1);

    strcpy(cfg.main.log_file, fl_g->input_log_filename->value());
    fl_g->input_log_filename->tooltip(cfg.main.log_file);
}

void button_cfg_browse_songfile_cb(void)
{
    Fl_My_Native_File_Chooser nfc;
    nfc.title(_("Select Songfile"));
    nfc.type(Fl_My_Native_File_Chooser::BROWSE_FILE);
    switch (nfc.show()) {
    case -1:
        fl_alert(_("ERROR: %s"), nfc.errmsg());
        break;
    case 1:
        break; // cancel
    default:
        fl_g->input_cfg_song_file->value(nfc.filename());
        input_cfg_song_file_cb();
    }
}

void input_cfg_song_file_cb(void)
{
    int len = strlen(fl_g->input_cfg_song_file->value());

    cfg.main.song_path = (char *)realloc(cfg.main.song_path, len + 1);

    strcpy(cfg.main.song_path, fl_g->input_cfg_song_file->value());

#ifdef WIN32 // Replace all "Windows slashes" with "unix slashes"
    char *p;
    p = cfg.main.song_path;
    while (*p != '\0') {
        if (*p == '\\') {
            *p = '/';
        }
        p++;
    }
#endif

    fl_g->input_cfg_song_file->value(cfg.main.song_path);
    fl_g->input_cfg_song_file->tooltip(cfg.main.song_path);
}

void check_gui_attach_cb(void)
{
    if (fl_g->check_gui_attach->value()) {
        cfg.gui.attach = 1;
        Fl::add_timeout(0.1, &cfg_win_pos_timer);
    }
    else {
        cfg.gui.attach = 0;
        Fl::remove_timeout(&cfg_win_pos_timer);
    }
}

void check_gui_ontop_cb(void)
{
    if (fl_g->check_gui_ontop->value()) {
        fl_g->window_main->stay_on_top(1);
        fl_g->window_cfg->stay_on_top(1);
        cfg.gui.ontop = 1;
    }
    else {
        fl_g->window_main->stay_on_top(0);
        fl_g->window_cfg->stay_on_top(0);
        cfg.gui.ontop = 0;
    }
}
void check_gui_hide_log_window_cb(void)
{
    if (fl_g->check_gui_hide_log_window->value()) {
        cfg.gui.hide_log_window = 1;
    }
    else {
        cfg.gui.hide_log_window = 0;
    }
}

void check_gui_remember_pos_cb(void)
{
    if (fl_g->check_gui_remember_pos->value()) {
        cfg.gui.remember_pos = 1;
    }
    else {
        cfg.gui.remember_pos = 0;
    }
}

void check_gui_lcd_auto_cb(void)
{
    if (fl_g->check_gui_lcd_auto->value()) {
        cfg.gui.lcd_auto = 1;
    }
    else {
        cfg.gui.lcd_auto = 0;
    }
}

void check_gui_start_minimized_cb(void)
{
    cfg.gui.start_minimized = fl_g->check_gui_start_minimized->value();
}

void check_gui_disable_gain_slider_cb(void)
{
    cfg.gui.disable_gain_slider = fl_g->check_gui_disable_gain_slider->value();

    if (cfg.gui.disable_gain_slider) {
        fl_g->slider_gain->deactivate();
        fl_g->label_n24dB->deactivate();
        fl_g->label_p24dB->deactivate();
        fl_g->slider_gain->tooltip(_("Gain control is disabled. Enable in Settings->GUI"));
    }
    else {
        fl_g->slider_gain->activate();
        fl_g->label_n24dB->activate();
        fl_g->label_p24dB->activate();
        fl_g->slider_gain->tooltip(_("Master Gain"));
    }
}

void check_gui_show_listeners_cb(void)
{
    cfg.gui.show_listeners = fl_g->check_gui_show_listeners->value();

    if (connected) {
        if (cfg.gui.show_listeners) {
            static int reset = 1;
            Fl::remove_timeout(&request_listener_count_timer);
            Fl::add_timeout(0.5, &request_listener_count_timer, &reset);
        }
        else {
            Fl::remove_timeout(&request_listener_count_timer);
            fl_g->label_current_listeners->hide();
        }
    }
}

void button_gui_bg_color_cb(void)
{
    uchar r, g, b;

    Fl_Color bg;
    bg = (Fl_Color)cfg.main.bg_color;

    // Set the r g b values the color_chooser should start with
    r = (bg & 0xFF000000) >> 24;
    g = (bg & 0x00FF0000) >> 16;
    b = (bg & 0x0000FF00) >> 8;

    fl_color_chooser(_("select background color"), r, g, b);

    // The color_chooser changes the r, g, b, values to selected color
    cfg.main.bg_color = fl_rgb_color(r, g, b);

    fl_g->button_gui_bg_color->color(cfg.main.bg_color, fl_lighter((Fl_Color)cfg.main.bg_color));
    fl_g->button_gui_bg_color->redraw();
    fl_g->lcd->redraw();
    fl_g->radio_co_logo->redraw();
}

void button_gui_text_color_cb(void)
{
    uchar r, g, b;

    Fl_Color txt;
    txt = (Fl_Color)cfg.main.txt_color;

    // Set the r g b values the color_chooser should start with
    r = (txt & 0xFF000000) >> 24;
    g = (txt & 0x00FF0000) >> 16;
    b = (txt & 0x0000FF00) >> 8;

    fl_color_chooser(_("select text color"), r, g, b);

    // The color_chooser changes the r, g, b, values to selected color
    cfg.main.txt_color = fl_rgb_color(r, g, b);

    fl_g->button_gui_text_color->color(cfg.main.txt_color, fl_lighter((Fl_Color)cfg.main.txt_color));
    fl_g->button_gui_text_color->redraw();
    fl_g->lcd->redraw();
    fl_g->radio_co_logo->redraw();
}

void choice_gui_language_cb(void)
{
    lang_id_to_str(fl_g->choice_gui_language->value(), &cfg.gui.lang_str, LANG_MAPPING_NEW);
    fl_alert(_("Please restart butt to apply new language."));
}

void radio_gui_vu_gradient_cb(void)
{
    cfg.gui.vu_mode = VU_MODE_GRADIENT;
}
void radio_gui_vu_solid_cb(void)
{
    cfg.gui.vu_mode = VU_MODE_SOLID;
}

void check_gui_always_show_vu_tabs_cb(void)
{
    cfg.gui.always_show_vu_tabs = fl_g->check_gui_always_show_vu_tabs->value();

    if (cfg.gui.always_show_vu_tabs == 0) {
        fl_g->invisible_tab_box->enable();
        hide_vu_tabs();
    }
    else {
        fl_g->invisible_tab_box->disable();
        show_vu_tabs();
    }
}

void input_gui_window_title_cb(void)
{
    cfg.gui.window_title = (char *)realloc(cfg.gui.window_title, strlen(fl_g->input_gui_window_title->value()) + 1);
    strcpy(cfg.gui.window_title, fl_g->input_gui_window_title->value());

    if (strlen(cfg.gui.window_title) > 0) {
        fl_g->window_main->label(cfg.gui.window_title);
    }
    else {
        fl_g->window_main->label(PACKAGE_STRING);
    }
}

void check_cfg_auto_start_rec_cb(void)
{
    cfg.rec.start_rec = fl_g->check_cfg_auto_start_rec->value();
    fl_g->lcd->redraw(); // update the little record icon
    fl_g->radio_co_logo->redraw();
}
void check_cfg_auto_stop_rec_cb(void)
{
    cfg.rec.stop_rec = fl_g->check_cfg_auto_stop_rec->value();
}

void check_cfg_rec_after_launch_cb(void)
{
    cfg.rec.rec_after_launch = fl_g->check_cfg_rec_after_launch->value();
}

void check_cfg_rec_hourly_cb(void)
{
    // cfg.rec.start_rec_hourly = fl_g->check_cfg_rec_hourly->value();
}

void check_cfg_connect_cb(void)
{
    cfg.main.connect_at_startup = fl_g->check_cfg_connect->value();
}

void check_cfg_force_reconnecting_cb(void)
{
    cfg.main.force_reconnecting = fl_g->check_cfg_force_reconnecting->value();
}

void input_cfg_present_level_cb(void)
{
    if (fl_g->input_cfg_present_level->value() < -90) {
        fl_alert(_("Value must be a number between -90.0 and 0"));
        fl_g->input_cfg_present_level->value(-cfg.audio.signal_level);
    }

    if (fl_g->input_cfg_present_level->value() > 0) {
        fl_alert(_("Value must be a number between -90.0 and 0"));
        fl_g->input_cfg_present_level->value(-cfg.audio.signal_level);
    }

    cfg.audio.signal_level = -fl_g->input_cfg_present_level->value();
}

void input_cfg_absent_level_cb(void)
{
    if (fl_g->input_cfg_absent_level->value() < -90) {
        fl_alert(_("Value must be a number between -90.0 and 0"));
        fl_g->input_cfg_absent_level->value(-cfg.audio.silence_level);
    }

    if (fl_g->input_cfg_absent_level->value() > 0) {
        fl_alert(_("Value must be a number between -90.0 and 0"));
        fl_g->input_cfg_absent_level->value(-cfg.audio.silence_level);
    }

    cfg.audio.silence_level = -fl_g->input_cfg_absent_level->value();
}

void check_stream_signal_cb(void)
{
    cfg.main.signal_detection = fl_g->check_stream_signal->value();

    if (cfg.main.signal_detection == 1) {
        if (fl_g->input_cfg_signal->value() < 1) {
            fl_g->input_cfg_signal->value(1);
        }
        Fl::remove_timeout(&stream_signal_timer);
        Fl::add_timeout(1, &stream_signal_timer);
    }
    else {
        Fl::remove_timeout(&stream_signal_timer);
    }
}

void check_stream_silence_cb(void)
{
    cfg.main.silence_detection = fl_g->check_stream_silence->value();

    if (cfg.main.silence_detection == 1) {
        if (fl_g->input_cfg_silence->value() < 1) {
            fl_g->input_cfg_silence->value(1);
        }
        Fl::remove_timeout(&stream_silence_timer);
        Fl::add_timeout(1, &stream_silence_timer);
    }
    else {
        Fl::remove_timeout(&stream_silence_timer);
    }
}

void input_cfg_signal_cb(void)
{
    if (cfg.main.signal_detection == 1) {
        // Values < 1 are not allowed
        if (fl_g->input_cfg_signal->value() < 1) {
            fl_g->input_cfg_signal->value(1);
        }
        Fl::remove_timeout(&stream_signal_timer);
        Fl::add_timeout(1, &stream_signal_timer);
    }
    cfg.main.signal_threshold = fl_g->input_cfg_signal->value();
}

void input_cfg_silence_cb(void)
{
    if (cfg.main.silence_detection == 1) {
        // Values < 1 are not allowed
        if (fl_g->input_cfg_silence->value() < 1) {
            fl_g->input_cfg_silence->value(1);
        }
        Fl::remove_timeout(&stream_silence_timer);
        Fl::add_timeout(1, &stream_silence_timer);
    }
    cfg.main.silence_threshold = fl_g->input_cfg_silence->value();
}

void input_cfg_reconnect_delay_cb(void)
{
    cfg.main.reconnect_delay = fl_g->input_cfg_reconnect_delay->value();
}

void check_rec_signal_cb(void)
{
    cfg.rec.signal_detection = fl_g->check_rec_signal->value();

    if (cfg.rec.signal_detection == 1) {
        if (fl_g->input_rec_signal->value() < 1) {
            fl_g->input_rec_signal->value(1);
        }
        Fl::remove_timeout(&record_signal_timer);
        Fl::add_timeout(1, &record_signal_timer);
    }
    else {
        Fl::remove_timeout(&record_signal_timer);
    }
}

void check_rec_silence_cb(void)
{
    cfg.rec.silence_detection = fl_g->check_rec_silence->value();

    if (cfg.rec.silence_detection == 1) {
        if (fl_g->input_rec_silence->value() < 1) {
            fl_g->input_rec_silence->value(1);
        }
        Fl::remove_timeout(&record_silence_timer);
        Fl::add_timeout(1, &record_silence_timer);
    }
    else {
        Fl::remove_timeout(&record_silence_timer);
    }
}

void input_rec_signal_cb(void)
{
    if (cfg.rec.signal_detection == 1) {
        // Values < 1 are not allowed
        if (fl_g->input_rec_signal->value() < 1) {
            fl_g->input_rec_signal->value(1);
        }
        Fl::remove_timeout(&record_signal_timer);
        Fl::add_timeout(1, &record_signal_timer);
    }
    cfg.rec.signal_threshold = fl_g->input_rec_signal->value();
}

void input_rec_silence_cb(void)
{
    if (cfg.rec.silence_detection == 1) {
        // Values < 1 are not allowed
        if (fl_g->input_rec_silence->value() < 1) {
            fl_g->input_rec_silence->value(1);
        }
        Fl::remove_timeout(&record_silence_timer);
        Fl::add_timeout(1, &record_silence_timer);
    }
    cfg.rec.silence_threshold = fl_g->input_rec_silence->value();
}

void check_song_update_active_cb(void)
{
    if (fl_g->check_song_update_active->value()) {
        if (connected) {
            static int reset = 1;
            Fl::remove_timeout(&songfile_timer);
            Fl::add_timeout(0.1, &songfile_timer, &reset);
        }
        cfg.main.song_update = 1;
    }
    else {
        Fl::remove_timeout(&songfile_timer);
        if (cfg.main.song != NULL) {
            free(cfg.main.song);
            cfg.main.song = NULL;
        }
        cfg.main.song_update = 0;
    }
}

void check_read_last_line_cb(void)
{
    cfg.main.read_last_line = fl_g->check_read_last_line->value();
}

void check_sync_to_full_hour_cb(void)
{
    if (fl_g->check_sync_to_full_hour->value()) {
        cfg.rec.sync_to_hour = 1;
    }
    else {
        cfg.rec.sync_to_hour = 0;
    }
}

void slider_gain_cb(void *called_by)
{
    char str[10];
    float gain_db;

    if (called_by != (void *)CB_CALLED_BY_MIDI) {
        cfg.midi.commands[MIDI_CMD_MASTER_GAIN].picked_up = 0;
    }

    // Without redrawing the main window the slider knob is not redrawn correctly
    fl_g->window_main->redraw();

    gain_db = (float)fl_g->slider_gain->value();

    snprintf(str, 10, "%+.1f dB", gain_db);
    fl_g->text_mixer_master_volume->copy_label(str);
    fl_g->slider_mixer_master_gain->value_cb2("dB");

    if (gain_db > -0.1 && gain_db < 0.1) {
        cfg.main.gain = 1;
        fl_g->slider_gain->value(0);
        fl_g->slider_mixer_master_gain->value(0);
    }
    else {
        cfg.main.gain = util_db_to_factor(gain_db);
        fl_g->slider_mixer_master_gain->value(gain_db);
    }

    fl_g->slider_gain->value_cb2("dB");
}

void vu_tabs_cb(void)
{
    const char *active_tab = fl_g->vu_tabs->value()->label();
    if (!strcmp(active_tab, _("Streaming"))) {
        fl_g->label_volume->label(_("Streaming volume"));
        snd_set_vu_level_type(0);
    }
    else {
        fl_g->label_volume->label(_("Recording volume"));
        snd_set_vu_level_type(1);
    }
}

void input_rec_split_time_cb(void)
{
    // Values < 0 are not allowed
    if (fl_g->input_rec_split_time->value() < 0) {
        fl_g->input_rec_split_time->value(0);
    }

    cfg.rec.split_time = fl_g->input_rec_split_time->value();
}

void button_cfg_export_cb(void)
{
    char *filename;

    Fl_My_Native_File_Chooser nfc;

    nfc.title(_("Export to..."));
    nfc.type(Fl_My_Native_File_Chooser::BROWSE_SAVE_FILE);
    nfc.options(Fl_My_Native_File_Chooser::NEW_FOLDER);

    switch (nfc.show()) {
    case -1:
        fl_alert(_("ERROR: %s"), nfc.errmsg()); // error
        return;
        break;
    case 1:
        return; // cancel
        break;
    default:
        filename = (char *)nfc.filename();
    }

    cfg_write_file(filename);
}

void button_cfg_import_cb(void)
{
    char *filename;
    char info_buf[256];

    Fl_My_Native_File_Chooser nfc;
    nfc.title(_("Import..."));
    nfc.type(Fl_My_Native_File_Chooser::BROWSE_FILE);

    switch (nfc.show()) {
    case -1:
        fl_alert(_("ERROR: %s"), nfc.errmsg()); // error
        return;
        break;
    case 1:
        return; // cancel
        break;
    default:
        filename = (char *)nfc.filename();
        break;
    }

    // read config and initialize config struct
    if (cfg_set_values(filename) != 0) {
        snprintf(info_buf, sizeof(info_buf), _("Could not import config %s"), filename);
        print_info(info_buf, 1);
        return;
    }

    // re-initialize some stuff after config has been successfully imported
    init_main_gui_and_audio();
    fill_cfg_widgets();
    if (snd_reopen_streams() != 0) {
        fl_alert(_("butt could not open selected audio device.\nPlease try another device.\n"));
        return;
    }
    update_channel_lists();

    snprintf(info_buf, sizeof(info_buf), _("Config imported %s"), filename);
    print_info(info_buf, 1);
}

void check_update_at_startup_cb(void)
{
    cfg.main.check_for_update = fl_g->check_update_at_startup->value();
}

void check_start_agent_cb(void)
{
    cfg.main.start_agent = fl_g->check_start_agent->value();
}

void button_start_agent_cb(void)
{
#ifdef WIN32
    if (tray_agent_is_running(NULL) == 0) {
        if (tray_agent_start() == 0) {
            tray_agent_send_cmd(TA_START);
            Fl::add_timeout(1, &has_agent_been_started_timer);
        }
    }
    else {
        fl_alert("butt agent is already running.");
    }
#endif
}

void button_stop_agent_cb(void)
{
#ifdef WIN32
    if (tray_agent_is_running(NULL) == 1) {
        tray_agent_stop();
        Fl::add_timeout(1, &has_agent_been_stopped_timer);
    }
    else {
        fl_alert("butt agent is currently not running.");
    }

#endif
}

void check_minimize_to_tray_cb(void)
{
    cfg.main.minimize_to_tray = fl_g->check_minimize_to_tray->value();

    if (cfg.main.minimize_to_tray == 1) {
        fl_g->window_main->minimize_to_tray = true;
    }
    else {
        fl_g->window_main->minimize_to_tray = false;
    }
}

void button_cfg_check_for_updates_cb(void)
{
    int rc;
    char uri[100];
    char *new_version;
    int ret = update_check_for_new_version();

    switch (ret) {
    case UPDATE_NEW_VERSION:
        new_version = update_get_version();
        rc = fl_choice(_("New version available: %s\nYou have version %s"), _("Cancel"), _("Get new version"), NULL, new_version, VERSION);
        if (rc == 1) {
            // snprintf(uri, sizeof(uri)-1, "https://sourceforge.net/projects/butt/files/butt/butt-%s/", new_version);
            snprintf(uri, sizeof(uri) - 1, "https://danielnoethen.de/butt/index.html#_download");
            fl_open_uri(uri);
        }

        break;
    case UPDATE_SOCKET_ERROR:
        fl_alert(_("Could not get update information.\nReason: Network error"));
        break;
    case UPDATE_ILLEGAL_ANSWER:
        fl_alert(_("Could not get update information.\nReason: Unknown answer from server"));
        break;
    case UPDATE_UP_TO_DATE:
        fl_alert(_("You have the latest version!"));
        break;
    default:
        fl_alert(_("Could not get update information.\nReason: Unknown"));
        break;
    }
}

void button_cfg_log_browse_cb(void)
{
    Fl_My_Native_File_Chooser nfc;
    nfc.title(_("Select logfile..."));
    nfc.type(Fl_My_Native_File_Chooser::BROWSE_SAVE_FILE);
    nfc.options(Fl_My_Native_File_Chooser::NEW_FOLDER);

    switch (nfc.show()) {
    case -1:
        fl_alert(_("ERROR: %s"), nfc.errmsg());
        break;
    case 1:
        break; // cancel
    default:
        fl_g->input_log_filename->value(nfc.filename());
        input_log_filename_cb();
    }
}

void window_main_close_cb(bool ask)
{
    if (ask && (connected || recording)) {
        int ret;
        if (connected) {
            fl_message_title("Streaming");
            ret = fl_choice(_("butt is currently streaming.\n"
                              "Do you really want to close butt now?"),
                            _("no"), _("yes"), NULL);
        }
        else {
            fl_message_title("Recording");
            ret = fl_choice(_("butt is currently recording.\n"
                              "Do you really want to close butt now?"),
                            _("no"), _("yes"), NULL);
        }

        if (ret == 0) {
            return;
        }
    }

    stop_recording(false);
    button_disconnect_cb(false);

    command_stop_server();
    if (cfg.gui.remember_pos) {
        cfg.gui.x_pos = fl_g->window_main->x_root();
        cfg.gui.y_pos = fl_g->window_main->y_root();
        cfg.gui.window_height = fl_g->window_main->h();
    }

    snd_close_streams();
    snd_close_portaudio();
    cfg_write_file(NULL);
    url_cleanup_curl();
    exit(0);
}

void check_cfg_use_app_cb(void)
{
    if (fl_g->check_cfg_use_app->value()) {
        int app = fl_g->choice_cfg_app->value();
        current_track_app = getCurrentTrackFunctionFromId(app);
        cfg.main.app_update = 1;
        cfg.main.app_update_service = app;

        static int reset = 1;
        Fl::remove_timeout(&app_timer);
        Fl::add_timeout(0.1, &app_timer, &reset);
    }
    else {
        cfg.main.app_update = 0;
        Fl::remove_timeout(&app_timer);

        if (cfg.main.song != NULL) {
            free(cfg.main.song);
            cfg.main.song = NULL;
        }
    }
}

void choice_cfg_app_cb(void)
{
    current_track_app = getCurrentTrackFunctionFromId(fl_g->choice_cfg_app->value());
    cfg.main.app_update_service = fl_g->choice_cfg_app->value();
}

void radio_cfg_artist_title_cb(void)
{
    cfg.main.app_artist_title_order = APP_ARTIST_FIRST;
}

void radio_cfg_title_artist_cb(void)
{
    cfg.main.app_artist_title_order = APP_TITLE_FIRST;
}

void choice_cfg_song_delay_cb(void)
{
    cfg.main.song_delay = 2 * fl_g->choice_cfg_song_delay->value();
}

void slider_mixer_primary_device_cb(double val, void *called_by)
{
    if (called_by != (void *)CB_CALLED_BY_MIDI) {
        cfg.midi.commands[MIDI_CMD_PRIMARY_DEV_VOL].picked_up = 0;
    }

    char str[10];
    snprintf(str, 10, "%+.1f dB", val);
    fl_g->text_mixer_primary_device_volume->copy_label(str);
    fl_g->slider_mixer_primary_device->value_cb2("dB"); // updates the tooltip

    if (val > -0.5 && val < 0.5) {
        cfg.mixer.primary_device_gain = 1;
        fl_g->slider_mixer_primary_device->value(0);
    }
    else {
        cfg.mixer.primary_device_gain = util_db_to_factor(val);
    }
}

void slider_mixer_secondary_device_cb(double val, void *called_by)
{
    if (called_by != (void *)CB_CALLED_BY_MIDI) {
        cfg.midi.commands[MIDI_CMD_SECONDARY_DEV_VOL].picked_up = 0;
    }

    char str[10];
    snprintf(str, 10, "%+.1f dB", val);
    fl_g->text_mixer_secondary_device_volume->copy_label(str);
    fl_g->slider_mixer_secondary_device->value_cb2("dB");

    if (val > -0.5 && val < 0.5) {
        cfg.mixer.secondary_device_gain = 1;
        fl_g->slider_mixer_secondary_device->value(0);
    }
    else {
        cfg.mixer.secondary_device_gain = util_db_to_factor(val);
    }
}

void slider_mixer_streaming_gain_cb(double val, void *called_by)
{
    if (called_by != (void *)CB_CALLED_BY_MIDI) {
        cfg.midi.commands[MIDI_CMD_STREAMING_GAIN].picked_up = 0;
    }

    char str[10];
    snprintf(str, 10, "%+.1f dB", val);
    fl_g->text_mixer_streaming_volume->copy_label(str);
    fl_g->slider_mixer_streaming_gain->value_cb2("dB");

    if (val > -0.5 && val < 0.5) {
        cfg.mixer.streaming_gain = 1;
        fl_g->slider_mixer_streaming_gain->value(0);
    }
    else {
        cfg.mixer.streaming_gain = util_db_to_factor(val);
    }
}

void slider_mixer_recording_gain_cb(double val, void *called_by)
{
    if (called_by != (void *)CB_CALLED_BY_MIDI) {
        cfg.midi.commands[MIDI_CMD_RECORDING_GAIN].picked_up = 0;
    }

    char str[10];
    snprintf(str, 10, "%+.1f dB", val);
    fl_g->text_mixer_recording_volume->copy_label(str);
    fl_g->slider_mixer_recording_gain->value_cb2("dB");

    if (val > -0.5 && val < 0.5) {
        cfg.mixer.recording_gain = 1;
        fl_g->slider_mixer_recording_gain->value(0);
    }
    else {
        cfg.mixer.recording_gain = util_db_to_factor(val);
    }
}
void slider_mixer_master_gain_cb(double val, void *called_by)
{
    if (called_by != (void *)CB_CALLED_BY_MIDI) {
        cfg.midi.commands[MIDI_CMD_MASTER_GAIN].picked_up = 0;
    }

    char str[10];
    snprintf(str, 10, "%+.1f dB", val);
    fl_g->text_mixer_master_volume->copy_label(str);
    fl_g->slider_mixer_master_gain->value_cb2("dB");

    if (val > -0.5 && val < 0.5) {
        cfg.main.gain = 1;
        fl_g->slider_mixer_master_gain->value(0);
        fl_g->slider_gain->value(0);
    }
    else {
        cfg.main.gain = util_db_to_factor(val);
        fl_g->slider_gain->value(val);
    }
    // Without redrawing the main window the slider knob is not redrawn correctly
    fl_g->window_main->redraw();
}

void slider_mixer_cross_fader_cb(double val, void *called_by)
{
    if (called_by != (void *)CB_CALLED_BY_MIDI) {
        cfg.midi.commands[MIDI_CMD_CROSS_FADER].picked_up = 0;
        // TOOD: Check if soft takeover works for all sliders
    }

    fl_g->slider_mixer_cross_fader->value_cb2("");

    if (val > -3.0 && val < 3.0) {
        cfg.mixer.cross_fader = 0;
        fl_g->slider_mixer_cross_fader->value(0);
    }
    else {
        cfg.mixer.cross_fader = val;
    }

    if (cfg.mixer.cross_fader == 0) {
        cfg.mixer.primary_X_fader = 1.0;
        cfg.mixer.secondary_X_fader = 1.0;
    }
    else if (cfg.mixer.cross_fader < 0) {
        cfg.mixer.primary_X_fader = 1.0;
        cfg.mixer.secondary_X_fader = 1 + (val / 100.0);
    }
    else {
        cfg.mixer.primary_X_fader = 1 - (val / 100.0);
        cfg.mixer.secondary_X_fader = 1.0;
    }

    // Without redrawing the mixer window the slider knob is not redrawn correctly
    fl_g->window_mixer->redraw();
}

void button_mixer_mute_primary_device_cb(void)
{
    cfg.mixer.primary_device_muted = fl_g->button_mixer_mute_primary_device->value();
    if (cfg.mixer.primary_device_muted == 1) {
        fl_g->slider_mixer_primary_device->deactivate();
        fl_g->text_mixer_primary_device_volume->deactivate();
    }
    else {
        fl_g->slider_mixer_primary_device->activate();
        fl_g->text_mixer_primary_device_volume->activate();
    }
}

void button_mixer_mute_secondary_device_cb(void)
{
    cfg.mixer.secondary_device_muted = fl_g->button_mixer_mute_secondary_device->value();
    if (cfg.mixer.secondary_device_muted == 1) {
        fl_g->slider_mixer_secondary_device->deactivate();
        fl_g->text_mixer_secondary_device_volume->deactivate();
    }
    else {
        fl_g->slider_mixer_secondary_device->activate();
        fl_g->text_mixer_secondary_device_volume->activate();
    }
}

void button_mixer_reset_cb(void)
{
    slider_mixer_primary_device_cb(0, (void *)CB_CALLED_BY_CODE);
    fl_g->slider_mixer_primary_device->value(0);

    slider_mixer_secondary_device_cb(0, (void *)CB_CALLED_BY_CODE);
    fl_g->slider_mixer_secondary_device->value(0);

    slider_mixer_streaming_gain_cb(0, (void *)CB_CALLED_BY_CODE);
    fl_g->slider_mixer_streaming_gain->value(0);

    slider_mixer_recording_gain_cb(0, (void *)CB_CALLED_BY_CODE);
    fl_g->slider_mixer_recording_gain->value(0);

    slider_mixer_master_gain_cb(0, (void *)CB_CALLED_BY_CODE);
    fl_g->slider_mixer_master_gain->value(0);

    slider_mixer_cross_fader_cb(0, (void *)CB_CALLED_BY_CODE);
    fl_g->slider_mixer_cross_fader->value(0);

    if (cfg.mixer.primary_device_muted == 1) {
        fl_g->button_mixer_mute_primary_device->value(0);
        button_mixer_mute_primary_device_cb();
    }

    if (cfg.mixer.secondary_device_muted == 1) {
        fl_g->button_mixer_mute_secondary_device->value(0);
        button_mixer_mute_secondary_device_cb();
    }
}

void check_stream_eq_cb(void)
{
    cfg.dsp.equalizer_stream = fl_g->check_stream_eq->value();
}

void check_rec_eq_cb(void)
{
    cfg.dsp.equalizer_rec = fl_g->check_rec_eq->value();
}

void choice_eq_preset_cb(void)
{
    int preset_id = fl_g->choice_eq_preset->value();
    const char *preset = fl_g->choice_eq_preset->text(preset_id);

    if (preset_id == 0) { // The string "Manual" is localized, therefore we check the id which will always be the first menu item
        // dsp->loadEQPreset("Manual");
        slider_equalizer1_cb(cfg.dsp.eq_gain[0]);
        fl_g->equalizerSlider1->value(cfg.dsp.eq_gain[0]);

        slider_equalizer2_cb(cfg.dsp.eq_gain[1]);
        fl_g->equalizerSlider2->value(cfg.dsp.eq_gain[1]);

        slider_equalizer3_cb(cfg.dsp.eq_gain[2]);
        fl_g->equalizerSlider3->value(cfg.dsp.eq_gain[2]);

        slider_equalizer4_cb(cfg.dsp.eq_gain[3]);
        fl_g->equalizerSlider4->value(cfg.dsp.eq_gain[3]);

        slider_equalizer5_cb(cfg.dsp.eq_gain[4]);
        fl_g->equalizerSlider5->value(cfg.dsp.eq_gain[4]);

        slider_equalizer6_cb(cfg.dsp.eq_gain[5]);
        fl_g->equalizerSlider6->value(cfg.dsp.eq_gain[5]);

        slider_equalizer7_cb(cfg.dsp.eq_gain[6]);
        fl_g->equalizerSlider7->value(cfg.dsp.eq_gain[6]);

        slider_equalizer8_cb(cfg.dsp.eq_gain[7]);
        fl_g->equalizerSlider8->value(cfg.dsp.eq_gain[7]);

        slider_equalizer9_cb(cfg.dsp.eq_gain[8]);
        fl_g->equalizerSlider9->value(cfg.dsp.eq_gain[8]);

        slider_equalizer10_cb(cfg.dsp.eq_gain[9]);
        fl_g->equalizerSlider10->value(cfg.dsp.eq_gain[9]);

        cfg.dsp.eq_preset = (char *)realloc(cfg.dsp.eq_preset, strlen("Manual") + 1);
        strcpy(cfg.dsp.eq_preset, "Manual");
    }
    else {
        double val;
        char str[10];

        val = streaming_dsp->getEQbandFromPreset(0, preset);
        snprintf(str, sizeof(str), "%+.1f", val);
        fl_g->equalizerGain1->copy_label(str);
        fl_g->equalizerSlider1->value(val);
        fl_g->equalizerSlider1->value_cb2("dB"); // updates the tooltip
        streaming_dsp->setEQband(0, val);
        recording_dsp->setEQband(0, val);

        val = streaming_dsp->getEQbandFromPreset(1, preset);
        snprintf(str, sizeof(str), "%+.1f", val);
        fl_g->equalizerGain2->copy_label(str);
        fl_g->equalizerSlider2->value(val);
        fl_g->equalizerSlider2->value_cb2("dB");
        streaming_dsp->setEQband(1, val);
        recording_dsp->setEQband(1, val);

        val = streaming_dsp->getEQbandFromPreset(2, preset);
        snprintf(str, sizeof(str), "%+.1f", val);
        fl_g->equalizerGain3->copy_label(str);
        fl_g->equalizerSlider3->value(val);
        fl_g->equalizerSlider3->value_cb2("dB");
        streaming_dsp->setEQband(2, val);
        recording_dsp->setEQband(2, val);

        val = streaming_dsp->getEQbandFromPreset(3, preset);
        snprintf(str, sizeof(str), "%+.1f", val);
        fl_g->equalizerGain4->copy_label(str);
        fl_g->equalizerSlider4->value(val);
        fl_g->equalizerSlider4->value_cb2("dB");
        streaming_dsp->setEQband(3, val);
        recording_dsp->setEQband(3, val);

        val = streaming_dsp->getEQbandFromPreset(4, preset);
        snprintf(str, sizeof(str), "%+.1f", val);
        fl_g->equalizerGain5->copy_label(str);
        fl_g->equalizerSlider5->value(val);
        fl_g->equalizerSlider5->value_cb2("dB");
        streaming_dsp->setEQband(4, val);
        recording_dsp->setEQband(4, val);

        val = streaming_dsp->getEQbandFromPreset(5, preset);
        snprintf(str, sizeof(str), "%+.1f", val);
        fl_g->equalizerGain6->copy_label(str);
        fl_g->equalizerSlider6->value(val);
        fl_g->equalizerSlider6->value_cb2("dB");
        streaming_dsp->setEQband(5, val);
        recording_dsp->setEQband(5, val);

        val = streaming_dsp->getEQbandFromPreset(6, preset);
        snprintf(str, sizeof(str), "%+.1f", val);
        fl_g->equalizerGain7->copy_label(str);
        fl_g->equalizerSlider7->value(val);
        fl_g->equalizerSlider7->value_cb2("dB");
        streaming_dsp->setEQband(6, val);
        recording_dsp->setEQband(6, val);

        val = streaming_dsp->getEQbandFromPreset(7, preset);
        snprintf(str, sizeof(str), "%+.1f", val);
        fl_g->equalizerGain8->copy_label(str);
        fl_g->equalizerSlider8->value(val);
        fl_g->equalizerSlider8->value_cb2("dB");
        streaming_dsp->setEQband(7, val);
        recording_dsp->setEQband(7, val);

        val = streaming_dsp->getEQbandFromPreset(8, preset);
        snprintf(str, sizeof(str), "%+.1f", val);
        fl_g->equalizerGain9->copy_label(str);
        fl_g->equalizerSlider9->value(val);
        fl_g->equalizerSlider9->value_cb2("dB");
        streaming_dsp->setEQband(8, val);
        recording_dsp->setEQband(8, val);

        val = streaming_dsp->getEQbandFromPreset(9, preset);
        snprintf(str, sizeof(str), "%+.1f", val);
        fl_g->equalizerGain10->copy_label(str);
        fl_g->equalizerSlider10->value(val);
        fl_g->equalizerSlider10->value_cb2("dB");
        streaming_dsp->setEQband(9, val);
        recording_dsp->setEQband(9, val);

        cfg.dsp.eq_preset = (char *)realloc(cfg.dsp.eq_preset, strlen(preset) + 1);
        strcpy(cfg.dsp.eq_preset, preset);
    }
}

void button_eq_reset_cb(void)
{
    fl_g->equalizerSlider1->value(0);
    fl_g->equalizerSlider2->value(0);
    fl_g->equalizerSlider3->value(0);
    fl_g->equalizerSlider4->value(0);
    fl_g->equalizerSlider5->value(0);
    fl_g->equalizerSlider6->value(0);
    fl_g->equalizerSlider7->value(0);
    fl_g->equalizerSlider8->value(0);
    fl_g->equalizerSlider9->value(0);
    fl_g->equalizerSlider10->value(0);
    slider_equalizer1_cb(0);
    slider_equalizer2_cb(0);
    slider_equalizer3_cb(0);
    slider_equalizer4_cb(0);
    slider_equalizer5_cb(0);
    slider_equalizer6_cb(0);
    slider_equalizer7_cb(0);
    slider_equalizer8_cb(0);
    slider_equalizer9_cb(0);
    slider_equalizer10_cb(0);
}

void slider_equalizer1_cb(double v)
{
    char str[10];

    cfg.dsp.eq_gain[0] = v;
    snprintf(str, 10, "%+.1f", v);
    fl_g->equalizerGain1->copy_label(str);
    fl_g->equalizerSlider1->value_cb2("dB"); // updates the tooltip
    streaming_dsp->setEQband(0, v);
    recording_dsp->setEQband(0, v);

    // If the an EQ slider is moved while a preset is selected we switch to manual mode
    // and set the manual EQ sliders to the values of the preset
    if (fl_g->choice_eq_preset->value() != 0) {
        fl_g->choice_eq_preset->value(0);
        read_eq_slider_values();
        cfg.dsp.eq_preset = (char *)realloc(cfg.dsp.eq_preset, strlen("Manual") + 1);
        strcpy(cfg.dsp.eq_preset, "Manual");
    }
}

void slider_equalizer2_cb(double v)
{
    char str[10];

    cfg.dsp.eq_gain[1] = v;
    snprintf(str, 10, "%+.1f", v);
    fl_g->equalizerGain2->copy_label(str);
    fl_g->equalizerSlider2->value_cb2("dB");
    streaming_dsp->setEQband(1, v);
    recording_dsp->setEQband(1, v);

    if (fl_g->choice_eq_preset->value() != 0) {
        fl_g->choice_eq_preset->value(0);
        read_eq_slider_values();
        cfg.dsp.eq_preset = (char *)realloc(cfg.dsp.eq_preset, strlen("Manual") + 1);
        strcpy(cfg.dsp.eq_preset, "Manual");
    }
}

void slider_equalizer3_cb(double v)
{
    char str[10];

    cfg.dsp.eq_gain[2] = v;
    snprintf(str, 10, "%+.1f", v);
    fl_g->equalizerGain3->copy_label(str);
    fl_g->equalizerSlider3->value_cb2("dB");
    streaming_dsp->setEQband(2, v);
    recording_dsp->setEQband(2, v);

    if (fl_g->choice_eq_preset->value() != 0) {
        fl_g->choice_eq_preset->value(0);
        read_eq_slider_values();
        cfg.dsp.eq_preset = (char *)realloc(cfg.dsp.eq_preset, strlen("Manual") + 1);
        strcpy(cfg.dsp.eq_preset, "Manual");
    }
}

void slider_equalizer4_cb(double v)
{
    char str[10];

    cfg.dsp.eq_gain[3] = v;
    snprintf(str, 10, "%+.1f", v);
    fl_g->equalizerGain4->copy_label(str);
    fl_g->equalizerSlider4->value_cb2("dB");
    streaming_dsp->setEQband(3, v);
    recording_dsp->setEQband(3, v);

    if (fl_g->choice_eq_preset->value() != 0) {
        fl_g->choice_eq_preset->value(0);
        read_eq_slider_values();
        cfg.dsp.eq_preset = (char *)realloc(cfg.dsp.eq_preset, strlen("Manual") + 1);
        strcpy(cfg.dsp.eq_preset, "Manual");
    }
}

void slider_equalizer5_cb(double v)
{
    char str[10];

    cfg.dsp.eq_gain[4] = v;
    snprintf(str, 10, "%+.1f", v);
    fl_g->equalizerGain5->copy_label(str);
    fl_g->equalizerSlider5->value_cb2("dB");
    streaming_dsp->setEQband(4, v);
    recording_dsp->setEQband(4, v);

    if (fl_g->choice_eq_preset->value() != 0) {
        fl_g->choice_eq_preset->value(0);
        read_eq_slider_values();
        cfg.dsp.eq_preset = (char *)realloc(cfg.dsp.eq_preset, strlen("Manual") + 1);
        strcpy(cfg.dsp.eq_preset, "Manual");
    }
}

void slider_equalizer6_cb(double v)
{
    char str[10];

    cfg.dsp.eq_gain[5] = v;
    snprintf(str, 10, "%+.1f", v);
    fl_g->equalizerGain6->copy_label(str);
    fl_g->equalizerSlider6->value_cb2("dB");
    streaming_dsp->setEQband(5, v);
    recording_dsp->setEQband(5, v);

    if (fl_g->choice_eq_preset->value() != 0) {
        fl_g->choice_eq_preset->value(0);
        read_eq_slider_values();
        cfg.dsp.eq_preset = (char *)realloc(cfg.dsp.eq_preset, strlen("Manual") + 1);
        strcpy(cfg.dsp.eq_preset, "Manual");
    }
}

void slider_equalizer7_cb(double v)
{
    char str[10];

    cfg.dsp.eq_gain[6] = v;
    snprintf(str, 10, "%+.1f", v);
    fl_g->equalizerGain7->copy_label(str);
    fl_g->equalizerSlider7->value_cb2("dB");
    streaming_dsp->setEQband(6, v);
    recording_dsp->setEQband(6, v);

    if (fl_g->choice_eq_preset->value() != 0) {
        fl_g->choice_eq_preset->value(0);
        read_eq_slider_values();
        cfg.dsp.eq_preset = (char *)realloc(cfg.dsp.eq_preset, strlen("Manual") + 1);
        strcpy(cfg.dsp.eq_preset, "Manual");
    }
}

void slider_equalizer8_cb(double v)
{
    char str[10];

    cfg.dsp.eq_gain[7] = v;
    snprintf(str, 10, "%+.1f", v);
    fl_g->equalizerGain8->copy_label(str);
    fl_g->equalizerSlider8->value_cb2("dB");
    streaming_dsp->setEQband(7, v);
    recording_dsp->setEQband(7, v);

    if (fl_g->choice_eq_preset->value() != 0) {
        fl_g->choice_eq_preset->value(0);
        read_eq_slider_values();
        cfg.dsp.eq_preset = (char *)realloc(cfg.dsp.eq_preset, strlen("Manual") + 1);
        strcpy(cfg.dsp.eq_preset, "Manual");
    }
}

void slider_equalizer9_cb(double v)
{
    char str[10];

    cfg.dsp.eq_gain[8] = v;
    snprintf(str, 10, "%+.1f", v);
    fl_g->equalizerGain9->copy_label(str);
    fl_g->equalizerSlider9->value_cb2("dB");
    streaming_dsp->setEQband(8, v);
    recording_dsp->setEQband(8, v);

    if (fl_g->choice_eq_preset->value() != 0) {
        fl_g->choice_eq_preset->value(0);
        read_eq_slider_values();
        cfg.dsp.eq_preset = (char *)realloc(cfg.dsp.eq_preset, strlen("Manual") + 1);
        strcpy(cfg.dsp.eq_preset, "Manual");
    }
}

void slider_equalizer10_cb(double v)
{
    char str[10];

    cfg.dsp.eq_gain[9] = v;
    snprintf(str, 10, "%+.1f", v);
    fl_g->equalizerGain10->copy_label(str);
    fl_g->equalizerSlider10->value_cb2("dB");
    streaming_dsp->setEQband(9, v);
    recording_dsp->setEQband(9, v);

    if (fl_g->choice_eq_preset->value() != 0) {
        fl_g->choice_eq_preset->value(0);
        read_eq_slider_values();
        cfg.dsp.eq_preset = (char *)realloc(cfg.dsp.eq_preset, strlen("Manual") + 1);
        strcpy(cfg.dsp.eq_preset, "Manual");
    }
}

void check_stream_drc_cb(void)
{
    cfg.dsp.compressor_stream = fl_g->check_stream_drc->value();

    if (cfg.dsp.compressor_stream == 0) {
        fl_g->LED_comp_threshold->set_state(LED::LED_OFF);
    }

    // Make sure compressor starts in a clean state
    snd_reset_streaming_compressor();
}

void check_rec_drc_cb(void)
{
    cfg.dsp.compressor_rec = fl_g->check_rec_drc->value();

    if (cfg.dsp.compressor_rec == 0) {
        fl_g->LED_comp_threshold->set_state(LED::LED_OFF);
    }

    snd_reset_recording_compressor();
}

void button_drc_reset_cb(void)
{
    fl_g->thresholdSlider->value(-20);
    fl_g->ratioSlider->value(5);
    fl_g->attackSlider->value(0.01);
    fl_g->releaseSlider->value(1);
    fl_g->makeupSlider->value(0);
    slider_threshold_cb(-20);
    slider_ratio_cb(5);
    slider_attack_cb(0.01);
    slider_release_cb(1);
    slider_makeup_cb(0);
}

void check_aggressive_mode_cb(void)
{
    cfg.dsp.aggressive_mode = fl_g->check_aggressive_mode->value();
    snd_reset_streaming_compressor();
    snd_reset_recording_compressor();
}

void slider_threshold_cb(double v)
{
    static char str[10];
    cfg.dsp.threshold = v;
    snprintf(str, 10, "%+.1f", v);
    fl_g->threshold->label(str);
    fl_g->thresholdSlider->value_cb2("dBFS"); // updates the tooltip

    snd_reset_streaming_compressor();
    snd_reset_recording_compressor();
}

void slider_ratio_cb(double v)
{
    static char str[10];
    cfg.dsp.ratio = v;
    snprintf(str, 10, "%.1f", v);
    fl_g->ratio->label(str);
    fl_g->ratioSlider->value_cb2(":1");

    snd_reset_streaming_compressor();
    snd_reset_recording_compressor();
}

void slider_attack_cb(double v)
{
    static char str[10];
    cfg.dsp.attack = v;
    snprintf(str, 10, "%.2fs", v);
    fl_g->attack->label(str);
    fl_g->attackSlider->value_cb2("s");

    snd_reset_streaming_compressor();
    snd_reset_recording_compressor();
}

void slider_release_cb(double v)
{
    static char str[10];
    cfg.dsp.release = v;
    snprintf(str, 10, "%.2fs", v);
    fl_g->release->label(str);
    fl_g->releaseSlider->value_cb2("s");

    snd_reset_streaming_compressor();
    snd_reset_recording_compressor();
}

void slider_makeup_cb(double v)
{
    static char str[10];
    cfg.dsp.makeup_gain = v;
    snprintf(str, 10, "%+.1f", v);
    fl_g->makeup->label(str);
    fl_g->makeupSlider->value_cb2("dB");
}

void choice_stream_mp3_enc_quality_cb(void)
{
    lame_enc test_enc;

    test_enc = lame_stream;
    test_enc.gfp = NULL;
    test_enc.enc_quality = fl_g->choice_stream_mp3_enc_quality->value();

    if (lame_enc_init(&test_enc) == 0) {
        cfg.mp3_codec_stream.enc_quality = fl_g->choice_stream_mp3_enc_quality->value();
        lame_stream.enc_quality = fl_g->choice_stream_mp3_enc_quality->value();

        lame_enc_reinit(&lame_stream);
        lame_enc_close(&test_enc);
    }
    else {
        printf("choice_stream_mp3_enc_quality_cb: Illegal value\n");
    }
}
void choice_stream_mp3_stereo_mode_cb(void)
{
    lame_enc test_enc;

    test_enc = lame_stream;
    test_enc.gfp = NULL;
    test_enc.stereo_mode = fl_g->choice_stream_mp3_stereo_mode->value() - 1;

    if (lame_enc_init(&test_enc) == 0) {
        cfg.mp3_codec_stream.stereo_mode = fl_g->choice_stream_mp3_stereo_mode->value();
        lame_stream.stereo_mode = fl_g->choice_stream_mp3_stereo_mode->value() - 1;

        lame_enc_reinit(&lame_stream);
        lame_enc_close(&test_enc);
    }
    else {
        printf("choice_stream_mp3_stereo_mode_cb: Illegal value\n");
    }
}
void choice_stream_mp3_bitrate_mode_cb(void)
{
    lame_enc test_enc;

    test_enc = lame_stream;
    test_enc.gfp = NULL;
    test_enc.bitrate_mode = fl_g->choice_stream_mp3_bitrate_mode->value();

    if (lame_enc_init(&test_enc) == 0) {
        cfg.mp3_codec_stream.bitrate_mode = fl_g->choice_stream_mp3_bitrate_mode->value();
        lame_stream.bitrate_mode = fl_g->choice_stream_mp3_bitrate_mode->value();

        lame_enc_reinit(&lame_stream);
        lame_enc_close(&test_enc);
    }
    else {
        printf("choice_stream_mp3_bitrate_mode_cb: Illegal value\n");
    }
}
void choice_stream_mp3_vbr_quality_cb(void)
{
    lame_enc test_enc;

    test_enc = lame_stream;
    test_enc.gfp = NULL;
    test_enc.vbr_quality = fl_g->choice_stream_mp3_vbr_quality->value();

    if (lame_enc_init(&test_enc) == 0) {
        cfg.mp3_codec_stream.vbr_quality = fl_g->choice_stream_mp3_vbr_quality->value();
        lame_stream.vbr_quality = fl_g->choice_stream_mp3_vbr_quality->value();

        lame_enc_reinit(&lame_stream);
        lame_enc_close(&test_enc);
    }
    else {
        printf("choice_stream_mp3_vbr_quality_cb: Illegal value\n");
    }
}
void choice_stream_mp3_vbr_min_bitrate_cb(void)
{
    int *br_list;
    get_bitrate_list_for_codec(CHOICE_MP3, &br_list);

    lame_enc test_enc;

    test_enc = lame_stream;
    test_enc.gfp = NULL;
    test_enc.vbr_min_bitrate = br_list[fl_g->choice_stream_mp3_vbr_min_bitrate->value()];

    if (lame_enc_init(&test_enc) == 0) {
        cfg.mp3_codec_stream.vbr_min_bitrate = br_list[fl_g->choice_stream_mp3_vbr_min_bitrate->value()];
        lame_stream.vbr_min_bitrate = br_list[fl_g->choice_stream_mp3_vbr_min_bitrate->value()];

        lame_enc_reinit(&lame_stream);
        lame_enc_close(&test_enc);
    }
    else {
        printf("choice_stream_mp3_vbr_min_bitrate_cb: Illegal value\n");
    }
}
void choice_stream_mp3_vbr_max_bitrate_cb(void)
{
    int *br_list;
    get_bitrate_list_for_codec(CHOICE_MP3, &br_list);

    lame_enc test_enc;

    test_enc = lame_stream;
    test_enc.gfp = NULL;
    test_enc.vbr_max_bitrate = br_list[fl_g->choice_stream_mp3_vbr_max_bitrate->value()];

    if (lame_enc_init(&test_enc) == 0) {
        cfg.mp3_codec_stream.vbr_max_bitrate = br_list[fl_g->choice_stream_mp3_vbr_max_bitrate->value()];
        lame_stream.vbr_max_bitrate = br_list[fl_g->choice_stream_mp3_vbr_max_bitrate->value()];

        lame_enc_reinit(&lame_stream);
        lame_enc_close(&test_enc);
    }
    else {
        printf("choice_stream_mp3_vbr_max_bitrate_cb: Illegal value\n");
    }
}

void choice_stream_vorbis_bitrate_mode_cb(void)
{
    vorbis_enc test_enc;

    test_enc = vorbis_stream;
    test_enc.bitrate_mode = fl_g->choice_stream_vorbis_bitrate_mode->value();

    if (vorbis_enc_init(&test_enc) == 0) {
        cfg.vorbis_codec_stream.bitrate_mode = fl_g->choice_stream_vorbis_bitrate_mode->value();
        vorbis_stream.bitrate_mode = fl_g->choice_stream_vorbis_bitrate_mode->value();

        vorbis_enc_reinit(&vorbis_stream);
        vorbis_enc_close(&test_enc);
    }
    else {
        fl_g->choice_stream_vorbis_bitrate_mode->value(cfg.vorbis_codec_stream.bitrate_mode);
        printf("choice_stream_vorbis_bitrate_mode_cb: Illegal value\n");
    }
}

void choice_stream_vorbis_vbr_quality_cb(void)
{
    vorbis_enc test_enc;

    test_enc = vorbis_stream;
    test_enc.vbr_quality = 1.0 - (fl_g->choice_stream_vorbis_vbr_quality->value() * 0.1);

    if (vorbis_enc_init(&test_enc) == 0) {
        cfg.vorbis_codec_stream.vbr_quality = fl_g->choice_stream_vorbis_vbr_quality->value();
        vorbis_stream.vbr_quality = 1.0 - (fl_g->choice_stream_vorbis_vbr_quality->value() * 0.1);

        vorbis_enc_reinit(&vorbis_stream);
        vorbis_enc_close(&test_enc);
    }
    else {
        fl_g->choice_stream_vorbis_vbr_quality->value(cfg.vorbis_codec_stream.vbr_quality);
        printf("choice_stream_vorbis_vbr_quality_cb: Illegal value\n");
    }
}

void choice_stream_vorbis_vbr_min_bitrate_cb(void)
{
    uint32_t i;
    int num_of_bitrates;
    int selected_bitrate;
    int *br_list;
    vorbis_enc test_enc;

    num_of_bitrates = get_bitrate_list_for_codec(CHOICE_OGG, &br_list);

    test_enc = vorbis_stream;
    if (fl_g->choice_stream_vorbis_vbr_min_bitrate->value() > 0) {
        selected_bitrate = br_list[fl_g->choice_stream_vorbis_vbr_min_bitrate->value() - 1];
    }
    else {
        selected_bitrate = 0;
    }

    test_enc.vbr_min_bitrate = selected_bitrate;

    if (vorbis_enc_init(&test_enc) == 0) {
        cfg.vorbis_codec_stream.vbr_min_bitrate = selected_bitrate;
        vorbis_stream.vbr_min_bitrate = selected_bitrate;

        vorbis_enc_reinit(&vorbis_stream);
        vorbis_enc_close(&test_enc);
    }
    else { // Set to old value
        for (i = 0; i < num_of_bitrates; i++) {
            if (cfg.vorbis_codec_stream.vbr_min_bitrate == br_list[i]) {
                break;
            }
        }
        fl_g->choice_stream_vorbis_vbr_min_bitrate->value(i + 1);
        printf("choice_stream_vorbis_vbr_min_bitrate_cb: Illegal value\n");
    }
}

void choice_stream_vorbis_vbr_max_bitrate_cb(void)
{
    uint32_t i;
    int num_of_bitrates;
    int selected_bitrate;
    int *br_list;
    vorbis_enc test_enc;

    num_of_bitrates = get_bitrate_list_for_codec(CHOICE_OGG, &br_list);

    test_enc = vorbis_stream;

    if (fl_g->choice_stream_vorbis_vbr_max_bitrate->value() > 0) {
        selected_bitrate = br_list[fl_g->choice_stream_vorbis_vbr_max_bitrate->value() - 1];
    }
    else {
        selected_bitrate = 0;
    }
    test_enc.vbr_max_bitrate = selected_bitrate;

    if (vorbis_enc_init(&test_enc) == 0) {
        cfg.vorbis_codec_stream.vbr_max_bitrate = selected_bitrate;
        vorbis_stream.vbr_max_bitrate = selected_bitrate;

        vorbis_enc_reinit(&vorbis_stream);
        vorbis_enc_close(&test_enc);
    }
    else { // set to old value
        for (i = 0; i < num_of_bitrates; i++) {
            if (cfg.vorbis_codec_stream.vbr_max_bitrate == br_list[i]) {
                break;
            }
        }
        fl_g->choice_stream_vorbis_vbr_max_bitrate->value(i + 1);
        printf("choice_stream_vorbis_vbr_max_bitrate_cb: Illegal value\n");
    }
}

void choice_stream_opus_bitrate_mode_cb(void)
{
    opus_enc test_enc;

    test_enc = opus_stream;
    test_enc.bitrate_mode = fl_g->choice_stream_opus_bitrate_mode->value();

    if (opus_enc_init(&test_enc) == 0) {
        cfg.opus_codec_stream.bitrate_mode = fl_g->choice_stream_opus_bitrate_mode->value();
        opus_stream.bitrate_mode = fl_g->choice_stream_opus_bitrate_mode->value();

        opus_enc_init(&opus_stream);
        opus_enc_close(&test_enc);
    }
    else {
        fl_g->choice_stream_opus_bitrate_mode->value(cfg.opus_codec_stream.bitrate_mode);
        printf("choice_stream_opus_bitrate_mode_cb: Illegal value\n");
    }
}

void choice_stream_opus_audio_type_cb(void)
{
    opus_enc test_enc;

    test_enc = opus_stream;
    test_enc.audio_type = fl_g->choice_stream_opus_audio_type->value();

    if (opus_enc_init(&test_enc) == 0) {
        cfg.opus_codec_stream.audio_type = fl_g->choice_stream_opus_audio_type->value();
        opus_stream.audio_type = fl_g->choice_stream_opus_audio_type->value();

        opus_enc_init(&opus_stream);
        opus_enc_close(&test_enc);
    }
    else {
        fl_g->choice_stream_opus_audio_type->value(cfg.opus_codec_stream.audio_type);
        printf("choice_stream_opus_audio_type_cb: Illegal value\n");
    }
}

void choice_stream_opus_quality_cb(void)
{
    opus_enc test_enc;

    test_enc = opus_stream;
    test_enc.quality = fl_g->choice_stream_opus_quality->value();

    if (opus_enc_init(&test_enc) == 0) {
        cfg.opus_codec_stream.quality = fl_g->choice_stream_opus_quality->value();
        opus_stream.quality = fl_g->choice_stream_opus_quality->value();

        opus_enc_init(&opus_stream);
        opus_enc_close(&test_enc);
    }
    else {
        fl_g->choice_stream_opus_quality->value(cfg.opus_codec_stream.quality);
        printf("choice_stream_opus_quality_cb: Illegal value\n");
    }
}

void choice_stream_opus_bandwidth_cb(void)
{
    opus_enc test_enc;

    test_enc = opus_stream;
    test_enc.bandwidth = fl_g->choice_stream_opus_bandwidth->value();

    if (opus_enc_init(&test_enc) == 0) {
        cfg.opus_codec_stream.bandwidth = fl_g->choice_stream_opus_bandwidth->value();
        opus_stream.bandwidth = fl_g->choice_stream_opus_bandwidth->value();

        opus_enc_init(&opus_stream);
        opus_enc_close(&test_enc);
    }
    else {
        fl_g->choice_stream_opus_bandwidth->value(cfg.opus_codec_stream.bandwidth);
        printf("choice_stream_opus_bandwidth_cb: Illegal value\n");
    }
}

void choice_stream_aac_profile_cb(void)
{
#ifdef HAVE_LIBFDK_AAC
    aac_enc test_enc;

    test_enc = aac_stream;
    test_enc.profile = fl_g->choice_stream_aac_profile->value();

    if (aac_enc_init(&test_enc) == 0) {
        cfg.aac_codec_stream.profile = fl_g->choice_stream_aac_profile->value();
        aac_stream.profile = fl_g->choice_stream_aac_profile->value();

        aac_enc_init(&aac_stream);
        aac_enc_close(&test_enc);
    }
    else {
        fl_g->choice_stream_aac_profile->value(cfg.aac_codec_stream.profile);
        printf("choice_stream_aac_profile_cb: Illegal value\n");
    }
#endif
}

void choice_stream_aac_afterburner_cb(void)
{
#ifdef HAVE_LIBFDK_AAC
    aac_enc test_enc;

    test_enc = aac_stream;
    test_enc.afterburner = fl_g->choice_stream_aac_afterburner->value() == 0 ? 1 : 0;

    if (aac_enc_init(&test_enc) == 0) {
        cfg.aac_codec_stream.afterburner = fl_g->choice_stream_aac_afterburner->value();
        aac_stream.afterburner = fl_g->choice_stream_aac_afterburner->value() == 0 ? 1 : 0;

        aac_enc_init(&aac_stream);
        aac_enc_close(&test_enc);
    }
    else {
        fl_g->choice_stream_aac_afterburner->value(cfg.aac_codec_stream.afterburner);
        printf("choice_stream_aac_afterburner_cb: Illegal value\n");
    }
#endif
}

void choice_stream_aac_bitrate_mode_cb(void)
{
#ifdef HAVE_LIBFDK_AAC
    aac_enc test_enc;

    test_enc = aac_stream;
    test_enc.bitrate_mode = fl_g->choice_stream_aac_bitrate_mode->value();

    if (aac_enc_init(&test_enc) == 0) {
        cfg.aac_codec_stream.bitrate_mode = fl_g->choice_stream_aac_bitrate_mode->value();
        aac_stream.bitrate_mode = fl_g->choice_stream_aac_bitrate_mode->value();

        aac_enc_init(&aac_stream);
        aac_enc_close(&test_enc);
    }
    else {
        fl_g->choice_stream_aac_bitrate_mode->value(cfg.aac_codec_stream.bitrate_mode);
        printf("choice_stream_aac_bitrate_mode_cb: Illegal value\n");
    }
#endif
}

void radio_stream_flac_bit_depth_16_cb(void)
{
    flac_enc test_enc;

    test_enc = flac_stream;
    test_enc.bit_depth = 16;

    if (flac_enc_init(&test_enc) == 0) {
        cfg.flac_codec_stream.bit_depth = test_enc.bit_depth;
        flac_stream.bit_depth = test_enc.bit_depth;

        flac_enc_init(&flac_stream);
        flac_enc_close(&test_enc);
    }
    else {
        fl_g->radio_stream_flac_bit_depth_24->setonly();
        printf("radio_stream_flac_bit_depth_16_cb: Illegal value\n");
    }
}
void radio_stream_flac_bit_depth_24_cb(void)
{
    flac_enc test_enc;

    test_enc = flac_stream;
    test_enc.bit_depth = 24;

    if (flac_enc_init(&test_enc) == 0) {
        cfg.flac_codec_stream.bit_depth = test_enc.bit_depth;
        flac_stream.bit_depth = test_enc.bit_depth;

        flac_enc_init(&flac_stream);
        flac_enc_close(&test_enc);
    }
    else {
        fl_g->radio_stream_flac_bit_depth_16->setonly();
        printf("radio_stream_flac_bit_depth_24: Illegal value\n");
    }
}

void choice_rec_mp3_enc_quality_cb(void)
{
    lame_enc test_enc;

    test_enc = lame_rec;
    test_enc.gfp = NULL;
    test_enc.enc_quality = fl_g->choice_rec_mp3_enc_quality->value();

    if (lame_enc_init(&test_enc) == 0) {
        cfg.mp3_codec_rec.enc_quality = fl_g->choice_rec_mp3_enc_quality->value();
        lame_rec.enc_quality = fl_g->choice_rec_mp3_enc_quality->value();

        lame_enc_reinit(&lame_rec);
        lame_enc_close(&test_enc);
    }
    else {
        printf("choice_rec_mp3_enc_quality_cb: Illegal value\n");
    }
}
void choice_rec_mp3_stereo_mode_cb(void)
{
    lame_enc test_enc;

    test_enc = lame_rec;
    test_enc.gfp = NULL;
    test_enc.stereo_mode = fl_g->choice_rec_mp3_stereo_mode->value() - 1;

    if (lame_enc_init(&test_enc) == 0) {
        cfg.mp3_codec_rec.stereo_mode = fl_g->choice_rec_mp3_stereo_mode->value();
        lame_rec.stereo_mode = fl_g->choice_rec_mp3_stereo_mode->value() - 1;

        lame_enc_reinit(&lame_rec);
        lame_enc_close(&test_enc);
    }
    else {
        printf("choice_rec_mp3_stereo_mode_cb: Illegal value\n");
    }
}
void choice_rec_mp3_bitrate_mode_cb(void)
{
    lame_enc test_enc;

    test_enc = lame_rec;
    test_enc.gfp = NULL;
    test_enc.bitrate_mode = fl_g->choice_rec_mp3_bitrate_mode->value();

    if (lame_enc_init(&test_enc) == 0) {
        cfg.mp3_codec_rec.bitrate_mode = fl_g->choice_rec_mp3_bitrate_mode->value();
        lame_rec.bitrate_mode = fl_g->choice_rec_mp3_bitrate_mode->value();

        lame_enc_reinit(&lame_rec);
        lame_enc_close(&test_enc);
    }
    else {
        printf("choice_rec_mp3_bitrate_mode_cb: Illegal value\n");
    }
}
void choice_rec_mp3_vbr_quality_cb(void)
{
    lame_enc test_enc;

    test_enc = lame_rec;
    test_enc.gfp = NULL;
    test_enc.vbr_quality = fl_g->choice_rec_mp3_vbr_quality->value();

    if (lame_enc_init(&test_enc) == 0) {
        cfg.mp3_codec_rec.vbr_quality = fl_g->choice_rec_mp3_vbr_quality->value();
        lame_rec.vbr_quality = fl_g->choice_rec_mp3_vbr_quality->value();

        lame_enc_reinit(&lame_rec);
        lame_enc_close(&test_enc);
    }
    else {
        printf("choice_rec_mp3_vbr_quality_cb: Illegal value\n");
    }
}
void choice_rec_mp3_vbr_min_bitrate_cb(void)
{
    int *br_list;
    get_bitrate_list_for_codec(CHOICE_MP3, &br_list);

    lame_enc test_enc;

    test_enc = lame_rec;
    test_enc.gfp = NULL;
    test_enc.vbr_min_bitrate = br_list[fl_g->choice_rec_mp3_vbr_min_bitrate->value()];

    if (lame_enc_init(&test_enc) == 0) {
        cfg.mp3_codec_rec.vbr_min_bitrate = br_list[fl_g->choice_rec_mp3_vbr_min_bitrate->value()];
        lame_rec.vbr_min_bitrate = br_list[fl_g->choice_rec_mp3_vbr_min_bitrate->value()];

        lame_enc_reinit(&lame_rec);
        lame_enc_close(&test_enc);
    }
    else {
        printf("choice_rec_mp3_vbr_min_bitrate_cb: Illegal value\n");
    }
}
void choice_rec_mp3_vbr_max_bitrate_cb(void)
{
    int *br_list;
    get_bitrate_list_for_codec(CHOICE_MP3, &br_list);

    lame_enc test_enc;

    test_enc = lame_rec;
    test_enc.gfp = NULL;
    test_enc.vbr_max_bitrate = br_list[fl_g->choice_rec_mp3_vbr_max_bitrate->value()];

    if (lame_enc_init(&test_enc) == 0) {
        cfg.mp3_codec_rec.vbr_max_bitrate = br_list[fl_g->choice_rec_mp3_vbr_max_bitrate->value()];
        lame_rec.vbr_max_bitrate = br_list[fl_g->choice_rec_mp3_vbr_max_bitrate->value()];

        lame_enc_reinit(&lame_rec);
        lame_enc_close(&test_enc);
    }
    else {
        printf("choice_rec_mp3_vbr_max_bitrate_cb: Illegal value\n");
    }
}

void choice_rec_vorbis_bitrate_mode_cb(void)
{
    vorbis_enc test_enc;

    test_enc = vorbis_rec;
    test_enc.bitrate_mode = fl_g->choice_rec_vorbis_bitrate_mode->value();

    if (vorbis_enc_init(&test_enc) == 0) {
        cfg.vorbis_codec_rec.bitrate_mode = fl_g->choice_rec_vorbis_bitrate_mode->value();
        vorbis_rec.bitrate_mode = fl_g->choice_rec_vorbis_bitrate_mode->value();

        vorbis_enc_reinit(&vorbis_rec);
        vorbis_enc_close(&test_enc);
    }
    else {
        fl_g->choice_rec_vorbis_bitrate_mode->value(cfg.vorbis_codec_rec.bitrate_mode);
        printf("choice_rec_vorbis_bitrate_mode_cb: Illegal value\n");
    }
}

void choice_rec_vorbis_vbr_quality_cb(void)
{
    vorbis_enc test_enc;

    test_enc = vorbis_rec;
    test_enc.vbr_quality = 1.0 - (fl_g->choice_rec_vorbis_vbr_quality->value() * 0.1);

    if (vorbis_enc_init(&test_enc) == 0) {
        cfg.vorbis_codec_rec.vbr_quality = fl_g->choice_rec_vorbis_vbr_quality->value();
        vorbis_rec.vbr_quality = 1.0 - (fl_g->choice_rec_vorbis_vbr_quality->value() * 0.1);

        vorbis_enc_reinit(&vorbis_rec);
        vorbis_enc_close(&test_enc);
    }
    else {
        fl_g->choice_rec_vorbis_vbr_quality->value(cfg.vorbis_codec_rec.vbr_quality);
        printf("choice_rec_vorbis_vbr_quality_cb: Illegal value\n");
    }
}

void choice_rec_vorbis_vbr_min_bitrate_cb(void)
{
    uint32_t i;
    int num_of_bitrates;
    int selected_bitrate;
    int *br_list;
    vorbis_enc test_enc;

    num_of_bitrates = get_bitrate_list_for_codec(CHOICE_OGG, &br_list);

    test_enc = vorbis_rec;
    selected_bitrate = br_list[fl_g->choice_rec_vorbis_vbr_min_bitrate->value() - 1];

    if (fl_g->choice_rec_vorbis_vbr_min_bitrate->value() > 0) {
        selected_bitrate = br_list[fl_g->choice_rec_vorbis_vbr_min_bitrate->value() - 1];
    }
    else {
        selected_bitrate = 0;
    }
    test_enc.vbr_min_bitrate = selected_bitrate;

    if (vorbis_enc_init(&test_enc) == 0) {
        cfg.vorbis_codec_rec.vbr_min_bitrate = selected_bitrate;
        vorbis_rec.vbr_min_bitrate = selected_bitrate;

        vorbis_enc_reinit(&vorbis_rec);
        vorbis_enc_close(&test_enc);
    }
    else {
        for (i = 0; i < num_of_bitrates; i++) {
            if (cfg.vorbis_codec_rec.vbr_min_bitrate == br_list[i]) {
                break;
            }
        }
        fl_g->choice_rec_vorbis_vbr_min_bitrate->value(i + 1);
        printf("choice_rec_vorbis_vbr_min_bitrate_cb: Illegal value\n");
    }
}

void choice_rec_vorbis_vbr_max_bitrate_cb(void)
{
    uint32_t i;
    int num_of_bitrates;
    int selected_bitrate;
    int *br_list;
    vorbis_enc test_enc;

    num_of_bitrates = get_bitrate_list_for_codec(CHOICE_OGG, &br_list);

    test_enc = vorbis_rec;

    if (fl_g->choice_rec_vorbis_vbr_max_bitrate->value() > 0) {
        selected_bitrate = br_list[fl_g->choice_rec_vorbis_vbr_max_bitrate->value() - 1];
    }
    else {
        selected_bitrate = 0;
    }
    test_enc.vbr_max_bitrate = selected_bitrate;

    if (vorbis_enc_init(&test_enc) == 0) {
        cfg.vorbis_codec_rec.vbr_max_bitrate = selected_bitrate;
        vorbis_rec.vbr_max_bitrate = selected_bitrate;

        vorbis_enc_reinit(&vorbis_rec);
        vorbis_enc_close(&test_enc);
    }
    else {
        for (i = 0; i < num_of_bitrates; i++) {
            if (cfg.vorbis_codec_rec.vbr_max_bitrate == br_list[i]) {
                break;
            }
        }
        fl_g->choice_rec_vorbis_vbr_max_bitrate->value(i + 1);
        printf("choice_rec_vorbis_vbr_max_bitrate_cb: Illegal value\n");
    }
}

void choice_rec_opus_bitrate_mode_cb(void)
{
    opus_enc test_enc;

    test_enc = opus_rec;
    test_enc.bitrate_mode = fl_g->choice_rec_opus_bitrate_mode->value();

    if (opus_enc_init(&test_enc) == 0) {
        cfg.opus_codec_rec.bitrate_mode = fl_g->choice_rec_opus_bitrate_mode->value();
        opus_rec.bitrate_mode = fl_g->choice_rec_opus_bitrate_mode->value();

        opus_enc_init(&opus_rec);
        opus_enc_close(&test_enc);
    }
    else {
        fl_g->choice_rec_opus_bitrate_mode->value(cfg.opus_codec_rec.bitrate_mode);
        printf("choice_rec_opus_bitrate_mode_cb: Illegal value\n");
    }
}

void choice_rec_opus_audio_type_cb(void)
{
    opus_enc test_enc;

    test_enc = opus_rec;
    test_enc.audio_type = fl_g->choice_rec_opus_audio_type->value();

    if (opus_enc_init(&test_enc) == 0) {
        cfg.opus_codec_rec.audio_type = fl_g->choice_rec_opus_audio_type->value();
        opus_rec.audio_type = fl_g->choice_rec_opus_audio_type->value();

        opus_enc_init(&opus_rec);
        opus_enc_close(&test_enc);
    }
    else {
        fl_g->choice_rec_opus_audio_type->value(cfg.opus_codec_rec.audio_type);
        printf("choice_rec_opus_audio_type_cb: Illegal value\n");
    }
}

void choice_rec_opus_quality_cb(void)
{
    opus_enc test_enc;

    test_enc = opus_rec;
    test_enc.quality = fl_g->choice_rec_opus_quality->value();

    if (opus_enc_init(&test_enc) == 0) {
        cfg.opus_codec_rec.quality = fl_g->choice_rec_opus_quality->value();
        opus_rec.quality = fl_g->choice_rec_opus_quality->value();

        opus_enc_init(&opus_rec);
        opus_enc_close(&test_enc);
    }
    else {
        fl_g->choice_rec_opus_quality->value(cfg.opus_codec_rec.quality);
        printf("choice_rec_opus_quality_cb: Illegal value\n");
    }
}

void choice_rec_opus_bandwidth_cb(void)
{
    opus_enc test_enc;

    test_enc = opus_rec;
    test_enc.bandwidth = fl_g->choice_rec_opus_bandwidth->value();

    if (opus_enc_init(&test_enc) == 0) {
        cfg.opus_codec_rec.bandwidth = fl_g->choice_rec_opus_bandwidth->value();
        opus_rec.bandwidth = fl_g->choice_rec_opus_bandwidth->value();

        opus_enc_init(&opus_rec);
        opus_enc_close(&test_enc);
    }
    else {
        fl_g->choice_rec_opus_bandwidth->value(cfg.opus_codec_rec.bandwidth);
        printf("choice_rec_opus_bandwidth_cb: Illegal value\n");
    }
}

void choice_rec_aac_profile_cb(void)
{
#ifdef HAVE_LIBFDK_AAC
    aac_enc test_enc;

    test_enc = aac_rec;
    test_enc.profile = fl_g->choice_rec_aac_profile->value();

    if (aac_enc_init(&test_enc) == 0) {
        cfg.aac_codec_rec.profile = fl_g->choice_rec_aac_profile->value();
        aac_rec.profile = fl_g->choice_rec_aac_profile->value();

        aac_enc_init(&aac_rec);
        aac_enc_close(&test_enc);
    }
    else {
        fl_g->choice_rec_aac_profile->value(cfg.aac_codec_rec.profile);
        printf("choice_rec_aac_profile_cb: Illegal value\n");
    }
#endif
}

void choice_rec_aac_afterburner_cb(void)
{
#ifdef HAVE_LIBFDK_AAC
    aac_enc test_enc;

    test_enc = aac_rec;
    test_enc.afterburner = fl_g->choice_rec_aac_afterburner->value() == 0 ? 1 : 0;

    if (aac_enc_init(&test_enc) == 0) {
        cfg.aac_codec_rec.afterburner = fl_g->choice_rec_aac_afterburner->value();
        aac_rec.afterburner = fl_g->choice_rec_aac_afterburner->value() == 0 ? 1 : 0;

        aac_enc_init(&aac_rec);
        aac_enc_close(&test_enc);
    }
    else {
        fl_g->choice_rec_aac_afterburner->value(cfg.aac_codec_rec.afterburner);
        printf("choice_rec_aac_afterburner_cb: Illegal value\n");
    }
#endif
}

void choice_rec_aac_bitrate_mode_cb(void)
{
#ifdef HAVE_LIBFDK_AAC
    aac_enc test_enc;

    test_enc = aac_rec;
    test_enc.bitrate_mode = fl_g->choice_rec_aac_bitrate_mode->value();

    if (aac_enc_init(&test_enc) == 0) {
        cfg.aac_codec_rec.bitrate_mode = fl_g->choice_rec_aac_bitrate_mode->value();
        aac_rec.bitrate_mode = fl_g->choice_rec_aac_bitrate_mode->value();

        aac_enc_init(&aac_rec);
        aac_enc_close(&test_enc);
    }
    else {
        fl_g->choice_rec_aac_bitrate_mode->value(cfg.aac_codec_rec.bitrate_mode);
        printf("choice_rec_aac_bitrate_mode_cb: Illegal value\n");
    }

#endif
}

void radio_rec_flac_bit_depth_16_cb(void)
{
    flac_enc test_enc;

    test_enc = flac_rec;
    test_enc.bit_depth = 16;

    if (flac_enc_init(&test_enc) == 0) {
        cfg.flac_codec_rec.bit_depth = test_enc.bit_depth;
        flac_rec.bit_depth = test_enc.bit_depth;

        flac_enc_init(&flac_rec);
        flac_enc_close(&test_enc);
    }
    else {
        fl_g->radio_rec_flac_bit_depth_24->setonly();
        printf("radio_rec_flac_bit_depth_16_cb: Illegal value\n");
    }
}

void radio_rec_flac_bit_depth_24_cb(void)
{
    flac_enc test_enc;

    test_enc = flac_rec;
    test_enc.bit_depth = 24;

    if (flac_enc_init(&test_enc) == 0) {
        cfg.flac_codec_rec.bit_depth = test_enc.bit_depth;
        flac_rec.bit_depth = test_enc.bit_depth;

        flac_enc_init(&flac_rec);
        flac_enc_close(&test_enc);
    }
    else {
        fl_g->radio_rec_flac_bit_depth_16->setonly();
        printf("radio_rec_flac_bit_depth_24: Illegal value\n");
    }
}

void radio_rec_wav_bit_depth_16_cb(void)
{
    cfg.wav_codec_rec.bit_depth = 16;
}
void radio_rec_wav_bit_depth_24_cb(void)
{
    cfg.wav_codec_rec.bit_depth = 24;
}
void radio_rec_wav_bit_depth_32_cb(void)
{
    cfg.wav_codec_rec.bit_depth = 32;
}

void choice_midi_dev_cb(void)
{
    char info_buf[256];
    int dev_id = fl_g->choice_midi_dev->value() - 1;
    const char *dev_name = fl_g->choice_midi_dev->text();

    midi_close();
    if (dev_id > -1) {
        if (midi_open_device((char *)dev_name) != 0) {
            fl_alert(_("MIDI device could not be opened."));
            goto set_disabled_device;
        }
        if (midi_start_polling() == 0) {
            cfg.midi.dev_name = (char *)realloc(cfg.midi.dev_name, strlen(dev_name) + 1);
            strcpy(cfg.midi.dev_name, dev_name);
            snprintf(info_buf, sizeof(info_buf), _("MIDI device:\n%s\n"), cfg.midi.dev_name);
            print_info(info_buf, 1);
            return;
        }
        else {
            fl_alert(_("MIDI device could not be started."));
            goto set_disabled_device;
        }
    }
    else {
        print_info(_("MIDI support disabled\n"), 1);
        goto set_disabled_device;
    }

set_disabled_device:
    fl_g->choice_midi_dev->value(0);
    cfg.midi.dev_name = (char *)realloc(cfg.midi.dev_name, strlen("Disabled") + 1);
    strcpy(cfg.midi.dev_name, "Disabled");
}

void button_midi_rescan_devices_cb(void)
{
    // PortMidi needs a complete new initialization to detect device changes
    midi_close();
    midi_terminate();
    midi_init();

    // Populate MIDI device list and select the device from config file in case it was found
    populate_midi_dev_list();

    // Initialize the MIDI device in case it was found
    if (fl_g->choice_midi_dev->value() > 0) {
        choice_midi_dev_cb();
    }
}

void browser_midi_command_cb(void)
{
    int selected_line = fl_g->browser_midi_command->value() - 1;

    // Select last line when user clicks on any empty space in the command browser
    if (selected_line == -1) {
        selected_line = fl_g->browser_midi_command->size() - 1;
        fl_g->browser_midi_command->value(selected_line + 1);
    }

    fl_g->choice_midi_channel->value(cfg.midi.commands[selected_line].channel);
    fl_g->input_midi_msg_num->value(cfg.midi.commands[selected_line].msg_num);
    fl_g->choice_midi_cc_mode->value(cfg.midi.commands[selected_line].mode);
    fl_g->check_midi_soft_takeover->value(cfg.midi.commands[selected_line].soft_takeover);
    fl_g->check_midi_command_enable->value(cfg.midi.commands[selected_line].enabled);

    // Update accessibility of UI elements that depend on the CC mode
    fl_g->choice_midi_cc_mode->do_callback();
    fl_g->check_midi_command_enable->do_callback();

    if (get_midi_ctrl_type(selected_line) == MIDI_CTRL_TYPE_KNOB) {
        fl_g->choice_midi_cc_mode->activate();
        fl_g->check_midi_soft_takeover->activate();
    }
    else {
        fl_g->choice_midi_cc_mode->deactivate();
        fl_g->check_midi_soft_takeover->deactivate();
    }
}

void input_midi_msg_num_cb(void)
{
    int selected_line = fl_g->browser_midi_command->value() - 1;
    int msg_num = fl_g->input_midi_msg_num->value();

    if (msg_num < 0 || msg_num > 127) {
        fl_alert(_("The CC value must be between 0 and 127"));
        fl_g->input_midi_msg_num->value(cfg.midi.commands[selected_line].msg_num);
        return;
    }

    cfg.midi.commands[selected_line].msg_num = fl_g->input_midi_msg_num->value();
}

void choice_midi_channel_cb(void)
{
    int selected_line = fl_g->browser_midi_command->value() - 1;
    cfg.midi.commands[selected_line].channel = fl_g->choice_midi_channel->value();
}

void button_midi_learn_cb(void)
{
    int pushed_down = fl_g->button_midi_learn->value();
    printf("pushed_down: %d\n", pushed_down);

    if (pushed_down == 0) {
        fl_g->button_midi_learn->label(_("Learn"));
        midi_set_learning(0);
    }
    else {
        fl_g->button_midi_learn->label(_("Waiting..."));
        midi_set_learning(1);
    }
}

void choice_midi_cc_mode_cb(void)
{
    int selected_line = fl_g->browser_midi_command->value() - 1;
    cfg.midi.commands[selected_line].mode = fl_g->choice_midi_cc_mode->value();

    if (fl_g->choice_midi_cc_mode->value() == MIDI_MODE_ABSOLUTE) {
        fl_g->check_midi_soft_takeover->activate();
    }
    else { // MIDI_MODE_RELATIVE_*
        fl_g->check_midi_soft_takeover->deactivate();
    }
}

void check_midi_soft_takeover_cb(void)
{
    int selected_line = fl_g->browser_midi_command->value() - 1;
    cfg.midi.commands[selected_line].soft_takeover = fl_g->check_midi_soft_takeover->value();
}

void check_midi_command_enable_cb(void)
{
    int enabled = fl_g->check_midi_command_enable->value();
    int selected_line = fl_g->browser_midi_command->value() - 1;

    cfg.midi.commands[selected_line].enabled = enabled;

    if (enabled == 1) {
        fl_g->group_midi_command_settings->activate();
    }
    else {
        fl_g->group_midi_command_settings->deactivate();
    }
}

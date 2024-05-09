// config functions for butt
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

#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "config.h"
#include "gettext.h"

#include "cfg.h"
#include "butt.h"
#include "flgui.h"
#include "util.h"
#include "fl_funcs.h"
#include "strfuncs.h"
#include "port_midi.h"

#ifdef WIN32
const char CONFIG_FILE[] = "buttrc";
#else
const char CONFIG_FILE[] = ".buttrc";
#endif

const char *lang_array_new[] = {"system", "ar", "de", "en", "es", "fr", "nl", "pt_BR"};
const char *lang_array_old[] = {"system", "de", "en", "fr", "pt_BR"};

config_t cfg;
char *cfg_path;

int cfg_write_file(char *path)
{
    int i;
    FILE *cfg_fd;
    char info_buf[256];

    if (path == NULL) {
        path = cfg_path;
    }

    cfg_fd = fl_fopen(path, "wb");
    if (cfg_fd == NULL) {
        snprintf(info_buf, sizeof(info_buf), _("Could not write to file: %s"), path);
        print_info(cfg_path, 1);
        return 1;
    }

    fprintf(cfg_fd, "#This is a configuration file for butt (broadcast using this tool)\n\n");
    fprintf(cfg_fd, "[main]\n");

    fprintf(cfg_fd, "bg_color = %d\n", cfg.main.bg_color);
    fprintf(cfg_fd, "txt_color = %d\n", cfg.main.txt_color);

    if (cfg.main.num_of_srv > 0) {
        fprintf(cfg_fd,
                "server = %s\n"
                "srv_ent = %s\n",
                cfg.main.srv, cfg.main.srv_ent);
    }
    else {
        fprintf(cfg_fd, "server = \n"
                        "srv_ent = \n");
    }

    if (cfg.main.num_of_icy > 0) {
        fprintf(cfg_fd,
                "icy = %s\n"
                "icy_ent = %s\n",
                cfg.main.icy, cfg.main.icy_ent);
    }
    else {
        fprintf(cfg_fd, "icy = \n"
                        "icy_ent = \n");
    }

    fprintf(cfg_fd,
            "num_of_srv = %d\n"
            "num_of_icy = %d\n",
            cfg.main.num_of_srv, cfg.main.num_of_icy);

    if (cfg.main.song_path != NULL) {
        fprintf(cfg_fd, "song_path = %s\n", cfg.main.song_path);
    }
    else {
        fprintf(cfg_fd, "song_path = \n");
    }

    fprintf(cfg_fd, "song_update = %d\n", cfg.main.song_update);
    fprintf(cfg_fd, "song_delay = %d\n", cfg.main.song_delay);

    if (cfg.main.song_prefix != NULL) {
        char *tmp = strdup(cfg.main.song_prefix);
        strrpl(&tmp, (char *)" ", (char *)"%20", MODE_ALL);
        fprintf(cfg_fd, "song_prefix = %s\n", tmp);
        free(tmp);
    }
    else
        fprintf(cfg_fd, "song_prefix = \n");

    if (cfg.main.song_suffix != NULL) {
        char *tmp = strdup(cfg.main.song_suffix);
        strrpl(&tmp, (char *)" ", (char *)"%20", MODE_ALL);
        fprintf(cfg_fd, "song_suffix = %s\n", tmp);
        free(tmp);
    }
    else
        fprintf(cfg_fd, "song_suffix = \n");

    fprintf(cfg_fd, "read_last_line = %d\n", cfg.main.read_last_line);

    fprintf(cfg_fd, "app_update_service = %d\n", cfg.main.app_update_service);
    fprintf(cfg_fd, "app_update = %d\n", cfg.main.app_update);
    fprintf(cfg_fd, "app_artist_title_order = %d\n", cfg.main.app_artist_title_order);

    fprintf(cfg_fd, "gain = %f\n", cfg.main.gain);

    fprintf(cfg_fd, "signal_threshold = %d\n", cfg.main.signal_threshold);
    fprintf(cfg_fd, "silence_threshold = %d\n", cfg.main.silence_threshold);
    fprintf(cfg_fd, "signal_detection = %d\n", cfg.main.signal_detection);
    fprintf(cfg_fd, "silence_detection = %d\n", cfg.main.silence_detection);
    fprintf(cfg_fd, "check_for_update = %d\n", cfg.main.check_for_update);
    fprintf(cfg_fd, "start_agent = %d\n", cfg.main.start_agent);
    fprintf(cfg_fd, "minimize_to_tray = %d\n", cfg.main.minimize_to_tray);
    fprintf(cfg_fd, "connect_at_startup = %d\n", cfg.main.connect_at_startup);
    fprintf(cfg_fd, "force_reconnecting = %d\n", cfg.main.force_reconnecting);
    fprintf(cfg_fd, "reconnect_delay = %d\n", cfg.main.reconnect_delay);

    if (cfg.main.ic_charset != NULL) {
        fprintf(cfg_fd, "ic_charset = %s\n", cfg.main.ic_charset);
    }
    else {
        fprintf(cfg_fd, "ic_charset = \n");
    }

    if (cfg.main.log_file != NULL) {
        fprintf(cfg_fd, "log_file = %s\n\n", cfg.main.log_file);
    }
    else {
        fprintf(cfg_fd, "log_file = \n\n");
    }

    fprintf(cfg_fd,
            "[audio]\n"
            "device = %d\n"
            "device2 = %d\n"
            "dev_remember = %d\n"
            "samplerate = %d\n"
            "bitrate = %d\n"
            "channel = %d\n"
            "left_ch = %d\n"
            "right_ch = %d\n"
            "left_ch2 = %d\n"
            "right_ch2 = %d\n"
            "codec = %s\n"
            "resample_mode = %d\n"
            "silence_level = %f\n"
            "signal_level = %f\n"
            "disable_dithering = %d\n"
            "buffer_ms = %d\n",
            cfg.audio.dev_num, cfg.audio.dev2_num, cfg.audio.dev_remember, cfg.audio.samplerate, cfg.audio.bitrate, cfg.audio.channel, cfg.audio.left_ch,
            cfg.audio.right_ch, cfg.audio.left_ch2, cfg.audio.right_ch2, cfg.audio.codec, cfg.audio.resample_mode, cfg.audio.silence_level,
            cfg.audio.signal_level, cfg.audio.disable_dithering, cfg.audio.buffer_ms);

    if (cfg.audio.dev_name != NULL) {
        fprintf(cfg_fd, "dev_name = %s\n", cfg.audio.dev_name);
    }
    else {
        fprintf(cfg_fd, "dev_name = \n");
    }

    if (cfg.audio.dev2_name != NULL) {
        fprintf(cfg_fd, "dev2_name = %s\n\n", cfg.audio.dev2_name);
    }
    else {
        fprintf(cfg_fd, "dev2_name = \n\n");
    }

    fprintf(cfg_fd,
            "[record]\n"
            "bitrate = %d\n"
            "codec = %s\n"
            "start_rec = %d\n"
            "stop_rec = %d\n"
            "rec_after_launch = %d\n"
            "sync_to_hour  = %d\n"
            "split_time = %d\n"
            "filename = %s\n"
            "signal_threshold = %d\n"
            "silence_threshold = %d\n"
            "signal_detection = %d\n"
            "silence_detection = %d\n"
            "folder = %s\n\n",
            cfg.rec.bitrate, cfg.rec.codec, cfg.rec.start_rec, cfg.rec.stop_rec, cfg.rec.rec_after_launch, cfg.rec.sync_to_hour, cfg.rec.split_time,
            cfg.rec.filename, cfg.rec.signal_threshold, cfg.rec.silence_threshold, cfg.rec.signal_detection, cfg.rec.silence_detection, cfg.rec.folder);

    fprintf(cfg_fd,
            "[tls]\n"
            "cert_file = %s\n"
            "cert_dir = %s\n\n",
            cfg.tls.cert_file != NULL ? cfg.tls.cert_file : "", cfg.tls.cert_dir != NULL ? cfg.tls.cert_dir : "");

    fprintf(cfg_fd,
            "[dsp]\n"
            "equalizer = %d\n"
            "equalizer_rec = %d\n"
            "eq_preset = %s\n"
            "gain1 = %f\n"
            "gain2 = %f\n"
            "gain3 = %f\n"
            "gain4 = %f\n"
            "gain5 = %f\n"
            "gain6 = %f\n"
            "gain7 = %f\n"
            "gain8 = %f\n"
            "gain9 = %f\n"
            "gain10 = %f\n"
            "compressor = %d\n"
            "compressor_rec = %d\n"
            "aggressive_mode = %d\n"
            "threshold = %f\n"
            "ratio = %f\n"
            "attack = %f\n"
            "release = %f\n"
            "makeup_gain = %f\n\n",
            cfg.dsp.equalizer_stream, cfg.dsp.equalizer_rec, cfg.dsp.eq_preset, cfg.dsp.eq_gain[0], cfg.dsp.eq_gain[1], cfg.dsp.eq_gain[2], cfg.dsp.eq_gain[3],
            cfg.dsp.eq_gain[4], cfg.dsp.eq_gain[5], cfg.dsp.eq_gain[6], cfg.dsp.eq_gain[7], cfg.dsp.eq_gain[8], cfg.dsp.eq_gain[9], cfg.dsp.compressor_stream,
            cfg.dsp.compressor_rec, cfg.dsp.aggressive_mode, cfg.dsp.threshold, cfg.dsp.ratio, cfg.dsp.attack, cfg.dsp.release, cfg.dsp.makeup_gain);

    fprintf(cfg_fd,
            "[mixer]\n"
            "primary_device_gain = %f\n"
            "primary_device_muted = %d\n"
            "secondary_device_gain = %f\n"
            "secondary_device_muted = %d\n"
            "streaming_gain = %f\n"
            "recording_gain = %f\n"
            "cross_fader = %f\n\n",
            cfg.mixer.primary_device_gain, cfg.mixer.primary_device_muted, cfg.mixer.secondary_device_gain, cfg.mixer.secondary_device_muted,
            cfg.mixer.streaming_gain, cfg.mixer.recording_gain, cfg.mixer.cross_fader);

    fprintf(cfg_fd,
            "[gui]\n"
            "attach = %d\n"
            "ontop = %d\n"
            "hide_log_window = %d\n"
            "remember_pos = %d\n"
            "x_pos = %d\n"
            "y_pos = %d\n"
            "window_height = %d\n"
            "lcd_auto = %d\n"
            "start_minimized = %d\n"
            "disable_gain_slider = %d\n"
            "show_listeners = %d\n"
            "lang_str = %s\n"
            "always_show_vu_tabs = %d\n"
            "window_title = %s\n"
            "vu_mode = %d\n\n",
            cfg.gui.attach, cfg.gui.ontop, cfg.gui.hide_log_window, cfg.gui.remember_pos, cfg.gui.x_pos, cfg.gui.y_pos, cfg.gui.window_height, cfg.gui.lcd_auto,
            cfg.gui.start_minimized, cfg.gui.disable_gain_slider, cfg.gui.show_listeners, cfg.gui.lang_str, cfg.gui.always_show_vu_tabs, cfg.gui.window_title,
            cfg.gui.vu_mode);

    fprintf(cfg_fd,
            "[mp3_codec_stream]\n"
            "enc_quality = %d\n"
            "stereo_mode = %d\n"
            "bitrate_mode = %d\n"
            "vbr_quality = %d\n"
            "vbr_min_bitrate = %d\n"
            "vbr_max_bitrate = %d\n\n",
            cfg.mp3_codec_stream.enc_quality, cfg.mp3_codec_stream.stereo_mode, cfg.mp3_codec_stream.bitrate_mode, cfg.mp3_codec_stream.vbr_quality,
            cfg.mp3_codec_stream.vbr_min_bitrate, cfg.mp3_codec_stream.vbr_max_bitrate);

    fprintf(cfg_fd,
            "[mp3_codec_rec]\n"
            "enc_quality = %d\n"
            "stereo_mode = %d\n"
            "bitrate_mode = %d\n"
            "vbr_quality = %d\n"
            "vbr_min_bitrate = %d\n"
            "vbr_max_bitrate = %d\n\n",
            cfg.mp3_codec_rec.enc_quality, cfg.mp3_codec_rec.stereo_mode, cfg.mp3_codec_rec.bitrate_mode, cfg.mp3_codec_rec.vbr_quality,
            cfg.mp3_codec_rec.vbr_min_bitrate, cfg.mp3_codec_rec.vbr_max_bitrate);

    fprintf(cfg_fd,
            "[vorbis_codec_stream]\n"
            "bitrate_mode = %d\n"
            "vbr_quality = %d\n"
            "vbr_min_bitrate = %d\n"
            "vbr_max_bitrate = %d\n\n",
            cfg.vorbis_codec_stream.bitrate_mode, cfg.vorbis_codec_stream.vbr_quality, cfg.vorbis_codec_stream.vbr_min_bitrate,
            cfg.vorbis_codec_stream.vbr_max_bitrate);

    fprintf(cfg_fd,
            "[vorbis_codec_rec]\n"
            "bitrate_mode = %d\n"
            "vbr_quality = %d\n"
            "vbr_min_bitrate = %d\n"
            "vbr_max_bitrate = %d\n\n",
            cfg.vorbis_codec_rec.bitrate_mode, cfg.vorbis_codec_rec.vbr_quality, cfg.vorbis_codec_rec.vbr_min_bitrate, cfg.vorbis_codec_rec.vbr_max_bitrate);

    fprintf(cfg_fd,
            "[opus_codec_stream]\n"
            "bitrate_mode = %d\n"
            "quality = %d\n"
            "audio_type = %d\n"
            "bandwidth = %d\n\n",
            cfg.opus_codec_stream.bitrate_mode, cfg.opus_codec_stream.quality, cfg.opus_codec_stream.audio_type, cfg.opus_codec_stream.bandwidth);

    fprintf(cfg_fd,
            "[opus_codec_rec]\n"
            "bitrate_mode = %d\n"
            "quality = %d\n"
            "audio_type = %d\n"
            "bandwidth = %d\n\n",
            cfg.opus_codec_rec.bitrate_mode, cfg.opus_codec_rec.quality, cfg.opus_codec_rec.audio_type, cfg.opus_codec_rec.bandwidth);

    fprintf(cfg_fd,
            "[aac_codec_stream]\n"
            "bitrate_mode = %d\n"
            "afterburner = %d\n"
            "profile = %d\n\n",
            cfg.aac_codec_stream.bitrate_mode, cfg.aac_codec_stream.afterburner, cfg.aac_codec_stream.profile);

    fprintf(cfg_fd,
            "[aac_codec_rec]\n"
            "bitrate_mode = %d\n"
            "afterburner = %d\n"
            "profile = %d\n\n",
            cfg.aac_codec_rec.bitrate_mode, cfg.aac_codec_rec.afterburner, cfg.aac_codec_rec.profile);

    fprintf(cfg_fd,
            "[flac_codec_stream]\n"
            "bit_depth = %d\n\n",
            cfg.flac_codec_stream.bit_depth);

    fprintf(cfg_fd,
            "[flac_codec_rec]\n"
            "bit_depth = %d\n\n",
            cfg.flac_codec_rec.bit_depth);

    fprintf(cfg_fd,
            "[wav_codec_rec]\n"
            "bit_depth = %d\n\n",
            cfg.wav_codec_rec.bit_depth);

    fprintf(cfg_fd,
            "[midi]\n"
            "dev_name = %s\n\n",
            cfg.midi.dev_name);

    for (i = 0; i < MIDI_COMMANDS_COUNT; i++) {
        fprintf(cfg_fd,
                "[midi_cmd_%u]\n"
                "enabled = %u\n"
                "channel = %u\n"
                "msg_num = %u\n"
                "msg_type = %u\n"
                "mode = %u\n"
                "soft_takeover = %u\n\n",
                i, cfg.midi.commands[i].enabled, cfg.midi.commands[i].channel, cfg.midi.commands[i].msg_num, cfg.midi.commands[i].msg_type,
                cfg.midi.commands[i].mode, cfg.midi.commands[i].soft_takeover);
    }

    for (i = 0; i < cfg.main.num_of_srv; i++) {
        fprintf(cfg_fd,
                "[%s]\n"
                "address = %s\n"
                "port = %u\n"
                "password = %s\n"
                "type = %d\n"
                "tls = %d\n",
                cfg.srv[i]->name, cfg.srv[i]->addr, cfg.srv[i]->port, cfg.srv[i]->pwd, cfg.srv[i]->type, cfg.srv[i]->tls);

        if (cfg.srv[i]->cert_hash != NULL) {
            fprintf(cfg_fd, "cert_hash = %s\n", cfg.srv[i]->cert_hash);
        }
        else {
            fprintf(cfg_fd, "cert_hash = \n");
        }

        if (cfg.srv[i]->type == ICECAST) {
            fprintf(cfg_fd, "mount = %s\n", cfg.srv[i]->mount);
            fprintf(cfg_fd, "usr = %s\n", cfg.srv[i]->usr);
            fprintf(cfg_fd, "protocol = %d\n\n", cfg.srv[i]->icecast_protocol);
        }
        else { // Shoutcast has no mountpoint and user
            fprintf(cfg_fd, "mount = (none)\n");
            fprintf(cfg_fd, "usr = (none)\n\n");
        }
    }

    for (i = 0; i < cfg.main.num_of_icy; i++) {
        fprintf(cfg_fd,
                "[%s]\n"
                "expand_variables = %d\n"
                "pub = %s\n",
                cfg.icy[i]->name,             //
                cfg.icy[i]->expand_variables, //
                cfg.icy[i]->pub);

        if (cfg.icy[i]->desc != NULL) {
            fprintf(cfg_fd, "description = %s\n", cfg.icy[i]->desc);
        }
        else {
            fprintf(cfg_fd, "description = \n");
        }

        if (cfg.icy[i]->genre != NULL) {
            fprintf(cfg_fd, "genre = %s\n", cfg.icy[i]->genre);
        }
        else {
            fprintf(cfg_fd, "genre = \n");
        }

        if (cfg.icy[i]->url != NULL) {
            fprintf(cfg_fd, "url = %s\n", cfg.icy[i]->url);
        }
        else {
            fprintf(cfg_fd, "url = \n");
        }

        if (cfg.icy[i]->irc != NULL) {
            fprintf(cfg_fd, "irc = %s\n", cfg.icy[i]->irc);
        }
        else {
            fprintf(cfg_fd, "irc = \n");
        }

        if (cfg.icy[i]->icq != NULL) {
            fprintf(cfg_fd, "icq = %s\n", cfg.icy[i]->icq);
        }
        else {
            fprintf(cfg_fd, "icq = \n");
        }

        if (cfg.icy[i]->aim != NULL) {
            fprintf(cfg_fd, "aim = %s\n\n", cfg.icy[i]->aim);
        }
        else {
            fprintf(cfg_fd, "aim = \n\n");
        }
    }

    fclose(cfg_fd);

    snprintf(info_buf, sizeof(info_buf), _("Config written to %s"), path);
    print_info(info_buf, 0);

    return 0;
}

int cfg_set_values(char *path)
{
    int i;
    char *srv_ent;
    char *icy_ent;
    char *strtok_buf = NULL;

    if (path == NULL) {
        path = cfg_path;
    }

    if (cfg_parse_file(path) == -1) {
        return 1;
    }

    cfg.main.log_file = cfg_get_str("main", "log_file", NULL);
    cfg.main.ic_charset = cfg_get_str("main", "ic_charset", NULL);
    cfg.audio.pcm_list = snd_get_devices(&cfg.audio.dev_count);
    cfg.audio.dev_num = cfg_get_int("audio", "device", 0);
    cfg.audio.dev2_num = cfg_get_int("audio", "device2", -1);
    cfg.audio.dev_name = cfg_get_str("audio", "dev_name", _("Default PCM device (default)"));
    cfg.audio.dev2_name = cfg_get_str("audio", "dev2_name", _("None"));
    cfg.audio.dev_remember = cfg_get_int("audio", "dev_remember", REMEMBER_BY_NAME);
    cfg.audio.samplerate = cfg_get_int("audio", "samplerate", 44100);
    cfg.audio.resolution = 16;
    cfg.audio.bitrate = cfg_get_int("audio", "bitrate", 128);
    cfg.audio.channel = cfg_get_int("audio", "channel", 2);
    cfg.audio.left_ch = cfg_get_int("audio", "left_ch", 1);
    cfg.audio.right_ch = cfg_get_int("audio", "right_ch", 2);
    cfg.audio.left_ch2 = cfg_get_int("audio", "left_ch2", 1);
    cfg.audio.right_ch2 = cfg_get_int("audio", "right_ch2", 2);
    cfg.audio.buffer_ms = cfg_get_int("audio", "buffer_ms", 50);
    cfg.audio.disable_dithering = cfg_get_int("audio", "disable_dithering", 0);
    cfg.audio.resample_mode = cfg_get_int("audio", "resample_mode", 0); // 0 = SRC_SINC_BEST_QUALITY

    cfg.audio.codec = cfg_get_str("audio", "codec", "mp3");
    // Make sure that also "opus" and "flac" fit into the codec char array
    cfg.audio.codec = (char *)realloc((char *)cfg.audio.codec, 5 * sizeof(char));

    if (!strcmp(cfg.audio.codec, "aac") && g_aac_lib_available == 0) {
        strcpy(cfg.audio.codec, "mp3");
    }

    // Will be interpreted as negative value (-50 dB)
    cfg.audio.silence_level = cfg_get_float("audio", "silence_level", 50.0);
    cfg.audio.signal_level = cfg_get_float("audio", "signal_level", 50.0);

    cfg.rec.bitrate = cfg_get_int("record", "bitrate", 192);
    cfg.rec.start_rec = cfg_get_int("record", "start_rec", 0);
    cfg.rec.stop_rec = cfg_get_int("record", "stop_rec", 0);
    cfg.rec.rec_after_launch = cfg_get_int("record", "rec_after_launch", 0);
    cfg.rec.sync_to_hour = cfg_get_int("record", "sync_to_hour", 0);
    cfg.rec.split_time = cfg_get_int("record", "split_time", 0);
    cfg.rec.filename = cfg_get_str("record", "filename", "rec_%Y%m%d-%H%M%S_%i.mp3");
    cfg.rec.signal_threshold = cfg_get_int("record", "signal_threshold", 0);
    cfg.rec.silence_threshold = cfg_get_int("record", "silence_threshold", 0);
    cfg.rec.signal_detection = cfg_get_int("record", "signal_detection", -1);
    cfg.rec.silence_detection = cfg_get_int("record", "silence_detection", -1);

    // Backwards compatibility with versions < 0.1.41
    if (cfg.rec.signal_detection == -1) {
        cfg.rec.signal_detection = cfg.rec.signal_threshold > 0 ? 1 : 0;
    }
    if (cfg.rec.silence_detection == -1) {
        cfg.rec.silence_detection = cfg.rec.silence_threshold > 0 ? 1 : 0;
    }

    cfg.rec.codec = cfg_get_str("record", "codec", "mp3");
    // Make sure that also "opus" and "flac" fit into the codec char array
    cfg.rec.codec = (char *)realloc((char *)cfg.rec.codec, 5 * sizeof(char));

    if (!strcmp(cfg.rec.codec, "aac") && g_aac_lib_available == 0) {
        strcpy(cfg.rec.codec, "mp3");
    }

    cfg.rec.path = NULL; // Set to NULL, button_record_cb() may use realloc()
    cfg.rec.folder = cfg_get_str("record", "folder", NULL);
    if (cfg.rec.folder == NULL) {
        char *p;
        cfg.rec.folder = (char *)malloc(PATH_MAX * sizeof(char));
#ifdef WIN32
        p = fl_getenv("USERPROFILE");
        if (p != NULL) {
            snprintf(cfg.rec.folder, PATH_MAX, "%s\\Music\\", p);
        }
        else {
            snprintf(cfg.rec.folder, PATH_MAX, "./");
        }
#elif __APPLE__
        p = fl_getenv("HOME");
        if (p != NULL) {
            snprintf(cfg.rec.folder, PATH_MAX, "%s/Music/", p);
        }
        else {
            snprintf(cfg.rec.folder, PATH_MAX, "~/");
        }
#else // UNIX
        p = fl_getenv("HOME");
        if (p != NULL) {
            snprintf(cfg.rec.folder, PATH_MAX, "%s/", p);
        }
        else {
            snprintf(cfg.rec.folder, PATH_MAX, "~/");
        }
#endif
    }

    cfg.tls.cert_file = cfg_get_str("tls", "cert_file", NULL);
    cfg.tls.cert_dir = cfg_get_str("tls", "cert_dir", NULL);

    cfg.selected_srv = 0;

    cfg.main.num_of_srv = cfg_get_int("main", "num_of_srv", 0);
    if (cfg.main.num_of_srv > 0) {
        cfg.main.srv = cfg_get_str("main", "server", NULL);
        if (cfg.main.srv == NULL) {
            fl_alert(_("error while parsing config. Missing main/server entry.\nbutt will start with default settings"));
            return 1;
        }

        cfg.main.srv_ent = cfg_get_str("main", "srv_ent", NULL);
        if (cfg.main.srv_ent == NULL) {
            fl_alert(_("error while parsing config. Missing main/srv_ent entry.\nbutt will start with default settings"));
            return 1;
        }

        cfg.srv = (server_t **)malloc(sizeof(server_t *) * cfg.main.num_of_srv);

        for (i = 0; i < cfg.main.num_of_srv; i++) {
            cfg.srv[i] = (server_t *)malloc(sizeof(server_t));
        }

        strtok_buf = strdup(cfg.main.srv_ent);
        srv_ent = strtok(strtok_buf, ";");

        for (i = 0; srv_ent != NULL; i++) {
            cfg.srv[i]->name = (char *)malloc(strlen(srv_ent) + 1);
            snprintf(cfg.srv[i]->name, strlen(srv_ent) + 1, "%s", srv_ent);

            cfg.srv[i]->addr = cfg_get_str(srv_ent, "address", NULL);
            if (cfg.srv[i]->addr == NULL) {
                fl_alert(_("error while parsing config. Missing address entry for server \"%s\"."
                           "\nbutt will start with default settings"),
                         srv_ent);
                return 1;
            }

            cfg.srv[i]->port = cfg_get_int(srv_ent, "port", -1);
            if (cfg.srv[i]->port == -1) {
                fl_alert(_("error while parsing config. Missing port entry for server \"%s\"."
                           "\nbutt will start with default settings"),
                         srv_ent);
                return 1;
            }

            cfg.srv[i]->pwd = cfg_get_str(srv_ent, "password", NULL);
            if (cfg.srv[i]->pwd == NULL) {
                fl_alert(_("error while parsing config. Missing password entry for server \"%s\"."
                           "\nbutt will start with default settings"),
                         srv_ent);
                return 1;
            }

            cfg.srv[i]->type = cfg_get_int(srv_ent, "type", -1);
            if (cfg.srv[i]->type == -1) {
                fl_alert(_("error while parsing config. Missing type entry for server \"%s\"."
                           "\nbutt will start with default settings"),
                         srv_ent);
                return 1;
            }

            cfg.srv[i]->mount = cfg_get_str(srv_ent, "mount", NULL);
            if ((cfg.srv[i]->mount == NULL) && (cfg.srv[i]->type == ICECAST)) {
                fl_alert(_("error while parsing config. Missing mount entry for server \"%s\"."
                           "\nbutt will start with default settings"),
                         srv_ent);
                return 1;
            }

            cfg.srv[i]->usr = cfg_get_str(srv_ent, "usr", NULL);
            if ((cfg.srv[i]->usr == NULL) && (cfg.srv[i]->type == ICECAST)) {
                cfg.srv[i]->usr = (char *)malloc(strlen("source") + 1);
                strcpy(cfg.srv[i]->usr, "source");
            }

            cfg.srv[i]->icecast_protocol = cfg_get_int(srv_ent, "protocol", ICECAST_PROTOCOL_PUT);

#ifdef HAVE_LIBSSL
            cfg.srv[i]->tls = cfg_get_int(srv_ent, "tls", 0);
#else
            cfg.srv[i]->tls = 0;
#endif
            cfg.srv[i]->cert_hash = cfg_get_str(srv_ent, "cert_hash", NULL);
            if ((cfg.srv[i]->cert_hash != NULL) && (strlen(cfg.srv[i]->cert_hash) != 64)) {
                free(cfg.srv[i]->cert_hash);
                cfg.srv[i]->cert_hash = NULL;
            }

            if (!strcmp(cfg.srv[i]->name, cfg.main.srv)) {
                cfg.selected_srv = i;
            }

            srv_ent = strtok(NULL, ";");
        }
        free(strtok_buf);
    } // if(cfg.main.num_of_srv > 0)
    else {
        cfg.main.srv = NULL;
    }

    cfg.main.num_of_icy = cfg_get_int("main", "num_of_icy", 0);

    cfg.selected_icy = 0;
    if (cfg.main.num_of_icy > 0) {
        cfg.main.icy = cfg_get_str("main", "icy", NULL);
        if (cfg.main.icy == NULL) {
            fl_alert(_("error while parsing config. Missing main/icy entry.\nbutt will start with default settings"));
            return 1;
        }
        cfg.main.icy_ent = cfg_get_str("main", "icy_ent", NULL); // icy entries
        if (cfg.main.icy_ent == NULL) {
            fl_alert(_("error while parsing config. Missing main/icy_ent entry.\nbutt will start with default settings"));
            return 1;
        }

        cfg.icy = (icy_t **)malloc(sizeof(icy_t *) * cfg.main.num_of_icy);

        for (i = 0; i < cfg.main.num_of_icy; i++) {
            cfg.icy[i] = (icy_t *)malloc(sizeof(icy_t));
        }

        strtok_buf = strdup(cfg.main.icy_ent);
        icy_ent = strtok(strtok_buf, ";");

        for (i = 0; icy_ent != NULL; i++) {
            cfg.icy[i]->name = (char *)malloc(strlen(icy_ent) + 1);
            snprintf(cfg.icy[i]->name, strlen(icy_ent) + 1, "%s", icy_ent);

            cfg.icy[i]->desc = cfg_get_str(icy_ent, "description", NULL);
            cfg.icy[i]->genre = cfg_get_str(icy_ent, "genre", NULL);
            cfg.icy[i]->url = cfg_get_str(icy_ent, "url", NULL);
            cfg.icy[i]->irc = cfg_get_str(icy_ent, "irc", NULL);
            cfg.icy[i]->icq = cfg_get_str(icy_ent, "icq", NULL);
            cfg.icy[i]->aim = cfg_get_str(icy_ent, "aim", NULL);
            cfg.icy[i]->pub = cfg_get_str(icy_ent, "pub", NULL);
            cfg.icy[i]->expand_variables = cfg_get_int(icy_ent, "expand_variables", 0);
            if (cfg.icy[i]->pub == NULL) {
                fl_alert(_("error while parsing config. Missing pub entry for icy \"%s\"."
                           "\nbutt will start with default settings"),
                         icy_ent);
                return 1;
            }

            if (!strcmp(cfg.icy[i]->name, cfg.main.icy)) {
                cfg.selected_icy = i;
            }

            icy_ent = strtok(NULL, ";");
        }
        free(strtok_buf);
    } // if(cfg.main.num_of_icy > 0)
    else {
        cfg.main.icy = NULL;
    }

    cfg.main.song_path = cfg_get_str("main", "song_path", NULL);

    cfg.main.song_prefix = cfg_get_str("main", "song_prefix", NULL);
    if (cfg.main.song_prefix != NULL) {
        strrpl(&cfg.main.song_prefix, (char *)"%20", (char *)" ", MODE_ALL);
    }

    cfg.main.song_suffix = cfg_get_str("main", "song_suffix", NULL);
    if (cfg.main.song_suffix != NULL) {
        strrpl(&cfg.main.song_suffix, (char *)"%20", (char *)" ", MODE_ALL);
    }

    // song update from file is default set to off
    cfg.main.song_update = cfg_get_int("main", "song_update", 0);
    cfg.main.song_delay = cfg_get_int("main", "song_delay", 0);
    cfg.main.read_last_line = cfg_get_int("main", "read_last_line", 0);
    cfg.main.app_update = cfg_get_int("main", "app_update", 0);
    cfg.main.app_update_service = cfg_get_int("main", "app_update_service", 0);
    cfg.main.app_artist_title_order = cfg_get_int("main", "app_artist_title_order", APP_TITLE_FIRST);
    cfg.main.signal_threshold = cfg_get_int("main", "signal_threshold", 0);
    cfg.main.silence_threshold = cfg_get_int("main", "silence_threshold", 0);
    cfg.main.signal_detection = cfg_get_int("main", "signal_detection", -1);
    cfg.main.silence_detection = cfg_get_int("main", "silence_detection", -1);

    // Backwards compatibility with versions < 0.1.41
    if (cfg.main.signal_detection == -1) {
        cfg.main.signal_detection = cfg.main.signal_threshold > 0 ? 1 : 0;
    }
    if (cfg.main.silence_detection == -1) {
        cfg.main.silence_detection = cfg.main.silence_threshold > 0 ? 1 : 0;
    }

    cfg.main.connect_at_startup = cfg_get_int("main", "connect_at_startup", 0);
    cfg.main.force_reconnecting = cfg_get_int("main", "force_reconnecting", 0);
    cfg.main.reconnect_delay = cfg_get_int("main", "reconnect_delay", 1);
    cfg.main.check_for_update = cfg_get_int("main", "check_for_update", 1);
    cfg.main.start_agent = cfg_get_int("main", "start_agent", 0);
    cfg.main.minimize_to_tray = cfg_get_int("main", "minimize_to_tray", 0);

    cfg.main.gain = cfg_get_float("main", "gain", 1.0);
    if (cfg.main.gain > util_db_to_factor(24)) {
        cfg.main.gain = util_db_to_factor(24);
    }
    if (cfg.main.gain < 0) {
        cfg.main.gain = util_db_to_factor(-24);
    }

    // Mixer
    cfg.mixer.primary_device_gain = cfg_get_float("mixer", "primary_device_gain", 1.0);
    cfg.mixer.primary_device_muted = cfg_get_int("mixer", "primary_device_muted", 0);
    cfg.mixer.secondary_device_gain = cfg_get_float("mixer", "secondary_device_gain", 1.0);
    cfg.mixer.secondary_device_muted = cfg_get_int("mixer", "secondary_device_muted", 0);
    cfg.mixer.streaming_gain = cfg_get_float("mixer", "streaming_gain", 1.0);
    cfg.mixer.recording_gain = cfg_get_float("mixer", "recording_gain", 1.0);
    cfg.mixer.cross_fader = cfg_get_float("mixer", "cross_fader", 0.0);

    // DSP
    cfg.dsp.equalizer_stream = cfg_get_int("dsp", "equalizer", 0);
    cfg.dsp.equalizer_rec = cfg_get_int("dsp", "equalizer_rec", -1);

    // Backwards compatability with <= 0.1.40
    if (cfg.dsp.equalizer_rec == -1) {
        cfg.dsp.equalizer_rec = cfg.dsp.equalizer_stream;
    }

    cfg.dsp.eq_preset = cfg_get_str("dsp", "eq_preset", "Manual");

    cfg.dsp.eq_gain[0] = cfg_get_float("dsp", "gain1", 0.0);
    cfg.dsp.eq_gain[1] = cfg_get_float("dsp", "gain2", 0.0);
    cfg.dsp.eq_gain[2] = cfg_get_float("dsp", "gain3", 0.0);
    cfg.dsp.eq_gain[3] = cfg_get_float("dsp", "gain4", 0.0);
    cfg.dsp.eq_gain[4] = cfg_get_float("dsp", "gain5", 0.0);
    cfg.dsp.eq_gain[5] = cfg_get_float("dsp", "gain6", 0.0);
    cfg.dsp.eq_gain[6] = cfg_get_float("dsp", "gain7", 0.0);
    cfg.dsp.eq_gain[7] = cfg_get_float("dsp", "gain8", 0.0);
    cfg.dsp.eq_gain[8] = cfg_get_float("dsp", "gain9", 0.0);
    cfg.dsp.eq_gain[9] = cfg_get_float("dsp", "gain10", NAN);
    if (isnan(cfg.dsp.eq_gain[9])) {
        // Reset all EQ gains if band 10 has no config value. This prevents false values when the user updates from an earlier 5-band EQ to the new
        // 10-band EQ
        for (int i = 0; i < EQ_BAND_COUNT; i++) {
            cfg.dsp.eq_gain[i] = 0.0;
        }
    }

    cfg.dsp.compressor_stream = cfg_get_int("dsp", "compressor", 0);
    cfg.dsp.compressor_rec = cfg_get_int("dsp", "compressor_rec", -1);

    // Backwards compatability with <= 0.1.40
    if (cfg.dsp.compressor_rec == -1) {
        cfg.dsp.compressor_rec = cfg.dsp.compressor_stream;
    }

    cfg.dsp.aggressive_mode = cfg_get_int("dsp", "aggressive_mode", 0);
    cfg.dsp.threshold = cfg_get_float("dsp", "threshold", -20.0);
    cfg.dsp.ratio = cfg_get_float("dsp", "ratio", 5.0);
    cfg.dsp.attack = cfg_get_float("dsp", "attack", 0.01);
    cfg.dsp.release = cfg_get_float("dsp", "release", 1.0);
    cfg.dsp.makeup_gain = cfg_get_float("dsp", "makeup_gain", 0.0);

    // MIDI
    cfg.midi.dev_name = cfg_get_str("midi", "dev_name", "Disabled");
    char midi_section[16];
    for (i = 0; i < MIDI_COMMANDS_COUNT; i++) {
        snprintf(midi_section, sizeof(midi_section), "midi_cmd_%d", i);
        cfg.midi.commands[i].enabled = cfg_get_int(midi_section, "enabled", 0);
        cfg.midi.commands[i].channel = cfg_get_int(midi_section, "channel", 0); // channel = 0 means all channels are allowed
        cfg.midi.commands[i].msg_num = cfg_get_int(midi_section, "msg_num", i);
        cfg.midi.commands[i].msg_type = cfg_get_int(midi_section, "msg_type", MIDI_MSG_TYPE_CC); // As of BUTT 0.1.41 "type" defaults to CC.
        cfg.midi.commands[i].mode = cfg_get_int(midi_section, "mode", MIDI_MODE_ABSOLUTE);
        cfg.midi.commands[i].soft_takeover = cfg_get_int(midi_section, "soft_takeover", 0);
    }

    // read GUI stuff
    cfg.gui.attach = cfg_get_int("gui", "attach", 0);
    cfg.gui.ontop = cfg_get_int("gui", "ontop", 0);
    cfg.gui.hide_log_window = cfg_get_int("gui", "hide_log_window", 0);
    cfg.gui.remember_pos = cfg_get_int("gui", "remember_pos", 1);
    cfg.gui.x_pos = cfg_get_int("gui", "x_pos", -1);
    cfg.gui.y_pos = cfg_get_int("gui", "y_pos", -1);
    cfg.gui.window_height = cfg_get_int("gui", "window_height", 0);
    cfg.gui.lcd_auto = cfg_get_int("gui", "lcd_auto", 0);
    cfg.gui.start_minimized = cfg_get_int("gui", "start_minimized", 0);
    cfg.gui.disable_gain_slider = cfg_get_int("gui", "disable_gain_slider", 0);
    cfg.gui.show_listeners = cfg_get_int("gui", "show_listeners", 1);
    cfg.gui.vu_mode = cfg_get_int("gui", "vu_mode", VU_MODE_SOLID);
    cfg.gui.always_show_vu_tabs = cfg_get_int("gui", "always_show_vu_tabs", 1);

    int lang_id = cfg_get_int("gui", "lang", -1);
    if (lang_id != -1) { // For compatibility with butt configurations <= 0.1.33
        cfg.gui.lang_str = NULL;
        lang_id_to_str(lang_id, &cfg.gui.lang_str, LANG_MAPPING_OLD);
    }
    else {
        cfg.gui.lang_str = cfg_get_str("gui", "lang_str", "system"); // butt > 0.1.33
        if (cfg.gui.lang_str == NULL) {
            lang_id_to_str(0, &cfg.gui.lang_str, LANG_MAPPING_NEW);
        }
    }

    cfg.gui.window_title = cfg_get_str("gui", "window_title", NULL);
    if (cfg.gui.window_title == NULL) {
        cfg.gui.window_title = (char *)malloc(1);
        cfg.gui.window_title[0] = '\0';
    }

    // read FLTK related stuff
    cfg.main.bg_color = cfg_get_int("main", "bg_color", 252645120); // dark gray
    cfg.main.txt_color = cfg_get_int("main", "txt_color", -256);    // white

    // read mp3 codec related stuff
    cfg.mp3_codec_stream.enc_quality = cfg_get_int("mp3_codec_stream", "enc_quality", 3);
    cfg.mp3_codec_stream.stereo_mode = cfg_get_int("mp3_codec_stream", "stereo_mode", 0);
    cfg.mp3_codec_stream.bitrate_mode = cfg_get_int("mp3_codec_stream", "bitrate_mode", CHOICE_CBR);
    cfg.mp3_codec_stream.vbr_quality = cfg_get_int("mp3_codec_stream", "vbr_quality", 4);
    cfg.mp3_codec_stream.vbr_min_bitrate = cfg_get_int("mp3_codec_stream", "vbr_min_bitrate", 32);
    cfg.mp3_codec_stream.vbr_max_bitrate = cfg_get_int("mp3_codec_stream", "vbr_max_bitrate", 320);

    cfg.mp3_codec_rec.enc_quality = cfg_get_int("mp3_codec_rec", "enc_quality", 3);
    cfg.mp3_codec_rec.stereo_mode = cfg_get_int("mp3_codec_rec", "stereo_mode", 0);
    cfg.mp3_codec_rec.bitrate_mode = cfg_get_int("mp3_codec_rec", "bitrate_mode", CHOICE_CBR);
    cfg.mp3_codec_rec.vbr_quality = cfg_get_int("mp3_codec_rec", "vbr_quality", 4);
    cfg.mp3_codec_rec.vbr_min_bitrate = cfg_get_int("mp3_codec_rec", "vbr_min_bitrate", 32);
    cfg.mp3_codec_rec.vbr_max_bitrate = cfg_get_int("mp3_codec_rec", "vbr_max_bitrate", 320);

    // read ogg/vorbis codec related stuff
    cfg.vorbis_codec_stream.bitrate_mode = cfg_get_int("vorbis_codec_stream", "bitrate_mode", CHOICE_CBR);
    cfg.vorbis_codec_stream.vbr_quality = cfg_get_int("vorbis_codec_stream", "vbr_quality", 0);
    cfg.vorbis_codec_stream.vbr_min_bitrate = cfg_get_int("vorbis_codec_stream", "vbr_min_bitrate", 0);
    cfg.vorbis_codec_stream.vbr_max_bitrate = cfg_get_int("vorbis_codec_stream", "vbr_max_bitrate", 0);
    cfg.vorbis_codec_rec.bitrate_mode = cfg_get_int("vorbis_codec_rec", "bitrate_mode", CHOICE_CBR);
    cfg.vorbis_codec_rec.vbr_quality = cfg_get_int("vorbis_codec_rec", "vbr_quality", 0);
    cfg.vorbis_codec_rec.vbr_min_bitrate = cfg_get_int("vorbis_codec_rec", "vbr_min_bitrate", 0); // 0 = Auto
    cfg.vorbis_codec_rec.vbr_max_bitrate = cfg_get_int("vorbis_codec_rec", "vbr_max_bitrate", 0); // 0 = Auto

    // read opus codec related stuff
    cfg.opus_codec_stream.bitrate_mode = cfg_get_int("opus_codec_stream", "bitrate_mode", CHOICE_VBR);
    cfg.opus_codec_stream.quality = cfg_get_int("opus_codec_stream", "quality", 0);
    cfg.opus_codec_stream.audio_type = cfg_get_int("opus_codec_stream", "audio_type", CHOICE_TYPE_MUSIC);
    cfg.opus_codec_stream.bandwidth = cfg_get_int("opus_codec_stream", "bandwidth", 0);

    cfg.opus_codec_rec.bitrate_mode = cfg_get_int("opus_codec_rec", "bitrate_mode", CHOICE_VBR);
    cfg.opus_codec_rec.quality = cfg_get_int("opus_codec_rec", "quality", 0);
    cfg.opus_codec_rec.audio_type = cfg_get_int("opus_codec_rec", "audio_type", CHOICE_TYPE_MUSIC);
    cfg.opus_codec_rec.bandwidth = cfg_get_int("opus_codec_rec", "bandwidth", 0);

    // read flac codec related stuff
    cfg.flac_codec_stream.bit_depth = cfg_get_int("flac_codec_stream", "bit_depth", 16);
    cfg.flac_codec_rec.bit_depth = cfg_get_int("flac_codec_rec", "bit_depth", 16);

    // read wav codec related stuff
    cfg.wav_codec_rec.bit_depth = cfg_get_int("wav_codec_rec", "bit_depth", 16);

    // read aac codec related stuff
    cfg.aac_codec_stream.bitrate_mode = cfg_get_int("aac_codec_stream", "bitrate_mode", CHOICE_CBR);
    cfg.aac_codec_stream.profile = cfg_get_int("aac_codec_stream", "profile", CHOICE_AAC_PROFILE_AUTO);
    cfg.aac_codec_stream.afterburner = cfg_get_int("aac_codec_stream", "afterburner", 0); // Default: After burner on

    cfg.aac_codec_rec.bitrate_mode = cfg_get_int("aac_codec_rec", "bitrate_mode", CHOICE_CBR);
    cfg.aac_codec_rec.profile = cfg_get_int("aac_codec_rec", "profile", CHOICE_AAC_PROFILE_AUTO);
    cfg.aac_codec_rec.afterburner = cfg_get_int("aac_codec_rec", "afterburner", 0); // Default: After burner on

#ifdef HAVE_LIBFDK_AAC
    // Backward compatability
    if (cfg_get_int("audio", "aac_overwrite_aot", -1) != -1) {
        switch (cfg_get_int("audio", "aac_aot", -1)) {
        case 2:
            cfg.aac_codec_stream.profile = CHOICE_AAC_PROFILE_AAC_LC;
            cfg.aac_codec_rec.profile = CHOICE_AAC_PROFILE_AAC_LC;
            break;
        case 5:
            cfg.aac_codec_stream.profile = CHOICE_AAC_PROFILE_HE_AACv1;
            cfg.aac_codec_rec.profile = CHOICE_AAC_PROFILE_HE_AACv1;
            break;
        case 29:
            cfg.aac_codec_stream.profile = CHOICE_AAC_PROFILE_HE_AACv2;
            cfg.aac_codec_rec.profile = CHOICE_AAC_PROFILE_HE_AACv2;
            break;
        default:
            cfg.aac_codec_stream.profile = CHOICE_AAC_PROFILE_AUTO;
            cfg.aac_codec_rec.profile = CHOICE_AAC_PROFILE_AUTO;
            break;
        }
    }

#endif

    return 0;
}

int cfg_create_default(void)
{
    FILE *cfg_fd;
    char *p;
    char def_rec_folder[PATH_MAX];

    cfg_fd = fl_fopen(cfg_path, "wb");
    if (cfg_fd == NULL) {
        return 1;
    }

#ifdef WIN32
    p = fl_getenv("USERPROFILE");
    if (p != NULL) {
        snprintf(def_rec_folder, PATH_MAX, "%s\\Music\\", p);
    }
    else {
        snprintf(def_rec_folder, PATH_MAX, "./");
    }
#elif __APPLE__
    p = fl_getenv("HOME");
    if (p != NULL) {
        snprintf(def_rec_folder, PATH_MAX, "%s/Music/", p);
    }
    else {
        snprintf(def_rec_folder, PATH_MAX, "~/");
    }
#else // UNIX
    p = fl_getenv("HOME");
    if (p != NULL) {
        snprintf(def_rec_folder, PATH_MAX, "%s/", p);
    }
    else {
        snprintf(def_rec_folder, PATH_MAX, "~/");
    }
#endif

    fprintf(cfg_fd, "#This is a configuration file for butt (broadcast using this tool)\n\n");
    fprintf(cfg_fd, "[main]\n"
                    "server =\n"
                    "icy =\n"
                    "num_of_srv = 0\n"
                    "num_of_icy = 0\n"
                    "srv_ent =\n"
                    "icy_ent =\n"
                    "song_path =\n"
                    "song_update = 0\n"
                    "song_delay = 0\n"
                    "song_prefix = \n"
                    "song_suffix = \n"
                    "app_update = 0\n"
                    "app_update_service = 0\n"
                    "app_artist_title_order = 1\n"
                    "read_last_line = 0\n"
                    "log_file =\n"
                    "gain = 1.0\n"
                    "ic_charset =\n"
                    "signal_threshhold = 0\n"
                    "silence_threshhold = 0\n"
                    "signal_detection = 0\n"
                    "silence_detection = 0\n"
                    "check_for_update = 1\n"
                    "start_agent = 0\n"
                    "minimize_to_tray = 0\n"
                    "force_reconnecting = 0\n"
                    "reconnect_delay = 1\n"
                    "connect_at_startup = 0\n\n");

    fprintf(cfg_fd,
            "[audio]\n"
            "device = default\n"
            "samplerate = 44100\n"
            "bitrate = 128\n"
            "channel = 2\n"
            "codec = mp3\n"
            "resample_mode = 1\n" // SRC_SINC_MEDIUM_QUALITY
            "silence_level = 50.0\n"
            "signal_level = 50.0\n"
            "disable_dithering = 0\n"
            "buffer_ms = 50\n\n");

    fprintf(cfg_fd,
            "[record]\n"
            "samplerate = 44100\n"
            "bitrate = 192\n"
            "channel = 2\n"
            "codec = mp3\n"
            "start_rec = 0\n"
            "stop_rec = 0\n"
            "rec_after_launch = 0\n"
            "sync_to_hour = 0\n"
            "split_time = 0\n"
            "filename = rec_%%Y%%m%%d-%%H%%M%%S.mp3\n"
            "signal_threshhold = 0\n"
            "silence_threshhold = 0\n"
            "signal_detection = 0\n"
            "silence_detection = 0\n"
            "folder = %s\n\n",
            def_rec_folder);

    fprintf(cfg_fd, "[tls]\n"
                    "cert_file =\n"
                    "cert_dir =\n\n");

    fprintf(cfg_fd, "[dsp]\n"
                    "equalizer = 0\n"
                    "eq_preset = Manual\n"
                    "gain1 = 0.0\n"
                    "gain2 = 0.0\n"
                    "gain3 = 0.0\n"
                    "gain4 = 0.0\n"
                    "gain5 = 0.0\n"
                    "gain6 = 0.0\n"
                    "gain7 = 0.0\n"
                    "gain8 = 0.0\n"
                    "gain9 = 0.0\n"
                    "gain10 = 0.0\n"
                    "compressor = 0\n"
                    "aggressive_mode = 0\n"
                    "threshold = -20.0\n"
                    "ratio = 5\n"
                    "attack = 0.01\n"
                    "release = 1.0\n"
                    "makeup_gain = 0.0\n");

    fprintf(cfg_fd, "[gui]\n"
                    "attach = 0\n"
                    "ontop = 0\n"
                    "hide_log_window = 0\n"
                    "remember_pos = 1\n"
                    "lcd_auto = 0\n"
                    "start_minimized = 0\n"
                    "disable_gain_slider = 0\n"
                    "show_listeners = 1\n"
                    "window_title = \n"
                    "lang_str = system\n\n");

    fprintf(cfg_fd, "[mp3_codec_stream]\n"
                    "enc_quality = 3\n"
                    "stereo_mode = 0\n"
                    "bitrate_mode = 0\n"
                    "vbr_quality = 4\n"
                    "vbr_min_bitrate = 32\n"
                    "vbr_max_bitrate = 320\n\n");

    fprintf(cfg_fd, "[mp3_codec_rec]\n"
                    "enc_quality = 3\n"
                    "stereo_mode = 0\n"
                    "bitrate_mode = 0\n"
                    "vbr_quality = 4\n"
                    "vbr_min_bitrate = 32\n"
                    "vbr_max_bitrate = 320\n\n");

    fprintf(cfg_fd, "[vorbis_codec_stream]\n"
                    "bitrate_mode = 0\n"
                    "vbr_quality = 0\n"
                    "vbr_min_bitrate = 0\n"
                    "vbr_max_bitrate = 0\n\n");

    fprintf(cfg_fd, "[vorbis_codec_rec]\n"
                    "bitrate_mode = 0\n"
                    "vbr_quality = 0\n"
                    "vbr_min_bitrate = 0\n"
                    "vbr_max_bitrate = 0\n\n");

    fprintf(cfg_fd, "[opus_codec_stream]\n"
                    "bitrate_mode = 1\n"
                    "quality = 0\n"
                    "audio_type = 0\n"
                    "bandwidth = 0\n\n");

    fprintf(cfg_fd, "[opus_codec_rec]\n"
                    "bitrate_mode = 1\n"
                    "quality = 0\n"
                    "audio_type = 0\n"
                    "bandwidth = 0\n\n");

    fprintf(cfg_fd, "[aac_codec_stream]\n"
                    "bitrate_mode = 0\n"
                    "afterburner = 0\n"
                    "profile = 0\n\n");

    fprintf(cfg_fd, "[aac_codec_rec]\n"
                    "bitrate_mode = 0\n"
                    "afterburner = 0\n"
                    "profile = 0\n\n");

    fprintf(cfg_fd, "[flac_codec_stream]\n"
                    "bit_depth = 16\n\n");

    fprintf(cfg_fd, "[flac_codec_rec]\n"
                    "bit_depth = 16\n\n");

    fprintf(cfg_fd, "[wav_codec_rec]\n"
                    "bit_depth = 16\n\n");

    fclose(cfg_fd);

    return 0;
}

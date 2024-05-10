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

#ifndef CFG_H
#define CFG_H

#ifndef BUILD_CLIENT
#include "port_audio.h"
#endif
#include "parseconfig.h"

enum {
    SHOUTCAST = 0,
    ICECAST = 1,
    RADIOCO = 2,
};

enum {
    ICECAST_PROTOCOL_PUT = 0,
    ICECAST_PROTOCOL_SOURCE = 1,
};

enum {
    CHOICE_STEREO = 0,
    CHOICE_MONO = 1,
};

enum {
    CHOICE_MP3 = 0,
    CHOICE_OGG = 1,
    CHOICE_OPUS = 2,
    CHOICE_AAC = 3,
    CHOICE_FLAC = 4,
    CHOICE_WAV = 5,
};

enum {
    CHOICE_CBR = 0,
    CHOICE_VBR = 1,
    CHOICE_ABR = 2,
};

enum {
    CHOICE_TYPE_MUSIC = 0,
    CHOICE_TYPE_SPEECH = 1,
};

enum {
    CHOICE_AAC_PROFILE_AUTO = 0,
    CHOICE_AAC_PROFILE_AAC_LC = 1,
    CHOICE_AAC_PROFILE_HE_AACv1 = 2,
    CHOICE_AAC_PROFILE_HE_AACv2 = 3,
};


enum {
    LANG_MAPPING_OLD = 0,
    LANG_MAPPING_NEW = 1,
};

enum {
    LANG_COUNT = 8,
};

enum {
    MIDI_COMMANDS_COUNT = 10,
};

enum {
    APP_ARTIST_FIRST = 0,
    APP_TITLE_FIRST = 1,
};

enum {
    VU_MODE_GRADIENT = 0,
    VU_MODE_SOLID = 1,
};

enum {
    REMEMBER_BY_ID = 0,
    REMEMBER_BY_NAME = 1,
};

extern const char *lang_array_new[];
extern const char *lang_array_old[];
extern const char CONFIG_FILE[];

typedef struct {
    char *name;
    char *addr;
    char *pwd;
    char *mount;     // mountpoint for icecast server
    char *usr;       // user for icecast server
    char *cert_hash; // sha256 hash of trusted certificate
    int port;
    int type;             // SHOUTCAST or ICECAST
    int tls;              // use tls: 0 = no, 1 = yes
    int icecast_protocol; // Icecast protocol: 0 = PUT, 1 = SOURCE
} server_t;

typedef struct {
    int enabled;
    int channel;       // Any, or 1-16
    int msg_num;       // CC number
    int msg_type;      // Currently unused
    int mode;          // CC Mode. Absolute or relative
    int soft_takeover; // If active, value jumps are prevented
    int picked_up;     // 0 = not picked up, 1 = picked up (Only relevant if soft_takeover is active)
} midi_command_t;

typedef struct {
    char *name;
    char *desc; // description
    char *genre;
    char *url;
    char *irc;
    char *icq;
    char *aim;
    char *pub;
    int expand_variables;
} icy_t;

typedef struct {
    int selected_srv;
    int selected_icy;

    struct {
        char *srv;
        char *icy;
        char *srv_ent;
        char *icy_ent;
        char *song;
        char *song_path;
        FILE *song_fd;
        char *song_prefix;
        char *song_suffix;
        int song_delay;
        int song_update; // 1 = song info will be read from file
        int read_last_line;
        int app_update;
        int app_update_service;
        int app_artist_title_order;
        int num_of_srv;
        int num_of_icy;
        int bg_color, txt_color;
        int connect_at_startup;
        int force_reconnecting;
        int reconnect_delay;
        int silence_threshold; // timeout duration of automatic stream stop
        int signal_threshold;  // timeout duration of automatic stream start
        int signal_detection;
        int silence_detection;
        int check_for_update;
        int start_agent;
        int minimize_to_tray;
        float gain;
        char *log_file;
        char *ic_charset;
    } main;

    struct {
        int dev_count;
        int dev_num;
        int dev2_num;
        char *dev_name;
        char *dev2_name;
        int dev_remember; // Remember device by ID or Name
        snd_dev_t **pcm_list;
        int samplerate;
        int resolution;
        int channel;
        int left_ch;
        int right_ch;
        int left_ch2;
        int right_ch2;
        int bitrate;
        int buffer_ms;
        int resample_mode;
        float silence_level;
        float signal_level;
        int disable_dithering;
        char *codec;
    } audio;

    struct {
        char *dev_name;
        midi_command_t commands[MIDI_COMMANDS_COUNT];
    } midi;

    struct {
        int channel;
        int bitrate;
        int quality;
        int samplerate;
        char *codec;
        char *filename;
        char *folder;
        char *path;
        FILE *fd;
        int start_rec;
        int stop_rec;
        int rec_after_launch;
        int split_time;
        int sync_to_hour;
        int silence_threshold;
        int signal_threshold;
        int signal_detection;
        int silence_detection;
    } rec;

    struct {
        char *cert_file;
        char *cert_dir;
    } tls;

    struct {
        int equalizer_stream;
        int equalizer_rec;
        char *eq_preset;
        double eq_gain[10];
        int compressor_stream;
        int compressor_rec;
        int aggressive_mode;
        double threshold, ratio, attack, release, makeup_gain;
    } dsp;

    struct {
        double primary_device_gain;
        int primary_device_muted;
        int secondary_device_muted;
        double secondary_device_gain;
        double streaming_gain;
        double recording_gain;
        double cross_fader;
        float primary_X_fader;
        float secondary_X_fader;
    } mixer;

    struct {
        int attach;
        int ontop;
        int lcd_auto;
        int hide_log_window;
        int remember_pos;
        int window_height;
        int x_pos, y_pos;
        char *lang_str;
        int vu_mode;
        int always_show_vu_tabs;
        int start_minimized;
        int disable_gain_slider;
        int show_listeners;
        char *window_title;
    } gui;

    struct {
        int enc_quality;
        int stereo_mode;
        int bitrate_mode;
        int vbr_quality;
        int vbr_min_bitrate;
        int vbr_max_bitrate;
    } mp3_codec_stream;

    struct {
        int enc_quality;
        int stereo_mode;
        int bitrate_mode;
        int vbr_quality;
        int vbr_min_bitrate;
        int vbr_max_bitrate;
    } mp3_codec_rec;

    struct {
        int bitrate_mode;
        int vbr_quality;
        int vbr_min_bitrate;
        int vbr_max_bitrate;
    } vorbis_codec_stream;

    struct {
        int bitrate_mode;
        int vbr_quality;
        int vbr_min_bitrate;
        int vbr_max_bitrate;
    } vorbis_codec_rec;

    struct {
        int bitrate_mode;
        int quality;
        int audio_type;
        int bandwidth;
    } opus_codec_stream;

    struct {
        int bitrate_mode;
        int quality;
        int audio_type;
        int bandwidth;
    } opus_codec_rec;

    struct {
        int bitrate_mode;
        int profile;
        int afterburner;
    } aac_codec_stream;

    struct {
        int bitrate_mode;
        int profile;
        int afterburner;
    } aac_codec_rec;

    struct {
        int bit_depth;
    } flac_codec_stream;

    struct {
        int bit_depth;
    } flac_codec_rec;

    struct {
        int bit_depth;
    } wav_codec_rec;

    server_t **srv;
    icy_t **icy;

} config_t;

extern char *cfg_path; // Path to config file
extern config_t cfg;   // Holds config parameters

int cfg_write_file(char *path); // Writes current config_t struct to path or cfg_path if path is NULL
int cfg_set_values(char *path); // Reads config file from path or cfg_path if path is NULL and fills the config_t struct
int cfg_create_default(void);   // Creates a default config file, if there isn't one yet

#endif

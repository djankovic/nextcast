// audio functions for butt
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

#ifndef PORT_AUDIO_H
#define PORT_AUDIO_H

#include <stdio.h>
#include <stdlib.h>

#include <portaudio.h>

#include "lame_encode.h"
#include "dsp.hpp"

#define SND_MAX_DEVICES (256)

#define INT24_MAX ((1 << 23) - 1)
#define INT24_MIN (-(1 << 23))

extern DSPEffects *streaming_dsp;
extern DSPEffects *recording_dsp;

typedef struct {
    char *name;
    int dev_id;
    int sr_list[12]; // 8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000, 88200, 96000, 0
    int num_of_sr;
    int num_of_channels;
    int left_ch;
    int right_ch;
    int is_asio;
} snd_dev_t;

enum {
    SND_STREAM = 0,
    SND_REC = 1,
};

extern int enc_buf_size;

extern bool try_to_connect;
extern bool pa_new_frames;
extern bool reconnect;
extern bool next_file;
extern bool silence_detected;
extern bool signal_detected;

extern FILE *next_fd;

int *snd_get_samplerates(int *sr_count);
void snd_free_device_list(snd_dev_t **dev_list, int dev_count);
snd_dev_t **snd_get_devices(int *dev_count);
void *snd_rec_thread(void *data);
void *snd_stream_thread(void *data);
void *snd_mixer_thread(void *data);

void snd_update_vu(int reset);
void snd_start_streaming_thread(void);
void snd_stop_streaming_thread(void);
void snd_start_recording_thread(void);
void snd_stop_recording_thread(void);
void snd_start_mixer_thread(void);
void snd_stop_mixer_thread(void);

void snd_set_vu_level_type(int type);

int snd_init(void);
void snd_init_dsp(void);
int snd_open_streams(void);
int snd_reopen_streams(void);
void snd_close_streams(void);
void snd_close_portaudio(void);
void snd_print_devices(void);
int snd_write_buf(void);
void snd_reset_samplerate_conv(int rec_or_stream);

int snd_callback(const void *input, void *output, unsigned long frameCount, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags,
                 void *userData);
int snd_callback2(const void *input, void *output, unsigned long frameCount, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags,
                  void *userData);

void snd_reset_streaming_compressor(void);
void snd_reset_recording_compressor(void);
int snd_get_dev_num_by_name(char *name);

#endif

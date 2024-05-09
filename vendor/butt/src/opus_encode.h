// opus encoding functions for butt
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

#ifndef OPUS_ENCODE_H
#define OPUS_ENCODE_H

#include <opus/opus.h>
#include <opus/opus_multistream.h>
#include <ogg/ogg.h>

#include "config.h"

#define OPUS_ENC_TYPE_REC    0
#define OPUS_ENC_TYPE_STREAM 1

#define OPUS_KEY_FRAME_OFFSET 50

#define OPUS_STATE_OK                 0
#define OPUS_STATE_NORMAL_FRAME       1
#define OPUS_STATE_NEW_STREAM         2
#define OPUS_STATE_LAST_FRAME         3
#define OPUS_STATE_NEW_SONG_AVAILABLE 4

#define OPUS_FRAME_SIZE 960 // Must be multiple of 120 (120 = 2,5ms, 960 = 20ms)
//#define OPUS_FRAME_SIZE (960/2) // Must be multiple of 120 (120 = 2,5ms, 960 = 20ms)
#define DEFAULT_OPUS_BITRATE 128000

typedef struct {
    int version;
    int channels; /* Number of channels: 1..255 */
    int preskip;
    ogg_uint32_t input_sample_rate;
    int gain; /* in dB S7.8 should be zero whenever possible */
    int channel_mapping;
    /* The rest is only used if channel_mapping != 0 */
    int nb_streams;
    int nb_coupled;
    unsigned char stream_map[255];
} OpusHeader;

struct opus_enc {
    ogg_stream_state os;
    ogg_page og;
    ogg_packet op;
    ogg_packet op_last;
    OpusEncoder *encoder;
    OpusHeader *header;
    int lookahead;
    unsigned char *header_data;
    unsigned char *tags;
    int tags_size;
    int header_size;

    ogg_int64_t prev_ganule;
    ogg_int64_t granulepos;

    int last_bitrate;
    int bitrate;
    int samplerate;
    int channel;
    int state;
    int bitrate_mode;
    int quality;
    int audio_type;
    int bandwidth;

    char song_title[256];

    unsigned char *buffer;
    unsigned char *buffer_last;
};

enum {
    OPUS_READY = 0,
    OPUS_BUSY = 1,
};

// extern opus_info  opus_vi;
extern OpusEncoder opus_vi;
extern char *opus_buf;
int opus_header_to_packet(const OpusHeader *h, unsigned char *packet, int len);

int opus_enc_alloc(opus_enc *opus);
int opus_enc_init(opus_enc *opus);
int opus_enc_reinit(opus_enc *opus);

void opus_enc_write_header(opus_enc *opus);
int opus_enc_encode(opus_enc *opus, float *pcm_buf, char *enc_buf, int size);
int opus_enc_flush(opus_enc *opus, char *enc_buf);
void opus_enc_close(opus_enc *opus);

bool opus_enc_is_valid_srate(int samplerate);
int opus_enc_get_valid_srate(int samplerate);
int opus_enc_get_samplerate(opus_enc *opus);

void opus_update_song_title(opus_enc *opus, char *song_title);

#endif

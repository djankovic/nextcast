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

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#include "opus_encode.h"

/* Copyright (C)2012 Xiph.Org Foundation
   File: opus_header.c

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdio.h>

typedef struct {
    unsigned char *data;
    int maxlen;
    int pos;
} Packet;

typedef struct {
    const unsigned char *data;
    int maxlen;
    int pos;
} ROPacket;

static int write_uint32(Packet *p, ogg_uint32_t val)
{
    if (p->pos > p->maxlen - 4) {
        return 0;
    }

    p->data[p->pos] = (val)&0xFF;
    p->data[p->pos + 1] = (val >> 8) & 0xFF;
    p->data[p->pos + 2] = (val >> 16) & 0xFF;
    p->data[p->pos + 3] = (val >> 24) & 0xFF;
    p->pos += 4;
    return 1;
}

static int write_uint16(Packet *p, ogg_uint16_t val)
{
    if (p->pos > p->maxlen - 2) {
        return 0;
    }

    p->data[p->pos] = (val)&0xFF;
    p->data[p->pos + 1] = (val >> 8) & 0xFF;
    p->pos += 2;
    return 1;
}

static int write_chars(Packet *p, const unsigned char *str, int nb_chars)
{
    int i;
    if (p->pos > p->maxlen - nb_chars) {
        return 0;
    }
    for (i = 0; i < nb_chars; i++) {
        p->data[p->pos++] = str[i];
    }
    return 1;
}

int opus_header_to_packet(const OpusHeader *h, unsigned char *packet, int len)
{
    int i;
    Packet p;
    unsigned char ch;

    p.data = packet;
    p.maxlen = len;
    p.pos = 0;
    if (len < 19) {
        return 0;
    }
    if (!write_chars(&p, (const unsigned char *)"OpusHead", 8)) {
        return 0;
    }
    /* Version is 1 */
    ch = 1;
    if (!write_chars(&p, &ch, 1)) {
        return 0;
    }

    ch = h->channels;
    if (!write_chars(&p, &ch, 1)) {
        return 0;
    }

    if (!write_uint16(&p, h->preskip)) {
        return 0;
    }

    if (!write_uint32(&p, h->input_sample_rate)) {
        return 0;
    }

    if (!write_uint16(&p, h->gain)) {
        return 0;
    }

    ch = h->channel_mapping;
    if (!write_chars(&p, &ch, 1)) {
        return 0;
    }

    if (h->channel_mapping != 0) {
        ch = h->nb_streams;
        if (!write_chars(&p, &ch, 1)) {
            return 0;
        }

        ch = h->nb_coupled;
        if (!write_chars(&p, &ch, 1)) {
            return 0;
        }

        /* Multi-stream support */
        for (i = 0; i < h->channels; i++) {
            if (!write_chars(&p, &h->stream_map[i], 1)) {
                return 0;
            }
        }
    }

    return p.pos;
}

int opus_enc_alloc(opus_enc *opus)
{
    opus->header = (OpusHeader *)calloc(1, sizeof(OpusHeader));
    opus->header_data = (unsigned char *)calloc(1, 1024);
    opus->tags = (unsigned char *)calloc(1, 1024);
    opus->buffer = (unsigned char *)calloc(1, 2 * OPUS_FRAME_SIZE * sizeof(float));
    opus->buffer_last = (unsigned char *)calloc(1, 2 * OPUS_FRAME_SIZE * sizeof(float));
    opus->granulepos = 0;

    memset(opus->song_title, 0, sizeof(opus->song_title));

    srand(time(NULL));

    return 0;
}

int opus_enc_init(opus_enc *opus)
{
    int err;
    int ret;
    int bandwidth;
    int audio_type;

    err = 0;
    ret = OPUS_OK;

    opus->header->gain = 0;
    opus->header->channels = opus->channel;

    if ((opus->bitrate < 8000) || (opus->bitrate > 510000)) {
        opus->bitrate = DEFAULT_OPUS_BITRATE;
    }

    if (opus->audio_type == 0) {
        audio_type = OPUS_APPLICATION_AUDIO;
    }
    else {
        audio_type = OPUS_APPLICATION_VOIP;
    }

    opus->header->input_sample_rate = 48000;
    opus->encoder = opus_encoder_create(opus->header->input_sample_rate, opus->channel, audio_type, &err);
    if (opus->encoder == NULL) {
        printf("Opus Encoder creation error: %s\n", opus_strerror(err));
        fflush(stdout);
        return 1;
    }

    ret |= opus_encoder_ctl(opus->encoder, OPUS_SET_BITRATE(opus->bitrate));

    if (opus->bitrate_mode == 0) { // CBR
        ret |= opus_encoder_ctl(opus->encoder, OPUS_SET_VBR(0));
    }
    else {
        ret |= opus_encoder_ctl(opus->encoder, OPUS_SET_VBR(1));
    }

    ret |= opus_encoder_ctl(opus->encoder, OPUS_SET_COMPLEXITY(10 - opus->quality));

    switch (opus->bandwidth) {
    case 0:
        bandwidth = OPUS_BANDWIDTH_FULLBAND; // 20 kHz
        break;
    case 1:
        bandwidth = OPUS_BANDWIDTH_SUPERWIDEBAND; // 12 kHz
        break;
    case 2:
        bandwidth = OPUS_BANDWIDTH_WIDEBAND; // 8 kHz
        break;
    case 3:
        bandwidth = OPUS_BANDWIDTH_MEDIUMBAND; // 6 kHz
        break;
    case 4:
        bandwidth = OPUS_BANDWIDTH_NARROWBAND; // 4 kHz
        break;
    default:
        bandwidth = OPUS_BANDWIDTH_FULLBAND;
        break;
    }

    ret |= opus_encoder_ctl(opus->encoder, OPUS_SET_MAX_BANDWIDTH(bandwidth));

    opus->last_bitrate = opus->bitrate;

    ret |= opus_encoder_ctl(opus->encoder, OPUS_GET_LOOKAHEAD(&opus->lookahead));
    opus->header->preskip = opus->lookahead;
    if (opus->state == OPUS_STATE_NEW_STREAM) {
        // opus->header->preskip += (OPUS_FRAME_SIZE - OPUS_KEY_FRAME_OFFSET);
        opus->header->preskip += OPUS_FRAME_SIZE;
    }

    if (ret != OPUS_OK) {
        return 1;
    }

    opus->header_size = opus_header_to_packet(opus->header, opus->header_data, 100);

    int tags_offset = 0;
    uint32_t len;
    uint32_t num_comments = opus->state == OPUS_STATE_NEW_STREAM ? 2 : 1;

    memcpy(opus->tags, "OpusTags", strlen("OpusTags"));
    tags_offset += strlen("OpusTags");

    len = strlen(opus_get_version_string());
    memcpy(opus->tags + tags_offset, &len, sizeof(len));
    tags_offset += sizeof(len);

    memcpy(opus->tags + tags_offset, opus_get_version_string(), strlen(opus_get_version_string()));
    tags_offset += strlen(opus_get_version_string());

    memcpy(opus->tags + tags_offset, &num_comments, sizeof(num_comments));
    tags_offset += sizeof(num_comments);

    len = strlen("ENCODER=") + strlen(PACKAGE_STRING);
    memcpy(opus->tags + tags_offset, &len, sizeof(len));
    tags_offset += sizeof(len);

    memcpy(opus->tags + tags_offset, "ENCODER=", strlen("ENCODER="));
    tags_offset += strlen("ENCODER=");

    memcpy(opus->tags + tags_offset, PACKAGE_STRING, strlen(PACKAGE_STRING));
    tags_offset += strlen(PACKAGE_STRING);

    len = strlen(opus->song_title);
    if (len > 0) {
        memcpy(opus->tags + tags_offset, &len, sizeof(len));
        tags_offset += sizeof(len);

        memcpy(opus->tags + tags_offset, opus->song_title, len);
        tags_offset += len;
    }

    opus->tags_size = tags_offset;

    /*
    int32_t val;

    opus_encoder_ctl (opus->encoder, OPUS_GET_BITRATE(&val));
    printf("OPUS bitrate: %d\n", val);

    opus_encoder_ctl (opus->encoder, OPUS_GET_COMPLEXITY(&val));
    printf("OPUS QUALITY: %d\n", val);

    opus_encoder_ctl (opus->encoder, OPUS_GET_APPLICATION(&val));
    printf("OPUS audio_type: %d\n", opus->audio_type );

    opus_encoder_ctl (opus->encoder, OPUS_GET_MAX_BANDWIDTH(&val));
    printf("OPUS bandwidth: %d\n", val);

    opus_encoder_ctl (opus->encoder, OPUS_GET_VBR(&val));
    printf("OPUS VBR: %d\n\n", val);
    */

    //printf("OPUS bitrate: %d\n", opus->bitrate);

    return 0;
}

// This function needs to be called before
// every connection
void opus_enc_write_header(opus_enc *opus)
{
    ogg_packet op;

    ogg_stream_clear(&opus->os);
    ogg_stream_init(&opus->os, rand());
    opus_encoder_ctl(opus->encoder, OPUS_RESET_STATE);

    op.b_o_s = 1;
    op.e_o_s = 0;
    op.granulepos = 0;
    op.packetno = 0;
    op.packet = opus->header_data;
    op.bytes = opus->header_size;
    ogg_stream_packetin(&opus->os, &op);

    op.b_o_s = 0;
    op.e_o_s = 0;
    op.granulepos = 0;
    op.packetno = opus->os.packetno;
    op.packet = opus->tags;
    op.bytes = opus->tags_size;
    ogg_stream_packetin(&opus->os, &op);
}

void opus_update_song_title(opus_enc *opus, char *song_title)
{
    snprintf(opus->song_title, sizeof(opus->song_title), "TITLE=%s", song_title);
    opus->state = OPUS_STATE_NEW_SONG_AVAILABLE;
}

int opus_enc_reinit(opus_enc *opus)
{
    opus_enc_close(opus);
    return opus_enc_init(opus);
}

int opus_enc_encode(opus_enc *opus, float *pcm_buf, char *enc_buf, int size)
{
    int w = 0;
    static int up_down = 1;
    int ret;
    ogg_packet op;

    if (size == 0) {
        return 0;
    }

    if (opus->encoder == NULL) {
        // printf("Opus Encoder NULL wtf?\n");
        return 0;
    }

    // Write header
    while (ogg_stream_flush(&opus->os, &opus->og) != 0) {
        memcpy(enc_buf + w, opus->og.header, opus->og.header_len);
        w += opus->og.header_len;
        memcpy(enc_buf + w, opus->og.body, opus->og.body_len);
        w += opus->og.body_len;
    }

    if (opus->last_bitrate != opus->bitrate) {
        if ((opus->bitrate < 9600) || (opus->bitrate > 320000)) {
            opus->bitrate = DEFAULT_OPUS_BITRATE;
        }
        opus_encoder_ctl(opus->encoder, OPUS_SET_BITRATE(opus->bitrate));
        opus->last_bitrate = opus->bitrate;
    }

    opus->granulepos += OPUS_FRAME_SIZE;
    // opus_encoder_ctl(opus->encoder, OPUS_SET_PREDICTION_DISABLED(1));

    if (opus->state == OPUS_STATE_LAST_FRAME) {
        opus->granulepos -= OPUS_FRAME_SIZE - opus->lookahead;
        opus_encoder_ctl(opus->encoder, OPUS_SET_PREDICTION_DISABLED(1));
        op.e_o_s = 1;
    }
    else {
        op.e_o_s = 0;
    }

    if (opus->state == OPUS_STATE_NEW_STREAM) {
        opus_encoder_ctl(opus->encoder, OPUS_SET_PREDICTION_DISABLED(0));
        ogg_stream_packetin(&opus->os, &opus->op_last);
        while (ogg_stream_flush(&opus->os, &opus->og) != 0) {
            memcpy(enc_buf + w, opus->og.header, opus->og.header_len);
            w += opus->og.header_len;
            memcpy(enc_buf + w, opus->og.body, opus->og.body_len);
            w += opus->og.body_len;
        }
        opus->granulepos += OPUS_FRAME_SIZE;
    }

    ret = opus_encode_float(opus->encoder, pcm_buf, OPUS_FRAME_SIZE, opus->buffer, 2 * OPUS_FRAME_SIZE * sizeof(float));

    op.b_o_s = 0;
    op.granulepos = opus->granulepos;
    op.packetno = opus->os.packetno;
    op.packet = opus->buffer;
    op.bytes = ret;

    if (opus->state == OPUS_STATE_LAST_FRAME) {
        opus->op_last = op;
        opus->op_last.granulepos = OPUS_FRAME_SIZE;
        opus->op_last.e_o_s = 0;
        opus->op_last.packet = opus->buffer_last;
        memcpy(opus->buffer_last, op.packet, op.bytes);
    }

    ogg_stream_packetin(&opus->os, &op);

    while (ogg_stream_flush(&opus->os, &opus->og) != 0) {
        memcpy(enc_buf + w, opus->og.header, opus->og.header_len);
        w += opus->og.header_len;
        memcpy(enc_buf + w, opus->og.body, opus->og.body_len);
        w += opus->og.body_len;
    }

    switch (opus->state) {
    case OPUS_STATE_LAST_FRAME:
        opus->state = OPUS_STATE_NEW_STREAM;
        break;
    case OPUS_STATE_NEW_STREAM:
        opus->state = OPUS_STATE_NORMAL_FRAME;
        break;
    }

    return w;
}

int opus_enc_flush(opus_enc *opus, char *enc_buf)
{
    int w = 0;
    while (ogg_stream_flush(&opus->os, &opus->og) != 0) {
        memcpy(enc_buf + w, opus->og.header, opus->og.header_len);
        w += opus->og.header_len;
        memcpy(enc_buf + w, opus->og.body, opus->og.body_len);
        w += opus->og.body_len;
    }

    return w;
}

void opus_enc_close(opus_enc *opus)
{
    opus->granulepos = 0;
    ogg_stream_clear(&opus->os);
    opus_encoder_destroy(opus->encoder);
}

int opus_enc_get_samplerate(opus_enc *opus)
{
    int32_t val;
    opus_encoder_ctl(opus->encoder, OPUS_GET_SAMPLE_RATE(&val));

    return val;
}

// Currently unusedint opus_enc_get_samplerate(opus_enc *opus);
/*

bool opus_enc_is_valid_srate(int samplerate)
{
    int i;
    int valid_srates[] = {8000, 12000, 16000, 24000, 48000};

    for (i = 0; i < 5; i++)
    {
        if (samplerate == valid_srates[i]) {
            return true;
        }
    }

    return false;
}

int opus_enc_get_valid_srate(int samplerate)
{
    int i;
    int valid_srates[] = {8000, 12000, 16000, 24000, 48000};

    if (samplerate > 48000) {
        return 48000;
    }

    for (i = 0; i < 5; i++) {
        if (samplerate < valid_srates[i]) {
            return valid_srates[i];
        }
    }

    return -1;
}
*/

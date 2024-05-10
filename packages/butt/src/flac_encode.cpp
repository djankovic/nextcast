// flac encoding functions for butt
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
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "flac_encode.h"

#define INT24_MAX ((1 << 23) - 1)
#define INT24_MIN (-(1 << 23))

FLAC__uint64 g_bytes_written = 0;

static void progress_callback(const FLAC__StreamEncoder *encoder, FLAC__uint64 bytes_written, FLAC__uint64 samples_written, unsigned frames_written,
                              unsigned total_frames_estimate, void *client_data);

static FLAC__StreamEncoderWriteStatus ogg_stream_callback(const FLAC__StreamEncoder *encoder, const FLAC__byte buffer[], size_t bytes, unsigned samples,
                                                          unsigned current_frame, void *client_data);

uint8_t *g_enc_p = NULL;
uint8_t *g_hdr_p;
uint8_t g_hdr[1024];

int g_hdr_written;
int g_hdr_size = 0;

static bool set_settings(flac_enc *flac)
{
    FLAC__bool ret = true;
    ret &= FLAC__stream_encoder_set_verify(flac->encoder, false);
    ret &= FLAC__stream_encoder_set_compression_level(flac->encoder, 5);
    ret &= FLAC__stream_encoder_set_channels(flac->encoder, flac->channel);
    ret &= FLAC__stream_encoder_set_bits_per_sample(flac->encoder, flac->bit_depth);
    ret &= FLAC__stream_encoder_set_sample_rate(flac->encoder, flac->samplerate);
    ret &= FLAC__stream_encoder_set_total_samples_estimate(flac->encoder, 0);
    ret &= FLAC__stream_encoder_set_ogg_serial_number(flac->encoder, rand());

    return ret;
}

static void inject_new_song_title(flac_enc *flac)
{
    int comment_len;

    if (flac->vorbis_comment.length > 0) {
        for (uint32_t i = 0; i < flac->vorbis_comment.data.vorbis_comment.num_comments; i++)
            free(flac->vorbis_comment.data.vorbis_comment.comments[i].entry);

        free(flac->vorbis_comment.data.vorbis_comment.comments);
        memset(&flac->vorbis_comment, 0, sizeof(FLAC__StreamMetadata));
    }

    comment_len = strlen(flac->song_title);

    flac->vorbis_comment.is_last = true;
    flac->vorbis_comment.type = FLAC__METADATA_TYPE_VORBIS_COMMENT;
    flac->vorbis_comment.length = 3 * sizeof(uint32_t) + comment_len; // size of data field with ".length" and ".num_comments" variables included
    flac->vorbis_comment.data.vorbis_comment.vendor_string.length = 0;
    flac->vorbis_comment.data.vorbis_comment.vendor_string.entry = 0;
    flac->vorbis_comment.data.vorbis_comment.num_comments = 1;

    flac->vorbis_comment.data.vorbis_comment.comments = (FLAC__StreamMetadata_VorbisComment_Entry *)malloc(
        flac->vorbis_comment.data.vorbis_comment.num_comments * sizeof(FLAC__StreamMetadata_VorbisComment_Entry));

    flac->vorbis_comment.data.vorbis_comment.comments[0].length = comment_len;
    flac->vorbis_comment.data.vorbis_comment.comments[0].entry = (FLAC__byte *)malloc(comment_len + 1);

    memcpy(flac->vorbis_comment.data.vorbis_comment.comments[0].entry, flac->song_title, comment_len + 1);
}

int flac_enc_init(flac_enc *flac)
{
    FLAC__bool ret = true;

    if ((flac->encoder = FLAC__stream_encoder_new()) == NULL) {
        fprintf(stderr, "ERROR: allocating encoder\n");
        return 1;
    }
    memset(&flac->vorbis_comment, 0, sizeof(FLAC__StreamMetadata));

    if (flac->enc_type == FLAC_ENC_TYPE_STREAM) {
        ret = flac_enc_init_ogg_stream(flac);
    }

    if (ret == true) {
        flac->state = FLAC_STATE_OK;
        return 0;
    }
    else {
        return 1;
    }
}

int flac_enc_init_FILE(flac_enc *flac, FILE *fout)
{
    FLAC__StreamEncoderInitStatus init_status;

    set_settings(flac);

    init_status = FLAC__stream_encoder_init_FILE(flac->encoder, fout, progress_callback, /*client_data=*/NULL);
    if (init_status != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
        fprintf(stderr, "ERROR: initializing encoder: %s\n", FLAC__StreamEncoderInitStatusString[init_status]);

        return 1;
    }

    return 0;
}

FLAC__bool flac_enc_init_ogg_stream(flac_enc *flac)
{
    FLAC__StreamEncoderInitStatus init_status;

    g_hdr_written = 0;
    g_hdr_size = 0;
    g_hdr_p = g_hdr;

    set_settings(flac);

    if (flac->state == FLAC_STATE_UPDATE_META_DATA) {
        inject_new_song_title(flac);
        FLAC__StreamMetadata *comment_pointer = &flac->vorbis_comment;
        FLAC__stream_encoder_set_metadata(flac->encoder, &comment_pointer, 1);
        flac->state = FLAC_STATE_OK;
    }

    init_status = FLAC__stream_encoder_init_ogg_stream(flac->encoder, NULL, ogg_stream_callback, NULL, NULL, NULL, NULL);
    if (init_status != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
        fprintf(stderr, "ERROR: initializing encoder: %s\n", FLAC__StreamEncoderInitStatusString[init_status]);
        return false;
    }

    flac->state = FLAC_STATE_OK;

    return true;
}

void flac_update_song_title(flac_enc *flac, char *song_title)
{
    snprintf(flac->song_title, sizeof(flac->song_title), "TITLE=%s", song_title);
    flac->state = FLAC_STATE_NEW_SONG_AVAILABLE;
}

void flac_set_initial_song_title(flac_enc *flac, char *song_title)
{
    snprintf(flac->song_title, sizeof(flac->song_title), "TITLE=%s", song_title);
    flac->state = FLAC_STATE_UPDATE_META_DATA;
    flac_enc_reinit(flac);
}

int flac_enc_encode(flac_enc *flac, float *pcm_float, int samples_per_chan, int channel)
{
    int i;
    int samples_left;
    int chunk_size;
    int32_t *pcm_int32;

    int samples_written = 0;

    chunk_size = 2048;
    if (chunk_size > samples_per_chan) {
        chunk_size = samples_per_chan;
    }

    samples_left = samples_per_chan;

    pcm_int32 = (int32_t *)pcm_float;
    while (samples_left > 0) {
        for (i = 0; i < chunk_size * channel; i++) {
            if (pcm_float[i] > 0) {
                if (flac->bit_depth == 16) {
                    pcm_int32[i] = (int32_t)round(fmin(pcm_float[i + samples_written] * INT16_MAX, INT16_MAX));
                }
                else { // 24 bit
                    pcm_int32[i] = (int32_t)round(fmin(pcm_float[i + samples_written] * INT24_MAX, INT24_MAX));
                }
            }
            else {
                if (flac->bit_depth == 16) {
                    pcm_int32[i] = (int32_t)round(fmax(-pcm_float[i + samples_written] * INT16_MIN, INT16_MIN));
                }
                else { // 24 bit
                    pcm_int32[i] = (int32_t)round(fmax(-pcm_float[i + samples_written] * INT24_MIN, INT24_MIN));
                }
            }
        }

        FLAC__stream_encoder_process_interleaved(flac->encoder, pcm_int32, chunk_size);

        samples_written += chunk_size * channel;
        samples_left -= chunk_size;

        if (samples_left < chunk_size) {
            chunk_size = samples_left;
        }
    }

    return 0;
}

int flac_enc_encode_stream(flac_enc *flac, float *pcm_float, uint8_t *enc_buf, int samples_per_chan, int channel, int new_stream)
{
    int i;
    int samples_left;
    int chunk_size;
    int bytes_written;
    int32_t *pcm_int32;

    g_enc_p = enc_buf;

    if (g_hdr_written == 0) {
        memcpy(g_enc_p, g_hdr, g_hdr_size);
        g_enc_p += g_hdr_size;
        g_hdr_written = 1;
    }

    int samples_written = 0;

    chunk_size = 2048;
    if (chunk_size > samples_per_chan) {
        chunk_size = samples_per_chan;
    }

    samples_left = samples_per_chan;

    pcm_int32 = (int32_t *)pcm_float;
    float sum;
    while (samples_left > 0) {
        // Convert 16bit samples to 32bit samples
        sum = 0;
        for (i = 0; i < chunk_size * channel; i++) {
            sum += abs(pcm_float[i + samples_written]);

            if (pcm_float[i + samples_written] > 0) {
                if (flac->bit_depth == 16) {
                    pcm_int32[i] = (int32_t)round(fmin(pcm_float[i + samples_written] * INT16_MAX, INT16_MAX));
                }
                else { // 24 bit
                    pcm_int32[i] = (int32_t)round(fmin(pcm_float[i + samples_written] * INT24_MAX, INT24_MAX));
                }
            }
            else {
                if (flac->bit_depth == 16) {
                    pcm_int32[i] = (int32_t)round(fmax(-pcm_float[i + samples_written] * INT16_MIN, INT16_MIN));
                }
                else { // 24 bit
                    pcm_int32[i] = (int32_t)round(fmax(-pcm_float[i + samples_written] * INT24_MIN, INT24_MIN));
                }
            }
        }

        // Add some noise with minimum volume in case the audio data is totaly silent.
        // This makes sure the FLAC encoder returns encoded data.
        // Otherwise, if the encoder receives 100 % silence, no encoded audio data
        // would be send to the streaming server and thus would drop the connection.
        if (sum == 0) {
            for (i = 0; i < chunk_size * channel; i++) {
                pcm_int32[i] = (rand() % 3) - 1;
            }
        }

        FLAC__stream_encoder_process_interleaved(flac->encoder, pcm_int32, chunk_size);

        samples_written += chunk_size * channel;
        samples_left -= chunk_size;

        if (samples_left < chunk_size) {
            chunk_size = samples_left;
        }
    }

    if (flac->state == FLAC_STATE_NEW_SONG_AVAILABLE) {
        FLAC__stream_encoder_finish(flac->encoder);
        flac->state = FLAC_STATE_UPDATE_META_DATA;
    }

    bytes_written = (int)(g_enc_p - enc_buf);

    return bytes_written;
}

int flac_enc_get_samplerate(flac_enc *flac)
{
    return FLAC__stream_encoder_get_sample_rate(flac->encoder);
}

int flac_enc_reinit(flac_enc *flac)
{
    flac_enc_close(flac);
    return flac_enc_init(flac);
}

void flac_enc_close_file(flac_enc *flac)
{
    if (flac->encoder != NULL) {
        FLAC__stream_encoder_finish(flac->encoder);
        flac_enc_close(flac);
    }
}

void flac_enc_close(flac_enc *flac)
{
    if (flac->encoder != NULL) {
        FLAC__stream_encoder_delete(flac->encoder);
    }

    flac->encoder = NULL;
}

void progress_callback(const FLAC__StreamEncoder *encoder, FLAC__uint64 bytes_written, FLAC__uint64 samples_written, unsigned frames_written,
                       unsigned total_frames_estimate, void *client_data)
{
    g_bytes_written = bytes_written;
}

FLAC__StreamEncoderWriteStatus ogg_stream_callback(const FLAC__StreamEncoder *encoder, const FLAC__byte buffer[], size_t bytes, unsigned samples,
                                                   unsigned current_frame, void *client_data)
{
    if (current_frame == 0) {
        // assemble header
        memcpy(g_hdr_p, buffer, bytes);
        g_hdr_p += bytes;
        g_hdr_size += bytes;
        return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
    }

    if (g_enc_p != NULL) {
        memcpy(g_enc_p, buffer, bytes);
        g_enc_p += bytes;
    }

    return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
}

FLAC__uint64 flac_enc_get_bytes_written(void)
{
    return g_bytes_written;
}

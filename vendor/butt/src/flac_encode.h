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

#ifndef FLAC_ENCODE_H
#define FLAC_ENCODE_H

#include <FLAC/stream_encoder.h>

#define FLAC_ENC_TYPE_REC    0
#define FLAC_ENC_TYPE_STREAM 1

#define FLAC_STATE_OK                 0
#define FLAC_STATE_NEW_SONG_AVAILABLE 1
#define FLAC_STATE_UPDATE_META_DATA   2

struct flac_enc {
    FLAC__StreamEncoder *encoder;
    int bitrate;
    int samplerate;
    int channel;
    int enc_type;
    char song_title[256];
    int state;
    int bit_depth;
    FLAC__StreamMetadata vorbis_comment;
};

int flac_enc_init(flac_enc *flac);
int flac_enc_init_FILE(flac_enc *flac, FILE *fout);
int flac_enc_encode(flac_enc *flac, float *pcm_buf, int samples_per_chan, int channel);
int flac_enc_encode_stream(flac_enc *flac, float *pcm_buf, uint8_t *enc_buf, int samples_per_chan, int channel, int new_stream);
FLAC__bool flac_enc_init_ogg_stream(flac_enc *flac);

void flac_update_song_title(flac_enc *flac, char *song_title);
void flac_set_initial_song_title(flac_enc *flac, char *song_title);

int flac_enc_get_samplerate(flac_enc *flac);
int flac_enc_reinit(flac_enc *flac);
FLAC__uint64 flac_enc_get_bytes_written(void);
void flac_enc_close(flac_enc *flac);
void flac_enc_close_file(flac_enc *flac);

#endif

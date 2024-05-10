// wav header functions for butt
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

#ifndef WAV_HEADER_H
#define WAV_HEADER_H

#include <stdio.h>
#include <stdint.h>

#define WAV_HDR_SIZE 44

typedef union {
    char data[WAV_HDR_SIZE];

    struct wav_header {
        char riff_id[4];     //"RIFF"
        uint32_t riff_size;  // file_length - 8
        char riff_format[4]; //"WAVE"

        char fmt_id[4];           //"FMT "(the space is essential
        uint32_t fmt_size;        // fmt data size (16 bits)
        uint16_t fmt_format;      // format (PCM = 1)
        uint16_t fmt_channel;     // 1 = mono; 2 = stereo
        uint32_t fmt_samplerate;  //...
        uint32_t fmt_byte_rate;   // samplerate * block_align
        uint16_t fmt_block_align; // channels * bits_per_sample / 8
        uint16_t fmt_bps;         // bits per sample = 16

        char data_id[4];    //"data"
        uint32_t data_size; // file_length - 44
    } wav;
} wav_hdr_t;

int wav_write_header(FILE *fd, short ch, int srate, short bps);

#endif

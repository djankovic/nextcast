// wav functions for butt
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

#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <string.h>

#include "wav_header.h"

int wav_write_header(FILE *fd, short ch, int srate, short bps)
{
    off_t file_size;
    uint32_t wav_size;
    wav_hdr_t hdr;

    file_size = ftello(fd);
    if (file_size == -1) {
        return -1;
    }

    wav_size = file_size > UINT32_MAX ? UINT32_MAX : (uint32_t)file_size;

    memcpy(&hdr.wav.riff_id, "RIFF", 4);
    hdr.wav.riff_size = wav_size >= WAV_HDR_SIZE ? (uint32_t)(wav_size - 8) : 0;
    memcpy(&hdr.wav.riff_format, "WAVE", 4);
    memcpy(hdr.wav.fmt_id, "fmt ", 4);

    hdr.wav.fmt_size = 16;
    hdr.wav.fmt_format = 1;
    hdr.wav.fmt_channel = ch;
    hdr.wav.fmt_samplerate = srate;
    hdr.wav.fmt_byte_rate = srate * bps * ch / 8;
    hdr.wav.fmt_block_align = bps * ch / 8;
    hdr.wav.fmt_bps = bps;

    memcpy(&hdr.wav.data_id, "data", 4);
    hdr.wav.data_size = wav_size >= WAV_HDR_SIZE ? (uint32_t)(wav_size - WAV_HDR_SIZE) : 0;

    // write header to the beginning of the file
    rewind(fd);
    fwrite(&hdr.data, 1, sizeof(wav_hdr_t), fd);

    // set the fd back to the fileend
    fseeko(fd, 0, SEEK_END);

    return 0;
}

// butt - broadcast using this tool
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

#ifndef BUTT_H
#define BUTT_H

#include "config.h"
#ifndef BUILD_CLIENT
#include "timer.h"
#include "lame_encode.h"
#include "vorbis_encode.h"
#include "opus_encode.h"
#include "flac_encode.h"
#include "aac_encode.h"

extern bool recording;  // TRUE if butt is recording
extern bool connected;  // TRUE if butt is connected to server
extern bool disconnect; // TRUE if butt should disconnect
extern bool streaming;

extern int stream_socket;
extern double kbytes_sent;
extern double kbytes_written;

extern unsigned int record_start_hour; // the hour when last recording started

extern timer_ms_t rec_timer;
extern timer_ms_t stream_timer;

extern lame_enc lame_stream;
extern lame_enc lame_rec;
extern vorbis_enc vorbis_stream;
extern vorbis_enc vorbis_rec;
extern opus_enc opus_stream;
extern opus_enc opus_rec;
extern flac_enc flac_rec;
extern flac_enc flac_stream;
#ifdef HAVE_LIBFDK_AAC
extern aac_enc aac_stream;
extern aac_enc aac_rec;
#endif

#endif // #ifndef BUILD_CLIENT
#endif // #ifndef BUTT_H

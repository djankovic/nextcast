// shoutcast functions for butt
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

#ifndef SHOUTCAST_H
#define SHOUTCAST_H

enum {
    SC_OK = 0,
    SC_RETRY = 1,
    SC_ABORT = 2,
    SC_ASK = 3,
};

enum {
    SC_VERSION_UNKNOWN = 0,
    SC_VERSION_1 = 1,
    SC_VERSION_2 = 2,
};

int sc_update_song(char *song_name);
int sc_connect(void);
int sc_send(char *buf, int buf_len);
void sc_disconnect(void);
int sc_get_listener_count(void);

#endif

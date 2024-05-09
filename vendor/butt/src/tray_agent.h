// tray agent functions for butt
//
// Copyright 2007-2021 by Daniel Noethen.
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
#ifndef TRAY_AGENT_H
#define TRAY_AGENT_H

#ifdef WIN32
#include <windows.h>

enum tray_agent_cmd {
    TA_START = 0,
    TA_MINIMIZE = 1,
    TA_SERVER_NAME = 2,
    TA_CONNECT_STATE = 3,
    TA_SONG_UPDATE = 4,
};

int tray_agent_start(void);
int tray_agent_stop(void);
int tray_agent_send_cmd(int cmd);
int tray_agent_is_running(HWND *hwAgent);
void tray_agent_set_song(char *song);

#endif // ifdef WIN32
#endif // ifndef TRAY_AGENT_H

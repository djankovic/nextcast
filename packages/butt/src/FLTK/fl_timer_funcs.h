// FLTK GUI related functions
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

#ifndef FL_TIMER_FUNCS_H
#define FL_TIMER_FUNCS_H

void display_info_timer(void *);
void agent_watchdog_timer(void *);
void has_agent_been_started_timer(void *);
void has_agent_been_stopped_timer(void *);
void vu_meter_timer(void *);
void cmd_timer(void *);
void reconnect_timer(void *);
void is_connected_timer(void *);
void cfg_win_pos_timer(void *);
void songfile_timer(void *);
void stream_silence_timer(void *);
void record_silence_timer(void *);
void stream_signal_timer(void *);
void record_signal_timer(void *);
void wait_for_radioco_timer(void *);
void request_listener_count_timer(void *reset);

void display_rotate_timer(void *);
void app_timer(void *);

void split_recording_file_timer(void);
void split_recording_file(void);

extern const char *(*current_track_app)(int);
extern int g_vu_meter_timer_is_active;
extern int g_stop_vu_meter_timer;

#endif

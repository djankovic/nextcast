// timer related functions
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

#ifndef TIMER_H
#define TIMER_H

#include <time.h>
#include <sys/time.h>
#include <stdint.h>

typedef struct timer_ms {
    uint64_t start_time;
    uint64_t new_time;
    float duration;
    bool is_running = false;
} timer_ms_t;

void timer_init(timer_ms_t *t, float duration);
void timer_start(timer_ms_t *t);
int timer_is_elapsed(timer_ms_t *t);
char *timer_get_time_str(timer_ms_t *t);
float timer_get_elapsed_time(timer_ms_t *t);
uint64_t timer_get_cur_time(void);
void timer_reset(timer_ms_t *t);
void timer_stop(timer_ms_t *t);

#endif

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

#include <stdio.h>
#include <stdlib.h>

#include "timer.h"

void timer_init(timer_ms_t *t, float duration)
{
    t->duration = duration;
    t->is_running = false;
}

void timer_start(timer_ms_t *t)
{
    t->start_time = timer_get_cur_time();
    t->new_time = t->start_time;
    t->is_running = true;
}

int timer_is_elapsed(timer_ms_t *t)
{
    uint64_t cur_time = timer_get_cur_time();
    uint64_t duration = t->duration * 1000;

    if (cur_time >= t->new_time + duration) {
        t->new_time = timer_get_cur_time();
        return 1;
    }
    else {
        return 0;
    }
}

char *timer_get_time_str(timer_ms_t *t)
{
    static char time_str[64];
    long hour = 0, min = 0, sec = 0;
    uint64_t cur_time = timer_get_cur_time();

    sec = (cur_time - t->start_time) / 1000;

    min = sec / 60;
    hour = min / 60;
    min %= 60;
    sec %= 60;

    snprintf(time_str, sizeof(time_str), "%02ld:%02ld:%02ld", hour, min, sec);

    return time_str;
}

float timer_get_elapsed_time(timer_ms_t *t)
{
    uint64_t diff_time = timer_get_cur_time() - t->start_time;
    return float(diff_time / 1000.0);
}

void timer_reset(timer_ms_t *t)
{
    t->is_running = false;
    t->start_time = timer_get_cur_time();
    t->new_time = t->start_time;
}

void timer_stop(timer_ms_t *t)
{
    t->is_running = false;
}

uint64_t timer_get_cur_time(void)
{
    struct timeval time;
    gettimeofday(&time, NULL);

    return (uint64_t)time.tv_sec * 1000 + time.tv_usec / 1000;
}

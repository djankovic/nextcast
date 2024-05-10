// vu-meter functions for butt
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
//
#include <math.h>
#include "flgui.h"

#include "vu_meter.h"

void vu_left_peak_timer(void *);
void vu_right_peak_timer(void *);

float left_peak;
float right_peak;

void vu_init(void)
{
    left_peak = -90;
    right_peak = -90;
}
void vu_meter(float left, float right)
{
    float left_dB;
    float right_dB;

    if (left > 0) {
        left_dB = 20 * log10(left);
    }
    else {
        left_dB = -90;
    }

    if (right > 0) {
        right_dB = 20 * log10(right);
    }
    else {
        right_dB = -90;
    }

    if (left_dB > left_peak) {
        left_peak = left_dB;
        Fl::remove_timeout(&vu_left_peak_timer);
        Fl::add_timeout(2 /*second*/, &vu_left_peak_timer);
    }

    if (right_dB > right_peak) {
        right_peak = right_dB;
        Fl::remove_timeout(&vu_right_peak_timer);
        Fl::add_timeout(2 /*second*/, &vu_right_peak_timer);
    }

    fl_g->vumeter->value(left_dB, right_dB, left_peak, right_peak);
}

void vu_left_peak_timer(void *)
{
    left_peak = -90;
}

void vu_right_peak_timer(void *)
{
    right_peak = -90;
}

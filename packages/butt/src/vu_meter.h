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

#ifndef VU_METER
#define VU_METER

#include <FL/Fl_Widget.H>

enum LED_state {
    OFF = 0,
    ON = 1,
};

struct vu_led_t {
    float thld;

    struct {
        Fl_Widget *widget;
        int is_peak;
    } left, right;
};

void vu_init(void);
void vu_meter(float left, float right);

#endif

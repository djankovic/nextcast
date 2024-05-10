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

#ifndef FL_MY_INVISIBLE_BOX
#define FL_MY_INVISIBLE_BOX

#include <FL/Fl.H>
#include <FL/Fl_Box.H>

#include "fl_callbacks.h"

class Fl_My_Invisible_Box : public Fl_Box {
  public:
    Fl_My_Invisible_Box(int x, int y, int w, int h, const char *label = 0) : Fl_Box(x, y, w, h, label)
    {
        enabled = 0;
    }

    void disable(void)
    {
        enabled = 0;
    }
    
    void enable(void)
    {
        enabled = 1;
    }
    
    int handle(int e)
    {
        if (enabled == 1) {
            switch (e) {
            case FL_ENTER:
                show_vu_tabs();
                break;
            case FL_LEAVE:
                hide_vu_tabs();
                break;
            }
        }
        return Fl_Box::handle(e);
    }
    
private:
    int enabled;
};

#endif

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

#ifndef FL_MY_DOUBLE_WINDOW_H
#define FL_MY_DOUBLE_WINDOW_H

#include <FL/x.H>
#include <FL/Fl_Double_Window.H>

#ifdef WIN32
#include <windows.h>
#include "tray_agent.h"
#include "fl_timer_funcs.h"
#include "fl_callbacks.h"
#elif __APPLE__
#import <Cocoa/Cocoa.h>
#else
#include <X11/Xlib.h>
#endif

class Fl_My_Double_Window : public Fl_Double_Window {
  public:
    bool is_main_window = false;
    bool minimize_to_tray = false;

    void stay_on_top(int ontop)
    {
#ifdef WIN32
        SetWindowPos(fl_xid(this), ontop ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER);

#elif __APPLE__

        if (ontop) {
            [fl_xid(this) setLevel:NSFloatingWindowLevel];
        }
        else {
            [fl_xid(this) setLevel:NSNormalWindowLevel];
        }

#else // UNIX
        XEvent e;

        Atom WM_STATE = XInternAtom(fl_display, "_NET_WM_STATE", 0);
        Atom WM_STATE_ABOVE = XInternAtom(fl_display, "_NET_WM_STATE_ABOVE", 0);

        e.xclient.type = ClientMessage;
        e.xclient.window = fl_xid(this);
        e.xclient.message_type = WM_STATE;
        e.xclient.format = 32;
        e.xclient.data.l[0] = (long)ontop;
        e.xclient.data.l[1] = (long)WM_STATE_ABOVE;
        e.xclient.data.l[2] = (long)0;
        e.xclient.data.l[3] = (long)0;
        e.xclient.data.l[4] = (long)0;

        XSendEvent(fl_display, RootWindow(fl_display, fl_screen), 0, SubstructureNotifyMask | SubstructureRedirectMask, &e);
        XFlush(fl_display);
#endif
    }

#ifdef WIN32
    int handle(int event)
    {
        if (event == FL_HIDE && is_main_window == true && minimize_to_tray == true) {
            int ret = tray_agent_send_cmd(TA_MINIMIZE);
            if (ret == 0) {
                Fl::remove_timeout(&agent_watchdog_timer);
                Fl::add_timeout(1, &agent_watchdog_timer);
            }
        }

        return Fl_Double_Window::handle(event);
    }
#endif

    Fl_My_Double_Window(int W, int H, const char *l = 0) : Fl_Double_Window(W, H, l) {}

    Fl_My_Double_Window(int X, int Y, int W, int H, const char *l = 0) : Fl_Double_Window(X, Y, W, H, l) {}

    ~Fl_My_Double_Window() { hide(); }
};

#endif // FL_MY_DOUBLE_WINDOW_H

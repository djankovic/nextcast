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

#ifndef FL_MY_VALUE_SLIDER
#define FL_MY_VALUE_SLIDER

#include <stdlib.h>
#include <string.h>
#include <FL/fl_draw.H>
#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Slider.H>
#include <FL/Fl_Menu_Window.H>
#include <FL/Fl_Tooltip.H>
#include <FL/Fl_Spinner.H>

#include "gettext.h"

class Fl_My_Value_Slider;

// FLOATING TIP WINDOW
class TipWin : public Fl_Menu_Window {
    char tip[40];

  public:
    TipWin() : Fl_Menu_Window(1, 1)
    { // will autosize
        strcpy(tip, "X.XX");
        set_override();
        end();
    }
    void draw(void)
    {
        draw_box(FL_BORDER_BOX, 0, 0, w(), h(), Fl_Color(175));
        fl_color(FL_BLACK);
        fl_font(labelfont(), labelsize());
        fl_draw(tip, 3, 3, w() - 6, h() - 6, Fl_Align(FL_ALIGN_LEFT | FL_ALIGN_WRAP));
        show();
    }
    void value(char *t)
    {
        strncpy(tip, t, sizeof(tip) - 1);
        // Recalc size of window
        fl_font(labelfont(), labelsize());
        int W = w(), H = h();
        fl_measure(tip, W, H, 0);
        W += 8;
        size(W, H);
        redraw();
    }
};

class My_Value_Input : public Fl_Value_Input {
  public:
    My_Value_Input(int x, int y, int w, int h, const char *label = 0) : Fl_Value_Input(x, y, w, h, label) {}

    void (*custom_callback)(Fl_Widget *, void *);
    void *data = NULL;

    int handle(int e)
    {
        switch (e) {
        case FL_KEYDOWN:
            if (Fl::event_key() == FL_Enter) {
                custom_callback(this, data);
                return 1;
            }
            break;
        }
        return Fl_Value_Input::handle(e);
    }
};

// VALUE SLIDER WITH FLOATING TIP WINDOW
class Fl_My_Value_Slider : public Fl_Slider {
    TipWin *tipwin;
    Fl_Window *window_value;
    My_Value_Input *input_value;

  public:
    float default_value = 0.0;
    bool hide_tipwin = false;

    Fl_My_Value_Slider(int x, int y, int w, int h, const char *l = 0) : Fl_Slider(x, y, w, h, l)
    {
        //     type(FL_HORIZONTAL);

        Fl_Group *save = Fl_Group::current(); // save current widget..
        tipwin = new TipWin();                // ..because this trashes it
        tipwin->hide();
        create_value_window();
        Fl_Group::current(save); // ..then back to previous.
    }

    void value_cb2(const char *suffix)
    {
        char tip[40];
        snprintf(tip, sizeof(tip), "%.2f%s", value(), suffix);
        tipwin->value(tip);
        tipwin->position(Fl::event_x_root() + 20, Fl::event_y_root());
    }

    void create_value_window()
    {
        int window_width = 180, window_height = 90;

        window_value = new Fl_Window(0, 0, window_width, window_height);
        window_value->user_data(this);
        window_value->set_modal();
        window_value->label(label());
        // window_value->border(0);
        window_value->color(fl_darker(window_value->color()));

        /*    Fl_Box *title = new Fl_Box(0, 5, window_width, 10);
            title->label(label());
            title->align(FL_ALIGN_INSIDE);*/

        input_value = new My_Value_Input(5, 25, 165, 22);
        input_value->align(FL_ALIGN_TOP_LEFT);
        input_value->custom_callback = &input_value_cb_;

        Fl_Button *button_close = new Fl_Button(5, 55, 80, 22, _("Close"));
        button_close->box(FL_ENGRAVED_BOX);
        button_close->callback((Fl_Callback *)button_close_cb_);

        Fl_Button *button_ok = new Fl_Button(5 + 80 + 5, 55, 80, 22, _("Ok"));
        button_ok->box(FL_ENGRAVED_BOX);
        button_ok->callback((Fl_Callback *)button_ok_cb_);

        window_value->end();
    }

    // MARK: Callbacks

    // Close button
    static void button_close_cb_(Fl_Widget *widget, void *data)
    {
        Fl_Widget *p = widget;
        while (p->parent()) {
            p = p->parent();
        }

        ((Fl_My_Value_Slider *)p->user_data())->button_close_cb(widget, p->user_data());
    }
    void button_close_cb(Fl_Widget *widget, void *data) { this->window_value->hide(); }

    // ok button
    static void button_ok_cb_(Fl_Widget *widget, void *data)
    {
        Fl_Widget *p = widget;
        while (p->parent()) {
            p = p->parent();
        }

        ((Fl_My_Value_Slider *)p->user_data())->button_ok_cb(widget, data);
    }

    void button_ok_cb(Fl_Widget *button, void *data)
    {
        this->set_value(clamp(this->input_value->value()));
        this->do_callback();
        this->parent()->redraw();
        this->window_value->hide();
    }

    // input field
    static void input_value_cb_(Fl_Widget *widget, void *data)
    {
        Fl_Widget *p = widget;
        while (p->parent()) {
            p = p->parent();
        }

        ((Fl_My_Value_Slider *)p->user_data())->input_value_cb(widget, data);
    }

    void input_value_cb(Fl_Widget *button, void *data)
    {
        this->set_value(clamp(this->input_value->value()));
        this->do_callback();
        this->parent()->redraw();
    }

    int handle(int e)
    {
        switch (e) {
        case FL_PUSH:
            if (Fl::event_button() == FL_LEFT_MOUSE) {
                if (Fl::event_clicks()) // move slider to zero when double clicking
                {
                    this->value(default_value);
                    this->do_callback();
                    this->parent()->redraw();
                    return 1;
                }
                if (hide_tipwin == false) {
                    tipwin->position(Fl::event_x_root() + 20, Fl::event_y_root());
                    tipwin->show();
                }
            }
            if (Fl::event_button() == FL_RIGHT_MOUSE) {
                return 1;
            }

            break;
        case FL_RELEASE: // release mouse
            tipwin->hide();
            if (Fl::event_button() == FL_RIGHT_MOUSE) {
                char input_label[64];
                float current_slider_value = value();
                input_value->value(current_slider_value);
                window_value->position(Fl::event_x_root() - window_value->w() / 2, Fl::event_y_root() - window_value->h() - 15);

                if (this->minimum() < this->maximum()) {
                    snprintf(input_label, sizeof(input_label), "Min: %.2f    Max: %.2f", this->minimum(), this->maximum());
                }
                else {
                    snprintf(input_label, sizeof(input_label), "Min: %.2f    Max: %.2f", this->maximum(),
                             this->minimum()); // Reversed direction (for vertical sliders)
                }

                input_value->copy_label(input_label);
                input_value->take_focus();
                window_value->show();

                return 1;
            }
        case FL_HIDE:  // valuator goes away
        case FL_LEAVE: // leave focus
            // Make sure tipwin closes when app closes
            tipwin->hide();
            break;
        }
        return (Fl_Slider::handle(e));
    }
};

#endif

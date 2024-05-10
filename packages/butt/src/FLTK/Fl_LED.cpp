#include <stdlib.h>
#include <math.h>
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Color_Chooser.H>

#include "Fl_LED.h"

LED::LED(int X, int Y, int W, int H, const char *L) : Fl_Group(X, Y, W, H, L)
{
    bg_color = color();
    width = w();
    height = h();
    x_origin = x();
    y_origin = y();
    LED_color = fl_rgb_color((uchar)(6), (uchar)(209), (uchar)(40));
    state = LED_OFF;
}

void LED::draw()
{
    // Fl_Group::draw();
    //  Clear whole widget area
    // fl_rectf(x_origin, y_origin, width, height, bg_color);

    Fl_Color last_color = fl_color();

    Fl_Color draw_color;
    if (state == LED_ON) {
        draw_color = LED_color;
    }
    else {
        uchar r, g, b;
        Fl::get_color(LED_color, r, g, b);
        draw_color = fl_rgb_color(r / 2, g / 2, b / 2);
    }

    fl_color(draw_color);
    fl_pie(x_origin, y_origin, width - 1, height - 1, 0, 360);

    fl_color(FL_BLACK);
    fl_arc(x_origin, y_origin, width, height, 0, 360);

    fl_color(last_color);
}

void LED::set_color(Fl_Color color)
{
    LED_color = color;
    redraw();
}

void LED::set_state(LED_STATE on_off)
{
    state = on_off;
    redraw();
}

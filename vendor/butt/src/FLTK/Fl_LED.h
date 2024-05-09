#ifndef FL_LED_H
#define FL_LED_H
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Color_Chooser.H>

class LED : public Fl_Group {
  public:
    enum LED_STATE {
        LED_OFF = 0,
        LED_ON = 1,
    };

    int width, height;
    int x_origin, y_origin;
    LED_STATE state;

    Fl_Color LED_color;
    Fl_Color bg_color;

    LED(int X, int Y, int W, int H, const char *L = 0);

    void set_color(Fl_Color color);
    void set_state(LED_STATE state);
    void draw();
};

#endif // FL_VU_METER_H

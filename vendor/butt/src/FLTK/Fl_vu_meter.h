#ifndef FL_VU_METER_H
#define FL_VU_METER_H
#include <stdlib.h>
#include <math.h>
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Color_Chooser.H>

class VUMeter : public Fl_Group {
  public:
    float left_dB, right_dB;

  private:
    float left_peak_dB, right_peak_dB;
    float min_value, max_value;
    int channel_height;
    int width, height;
    int x_origin, y_origin;
    int left_chan_y1, left_chan_y2;
    int right_chan_y1, right_chan_y2;
    int tick_distance_dB;
    int tick_distance_px;
    int num_of_ticks;

    Fl_Color bg_color;

  public:
    VUMeter(int X, int Y, int W, int H, const char *L = 0);

    void value(float left, float right, float left_peak, float right_peak);
    int dB_to_xpos(float dB);
    void draw();
};

#endif // FL_VU_METER_H

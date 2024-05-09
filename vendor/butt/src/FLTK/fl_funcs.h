// FLTK GUI related functions
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

#ifndef FL_FUNCS_H
#define FL_FUNCS_H

#include <FL/fl_ask.H>

#define GUI_LOOP()     Fl::run();
#define CHECK_EVENTS() Fl::check()

void fill_cfg_widgets(void);
void update_samplerates_list(void);
void update_codec_samplerates(void);
void update_channel_lists(void);
void print_info(const char *info, int info_type);
void write_log(const char *message);
void print_lcd(const char *text, int len, int home, int clear);
void test_file_extension(void);
int expand_string(char **str);
void init_main_gui_and_audio(void);
void ask_user_set_msg(char *m);
void ask_user_set_hash(char *h);
void ask_user_reset(void);
void ask_user_ask(void);
int ask_user_get_answer(void);
int ask_user_get_has_clicked(void);
void activate_stream_ui_elements(void);
void deactivate_stream_ui_elements(void);
void activate_rec_ui_elements(void);
void deactivate_rec_ui_elements(void);
void lang_id_to_str(int lang_id, char **lang_str, int mapping_type);
int lang_str_to_id(char *lang_str);
int eval_record_path(int use_previous_index);
void read_eq_slider_values(void);
int get_bitrate_list_for_codec(int codec, int **bitrates);
void update_stream_bitrate_list(int codec);
void update_rec_bitrate_list(int codec);
int get_bitrate_index(int codec, int bitrate);
int get_codec_index(char *codec);
int get_midi_ctrl_type(int midi_command);

typedef const char *(*currentTrackFunction)(int);
extern "C" currentTrackFunction getCurrentTrackFunctionFromId(int i);
int exec_midi_command(int command_id, int value);
void populate_midi_dev_list(void);

#endif

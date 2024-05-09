// FLTK callback functions for butt
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

#ifndef FL_CALLBACKS_H
#define FL_CALLBACKS_H

enum {
    STREAM_TIME = 0,
    REC_TIME = 1,
    SENT_DATA = 2,
    REC_DATA = 3,
};

enum {
    STREAM = 0,
    RECORD = 1,
};

enum {
    CB_CALLED_BY_USER = 0,
    CB_CALLED_BY_CODE = 1,
    CB_CALLED_BY_MIDI = 2,
};

class flgui;

extern int display_info;
extern flgui *fl_g;

void show_vu_tabs(void);
void hide_vu_tabs(void);

void button_cfg_cb(void);
void button_mixer_cb(void);
void button_info_cb(void);
void button_record_cb(bool ask);
void button_connect_cb(void);
void button_disconnect_cb(bool ask);
void button_add_icy_add_cb(void);
void button_cfg_del_srv_cb(void);
void button_cfg_del_icy_cb(void);
void choice_cfg_act_srv_cb(void);
void choice_cfg_act_icy_cb(void);
void button_cfg_add_srv_cb(void);
void button_cfg_add_icy_cb(void);
void choice_cfg_bitrate_cb(void);
void choice_cfg_samplerate_cb(void);
void button_cfg_song_go_cb(void);
void choice_cfg_codec_mp3_cb(void);
void choice_cfg_codec_ogg_cb(void);
void choice_cfg_codec_opus_cb(void);
void choice_cfg_codec_aac_cb(void);
void choice_cfg_codec_flac_cb(void);
void button_cfg_export_cb(void);
void button_cfg_import_cb(void);
void button_add_icy_save_cb(void);
void button_add_srv_cancel_cb(void);
void button_add_icy_cancel_cb(void);
void choice_cfg_channel_stereo_cb(void);
void choice_cfg_channel_mono_cb(void);
void button_cfg_browse_songfile_cb(void);
void input_cfg_song_file_cb(void);
void input_cfg_song_cb(void);
void input_cfg_song_prefix_cb(void);
void input_cfg_song_suffix_cb(void);
void input_cfg_buffer_cb(bool print_message);
void input_cfg_signal_cb(void);
void input_cfg_silence_cb(void);
void check_stream_signal_cb(void);
void check_stream_silence_cb(void);
void input_cfg_reconnect_delay_cb(void);
void input_cfg_present_level_cb(void);
void input_cfg_absent_level_cb(void);
void choice_cfg_resample_mode_cb(void);
void button_cfg_check_for_updates_cb(void);
void choice_cfg_dev_cb(void);
void choice_cfg_dev2_cb(void);
void button_cfg_rescan_devices_cb(void);
void radio_cfg_ID_cb(void);
void radio_cfg_name_cb(void);
void choice_cfg_right_channel_cb(void);
void choice_cfg_left_channel_cb(void);
void choice_cfg_right_channel2_cb(void);
void choice_cfg_left_channel2_cb(void);

void button_add_srv_add_cb(void);
void button_add_srv_save_cb(void);
void button_add_srv_show_pwd_cb(void);
void radio_add_srv_shoutcast_cb(void);
void radio_add_srv_icecast_cb(void);
void radio_add_srv_radioco_cb(void);
void button_add_srv_revoke_cert_cb(void);
void button_add_srv_get_stations_cb(void);
void button_add_srv_select_all_cb(void);
void button_add_srv_deselect_all_cb(void);

void button_start_agent_cb(void);
void button_stop_agent_cb(void);
void check_update_at_startup_cb(void);
void check_start_agent_cb(void);
void check_minimize_to_tray_cb(void);

void button_stream_codec_settings_cb(void);

void button_rec_browse_cb(void);
void button_rec_split_now_cb(void);
void choice_rec_bitrate_cb(void);
void choice_rec_samplerate_cb(void);
void choice_rec_channel_stereo_cb(void);
void choice_rec_channel_mono_cb(void);
void choice_rec_codec_mp3_cb(void);
void choice_rec_codec_ogg_cb(void);
void choice_rec_codec_wav_cb(void);
void choice_rec_codec_opus_cb(void);
void choice_rec_codec_aac_cb(void);
void choice_rec_codec_flac_cb(void);
void button_rec_codec_settings_cb(void);
void input_rec_signal_cb(void);
void input_rec_silence_cb(void);
void check_rec_signal_cb(void);
void check_rec_silence_cb(void);

void input_tls_cert_file_cb(void);
void input_tls_cert_dir_cb(void);
void button_tls_browse_file_cb(void);
void button_tls_browse_dir_cb(void);

void button_cfg_edit_srv_cb(void);
void button_cfg_edit_icy_cb(void);
void check_song_update_active_cb(void);
void check_read_last_line_cb(void);
void check_sync_to_full_hour_cb(void);

void input_rec_filename_cb(void);
void input_rec_folder_cb(void);
void input_rec_split_time_cb(void);
void input_log_filename_cb(void);
void button_cfg_log_browse_cb(void);

void check_gui_attach_cb(void);
void check_gui_ontop_cb(void);
void check_gui_hide_log_window_cb(void);
void check_gui_remember_pos_cb(void);
void check_gui_lcd_auto_cb(void);
void check_gui_start_minimized_cb(void);
void check_gui_disable_gain_slider_cb(void);
void check_gui_show_listeners_cb(void);
void button_gui_bg_color_cb(void);
void button_gui_text_color_cb(void);
void choice_gui_language_cb(void);
void radio_gui_vu_gradient_cb(void);
void radio_gui_vu_solid_cb(void);
void check_gui_always_show_vu_tabs_cb(void);
void input_gui_window_title_cb(void);

void slider_gain_cb(void *called_by);
void vu_tabs_cb(void);

void check_stream_eq_cb(void);
void check_rec_eq_cb(void);
void button_eq_reset_cb(void);
void choice_eq_preset_cb(void);
void slider_equalizer1_cb(double);
void slider_equalizer2_cb(double);
void slider_equalizer3_cb(double);
void slider_equalizer4_cb(double);
void slider_equalizer5_cb(double);
void slider_equalizer6_cb(double);
void slider_equalizer7_cb(double);
void slider_equalizer8_cb(double);
void slider_equalizer9_cb(double);
void slider_equalizer10_cb(double);

void check_stream_drc_cb(void);
void check_rec_drc_cb(void);
void button_drc_reset_cb(void);
void check_aggressive_mode_cb(void);
void slider_threshold_cb(double);
void slider_ratio_cb(double);
void slider_attack_cb(double);
void slider_release_cb(double);
void slider_makeup_cb(double);

void slider_mixer_primary_device_cb(double val, void *called_by);
void slider_mixer_secondary_device_cb(double val, void *called_by);
void slider_mixer_streaming_gain_cb(double val, void *called_by);
void slider_mixer_recording_gain_cb(double val, void *called_by);
void slider_mixer_master_gain_cb(double val, void *called_by);
void slider_mixer_cross_fader_cb(double val, void *called_by);
void button_mixer_reset_cb(void);
void button_mixer_mute_primary_device_cb(void);
void button_mixer_mute_secondary_device_cb(void);

void check_cfg_auto_start_rec_cb(void);
void check_cfg_auto_stop_rec_cb(void);
void check_cfg_rec_after_launch_cb(void);

void check_cfg_rec_hourly_cb(void);
void check_cfg_connect_cb(void);
void check_cfg_force_reconnecting_cb(void);

void lcd_rotate(void *);
void ILM216_cb(void);
void window_main_close_cb(bool ask);

void check_cfg_use_app_cb(void);
void choice_cfg_app_cb(void);
void radio_cfg_artist_title_cb(void);
void radio_cfg_title_artist_cb(void);

void choice_cfg_song_delay_cb(void);

void update_song(void *user_data);
bool stop_recording(bool ask);

void choice_stream_mp3_enc_quality_cb(void);
void choice_stream_mp3_stereo_mode_cb(void);
void choice_stream_mp3_bitrate_mode_cb(void);
void choice_stream_mp3_vbr_quality_cb(void);
void choice_stream_mp3_vbr_min_bitrate_cb(void);
void choice_stream_mp3_vbr_max_bitrate_cb(void);

void choice_stream_vorbis_bitrate_mode_cb(void);
void choice_stream_vorbis_vbr_quality_cb(void);
void choice_stream_vorbis_vbr_min_bitrate_cb(void);
void choice_stream_vorbis_vbr_max_bitrate_cb(void);

void choice_stream_opus_audio_type_cb(void);
void choice_stream_opus_bitrate_mode_cb(void);
void choice_stream_opus_quality_cb(void);
void choice_stream_opus_bandwidth_cb(void);

void choice_stream_aac_profile_cb(void);
void choice_stream_aac_afterburner_cb(void);
void choice_stream_aac_bitrate_mode_cb(void);

void radio_stream_flac_bit_depth_16_cb(void);
void radio_stream_flac_bit_depth_24_cb(void);

void choice_rec_mp3_enc_quality_cb(void);
void choice_rec_mp3_stereo_mode_cb(void);
void choice_rec_mp3_bitrate_mode_cb(void);
void choice_rec_mp3_vbr_quality_cb(void);
void choice_rec_mp3_vbr_min_bitrate_cb(void);
void choice_rec_mp3_vbr_max_bitrate_cb(void);

void choice_rec_vorbis_bitrate_mode_cb(void);
void choice_rec_vorbis_vbr_quality_cb(void);
void choice_rec_vorbis_vbr_min_bitrate_cb(void);
void choice_rec_vorbis_vbr_max_bitrate_cb(void);

void choice_rec_opus_audio_type_cb(void);
void choice_rec_opus_bitrate_mode_cb(void);
void choice_rec_opus_quality_cb(void);
void choice_rec_opus_bandwidth_cb(void);

void choice_rec_aac_profile_cb(void);
void choice_rec_aac_afterburner_cb(void);
void choice_rec_aac_bitrate_mode_cb(void);

void radio_rec_flac_bit_depth_16_cb(void);
void radio_rec_flac_bit_depth_24_cb(void);

void radio_rec_wav_bit_depth_16_cb(void);
void radio_rec_wav_bit_depth_24_cb(void);
void radio_rec_wav_bit_depth_32_cb(void);

void choice_midi_dev_cb(void);
void choice_midi_cc_mode_cb(void);
void check_midi_soft_takeover_cb(void);
void button_midi_rescan_devices_cb(void);
void browser_midi_command_cb(void);
void input_midi_msg_num_cb(void);
void choice_midi_channel_cb(void);
void button_midi_learn_cb(void);
void check_midi_command_enable_cb(void);

#endif

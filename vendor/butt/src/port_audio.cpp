// portaudio functions for butt
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#include <string.h>
#include <pthread.h>
#include <samplerate.h>

#ifdef WIN32
#include <windows.h>
#endif

#include "gettext.h"
#include "config.h"

#include "butt.h"
#include "cfg.h"
#include "port_audio.h"
#include "parseconfig.h"
#include "lame_encode.h"
#include "aac_encode.h"
#include "shoutcast.h"
#include "icecast.h"
#include "strfuncs.h"
#include "wav_header.h"
#include "ringbuffer.h"
#include "vu_meter.h"
#include "flgui.h"
#include "fl_funcs.h"
#include "Fl_LED.h"
#include "dsp.hpp"
#include "util.h"
#include "atom.h"

#define TEST_RESAMPLING 0

int pa_frames;

char *encode_buf;
float *pa_pcm_buf;
float *pa_pcm_buf2;
float *pa_mixer_buf;
float *pa_mixer_buf2;
float *stream_buf;
float *record_buf;
int framepacket_size;
int framepacket_size2;
int samplerate_dev2;
int enc_buf_size;

bool try_to_connect;
bool pa_new_frames;
bool reconnect;
bool silence_detected;
bool signal_detected;
bool reset_streaming_compressor = false;
bool reset_recording_compressor = false;

int num_of_input_channels;
int num_of_input_channels2;

int vu_level_type = 0; // 0 = Streaming, 1 = Recording

bool next_file;
FILE *next_fd;

ringbuf_t rec_rb;
ringbuf_t stream_rb;
ringbuf_t pa_pcm_rb;
ringbuf_t pa_pcm2_rb;

SRC_STATE *srconv_state_opus_stream = NULL;
SRC_STATE *srconv_state_opus_record = NULL;
SRC_STATE *srconv_state_dev2 = NULL;
SRC_DATA srconv_opus_stream;
SRC_DATA srconv_opus_record;
SRC_DATA srconv_dev2;

ATOM_NEW_COND(stream_cond);
ATOM_NEW_COND(rec_cond);

ATOM_NEW_INT(close_mixer_thread);

pthread_t rec_thread_detached;
pthread_t stream_thread_detached;
pthread_t mixer_thread_joinable;

PaStream *stream;
PaStream *stream2;

DSPEffects *streaming_dsp = NULL;
DSPEffects *recording_dsp = NULL;

void snd_reset_streaming_compressor(void)
{
    reset_streaming_compressor = true;
}

void snd_reset_recording_compressor(void)
{
    reset_recording_compressor = true;
}

void snd_set_vu_level_type(int type)
{
    vu_level_type = type;
    snd_update_vu(1);
}

int snd_init(void)
{
    char info_buf[256];

    PaError p_err;
    if ((p_err = Pa_Initialize()) != paNoError) {
        snprintf(info_buf, sizeof(info_buf), _("PortAudio init failed:\n%s\n"), Pa_GetErrorText(p_err));

        fl_alert("%s", info_buf);
        return 1;
    }

    reconnect = false;
    silence_detected = false;
    return 0;
}

int snd_reopen_streams(void)
{
    snd_close_streams();
    if ((snd_open_streams() != 0)) {
        return 1;
    }
    return 0;
}

void snd_init_dsp(void)
{
    if (streaming_dsp != NULL) {
        delete streaming_dsp;
        streaming_dsp = NULL;
    }

    if (recording_dsp != NULL) {
        delete recording_dsp;
        recording_dsp = NULL;
    }

    streaming_dsp = new DSPEffects(pa_frames, cfg.audio.channel, cfg.audio.samplerate);
    recording_dsp = new DSPEffects(pa_frames, cfg.audio.channel, cfg.audio.samplerate);
}

int snd_open_streams(void)
{
    int ret = 0;
    int flag;
    char info_buf[256];

    PaDeviceIndex pa_dev_id;
    PaStreamParameters pa_params;
    PaStreamParameters pa_params2;
    PaError pa_err;
    const PaDeviceInfo *pa_dev_info;

    if (cfg.audio.dev_count == 0) {
        print_info(_("ERROR: no sound device with input channels found"), 1);
        return 1;
    }

    pa_frames = round((cfg.audio.buffer_ms / 1000.0) * cfg.audio.samplerate);

    if (cfg.audio.dev_remember == REMEMBER_BY_NAME) {
        cfg.audio.dev_num = snd_get_dev_num_by_name(cfg.audio.dev_name);
    }

    if (cfg.audio.dev_num == -1) {
        cfg.audio.dev_num = 0;
    }

    pa_dev_id = cfg.audio.pcm_list[cfg.audio.dev_num]->dev_id;

    pa_dev_info = Pa_GetDeviceInfo(pa_dev_id);
    if (pa_dev_info == NULL) {
        snprintf(info_buf, sizeof(info_buf), _("Error getting device Info (%d)"), pa_dev_id);
        print_info(info_buf, 1);
        return 1;
    }

    num_of_input_channels = pa_dev_info->maxInputChannels;
    if (num_of_input_channels == 1) {
        cfg.audio.left_ch = 1;
        cfg.audio.right_ch = 1;
    }
    else if ((cfg.audio.left_ch > num_of_input_channels) || (cfg.audio.right_ch > num_of_input_channels)) {
        cfg.audio.left_ch = 1;
        cfg.audio.right_ch = 2;
    }

    framepacket_size = pa_frames * cfg.audio.channel;

    pa_pcm_buf = (float *)malloc(2 * framepacket_size * sizeof(float));
    pa_mixer_buf = (float *)malloc(2 * framepacket_size * sizeof(float));
    stream_buf = (float *)malloc(2 * framepacket_size * sizeof(float));
    record_buf = (float *)malloc(2 * framepacket_size * sizeof(float));
    encode_buf = (char *)malloc(2 * framepacket_size * sizeof(char));

    srconv_opus_stream.data_in = stream_buf;
    srconv_opus_record.data_in = record_buf;
    srconv_opus_stream.data_out = (float *)malloc(32 * framepacket_size * sizeof(float));
    srconv_opus_record.data_out = (float *)malloc(32 * framepacket_size * sizeof(float));
    srconv_dev2.data_out = (float *)malloc(32 * framepacket_size * sizeof(float));

    rb_init(&rec_rb, 16 * framepacket_size * sizeof(float));
    rb_init(&stream_rb, 16 * framepacket_size * sizeof(float));
    rb_init(&pa_pcm_rb, 16 * framepacket_size * sizeof(float));

    enc_buf_size = stream_rb.size * 10;

    pa_params.device = pa_dev_id;
    pa_params.channelCount = pa_dev_info->maxInputChannels;
    pa_params.sampleFormat = paFloat32;
    pa_params.suggestedLatency = pa_dev_info->defaultHighInputLatency;
    pa_params.hostApiSpecificStreamInfo = NULL;

    pa_err = Pa_IsFormatSupported(&pa_params, NULL, cfg.audio.samplerate);
    if (pa_err != paFormatIsSupported) {
        if (pa_err == paInvalidSampleRate) {
            snprintf(info_buf, sizeof(info_buf),
                     _("Samplerate not supported: %dHz\n"
                       "Using default samplerate: %dHz"),
                     cfg.audio.samplerate, (int)pa_dev_info->defaultSampleRate);
            print_info(info_buf, 1);

            if (Pa_IsFormatSupported(&pa_params, NULL, pa_dev_info->defaultSampleRate) != paFormatIsSupported) {
                print_info("FAILED", 1);
                ret = 1;
                goto cleanup1;
            }
            else {
                cfg.audio.samplerate = (int)pa_dev_info->defaultSampleRate;
                update_samplerates_list();
                update_codec_samplerates();
            }
        }
        else {
            snprintf(info_buf, sizeof(info_buf), _("PA: Format not supported: %s\n"), Pa_GetErrorText(pa_err));
            print_info(info_buf, 1);
            ret = 1;
            goto cleanup1;
        }
    }

    flag = cfg.audio.disable_dithering == 0 ? paNoFlag : paDitherOff;
    pa_err = Pa_OpenStream(&stream, &pa_params, NULL, cfg.audio.samplerate, pa_frames, flag, snd_callback, NULL);
    if (pa_err != paNoError) {
        snprintf(info_buf, sizeof(info_buf), _("error opening sound device: %s"), Pa_GetErrorText(pa_err));
        print_info(info_buf, 1);
        ret = 1;
        goto cleanup1;
    }

    // Secondary device
    if (cfg.audio.dev2_num >= 0) {
        int frames_in_dev2;
        if (cfg.audio.dev_remember == REMEMBER_BY_NAME) {
            cfg.audio.dev2_num = snd_get_dev_num_by_name(cfg.audio.dev2_name);
        }

        if (cfg.audio.dev2_num == -1) {
            cfg.audio.dev2_num = 0;
        }

        pa_dev_id = cfg.audio.pcm_list[cfg.audio.dev2_num]->dev_id;

        pa_dev_info = Pa_GetDeviceInfo(pa_dev_id);
        if (pa_dev_info == NULL) {
            snprintf(info_buf, sizeof(info_buf), _("Error getting device Info (%d)"), pa_dev_id);
            print_info(info_buf, 1);
            ret = 1;
            goto cleanup1;
        }

        num_of_input_channels2 = pa_dev_info->maxInputChannels;
        if (num_of_input_channels2 == 1) {
            cfg.audio.left_ch2 = 1;
            cfg.audio.right_ch2 = 1;
        }
        else if ((cfg.audio.left_ch2 > num_of_input_channels2) || (cfg.audio.right_ch2 > num_of_input_channels2)) {
            cfg.audio.left_ch2 = 1;
            cfg.audio.right_ch2 = 2;
        }

        pa_params2.device = pa_dev_id;
        pa_params2.channelCount = num_of_input_channels2;
        pa_params2.sampleFormat = paFloat32;
        pa_params2.suggestedLatency = pa_dev_info->defaultHighInputLatency;
        pa_params2.hostApiSpecificStreamInfo = NULL;

        pa_err = Pa_IsFormatSupported(&pa_params2, NULL, cfg.audio.samplerate);
#if TEST_RESAMPLING == 1
        if (pa_err == paFormatIsSupported) {
            if (pa_err != paInvalidSampleRate) {
#else
        if (pa_err != paFormatIsSupported) {
            if (pa_err == paInvalidSampleRate) {
#endif

#if TEST_RESAMPLING == 1
                if (Pa_IsFormatSupported(&pa_params2, NULL, 44100) != paFormatIsSupported) {
#else
                // Use default sample rate of secondary audio device and resample to cfg.audio.samplerate
                if (Pa_IsFormatSupported(&pa_params2, NULL, pa_dev_info->defaultSampleRate) != paFormatIsSupported) {
#endif
                    print_info(_("The selected secondary audio device can not be used"), 1);
                    ret = 1;
                    goto cleanup1;
                }
                else {
#if TEST_RESAMPLING == 1
                    samplerate_dev2 = 44100;
#else
                    samplerate_dev2 = (int)pa_dev_info->defaultSampleRate;
#endif
                    if (srconv_state_dev2 != NULL) {
                        src_delete(srconv_state_dev2);
                        srconv_state_dev2 = NULL;
                    }

                    srconv_state_dev2 = src_new(cfg.audio.resample_mode, cfg.audio.channel, &pa_err);
                    if (srconv_state_dev2 == NULL) {
                        print_info(_("ERROR: Could not initialize samplerate converter"), 0);
                        ret = 1;
                        goto cleanup1;
                    }

                    frames_in_dev2 = (cfg.audio.buffer_ms * samplerate_dev2) / 1000;
                    srconv_dev2.src_ratio = (float)cfg.audio.samplerate / samplerate_dev2;
                    srconv_dev2.input_frames = frames_in_dev2;
                    srconv_dev2.output_frames = pa_frames;
                    srconv_dev2.end_of_input = 0;

                    snprintf(info_buf, sizeof(info_buf), _("Samplerate of secondary device is resampled from %dHz to %dHz\n"), samplerate_dev2,
                             cfg.audio.samplerate);
                    print_info(info_buf, 1);
                }
            }
            else {
                snprintf(info_buf, sizeof(info_buf), _("PA: Format not supported: %s\n"), Pa_GetErrorText(pa_err));
                print_info(info_buf, 1);
                ret = 1;
                goto cleanup1;
            }
        }
        else {
            frames_in_dev2 = pa_frames;
            samplerate_dev2 = cfg.audio.samplerate;
        }

        framepacket_size2 = frames_in_dev2 * num_of_input_channels2;
        pa_pcm_buf2 = (float *)malloc(2 * framepacket_size2 * sizeof(float));
        pa_mixer_buf2 = (float *)malloc(2 * framepacket_size2 * sizeof(float));
        rb_init(&pa_pcm2_rb, 16 * framepacket_size2 * sizeof(float));
        srconv_dev2.data_in = pa_pcm_buf2;

        int flag = cfg.audio.disable_dithering == 0 ? paNoFlag : paDitherOff;
        pa_err = Pa_OpenStream(&stream2, &pa_params2, NULL, samplerate_dev2, frames_in_dev2, flag, snd_callback2, NULL);

        if (pa_err != paNoError) {
            snprintf(info_buf, sizeof(info_buf), _("error opening secondary sound device: %s"), Pa_GetErrorText(pa_err));
            print_info(info_buf, 1);
            ret = 1;
            goto cleanup2;
        }

        Pa_StartStream(stream2);
    }

    Pa_StartStream(stream);

    snd_init_dsp();
    snd_start_mixer_thread();

    g_vu_meter_timer_is_active = 1;
    Fl::add_timeout(0.01, &vu_meter_timer);

    return 0;

cleanup2:
    free(pa_pcm_buf2);
    free(pa_mixer_buf2);
    rb_free(&pa_pcm2_rb);

cleanup1:
    if (Pa_IsStreamStopped(&stream)) { // Primary stream has been opened but not started yet
        Pa_CloseStream(&stream);
    }
    free(pa_pcm_buf);
    free(pa_mixer_buf);
    free(stream_buf);
    free(record_buf);
    free(encode_buf);
    free(srconv_opus_stream.data_out);
    free(srconv_opus_record.data_out);
    free(srconv_dev2.data_out);
    rb_free(&rec_rb);
    rb_free(&stream_rb);
    rb_free(&pa_pcm_rb);

    return ret;
}

// this function is called by PortAudio when new audio data arrived
int snd_callback(const void *input, void *output, unsigned long frameCount, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags,
                 void *userData)
{
    float *pcm_input = (float *)input;

    if (statusFlags != 0) {
        printf("1 status: %lu\n", statusFlags);
    }

    if (cfg.audio.channel == 1) {             // User has selected mono
        for (uint32_t i = 0; i < frameCount; i++) {
            if (num_of_input_channels == 1) { // If the device has only one channel use that channel as mono input source
                pa_pcm_buf[i] = pcm_input[i];
            }
            else { // If the device has more than one channel, average the user selected left and right channel into a mono channel
                float left_sample, right_sample;
                float mono_sample;
                left_sample = pcm_input[num_of_input_channels * i + (cfg.audio.left_ch - 1)];
                right_sample = pcm_input[num_of_input_channels * i + (cfg.audio.right_ch - 1)];
                mono_sample = (left_sample + right_sample) / 2.0;
                pa_pcm_buf[i] = mono_sample;
            }
        }
        rb_write(&pa_pcm_rb, (char *)pa_pcm_buf, (int)(1 * frameCount * sizeof(float)));
    }
    else {                                    // User has selected stereo
        for (uint32_t i = 0; i < frameCount; i++) {
            if (num_of_input_channels == 1) { // If the device has only one channel, use the same channel for left and right
                pa_pcm_buf[2 * i] = pcm_input[i];
                pa_pcm_buf[2 * i + 1] = pcm_input[i];
            }
            else { // If the device has more than one channel, use the selected left and right channel as input source
                pa_pcm_buf[2 * i] = pcm_input[num_of_input_channels * i + (cfg.audio.left_ch - 1)];
                pa_pcm_buf[2 * i + 1] = pcm_input[num_of_input_channels * i + (cfg.audio.right_ch - 1)];
            }
        }
        if (rb_write(&pa_pcm_rb, (char *)pa_pcm_buf, (int)(2 * frameCount * sizeof(float))) != 0) {
            printf("Write to pa_pcm_rb failed\n");
        }
    }

    /*
    samplerate_out = cfg.audio.samplerate;

    if (dsp->hasToProcessSamples()) {
        if (reset_compressor == true) {
            dsp->reset_compressor();
            reset_compressor = false;
        }

        dsp->processSamples(pa_pcm_buf);
    }

    if (streaming) {
        if ((!strcmp(cfg.audio.codec, "opus")) && (cfg.audio.samplerate != 48000)) {
            convert_stream = true;
            samplerate_out = 48000;
        }

        if (convert_stream == true) {
            srconv_stream.end_of_input = 0;
            srconv_stream.src_ratio = (float)samplerate_out / cfg.audio.samplerate;
            srconv_stream.input_frames = frameCount;
            srconv_stream.output_frames = frameCount * cfg.audio.channel * (srconv_stream.src_ratio + 1) * sizeof(float);

            memcpy((float *)srconv_stream.data_in, pa_pcm_buf, frameCount * cfg.audio.channel * sizeof(float));

            // The actual resample process
            src_process(srconv_state_stream, &srconv_stream);

            memcpy(stream_buf, srconv_stream.data_out, srconv_stream.output_frames_gen * cfg.audio.channel * sizeof(float));

            rb_write(&stream_rb, (char *)stream_buf, srconv_stream.output_frames_gen * cfg.audio.channel * sizeof(float));
        }
        else {
            rb_write(&stream_rb, (char *)pa_pcm_buf, frameCount * cfg.audio.channel * sizeof(float));
        }

        atom_cond_signal(&stream_cond);
    }

    if (recording) {
        if ((!strcmp(cfg.rec.codec, "opus")) && (cfg.audio.samplerate != 48000)) {
            convert_record = true;
            samplerate_out = 48000;
        }

        if (convert_record == true) {
            srconv_record.end_of_input = 0;
            srconv_record.src_ratio = (float)samplerate_out / cfg.audio.samplerate;
            srconv_record.input_frames = frameCount;
            srconv_record.output_frames = frameCount * cfg.audio.channel * (srconv_record.src_ratio + 1) * sizeof(float);

            memcpy((float *)srconv_record.data_in, pa_pcm_buf, frameCount * cfg.audio.channel * sizeof(float));

            // The actual resample process
            src_process(srconv_state_record, &srconv_record);

            memcpy(record_buf, srconv_record.data_out, srconv_record.output_frames_gen * cfg.audio.channel * sizeof(float));

            rb_write(&rec_rb, (char *)record_buf, srconv_record.output_frames_gen * cfg.audio.channel * sizeof(float));
        }
        else {
            rb_write(&rec_rb, (char *)pa_pcm_buf, frameCount * cfg.audio.channel * sizeof(float));
        }

        atom_cond_signal(&rec_cond);
    }

    // tell snd_update_vu() that there is new audio data
    pa_new_frames = 1;
*/

    return paContinue;
}

int snd_callback2(const void *input, void *output, unsigned long frameCount, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags,
                  void *userData)
{
    float *pcm_input = (float *)input;
    if (statusFlags != 0) {
        printf("2 status: %lu\n", statusFlags);
    }

    if (cfg.audio.channel == 1) {              // User has selected mono
        for (uint32_t i = 0; i < frameCount; i++) {
            if (num_of_input_channels2 == 1) { // If the device has only one channel use that channel as mono input source
                pa_pcm_buf2[i] = pcm_input[i];
            }
            else { // If the device has more than one channel, average the user selected left and right channel into a mono channel
                float left_sample, right_sample;
                float mono_sample;
                left_sample = pcm_input[num_of_input_channels2 * i + (cfg.audio.left_ch2 - 1)];
                right_sample = pcm_input[num_of_input_channels2 * i + (cfg.audio.right_ch2 - 1)];
                mono_sample = (left_sample + right_sample) / 2.0;
                pa_pcm_buf2[i] = mono_sample;
            }
        }
    }
    else {                                     // User has selected stereo
        for (uint32_t i = 0; i < frameCount; i++) {
            if (num_of_input_channels2 == 1) { // If the device has only one channel, use the same channel for left and right
                pa_pcm_buf2[2 * i] = pcm_input[i];
                pa_pcm_buf2[2 * i + 1] = pcm_input[i];
            }
            else { // If the device has more than one channel, use the selected left and right channel as input source
                pa_pcm_buf2[2 * i] = pcm_input[num_of_input_channels2 * i + (cfg.audio.left_ch2 - 1)];
                pa_pcm_buf2[2 * i + 1] = pcm_input[num_of_input_channels2 * i + (cfg.audio.right_ch2 - 1)];
            }
        }
    }

    if (samplerate_dev2 != cfg.audio.samplerate) {
        // srconv_dev2.input_frames is equal to frameCount
        // srconv_dev2.output_frames_gen is equal to pa_frames
        src_process(srconv_state_dev2, &srconv_dev2);
        memcpy(pa_pcm_buf2, srconv_dev2.data_out, srconv_dev2.output_frames_gen * cfg.audio.channel * sizeof(float));
    }

    if (rb_write(&pa_pcm2_rb, (char *)pa_pcm_buf2, pa_frames * cfg.audio.channel * sizeof(float)) != 0) {
        printf("Write to pa_pcm_rb2 failed\n");
    }

    return paContinue;
}

void snd_start_streaming_thread(void)
{
    atom_cond_init(&stream_cond);

    kbytes_sent = 0;
    streaming = 1;
    if (pthread_create(&stream_thread_detached, NULL, snd_stream_thread, NULL) != 0) {
        print_info("Fatal error: Could not launch streaming thread. Please restart BUTT", 1);
        streaming = 0;
        return;
    }
}

void snd_start_mixer_thread(void)
{
    atom_set_int(&close_mixer_thread, 0);
    rb_clear(&pa_pcm_rb);

    if (cfg.audio.dev2_num >= 0) {
        rb_clear(&pa_pcm2_rb);
    }

    snd_reset_samplerate_conv(SND_STREAM);
    snd_reset_samplerate_conv(SND_REC);

    if (pthread_create(&mixer_thread_joinable, NULL, snd_mixer_thread, NULL) != 0) {
        print_info("Fatal error: Could not launch mixer thread. Please restart BUTT", 1);
        return;
    }
}

void snd_stop_mixer_thread(void)
{
    atom_set_int(&close_mixer_thread, 1);
    pthread_join(mixer_thread_joinable, NULL);
}

void *snd_mixer_thread(void *data)
{
    int frame_size = pa_frames * cfg.audio.channel * sizeof(float);
    int frame_len = frame_size / sizeof(float);

    int filled1, filled2;

    for (;;) {
        if (cfg.audio.dev2_num < 0) { // Only primary audio device is active
            do {
                filled1 = rb_filled(&pa_pcm_rb);
                if (atom_get_int(&close_mixer_thread) == 1) {
                    break;
                }
                Pa_Sleep(1); // ms
            } while (filled1 < frame_size);

            if (atom_get_int(&close_mixer_thread) == 1) {
                break;
            }

            rb_read_len(&pa_pcm_rb, (char *)pa_mixer_buf, frame_size);

            // Apply gain to primary device
            for (int i = 0; i < frame_len; i++) {
                // Apply gain to primary device
                if (cfg.mixer.primary_device_muted == 1) {
                    pa_mixer_buf[i] = 0;
                }
                else if (cfg.mixer.primary_device_gain != 1) {
                    pa_mixer_buf[i] *= cfg.mixer.primary_device_gain;
                }

                if (cfg.mixer.cross_fader != 0) {
                    pa_mixer_buf[i] *= cfg.mixer.primary_X_fader;
                }
            }
        }
        else { // Secondary audio device is active as well
            do {
                filled1 = rb_filled(&pa_pcm_rb);
                filled2 = rb_filled(&pa_pcm2_rb);
                if (atom_get_int(&close_mixer_thread) == 1) {
                    break;
                }
                Pa_Sleep(1); // ms
            } while (filled1 < frame_size || filled2 < frame_size);

            if (atom_get_int(&close_mixer_thread) == 1) {
                break;
            }

            /* cnt++;
             if (cnt == 21) {
                 printf("filled1: %d\nfilled2: %d\n\n", filled1, filled2);
                 cnt = 0;
             }*/

            rb_read_len(&pa_pcm_rb, (char *)pa_mixer_buf, frame_size);
            rb_read_len(&pa_pcm2_rb, (char *)pa_mixer_buf2, frame_size);

            for (int i = 0; i < frame_len; i++) {
                // Apply gain to primary device
                if (cfg.mixer.primary_device_muted == 1) {
                    pa_mixer_buf[i] = 0;
                }
                else if (cfg.mixer.primary_device_gain != 1) {
                    pa_mixer_buf[i] *= cfg.mixer.primary_device_gain;
                }

                // Apply gain to secondary device
                if (cfg.mixer.secondary_device_muted == 1) {
                    pa_mixer_buf2[i] = 0;
                }
                else if (cfg.mixer.secondary_device_gain != 1) {
                    pa_mixer_buf2[i] *= cfg.mixer.secondary_device_gain;
                }

                if (cfg.mixer.cross_fader != 0) {
                    pa_mixer_buf[i] *= cfg.mixer.primary_X_fader;
                    pa_mixer_buf2[i] *= cfg.mixer.secondary_X_fader;
                }

                pa_mixer_buf[i] = (pa_mixer_buf[i] + pa_mixer_buf2[i]);
            }
        }

        // To my future self: Do not move this part into the "if (streaming)" block below
        // because the VU meter displays the contents of "stream_buf"
        memcpy(stream_buf, pa_mixer_buf, frame_size);
        if (cfg.mixer.streaming_gain != 1) {
            for (int i = 0; i < frame_len; i++) {
                stream_buf[i] *= cfg.mixer.streaming_gain;
            }
        }

        streaming_dsp->eq_active = cfg.dsp.equalizer_stream;
        streaming_dsp->drc_active = cfg.dsp.compressor_stream;
        if (streaming_dsp->hasToProcessSamples()) {
            // printf("stream: \n");
            if (reset_streaming_compressor == true) {
                streaming_dsp->reset_compressor();
                reset_streaming_compressor = false;
            }
            streaming_dsp->processSamples(stream_buf);
        }
        if (streaming) {
            if ((!strcmp(cfg.audio.codec, "opus")) && (cfg.audio.samplerate != 48000)) {
                srconv_opus_stream.data_in = stream_buf;
                src_process(srconv_state_opus_stream, &srconv_opus_stream);
                rb_write(&stream_rb, (char *)srconv_opus_stream.data_out, (int)(srconv_opus_stream.output_frames_gen * cfg.audio.channel * sizeof(float)));
            }
            else {
                rb_write(&stream_rb, (char *)stream_buf, frame_size);
            }
            atom_cond_signal(&stream_cond);
        }

        memcpy(record_buf, pa_mixer_buf, frame_size);
        if (cfg.mixer.recording_gain != 1) {
            for (int i = 0; i < frame_len; i++) {
                record_buf[i] *= cfg.mixer.recording_gain;
            }
        }

        recording_dsp->eq_active = cfg.dsp.equalizer_rec;
        recording_dsp->drc_active = cfg.dsp.compressor_rec;
        if (recording_dsp->hasToProcessSamples()) {
            // printf("rec: \n");
            if (reset_recording_compressor == true) {
                recording_dsp->reset_compressor();
                reset_recording_compressor = false;
            }
            recording_dsp->processSamples(record_buf);
        }

        if (recording) {
            if ((!strcmp(cfg.rec.codec, "opus")) && (cfg.audio.samplerate != 48000)) {
                src_process(srconv_state_opus_record, &srconv_opus_record);
                rb_write(&rec_rb, (char *)srconv_opus_record.data_out, (int)(srconv_opus_record.output_frames_gen * cfg.audio.channel * sizeof(float)));
            }
            else {
                rb_write(&rec_rb, (char *)record_buf, frame_size);
            }
            atom_cond_signal(&rec_cond);
        }

        pa_new_frames = 1;
    }

    return NULL;
}

void *snd_stream_thread(void *data)
{
    int sent;
    int rb_bytes_read;
    int encode_bytes_read = 0;
    int bytes_to_read;

    char *enc_buf = (char *)malloc(enc_buf_size);
    char *audio_buf = (char *)malloc(enc_buf_size);

    int (*xc_send)(char *buf, int buf_len) = NULL;

    static int new_stream = 0;

    if (cfg.srv[cfg.selected_srv]->type == ICECAST) {
        xc_send = &ic_send;
    }
    else { // Icecast
        xc_send = &sc_send;
    }

    set_max_thread_priority();

    while (connected) {
        atom_cond_wait(&stream_cond);
        if (!connected) {
            break;
        }

        if (!strcmp(cfg.audio.codec, "opus")) {
            // Read always chunks of OPUS_FRAME_SIZE frames from the audio ringbuffer to be
            // compatible with OPUS
            bytes_to_read = OPUS_FRAME_SIZE * cfg.audio.channel * sizeof(float);

            while ((rb_filled(&stream_rb)) >= bytes_to_read) {
                if (opus_stream.state == OPUS_STATE_NEW_SONG_AVAILABLE) {
                    opus_stream.state = OPUS_STATE_LAST_FRAME;
                }

                if (opus_stream.state == OPUS_STATE_NEW_STREAM) {
                    opus_enc_reinit(&opus_stream);
                    opus_enc_write_header(&opus_stream);
                }

                rb_read_len(&stream_rb, audio_buf, bytes_to_read);
                encode_bytes_read = opus_enc_encode(&opus_stream, (float *)audio_buf, enc_buf, bytes_to_read / (cfg.audio.channel * sizeof(float)));

                if ((sent = xc_send(enc_buf, encode_bytes_read)) == -1) {
                    connected = 0;
                }
                else {
                    kbytes_sent += encode_bytes_read / 1024.0;
                }
            }
        }
#ifdef HAVE_LIBFDK_AAC
        else if (!strcmp(cfg.audio.codec, "aac")) {
            bytes_to_read = aac_stream.info.frameLength * cfg.audio.channel * sizeof(float);
            while ((rb_filled(&stream_rb)) >= bytes_to_read) {
                rb_read_len(&stream_rb, audio_buf, bytes_to_read);
                encode_bytes_read =
                    aac_enc_encode(&aac_stream, (float *)audio_buf, enc_buf, bytes_to_read / (cfg.audio.channel * sizeof(float)), stream_rb.size * 10);

                if ((sent = xc_send(enc_buf, encode_bytes_read)) == -1) {
                    connected = 0;
                }
                else {
                    kbytes_sent += encode_bytes_read / 1024.0;
                }
            }
        }
#endif
        else // ogg, mp3 and flac need more data than opus in order to compress the audio data
        {
            if (rb_filled(&stream_rb) < (int)(framepacket_size * sizeof(float))) {
                continue;
            }

            rb_bytes_read = rb_read(&stream_rb, audio_buf);
            if (rb_bytes_read == 0) {
                continue;
            }

            if (!strcmp(cfg.audio.codec, "mp3")) {
                encode_bytes_read =
                    lame_enc_encode(&lame_stream, (float *)audio_buf, enc_buf, rb_bytes_read / (cfg.audio.channel * sizeof(float)), stream_rb.size * 10);
            }

            if (!strcmp(cfg.audio.codec, "ogg")) {
                encode_bytes_read = vorbis_enc_encode(&vorbis_stream, (float *)audio_buf, enc_buf, rb_bytes_read / (cfg.audio.channel * sizeof(float)));
            }

            if (!strcmp(cfg.audio.codec, "flac")) {
                encode_bytes_read = flac_enc_encode_stream(&flac_stream, (float *)audio_buf, (uint8_t *)enc_buf,
                                                           rb_bytes_read / (cfg.audio.channel * sizeof(float)), cfg.audio.channel, new_stream);

                if (flac_stream.state == FLAC_STATE_UPDATE_META_DATA) {
                    flac_enc_init_ogg_stream(&flac_stream);
                }

                if (encode_bytes_read == 0) {
                    continue;
                }
            }

            if ((sent = xc_send(enc_buf, encode_bytes_read)) == -1) {
                connected = 0;
            }
            else {
                kbytes_sent += encode_bytes_read / 1024.0;
            }
        }
    }

    free(enc_buf);
    free(audio_buf);

    // Detach thread (free ressources) because no one will call pthread_join() on it
    pthread_detach(pthread_self());

    return NULL;
}

void snd_stop_streaming_thread(void)
{
    connected = 0;
    streaming = 0;

    atom_cond_signal(&stream_cond);
    atom_cond_destroy(&stream_cond);

    print_info(_("disconnected\n"), 0);
}

void snd_start_recording_thread(void)
{
    next_file = 0;
    kbytes_written = 0;
    recording = 1;

    atom_cond_init(&rec_cond);

    if (pthread_create(&rec_thread_detached, NULL, snd_rec_thread, NULL) != 0) {
        print_info("Fatal error: Could not launch recording thread. Please restart BUTT", 1);
        recording = 0;
        return;
    }

    print_info(_("Recording to:"), 0);
    print_info(cfg.rec.path, 0);
}

void snd_stop_recording_thread(void)
{
    recording = 0;

    atom_cond_signal(&rec_cond);
    atom_cond_destroy(&rec_cond);

    print_info(_("recording stopped"), 0);
}

// The recording stuff runs in its own thread
// this prevents dropouts in the recording in case the
// bandwidth is smaller than the selected streaming bitrate
void *snd_rec_thread(void *data)
{
    int rb_bytes_read;
    int bytes_to_read;
    int ogg_header_written;
    int opus_header_written;
    int enc_bytes_read;

    char *enc_buf = (char *)malloc(rec_rb.size * sizeof(char) * 10);
    char *audio_buf = (char *)malloc(rec_rb.size * sizeof(char) * 10);

    ogg_header_written = 0;
    opus_header_written = 0;

    set_max_thread_priority();

    while (recording) {
        atom_cond_wait(&rec_cond);

        if (next_file == 1) {
            if (!strcmp(cfg.rec.codec, "flac")) { // The flac encoder closes the file
                flac_enc_close(&flac_rec);
            }
            else {
                fclose(cfg.rec.fd);
            }

            cfg.rec.fd = next_fd;
            next_file = 0;
            if (!strcmp(cfg.rec.codec, "ogg")) {
                vorbis_enc_reinit(&vorbis_rec);
                ogg_header_written = 0;
            }
            if (!strcmp(cfg.rec.codec, "opus")) {
                opus_enc_reinit(&opus_rec);
                opus_header_written = 0;
            }
            if (!strcmp(cfg.rec.codec, "flac")) {
                flac_enc_reinit(&flac_rec);
                flac_enc_init_FILE(&flac_rec, cfg.rec.fd);
            }
        }

        // Opus and aac need  special treatments
        // The encoders need a predefined number of frames
        // Therefore we don't feed the encoder with all data we have in the
        // ringbuffer at once
        if (!strcmp(cfg.rec.codec, "opus")) {
            bytes_to_read = OPUS_FRAME_SIZE * cfg.audio.channel * sizeof(float);
            while ((rb_filled(&rec_rb)) >= bytes_to_read) {
                rb_read_len(&rec_rb, audio_buf, bytes_to_read);

                if (!opus_header_written) {
                    opus_enc_write_header(&opus_rec);
                    opus_header_written = 1;
                }

                enc_bytes_read = opus_enc_encode(&opus_rec, (float *)audio_buf, enc_buf, bytes_to_read / (cfg.audio.channel * sizeof(float)));
                kbytes_written += fwrite(enc_buf, 1, enc_bytes_read, cfg.rec.fd) / 1024.0;
            }
        }
#ifdef HAVE_LIBFDK_AAC
        else if (!strcmp(cfg.rec.codec, "aac")) {
            bytes_to_read = aac_rec.info.frameLength * cfg.audio.channel * sizeof(float);
            while ((rb_filled(&rec_rb)) >= bytes_to_read) {
                rb_read_len(&rec_rb, audio_buf, bytes_to_read);

                enc_bytes_read =
                    aac_enc_encode(&aac_rec, (float *)audio_buf, enc_buf, bytes_to_read / (cfg.audio.channel * sizeof(float)), rec_rb.size * sizeof(char) * 10);
                kbytes_written += fwrite(enc_buf, 1, enc_bytes_read, cfg.rec.fd) / 1024.0;
            }
        }
#endif
        else {
            if (rb_filled(&rec_rb) < (int)(framepacket_size * sizeof(float))) {
                continue;
            }

            rb_bytes_read = rb_read(&rec_rb, audio_buf);
            if (rb_bytes_read == 0) {
                continue;
            }

            if (!strcmp(cfg.rec.codec, "mp3")) {
                enc_bytes_read = lame_enc_encode(&lame_rec, (float *)audio_buf, enc_buf, rb_bytes_read / (cfg.audio.channel * sizeof(float)), rec_rb.size * 10);
                kbytes_written += fwrite(enc_buf, 1, enc_bytes_read, cfg.rec.fd) / 1024.0;
            }

            if (!strcmp(cfg.rec.codec, "ogg")) {
                if (!ogg_header_written) {
                    vorbis_enc_write_header(&vorbis_rec);
                    ogg_header_written = 1;
                }

                enc_bytes_read = vorbis_enc_encode(&vorbis_rec, (float *)audio_buf, enc_buf, rb_bytes_read / (cfg.audio.channel * sizeof(float)));
                kbytes_written += fwrite(enc_buf, 1, enc_bytes_read, cfg.rec.fd) / 1024.0;
            }

            if (!strcmp(cfg.rec.codec, "flac")) {
                flac_enc_encode(&flac_rec, (float *)audio_buf, rb_bytes_read / (cfg.audio.channel * sizeof(float)), cfg.audio.channel);
                kbytes_written = flac_enc_get_bytes_written() / 1024.0;
            }

            if (!strcmp(cfg.rec.codec, "wav")) {
                // Permanently update the WAV header
                // so in case of a crash we still have a valid WAV file
                wav_write_header(cfg.rec.fd, cfg.audio.channel, cfg.audio.samplerate, cfg.wav_codec_rec.bit_depth);

                // Convert float to samples to int16 samples
                float *pcm_float = (float *)audio_buf;
                int16_t *pcm_int16 = (int16_t *)audio_buf;
                int32_t *pcm_int32 = (int32_t *)audio_buf;

                for (uint32_t i = 0; i < rb_bytes_read / sizeof(float); i++) {
                    if (pcm_float[i] > 0) {
                        if (cfg.wav_codec_rec.bit_depth == 16) {
                            pcm_int16[i] = (int16_t)round(fmin(pcm_float[i] * INT16_MAX, INT16_MAX));
                        }
                        else if (cfg.wav_codec_rec.bit_depth == 24) {
                            pcm_int32[i] = (int32_t)round(fmin(pcm_float[i] * INT24_MAX, INT24_MAX));

                            audio_buf[i * 3 + 0] = audio_buf[i * 4 + 0]; // lsb
                            audio_buf[i * 3 + 1] = audio_buf[i * 4 + 1]; // center byte
                            audio_buf[i * 3 + 2] = audio_buf[i * 4 + 2]; // msb
                        }
                        else {                                           // cfg.wav_codec_rec.bit_depth == 32
                            pcm_int32[i] = (int32_t)round(fmin(pcm_float[i] * INT32_MAX, INT32_MAX));
                        }
                    }
                    else {
                        if (cfg.wav_codec_rec.bit_depth == 16) {
                            pcm_int16[i] = (int16_t)round(fmax(-pcm_float[i] * INT16_MIN, INT16_MIN));
                        }
                        else if (cfg.wav_codec_rec.bit_depth == 24) {
                            pcm_int32[i] = (int32_t)round(fmax(-pcm_float[i] * INT24_MIN, INT24_MIN));

                            audio_buf[i * 3 + 0] = audio_buf[i * 4 + 0]; // lsb
                            audio_buf[i * 3 + 1] = audio_buf[i * 4 + 1]; // center byte
                            audio_buf[i * 3 + 2] = audio_buf[i * 4 + 2]; // msb
                        }
                        else {                                           // cfg.wav_codec_rec.bit_depth == 32
                            pcm_int32[i] = (int32_t)round(fmax(-pcm_float[i] * INT32_MIN, INT32_MIN));
                        }
                    }
                }

                kbytes_written += fwrite(audio_buf, cfg.wav_codec_rec.bit_depth / 8, rb_bytes_read / sizeof(float), cfg.rec.fd) / 1024.0;
            }
        }
    }

    if (!strcmp(cfg.rec.codec, "wav")) {
        wav_write_header(cfg.rec.fd, cfg.audio.channel, cfg.audio.samplerate, cfg.wav_codec_rec.bit_depth);
    }

    if (!strcmp(cfg.rec.codec, "flac")) { // The flac encoder closes the file
        flac_enc_close_file(&flac_rec);
    }
    else {
        fclose(cfg.rec.fd);
    }

    free(enc_buf);
    free(audio_buf);

    // Detach thread (free ressources) because no one will call pthread_join() on it
    pthread_detach(pthread_self());

    return NULL;
}

void snd_update_vu(int reset)
{
    int i;
    static float lpeak = 0;
    static float rpeak = 0;
    static int call_cnt = 1;
    float *p;
    float decay = cfg.audio.samplerate < 88200 ? 0.5 : 0.7;
    static double lavg = 0;
    static double ravg = 0;

    if (reset == 1) {
        memset(stream_buf, 0, framepacket_size * sizeof(float) * cfg.audio.channel);
        memset(record_buf, 0, framepacket_size * sizeof(float) * cfg.audio.channel);

        vu_init(); // Reset peak indicators
        call_cnt = 1;
        lpeak = 0;
        rpeak = 0;
        lavg = 0;
        ravg = 0;
    }

    if (vu_level_type == 0) {
        p = stream_buf;
    }
    else {
        p = record_buf;
    }
    for (i = 0; i < framepacket_size; i += cfg.audio.channel) {
        if (abs(p[i]) > lpeak) {
            lpeak = abs(p[i]);
        }
        if (abs(p[i + (cfg.audio.channel - 1)]) > rpeak) {
            rpeak = abs(p[i + (cfg.audio.channel - 1)]);
        }
    }

    float mean_peak = lpeak / 2 + rpeak / 2;
    float mean_peak_dB = 20 * log10(mean_peak);

    if (mean_peak_dB < -cfg.audio.silence_level) {
        silence_detected = true;
    }
    else {
        silence_detected = false;
    }

    if (mean_peak_dB > -cfg.audio.signal_level) {
        signal_detected = true;
    }
    else {
        signal_detected = false;
    }

    lavg = (decay * lpeak) + (1.0 - decay) * lavg;
    ravg = (decay * rpeak) + (1.0 - decay) * ravg;

    // Update the vu meter UI only every second call of this function.
    // This reduces the CPU usage without not missing any samples
    if (call_cnt == 1) {
        vu_meter(lavg, ravg);

        if (cfg.dsp.compressor_stream == 1) {
            fl_g->LED_comp_threshold->set_state(streaming_dsp->is_compressing == true ? LED::LED_ON : LED::LED_OFF);
        }
        else if (cfg.dsp.compressor_rec == 1) {
            fl_g->LED_comp_threshold->set_state(recording_dsp->is_compressing == true ? LED::LED_ON : LED::LED_OFF);
        }

        call_cnt = 0;
    }
    else {
        call_cnt++;
        lpeak = 0;
        rpeak = 0;
    }

    pa_new_frames = 0;
}

void snd_free_device_list(snd_dev_t **dev_list, int dev_count)
{
    if (dev_count == 0) {
        return;
    }

    for (int i = 0; i < SND_MAX_DEVICES; i++) {
        if (i < dev_count) {
            free(dev_list[i]->name);
        }

        free(dev_list[i]);
    }
    free(dev_list);
}

snd_dev_t **snd_get_devices(int *dev_count)
{
    int available_devices, sr_count, dev_num;
    bool sr_supported = 0;
    bool has_default_input_devices = 0;
    const PaDeviceInfo *p_di;
    char info_buf[256];
    PaStreamParameters pa_params;

    int sr[] = {8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000, 88200, 96000};

    snd_dev_t **dev_list;
    dev_list = (snd_dev_t **)malloc(SND_MAX_DEVICES * sizeof(snd_dev_t *));

    for (int i = 0; i < SND_MAX_DEVICES; i++) {
        dev_list[i] = (snd_dev_t *)malloc(sizeof(snd_dev_t));
    }

    dev_num = 0;
    if (Pa_GetDefaultInputDevice() != paNoDevice) {
        dev_list[dev_num]->name = (char *)malloc(strlen(_("Default PCM device (default)")) + 1);
        strcpy(dev_list[dev_num]->name, _("Default PCM device (default)"));
        dev_list[dev_num]->dev_id = Pa_GetDefaultInputDevice();
        has_default_input_devices = 1;
        dev_num = 1;
    }

    available_devices = Pa_GetDeviceCount();
    if (available_devices < 0) {
        snprintf(info_buf, sizeof(info_buf), "PaError: %s", Pa_GetErrorText(available_devices));
        print_info(info_buf, 1);
    }

    for (int i = 0; i < available_devices && i < SND_MAX_DEVICES - 1; i++) {
        sr_count = 0;
        p_di = Pa_GetDeviceInfo(i);
        if (p_di == NULL) {
            snprintf(info_buf, sizeof(info_buf), _("Error getting device Info (%d)"), i);
            print_info(info_buf, 1);
            continue;
        }

        // Save only devices which have input Channels
        if (p_di->maxInputChannels <= 0) {
            continue;
        }

        const PaHostApiInfo *pa_hostapi = Pa_GetHostApiInfo(p_di->hostApi);
        pa_params.device = i;
        pa_params.channelCount = p_di->maxInputChannels;
        pa_params.sampleFormat = paFloat32;
        pa_params.suggestedLatency = p_di->defaultHighInputLatency;
        pa_params.hostApiSpecificStreamInfo = NULL;

        // add the supported samplerates to the device structure
        for (uint32_t j = 0; j < sizeof(sr) / sizeof(sr[0]); j++) {
            if (Pa_IsFormatSupported(&pa_params, NULL, sr[j]) == paFormatIsSupported) {
                dev_list[dev_num]->sr_list[sr_count] = sr[j];
                sr_count++;
                sr_supported = 1;
            }
        }
        // Go to the next device if this one doesn't support at least one of our samplerates
        if (!sr_supported) {
            continue;
        }

        dev_list[dev_num]->num_of_sr = sr_count;

        // Mark the end of the samplerate list for this device with a 0
        dev_list[dev_num]->sr_list[sr_count] = 0;

        dev_list[dev_num]->name = (char *)malloc(strlen(p_di->name) + strlen(pa_hostapi->name) + 10);
        dev_list[dev_num]->dev_id = i;
        dev_list[dev_num]->num_of_channels = p_di->maxInputChannels;
        dev_list[dev_num]->is_asio = pa_hostapi->type == paASIO;
        snprintf(dev_list[dev_num]->name, strlen(p_di->name) + strlen(pa_hostapi->name) + 10, "%s [%s]", p_di->name, pa_hostapi->name);

        // copy the sr_list from the device where the
        // virtual default device points to
        if (has_default_input_devices == 1) {
            if (dev_list[0]->dev_id == dev_list[dev_num]->dev_id) {
                memcpy(dev_list[0]->sr_list, dev_list[dev_num]->sr_list, sizeof(dev_list[dev_num]->sr_list));
                dev_list[0]->num_of_sr = dev_list[dev_num]->num_of_sr;
                dev_list[0]->num_of_channels = dev_list[dev_num]->num_of_channels;
                dev_list[0]->is_asio = dev_list[dev_num]->is_asio;
            }
        }

        // Replace  characters from the device name that have a special meaning to FLTK
        strrpl(&dev_list[dev_num]->name, (char *)"\\", (char *)" ", MODE_ALL); // Must come first
        strrpl(&dev_list[dev_num]->name, (char *)"/", (char *)"\\/", MODE_ALL);
        strrpl(&dev_list[dev_num]->name, (char *)"|", (char *)" ", MODE_ALL);
        strrpl(&dev_list[dev_num]->name, (char *)"\t", (char *)" ", MODE_ALL);
        strrpl(&dev_list[dev_num]->name, (char *)"_", (char *)" ", MODE_ALL);
        strrpl(&dev_list[dev_num]->name, (char *)"&", (char *)"+", MODE_ALL);

        dev_num++;
    } // for(i = 0; i < devcount && i < 100; i++)

    *dev_count = dev_num;

    return dev_list;
}

void snd_print_devices(void)
{
    PaError err;
    const PaDeviceInfo *di;

    if ((err = Pa_Initialize()) != paNoError) {
        printf("PortAudio init failed:\n%s\n", Pa_GetErrorText(err));
        return;
    }

    int dev_count;
    snd_dev_t **dev_list;
    dev_list = snd_get_devices(&dev_count);

    if (dev_count == 0) {
        printf("No input audio device available\n");
        return;
    }

    for (int i = 0; i < dev_count; i++) {
        printf("Device ID: %d\n", i);
        printf("Device name: %s\n", dev_list[i]->name);
        printf("Number of channels: %d\n", dev_list[i]->num_of_channels);
        printf("Supported sample rates: ");
        for (int j = 0; j < dev_list[i]->num_of_sr; j++) {
            if (j < dev_list[i]->num_of_sr - 1) {
                printf("%d Hz, ", dev_list[i]->sr_list[j]);
            }
            else {
                printf("%d Hz", dev_list[i]->sr_list[j]);
            }
        }

        printf("\n\n");
    }

    snd_free_device_list(dev_list, dev_count);

    Pa_Terminate();
}

void snd_reset_samplerate_conv(int rec_or_stream)
{
    int error;

    if (rec_or_stream == SND_STREAM) {
        if (srconv_state_opus_stream != NULL) {
            src_delete(srconv_state_opus_stream);
            srconv_state_opus_stream = NULL;
        }

        srconv_state_opus_stream = src_new(cfg.audio.resample_mode, cfg.audio.channel, &error);
        if (srconv_state_opus_stream == NULL) {
            print_info(_("ERROR: Could not initialize samplerate converter"), 0);
        }

        srconv_opus_stream.end_of_input = 0;
        srconv_opus_stream.src_ratio = 48000.0 / cfg.audio.samplerate;
        srconv_opus_stream.input_frames = pa_frames;
        srconv_opus_stream.output_frames = pa_frames * (1 + srconv_opus_stream.src_ratio);
    }

    if (rec_or_stream == SND_REC) {
        if (srconv_state_opus_record != NULL) {
            src_delete(srconv_state_opus_record);
            srconv_state_opus_record = NULL;
        }

        srconv_state_opus_record = src_new(cfg.audio.resample_mode, cfg.audio.channel, &error);
        if (srconv_state_opus_record == NULL) {
            print_info(_("ERROR: Could not initialize samplerate converter"), 0);
        }

        srconv_opus_record.end_of_input = 0;
        srconv_opus_record.src_ratio = 48000.0 / cfg.audio.samplerate;
        srconv_opus_record.input_frames = pa_frames;
        srconv_opus_record.output_frames = pa_frames * (1 + srconv_opus_record.src_ratio);
    }
}

char *snd_get_device_name(int id)
{
    const PaDeviceInfo *pa_dev_info;

    pa_dev_info = Pa_GetDeviceInfo(id);
    if (pa_dev_info == NULL) {
        return NULL;
    }
    else {
        return strdup(pa_dev_info->name); // Note: Make sure it will be free'd on the caller side
    }
}

int snd_get_number_of_channels(int id)
{
    const PaDeviceInfo *pa_dev_info;

    pa_dev_info = Pa_GetDeviceInfo(id);
    if (pa_dev_info == NULL) {
        return 0;
    }
    else {
        return pa_dev_info->maxInputChannels;
    }
}

int snd_get_dev_num_by_name(char *name)
{
    if (name != NULL) {
        for (int i = 0; i < cfg.audio.dev_count; i++) {
            if (!strcmp(name, cfg.audio.pcm_list[i]->name)) {
                return i;
            }
        }
    }

    return -1; // Device not found
}

void snd_close_streams(void)
{
    int stream_is_active = Pa_IsStreamActive(stream);
    int stream2_is_active = Pa_IsStreamActive(stream2);

    if (stream2_is_active == 1) {
        Pa_StopStream(stream2);
        Pa_CloseStream(stream2);
    }

    if (stream_is_active == 1) {
        snd_stop_mixer_thread();
        Pa_StopStream(stream);
        Pa_CloseStream(stream);
        g_stop_vu_meter_timer = 1;
        while (g_vu_meter_timer_is_active == 1) {
            Fl::check();
            Pa_Sleep(1);
        }
        g_stop_vu_meter_timer = 0;
        snd_update_vu(1);

        free(pa_pcm_buf);
        free(pa_mixer_buf);
        free(encode_buf);
        free(stream_buf);
        free(record_buf);
        free(srconv_opus_stream.data_out);
        free(srconv_opus_record.data_out);
        free(srconv_dev2.data_out);

        rb_free(&pa_pcm_rb);
        rb_free(&rec_rb);
        rb_free(&stream_rb);
    }

    if (stream2_is_active == 1) {
        free(pa_pcm_buf2);
        free(pa_mixer_buf2);
        rb_free(&pa_pcm2_rb);
    }
}

void snd_close_portaudio(void)
{
    Pa_Terminate();
}

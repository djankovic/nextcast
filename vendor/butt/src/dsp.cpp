//
//  dsp.cpp
//  butt
//
//  Created by Melchor Garau Madrigal on 16/2/16.
//  Copyright © 2016 Daniel Noethen. All rights reserved.
//

#include "dsp.hpp"
#include <cmath>
#include <algorithm>
#include <string.h>
#include "Biquad.h"
#include "cfg.h"

DSPEffects::DSPEffects(uint32_t frames, uint8_t channels, uint32_t sampleRate) : samplerate(sampleRate)
{
    chans = (channels == 2) ? 2 : 1;
    dsp_size = frames * channels;

    eq_active = 0;
    drc_active = 0;

    for (uint32_t i = 0; i < eq_band_count; i++) {
        eq_bands_left[i] = new Biquad(bq_type_peak, eq_freqs[i] / double(samplerate), 2, cfg.dsp.eq_gain[i]);
        eq_bands_right[i] = new Biquad(bq_type_peak, eq_freqs[i] / double(samplerate), 2, cfg.dsp.eq_gain[i]);
    }
}

bool DSPEffects::hasToProcessSamples()
{
    return cfg.main.gain != 1 || eq_active || drc_active;
}

void DSPEffects::loadEQPreset(const char *preset)
{
    double *gain_vals;

    if (!strcmp(preset, "Acoustic")) {
        gain_vals = eq_acoustic;
    }
    else if (!strcmp(preset, "Bass Booster")) {
        gain_vals = eq_bass_booster;
    }
    else if (!strcmp(preset, "Bass Reducer")) {
        gain_vals = eq_bass_reducer;
    }
    else if (!strcmp(preset, "Classical")) {
        gain_vals = eq_classical;
    }
    else if (!strcmp(preset, "Dance")) {
        gain_vals = eq_dance;
    }
    else if (!strcmp(preset, "Deep")) {
        gain_vals = eq_deep;
    }
    else if (!strcmp(preset, "Electronic")) {
        gain_vals = eq_electronic;
    }
    else if (!strcmp(preset, "Hip-Hop")) {
        gain_vals = eq_hiphop;
    }
    else if (!strcmp(preset, "Jazz")) {
        gain_vals = eq_jazz;
    }
    else if (!strcmp(preset, "Latin")) {
        gain_vals = eq_latin;
    }
    else if (!strcmp(preset, "Loudness")) {
        gain_vals = eq_loudness;
    }
    else if (!strcmp(preset, "Lounge")) {
        gain_vals = eq_lounge;
    }
    else if (!strcmp(preset, "Piano")) {
        gain_vals = eq_piano;
    }
    else if (!strcmp(preset, "RnB")) {
        gain_vals = eq_r_n_b;
    }
    else if (!strcmp(preset, "Rock")) {
        gain_vals = eq_rock;
    }
    else if (!strcmp(preset, "Speech")) {
        gain_vals = eq_speech;
    }
    else if (!strcmp(preset, "Treble Booster")) {
        gain_vals = eq_treble_booster;
    }
    else if (!strcmp(preset, "Treble Reducer")) {
        gain_vals = eq_trebles_reducer;
    }
    else if (!strcmp(preset, "Vocal Booster")) {
        gain_vals = eq_vocal_booster;
    }

    for (uint32_t i = 0; i < eq_band_count; i++) {
        setEQband(0, gain_vals[i]);
    }
}

void DSPEffects::setEQband(int band, double gain_val)
{
    eq_bands_left[band]->setPeakGain(gain_val);
    eq_bands_right[band]->setPeakGain(gain_val);
}

/*
double DSPEffects::getEQband(int band)
{
   // return eq_bands_left[band]; // left and right have always the same gain value
}*/

double DSPEffects::getEQbandFromPreset(int band, const char *preset)
{
    if (!strcmp(preset, "Acoustic")) {
        return eq_acoustic[band];
    }
    else if (!strcmp(preset, "Bass Booster")) {
        return eq_bass_booster[band];
    }
    else if (!strcmp(preset, "Bass Reducer")) {
        return eq_bass_reducer[band];
    }
    else if (!strcmp(preset, "Classical")) {
        return eq_classical[band];
    }
    else if (!strcmp(preset, "Dance")) {
        return eq_dance[band];
    }
    else if (!strcmp(preset, "Deep")) {
        return eq_deep[band];
    }
    else if (!strcmp(preset, "Electronic")) {
        return eq_electronic[band];
    }
    else if (!strcmp(preset, "Hip-Hop")) {
        return eq_hiphop[band];
    }
    else if (!strcmp(preset, "Jazz")) {
        return eq_jazz[band];
    }
    else if (!strcmp(preset, "Latin")) {
        return eq_latin[band];
    }
    else if (!strcmp(preset, "Loudness")) {
        return eq_loudness[band];
    }
    else if (!strcmp(preset, "Lounge")) {
        return eq_lounge[band];
    }
    else if (!strcmp(preset, "Piano")) {
        return eq_piano[band];
    }
    else if (!strcmp(preset, "RnB")) {
        return eq_r_n_b[band];
    }
    else if (!strcmp(preset, "Rock")) {
        return eq_rock[band];
    }
    else if (!strcmp(preset, "Speech")) {
        return eq_speech[band];
    }
    else if (!strcmp(preset, "Treble Booster")) {
        return eq_treble_booster[band];
    }
    else if (!strcmp(preset, "Treble Reducer")) {
        return eq_trebles_reducer[band];
    }
    else if (!strcmp(preset, "Vocal Booster")) {
        return eq_vocal_booster[band];
    }

    return 0;
}

void DSPEffects::processSamples(float *audio_buf)
{
    if (cfg.main.gain != 1) {
        for (uint32_t i = 0; i < dsp_size; i += chans) {
            audio_buf[i] *= cfg.main.gain;

            if (chans == 2) {
                audio_buf[i + 1] *= cfg.main.gain;
            }
        }
    }

    if (drc_active) {
        attack_const = expf(-1.0f / (cfg.dsp.attack * samplerate));
        release_const = expf(-1.0f / (cfg.dsp.release * samplerate));

        if (cfg.dsp.aggressive_mode == 1) {
            lowpass_const = 0;
        }
        else {
            float lowpass_time = std::min(cfg.dsp.attack, cfg.dsp.release);
            lowpass_time = std::max(0.002f, lowpass_time);
            lowpass_const = expf(-1.0f / (lowpass_time * samplerate));
        }
        compress(audio_buf);
    }

    if (eq_active) {
        for (uint32_t i = 0; i < dsp_size; i += chans) {
            float s = audio_buf[i];
            for (int32_t j = eq_band_count - 1; j >= 0; j--) {
                s = eq_bands_left[j]->process(s);
            }
            audio_buf[i] = s;

            if (chans == 2) {
                float s = audio_buf[i + 1];
                for (int32_t j = eq_band_count - 1; j >= 0; j--) {
                    s = eq_bands_right[j]->process(s);
                }
                audio_buf[i + 1] = s;
            }
        }
    }

    // Clamp to -1.0 .. 1.0 range
    for (uint32_t i = 0; i < dsp_size; i++) {
        if (audio_buf[i] > 1.0) {
            audio_buf[i] = 1.0;
            continue;
        }
        if (audio_buf[i] < -1.0) {
            audio_buf[i] = -1.0;
        }
    }
}

DSPEffects::~DSPEffects()
{
    for (uint32_t i = 0; i < eq_band_count; i++) {
        delete eq_bands_left[i];
        delete eq_bands_right[i];
    }
}

// loosely based on https://openaudio.blogspot.com/2017/01/basic-dynamic-range-compressor.html
// https://github.com/chipaudette/OpenAudio_ArduinoLibrary/blob/master/AudioEffectCompressor_F32.h
void DSPEffects::compress(float *audio_buf)
{
    float one_minus_attack_const = 1.0f - attack_const;
    float one_minus_release_const = 1.0f - release_const;
    float c1 = lowpass_const, c2 = 1.0f - c1;

    float ratio = cfg.dsp.ratio;

    if (ratio < 0.001) {
        ratio = 1.0f;
    }

    is_compressing = false;

    for (uint32_t i = 0; i < dsp_size; i += chans) {
        // calculate instantaneous signal power
        // (square signal and average channels)
        float power = audio_buf[i] * audio_buf[i];
        if (chans == 2) {
            power += audio_buf[i + 1] * audio_buf[i + 1];
            power /= 2;
        }

        // low-pass filter
        power = c1 * prev_power + c2 * power;
        prev_power = power;

        // Do not apply compression if signal power is nearly zero to prevent -inf results for log10 calculations
        if (power < 1.0E-13) {
            continue;
        }

        // convert to dB
        float power_dB = 10.0f * std::log10(power);

        // calculate instantaneous target gain
        float above_threshold = power_dB - cfg.dsp.threshold;
        float gain_dB = (above_threshold / ratio) - above_threshold;

        if (above_threshold > 0) {
            is_compressing = true;
        }

        if (gain_dB > 0.0f) {
            gain_dB = 0.0f;
        }

        // smooth gain by attack and release
        if (std::isnan(prev_gain_dB)) {
            prev_gain_dB = 0.0f;
        }

        if (gain_dB < prev_gain_dB) {
            // attack phase
            gain_dB = attack_const * prev_gain_dB + one_minus_attack_const * gain_dB;
        }
        else {
            // release phase
            gain_dB = release_const * prev_gain_dB + one_minus_release_const * gain_dB;
        }
        prev_gain_dB = gain_dB;

        // convert to linear gain
        float gain_linear = pow(10, (gain_dB + cfg.dsp.makeup_gain) / 20.0f);

        // apply gain to samples
        audio_buf[i] *= gain_linear;
        if (chans == 2) {
            audio_buf[i + 1] *= gain_linear;
        }
    }

    // limit the amount that the state of the smoothing filter can go towards -inf
    if (prev_power < (1.0E-13)) {
        prev_power = 1.0E-13; // never go less than -130 dbFS
    }
}

void DSPEffects::reset_compressor()
{
    prev_power = 1.0;
    prev_gain_dB = 0.0;
}

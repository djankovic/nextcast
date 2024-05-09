//
//  dsp.hpp
//  butt
//
//  Created by Melchor Garau Madrigal on 16/2/16.
//  Copyright © 2016 Daniel Nöthen. All rights reserved.
//

#ifndef dsp_hpp
#define dsp_hpp

#include <stdint.h>

#define EQ_BAND_COUNT (10)

class DSPEffects {
private:
    double eq_acoustic[10] = {5, 5, 4, 1.5, 2.5, 2, 3.5, 4, 3.5, 2.5};
    double eq_bass_booster[10] = {6, 4, 3.5, 3, 2, 0, 0, 0, 0, 0};
    double eq_bass_reducer[10] = {-6, -4, -3.5, -3, -2, 0, 0, 0, 0, 0};
    double eq_classical[10] = {4.5, 3.5, 3, 3, -2, -2, 0, 2.5, 3, 3.5};
    double eq_dance[10] = {3.5, 6.5, 5, 0, 2.5, 3.5, 6, 4.5, 4, 0};
    double eq_deep[10] = {5, 3.5, 2, 1, 3, 3, 2, -2.5, -3.5, 4};
    double eq_electronic[10] = {4, 3.8, 2, 0, -2.5, 2.5, 2.5, 2, 4, 4.5};
    double eq_hiphop[10] = {5, 4, 2, 3, -1, -1, 2, -0.5, 2.5, 3};
    double eq_jazz[10] = {4, 3, 2, 2.5, -2, -2, 0, 2, 3, 3.5};
    double eq_latin[10] = {4, 3, 0, 0, -2, -2, -2, 0, 3, 4};
    double eq_loudness[10] = {6.5, 4, 0, 0, -2, 0, -1, -5, 5, 1.5};
    double eq_lounge[10] = {-3, -2, 0, 1.5, 4, 3, 0, -1.5, 2, 1};
    double eq_piano[10] = {3, 2.5, 0, 3, 3, 2, 3.5, 4, 3, 3.5};
    double eq_pop[10] = {-1.5, -1, 0, 2.5, 4, 4, 2.5, 0, -1.5, -2};
    double eq_r_n_b[10] = {3, 7.5, 6, 2, -2.5, -2, 2.8, 2.9, 3, 3.5};
    double eq_rock[10] = {5.5, 4, 3, 2, -0.5, -1, 1, 2.5, 3.5, 4};
    double eq_small_speakers[10] = {6, 4, 3.5, 3, 2, 0, -2, -3, -3.5, -4};
    double eq_speech[10] = {-3.5, -0.5, 0, 0.5, 3.5, 4.5, 4.5, 4, 3, 0};
    double eq_treble_booster[10] = {0, 0, 0, 0, 0, 2, 3, 3.5, 4, 5.5};
    double eq_trebles_reducer[10] = {0, 0, 0, 0, 0, -2, -3, -3.5, -4, -5.5};
    double eq_vocal_booster[10] = {-2, -3, -3, 2, 4, 4, 3.5, 2, 0, -2};
    
    
    double eq_freqs[EQ_BAND_COUNT] = {32, 64, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};


    uint32_t dsp_size;
    uint32_t samplerate;
	uint8_t chans;
    class Biquad *eq_bands_left[EQ_BAND_COUNT];
    class Biquad *eq_bands_right[EQ_BAND_COUNT];
    
    
	float attack_const, release_const, lowpass_const;
	float prev_power = 1.0;
	float prev_gain_dB = 0.0;
	
	void compress(float *audio_buf);

public:
    DSPEffects(uint32_t frames, uint8_t channels, uint32_t sampleRate);
    ~DSPEffects();
    
    uint32_t eq_band_count = EQ_BAND_COUNT;
    int eq_active;
    int drc_active;
    bool is_compressing;
    bool hasToProcessSamples();
    void loadEQPreset(const char *preset);
    void setEQband(int band, double gain_val);
    double getEQband(int band);
    double getEQbandFromPreset(int band, const char* preset);


    void processSamples(float* audio_buf);
    void reset_compressor();
};

#endif /* dsp_hpp */

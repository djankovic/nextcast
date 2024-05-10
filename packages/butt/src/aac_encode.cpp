// aac encoding functions for butt
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
#include <math.h>
#include "config.h"
#include "fl_funcs.h"
#include "aac_encode.h"
#include "wav_header.h"

int g_aac_lib_available = 0;

#ifdef HAVE_LIBFDK_AAC

int aac_enc_init(aac_enc *aac)
{
    int rc;
    int aot;
    int vbr_mode = 0;
    char info_buf[256];

    if (g_aac_lib_available == 0) {
        snprintf(info_buf, sizeof(info_buf), "libfdk-aac-2.dll not loaded");
        print_info(info_buf, 1);
        return 1;
    }

    switch (aac->profile) {
    case 0: // Auto select profile
        if (aac->bitrate < 48) {
            aot = 29; // AAC+v2
            vbr_mode = 1;
        }
        else if (aac->bitrate >= 96) {
            aot = 2; // AAC-LC
            vbr_mode = 4;
        }
        else {
            aot = 5; // AAC+v1
            vbr_mode = 2;
        }
        break;
    case 1:
        aot = 2;
        vbr_mode = 4;
        break;
    case 2:
        aot = 5;
        vbr_mode = 2;
        break;
    case 3:
        aot = 29;
        vbr_mode = 1;
        break;
    default:
        aot = 5;
        vbr_mode = 2;
        break;
    }

    aacEncOpen_butt(&aac->handle, 0, aac->channel);
    aacEncoder_SetParam_butt(aac->handle, AACENC_AOT, aot);
    aacEncoder_SetParam_butt(aac->handle, AACENC_SAMPLERATE, aac->samplerate);
    aacEncoder_SetParam_butt(aac->handle, AACENC_CHANNELMODE, aac->channel);
    aacEncoder_SetParam_butt(aac->handle, AACENC_CHANNELORDER, 1);
    aacEncoder_SetParam_butt(aac->handle, AACENC_BITRATE, aac->bitrate * 1000);
    aacEncoder_SetParam_butt(aac->handle, AACENC_TRANSMUX, 2); // taken from the example aac-enc.c
    aacEncoder_SetParam_butt(aac->handle, AACENC_AFTERBURNER, aac->afterburner);

    if (aac->bitrate_mode == 1) { // VBR
        aacEncoder_SetParam_butt(aac->handle, AACENC_BITRATEMODE, vbr_mode);
    }

    if ((rc = aacEncEncode_butt(aac->handle, NULL, NULL, NULL, NULL)) != AACENC_OK) {
        snprintf(info_buf, sizeof(info_buf), "unable to init aac params %d", rc);
        print_info(info_buf, 1);
        return 1;
    }

    aacEncInfo_butt(aac->handle, &aac->info);

    /*
    printf("AAC profile: %d (aot: %d)\n", aac->profile, aot);
    printf("AAC afterburner: %d\n", aac->afterburner);
    printf("AAC bitrate_mode: %d (vbr: %d)\n\n", aac->bitrate_mode, vbr_mode);
    */
    //printf("AAC bitrate: %d\n", aac->bitrate);

    return 0;
}

int aac_enc_reinit(aac_enc *aac)
{
    if (g_aac_lib_available == 0) {
        char info_buf[256];
        snprintf(info_buf, sizeof(info_buf), "libfdk-aac-2.dll not loaded");
        print_info(info_buf, 1);
        return 1;
    }

    if (aac != NULL) {
        aac_enc_close(aac);
        return aac_enc_init(aac);
    }
    return 1;
}

void aac_enc_close(aac_enc *aac)
{
    if (g_aac_lib_available == 0) {
        char info_buf[256];
        snprintf(info_buf, sizeof(info_buf), "libfdk-aac-2.dll not loaded");
        print_info(info_buf, 1);
        return;
    }

    while (aac->state == AAC_BUSY)
        ;

    if (aac->handle != NULL) {
        aacEncClose_butt(&aac->handle);
    }

    aac->handle = NULL;
}

int aac_enc_encode(aac_enc *aac, float *pcm_float, char *enc_buf, int frames, int enc_buf_size)
{
    if (g_aac_lib_available == 0) {
        char info_buf[256];
        snprintf(info_buf, sizeof(info_buf), "libfdk-aac-2.dll not loaded");
        print_info(info_buf, 1);
        return 0;
    }

    int rc;
    AACENC_BufDesc in_buf = {0};
    AACENC_BufDesc out_buf = {0};
    AACENC_InArgs in_args = {0};
    AACENC_OutArgs out_args = {0};

    int in_identifier = IN_AUDIO_DATA;
    int in_size, in_elem_size;
    int out_identifier = OUT_BITSTREAM_DATA;
    int out_size, out_elem_size;
    void *in_ptr, *out_ptr;
    int32_t *pcm_int32;

    if (frames == 0 || aac->handle == NULL) {
        return 0;
    }

    in_size = frames * sizeof(int32_t) * aac->channel;
    in_elem_size = sizeof(int32_t);

    pcm_int32 = (int32_t *)pcm_float;
    in_ptr = pcm_int32;
    in_args.numInSamples = frames * aac->channel;
    in_buf.numBufs = 1;
    in_buf.bufs = &in_ptr;
    in_buf.bufferIdentifiers = &in_identifier;
    in_buf.bufSizes = &in_size;
    in_buf.bufElSizes = &in_elem_size;

    out_ptr = enc_buf;
    out_size = enc_buf_size;
    out_elem_size = 1;
    out_buf.numBufs = 1;
    out_buf.bufs = &out_ptr;
    out_buf.bufferIdentifiers = &out_identifier;
    out_buf.bufSizes = &out_size;
    out_buf.bufElSizes = &out_elem_size;

    aac->state = AAC_BUSY;

    // Convert from float to int32
    for (int i = 0; i < frames * aac->channel; i++) {
        if (pcm_float[i] > 0) {
            pcm_int32[i] = (int32_t)round(fmin(pcm_float[i] * INT32_MAX, INT32_MAX));
        }
        else {
            pcm_int32[i] = (int32_t)round(fmax(-pcm_float[i] * INT32_MIN, INT32_MIN));
        }
    }

    if ((rc = aacEncEncode_butt(aac->handle, &in_buf, &out_buf, &in_args, &out_args)) != AACENC_OK) {
        return 0;
    }

    aac->state = AAC_READY;

    return out_args.numOutBytes;
}

int aac_enc_get_samplerate(aac_enc *aac)
{
    //    return aacEncoder_GetParam_butt(aac->handle, AACENC_SAMPLERATE);
    return aacEncoder_GetParam_butt(aac->handle, AACENC_SAMPLERATE);
}
#endif

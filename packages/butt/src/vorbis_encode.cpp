// vorbis encoding functions for butt
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

#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "config.h"
#include "cfg.h"
#include "vorbis_encode.h"

int vorbis_enc_init(vorbis_enc *vorbis)
{
    int ret;
    int min_bitrate, nominal_bitrate, max_bitrate;

    vorbis_info_init(&(vorbis->vi));

    vorbis->os.body_data = NULL;

    // CBR
    if (vorbis->bitrate_mode == 0) {
        min_bitrate = vorbis->bitrate * 1000;
        nominal_bitrate = vorbis->bitrate * 1000;
        max_bitrate = vorbis->bitrate * 1000;
    }

    // VBR
    if (vorbis->bitrate_mode == 1) {
        min_bitrate = vorbis->vbr_min_bitrate * 1000;
        nominal_bitrate = vorbis->bitrate * 1000;
        max_bitrate = vorbis->vbr_min_bitrate * 1000;
    }

    // ABR
    if (vorbis->bitrate_mode == 2) {
        min_bitrate = -1;
        nominal_bitrate = vorbis->bitrate * 1000;
        max_bitrate = -1;
    }

    // CBR or ABR
    if (vorbis->bitrate_mode == 0 || vorbis->bitrate_mode == 2) {
        ret = vorbis_encode_init(&(vorbis->vi), vorbis->channel, vorbis->samplerate, min_bitrate, nominal_bitrate, max_bitrate);
        if (ret) {
            return ret;
        }
    } // VBR
    else {
        ret = vorbis_encode_init_vbr(&(vorbis->vi), vorbis->channel, vorbis->samplerate, vorbis->vbr_quality);

        if (ret) {
            return ret;
        }
    }

    /*
    printf("vorbis - bitrate_mode: %d\n", vorbis->bitrate_mode);
    printf("vorbis - quality: %0.2f\n", vorbis->vbr_quality);
    printf("vorbis - min_bitrate: %d\n", min_bitrate);
    printf("vorbis - nominal_bitrate: %d\n", nominal_bitrate);
    printf("vorbis - max_bitrate: %d\n\n", max_bitrate);
    */

    //printf("Vorbis bitrate: %d\n", vorbis->bitrate);

    vorbis_comment_init(&(vorbis->vc));
    vorbis_comment_add_tag(&(vorbis->vc), "ENCODER", PACKAGE_STRING);

    vorbis_analysis_init(&(vorbis->vd), &(vorbis->vi));
    vorbis_block_init(&(vorbis->vd), &(vorbis->vb));

    return 0;
}

// This function needs to be called before
// every connection
void vorbis_enc_write_header(vorbis_enc *vorbis)
{
    ogg_packet header;
    ogg_packet header_comm;
    ogg_packet header_code;

    srand(time(NULL));
    ogg_stream_init(&(vorbis->os), rand());

    vorbis_analysis_headerout(&(vorbis->vd), &(vorbis->vc), &header, &header_comm, &header_code);

    ogg_stream_packetin(&(vorbis->os), &header);
    ogg_stream_packetin(&(vorbis->os), &header_comm);
    ogg_stream_packetin(&(vorbis->os), &header_code);
}

int vorbis_enc_reinit(vorbis_enc *vorbis)
{
    if (vorbis != NULL) {
        vorbis_enc_close(vorbis);
        return vorbis_enc_init(vorbis);
    }
    return 1;
}

int vorbis_enc_encode(vorbis_enc *vorbis, float *pcm_buf, char *enc_buf, int size)
{
    int i, result;
    int eos = 0;
    int w = 0;
    float **vorbis_buf;

    if (size == 0) {
        return 0;
    }

    /* This ensures the actual
     * audio data will start on a new page, as per spec
     */
    result = ogg_stream_flush(&(vorbis->os), &(vorbis->og));
    memcpy(enc_buf + w, vorbis->og.header, (size_t)vorbis->og.header_len);
    w += vorbis->og.header_len;
    memcpy(enc_buf + w, vorbis->og.body, (size_t)vorbis->og.body_len);
    w += vorbis->og.body_len;

    vorbis_buf = vorbis_analysis_buffer(&(vorbis->vd), size);

    // deinterlace audio data
    // stereo
    if (vorbis->channel == 2) {
        for (i = 0; i < size; i++) {
            vorbis_buf[0][i] = pcm_buf[i * 2];
            vorbis_buf[1][i] = pcm_buf[i * 2 + 1];
        }
    } // mono
    else {
        for (i = 0; i < size; i++) {
            vorbis_buf[0][i] = pcm_buf[i];
        }
    }

    vorbis_analysis_wrote(&(vorbis->vd), size);

    while (vorbis_analysis_blockout(&(vorbis->vd), &(vorbis->vb)) == 1) {
        vorbis_analysis(&(vorbis->vb), &(vorbis->op));
        vorbis_bitrate_addblock(&(vorbis->vb));

        while (vorbis_bitrate_flushpacket(&(vorbis->vd), &(vorbis->op))) {
            /* weld the packet into the bitstream */
            ogg_stream_packetin(&(vorbis->os), &(vorbis->op));

            /* write out pages (if any) */
            while (!eos) {
                result = ogg_stream_pageout(&(vorbis->os), &(vorbis->og));
                if (result == 0) {
                    break;
                }

                memcpy(enc_buf + w, vorbis->og.header, vorbis->og.header_len);
                w += vorbis->og.header_len;
                memcpy(enc_buf + w, vorbis->og.body, vorbis->og.body_len);
                w += vorbis->og.body_len;
                eos = ogg_page_eos(&(vorbis->og));
            }
        }
    }

    return w;
}

int vorbis_enc_get_samplerate(vorbis_enc *vorbis)
{
    return vorbis->vi.rate;
}

void vorbis_enc_close(vorbis_enc *vorbis)
{
    if (vorbis->os.body_data != NULL) {
        ogg_stream_clear(&(vorbis->os));
    }

    vorbis_block_clear(&(vorbis->vb));
    vorbis_dsp_clear(&(vorbis->vd));
    vorbis_comment_clear(&(vorbis->vc));
    vorbis_info_clear(&(vorbis->vi));
}

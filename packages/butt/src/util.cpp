// utility functions for butt
//
//  Copyright 2007-2018 by Daniel Noethen.
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2, or (at your option)
//  any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/stat.h>
#include <FL/fl_utf8.h>

#include "util.h"

void set_max_thread_priority(void)
{
    int policy, max_prio;
    pthread_attr_t attr;
    struct sched_param param;
    pthread_attr_init(&attr);
    pthread_getschedparam(pthread_self(), &policy, &param);
    policy = SCHED_RR;
    max_prio = sched_get_priority_max(policy);

    // printf("max_prio: %d\n", max_prio);

    if (max_prio != -1) {
        param.sched_priority = max_prio;
        pthread_setschedparam(pthread_self(), policy, &param);
    }
    else {
        printf("could not set priority\n");
    }
}

char *util_base64_enc(char *data)
{
    int i, j;
    int len;
    int chunk;
    char *b64_data;
    static char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                        "abcdefghijklmnopqrstuvwxyz"
                        "0123456789+/";

    len = strlen(data);

    b64_data = (char *)malloc(len * 4 / 3 + 4);

    j = 0;
    for (i = 0; i < len; i += 3) {
        chunk = (data[i] & 0xFC) >> 2;
        b64_data[j] = b64[chunk];

        chunk = ((data[i] & 0x03) << 4) | ((data[i + 1] & 0xF0) >> 4);
        b64_data[j + 1] = b64[chunk];

        if (i + 2 <= len) {
            chunk = ((data[i + 1] & 0x0F) << 2) | ((data[i + 2] & 0xC0) >> 6);
            b64_data[j + 2] = b64[chunk];
        }
        else {
            b64_data[j + 2] = '='; // padding
        }

        if (i + 3 <= len) {
            chunk = data[i + 2] & 0x3F;
            b64_data[j + 3] = b64[chunk];
        }
        else {
            b64_data[j + 3] = '='; // padding
        }

        j += 4;
    }

    b64_data[j] = '\0';
    return b64_data;
}

char *util_get_file_extension(char *filename)
{
    static char *ext;
    // find the last occurence of '.' in the filename
    ext = strrchr(filename, (int)'.');

    // return NULL if no '.' was found or the '.' is the last char in the
    // filename
    if (ext == NULL || ext[1] == '\0') {
        return NULL;
    }
    else {
        return ++ext;
    }
}

float util_factor_to_db(float factor)
{
    return 20 * log10(factor);
}

float util_db_to_factor(float dB)
{
    return pow(10, dB / 20);
}

int util_mkpath(char *path)
{
    char *tmp;
    char *p = NULL;
    size_t len;

    tmp = strdup(path);
    len = strlen(tmp);

    if (tmp[len - 1] == '/') {
        tmp[len - 1] = 0;
    }

    for (p = tmp + 1; *p != '\0'; p++) {
#ifdef WIN32
        // Executing fl_mkdir on a disk other than C:\ returns EACCESS instead of EEXIST
        // Therefore we make sure fl_mkdir is not executed for C:\, D:\, etc.
        if (*p == ':') {
            p += 2;
        }
#endif
        if (*p == '/') {
            *p = 0;
            if (fl_mkdir(tmp, 0700) != 0 && errno != EEXIST) {
                free(tmp);
                return -1;
            }
            *p = '/';
        }
    }

    if (fl_mkdir(tmp, 0777) != 0 && errno != EEXIST) {
        free(tmp);
        return -1;
    }

    free(tmp);
    return 0;
}

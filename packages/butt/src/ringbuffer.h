// ringbuffer functions for butt
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
//
#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <pthread.h>

typedef struct ringbuf {
    char *buf;
    char *w_ptr;
    char *r_ptr;
    unsigned int size;
    unsigned int full;
    pthread_mutex_t mutex;
} ringbuf_t;

int rb_init(ringbuf_t *rb, unsigned int size);
int rb_filled(ringbuf_t *rb);
int rb_space(ringbuf_t *rb);
unsigned int rb_read(ringbuf_t *rb, char *dest);
unsigned int rb_read_len(ringbuf_t *rb, char *dest, unsigned int len);
int rb_write(ringbuf_t *rb, char *src, unsigned int size);
int rb_clear(ringbuf_t *rb);
int rb_free(ringbuf_t *rb);

#endif /*RINGBUFFER_H*/

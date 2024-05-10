// socket functions for butt
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

#ifndef SOCKFUNCS_H
#define SOCKFUNCS_H

#ifdef WIN32
#include <windows.h>
typedef int socklen_t;
#else
#include <sys/socket.h>
#endif

#define SOCK_MAX_DATAGRAM_SIZE 65535

enum {
    READ = 0,
    WRITE = 1,
    RW = 2,
};

enum {
    SOCK_ERR_CREATE = -1,
    SOCK_ERR_RESOLVE = -2,
    SOCK_TIMEOUT = -3,
    SOCK_INVALID = -4,
    SOCK_NO_MODE = -5,
    SOCK_ERR_SET_SBUF = -6,
    SOCK_ERR_SET_RBUF = -7,
    SOCK_ERR_BIND = -8,
    SOCK_ERR_LISTEN = -9,
    SOCK_ERR_RECV = -10,
};

enum {
    CONN_TIMEOUT = 1000,
    SEND_TIMEOUT = 3000,
    RECV_TIMEOUT = 1000,
};

typedef enum sock_proto { SOCK_PROTO_TCP = 0, SOCK_PROTO_UDP = 1 } sock_proto_t;

typedef struct sock_udp_conn {
    struct sockaddr addr;
    socklen_t addr_len;

} sock_udp_conn_t;

void sock_init(void);
int sock_init_udp_server(sock_udp_conn_t *udp_conn);
int sock_connect(const char *addr, unsigned int port, sock_proto_t proto, int timout_ms);
int sock_listen(int port, int *listen_sock);
int sock_setbufsize(int s, int send_size, int recv_size);
int sock_isdisconnected(int s);
int sock_send(int s, const char *buf, int len, int timout_ms);
int sock_sendto(int s, const char *buf, int len, sock_udp_conn_t *udp_conn, int timout_ms);
int sock_recv(int s, char *buf, int len, int timout_ms);
int sock_recvfrom(int s, char *buf, int len, sock_udp_conn_t *udp_conn, int timout_ms);
int sock_select(int s, int timout_ms, int mode);
int sock_nonblock(int s);
int sock_block(int s);
int sock_isvalid(int s);
void sock_close(int s);
void sock_rst_close(int s);
void sock_fdinit(int s);
void sock_fdclr(int s);
void sock_fdzero(void);
int sock_get_errno(void);

#endif

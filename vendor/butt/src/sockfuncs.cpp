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

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#ifdef WIN32
#include <winsock2.h>
#include <Ws2tcpip.h>
#undef errno
#undef EWOULDBLOCK
#undef ECONNRESET
#define errno       WSAGetLastError()
#define EWOULDBLOCK WSAEWOULDBLOCK
#define ECONNRESET  WSAECONNRESET
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h> //defines IPPROTO_TCP on BSD
#include <netdb.h>
#include <sys/select.h>
#include <errno.h>
#endif

#include "sockfuncs.h"

void sock_init(void)
{
#ifdef WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
}

int sock_connect(const char *addr, unsigned int port, sock_proto_t proto, int timout_ms)
{
    int sock;
    int ret;
    char port_str[8];
    struct addrinfo hints, *infoptr, *p; // So no need to use memset global variables

    snprintf(port_str, sizeof(port_str), "%d", port);
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = proto == SOCK_PROTO_TCP ? SOCK_STREAM : SOCK_DGRAM;

    if (getaddrinfo(addr, port_str, &hints, &infoptr) != 0) {
        return SOCK_ERR_RESOLVE;
    }

    for (p = infoptr; p != NULL; p = p->ai_next) {
        sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sock == -1) {
            continue;
        }

        ret = connect(sock, p->ai_addr, p->ai_addrlen);
        if (ret == -1) {
            sock_close(sock);
            continue;
        }

        break;
    }

    if (sock == -1) {
        freeaddrinfo(infoptr);
        return SOCK_ERR_CREATE;
    }

    if (ret == -1) {
        freeaddrinfo(infoptr);
        return SOCK_TIMEOUT;
    }

    sock_nonblock(sock);
    if (sock_select(sock, timout_ms, WRITE) <= 0) {
        sock_close(sock);
        freeaddrinfo(infoptr);
        return SOCK_TIMEOUT;
    }

    if (!sock_isvalid(sock)) {
        sock_close(sock);
        freeaddrinfo(infoptr);
        return SOCK_INVALID;
    }

    freeaddrinfo(infoptr);
    return sock;
}

int sock_setbufsize(int s, int send_size, int recv_size)
{
    int ret;
    socklen_t len = sizeof(send_size);

    if (send_size > 0) {
        ret = setsockopt(s, SOL_SOCKET, SO_SNDBUF, (char *)(&send_size), len);
        if (ret) {
            return SOCK_ERR_SET_SBUF;
        }
    }
    if (recv_size > 0) {
        ret = setsockopt(s, SOL_SOCKET, SO_RCVBUF, (char *)(&recv_size), len);
        if (ret) {
            return SOCK_ERR_SET_RBUF;
        }
    }

    return 0;
}

int sock_send(int s, const char *buf, int len, int timout_ms)
{
    int rc;
    int sent = 0;

    while (sent < len) {
        rc = sock_select(s, 10000, WRITE); // check if socket is connected

        if (rc == 0) {
            return SOCK_TIMEOUT;
        }

        if (rc == -1) {
            return SOCK_TIMEOUT;
        }

        if ((rc = send(s, buf + sent, len - sent, 0)) < 0) {
            if (errno != EWOULDBLOCK) {
                return SOCK_TIMEOUT;
            }
            rc = 0;
        }

        sent += rc;
    }
    return sent;
}

int sock_recv(int s, char *buf, int len, int timout_ms)
{
    int rc;

    if (sock_select(s, timout_ms, READ) <= 0) {
        return SOCK_TIMEOUT;
    }

    rc = recv(s, buf, len, 0);

    if (rc == -1 && errno == ECONNRESET) {
        return SOCK_ERR_RECV; // The other end has closed the connection with RST
    }
    if (rc == 0) {
        return SOCK_ERR_RECV; // The other end has closed the connection with FIN
    }

    return rc;
}

int sock_recvfrom(int s, char *buf, int len, sock_udp_conn_t *udp_conn, int timout_ms)
{
    int rc;

    if (sock_select(s, timout_ms, READ) <= 0) {
        return SOCK_TIMEOUT;
    }

    udp_conn->addr_len = sizeof(udp_conn->addr);
    rc = recvfrom(s, buf, len, 0, &udp_conn->addr, &udp_conn->addr_len);

    if (rc == -1) {
        return SOCK_ERR_RECV;
    }

    return rc;
}

int sock_sendto(int s, const char *buf, int len, sock_udp_conn_t *udp_conn, int timout_ms)
{
    int rc;
    int sent = 0;

    while (sent < len) {
        rc = sock_select(s, 10000, WRITE); // check if socket is connected

        if (rc == 0) {
            return SOCK_TIMEOUT;
        }

        if (rc == -1) {
            return SOCK_TIMEOUT;
        }

        if ((rc = sendto(s, buf + sent, len - sent, 0, &udp_conn->addr, udp_conn->addr_len)) < 0) {
            if (errno != EWOULDBLOCK) {
                return SOCK_TIMEOUT;
            }
            rc = 0;
        }

        sent += rc;
    }
    return sent;
}

int sock_nonblock(int s)
{
#ifndef WIN32
    long arg;

    arg = fcntl(s, F_GETFL);
    arg |= O_NONBLOCK;

    return fcntl(s, F_SETFL, arg);
#else
    unsigned long arg = 1;
    return ioctlsocket(s, FIONBIO, &arg);
#endif
}

int sock_block(int s)
{
#ifndef WIN32
    long arg;

    arg = fcntl(s, F_GETFL);
    arg &= ~O_NONBLOCK;

    return fcntl(s, F_SETFL, arg);
#else
    unsigned long arg = 0;
    return ioctlsocket(s, FIONBIO, &arg);
#endif
}

int sock_select(int s, int timout_ms, int mode)
{
    struct timeval tv;
    fd_set fd_wr, fd_rd;

    FD_ZERO(&fd_wr);
    FD_ZERO(&fd_rd);
    FD_SET(s, &fd_wr);
    FD_SET(s, &fd_rd);

    tv.tv_sec = timout_ms / 1000;
    tv.tv_usec = (timout_ms % 1000) * 1000;

    switch (mode) {
    case READ:
        return select(s + 1, &fd_rd, NULL, NULL, &tv);
        break;
    case WRITE:
        return select(s + 1, NULL, &fd_wr, NULL, &tv);
        break;
    case RW:
        return select(s + 1, &fd_rd, &fd_wr, NULL, &tv);
        break;
    default:
        return SOCK_NO_MODE;
    }
}

int sock_isvalid(int s)
{
    int optval;
    socklen_t len = sizeof(optval);

    getsockopt(s, SOL_SOCKET, SO_ERROR, (char *)(&optval), &len);

    if (optval) {
        return 0;
    }

    return 1;
}

int sock_listen(int port, int *listen_sock)
{
    int conn_sock;
    int val = 1;

    struct sockaddr_in servaddr;

    *listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (*listen_sock == -1) {
        return SOCK_ERR_CREATE;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);

    // Make sure the port can be re-used without waiting for the TIME_WAIT period is over
    //(https://stackoverflow.com/questions/3229860/what-is-the-meaning-of-so-reuseaddr-setsockopt-option-linux)
    setsockopt(*listen_sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&val, sizeof(int));

    if (bind(*listen_sock, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0) {
        return SOCK_ERR_BIND;
    }

    if ((listen(*listen_sock, 1)) != 0) {
        return SOCK_ERR_LISTEN;
    }

    conn_sock = accept(*listen_sock, NULL, NULL);

    if (conn_sock >= 0) {
        sock_close(*listen_sock);
    }

    return conn_sock;
}

void sock_close(int s)
{
#ifdef WIN32
    closesocket(s);
#else
    close(s);
#endif
}

int sock_get_errno(void)
{
    return errno;
}

// Sends RST instead of FIN
void sock_rst_close(int s)
{
    struct linger ling;
    ling.l_onoff = 1;
    ling.l_linger = 0;

    setsockopt(s, SOL_SOCKET, SO_LINGER, (const char *)&ling, sizeof(ling));

    sock_close(s);
}

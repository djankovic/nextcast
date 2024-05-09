// shoutcast functions for butt
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
#include <string.h>

#ifdef WIN32
#define usleep(us) Sleep(us / 1000)
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h> //defines IPPROTO_TCP on BSD
#include <netdb.h>
#endif

#include <errno.h>

#include "gettext.h"

#include "cfg.h"
#include "butt.h"
#include "timer.h"
#include "strfuncs.h"
#include "shoutcast.h"
#include "parseconfig.h"
#include "sockfuncs.h"
#include "flgui.h"
#include "fl_funcs.h"
#include "url.h"

int server_version = SC_VERSION_UNKNOWN;

void send_icy_header(char *key, char *val)
{
    char *icy_line;
    int len;

    if ((val == NULL) || (strlen(val) == 0)) {
        return;
    }

    len = strlen(key) + strlen(val) + strlen(":\r\n") + 1;
    icy_line = (char *)malloc(len * sizeof(char) + 1);
    snprintf(icy_line, len, "%s:%s\r\n", key, val);

    sock_send(stream_socket, icy_line, strlen(icy_line), SEND_TIMEOUT);

    free(icy_line);
}

int sc_connect(void)
{
    int ret;
    char recv_buf[100];
    char send_buf[100];
    static bool error_printed = 0;
    server_version = SC_VERSION_UNKNOWN;

    stream_socket = sock_connect(cfg.srv[cfg.selected_srv]->addr, cfg.srv[cfg.selected_srv]->port + 1, SOCK_PROTO_TCP, CONN_TIMEOUT);

    if (stream_socket < 0) {
        switch (stream_socket) {
        case SOCK_ERR_CREATE:
            if (!error_printed) {
                print_info(_("\nConnect: Could not create network socket"), 1);
            }
            if (cfg.main.force_reconnecting == 1) {
                error_printed = 1;
                ret = SC_RETRY;
            }
            else {
                ret = SC_ABORT;
            }
            break;
        case SOCK_ERR_RESOLVE:
            if (!error_printed) {
                print_info(_("\nConnect: Error resolving server address"), 1);
                error_printed = 1;
            }
            ret = SC_RETRY;
            break;
        case SOCK_TIMEOUT:
        case SOCK_INVALID:
            ret = SC_RETRY;
            break;
        default:
            if (cfg.main.force_reconnecting == 1) {
                ret = SC_RETRY;
            }
            else {
                ret = SC_ABORT;
            }
        }

        return ret;
    }

    snprintf(send_buf, sizeof(send_buf), "%s%s", cfg.srv[cfg.selected_srv]->pwd, "\r\n");
    sock_send(stream_socket, send_buf, strlen(send_buf), SEND_TIMEOUT);

    // Make butt compatible to proxies/load balancers. Thanks to boyska
    if (cfg.srv[cfg.selected_srv]->port == 80) {
        snprintf(send_buf, sizeof(send_buf), "Host: %s\r\n", cfg.srv[cfg.selected_srv]->addr);
    }
    else {
        snprintf(send_buf, sizeof(send_buf), "Host: %s:%d\r\n", cfg.srv[cfg.selected_srv]->addr, cfg.srv[cfg.selected_srv]->port);
    }
    sock_send(stream_socket, send_buf, strlen(send_buf), SEND_TIMEOUT);

    if (cfg.main.num_of_icy > 0) {
        char *icy_name = strdup(cfg.icy[cfg.selected_icy]->name);
        if (cfg.icy[cfg.selected_icy]->expand_variables == 1) {
            expand_string(&icy_name);
            strrpl(&icy_name, (char *)"%N", cfg.srv[cfg.selected_srv]->name, MODE_ALL);
        }
        snprintf(send_buf + strlen(send_buf), sizeof(send_buf) - strlen(send_buf), "ice-name: %s\r\n", icy_name);
        send_icy_header((char *)"icy-name", icy_name);
        send_icy_header((char *)"icy-genre", cfg.icy[cfg.selected_icy]->genre);
        send_icy_header((char *)"icy-url", cfg.icy[cfg.selected_icy]->url);
        send_icy_header((char *)"icy-irc", cfg.icy[cfg.selected_icy]->irc);
        send_icy_header((char *)"icy-icq", cfg.icy[cfg.selected_icy]->icq);
        send_icy_header((char *)"icy-aim", cfg.icy[cfg.selected_icy]->aim);
        send_icy_header((char *)"icy-pub", cfg.icy[cfg.selected_icy]->pub);
        free(icy_name);
    }
    else {
        send_icy_header((char *)"icy-name", (char *)"No Name");
        send_icy_header((char *)"icy-pub", (char *)"0");
    }

    snprintf(send_buf, sizeof(send_buf), "%u", cfg.audio.bitrate);
    send_icy_header((char *)"icy-br", send_buf);

    sock_send(stream_socket, "content-type:", 13, SEND_TIMEOUT);

    if (!strcmp(cfg.audio.codec, "mp3")) {
        strcpy(send_buf, "audio/mpeg");
    }
    else if (!strcmp(cfg.audio.codec, "aac")) {
        strcpy(send_buf, "audio/aac");
    }
    else {
        strcpy(send_buf, "audio/ogg");
    }

    sock_send(stream_socket, send_buf, strlen(send_buf), SEND_TIMEOUT);

    sock_send(stream_socket, "\r\n\r\n", 4, SEND_TIMEOUT);

    if ((ret = sock_recv(stream_socket, recv_buf, sizeof(recv_buf) - 1, 5 * RECV_TIMEOUT)) == 0) {
        usleep(100 * 1000);
        sc_disconnect();
        return SC_RETRY;
    }

    if (ret == SOCK_TIMEOUT) {
        print_info(_("\nconnect: connection timed out. Trying again...\n"), 1);
        usleep(100 * 1000);
        sc_disconnect();
        return SC_RETRY;
    }

    if (ret < 0) {
        usleep(100 * 1000);
        sc_disconnect();
        return SC_RETRY;
    }

    recv_buf[ret] = '\0';

    if ((recv_buf[0] != 'O') || (recv_buf[1] != 'K') || (ret <= 2)) {
        if (strstr(strtolower(recv_buf), "invalid password") != NULL) {
            if (!error_printed) {
                print_info(_("\nConnect: Invalid password!\n"), 1);
            }
            sc_disconnect();
            if (cfg.main.force_reconnecting == 1) {
                error_printed = 1;
                return SC_RETRY;
            }
            else {
                return SC_ABORT;
            }
        }

        sc_disconnect();
        return SC_RETRY;
    }

    connected = 1;
    error_printed = 0;

    timer_init(&stream_timer, 1); // starts the "online" timer
    timer_start(&stream_timer);

    return SC_OK;
}

int sc_send(char *buf, int buf_len)
{
    int ret;
    ret = sock_send(stream_socket, buf, buf_len, SEND_TIMEOUT);

    if (ret == SOCK_TIMEOUT) {
        ret = -1;
    }

    return ret;
}

int sc_update_song(char *song_name)
{
    int ret;
    int web_socket;
    char send_buf[1024];
    char *song_buf;

    web_socket = sock_connect(cfg.srv[cfg.selected_srv]->addr, cfg.srv[cfg.selected_srv]->port, SOCK_PROTO_TCP, CONN_TIMEOUT);

    if (web_socket < 0) {
        switch (web_socket) {
        case SOCK_ERR_CREATE:
            print_info(_("\nUpdate song: Could not create network socket"), 1);
            ret = SC_ABORT;
            break;
        case SOCK_ERR_RESOLVE:
            print_info(_("\nUpdate song: Error resolving server address"), 1);
            ret = SC_ABORT;
            break;
        case SOCK_TIMEOUT:
        case SOCK_INVALID:
            ret = SC_RETRY;
            break;
        default:
            ret = SC_ABORT;
        }

        return ret;
    }

    song_buf = strdup(song_name);
    strencoderfc3986(&song_buf);

    snprintf(send_buf, sizeof(send_buf),
             "GET /admin.cgi?pass=%s&mode=updinfo&song=%s&url= HTTP/1.0\r\n"
             "User-Agent: ShoutcastDSP (Mozilla Compatible)\r\n",
             cfg.srv[cfg.selected_srv]->pwd, song_buf);

    sock_send(web_socket, send_buf, strlen(send_buf), SEND_TIMEOUT);

    if (cfg.srv[cfg.selected_srv]->port == 80) {
        snprintf(send_buf, sizeof(send_buf), "Host: %s\r\n\r\n", cfg.srv[cfg.selected_srv]->addr);
    }
    else {
        snprintf(send_buf, sizeof(send_buf), "Host: %s:%d\r\n\r\n", cfg.srv[cfg.selected_srv]->addr, cfg.srv[cfg.selected_srv]->port);
    }

    sock_send(web_socket, send_buf, strlen(send_buf), SEND_TIMEOUT);

    sock_close(web_socket);
    free(song_buf);

    return SC_OK;
}

void sc_disconnect(void)
{
    sock_close(stream_socket);
}

int sc_get_listener_count(void)
{
    uint32_t data_size;
    char *sid;
    char *listeners;
    char *listeners_start;
    char *listeners_end;
    char url[256];
    char proto[8];
    char recv_buf[100 * 1024];

    strncpy(proto, "http", sizeof(proto));

    if (server_version == SC_VERSION_2 || server_version == SC_VERSION_UNKNOWN) {
        sid = strstr(cfg.srv[cfg.selected_srv]->pwd, ":#");
        if (sid != NULL) {
            sid += 2;
            snprintf(url, sizeof(url), "%s://%s:%d/stats?sid=%s", proto, cfg.srv[cfg.selected_srv]->addr, cfg.srv[cfg.selected_srv]->port, sid);
        }
        else {
            snprintf(url, sizeof(url), "%s://%s:%d/stats?sid=1", proto, cfg.srv[cfg.selected_srv]->addr, cfg.srv[cfg.selected_srv]->port);
        }

        data_size = url_get_listener_count(url, recv_buf, sizeof(recv_buf));

        if (data_size > 0) {
            listeners_start = strstr(strtoupper(recv_buf), "<CURRENTLISTENERS>");
            listeners_end = strstr(strtoupper(recv_buf), "</CURRENTLISTENERS>");
            if (listeners_start != NULL && listeners_end != NULL) {
                listeners = listeners_start + strlen("<CURRENTLISTENERS>");
                *listeners_end = '\0';
                server_version = SC_VERSION_2;
                return atoi(listeners);
            }
        }
    }

    if (server_version == SC_VERSION_1 || server_version == SC_VERSION_UNKNOWN) {
        snprintf(url, sizeof(url), "%s://%s:%d/7.html", proto, cfg.srv[cfg.selected_srv]->addr, cfg.srv[cfg.selected_srv]->port);

        data_size = url_get_listener_count(url, recv_buf, sizeof(recv_buf));

        // <HTML><meta http-equiv="Pragma" content="no-cache"></head><body>0,1,1,25,0,128,</body></html>
        // Numbers in comma list mean:
        // current_listeners,server_online,peak_listers,max_allowed_isteners,uniq,bitrate,songname
        if (data_size > 0) {
            listeners_start = strstr(strtolower(recv_buf), "<body>");
            listeners_end = strchr(listeners_start, ',');
            if (listeners_start != NULL && listeners_end != NULL) {
                listeners = listeners_start + strlen("<body>");
                *listeners_end = '\0';
                server_version = SC_VERSION_1;
                return atoi(listeners);
            }
        }
    }

    return -1;
}

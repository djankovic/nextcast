// command functions for butt
//
// Copyright 2007-2020 by Daniel Noethen.
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

#ifndef BUILD_CLIENT
#include <pthread.h>
#endif

#ifdef WIN32
#include <winsock2.h>
#define usleep(us) Sleep(us / 1000)
#ifndef errno
#define errno WSAGetLastError()
#endif
#ifndef EWOULDBLOCK
#define EWOULDBLOCK WSAEWOULDBLOCK
#endif
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h> //defines IPPROTO_TCP on BSD
#include <netdb.h>
#include <sys/select.h>
#include <errno.h>
#endif

#ifdef WIN32
typedef int socklen_t;
#endif

#include "command.h"
#include "sockfuncs.h"

#ifndef BUILD_CLIENT
#include "timer.h"
#include "ringbuffer.h"
#endif

int client_sock;
sock_proto_t server_proto;

#ifndef BUILD_CLIENT

pthread_t listen_thread_detached;
int listen_sock = -1;
int conn_sock;
int connection_established;
sock_udp_conn_t udp_conn;
ringbuf_t command_fifo;

void *listen_thread_func(void *data)
{
    timer_ms_t status_timer;
    timer_ms_t connection_timer;
    command_t command;
    int bytes_count;
    struct sockaddr_in cli;
    socklen_t len;
    len = sizeof(cli);
    char recv_buf[1024];
    int response = 0;

    rb_init(&command_fifo, COMMAND_FIFO_LEN * sizeof(command_t));

    timer_init(&status_timer, 0.8); // Use 0.8 s instead of 1.0 s to allow small time fluctations
    timer_init(&connection_timer, 5);
    connection_established = 0;

    for (;;) {
        //   printf("num of commands in fifo: %d\n", rb_filled(&command_fifo)/sizeof(command_t));
        if (connection_established == 1 ||
            rb_space(&command_fifo) < (int)sizeof(command_t)) { // Do not accept new connections while a connection is established or the command fifo is full
            // To prevent a deadlock reset the connection in case it stays in "connection_established = 1" for // more then 5 seconds
            if (timer_is_elapsed(&connection_timer) == 1) {
                if (server_proto == SOCK_PROTO_TCP) {
                    sock_close(conn_sock);
                }
                rb_clear(&command_fifo);
                connection_established = 0;
            }
            usleep(100 * 1000);
            //      printf("continued\n");
            continue;
        }
        if (server_proto == SOCK_PROTO_TCP) {
            conn_sock = accept(listen_sock, (struct sockaddr *)&cli, &len);
            if (conn_sock < 0) { // conn_sock < 0 if error
                usleep(100 * 1000);
                continue;
            }
        }
        else {
            conn_sock = listen_sock;
        }
        sock_nonblock(conn_sock);

        if (server_proto == SOCK_PROTO_TCP) {
            bytes_count = sock_recv(conn_sock, recv_buf, sizeof(recv_buf), COMMAND_TIMEOUT);
        }
        else {
            bytes_count = sock_recvfrom(conn_sock, recv_buf, sizeof(recv_buf), &udp_conn, COMMAND_TIMEOUT);
        }

        if (bytes_count == SOCK_TIMEOUT) {
            if (server_proto == SOCK_PROTO_TCP) {
                sock_close(conn_sock);
            }
            connection_established = 0;
            continue;
        }

        connection_established = 1;
        timer_start(&connection_timer);

        if (bytes_count > 0) {
            memcpy((char *)&command, recv_buf, COMMAND_PACKET_SIZE);
            if (command.param_size > 0) {
                // Limit parameter size to COMMAND_MAX_PARAM_SIZE
                command.param_size = command.param_size > COMMAND_MAX_PARAM_SIZE ? COMMAND_MAX_PARAM_SIZE : command.param_size;

                int expected_bytes = COMMAND_PACKET_SIZE + command.param_size;
                if (bytes_count < expected_bytes) {
                    int remaining_bytes = expected_bytes - bytes_count;
                    bytes_count += sock_recv(conn_sock, recv_buf + bytes_count, remaining_bytes, COMMAND_TIMEOUT);
                    if (bytes_count != expected_bytes) {
                        if (server_proto == SOCK_PROTO_TCP) {
                            sock_close(conn_sock);
                        }
                        connection_established = 0;
                        continue; // Still not all bytes received -> dismiss this command
                    }
                }
                command.param = (void *)calloc(command.param_size + 1, sizeof(uint8_t));
                memcpy((char *)command.param, recv_buf + COMMAND_PACKET_SIZE, command.param_size);
            }
            if (command.cmd == CMD_GET_STATUS) {
                if (status_timer.is_running == false) {
                    timer_start(&status_timer);
                }
                else if (timer_is_elapsed(&status_timer) == 0) { // Limit status request to approx. one per second
                    if (server_proto == SOCK_PROTO_TCP) {
                        sock_close(conn_sock);
                    }
                    else {
                        response = SOCK_ERR_RECV;
                        sock_sendto(conn_sock, (char *)&response, sizeof(response), &udp_conn, SEND_TIMEOUT);
                    }
                    connection_established = 0;
                    continue;
                }
            }
            else { // (command.cmd != CMD_GET_STATUS)
                response = 0;
                if (server_proto == SOCK_PROTO_TCP) {
                    sock_send(conn_sock, (char *)&response, sizeof(response), SEND_TIMEOUT);
                    sock_close(conn_sock);
                    connection_established = 0;
                }
                else {
                    sock_sendto(conn_sock, (char *)&response, sizeof(response), &udp_conn, SEND_TIMEOUT);
                    connection_established = 0;
                }
            }

            command_add_cmd_to_fifo(command);
        }
    }
    
    // Detach thread (free ressources) because no one will call pthread_join() on it
    pthread_detach(pthread_self());

    return NULL;
}

int command_add_cmd_to_fifo(command_t new_cmd)
{
    return rb_write(&command_fifo, (char *)&new_cmd, sizeof(command_t));
}

int command_get_cmd_from_fifo(command_t *cmd)
{
    if (rb_filled(&command_fifo) < (int)sizeof(command_t)) {
        return CMD_ERR_FIFO_EMPTY;
    }

    return rb_read_len(&command_fifo, (char *)cmd, sizeof(command_t));
}

int command_start_server(int port, int search_port, int mode, sock_proto_t proto)
{
#ifdef WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    struct sockaddr_in servaddr;

    listen_sock = socket(AF_INET, proto == SOCK_PROTO_TCP ? SOCK_STREAM : SOCK_DGRAM, 0);
    if (listen_sock == -1) {
        return SOCK_ERR_CREATE;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;

    if (mode == SERVER_MODE_LOCAL) {
        servaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    }
    else { // SERVER_MODE_ALL (accessable from the network)
        servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    }

    int val = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&val, sizeof(int));

    int bind_succeeded = 0;
    int p;
    for (p = port; p < port + 10; p++) {
        servaddr.sin_port = htons(p);

        if (bind(listen_sock, (struct sockaddr *)&servaddr, sizeof(servaddr)) == 0) {
            bind_succeeded = 1;
            break;
        }

        if (search_port == 0) {
            break;
        }
    }

    if (bind_succeeded == 0) {
        return SOCK_ERR_BIND;
    }

    if (proto == SOCK_PROTO_TCP) {
        if ((listen(listen_sock, 1)) != 0) {
            return SOCK_ERR_LISTEN;
        }
    }

    if (pthread_create(&listen_thread_detached, NULL, listen_thread_func, NULL) != 0) {
        return 0;
    }

    server_proto = proto;

    return p;
}

void command_send_status_reply(status_packet_t *status_packet)
{
    if (server_proto == SOCK_PROTO_TCP) {
        sock_send(conn_sock, (char *)status_packet, STATUS_PACKET_SIZE, COMMAND_TIMEOUT);
        sock_send(conn_sock, status_packet->song, status_packet->song_len, COMMAND_TIMEOUT);
        sock_send(conn_sock, status_packet->rec_path, status_packet->rec_path_len, COMMAND_TIMEOUT);
        sock_close(conn_sock);
        connection_established = 0;
    }
    else {
        char *udp_buf = (char *)malloc(STATUS_PACKET_SIZE + status_packet->song_len + status_packet->rec_path_len);
        memcpy(udp_buf, status_packet, STATUS_PACKET_SIZE);
        memcpy(udp_buf + STATUS_PACKET_SIZE, status_packet->song, status_packet->song_len);
        memcpy(udp_buf + STATUS_PACKET_SIZE + status_packet->song_len, status_packet->rec_path, status_packet->rec_path_len);
        sock_sendto(conn_sock, udp_buf, STATUS_PACKET_SIZE + status_packet->song_len + status_packet->rec_path_len, &udp_conn, COMMAND_TIMEOUT);
        connection_established = 0;
        free(udp_buf);
    }
}

void command_stop_server(void)
{
    if (listen_sock != -1) {
        sock_close(listen_sock);
    }
}
#endif // ifndef BUILD_CLIENT

int command_send_cmd(command_t command, char *addr, int port, sock_proto_t proto)
{
    int ret;
    int server_response;
    char send_buf[1024];

    client_sock = sock_connect(addr, port, proto, COMMAND_TIMEOUT);
    if (client_sock < 0) {
        return CMD_ERR_CONNECT;
    }

    memcpy(send_buf, &command, COMMAND_PACKET_SIZE);
    memcpy(send_buf + COMMAND_PACKET_SIZE, (char *)command.param, command.param_size);

    ret = sock_send(client_sock, send_buf, COMMAND_PACKET_SIZE + command.param_size, COMMAND_TIMEOUT);
    if (ret < 0) {
        if (proto == SOCK_PROTO_TCP) {
            sock_close(client_sock);
        }
        return CMD_ERR_SEND_CMD;
    }

    if (command.cmd != CMD_GET_STATUS) {
        ret = sock_recv(client_sock, (char *)&server_response, sizeof(server_response), COMMAND_TIMEOUT);
        if (ret < 0) {
            if (proto == SOCK_PROTO_TCP) {
                sock_close(client_sock);
            }
            return CMD_ERR_RECV_RESPONSE;
        }
        if (proto == SOCK_PROTO_TCP) {
            sock_close(client_sock);
        }
        return server_response;
    }

    return 0;
}

int command_recv_status_reply(status_packet_t *status_packet, sock_proto_t proto)
{
    int ret;
    uint32_t is_extended_packet;

    if (proto == SOCK_PROTO_TCP) {
        // Receive 32 bit status register
        ret = sock_recv(client_sock, (char *)&status_packet->status, sizeof(status_packet->status), COMMAND_TIMEOUT);
        if (ret < 0) {
            sock_close(client_sock);
            return ret;
        }

        // Check if status is returned by an old server that returns only the bit status register
        is_extended_packet = (status_packet->status & (1 << STATUS_EXTENDED_PACKET)) > 0;
        if (is_extended_packet == 0) {
            sock_close(client_sock);
            return sizeof(status_packet->status);
        }

        // Receive version and check status protocol version
        ret = sock_recv(client_sock, (char *)&status_packet->version, sizeof(status_packet->version), COMMAND_TIMEOUT);
        if (ret < 0) {
            sock_close(client_sock);
            return ret;
        }
        if (status_packet->version != STATUS_PACKET_VERSION) {
            sock_close(client_sock);
            return 0; // Version missmatch
        }

        // Receive remaining fixed len status variables
        ret = sock_recv(client_sock, ((char *)status_packet) + sizeof(status_packet->status) + sizeof(status_packet->version),
                        STATUS_PACKET_SIZE - sizeof(status_packet->status) - sizeof(status_packet->version), COMMAND_TIMEOUT);
        if (ret < 0) {
            sock_close(client_sock);
            return ret;
        }

        // Receive song title
        status_packet->song = (char *)malloc(status_packet->song_len); // song_len includes trailing '\0'
        ret = sock_recv(client_sock, status_packet->song, status_packet->song_len, COMMAND_TIMEOUT);
        if (ret < 0) {
            sock_close(client_sock);
            return ret;
        }

        // Receive file record path
        status_packet->rec_path = (char *)malloc(status_packet->rec_path_len);
        ret = sock_recv(client_sock, status_packet->rec_path, status_packet->rec_path_len, COMMAND_TIMEOUT);
        if (ret < 0) {
            sock_close(client_sock);
            return ret;
        }

        sock_close(client_sock);
    }
    else {
        char *udp_buf = (char *)malloc(SOCK_MAX_DATAGRAM_SIZE);

        ret = sock_recv(client_sock, udp_buf, SOCK_MAX_DATAGRAM_SIZE, COMMAND_TIMEOUT);
        if (ret < 0) {
            free(udp_buf);
            return CMD_ERR_RECV_STATUS;
        }

        if (ret < (int)STATUS_PACKET_SIZE) {
            int response = *((int *)udp_buf);
            free(udp_buf);
            return response;
        }

        memcpy((char *)status_packet, udp_buf, STATUS_PACKET_SIZE);
        if (status_packet->version != STATUS_PACKET_VERSION) {
            free(udp_buf);
            return 0; // Version missmatch
        }

        // Copy song title
        status_packet->song = (char *)malloc(status_packet->song_len); // song_len includes trailing '\0'
        memcpy(status_packet->song, udp_buf + STATUS_PACKET_SIZE, status_packet->song_len);

        // Copy file record path
        status_packet->rec_path = (char *)malloc(status_packet->rec_path_len);
        memcpy(status_packet->rec_path, udp_buf + STATUS_PACKET_SIZE + status_packet->song_len, status_packet->rec_path_len);

        free(udp_buf);
    }

    return STATUS_PACKET_SIZE;
}

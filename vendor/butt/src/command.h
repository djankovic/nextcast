#ifndef COMMAND_H
#define COMMAND_H

#include <stdint.h>
#include "sockfuncs.h"

#define COMMAND_TIMEOUT        2000
#define COMMAND_FIFO_LEN       32
#define COMMAND_PACKET_SIZE    (sizeof(command_t) - sizeof(void *))
#define COMMAND_MAX_PARAM_SIZE 1000

#define DEFAULT_PORT 1256

#define STATUS_CONNECTED        0
#define STATUS_CONNECTING       1
#define STATUS_RECORDING        2
#define STATUS_SIGNAL_DETECTED  3
#define STATUS_SILENCE_DETECTED 4
#define STATUS_EXTENDED_PACKET  31
#define STATUS_PACKET_VERSION   3
#define STATUS_PACKET_SIZE      (sizeof(status_packet_t) - 2 * sizeof(char *))

enum {
    CMD_EMPTY = 0,
    CMD_CONNECT = 1,
    CMD_DISCONNECT = 2,
    CMD_START_RECORDING = 3,
    CMD_STOP_RECORDING = 4,
    CMD_GET_STATUS = 5,
    CMD_SPLIT_RECORDING = 6,
    CMD_QUIT = 7,
    CMD_UPDATE_SONGNAME = 8,
    CMD_SET_STREAM_SIGNAL_THRESHOLD = 9,
    CMD_SET_STREAM_SILENCE_THRESHOLD = 10,
    CMD_SET_RECORD_SIGNAL_THRESHOLD = 11,
    CMD_SET_RECORD_SILENCE_THRESHOLD = 12,
};

enum {
    CMD_ERR_CONNECT = -1,
    CMD_ERR_SEND_CMD = -2,
    CMD_ERR_RECV_RESPONSE = -3,
    CMD_ERR_RECV_STATUS = -4,
    CMD_ERR_FIFO_EMPTY = -5,
    CMD_ERR_FIFO_FULL = -6,
};

enum {
    SERVER_MODE_OFF = 0,
    SERVER_MODE_LOCAL = 1,
    SERVER_MODE_ALL = 2,
};

typedef struct command {
    uint32_t cmd;
    uint32_t param_size;
    uint8_t padding[8];
    void *param;
} __attribute__((packed)) command_t;

typedef struct status_packet {
    uint32_t status;
    uint16_t version;
    int16_t volume_left;
    int16_t volume_right;
    uint32_t stream_seconds;
    uint32_t stream_kByte;
    uint32_t record_seconds;
    uint32_t record_kByte;
    uint16_t song_len;
    uint16_t rec_path_len;
    int32_t listener_count;
    char *song;
    char *rec_path;
} __attribute__((packed)) status_packet_t;

int command_start_server(int port, int search_port, int mode, sock_proto_t proto);
int command_add_cmd_to_fifo(command_t new_cmd);
int command_get_cmd_from_fifo(command_t *cmd);
int command_send_cmd(command_t command, char *addr, int port, sock_proto_t proto);
void command_get_last_cmd(command_t *command);
void command_send_status_reply(status_packet_t *status_packet);
int command_recv_status_reply(status_packet_t *status_packet, sock_proto_t proto);
void command_stop_server(void);

#endif

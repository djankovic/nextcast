//
//  port_midi.hpp
//  butt
//
//  Created by Daniel Nöthen on 03.02.24.
//  Copyright © 2024 Daniel Nöthen. All rights reserved.
//

#ifndef MIDI_H
#define MIDI_H

#include "portmidi.h"
#include "porttime.h"

#define MIDI_MAX_DEVICES (256)

typedef struct {
    char *name;
    int dev_id;
} midi_dev_t;

enum {
    MIDI_MSG_TYPE_NOTE_OFF = 0x80,
    MIDI_MSG_TYPE_NOTE_ON = 0x90,
    MIDI_MSG_TYPE_POLY_PRESSURE = 0xA0,
    MIDI_MSG_TYPE_CC = 0xB0,
    MIDI_MSG_TYPE_PC = 0xC0,
    MIDI_MSG_TYPE_CHANNEL_PRESSURE = 0xD0,
    MIDI_MSG_TYPE_PITCH_BEND = 0xE0,
};

enum {
    MIDI_ANY_CHANNEL = 0,
};

enum {
    MIDI_MODE_ABSOLUTE = 0,
    MIDI_MODE_RELATIVE_TWOS_COMPLEMENT = 1,
    MIDI_MODE_RELATIVE_BINARY_OFFSET = 2,
    MIDI_MODE_RELATIVE_SIGN_MAGNITUDE = 3,
};

enum {
    MIDI_CMD_START_STOP_BROADCASTING = 0,
    MIDI_CMD_START_STOP_RECORDING = 1,
    MIDI_CMD_MASTER_GAIN = 2,
    MIDI_CMD_STREAMING_GAIN = 3,
    MIDI_CMD_RECORDING_GAIN = 4,
    MIDI_CMD_PRIMARY_DEV_VOL = 5,
    MIDI_CMD_SECONDARY_DEV_VOL = 6,
    MIDI_CMD_CROSS_FADER = 7,
    MIDI_CMD_PRIMARY_DEV_MUTE_UNMUTE = 8,
    MIDI_CMD_SECONDARY_DEV_MUTE_UNMUTE = 9,
};

enum {
    MIDI_CTRL_TYPE_KNOB = 0,
    MIDI_CTRL_TYPE_BUTTON = 1,
};

PmError midi_init(void);
int midi_open_device(int device_num);
int midi_open_device(char *device_name);
int midi_start_polling(void);
int midi_stop_polling(void);
void midi_set_learning(int is_active);
midi_dev_t **midi_get_devices(int *dev_count);
int midi_get_dev_num_by_name(midi_dev_t **dev_list, char *dev_name);
void midi_free_device_list(midi_dev_t **dev_list);
void midi_close(void);
void midi_terminate(void);

#endif // MIDI_H

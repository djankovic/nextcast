//
//  port_midi.cpp
//  butt
//
//  Created by Daniel Nöthen on 03.02.24.
//  Copyright © 2024 Daniel Nöthen. All rights reserved.
//

#include <stdio.h>
#include <pthread.h>

#include "port_midi.h"
#include "atom.h"
#include "strfuncs.h"
#include "fl_funcs.h"
#include "fl_timer_funcs.h"
#include "flgui.h"

#define INPUT_BUFFER_SIZE 100
#define TIME_PROC         ((int32_t(*)(void *))Pt_Time)
#define TIME_INFO         NULL

PortMidiStream *midi_stream;
pthread_t midi_poll_thread_joinable;
ATOM_NEW_INT(stop_midi_thread);
ATOM_NEW_INT(learning_active);
ATOM_NEW_INT(polling_is_active);
int is_initialized = 0;

int get_dev_list_elem_count(midi_dev_t **dev_list)
{
    if (dev_list == NULL) {
        return 0;
    }

    int elem_count = 0;
    while (dev_list[elem_count] != NULL) {
        elem_count++;
    }

    return elem_count;
}

void *midi_poll_thread_func(void *data)
{
    int LED_counter;
    PmEvent buffer[INPUT_BUFFER_SIZE];
    atom_set_int(&stop_midi_thread, 0);
    atom_set_int(&polling_is_active, 1);

    // Pm_SetChannelMask(midi_stream, Pm_Channel(0));

    // Main loop to listen for MIDI events
    while (atom_get_int(&stop_midi_thread) == 0) {
        int numEvents = Pm_Read(midi_stream, buffer, INPUT_BUFFER_SIZE);
        if (numEvents > 0) {
            LED_counter = 0;
            Fl::lock();
            fl_g->LED_midi_command->set_state(LED::LED_ON);
            Fl::unlock();
            Fl::awake();

            for (int i = 0; i < numEvents; i++) {
                PmMessage message = buffer[i].message;
                uint8_t channel = (Pm_MessageStatus(message) & 0x0F) + 1;
                uint8_t msg_type = Pm_MessageStatus(message) & 0xF0;
                uint8_t msg_num = Pm_MessageData1(message);
                uint8_t val = Pm_MessageData2(message);

                if (atom_get_int(&learning_active) == 1) {
                    if (msg_type != MIDI_MSG_TYPE_CC) { // Allow only CC messages for now
                        break;
                    }
                    Fl::lock();
                    int command = fl_g->browser_midi_command->value() - 1;
                    cfg.midi.commands[command].channel = channel;
                    cfg.midi.commands[command].msg_type = msg_type;
                    cfg.midi.commands[command].msg_num = msg_num;
                    fl_g->choice_midi_channel->value(channel);
                    fl_g->input_midi_msg_num->value(msg_num);
                    fl_g->button_midi_learn->value(0);
                    fl_g->button_midi_learn->do_callback();
                    Fl::unlock();
                    midi_set_learning(0);
                    break;
                }

                for (int j = 0; j < MIDI_COMMANDS_COUNT; j++) {
                    if ((cfg.midi.commands[j].channel == MIDI_ANY_CHANNEL || cfg.midi.commands[j].channel == channel) && //
                        cfg.midi.commands[j].msg_type == msg_type &&                                                     //
                        cfg.midi.commands[j].msg_num == msg_num &&                                                       //
                        cfg.midi.commands[j].enabled == 1) {
                        exec_midi_command(j, val);
                    }
                }

                /*  if (type == MIDI_MSG_TYPE_CC) {
                      Fl::lock();

                      double min = fl_g->slider_gain->minimum();
                      double max = fl_g->slider_gain->maximum();
                      double new_val = min + val * (max-min) / 127;
                      fl_g->slider_gain->value(new_val);
                      fl_g->slider_gain->do_callback();
                      printf("Channel: %u, Type: %u, CC: %u, Val: %u\n", channel, type, msg_num, val);
                      Fl::unlock();
                  }*/
            }
        }
        else if (LED_counter++ >= 10) {
            LED_counter = 0;
            Fl::lock();
            fl_g->LED_midi_command->set_state(LED::LED_OFF);
            Fl::unlock();
            Fl::awake();
        }

        Pt_Sleep(10);
    }

    // Cleanup
    Pm_Close(midi_stream);

    atom_set_int(&polling_is_active, 0);
    return NULL;
}

PmError midi_init(void)
{
    PmError res = Pm_Initialize();
    if (res != pmNoError) {
        return res;
    }

    is_initialized = 1;
    atom_set_int(&polling_is_active, 0);
    return res;
}

midi_dev_t **midi_get_devices(int *dev_count)
{
    if (is_initialized == 0) {
        print_info(_("Error: PortMidi was not initialized."), 1);
        return NULL;
    }

    int available_devices;
    int dev_num = 0;
    PmDeviceInfo *device_info;
    char info_buf[256];

    available_devices = Pm_CountDevices();
    if (available_devices <= 0) {
        *dev_count = 0;
        return NULL;
    }

    midi_dev_t **dev_list;
    dev_list = (midi_dev_t **)malloc(MIDI_MAX_DEVICES * sizeof(midi_dev_t *));

    for (int i = 0; i < MIDI_MAX_DEVICES; i++) {
        dev_list[i] = (midi_dev_t *)malloc(sizeof(midi_dev_t));
    }

    for (int i = 0; i < available_devices && i < MIDI_MAX_DEVICES - 1; i++) {
        device_info = (PmDeviceInfo *)Pm_GetDeviceInfo(i);
        if (device_info == NULL) {
            snprintf(info_buf, sizeof(info_buf), _("Error getting device Info (%d)"), i);
            print_info(info_buf, 1);
            continue;
        }

        // Save only input devices
        if (device_info->input == 0) {
            continue;
        }

        dev_list[dev_num]->name = (char *)malloc(strlen(device_info->name) + strlen(device_info->interf) + 10);
        dev_list[dev_num]->dev_id = i;
        snprintf(dev_list[dev_num]->name, strlen(device_info->name) + strlen(device_info->interf) + 10, "%s [%s]", device_info->name, device_info->interf);

        // Replace characters from the device name that have a special meaning to FLTK
        strrpl(&dev_list[dev_num]->name, (char *)"\\", (char *)" ", MODE_ALL); // Must come first
        strrpl(&dev_list[dev_num]->name, (char *)"/", (char *)"\\/", MODE_ALL);
        strrpl(&dev_list[dev_num]->name, (char *)"|", (char *)" ", MODE_ALL);
        strrpl(&dev_list[dev_num]->name, (char *)"\t", (char *)" ", MODE_ALL);
        strrpl(&dev_list[dev_num]->name, (char *)"_", (char *)" ", MODE_ALL);
        strrpl(&dev_list[dev_num]->name, (char *)"&", (char *)"+", MODE_ALL);

        dev_num++;
    }

    dev_list[dev_num] = NULL;

    *dev_count = dev_num;
    return dev_list;
}

void midi_free_device_list(midi_dev_t **dev_list)
{
    if (is_initialized == 0) {
        print_info(_("Error: PortMidi was not initialized."), 1);
        return;
    }

    int dev_count = get_dev_list_elem_count(dev_list);
    if (dev_count == 0) {
        return;
    }

    for (int i = 0; i < MIDI_MAX_DEVICES; i++) {
        if (i < dev_count) {
            free(dev_list[i]->name);
        }

        free(dev_list[i]);
    }
    free(dev_list);
}

int midi_open_device(int device_id)
{
    if (is_initialized == 0) {
        print_info(_("Error: PortMidi was not initialized."), 1);
        return -1;
    }

    PmError result;
    Pt_Start(1, NULL, NULL);

    result = Pm_OpenInput(&midi_stream, device_id, NULL, INPUT_BUFFER_SIZE, NULL, NULL);
    if (result != pmNoError) {
        printf("device id: %d\n", device_id);
        printf("Error opening MIDI input: %s\n", Pm_GetErrorText(result));
        return -1;
    }

    return 0;
}

int midi_open_device(char *device_name)
{
    char info_buf[256];

    if (is_initialized == 0) {
        print_info(_("Error: PortMidi was not initialized."), 1);
        return -1;
    }

    int dev_count;
    midi_dev_t **dev_list = midi_get_devices(&dev_count);
    if (dev_list == NULL) {
        snprintf(info_buf, sizeof(info_buf), _("Error: MIDI device %s could not be opened because no MIDI devices were found."), device_name);
        print_info(info_buf, 1);
        return -1;
    }

    int dev_num = midi_get_dev_num_by_name(dev_list, device_name);
    if (dev_num == -1) {
        snprintf(info_buf, sizeof(info_buf), _("Error: MIDI device %s could not be found."), device_name);
        print_info(info_buf, 1);
        return -1;
    }

    int dev_id = dev_list[dev_num]->dev_id;

    return midi_open_device(dev_id);
}

int midi_start_polling(void)
{
    if (is_initialized == 0) {
        print_info(_("Error: PortMidi was not initialized."), 1);
        return -1;
    }
    if (atom_get_int(&polling_is_active) == 1) {
        print_info(_("Error: MIDI thread already active."), 1);
        return -1;
    }

    if (pthread_create(&midi_poll_thread_joinable, NULL, midi_poll_thread_func, NULL) != 0) {
        print_info(_("Error: Could not start MIDI thread."), 1);
        return -1;
    }

    return 0;
}

int midi_stop_polling(void)
{
    if (is_initialized == 0) {
        print_info(_("Error: PortMidi was not initialized."), 1);
        return -1;
    }

    if (atom_get_int(&polling_is_active) == 1) {
        atom_set_int(&stop_midi_thread, 1);
        pthread_join(midi_poll_thread_joinable, NULL);
    }

    return 0;
}

int midi_get_dev_num_by_name(midi_dev_t **dev_list, char *dev_name)
{
    if (is_initialized == 0) {
        print_info(_("Error: PortMidi was not initialized."), 1);
        return -1;
    }

    if (dev_list == NULL || dev_name == NULL) {
        return -1;
    }

    int dev_count = get_dev_list_elem_count(dev_list);
    if (dev_count == 0) {
        return -1;
    }

    for (int i = 0; i < dev_count; i++) {
        if (!strcmp(dev_name, dev_list[i]->name)) {
            return i;
        }
    }

    return -1; // Device not found
}

void midi_close(void)
{
    if (is_initialized == 0) {
        print_info(_("Error: PortMidi was not initialized."), 1);
        return;
    }

    midi_stop_polling();
    Pm_Close(midi_stream);
}

void midi_terminate(void)
{
    if (is_initialized == 0) {
        print_info(_("Error: PortMidi was not initialized."), 1);
        return;
    }
    Pm_Terminate();
    is_initialized = 0;
}

void midi_set_learning(int is_active)
{
    atom_set_int(&learning_active, is_active);
}

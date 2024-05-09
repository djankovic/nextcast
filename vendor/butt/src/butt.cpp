// butt - broadcast using this tool
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
#include <unistd.h>

#include <signal.h>
#include <limits.h>
#ifdef WIN32
#include <time.h>
#define IDI_ICON 101
#endif

#ifndef BUILD_CLIENT
#include <lame/lame.h>
#include <pthread.h>
#if !defined(__APPLE__) && !defined(WIN32)
#include <FL/Fl_File_Icon.H>
#include <dlfcn.h>
#endif

#if defined(__APPLE__)
#include <libgen.h>      // for dirname
#include <mach-o/dyld.h> // for _NSGetExecutablePath
#include <dlfcn.h>
#include "AskForMicPermission.h"
#endif
#endif // #ifndef BUILD_CLIENT

#include "config.h"
#include "gettext.h"

#include "butt.h"
#include "command.h"
#include "sockfuncs.h"
#ifndef BUILD_CLIENT
#include "cfg.h"
#include "port_audio.h"
#include "lame_encode.h"
#include "opus_encode.h"
#include "flac_encode.h"
#include "shoutcast.h"
#include "parseconfig.h"
#include "vu_meter.h"
#include "util.h"
#include "flgui.h"
#include "fl_funcs.h"
#include "fl_timer_funcs.h"
#include "update.h"
#include "tray_agent.h"
#ifdef WITH_RADIOCO
#include "radioco.h"
#endif
#ifdef HAVE_LIBFDK_AAC
#include "aac_encode.h"
#endif

#include "port_midi.h"

bool recording;
bool connected;
bool streaming;
bool disconnect;

int stream_socket;
double kbytes_sent;
double kbytes_written;
unsigned int record_start_hour;

timer_ms_t rec_timer;
timer_ms_t stream_timer;

lame_enc lame_stream;
lame_enc lame_rec;
vorbis_enc vorbis_stream;
vorbis_enc vorbis_rec;
opus_enc opus_stream;
opus_enc opus_rec;
flac_enc flac_stream;
flac_enc flac_rec;

#ifdef HAVE_LIBFDK_AAC
aac_enc aac_stream;
aac_enc aac_rec;
aacEncOpenPtr aacEncOpen_butt;
aacEncoder_SetParamPtr aacEncoder_SetParam_butt;
aacEncoder_GetParamPtr aacEncoder_GetParam_butt;
aacEncEncodePtr aacEncEncode_butt;
aacEncInfoPtr aacEncInfo_butt;
aacEncClosePtr aacEncClose_butt;
#endif // ifdef HAVE_LIBFDK_AAC

#endif // ifndef BUILD_CLIENT

void print_usage(void)
{
#ifndef BUILD_CLIENT
    printf(
        "Usage: butt [-h | -v | -s [name] | -u <song name> | -M <streaming signal threshold> | -m <streaming silence threshold> | -O <recording signal threshold> | -o <recording silence threshold> | -SLdrtqn | -c <config_path>] [-A | -U | -x] [-a <addr>] [-p <port>]\n");
#else
    printf(
        "Usage: butt-client [-h | -v | -s [name] | -u <song name> | -M <streaming signal threshold> | -m <streaming silence threshold> | -O <recording signal threshold> | -o <recording silence threshold> | -USdrtqn ] [-a <addr>] [-p <port>]\n");
#endif
    fflush(stdout);
}

#ifndef BUILD_CLIENT
void load_AAC_lib(void)
{
#ifdef WIN32
    // Load aac library
    HMODULE hModule = LoadLibrary(TEXT("libfdk-aac-2.dll"));
    if (hModule != NULL) {
        aacEncOpen_butt = (aacEncOpenPtr)GetProcAddress(hModule, "aacEncOpen");
        aacEncoder_SetParam_butt = (aacEncoder_SetParamPtr)GetProcAddress(hModule, "aacEncoder_SetParam");
        aacEncoder_GetParam_butt = (aacEncoder_GetParamPtr)GetProcAddress(hModule, "aacEncoder_GetParam");
        aacEncEncode_butt = (aacEncEncodePtr)GetProcAddress(hModule, "aacEncEncode");
        aacEncInfo_butt = (aacEncInfoPtr)GetProcAddress(hModule, "aacEncInfo");
        aacEncClose_butt = (aacEncClosePtr)GetProcAddress(hModule, "aacEncClose");
        g_aac_lib_available = 1;
    }
#endif

#if defined(__APPLE__)
#ifdef HAVE_LIBFDK_AAC
    void *dylib = dlopen("libfdk-aac.2.dylib", RTLD_LAZY);

    if (dylib == NULL) {
        dylib = dlopen("/Library/Application Support/butt/libfdk-aac.2.dylib", RTLD_LAZY); //  New path since 0.1.34
    }

    if (dylib != NULL) {
        // typedef AACENC_ERROR(WINAPI *aacEncOpenPtr)(HANDLE_AACENCODER*, UINT, UINT);

        aacEncOpen_butt = (aacEncOpenPtr)dlsym(dylib, "aacEncOpen");
        aacEncoder_SetParam_butt = (aacEncoder_SetParamPtr)dlsym(dylib, "aacEncoder_SetParam");
        aacEncoder_GetParam_butt = (aacEncoder_GetParamPtr)dlsym(dylib, "aacEncoder_GetParam");
        aacEncEncode_butt = (aacEncEncodePtr)dlsym(dylib, "aacEncEncode");
        aacEncInfo_butt = (aacEncInfoPtr)dlsym(dylib, "aacEncInfo");
        aacEncClose_butt = (aacEncClosePtr)dlsym(dylib, "aacEncClose");

        g_aac_lib_available = 1;
    }

    //  askForMicPermission();
#endif
#endif

#if !defined(__APPLE__) && !defined(WIN32) // LINUX
#ifdef HAVE_LIBFDK_AAC
    void *dylib = dlopen("libfdk-aac.so", RTLD_LAZY);

    if (dylib == NULL) { // Try other lib name
        dylib = dlopen("libfdk-aac.so.2", RTLD_LAZY);
    }

    if (dylib != NULL) {
        // typedef AACENC_ERROR(WINAPI *aacEncOpenPtr)(HANDLE_AACENCODER*, UINT, UINT);

        aacEncOpen_butt = (aacEncOpenPtr)dlsym(dylib, "aacEncOpen");
        aacEncoder_SetParam_butt = (aacEncoder_SetParamPtr)dlsym(dylib, "aacEncoder_SetParam");
        aacEncoder_GetParam_butt = (aacEncoder_GetParamPtr)dlsym(dylib, "aacEncoder_GetParam");
        aacEncEncode_butt = (aacEncEncodePtr)dlsym(dylib, "aacEncEncode");
        aacEncInfo_butt = (aacEncInfoPtr)dlsym(dylib, "aacEncInfo");
        aacEncClose_butt = (aacEncClosePtr)dlsym(dylib, "aacEncClose");

        g_aac_lib_available = 1;
    }
#endif
#endif
}

int read_cfg(void)
{
    char *p;
    int shift_pressed;

    if (Fl::get_key(FL_Shift_L) || Fl::get_key(FL_Shift_R)) {
        shift_pressed = 1;
    }
    else {
        shift_pressed = 0;
    }

    // Prepare configuration loading
#ifdef WIN32
    if ((cfg_path == NULL) || (shift_pressed == 1)) {
        if ((p = fl_getenv("APPDATA")) == NULL) {
            // If there is no "%APPDATA% we are probably in none-NT Windows
            // So we save the config file to the programm directory
            cfg_path = (char *)malloc(strlen(CONFIG_FILE) + 1);
            strcpy(cfg_path, CONFIG_FILE);
        }
        else {
            cfg_path = (char *)malloc((PATH_MAX + strlen(CONFIG_FILE) + 1) * sizeof(char));
            snprintf(cfg_path, PATH_MAX + strlen(CONFIG_FILE), "%s\\%s", p, CONFIG_FILE);
        }
    }
#else // UNIX/MacOS
    if ((cfg_path == NULL) || (shift_pressed == 1)) {
        if ((p = fl_getenv("HOME")) == NULL) {
            fl_alert(_("No home-directory found"));
            return 1;
        }
        else {
            cfg_path = (char *)malloc((PATH_MAX + strlen(CONFIG_FILE) + 1) * sizeof(char));
            snprintf(cfg_path, PATH_MAX + strlen(CONFIG_FILE), "%s/%s", p, CONFIG_FILE);
        }
    }
#endif

    if (shift_pressed) {
        int rc = fl_choice(_("The shift key was held down during startup.\n"
                             "Do you want to start butt with a new configuration file?\n"
                             "This will overwrite your existing configuration file at\n%s"),
                           0, _("Start with old"), _("Start with new"), cfg_path);
        if (rc == 2) {
            if (cfg_create_default()) {
                fl_alert(_("Could not create config %s\nbutt is going to close now"), cfg_path);
                return 1;
            }
        }
    }

    printf(_("Reading config %s\n"), cfg_path);
    fflush(stdout);

    if (cfg_set_values(NULL) != 0) // read config file and initialize config struct
    {
        printf(_("Could not find config %s\n"), cfg_path);
        fflush(stdout);
        if (cfg_create_default()) {
            fl_alert(_("Could not create config %s\nbutt is going to close now"), cfg_path);
            return 1;
        }
        printf(_("butt created a default config at\n%s\n"), cfg_path);
        fflush(stdout);
        cfg_set_values(NULL);
    }

    if (cfg.audio.dev_count == 0) {
        fl_alert(_(
            "Could not find any audio device with input channels.\nbutt requires at least one audio device with input channels in order to work.\nThis can either be a built-in audio device, an external audio device or a virtual audio device.\n\nbutt is going to close now."));
        return 1;
    }

    return 0;
}

void set_locale_from_system(void)
{
    setlocale(LC_CTYPE, "");
    setlocale(LC_MESSAGES, "");

#if defined(__APPLE__)

    char path_to_executeable[PATH_MAX];
    char path_to_locale_dir[PATH_MAX];

    uint32_t path_len = (uint32_t)sizeof(path_to_executeable);
    if (_NSGetExecutablePath(path_to_executeable, &path_len) == 0) {
        char *folder_of_executable = strdup(dirname(path_to_executeable));
        snprintf(path_to_locale_dir, PATH_MAX, "%s%s", folder_of_executable, "/../Resources/locale");
        free(folder_of_executable);
    }

    bindtextdomain(PACKAGE, path_to_locale_dir);

#elif defined(WIN32)
    bindtextdomain(PACKAGE, "locale");
#else // Linux
    char *p;
    if ((p = fl_getenv("APPDIR")) != NULL) { // Set locale dir when launched from .AppImage
        char path_to_locale_dir[PATH_MAX];
        snprintf(path_to_locale_dir, sizeof(path_to_locale_dir), "%s/usr/share/locale", p);
        bindtextdomain(PACKAGE, path_to_locale_dir);
    }
    else { // Set locale dir to dir found by ./configure
        bindtextdomain(PACKAGE, LOCALEDIR);
    }
#endif

    textdomain(PACKAGE);
    bind_textdomain_codeset(PACKAGE, "UTF-8");
}

void set_locale_from_config(void)
{
    int lang;
    static char env_str[32];

    setlocale(LC_CTYPE, "");
    setlocale(LC_MESSAGES, "");
    snprintf(env_str, sizeof(env_str), "LANG=%s", cfg.gui.lang_str);
    putenv(env_str);

    snprintf(env_str, sizeof(env_str), "LANGUAGE=%s", cfg.gui.lang_str);
    putenv(env_str);

#if defined(__APPLE__)
    char path_to_executeable[PATH_MAX];
    char path_to_locale_dir[PATH_MAX];

    uint32_t path_len = (uint32_t)sizeof(path_to_executeable);
    if (_NSGetExecutablePath(path_to_executeable, &path_len) == 0) {
        char *folder_of_executable = strdup(dirname(path_to_executeable));
        snprintf(path_to_locale_dir, PATH_MAX, "%s%s", folder_of_executable, "/../Resources/locale");
        printf("%s\n", path_to_locale_dir);
        free(folder_of_executable);
    }

    bindtextdomain(PACKAGE, path_to_locale_dir);
#elif defined(WIN32)
    bindtextdomain(PACKAGE, "locale");
#else // Linux
    char *p;
    if ((p = fl_getenv("APPDIR")) != NULL) { // Set locale dir when launched from .AppImage
        char path_to_locale_dir[PATH_MAX];
        snprintf(path_to_locale_dir, sizeof(path_to_locale_dir), "%s/usr/share/locale", p);
        bindtextdomain(PACKAGE, path_to_locale_dir);
    }
    else { // Set locale dir to dir found by ./configure
        bindtextdomain(PACKAGE, LOCALEDIR);
    }
#endif

    textdomain(PACKAGE);
    bind_textdomain_codeset(PACKAGE, "UTF-8");
}
#endif // BUILD_CLIENT

int get_threshold_from_args(char opt, char *optarg, command_t *command)
{
    int threshold = atoi(optarg);
    if (threshold < 0) {
        printf(_("Illegal argument: Threshold must be a non-negative number\n"));
        fflush(stdout);
    }
    else {
        switch (opt) {
        case 'M':
            command->cmd = CMD_SET_STREAM_SIGNAL_THRESHOLD;
            break;
        case 'm':
            command->cmd = CMD_SET_STREAM_SILENCE_THRESHOLD;
            break;
        case 'O':
            command->cmd = CMD_SET_RECORD_SIGNAL_THRESHOLD;
            break;
        case 'o':
            command->cmd = CMD_SET_RECORD_SILENCE_THRESHOLD;
            break;
        default:
            printf("Internal error: Illegal value for \"opt\"\n");
            return -1;
        }
        command->param_size = sizeof(int);
        command->param = (int *)malloc(command->param_size);
        *((int *)command->param) = threshold;
        printf(_("%c threshold set to %d\n"), opt, threshold);
    }
    return threshold;
}

int main(int argc, char *argv[])
{
    char lcd_buf[33];
    char info_buf[256];
    int opt;
    int port = 1256;
    int search_port = 1;
    int skip_opening_audio_device = 0;
    int server_mode = SERVER_MODE_LOCAL;
    sock_proto_t command_proto = SOCK_PROTO_TCP;
    char server_addr[128];
    uint32_t song_len;

#ifndef BUILD_CLIENT

    cfg_path = NULL;
    // Activate support for multi threading
    Fl::lock();
#endif

    command_t command;
    command.cmd = CMD_EMPTY;
    command.param_size = 0;
    command.param = NULL;

    snprintf(server_addr, sizeof(server_addr), "%s", "127.0.0.1");

#ifndef WIN32
    // ignore the SIGPIPE signal. (In case the server closes the connection unexpected)
    signal(SIGPIPE, SIG_IGN);
#endif

#ifndef BUILD_CLIENT
#ifdef ENABLE_NLS
    set_locale_from_system();
#endif
#endif

    sock_init();

#ifdef BUILD_CLIENT
    if (argc < 2) {
        print_usage();
    }
#endif

    // Parse command line parameters
    while ((opt = getopt(argc, argv, ":vhc:AULxs:drtnqu:a:p:SM:m:O:o:")) != -1) {
        switch (opt) {
#ifndef BUILD_CLIENT
        case 'A':
            server_mode = SERVER_MODE_ALL;
            break;
        case 'x':
            server_mode = SERVER_MODE_OFF;
            break;
        case 'c':
            cfg_path = (char *)malloc((strlen(optarg) + 1) * sizeof(char));
            strcpy(cfg_path, optarg);
            break;
        case 'L':
            snd_print_devices();
            return 0;
            break;
#endif
        case 'U':
            command_proto = SOCK_PROTO_UDP;
            break;
        case 'p':
            port = atoi(optarg);
            if (port < 1024 || port > 65535) {
                printf(_("Illegal argument: Port must be a number between 1023 and 65535\n"));
                fflush(stdout);
                return 1;
            }
            search_port = 0;
            break;
        case 's':
            if (command.cmd != CMD_EMPTY) {
                printf(_("Warning: You may only pass one control option. Option -%c has been ignored.\n"), opt);
                break;
            }
            command.cmd = CMD_CONNECT;
            command.param_size = (uint32_t)strlen(optarg) + 1;
            command.param = (char *)malloc(command.param_size);
            sprintf((char *)command.param, "%s", optarg);
            break;
        case 'd':
            if (command.cmd != CMD_EMPTY) {
                printf(_("Warning: You may only pass one control option. Option -%c has been ignored.\n"), opt);
                break;
            }
            command.cmd = CMD_DISCONNECT;
            break;
        case 'r':
            if (command.cmd != CMD_EMPTY) {
                printf(_("Warning: You may only pass one control option. Option -%c has been ignored.\n"), opt);
                break;
            }
            command.cmd = CMD_START_RECORDING;
            break;
        case 't':
            if (command.cmd != CMD_EMPTY) {
                printf(_("Warning: You may only pass one control option. Option -%c has been ignored.\n"), opt);
                break;
            }
            command.cmd = CMD_STOP_RECORDING;
            break;
        case 'n':
            if (command.cmd != CMD_EMPTY) {
                printf(_("Warning: You may only pass one control option. Option -%c has been ignored.\n"), opt);
                break;
            }
            command.cmd = CMD_SPLIT_RECORDING;
            break;
        case 'q':
            if (command.cmd != CMD_EMPTY) {
                printf(_("Warning: You may only pass one control option. Option -%c has been ignored.\n"), opt);
                break;
            }
            command.cmd = CMD_QUIT;
            break;
        case 'v':
            printf("%s %s\n", argv[0], VERSION);
            fflush(stdout);
            return 0;
        case 'u':
            if (command.cmd != CMD_EMPTY) {
                printf(_("Warning: You may only pass one control option. Option -%c has been ignored.\n"), opt);
                break;
            }
            song_len = strlen(optarg) + 1;
            if (song_len > 1000) {
                song_len = 1000;
            }

            command.cmd = CMD_UPDATE_SONGNAME;
            command.param_size = song_len;
            command.param = (char *)malloc(command.param_size);
            sprintf((char *)command.param, "%s", optarg);
            break;
        case 'a':
            memset(server_addr, 0, sizeof(server_addr));
            snprintf(server_addr, sizeof(server_addr), "%s", optarg);
            break;
        case 'S':
            if (command.cmd != CMD_EMPTY) {
                printf(_("Warning: You may only pass one control option. Option -%c has been ignored.\n"), opt);
                break;
            }
            command.cmd = CMD_GET_STATUS;
            break;
        case 'M':
        case 'm':
        case 'O':
        case 'o':
            if (command.cmd != CMD_EMPTY) {
                printf(_("Warning: You may only pass one control option. Option -%c has been ignored.\n"), opt);
                break;
            }
            if (get_threshold_from_args(opt, optarg, &command) < 0) {
                return 1;
            }
            break;
        case 'h':
            print_usage();
            printf(_("\nOptions:\n"
                     "-h\tPrint this help text\n"
                     "-v\tPrint version information\n"));
#ifndef BUILD_CLIENT
            printf(_("\nOptions for operating mode:\n"
                     "-c\tPath to configuration file\n"
                     "-L\tPrint available audio devices\n"
                     "-A\tCommand server will be accessible from your network/internet (default: localhost only)\n"
                     "-U\tCommand server will use UDP instead of TCP\n"
                     "-x\tDo not start a command server\n"
                     "-p\tPort where the command server shall listen to (default: 1256)\n"));
#endif

            printf(_("\nOptions for control mode:\n"
                     "-s\tConnect to streaming server\n"
                     "-d\tDisconnect from streaming server\n"
                     "-r\tStart recording\n"
                     "-t\tStop recording\n"
                     "-n\tSplit recording\n"
                     "-q\tQuit butt\n"
                     "-u\tupdate song name\n"
                     "-S\tRequest status\n"
                     "-M\tSet streaming signal threshold (seconds)\n"
                     "-m\tSet streaming silence threshold (seconds)\n"
                     "-O\tSet recording signal threshold (seconds)\n"
                     "-o\tSet recording silence threshold (seconds)\n"
                     "-U\tConnect via UDP instead of TCP\n"
                     "-a\tAddress of the butt instance to be controlled (default: 127.0.0.1)\n"
                     "-p\tPort of the butt instance to be controlled (default: 1256)\n"));
            fflush(stdout);
            return 0;
        case '?':
            printf(_("Illegal option -%c.\nType butt -h to get a list of supported options.\n"), optopt);
            print_usage();
            return 1;
        case ':':
            if (optopt == 's') { // Handle connect option without argument
                if (command.cmd != CMD_EMPTY) {
                    printf(_("Warning: You may only pass one control option. Option -%c has been ignored.\n"), optopt);
                    break;
                }
                command.cmd = CMD_CONNECT;
                break;
            }
            printf(_("Option -%c requires an argument\n"), optopt);
            print_usage();
            return 1;
        default:
            printf(_("Command line parsing failed\n"));
            print_usage();
            return 1;
        }
    }

    // Handle commands
    if (command.cmd != CMD_EMPTY) {
        int ret;
#ifndef BUILD_CLIENT
        if (cfg_path != NULL) {
            free(cfg_path);
        }
#endif

        ret = command_send_cmd(command, server_addr, port, command_proto);

        if (command.param != NULL) {
            free(command.param);
        }

        switch (ret) {
        case CMD_ERR_CONNECT:
            printf(_("No butt instance running on %s at port %d\n"), server_addr, port);
            fflush(stdout);
            return 1;
            break;
        case CMD_ERR_SEND_CMD:
            printf(_("Error while sending command\n"));
            fflush(stdout);
            return 1;
            break;
        case CMD_ERR_RECV_RESPONSE:
            printf(_("Error: Did not receive response packet\n"));
            fflush(stdout);
            return 1;
            break;
        default:
            break;
        }

        if (command.cmd == CMD_GET_STATUS) {
            status_packet_t status_packet;

            ret = command_recv_status_reply(&status_packet, command_proto);
            if (ret == SOCK_ERR_RECV) {
                printf(_("Error: You may only request one status packet per second\n"));
                return 1;
            }
            if (ret == CMD_ERR_RECV_STATUS) {
                printf(_("Error: Did not receive status packet (UDP server not running?)\n"));
                return 1;
            }
            if (ret < 0) {
                printf(_("Network error while receiving status packet: %d\n"), ret);
                return 1;
            }
            else if (ret == 0) {
                printf(_("Error: Client and server versions do not match\n"));
                return 1;
            }
            else if (ret > 0) {
                int connected = (status_packet.status & (1 << STATUS_CONNECTED)) > 0;
                int connecting = (status_packet.status & (1 << STATUS_CONNECTING)) > 0;
                int recording = (status_packet.status & (1 << STATUS_RECORDING)) > 0;
                int signal = (status_packet.status & (1 << STATUS_SIGNAL_DETECTED)) > 0;
                int silence = (status_packet.status & (1 << STATUS_SILENCE_DETECTED)) > 0;
                int is_extended_packet = (status_packet.status & (1 << STATUS_EXTENDED_PACKET)) > 0;

                printf(_("connected: %d\nconnecting: %d\nrecording: %d\nsignal present: %d\nsignal absent: %d\n"), connected, connecting, recording, signal,
                       silence);
                fflush(stdout);

                if (is_extended_packet == 1) {
                    printf(_("stream seconds: %lu\n"), (unsigned long)status_packet.stream_seconds);
                    printf(_("stream kBytes: %lu\n"), (unsigned long)status_packet.stream_kByte);
                    printf(_("record seconds: %lu\n"), (unsigned long)status_packet.record_seconds);
                    printf(_("record kBytes: %lu\n"), (unsigned long)status_packet.record_kByte);
                    printf(_("volume left: %0.1f\n"), status_packet.volume_left / 10.0);
                    printf(_("volume right: %0.1f\n"), status_packet.volume_right / 10.0);
                    printf(_("song: %s\n"), status_packet.song);
                    printf(_("record path: %s\n"), status_packet.rec_path);
                    printf(_("listeners: %d\n"), status_packet.listener_count);
                    fflush(stdout);
                    free(status_packet.song);
                    free(status_packet.rec_path);
                }
            }
        }

        return 0;
    }

#ifndef BUILD_CLIENT
    if (Fl::get_key(FL_Control_L) || Fl::get_key(FL_Control_R)) {
        fl_alert(_("The control key was held down during startup.\n"
                   "butt will start without opening an audio device.\n"
                   "Please select your preferred audio device in settings->audio"));

        skip_opening_audio_device = 1;
    }

    if (snd_init() != 0) {
        fl_alert(_("PortAudio init failed\nbutt is going to close now"));
        return 1;
    }

    load_AAC_lib();

    if (read_cfg() != 0) {
        return 1;
    }

    snd_init_dsp();

#ifdef ENABLE_NLS
    if (strcmp(cfg.gui.lang_str, "system") != 0) {
        set_locale_from_config();
    }
#endif

#if !defined(__APPLE__) && !defined(WIN32) // LINUX
    Fl_File_Icon::load_system_icons();
    FL_NORMAL_SIZE = 12;
#endif

    fl_g = new flgui();
    fl_g->window_main->xclass("butt_FLTK");

    fl_g->window_main->show();
    fl_font(fl_font(), 10);

    strcpy(lcd_buf, _("idle"));
    print_lcd(lcd_buf, strlen(lcd_buf), -2, 1);
    fl_g->label_current_listeners->hide();

#ifdef WIN32
    fl_g->window_main->icon((char *)LoadIcon(fl_display, MAKEINTRESOURCE(IDI_ICON)));
    // The fltk icon code above only loads the default icon.
    // Here, once the window is shown, we can assign
    // additional icons, just to make things look a bit nicer.
    HANDLE bigicon = LoadImage(GetModuleHandle(0), MAKEINTRESOURCE(IDI_ICON), IMAGE_ICON, 32, 32, 0);
    SendMessage(fl_xid(fl_g->window_main), WM_SETICON, ICON_BIG, (LPARAM)bigicon);
    HANDLE smallicon = LoadImage(GetModuleHandle(0), MAKEINTRESOURCE(IDI_ICON), IMAGE_ICON, 16, 16, 0);
    SendMessage(fl_xid(fl_g->window_main), WM_SETICON, ICON_SMALL, (LPARAM)smallicon);
#endif

#ifndef BUILD_CLIENT
#if defined(__APPLE__)
    askForMicPermission();
#endif
#endif

    lame_stream.gfp = NULL;
    lame_rec.gfp = NULL;
    flac_rec.encoder = NULL;
    flac_stream.encoder = NULL;
#ifdef HAVE_LIBFDK_AAC
    aac_stream.handle = NULL;
    aac_rec.handle = NULL;
#endif

    snprintf(info_buf, sizeof(info_buf),
             _("Starting %s\nWritten by Daniel Nöthen\n"
               "iPhone/iPad client: https://izicast.de\n"
               "Donate: paypal@danielnoethen.de\n"),
             PACKAGE_STRING);
    print_info(info_buf, 0);

    init_main_gui_and_audio();
    if ((skip_opening_audio_device == 0) && (snd_open_streams() != 0)) {
        cfg.audio.dev_num = 0;
        cfg.audio.dev_name = (char *)realloc(cfg.audio.dev_name, strlen(_("Default PCM device (default)")) + 1);
        strcpy(cfg.audio.dev_name, _("Default PCM device (default)"));

        cfg.audio.dev2_num = -1;
        cfg.audio.dev2_name = (char *)realloc(cfg.audio.dev2_name, strlen(_("None")) + 1);
        strcpy(cfg.audio.dev2_name, _("None"));

        if (snd_open_streams() != 0) {
            fl_alert(_("Could not open audio device.\nPlease select your preferred audio device in settings->audio"));
        }
        else {
            fl_alert(_("butt could not open previously used audio device.\nThe system default audio device will be used.\n"));
        }
    }
    vu_init();

    Fl::add_timeout(5, &display_rotate_timer);
    Fl::add_timeout(0.25, &cmd_timer);

    if (cfg.main.connect_at_startup) {
        button_connect_cb();
    }
    else if (cfg.main.signal_detection == 1 && cfg.main.signal_threshold > 0) {
        Fl::add_timeout(1, &stream_signal_timer);
    }

    if (cfg.rec.rec_after_launch && !recording) {
        button_record_cb(false);
    }
    else if (cfg.rec.signal_detection == 1 && cfg.rec.signal_threshold > 0) {
        Fl::add_timeout(1, &record_signal_timer);
    }

    if (server_mode != SERVER_MODE_OFF) {
        int command_port = command_start_server(port, search_port, server_mode, command_proto);
        if (command_port > 0) {
            snprintf(info_buf, sizeof(info_buf), _("Command server listening on port %d\n"), command_port);
            printf("%s", info_buf);
            fflush(stdout);
            // print_info(info_buf, 0);
        }
        else {
            printf("ret command_start_server: %d\n", command_port);
            snprintf(info_buf, sizeof(info_buf), _("Warning: could not start command server on port %d\n"), port);
            printf("%s", info_buf);
            fflush(stdout);
            print_info(info_buf, 0);
        }
    }

    if (cfg.main.check_for_update) {
        int rc;
        char uri[100];
        char *new_version;
        int ret = update_check_for_new_version();

        if (ret == UPDATE_NEW_VERSION) {
            new_version = update_get_version();
            rc = fl_choice(_("New version available: %s\nYou have version %s"), _("Don't ask again"), _("Cancel"), _("Get new version"), new_version, VERSION);
            if (rc == 0) {
                cfg.main.check_for_update = 0;
            }
            if (rc == 2) {
                snprintf(uri, sizeof(uri) - 1, "https://danielnoethen.de/butt/index.html#_download");
                fl_open_uri(uri);
            }
        }
    }

    PmError midi_result = midi_init();
    if (midi_result == 0) {
        if (strcmp(cfg.midi.dev_name, "Disabled") != 0) {
            if (midi_open_device(cfg.midi.dev_name) == 0) {
                midi_start_polling();
            }
        }
    }
    else {
        snprintf(info_buf, sizeof(info_buf), _("Could not initialize PortMidi: %s"), Pm_GetErrorText(midi_result));
        print_info(info_buf, 1);
    }

    fill_cfg_widgets();

#ifdef WIN32
    if (cfg.main.minimize_to_tray == 1) {
        fl_g->window_main->minimize_to_tray = true;
    }

    if (cfg.main.start_agent == 1) {
        if ((tray_agent_start() == 0) && (cfg.gui.start_minimized == 0)) {
            tray_agent_send_cmd(TA_START);
        }
    }

#else
    fl_g->group_agent->deactivate();
#endif

#ifndef WITH_RADIOCO
    fl_g->radio_add_srv_radioco->hide();
#endif

    if (cfg.gui.start_minimized == 1) {
        fl_g->window_main->iconize();
    }

    return GUI_LOOP();
#endif // BUILD_CLIENT
}

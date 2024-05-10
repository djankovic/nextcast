
// tray agent functions for butt
//
// Copyright 2007-2021 by Daniel Noethen.
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

#ifdef WIN32
#include <windows.h>
#include <string>
#include <FL/x.H>

#include "flgui.h"
#include "cfg.h"
#include "config.h"
#include "butt.h"
#include "sha256.h"
#include "tray_agent.h"

#define AGENT_CLASS      "AutoHotkeyGUI"
#define AGENT_TITLE      "buttTrayAgent11-03-2021"
#define AGENT_SHA256     "80015491aabc0b6a3470cd375d8304a5b4446b2ee88929e552c03676ca877f53"
#define AGENT_EXECUTABLE "butt_agent.exe"

static char song_buf[512];

static int check_hash(void)
{

    char agent_sha256[65];
    const int salt = 0xA50D;

    if (sha256_file((char *)AGENT_EXECUTABLE, agent_sha256, salt) == 0) {
        //#warning "REMOVE THIS RETURN FOR PRODUCTION"
        //    printf("agent hash: %s\n", agent_sha256);
        //    return 0;
        //#warning "REMOVE THIS RETURN FOR PRODUCTION"

        if (strncmp(agent_sha256, AGENT_SHA256, 64) == 0) {
            return 0; // correct hash found
        }
    }

    return 1; // error
}

static int send_msg(char *cmsg)
{
    COPYDATASTRUCT MyCDS;
    wchar_t wmsg[512];

    // Convert normal char array to wide char array
    mbstowcs(wmsg, cmsg, 512);

    MyCDS.dwData = 0;
    MyCDS.cbData = sizeof(wmsg);
    MyCDS.lpData = &wmsg;

    // Get handle of butt main window
    HWND hwButt = fl_xid(fl_g->window_main);
    HWND hwAgent;

    if (tray_agent_is_running(&hwAgent) == 1) {
        SendMessage(hwAgent, WM_COPYDATA, (WPARAM)(HWND)hwButt, (LPARAM)(LPVOID)&MyCDS);
        return 0;
    }
    else {
        return 1;
    }
}

int tray_agent_start(void)
{
    char command[32];
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;

    // Do not start if it is already running
    if (tray_agent_is_running(NULL) == 1) {
        return 1;
    }

    if (check_hash() != 0) {
        return 1; // agent has wrong hash. Do not execute it!
    }

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    snprintf(command, sizeof(command), "%s -s", AGENT_EXECUTABLE);

    if (CreateProcessA(NULL, command, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi) != 0) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        struct timespec wait_50ms;
        wait_50ms.tv_sec = 0;
        wait_50ms.tv_nsec = (50 * 1000 * 1000);

        // Wait max 40*50 ms = 2 seconds until agent has been started
        int timed_out = 1;
        for (int i = 0; i < 40; i++) {
            if (tray_agent_is_running(NULL) == 1) {
                timed_out = 0;
                break;
            }
            nanosleep(&wait_50ms, NULL);
        }
        if (timed_out == 1) {
            return 1; // error: tray agent still not running
        }
        else {
            return 0;
        }
    }
    else {
        return 1;
    }
}

int tray_agent_is_running(HWND *hwAgent)
{
    HWND hwWindow = FindWindow(AGENT_CLASS, AGENT_TITLE);

    if (hwWindow != NULL) {
        if (hwAgent != NULL) {
            *hwAgent = hwWindow;
        }

        return 1;
    }
    else {
        return 0;
    }
}

void tray_agent_set_song(char *song)
{
    snprintf(song_buf, sizeof(song_buf), "%s", song);
}

int tray_agent_send_cmd(int cmd)
{
    char msg[512];
    char tray_title[128];
    char custom_window_title[128];
    int minimize;

    int pid = GetCurrentProcessId();

    if (cfg.gui.window_title[0] != '\0') {
        snprintf(custom_window_title, sizeof(custom_window_title), "%s", cfg.gui.window_title);
    }
    else {
        snprintf(custom_window_title, sizeof(custom_window_title), "dummy");
    }
    

    if (cfg.main.num_of_srv > 0) {
        snprintf(tray_title, sizeof(tray_title), "%s", cfg.srv[cfg.selected_srv]->name);
    }
    else {
        snprintf(tray_title, sizeof(tray_title), "%s", "no server");
    }

    switch (cmd) {
    case TA_MINIMIZE:
        // Start agent when minimizing and agent is not running yet
        if (tray_agent_is_running(NULL) == 0) {
            if (tray_agent_start() != 0) {
                return 1; // Tray agent could not be started
            }
        }
        minimize = 1;
        snprintf(msg, sizeof(msg), "%s\t%d\t%d\t%s\t%d\t%s", VERSION, minimize, pid, tray_title, connected, custom_window_title);
        break;
    case TA_START:
    case TA_SERVER_NAME:
    case TA_CONNECT_STATE:
        minimize = 0;
        snprintf(msg, sizeof(msg), "%s\t%d\t%d\t%s\t%d\t%s", VERSION, minimize, pid, tray_title, connected, custom_window_title);
        break;
    case TA_SONG_UPDATE:
        minimize = 0;
        snprintf(msg, sizeof(msg), "%s\t%d\t%d\t%s\t%d\t%s\t%s", VERSION, minimize, pid, tray_title, connected, custom_window_title, song_buf);
        break;
    default:
        return 1;
    }

    int ret = send_msg(msg);
    if ((ret == 0) && (minimize == 1)) {
        if (fl_g->window_cfg->shown()) // Close settings window when minimizing to tray
        {
            fl_g->window_cfg->hide();
            Fl::remove_timeout(&cfg_win_pos_timer);
        }
    }

    return ret;
}

int tray_agent_stop(void)
{
    char command[32];
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;

    if (check_hash() != 0) {
        return 1; // Agent has wrong hash. Do not execute it!
    }

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    snprintf(command, sizeof(command), "%s -q", AGENT_EXECUTABLE);

    if (CreateProcessA(NULL, command, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi) != 0) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return 0;
    }
    else {
        return 1; // CreateProcessA failed
    }
}

#endif // ifdef WIN32

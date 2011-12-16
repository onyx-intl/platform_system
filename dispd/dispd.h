/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Copyright (c) 2010-2011 Freescale Semiconductor, Inc. */

#ifndef DISPD_H__
#define DISPD_H__

#define LOG_TAG "dispd"
#include "cutils/log.h"

typedef int boolean;
enum {
    false = 0,
    true = 1
};

#define DEVPATH "/dev/block/"
#define DEVPATHLENGTH 11

#define WEXITSTATUS(status) (((status) & 0xff00) >> 8)

// set this to log dispd events
#define ENABLE_LOG_DISP

#ifdef ENABLE_LOG_DISP
#define LOG_DISP(fmt, args...) \
    { LOGD(fmt , ## args); }
#else
#define LOG_DISP(fmt, args...) \
    do { } while (0)
#endif /* ENABLE_LOG_VOL */



/*
 * Prototypes
 */

int process_framework_command(int socket);
int process_uevent_message(int socket);


int switch_bootstrap(void);

int send_msg(char *msg);
int send_msg_with_data(char *msg, char *data);
extern int bootstrap;
#endif

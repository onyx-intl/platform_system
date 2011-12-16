
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

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>
#include <sched.h>

#include <sys/mount.h>

#include <cutils/config_utils.h>
#include <cutils/properties.h>

#include "dispd.h"
#include "dispmgr.h"
#include "disp.h"

#define DEBUG_DISPMGR 0

int dispmgr_enable_disp(boolean enabled)
{
    LOGI("dispmgr_enable_disp enabled %d", enabled);
    return disp_enabled_set(enabled);
}

int dispmgr_connected_set(boolean enabled)
{
    LOGI("dispmgr_connected_set enabled %d", enabled);
    return disp_connected_set(enabled);
}

int dispmgr_send_status(void)
{
    int rc;

    //pthread_mutex_lock(&lock);
    if ((rc = disp_send_status()) < 0) {
        LOGE("Error sending state to framework (%d)", rc);
    }
    //pthread_mutex_unlock(&lock);


    return 0;
}

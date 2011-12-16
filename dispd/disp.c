
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

#include <fcntl.h>
#include <errno.h>
#include <cutils/properties.h>
#include "dispd.h"
#include "disp.h"

#define DEBUG_DISP 0

static boolean disp_connected = false;
static boolean disp_enabled = false;

int disp_connected_set(boolean enabled)
{
    LOGI("disp_connected_set(): %d",enabled);
    disp_connected = enabled;
    //Disable the connection will also automatically disable the display
    if((disp_connected == false) && (disp_enabled == true)) {
        disp_enabled = false;
        send_msg(DISPD_EVT_DISP_DISABLED);
    }
    
    if(enabled) {
        LOGI("rw.SECOND_DISPLAY_CONNECTED set to 1");
        property_set("rw.SECOND_DISPLAY_CONNECTED", "1");
    }
    else{
        LOGI("rw.SECOND_DISPLAY_CONNECTED set to 0");
        property_set("rw.SECOND_DISPLAY_CONNECTED", "0");
    }

    send_msg(enabled ? DISPD_EVT_DISP_CONNECTED : DISPD_EVT_DISP_DISCONNECTED);
    return 0;
}

boolean disp_connected_get()
{
    return disp_connected;
}

int disp_enabled_set(boolean enabled)
{
#if DEBUG_DISP
    LOG_DISP("disp_connected_set(): %d",enabled);
#endif
    if((enabled == true)&&(disp_connected != true)) {
        LOGE("Error!Please connect extended display connection");
        return -1;
    }
    disp_enabled = enabled;
    send_msg(enabled ? DISPD_EVT_DISP_ENABLED : DISPD_EVT_DISP_DISABLED);
    return 0;
}

boolean disp_enabled_get()
{
    return disp_enabled;
}

int disp_send_status(void)
{
    int rc;

#if DEBUG_DISP
    LOG_DISP("disp_send_status():");
#endif

    rc = send_msg(disp_connected_get() ? DISPD_EVT_DISP_CONNECTED :
                                      DISPD_EVT_DISP_DISCONNECTED);
    if (rc < 0)
        return rc;

    rc = send_msg(disp_enabled_get() ? DISPD_EVT_DISP_ENABLED :
                                      DISPD_EVT_DISP_DISABLED);
    return rc;
}

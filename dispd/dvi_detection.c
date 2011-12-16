
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

/* Copyright 2010-2011 Freescale Semiconductor, Inc. */

#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>

#include <sys/types.h>

#include "dispd.h"
#include "dvi_detection.h"
#include "uevent.h"

#define DEBUG_BOOTSTRAP 0

#define SYSFS_CLASS_DVI_DETECTION_PATH_STATE "/sys/devices/platform/mxc_ddc.0/cable_state"

int dvi_detection_bootstrap()
{
    char filename[255];
    char event_state[255];
    char tmp[255];
    char *uevent_params[2];
    FILE *fp;
    memset(filename, 0, 255);
    strcpy(filename,SYSFS_CLASS_DVI_DETECTION_PATH_STATE);
    LOGI("detection_bootstrap IN");
    if (!(fp = fopen(filename, "r"))) {
        LOGE("Error opening dvi name path '%s' (%s)",
             SYSFS_CLASS_DVI_DETECTION_PATH_STATE, strerror(errno));
       return -errno;
    }
    if (!fgets(event_state, sizeof(event_state), fp)) {
        LOGE("Unable to read dvi name");
        fclose(fp);
        return -EIO;
    }
    fclose(fp);

    event_state[strlen(event_state) -1] = '\0';
    sprintf(tmp, "EVENT=%s", event_state);
    uevent_params[0] = (char *) strdup(tmp);
    uevent_params[1] = (char *) NULL;

    if (simulate_uevent("mxc_ddc", SYSFS_CLASS_DVI_DETECTION_PATH, "add", uevent_params) < 0) {
        LOGE("Error simulating uevent (%s)", strerror(errno));
        return -errno;
    }
    return 0;
}



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

#include <unistd.h>
#include <errno.h>

#include "dispd.h"
#include "cmd_dispatch.h"
#include "disp.h"
#include "dispmgr.h"

struct cmd_dispatch {
    char *cmd;
    int (* dispatch) (char *);
};

static void dispatch_cmd(char *cmd);
static int do_send_disp_status(char *cmd);
static int do_set_disp_enable(char *cmd);

#define DISPD_CMD_ENABLE_DISP         "enable_display"
#define DISPD_CMD_DISABLE_DISP        "disable_display"
#define DISPD_CMD_SEND_DISP_STATUS    "send_display_status"

static struct cmd_dispatch dispatch_table[] = {
    { DISPD_CMD_ENABLE_DISP,      do_set_disp_enable },
    { DISPD_CMD_DISABLE_DISP,     do_set_disp_enable },
    { DISPD_CMD_SEND_DISP_STATUS, do_send_disp_status },
    { NULL, NULL }
};

int process_framework_command(int socket)
{
    int rc;
    char buffer[101];

    if ((rc = read(socket, buffer, sizeof(buffer) -1)) < 0) {
        LOGE("Unable to read framework command (%s)", strerror(errno));
        return -errno;
    } else if (!rc)
        return -ECONNRESET;

    int start = 0;
    int i;

    buffer[rc] = 0;

    for (i = 0; i < rc; i++) {
        if (buffer[i] == 0) {
            dispatch_cmd(buffer + start);
            start = i + 1;
        }
    }
    return 0;
}

static void dispatch_cmd(char *cmd)
{
    struct cmd_dispatch *c;

    LOG_DISP("dispatch_cmd(%s):", cmd);

    for (c = dispatch_table; c->cmd != NULL; c++) {
        if (!strncmp(c->cmd, cmd, strlen(c->cmd))) {
            c->dispatch(cmd);
            return;
        }
    }

    LOGE("No cmd handlers defined for '%s'", cmd);
}

static int do_send_disp_status(char *cmd)
{
    return disp_send_status();
}

static int do_set_disp_enable(char *cmd)
{
    if (!strcmp(cmd, DISPD_CMD_ENABLE_DISP))
        return dispmgr_enable_disp(true);

    return dispmgr_enable_disp(false);
}


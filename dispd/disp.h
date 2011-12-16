
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

#ifndef _DISP_H
#define _DISP_H

#define DISPD_EVT_DISP_CONNECTED            "disp_connected"
#define DISPD_EVT_DISP_DISCONNECTED         "disp_disconnected"
#define DISPD_EVT_DISP_ENABLED            "disp_enabled"
#define DISPD_EVT_DISP_DISABLED         "disp_disabled"

int disp_send_status(void);
boolean disp_connected_get(void);
int disp_connected_set(boolean enabled);
int disp_enabled_set(boolean enabled);
boolean disp_enabled_get();

#endif

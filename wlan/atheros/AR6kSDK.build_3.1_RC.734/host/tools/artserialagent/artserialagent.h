/*
 * Copyright (c) 2009 Atheros Communications Inc.
 * All rights reserved.
 *
 */

#ifndef _ARTAGENT_H_
#define _ARTAGENT_H_

#define ART_PORT                5454
#define LINE_ARRAY_SIZE         4096
#define SEND_ENDPOINT	        0

#define ART_SOCK_OP_RD          (0x80 << 24)
#define ART_SOCK_OP_WR          (0x08 << 24)

#define AR6000_XIOCTL_HTC_RAW_OPEN      13
#define AR6000_XIOCTL_HTC_RAW_CLOSE     14
#define AR6000_XIOCTL_HTC_RAW_READ      15
#define AR6000_XIOCTL_HTC_RAW_WRITE     16

#define DISCONNECT_PIPE_CMD_ID      16
#define CLOSE_PIPE_CMD_ID           17

#endif /* _ARTAGENT_H_ */

/*
 * Copyright (C) 2010 Freescale Semiconductor, Inc.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <sys/mman.h>
#include <cutils/properties.h>

#define LOG_BUF_MAX 512

#ifndef TS_DEVICE
#error "no touch screen device defined"
#endif

#define DEV1_(x) #x
#define DEV_(x) DEV1_(x)
#define TS_INPUT_DEV DEV_(TS_DEVICE)

static const char fb_dev[] = "/dev/graphics/fb0";
static const char input_dev[] = "/dev/input/event";
static const char cf_file[] = "/data/system/calibration";
static const char log[] = "/ts.log";
static const char dev_name[] = TS_INPUT_DEV;
static int log_fd;
static struct fb_var_screeninfo info;
static void *scrbuf;
static int fb_fd, ts_fd, cf_fd;
static int cal_val[7];

static void log_write(const char *fmt, ...)
{
    char buf[LOG_BUF_MAX];
    va_list ap;

    if (log_fd < 0) return;

    va_start(ap, fmt);
    vsnprintf(buf, LOG_BUF_MAX, fmt, ap);
    buf[LOG_BUF_MAX - 1] = 0;
    va_end(ap);
    write(log_fd, buf, strlen(buf));
}

static void write_conf(int *data)
{
    char param_path[256];
    char buf[200];
    int fd, len;

    sprintf(param_path,
	    "/sys/module/%s/parameters/calibration", dev_name);
    fd = open(param_path, O_WRONLY);
    if (fd < 0) {
	log_write("write_conf() error, can not write driver parameters\n");
	return;
    }
    len = sprintf(buf, "%d,%d,%d,%d,%d,%d,%d",
			    data[0], data[1], data[2],
			    data[3], data[4], data[5],
			    data[6]);
    log_write("write_conf(), write driver parameters:\n\t%s\n", buf);
    write(fd, buf, len);
    close(fd);
}

static void save_conf(int *data)
{
    char buf[200];
    int len;

    cf_fd = open(cf_file, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (cf_fd < 0) {
	log_write("create, open file %s error:%d\n", cf_file, errno);
	return;
    }

    len = sprintf(buf, "%d\n%d\n%d\n%d\n%d\n%d\n%d",
			    data[0], data[1], data[2],
			    data[3], data[4], data[5],
			    data[6]);
    write(cf_fd, buf, len);
    close(cf_fd);
}

static void get_input(int *px, int *py)
{
    int rd, i;
    struct input_event ev[64];
    int step = 0;

    while (1) {

	/* read ts input */
	rd = read(ts_fd, ev, sizeof(struct input_event) * 64);

	if (rd < (int) sizeof(struct input_event)) {
	    log_write("Read input error\n");
	    continue;
	}

	for (i = 0; i < (int)(rd / sizeof(struct input_event)); i++) {

	    switch (ev[i].type) {

	    case EV_SYN:
		if (step)
		    return;
	    case EV_KEY:
		if (ev[i].code == BTN_TOUCH && ev[i].value == 0)
		    /* get the final touch */
		    step = 1;
		break;
	    case EV_ABS:
		if (ev[i].code == REL_X)
		    *px = ev[i].value;
		else if (ev[i].code == REL_Y)
		    *py = ev[i].value;
		break;
	    default:
		break;
	    }
	}

    }

}

#define LINE_LEN 16
static void draw_cross(int x, int y, int clear)
{
    int px_byte = info.bits_per_pixel / 8;
    int h_start, v_start;
    int i;
    __u32 pixel = ~(0U);
    __u8 *buf = scrbuf;
    __u16 *buf16;
    __u32 *buf32;

    if (clear)
	pixel = 0;

    h_start = (x + y*info.xres - LINE_LEN/2) * px_byte;
    v_start = (x + (y - LINE_LEN/2)*info.xres) * px_byte;

    switch (info.bits_per_pixel) {

    case 16:
	buf16 = (__u16*)((__u8*)scrbuf + h_start);
	for (i = 0; i <= LINE_LEN; i ++)
	    *buf16++ = (__u16)pixel;
	buf16 = (__u16*)((__u8*)scrbuf + v_start);
	for (i = 0; i <= LINE_LEN; i ++) {
	    *buf16 = (__u16)pixel;
	    buf16 += info.xres;
	}
	break;
    case 24:
	buf += h_start;
	for (i = 0; i <= LINE_LEN; i ++) {
	    *buf++ = *((__u8*)pixel + 2);
	    *buf++ = *((__u8*)pixel + 1);
	    *buf++ = *(__u8*)pixel;
	}
	buf = (__u8*)scrbuf + v_start;
	for (i = 0; i <= LINE_LEN; i ++) {
	    *buf++ = *((__u8*)pixel + 2);
	    *buf++ = *((__u8*)pixel + 1);
	    *buf = *(__u8*)pixel;
	    buf += info.xres * px_byte - 2;
	}
	break;
    case 32:
	buf32 = (__u32*)((__u8*)scrbuf + h_start);
	pixel &= (((1 << info.transp.length) - 1) << info.transp.offset);
	for (i = 0; i <= LINE_LEN; i ++)
	    *buf32++ = pixel;
	buf32 = (__u32*)((__u8*)scrbuf + v_start);
	for (i = 0; i <= LINE_LEN; i ++) {
	    *buf32 = pixel;
	    buf32 += info.xres;
	}
	break;
    default:
	break;
    }

}

static void do_calibration(void)
{
    int i, x, y;
    int dx[3], dy[3];
    int tx[3], ty[3];
    int delta, delta_x[3], delta_y[3];

    /* calculate the expected point */
    x = info.xres / 4;
    y = info.yres / 4;

    dx[0] = x;
    dy[0] = info.yres / 2;
    dx[1] = info.xres / 2;
    dy[1] = y;
    dx[2] = info.xres - x;
    dy[2] = info.yres - y;

retry:

    for (i = 0; i < 3; i ++) {
	draw_cross(dx[i], dy[i], 0);
	get_input(&tx[i], &ty[i]);
	log_write("get event: %d,%d -> %d,%d\n",
			tx[i], ty[i], dx[i], dy[i]);
	draw_cross(dx[i], dy[i], 1);
    }

    /* check ok, calulate the result */
    delta = (tx[0] - tx[2]) * (ty[1] - ty[2])
                - (tx[1] - tx[2]) * (ty[0] - ty[2]);
    delta_x[0] = (dx[0] - dx[2]) * (ty[1] - ty[2])
                - (dx[1] - dx[2]) * (ty[0] - ty[2]);
    delta_x[1] = (tx[0] - tx[2]) * (dx[1] - dx[2])
                - (tx[1] - tx[2]) * (dx[0] - dx[2]);
    delta_x[2] = dx[0] * (tx[1] * ty[2] - tx[2] * ty[1]) -
                dx[1] * (tx[0] * ty[2] - tx[2] * ty[0]) +
                dx[2] * (tx[0] * ty[1] - tx[1] * ty[0]);
    delta_y[0] = (dy[0] - dy[2]) * (ty[1] - ty[2])
                - (dy[1] - dy[2]) * (ty[0] - ty[2]);
    delta_y[1] = (tx[0] - tx[2]) * (dy[1] - dy[2])
                - (tx[1] - tx[2]) * (dy[0] - dy[2]);
    delta_y[2] = dy[0] * (tx[1] * ty[2] - tx[2] * ty[1]) -
                dy[1] * (tx[0] * ty[2] - tx[2] * ty[0]) +
                dy[2] * (tx[0] * ty[1] - tx[1] * ty[0]);

    cal_val[0] = delta_x[0];
    cal_val[1] = delta_x[1];
    cal_val[2] = delta_x[2];
    cal_val[3] = delta_y[0];
    cal_val[4] = delta_y[1];
    cal_val[5] = delta_y[2];
    cal_val[6] = delta;

    save_conf(cal_val);
    write_conf(cal_val);
}

static void test_calibration(void)
{
    int sample[3][2] = {
	{ 200, 200 },
	{ 100, 400 },
	{ 600, 330 },
    };
    int tx[3];
    int ty[3];
    int i, x, y;

    for (i = 0; i < 3; i ++) {
	draw_cross(sample[i][0], sample[i][1], 0);
	get_input(&tx[i], &ty[i]);
	x = (cal_val[0] * tx[i]) +
		(cal_val[1] * ty[i]) +
		cal_val[2];
	y = (cal_val[3] * tx[i]) +
		(cal_val[4] * ty[i]) +
		cal_val[5];
	log_write("get event: %d,%d\n", x/cal_val[6], y/cal_val[6]);
	draw_cross(sample[i][0], sample[i][1], 1);
    }
}

static int check_conf(void)
{
    int data[7];
    char *buffer;
    int ret;
    struct stat s;

    /* check conf file */
    if (stat(cf_file, &s) == 0) {
	/* conf file already existed */
	cf_fd = open(cf_file, O_RDWR);
	if (cf_fd >= 0) {
	    buffer = calloc(1, s.st_size + 1);
	    read(cf_fd, buffer, s.st_size);
	    ret = sscanf(buffer, "%d\n%d\n%d\n%d\n%d\n%d\n%d",
				&data[0], &data[1], &data[2],
				&data[3], &data[4], &data[5],
				&data[6]);
	    if (ret == 7) {
		free(buffer);
		/* write to driver */
		write_conf(data);
		close(cf_fd);
		return 1;
	    }
	    log_write("Failed to get datas from conf file: %d\n", ret);
	    free(buffer);
	    close(cf_fd);
	}
    }
    return 0;
}

int main(int argc, char **argv)
{
    struct fb_fix_screeninfo finfo;
    struct stat s;
    int i;
    char runme[PROPERTY_VALUE_MAX];

    property_get("ro.calibration", runme, "");
    if (runme[0] != '1')
	return 0;

    /* open log */
    log_fd = open(log, O_WRONLY | O_CREAT | O_TRUNC);

    if (check_conf())
	goto err_log;

    /* read framebuffer for resolution */
    fb_fd = open(fb_dev, O_RDWR);
    if (fb_fd <= 0) {
	log_write("Failed to open %s\n", fb_dev);
	goto err_log;
    }
    if (-1 == ioctl(fb_fd, FBIOGET_VSCREENINFO, &info)) {
	log_write("Failed to get screen info\n");
	goto err_fb;
    }
    log_write("Screen resolution: %dx%d\n", info.xres, info.yres);
    /* map buffer */
    if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo) == -1) {
	log_write("Failed to get screen info: %d\n", errno);
        goto err_fb;
    }
    scrbuf = (__u16*) mmap(0, finfo.smem_len,
			    PROT_READ | PROT_WRITE,
			    MAP_SHARED,
			    fb_fd, 0);
    if (scrbuf== MAP_FAILED) {
	log_write("Failed to map screen\n");
	goto err_fb;
    }

    memset(scrbuf, 0, finfo.smem_len);

    /* print information on screen */
    fb_fd = open("/dev/tty0", O_RDWR);
    if (fb_fd >= 0) {
        const char *msg = "\n"
        "\n"
        "\n"  // console is 40 cols x 30 lines
        "\n"
        "\n"
        "\n"
        "\n"
        "\n"
        "\n"
        "\n"
        "\n"
        "\n"
        "\t\tTouchscreen Calibration";
	char esc = 27;
	char clear[10];
	sprintf(clear, "%c[2J", esc);
        write(fb_fd, clear, strlen(clear));
	sprintf(clear, "%c[1;1H", esc);
        write(fb_fd, clear, strlen(clear));
        write(fb_fd, msg, strlen(msg));
        close(fb_fd);
    }

    for (i = 0; ; i++) {
	/* open touchscreen input dev */
	char ts_dev[256];
	char name[256] = "Unknow";
	sprintf(ts_dev, "%s%d", input_dev, i);

	if (stat(ts_dev, &s) != 0) {
	    log_write("can not find ts device\n");
	    goto err_map;
	}
	ts_fd = open(ts_dev, O_RDWR);
	if (ts_fd < 0) {
	    log_write("Failed to open %s\n", ts_dev);
	    continue;
	}

	ioctl(ts_fd, EVIOCGNAME(sizeof(name)), name);
	log_write("%s: get name: %s\n", ts_dev, name);
	if (strncmp(name, dev_name, strlen(dev_name)) == 0) {
	    break;
	} else {
	    log_write("%s: not %s\n", ts_dev, dev_name);
	    close(ts_fd);
	    continue;
	}
    }

    do_calibration();

    log_write("Calibration done!!\n");

    //test_calibration();

    close(ts_fd);
err_map:
    munmap(scrbuf, finfo.smem_len);
err_fb:
    close(fb_fd);
err_log:
    close(log_fd);

    return 0;
}

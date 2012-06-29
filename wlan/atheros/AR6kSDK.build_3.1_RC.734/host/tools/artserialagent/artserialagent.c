/*
 * Copyright (c) 2004-2010 Atheros Communications Inc.
 * All rights reserved.
 *
 *
 * 
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
//
//
 * 
 */

#include <sys/types.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/types.h>
#include <linux/if.h>
#include <linux/wireless.h>
#include <netinet/in.h> 
#include <netinet/tcp.h> 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <err.h>

#include <a_config.h>
#include <a_osapi.h>
#include <athdefs.h>
#include <a_types.h>
#include <wmi.h>

#include "athdrv_linux.h"
#include "artserialagent.h"
#include <termios.h>
//#include "xmodem.h"


static int            sid, cid, aid;
static unsigned char  line[LINE_ARRAY_SIZE];
static char           ar6kifname[32];
static int            no_data_count = 0;
static unsigned char  art_htc_buf[780];
static unsigned char  art_debug = 0;


#define DEBUG_PRINT       0

#if 1
#define  ART_PORT_COM
#undef   ART_PORT_SOCKET
#else
#undef   ART_PORT_COM
#define  ART_PORT_SOCKET
#endif

static void art_print_buf(A_UINT8 *buf, A_UINT32 len)
{
#if 0 //DEBUG_PRINT
    A_UINT32  i;

    printf("Buffer len %d, buffer content: \n", len);
    for (i = 0; i < len; i++)
    {
        printf("%02x ", buf[i]);
        if ((i % 15) == 15)
            printf("\n");
    }
    printf("\n");
#else
    return;
#endif
}

int art_htc_raw_open(int sockid)
{
    struct ifreq  ifr;

    memset(&ifr, 0, sizeof(struct ifreq));
    strncpy(ifr.ifr_name, ar6kifname, sizeof(ifr.ifr_name));
    //ifr.ifr_data = (char *)malloc(4);
    ifr.ifr_data = art_htc_buf;
    ((int *)ifr.ifr_data)[0] = AR6000_XIOCTL_HTC_RAW_OPEN;
    if (ioctl(sockid, AR6000_IOCTL_EXTENDED, &ifr) < 0) {
        printf("[%s] Open ioctl for RAW HTC failed\n", __FUNCTION__);
        free(ifr.ifr_data);
        return -1;
    }
    //free(ifr.ifr_data);

    return 0;
}


int art_htc_raw_close(int sockid)
{
    struct ifreq  ifr;

    memset(&ifr, 0, sizeof(struct ifreq));
    strncpy(ifr.ifr_name, ar6kifname, sizeof(ifr.ifr_name));
    //ifr.ifr_data = (char *)malloc(4);
    ifr.ifr_data = art_htc_buf;
    ((int *)ifr.ifr_data)[0] = AR6000_XIOCTL_HTC_RAW_CLOSE;
    if (ioctl(sockid, AR6000_IOCTL_EXTENDED, &ifr) < 0) {
        printf("[%s] Open ioctl for RAW HTC failed\n", __FUNCTION__);
        free(ifr.ifr_data);
        return -1;
    }
    //free(ifr.ifr_data);

    return 0;
}

int art_htc_raw_write(int sockid, unsigned char *buf, int buflen)
{
    int             ret;
    struct ifreq    ifr;

#if DEBUG_PRINT
    printf("[%s] Enter, data length = %d\n", __FUNCTION__, buflen);
#endif
   
    memset(&ifr, 0, sizeof(struct ifreq));
    strncpy(ifr.ifr_name, ar6kifname, sizeof(ifr.ifr_name));

    //ifr.ifr_data = (char *)malloc(12 + buflen);;
    ifr.ifr_data = art_htc_buf;

    ((int *)ifr.ifr_data)[0] = AR6000_XIOCTL_HTC_RAW_WRITE;
    ((int *)ifr.ifr_data)[1] = SEND_ENDPOINT;
    ((int *)ifr.ifr_data)[2] = buflen;
    memcpy(&(((char *)(ifr.ifr_data))[12]), buf, buflen);

    if (ioctl(sockid, AR6000_IOCTL_EXTENDED, &ifr) < 0) {
        printf("[%s] ioctl for HTC raw write failed\n", __FUNCTION__);
        free(ifr.ifr_data);
        return 0;
    }

    ret = ((int *)ifr.ifr_data)[0];
#if DEBUG_PRINT
    printf("[%s] ioctl complete, return value %d\n", __FUNCTION__, ret);
#endif

    //free(ifr.ifr_data);

#if DEBUG_PRINT
    printf("[%s] Exit, return value %d\n", __FUNCTION__, ret);
#endif

    return ret;
}

int art_htc_raw_read(int sockid, unsigned char *buf, int buflen)
{
    int             ret;
    struct ifreq    ifr;

#if DEBUG_PRINT
    printf("[%s] Enter, length = %d\n", __FUNCTION__, buflen);
#endif

    memset(&ifr, 0, sizeof(struct ifreq));
    strncpy(ifr.ifr_name, ar6kifname, sizeof(ifr.ifr_name));

    //ifr.ifr_data = (char *)malloc(12 + buflen);
    ifr.ifr_data = art_htc_buf;

    ((int *)ifr.ifr_data)[0] = AR6000_XIOCTL_HTC_RAW_READ;
    ((int *)ifr.ifr_data)[1] = SEND_ENDPOINT;
    ((int *)ifr.ifr_data)[2] = buflen;

    if (ioctl(sockid, AR6000_IOCTL_EXTENDED, &ifr) < 0) {
        printf("[%s] ioctl for HTC raw read failed\n", __FUNCTION__);
        return 0;
    }
    ret = ((int *)ifr.ifr_data)[0];
#if DEBUG_PRINT
    printf("[%s] ioctl complete, return value %d\n", __FUNCTION__, ret);
#endif

    memcpy(buf, &(((char *)(ifr.ifr_data))[4]), ret);
    //free(ifr.ifr_data);

#if DEBUG_PRINT
    printf("[%s] Exit, length = %d\n", __FUNCTION__, ret);
#endif

    return ret;
}

#ifdef ART_PORT_SOCKET
int sock_init()
{
    int                sockid;
    struct sockaddr_in myaddr;
    int                sinsize;
    int                i, res;

    /* Create socket */
    sockid = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockid == -1) { 
        perror(__FUNCTION__);
        printf("Create socket to PC failed\n");
        return -1;
    } 

    i = 1;
    res = setsockopt(sockid, SOL_SOCKET, SO_REUSEADDR, (char *)&i, sizeof(i));
    if (res == -1) {
        close(sockid);
        return -1;
    }

    i = 1;
    res = setsockopt(sockid, IPPROTO_TCP, TCP_NODELAY, (char *)&i, sizeof(i));
    if (res == -1) {
        close(sockid);
        return -1;
    }


    myaddr.sin_family      = AF_INET; 
    myaddr.sin_port        = htons(ART_PORT); 
    myaddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    memset(&(myaddr.sin_zero), 0, 8);
    res = bind(sockid, (struct sockaddr *)&myaddr, sizeof(struct sockaddr));
    if (res != 0) { 
        perror(__FUNCTION__);
        printf("Bind failed\n");
		close(sockid);
        return -1;
    } 
    if (listen(sockid, 4) == -1) { 
        perror(__FUNCTION__);
        printf("Listen failed\n");
		close(sockid);
        return -1;
    } 

    printf("Waiting for client to connect...\n");
    sinsize = sizeof(struct sockaddr_in); 
    if ((cid = accept(sockid, (struct sockaddr *)&myaddr, &sinsize)) == -1) { 
        printf("Accept failed\n");
		close(cid);
		close(sockid);
        return -1;
    } 
    printf("Client connected!\n");

    return sockid;
}

void sock_wait_for_recv(int sockid)
{
    fd_set           rfds;
    struct timeval   tv;
    int              ret;
    int              tsec = 5;

    while (TRUE)
    {
        FD_ZERO(&rfds);
        FD_SET(sockid, &rfds);
        tv.tv_sec = tsec;
        tv.tv_usec = 0;

        ret = select(sockid + 1, &rfds, NULL, NULL, &tv);
        if (ret == -1)
            perror(__FUNCTION__);
        else if (ret)
            ;//printf("Data is available now, fd num %d.\n", ret);
        else
            printf("[%04d] No data within %d seconds.\n", no_data_count++, tsec);

        if (FD_ISSET(sockid, &rfds))
            break;
    }
}

int sock_send(int sockid, unsigned char *buf, int buflen)
{
    int  len;

    len = send(sockid, buf, buflen, 0);
    if (len < 0) 
    {
        perror(__FUNCTION__);
        printf("Send data to socket %d failed\n", sockid);
        exit(1);
    }

    return len;
}

int sock_recv(int sockid, unsigned char *buf, int maxlen)
{
    int  len;

    sock_wait_for_recv(sockid);
    len = recv(sockid, buf, maxlen, 0);
    if (len < 0)
    {
        perror(__FUNCTION__);
        printf("Receive data from socket %d failed\n", sockid);
        exit(1);
    }

    return len;
}

int art_recv_from_host(int sockid, unsigned char *buf, int pktlen)
{
    int        len, totallen;

    len = sock_recv(sockid, buf, pktlen);
    if (len == pktlen)
    {
#if DEBUG_PRINT
        printf("[%s] Received %d bytes from ART host\n", __FUNCTION__, len);
#endif
        return len;
    }

    /* Fragmented packets */
    totallen = 0;
    while (TRUE)
    {
        /* Send ACK */
        sock_send(sockid, buf, 2);

#if DEBUG_PRINT
        printf("[%s][Fragment] Received %d bytes from ART host\n", __FUNCTION__, len);
#endif

        totallen += len;
        buf      += len;
        pktlen   -= len;
        if (pktlen == 0)
            break;

        len = sock_recv(sockid, buf, pktlen);
    }

#if DEBUG_PRINT
    printf("[%s] Finally received %d bytes from ART host\n", __FUNCTION__, totallen);
#endif

    return totallen;
}
#endif

#ifdef ART_PORT_COM
int com_init(char *comname)
{
    int  fd1 = -1;
    int  fd2 = -1;
    int  len;
    char buf[10] = "";
    struct termios term_params;

    /* Close physical COM */
    fd1 = open("/tmp/ash.pid", O_RDWR);
    if (fd1 >=0 )
    {
        len = read(fd1, buf, 9);
       // kill(atoi(buf), 30+11);
    }
    close(fd1);

    /* Open USB-COM */
    if((fd2 = open(comname, O_RDWR)) < 0)
    {
        printf( "open console dev fail\n");
        return -1;
    }
    printf( "Open console dev success = %d\n", fd2);

    if (tcgetattr (fd2, &term_params) < 0)
    {
        printf ("tcgetattr() call fails\n");
        return -1;
    }

    term_params.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    term_params.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    term_params.c_cflag &= ~(CSIZE | PARENB);
    term_params.c_cflag |= CS8;
    term_params.c_oflag &= ~(OPOST);
    term_params.c_cc[VMIN] = 1;
    term_params.c_cc[VTIME] = 0;

    if (tcsetattr (fd2, TCSAFLUSH, &term_params) < 0)
    {
        printf ("tcsetattr() call fails\n");
        return -1;
    }

    return fd2;
}

void com_wait_for_recv(int comfd)
{
    fd_set           rfds;
    struct timeval   tv;
    int              ret;
    int              tsec = 5;

    while (TRUE)
    {
        FD_ZERO(&rfds);
        FD_SET(comfd, &rfds);
        tv.tv_sec = tsec;
        tv.tv_usec = 0;

        ret = select(comfd + 1, &rfds, NULL, NULL, &tv);
        if (ret == -1)
            perror(__FUNCTION__);
        else if (ret)
            ;//printf("Data is available now, fd num %d.\n", ret);
        else
            printf("[%04d] No data within %d seconds.\n", no_data_count++, tsec);

        if (FD_ISSET(comfd, &rfds))
            break;
    }
}


int com_recv(int comfd, unsigned char *buf, int maxlen)
{
    int  len, totallen;

    com_wait_for_recv(comfd);
#if DEBUG_PRINT
    printf("[%s] Data is available now, maxlen %d\n", __FUNCTION__, maxlen);
#endif

    totallen = 0;
    while (TRUE)
    {
        //printf("[%s] Receiving...\n", __FUNCTION__);
        len = read(comfd, buf, maxlen);
        if (len <= 0)
        {
            printf("Receive data from COM %d failed\n", comfd);
            exit(1);
        }

        totallen += len;
        if (len == maxlen)
            break;

        buf      += len;
        maxlen   -= len;
        printf("[%s][Fragment] Received %d bytes\n", __FUNCTION__, len);
    }

#if DEBUG_PRINT
    printf("[%s] Received %d bytes from ART host\n", __FUNCTION__, totallen);
#endif
    art_print_buf(buf, totallen);

    return totallen;
}

int com_send(int comfd, unsigned char *buf, int buflen)
{
    int  len;

    len = write(comfd, buf, buflen);
    if (len <= 0) 
    {
        perror(__FUNCTION__);
        printf("Send data to COM %d failed\n", comfd);
        exit(1);
    }

    return len;
}

int daemon(int nochdir, int noclose )
{
    int fd = -1;

    switch ( fork() ) 
    {
        case -1:
    	        return -1;
        case 0:
            break;
        default:
            exit(0);
    }

    if (setsid() == -1)
    {
        return -1;
    }

    if (!nochdir)
    {
        chdir("/");
    }
    

    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);

    printf("fd = %d\n", fd);

    while (fd > 2) close(fd--);
    
    return 0;
}

#endif

void usage(char *pname)
{
    printf("An agent that connects ART host and AR6K device. Must be started\n");
    printf("after AR6K device driver is successfully installed.\n\n");
    printf("Usage: %s ifname comx fragsize\n\n", pname);
    printf("  ifname    AR6K interface name\n");
    printf("  comx      COM port name\n");
    printf("  fragsize  Fragment size, must be multiple of 4\n\n");
    printf("Example:\n");
    printf("  %s eth0 /dev/ttyHSUSB0 80\n\n", pname);
}

int main (int argc, char **argv)
{
    int              pktlen, len, ret;
    unsigned char    *pos;
    int              fragsize = 80;

    if (argc < 4)
    {
        usage(argv[0]);
        goto main_exit;
    }

    if (argc >= 5)
        art_debug = atoi(argv[4]);

    /* Connect to AR6002 */
    memset(ar6kifname, '\0', sizeof(ar6kifname));
    strcpy(ar6kifname, argv[1]);
    
    if (art_debug == 0)
    {
	    aid = socket(AF_INET, SOCK_DGRAM, 0);
	    if (aid < 0) 
	    {
		    printf("Create socket to AR6002 failed\n");
		    goto main_exit;
	    }

	    if (art_htc_raw_open(aid) < 0)
	    {
		    printf("HTC RAW open failed\n");
		    goto main_exit;
	    }
    }

#ifdef ART_PORT_SOCKET
    sid = sock_init();
    if (sid < 0) {
        printf("Create socket to ART failed\n");
        goto main_exit;
    }
#else
    //daemon(0, 0);
    cid = com_init(argv[2]);
#endif

    fragsize = atoi(argv[3]);
    if ((fragsize <= 0) || ((fragsize % 4) != 0))
    {
        printf("fragsize must be multiple of 4\n");
        goto main_exit;
    }
    else
    {
        printf("Use fragsize %d\n", fragsize);
    }
    
    if (art_debug == 0)
    {
	    /* Main loop */
	    while (TRUE)
	    {
		    /* Receive from ART host */
		    len = com_recv(cid, line, 4);
		    memcpy((unsigned char *)&pktlen, line, 4);
#if 1//DEBUG_PRINT
		    printf("Will read %d bytes from ART host\n", pktlen);
#endif
		    len = com_recv(cid, line, pktlen);

		    if (art_debug == 0)
		    {
			    if ((line[2] == DISCONNECT_PIPE_CMD_ID) || (line[2] == CLOSE_PIPE_CMD_ID))
			    {
				    printf("Received disconnect/close command, program exit.\n");
				    art_htc_raw_close(aid);
				    break;
			    }

			    /* Write to AR6002 */
			    pos = line;
			    while (len)
			    {
				    if (len > fragsize)
					    ret = art_htc_raw_write(aid, pos, fragsize);
				    else
					    ret = art_htc_raw_write(aid, pos, len);
				    pos += ret;
				    len -= ret;
			    }

			    /* Read from AR6002 */
			    len = art_htc_raw_read(aid, line, 4);
			    if (len == 4)
			    {
				    art_print_buf(line, 4);
				    com_send(cid, line, 4);
				    memcpy((unsigned char *)&pktlen, line, 4);
				    art_htc_raw_read(aid, line, pktlen);
				    art_print_buf(line, pktlen);
				    com_send(cid, line, pktlen);
			    }
			    else
			    {
				    printf("Get %d bytes reply from AR6002, expects 4\n", len);
				    com_send(cid, line, len);
			    }
		    }
	    }

	    //recv_file(cid, "/system/wifi/U8220.bin");
	    //printf("get U8220.bin\n");
	    //printf("sucess\n");
    }
    else 
    {
          //recv_file(cid, "/system/wifi/U8220.bin");
    }

main_exit:
    close(cid);
    close(sid);
    close(aid);

    return 0;
}


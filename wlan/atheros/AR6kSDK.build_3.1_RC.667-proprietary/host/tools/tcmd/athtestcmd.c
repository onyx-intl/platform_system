/*
 * Copyright (c) 2006 Atheros Communications Inc.
 * All rights reserved.
 * 
 * 
// The software source and binaries included in this development package are
// licensed, not sold. You, or your company, received the package under one
// or more license agreements. The rights granted to you are specifically
// listed in these license agreement(s). All other rights remain with Atheros
// Communications, Inc., its subsidiaries, or the respective owner including
// those listed on the included copyright notices.  Distribution of any
// portion of this package must be in strict compliance with the license
// agreement(s) terms.
// </copyright>
// 
// <summary>
// 	Wifi driver for AR6003
// </summary>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <a_config.h>
#include <a_osapi.h>
#include <athdefs.h>
#include <a_types.h>
#include <wmi.h>
#include <testcmd.h>
#include <regDb.h>

#include "athdrv_linux.h"
#include "athtestcmd.h"

#ifdef ATHTESTCMD_LIB
#include <setjmp.h>
extern void testcmd_error(int code, const char *fmt, ...);
#define A_ERR testcmd_error
#else
#define A_ERR err
#endif

const char *progname;
const char commands[] =
"commands:\n\
--tx <frame/tx99/tx100/off> --txfreq <Tx channel or freq(default 2412)> --txrate <rate index> \n\
      --txpwr <frame/tx99/tx100: 0-30dBm,0.5dBm resolution; sine: 0-60, PCDAC vaule> --txantenna <1/2/0 (auto)>\n\
      --txpktsz <pkt size, [32-1500](default 1500)>\n\
      --txpattern <tx data pattern, 0: all zeros; 1: all ones; 2: repeating 10; 3: PN7; 4: PN9; 5: PN15\n\
      --ani (Enable ANI. The ANI is disabled if this option is not specified)\n\
      --scrambleroff (Disable scrambler. The scrambler is enabled by default)\n\
      --aifsn <AIFS slots num,[0-252](Used only under '--tx frame' mode)>\n\
      --shortguard (use short guard)\n\
      --mode <ht40plus/ht40minus/ht20>\n\
      --setlongpreamble <1/0>\n\
      --numpackets <number of packets to send 0-65535>\n\
--tx sine --txfreq <Tx channel or freq(default 2412)>\n\
--rx <promis/filter/report> --rxfreq <Rx channel or freq(default 2412)> --rxantenna <1/2/0 (auto)> --mode <ht40plus/ht40minus>\n\
--pm <wakeup/sleep/deepsleep>\n\
--setmac <mac addr like 00:03:7f:be:ef:11>\n\
--SetAntSwitchTable <table1 in hex value> <table2 in hex value>  (Set table1=0 and table2=0 will restore the default AntSwitchTable)\n\
--efusedump --start <start address> --end <end address>\n\
--efusewrite --start <start address> --data <data> (could be one or multiple data in quotation marks)\n\
--otpwrite --data (could be one or multiple data in quotation marks)\n\
--otpdump\n\
";

#define INVALID_FREQ    0

#define A_RATE_NUM      28 
#define G_RATE_NUM      28

#define RATE_STR_LEN    20
#define VENUS_OTP_SIZE  512
static TC_CMDS sTcCmds;
typedef const char RATE_STR[RATE_STR_LEN];

const RATE_STR  bgRateStrTbl[G_RATE_NUM] = {
    { "1   Mb" },
    { "2   Mb" },
    { "5.5 Mb" },
    { "11  Mb" },
    { "6   Mb" },
    { "9   Mb" },
    { "12  Mb" },
    { "18  Mb" },
    { "24  Mb" },
    { "36  Mb" },
    { "48  Mb" },
    { "54  Mb" },
    { "HT20 MCS0 6.5  Mb" },
    { "HT20 MCS1 13  Mb" },
    { "HT20 MCS2 19.5  Mb" },
    { "HT20 MCS3 26  Mb" },
    { "HT20 MCS4 39  Mb" },
    { "HT20 MCS5 52  Mb" },
    { "HT20 MCS6 58.5  Mb" },
    { "HT20 MCS7 65  Mb" },
    { "HT40 MCS0 13.5  Mb" },
    { "HT40 MCS1 27.0  Mb" },
    { "HT40 MCS2 40.5  Mb" },
    { "HT40 MCS3 54  Mb" },
    { "HT40 MCS4 81  Mb" },
    { "HT40 MCS5 108  Mb" },
    { "HT40 MCS6 121.5  Mb" },
    { "HT40 MCS7 135  Mb" }	
};

static void rxReport(void *buf);
static A_UINT32 freqValid(A_UINT32 val);
static A_UINT16 wmic_ieee2freq(A_UINT32 chan);
static void prtRateTbl(A_UINT32 freq);
static A_UINT32 rateValid(A_UINT32 val, A_UINT32 freq);
static A_UINT32 antValid(A_UINT32 val);
static A_UINT32 txPwrValid(TCMD_CONT_TX *txCmd);
static A_STATUS ath_ether_aton(const char *orig, A_UINT8 *eth);
static A_UINT32 pktSzValid(A_UINT32 val);

static A_BOOL isHex(char c) { 
    return (((c >= '0') && (c <= '9')) ||
            ((c >= 'A') && (c <= 'F')) ||
            ((c >= 'a') && (c <= 'f')));
}

static void
usage(void)
{
    fprintf(stderr, "usage:\n%s [-i device] commands\n", progname);
    fprintf(stderr, "%s\n", commands);
    prtRateTbl(INVALID_FREQ);
    A_ERR(-1, "Incorrect usage");
}

#ifdef ATHTESTCMD_LIB
static int parseCmd(const char *cmdline, char *buf, char **argv, size_t argvlen)
{
    int argc = 0;
    char *token = buf;
    strcpy(buf, cmdline);

    while ( *token && isspace(*token) )
            ++token;
    while (*token)
    {
        if (argc>=argvlen)
        {
            break;           
        }
        argv[argc++] = token;
        while ( *token && !isspace(*token) )
            ++token;
        if (*token)
        {
            *token++ = '\0';
            while ( *token && isspace(*token) )
                ++token;
        }
    }
    if (argc==0 || (argc>0 && strcmp(argv[0], "athtestcmd")!=0))
    {
        argv[0] = "athtestcmd";
    }
    return argc;
}

int tcmd_exec(const char *cmdline, void (*reportCB)(void *), jmp_buf *jbuf)
{
#define MAX_ARGS 30
    char cmdbuf[2048];
    char *argv[MAX_ARGS]; /* max 30 arguments */
    int argc = parseCmd(cmdline, cmdbuf, argv, MAX_ARGS);
#else

static void cmdReplyFunc_readThermal(void *buf)
{
    A_UINT8 *reply = (A_UINT8*)buf;
    TC_CMDS tCmdReply;
    tCmdReply.hdr.u.parm.length = *(A_UINT16 *)&(reply[0]);
    tCmdReply.hdr.u.parm.version = (A_UINT8)(reply[2]);
    memcpy((void*)&(tCmdReply.buf), (void*)(buf+4),  tCmdReply.hdr.u.parm.length); 

    printf("chip thermal value:%d\n", tCmdReply.buf[0]); 
//    printf("reply len %d ver %d act %d buf %x %x %x %x %x %x %x %x\n", tCmdReply.hdr.u.parm.length, tCmdReply.hdr.u.parm.version, tCmdReply.hdr.act, tCmdReply.buf[0], tCmdReply.buf[1],tCmdReply.buf[2], tCmdReply.buf[3], tCmdReply.buf[4],tCmdReply.buf[5], tCmdReply.buf[6], tCmdReply.buf[7]);
    return;
}

static void cmdReplyFunc(void *buf)
{
    A_UINT8  *reply = (A_UINT8*)buf;
    TC_CMDS  tCmdReply;
    A_UINT32 act;
    /* A_UINT32 i; */

    tCmdReply.hdr.u.parm.length = *(A_UINT16 *)&(reply[0]);
    tCmdReply.hdr.u.parm.version = (A_UINT8)(reply[2]);
    act = tCmdReply.hdr.u.parm.version;

    /* Error Check */
    if (tCmdReply.hdr.u.parm.length > (TC_CMDS_SIZE_MAX + 1)) {
        printf("Error: Reply lenth=%d, limit=%d\n", tCmdReply.hdr.u.parm.length, TC_CMDS_SIZE_MAX);
        return;
    } else {
        printf(">> Reply length = %d, type = %d ", tCmdReply.hdr.u.parm.length, tCmdReply.hdr.u.parm.version);
    }

    switch (act) {
        case TC_CMDS_EFUSEDUMP:
            printf("eFuse data:\n");
            break;
        case TC_CMDS_EFUSEWRITE:
            printf("(write eFuse data)\n");
            break;
        case TC_CMDS_OTPSTREAMWRITE:
            printf("(OTP stream write)\n");
            break;
        case TC_CMDS_OTPDUMP:
            printf("OTP Dump\n");
            break;
        default:
            printf("Invalid action!\n");
            break;
    }

    if (tCmdReply.hdr.u.parm.length > 0) {
        /* Copy the data field */
        memcpy((void*)&(tCmdReply.buf), (void*)(buf+4),  tCmdReply.hdr.u.parm.length);

        memcpy((void*)&(sTcCmds.buf[0]), (void*)&(tCmdReply.buf[0]),  tCmdReply.hdr.u.parm.length);
        sTcCmds.hdr.u.parm.length = tCmdReply.hdr.u.parm.length;
    }

//    printf("reply len %d ver %d act %d buf %x %x %x %x %x %x %x %x\n", tCmdReply.hdr.u.parm.length, tCmdReply.hdr.u.parm.version, tCmdReply.hdr.act, tCmdReply.buf[0], tCmdReply.buf[1],tCmdReply.buf[2], tCmdReply.buf[3], tCmdReply.buf[4],tCmdReply.buf[5], tCmdReply.buf[6], tCmdReply.buf[7]);
//
    
    return;
}



int main (int argc, char **argv)
{
    void (*reportCB)(void *) = NULL;
    void (*cmdRespCB)(void *) = cmdReplyFunc_readThermal;
#endif /* ATHTESTCMD_LIB */
    int c, s=-1;
    char ifname[IFNAMSIZ];
    unsigned int cmd = 0;
    progname = argv[0];
    struct ifreq ifr;
    char buf[2048];
    TCMD_CONT_TX *txCmd = (TCMD_CONT_TX *)((A_UINT32 *)buf + 1); /* first 32-bit is XIOCTL_CMD */
    TCMD_CONT_RX *rxCmd   = (TCMD_CONT_RX *)((A_UINT32 *)buf + 1);
    TCMD_PM *pmCmd = (TCMD_PM *)((A_UINT32 *)buf + 1);
    WMI_SET_LPREAMBLE_CMD *setLpreambleCmd = (WMI_SET_LPREAMBLE_CMD *)((A_UINT32 *)buf + 1);
    TCMD_SET_REG *setRegCmd = (TCMD_SET_REG *)((A_UINT32 *)buf + 1);
    TC_CMDS  *tCmds = (TC_CMDS *)((A_UINT32 *)buf + 1);
    A_BOOL needRxReport = FALSE;    
#ifndef ATHTESTCMD_LIB
    A_UINT16 efuse_begin = 0, efuse_end = (VENUS_OTP_SIZE - 1);
    A_UINT8  efuseBuf[VENUS_OTP_SIZE];
    A_UINT8  efuseWriteBuf[VENUS_OTP_SIZE];
    A_UINT16 data_length = 0;
#endif
    txCmd->numPackets = 0;
    txCmd->wlanMode = TCMD_WLAN_MODE_NOHT;
    txCmd->tpcm = TPC_TX_PWR;
/* default to tx power */
    rxCmd->u.para.wlanMode = TCMD_WLAN_MODE_NOHT;
#ifdef ATHTESTCMD_LIB
    if (setjmp(*jbuf)!=0) {
        if (s>=0)
            close(s);
        return -1;
    }
#endif 

    if (argc == 1) {
        usage();
    }

    memset(buf, 0, sizeof(buf));
    memset(ifname, '\0', IFNAMSIZ);
    strcpy(ifname, "eth1");
    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
        A_ERR(1, "socket");
    }

    while (1) {
        int option_index = 0;
        static struct option long_options[] = {
            {"version", 0, NULL, 'v'},
            {"interface", 1, NULL, 'i'},
            {"tx", 1, NULL, 't'},
            {"txfreq", 1, NULL, 'f'},
            {"txrate", 1, NULL, 'g'},
            {"txpwr", 1, NULL, 'h'},
            {"tgtpwr", 0, NULL, 'H'},
            {"pcdac", 1, NULL, 'I'},
            {"txantenna", 1, NULL, 'j'},
            {"txpktsz", 1, NULL, 'z'},
            {"txpattern", 1, NULL, 'e'},
            {"rx", 1, NULL, 'r'},
            {"rxfreq", 1, NULL, 'p'},
            {"rxantenna", 1, NULL, 'q'},
            {"pm", 1, NULL, 'x'},
            {"setmac", 1, NULL, 's'},
            {"ani", 0, NULL, 'a'},
            {"scrambleroff", 0, NULL, 'o'},
            {"aifsn", 1, NULL, 'u'},
            {"SetAntSwitchTable", 1, NULL, 'S'},
            {"shortguard", 0, NULL, 'G'},
            {"numpackets", 1, NULL, 'n'},
            {"mode", 1, NULL, 'M'},
            {"setlongpreamble", 1, NULL, 'l'},
            {"setreg", 1, NULL, 'R'},                  
            {"regval", 1, NULL, 'V'},                
            {"flag", 1, NULL, 'F'},     
            {"writeotp", 0, NULL, 'w'},  	
            {"otpregdmn", 1, NULL, 'E'},		
#ifndef ATHTESTCMD_LIB
            {"efusedump", 0, NULL, 'm'},
            {"efusewrite", 0, NULL, 'W'},
            {"start", 1, NULL, 'A'},
            {"end", 1, NULL, 'L'},
            {"data", 1, NULL, 'U'},
            {"otpwrite", 0, NULL, 'O'},
            {"otpdump", 0, NULL, 'P'},
#endif
            {"btaddr", 1, NULL, 'B'},			
            {"therm", 0, NULL, 'c'},
            {0, 0, 0, 0}
        };

        c = getopt_long(argc, argv, "vi:t:f:g:h:HI:r:p:q:x:u:ao:M:A:L:mU:WOP",
                         long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'i':
            memset(ifname, '\0', 8);
            strcpy(ifname, optarg);
            break;
        case 't':
            cmd = TESTMODE_CONT_TX;
			txCmd->testCmdId = TCMD_CONT_TX_ID;
            if (!strcmp(optarg, "sine")) {
                txCmd->mode = TCMD_CONT_TX_SINE;
            } else if (!strcmp(optarg, "frame")) {
                txCmd->mode = TCMD_CONT_TX_FRAME;
            } else if (!strcmp(optarg, "tx99")) {
                txCmd->mode = TCMD_CONT_TX_TX99;
            } else if (!strcmp(optarg, "tx100")) {
                txCmd->mode = TCMD_CONT_TX_TX100;
            } else if (!strcmp(optarg, "off")) {
                txCmd->mode = TCMD_CONT_TX_OFF;
            }else {
                cmd = 0;
            }
            break;
        case 'f':
            txCmd->freq = freqValid(atoi(optarg));
            break;
        case 'G':
            txCmd->shortGuard = 1;
            break;
        case 'M':
            if(cmd == TESTMODE_CONT_TX) {
                if (!strcmp(optarg, "ht20")) {
                    txCmd->wlanMode = TCMD_WLAN_MODE_HT20;
                } else if (!strcmp(optarg, "ht40plus")) {
                    txCmd->wlanMode = TCMD_WLAN_MODE_HT40PLUS;
                } else if (!strcmp(optarg, "ht40minus")) {
                    txCmd->wlanMode = TCMD_WLAN_MODE_HT40MINUS;
                }
            } else if(cmd == TESTMODE_CONT_RX) {
                if (!strcmp(optarg, "ht20")) {
                    rxCmd->u.para.wlanMode = TCMD_WLAN_MODE_HT20;
                } else if (!strcmp(optarg, "ht40plus")) {
                    rxCmd->u.para.wlanMode = TCMD_WLAN_MODE_HT40PLUS;
                } else if (!strcmp(optarg, "ht40minus")) {
                    rxCmd->u.para.wlanMode = TCMD_WLAN_MODE_HT40MINUS;
                }
            }
            break;
        case 'n':
            txCmd->numPackets = atoi(optarg);
            break;
        case 'g':
            /* let user input index of rateTable instead of string parse */
            txCmd->dataRate = rateValid(atoi(optarg), txCmd->freq);
            break;
        case 'h':
        {
            int txPowerAsInt;
            /* Get tx power from user.  This is given in the form of a number
             * that's supposed to be either an integer, or an integer + 0.5
             */
            double txPowerIndBm = atof(optarg);

            /*
             * Check to make sure that the number given is either an integer
             * or an integer + 0.5
             */
            txPowerAsInt = (int)txPowerIndBm;
            if (((txPowerIndBm - (double)txPowerAsInt) == 0) ||
                (((txPowerIndBm - (double)txPowerAsInt)) == 0.5) ||
                (((txPowerIndBm - (double)txPowerAsInt)) == -0.5)) {
                if (txCmd->mode != TCMD_CONT_TX_SINE) {
                    txCmd->txPwr = txPowerIndBm * 2;
                } else {
                    txCmd->txPwr = txPowerIndBm;
                }
           } else {
                printf("Bad argument to --txpwr, must be in steps of 0.5 dBm\n");
                cmd = 0;
           }
             
            txCmd->tpcm = TPC_TX_PWR;
        }
            break;
        case 'H':
            txCmd->tpcm = TPC_TGT_PWR;
            break;
        case 'I':
            txCmd->tpcm = TPC_FORCED_GAIN;
            txCmd->txPwr = atof(optarg);
            break;
        case 'j':
            txCmd->antenna = antValid(atoi(optarg));
            break;       
        case 'z':
            txCmd->pktSz = pktSzValid(atoi(optarg));
            break;
        case 'e':
            txCmd->txPattern = atoi(optarg);
            break;
        case 'r':
            cmd = TESTMODE_CONT_RX;
			rxCmd->testCmdId = TCMD_CONT_RX_ID;
            if (!strcmp(optarg, "promis")) {
                rxCmd->act = TCMD_CONT_RX_PROMIS;
			 	printf(" Its cont Rx promis mode \n");
            } else if (!strcmp(optarg, "filter")) {
                rxCmd->act = TCMD_CONT_RX_FILTER;
				printf(" Its cont Rx  filter  mode \n");
            } else if (!strcmp(optarg, "report")) {
				 printf(" Its cont Rx report  mode \n");
                rxCmd->act = TCMD_CONT_RX_REPORT;
                needRxReport = TRUE;
            } else {
                cmd = 0;
            }
            break;
        case 'p':
            rxCmd->u.para.freq = freqValid(atoi(optarg));
            break;
        case 'q':
            rxCmd->u.para.antenna = antValid(atoi(optarg));
            break;
        case 'x':
            cmd = TESTMODE_PM;
			pmCmd->testCmdId = TCMD_PM_ID;
            if (!strcmp(optarg, "wakeup")) {
                pmCmd->mode = TCMD_PM_WAKEUP;
            } else if (!strcmp(optarg, "sleep")) {
                pmCmd->mode = TCMD_PM_SLEEP;
            } else if (!strcmp(optarg, "deepsleep")) {
                pmCmd->mode = TCMD_PM_DEEPSLEEP;
            } else {
                cmd = 0;
            }
            break;
        case 's':
            {
                A_UINT8 mac[ATH_MAC_LEN];

                cmd = TESTMODE_CONT_RX;
                rxCmd->testCmdId = TCMD_CONT_RX_ID;
                rxCmd->act = TCMD_CONT_RX_SETMAC;
                if (ath_ether_aton(optarg, mac) != A_OK) {
                    A_ERR(-1, "Invalid mac address format! \n");
                }
                memcpy(rxCmd->u.mac.addr, mac, ATH_MAC_LEN);
#ifdef TCMD_DEBUG
                printf("JLU: tcmd: setmac 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x\n", 
                        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
#endif
                break;
            }
        case 'u':
            {
                txCmd->aifsn = atoi(optarg) & 0xff;
                printf("AIFS:%d\n", txCmd->aifsn);
            }
            break;
        case 'a':
            if(cmd == TESTMODE_CONT_TX) {
                txCmd->enANI = TRUE;
            } else if(cmd == TESTMODE_CONT_RX) {
                rxCmd->enANI = TRUE;
            }
            break;
        case 'o':
            txCmd->scramblerOff = TRUE;
            break;
        case 'S':
            if (argc < 4)
                usage();
            cmd = TESTMODE_CONT_RX;
            rxCmd->testCmdId = TCMD_CONT_RX_ID;		
            rxCmd->act = TCMD_CONT_RX_SET_ANT_SWITCH_TABLE;				
            rxCmd->u.antswitchtable.antswitch1 = strtoul(argv[2], (char **)NULL,0);
            rxCmd->u.antswitchtable.antswitch2 = strtoul(argv[3], (char **)NULL,0);
            break;
        case 'l':
            cmd = TESTMODE_SETLPREAMBLE;
            setLpreambleCmd->status = atoi(optarg);
            break;
        case 'R':
            if (argc < 5) {
                printf("usage:athtestcmd -i eth0 --setreg 0x1234 --regval 0x01 --flag 0\n");
            }
            cmd = TESTMODE_SETREG;
            setRegCmd->testCmdId = TCMD_SET_REG_ID;
            setRegCmd->regAddr   = strtoul(optarg, (char **)NULL, 0);//atoi(optarg);
            break; 
        case 'V':
            setRegCmd->val = strtoul(optarg, (char **)NULL, 0);
            break;            
        case 'F':
            setRegCmd->flag = atoi(optarg);
            break;                       
        case 'w':
            rxCmd->u.mac.otpWriteFlag = 1;	
            break;		
        case 'E':
            rxCmd->u.mac.regDmn[0] = 0xffff&(strtoul(optarg, (char **)NULL, 0));
            rxCmd->u.mac.regDmn[1] = 0xffff&(strtoul(optarg, (char **)NULL, 0)>>16);		
            break;		
        case 'B':
            {    				           
                A_UINT8 btaddr[ATH_MAC_LEN];
                if (ath_ether_aton(optarg, btaddr) != A_OK) {
                    A_ERR(-1, "Invalid mac address format! \n");
                } 
                memcpy(rxCmd->u.mac.btaddr, btaddr, ATH_MAC_LEN);
#ifdef TCMD_DEBUG
                printf("JLU: tcmd: setbtaddr 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x\n", 
                        btaddr[0], btaddr[1], btaddr[2], btaddr[3], btaddr[4], btaddr[5]);
#endif		
            }
            break;		
        case 'c':
            cmd = TESTMODE_CMDS;
            tCmds->hdr.testCmdId = TC_CMDS_ID;
            {
                tCmds->hdr.u.parm.length = (A_UINT16)0;    
                tCmds->hdr.u.parm.version = TC_CMDS_VERSION_TS;    
                tCmds->hdr.act = TC_CMDS_READTHERMAL;//TC_CMDS_CAL_THERMALVOLT;
            }
            break;
	
#ifndef ATHTESTCMD_LIB
        case 'A':
            efuse_begin = atoi(optarg);
            break;

        case 'L':
            efuse_end = atoi(optarg);
            break;

        case 'U':
            {
                A_UINT8* pucArg = (A_UINT8*)optarg;
                A_UINT8  c;
                A_UINT8  strBuf[256];
                A_UINT8  pos = 0;
                A_UINT16 length = 0;
                A_UINT32  data;

                /* Sweep string to end */
                while (1) {
                    c = *pucArg++;
                    if (isHex(c)) {
                        strBuf[pos++] = c;
                    } else {
                        strBuf[pos] = '\0';
                        pos = 0;
                        sscanf(((char *)&strBuf), "%x", &data);
                        efuseWriteBuf[length++] = (data & 0xFF);

                        /* End of arg string */
                        if (c == '\0') {
                            break;
                        }
                    }
                }

                data_length = length;
            }
            break;

        case 'm':
            cmd = TESTMODE_CMDS;
            tCmds->hdr.testCmdId      = TC_CMDS_ID;
            tCmds->hdr.act            = TC_CMDS_EFUSEDUMP;
            cmdRespCB = cmdReplyFunc;
            break;

        case 'W':
            cmd = TESTMODE_CMDS;
            tCmds->hdr.testCmdId      = TC_CMDS_ID;
            tCmds->hdr.act            = TC_CMDS_EFUSEWRITE;
            cmdRespCB = cmdReplyFunc;
            break;

        case 'O':
            cmd = TESTMODE_CMDS;
            tCmds->hdr.testCmdId      = TC_CMDS_ID;
            tCmds->hdr.act            = TC_CMDS_OTPSTREAMWRITE;
            cmdRespCB = cmdReplyFunc;
            break;

        case 'P':
            cmd = TESTMODE_CMDS;
            tCmds->hdr.testCmdId      = TC_CMDS_ID;
            tCmds->hdr.act            = TC_CMDS_OTPDUMP;
            cmdRespCB = cmdReplyFunc;
            break;
#endif		
        default:
            usage();
        }
    }

    strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));

    switch (cmd) {
    case TESTMODE_CONT_TX:
#ifdef CONFIG_HOST_TCMD_SUPPORT
        *(A_UINT32 *)buf = AR6000_XIOCTL_TCMD_CONT_TX;

        txPwrValid(txCmd);

        ifr.ifr_data = (void *)buf;
        if (ioctl(s, AR6000_IOCTL_EXTENDED, &ifr) < 0) {
            A_ERR(1, "%s", ifr.ifr_name);
        }
#endif /* CONFIG_HOST_TCMD_SUPPORT */
        break;
    case TESTMODE_CONT_RX:
#ifdef CONFIG_HOST_TCMD_SUPPORT
        *(A_UINT32 *)buf = AR6000_XIOCTL_TCMD_CONT_RX;

        if (rxCmd->act == TCMD_CONT_RX_PROMIS ||
             rxCmd->act == TCMD_CONT_RX_FILTER) {
            if (rxCmd->u.para.freq == 0)
                rxCmd->u.para.freq = 2412;
        }

        ifr.ifr_data = (void *)buf;
        if (ioctl(s, AR6000_IOCTL_EXTENDED, &ifr) < 0) {
            A_ERR(1, "%s", ifr.ifr_name);
        }
        if (reportCB) {
            reportCB(ifr.ifr_data);
        }
        if (needRxReport) {
            rxReport(ifr.ifr_data);
            needRxReport = FALSE;
        }
#endif /* CONFIG_HOST_TCMD_SUPPORT */
        break;
    case TESTMODE_PM:
#ifdef CONFIG_HOST_TCMD_SUPPORT
        *(A_UINT32 *)buf = AR6000_XIOCTL_TCMD_PM;
        ifr.ifr_data = (void *)buf;
        if (ioctl(s, AR6000_IOCTL_EXTENDED, &ifr) < 0) {
            A_ERR(1, "%s", ifr.ifr_name);
        }
#endif /* CONFIG_HOST_TCMD_SUPPORT */
        break;
    case TESTMODE_SETLPREAMBLE:
        *(A_UINT32 *)buf = AR6000_XIOCTL_WMI_SET_LPREAMBLE;
        ifr.ifr_data = (void *)buf;
        if (ioctl(s, AR6000_IOCTL_EXTENDED, &ifr) < 0) {
            A_ERR(1, "%s", ifr.ifr_name);
        }   
        break;   
    case TESTMODE_SETREG:
        *(A_UINT32 *)buf = AR6000_XIOCTL_TCMD_SETREG;
        ifr.ifr_data = (void *)buf;
        if (ioctl(s, AR6000_IOCTL_EXTENDED, &ifr) < 0) {
            printf("%s", ifr.ifr_name);
        }   
        break;
#ifndef ATHTESTCMD_LIB              
    case TESTMODE_CMDS:
        if (tCmds->hdr.act == TC_CMDS_EFUSEDUMP) {
            int i, k;
            int blkNum;
            A_UINT16 efuseEnd   = efuse_end;
            A_UINT16 efuseBegin = efuse_begin;
            A_UINT16 efusePrintAnkor;
            A_UINT16 numPlaceHolder;

            /* Input check */
            if (efuseEnd > (VENUS_OTP_SIZE - 1)) {
                efuseEnd = (VENUS_OTP_SIZE - 1);
            }

            if (efuseBegin > efuseEnd) {
                efuseBegin = efuseEnd;
            }

            efusePrintAnkor = efuseBegin;

            blkNum = ((efuseEnd - efuseBegin) / TC_CMDS_SIZE_MAX) + 1;

            /* Request data in several trys */
            for (i = 0; i < blkNum; i++) {
                tCmds->hdr.testCmdId      = TC_CMDS_ID;
                tCmds->hdr.act            = TC_CMDS_EFUSEDUMP;
                tCmds->hdr.u.parm.length  = 4;    
                tCmds->hdr.u.parm.version = TC_CMDS_VERSION_TS;  

                tCmds->buf[0] = (efuseBegin & 0xFF);
                tCmds->buf[1] = (efuseBegin >> 8) & 0xFF;
                tCmds->buf[2] = (efuseEnd & 0xFF);
                tCmds->buf[3] = (efuseEnd >> 8) & 0xFF;      

                *(A_UINT32 *)buf = AR6000_XIOCTL_TCMD_CMDS;

                ifr.ifr_data = (void *)buf;

                if (ioctl(s, AR6000_IOCTL_EXTENDED, &ifr) < 0) {
                    A_ERR(1,"%s", ifr.ifr_name);
                }

                if (cmdRespCB) {
                    cmdRespCB(ifr.ifr_data);
                }

                /* Last block? */
                if ((efuseEnd - efuseBegin + 1) < TC_CMDS_SIZE_MAX) {
                    memcpy((void*)(efuseBuf + efuseBegin), (void*)&(sTcCmds.buf[0]), (efuseEnd - efuseBegin + 1));
                } else {
                    memcpy((void*)(efuseBuf + efuseBegin), (void*)&(sTcCmds.buf[0]), TC_CMDS_SIZE_MAX);
                }

                /* Adjust the efuseBegin but keep efuseEnd unchanged */
                efuseBegin += TC_CMDS_SIZE_MAX;
            }

            /* Output Dump */
            printf("------------------- eFuse Dump ----------------------");
            for (i = efusePrintAnkor; i <= efuseEnd; i++) {
                /* Cosmetics */
                if (i == efusePrintAnkor) {
                    numPlaceHolder = (efusePrintAnkor & 0x0F);
                    printf("\n%04X:", (efusePrintAnkor & 0xFFF0));
                    for (k = 0; k < numPlaceHolder; k++) {
                        printf("   ");
                    }
                } else if ((i & 0x0F) == 0) {
                    printf("\n%04X:", i);
                }

                printf(" %02X", efuseBuf[i]);
            }
            printf("\n\n");
        } else if (tCmds->hdr.act == TC_CMDS_EFUSEWRITE) {
            int i;
            A_UINT16 wBytes;

            /* Error check */
            if (data_length == 0) {
                printf("No data to write, exit..\n");
                break;
            } else if ((efuse_begin + data_length + 4) > TC_CMDS_SIZE_MAX) {
                printf("Exceed eFuse border: %d, exit..\n", (TC_CMDS_SIZE_MAX - 1));
                break;
            }

            /* PRINT */
            printf("eFuse data (%d Bytes): ", data_length);
            for (i = 0; i < data_length; i++) {
                printf("%02X ", efuseWriteBuf[i]);
            }
            printf("\n");

            /* Write address and data length */
            tCmds->buf[0] = (efuse_begin & 0xFF);
            tCmds->buf[1] = (efuse_begin >> 8) & 0xFF;
            tCmds->buf[2] = (data_length & 0xFF);
            tCmds->buf[3] = (data_length >> 8) & 0xFF;

            /* Copy data to tcmd buffer. The first 4 bytes are the ID and length */
            memcpy((void*)&(tCmds->buf[4]), (void*)&(efuseWriteBuf[0]), data_length);

            /* Construct eFuse Write */
            tCmds->hdr.u.parm.length  = (4 + data_length);
            tCmds->hdr.u.parm.version = TC_CMDS_VERSION_TS;  

            /* Transfer commands */ 
            *(A_UINT32 *)buf = AR6000_XIOCTL_TCMD_CMDS;
            ifr.ifr_data = (void *)buf;

            if (ioctl(s, AR6000_IOCTL_EXTENDED, &ifr) < 0) {
                A_ERR(1,"%s", ifr.ifr_name);
            }

            if (cmdRespCB) {
                cmdRespCB(ifr.ifr_data);
            }

            wBytes = ((sTcCmds.buf[1] << 8) | sTcCmds.buf[0]);
            printf("%d bytes written to eFuse.\n", wBytes);
        } else if (tCmds->hdr.act == TC_CMDS_OTPSTREAMWRITE) {
            int i;
            A_STATUS status;

            /* Error check */
            if (data_length == 0) {
                printf("No data to write, exit..\n");
                break;
            } else if ((data_length + 4) > TC_CMDS_SIZE_MAX) {
                printf("Exceed OTP size: %d, exit..\n", data_length);
                break;
            }

            /* PRINT */
            printf("Write OTP data (%d Bytes): ", data_length);
            for (i = 0; i < data_length; i++) {
                printf("%02X ", efuseWriteBuf[i]);
            }
            printf("\n");

            /* Copy data to tcmd buffer. The first 4 bytes are the ID and length */
            memcpy((void*)&(tCmds->buf[0]), (void*)&(efuseWriteBuf[0]), data_length);

            /* Construct eFuse Write */
            tCmds->hdr.u.parm.length  = data_length;
            tCmds->hdr.u.parm.version = TC_CMDS_VERSION_TS;  

            /* Transfer commands */ 
            *(A_UINT32 *)buf = AR6000_XIOCTL_TCMD_CMDS;
            ifr.ifr_data = (void *)buf;

            if (ioctl(s, AR6000_IOCTL_EXTENDED, &ifr) < 0) {
                A_ERR(1,"%s", ifr.ifr_name);
            }

            if (cmdRespCB) {
                cmdRespCB(ifr.ifr_data);
            }

            status = sTcCmds.buf[0];

            if (status == A_OK) {
                printf("Write %d bytes to OTP\n", data_length);
            } else {
                printf("Failed to write OTP\n");
            }
        } else if (tCmds->hdr.act == TC_CMDS_OTPDUMP) {
            int i;

            /* Construct eFuse Write */
            tCmds->hdr.u.parm.length  = 0;
            tCmds->hdr.u.parm.version = TC_CMDS_VERSION_TS;  

            /* Transfer commands */ 
            *(A_UINT32 *)buf = AR6000_XIOCTL_TCMD_CMDS;
            ifr.ifr_data = (void *)buf;

            if (ioctl(s, AR6000_IOCTL_EXTENDED, &ifr) < 0) {
                A_ERR(1,"%s", ifr.ifr_name);
            }

            if (cmdRespCB) {
                cmdRespCB(ifr.ifr_data);
            }

            if (sTcCmds.hdr.u.parm.length) {
                /* Received bytes are in sTcCmds */
                for (i = 0; i < sTcCmds.hdr.u.parm.length; i++) {
                    printf("%02x ", sTcCmds.buf[i]);
                }
                printf("\n");
            } else {
                printf("No valid stream found in OTP!\n");
            }
        } else {
            *(A_UINT32 *)buf = AR6000_XIOCTL_TCMD_CMDS;
            ifr.ifr_data = (void *)buf;
            if (ioctl(s, AR6000_IOCTL_EXTENDED, &ifr) < 0) {
                A_ERR(1,"%s", ifr.ifr_name);
            }   
            if (cmdRespCB) {
                cmdRespCB(ifr.ifr_data);
            }
        }

        break;     
#endif
    default:
        usage();
    }

#ifdef ATHTESTCMD_LIB
    return 0;
#else
    exit (0);
#endif
}

static void
rxReport(void *buf)
{
    A_UINT32 pkt;
    A_INT32  rssi;
    A_UINT32 crcError;
    A_UINT32 secErr;
    A_UINT16 rateCnt[TCMD_MAX_RATES];
    A_UINT16 rateCntShortGuard[TCMD_MAX_RATES];

    pkt = *(A_UINT32 *)buf;
    rssi = *((A_INT32 *)buf + 1);
    crcError = *((A_UINT32 *)buf + 2);
    secErr = *((A_UINT32 *)buf + 3);

    printf("total pkt %d ; crcError pkt %d ; secErr pkt %d ;  average rssi %d\n", pkt, crcError, secErr,
          (A_INT32)( pkt ? (rssi / (A_INT32)pkt) : 0));


    A_MEMCPY(rateCnt, ((A_UCHAR *)buf)+(4*sizeof(A_UINT32)), sizeof(rateCnt));
    A_MEMCPY(rateCntShortGuard, ((A_UCHAR *)buf)+(4*sizeof(A_UINT32))+(TCMD_MAX_RATES * sizeof(A_UINT16)), sizeof(rateCntShortGuard));

    printf("1Mbps     %d\n", rateCnt[0]);
    printf("2Mbps     %d\n", rateCnt[1]);
    printf("5.5Mbps   %d\n", rateCnt[2]);
    printf("11Mbps    %d\n", rateCnt[3]);
    printf("6Mbps     %d\n", rateCnt[4]);
    printf("9Mbps     %d\n", rateCnt[5]);
    printf("12Mbps    %d\n", rateCnt[6]);
    printf("18Mbps    %d\n", rateCnt[7]);
    printf("24Mbps    %d\n", rateCnt[8]);
    printf("36Mbps    %d\n", rateCnt[9]);
    printf("48Mbps    %d\n", rateCnt[10]);
    printf("54Mbps    %d\n", rateCnt[11]);
    printf("\n");
    printf("HT20 MCS0 6.5Mbps   %d (SGI: %d)\n", rateCnt[12], rateCntShortGuard[12]);
    printf("HT20 MCS1 13Mbps    %d (SGI: %d)\n", rateCnt[13], rateCntShortGuard[13]);
    printf("HT20 MCS2 19.5Mbps  %d (SGI: %d)\n", rateCnt[14], rateCntShortGuard[14]);
    printf("HT20 MCS3 26Mbps    %d (SGI: %d)\n", rateCnt[15], rateCntShortGuard[15]);
    printf("HT20 MCS4 39Mbps    %d (SGI: %d)\n", rateCnt[16], rateCntShortGuard[16]);
    printf("HT20 MCS5 52Mbps    %d (SGI: %d)\n", rateCnt[17], rateCntShortGuard[17]);
    printf("HT20 MCS6 58.5Mbps  %d (SGI: %d)\n", rateCnt[18], rateCntShortGuard[18]);
    printf("HT20 MCS7 65Mbps    %d (SGI: %d)\n", rateCnt[19], rateCntShortGuard[19]);
    printf("\n");	
    printf("HT40 MCS0 13.5Mbps    %d (SGI: %d)\n", rateCnt[20], rateCntShortGuard[20]);
    printf("HT40 MCS1 27.0Mbps    %d (SGI: %d)\n", rateCnt[21], rateCntShortGuard[21]);
    printf("HT40 MCS2 40.5Mbps    %d (SGI: %d)\n", rateCnt[22], rateCntShortGuard[22]);
    printf("HT40 MCS3 54Mbps      %d (SGI: %d)\n", rateCnt[23], rateCntShortGuard[23]);
    printf("HT40 MCS4 81Mbps      %d (SGI: %d)\n", rateCnt[24], rateCntShortGuard[24]);
    printf("HT40 MCS5 108Mbps     %d (SGI: %d)\n", rateCnt[25], rateCntShortGuard[25]);
    printf("HT40 MCS6 121.5Mbps   %d (SGI: %d)\n", rateCnt[26], rateCntShortGuard[26]);
    printf("HT40 MCS7 135Mbps     %d (SGI: %d)\n", rateCnt[27], rateCntShortGuard[27]);

	
}

static A_UINT32
freqValid(A_UINT32 val)
{
    do {
        if (val <= A_CHAN_MAX) {
            A_UINT16 freq;

            if (val < BG_CHAN_MIN)
                break;

            freq = wmic_ieee2freq(val);
            if (INVALID_FREQ == freq)
                break;
            else
                return freq;
        }

        if ((val == BG_FREQ_MAX) || 
            ((val < BG_FREQ_MAX) && (val >= BG_FREQ_MIN) && !((val - BG_FREQ_MIN) % 5)))
            return val;
        else if ((val >= A_FREQ_MIN) && (val < A_20MHZ_BAND_FREQ_MAX) && !((val - A_FREQ_MIN) % 20))
            return val;
        else if ((val >= A_20MHZ_BAND_FREQ_MAX) && (val <= A_FREQ_MAX) && !((val - A_20MHZ_BAND_FREQ_MAX) % 5))
            return val;
    } while (FALSE);

    A_ERR(-1, "Invalid channel or freq #: %d !\n", val);
    return 0;
}

static A_UINT32 rateValid(A_UINT32 val, A_UINT32 freq)
{
    if (((freq >= A_FREQ_MIN) && (freq <= A_FREQ_MAX) && (val >= A_RATE_NUM)) ||
        ((freq >= BG_FREQ_MIN) && (freq <= BG_FREQ_MAX) && (val >= G_RATE_NUM))) {
        printf("Invalid rate value %d for frequency %d! \n", val, freq);
        prtRateTbl(freq);
        A_ERR(-1, "Invalid rate value %d for frequency %d! \n", val, freq);
    }

    return val;
}

static void prtRateTbl(A_UINT32 freq)
{
    int i;

        for (i = 0; i < G_RATE_NUM; i++) {
            printf("<rate> %d \t \t %s \n", i, bgRateStrTbl[i]);
        }
        printf("\n");
    }
    
/*
 * converts ieee channel number to frequency
 */
static A_UINT16
wmic_ieee2freq(A_UINT32 chan)
{
    if (chan == BG_CHAN_MAX) {
        return BG_FREQ_MAX;
    }
    if (chan < BG_CHAN_MAX) {    /* 0-13 */
        return (BG_CHAN0_FREQ + (chan*5));
    }
    if (chan <= A_CHAN_MAX) {
        return (A_CHAN0_FREQ + (chan*5));
    }
    else {
        return INVALID_FREQ;
    }
}

static A_UINT32 antValid(A_UINT32 val)
{
    if (val > 2) {
        A_ERR(-1, "Invalid antenna setting! <0: auto;  1/2: ant 1/2>\n");
    }

    return val;
}

static A_UINT32 txPwrValid(TCMD_CONT_TX *txCmd)
{
    if (txCmd->mode == TCMD_CONT_TX_SINE) {
        if ((txCmd->txPwr >= 0) && (txCmd->txPwr <= 60))
            return txCmd->txPwr;
    } else if (txCmd->mode != TCMD_CONT_TX_OFF) {
        if (txCmd->tpcm != TPC_FORCED_GAIN) {
            if ((txCmd->txPwr >= -30) && (txCmd->txPwr <= 60))
                return txCmd->txPwr;
        } else {
             if ((txCmd->txPwr >= 0) && (txCmd->txPwr <= 120))
                return txCmd->txPwr;
        }
    } else if (txCmd->mode == TCMD_CONT_TX_OFF) {
        return 0;
    }

    A_ERR(1, "Invalid Tx Power value! \nTx data: [-15 - 14]dBm  \nTx sine: [  0 - 60]PCDAC value \n");
    return 0;
}
static A_UINT32 pktSzValid(A_UINT32 val)
{
    if (( val < 32 )||(val > 1500)){
        A_ERR(-1, "Invalid package size! < 32 - 1500 >\n");
    }
    return val;
}
#ifdef NOTYET

// Validate a hex character
static A_BOOL
_is_hex(char c)
{
    return (((c >= '0') && (c <= '9')) ||
            ((c >= 'A') && (c <= 'F')) ||
            ((c >= 'a') && (c <= 'f')));
}

// Convert a single hex nibble
static int
_from_hex(char c) 
{
    int ret = 0;

    if ((c >= '0') && (c <= '9')) {
        ret = (c - '0');
    } else if ((c >= 'a') && (c <= 'f')) {
        ret = (c - 'a' + 0x0a);
    } else if ((c >= 'A') && (c <= 'F')) {
        ret = (c - 'A' + 0x0A);
    }
    return ret;
}

// Convert a character to lower case
static char
_tolower(char c)
{
    if ((c >= 'A') && (c <= 'Z')) {
        c = (c - 'A') + 'a';
    }
    return c;
}

// Validate alpha
static A_BOOL
isalpha(int c)
{
    return (((c >= 'a') && (c <= 'z')) || 
            ((c >= 'A') && (c <= 'Z')));
}

// Validate digit
static A_BOOL
isdigit(int c)
{
    return ((c >= '0') && (c <= '9'));
}

// Validate alphanum
static A_BOOL
isalnum(int c)
{
    return (isalpha(c) || isdigit(c));
}
#endif

/*------------------------------------------------------------------*/
/*
 * Input an Ethernet address and convert to binary.
 */
static A_STATUS
ath_ether_aton(const char *orig, A_UINT8 *eth)
{
    int mac[6];
    if (sscanf(orig, "%02x:%02x:%02X:%02X:%02X:%02X", 
               &mac[0], &mac[1], &mac[2],  
               &mac[3], &mac[4], &mac[5])==6) {
        int i;
#ifdef DEBUG
        if (*(orig+12+5) !=0) {
            fprintf(stderr, "%s: trailing junk '%s'!\n", __func__, orig);
            return A_EINVAL;
        }
#endif
        for (i=0; i<6; ++i) 
            eth[i] = mac[i] & 0xff;
        return A_OK;
    }
    return A_EINVAL;
}

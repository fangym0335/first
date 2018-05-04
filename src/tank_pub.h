/*			public definitions for  all
*/
#ifndef __TANK_PUB_H__
#define __TANK_PUB_H__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <sys/socket.h>
#include <termios.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <math.h>
#include "bcd.h"
#include "ifsf_pub.h"
#include "ifsf_tlg.h"
#include "ifsf_tlgtp.h"
#include "ifsf_tlgtlg.h"
#include "pubsock.h"

/*  			For versin control 			
*	Each new version must be released with a common line that includes the information:
*	1. the previous version
*	2. changed files
*	3. what's new of the new version
*	2007.10.18
*/

#define VERSION	"V1.2"
/* v1.2:  After the simple test in CPO, fixed some buges. */
/* v1.1: 	modify name of the files and fixed many bugs. It's the first stable version for opw and
		aoke_tb. 2008-01-17*/
/* v1.0:	This is the first version of the program, the detail information of the program can be
	found in development manual 2007-11-22*/

#define INTERVAL		3
#define WAITFORHEART 20
/* DEBUG_P to point the which message level will be print on screen,
"0" print all message
"1" print public level
"2" print monitor local level
"3" temp level		*/
#define DEBUG_P	1

struct msg{
	long msg_type;
	unsigned char buf[256];
};


/* tank monitor programs fefine*/
void tank_vr350(int);
void tank_aoke(int);
void tank_aoke_tb(int);
void tank_opw(int);
void tank_opw_pv4(int);
void tank_brite(int);
void tank_frkl(int);
void tank_tls2(int);
void tank_lanhua_lme900(int);
void tank_lanhua_lmes1(int);
void tank_rtl750b(int);
void tank_guihe(int);
void tank_yicheng(int);


extern TLG_DATA *  pub_tk;

/*
 * TK_VENDOR 标记相应品牌处理函数的函数指针在 tank_list 中的索引值, 即TANK_TYPE
 */
#define TK_VR_WITH_ADJUST            0
#define TK_VR                        1
#define TK_AOKE                      2
#define TK_AOKE_TP                   3
#define TK_OPW_SS1                   4
#define TK_OPW_GALAXY                5
#define TK_OPW_PV4                   6
#define TK_YITONG                    7
#define TK_BRITE                     8
#define TK_FRANKLIN                  9
#define TK_LANHUA_LME900             10
#define TK_LANHUA_LMES1              11
#define TK_RTL750B                   12
#define TK_GUIHE                     13
#define TK_OPW_ISITE                 14

static void (*tank_list[])() = {					//add tank program pointer here
		tank_vr350,						//  [0], vr350, with adjust
		tank_tls2,						//  [1], vr350, without adjust
		tank_aoke,                                      //  [2], aoke console
		tank_aoke_tb,                                 // [3], aoke TP
		tank_opw,						// [4], opw ss1
		tank_opw,						// [5], opw galaxy
		tank_opw_pv4,                               // [6], opw isite -pv4 adjust
		tank_opw,						// [7], Yitong 
		tank_brite,                                      // [8], Brite
		tank_opw,                                       // [9], Franklin
		tank_lanhua_lme900,                    // [10],lanhua lme900
		tank_lanhua_lmes1,                      //  [11],lanhua lmes1
		tank_rtl750b,                                 //  [12],爱国者
		tank_guihe,                                     //  [13],贵和
		tank_opw,                                        // [14], opw isite
		tank_yicheng						//[15],怡诚撬装
};

/* public functions define */
void __debug(int , const char *fmt,...);
unsigned char *get_serial(int p);
int asc2hex(unsigned char *src, unsigned char *des);
void	AsciiToHex(unsigned	char *pSrc , unsigned char *pDes, unsigned int  n);
float exp2float(unsigned char *);
int set_systime( char * );
void print_result(void);
int tank_online(__u8 status);
	
/* globe variables define */
#define MAX_TP_NUM				31
#define MAX_MSG_LEN				128
#define MAX_RECORD_MSG_LEN	     	 	256
#define MAX_CMD_LEN				256
#define MAX_TANK_PARA_LEN			1024
#define MAX_CONFIG_LEN				1024
#define MAX_TANK_NUM				16
#define CALL					1
#define OFF					0
#define CMD_LEN					2
#define TANK_INTERVAL                          950000   // 950 ms
#define TANK_INTERVAL_READ                    	950000   // 950 ms
#define TANK_INTERVAL_ALARM                     100000   // 100 ms
//#define TANK_MSG_ID		102
//
#define SHOW_ALARM_MSG                          1

static const unsigned char GUN_CALL[CMD_LEN+1] = { 0x01, 0x00 };
static const unsigned char GUN_OFF[CMD_LEN+1] =  { 0x02, 0x00 };
static const unsigned char GUN_BUSY[CMD_LEN+1] = { 0x0A, 0x00 };
int TANK_TYPE;
int PROBER_NUM;

#define TANK_CONF_CNT 30
#define TANK_CONF_LNG 63
#define TANK_CONFIG_FILE                     "/home/App/ifsf/etc/dlb.cfg" //???????t


typedef struct {//DLB_ini_name_value
       char name[TANK_CONF_LNG + 1];
       char value[TANK_CONF_LNG + 1];
}TANK_INI;

TANK_INI tank_ini[TANK_CONF_CNT];

#define MAX_PAR_NUM 15
//extern char TLG_CONFIG[2];
static int globle_config;
static int tank_cnt;
static int par_cnt;
static unsigned char stano_bcd[5];
static char ipname[64];
#define inttobcd(data) (((((data) -(data)%10)/10)&0x0f)<<4|(data)%10)

static int DLB_CheckInterruptConnect(int sock)
{
	struct pollfd unix_really_sucks;
	int some_more_junk;
	socklen_t yet_more_useless_junk;
	
	if ( errno != EINTR /* && errno != EINPROGRESS */ ) {
        	RunLog ("connect");
        	return -1;
      	}
    	unix_really_sucks.fd = sock;
    	unix_really_sucks.events = POLLOUT;
    	while ( poll (&unix_really_sucks, 1, -1) == -1 ) {
      		if ( errno != EINTR ) {
          		RunLog ("poll");
          		return -1;
        	}
    	}
    	yet_more_useless_junk = sizeof(some_more_junk);
    	if ( getsockopt (sock, SOL_SOCKET, SO_ERROR,\
               &some_more_junk,\
               &yet_more_useless_junk) == -1 ) {
        	RunLog ("getsockopt");
        	return -1;
      	}
    	if ( some_more_junk != 0 ) {
        	RunLog( "connect: %s\n", strerror (some_more_junk));
        	return -1;
      	}
	return 0;
	
}
/*当nTimeOut设置为TIME_INFINITE或者设置的时间比系统缺省时间长时,
 connect在完成返回前都只等待系统缺省时间*/
static int Tank_TcpConnect( const char *sIPName, const char *sPortName, int cilent_port, int nTimeOut )
{
	struct sockaddr_in serverAddr, cilentAddr;
	int sock;
	in_port_t port;
	in_addr_t addr;
	int ret;

	Sigfunc *OldAction;

	Assert( sIPName != NULL && *sIPName != 0 );
	Assert( sPortName != NULL && *sPortName != 0 );

	//RunLog("9004");
	if( ( port = GetTcpPort( sPortName ) ) == INPORT_NONE )
	{
		RptLibErr( TCP_ILL_PORT );
		return -1;
	}

	if( ( addr = GetTcpAddr( sIPName ) ) == INADDR_NONE )
	{
		RptLibErr( TCP_ILL_IP );
		return -1;
	}

	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = addr;
	serverAddr.sin_port = port;
	cilentAddr.sin_family = AF_INET;
	cilentAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	cilentAddr.sin_port = htons(cilent_port);

	sock = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
	if( sock < 0 )
	{
		RptLibErr( errno );
		return -1;
	}

	//int opt=SO_REUSEADDR;
       //setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
	   
	if (bind( sock, (SA *)&cilentAddr, sizeof( cilentAddr ) ) < 0){
		sleep(1);
		if (bind( sock, (SA *)&cilentAddr, sizeof( cilentAddr ) ) < 0){
			RunLog("TANK两次用[%d]绑定端口失败", cilent_port);
			return -1;
		}
	}
	if( nTimeOut == TIME_INFINITE )
	{
		ret = connect( sock, (struct sockaddr *)&serverAddr, sizeof( serverAddr ) );
		/* Start with fd just returned by socket(), blocking, SOCK_STREAM... */
		if ( ret == -1 ) {
			if (DLB_CheckInterruptConnect(sock) == -1 ) {
				RunLog("check interrupt connect failed ", sock);
			}
  		}
		/* At this point, fd is connected. */
		RunLog("connect TIME_INFINITE ret[%d],errno[%d],IP[%s], port[%d]\n",ret,errno,sIPName, cilent_port);
		
	}
	else
	{
		OldAction = SetAlrm( nTimeOut, NULL );  /*如果在alarm调用后,connect调用前超时,则connect等待时间为缺省时间*/
		ret = connect( sock, (struct sockaddr *)&serverAddr, sizeof( serverAddr ) );
		/* Start with fd just returned by socket(), blocking, SOCK_STREAM... */
		if ( ret == -1 ) {
			if (DLB_CheckInterruptConnect(sock) == -1 ) {
				RunLog("check interrupt connect failed ", sock);
			}
  		}
		/* At this point, fd is connected. */
		RunLog("connect TIME_INFINITE ret[%d],errno[%d],IP[%s], port[%d]\n",ret,errno,sIPName, cilent_port);
		
		UnSetAlrm();
	}

	if( ret == 0 )
		return sock;

	close( sock );

	RptLibErr( errno );

	return -1;

}

/*
	 0:	成功
	-1:	失败
	-2:	超时
*/
static ssize_t TcpSendDataInTime( int fd, const char *s, size_t len,  int timeout )
{
	ssize_t	 retlen;
	size_t	sendlen;
	char	*head;
	char	*t;
	char	buf[TCP_MAX_SIZE];
	int	mflag; 	/*标识是否分配了空间 0未分配. 1已分配*/
	int	n;
	int     i;
	
	Assert(s != NULL && *s != 0);
	Assert(len != 0);
	Assert(timeout > 0 || timeout == TIME_INFINITE);

	mflag = 0;
	sendlen = len;

	if (sizeof(buf) >= sendlen ) {	/*预先分配的变量buf的空间足够容纳所有要发送的数据*/
		t = buf;
	} else {			/*预先分配的变量buf的空间不足容纳所有要发送的数据,故分配空间*/
		t = (char*)Malloc(sendlen);
		if ( t == NULL ) {
			RptLibErr( errno );
			return -1;
		}
		mflag = 1;
	}
	

	//sprintf( t, "%0*d", headlen, len );
	memcpy(t, s, len );
	
	n = WriteByLengthInTime(fd, t, sendlen, timeout );

	if ( mflag == 1 ) {
		Free(t);
	}

	if ( n == sendlen ) {	/*数据全部发送*/
		return 0;
	} else if ( n == -1 ) {/*出错*/
		return -1;
	} else if ( n >= 0 && n < sendlen ) {
		if ( GetLibErrno() == ETIME ) {/*超时*/
			return -2;
		} else {
			return -1;
		}
	}

}

/*
 * s:	保存读到的数据
 * buflen:	指针。*buflen在输入时为s能接受的最大长度的数据（不包含尾部的0.如char s[100], *buflen = sizeof(s) -1 )
 *		在输出时为s实际保存的数据的长度(不包含尾部的0.)
 * 返回值:
 *	0	成功
 *	1	成功.但数据长度超过能接受的最大长度.数据被截短
 *	-1	失败
 *	-2	超时或者连接过早关闭
 */

static ssize_t TcpRecvDataInTime( int fd, char *s, size_t *buflen,  int timeout ) //size_t headlen,
{
	ssize_t	recvlen;
	size_t	len;
	size_t headlen =8;
	char	head[TCP_MAX_HEAD_LEN + 1];
	char	buf[TCP_MAX_SIZE];
	char	*t;
	int	mflag;
	int 	n;
	int	ret;

	Assert( s != NULL );


	n = ReadByLengthInTime( fd,s, *buflen, timeout );
		
	if ( n == 2 ) {
		ret = 2;
	} else if ( n != 2 ) {
		RunLog("收到的数据长度有问题");
		ret = -1;
	}

	return ret;
}

#endif


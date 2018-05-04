#ifndef __TCP_H__
#define __TCP_H__

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <signal.h>

#include <sys/ioctl.h>
#include <linux/if.h>


#include "mycomm.h"
#include "dodigital.h"
#include "dosignal.h"
#include "doio.h"

/*写起来简单点*/
typedef struct sockaddr SA;

/*定义一个发送数据的最大长度10240。当发送的数据小于等于此长度时，不需要动态分配内存*/
#define TCP_MAX_SIZE		1280
/*定义最大用几位表示包数据的长度*/
#define TCP_MAX_HEAD_LEN	8

#ifndef TIME_INFINITE
#define TIME_INFINITE  -1
#endif

#ifndef INADDR_NONE
#define INADDR_NONE 0xFFFFFFFF
#endif /*INADDR_NONE*/

#ifndef INPORT_NONE
#define INPORT_NONE 0
#endif /*INPORT_NONE*/

#ifndef IPADDR_LENGTH
#define IPADDR_LENGTH sizeof("255.255.255.255")
#endif /*ADDR_LENGTH*/

#if defined(__SCO__)
typedef int in_port_t;
typedef unsigned long in_addr_t;
#endif

typedef struct{
	char 	sServAddr[IPADDR_LENGTH];
	in_port_t servPort;
	char	sClientAddr[IPADDR_LENGTH];
	in_port_t clientPort;
}SockPair;

/**********************************
struct in_addr {
        in_addr_t       s_addr;
};
**********************************/

/**********************************
struct sockaddr_in {
        uchar_t        sin_len;
        sa_family_t    sin_family;
        in_port_t      sin_port;
        struct in_addr sin_addr;
        uchar_t        sin_zero[8];
};
**********************************/

/*INPORT_NOE失败 其他成功*/
in_port_t GetTcpPort( const char *sPortName );
/*INADDR_NONE失败.其他成功*/
in_addr_t GetAddr( const char *sHostName );
int GetPeerInfo( int sockFd, char *sIP, in_port_t *port );
int GetSockInfo( int sockFd, char *sIP, in_port_t *port );
int TcpServInit( const char *sPortName, int backLog );
void __TcpClose( int *psock );
int TcpAccept( int sock, SockPair *pSockPair  );
int TcpConnect( const char *sIPName, const char *sPortName, int nTimeOut );

/*
ssize_t TcpRecvDataByHead( int fd, char *s, size_t *buflen, size_t headlen, int timeout )

s		:	保存读到的数据
buflen	: 	指针。*buflen在输入时为s能接受的最大长度的数据（不包含尾部的0.如char s[100], *buflen = sizeof(s) -1 )
						在输出时为s实际保存的数据的长度(不包含尾部的0.)
返回值	:
			0		成功
			1		成功.但数据长度超过能接受的最大长度.数据被截短
			-1		失败
			-2		超时或者连接过早关闭
*/
ssize_t TcpRecvDataByHead( int fd, char *s, size_t *buflen, size_t headlen, int timeout );
ssize_t TcpSendDataByHead( int fd, const char *s, size_t len, size_t headlen, int timeout );


#define TcpClose( a ) __TcpClose( &(a) )


#endif /*__TCP_H__*/

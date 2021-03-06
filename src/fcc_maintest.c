/*
 * 2008-11-10 - Chen Fu-ming <chenfm@css-intelligent.com>
 思路:本程序在inittab中用wait自动启动,然后本程序根据配置文件 配置的IP地址和端口号
 连接PC机上的测试程序,如果持续1分钟多次的连接都不能成功,那么本程序
 自动退出;如果连接成功就开始测试.因此本程序在成品中也可以存在. 
 本程序运行需要PC和2个同型号的FCC,最好是一个好的一个待测试的,
 PC根据IP地址区分,第一个注册的是主FCC,第二个是从FCC,使用时注意,建议待测FCC为从.
 * *******************************************************************************************************
 协议:
 数据格式都是HEX,除注册数据外,其它数据的标志字节的后4位1为1号FCC,2为2号FCC
1. FCC向PC 注册:
FCC发:1104+本机IP地址+4个串口通道数,PC回复:110111或者 110122,其中110111为1号,110122为2号FCC.
2.FCC的所有调试信息都用UDP发送给PC的测试程序,都在一个窗口显示
3. 开始全部测试:(等待从测试FCC注册后PC自动发)
  PC向两个FCC发:220111和220122, 11的是主测试, 22的是从测试,FCC收到回复2200表示开始测试
  测试完成后,FCC发送测试结果,测试结果按照位做标志,0测试失败,1测试成功:
  220a+1字节串口成功标志+8字节串口通道标志+1字节USB标志字节,共12字节,PC回复2200
  8字节串口通道标志,每个串口2个字节最多16个通道,因此最多支持64个通道
4.同步测试命令:
    FCC发送0xff03110000或者0xff03220000,FCC收到0xff0111或者0xff0122继续测试, 否则循环等待
5.数据错误处理:
  所有数据错误的时候,第一个字节,也就是命令字节的低4位要置1，即值f,
  第二个字节为0,   如1100变成1f00,
6. 退出测试程序:
    PC向两个FCC发9900,FCC收到回复9900

-------------------------------------------------------------------------------------------
以下功能暂时没有完成
7. 部分测试:(部分测试必须在全部测试做完成后才能进行)
   1).交叉通道测试: PC向两个FCC发:3301XX3301YY,XX是主测试的通道号,YY是从测试的通道号
      两个通道要物理连接.FCC回复3300. 成功后FCC回复3301XX或者3301YY,PC回复3300.
   2).普通串口测试:PC向两个FCC发:4401XX4401YY,XX是主测试的串口号,YY是从测试的串口号
      两个通道要物理连接.串口号从0开始.FCC回复4400.成功后回复4401XX或者4401YY,PC回复4400.
   3).USB测试:PC向两个个FCC发:5501XX5501YY,XX是主测试的USB号,YY是从测试的USB号
      两个通道要物理连接.USB号从1开始.0表示不测试
      FCC回复5500. 成功后FCC回复5501XX或者5501YY,PC回复5500.
  4).测试中提醒插入U盘:   FCC发6601XX, XX是测试的USB号PC回复6600.插入结束,PC发6601XX,
     FCC回复6600,并开始测试.   测试结束FCC发6602XX00或者6602XX11, XX是测试的USB号,
     00表示测试成功,11表示测试失败,PC回复6600
 ------change log
*Change config file to /etc/   and delete some not used code
2009-01-13 by Chen Fu-ming <chenfm@css-intelligent.com>
*Use mutitread to test channel and sierial port.
2009-01-15 by Chen Fu-ming <chenfm@css-intelligent.com>
*all usb mount.
2013.03.29 by LL
 */

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <signal.h>
#include <sys/ipc.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/sem.h>

#include "log.h"
#include "ifsf_pub.h"
#include "ifsf_tlg.h"

//#define OLD_PROTOCOL  1

//#define  MAIN_CONFIG_FILE                     "/home/App/ifsf/etc/fcc_selftest.cfg"
#define MAIN_CONFIG_FILE                     "/etc/fcc_selftest.cfg"
#define MAX_PATH_NAME_LENGTH 1024
#define MAX_PORT_CMD_LEN 255

#define MAIN_TCP_MAX_SIZE 1280
#define MAIN_MAX_ORDER_LEN		128
#define PCD_VER       "v2.01 - 2013.04.01"

//锁信号量索引值


//----------------------------------------------
//实时日志定义
//===============================
#define MAIN_MAX_VARIABLE_PARA_LEN 1024 
#define MAIN_MAX_FILE_NAME_LEN 1024 
#define USB_DEV_SDA 0	//USB从设备号
#define USB_DEV_SDB 16
#define USB_DEV_SDC 32
#define USB_DEV_SDD 48
static char gsFccFileName[MAIN_MAX_FILE_NAME_LEN + 1];	//记录调试信息的文件名
static int giFccLine;								//记录调试信息的行
static int giFccFd;                                                       //日志文件句柄
static char gcFccLogFlag;
static unsigned  char  gsLocalIP[16];
//自定义分级文件日志,见__Log.
#define Main_Log Main_SetFileAndLine( __FILE__, __LINE__ ), Main__Log
//#define Main_Log Main_SetFileAndLine( __FILE__, __LINE__ ),Main__FileLog
//--------------------------------------------------------------

//-------------------------------------------------------
//串口定义
//串口特征,串口从0开始
//#define START_PORT 3
#define MAX_PORT 6
//#define PORT_USED_COUNT 3
//串口字符串
static char MAIN_alltty[][11] =
{
"/dev/ttyS0",
"/dev/ttyS1",
"/dev/ttyS2",
"/dev/ttyS3",
"/dev/ttyS4",
"/dev/ttyS5",
"/dev/ttyS6"
};
//串口通道数目,不连接通道值为0.
//unsigned char portChanel[MAX_PORT+1];
//----------------------------------------------------

//-------------------------------------------
//初始化配置参数
//-------------------------------------------
static char PC_SERVER_IP[16]; //PC服务器IP地址
static int PC_SERVER_TCP_PORT,PC_SERVER_UDP_PORT,MAIN_TCP_PORT;

#define MAIN_INI_FIELD_LEN 63
#define MAIN_INI_MAX_CNT 30
typedef struct fcc_ini_name_value{
char name[MAIN_INI_FIELD_LEN + 1];
char value[MAIN_INI_FIELD_LEN + 1];
}MAIN_INI_NAME_VALUE;
static MAIN_INI_NAME_VALUE fcc_ini[MAIN_INI_MAX_CNT];
//-------------------------------------------------------

//--------------------------------------------------------
//向PC注册
/*FCC向PC 注册:
*FCC发:1104+本机IP地址+4个串口通道数,PC回复:110111或者 110122,其中110111为1号,110122为2号FCC.*/
//static unsigned char FccNum;
//-------------------------------------------------

//------------------------------------------------

static char Main_USB_UM[]="/bin/umount -l /home/ftp/";
static char Main_USB_File[]="/home/ftp/fcctest.txt";
static char Main_USB_Data[]="1234567890";
//---------------------------------------------

//------------------------------------------------
//结果回复数据
static unsigned char Main_Result[8];
//---------------------------------------------




//------------------------------------------------
static unsigned  long  Main_GetLocalIP();

//-------------------------------------------------

//----------------------------------------------------

//--------------------------------------------------------------------------------
//实时日志
//----------------------------------------------
void Main_SetLogFd( const int fd )
{
	giFccFd= fd;
	if(  giFccFd <=  0){
		printf( " {日志文件描述符错误[%d] }\n", fd );
		
	}
}
void Main_SetFileAndLine( const char *filename, int line )
{
	if( strlen( filename ) + 1 > sizeof( gsFccFileName) ){
		printf("");
		exit(-1);}
	strcpy( gsFccFileName, filename );
	giFccLine = line;
}

void Main_GetFileAndLine( char *fileName, int *line )
{
	strcpy( fileName, gsFccFileName );
	*line = giFccLine;
}
void Main_SendUDP(const int fd,  const char *sMessage, int len )
{    
	struct sockaddr_in remoteSrv;	
	//struct in_addr  iplong;
	//struct in_addr  ipmask;
	int ret;
	//int 	port;
	extern int errno;
	//port = PC_SERVER_UDP_PORT;	
	memset( &remoteSrv, 0, sizeof(remoteSrv) );
	remoteSrv.sin_family = AF_INET;	
	remoteSrv.sin_addr.s_addr = inet_addr(PC_SERVER_IP);//(GetLocalIP() | 0xff);	
	remoteSrv.sin_port = htons( PC_SERVER_UDP_PORT );
	ret = sendto(fd, sMessage, len,	0, 
		(struct sockaddr *)(&remoteSrv), sizeof(struct sockaddr));//发送数据
	/*if (ret < 0) {				
		printf("心跳数据发送失败,error:[%s]", strerror(errno));	
	}*/		
	
}

//giLogLevel=0, no log;giLogLevel=1, only print log info; giLogLevel = 2, print log info and write log info to file 
void Main__Log(const char *fmt,...){//const int loglevel, const int fd, 
	va_list argptr;
	char sMessage[MAIN_MAX_VARIABLE_PARA_LEN + 1];
	char fileName[MAIN_MAX_FILE_NAME_LEN + 1];	/*记录调试信息的文件名*/
	int line;								/*记录调试信息的行*/
	//int level = giFccLogLevel;
	
	
	Main_GetFileAndLine(fileName, &line);	
	//sprintf(sMessage, "[%s]", gsLocalIP);
	sprintf(sMessage, "%s:%d  ", fileName, line);
	sprintf(sMessage+strlen(sMessage), "pid = [%s:%d]", gsLocalIP, getpid());

	va_start(argptr, fmt);
	vsprintf(sMessage+strlen(sMessage),fmt, argptr);
	va_end( argptr );
	sprintf(sMessage+strlen(sMessage), "\n");
	
	
	if(giFccFd <=0 )	/*日志文件未打开?*/
	{
		printf( " {日志文件描述符错误[%d] }\n", giFccFd );
		return;
	}
	//printf(sMessage);
	if(0==gcFccLogFlag)Main_SendUDP( giFccFd, sMessage, strlen(sMessage) );//write
	else{
		printf(sMessage);
		Main_SendUDP( giFccFd, sMessage, strlen(sMessage) );
	}
	//write( giFccFd, "\n", strlen("\n") );
	
	return;
}

int Main_InitLog(int flag){//flag---0: UDP日志,1:打印日志
	int sockfd;
	unsigned char *pIP;
	struct in_addr in_ip;
    	 /*if( ( fd = open( MAIN_LOG_FILE, O_WRONLY|O_CREAT|O_TRUNC, 00644 ) ) <= 0 ) {
		printf( "打开日志文件[%s]失败\n", MAIN_LOG_FILE );
		return -1;
	}*/
	if ( (sockfd = socket(AF_INET,SOCK_DGRAM,0)) == -1 )
        {
                printf("socket function error!\n");
                return -1;
        }
	bzero(gsLocalIP,sizeof(gsLocalIP));
	
	in_ip.s_addr = Main_GetLocalIP() ;
	//printf("[%d]   ",in_ip.s_addr );
	//pIP = inet_ntoa(in_ip );	
	sprintf(gsLocalIP, "%s", inet_ntoa(in_ip ) );
	//printf("[%s] \n",gsLocalIP );
	//memcpy(gsLocalIP,pIP,15);
	giFccFd = sockfd;
	if(0 == flag) gcFccLogFlag = 0;
	else if(1 == flag) gcFccLogFlag = 1;
	else printf("Main_InitLog函数日志初始化错误,flag[%d]\n",flag);
	//Main_SetLogLevel(loglevel);
	return 1;
}
//------------------------------------------------

//-----------------------------------------------------------------------
//公用函数
//-----------------------------------------------------------------------

unsigned char* Main_Asc2Hex( unsigned char *cmd, int cmdlen )
{
	static char retcmd[MAIN_MAX_ORDER_LEN*2+1];
	int	i;

	memset( retcmd, 0, sizeof(retcmd) );
	for( i = 0; i < cmdlen; i++ ) {
		sprintf(retcmd + i * 2, "%02x", cmd[i]);//为什么要i * 2
	}

	return retcmd;
}
//获取本机IP,返回本机IP地址，返回-1失败，解决DHCP问题
static unsigned  long  Main_GetLocalIP(){//参见tcp.h和tcp.c文件中有,可以选择一个.
	int  MAXINTERFACES=16;
	long ip;
	int fd, intrface;
 	struct ifreq buf[MAXINTERFACES]; ///if.h
	struct ifconf ifc; ///if.h
	ip = -1;
	if ((fd = socket (AF_INET, SOCK_DGRAM, 0)) >= 0) //socket.h
	{
 		ifc.ifc_len = sizeof(buf);
 		ifc.ifc_buf = (caddr_t) buf;
 		if (!ioctl (fd, SIOCGIFCONF, (char *) &ifc)) //ioctl.h
		{
 			intrface = ifc.ifc_len / sizeof (struct ifreq); 
			while (intrface-- > 0)
 			{
 				if (!(ioctl (fd, SIOCGIFADDR, (char *) &buf[intrface])))
 				{
 					ip= inet_addr( inet_ntoa( ((struct sockaddr_in*)(&buf[intrface].ifr_addr))->sin_addr) );//types
 					break;  //only find firrst eth.
				}
           		}
			
		}
 		close (fd);
 	}
	return ip;	
}

int Main_TcpServInit( int port )//const char *sPortName, int backLog
{

	struct sockaddr_in serverAddr;
	//in_port_t	port;
	int			sockFd;
	int 		backLog = 1;
	
	/*
	if( backLog < 5 )
		backLog = 5;

	port = GetTcpPort( sPortName );	//根据端口号或者端口名取端口 取出的port为网络字节序//
	if( port == INPORT_NONE )	//GetTcpPort已经报告了错误,此处省略//
		return -1;
	*/

	sockFd = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );//zhangjm IPPROTO_TCP 是tcp协议
	if( sockFd < 0 )
	{
		RptLibErr( errno );
		return -1;
	}

	//opt = 1;	//标志打开//
	//setsockopt( sockFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof( opt ) );	//设置端口可重用//
	
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);	/*htonl 主机字节序到网络字节序.不过INADDR_ANY为0,所以可以不用*//*INADDR_ANY 由内核选择地址*/
	serverAddr.sin_port = htons(port);

	if( bind( sockFd, (SA *)&serverAddr, sizeof( serverAddr ) ) < 0 )
	{
		Main_Log( "错误号:%d", errno );
		return -1;
	}

	if( listen( sockFd, backLog ) < 0 )
	{
		Main_Log( "错误号:%d",errno );
		return -1;
	}

	return sockFd;
}

int Main_TcpAccept( int sock )// , SockPair *pSockPair
{
	int			newsock;
#ifdef __SCO__
	int		len;
#else
	size_t	len;
#endif
	//char 		*sIP;
	//in_port_t 	port;

	struct sockaddr_in ClientAddr;


	while(1)
	{
		len = sizeof( ClientAddr );
		memset( &ClientAddr, 0, len );

		newsock = accept( sock, ( SA* )&ClientAddr, &len );//zhangjm客户的地址不关系，所以清零或为空，其中sock为前面客户连接指定的stock
		if( newsock < 0 && errno == EINTR ) /*被信号中断*/
			continue;
		break;
	}

	if( newsock >= 0 )
	{
		/*
		if(  pSockPair != NULL )
		{
			sIP = inet_ntoa( ClientAddr.sin_addr );
			if( sIP == (char*)(-1) )
			{
				RptLibErr( TCP_ILL_IP );
				close( newsock );
				return -1;
			}
			port = ntohs( ClientAddr.sin_port );

			if( pSockPair != NULL )
			{
				strcpy( pSockPair->sClientAddr, sIP );
				pSockPair->clientPort = port;

				DMsg( "Connection from 	[%s].port [%d]", sIP, port );


				if( GetSockInfo( newsock, pSockPair->sServAddr, &(pSockPair->servPort) ) < 0 )
				{
					close( newsock );
					return -1;
				}
				DMsg( "Local Addr [%s].port [%d]", pSockPair->sServAddr, pSockPair->servPort );
			}
		}
		*/
		return newsock;
	}

	Main_Log( "错误号:%d",errno );
	return -1;
}
/*当nTimeOut设置为TIME_INFINITE或者设置的时间比系统缺省时间长时,
 connect在完成返回前都只等待系统缺省时间*/
int Main_TcpConnect( const char *sIPName,int Ports , int nTimeOut )//const char *sPortName
{
	struct sockaddr_in serverAddr;
	int sock;
	in_port_t port;
	in_addr_t addr;
	int ret;

	Sigfunc *OldAction;

	Assert( sIPName != NULL && *sIPName != 0 );
	//Assert( sPortName != NULL && *sPortName != 0 );

	/*if( ( port = GetTcpPort( sPortName ) ) == INPORT_NONE )
	{
		RptLibErr( TCP_ILL_PORT );
		return -1;
	} */
	port = htons(Ports);

	if( ( addr = GetTcpAddr( sIPName ) ) == INADDR_NONE )//zhangjm什么意思�
	{
		RptLibErr( TCP_ILL_IP );//Main_Log
		return -1;
	}

	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = addr;
	serverAddr.sin_port = port;

	sock = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
	if( sock < 0 )
	{
		RptLibErr( errno );
		return -1;
	}
	errno = 0;

	if( nTimeOut == TIME_INFINITE )
	{
		ret = connect( sock, (struct sockaddr *)&serverAddr, sizeof( serverAddr ) );
		printf("connect TIME_INFINITE ret[%d],errer[%s],IP[%s], port[%d],port_ns[%d]\n",ret,strerror(errno),sIPName,Ports,port);
	}
	else
	{
		OldAction = SetAlrm( nTimeOut, NULL );  /*如果在alarm调用后,connect调用前超时,则connect等待时间为缺省时间*/
		ret = connect( sock, (struct sockaddr *)&serverAddr, sizeof( serverAddr ) );
		printf("connect secend ret[%d],sock[%d],errer[%s],IP[%s], port[%d],port_ns[%d]\n",ret, sock,strerror(errno),sIPName,Ports,port);
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
static ssize_t Main_TcpSendDataInTime( int fd, const char *s, size_t len,  int timeout )
{
#if 1
	ssize_t	 retlen;
	size_t	sendlen;
	char	*head;
	char	*t;
	char	buf[MAIN_TCP_MAX_SIZE];//zhangjm TCP_MAX_SIZE没有定义
	int	mflag; 	/*标识是否分配了空间 0未分配. 1已分配*/
	int	n;
	int     i;
	
	Assert( s != NULL && *s != 0 );//条件为真则继续，否则调用abort停止。
	Assert( len != 0  );
	Assert( timeout > 0 || timeout == TIME_INFINITE );//这里的timeout是用来干什么的时间�

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
	memcpy( t, s, len );
	
	Main_Log("before WirteLengthInTime -------------");
	n = WriteByLengthInTime( fd, t, sendlen, timeout );
	Main_Log("after WirteLengthInTime -------------");
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
#endif
#if 0
//带4字节长度的传送方式
	ssize_t		retlen;
	char		*head;
	char		*t;
	char		buf[TCP_MAX_SIZE];
	size_t		sendlen;
	int			mflag; 	/*标识是否分配了空间 0未分配. 1已分配*/
	int			n;
	int 		headlen=4;
	Assert( s != NULL && *s != 0 );
	Assert( len != 0 && headlen != 0 );
	Assert( timeout > 0 || timeout == TIME_INFINITE );

	mflag = 0;
	sendlen = headlen+len;

	if( sizeof(buf) >= sendlen )	/*预先分配的变量buf的空间足够容纳所有要发送的数据*/
		t = buf;
	else							/*预先分配的变量buf的空间不足容纳所有要发送的数据,故分配空间*/
	{
		t = (char*)Malloc(sendlen);
		if( t == NULL )
		{
			RptLibErr( errno );
			return -1;
		}
		mflag = 1;
	}

	sprintf( t, "%0*d", headlen, len );
	memcpy( t+headlen, s, len );
	
	Main_Log("before WirteLengthInTime -------------");	
	n = WriteByLengthInTime( fd, t, sendlen, timeout );
	Main_Log("after WirteLengthInTime -----------length=[%d]",n);
	
	if( mflag == 1 )
		Free(t);

	if( n == sendlen )	/*数据全部发送*/
		return 0;
	else if( n == -1 ) /*出错*/
		return -1;
	else if( n >= 0 && n < sendlen )
	{
		if( GetLibErrno() == ETIME ) /*超时*/
			return -2;
		else
			return -1;
	}

#endif
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

static ssize_t Main_TcpRecvDataInTime( int fd, char *s, size_t *buflen,  int timeout ) //size_t headlen,
{
	ssize_t	recvlen;
	size_t	len;
	//size_t headlen =8;
	//char	head[TCP_MAX_HEAD_LEN + 1];
	char	buf[MAIN_TCP_MAX_SIZE];
	char	*t;
	int	mflag;
	int 	n;
	int	ret;
	
	//Assert( TCP_MAX_HEAD_LEN >= headlen );
	Assert( s != NULL );

	mflag = 0;
	memset( buf, 0, sizeof(buf) );
	len =*buflen;  
	if ( sizeof(buf) >= len ) {
		t = buf;
		memset( t, 0, *buflen );
	} else {
		t = Malloc( len );
		if ( t == NULL )
			return -1;
		memset( t, 0, len );
		mflag = 1;
	}
	n = ReadByLengthInTime( fd,t, len, timeout );
	if ( n == -1 ) {
		ret = -1;
	} else if ( n >= 0 && n < len ) {
		printf("收到的长度[%d]小于预期收到的长度[%d],值[%s]\n", n,len,Main_Asc2Hex(t, len));
		memcpy( s, t, len);		
		//*buflen = 0;		
		memset(buflen,0, sizeof(size_t));
		//memcpy(buflen,&n, sizeof(size_t));
		ret = -2;
	} else {		
		memcpy( s, t, *buflen);
		s[*buflen] = 0;
		ret = 1;
	}
	if ( mflag == 1 ) {
		Free( t );
	}
	//if(n>0){*buflen =n;}else{*buflen =0;}
	return ret;
}

static int Main_ReadALine( FILE *f, char *s, int MaxLen ){
	int i, c;

	for( i = 0; i < MaxLen; i++ )
	{
		c = fgetc( f);
		if( c == '\t' )
			break;//tab is also the end.
		if( c == EOF || c == '\n' )
			break;
		s[i] = c;
	}

	s[i] = 0;

	if( i == MaxLen )
	{
		for( ; ; )
		{
			c = fgetc(f);
			if( c == EOF || c == '\n' )
				break;
		}
	}
	if( c == EOF )
		return 1;
	else
		return 0;
}


static int Main_IsFileExist(const char * sFileName ){
	int ret;
	extern int errno;
	if( sFileName == NULL || *sFileName == 0 )
		return -1;
	ret = access( sFileName, F_OK );//成功返回0.判断是否存在sFileName F_OK为是否存在R_OK，W_OK与X_OK用来检查文件是否具有读取、写入和执行的权限。
	if( ret < 0 )
	{
		if( errno == ENOENT )
		{
			return 0;//not exist
		}
		else
		{			
			return -1;
		}
		
	}
	return 1;
}
//-
//去除字符串两侧空格及回车换行符,需要字符串带结束符
//-
static char *Main_TrimAll( char *buf )
{
	int i,firstchar,endpos,firstpos;

	if(  NULL  ==buf){
		return NULL;
		};

	endpos = firstchar = firstpos = 0;
	for( i = 0; buf[ i ] != '\0'; i++ )
	{
		if( buf[i] ==' ' || buf[i] == '\t' || buf[i] == '\n' || buf[i] == '\r')
		{
			if( firstchar == 0 )
				firstpos++;
		}
		else
		{
			endpos = i;
			firstchar = 1;
		}
	}

	for( i = firstpos; i <= endpos; i++ )
		buf[i - firstpos] = buf[i];
	buf[i - firstpos] = 0;
	return buf;
}
//split s with stop, return the begin of s to the first found char, stop. 
static char *Main_split(char *s, char stop){
	char *data;
	char *tmp;
	int i, len, j;

	len = strlen(s);
	tmp = s;
	data = (char *)malloc( sizeof(char) * (len + 1));
	if(NULL == data ){
		return NULL;
	}

	for(i =0; i < len; i ++){
		if (s[i] != stop){
			data[i] = s[i] ;
		}
		else {
			i ++;
			break;
		}
	}
	data[i] = '\0';
	for(j = i; j < len; j++) 
		s[j - i] = tmp[j];
	s[len -i] = '\0';
	return data;
}
//--------------------------------------------------------------------------



//-------------------------------------------
//初始化配置参数
//-------------------------------------------


//first read ini file by readIni fuction , then change or add ini data, last write ini data to ini file by writeIni fuction.
static int Main_readIni(char *fileName){

	//char *method;
	char  my_data[1024];
	char *tmp_ptr, *tmp;
	int data_len;
	int i,ret;
	int c;
	FILE *f;
	
	if(  NULL== fileName ){
		return -1;
	}
	ret = Main_IsFileExist(fileName);//成功返回1
	if(ret != 1){
		char pbuff[256];
		bzero(pbuff,sizeof(pbuff));
		sprintf( pbuff, "file[%s] not exist in readIni  error!! ", fileName );
		Main_Log(pbuff);
		exit(1);//zhangjmexit(1)干什么的
		return -2;
	}
	f = fopen( fileName, "rt" );//返回指向该文件的指针
	if( !f  ){	
		char pbuff[256];
		bzero(pbuff,sizeof(pbuff));
		sprintf( pbuff, "file[%s] open error in readIni!! ", fileName );
		Main_Log(pbuff);
		exit(1);
		return -3;
	}	
	bzero(my_data, sizeof(my_data));
	for(i=0;i<sizeof(my_data)-1;i++){
		c = fgetc( f);//f所指的文件中读取一个字符。若读到文件尾而无数据时便返回EOF
		if( c == EOF)
			break;
		my_data[i] = c;
	}
	my_data[i] = 0;	
	fclose( f );
	
	
	bzero(&(fcc_ini[0].name), sizeof(MAIN_INI_NAME_VALUE)*MAIN_INI_MAX_CNT);	
	i = 0;
	while (my_data[0] !='\0'){
		tmp = Main_split(my_data, '=');// split data to get field name.分离出=号之前的东西
		bzero(&(fcc_ini[i].name), sizeof(fcc_ini[i].name));
		strcpy(fcc_ini[i].name, Main_TrimAll(tmp) );
		tmp = Main_split(my_data, '\n');// split data to get value.
		bzero(&(fcc_ini[i].value), sizeof(fcc_ini[i].value));
		strcpy(fcc_ini[i].value, Main_TrimAll(tmp));
		i ++;
	}
	//free(my_data);
	return i--;	
}

static int Main_writeIni(const char *fileName){
	int fd;
	int i, ret;
	char temp[128];
	
	if(  NULL== fileName ){
		return -1;
	}	
	//打开文件
	fd = open( fileName, O_WRONLY|O_CREAT|O_TRUNC, 00644 );
	if( fd < 0 )
	{
		//UserLog( "打开当前运行工作路径信息文件[%s]失败.",fileName );
		Main_Log("file open error in writeIni!!");
		exit(1);
		return -1;
	}
	for(i=0;i<MAIN_INI_MAX_CNT;i++){
		if( (fcc_ini[i].name[0] !=0)&&(fcc_ini[i].value[0] !=0) ){
			bzero(temp,sizeof(temp));
			sprintf(temp,"%s\t = %s  \n", fcc_ini[i].name,  fcc_ini[i].value);
			write( fd, temp, strlen(temp) );
		}
	}	
	close(fd);
	return 0;
}
static int Main_addIni(const char *name, const char *value){
	int i,n;
	
	if(  (NULL== name)||(NULL== value)  ){
		return -1;
	}	
	n = strlen(name);
	for(i=0;i<MAIN_INI_MAX_CNT;i++){
		if( memcmp(fcc_ini[i].name,name,n)  == 0 ){
			return -1;
			break;
		}
		else if(fcc_ini[i].name[0] == 0){
			bzero( fcc_ini[i].name, sizeof(fcc_ini[i].name)  );
			bzero( fcc_ini[i].value, sizeof(fcc_ini[i].value)  );			
			memcpy(fcc_ini[i].name, name, strlen(name) );
			memcpy(fcc_ini[i].value, value, strlen(value) );
			return 0;
		}
	}
		
		return -2;
}
static int Main_changeIni(const char *name, const char *value){
	int i,n;
	int ret;
	
	if(  (NULL== name)||(NULL== value)  ){
		return -1;
	}	
	n = strlen(name);
	for(i=0;i<MAIN_INI_MAX_CNT;i++){
		if( memcmp(fcc_ini[i].name,name,n)  == 0 ){
			bzero( fcc_ini[i].value, sizeof(fcc_ini[i].value)  );
			memcpy(fcc_ini[i].value, value, strlen(value) );
			break;
		}
	}
	if(i == MAIN_INI_MAX_CNT){
		ret = Main_addIni(name, value);
		if(ret < 0){
			return -2;
		}
	}		
	else
		return 0;
}


char *Main_upper(char *str){
	int i, n;
	if(NULL == str){
		return NULL;
	}
	n = strlen(str);
	for(i=0;i<n;i++){
		if( (str[i]>='a') && (str[i]<='z') ){
			str[i] +='a' -'A';
		}
	}
	return str;
}
int Main_InitConfig(){
	int ret,i,iniFlag;
	char configFile[256];
	
	bzero(configFile,sizeof(configFile));
	memcpy(configFile,MAIN_CONFIG_FILE,sizeof(MAIN_CONFIG_FILE));
	ret = Main_readIni(configFile);//读配置文件，将IP和各个端口读出存放在结构体fcc_ini中返回值为读到的个数
	if(ret < 0){
		Main_Log("readIni error in _InitServerIP");
		return -1;
	}
	iniFlag = 0;
	//static int PC_SERVER_TCP_PORT,PC_SERVER_UDP_PORT,MAIN_TCP_PORT;
	for(i=0;i<ret;i++){
		if( memcmp(fcc_ini[i].name,"PC_SERVER_IP",11) == 0){
			memcpy(PC_SERVER_IP, fcc_ini[i].value,sizeof(PC_SERVER_IP ) );//sizeof(fcc_ini[i].value memcmp两个比较一般大返回0前一个大返回正后一个大返回负
			Main_Log("PC_SERVER_IP[%s]",PC_SERVER_IP);
			iniFlag += 1;
		}else if( memcmp(fcc_ini[i].name,"PC_SERVER_TCP_PORT",18) == 0 ){
			PC_SERVER_TCP_PORT=atoi(fcc_ini[i].value);
			Main_Log("PC_SERVER_TCP_PORT[%d]",PC_SERVER_TCP_PORT);
			iniFlag += 1;
		}else if( memcmp(fcc_ini[i].name,"PC_SERVER_UDP_PORT",18) == 0 ){
			PC_SERVER_UDP_PORT=atoi(fcc_ini[i].value);//atoi是将字符串转换为数字如"100"变成100
			Main_Log("PC_SERVER_UDP_PORT[%d]",PC_SERVER_UDP_PORT);
			iniFlag += 1;
			//memcpy(gFcc._SERVER_UDP_PORT, fcc_ini[2].value, sizeof(fcc_ini[2].value) );
		}else if( memcmp(fcc_ini[i].name,"FCC_TCP_PORT",12) == 0 ){
			MAIN_TCP_PORT=atoi(fcc_ini[i].value);
			Main_Log("MAIN_TCP_PORT[%d]",MAIN_TCP_PORT);
			iniFlag += 1;
			//memcpy(gFcc._SERVER_UDP_PORT, fcc_ini[2].value, sizeof(fcc_ini[2].value) );
		}else{
			//Main_Log("配置数据出错,  不是预期的三个配置,名字是[%s],值是[%s]",fcc_ini[i].name,fcc_ini[i].value);
			//iniFlag = -1;
		}
	}
	if(iniFlag != 4 ) 
		return -1;
	else 
		return 0;
 }


 
//-------------------------------------------

//--------------------------------------------------------------------------------
//串口和通道访问函数
//------------------------------------------------------------------------------


static int Main_OpenSerial( char *ttynam ){
	int fd;
	struct termios termios_new;

	fd = open (ttynam, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if( fd < 0 )
	{
		//UserLog( "打开设备[%s]失败", ttynam );
		return -1;
	}
	tcgetattr(fd, &termios_new);
	cfsetispeed(&termios_new,B115200);
	cfsetospeed(&termios_new,B115200);
	/*set 8 bit */
	termios_new.c_cflag &= ~CSIZE;
	termios_new.c_cflag |= CS8;
	/*parity checking */
	termios_new.c_cflag &= ~PARENB;   /* Clear parity enable */
	/*1 stop bit */
	termios_new.c_cflag &= ~CSTOPB;
	/*no start bit */
	termios_new.c_cflag |= CLOCAL;
	termios_new.c_cflag |= CREAD;           //make port can read
	termios_new.c_cflag &= ~CRTSCTS;  //NO flow control
	termios_new.c_oflag = 0;
	termios_new.c_lflag  = 0;
	termios_new.c_iflag &= ~(IXON |IXOFF |IXANY);
	termios_new.c_iflag &= ~(INPCK | ISTRIP | ICRNL);
	termios_new.c_cc[VTIME] = 1;       /* unit: 1/10 second. */
	termios_new.c_cc[VMIN] = 1; /* minimal characters for reading */

	tcflush (fd, TCIOFLUSH);
	/* 0 on success, -1 on failure */
	if( tcsetattr(fd, TCSANOW, &termios_new) == 0 )	/*success*/
		return fd;
	return -1;

}


//读写串口
int Main_WriteTty(int fd, unsigned char *msg, int msgLen){
	int ret;
	int timeout = 5;
	//unsigned char buff[64],*result;
	
	//fd = OpenSerial( MAIN_alltty[serial_num]);
	if( fd <= 0 )
	{
		Main_Log( "串口fd[%d]负值错误",fd );
		return -1;
	}
	//Main_Log( "after Main_Writetty and 0 to moved test!" );
	
	//ret = WriteByLengthInTime( fd, Hex2Asc(nv[1].value, strlen(nv[1].value)),strlen(nv[1].value)/2, timeout );
	ret = WriteByLengthInTime( fd, msg,msgLen, timeout );
	if( ret <= 0 )
	{
		Main_Log( "Write Failed" );
		return -1;
	}
	//close(fd);
	//result=Main_Asc2Hex(buff,ret);
	//len = strlen(result);
	return ret;
}

int Main_ReadTty(int fd, unsigned char *msg, int *recvLen){
	int ret,len;
	int timeout = 5;
	unsigned char buff[64],*result;
	
	//fd = OpenSerial( MAIN_alltty[serial_num]);
	if( fd <= 0 )
	{
		Main_Log( "串口fd[%d]负值错误",fd );
		return -1;
	}
	//Main_Log( "after Main_Writetty and 0 to moved test!" );
	len = *recvLen;
	if( sizeof(buff) < len)
	{
		Main_Log( "FccReadTty错误,buff长度[%d]小于待接收长度[%d]",sizeof(buff), len);
		return -1;
	}
	
	memset( buff, 0, sizeof(buff) );
	ret = ReadByLengthInTime( fd, buff, sizeof(buff)-1, timeout);
	if( ret <= 0 )
	{
		Main_Log( "Read Failed" );
		return -1;
	}
	
	//result=Main_Asc2Hex(buff,ret);
	len = ret;
	memcpy(msg,buff,len);
	return len;
}


//--------------------------------
//向PC注册
/*FCC向PC 注册:
*FCC发:1104+本机IP地址+4个串口通道数,PC回复:110111或者 110122,其中110111为1号,110122为2号FCC.*/
//static unsigned char FccNum;
//static char PC_SERVER_IP[16]; //PC服务器IP地址
//static int PC_SERVER_TCP_PORT,PC_SERVER_UDP_PORT,MAIN_TCP_PORT;
int Main_Login(){
	#define  LOGIN_TIME 2
	int i, ret, tmp, sockfd,fd,n,k;//,
	//int sockflag =0;
	unsigned char *sIPName;	//sPortName[5],
	unsigned char sendBuffer[16] ,*pBuff;	//,buff[64]
	unsigned int msgLength  ;
	int timeout = 5, tryn=10;
	unsigned long localIP;

	
	bzero(sendBuffer,sizeof(sendBuffer));
	
	
	//头部数据设置
	
	msgLength =6;
	sendBuffer[0] = 0x11;
	sendBuffer[1] = 0x04;	
	localIP = Main_GetLocalIP();
	
	//FCC的 IP地址
	memcpy(&sendBuffer[2],(unsigned char *)&localIP,4);//zhangjm(unsigned char *)&localIP什么意思�

	//串口通道数目
	//memcpy(&sendBuffer[6],&portChanel[START_PORT],4);//(unsigned char *)&localIP
	memset(&sendBuffer[6],0,4);
	//i=k;
	//向PC上传数据
	Main_Log("向PC上传数据[%s]",Main_Asc2Hex(sendBuffer, msgLength) );
	i = 0;
	sIPName =PC_SERVER_IP;//inet_ntoa(  *((struct in_addr *)(&gpIfsf_shm->recv_hb[i].host_ip))  );
	while(1){
		i++;
		if( i > LOGIN_TIME ) exit(-1);//zhangjm这里的i是为了3次握手嘛
		sockfd = Main_TcpConnect(sIPName,PC_SERVER_TCP_PORT , timeout);//(timeout+1)/2   //sPortName
		if (sockfd >0 ){
			//sleep(1);
			ret =Main_TcpSendDataInTime(sockfd, sendBuffer, msgLength, timeout);			//Act_V(CD_SOCK_INDEX);			
			if (ret < 0 ){
				Main_Log("TcpSendDataInTime发送数据不成功,Main_Login发送数据 失败,IP[%s],port[%d]",sIPName,PC_SERVER_TCP_PORT);
				close(sockfd);
				sleep(1);//sleep(10000);
				continue;
			}else{				
				break;
			}
		} else {
			Main_Log("Tcp Connect 不成功,Main_Login发送数据 失败,IP[%s],port[%d]",sIPName,PC_SERVER_TCP_PORT);
			close(sockfd);
			sleep(1);//sleep(10000); 			
			continue;
			//return -1;
		}
	}
	 //开始从PC接收返回数据
	Main_Log("开始从PC接收返回数据");
	bzero(sendBuffer,sizeof(sendBuffer));
	for(i=0;i<tryn;i++){
		msgLength = 3;		
		ret =Main_TcpRecvDataInTime(sockfd, sendBuffer, &msgLength, timeout);			//Act_V(CD_SOCK_INDEX);			
		if (ret < 0 ){
			Main_Log("TcpRecvDataInTime接收数据不成功,Main_Login接收数据 失败");
			sleep(10);
			continue;
		}
		break;
	}
	
      if( (sendBuffer[0] != 0x11)||(sendBuffer[1] != 0x01) ){
	  	Main_Log("Tcp Connect 不成功,Main_Login接收数据 失败,[%02x%02x%02x]",sendBuffer[0],sendBuffer[1],sendBuffer[2]);
		close(sockfd);
		return -1;
	}
	//CSS_LevelLog("gpCSS_Shm->css._TLG_SEND_INTERVAL赋值[%02x%02x]",sendBuffer[1],sendBuffer[2]);
      	//if(sendBuffer[2] == 0x11) {
	//		FccNum = 1;
	//		Main_Log("FccNum赋值[%d],主FCC",FccNum);
	//}else if(sendBuffer[2] == 0x22){ 
	//	FccNum = 2;
	//	Main_Log("FccNum赋值[%d],从FCC",FccNum);
	//}else{
	//	Main_Log("Main_Login接收数据错误,Main_Login接收数据 失败");
	//	return -1;
	//}
	close(sockfd);
	//Main_Log("FccNum赋值[%d]",FccNum);
	return 0;
}

int Main_TCPSendRecv(unsigned char *sendBuffer, int sendLength, unsigned char *recvBuffer, int *recvLength){
	//#define  LOGIN_TIME 1000
	int i, ret, tmp, sockfd,fd,n,k;//,
	//int sockflag =0;
	unsigned char *sIPName;	//sPortName[5],
	unsigned char buff[64] ,*pBuff;	//,buff[64]sendBuffer[16] ,
	unsigned int msgLength  ;
	int timeout = 5, tryn = 10;
	unsigned long localIP;

	
	i = 0;
	sIPName =PC_SERVER_IP;//inet_ntoa(  *((struct in_addr *)(&gpIfsf_shm->recv_hb[i].host_ip))  );		
	while(1){
		msgLength = sendLength;
		i++;
		if( i > tryn ) return -1;
		sockfd = Main_TcpConnect(sIPName,PC_SERVER_TCP_PORT , timeout);//(timeout+1)/2   //sPortName
		if (sockfd >0 ){
			sleep(5);
			ret =Main_TcpSendDataInTime(sockfd, sendBuffer, msgLength, timeout);			//Act_V(CD_SOCK_INDEX);			
			if (ret < 0 ){
				Main_Log("TcpSendDataInTime发送数据不成功,IP[%s],port[%d]",sIPName,PC_SERVER_TCP_PORT);
				close(sockfd);
				sleep(10);//sleep(10000);
				continue;
			}else{				
				break;
			}
		} else {
			Main_Log("Tcp Connect 不成功,IP[%s],port[%d]",sIPName,PC_SERVER_TCP_PORT);
			close(sockfd);
			sleep(60);//sleep(10000); 			
			continue;
			//return -1;
		}
	}
	 //从PC接收返回数据
	Main_Log("从PC接收返回数据");
	 //sleep(5);//等待对方执行完毕
	//bzero(recvBuffer,sizeof(recvBuffer));
	for(i=0;i<tryn;i++){
		msgLength = *recvLength;		
		ret =Main_TcpRecvDataInTime(sockfd, recvBuffer, &msgLength, timeout);			//Act_V(CD_SOCK_INDEX);			
		if (ret < 0 ){
			Main_Log("TcpRecvDataInTime接收数据不成功");
			sleep(10);
			continue;
		}
		break;
	}
      if( (recvBuffer[0] == 0)&&(recvBuffer[1] == 0) ){
	  	Main_Log("Tcp Connect 不成功,[%02x%02x%02x]",recvBuffer[0],recvBuffer[1],recvBuffer[2]);
		close(sockfd);
		return -1;
	}	
	close(sockfd);
	memcpy((char *)recvLength,(char *)&msgLength,sizeof(int));
	Main_Log("Main_SendRecv接收数据[%s]",Main_Asc2Hex(recvBuffer, msgLength));	
	return 0;
}


//--------------------------------------------------
//通讯服务程序
//-----------------------------------------------
//static char PC_SERVER_IP[16]; //PC服务器IP地址
//static int PC_SERVER_TCP_PORT,PC_SERVER_UDP_PORT,MAIN_TCP_PORT;
int  Main_WaitStartCmd( ){
	int sock;
	int newsock;
	int tmp_sock;
	int startFlag;
	int ret;
	int i;
	int len;
	int msg_lg = 512;
	int timeout = 10;       // 10s
	int tryNum = 5;
	//int tryN = 5;  
	int errcnt = 0;
	int cd_exit = 0;
	unsigned char buffer[64];
	unsigned char sBuffer[64];
	extern int errno;
		
	
	
	
	sock = Main_TcpServInit(MAIN_TCP_PORT);
	if ( sock < 0 ) {
		Main_Log( "实时进程初始化socket失败.port=[%s]\n", MAIN_TCP_PORT );
		//Free( cmd );
		return  ;
	}
	//fcntl(sock, F_SETFL, O_NONBLOCK);   // 非阻塞模式
	
	newsock  = -1;
	tmp_sock = -1;
	while (1) {	
		if (errcnt > 32) {
			Main_Log("out of ERR_CNT, close the tcp connection (PC.C --> FCC.S)");
			close(newsock);
			newsock = -1;
			errcnt = 0;
//			RunLog("############################## errcnt: %d", errcnt);
		}



#if 0
		if (newsock < 0) {  // 如果没有连接，则监听
			while (1) {
				newsock = TcpAccept( sock, NULL );
				if (newsock < 0) {
					continue;
				}
				CSS_LevelLog("TcpAccept new one, newsock: %d ", newsock);
				break;
			}
		}
#endif

#if 1
		tmp_sock = Main_TcpAccept(sock);//, NULL
		Main_Log("Main_WaitStartCmd连接,soket号[%d]", tmp_sock);
		if (tmp_sock > 0) {
			if (newsock > 0) {
				close(newsock);
			}
			newsock = tmp_sock;
			tmp_sock = -1;//zhangjm为什么要关闭文件描述符newsock再赋值
		}
#endif
		if (newsock <= 0) {
			continue;
		}

		
		//msg_lg = sizeof(buffer);
		memset( buffer, 0, sizeof(buffer) );
		startFlag = 0;
		sleep(5);
		while(!startFlag){
			if ( errcnt > tryNum ) {
				/*错误处理*/
				Main_Log( "接收数据[%d]次都失败", tryNum );
				break; 
			}
			msg_lg = 3;
			sleep(5);
			ret = Main_TcpRecvDataInTime(newsock, buffer, &msg_lg, timeout );//TIME_INFINITE	
			if (ret < 0) {
				if (errno == 4) {  // time out zhangjm errno是从那里计数的�
					Main_Log("进程超时,接收到后台命令 buffer[%s]\n" , Main_Asc2Hex(buffer, msg_lg) );//
					continue;
				} else if (errno == 11)  {  // maybe CD quit or restart, && (gpIfsf_shm->need_reconnect == 0))
					//gpIfsf_shm->need_reconnect = 1;
					Main_Log("Client need to reconnect to Server");
				}	
				Main_Log( "进程接收数据失败,errno:[%d][%s],buffer: %s", errno, strerror(errno), \
					Main_Asc2Hex(buffer, 2) );
				errcnt++;
				sleep(1);
				continue;
			} else {
				Main_Log( "<<<进程收到命令: [%s(%d)]", Main_Asc2Hex(buffer, msg_lg ), msg_lg);				
				if ( 0x22 != buffer[0] ){ //received data must be error!!(0 == (buffer[0]>>4) )||(0 ==( buffer[0]&0x0f) )
					Main_Log( "received data error" );
					errcnt++;
					sleep(1);
					continue;
				}

				if (0 == msg_lg){ //receivd data must be error!!
					Main_Log( "received size is  0 error" );
					errcnt++;
					//回复错误
					sBuffer[0] = 0x2f;
					sBuffer[1] = 0;
					//memcpy(&sBuffer[1],gpCSS_Shm->css._STATION_NO_BCD,5);
					ret = Main_TcpSendDataInTime(newsock, sBuffer, 2, timeout);
					if(ret <=0){				
						Main_Log("实时进程回复数据 失败");
					}
					
					sleep(1);
					continue;
				}

				
				
				//处理后台命令.
				if (1 == buffer[1]){ 
					if(0x11 == buffer[2]){
						/*if(1 != FccNum) {
							Main_Log("PC启动命令 错误,本机主FCC");
							sBuffer[0] = 0x2f;
							sBuffer[1] = 0;
							//memcpy(&sBuffer[1],gpCSS_Shm->css._STATION_NO_BCD,5);
							ret = Main_TcpSendDataInTime(newsock, sBuffer, 2, timeout);
							if(ret <0){				
								Main_Log("进程回复数据 失败");
								//continue;
							}
							close(newsock);close(sock);
							return -1;
						}*/
						sBuffer[0] = buffer[0];
						sBuffer[1] = 0;
						//memcpy(&sBuffer[1],gpCSS_Shm->css._STATION_NO_BCD,5);
						ret = Main_TcpSendDataInTime(newsock, sBuffer, 2, timeout);
						if(ret <0){				
							Main_Log("进程回复数据 失败");
							//continue;
						}
						errcnt = 0;
						close(newsock);close(sock);
						return 1;//break;
					}/*else if(0x22 == buffer[2]){
						if(2 != FccNum) {
							Main_Log("PC启动命令 错误,本机从FCC");
							sBuffer[0] = 0x2f;
							sBuffer[1] = 0;
							//memcpy(&sBuffer[1],gpCSS_Shm->css._STATION_NO_BCD,5);
							ret = Main_TcpSendDataInTime(newsock, sBuffer, 2, timeout);
							if(ret <0){				
								Main_Log("进程回复数据 失败");
								//continue;
							}
							close(newsock);close(sock);
							return -1;
						}
						sBuffer[0] = buffer[0];
						sBuffer[1] = 0;
						//memcpy(&sBuffer[1],gpCSS_Shm->css._STATION_NO_BCD,5);
						ret = Main_TcpSendDataInTime(newsock, sBuffer, 6, timeout);
						if(ret <0){				
							Main_Log("进程回复数据 失败");
							//continue;
						}
						errcnt = 0;
						close(newsock);close(sock);
						return 1;//break;
					}*/
					else{
						Main_Log( "解析数据失败,收到的数据值不是0x11和0x22" );
					}
				}  else{
					Main_Log( "解析数据失败,收到的数据的长度字节不是1" );
				}				
				break;
				//sleep(10);			
			}
		}
		if ( errcnt > 40 ) {
			/*错误处理*/
			Main_Log( "初始化socket 失败.port=[%d]", MAIN_TCP_PORT );
			break; 
		}
	}

	Main_Log("Main_AcceptPCCmd  return ......");
	close(sock);

	return  -1;
}




//串口收发函数
int Main_ComSendRecv(int serialPortNum){
	int i, ret, tmp,fd,n,k;//,, sockfd
	unsigned char buff[64] ;
	int timeout = 5, tryn = 5;
	char		cmd[32];
	int			cnt;
	
	
	fd = Main_OpenSerial(MAIN_alltty[serialPortNum]);
	if(fd <=0){
		Main_Log( "Main_ComRecvSend打开串口错误" );
		return -1;
	}
	//Main_Log("向PC发送数据");
	Main_Log( "Main_ComSendRecv开始测试串口[%d]" ,serialPortNum);
	for(i=0;i<tryn;i++){
		memset(cmd,0xa5,sizeof(cmd));
		if( sizeof(cmd) > MAX_PORT_CMD_LEN ) {
			Main_Log( "向串口[%d]写的数据长度[%d]超过最大长度[%d]", serialPortNum,sizeof(cmd), MAX_PORT_CMD_LEN );
			close(fd);
			return -1;
		}		 
		//cmd[2] = 0x00+ (sizeof(cmd) -3 ) ;
		//memcpy( cmd+3, buff, len );
		cnt =sizeof(cmd);
		Main_Log("Main_ComRecvSend发送数据[%s]",Main_Asc2Hex(cmd, cnt));
		ret = Main_WriteTty(fd, cmd, cnt);
		if(ret <=0){
			Main_Log( "向串口[%d]写数据失败" ,serialPortNum);
			sleep(5);
			continue;
		}
		//break;
		sleep(5);
		bzero(buff,sizeof(buff));
		cnt =sizeof(cmd);
		ret = Main_ReadTty(fd, buff, &cnt);
		if(ret <=0){
			Main_Log( "串口[%d]读数据失败",serialPortNum );
			sleep(5);
			continue;
		}
		Main_Log("Main_ComRecvSend接收数据[%s]",Main_Asc2Hex(buff, cnt));	
		break;
	}
	if( (ret <=0) ||(i >=tryn) ){
		Main_Log( "从串口[%d]读数据[%d]次全部失败" ,serialPortNum,tryn);
		close(fd);
		return -1;
	}
		
	close(fd);
	return 0;
}
int Main_ComRecvSend(int serialPortNum){
	//#define  LOGIN_TIME 1000
	int i, ret, tmp,fd,n,k;//,, sockfd
	//int sockflag =0;
	//unsigned char *sIPName;	//sPortName[5],
	unsigned char buff[64] ;	//,buff[64]sendBuffer[16] ,,*pBuff
	//unsigned int msgLength  ;
	int timeout = 5, tryn = 5;
	//unsigned long localIP;	
	//int serialPortNum, phyChnl;
	char		cmd[32];
	int			cnt;
	
	
	fd = Main_OpenSerial(MAIN_alltty[serialPortNum]);
	if(fd <=0){
		Main_Log( "Main_ComRecvSend打开串口错误" );
		return -1;
	}
	//Main_Log("向PC发送数据");
	Main_Log( "Main_ComRecvSend开始测试串口[%d]" ,serialPortNum);
	for(i=0;i<tryn;i++){		
		sleep(1);//等待对方执行完毕
		bzero(buff,sizeof(buff));
		cnt =sizeof(cmd);
		ret = Main_ReadTty(fd, buff, &cnt);
		if(ret <=0){
			Main_Log( "串口[%d]读数据失败",serialPortNum );
			sleep(5);
			continue;
		}
		Main_Log("Main_ComRecvSend接收数据[%s]",Main_Asc2Hex(buff, cnt));
		sleep(5);
		//break;
		memset(cmd,0x5a,sizeof(cmd));
		if( sizeof(cmd) > MAX_PORT_CMD_LEN ) {
			Main_Log( "向串口[%d]写的数据长度[%d]超过最大长度[%d]", serialPortNum,sizeof(cmd), MAX_PORT_CMD_LEN );
			close(fd);
			return -1;
		}		 
		//cmd[2] = 0x00+ (sizeof(cmd) -3 ) ;
		//memcpy( cmd+3, buff, len );
		cnt =sizeof(cmd);
		Main_Log("Main_ComRecvSend发送数据[%s]",Main_Asc2Hex(cmd, cnt));
		ret = Main_WriteTty(fd, cmd, cnt);
		if(ret <=0){
			Main_Log( "向串口[%d]写数据失败" ,serialPortNum);
			sleep(5);
			continue;
		}
		sleep(5);//再发送一次
		cnt =sizeof(cmd);
		Main_Log("Main_ComRecvSend发送数据[%s]",Main_Asc2Hex(cmd, cnt));
		ret = Main_WriteTty(fd, cmd, cnt);
		if(ret <=0){
			Main_Log( "向串口[%d]写数据失败" ,serialPortNum);
			sleep(5);
			continue;
		}
		break;
	}
	if( (ret <=0) ||(i >=tryn) ){
		Main_Log( "从串口[%d]读数据[%d]次全部失败" ,serialPortNum,tryn);
		close(fd);
		return -1;
	}
	
	close(fd);
	return 0;
}


//根据检测到的U盘名称挂载U盘
int Main_mountUsb(){

	unsigned char *fileName = "/proc/partitions";
	unsigned char buff[64],usbDev[4],devName[64],mountName[128],rmDev[64];
	FILE *fp;
	int i, j,ret, first_sdx;
	
	fp = fopen(fileName, "rt");
	if(fp == NULL){
		Main_Log("fileName[%s] 打开失败!",fileName);
		return -1;
	}
	
	bzero(buff, sizeof(buff));	
	bzero(usbDev, sizeof(usbDev)); //读取  U盘设备名称
	
	i = 0;
	first_sdx = 0; //第一次读取到sd，置1
	while(i < 15){
		fgets(buff, sizeof(buff), fp);

		if(1 == first_sdx){
			//U盘名称保存到usbDev中
			for(j =0; j < sizeof(buff); j++){
				if(buff[j] == 's'&& buff[++j] == 'd'){ //第二次检测到sdx
					usbDev[0] = buff[++j];
					usbDev[1] = buff[++j];
					Main_Log("buff[%s]  usbDev[%s]",buff, usbDev);
					break;
				}
			}
			break;
		}

		for(j =0; j < sizeof(buff); j++){
			if(buff[j] == 's'&& buff[++j] == 'd'){ //第二次检测到sdx
				first_sdx = 1;  //第一次检测到sdx
			}
		}
		
		bzero(buff, sizeof(bzero));
		i++;
	}

	if(i == sizeof(buff)){
		Main_Log("读取U盘名称失败!buff[%s]", buff);
		return -1;
	}

	//创建设备文件
	bzero(devName, sizeof(devName));
	switch(usbDev[0]){
		case 'a':
			sprintf(devName, "/bin/mknod /dev/sd%s b 8 %d",usbDev, USB_DEV_SDA+atoi(&usbDev[1]));
			break;
		case 'b':
			sprintf(devName, "/bin/mknod /dev/sd%s b 8 %d",usbDev, USB_DEV_SDB+atoi(&usbDev[1]));
			break;
		case 'c':
			sprintf(devName, "/bin/mknod /dev/sd%s b 8 %d",usbDev, USB_DEV_SDC+atoi(&usbDev[1]));
			break;
		case 'd':
			sprintf(devName, "/bin/mknod /dev/sd%s b 8 %d",usbDev, USB_DEV_SDD+atoi(&usbDev[1]));
			break;
		default:
			return -1;
	}

	//尝试删除已有设备文件，重新创建
	bzero(rmDev, sizeof(rmDev));
	sprintf(rmDev,"/bin/rm /dev/sd%s", usbDev);
	system(rmDev);

	ret = system(devName);
	if(ret < 0){
		Main_Log("devName[%s] 创建U盘设备文件失败!", devName);
		return -1;
	}

	//卸载挂载项
	system(Main_USB_UM);
	//挂载U盘
	bzero(mountName,sizeof(mountName));
	sprintf(mountName,"/bin/mount -t vfat /dev/sd%s /home/ftp",usbDev);
	ret = system(mountName);
	if(ret < 0){
		Main_Log("mountName[%s] 挂载U盘失败!", mountName);
		return -1;
	}

	return 0;
}


//
static int Main_readUSB(int usbnum){

	//char *method;
	char  my_data[16];
	char *tmp_ptr, *tmp;
	int data_len;
	int i,ret;
	int c;
	FILE *f;
	
	if( (1 !=usbnum )&&(2 !=usbnum)  ){
		Main_Log("Main_readUSB参数值错误,值[%d]",usbnum);
		return -1;
	}
	//挂接U盘
      	ret = Main_mountUsb();
	if( ret < 0 )
	{
		Main_Log( "***挂载USB命令执行失败\n" );
		return -1;//exit(1);
	}	
	ret = Main_IsFileExist(Main_USB_File);
	if(ret != 1){
		char pbuff[256];
		bzero(pbuff,sizeof(pbuff));
		sprintf( pbuff, "file[%s] not exist in Main_readUSB  error!! ", Main_USB_File );
		Main_Log(pbuff);
		system(Main_USB_UM);
		return -2;
	}
	
	f = fopen( Main_USB_File, "rt" );
	if( !f  ){	
		char pbuff[256];
		bzero(pbuff,sizeof(pbuff));
		sprintf( pbuff, "file[%s] open error in readIni!! ", Main_USB_File );
		Main_Log(pbuff);
		//exit(1);
		system(Main_USB_UM);
		return -3;
	}	
	bzero(my_data, sizeof(my_data));
	for(i=0; i<sizeof(Main_USB_Data); i++){
		c = fgetc( f);
		if( c == EOF)
			break;
		my_data[i] = c;
	}
	my_data[i] = 0;	
	fclose( f );
	//去除挂接U盘
	ret = system(Main_USB_UM);
	if( ret < 0 )
	{
		Main_Log( "***USB命令[%s]执行失败\n" ,Main_USB_UM);
		return -1;//exit(1);
	}	
	if(  0 != memcmp( my_data,Main_USB_Data,sizeof(Main_USB_Data) )  ){
		Main_Log( "读取的数据[%s]和应当读取到的数据[%s]不同\n" ,my_data,Main_USB_Data);
		return -1;
	}
	return 0;	
}

void Main_AcceptPCCmd( ){
	int sock;
	int newsock;
	int tmp_sock;
	int ret;
	int i;
	int len;
	int msg_lg = 512;
	int timeout = 10;       // 10s
	int tryNum = 5;
	//int tryN = 5;  
	int errcnt = 0;
	int cd_exit = 0;
	unsigned char buffer[64];
	unsigned char sBuffer[64];
	extern int errno;
		
	
	
	
	sock = Main_TcpServInit(MAIN_TCP_PORT);
	if ( sock < 0 ) {
		Main_Log( "实时进程初始化socket失败.port=[%s]\n", MAIN_TCP_PORT );
		//Free( cmd );
		return  ;
	}
	//fcntl(sock, F_SETFL, O_NONBLOCK);   // 非阻塞模式
	
	newsock  = -1;
	tmp_sock = -1;
	while (1) {	
		if (errcnt > 32) {
			Main_Log("out of ERR_CNT, close the tcp connection (PC.C --> FCC.S)");
			close(newsock);
			newsock = -1;
			errcnt = 0;
//			RunLog("############################## errcnt: %d", errcnt);
		}



#if 0
		if (newsock < 0) {  // 如果没有连接，则监听
			while (1) {
				newsock = TcpAccept( sock, NULL );
				if (newsock < 0) {
					continue;
				}
				CSS_LevelLog("TcpAccept new one, newsock: %d ", newsock);
				break;
			}
		}
#endif

#if 1
		tmp_sock = Main_TcpAccept(sock);//, NULL
		Main_Log("Main_AcceptPCCmd连接,soket号[%d]", tmp_sock);
		if (tmp_sock > 0) {
			if (newsock > 0) {
				close(newsock);
			}
			newsock = tmp_sock;
			tmp_sock = -1;
		}
#endif
		if (newsock <= 0) {
			continue;
		}

		
		msg_lg = sizeof(buffer);
		memset( buffer, 0, sizeof(buffer) );
		while(1){
			if ( errcnt > tryNum ) {
				/*错误处理*/
				Main_Log( "接收数据[%d]次都失败", tryNum );
				break; 
			}
			ret = Main_TcpRecvDataInTime(newsock, buffer, &msg_lg, timeout );//TIME_INFINITE	
			if (ret < 0) {
				if (errno == 4) {  // time out
					Main_Log("进程超时,接收到后台命令 buffer[%s]\n" , Main_Asc2Hex(buffer, 32) );//
					continue;
				} else if (errno == 11)  {  // maybe CD quit or restart, && (gpIfsf_shm->need_reconnect == 0))
					//gpIfsf_shm->need_reconnect = 1;
					Main_Log("Client need to reconnect to Server");
				}	
				Main_Log( "进程接收数据失败,errno:[%d][%s],buffer: %s", errno, strerror(errno), \
					Main_Asc2Hex(buffer, 2) );
				errcnt++;
				sleep(1);
				continue;
			} else {
				Main_Log( "<<<进程收到命令: [%s(%d)]", Main_Asc2Hex(buffer, msg_lg ), msg_lg);				
				if ( (0 == (buffer[0]>>4) )||(0 ==( buffer[0]&0x0f) )  ){ //received data must be error!!
					Main_Log( "received data error" );
					errcnt++;
					sleep(1);
					continue;
				}

				if (0 == msg_lg){ //receivd data must be error!!
					Main_Log( "received size is  0 error" );
					errcnt++;
					//回复错误
					sBuffer[0] = 0x1f;
					sBuffer[1] = 0;
					//memcpy(&sBuffer[1],gpCSS_Shm->css._STATION_NO_BCD,5);
					ret = Main_TcpSendDataInTime(newsock, sBuffer, 2, timeout);
					if(ret <=0){				
						Main_Log("实时进程回复数据 失败");
					}
					
					sleep(1);
					continue;
				}

				
				
				//处理后台命令.
				Main_Log( "开始处理收到命令: [%s(%d)]", Main_Asc2Hex(buffer, msg_lg ), msg_lg);
				if (0 == buffer[1]){ 
					if(0x99 == buffer[0]){
						Main_Log( "收到退出命令: [%s(%d)],退出", Main_Asc2Hex(buffer, msg_lg ), msg_lg);
						
					}
				}
				
				break;
				//sleep(10);			
			}
		}
		if ( errcnt > 40 ) {
			/*错误处理*/
			Main_Log( "初始化socket 失败.port=[%d]", MAIN_TCP_PORT );
			break; 
		}
	}

	Main_Log("Main_AcceptPCCmd  Stop ......");
	close(sock);

	return  ;
}
/*//主从FCC同步
int Main_SynStart(int NumFcc){
	int ret,n;
	unsigned char WaitSendBuff[2][5] =
		{
			{0xff,0x03,0x11,0,0},
			{0xff,0x03,0x22,0,0}
		};
	unsigned char WaitFlag[2] = {0x11,0x22};
	unsigned char WaitRecvBuff[8];

	
	if((1 != NumFcc)&&(2 !=NumFcc)){
		Main_Log( "***Fcc_SynStart参数错误,程序退出" );
		exit(-1);
	}
	while(1){
		n = 3;
		ret = Main_TCPSendRecv(WaitSendBuff[NumFcc-1],5, WaitRecvBuff,&n);
		if(ret <0){
			Main_Log("发送测试中间同步值错误继续");
			sleep(5);
			continue;
		}else{
			if( (0xff != WaitRecvBuff[0]) || (0x01 != WaitRecvBuff[1])|| (WaitFlag[NumFcc-1] != WaitRecvBuff[2]) ){//0 != memcmp(TestFlag,"2200",2)
				sleep(2);
				continue;
			}else{
				Main_Log("测试中间同步成功,继续测试");
				break;
			}
		}
	}
	return 0;
}
*/
//
void Main_Com_Thread(void  *arg){
	int ret;
	int sn;
	//unsigned char TestFlag[8];
	
	sn = (int *)arg;
	if(sn<0 ||sn >6){
		Main_Log("Main_Com_Thread参数值错误,值[%d],表示串口号",sn);
		//if(2 == FccNum){
			Main_Log("***串口[%d]测试失败",sn);
		//}else{
		//	Main_Log("串口[%d]测试失败",sn);
		//}
		return;
	}
	if(sn == 0){
		Main_Log("***开始串口[%d]测试",sn);		
		ret = 1;//Main_ComSendRecv(sn);
		if(ret <0){			
			Main_Log("***串口[%d]测试失败",sn);				
		}else{
			Main_Result[2]  = Main_Result[2]  | (1<<7 );
			Main_Log("***串口[%d]测试成功",sn);
		}
	}else if(sn%2 == 0){
		Main_Log("***开始串口[%d]测试",sn);		
		ret = Main_ComRecvSend(sn);
		if(ret <0){			
			Main_Log("***串口[%d]测试失败",sn);				
		}else{
			Main_Result[2]  = Main_Result[2]  | (1<<(8-sn-1) );
			Main_Log("***串口[%d]测试成功",sn);
		}
	}else if(sn%2 == 1){
		Main_Log("***开始串口[%d]测试",sn);
		ret = Main_ComSendRecv(sn);
		if(ret <0){			
			Main_Log("***串口[%d]测试失败",sn);				
		}else{
			Main_Result[2]  = Main_Result[2]  | (1<<(8-sn-1) );
			Main_Log("***串口[%d]测试成功",sn);
		}
	}else{
		Main_Log("***串口出错[%d],退出",sn);
		exit(0);
	}
	//return;
}

//主从FCC同步
int Main_SynStart(int NumFcc){
	int ret,n;
	unsigned char WaitSendBuff[2][5] =
		{
			{0xff,0x03,0x11,0,0},
			{0xff,0x03,0x22,0,0}
		};
	//unsigned char WaitFlag[2] = {0x11,0x22};
	unsigned char WaitRecvBuff[8];

	
	if((1 != NumFcc)&&(2 !=NumFcc)){
		Main_Log( "***Fcc_SynStart参数错误,程序退出" );
		exit(-1);
	}
	while(1){
		n = 3;
		ret = Main_TCPSendRecv(WaitSendBuff[NumFcc-1],5, WaitRecvBuff,&n);
		if(ret <0){
			Main_Log("发送测试同步值错误继续");
			sleep(5);
			continue;
		}else{
			if( (0xff != WaitRecvBuff[0]) || (0x01 != WaitRecvBuff[1])){//|| (WaitFlag[NumFcc-1] != WaitRecvBuff[2]) 0 != memcmp(TestFlag,"2200",2)
				sleep(2);
				continue;
			}else{
				break;
			}
		}
	}
	return 0;
}


//主程序
int main( int argc, char* argv[] )
{
	//#define GUN_MAX_NUM 256 //最多256把枪
	int i, j, n,k,len,ret,fd;//,pid,isChild
	unsigned char TestFlag[8];
	//pthread_t ThreadChFlag[64];//通道线程数组
	pthread_t ThreadSrFlag[8];//串口线程数组
	if (argc == 2) {
	       if (strcmp(argv[1], "-v") == 0) {
			printf( "fcc_selftest. PCD version %s\n", PCD_VER);
	       } 
		return 0;
	}
	
	printf("开始运行测试程序\n");
	
	//初始化日志
	printf("初始化日志.\n");
	ret = Main_InitLog(1);//0: UDP日志,1:打印日志
	if(ret < 0){
		printf("日志初始化出错.\n");
		exit(-1);  
	}

		
	/*//初始化共享内存
	ret = Main_InitTheKeyFile();
	if( ret < 0 )
	{
		Main_Log( "初始化Key File失败\n" );
		exit(1);
	}
	*/
		
	//初始化串口通道数目
	
	
	//初始化配置参数
	Main_Log("开始初始化配置参数");
	ret = Main_InitConfig();//成功返回0不成返回-1
	if(ret <0){
		Main_Log("配置参数初始化出错.");
		exit(-1);
	}

	//初始化注册
	Main_Log("开始初始化注册");
	ret = Main_Login();
	if(ret <0){
		Main_Log("初始化注册出错.");
		exit(-1);
	}
	Main_Log("初始化注册成功");
	sleep(5);
	// 创建一个信号量.	
	//通道读初始化	
	
	//循环测试
	while(1){
		//等待测试开始命令
		//if(2 == FccNum){
			Main_Log("***等待测试开始命令");
		//}else{
		//	Main_Log("等待测试开始命令");
		//}
		ret = Main_WaitStartCmd();
		if(ret <0){
			Main_Log("开始测试命令错误,退出");
			exit(-1);
		}
		Main_Log("测试开始命令成功");
# if 0
		if(2 == FccNum){
			Main_Log("***开始控制口(串口0)测试",i);
			ret = Main_ComSendRecv(0);
			if(ret <0){
				Main_Log("***控制口(串口0)测试失败",i);
				//exit(0);
			}else{
				//TestFlag[0] = TestFlag[0] | (1<<7 );					
				Main_Log("***控制口(串口0)测试成功",i);
			}
			ret = Main_SynStart(FccNum);
		}else{
			sleep(30);
			ret = Main_SynStart(FccNum);
		}
#endif		
		//-----------------------------
		//开始测试
		bzero(Main_Result,sizeof(Main_Result) );
		//bzero(ThreadChFlag,sizeof(ThreadChFlag) );
		bzero(ThreadSrFlag,sizeof(ThreadSrFlag) );
		Main_Result[0] = 0x22;
		Main_Result[1] = 0x01;
		
			
		
		//计算通道数目k = 0;	
		
		//通道(背板连接号)测试
		
		//串口测试
		Main_Log("***开始串口测试");
		bzero(TestFlag,sizeof(TestFlag));
		for(i=0; i<=6; i++){
			//Main_Log("开始串口[%d]测试",i);
			if ( pthread_create( &ThreadSrFlag[i], NULL, (void *)Main_Com_Thread, (void *)i ) )  {
				Main_Log("创建串口[%d] 线程错误",i);
				abort();
			}
			sleep(1);
		}		
			
		
		//memcpy(&Main_Result[2],TestFlag,1);
		//Main_Result[2] = TestFlag[0];
		
		//USB口测试
		//if(2 == FccNum){
		/*Main_Log("***开始控制口(串口0)测试",i);
		ret = Main_ComSendRecv(0);
		if(ret <0){
			Main_Log("***控制口(串口0)测试失败",i);
			//exit(0);
		}else{
			TestFlag[0] = TestFlag[0] | (1<<7 );					
			Main_Log("***控制口(串口0)测试成功",i);
		}
		Main_Result[2] =Main_Result[2] | TestFlag[0];*/
		Main_Log("***开始USB口测试");
		bzero(TestFlag,sizeof(TestFlag));
		//USB1口测试
		Main_Log("***开始USB口1测试");
		ret = Main_readUSB(1);
		if(ret <0){
			Main_Log("***USB口[1]测试失败");
			//exit(0);
			sleep(1);
			//USB2口测试
			Main_Log("***开始USB口2测试");
			ret = Main_readUSB(2);
			if(ret <0){
						
			}else{
				Main_Log("***USB口[2]测试成功");
				TestFlag[0] = TestFlag[0] | 0x01;//0x40
			}			
		}else{
			Main_Log("***USB口[1]测试成功");
			TestFlag[0] = TestFlag[0] | 0x01;//0x80
		}		
		Main_Result[2] = Main_Result[2] | TestFlag[0];
		//}else{
		//	Main_Log("不是从FCC[%d],不测试控制口(串口0)和USB口",FccNum);
		//}
		/*for(i=0;i<k;i++){
			if ( pthread_join ( ThreadChFlag[i], NULL ) ) {
				Main_Log("joining 通道[%d]线程错误",i+1);
				ret = Main_PortReadEnd();
				abort();
			}else{
				Main_Log("joining 通道[%d]线程结束",i+1);
			}
		}*/
		for(i=0; i<=6; i++){
			if(0==ThreadSrFlag[i]){
				continue;
			}else if ( pthread_join ( ThreadSrFlag[i], NULL ) ) {
				Main_Log("joining串口[%d]线程错误",i);
				abort();
			}else{
				Main_Log("joining串口[%d]线程结束",i);
			}
		}
			
		
		/*ret = Main_SynStart(1);//同步
		if( ret < 0 ) {
			Main_Log( "通道读结束失败" );
			return -1;
		}*/
		//发送测试结果
		n = 2;
		// if(2 == FccNum){
	 	ret = Main_TCPSendRecv(Main_Result,3, TestFlag,&n);
		if(ret <0){
			Main_Log("发送测试结果错误,退出");
			exit(-1);
		}else{
			if( (0x22 != TestFlag[0]) || (0x0 != TestFlag[1]) ){//0 != memcmp(TestFlag,"2200",2)
				Main_Log("发送测试结果,收到的数据[%02x%02x]错误,退出",TestFlag[0],TestFlag[1]);
				exit(-1);
			}
		}
		//k = 0;//计算通道数目	
		//k=portChanel[3]+portChanel[4]+portChanel[5]+portChanel[6];
		bzero(TestFlag,sizeof(TestFlag));
		TestFlag[0] =1;
		if((0xff&Main_Result[2]) !=0xff){//串口只要0,1,2三个成功就可以了
			TestFlag[0] = 0;
			Main_Log("串口0,1,2,..,6和USB中至少一个没有测试通过[%02x]",Main_Result[2]);
			//exit(0);
		}
		
		if(1 != TestFlag[0] ){
			Main_Log("***FCC主板测试失败,请关闭FCC主板,检查情况,等待再次测试");
			exit(-1);
		}else{
		 	Main_Log("***FCC主板测试成功,等待下一个FCC主板测试");
			exit(0);
		 }
		
	}
	 //Main_AcceptPCCmd();
	
	 Main_Log("***自测试程序异常退出");
	return -1;
}








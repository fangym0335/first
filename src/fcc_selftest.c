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

//#define  TEST_CONFIG_FILE                     "/home/App/ifsf/etc/fcc_selftest.cfg"
#define  TEST_CONFIG_FILE                     "/etc/fcc_selftest.cfg"
#define MAX_PATH_NAME_LENGTH 1024
#define MAX_PORT_CMD_LEN 255

#define TEST_TCP_MAX_SIZE 1280
#define TEST_MAX_ORDER_LEN		128
#define PCD_VER       "v1.03 - 2013.04.01"

//锁信号量索引值
#define TEST_SEM_FILE_INDEX 1000 
#define TEST_SEM_COM3 1003
#define TEST_SEM_COM4 1004
#define TEST_SEM_COM5 1005
#define TEST_SEM_COM6 1006
#define TEST_SEM_RESULT 1100

#define USB_DEV_SDA 0	//USB从设备号
#define USB_DEV_SDB 16
#define USB_DEV_SDC 32
#define USB_DEV_SDD 48
//----------------------------------------------
//实时日志定义
//===============================
#define TEST_MAX_VARIABLE_PARA_LEN 1024 
#define TEST_MAX_FILE_NAME_LEN 1024 
static char gsFccFileName[TEST_MAX_FILE_NAME_LEN + 1];	//记录调试信息的文件名
static int giFccLine;								//记录调试信息的行
static int giFccFd;                                                       //日志文件句柄
static char gcFccLogFlag;
static unsigned  char  gsLocalIP[16];
//自定义分级文件日志,见__Log.
#define Test_Log Test_SetFileAndLine( __FILE__, __LINE__ ), Test__Log
//#define Test_Log Test_SetFileAndLine( __FILE__, __LINE__ ),Test__FileLog
//--------------------------------------------------------------

//-------------------------------------------------------
//串口定义
//串口特征,串口从0开始
#define START_PORT 3
#define MAX_PORT 6
#define PORT_USED_COUNT 3
#define RECLEN 16
//串口字符串
static char TEST_alltty[][11] =
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
unsigned char portChanel[MAX_PORT+1];
//----------------------------------------------------

//-------------------------------------------
//初始化配置参数
//-------------------------------------------
static char PC_SERVER_IP[16]; //PC服务器IP地址
static int PC_SERVER_TCP_PORT,PC_SERVER_UDP_PORT,TEST_TCP_PORT;

#define TEST_INI_FIELD_LEN 63
#define TEST_INI_MAX_CNT 30
typedef struct fcc_ini_name_value{
char name[TEST_INI_FIELD_LEN + 1];
char value[TEST_INI_FIELD_LEN + 1];
}TEST_INI_NAME_VALUE;
static TEST_INI_NAME_VALUE fcc_ini[TEST_INI_MAX_CNT];
//-------------------------------------------------------

//--------------------------------------------------------
//向PC注册
/*FCC向PC 注册:
*FCC发:1104+本机IP地址+4个串口通道数,PC回复:110111或者 110122,其中110111为1号,110122为2号FCC.*/
static unsigned char FccNum;
static unsigned char Gilbarco;
//-------------------------------------------------

//------------------------------------------------
static char Test_USB_UM[]="/bin/umount -l /home/ftp/";
static char Test_USB_File[]="/home/ftp/fcctest.txt";
static char Test_USB_Data[]="1234567890";
//---------------------------------------------

//------------------------------------------------
//结果回复数据
static unsigned char Test_Result[12];
//---------------------------------------------




//------------------------------------------------
static unsigned  long  Test_GetLocalIP();

//-------------------------------------------------
//
int Test_IsSemExist( int index )
{
	int SemId;

	SemId = semget( index, 0, 0 );
	if( SemId == -1 )
		return 0;
	return 1;
}


int Test_CreateSem( int index )
{
	int SemId;
	int	ret;

	union semun {
		int val;
		struct semid_ds *buf;
		ushort *array;
	};
	union semun semun;

	if( Test_IsSemExist( index ) )
		Test_RemoveSem( index );
	SemId = semget( index, 1, IPC_CREAT | PERMS );
	if( SemId < 0 )
	{
		return -1;
	}
	semun.val = SEMINITVAL;
	ret = semctl( SemId, 0, SETVAL, semun );
	if( ret < 0 )
	{
		return -1;
	}
	return 0;
}

int Test_RemoveSem( int index )
{
	int SemId;
	int ret;

	SemId = semget( index, 0, 0 );
	if( SemId == -1 )
	{
		return -1;
	}
	ret = semctl( SemId, 0, IPC_RMID, 0 );
	if( ret < 0 )
	{
		return -1;
	}
	return 0;
}

int Test_GetSemNum( int index )
{
	int SemId;
	int retnum;

	SemId = semget( index, 0, 0 );
	if( SemId == -1 )
	{
		return -1;
	}

	retnum = semctl(SemId, 0, GETVAL, 0);
	if( retnum < 0 )
	{
		return -1;
	}
	return retnum;
}


int Test_Act_P( int index )
{
	int SemId, r;
	struct sembuf buf;

	SemId = semget( index, 0, 0 );
	if( SemId == -1 )
	{
		return -1;
	}

	buf.sem_op  = -1;
	buf.sem_flg = SEM_UNDO;
	buf.sem_num = 0;

	while(1)
	{
		r = semop( SemId, &buf, 1 );
		if( r == -1 && errno == EINTR )
			continue;
		break;
	}
	if( r == -1 )
	{
		return -1;
	}
	return 0;
}


int Test_Act_V( int index )
{
	int SemId, r;
	struct sembuf buf;

	SemId = semget( index, 0, 0 );

	if( SemId == -1 )
		return -1;

	buf.sem_op  = 1;
	buf.sem_flg = SEM_UNDO;
	buf.sem_num = 0;

	while(1)
	{
		r = semop( SemId, &buf, 1 );
		if( r == -1 && errno == EINTR )
			continue;
		break;
	}
	if( r == -1 )
	{
		return -1;
	}
	return 0;
}
//----------------------------------------------------

//--------------------------------------------------------------------------------
//实时日志
//----------------------------------------------
void Test_SetLogFd( const int fd )
{
	giFccFd= fd;
	if(  giFccFd <=  0){
		printf( " {日志文件描述符错误[%d] }\n", fd );
		
	}
}
void Test_SetFileAndLine( const char *filename, int line )
{
	if( strlen( filename ) + 1 > sizeof( gsFccFileName) ){
		printf("");
		exit(-1);}
	strcpy( gsFccFileName, filename );
	giFccLine = line;
}

void Test_GetFileAndLine( char *fileName, int *line )
{
	strcpy( fileName, gsFccFileName );
	*line = giFccLine;
}
void Test_SendUDP(const int fd,  const char *sMessage, int len )
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
		(struct sockaddr *)(&remoteSrv), sizeof(struct sockaddr));
	/*if (ret < 0) {				
		printf("心跳数据发送失败,error:[%s]", strerror(errno));	
	}*/		
	
}

//giLogLevel=0, no log;giLogLevel=1, only print log info; giLogLevel = 2, print log info and write log info to file 
void Test__Log(const char *fmt,...){//const int loglevel, const int fd, 
	va_list argptr;
	char sMessage[TEST_MAX_VARIABLE_PARA_LEN + 1];
	char fileName[TEST_MAX_FILE_NAME_LEN + 1];	/*记录调试信息的文件名*/
	int line;								/*记录调试信息的行*/
	//int level = giFccLogLevel;
	
	
	Test_GetFileAndLine(fileName, &line);	
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
	if(0==gcFccLogFlag)Test_SendUDP( giFccFd, sMessage, strlen(sMessage) );//write
	else{
		printf(sMessage);
		Test_SendUDP( giFccFd, sMessage, strlen(sMessage) );
	}
	//write( giFccFd, "\n", strlen("\n") );
	
	return;
}

int Test_InitLog(int flag){//flag---0: UDP日志,1:打印日志
	int sockfd;
	unsigned char *pIP;
	struct in_addr in_ip;
    	 /*if( ( fd = open( TEST_LOG_FILE, O_WRONLY|O_CREAT|O_TRUNC, 00644 ) ) <= 0 ) {
		printf( "打开日志文件[%s]失败\n", TEST_LOG_FILE );
		return -1;
	}*/
	if ( (sockfd = socket(AF_INET,SOCK_DGRAM,0)) == -1 )
        {
                printf("socket function error!\n");
                return -1;
        }
	bzero(gsLocalIP,sizeof(gsLocalIP));
	
	in_ip.s_addr = Test_GetLocalIP() ;
	//printf("[%d]   ",in_ip.s_addr );
	//pIP = inet_ntoa(in_ip );	
	sprintf(gsLocalIP, "%s", inet_ntoa(in_ip ) );
	//printf("[%s] \n",gsLocalIP );
	//memcpy(gsLocalIP,pIP,15);
	giFccFd = sockfd;
	if(0 == flag) gcFccLogFlag = 0;
	else if(1 == flag) gcFccLogFlag = 1;
	else printf("Test_InitLog函数日志初始化错误,flag[%d]\n",flag);
	//Test_SetLogLevel(loglevel);
	return 1;
}
//------------------------------------------------

//-----------------------------------------------------------------------
//公用函数
//-----------------------------------------------------------------------

unsigned char* Test_Asc2Hex( unsigned char *cmd, int cmdlen )
{
	static char retcmd[TEST_MAX_ORDER_LEN*2+1];
	int	i;

	memset( retcmd, 0, sizeof(retcmd) );
	for( i = 0; i < cmdlen; i++ ) {
		sprintf(retcmd + i * 2, "%02x", cmd[i]);
	}

	return retcmd;
}
//获取本机IP,返回本机IP地址，返回-1失败，解决DHCP问题
static unsigned  long  Test_GetLocalIP(){//参见tcp.h和tcp.c文件中有,可以选择一个.
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

int Test_TcpServInit( int port )//const char *sPortName, int backLog
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

	sockFd = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
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
		Test_Log( "错误号:%d", errno );
		return -1;
	}

	if( listen( sockFd, backLog ) < 0 )
	{
		Test_Log( "错误号:%d",errno );
		return -1;
	}

	return sockFd;
}

int Test_TcpAccept( int sock )// , SockPair *pSockPair
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

		newsock = accept( sock, ( SA* )&ClientAddr, &len );
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

	Test_Log( "错误号:%d",errno );
	return -1;
}
/*当nTimeOut设置为TIME_INFINITE或者设置的时间比系统缺省时间长时,
 connect在完成返回前都只等待系统缺省时间*/
int Test_TcpConnect( const char *sIPName,int Ports , int nTimeOut )//const char *sPortName
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

	if( ( addr = GetTcpAddr( sIPName ) ) == INADDR_NONE )
	{
		RptLibErr( TCP_ILL_IP );//Test_Log
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
static ssize_t Test_TcpSendDataInTime( int fd, const char *s, size_t len,  int timeout )
{
#if 1
	ssize_t	 retlen;
	size_t	sendlen;
	char	*head;
	char	*t;
	char	buf[TCP_MAX_SIZE];
	int	mflag; 	/*标识是否分配了空间 0未分配. 1已分配*/
	int	n;
	int     i;
	
	Assert( s != NULL && *s != 0 );
	Assert( len != 0  );
	Assert( timeout > 0 || timeout == TIME_INFINITE );

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
	
	Test_Log("before WirteLengthInTime -------------");
	n = WriteByLengthInTime( fd, t, sendlen, timeout );
	Test_Log("after WirteLengthInTime -------------");
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
	
	Test_Log("before WirteLengthInTime -------------");	
	n = WriteByLengthInTime( fd, t, sendlen, timeout );
	Test_Log("after WirteLengthInTime -----------length=[%d]",n);
	
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

static ssize_t Test_TcpRecvDataInTime( int fd, char *s, size_t *buflen,  int timeout ) //size_t headlen,
{
	ssize_t	recvlen;
	size_t	len;
	//size_t headlen =8;
	//char	head[TCP_MAX_HEAD_LEN + 1];
	char	buf[TEST_TCP_MAX_SIZE];
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
		printf("收到的长度[%d]小于预期收到的长度[%d],值[%s]\n", n,len,Test_Asc2Hex(t, len));
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

static int Test_ReadALine( FILE *f, char *s, int MaxLen ){
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


static int Test_IsFileExist(const char * sFileName ){
	int ret;
	extern int errno;
	if( sFileName == NULL || *sFileName == 0 )
		return -1;
	ret = access( sFileName, F_OK );
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
static char *Test_TrimAll( char *buf )
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
static char *Test_split(char *s, char stop){
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
static int Test_readIni(char *fileName){

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
	ret = Test_IsFileExist(fileName);
	if(ret != 1){
		char pbuff[256];
		bzero(pbuff,sizeof(pbuff));
		sprintf( pbuff, "file[%s] not exist in readIni  error!! ", fileName );
		Test_Log(pbuff);
		exit(1);
		return -2;
	}
	f = fopen( fileName, "rt" );
	if( !f  ){	
		char pbuff[256];
		bzero(pbuff,sizeof(pbuff));
		sprintf( pbuff, "file[%s] open error in readIni!! ", fileName );
		Test_Log(pbuff);
		exit(1);
		return -3;
	}	
	bzero(my_data, sizeof(my_data));
	for(i=0;i<sizeof(my_data)-1;i++){
		c = fgetc( f);
		if( c == EOF)
			break;
		my_data[i] = c;
	}
	my_data[i] = 0;	
	fclose( f );
	
	
	bzero(&(fcc_ini[0].name), sizeof(TEST_INI_NAME_VALUE)*TEST_INI_MAX_CNT);	
	i = 0;
	while (my_data[0] !='\0'){
		tmp = Test_split(my_data, '=');// split data to get field name.
		bzero(&(fcc_ini[i].name), sizeof(fcc_ini[i].name));
		strcpy(fcc_ini[i].name, Test_TrimAll(tmp) );
		tmp = Test_split(my_data, '\n');// split data to get value.
		bzero(&(fcc_ini[i].value), sizeof(fcc_ini[i].value));
		strcpy(fcc_ini[i].value, Test_TrimAll(tmp));
		i ++;
	}
	//free(my_data);
	return i--;	
}

static int Test_writeIni(const char *fileName){
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
		Test_Log("file open error in writeIni!!");
		exit(1);
		return -1;
	}
	for(i=0;i<TEST_INI_MAX_CNT;i++){
		if( (fcc_ini[i].name[0] !=0)&&(fcc_ini[i].value[0] !=0) ){
			bzero(temp,sizeof(temp));
			sprintf(temp,"%s\t = %s  \n", fcc_ini[i].name,  fcc_ini[i].value);
			write( fd, temp, strlen(temp) );
		}
	}	
	close(fd);
	return 0;
}
static int Test_addIni(const char *name, const char *value){
	int i,n;
	
	if(  (NULL== name)||(NULL== value)  ){
		return -1;
	}	
	n = strlen(name);
	for(i=0;i<TEST_INI_MAX_CNT;i++){
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
static int Test_changeIni(const char *name, const char *value){
	int i,n;
	int ret;
	
	if(  (NULL== name)||(NULL== value)  ){
		return -1;
	}	
	n = strlen(name);
	for(i=0;i<TEST_INI_MAX_CNT;i++){
		if( memcmp(fcc_ini[i].name,name,n)  == 0 ){
			bzero( fcc_ini[i].value, sizeof(fcc_ini[i].value)  );
			memcpy(fcc_ini[i].value, value, strlen(value) );
			break;
		}
	}
	if(i == TEST_INI_MAX_CNT){
		ret = Test_addIni(name, value);
		if(ret < 0){
			return -2;
		}
	}		
	else
		return 0;
}


char *Test_upper(char *str){
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
int Test_InitConfig(){
	int ret,i,iniFlag;
	char configFile[256];
	
	bzero(configFile,sizeof(configFile));
	memcpy(configFile,TEST_CONFIG_FILE,sizeof(TEST_CONFIG_FILE));
	ret = Test_readIni(configFile);
	if(ret < 0){
		Test_Log("readIni error in _InitServerIP");
		return -1;
	}
	iniFlag = 0;
	//static int PC_SERVER_TCP_PORT,PC_SERVER_UDP_PORT,TEST_TCP_PORT;
	for(i=0;i<ret;i++){
		if( memcmp(fcc_ini[i].name,"PC_SERVER_IP",11) == 0){
			memcpy(PC_SERVER_IP, fcc_ini[i].value,sizeof(PC_SERVER_IP ) );//sizeof(fcc_ini[i].value
			Test_Log("PC_SERVER_IP[%s]",PC_SERVER_IP);
			iniFlag += 1;
		}else if( memcmp(fcc_ini[i].name,"PC_SERVER_TCP_PORT",18) == 0 ){
			PC_SERVER_TCP_PORT=atoi(fcc_ini[i].value);
			Test_Log("PC_SERVER_TCP_PORT[%d]",PC_SERVER_TCP_PORT);
			iniFlag += 1;
		}else if( memcmp(fcc_ini[i].name,"PC_SERVER_UDP_PORT",18) == 0 ){
			PC_SERVER_UDP_PORT=atoi(fcc_ini[i].value);
			Test_Log("PC_SERVER_UDP_PORT[%d]",PC_SERVER_UDP_PORT);
			iniFlag += 1;
			//memcpy(gFcc._SERVER_UDP_PORT, fcc_ini[2].value, sizeof(fcc_ini[2].value) );
		}else if( memcmp(fcc_ini[i].name,"FCC_TCP_PORT",12) == 0 ){
			TEST_TCP_PORT=atoi(fcc_ini[i].value);
			Test_Log("TEST_TCP_PORT[%d]",TEST_TCP_PORT);
			iniFlag += 1;
			//memcpy(gFcc._SERVER_UDP_PORT, fcc_ini[2].value, sizeof(fcc_ini[2].value) );
		}else{
			//Test_Log("配置数据出错,  不是预期的三个配置,名字是[%s],值是[%s]",fcc_ini[i].name,fcc_ini[i].value);
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


static int Test_OpenSerial( char *ttynam ){
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

static int Test_ReadPortNum(int portnum) 
{	
	unsigned char IO_cmd[2] = {0x1c,0x50};
	int fd;
	char ttynam[30];
	int	ret;
	char msg[16]; //,tmp[2],msg2[512];
	int timeout = 1;
	//int len;
	
	strcpy( ttynam, TEST_alltty[portnum] );//-1


	fd = Test_OpenSerial( ttynam);
	if( fd < 0 ) {
		//Test_Log( "打开串口失败" );
		return -1;
	}
	ret = WriteByLengthInTime( fd, IO_cmd, 2,timeout );
	if( ret < 0 ) {
		Test_Log( "Write Failed" );
		return -1;
	}

	memset( msg, 0, sizeof(msg) );
	ret = ReadByLengthInTime( fd, msg, sizeof(msg)-1, timeout );
	if( ret < 0 ) {
		Test_Log( "Read Failed" );
		return -1;
	}
	if( ret == 0 ) {
		Test_Log( "Read data error" );
		return -1;
	}
	Test_Log( "Port[%d]: [%d] channels" , portnum, msg[0]);
	//msg[ret] = 0;	
	return msg[0];	
}

int Test_GetPortChannel(char *portChnl){
	int i, ret;	
	int startPort;
	int portUsedCnt;

	bzero(portChnl,MAX_PORT+1);
	//memset(&portChnl[START_PORT], 8, MAX_PORT-START_PORT+1);
	startPort =START_PORT;//atoi(START_PORT);

	if( (startPort <=0 ) || (startPort >MAX_PORT) ){
		Test_Log("value of SERIAL_PORT_START[%d] is error",startPort );
		return -1;
	}
	portUsedCnt =PORT_USED_COUNT;//atoi(PORT_USED_COUNT);
	if( (portUsedCnt <=0 ) || (portUsedCnt >MAX_PORT-START_PORT+1) ){
		Test_Log("value of SERIAL_PORT_USED_COUNT[%d] is error",portUsedCnt );
		return -1;
	}
	for(i=0; i<portUsedCnt; i++){
		ret = Test_ReadPortNum(startPort+i);
		if(16 == ret){
			Test_Log("read port[%d] channel number is [%d]",startPort + i, ret );
			portChnl[startPort+i] = 16;
		} else if (8 == ret){
			Test_Log("read port[%d] channel number is is [%d]",startPort + i, ret );
			portChnl[startPort+i] = 8;
		}else{
			Test_Log("read port[%d] channel number is error, return number is [%d]",startPort + i, ret );
			portChnl[startPort+i] = 0;
		}
	}
	return 0;
}
int Test_GetSerialPort(const int chnLog, const char *portChnl){
	int i, ret;	
	int startPort;
	int tmp;
	int portUsedCnt;

	startPort =START_PORT;//atoi(START_PORT);
	if( (startPort <=0 ) || (startPort >MAX_PORT) ){
		Test_Log("value of SERIAL_PORT_START[%d] is error",startPort );
		return -1;
	}
	portUsedCnt =PORT_USED_COUNT;//atoi(PORT_USED_COUNT);
	if( (portUsedCnt <=0 ) || (portUsedCnt >MAX_PORT) ){
		Test_Log("value of SERIAL_PORT_USED_COUNT[%d] is error",portUsedCnt );
		return -1;
	}
	tmp = chnLog;
	for(i = 0; i < portUsedCnt; i++){
		tmp = tmp - portChnl[ startPort +i] ;//tmp = tmp - portChnl[i] - 1;
		if(tmp <= 0){
			return startPort + i;
		}		
	}
	Test_Log("GetSerialPort  error, chnLog[%d],portChnl[%s]",
			chnLog,Test_Asc2Hex((unsigned char *)portChnl, MAX_PORT+1));

	return -1;
}

/*
 * func: 将背板连接号(逻辑通道号)转换为物理通道号
 */
int Test_GetChnPhy(const int chnLog, const char *portChnl)
{
	int i;
	int startPort;
	int tmp;
	int portUsedCnt;

	startPort =START_PORT;//atoi(START_PORT);
	if( (startPort <=0 ) || (startPort >MAX_PORT) ){
		Test_Log("value of SERIAL_PORT_START[%d] is error",startPort );
		return -1;
	}

	portUsedCnt =PORT_USED_COUNT;//atoi(SERIAL_PORT_USED_COUNT);
	if( (portUsedCnt <=0 ) || (portUsedCnt >MAX_PORT) ){
		Test_Log("value of SERIAL_PORT_USED_COUNT[%d] is error",portUsedCnt );
		return -1;
	}

	tmp = chnLog;	
	for (i = 0; i < portUsedCnt; i++) {		
		if ((tmp - portChnl[startPort+i] - 1) < 0) {
			return tmp;
		} 
		tmp = tmp - portChnl[startPort+i] ;
	}

	Test_Log("Test_GetChnPhy  error, chnLog[%d],portChnl[%s]",chnLog,Test_Asc2Hex((unsigned char *)portChnl,MAX_PORT+1));

	return -1;
}
int Test_InitPortChn(){
	int ret;
	ret= Test_GetPortChannel(portChanel);
	if(ret < 0 ) {
		Test_Log( "Test_GetPortChannel 失败.请检查" );
		return -1;
	}
	Test_Log("Test_InitPortChn获取到portChnl[%s]",Test_Asc2Hex((unsigned char *)portChanel, MAX_PORT+1));
	return 0;
}

//读写串口
int Test_WriteTty(int fd, unsigned char *msg, int msgLen){
	int ret;
	int timeout = 5;
	//unsigned char buff[64],*result;
	
	//fd = OpenSerial( TEST_alltty[serial_num]);
	if( fd <= 0 )
	{
		Test_Log( "串口fd[%d]负值错误",fd );
		return -1;
	}
	//Test_Log( "after Test_Writetty and 0 to moved test!" );
	
	//ret = WriteByLengthInTime( fd, Hex2Asc(nv[1].value, strlen(nv[1].value)),strlen(nv[1].value)/2, timeout );
	ret = WriteByLengthInTime( fd, msg,msgLen, timeout );
	if( ret <= 0 )
	{
		Test_Log( "Write Failed" );
		return -1;
	}
	//close(fd);
	//result=Test_Asc2Hex(buff,ret);
	//len = strlen(result);
	return ret;
}

//通道读取数据处理,此处没有使用消息队列,改用临时文件.
int Test_PortReadInit(){
	int ret;
	ret = system("mkdir /tmp/testfcc");
	ret = system("touch /tmp/testfcc/flag.txt");
	ret = Test_IsFileExist("/tmp/testfcc/flag.txt");
	if(ret <= 0){
		Test_Log( "Cann't creat temp file. " );
		return -9;
	}else{
		return 1;
	}
	
}
int Test_PortReadEnd(){
	int ret;
	ret = system("rm -f /tmp/testfcc/*");
	ret = system("rm -f /tmp/testfcc/*.*");	
	ret = Test_IsFileExist("/tmp/testfcc/flag.txt");
	if(ret == 0){
		ret = system("rm -f /tmp/testfcc");
		Test_Log( "Temp files have been deleted." );
		return 1;
	}else if(ret < 0){
		Test_Log( "Something error." );
		return -9;
	}else {
		Test_Log( "Cann't delete temp file. " );
		return -8;
	}
}
int Test_PortRead4Exit(){	
	return Test_PortReadEnd();
}

int Test_ReadTty(int fd, unsigned char *msg, int *recvLen){
	int ret,len;
	int timeout = 5;
	unsigned char buff[32],*result;
	
	//fd = OpenSerial( TEST_alltty[serial_num]);
	if( fd <= 0 )
	{
		Test_Log( "串口fd[%d]负值错误",fd );
		return -1;
	}
	//Test_Log( "after Test_Writetty and 0 to moved test!" );
	len = *recvLen;
	if( sizeof(buff) < len)
	{
		Test_Log( "FccReadTty错误,buff长度[%d]小于待接收长度[%d]",sizeof(buff), len);
		return -1;
	}
	
	memset( buff, 0, sizeof(buff) );
	ret = ReadByLengthInTime( fd, buff, sizeof(buff)-1, timeout);
	if( ret <= 0 )
	{
		Test_Log( "Read Failed" );
		return -1;
	}
	
	//result=Test_Asc2Hex(buff,ret);
	len = ret;
	memcpy(msg,buff,len);
	return len;
}
int Test_PortRead(int fd, int serialPortNum, int phyChnl, unsigned char *msg, int *recvLen){
	int i,j,k,ret,len,y;//,cflag
	int tryNum = 1;
	unsigned char buff[64],sTemp[32];
	for(i=0;i<tryNum;i++){
		bzero(buff,sizeof(buff));
		len = *recvLen;
		switch(serialPortNum){
			case 3:Test_Act_P(TEST_SEM_COM3);
				break;
			case 4:Test_Act_P(TEST_SEM_COM4);
				break;
			case 5:Test_Act_P(TEST_SEM_COM5);
				break;
			case 6:Test_Act_P(TEST_SEM_COM6);
				break;
			default:Test_Log( "串口号[%d]错误",serialPortNum );
				break;			
		}
		ret = Test_ReadTty(fd, buff, &len);
		switch(serialPortNum){
			case 3:Test_Act_V(TEST_SEM_COM3);
				break;
			case 4:Test_Act_V(TEST_SEM_COM4);
				break;
			case 5:Test_Act_V(TEST_SEM_COM5);
				break;
			case 6:Test_Act_V(TEST_SEM_COM6);
				break;
			default:Test_Log( "Test_PortRead串口号[%d]错误",serialPortNum );
				break;			
		}		
		if(ret <=0){
			Test_Log( "Test_PortRead端口[%02x]读[串口%d]数据失败" ,phyChnl,serialPortNum);
			sleep(1);
			continue;
		}
		Test_Log("Test_PortRead[串口%d][通道%02x]接收数据[%s]",serialPortNum,phyChnl,Test_Asc2Hex(buff, len));	
		for(y=0;y<RECLEN;y++){
		       if(0x30+phyChnl-1 == buff[y]){
			Test_Log("Test_PortRead[串口%d][通道%02x]接收数据成功",serialPortNum,phyChnl);
			return 1;//读取成功//break;
		             }
			}
			bzero(sTemp,sizeof(sTemp));
			if( (buff[0]&0xf0) == 0x30){
				sprintf(sTemp,"/tmp/testfcc/%02d%02x",serialPortNum,buff[0]);
			}else if( (buff[1]&0xf0) == 0x30){
				sprintf(sTemp,"/tmp/testfcc/%02d%02x",serialPortNum,buff[1]);
			}else{
				continue;//return -8;
			}

			j = 0;
			for(k=0;k<len;k++){
				if( (buff[k]&0xf0) != 0x30){
					msg[j] = buff[k];
					j++;
				}		
			}
			msg[j] = 0;
			fd = open( sTemp, O_WRONLY|O_CREAT|O_TRUNC, 00644 );
			if( fd < 0 ){
				
				Test_Log("file open error in Test_PortRead!!");
				return -7;
			}
			write( fd, msg, strlen(msg) );			
			Test_Log("写了数据文件[%s]",sTemp);
			close(fd);
			sleep(1);
			continue;
	}
	
	/*if( (0x30+phyChnl-1 == msg[0])||(0x30+phyChnl-1 == msg[1])){
		return 1;//读取成功
	}*/	
	bzero(sTemp,sizeof(sTemp));
	sprintf(sTemp,"/tmp/testfcc/%02d%02x",serialPortNum,0x30+phyChnl-1);
	ret = Test_IsFileExist(sTemp);
	if(ret <= 0){
		Test_Log( "没有该通道[串口%d][通道%02x]数据." ,serialPortNum,phyChnl);
		return -6;
	}else{
		unlink(sTemp);//删除该文件
		Test_Log("删除了数据文件[%s]",sTemp);
		return 1;//保存有该通道数据
	}
}

//--------------------------------
//向PC注册
/*FCC向PC 注册:
*FCC发:1104+本机IP地址+4个串口通道数,PC回复:110111或者 110122,其中110111为1号,110122为2号FCC.*/
//static unsigned char FccNum;
//static char PC_SERVER_IP[16]; //PC服务器IP地址
//static int PC_SERVER_TCP_PORT,PC_SERVER_UDP_PORT,TEST_TCP_PORT;
int Test_Login(){
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
	localIP = Test_GetLocalIP();
	
	//FCC的 IP地址
	memcpy(&sendBuffer[2],(unsigned char *)&localIP,4);

	//串口通道数目
	memcpy(&sendBuffer[62],&portChanel[START_PORT],4);//(unsigned char *)&localIP
	//i=k;
	//向PC上传数据
	Test_Log("向PC上传数据[%s]",Test_Asc2Hex(sendBuffer, msgLength) );
	i = 0;
	sIPName =PC_SERVER_IP;//inet_ntoa(  *((struct in_addr *)(&gpIfsf_shm->recv_hb[i].host_ip))  );
	while(1){
		i++;
		if( i > LOGIN_TIME ) exit(-1);
		sockfd = Test_TcpConnect(sIPName,PC_SERVER_TCP_PORT , timeout);//(timeout+1)/2   //sPortName
		if (sockfd >0 ){
			//sleep(1);
			ret =Test_TcpSendDataInTime(sockfd, sendBuffer, msgLength, timeout);			//Act_V(CD_SOCK_INDEX);			
			if (ret < 0 ){
				Test_Log("TcpSendDataInTime发送数据不成功,Test_Login发送数据 失败,IP[%s],port[%d]",sIPName,PC_SERVER_TCP_PORT);
				close(sockfd);
				sleep(1);//sleep(10000);
				continue;
			}else{				
				break;
			}
		} else {
			Test_Log("Tcp Connect 不成功,Test_Login发送数据 失败,IP[%s],port[%d]",sIPName,PC_SERVER_TCP_PORT);
			close(sockfd);
			sleep(1);//sleep(10000); 			
			continue;
			//return -1;
		}
	}
	 //开始从PC接收返回数据
	Test_Log("开始从PC接收返回数据");
	bzero(sendBuffer,sizeof(sendBuffer));
	for(i=0;i<tryn;i++){
		msgLength = 3;		
		ret =Test_TcpRecvDataInTime(sockfd, sendBuffer, &msgLength, timeout);			//Act_V(CD_SOCK_INDEX);			
		if (ret < 0 ){
			Test_Log("TcpRecvDataInTime接收数据不成功,Test_Login接收数据 失败");
			sleep(10);
			continue;
		}
		break;
	}
	
      if( (sendBuffer[0] != 0x11)||(sendBuffer[1] != 0x01) ){
	  	Test_Log("Tcp Connect 不成功,Test_Login接收数据 失败,[%02x%02x%02x]",sendBuffer[0],sendBuffer[1],sendBuffer[2]);
		close(sockfd);
		return -1;
	}
	//CSS_LevelLog("gpCSS_Shm->css._TLG_SEND_INTERVAL赋值[%02x%02x]",sendBuffer[1],sendBuffer[2]);
	Gilbarco = 0;
      	if(sendBuffer[2] == 0x11) {
			FccNum = 1;
			Test_Log("FccNum赋值[%d],主FCC",FccNum);
	}else if(sendBuffer[2] == 0x22){ 
		FccNum = 2;
		Test_Log("FccNum赋值[%d],从FCC",FccNum);
	}else if(sendBuffer[2] == 0x33){ 
		FccNum = 1;
		Gilbarco = 1;
		Test_Log("长吉专用,FccNum赋值[%d],主FCC",FccNum);
	}else if(sendBuffer[2] == 0x44){ 
		FccNum = 2;
		Gilbarco = 1;
		Test_Log("长吉专用,FccNum赋值[%d],从FCC",FccNum);
	}else{
		Test_Log("Test_Login接收数据错误,Test_Login接收数据 失败");
		return -1;
	}
	close(sockfd);
	//Test_Log("FccNum赋值[%d]",FccNum);
	return 0;
}

int Test_TCPSendRecv(unsigned char *sendBuffer, int sendLength, unsigned char *recvBuffer, int *recvLength){
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
		sockfd = Test_TcpConnect(sIPName,PC_SERVER_TCP_PORT , timeout);//(timeout+1)/2   //sPortName
		if (sockfd >0 ){
			sleep(5);
			ret =Test_TcpSendDataInTime(sockfd, sendBuffer, msgLength, timeout);			//Act_V(CD_SOCK_INDEX);			
			if (ret < 0 ){
				Test_Log("TcpSendDataInTime发送数据不成功,IP[%s],port[%d]",sIPName,PC_SERVER_TCP_PORT);
				close(sockfd);
				sleep(10);//sleep(10000);
				continue;
			}else{				
				break;
			}
		} else {
			Test_Log("Tcp Connect 不成功,IP[%s],port[%d]",sIPName,PC_SERVER_TCP_PORT);
			close(sockfd);
			sleep(60);//sleep(10000); 			
			continue;
			//return -1;
		}
	}
	 //从PC接收返回数据
	Test_Log("从PC接收返回数据");
	 //sleep(5);//等待对方执行完毕
	//bzero(recvBuffer,sizeof(recvBuffer));
	for(i=0;i<tryn;i++){
		msgLength = *recvLength;		
		ret =Test_TcpRecvDataInTime(sockfd, recvBuffer, &msgLength, timeout);			//Act_V(CD_SOCK_INDEX);			
		if (ret < 0 ){
			Test_Log("TcpRecvDataInTime接收数据不成功");
			sleep(10);
			continue;
		}
		break;
	}
      if( (recvBuffer[0] == 0)&&(recvBuffer[1] == 0) ){
	  	Test_Log("Tcp Connect 不成功,[%02x%02x%02x]",recvBuffer[0],recvBuffer[1],recvBuffer[2]);
		close(sockfd);
		return -1;
	}	
	close(sockfd);
	memcpy((char *)recvLength,(char *)&msgLength,sizeof(int));
	Test_Log("Test_SendRecv接收数据[%s]",Test_Asc2Hex(recvBuffer, msgLength));	
	return 0;
}


//--------------------------------------------------
//通讯服务程序
//-----------------------------------------------
//static char PC_SERVER_IP[16]; //PC服务器IP地址
//static int PC_SERVER_TCP_PORT,PC_SERVER_UDP_PORT,TEST_TCP_PORT;
int  Test_WaitStartCmd( ){
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
		
	
	
	
	sock = Test_TcpServInit(TEST_TCP_PORT);
	if ( sock < 0 ) {
		Test_Log( "实时进程初始化socket失败.port=[%s]\n", TEST_TCP_PORT );
		//Free( cmd );
		return  ;
	}
	//fcntl(sock, F_SETFL, O_NONBLOCK);   // 非阻塞模式
	
	newsock  = -1;
	tmp_sock = -1;
	while (1) {	
		if (errcnt > 32) {
			Test_Log("out of ERR_CNT, close the tcp connection (PC.C --> FCC.S)");
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
		tmp_sock = Test_TcpAccept(sock);//, NULL
		Test_Log("Test_WaitStartCmd连接,soket号[%d]", tmp_sock);
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

		
		//msg_lg = sizeof(buffer);
		memset( buffer, 0, sizeof(buffer) );
		startFlag = 0;
		sleep(10);
		while(!startFlag){
			if ( errcnt > tryNum ) {
				/*错误处理*/
				Test_Log( "接收数据[%d]次都失败", tryNum );
				break; 
			}
			msg_lg = 3;
			sleep(10);
			ret = Test_TcpRecvDataInTime(newsock, buffer, &msg_lg, timeout );//TIME_INFINITE	
			if (ret < 0) {
				if (errno == 4) {  // time out
					Test_Log("进程超时,接收到后台命令 buffer[%s]\n" , Test_Asc2Hex(buffer, msg_lg) );//
					continue;
				} else if (errno == 11)  {  // maybe CD quit or restart, && (gpIfsf_shm->need_reconnect == 0))
					//gpIfsf_shm->need_reconnect = 1;
					Test_Log("Client need to reconnect to Server");
				}	
				Test_Log( "进程接收数据失败,errno:[%d][%s],buffer: %s", errno, strerror(errno), \
					Test_Asc2Hex(buffer, 2) );
				errcnt++;
				sleep(1);
				continue;
			} else {
				Test_Log( "<<<进程收到命令: [%s(%d)]", Test_Asc2Hex(buffer, msg_lg ), msg_lg);				
				if ( 0x22 != buffer[0] ){ //received data must be error!!(0 == (buffer[0]>>4) )||(0 ==( buffer[0]&0x0f) )
					Test_Log( "received data error" );
					errcnt++;
					sleep(1);
					continue;
				}

				if (0 == msg_lg){ //receivd data must be error!!
					Test_Log( "received size is  0 error" );
					errcnt++;
					//回复错误
					sBuffer[0] = 0x2f;
					sBuffer[1] = 0;
					//memcpy(&sBuffer[1],gpCSS_Shm->css._STATION_NO_BCD,5);
					ret = Test_TcpSendDataInTime(newsock, sBuffer, 2, timeout);
					if(ret <=0){				
						Test_Log("实时进程回复数据 失败");
					}
					
					sleep(1);
					continue;
				}

				
				
				//处理后台命令.
				if (1 == buffer[1]){ 
					if(0x11 == buffer[2]){
						if(1 != FccNum) {
							Test_Log("PC启动命令 错误,本机主FCC");
							sBuffer[0] = 0x2f;
							sBuffer[1] = 0;
							//memcpy(&sBuffer[1],gpCSS_Shm->css._STATION_NO_BCD,5);
							ret = Test_TcpSendDataInTime(newsock, sBuffer, 2, timeout);
							if(ret <0){				
								Test_Log("进程回复数据 失败");
								//continue;
							}
							close(newsock);close(sock);
							return -1;
						}
						sBuffer[0] = buffer[0];
						sBuffer[1] = 0;
						//memcpy(&sBuffer[1],gpCSS_Shm->css._STATION_NO_BCD,5);
						ret = Test_TcpSendDataInTime(newsock, sBuffer, 2, timeout);
						if(ret <0){				
							Test_Log("进程回复数据 失败");
							//continue;
						}
						errcnt = 0;
						close(newsock);close(sock);
						return 1;//break;
					}else if(0x22 == buffer[2]){
						if(2 != FccNum) {
							Test_Log("PC启动命令 错误,本机从FCC");
							sBuffer[0] = 0x2f;
							sBuffer[1] = 0;
							//memcpy(&sBuffer[1],gpCSS_Shm->css._STATION_NO_BCD,5);
							ret = Test_TcpSendDataInTime(newsock, sBuffer, 2, timeout);
							if(ret <0){				
								Test_Log("进程回复数据 失败");
								//continue;
							}
							close(newsock);close(sock);
							return -1;
						}
						sBuffer[0] = buffer[0];
						sBuffer[1] = 0;
						//memcpy(&sBuffer[1],gpCSS_Shm->css._STATION_NO_BCD,5);
						ret = Test_TcpSendDataInTime(newsock, sBuffer, 6, timeout);
						if(ret <0){				
							Test_Log("进程回复数据 失败");
							//continue;
						}
						errcnt = 0;
						close(newsock);close(sock);
						return 1;//break;
					}else{
						Test_Log( "解析数据失败,收到的数据值不是0x11和0x22" );
					}
				}  else{
					Test_Log( "解析数据失败,收到的数据的长度字节不是1" );
				}				
				break;
				//sleep(10);			
			}
		}
		if ( errcnt > 40 ) {
			/*错误处理*/
			Test_Log( "初始化socket 失败.port=[%d]", TEST_TCP_PORT );
			break; 
		}
	}

	Test_Log("Test_AcceptPCCmd  return ......");
	close(sock);

	return  -1;
}


//通道收发函数
int Test_PortSendRecv(int chnlog){
	//#define  LOGIN_TIME 1000
	int i,j, ret, tmp,fd,n,k;//, sockfd,
	//int sockflag =0;
	//unsigned char *sIPName;	//sPortName[5],
	unsigned char buff[64] ,*pBuff;	//,buff[64]sendBuffer[16] ,
	//unsigned int msgLength  ;
	int timeout = 5, tryn = 2;
	//unsigned long localIP;	
	int serialPortNum, phyChnl;
	char		cmd[11];
	int			cnt;
	
	serialPortNum = -1;
	phyChnl = -1;
	serialPortNum = Test_GetSerialPort(chnlog,portChanel);	
	if(serialPortNum < 0){
		Test_Log( "serialPortNum < 0错误" );
		return -1;
	}
	phyChnl = Test_GetChnPhy(chnlog,portChanel);
	if(phyChnl <=0){
		Test_Log( "phyChnl <=0错误" );
		return -1;
	}
	fd = Test_OpenSerial(TEST_alltty[serialPortNum]);
	if(fd <=0){
		Test_Log( "Test_PortSendRecv打开串口错误" );
		return -1;
	}
	Test_Log( "Test_PortSendRecv开始测试通道[%d]" ,chnlog);
	//Test_Log("向PC发送数据");
	for(i=0;i<tryn;i++){
		memset(cmd,0x5a,sizeof(cmd));
		cmd[0] = 0x1B;
		cmd[1] = 0x30+phyChnl-1;
		
		if( sizeof(cmd) > MAX_PORT_CMD_LEN ) {
			Test_Log( "向端口写的数据长度[%d]超过最大长度[%d]", sizeof(cmd), MAX_PORT_CMD_LEN );
			close(fd);
			return -1;
		}		 
		cmd[2] = 0x00+ (sizeof(cmd) -3 ) ;
		//memcpy( cmd+3, buff, len );
		cnt =sizeof(cmd);
		sleep(3);
		Test_Log("Test_SendRecv发送数据[%s]",Test_Asc2Hex(cmd, cnt));
		switch(serialPortNum){
			case 3:Test_Act_P(TEST_SEM_COM3);
				break;
			case 4:Test_Act_P(TEST_SEM_COM4);
				break;
			case 5:Test_Act_P(TEST_SEM_COM5);
				break;
			case 6:Test_Act_P(TEST_SEM_COM6);
				break;
			default:Test_Log( "串口号[%d]错误",serialPortNum );
				break;			
		}
		ret = Test_WriteTty(fd, cmd, cnt);
		switch(serialPortNum){
			case 3:Test_Act_V(TEST_SEM_COM3);
				break;
			case 4:Test_Act_V(TEST_SEM_COM4);
				break;
			case 5:Test_Act_V(TEST_SEM_COM5);
				break;
			case 6:Test_Act_V(TEST_SEM_COM6);
				break;
			default:Test_Log( "串口号[%d]错误",serialPortNum );
				break;			
		}
		sleep(1);
		switch(serialPortNum){
			case 3:Test_Act_P(TEST_SEM_COM3);
				break;
			case 4:Test_Act_P(TEST_SEM_COM4);
				break;
			case 5:Test_Act_P(TEST_SEM_COM5);
				break;
			case 6:Test_Act_P(TEST_SEM_COM6);
				break;
			default:Test_Log( "串口号[%d]错误",serialPortNum );
				break;			
		}
		ret = Test_WriteTty(fd, cmd, cnt);
		switch(serialPortNum){
			case 3:Test_Act_V(TEST_SEM_COM3);
				break;
			case 4:Test_Act_V(TEST_SEM_COM4);
				break;
			case 5:Test_Act_V(TEST_SEM_COM5);
				break;
			case 6:Test_Act_V(TEST_SEM_COM6);
				break;
			default:Test_Log( "串口号[%d]错误",serialPortNum );
				break;			
		}
		if(ret <=0){
			Test_Log( "向端口写数据失败" );
			sleep(5);
			continue;
		}
		//break;
		sleep(3);//等待对方执行完毕
		bzero(buff,sizeof(buff));
		cnt = RECLEN;//58
		ret = Test_PortRead(fd, serialPortNum, phyChnl, buff, &cnt);
		if(ret == 1){
			/*//成功再读一次,确保发送的都读取到
			//sleep(1);//等待对方执行完毕
			bzero(buff,sizeof(buff));
			cnt = RECLEN;//58
			ret = Test_PortRead(fd, serialPortNum, phyChnl, buff, &cnt);
			ret = 1;//恢复成功状� */
			break;
		}else{
			Test_Log( "Test_PortRead读[串口%d][通道%02x]数据失败" ,serialPortNum, phyChnl );
			//不成功再试一次
			//sleep(1);//等待对方执行完毕
			bzero(buff,sizeof(buff));
			cnt = RECLEN;//58
			ret = Test_PortRead(fd, serialPortNum, phyChnl, buff, &cnt);
			if(ret == 1){
				break;
			}else{
				continue;
			}	
		}
		/*switch(serialPortNum){
			case 3:Test_Act_P(TEST_SEM_COM3);
				break;
			case 4:Test_Act_P(TEST_SEM_COM4);
				break;
			case 5:Test_Act_P(TEST_SEM_COM5);
				break;
			case 6:Test_Act_P(TEST_SEM_COM6);
				break;
			default:Test_Log( "串口号[%d]错误",serialPortNum );
				break;			
		}
		ret = Test_ReadTty(fd, buff, &cnt);
		switch(serialPortNum){
			case 3:Test_Act_V(TEST_SEM_COM3);
				break;
			case 4:Test_Act_V(TEST_SEM_COM4);
				break;
			case 5:Test_Act_V(TEST_SEM_COM5);
				break;
			case 6:Test_Act_V(TEST_SEM_COM6);
				break;
			default:Test_Log( "串口号[%d]错误",serialPortNum );
				break;			
		}		
		if(ret <=0){
			Test_Log( "端口读数据失败" );
			sleep(1);
			continue;
		}
		Test_Log("Test_SendRecv接收数据[%s]",Test_Asc2Hex(buff, cnt));	
		if(  (0x30+phyChnl-1 == buff[0])||(0x30+phyChnl-1 == buff[1])){
			break;
		}else{
			sleep(1);
			continue;
		}*/
	}
	if( (ret <=0) ||(i >=tryn) ){
		Test_Log( "从端口[%d]读数据[%d]次全部失败" ,chnlog,tryn);
		close(fd);
		return -1;
	}	
	close(fd);
	return 0;
}

int Test_PortRecvSend(int chnlog){
	//#define  LOGIN_TIME 1000
	int i, ret, tmp,fd,n,k;//,sockfd ,
	//int sockflag =0;
	//unsigned char *sIPName;	//sPortName[5],
	unsigned char buff[64] ;	//,buff[64]sendBuffer[16] ,,*pBuff
	//unsigned int msgLength  ;
	int timeout = 5, tryn = 2;
	//unsigned long localIP;	
	int serialPortNum, phyChnl;
	char		cmd[11];
	int			cnt;
	
	serialPortNum = -1;
	phyChnl = -1;
	serialPortNum = Test_GetSerialPort(chnlog,portChanel);	
	if(serialPortNum < 0){
		Test_Log( "serialPortNum < 0错误" );
		return -1;
	}
	phyChnl = Test_GetChnPhy(chnlog,portChanel);	
	if(phyChnl <=0){
		Test_Log( "phyChnl <=0错误" );
		return -1;
	}
	fd = Test_OpenSerial(TEST_alltty[serialPortNum]);
	if(fd <=0){
		Test_Log( "Test_PortRecvSend打开串口错误" );
		return -1;
	}
	Test_Log( "Test_PortRecvSend开始测试通道[%d]" ,chnlog);
	 //从PC接收返回数据
	//Test_Log("从PC接收返回数据");
	for(i=0;i<tryn;i++){		
		sleep(1);//等待对方执行完毕
		bzero(buff,sizeof(buff));
		cnt =RECLEN;//58
		ret = Test_PortRead(fd, serialPortNum, phyChnl, buff, &cnt);
		if(ret <0){
			Test_Log( "Test_PortRead读数据失败" );
			//sleep(1);
			continue;	
		}
		memset(cmd,0x5a,sizeof(cmd));
		cmd[0] = 0x1B;
		cmd[1] = 0x30+phyChnl-1;
		
		if( sizeof(cmd) > MAX_PORT_CMD_LEN ) {
			Test_Log( "向端口写的数据长度[%d]超过最大长度[%d]", sizeof(cmd), MAX_PORT_CMD_LEN );
			close(fd);
			return -1;
		}		 
		cmd[2] = 0x00+ (sizeof(cmd) -3 ) ;
		//memcpy( cmd+3, buff, len );
		cnt =sizeof(cmd);
		sleep(1);
		Test_Log("Test_SendRecv发送数据[%s]",Test_Asc2Hex(cmd, cnt));
		switch(serialPortNum){
			case 3:Test_Act_P(TEST_SEM_COM3);
				break;
			case 4:Test_Act_P(TEST_SEM_COM4);
				break;
			case 5:Test_Act_P(TEST_SEM_COM5);
				break;
			case 6:Test_Act_P(TEST_SEM_COM6);
				break;
			default:Test_Log( "串口号[%d]错误",serialPortNum );
				break;			
		}
		ret = Test_WriteTty(fd, cmd, cnt);
		switch(serialPortNum){
			case 3:Test_Act_V(TEST_SEM_COM3);
				break;
			case 4:Test_Act_V(TEST_SEM_COM4);
				break;
			case 5:Test_Act_V(TEST_SEM_COM5);
				break;
			case 6:Test_Act_V(TEST_SEM_COM6);
				break;
			default:Test_Log( "串口号[%d]错误",serialPortNum );
				break;			
		}
		//ret = Test_WriteTty(fd, cmd, cnt);
		if(ret <=0){
			Test_Log( "向端口写数据失败" );
			sleep(1);
			continue;
		}
		sleep(1);//再发送一次
		switch(serialPortNum){
			case 3:Test_Act_P(TEST_SEM_COM3);
				break;
			case 4:Test_Act_P(TEST_SEM_COM4);
				break;
			case 5:Test_Act_P(TEST_SEM_COM5);
				break;
			case 6:Test_Act_P(TEST_SEM_COM6);
				break;
			default:Test_Log( "串口号[%d]错误",serialPortNum );
				break;			
		}
		ret = Test_WriteTty(fd, cmd, cnt);
		switch(serialPortNum){
			case 3:Test_Act_V(TEST_SEM_COM3);
				break;
			case 4:Test_Act_V(TEST_SEM_COM4);
				break;
			case 5:Test_Act_V(TEST_SEM_COM5);
				break;
			case 6:Test_Act_V(TEST_SEM_COM6);
				break;
			default:Test_Log( "串口号[%d]错误",serialPortNum );
				break;			
		}
		//ret = Test_WriteTty(fd, cmd, cnt);
		if(ret <=0){
			Test_Log( "向端口写数据失败" );
			sleep(1);
			continue;
		}
		break;
	}
	if( (ret <=0) ||(i >=tryn) ){
		Test_Log( "从端口[%d]读数据[%d]次全部失败" ,chnlog, tryn);
		close(fd);
		return -1;
	}	
	close(fd);
	return 0;
}
int Test_PortSend(int chnlog){
	//#define  LOGIN_TIME 1000
	int i,j, ret, tmp,fd,n,k;//, sockfd,
	//int sockflag =0;
	//unsigned char *sIPName;	//sPortName[5],
	unsigned char buff[64] ,*pBuff;	//,buff[64]sendBuffer[16] ,
	//unsigned int msgLength  ;
	int timeout = 5, tryn = 3;
	//unsigned long localIP;	
	int serialPortNum, phyChnl;
	char		cmd[11];
	int			cnt;
	
	serialPortNum = -1;
	phyChnl = -1;
	serialPortNum = Test_GetSerialPort(chnlog,portChanel);	
	if(serialPortNum < 0){
		Test_Log( "serialPortNum < 0错误" );
		return -1;
	}
	phyChnl = Test_GetChnPhy(chnlog,portChanel);
	if(phyChnl <=0){
		Test_Log( "phyChnl <=0错误" );
		return -1;
	}
	fd = Test_OpenSerial(TEST_alltty[serialPortNum]);
	if(fd <=0){
		Test_Log( "Test_PortSendRecv打开串口错误" );
		return -1;
	}
	Test_Log( "Test_PortSendRecv开始测试通道[%d]" ,chnlog);
	//Test_Log("向PC发送数据");
	for(i=0;i<tryn;i++){
		memset(cmd,0x5a,sizeof(cmd));
		cmd[0] = 0x1B;
		cmd[1] = 0x30+phyChnl-1;
		
		if( sizeof(cmd) > MAX_PORT_CMD_LEN ) {
			Test_Log( "向端口写的数据长度[%d]超过最大长度[%d]", sizeof(cmd), MAX_PORT_CMD_LEN );
			close(fd);
			return -1;
		}		 
		cmd[2] = 0x00+ (sizeof(cmd) -3 ) ;
		//memcpy( cmd+3, buff, len );
		cnt =sizeof(cmd);
		sleep(3);
		Test_Log("Test_SendRecv发送数据[%s]",Test_Asc2Hex(cmd, cnt));
		switch(serialPortNum){
			case 3:Test_Act_P(TEST_SEM_COM3);
				break;
			case 4:Test_Act_P(TEST_SEM_COM4);
				break;
			case 5:Test_Act_P(TEST_SEM_COM5);
				break;
			case 6:Test_Act_P(TEST_SEM_COM6);
				break;
			default:Test_Log( "串口号[%d]错误",serialPortNum );
				break;			
		}
		ret = Test_WriteTty(fd, cmd, cnt);
		switch(serialPortNum){
			case 3:Test_Act_V(TEST_SEM_COM3);
				break;
			case 4:Test_Act_V(TEST_SEM_COM4);
				break;
			case 5:Test_Act_V(TEST_SEM_COM5);
				break;
			case 6:Test_Act_V(TEST_SEM_COM6);
				break;
			default:Test_Log( "串口号[%d]错误",serialPortNum );
				break;			
		}
		/*sleep(1);
		switch(serialPortNum){
			case 3:Test_Act_P(TEST_SEM_COM3);
				break;
			case 4:Test_Act_P(TEST_SEM_COM4);
				break;
			case 5:Test_Act_P(TEST_SEM_COM5);
				break;
			case 6:Test_Act_P(TEST_SEM_COM6);
				break;
			default:Test_Log( "串口号[%d]错误",serialPortNum );
				break;			
		}
		ret = Test_WriteTty(fd, cmd, cnt);
		switch(serialPortNum){
			case 3:Test_Act_V(TEST_SEM_COM3);
				break;
			case 4:Test_Act_V(TEST_SEM_COM4);
				break;
			case 5:Test_Act_V(TEST_SEM_COM5);
				break;
			case 6:Test_Act_V(TEST_SEM_COM6);
				break;
			default:Test_Log( "串口号[%d]错误",serialPortNum );
				break;			
		}*/
		if(ret <=0){
			Test_Log( "向端口写数据失败" );
			sleep(5);
			continue;
		}
		//break;
		sleep(10);//等待对方执行完毕
		/*
		bzero(buff,sizeof(buff));
		cnt = RECLEN;//58
		ret = Test_PortRead(fd, serialPortNum, phyChnl, buff, &cnt);
		if(ret == 1){
			///成功再读一次,确保发送的都读取到
			//sleep(1);//等待对方执行完毕
			//bzero(buff,sizeof(buff));
			//cnt = RECLEN;//58
			//ret = Test_PortRead(fd, serialPortNum, phyChnl, buff, &cnt);
			//ret = 1;//恢复成功状//
			break;
		}else{
			Test_Log( "Test_PortRead读[串口%d][通道%02x]数据失败" ,serialPortNum, phyChnl );
			//不成功再试一次
			//sleep(1);//等待对方执行完毕
			bzero(buff,sizeof(buff));
			cnt = RECLEN;//58
			ret = Test_PortRead(fd, serialPortNum, phyChnl, buff, &cnt);
			if(ret == 1){
				break;
			}else{
				continue;
			}	
		}*/
		/*switch(serialPortNum){
			case 3:Test_Act_P(TEST_SEM_COM3);
				break;
			case 4:Test_Act_P(TEST_SEM_COM4);
				break;
			case 5:Test_Act_P(TEST_SEM_COM5);
				break;
			case 6:Test_Act_P(TEST_SEM_COM6);
				break;
			default:Test_Log( "串口号[%d]错误",serialPortNum );
				break;			
		}
		ret = Test_ReadTty(fd, buff, &cnt);
		switch(serialPortNum){
			case 3:Test_Act_V(TEST_SEM_COM3);
				break;
			case 4:Test_Act_V(TEST_SEM_COM4);
				break;
			case 5:Test_Act_V(TEST_SEM_COM5);
				break;
			case 6:Test_Act_V(TEST_SEM_COM6);
				break;
			default:Test_Log( "串口号[%d]错误",serialPortNum );
				break;			
		}		
		if(ret <=0){
			Test_Log( "端口读数据失败" );
			sleep(1);
			continue;
		}
		Test_Log("Test_SendRecv接收数据[%s]",Test_Asc2Hex(buff, cnt));	
		if(  (0x30+phyChnl-1 == buff[0])||(0x30+phyChnl-1 == buff[1])){
			break;
		}else{
			sleep(1);
			continue;
		}*/
	}
	if( (ret <=0) ||(i >=tryn) ){
		Test_Log( "从端口[%d]读数据[%d]次全部失败" ,chnlog,tryn);
		close(fd);
		return -1;
	}	
	close(fd);
	return 0;
}

int Test_PortRecv(int chnlog){
	//#define  LOGIN_TIME 1000
	int i, ret, tmp,fd,n,k;//,sockfd ,
	//int sockflag =0;
	//unsigned char *sIPName;	//sPortName[5],
	unsigned char buff[64] ;	//,buff[64]sendBuffer[16] ,,*pBuff
	//unsigned int msgLength  ;
	int timeout = 5, tryn = 3;
	//unsigned long localIP;	
	int serialPortNum, phyChnl;
	char		cmd[11];
	int			cnt;
	
	serialPortNum = -1;
	phyChnl = -1;
	serialPortNum = Test_GetSerialPort(chnlog,portChanel);	
	if(serialPortNum < 0){
		Test_Log( "serialPortNum < 0错误" );
		return -1;
	}
	phyChnl = Test_GetChnPhy(chnlog,portChanel);	
	if(phyChnl <=0){
		Test_Log( "phyChnl <=0错误" );
		return -1;
	}
	fd = Test_OpenSerial(TEST_alltty[serialPortNum]);
	if(fd <=0){
		Test_Log( "Test_PortRecv打开串口错误" );
		return -1;
	}
	Test_Log( "Test_PortRecv开始测试通道[%d]" ,chnlog);
	 //从PC接收返回数据
	//Test_Log("从PC接收返回数据");
	for(i=0;i<tryn;i++){		
		sleep(1);//等待对方执行完毕
		bzero(buff,sizeof(buff));
		cnt =RECLEN;//58
		ret = Test_PortRead(fd, serialPortNum, phyChnl, buff, &cnt);
		if(ret <=0){
			Test_Log( "Test_PortRead读数据失败" );
			sleep(10);
			continue;	
		}else{
			Test_Log( "Test_PortRead读数据成功" );
			return 1;
		}
		/*sleep(10);
		
		memset(cmd,0x5a,sizeof(cmd));
		cmd[0] = 0x1B;
		cmd[1] = 0x30+phyChnl-1;
		
		if( sizeof(cmd) > MAX_PORT_CMD_LEN ) {
			Test_Log( "向端口写的数据长度[%d]超过最大长度[%d]", sizeof(cmd), MAX_PORT_CMD_LEN );
			close(fd);
			return -1;
		}		 
		cmd[2] = 0x00+ (sizeof(cmd) -3 ) ;
		//memcpy( cmd+3, buff, len );
		cnt =sizeof(cmd);
		sleep(1);
		Test_Log("Test_SendRecv发送数据[%s]",Test_Asc2Hex(cmd, cnt));
		switch(serialPortNum){
			case 3:Test_Act_P(TEST_SEM_COM3);
				break;
			case 4:Test_Act_P(TEST_SEM_COM4);
				break;
			case 5:Test_Act_P(TEST_SEM_COM5);
				break;
			case 6:Test_Act_P(TEST_SEM_COM6);
				break;
			default:Test_Log( "串口号[%d]错误",serialPortNum );
				break;			
		}
		ret = Test_WriteTty(fd, cmd, cnt);
		switch(serialPortNum){
			case 3:Test_Act_V(TEST_SEM_COM3);
				break;
			case 4:Test_Act_V(TEST_SEM_COM4);
				break;
			case 5:Test_Act_V(TEST_SEM_COM5);
				break;
			case 6:Test_Act_V(TEST_SEM_COM6);
				break;
			default:Test_Log( "串口号[%d]错误",serialPortNum );
				break;			
		}
		//ret = Test_WriteTty(fd, cmd, cnt);
		if(ret <=0){
			Test_Log( "向端口写数据失败" );
			sleep(1);
			continue;
		}
		sleep(1);//再发送一次
		switch(serialPortNum){
			case 3:Test_Act_P(TEST_SEM_COM3);
				break;
			case 4:Test_Act_P(TEST_SEM_COM4);
				break;
			case 5:Test_Act_P(TEST_SEM_COM5);
				break;
			case 6:Test_Act_P(TEST_SEM_COM6);
				break;
			default:Test_Log( "串口号[%d]错误",serialPortNum );
				break;			
		}
		ret = Test_WriteTty(fd, cmd, cnt);
		switch(serialPortNum){
			case 3:Test_Act_V(TEST_SEM_COM3);
				break;
			case 4:Test_Act_V(TEST_SEM_COM4);
				break;
			case 5:Test_Act_V(TEST_SEM_COM5);
				break;
			case 6:Test_Act_V(TEST_SEM_COM6);
				break;
			default:Test_Log( "串口号[%d]错误",serialPortNum );
				break;			
		}
		//ret = Test_WriteTty(fd, cmd, cnt);
		if(ret <=0){
			Test_Log( "向端口写数据失败" );
			sleep(1);
			continue;
		}
		break;*/
	}
	if( (ret <=0) ||(i >=tryn) ){
		Test_Log( "从端口[%d]读数据[%d]次全部失败" ,chnlog, tryn);
		close(fd);
		return -1;
	}	
	close(fd);
	return 0;
}
//串口收发函数
int Test_ComSendRecv(int serialPortNum){
	int i, ret, tmp,fd,n,k;//,, sockfd
	unsigned char buff[64] ;
	int timeout = 5, tryn = 3;
	char		cmd[16];
	int			cnt;
	
	
	fd = Test_OpenSerial(TEST_alltty[serialPortNum]);
	if(fd <=0){
		Test_Log( "Test_ComRecvSend打开串口错误" );
		return -1;
	}
	//Test_Log("向PC发送数据");
	Test_Log( "Test_ComSendRecv开始测试串口[%d]" ,serialPortNum);
	for(i=0;i<tryn;i++){
		memset(cmd,0xa5,sizeof(cmd));
		if( sizeof(cmd) > MAX_PORT_CMD_LEN ) {
			Test_Log( "向串口[%d]写的数据长度[%d]超过最大长度[%d]", serialPortNum,sizeof(cmd), MAX_PORT_CMD_LEN );
			close(fd);
			return -1;
		}		 
		//cmd[2] = 0x00+ (sizeof(cmd) -3 ) ;
		//memcpy( cmd+3, buff, len );
		cnt =sizeof(cmd);
		Test_Log("Test_ComRecvSend发送数据[%s]",Test_Asc2Hex(cmd, cnt));
		ret = Test_WriteTty(fd, cmd, cnt);
		if(ret <=0){
			Test_Log( "向串口[%d]写数据失败" ,serialPortNum);
			sleep(5);
			continue;
		}
		//break;
		sleep(2);
		bzero(buff,sizeof(buff));
		cnt =sizeof(cmd);
		ret = Test_ReadTty(fd, buff, &cnt);
		if(ret <=0){
			Test_Log( "串口[%d]读数据失败",serialPortNum );
			sleep(5);
			continue;
		}
		Test_Log("Test_ComRecvSend接收数据[%s]",Test_Asc2Hex(buff, cnt));	
		break;
	}
	if( (ret <=0) ||(i >=tryn) ){
		Test_Log( "从串口[%d]读数据[%d]次全部失败" ,serialPortNum,tryn);
		close(fd);
		return -1;
	}
		
	close(fd);
	return 0;
}
int Test_ComRecvSend(int serialPortNum){
	//#define  LOGIN_TIME 1000
	int i, ret, tmp,fd,n,k;//,, sockfd
	//int sockflag =0;
	//unsigned char *sIPName;	//sPortName[5],
	unsigned char buff[64] ;	//,buff[64]sendBuffer[16] ,,*pBuff
	//unsigned int msgLength  ;
	int timeout = 5, tryn = 3;
	//unsigned long localIP;	
	//int serialPortNum, phyChnl;
	char		cmd[16];
	int			cnt;
	
	
	fd = Test_OpenSerial(TEST_alltty[serialPortNum]);
	if(fd <=0){
		Test_Log( "Test_ComRecvSend打开串口错误" );
		return -1;
	}
	//Test_Log("向PC发送数据");
	Test_Log( "Test_ComRecvSend开始测试串口[%d]" ,serialPortNum);
	for(i=0;i<tryn;i++){		
		sleep(1);//等待对方执行完毕
		bzero(buff,sizeof(buff));
		cnt =sizeof(cmd);
		ret = Test_ReadTty(fd, buff, &cnt);
		if(ret <=0){
			Test_Log( "串口[%d]读数据失败",serialPortNum );
			sleep(5);
			continue;
		}
		Test_Log("Test_ComRecvSend接收数据[%s]",Test_Asc2Hex(buff, cnt));
		sleep(5);
		//break;
		memset(cmd,0x5a,sizeof(cmd));
		if( sizeof(cmd) > MAX_PORT_CMD_LEN ) {
			Test_Log( "向串口[%d]写的数据长度[%d]超过最大长度[%d]", serialPortNum,sizeof(cmd), MAX_PORT_CMD_LEN );
			close(fd);
			return -1;
		}		 
		//cmd[2] = 0x00+ (sizeof(cmd) -3 ) ;
		//memcpy( cmd+3, buff, len );
		cnt =sizeof(cmd);
		Test_Log("Test_ComRecvSend发送数据[%s]",Test_Asc2Hex(cmd, cnt));
		ret = Test_WriteTty(fd, cmd, cnt);
		if(ret <=0){
			Test_Log( "向串口[%d]写数据失败" ,serialPortNum);
			sleep(5);
			continue;
		}
		sleep(2);//再发送一次
		cnt =sizeof(cmd);
		Test_Log("Test_ComRecvSend发送数据[%s]",Test_Asc2Hex(cmd, cnt));
		ret = Test_WriteTty(fd, cmd, cnt);
		if(ret <=0){
			Test_Log( "向串口[%d]写数据失败" ,serialPortNum);
			sleep(5);
			continue;
		}
		break;
	}
	if( (ret <=0) ||(i >=tryn) ){
		Test_Log( "从串口[%d]读数据[%d]次全部失败" ,serialPortNum,tryn);
		close(fd);
		return -1;
	}
	
	close(fd);
	return 0;
}


//根据检测到的U盘名称挂载U盘
static int Main_mountUsb(){

	unsigned char *fileName = "/proc/partitions";
	unsigned char buff[64],usbDev[4],devName[64],mountName[128],rmDev[64];
	FILE *fp;
	int i, j,ret, first_sdx;
	
	fp = fopen(fileName, "rt");
	if(fp == NULL){
		char pbuff[256];
		bzero(pbuff,sizeof(pbuff));
		sprintf( pbuff, "fileName[%s] 打开失败!",fileName);
		Test_Log(pbuff);
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
		char pbuff[256];
		bzero(pbuff,sizeof(pbuff));
		sprintf( pbuff, "读取U盘名称失败!buff[%s]", buff);
		Test_Log(pbuff);
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
		char pbuff[256];
		bzero(pbuff,sizeof(pbuff));
		sprintf( pbuff, "devName[%s] 创建U盘设备文件失败!", devName);
		Test_Log(pbuff);
		return -1;
	}

	//卸载挂载项
	system(Test_USB_UM);
	//挂载U盘
	bzero(mountName,sizeof(mountName));
	sprintf(mountName,"/bin/mount -t vfat /dev/sd%s /home/ftp",usbDev);
	ret = system(mountName);
	if(ret < 0){
		char pbuff[256];
		bzero(pbuff,sizeof(pbuff));
		sprintf( pbuff, "mountName[%s] 挂载U盘失败!", mountName );
		Test_Log(pbuff);
		return -1;
	}

	return 0;
}


//
static int Test_readUSB(int usbnum){

	//char *method;
	char  my_data[16];
	char *tmp_ptr, *tmp;
	int data_len;
	int i,ret;
	int c;
	FILE *f;
	
	if( (1 !=usbnum )&&(2 !=usbnum) &&(3 !=usbnum)&&(4 !=usbnum) ){
		Test_Log("Test_readUSB参数值错误,值[%d]",usbnum);
		return -1;
	}
	
	//挂接U盘
      	ret = Main_mountUsb();
	if( ret < 0 )
	{
		char pbuff[256];
		bzero(pbuff,sizeof(pbuff));
		sprintf( pbuff, "***挂载USB命令执行失败");
		Test_Log(pbuff);
		system(Test_USB_UM);
		return -1;//exit(1);
	}	
	ret = Test_IsFileExist(Test_USB_File);
	if(ret != 1){
		char pbuff[256];
		bzero(pbuff,sizeof(pbuff));
		sprintf( pbuff, "file[%s] not exist in Test_readUSB  error!! ", Test_USB_File );
		Test_Log(pbuff);
		system(Test_USB_UM);
		return -2;
	}
	
	f = fopen( Test_USB_File, "rt" );
	if( !f  ){	
		char pbuff[256];
		bzero(pbuff,sizeof(pbuff));
		sprintf( pbuff, "file[%s] open error in readIni!! ", Test_USB_File );
		Test_Log(pbuff);
		//exit(1);
		system(Test_USB_UM);
		return -3;
	}	
	bzero(my_data, sizeof(my_data));
	for(i=0; i<sizeof(Test_USB_Data); i++){
		c = fgetc( f);
		if( c == EOF)
			break;
		my_data[i] = c;
	}
	my_data[i] = 0;	
	fclose( f );
	//去除挂接U盘
	ret = system(Test_USB_UM);
	if( ret < 0 )
	{
		Test_Log( "***USB命令[%s]执行失败\n" ,Test_USB_UM);
		return -1;//exit(1);
	}	
	if(  0 != memcmp( my_data,Test_USB_Data,sizeof(Test_USB_Data) )  ){
		Test_Log( "读取的数据[%s]和应当读取到的数据[%s]不同\n" ,my_data,Test_USB_Data);
		return -1;
	}
	return 0;	
}

void Test_AcceptPCCmd( ){
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
		
	
	
	
	sock = Test_TcpServInit(TEST_TCP_PORT);
	if ( sock < 0 ) {
		Test_Log( "实时进程初始化socket失败.port=[%s]\n", TEST_TCP_PORT );
		//Free( cmd );
		return  ;
	}
	//fcntl(sock, F_SETFL, O_NONBLOCK);   // 非阻塞模式
	
	newsock  = -1;
	tmp_sock = -1;
	while (1) {	
		if (errcnt > 32) {
			Test_Log("out of ERR_CNT, close the tcp connection (PC.C --> FCC.S)");
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
		tmp_sock = Test_TcpAccept(sock);//, NULL
		Test_Log("Test_AcceptPCCmd连接,soket号[%d]", tmp_sock);
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
				Test_Log( "接收数据[%d]次都失败", tryNum );
				break; 
			}
			ret = Test_TcpRecvDataInTime(newsock, buffer, &msg_lg, timeout );//TIME_INFINITE	
			if (ret < 0) {
				if (errno == 4) {  // time out
					Test_Log("进程超时,接收到后台命令 buffer[%s]\n" , Test_Asc2Hex(buffer, 32) );//
					continue;
				} else if (errno == 11)  {  // maybe CD quit or restart, && (gpIfsf_shm->need_reconnect == 0))
					//gpIfsf_shm->need_reconnect = 1;
					Test_Log("Client need to reconnect to Server");
				}	
				Test_Log( "进程接收数据失败,errno:[%d][%s],buffer: %s", errno, strerror(errno), \
					Test_Asc2Hex(buffer, 2) );
				errcnt++;
				sleep(1);
				continue;
			} else {
				Test_Log( "<<<进程收到命令: [%s(%d)]", Test_Asc2Hex(buffer, msg_lg ), msg_lg);				
				if ( (0 == (buffer[0]>>4) )||(0 ==( buffer[0]&0x0f) )  ){ //received data must be error!!
					Test_Log( "received data error" );
					errcnt++;
					sleep(1);
					continue;
				}

				if (0 == msg_lg){ //receivd data must be error!!
					Test_Log( "received size is  0 error" );
					errcnt++;
					//回复错误
					sBuffer[0] = 0x1f;
					sBuffer[1] = 0;
					//memcpy(&sBuffer[1],gpCSS_Shm->css._STATION_NO_BCD,5);
					ret = Test_TcpSendDataInTime(newsock, sBuffer, 2, timeout);
					if(ret <=0){				
						Test_Log("实时进程回复数据 失败");
					}
					
					sleep(1);
					continue;
				}

				
				
				//处理后台命令.
				Test_Log( "开始处理收到命令: [%s(%d)]", Test_Asc2Hex(buffer, msg_lg ), msg_lg);
				if (0 == buffer[1]){ 
					if(0x99 == buffer[0]){
						Test_Log( "收到退出命令: [%s(%d)],退出", Test_Asc2Hex(buffer, msg_lg ), msg_lg);
						
					}
				}
				
				break;
				//sleep(10);			
			}
		}
		if ( errcnt > 40 ) {
			/*错误处理*/
			Test_Log( "初始化socket 失败.port=[%d]", TEST_TCP_PORT );
			break; 
		}
	}

	Test_Log("Test_AcceptPCCmd  Stop ......");
	close(sock);

	return  ;
}
/*//主从FCC同步
int Test_SynStart(int NumFcc){
	int ret,n;
	unsigned char WaitSendBuff[2][5] =
		{
			{0xff,0x03,0x11,0,0},
			{0xff,0x03,0x22,0,0}
		};
	unsigned char WaitFlag[2] = {0x11,0x22};
	unsigned char WaitRecvBuff[8];

	
	if((1 != NumFcc)&&(2 !=NumFcc)){
		Test_Log( "***Fcc_SynStart参数错误,程序退出" );
		exit(-1);
	}
	while(1){
		n = 3;
		ret = Test_TCPSendRecv(WaitSendBuff[NumFcc-1],5, WaitRecvBuff,&n);
		if(ret <0){
			Test_Log("发送测试中间同步值错误继续");
			sleep(5);
			continue;
		}else{
			if( (0xff != WaitRecvBuff[0]) || (0x01 != WaitRecvBuff[1])|| (WaitFlag[NumFcc-1] != WaitRecvBuff[2]) ){//0 != memcmp(TestFlag,"2200",2)
				sleep(2);
				continue;
			}else{
				Test_Log("测试中间同步成功,继续测试");
				break;
			}
		}
	}
	return 0;
}
*/
//
void Test_Com_Thread(void  *arg){
	int ret;
	int sn;
	//unsigned char TestFlag[8];
	
	sn = (int *)arg;
	//sn = (int )(*arg);
	if(sn<=0 ||sn >6){
		Test_Log("Test_Com_Thread参数值错误,值[%d],表示串口号",sn);
		if(2 == FccNum){
			Test_Log("***串口[%d]测试失败",sn);
		}else{
			Test_Log("串口[%d]测试失败",sn);
		}
		return;
	}
	if(2 == FccNum){
		Test_Log("***开始串口[%d]测试",sn);		
		ret = Test_ComRecvSend(sn);
		if(ret <0){			
			Test_Log("***串口[%d]测试失败",sn);				
		}else{
			Test_Result[2]  = Test_Result[2]  | (1<<(8-sn-1) );
			Test_Log("***串口[%d]测试成功",sn);
		}
	}else if(1 == FccNum){
		Test_Log("开始串口[%d]测试",sn);
		ret = Test_ComSendRecv(sn);
		if(ret <0){			
			Test_Log("串口[%d]测试失败",sn);				
		}else{
			Test_Result[2]  = Test_Result[2]  | (1<<(8-sn-1) );
			Test_Log("串口[%d]测试成功",sn);
		}
	}else{
		Test_Log("***主从FCC标志出错[%d],退出",FccNum);
		ret = Test_PortReadEnd();
		exit(0);
	}
	//return;
}
void Test_Port_Thread(void  *arg){//arg通道号-1
	int i,k,n,ret;//,spn
	int  sn;
	sn = (int *)arg;
	//sn = (int ) *arg;
	sn++;//sn通道号
	if(sn<=0 ||sn >64){//
		Test_Log("Test_Port_Thread参数值错误,值[%d],表示通道号",sn);
		return;
	}
	//计算通道数目	
	k=portChanel[3]+portChanel[4]+portChanel[5]+portChanel[6];
	/*//计算串口号spn
	spn = sn;
	for(i=3;i<=6;i++){
		if((spn-=portChanel[i])<=0){
			spn = i;
			break;
		}
	}*/
	if(1 == FccNum){
		n = ( (sn-1) / 8) +1;
		Test_Log( "开始测试通道[%d]" ,sn);
		ret = Test_PortSendRecv(sn);
		if(ret <0){
			Test_Log("通道[%d]测试失败",sn);
		}else{
			Test_Act_P(TEST_SEM_RESULT);
			Test_Result[3+n-1] = Test_Result[3+n-1] | (1<<(8*n-sn) );
			Test_Act_V(TEST_SEM_RESULT);
			Test_Log("通道[%d]测试成功", sn);
		}				
	}else if(2 == FccNum){			
			n = ( (sn-1) / 8) +1;
			Test_Log( "***开始测试通道[%d]" ,sn);
			ret = Test_PortRecvSend(sn);
			if(ret <0){
				Test_Log("***通道[%d]测试失败",sn);
			}else{
				Test_Act_P(TEST_SEM_RESULT);
				Test_Result[3+n-1] = Test_Result[3+n-1] | (1<<(8*n-sn) );
				Test_Act_V(TEST_SEM_RESULT);
				Test_Log( "***通道[%d]测试成功" ,sn);
			}		
	}else{
		Test_Log("***主从FCC标志出错[%d],退出",FccNum);
		ret = Test_PortReadEnd();
		exit(0);
	}
}
//Gilbarco专用线程
void Test_Port_Thread_Grb(void  *arg){//arg通道号-1
	int i,k,n,ret;//,spn
	int  sn;
	sn = (int *)arg;
	//sn = (int )*arg;
	sn++;//sn通道号
	if(sn<=0 ||sn >64){//
		Test_Log("Test_Port_Thread参数值错误,值[%d],表示通道号",sn);
		return;
	}
	//计算通道数目	
	k=portChanel[3]+portChanel[4]+portChanel[5]+portChanel[6];
	/*//计算串口号spn
	spn = sn;
	for(i=3;i<=6;i++){
		if((spn-=portChanel[i])<=0){
			spn = i;
			break;
		}
	}*/
	if(1 == FccNum){
		n = ( (sn-1) / 8) +1;
		Test_Log( "开始测试通道[%d]" ,sn);
		ret = Test_PortSend(sn);
		if(ret <0){
			Test_Log("通道[%d]测试失败",sn);
		}else{
			Test_Act_P(TEST_SEM_RESULT);
			Test_Result[3+n-1] = Test_Result[3+n-1] | (1<<(8*n-sn) );
			Test_Act_V(TEST_SEM_RESULT);
			Test_Log("通道[%d]测试成功", sn);
		}				
	}else if(2 == FccNum){			
			n = ( (sn-1) / 8) +1;
			Test_Log( "***开始测试通道[%d]" ,sn);
			ret = Test_PortRecv(sn);
			if(ret <0){
				Test_Log("***通道[%d]测试失败",sn);
			}else{
				Test_Act_P(TEST_SEM_RESULT);
				Test_Result[3+n-1] = Test_Result[3+n-1] | (1<<(8*n-sn) );
				Test_Act_V(TEST_SEM_RESULT);
				Test_Log( "***通道[%d]测试成功" ,sn);
			}		
	}else{
		Test_Log("***主从FCC标志出错[%d],退出",FccNum);
		ret = Test_PortReadEnd();
		exit(0);
	}
}


//主从FCC同步
int Test_SynStart(int NumFcc){
	int ret,n;
	unsigned char WaitSendBuff[2][5] =
		{
			{0xff,0x03,0x11,0,0},
			{0xff,0x03,0x22,0,0}
		};
	unsigned char WaitFlag[2] = {0x11,0x22};
	unsigned char WaitRecvBuff[8];

	
	if((1 != NumFcc)&&(2 !=NumFcc)){
		Test_Log( "***Fcc_SynStart参数错误,程序退出" );
		ret = Test_PortReadEnd();
		exit(-1);
	}
	while(1){
		n = 3;
		ret = Test_TCPSendRecv(WaitSendBuff[NumFcc-1],5, WaitRecvBuff,&n);
		if(ret <0){
			Test_Log("发送测试中间同步值错误继续");
			sleep(5);
			continue;
		}else{
			if( (0xff != WaitRecvBuff[0]) || (0x01 != WaitRecvBuff[1])|| (WaitFlag[NumFcc-1] != WaitRecvBuff[2]) ){//0 != memcmp(TestFlag,"2200",2)
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
	pthread_t ThreadChFlag[64];//通道线程数组
	pthread_t ThreadSrFlag[8];//串口线程数组
	if (argc == 2) {
	       if (strcmp(argv[1], "-v") == 0) {
			printf( "%s version: %s\n", argv[0], PCD_VER);
	       } 
		return 0;
	}

	
	printf("开始运行测试程序\n");
	
	//初始化日志
	printf("初始化日志.\n");
	ret = Test_InitLog(1);//0: UDP日志,1:打印日志
	if(ret < 0){
		printf("日志初始化出错.\n");
		exit(-1);  
	}

		
	/*//初始化共享内存
	ret = Test_InitTheKeyFile();
	if( ret < 0 )
	{
		Test_Log( "初始化Key File失败\n" );
		exit(1);
	}
	*/
		
	//初始化串口通道数目
	Test_Log("开始初始化串口通道数目");
	ret = Test_InitPortChn();
	if( ret < 0 )
	{
		Test_Log( "初始化串口通道失败\n" );
		exit(-1);
	}
	
	//初始化配置参数
	Test_Log("开始初始化配置参数");
	ret = Test_InitConfig();
	if(ret <0){
		Test_Log("配置参数初始化出错.");
		exit(-1);
	}

	//初始化注册
	Test_Log("开始初始化注册");
	ret = Test_Login();
	if(ret <0){
		Test_Log("初始化注册出错.");
		exit(-1);
	}
	// 创建一个信号量.
	ret = Test_CreateSem( TEST_SEM_COM3 );
	if( ret < 0 ) {
		Test_Log( "创建信号量失败" );
		exit(-1);//return -1;
	}
	ret = Test_CreateSem( TEST_SEM_COM4 );
	if( ret < 0 ) {
		Test_Log( "创建信号量失败" );
		exit(-1);//return -1;
	}
	ret = Test_CreateSem( TEST_SEM_COM5 );
	if( ret < 0 ) {
		Test_Log( "创建信号量失败" );
		exit(-1);//return -1;
	}
	ret = Test_CreateSem( TEST_SEM_COM6 );
	if( ret < 0 ) {
		Test_Log( "创建信号量失败" );
		exit(-1);//return -1;
	}
	ret = Test_CreateSem( TEST_SEM_RESULT );
	if( ret < 0 ) {
		Test_Log( "创建信号量失败" );
		exit(-1);//return -1;
	}
	
	//通道读初始化
	ret = Test_PortReadInit();
	if( ret <= 0 ) {
		Test_Log( "通道读初始化失败" );
		exit(-1);//return -1;
	}
	
	//循环测试
	while(1){
		//等待测试开始命令
		if(2 == FccNum){
			Test_Log("***等待测试开始命令");
		}else{
			Test_Log("等待测试开始命令");
		}
		ret = Test_WaitStartCmd();
		if(ret <0){
			Test_Log("开始测试命令错误,退出");
			exit(-1);
		}
# if 0
		if(2 == FccNum){
			Test_Log("***开始控制口(串口0)测试",i);
			ret = Test_ComSendRecv(0);
			if(ret <0){
				Test_Log("***控制口(串口0)测试失败",i);
				//exit(0);
			}else{
				//TestFlag[0] = TestFlag[0] | (1<<7 );					
				Test_Log("***控制口(串口0)测试成功",i);
			}
			ret = Test_SynStart(FccNum);
		}else{
			sleep(30);
			ret = Test_SynStart(FccNum);
		}
#endif		
		//-----------------------------
		//开始测试
		bzero(Test_Result,sizeof(Test_Result) );
		bzero(ThreadChFlag,sizeof(ThreadChFlag) );
		bzero(ThreadSrFlag,sizeof(ThreadSrFlag) );
		Test_Result[0] = 0x22;
		Test_Result[1] = 0x0a;
		
			
		
		//计算通道数目k = 0;	
		k=portChanel[3]+portChanel[4]+portChanel[5]+portChanel[6];
		if(k>64){
			Test_Log("***通道(背板连接号)数目太多[%d],退出",k);
			exit(0);
		}
		//通道(背板连接号)测试
		for(i=0;i<k;i++){
			if(0 == Gilbarco){
				if ( pthread_create( &ThreadChFlag[i], NULL, (void *)Test_Port_Thread, (void *)i ) )  {
					Test_Log("创建通道[%d] 线程错误",i+1);
					abort();
				}
			}else if(1 == Gilbarco){
				if ( pthread_create( &ThreadChFlag[i], NULL, (void *)Test_Port_Thread_Grb, (void *)i ) )  {
					Test_Log("创建长吉通道[%d] 线程错误",i+1);
					abort();
				}
			}else{
				Test_Log("长吉标志值[%d] 错误",Gilbarco);
			}
			sleep(5);
			//sleep(10);
			//ret = Test_SynStart(FccNum);
		}
				
		//串口测试
		if(2 == FccNum){
			Test_Log("***开始串口测试");
		}else{
			Test_Log("开始串口测试");
		}
		bzero(TestFlag,sizeof(TestFlag));
		if(1 == FccNum){
			//串口0单独与PC测试
			for(i=1;i<3;i++){
				//Test_Log("开始串口[%d]测试",i);
				if ( pthread_create( &ThreadSrFlag[i], NULL, (void *)Test_Com_Thread, (void *)i ) )  {
					Test_Log("创建串口[%d] 线程错误",i+1);
					ret = Test_PortReadEnd();
					abort();
				}
				sleep(1);
				//sleep(10);
				//ret = Test_SynStart(FccNum);	
				}
		}else if(2 == FccNum){
			//串口0单独与PC测试
			for(i=1;i<3;i++){
				//Test_Log("开始串口[%d]测试",i);
				if ( pthread_create( &ThreadSrFlag[i], NULL, (void *)Test_Com_Thread, (void *)i ) )  {
					Test_Log("***创建串口[%d] 线程错误",i+1);
					ret = Test_PortReadEnd();
					abort();
				}
				sleep(1);
				//sleep(10);
				//ret = Test_SynStart(FccNum);
			}
		}else{
			Test_Log("***主从FCC标志出错[%d], 退出",FccNum);
			ret = Test_PortReadEnd();
			exit(0);
		}
		//memcpy(&Test_Result[2],TestFlag,1);
		//Test_Result[2] = TestFlag[0];
		
		//USB口测试,只是测试从FCC
		if(2 == FccNum){
			Test_Log("***开始控制口(串口0)测试",i);
			ret = Test_ComSendRecv(0);
			if(ret <0){
				Test_Log("***控制口(串口0)测试失败",i);
				//exit(0);
			}else{
				TestFlag[0] = TestFlag[0] | (1<<7 );					
				Test_Log("***控制口(串口0)测试成功",i);
			}
			Test_Result[2] =Test_Result[2] | TestFlag[0];
			Test_Log("***开始USB口测试");
			bzero(TestFlag,sizeof(TestFlag));
			//USB1口测试
			Test_Log("***开始USB口1测试");
			ret = Test_readUSB(1);
			if(ret <0){
				Test_Log("***USB口[1]测试失败");
				sleep(1);
				//USB2口测试
				Test_Log("***开始USB口2测试");
				ret = Test_readUSB(2);
				if(ret <0){
					Test_Log("***USB口[2]测试失败");
				}else{
					Test_Log("***USB口[2]测试成功");
					TestFlag[0] = TestFlag[0] | 0xc0;//0x40
				}
					//exit(0);
			}else{
				Test_Log("***USB口[1]测试成功");
				TestFlag[0] = TestFlag[0] | 0xc0;//0x80
			}
		

			Test_Result[11] = TestFlag[0];
		}else{
			Test_Log("不是从FCC[%d],不测试控制口(串口0)和USB口",FccNum);
		}
		for(i=0;i<k;i++){
			if ( pthread_join ( ThreadChFlag[i], NULL ) ) {
				Test_Log("joining 通道[%d]线程错误",i+1);
				ret = Test_PortReadEnd();
				abort();
			}else{
				Test_Log("joining 通道[%d]线程结束",i+1);
			}
		}
		for(i=1;i<3;i++){
			if(0==ThreadSrFlag[i]){
				continue;
			}
			else if ( pthread_join ( ThreadSrFlag[i], NULL ) ) {
				Test_Log("joining串口[%d]线程错误",i);
				ret = Test_PortReadEnd();
				abort();
			}else{
				Test_Log("joining串口[%d]线程结束",i);
			}
		}
			
		/*if ( pthread_create( &mythread2, NULL, (void *)thread_function2, (void *)k ) )  {
		printf("error creating thread2.\n");
		abort();
		}
		while(1){
			sleep(4);
			if(0 == pthread_kill(mythread1,0) ){
				printf("thread1 ok.\n");
			}else{
				printf("thread1 exit[%d].\n", pthread_kill(mythread1,0));
				//exit(-1);
			}
			if(0 == pthread_kill(mythread2,0) ){
				printf("thread2 ok.\n");
			}else{
				printf("thread2 exit[%d].\n", pthread_kill(mythread2,0));
				//exit(-1);
			}
		}*/
		ret = Test_SynStart(FccNum);//同步
		ret = Test_PortReadEnd();
		if( ret < 0 ) {
			Test_Log( "通道读结束失败" );
			return -1;
		}
		//发送测试结果
		n = 2;
		 if(2 == FccNum){
		 	ret = Test_TCPSendRecv(Test_Result,12, TestFlag,&n);
			if(ret <0){
				Test_Log("发送测试结果错误,退出");
				exit(-1);
			}else{
				if( (0x22 != TestFlag[0]) || (0x0 != TestFlag[1]) ){//0 != memcmp(TestFlag,"2200",2)
					Test_Log("发送测试结果,收到的数据[%02x%02x]错误,退出",TestFlag[0],TestFlag[1]);
					exit(-1);
				}
			}
			k = 0;//计算通道数目	
			k=portChanel[3]+portChanel[4]+portChanel[5]+portChanel[6];
			//bzero(TestFlag,sizeof(TestFlag));
			TestFlag[0] =1;
			if((0xe0&Test_Result[2]) !=0xe0){//串口只要0,1,2三个成功就可以了
				TestFlag[0] = 0;
				Test_Log("串口0,1,2中至少一个没有测试通过[%02x]",Test_Result[2]);
				//exit(0);
			}
			for(i=0;i<k/8;i++){//通道要全部通过
				if(Test_Result[i+3] != 0xff){
					TestFlag[0] = 0;
					Test_Log("通道[%d~%d]测试没有通过",i*8+1,i*8 +8);
				}
			}
			if((0x80&Test_Result[11]) !=0x80){//USB只要第一个成功即可
				TestFlag[0] = 0;
				Test_Log("USB1测试没有通过[%02x]",Test_Result[11]);
			}
			if(1 != TestFlag[0] ){
				Test_Log("***从FCC测试失败,请关闭从FCC,检查情况,等待再次测试");
				exit(-1);
			}else{
			 	Test_Log("***从FCC测试成功,等待下一个从FCC测试");
				exit(0);
			 }
			 //Test_Log("***自测试程序一次测试完毕,等待下次测试");
		 }else{
		  	Test_Log("自测试程序一次测试完毕,等待下次测试");
		  }
	}
	 //Test_AcceptPCCmd();
	
	 Test_Log("***自测试程序异常退出");
	return -1;
}







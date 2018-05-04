/*
 * 2008-06-16 - Chen Fu-ming <chenfm@css-intelligent.com>
 *    
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
#include <errno.h>

#include "log.h"
#include "ifsf_pub.h"
#include "ifsf_tlg.h"

#define OLD_PROTOCOL  1

#define  DLA_CONFIG_FILE                     "/home/App/ifsf/etc/dla.cfg"
#define DLA_LOG_FILE                            "/home/App/ifsf/log/dla.log"
#define DEFAULT_WORK_DIR_4DLA  "/home/App/ifsf/"  //前庭控制器的默认工作目录,用于GetWorkDir()函数.
#define RUN_WORK_DIR_FILE           "/var/run/ifsf_main.wkd"  //应用程序运行目录文件,用于GetWorkDir()函数.
#define DLA_DSP_CFG_FILE                     "/home/App/ifsf/etc/dsp.cfg" 
#define MAX_PATH_NAME_LENGTH 1024
#define DLA_FIFO "/tmp/css_fifo.data"
//#define  DLA_SERVER_TCP_PORT_4LOGIN 9001
//#define  DLA_SERVER_TCP_PORT_4DATA  9002
//#define  DLA_SERVER_UDP_PORT_4REALTIME  9010
//#define  DLA_CSS_TCP_PORT_4REALTIME  9006

//油枪配置结构体,对应油枪配置文件dsp.cfg  modify by liys
typedef struct {
	unsigned char node;             //节点号
	unsigned char chnLog;          //逻辑通道号
	unsigned char nodeDriver;      //驱动号
	unsigned char phyfp;           //物理面板号, 1+.
	unsigned char logfp;           //逻辑面板号, 1+.
	unsigned char physical_gun_no; //物理油枪号
	unsigned char logical_gun_no;  //逻辑油枪号
	unsigned char oil_no[4];       //油品号
	unsigned char isused;          //是否启用
} DLA_GUN_CONFIG;
//大排序油枪号计算关系 
typedef struct {
	unsigned char node;             //IFSF节点号
	unsigned char fp;           //IFSF面板号, 1+.
	//unsigned char  physical_gun_no; //节点排序枪号
	unsigned char  logical_gun_no;//面板枪号
	unsigned char  big_gun_no; //大排序油枪号
	unsigned char  moved_gun_no; //本条枪的IFSF动枪号
} DLA_BIG_GUN;
typedef struct{ //import!!
	/*为了方便存取定义的量:*/
	unsigned char offline_flag;   // offline TR flag: 0 - normal TR, !0 - offline TR
	unsigned char node; //node is in a dispenser's LNA 
	unsigned char fp_ad; //21h,22h,23h,24h.It's also sequence number of FP.
	/*TRANSACTION DATA:*/
	unsigned char TR_Seq_Nb[2];                             unsigned char TR_Contr_Id[2];
	unsigned char TR_Release_Token;                        	unsigned char TR_Fuelling_Mode;
	unsigned char TR_Amount[5];                             unsigned char TR_Volume[5];
	unsigned char TR_Unit_Price[4];                         unsigned char TR_Log_Noz;
	//unsigned char TR_Prod_Description[16]; /*O, not used*/ 
	unsigned char TR_Prod_Nb[4];                            unsigned char TR_Error_Code;
	unsigned char TR_Security_Chksum[3];
	//unsigned char M1_Sub_Volume[5];//not used
	//unsigned char M2_Sub_Volume[5]; //not used 
	//unsigned char TR_Tax_Amount[5]; /*O, not used*/
	/*TRANSACTION BUFFER STATUS:*/
	unsigned char Trans_State;                              unsigned char TR_Buff_Contr_Id[2];
	//Clear_Transaction,Lock_Transaction,Unlock_Transaction.They are command.
	/*UNSOLICITED DATA:由MakeTrMsg函数计算出!!*/
	//unsigned char TR_Buff_States_Message[17]; //see IFSF 3-01,128
	/*MANUFACTURER/OIL COMPANY SPECIFIC*/
	unsigned char TR_Date[4]; // user defined, Data_Id 202, offline Transaction Date
	unsigned char TR_Time[3]; // user defined, Data_Id 203, offline Transaction Time
	unsigned char TR_Start_Total[7]; // user defined, Data_Id 204, Start Log_Noz_Vol_Total
	unsigned char TR_End_Total[7];  // user defined, Data_Id 205, End Log_Noz_Vol_Total
} DLA_FP_TR_DATA; //42 byte;
 typedef struct {
	unsigned char _SERVER_IP[16]; /*服务器IP地址*/
	int _SERVER_TCP_PORT;
	int _SERVER_UDP_PORT;
	int _REALTIME_FLAG; //实时数据上传标志,0不上传,1上传
	 int _SOCKTCP,_SOCKUDP;//_SOCKTCP用于交易数据上传,_SOCKUDP用于实时数据上传
	unsigned char *sendBuffer;//,buff[16],*pBuff	
	int msgLength ;//, cfgLength unsigned long
	int fd_fifo ; //FIFO的文件描述符
	unsigned char  totalGun;//总枪数
	DLA_BIG_GUN *bigGun;//最多256把枪
	//unsigned char _STATION_NO[11]; /*站点编号数字字符串10位数字(省2市2站6)*/
	//unsigned char _STATION_NO_BCD[5]; /*BCD码,站点编号10位数字(省2市2站6)*/
	// int _TLG_SEND_INTERVAL; /*液位仪上传时间间隔*/
	 
	 //struct sockaddr _SERVER_UDP_ADDR;//struct sockaddr_in _SERVER_UDP_ADDR;
	
	//unsigned char *readBuffer;//,buff[16],*pBuff	
	
} DLA_PUBLIC;


DLA_PUBLIC gDla;


GunShm *gpDlaGunShm;
IFSF_SHM *gpDlaIfsf_Shm;

//static char _SERVER_IP[16]; /*服务器IP地址*/
//static char _STATION_NO[11]; /*站点编号10位数字(省2市2站6)*/
//static char _STATION_NO_BCD[5]; /*站点编号10位数字(省2市2站6)*/
//static int _TLG_SEND_INTERVAL; /*液位仪上传时间间隔*/
//static int _REALTIME_FLAG; //实时数据上传标志,0不上传,1上传
//static int _SOCKTCP,_SOCKUDP;//_SOCKTCP用于交易数据上传,_SOCKUDP用于实时数据上传
//static struct sockaddr_in _SERVER_UDP_ADDR;



//--------------------------------------------------------------------------------
//日志,调用前必须初始化日志级别_SetLogLevel和日志文件句柄_SetLogFd
//------------------------------------------------------------------------------
#define DLA_DEFAULT_LOG_LEVEL 1                           //默认日志级别
#define DLA_MAX_VARIABLE_PARA_LEN 1024 
#define DLA_MAX_FILE_NAME_LEN 1024 
static char gsDlaFileName[DLA_MAX_FILE_NAME_LEN + 1];	/*记录调试信息的文件名*/
static int giDlaLine;								/*记录调试信息的行*/
 //自定义日志级别,0:什么也不做;1:只打印,不记录;2:打印且记录;
static int giDlaLogLevel;                                             //日志调试级别
static int giDlaFd;                                                       //日志文件句柄


//自定义分级文件日志,见__LevelLog.
#define Dla_LevelLog Dla_SetFileAndLine( __FILE__, __LINE__ ), Dla__LevelLog
#define Dla_FileLog Dla_SetFileAndLine( __FILE__, __LINE__ ),Dla__FileLog

void Dla_SetLogLevel( const int loglevel )
{
	giDlaLogLevel = loglevel;
	if( (giDlaLogLevel !=0) ||(giDlaLogLevel !=1) || (giDlaLogLevel !=2)   ){
		giDlaLogLevel = DLA_DEFAULT_LOG_LEVEL;
	}
}
void Dla_SetLogFd( const int fd )
{
	giDlaFd= fd;
	if(  giDlaFd <=  0){
		RunLog( " {日志文件描述符错误[%d] }\n", fd );
	}
}
void Dla_SetFileAndLine( const char *filename, int line )
{
	assert( strlen( filename ) + 1 <= sizeof( gsDlaFileName) );
	strcpy( gsDlaFileName, filename );
	giDlaLine = line;
}

void Dla_GetFileAndLine( char *fileName, int *line )
{
	strcpy( fileName, gsDlaFileName );
	*line = giDlaLine;
}


//giLogLevel=0, no log;giLogLevel=1, only print log info; giLogLevel = 2, print log info and write log info to file 
void Dla__LevelLog(const char *fmt,...){//const int loglevel, const int fd, 
	va_list argptr;
	char sMessage[DLA_MAX_VARIABLE_PARA_LEN + 1];
	char fileName[DLA_MAX_FILE_NAME_LEN + 1];	/*记录调试信息的文件名*/
	int line;								/*记录调试信息的行*/
	int level = giDlaLogLevel;
	
	if(level == 0 ){
		return;
	}
	Dla_GetFileAndLine(fileName, &line);
	sprintf(sMessage, "%s:%d	", fileName, line);
	sprintf(sMessage+strlen(sMessage), "pid = [%d]", getpid());

	va_start(argptr, fmt);
	vsprintf(sMessage+strlen(sMessage),fmt, argptr);
	va_end( argptr );

	
	if( (giDlaLogLevel !=0) ||(giDlaLogLevel !=1) || (giDlaLogLevel !=2)   ){
		level = DLA_DEFAULT_LOG_LEVEL;
	}
	if(level > 0 ){
		//printf( " {%s }\n", sMessage );
	}
	if(level == 2){
		if(giDlaFd <=0 )	/*日志文件未打开?*/
		{
			RunLog( " {日志文件描述符错误[%d] }\n", giDlaFd );
			return;
		}
		write( giDlaFd, sMessage, strlen(sMessage) );
		write( giDlaFd, "\n", strlen("\n") );
	}
	return;
}
void Dla__FileLog(const char *fmt,...){//const int loglevel, const int fd, 
	va_list argptr;
	char sMessage[DLA_MAX_VARIABLE_PARA_LEN + 1];
	char fileName[DLA_MAX_FILE_NAME_LEN + 1];	/*记录调试信息的文件名*/
	int line;								/*记录调试信息的行*/
	
	Dla_GetFileAndLine(fileName, &line);
	sprintf(sMessage, "%s:%d	", fileName, line);
	sprintf(sMessage+strlen(sMessage), "pid = [%d]", getpid());

	va_start(argptr, fmt);
	vsprintf(sMessage+strlen(sMessage),fmt, argptr);
	va_end( argptr );

	printf( " {%s }\n", sMessage );
	if(giDlaFd <=0 )	/*日志文件未打开?*/
	{
		RunLog( " {日志文件描述符错误[%d] }\n", giDlaFd );
		return;
	}
	
	write( giDlaFd, sMessage, strlen(sMessage) );
	write( giDlaFd, "\n", strlen("\n") );
	
	return;
}
int Dla_InitLog(int loglevel){
	int fd;
    	 if( ( fd = open( DLA_LOG_FILE, O_WRONLY|O_CREAT|O_TRUNC, 00644 ) ) <= 0 ) {
		RunLog( "打开日志文件[%s]失败\n", DLA_LOG_FILE );
		return -1;
	}
	giDlaFd = fd;
	Dla_SetLogLevel(loglevel);
	return 1;
}
//------------------------------------------------

//-----------------------------------------------------------------------
//公用函数
//-----------------------------------------------------------------------
#define DLA_TCP_MAX_SIZE 1280
#define DLA_MAX_ORDER_LEN		128
int  Dla_Long2Hex( long Num, char *buf, int len )
{
	int i;
	long base = 256;

	if( len < 1 )
		return -1;

	for( i = 0; i < len; i++ )
	{
		buf[len-i-1]= Num % base;  //big end mode
		Num = Num / base;
	}
	return 0;
}
//BCD有数据的长度不得超过5个字节，否则就转换不成功
//只考虑Bin8所占字节数,不考虑小数位数,全部按照整数处理
int Dla_Bin8Bcd2Hex( unsigned char *bin8bcd, int lenb, unsigned char *hex, int lenh )
{
	unsigned char a[16];
	const unsigned char max_bcd[] = { 0x42, 0x94, 0x96, 0x72, 0x95, 0x00 };
	unsigned long b;
	int ret,k,i,first_flag,x;//

	//printf("转换数据:");
	//for(i=0;i<lenb;i++)	printf("%02x",bin8bcd[i]);
	//printf("\n");

	/*超出unsigned long 的界限*/
	k=lenb-6;
	i=0;
	while(k-i>0){
		if(0 !=bin8bcd[i+1]){
			//printf("转换数据:0 !=bin8bcd[%d]\n",i+1);
			return -3;
		}
		i++;
	}
	if( lenb >= 6 && memcmp( bin8bcd+1+k, max_bcd,  5) > 0 )
	{
		//("转换数据: lenb >= 6 && memcmp( bin8bcd+1+k, max_bcd,  5) > 0 \n");
		return -2;
	}

	memset( a, 0, sizeof(a) );
	first_flag =1;
	k=0;
	for( i=0;i<lenb-1;i++ ){
		if( (1 == first_flag)&&(0 == bin8bcd[i+1]) ){
			k=k+2;
			continue;
		}			
		x= (bin8bcd[i+1]&0xf0)>>4;
		if( (1 == first_flag)&&(0 == x) ){
			k =k+1;
		}else if( x >= 0 && x <= 9 )
			a[2*i-k]= x+'0' ;
		else{
			//printf("转换数据: 不是BCD数据 1\n");
			return -1;
		}
		x= bin8bcd[i+1]&0x0f;
		if( x >= 0 && x <=9 )
			a[2*i+1-k]= x+'0' ;
		else{
			//printf("转换数据: 不是BCD数据 2\n");
			return -1;
		}
		first_flag = 0;
	}	
	b = atol( a );
	//printf("a=[%s],b=[%d],",a,b);
	ret =  Dla_Long2Hex( b, hex, lenh );
	//for(i=0;i<lenh;i++)	printf("%02x",hex[i]);
	//printf("\n");
	return ret;
}
int Dla_Bcd2Hex( unsigned char *bcd, int lenb, unsigned char *hex, int lenh )
{
	unsigned char a[16];
	const unsigned char max_bcd[] = { 0x42, 0x94, 0x96, 0x72, 0x95, 0x00 };
	unsigned long b;
	int x, ret,k,i,first_flag;//

	/*超出unsigned long 的界限*/
	k=lenb-5;
	i=0;
	while(k-i>0){
		if(0 !=bcd[i]){
			return -3;
		}
		i++;
	}
	if( lenb >= 5 && memcmp( bcd+k, max_bcd,  5) > 0 )
	{
		return -2;
	}

	memset( a, 0, sizeof(a) );
	first_flag =1;
	k=0;
	for( i=0;i<lenb;i++ ){
		if( (1 == first_flag)&&(0 == bcd[i]) ){
			k=k+2;
			continue;
		}			
		x= (bcd[i]&0xf0)>>4;
		if( (1 == first_flag)&&(0 == x) ){
			k =k+1;
		}else if( x >= 0 && x <=9 )
			a[2*i-k]= x+'0' ;
		else
			return -1;
		x= bcd[i]&0x0f;
		if( x >= 0 && x <=9 )
			a[2*i+1-k]= x+'0' ;
		else
			return -1;
		first_flag = 0;
	}	
	b = atol( a );
	ret =  Dla_Long2Hex( b, hex, lenh );	
	return ret;
}
unsigned char* Dla_Asc2Hex( unsigned char *cmd, int cmdlen )
{
	static char retcmd[DLA_MAX_ORDER_LEN*2+1];
	int	i;

	memset( retcmd, 0, sizeof(retcmd) );
	for( i = 0; i < cmdlen; i++ ) {
		sprintf(retcmd + i * 2, "%02x", cmd[i]);
	}

	return retcmd;
}
//获取本机IP,返回本机IP地址，返回-1失败，解决DHCP问题
static unsigned  long  Dla_GetLocalIP(){//参见tcp.h和tcp.c文件中有,可以选择一个.
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


/*
 * func: 获取广播地址
 */
static unsigned long  Dla_GetBroadCast()
{	
	int  MAXINTERFACES = 16;
	int i, fd, intrface;
 	struct ifreq buf[MAXINTERFACES]; ///if.h
	struct ifconf ifc; ///if.h
	char *lipaddr;
	char *lnetmask;
	long ip;

	ip = -1;

	if ((fd = socket (AF_INET, SOCK_DGRAM, 0)) >= 0) //socket.h
	{		
		ifc.ifc_len = sizeof(buf);
 		ifc.ifc_buf = (caddr_t) buf;
 		if (!ioctl (fd, SIOCGIFCONF, (char *) &ifc)) //ioctl.h
		{			
 			intrface = ifc.ifc_len / sizeof (struct ifreq); 
			//i = 0 ;
			while (intrface-- >0 )//intrface-- > 0
 			{ 	
 				i = intrface;
 				if (!(ioctl (fd, SIOCGIFBRDADDR, (char *) &buf[intrface])) )
 				{			
 				       close (fd); 					
					ip= inet_addr( inet_ntoa( ((struct sockaddr_in*)(&buf[intrface].ifr_addr))->sin_addr));
					return ip;
				}
           		}

		}
 		close (fd);
 	}
	return 0;		
}

int Dla_TcpServInit( int port )//const char *sPortName, int backLog
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
		RunLog( "错误号:%d", errno );
		return -1;
	}

	if( listen( sockFd, backLog ) < 0 )
	{
		RunLog( "错误号:%d",errno );
		return -1;
	}

	return sockFd;
}

int Dla_TcpAccept( int sock )// , SockPair *pSockPair
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

	RunLog( "错误号:%d",errno );
	return -1;
}
/*当nTimeOut设置为TIME_INFINITE或者设置的时间比系统缺省时间长时,
 connect在完成返回前都只等待系统缺省时间*/
int Dla_TcpConnect( const char *sIPName,int Ports , int nTimeOut )//const char *sPortName
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
		RptLibErr( TCP_ILL_IP );
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
		RunLog("connect TIME_INFINITE ret[%d],error[%s],IP[%s], port[%d],port_ns[%d]\n",ret,strerror(errno),sIPName,Ports,port);
	}
	else
	{
		OldAction = SetAlrm( nTimeOut, NULL );  /*如果在alarm调用后,connect调用前超时,则connect等待时间为缺省时间*/
		ret = connect( sock, (struct sockaddr *)&serverAddr, sizeof( serverAddr ) );
		RunLog("connect secend ret[%d],sock[%d],error[%s],IP[%s], port[%d],port_ns[%d]\n",ret, sock,strerror(errno),sIPName,Ports,port);
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
static ssize_t Dla_TcpSendDataInTime( int fd, const char *s, size_t len,  int timeout )
{
#if 0
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
	
	Dla_LevelLog("before WirteLengthInTime -------------");
	n = WriteByLengthInTime( fd, t, sendlen, timeout );
	Dla_LevelLog("after WirteLengthInTime -------------");
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
	
	//Dla_LevelLog("before WirteLengthInTime -------------");	
	n = WriteByLengthInTime( fd, t, sendlen, timeout );
	//Dla_LevelLog("after WirteLengthInTime -----------length=[%d]",n);
	
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

static ssize_t Dla_TcpRecvDataInTime( int fd, char *s, size_t *buflen,  int timeout ) //size_t headlen,
{
	ssize_t	recvlen;
	size_t	len;
	//size_t headlen =8;
	//char	head[TCP_MAX_HEAD_LEN + 1];
	char	buf[DLA_TCP_MAX_SIZE];
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
		RunLog("收到的长度[%d]小于预期收到的长度[%d],值[%s]\n", n,len,Dla_Asc2Hex(t, len));
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

static int Dla_ReadALine( FILE *f, char *s, int MaxLen ){
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
static int   Dla_GetWorkDir( char *workdir) {
	FILE *f;
	int i, r,trytime = 3;
	char s[96] ;

	if(  workdir == NULL ){		
		return -1;
	}	
	f = fopen( RUN_WORK_DIR_FILE, "rt" );
	if( ! f ){	// == NULL
		memcpy( workdir,DEFAULT_WORK_DIR_4DLA, strlen(DEFAULT_WORK_DIR_4DLA) );
		return 0;
	}	
	bzero(s, sizeof(s));
	for(i=0; i<trytime; i++){
		r = Dla_ReadALine( f, s, sizeof(s) );		
		if( s[0] == '/' )
			break;
	}
	fclose( f );
	if(s[0] == '/' ){
		memcpy(workdir,s, strlen(s));	
		return 0;
	}
	else{
		memcpy( workdir,DEFAULT_WORK_DIR_4DLA, strlen(DEFAULT_WORK_DIR_4DLA) );
		return 0;
	}
}

static int Dla_IsFileExist(const char * sFileName ){
	int ret;
	extern int errno;
	if( sFileName == NULL || *sFileName == 0 )
		return -1;
	ret = access( sFileName, F_OK );
	//cgi_ok("in  IsTheFileExist after call stat");
	//exit( 0 );
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
static char *Dla_TrimAll( char *buf )
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
static char *Dla_split(char *s, char stop){
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


#define DLA_INI_FIELD_LEN 63
#define DLA_INI_MAX_CNT 30
typedef struct dla_ini_name_value{
char name[DLA_INI_FIELD_LEN + 1];
char value[DLA_INI_FIELD_LEN + 1];
}DLA_INI_NAME_VALUE;
static DLA_INI_NAME_VALUE dla_ini[DLA_INI_MAX_CNT];

//first read ini file by readIni fuction , then change or add ini data, last write ini data to ini file by writeIni fuction.
static int Dla_readIni(char *fileName){

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
	ret = Dla_IsFileExist(fileName);
	if(ret != 1){
		char pbuff[256];
		bzero(pbuff,sizeof(pbuff));
		sprintf( pbuff, "file[%s] not exist in readIni  error!! ", fileName );
		RunLog(pbuff);
		exit(1);
		return -2;
	}
	f = fopen( fileName, "rt" );
	if( !f  ){	
		char pbuff[256];
		bzero(pbuff,sizeof(pbuff));
		sprintf( pbuff, "file[%s] open error in readIni!! ", fileName );
		RunLog(pbuff);
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
	
	
	bzero(&(dla_ini[0].name), sizeof(DLA_INI_NAME_VALUE)*DLA_INI_MAX_CNT);	
	i = 0;
	while (my_data[0] !='\0'){
		tmp = Dla_split(my_data, '=');// split data to get field name.
		bzero(&(dla_ini[i].name), sizeof(dla_ini[i].name));
		strcpy(dla_ini[i].name, Dla_TrimAll(tmp) );
		tmp = Dla_split(my_data, '\n');// split data to get value.
		bzero(&(dla_ini[i].value), sizeof(dla_ini[i].value));
		strcpy(dla_ini[i].value, Dla_TrimAll(tmp));
		i ++;
	}
	//free(my_data);
	return i--;	
}

static int Dla_writeIni(const char *fileName){
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
		RunLog("file open error in writeIni!!");
		exit(1);
		return -1;
	}
	for(i=0;i<DLA_INI_MAX_CNT;i++){
		if( (dla_ini[i].name[0] !=0)&&(dla_ini[i].value[0] !=0) ){
			bzero(temp,sizeof(temp));
			sprintf(temp,"%s\t = %s  \n", dla_ini[i].name,  dla_ini[i].value);
			write( fd, temp, strlen(temp) );
		}
	}	
	close(fd);
	return 0;
}
static int Dla_addIni(const char *name, const char *value){
	int i,n;
	
	if(  (NULL== name)||(NULL== value)  ){
		return -1;
	}	
	n = strlen(name);
	for(i=0;i<DLA_INI_MAX_CNT;i++){
		if( memcmp(dla_ini[i].name,name,n)  == 0 ){
			return -1;
			break;
		}
		else if(dla_ini[i].name[0] == 0){
			bzero( dla_ini[i].name, sizeof(dla_ini[i].name)  );
			bzero( dla_ini[i].value, sizeof(dla_ini[i].value)  );			
			memcpy(dla_ini[i].name, name, strlen(name) );
			memcpy(dla_ini[i].value, value, strlen(value) );
			return 0;
		}
	}
		
		return -2;
}
static int Dla_changeIni(const char *name, const char *value){
	int i,n;
	int ret;
	
	if(  (NULL== name)||(NULL== value)  ){
		return -1;
	}	
	n = strlen(name);
	for(i=0;i<DLA_INI_MAX_CNT;i++){
		if( memcmp(dla_ini[i].name,name,n)  == 0 ){
			bzero( dla_ini[i].value, sizeof(dla_ini[i].value)  );
			memcpy(dla_ini[i].value, value, strlen(value) );
			break;
		}
	}
	if(i == DLA_INI_MAX_CNT){
		ret = Dla_addIni(name, value);
		if(ret < 0){
			return -2;
		}
	}		
	else
		return 0;
}


char *Dla_upper(char *str){
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
int Dla_InitConfig(){
	int ret,i,iniFlag;
	char configFile[256];
	
	bzero(configFile,sizeof(configFile));
	memcpy(configFile,DLA_CONFIG_FILE,sizeof(DLA_CONFIG_FILE));
	ret = Dla_readIni(configFile);
	if(ret < 0){
		RunLog("readIni error in _InitServerIP");
		return -1;
	}
	iniFlag = 0;
	for(i=0;i<ret;i++){
		if( memcmp(dla_ini[i].name,"SERVER_IP",9) == 0){
			memcpy(gDla._SERVER_IP, dla_ini[i].value,sizeof(gDla._SERVER_IP ) );//sizeof(dla_ini[i].value
			RunLog("SERVER_IP[%s]",gDla._SERVER_IP);
			iniFlag += 1;
		}else if( memcmp(dla_ini[i].name,"SERVER_TCP_PORT",15) == 0 ){
			gDla._SERVER_TCP_PORT=atoi(dla_ini[i].value);
			RunLog("SERVER_TCP_PORT[%d]",gDla._SERVER_TCP_PORT);
			iniFlag += 1;
		}else if( memcmp(dla_ini[i].name,"SERVER_UDP_PORT",15) == 0 ){
			gDla._SERVER_UDP_PORT=atoi(dla_ini[i].value);
			RunLog("SERVER_UDP_PORT[%d]",gDla._SERVER_UDP_PORT);
			iniFlag += 1;
			//memcpy(gDla._SERVER_UDP_PORT, dla_ini[2].value, sizeof(dla_ini[2].value) );
		}else{
			RunLog("配置数据出错,  不是预期的三个配置,名字是[%s],值是[%s]",dla_ini[i].name,dla_ini[i].value);
			//iniFlag = -1;
		}
	}
	if(iniFlag != 3 ) 
		return -1;
	else 
		return 0;
 }


 
//-------------------------------------------





//------------------------------------------------
//共享内存处理
//------------------------------------------------
static char Dla_gKeyFile[MAX_PATH_NAME_LENGTH]="";

static int Dla_InitTheKeyFile() 
{
	char	*pFilePath, FilePath[]=DEFAULT_WORK_DIR_4DLA;
	char temp[128];
	int ret;
	
	//取keyfile文件路径//	
	pFilePath = getenv( "WORK_DIR" );	//WORK_DIR 工作目录的环境变量
	if( NULL ==  pFilePath){
		bzero(temp, sizeof(temp));
		ret = Dla_GetWorkDir(temp);
		if(ret >= 0 ){
			pFilePath = temp;
		}
		else{
			pFilePath = FilePath;
		}
		//cgi_error( "未定义环境变量WORK_DIR" );
		//exit(1);
		//return -1;
	}
	if((NULL ==  pFilePath) ||(pFilePath[0] !=  '/') ){
		pFilePath = FilePath;
		}
	sprintf( Dla_gKeyFile, "%s/etc/keyfile.cfg", pFilePath );
	//cgi_ok(gKeyFile);
	//exit(0);
	if( Dla_IsFileExist( Dla_gKeyFile ) == 1 )
		return 0;

	return -1;
}

static key_t Dla_GetTheKey( int index )
{
	key_t key;
	if( Dla_gKeyFile[0] == 0 ){
		printf( "gKeyFile[0] == 0" );
		exit(1);
	}
	key = ftok( Dla_gKeyFile, index );
	if ( key == (key_t)(-1) ){
		printf( " key == (key_t)(-1)" );
		exit(1);
	}
	return key;
}


static char *Dla_AttachTheShm( int index )
{
	int ShmId;
	char *p;

	ShmId = shmget( Dla_GetTheKey(index), 0, 0 );
	if( ShmId == -1 )
	{
		return NULL;
	}
	p = (char *)shmat( ShmId, NULL, 0 );
	if( p == (char*) -1 )
	{
		return NULL;
	}
	else
		return p;
}

static int Dla_DetachTheShm( char **p )
{
	int r;

	if( *p == NULL )
	{
		return -1;
	}
	r = shmdt( *p );
	if( r < 0 )
	{
		return -1;
	}
	*p = NULL;

	return 0;
}


 int  Dla_ReadTransData( unsigned char *resultBuff){
 	//#define _MAX_CHANNEL_CNT 64
	//#define _MAX_TR_NB_PER_DISPENSER 256
	#define READ_TIMES 1000
	#define READ_DATA_TIMES 2
	#define DLA_MAX_GUN_CNT 64 //一个站的最大枪数
	
	//unsigned char onlineFlag[2] ={0xaa,0x55};
	static unsigned char sendBuffer[512] ;//,buff[16],*pBuff
	unsigned char readBuffer[512] ;//,buff[16],*pBuff
	unsigned char  dla_node, dla_fp,moved_gun;
	int  ret, i, j, dataLen,readLength,len=sizeof(DLA_BIG_GUN);//, onlineFlagLen   = 2
	extern int errno;
	DLA_FP_TR_DATA  *pFp_tr_data;
	static unsigned char  gun_flag[DLA_MAX_GUN_CNT+1];//提枪标志
	
	/*if(  (NULL == ppIfsf_Shm) || (NULL == ppGunShm) ){
		_FileLog("_ReadTransData因共享内存空退出.");
		return -1;
	}*/	
	//*sendType = 0;
	if(gDla.fd_fifo<=0){
		gDla.fd_fifo = open(DLA_FIFO, O_RDONLY | O_NONBLOCK);
		if(gDla.fd_fifo <= 0){
			RunLog("FIFO只读打开出错.");
			close(gDla.fd_fifo);
			return -1;
		}
		else{
			//RunLog( "gDla.fd_fifo读打开完毕");
		}
	}	
	while(1){
		bzero(readBuffer, sizeof(readBuffer));
		dataLen =-1;
		readLength =-1;
		ret = 0;
		i = 0;
		while(1){		
			//dataLen = pGunShm->CDLen;
			i++;	
			if(i>=READ_TIMES){
				//RunLog( "空读gDla.fd_fifo[%d]次,没有数据",READ_TIMES );
				//if( (ret !=4 )|| (dataLen <=0) )
				//	_LevelLog("FIFO读取出的数据的长度值出错.ret[%d],dataLen[%d]",ret,dataLen);
				if(readLength != dataLen)
					RunLog("FIFO读取出的数据的长度出错.readLength[%d],dataLen[%d]",readLength,dataLen);
				i = 0;
			}
			//_LevelLog( "读gDla.fd_fifo开始");
			dataLen = -1;
			ret = read(gDla.fd_fifo, (char *)&dataLen, 4);//dataLen = 
			if( (ret !=4 )|| (dataLen <=0) ){
				dataLen = -1;
				//RunLog( "读gDla.fd_fifo结束,没有数据,长度[%d]",dataLen );
				continue;
			}
			if(dataLen>=512){
				RunLog("FIFO读取出的数据的长度出错.readLength[%d],dataLen[%d]",readLength,dataLen);
				continue;
			}
			for(j=0;j< READ_DATA_TIMES;j++){
				readLength = -1;
				readLength = read(gDla.fd_fifo, readBuffer, dataLen);
				RunLog( "读gDla.fd_fifo结束[%s]",Dla_Asc2Hex(readBuffer,32) );
				if(readLength != dataLen){
					RunLog( "读gDla.fd_fifo错误,readLength[%d], dataLen[%d],errno[%d]",readLength ,dataLen,errno);
					readLength = -1;
					continue;
				}else{
					//_LevelLog("FIFO读取出了数据.readLength[%d],dataLen[%d]",readLength,dataLen);
					break;
				}
			}
			if((readLength == dataLen)&&(readLength != -1)){
				if(readLength<=512){
					//_LevelLog( "读gDla.fd_fifo结束,开始复制数据");
					//memcpy(gDla.readBuffer,readBuffer,readLength);
					//RunLog( "读gDla.fd_fifo正常结束[%s]",Dla_Asc2Hex(readBuffer,readLength) );
				}else{
					RunLog( "读gDla.fd_fifo失败,长度大于512[%s]",Dla_Asc2Hex(readBuffer,128) );
					}
				
				break;
			}
		}	
		
		
		//处理数据		
		ret = -1;
		//Dla_LevelLog("FIFO读处理数据: 总枪数[%d]",gDla.totalGun);
		if( (readBuffer[0] == 0xff )  &&(readLength > 40 ) ){//Transaction data
			//RunLog("FIFO读处理数据:");
			
			pFp_tr_data =(DLA_FP_TR_DATA  *)&readBuffer[1];
			dla_node = pFp_tr_data->node;
			dla_fp = pFp_tr_data->fp_ad-0x20;
			moved_gun = pFp_tr_data->TR_Log_Noz;
			
			sendBuffer[0] = (unsigned char) 0x08;//交易数据标志
			sendBuffer[1] = 0;//数据长度
			RunLog("FIFO 交易数据:节点号[%d]面板号[%d]",dla_node,dla_fp);
			for(j=0;j<gDla.totalGun;j++){
				//Dla_LevelLog("gDla.bigGun[%d]节点号[%d]面板号[%d]",j,gDla.bigGun[j].node,gDla.bigGun[j].fp);
				if( (gDla.bigGun[j].node==dla_node) &&(gDla.bigGun[j].fp==dla_fp) ){
					if(gDla.bigGun[j].moved_gun_no==moved_gun){
						sendBuffer[2] = gDla.bigGun[j].big_gun_no;
						gun_flag[gDla.bigGun[j].big_gun_no] = 0;//挂枪了
						break;
					}else{
						RunLog("FIFO读处理数据:节点号[%d]面板号[%d]的 动枪号[%d]不等于配置动枪号[%d],", \
							dla_node,dla_fp, moved_gun, gDla.bigGun[j].moved_gun_no);
					}
				}
			}
			if( (j == gDla.totalGun)||(0 == sendBuffer[2]) ){
				//Dla_LevelLog("FIFO读处理数据出错:0 == sendBuffer[2],赋值大排序枪号1");
				//sendBuffer[2] = 1; //gDla.bigGun[0].big_gun_no
				RunLog("FIFO读处理数据出错:大排序枪号找不到或者0,异常返回");
				return -1;
			}
			sendBuffer[3] = 0;
			sendBuffer[4] = 0;
			memcpy((char *)&(sendBuffer[5]), pFp_tr_data->TR_Seq_Nb,2);//BCD到HEX转换吗,此处不用，不转换
			
			ret = Dla_Bin8Bcd2Hex(pFp_tr_data->TR_Unit_Price, 4, &sendBuffer[7], 2);
			ret = Dla_Bin8Bcd2Hex(pFp_tr_data->TR_Volume, 5, &sendBuffer[9], 3);
			ret = Dla_Bin8Bcd2Hex(pFp_tr_data->TR_Amount, 5, &sendBuffer[12], 4);

			#if OLD_PROTOCOL	//旧的协议
				ret = Dla_Bin8Bcd2Hex(pFp_tr_data->TR_End_Total, 7, &sendBuffer[16], 4);			
				memcpy(&sendBuffer[20] ,pFp_tr_data->TR_Date,4);
				memcpy(&sendBuffer[24] ,pFp_tr_data->TR_Time,3);			
				sendBuffer[27] = 0;
				sendBuffer[28] = 0;
				gDla.msgLength =29 ;
			#else
				ret = Dla_Bin8Bcd2Hex(pFp_tr_data->TR_End_Total, 7, &sendBuffer[16], 5);			
				memcpy(&sendBuffer[21] ,pFp_tr_data->TR_Date,4);
				memcpy(&sendBuffer[25] ,pFp_tr_data->TR_Time,3);			
				sendBuffer[28] = 0;
				sendBuffer[29] = 0;
				gDla.msgLength =30 ;
			#endif
			//Dla_LevelLog("构造交易数据完毕");
			ret = 1;
			break;
		}
		else if( (readBuffer[0] == 0xf0 )  &&(readLength < 9 ) &&(readLength > 3 ) ){//0交易
			bzero(sendBuffer,sizeof(sendBuffer));
			sendBuffer[0] =0x02;
			sendBuffer[1] =0;
			dla_node =readBuffer[1];
			dla_fp = readBuffer[2]-0x20;
			moved_gun = readBuffer[3]; //动枪号,此处值0,不用
			//RunLog("FIFO读处理数据:0  交易数据");
			RunLog("FIFO  零 交易数据:节点号[%d]面板号[%d],动枪号[%d]",dla_node,dla_fp,moved_gun);
			for(j=0;j<gDla.totalGun;j++){
				//Dla_LevelLog("gDla.bigGun[%d]节点号[%d]面板号[%d]",j,gDla.bigGun[j].node,gDla.bigGun[j].fp);
				if( (gDla.bigGun[j].node==dla_node) &&(gDla.bigGun[j].fp==dla_fp) ){
					if(gun_flag[gDla.bigGun[j].big_gun_no] == 1){
						sendBuffer[2] = gDla.bigGun[j].big_gun_no;
						gun_flag[gDla.bigGun[j].big_gun_no] = 0;//挂枪了
						break;
					}else{
						Dla_LevelLog("FIFO读处理数据:节点号[%d]面板号[%d]的 大枪号[%d]没有抬枪[%d], 没有0交易,", \
							dla_node,dla_fp,  gDla.bigGun[j].big_gun_no, gun_flag[gDla.bigGun[j].big_gun_no]);
					}
				}
			}
			if( (j == gDla.totalGun)||(0 == sendBuffer[2]) ){
				//Dla_LevelLog("FIFO读处理数据出错:0 == sendBuffer[2],赋值大排序枪号1");
				//sendBuffer[2] = 1;
				RunLog("FIFO读处理数据出错:大排序枪号找不到或者0,异常返回");
				return -1;
			}
			#if OLD_PROTOCOL //旧的协议	
			gDla.msgLength = 14;
			#else		
			gDla.msgLength = 15;
			#endif
			
			//RunLog("FIFO读处理数据:构造0交易挂枪数据完毕");
			ret = 2;
			break;
		}
		else if((readBuffer[2] != 1 ) ||(readBuffer[5] != 0x80 )  ){//dispenser, unsolicited data..
			RunLog("FIFO读处理数据:  非主动数据");
			ret = -1;
			continue;
		}else if( (readBuffer[7] == 0x0e ) &&(readBuffer[10] == 100 )&&(readBuffer[14] == 4 )  ){//Realtime Data:state
		//readBuffer[7] == 0x0e代表长度;readBuffer[10] == 100主动数据;readBuffer[14] == 4 提枪
				//RunLog("FIFO读处理数据:提枪状态数据");
				//ret = MakeStateData(resultBuff);//gDla.readBuffer
				dla_node =readBuffer[3];
				dla_fp = readBuffer[9]-0x20;
				moved_gun = readBuffer[17]; //动枪号
				
				gDla.msgLength =3 ;
				sendBuffer[0] = (unsigned char) 0x01;
				sendBuffer[1] =0;
				//RunLog("FIFO提枪状态:节点号[%d]面板号[%d]",dla_node,dla_fp);
				for(j=0;j<gDla.totalGun;j++){
					if( (gDla.bigGun[j].node==dla_node) &&(gDla.bigGun[j].fp==dla_fp) ){
						if(gDla.bigGun[j].moved_gun_no==moved_gun){
							sendBuffer[2] = gDla.bigGun[j].big_gun_no;
							gun_flag[gDla.bigGun[j].big_gun_no] = 1;//体枪了
							RunLog("FIFO提枪状态:节点号[%d]面板号[%d]的大枪号[%d]",dla_node,dla_fp,gDla.bigGun[j].big_gun_no);
							break;
						}else{
							RunLog("FIFO提枪状态:节点号[%d]面板号[%d]的 动枪号[%d]不等于配置动枪号[%d],", \
								dla_node,dla_fp, moved_gun, gDla.bigGun[j].moved_gun_no);
						}
					}
				}
				if( (j == gDla.totalGun)||(0 == sendBuffer[2]) ){
					//Dla_LevelLog("FIFO读处理数据出错:0 == sendBuffer[2],赋值大排序枪号1");
					//sendBuffer[2] = 1;
					RunLog("FIFO读处理数据出错:大排序枪号找不到或者0,异常返回");
					return -1;
				}
				//Dla_LevelLog("构造提枪状态数据完毕");
				ret = 2;
				break;
		}else if( (readBuffer[0] == 2 )&&(readBuffer[2] == 1 )&&(readBuffer[10] == 102 ) ){//Realtime Data:transaction
		//readBuffer[0] == 2代表CD;readBuffer[2] == 1加油机;readBuffer[10] == 102 实时交易	
				//RunLog("FIFO读处理数据: 实时加油数据");
				dla_node =readBuffer[3];
				dla_fp = readBuffer[9]-0x20;
				moved_gun = readBuffer[28];
				
				gDla.msgLength =6 ;
				sendBuffer[0] = (unsigned char) 0x0a;
				sendBuffer[1] = (unsigned char) 0x00;//交易金额标志
				//RunLog("FIFO读处理 实时加油数据:读到节点号[%d]面板号[%d],总抢数[%d]",dla_node,dla_fp,gDla.totalGun);
				for(j=0;j<gDla.totalGun;j++){
					if( (gDla.bigGun[j].node==dla_node) &&(gDla.bigGun[j].fp==dla_fp) ){
						if(gDla.bigGun[j].moved_gun_no==moved_gun){
							sendBuffer[2] = gDla.bigGun[j].big_gun_no;
							gun_flag[gDla.bigGun[j].big_gun_no] = 1;//提枪了
							break;
						}else{
							RunLog("FIFO读处理数据:节点号[%d]面板号[%d]的 动枪号[%d]不等于配置动枪号[%d],", \
								dla_node,dla_fp, moved_gun, gDla.bigGun[j].moved_gun_no);
						}
					}
				}
				if( (j == gDla.totalGun)||(0 == sendBuffer[2]) ){
					//sendBuffer[2] = 1;
					RunLog("FIFO读处理数据出错:大排序枪号找不到或者0,异常返回");
					return -1;
				}
				ret = Dla_Bin8Bcd2Hex(&readBuffer[14], 5, &sendBuffer[3], 3);//金额
				//memcpy(&sendBuffer[3], &readBuffer[16], 3);	
				
				//处理完毕
				ret = 2;
				break;
			
		}else{
			RunLog("FIFO读处理数据:  其它数据");
			ret = 0;
			continue;
		}
	}
	//处理完毕，返回数据
	//resultBuff =sendBuffer; 
	memcpy(resultBuff, sendBuffer, gDla.msgLength);	//
	//memcpy(gDla.sendBuffer,sendBuffer,gDla.msgLength);
	//RunLog( "FIFO读处理数据完毕[%s]",Dla_Asc2Hex(resultBuff, gDla.msgLength) );
	return ret;
 }
 int Dla_SendTransData( unsigned char *sendBuff){
 	 unsigned char sPortName[5],*sIPName;	
	  unsigned char sendBuffer[64];
	 unsigned char recvBuffer[64];
	 int recvLength;
	 int i,ret,tmp, tryN = 3, timeout = 10,sendLen; 
	 
	bzero(sendBuffer,sizeof(sendBuffer));
	sendLen = gDla.msgLength;
	memcpy(sendBuffer,sendBuff, sendLen);
	ret = -1;
	RunLog("发送交易数据[%s]\n" , Dla_Asc2Hex(sendBuffer, sendLen) );
	for(i=0;i<tryN;i++){		
		ret = Dla_TcpSendDataInTime(gDla._SOCKTCP, sendBuffer,sendLen, timeout);		
		if(ret <0){//0表示发送成功
			RunLog("Dla_SendTransData发送数据 [%s]失败,返回[%d]",Dla_Asc2Hex(sendBuffer, sendLen),ret);
			if(gDla._SOCKTCP <= 0){
				sIPName = gDla._SERVER_IP;//inet_ntoa(  *((struct in_addr *)(&gpIfsf_shm->recv_hb[i].host_ip))  );
				gDla._SOCKTCP = Dla_TcpConnect(sIPName, gDla._SERVER_TCP_PORT, timeout);//sPortName(timeout+1)/2
				if (gDla._SOCKTCP <= 0 ){
					RunLog("TcpConnect连接不成功,DLA发送数据 失败");
					continue;
				}
			}
		}else{
			//RunLog("Dla_SendTransData发送数据 [%s]成功,返回[%d]",Dla_Asc2Hex(sendBuffer,sendLen),ret);
			memset(recvBuffer,0xff,sizeof(recvBuffer));
			for(i=0;i<tryN;i++){
				recvLength = 6;
				ret = Dla_TcpRecvDataInTime(gDla._SOCKTCP, recvBuffer, &recvLength, timeout);
				RunLog("Dla_SendTransData发送数据后接收确认 ,返回[%d],recvLength[%d], recvBuffer前8字节:[%s]", \
					ret, recvLength,  Dla_Asc2Hex(recvBuffer, 8) );
				if(ret ==-2){
					if( (0x30 ==recvBuffer[0])&&(0x30 ==recvBuffer[1])&&(0x30 ==recvBuffer[2]) \
					  &&(0x32 ==recvBuffer[3])&&(0x08==recvBuffer[4]) ){ 
					  	if(0x10 ==recvBuffer[5]){
							return 0;
						}
						else{
							RunLog("Dla_SendTransData接收到非10数据，不成功");							
							continue;
						}
					}
					break;
				}else if(ret < 0){				
					RunLog("Dla_SendTransData接收数据不成功");
					continue;		
				}else{
					break;
				}			
			}
			
			if( (0x30 ==recvBuffer[0])&&(0x30 ==recvBuffer[1])&&(0x30 ==recvBuffer[2]) \
				&&(0x32 ==recvBuffer[3])&&(0x08==recvBuffer[4]) ){ 
					  	if(0x10 ==recvBuffer[5]){
							return 0;
						}
						else{
							RunLog("Dla_SendTransData接收到非10数据，不成功");							
							continue;
						}
					
			}else if(ret < 0){
				RunLog("错误, 从服务器没有接收到数据");
				sleep(1);//避免发送过于频繁
				continue;//重发数据
				//return -1;			
			}else{
				RunLog("从服务器接收的返回数据错误");
				return -1;
			}
			
		}
	}
	if( (ret <= 0) ||  (0x30 !=recvBuffer[0])||(0x30 !=recvBuffer[1])||(0x30 !=recvBuffer[2])  ){
			RunLog("DLA收发交易数据 失败");
			return -1;
	}
	return 0;
 }
 int Dla_SendRealtimeData( unsigned char *sendBuffer){
	int i, ret,tryNum = 5;
	 struct sockaddr_in  _SERVER_UDP_ADDR;
	 unsigned char recvBuffer[32];
	 int recvLength;
	 bzero(&_SERVER_UDP_ADDR,sizeof(_SERVER_UDP_ADDR));
	_SERVER_UDP_ADDR.sin_family=AF_INET;
	_SERVER_UDP_ADDR.sin_addr.s_addr=inet_addr(gDla._SERVER_IP);
	_SERVER_UDP_ADDR.sin_port = htons(gDla._SERVER_UDP_PORT);
	 if( gDla._SOCKUDP<=0){
		 for(i=0;i<tryNum;i++){			
			gDla._SOCKUDP = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
			if (  gDla._SOCKUDP<=0 ){//connect(_SOCKUDP, (struct sockaddr *)_SERVER_UDP_ADDR, sizeof(_SERVER_UDP_ADDR))
				RunLog("UDP连接建立不成功,_setRealTime 失败");
				//close(_SOCKUDP); 
				gDla._SOCKUDP = -1;	
				//exit(0);
			}
			else{
				 for(i=0;i<tryNum;i++){
					ret = sendto(gDla._SOCKUDP, sendBuffer, gDla.msgLength, MSG_DONTWAIT, (struct sockaddr *)&_SERVER_UDP_ADDR, sizeof(struct sockaddr));//
					if (ret < 0) {				
						RunLog("实时数据发送失败,error:[%s]", strerror(errno));	
					}else{
						/*Dla_LevelLog("实时数据发送成功,开始接收确认");
						recvLength = 0;//30
						bzero(recvBuffer,sizeof(recvBuffer));
						for(i=0;i<=tryNum;i++){
							//Dla_LevelLog("接收实时数据发送返回值");
							ret = recvfrom(gDla._SOCKUDP, recvBuffer,sizeof(recvBuffer), 0,  \
								(struct sockaddr *)&_SERVER_UDP_ADDR,  &recvLength);
							if((ret <=0)||(recvLength<=0)){				
								Dla_LevelLog("Dla_SendRealtimeData接收数据不成功");
								continue;		
							}
							if((recvBuffer[0] != 0x01)||(recvBuffer[0] != 0x02)||(recvBuffer[0] != 0x0a)){
							   	Dla_LevelLog("服务器接收的数据错误");
								continue;
							}else{
								Dla_LevelLog("实时数据发送成功并获得确认");
								break;
							}
						}*/
					       RunLog("实时数据发送成功");
						break;
					}
				}
				
				break;
			}
		}		 		
	}else{
	 	for(i=0;i<tryNum;i++){
			ret = sendto(gDla._SOCKUDP, sendBuffer, gDla.msgLength, MSG_DONTWAIT, (struct sockaddr *)&_SERVER_UDP_ADDR, sizeof(struct sockaddr));//
			if (ret < 0) {				
				RunLog("实时数据发送失败,error:[%s]", strerror(errno));	
			}else{
				/*Dla_LevelLog("实时数据发送成功,开始接收确认");
				recvLength = 0;//30
				bzero(recvBuffer,sizeof(recvBuffer));
				for(i=0;i<=tryNum;i++){
					//Dla_LevelLog("接收实时数据发送返回值");
					ret = recvfrom(gDla._SOCKUDP, recvBuffer,sizeof(recvBuffer), MSG_DONTWAIT,  \
						(struct sockaddr *)&_SERVER_UDP_ADDR,  &recvLength);
					if((ret <=0)||(recvLength<=0)){				
						Dla_LevelLog("_TcpRecvDataInTime接收数据不成功");
						continue;		
					}
					if((recvBuffer[0] != 0x01)||(recvBuffer[0] != 0x02)||(recvBuffer[0] != 0x0a)){
					   	Dla_LevelLog("服务器接收的数据错误");
						continue;
					}else{
						Dla_LevelLog("实时数据发送成功并获得确认");
						break;
					}
				}*/
				RunLog("实时数据发送成功");
				break;
			}
		}
		
		
	}	
 }

int main( int argc, char* argv[] )
{
	#define GUN_MAX_NUM 256 //最多256把枪
	int i, j, n,k,len,ret,fd;//,pid,isChild
	unsigned char sPortName[5],*sIPName;	
 	int    tmp, timeout =5, tryn=10;
	//struct sockaddr_in  _SERVER_UDP_ADDR;
	unsigned char *resultBuff;
	unsigned char sendBuff[512] ;//,buff[16],*pBuff
	unsigned char realtimeBuff[32] ;//,buff[16],*pBuff
	//static unsigned char readBuff[512] ;//,buff[16],*pBuff
	DLA_BIG_GUN bigGun[GUN_MAX_NUM];	
	DLA_BIG_GUN tempGun;//排序和计算动枪号用
	unsigned char buff[16];
	DLA_GUN_CONFIG *pGunConfig;

	if (argc == 2) {
	       if (strcmp(argv[1], "-v") == 0) {
			printf( "Ifsf_main. PCD version 1.01\n");
	       } 
		return 0;
	}
	//初始化日志
	ret = InitLog();
	if(ret < 0){
		exit(0);  
	}

	
       RunLog("开始运行二配程序......\n");
	
	sleep(30);//等待IFSF程序启动,设置30秒
	bzero(sendBuff, sizeof(sendBuff));
	bzero( (unsigned char *)&bigGun, sizeof(DLA_BIG_GUN)*GUN_MAX_NUM);
	gDla.sendBuffer = sendBuff;	
	//printf("初始化bigGun为0.");
	//bzero((unsigned char *)bigGun, GUN_MAX_NUM*sizeof(DLA_BIG_GUN));
	gDla.bigGun = bigGun;


	
	//初始化大排序枪号   //加油机节点号面板和大排序枪号关系数据
	RunLog(" 初始化大排序枪号.");
	fd = open( DLA_DSP_CFG_FILE, O_RDONLY );
	if(fd <=0){
		RunLog("  打开FCC加油机配置文件[%s]出错.");
		exit(0);
	}
	k = 0; // gun number
	len = sizeof(DLA_BIG_GUN);
	while (1) {
		n = read( fd, buff, sizeof(DLA_GUN_CONFIG) );
		if( n == 0 ) {	//读到了文件尾
			close(fd);
			break;
		} else if( n < 0 ) {
			RunLog( "读取 dsp.cfg 配置文件出错" );
			close(fd);
			return -1;
		}
		//RunLog("第[%d]次读取到dsp.cfg 数据[%d]个字符\n",i+1, n);
		if( n != sizeof(DLA_GUN_CONFIG) ) {
			RunLog( "dsp.cfg 配置信息有误" );
			close( fd );
			return -1;
		}
		pGunConfig = (DLA_GUN_CONFIG *) (&buff[0]);
		bigGun[k].node= pGunConfig->node;
		bigGun[k].fp = pGunConfig->logfp;
		//bigGun[k].physical_gun_no = pGunConfig->physical_gun_no;
		bigGun[k].logical_gun_no = pGunConfig->logical_gun_no;		
		RunLog( "记录号[%d],节点号[%d], 面板号[%d],面板排序枪号[%d]" ,k, \
				bigGun[k].node, bigGun[k].fp, bigGun[k].logical_gun_no );
		//gunBuffer[k*len+0] = pGunConfig->node;
		//gunBuffer[k*len+1] = pGunConfig->phyfp;
		//gunBuffer[k*len+2] = pGunConfig->physical_gun_no;
		//close(fd);
		//bigGun[k].big_gun_no = pGunConfig->logical_gun_no;	
		//if(bigGun[k].big_gun_no <= 0){
		//	Dla_FileLog( "有误,第[%d]个记录的phygun[%d],大排序枪号lgun[%d]" ,k, \
		//		pGunConfig->physical_gun_no, pGunConfig->logical_gun_no);
		//	bigGun[k].big_gun_no = 1;
		//}
		k++;// gun number ++
	}
	gDla.totalGun = k;	
	RunLog( "共有油枪[%d]把" ,gDla.totalGun);
	
	//按照节点号,面板号,面板枪号排序
	for(i=1;i<gDla.totalGun;i++){
		for(j=0;j<gDla.totalGun-i;j++){
			//if( (bigGun[j].node==0)&&(bigGun[j].fp == 0)&&(bigGun[j].logical_gun_no==0) ) continue;
			if(  (bigGun[j].node>bigGun[j+1].node) || ( (bigGun[j].node==bigGun[j+1].node)&&(bigGun[j].fp > bigGun[j+1].fp)  ) || \
			( (bigGun[j].node==bigGun[j+1].node)&&(bigGun[j].fp == bigGun[j+1].fp)&&(bigGun[j].logical_gun_no> bigGun[j+1].logical_gun_no) )  ){
				tempGun.node = bigGun[j].node;
				tempGun.fp = bigGun[j].fp;
				tempGun.logical_gun_no= bigGun[j].logical_gun_no;
				
				bigGun[j].node = bigGun[j+1].node;
				bigGun[j].fp = bigGun[j+1].fp;
				bigGun[j].logical_gun_no= bigGun[j+1].logical_gun_no;
				
				bigGun[j+1].node = tempGun.node;
				bigGun[j+1].fp = tempGun.fp;
				bigGun[j+1].logical_gun_no= tempGun.logical_gun_no;
				//printf("[%d]<-->[%d]  ",j+1,j);
				//memcpy((unsigned char *)&tempGun,(unsigned char *)&bigGun[j],sizeof(DLA_BIG_GUN));
				//memcpy((unsigned char *)&bigGun[j],(unsigned char *)&bigGun[j+1],sizeof(DLA_BIG_GUN));
				//memcpy((unsigned char *)&bigGun[j+1],(unsigned char *)&tempGun,sizeof(DLA_BIG_GUN));
			}
		}
	}
	//printf("\n");
	
	
	//赋值大排序枪号
	for(i=0;i<gDla.totalGun;i++){
		bigGun[i].big_gun_no = i+1;
	}
	
	//计算IFSF动枪号,假定每个面板最多8把枪
	/*k=0;
	tempGun.node = bigGun[0].node;
	tempGun.fp = bigGun[0].fp;*/
	
	for(i=0;i<gDla.totalGun;i++){
		/*if( (tempGun.node == bigGun[i].node)&&(tempGun.fp == bigGun[i].fp) ){
			k++;
		}else{
			tempGun.node = bigGun[0].node;
			tempGun.fp = bigGun[0].fp;
			k=1;
		}	
		bigGun[i].moved_gun_no = 1;*/
		
		switch(bigGun[i].logical_gun_no){
			case 1:bigGun[i].moved_gun_no = 1;
				break;
			case 2:bigGun[i].moved_gun_no = 2;
				break;
			case 3:bigGun[i].moved_gun_no = 4;
				break;
			case 4:bigGun[i].moved_gun_no = 8;
				break;
			case 5:bigGun[i].moved_gun_no = 16;
				break;
			case 6:bigGun[i].moved_gun_no = 32;
				break;
			case 7:bigGun[i].moved_gun_no = 0x40;
				break;
			case 8:bigGun[i].moved_gun_no = 0x80;
				break;
			default:Dla_LevelLog("相同节点号面板号的枪数超过8, 已经有[%d]",bigGun[i].logical_gun_no);
				break;			
		}
		RunLog("第[%d]把枪的节点号[%d], 面板号[%d],IFSF动枪号[%x]",bigGun[i].big_gun_no, \
			bigGun[i].node, bigGun[i].fp, bigGun[i].moved_gun_no );		
	}
		
	RunLog("DLA_MAIN 初始化共享内存...");
	//初始化共享内存
	ret = Dla_InitTheKeyFile();
	if( ret < 0 )
	{
		RunLog( "初始化Key File失败\n" );
		exit(1);
	}
	//#endif 
	
	RunLog("初始化配置参数...");
	//初始化配置参数
	ret = Dla_InitConfig();
	if(ret <0){
		RunLog("配置参数初始化出错.");
		exit(0);
	}
	RunLog("连接共享内存...");
	
	gpDlaGunShm = (GunShm*)Dla_AttachTheShm( GUN_SHM_INDEX );
	gpDlaIfsf_Shm = (IFSF_SHM*)Dla_AttachTheShm( IFSF_SHM_INDEX );
	if( (gpDlaGunShm == NULL)||(gpDlaIfsf_Shm == NULL) ){
		RunLog( "连接共享内存失败" );
		exit(1);
	}
	
	
	
	//循环处理并发送交易数据和实时数据(如果要的话)
	//先初始化UDP连接和打开FIFO
	//打开FIFO
	gDla.fd_fifo = -1;
	gDla.fd_fifo = open(DLA_FIFO, O_RDONLY | O_NONBLOCK);
	if(gDla.fd_fifo <= 0){
		RunLog("只读打开FIFO打开出错.");
		close(gDla.fd_fifo);
		exit(0);
	}
	//初始化UDP连接
	gDla._SOCKUDP = -1;
	//bzero(&_SERVER_UDP_ADDR,sizeof(_SERVER_UDP_ADDR));
	//_SERVER_UDP_ADDR.sin_family=AF_INET;
	//_SERVER_UDP_ADDR.sin_addr.s_addr=inet_addr(gDla._SERVER_IP);
	//_SERVER_UDP_ADDR.sin_port = htons(gDla._SERVER_UDP_PORT);	
	gDla._SOCKUDP = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
	if (  gDla._SOCKUDP<=0 ){//connect(_SOCKUDP, (struct sockaddr *)_SERVER_UDP_ADDR, sizeof(_SERVER_UDP_ADDR))
		RunLog("UDP连接建立不成功,初始化UDP连接失败");
		//close(_SOCKUDP); 
		gDla._SOCKUDP = -1;	
		exit(0);
	}
	gDla._SOCKTCP = -1;
	
	//循环处理数据
	gDla._REALTIME_FLAG ==1;  // 0
	gpDlaIfsf_Shm->css_data_flag = 2; // 1
	bzero(realtimeBuff, sizeof(realtimeBuff));
	bzero(sendBuff, sizeof(sendBuff));
	gDla.msgLength = 0;
	RunLog("开始循环处理管道数据.");
	while(1){
		//RunLog("循环读取FIFO数据:总枪数[%d]",gDla.totalGun);
		ret = Dla_ReadTransData(sendBuff );//resultBuff
		RunLog( "循环读取FIFO返回数据:[%s]",Dla_Asc2Hex(sendBuff,gDla.msgLength) );
		if( ret  == 1){//交易数据
			//组织并发送挂枪数据
			realtimeBuff[0] =0x02;
			realtimeBuff[1] =0;
			
			
			realtimeBuff[2] = sendBuff[2];//枪号
			#if OLD_PROTOCOL	//旧的协议
			memcpy(&realtimeBuff[3] ,&sendBuff[16],4); 
			memcpy(&realtimeBuff[7] ,&sendBuff[9],3);
			memcpy(&realtimeBuff[10] ,&sendBuff[12],4);
			tmp = gDla.msgLength;
			gDla.msgLength = 14;
			#else			
			memcpy(&realtimeBuff[3] ,&sendBuff[16],5); 
			memcpy(&realtimeBuff[8] ,&sendBuff[9],3);
			memcpy(&realtimeBuff[11] ,&sendBuff[12],4);
			tmp = gDla.msgLength;
			gDla.msgLength = 15;
			#endif
			RunLog( "准备发送挂枪数据:[%s]",Dla_Asc2Hex(realtimeBuff,gDla.msgLength) );
			ret = Dla_SendRealtimeData(realtimeBuff);
			 RunLog("重复发送挂枪信号第1 次");
		        ret = Dla_SendRealtimeData(sendBuff);	
			 RunLog("重复发送挂枪信号第2 次");	
			  ret = Dla_SendRealtimeData(sendBuff);
			//发送交易数据
			gDla.msgLength = tmp;
			//printf( "准备发送交易数据:[%s]",Dla_Asc2Hex(sendBuff, gDla.msgLength) );			
			for(i=0;i<tryn;i++){
				sIPName = gDla._SERVER_IP;//inet_ntoa(  *((struct in_addr *)(&gpIfsf_shm->recv_hb[i].host_ip))  );
				gDla._SOCKTCP = Dla_TcpConnect(sIPName, gDla._SERVER_TCP_PORT, timeout);//sPortName(timeout+1)/2
				if (gDla._SOCKTCP <= 0 ){
					continue;
				}else{
					break;
				}
			}
			if(gDla._SOCKTCP <= 0){
				RunLog("TcpConnect连接不成功,DLA发送交易数据 失败");
			}else{
				ret = Dla_SendTransData(sendBuff);
				close(gDla._SOCKTCP);
				if(ret>=0){
					RunLog( "发送交易数据成功,返回值[%d]",ret);
				}
				else{
					RunLog( "发送交易数据失败,返回值[%d]",ret);
				}
			}
		}else if( ret == 2 ){//实时数据(gDla._REALTIME_FLAG == 1)&& ()
			//Dla_LevelLog( "发送实时数据");
			RunLog( "准备发送实时数据:[%s]",Dla_Asc2Hex(sendBuff,gDla.msgLength) );
			ret = Dla_SendRealtimeData(sendBuff);
			//modify by liys 2008-09-25 ,避免挂枪信号丢失
			if (sendBuff[0] == 0x02){	
				 RunLog("重复发送挂枪信号第1 次");
				 ret = Dla_SendRealtimeData(sendBuff);	
				 RunLog("重复发送挂枪信号第2 次");
			       ret = Dla_SendRealtimeData(sendBuff);
				  
			}	
			Dla_LevelLog( "发送实时数据完毕,返回[%d]",ret);
		}
		else{
			RunLog( "返回数据值不是1和2");
		}
		
	}
	
	gpDlaIfsf_Shm->css_data_flag = 0;
	Dla_DetachTheShm((char**)(&gpDlaGunShm));
	Dla_DetachTheShm((char**)(&gpDlaIfsf_Shm));
	//#endif	
	exit(0);	
	return 0;
}



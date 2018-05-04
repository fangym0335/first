/*
 * 2008-11-10 - Chen Fu-ming <chenfm@css-intelligent.com>
 ˼·:��������inittab����wait�Զ�����,Ȼ�󱾳�����������ļ� ���õ�IP��ַ�Ͷ˿ں�
 ����PC���ϵĲ��Գ���,�������1���Ӷ�ε����Ӷ����ܳɹ�,��ô������
 �Զ��˳�;������ӳɹ��Ϳ�ʼ����.��˱������ڳ�Ʒ��Ҳ���Դ���. 
 ������������ҪPC��2��ͬ�ͺŵ�FCC,�����һ���õ�һ�������Ե�,
 PC����IP��ַ����,��һ��ע�������FCC,�ڶ����Ǵ�FCC,ʹ��ʱע��,�������FCCΪ��.
 * *******************************************************************************************************
 Э��:
 ���ݸ�ʽ����HEX,��ע��������,�������ݵı�־�ֽڵĺ�4λ1Ϊ1��FCC,2Ϊ2��FCC
1. FCC��PC ע��:
FCC��:1104+����IP��ַ+4������ͨ����,PC�ظ�:110111���� 110122,����110111Ϊ1��,110122Ϊ2��FCC.
2.FCC�����е�����Ϣ����UDP���͸�PC�Ĳ��Գ���,����һ��������ʾ
3. ��ʼȫ������:(�ȴ��Ӳ���FCCע���PC�Զ���)
  PC������FCC��:220111��220122, 11����������, 22���ǴӲ���,FCC�յ��ظ�2200��ʾ��ʼ����
  ������ɺ�,FCC���Ͳ��Խ��,���Խ������λ����־,0����ʧ��,1���Գɹ�:
  220a+1�ֽڴ��ڳɹ���־+8�ֽڴ���ͨ����־+1�ֽ�USB��־�ֽ�,��12�ֽ�,PC�ظ�2200
  8�ֽڴ���ͨ����־,ÿ������2���ֽ����16��ͨ��,������֧��64��ͨ��
4.ͬ����������:
    FCC����0xff03110000����0xff03220000,FCC�յ�0xff0111����0xff0122��������, ����ѭ���ȴ�
5.���ݴ�����:
  �������ݴ����ʱ��,��һ���ֽ�,Ҳ���������ֽڵĵ�4λҪ��1����ֵf,
  �ڶ����ֽ�Ϊ0,   ��1100���1f00,
6. �˳����Գ���:
    PC������FCC��9900,FCC�յ��ظ�9900

-------------------------------------------------------------------------------------------
���¹�����ʱû�����
7. ���ֲ���:(���ֲ��Ա�����ȫ����������ɺ���ܽ���)
   1).����ͨ������: PC������FCC��:3301XX3301YY,XX�������Ե�ͨ����,YY�ǴӲ��Ե�ͨ����
      ����ͨ��Ҫ��������.FCC�ظ�3300. �ɹ���FCC�ظ�3301XX����3301YY,PC�ظ�3300.
   2).��ͨ���ڲ���:PC������FCC��:4401XX4401YY,XX�������ԵĴ��ں�,YY�ǴӲ��ԵĴ��ں�
      ����ͨ��Ҫ��������.���ںŴ�0��ʼ.FCC�ظ�4400.�ɹ���ظ�4401XX����4401YY,PC�ظ�4400.
   3).USB����:PC��������FCC��:5501XX5501YY,XX�������Ե�USB��,YY�ǴӲ��Ե�USB��
      ����ͨ��Ҫ��������.USB�Ŵ�1��ʼ.0��ʾ������
      FCC�ظ�5500. �ɹ���FCC�ظ�5501XX����5501YY,PC�ظ�5500.
  4).���������Ѳ���U��:   FCC��6601XX, XX�ǲ��Ե�USB��PC�ظ�6600.�������,PC��6601XX,
     FCC�ظ�6600,����ʼ����.   ���Խ���FCC��6602XX00����6602XX11, XX�ǲ��Ե�USB��,
     00��ʾ���Գɹ�,11��ʾ����ʧ��,PC�ظ�6600
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

//���ź�������ֵ


//----------------------------------------------
//ʵʱ��־����
//===============================
#define MAIN_MAX_VARIABLE_PARA_LEN 1024 
#define MAIN_MAX_FILE_NAME_LEN 1024 
#define USB_DEV_SDA 0	//USB���豸��
#define USB_DEV_SDB 16
#define USB_DEV_SDC 32
#define USB_DEV_SDD 48
static char gsFccFileName[MAIN_MAX_FILE_NAME_LEN + 1];	//��¼������Ϣ���ļ���
static int giFccLine;								//��¼������Ϣ����
static int giFccFd;                                                       //��־�ļ����
static char gcFccLogFlag;
static unsigned  char  gsLocalIP[16];
//�Զ���ּ��ļ���־,��__Log.
#define Main_Log Main_SetFileAndLine( __FILE__, __LINE__ ), Main__Log
//#define Main_Log Main_SetFileAndLine( __FILE__, __LINE__ ),Main__FileLog
//--------------------------------------------------------------

//-------------------------------------------------------
//���ڶ���
//��������,���ڴ�0��ʼ
//#define START_PORT 3
#define MAX_PORT 6
//#define PORT_USED_COUNT 3
//�����ַ���
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
//����ͨ����Ŀ,������ͨ��ֵΪ0.
//unsigned char portChanel[MAX_PORT+1];
//----------------------------------------------------

//-------------------------------------------
//��ʼ�����ò���
//-------------------------------------------
static char PC_SERVER_IP[16]; //PC������IP��ַ
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
//��PCע��
/*FCC��PC ע��:
*FCC��:1104+����IP��ַ+4������ͨ����,PC�ظ�:110111���� 110122,����110111Ϊ1��,110122Ϊ2��FCC.*/
//static unsigned char FccNum;
//-------------------------------------------------

//------------------------------------------------

static char Main_USB_UM[]="/bin/umount -l /home/ftp/";
static char Main_USB_File[]="/home/ftp/fcctest.txt";
static char Main_USB_Data[]="1234567890";
//---------------------------------------------

//------------------------------------------------
//����ظ�����
static unsigned char Main_Result[8];
//---------------------------------------------




//------------------------------------------------
static unsigned  long  Main_GetLocalIP();

//-------------------------------------------------

//----------------------------------------------------

//--------------------------------------------------------------------------------
//ʵʱ��־
//----------------------------------------------
void Main_SetLogFd( const int fd )
{
	giFccFd= fd;
	if(  giFccFd <=  0){
		printf( " {��־�ļ�����������[%d] }\n", fd );
		
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
		(struct sockaddr *)(&remoteSrv), sizeof(struct sockaddr));//��������
	/*if (ret < 0) {				
		printf("�������ݷ���ʧ��,error:[%s]", strerror(errno));	
	}*/		
	
}

//giLogLevel=0, no log;giLogLevel=1, only print log info; giLogLevel = 2, print log info and write log info to file 
void Main__Log(const char *fmt,...){//const int loglevel, const int fd, 
	va_list argptr;
	char sMessage[MAIN_MAX_VARIABLE_PARA_LEN + 1];
	char fileName[MAIN_MAX_FILE_NAME_LEN + 1];	/*��¼������Ϣ���ļ���*/
	int line;								/*��¼������Ϣ����*/
	//int level = giFccLogLevel;
	
	
	Main_GetFileAndLine(fileName, &line);	
	//sprintf(sMessage, "[%s]", gsLocalIP);
	sprintf(sMessage, "%s:%d  ", fileName, line);
	sprintf(sMessage+strlen(sMessage), "pid = [%s:%d]", gsLocalIP, getpid());

	va_start(argptr, fmt);
	vsprintf(sMessage+strlen(sMessage),fmt, argptr);
	va_end( argptr );
	sprintf(sMessage+strlen(sMessage), "\n");
	
	
	if(giFccFd <=0 )	/*��־�ļ�δ��?*/
	{
		printf( " {��־�ļ�����������[%d] }\n", giFccFd );
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

int Main_InitLog(int flag){//flag---0: UDP��־,1:��ӡ��־
	int sockfd;
	unsigned char *pIP;
	struct in_addr in_ip;
    	 /*if( ( fd = open( MAIN_LOG_FILE, O_WRONLY|O_CREAT|O_TRUNC, 00644 ) ) <= 0 ) {
		printf( "����־�ļ�[%s]ʧ��\n", MAIN_LOG_FILE );
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
	else printf("Main_InitLog������־��ʼ������,flag[%d]\n",flag);
	//Main_SetLogLevel(loglevel);
	return 1;
}
//------------------------------------------------

//-----------------------------------------------------------------------
//���ú���
//-----------------------------------------------------------------------

unsigned char* Main_Asc2Hex( unsigned char *cmd, int cmdlen )
{
	static char retcmd[MAIN_MAX_ORDER_LEN*2+1];
	int	i;

	memset( retcmd, 0, sizeof(retcmd) );
	for( i = 0; i < cmdlen; i++ ) {
		sprintf(retcmd + i * 2, "%02x", cmd[i]);//ΪʲôҪi * 2
	}

	return retcmd;
}
//��ȡ����IP,���ر���IP��ַ������-1ʧ�ܣ����DHCP����
static unsigned  long  Main_GetLocalIP(){//�μ�tcp.h��tcp.c�ļ�����,����ѡ��һ��.
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

	port = GetTcpPort( sPortName );	//���ݶ˿ںŻ��߶˿���ȡ�˿� ȡ����portΪ�����ֽ���//
	if( port == INPORT_NONE )	//GetTcpPort�Ѿ������˴���,�˴�ʡ��//
		return -1;
	*/

	sockFd = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );//zhangjm IPPROTO_TCP ��tcpЭ��
	if( sockFd < 0 )
	{
		RptLibErr( errno );
		return -1;
	}

	//opt = 1;	//��־��//
	//setsockopt( sockFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof( opt ) );	//���ö˿ڿ�����//
	
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);	/*htonl �����ֽ��������ֽ���.����INADDR_ANYΪ0,���Կ��Բ���*//*INADDR_ANY ���ں�ѡ���ַ*/
	serverAddr.sin_port = htons(port);

	if( bind( sockFd, (SA *)&serverAddr, sizeof( serverAddr ) ) < 0 )
	{
		Main_Log( "�����:%d", errno );
		return -1;
	}

	if( listen( sockFd, backLog ) < 0 )
	{
		Main_Log( "�����:%d",errno );
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

		newsock = accept( sock, ( SA* )&ClientAddr, &len );//zhangjm�ͻ��ĵ�ַ����ϵ�����������Ϊ�գ�����sockΪǰ��ͻ�����ָ����stock
		if( newsock < 0 && errno == EINTR ) /*���ź��ж�*/
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

	Main_Log( "�����:%d",errno );
	return -1;
}
/*��nTimeOut����ΪTIME_INFINITE�������õ�ʱ���ϵͳȱʡʱ�䳤ʱ,
 connect����ɷ���ǰ��ֻ�ȴ�ϵͳȱʡʱ��*/
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

	if( ( addr = GetTcpAddr( sIPName ) ) == INADDR_NONE )//zhangjmʲô��˼�
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
		OldAction = SetAlrm( nTimeOut, NULL );  /*�����alarm���ú�,connect����ǰ��ʱ,��connect�ȴ�ʱ��Ϊȱʡʱ��*/
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
	 0:	�ɹ�
	-1:	ʧ��
	-2:	��ʱ
*/
static ssize_t Main_TcpSendDataInTime( int fd, const char *s, size_t len,  int timeout )
{
#if 1
	ssize_t	 retlen;
	size_t	sendlen;
	char	*head;
	char	*t;
	char	buf[MAIN_TCP_MAX_SIZE];//zhangjm TCP_MAX_SIZEû�ж���
	int	mflag; 	/*��ʶ�Ƿ�����˿ռ� 0δ����. 1�ѷ���*/
	int	n;
	int     i;
	
	Assert( s != NULL && *s != 0 );//����Ϊ����������������abortֹͣ��
	Assert( len != 0  );
	Assert( timeout > 0 || timeout == TIME_INFINITE );//�����timeout��������ʲô��ʱ���

	mflag = 0;
	sendlen = len;

	if (sizeof(buf) >= sendlen ) {	/*Ԥ�ȷ���ı���buf�Ŀռ��㹻��������Ҫ���͵�����*/
		t = buf;
	} else {			/*Ԥ�ȷ���ı���buf�Ŀռ䲻����������Ҫ���͵�����,�ʷ���ռ�*/
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

	if ( n == sendlen ) {	/*����ȫ������*/
		return 0;
	} else if ( n == -1 ) {/*����*/
		return -1;
	} else if ( n >= 0 && n < sendlen ) {
		if ( GetLibErrno() == ETIME ) {/*��ʱ*/
			return -2;
		} else {
			return -1;
		}
	}
#endif
#if 0
//��4�ֽڳ��ȵĴ��ͷ�ʽ
	ssize_t		retlen;
	char		*head;
	char		*t;
	char		buf[TCP_MAX_SIZE];
	size_t		sendlen;
	int			mflag; 	/*��ʶ�Ƿ�����˿ռ� 0δ����. 1�ѷ���*/
	int			n;
	int 		headlen=4;
	Assert( s != NULL && *s != 0 );
	Assert( len != 0 && headlen != 0 );
	Assert( timeout > 0 || timeout == TIME_INFINITE );

	mflag = 0;
	sendlen = headlen+len;

	if( sizeof(buf) >= sendlen )	/*Ԥ�ȷ���ı���buf�Ŀռ��㹻��������Ҫ���͵�����*/
		t = buf;
	else							/*Ԥ�ȷ���ı���buf�Ŀռ䲻����������Ҫ���͵�����,�ʷ���ռ�*/
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

	if( n == sendlen )	/*����ȫ������*/
		return 0;
	else if( n == -1 ) /*����*/
		return -1;
	else if( n >= 0 && n < sendlen )
	{
		if( GetLibErrno() == ETIME ) /*��ʱ*/
			return -2;
		else
			return -1;
	}

#endif
}

/*
 * s:	�������������
 * buflen:	ָ�롣*buflen������ʱΪs�ܽ��ܵ���󳤶ȵ����ݣ�������β����0.��char s[100], *buflen = sizeof(s) -1 )
 *		�����ʱΪsʵ�ʱ�������ݵĳ���(������β����0.)
 * ����ֵ:
 *	0	�ɹ�
 *	1	�ɹ�.�����ݳ��ȳ����ܽ��ܵ���󳤶�.���ݱ��ض�
 *	-1	ʧ��
 *	-2	��ʱ�������ӹ���ر�
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
		printf("�յ��ĳ���[%d]С��Ԥ���յ��ĳ���[%d],ֵ[%s]\n", n,len,Main_Asc2Hex(t, len));
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
	ret = access( sFileName, F_OK );//�ɹ�����0.�ж��Ƿ����sFileName F_OKΪ�Ƿ����R_OK��W_OK��X_OK��������ļ��Ƿ���ж�ȡ��д���ִ�е�Ȩ�ޡ�
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
//ȥ���ַ�������ո񼰻س����з�,��Ҫ�ַ�����������
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
//��ʼ�����ò���
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
	ret = Main_IsFileExist(fileName);//�ɹ�����1
	if(ret != 1){
		char pbuff[256];
		bzero(pbuff,sizeof(pbuff));
		sprintf( pbuff, "file[%s] not exist in readIni  error!! ", fileName );
		Main_Log(pbuff);
		exit(1);//zhangjmexit(1)��ʲô��
		return -2;
	}
	f = fopen( fileName, "rt" );//����ָ����ļ���ָ��
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
		c = fgetc( f);//f��ָ���ļ��ж�ȡһ���ַ����������ļ�β��������ʱ�㷵��EOF
		if( c == EOF)
			break;
		my_data[i] = c;
	}
	my_data[i] = 0;	
	fclose( f );
	
	
	bzero(&(fcc_ini[0].name), sizeof(MAIN_INI_NAME_VALUE)*MAIN_INI_MAX_CNT);	
	i = 0;
	while (my_data[0] !='\0'){
		tmp = Main_split(my_data, '=');// split data to get field name.�����=��֮ǰ�Ķ���
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
	//���ļ�
	fd = open( fileName, O_WRONLY|O_CREAT|O_TRUNC, 00644 );
	if( fd < 0 )
	{
		//UserLog( "�򿪵�ǰ���й���·����Ϣ�ļ�[%s]ʧ��.",fileName );
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
	ret = Main_readIni(configFile);//�������ļ�����IP�͸����˿ڶ�������ڽṹ��fcc_ini�з���ֵΪ�����ĸ���
	if(ret < 0){
		Main_Log("readIni error in _InitServerIP");
		return -1;
	}
	iniFlag = 0;
	//static int PC_SERVER_TCP_PORT,PC_SERVER_UDP_PORT,MAIN_TCP_PORT;
	for(i=0;i<ret;i++){
		if( memcmp(fcc_ini[i].name,"PC_SERVER_IP",11) == 0){
			memcpy(PC_SERVER_IP, fcc_ini[i].value,sizeof(PC_SERVER_IP ) );//sizeof(fcc_ini[i].value memcmp�����Ƚ�һ��󷵻�0ǰһ���󷵻�����һ���󷵻ظ�
			Main_Log("PC_SERVER_IP[%s]",PC_SERVER_IP);
			iniFlag += 1;
		}else if( memcmp(fcc_ini[i].name,"PC_SERVER_TCP_PORT",18) == 0 ){
			PC_SERVER_TCP_PORT=atoi(fcc_ini[i].value);
			Main_Log("PC_SERVER_TCP_PORT[%d]",PC_SERVER_TCP_PORT);
			iniFlag += 1;
		}else if( memcmp(fcc_ini[i].name,"PC_SERVER_UDP_PORT",18) == 0 ){
			PC_SERVER_UDP_PORT=atoi(fcc_ini[i].value);//atoi�ǽ��ַ���ת��Ϊ������"100"���100
			Main_Log("PC_SERVER_UDP_PORT[%d]",PC_SERVER_UDP_PORT);
			iniFlag += 1;
			//memcpy(gFcc._SERVER_UDP_PORT, fcc_ini[2].value, sizeof(fcc_ini[2].value) );
		}else if( memcmp(fcc_ini[i].name,"FCC_TCP_PORT",12) == 0 ){
			MAIN_TCP_PORT=atoi(fcc_ini[i].value);
			Main_Log("MAIN_TCP_PORT[%d]",MAIN_TCP_PORT);
			iniFlag += 1;
			//memcpy(gFcc._SERVER_UDP_PORT, fcc_ini[2].value, sizeof(fcc_ini[2].value) );
		}else{
			//Main_Log("�������ݳ���,  ����Ԥ�ڵ���������,������[%s],ֵ��[%s]",fcc_ini[i].name,fcc_ini[i].value);
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
//���ں�ͨ�����ʺ���
//------------------------------------------------------------------------------


static int Main_OpenSerial( char *ttynam ){
	int fd;
	struct termios termios_new;

	fd = open (ttynam, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if( fd < 0 )
	{
		//UserLog( "���豸[%s]ʧ��", ttynam );
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


//��д����
int Main_WriteTty(int fd, unsigned char *msg, int msgLen){
	int ret;
	int timeout = 5;
	//unsigned char buff[64],*result;
	
	//fd = OpenSerial( MAIN_alltty[serial_num]);
	if( fd <= 0 )
	{
		Main_Log( "����fd[%d]��ֵ����",fd );
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
		Main_Log( "����fd[%d]��ֵ����",fd );
		return -1;
	}
	//Main_Log( "after Main_Writetty and 0 to moved test!" );
	len = *recvLen;
	if( sizeof(buff) < len)
	{
		Main_Log( "FccReadTty����,buff����[%d]С�ڴ����ճ���[%d]",sizeof(buff), len);
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
//��PCע��
/*FCC��PC ע��:
*FCC��:1104+����IP��ַ+4������ͨ����,PC�ظ�:110111���� 110122,����110111Ϊ1��,110122Ϊ2��FCC.*/
//static unsigned char FccNum;
//static char PC_SERVER_IP[16]; //PC������IP��ַ
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
	
	
	//ͷ����������
	
	msgLength =6;
	sendBuffer[0] = 0x11;
	sendBuffer[1] = 0x04;	
	localIP = Main_GetLocalIP();
	
	//FCC�� IP��ַ
	memcpy(&sendBuffer[2],(unsigned char *)&localIP,4);//zhangjm(unsigned char *)&localIPʲô��˼�

	//����ͨ����Ŀ
	//memcpy(&sendBuffer[6],&portChanel[START_PORT],4);//(unsigned char *)&localIP
	memset(&sendBuffer[6],0,4);
	//i=k;
	//��PC�ϴ�����
	Main_Log("��PC�ϴ�����[%s]",Main_Asc2Hex(sendBuffer, msgLength) );
	i = 0;
	sIPName =PC_SERVER_IP;//inet_ntoa(  *((struct in_addr *)(&gpIfsf_shm->recv_hb[i].host_ip))  );
	while(1){
		i++;
		if( i > LOGIN_TIME ) exit(-1);//zhangjm�����i��Ϊ��3��������
		sockfd = Main_TcpConnect(sIPName,PC_SERVER_TCP_PORT , timeout);//(timeout+1)/2   //sPortName
		if (sockfd >0 ){
			//sleep(1);
			ret =Main_TcpSendDataInTime(sockfd, sendBuffer, msgLength, timeout);			//Act_V(CD_SOCK_INDEX);			
			if (ret < 0 ){
				Main_Log("TcpSendDataInTime�������ݲ��ɹ�,Main_Login�������� ʧ��,IP[%s],port[%d]",sIPName,PC_SERVER_TCP_PORT);
				close(sockfd);
				sleep(1);//sleep(10000);
				continue;
			}else{				
				break;
			}
		} else {
			Main_Log("Tcp Connect ���ɹ�,Main_Login�������� ʧ��,IP[%s],port[%d]",sIPName,PC_SERVER_TCP_PORT);
			close(sockfd);
			sleep(1);//sleep(10000); 			
			continue;
			//return -1;
		}
	}
	 //��ʼ��PC���շ�������
	Main_Log("��ʼ��PC���շ�������");
	bzero(sendBuffer,sizeof(sendBuffer));
	for(i=0;i<tryn;i++){
		msgLength = 3;		
		ret =Main_TcpRecvDataInTime(sockfd, sendBuffer, &msgLength, timeout);			//Act_V(CD_SOCK_INDEX);			
		if (ret < 0 ){
			Main_Log("TcpRecvDataInTime�������ݲ��ɹ�,Main_Login�������� ʧ��");
			sleep(10);
			continue;
		}
		break;
	}
	
      if( (sendBuffer[0] != 0x11)||(sendBuffer[1] != 0x01) ){
	  	Main_Log("Tcp Connect ���ɹ�,Main_Login�������� ʧ��,[%02x%02x%02x]",sendBuffer[0],sendBuffer[1],sendBuffer[2]);
		close(sockfd);
		return -1;
	}
	//CSS_LevelLog("gpCSS_Shm->css._TLG_SEND_INTERVAL��ֵ[%02x%02x]",sendBuffer[1],sendBuffer[2]);
      	//if(sendBuffer[2] == 0x11) {
	//		FccNum = 1;
	//		Main_Log("FccNum��ֵ[%d],��FCC",FccNum);
	//}else if(sendBuffer[2] == 0x22){ 
	//	FccNum = 2;
	//	Main_Log("FccNum��ֵ[%d],��FCC",FccNum);
	//}else{
	//	Main_Log("Main_Login�������ݴ���,Main_Login�������� ʧ��");
	//	return -1;
	//}
	close(sockfd);
	//Main_Log("FccNum��ֵ[%d]",FccNum);
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
				Main_Log("TcpSendDataInTime�������ݲ��ɹ�,IP[%s],port[%d]",sIPName,PC_SERVER_TCP_PORT);
				close(sockfd);
				sleep(10);//sleep(10000);
				continue;
			}else{				
				break;
			}
		} else {
			Main_Log("Tcp Connect ���ɹ�,IP[%s],port[%d]",sIPName,PC_SERVER_TCP_PORT);
			close(sockfd);
			sleep(60);//sleep(10000); 			
			continue;
			//return -1;
		}
	}
	 //��PC���շ�������
	Main_Log("��PC���շ�������");
	 //sleep(5);//�ȴ��Է�ִ�����
	//bzero(recvBuffer,sizeof(recvBuffer));
	for(i=0;i<tryn;i++){
		msgLength = *recvLength;		
		ret =Main_TcpRecvDataInTime(sockfd, recvBuffer, &msgLength, timeout);			//Act_V(CD_SOCK_INDEX);			
		if (ret < 0 ){
			Main_Log("TcpRecvDataInTime�������ݲ��ɹ�");
			sleep(10);
			continue;
		}
		break;
	}
      if( (recvBuffer[0] == 0)&&(recvBuffer[1] == 0) ){
	  	Main_Log("Tcp Connect ���ɹ�,[%02x%02x%02x]",recvBuffer[0],recvBuffer[1],recvBuffer[2]);
		close(sockfd);
		return -1;
	}	
	close(sockfd);
	memcpy((char *)recvLength,(char *)&msgLength,sizeof(int));
	Main_Log("Main_SendRecv��������[%s]",Main_Asc2Hex(recvBuffer, msgLength));	
	return 0;
}


//--------------------------------------------------
//ͨѶ�������
//-----------------------------------------------
//static char PC_SERVER_IP[16]; //PC������IP��ַ
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
		Main_Log( "ʵʱ���̳�ʼ��socketʧ��.port=[%s]\n", MAIN_TCP_PORT );
		//Free( cmd );
		return  ;
	}
	//fcntl(sock, F_SETFL, O_NONBLOCK);   // ������ģʽ
	
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
		if (newsock < 0) {  // ���û�����ӣ������
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
		Main_Log("Main_WaitStartCmd����,soket��[%d]", tmp_sock);
		if (tmp_sock > 0) {
			if (newsock > 0) {
				close(newsock);
			}
			newsock = tmp_sock;
			tmp_sock = -1;//zhangjmΪʲôҪ�ر��ļ�������newsock�ٸ�ֵ
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
				/*������*/
				Main_Log( "��������[%d]�ζ�ʧ��", tryNum );
				break; 
			}
			msg_lg = 3;
			sleep(5);
			ret = Main_TcpRecvDataInTime(newsock, buffer, &msg_lg, timeout );//TIME_INFINITE	
			if (ret < 0) {
				if (errno == 4) {  // time out zhangjm errno�Ǵ���������ġ
					Main_Log("���̳�ʱ,���յ���̨���� buffer[%s]\n" , Main_Asc2Hex(buffer, msg_lg) );//
					continue;
				} else if (errno == 11)  {  // maybe CD quit or restart, && (gpIfsf_shm->need_reconnect == 0))
					//gpIfsf_shm->need_reconnect = 1;
					Main_Log("Client need to reconnect to Server");
				}	
				Main_Log( "���̽�������ʧ��,errno:[%d][%s],buffer: %s", errno, strerror(errno), \
					Main_Asc2Hex(buffer, 2) );
				errcnt++;
				sleep(1);
				continue;
			} else {
				Main_Log( "<<<�����յ�����: [%s(%d)]", Main_Asc2Hex(buffer, msg_lg ), msg_lg);				
				if ( 0x22 != buffer[0] ){ //received data must be error!!(0 == (buffer[0]>>4) )||(0 ==( buffer[0]&0x0f) )
					Main_Log( "received data error" );
					errcnt++;
					sleep(1);
					continue;
				}

				if (0 == msg_lg){ //receivd data must be error!!
					Main_Log( "received size is  0 error" );
					errcnt++;
					//�ظ�����
					sBuffer[0] = 0x2f;
					sBuffer[1] = 0;
					//memcpy(&sBuffer[1],gpCSS_Shm->css._STATION_NO_BCD,5);
					ret = Main_TcpSendDataInTime(newsock, sBuffer, 2, timeout);
					if(ret <=0){				
						Main_Log("ʵʱ���̻ظ����� ʧ��");
					}
					
					sleep(1);
					continue;
				}

				
				
				//�����̨����.
				if (1 == buffer[1]){ 
					if(0x11 == buffer[2]){
						/*if(1 != FccNum) {
							Main_Log("PC�������� ����,������FCC");
							sBuffer[0] = 0x2f;
							sBuffer[1] = 0;
							//memcpy(&sBuffer[1],gpCSS_Shm->css._STATION_NO_BCD,5);
							ret = Main_TcpSendDataInTime(newsock, sBuffer, 2, timeout);
							if(ret <0){				
								Main_Log("���̻ظ����� ʧ��");
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
							Main_Log("���̻ظ����� ʧ��");
							//continue;
						}
						errcnt = 0;
						close(newsock);close(sock);
						return 1;//break;
					}/*else if(0x22 == buffer[2]){
						if(2 != FccNum) {
							Main_Log("PC�������� ����,������FCC");
							sBuffer[0] = 0x2f;
							sBuffer[1] = 0;
							//memcpy(&sBuffer[1],gpCSS_Shm->css._STATION_NO_BCD,5);
							ret = Main_TcpSendDataInTime(newsock, sBuffer, 2, timeout);
							if(ret <0){				
								Main_Log("���̻ظ����� ʧ��");
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
							Main_Log("���̻ظ����� ʧ��");
							//continue;
						}
						errcnt = 0;
						close(newsock);close(sock);
						return 1;//break;
					}*/
					else{
						Main_Log( "��������ʧ��,�յ�������ֵ����0x11��0x22" );
					}
				}  else{
					Main_Log( "��������ʧ��,�յ������ݵĳ����ֽڲ���1" );
				}				
				break;
				//sleep(10);			
			}
		}
		if ( errcnt > 40 ) {
			/*������*/
			Main_Log( "��ʼ��socket ʧ��.port=[%d]", MAIN_TCP_PORT );
			break; 
		}
	}

	Main_Log("Main_AcceptPCCmd  return ......");
	close(sock);

	return  -1;
}




//�����շ�����
int Main_ComSendRecv(int serialPortNum){
	int i, ret, tmp,fd,n,k;//,, sockfd
	unsigned char buff[64] ;
	int timeout = 5, tryn = 5;
	char		cmd[32];
	int			cnt;
	
	
	fd = Main_OpenSerial(MAIN_alltty[serialPortNum]);
	if(fd <=0){
		Main_Log( "Main_ComRecvSend�򿪴��ڴ���" );
		return -1;
	}
	//Main_Log("��PC��������");
	Main_Log( "Main_ComSendRecv��ʼ���Դ���[%d]" ,serialPortNum);
	for(i=0;i<tryn;i++){
		memset(cmd,0xa5,sizeof(cmd));
		if( sizeof(cmd) > MAX_PORT_CMD_LEN ) {
			Main_Log( "�򴮿�[%d]д�����ݳ���[%d]������󳤶�[%d]", serialPortNum,sizeof(cmd), MAX_PORT_CMD_LEN );
			close(fd);
			return -1;
		}		 
		//cmd[2] = 0x00+ (sizeof(cmd) -3 ) ;
		//memcpy( cmd+3, buff, len );
		cnt =sizeof(cmd);
		Main_Log("Main_ComRecvSend��������[%s]",Main_Asc2Hex(cmd, cnt));
		ret = Main_WriteTty(fd, cmd, cnt);
		if(ret <=0){
			Main_Log( "�򴮿�[%d]д����ʧ��" ,serialPortNum);
			sleep(5);
			continue;
		}
		//break;
		sleep(5);
		bzero(buff,sizeof(buff));
		cnt =sizeof(cmd);
		ret = Main_ReadTty(fd, buff, &cnt);
		if(ret <=0){
			Main_Log( "����[%d]������ʧ��",serialPortNum );
			sleep(5);
			continue;
		}
		Main_Log("Main_ComRecvSend��������[%s]",Main_Asc2Hex(buff, cnt));	
		break;
	}
	if( (ret <=0) ||(i >=tryn) ){
		Main_Log( "�Ӵ���[%d]������[%d]��ȫ��ʧ��" ,serialPortNum,tryn);
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
		Main_Log( "Main_ComRecvSend�򿪴��ڴ���" );
		return -1;
	}
	//Main_Log("��PC��������");
	Main_Log( "Main_ComRecvSend��ʼ���Դ���[%d]" ,serialPortNum);
	for(i=0;i<tryn;i++){		
		sleep(1);//�ȴ��Է�ִ�����
		bzero(buff,sizeof(buff));
		cnt =sizeof(cmd);
		ret = Main_ReadTty(fd, buff, &cnt);
		if(ret <=0){
			Main_Log( "����[%d]������ʧ��",serialPortNum );
			sleep(5);
			continue;
		}
		Main_Log("Main_ComRecvSend��������[%s]",Main_Asc2Hex(buff, cnt));
		sleep(5);
		//break;
		memset(cmd,0x5a,sizeof(cmd));
		if( sizeof(cmd) > MAX_PORT_CMD_LEN ) {
			Main_Log( "�򴮿�[%d]д�����ݳ���[%d]������󳤶�[%d]", serialPortNum,sizeof(cmd), MAX_PORT_CMD_LEN );
			close(fd);
			return -1;
		}		 
		//cmd[2] = 0x00+ (sizeof(cmd) -3 ) ;
		//memcpy( cmd+3, buff, len );
		cnt =sizeof(cmd);
		Main_Log("Main_ComRecvSend��������[%s]",Main_Asc2Hex(cmd, cnt));
		ret = Main_WriteTty(fd, cmd, cnt);
		if(ret <=0){
			Main_Log( "�򴮿�[%d]д����ʧ��" ,serialPortNum);
			sleep(5);
			continue;
		}
		sleep(5);//�ٷ���һ��
		cnt =sizeof(cmd);
		Main_Log("Main_ComRecvSend��������[%s]",Main_Asc2Hex(cmd, cnt));
		ret = Main_WriteTty(fd, cmd, cnt);
		if(ret <=0){
			Main_Log( "�򴮿�[%d]д����ʧ��" ,serialPortNum);
			sleep(5);
			continue;
		}
		break;
	}
	if( (ret <=0) ||(i >=tryn) ){
		Main_Log( "�Ӵ���[%d]������[%d]��ȫ��ʧ��" ,serialPortNum,tryn);
		close(fd);
		return -1;
	}
	
	close(fd);
	return 0;
}


//���ݼ�⵽��U�����ƹ���U��
int Main_mountUsb(){

	unsigned char *fileName = "/proc/partitions";
	unsigned char buff[64],usbDev[4],devName[64],mountName[128],rmDev[64];
	FILE *fp;
	int i, j,ret, first_sdx;
	
	fp = fopen(fileName, "rt");
	if(fp == NULL){
		Main_Log("fileName[%s] ��ʧ��!",fileName);
		return -1;
	}
	
	bzero(buff, sizeof(buff));	
	bzero(usbDev, sizeof(usbDev)); //��ȡ  U���豸����
	
	i = 0;
	first_sdx = 0; //��һ�ζ�ȡ��sd����1
	while(i < 15){
		fgets(buff, sizeof(buff), fp);

		if(1 == first_sdx){
			//U�����Ʊ��浽usbDev��
			for(j =0; j < sizeof(buff); j++){
				if(buff[j] == 's'&& buff[++j] == 'd'){ //�ڶ��μ�⵽sdx
					usbDev[0] = buff[++j];
					usbDev[1] = buff[++j];
					Main_Log("buff[%s]  usbDev[%s]",buff, usbDev);
					break;
				}
			}
			break;
		}

		for(j =0; j < sizeof(buff); j++){
			if(buff[j] == 's'&& buff[++j] == 'd'){ //�ڶ��μ�⵽sdx
				first_sdx = 1;  //��һ�μ�⵽sdx
			}
		}
		
		bzero(buff, sizeof(bzero));
		i++;
	}

	if(i == sizeof(buff)){
		Main_Log("��ȡU������ʧ��!buff[%s]", buff);
		return -1;
	}

	//�����豸�ļ�
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

	//����ɾ�������豸�ļ������´���
	bzero(rmDev, sizeof(rmDev));
	sprintf(rmDev,"/bin/rm /dev/sd%s", usbDev);
	system(rmDev);

	ret = system(devName);
	if(ret < 0){
		Main_Log("devName[%s] ����U���豸�ļ�ʧ��!", devName);
		return -1;
	}

	//ж�ع�����
	system(Main_USB_UM);
	//����U��
	bzero(mountName,sizeof(mountName));
	sprintf(mountName,"/bin/mount -t vfat /dev/sd%s /home/ftp",usbDev);
	ret = system(mountName);
	if(ret < 0){
		Main_Log("mountName[%s] ����U��ʧ��!", mountName);
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
		Main_Log("Main_readUSB����ֵ����,ֵ[%d]",usbnum);
		return -1;
	}
	//�ҽ�U��
      	ret = Main_mountUsb();
	if( ret < 0 )
	{
		Main_Log( "***����USB����ִ��ʧ��\n" );
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
	//ȥ���ҽ�U��
	ret = system(Main_USB_UM);
	if( ret < 0 )
	{
		Main_Log( "***USB����[%s]ִ��ʧ��\n" ,Main_USB_UM);
		return -1;//exit(1);
	}	
	if(  0 != memcmp( my_data,Main_USB_Data,sizeof(Main_USB_Data) )  ){
		Main_Log( "��ȡ������[%s]��Ӧ����ȡ��������[%s]��ͬ\n" ,my_data,Main_USB_Data);
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
		Main_Log( "ʵʱ���̳�ʼ��socketʧ��.port=[%s]\n", MAIN_TCP_PORT );
		//Free( cmd );
		return  ;
	}
	//fcntl(sock, F_SETFL, O_NONBLOCK);   // ������ģʽ
	
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
		if (newsock < 0) {  // ���û�����ӣ������
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
		Main_Log("Main_AcceptPCCmd����,soket��[%d]", tmp_sock);
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
				/*������*/
				Main_Log( "��������[%d]�ζ�ʧ��", tryNum );
				break; 
			}
			ret = Main_TcpRecvDataInTime(newsock, buffer, &msg_lg, timeout );//TIME_INFINITE	
			if (ret < 0) {
				if (errno == 4) {  // time out
					Main_Log("���̳�ʱ,���յ���̨���� buffer[%s]\n" , Main_Asc2Hex(buffer, 32) );//
					continue;
				} else if (errno == 11)  {  // maybe CD quit or restart, && (gpIfsf_shm->need_reconnect == 0))
					//gpIfsf_shm->need_reconnect = 1;
					Main_Log("Client need to reconnect to Server");
				}	
				Main_Log( "���̽�������ʧ��,errno:[%d][%s],buffer: %s", errno, strerror(errno), \
					Main_Asc2Hex(buffer, 2) );
				errcnt++;
				sleep(1);
				continue;
			} else {
				Main_Log( "<<<�����յ�����: [%s(%d)]", Main_Asc2Hex(buffer, msg_lg ), msg_lg);				
				if ( (0 == (buffer[0]>>4) )||(0 ==( buffer[0]&0x0f) )  ){ //received data must be error!!
					Main_Log( "received data error" );
					errcnt++;
					sleep(1);
					continue;
				}

				if (0 == msg_lg){ //receivd data must be error!!
					Main_Log( "received size is  0 error" );
					errcnt++;
					//�ظ�����
					sBuffer[0] = 0x1f;
					sBuffer[1] = 0;
					//memcpy(&sBuffer[1],gpCSS_Shm->css._STATION_NO_BCD,5);
					ret = Main_TcpSendDataInTime(newsock, sBuffer, 2, timeout);
					if(ret <=0){				
						Main_Log("ʵʱ���̻ظ����� ʧ��");
					}
					
					sleep(1);
					continue;
				}

				
				
				//�����̨����.
				Main_Log( "��ʼ�����յ�����: [%s(%d)]", Main_Asc2Hex(buffer, msg_lg ), msg_lg);
				if (0 == buffer[1]){ 
					if(0x99 == buffer[0]){
						Main_Log( "�յ��˳�����: [%s(%d)],�˳�", Main_Asc2Hex(buffer, msg_lg ), msg_lg);
						
					}
				}
				
				break;
				//sleep(10);			
			}
		}
		if ( errcnt > 40 ) {
			/*������*/
			Main_Log( "��ʼ��socket ʧ��.port=[%d]", MAIN_TCP_PORT );
			break; 
		}
	}

	Main_Log("Main_AcceptPCCmd  Stop ......");
	close(sock);

	return  ;
}
/*//����FCCͬ��
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
		Main_Log( "***Fcc_SynStart��������,�����˳�" );
		exit(-1);
	}
	while(1){
		n = 3;
		ret = Main_TCPSendRecv(WaitSendBuff[NumFcc-1],5, WaitRecvBuff,&n);
		if(ret <0){
			Main_Log("���Ͳ����м�ͬ��ֵ�������");
			sleep(5);
			continue;
		}else{
			if( (0xff != WaitRecvBuff[0]) || (0x01 != WaitRecvBuff[1])|| (WaitFlag[NumFcc-1] != WaitRecvBuff[2]) ){//0 != memcmp(TestFlag,"2200",2)
				sleep(2);
				continue;
			}else{
				Main_Log("�����м�ͬ���ɹ�,��������");
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
		Main_Log("Main_Com_Thread����ֵ����,ֵ[%d],��ʾ���ں�",sn);
		//if(2 == FccNum){
			Main_Log("***����[%d]����ʧ��",sn);
		//}else{
		//	Main_Log("����[%d]����ʧ��",sn);
		//}
		return;
	}
	if(sn == 0){
		Main_Log("***��ʼ����[%d]����",sn);		
		ret = 1;//Main_ComSendRecv(sn);
		if(ret <0){			
			Main_Log("***����[%d]����ʧ��",sn);				
		}else{
			Main_Result[2]  = Main_Result[2]  | (1<<7 );
			Main_Log("***����[%d]���Գɹ�",sn);
		}
	}else if(sn%2 == 0){
		Main_Log("***��ʼ����[%d]����",sn);		
		ret = Main_ComRecvSend(sn);
		if(ret <0){			
			Main_Log("***����[%d]����ʧ��",sn);				
		}else{
			Main_Result[2]  = Main_Result[2]  | (1<<(8-sn-1) );
			Main_Log("***����[%d]���Գɹ�",sn);
		}
	}else if(sn%2 == 1){
		Main_Log("***��ʼ����[%d]����",sn);
		ret = Main_ComSendRecv(sn);
		if(ret <0){			
			Main_Log("***����[%d]����ʧ��",sn);				
		}else{
			Main_Result[2]  = Main_Result[2]  | (1<<(8-sn-1) );
			Main_Log("***����[%d]���Գɹ�",sn);
		}
	}else{
		Main_Log("***���ڳ���[%d],�˳�",sn);
		exit(0);
	}
	//return;
}

//����FCCͬ��
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
		Main_Log( "***Fcc_SynStart��������,�����˳�" );
		exit(-1);
	}
	while(1){
		n = 3;
		ret = Main_TCPSendRecv(WaitSendBuff[NumFcc-1],5, WaitRecvBuff,&n);
		if(ret <0){
			Main_Log("���Ͳ���ͬ��ֵ�������");
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


//������
int main( int argc, char* argv[] )
{
	//#define GUN_MAX_NUM 256 //���256��ǹ
	int i, j, n,k,len,ret,fd;//,pid,isChild
	unsigned char TestFlag[8];
	//pthread_t ThreadChFlag[64];//ͨ���߳�����
	pthread_t ThreadSrFlag[8];//�����߳�����
	if (argc == 2) {
	       if (strcmp(argv[1], "-v") == 0) {
			printf( "fcc_selftest. PCD version %s\n", PCD_VER);
	       } 
		return 0;
	}
	
	printf("��ʼ���в��Գ���\n");
	
	//��ʼ����־
	printf("��ʼ����־.\n");
	ret = Main_InitLog(1);//0: UDP��־,1:��ӡ��־
	if(ret < 0){
		printf("��־��ʼ������.\n");
		exit(-1);  
	}

		
	/*//��ʼ�������ڴ�
	ret = Main_InitTheKeyFile();
	if( ret < 0 )
	{
		Main_Log( "��ʼ��Key Fileʧ��\n" );
		exit(1);
	}
	*/
		
	//��ʼ������ͨ����Ŀ
	
	
	//��ʼ�����ò���
	Main_Log("��ʼ��ʼ�����ò���");
	ret = Main_InitConfig();//�ɹ�����0���ɷ���-1
	if(ret <0){
		Main_Log("���ò�����ʼ������.");
		exit(-1);
	}

	//��ʼ��ע��
	Main_Log("��ʼ��ʼ��ע��");
	ret = Main_Login();
	if(ret <0){
		Main_Log("��ʼ��ע�����.");
		exit(-1);
	}
	Main_Log("��ʼ��ע��ɹ�");
	sleep(5);
	// ����һ���ź���.	
	//ͨ������ʼ��	
	
	//ѭ������
	while(1){
		//�ȴ����Կ�ʼ����
		//if(2 == FccNum){
			Main_Log("***�ȴ����Կ�ʼ����");
		//}else{
		//	Main_Log("�ȴ����Կ�ʼ����");
		//}
		ret = Main_WaitStartCmd();
		if(ret <0){
			Main_Log("��ʼ�����������,�˳�");
			exit(-1);
		}
		Main_Log("���Կ�ʼ����ɹ�");
# if 0
		if(2 == FccNum){
			Main_Log("***��ʼ���ƿ�(����0)����",i);
			ret = Main_ComSendRecv(0);
			if(ret <0){
				Main_Log("***���ƿ�(����0)����ʧ��",i);
				//exit(0);
			}else{
				//TestFlag[0] = TestFlag[0] | (1<<7 );					
				Main_Log("***���ƿ�(����0)���Գɹ�",i);
			}
			ret = Main_SynStart(FccNum);
		}else{
			sleep(30);
			ret = Main_SynStart(FccNum);
		}
#endif		
		//-----------------------------
		//��ʼ����
		bzero(Main_Result,sizeof(Main_Result) );
		//bzero(ThreadChFlag,sizeof(ThreadChFlag) );
		bzero(ThreadSrFlag,sizeof(ThreadSrFlag) );
		Main_Result[0] = 0x22;
		Main_Result[1] = 0x01;
		
			
		
		//����ͨ����Ŀk = 0;	
		
		//ͨ��(�������Ӻ�)����
		
		//���ڲ���
		Main_Log("***��ʼ���ڲ���");
		bzero(TestFlag,sizeof(TestFlag));
		for(i=0; i<=6; i++){
			//Main_Log("��ʼ����[%d]����",i);
			if ( pthread_create( &ThreadSrFlag[i], NULL, (void *)Main_Com_Thread, (void *)i ) )  {
				Main_Log("��������[%d] �̴߳���",i);
				abort();
			}
			sleep(1);
		}		
			
		
		//memcpy(&Main_Result[2],TestFlag,1);
		//Main_Result[2] = TestFlag[0];
		
		//USB�ڲ���
		//if(2 == FccNum){
		/*Main_Log("***��ʼ���ƿ�(����0)����",i);
		ret = Main_ComSendRecv(0);
		if(ret <0){
			Main_Log("***���ƿ�(����0)����ʧ��",i);
			//exit(0);
		}else{
			TestFlag[0] = TestFlag[0] | (1<<7 );					
			Main_Log("***���ƿ�(����0)���Գɹ�",i);
		}
		Main_Result[2] =Main_Result[2] | TestFlag[0];*/
		Main_Log("***��ʼUSB�ڲ���");
		bzero(TestFlag,sizeof(TestFlag));
		//USB1�ڲ���
		Main_Log("***��ʼUSB��1����");
		ret = Main_readUSB(1);
		if(ret <0){
			Main_Log("***USB��[1]����ʧ��");
			//exit(0);
			sleep(1);
			//USB2�ڲ���
			Main_Log("***��ʼUSB��2����");
			ret = Main_readUSB(2);
			if(ret <0){
						
			}else{
				Main_Log("***USB��[2]���Գɹ�");
				TestFlag[0] = TestFlag[0] | 0x01;//0x40
			}			
		}else{
			Main_Log("***USB��[1]���Գɹ�");
			TestFlag[0] = TestFlag[0] | 0x01;//0x80
		}		
		Main_Result[2] = Main_Result[2] | TestFlag[0];
		//}else{
		//	Main_Log("���Ǵ�FCC[%d],�����Կ��ƿ�(����0)��USB��",FccNum);
		//}
		/*for(i=0;i<k;i++){
			if ( pthread_join ( ThreadChFlag[i], NULL ) ) {
				Main_Log("joining ͨ��[%d]�̴߳���",i+1);
				ret = Main_PortReadEnd();
				abort();
			}else{
				Main_Log("joining ͨ��[%d]�߳̽���",i+1);
			}
		}*/
		for(i=0; i<=6; i++){
			if(0==ThreadSrFlag[i]){
				continue;
			}else if ( pthread_join ( ThreadSrFlag[i], NULL ) ) {
				Main_Log("joining����[%d]�̴߳���",i);
				abort();
			}else{
				Main_Log("joining����[%d]�߳̽���",i);
			}
		}
			
		
		/*ret = Main_SynStart(1);//ͬ��
		if( ret < 0 ) {
			Main_Log( "ͨ��������ʧ��" );
			return -1;
		}*/
		//���Ͳ��Խ��
		n = 2;
		// if(2 == FccNum){
	 	ret = Main_TCPSendRecv(Main_Result,3, TestFlag,&n);
		if(ret <0){
			Main_Log("���Ͳ��Խ������,�˳�");
			exit(-1);
		}else{
			if( (0x22 != TestFlag[0]) || (0x0 != TestFlag[1]) ){//0 != memcmp(TestFlag,"2200",2)
				Main_Log("���Ͳ��Խ��,�յ�������[%02x%02x]����,�˳�",TestFlag[0],TestFlag[1]);
				exit(-1);
			}
		}
		//k = 0;//����ͨ����Ŀ	
		//k=portChanel[3]+portChanel[4]+portChanel[5]+portChanel[6];
		bzero(TestFlag,sizeof(TestFlag));
		TestFlag[0] =1;
		if((0xff&Main_Result[2]) !=0xff){//����ֻҪ0,1,2�����ɹ��Ϳ�����
			TestFlag[0] = 0;
			Main_Log("����0,1,2,..,6��USB������һ��û�в���ͨ��[%02x]",Main_Result[2]);
			//exit(0);
		}
		
		if(1 != TestFlag[0] ){
			Main_Log("***FCC�������ʧ��,��ر�FCC����,������,�ȴ��ٴβ���");
			exit(-1);
		}else{
		 	Main_Log("***FCC������Գɹ�,�ȴ���һ��FCC�������");
			exit(0);
		 }
		
	}
	 //Main_AcceptPCCmd();
	
	 Main_Log("***�Բ��Գ����쳣�˳�");
	return -1;
}








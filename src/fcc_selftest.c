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

//���ź�������ֵ
#define TEST_SEM_FILE_INDEX 1000 
#define TEST_SEM_COM3 1003
#define TEST_SEM_COM4 1004
#define TEST_SEM_COM5 1005
#define TEST_SEM_COM6 1006
#define TEST_SEM_RESULT 1100

#define USB_DEV_SDA 0	//USB���豸��
#define USB_DEV_SDB 16
#define USB_DEV_SDC 32
#define USB_DEV_SDD 48
//----------------------------------------------
//ʵʱ��־����
//===============================
#define TEST_MAX_VARIABLE_PARA_LEN 1024 
#define TEST_MAX_FILE_NAME_LEN 1024 
static char gsFccFileName[TEST_MAX_FILE_NAME_LEN + 1];	//��¼������Ϣ���ļ���
static int giFccLine;								//��¼������Ϣ����
static int giFccFd;                                                       //��־�ļ����
static char gcFccLogFlag;
static unsigned  char  gsLocalIP[16];
//�Զ���ּ��ļ���־,��__Log.
#define Test_Log Test_SetFileAndLine( __FILE__, __LINE__ ), Test__Log
//#define Test_Log Test_SetFileAndLine( __FILE__, __LINE__ ),Test__FileLog
//--------------------------------------------------------------

//-------------------------------------------------------
//���ڶ���
//��������,���ڴ�0��ʼ
#define START_PORT 3
#define MAX_PORT 6
#define PORT_USED_COUNT 3
#define RECLEN 16
//�����ַ���
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
//����ͨ����Ŀ,������ͨ��ֵΪ0.
unsigned char portChanel[MAX_PORT+1];
//----------------------------------------------------

//-------------------------------------------
//��ʼ�����ò���
//-------------------------------------------
static char PC_SERVER_IP[16]; //PC������IP��ַ
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
//��PCע��
/*FCC��PC ע��:
*FCC��:1104+����IP��ַ+4������ͨ����,PC�ظ�:110111���� 110122,����110111Ϊ1��,110122Ϊ2��FCC.*/
static unsigned char FccNum;
static unsigned char Gilbarco;
//-------------------------------------------------

//------------------------------------------------
static char Test_USB_UM[]="/bin/umount -l /home/ftp/";
static char Test_USB_File[]="/home/ftp/fcctest.txt";
static char Test_USB_Data[]="1234567890";
//---------------------------------------------

//------------------------------------------------
//����ظ�����
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
//ʵʱ��־
//----------------------------------------------
void Test_SetLogFd( const int fd )
{
	giFccFd= fd;
	if(  giFccFd <=  0){
		printf( " {��־�ļ�����������[%d] }\n", fd );
		
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
		printf("�������ݷ���ʧ��,error:[%s]", strerror(errno));	
	}*/		
	
}

//giLogLevel=0, no log;giLogLevel=1, only print log info; giLogLevel = 2, print log info and write log info to file 
void Test__Log(const char *fmt,...){//const int loglevel, const int fd, 
	va_list argptr;
	char sMessage[TEST_MAX_VARIABLE_PARA_LEN + 1];
	char fileName[TEST_MAX_FILE_NAME_LEN + 1];	/*��¼������Ϣ���ļ���*/
	int line;								/*��¼������Ϣ����*/
	//int level = giFccLogLevel;
	
	
	Test_GetFileAndLine(fileName, &line);	
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
	if(0==gcFccLogFlag)Test_SendUDP( giFccFd, sMessage, strlen(sMessage) );//write
	else{
		printf(sMessage);
		Test_SendUDP( giFccFd, sMessage, strlen(sMessage) );
	}
	//write( giFccFd, "\n", strlen("\n") );
	
	return;
}

int Test_InitLog(int flag){//flag---0: UDP��־,1:��ӡ��־
	int sockfd;
	unsigned char *pIP;
	struct in_addr in_ip;
    	 /*if( ( fd = open( TEST_LOG_FILE, O_WRONLY|O_CREAT|O_TRUNC, 00644 ) ) <= 0 ) {
		printf( "����־�ļ�[%s]ʧ��\n", TEST_LOG_FILE );
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
	else printf("Test_InitLog������־��ʼ������,flag[%d]\n",flag);
	//Test_SetLogLevel(loglevel);
	return 1;
}
//------------------------------------------------

//-----------------------------------------------------------------------
//���ú���
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
//��ȡ����IP,���ر���IP��ַ������-1ʧ�ܣ����DHCP����
static unsigned  long  Test_GetLocalIP(){//�μ�tcp.h��tcp.c�ļ�����,����ѡ��һ��.
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

	port = GetTcpPort( sPortName );	//���ݶ˿ںŻ��߶˿���ȡ�˿� ȡ����portΪ�����ֽ���//
	if( port == INPORT_NONE )	//GetTcpPort�Ѿ������˴���,�˴�ʡ��//
		return -1;
	*/

	sockFd = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
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
		Test_Log( "�����:%d", errno );
		return -1;
	}

	if( listen( sockFd, backLog ) < 0 )
	{
		Test_Log( "�����:%d",errno );
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

	Test_Log( "�����:%d",errno );
	return -1;
}
/*��nTimeOut����ΪTIME_INFINITE�������õ�ʱ���ϵͳȱʡʱ�䳤ʱ,
 connect����ɷ���ǰ��ֻ�ȴ�ϵͳȱʡʱ��*/
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
static ssize_t Test_TcpSendDataInTime( int fd, const char *s, size_t len,  int timeout )
{
#if 1
	ssize_t	 retlen;
	size_t	sendlen;
	char	*head;
	char	*t;
	char	buf[TCP_MAX_SIZE];
	int	mflag; 	/*��ʶ�Ƿ�����˿ռ� 0δ����. 1�ѷ���*/
	int	n;
	int     i;
	
	Assert( s != NULL && *s != 0 );
	Assert( len != 0  );
	Assert( timeout > 0 || timeout == TIME_INFINITE );

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
	
	Test_Log("before WirteLengthInTime -------------");
	n = WriteByLengthInTime( fd, t, sendlen, timeout );
	Test_Log("after WirteLengthInTime -------------");
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
	
	Test_Log("before WirteLengthInTime -------------");	
	n = WriteByLengthInTime( fd, t, sendlen, timeout );
	Test_Log("after WirteLengthInTime -----------length=[%d]",n);
	
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
		printf("�յ��ĳ���[%d]С��Ԥ���յ��ĳ���[%d],ֵ[%s]\n", n,len,Test_Asc2Hex(t, len));
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
//ȥ���ַ�������ո񼰻س����з�,��Ҫ�ַ�����������
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
//��ʼ�����ò���
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
	//���ļ�
	fd = open( fileName, O_WRONLY|O_CREAT|O_TRUNC, 00644 );
	if( fd < 0 )
	{
		//UserLog( "�򿪵�ǰ���й���·����Ϣ�ļ�[%s]ʧ��.",fileName );
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
			//Test_Log("�������ݳ���,  ����Ԥ�ڵ���������,������[%s],ֵ��[%s]",fcc_ini[i].name,fcc_ini[i].value);
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


static int Test_OpenSerial( char *ttynam ){
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
		//Test_Log( "�򿪴���ʧ��" );
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
 * func: ���������Ӻ�(�߼�ͨ����)ת��Ϊ����ͨ����
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
		Test_Log( "Test_GetPortChannel ʧ��.����" );
		return -1;
	}
	Test_Log("Test_InitPortChn��ȡ��portChnl[%s]",Test_Asc2Hex((unsigned char *)portChanel, MAX_PORT+1));
	return 0;
}

//��д����
int Test_WriteTty(int fd, unsigned char *msg, int msgLen){
	int ret;
	int timeout = 5;
	//unsigned char buff[64],*result;
	
	//fd = OpenSerial( TEST_alltty[serial_num]);
	if( fd <= 0 )
	{
		Test_Log( "����fd[%d]��ֵ����",fd );
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

//ͨ����ȡ���ݴ���,�˴�û��ʹ����Ϣ����,������ʱ�ļ�.
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
		Test_Log( "����fd[%d]��ֵ����",fd );
		return -1;
	}
	//Test_Log( "after Test_Writetty and 0 to moved test!" );
	len = *recvLen;
	if( sizeof(buff) < len)
	{
		Test_Log( "FccReadTty����,buff����[%d]С�ڴ����ճ���[%d]",sizeof(buff), len);
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
			default:Test_Log( "���ں�[%d]����",serialPortNum );
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
			default:Test_Log( "Test_PortRead���ں�[%d]����",serialPortNum );
				break;			
		}		
		if(ret <=0){
			Test_Log( "Test_PortRead�˿�[%02x]��[����%d]����ʧ��" ,phyChnl,serialPortNum);
			sleep(1);
			continue;
		}
		Test_Log("Test_PortRead[����%d][ͨ��%02x]��������[%s]",serialPortNum,phyChnl,Test_Asc2Hex(buff, len));	
		for(y=0;y<RECLEN;y++){
		       if(0x30+phyChnl-1 == buff[y]){
			Test_Log("Test_PortRead[����%d][ͨ��%02x]�������ݳɹ�",serialPortNum,phyChnl);
			return 1;//��ȡ�ɹ�//break;
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
			Test_Log("д�������ļ�[%s]",sTemp);
			close(fd);
			sleep(1);
			continue;
	}
	
	/*if( (0x30+phyChnl-1 == msg[0])||(0x30+phyChnl-1 == msg[1])){
		return 1;//��ȡ�ɹ�
	}*/	
	bzero(sTemp,sizeof(sTemp));
	sprintf(sTemp,"/tmp/testfcc/%02d%02x",serialPortNum,0x30+phyChnl-1);
	ret = Test_IsFileExist(sTemp);
	if(ret <= 0){
		Test_Log( "û�и�ͨ��[����%d][ͨ��%02x]����." ,serialPortNum,phyChnl);
		return -6;
	}else{
		unlink(sTemp);//ɾ�����ļ�
		Test_Log("ɾ���������ļ�[%s]",sTemp);
		return 1;//�����и�ͨ������
	}
}

//--------------------------------
//��PCע��
/*FCC��PC ע��:
*FCC��:1104+����IP��ַ+4������ͨ����,PC�ظ�:110111���� 110122,����110111Ϊ1��,110122Ϊ2��FCC.*/
//static unsigned char FccNum;
//static char PC_SERVER_IP[16]; //PC������IP��ַ
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
	
	
	//ͷ����������
	
	msgLength =6;
	sendBuffer[0] = 0x11;
	sendBuffer[1] = 0x04;	
	localIP = Test_GetLocalIP();
	
	//FCC�� IP��ַ
	memcpy(&sendBuffer[2],(unsigned char *)&localIP,4);

	//����ͨ����Ŀ
	memcpy(&sendBuffer[62],&portChanel[START_PORT],4);//(unsigned char *)&localIP
	//i=k;
	//��PC�ϴ�����
	Test_Log("��PC�ϴ�����[%s]",Test_Asc2Hex(sendBuffer, msgLength) );
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
				Test_Log("TcpSendDataInTime�������ݲ��ɹ�,Test_Login�������� ʧ��,IP[%s],port[%d]",sIPName,PC_SERVER_TCP_PORT);
				close(sockfd);
				sleep(1);//sleep(10000);
				continue;
			}else{				
				break;
			}
		} else {
			Test_Log("Tcp Connect ���ɹ�,Test_Login�������� ʧ��,IP[%s],port[%d]",sIPName,PC_SERVER_TCP_PORT);
			close(sockfd);
			sleep(1);//sleep(10000); 			
			continue;
			//return -1;
		}
	}
	 //��ʼ��PC���շ�������
	Test_Log("��ʼ��PC���շ�������");
	bzero(sendBuffer,sizeof(sendBuffer));
	for(i=0;i<tryn;i++){
		msgLength = 3;		
		ret =Test_TcpRecvDataInTime(sockfd, sendBuffer, &msgLength, timeout);			//Act_V(CD_SOCK_INDEX);			
		if (ret < 0 ){
			Test_Log("TcpRecvDataInTime�������ݲ��ɹ�,Test_Login�������� ʧ��");
			sleep(10);
			continue;
		}
		break;
	}
	
      if( (sendBuffer[0] != 0x11)||(sendBuffer[1] != 0x01) ){
	  	Test_Log("Tcp Connect ���ɹ�,Test_Login�������� ʧ��,[%02x%02x%02x]",sendBuffer[0],sendBuffer[1],sendBuffer[2]);
		close(sockfd);
		return -1;
	}
	//CSS_LevelLog("gpCSS_Shm->css._TLG_SEND_INTERVAL��ֵ[%02x%02x]",sendBuffer[1],sendBuffer[2]);
	Gilbarco = 0;
      	if(sendBuffer[2] == 0x11) {
			FccNum = 1;
			Test_Log("FccNum��ֵ[%d],��FCC",FccNum);
	}else if(sendBuffer[2] == 0x22){ 
		FccNum = 2;
		Test_Log("FccNum��ֵ[%d],��FCC",FccNum);
	}else if(sendBuffer[2] == 0x33){ 
		FccNum = 1;
		Gilbarco = 1;
		Test_Log("����ר��,FccNum��ֵ[%d],��FCC",FccNum);
	}else if(sendBuffer[2] == 0x44){ 
		FccNum = 2;
		Gilbarco = 1;
		Test_Log("����ר��,FccNum��ֵ[%d],��FCC",FccNum);
	}else{
		Test_Log("Test_Login�������ݴ���,Test_Login�������� ʧ��");
		return -1;
	}
	close(sockfd);
	//Test_Log("FccNum��ֵ[%d]",FccNum);
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
				Test_Log("TcpSendDataInTime�������ݲ��ɹ�,IP[%s],port[%d]",sIPName,PC_SERVER_TCP_PORT);
				close(sockfd);
				sleep(10);//sleep(10000);
				continue;
			}else{				
				break;
			}
		} else {
			Test_Log("Tcp Connect ���ɹ�,IP[%s],port[%d]",sIPName,PC_SERVER_TCP_PORT);
			close(sockfd);
			sleep(60);//sleep(10000); 			
			continue;
			//return -1;
		}
	}
	 //��PC���շ�������
	Test_Log("��PC���շ�������");
	 //sleep(5);//�ȴ��Է�ִ�����
	//bzero(recvBuffer,sizeof(recvBuffer));
	for(i=0;i<tryn;i++){
		msgLength = *recvLength;		
		ret =Test_TcpRecvDataInTime(sockfd, recvBuffer, &msgLength, timeout);			//Act_V(CD_SOCK_INDEX);			
		if (ret < 0 ){
			Test_Log("TcpRecvDataInTime�������ݲ��ɹ�");
			sleep(10);
			continue;
		}
		break;
	}
      if( (recvBuffer[0] == 0)&&(recvBuffer[1] == 0) ){
	  	Test_Log("Tcp Connect ���ɹ�,[%02x%02x%02x]",recvBuffer[0],recvBuffer[1],recvBuffer[2]);
		close(sockfd);
		return -1;
	}	
	close(sockfd);
	memcpy((char *)recvLength,(char *)&msgLength,sizeof(int));
	Test_Log("Test_SendRecv��������[%s]",Test_Asc2Hex(recvBuffer, msgLength));	
	return 0;
}


//--------------------------------------------------
//ͨѶ�������
//-----------------------------------------------
//static char PC_SERVER_IP[16]; //PC������IP��ַ
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
		Test_Log( "ʵʱ���̳�ʼ��socketʧ��.port=[%s]\n", TEST_TCP_PORT );
		//Free( cmd );
		return  ;
	}
	//fcntl(sock, F_SETFL, O_NONBLOCK);   // ������ģʽ
	
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
		tmp_sock = Test_TcpAccept(sock);//, NULL
		Test_Log("Test_WaitStartCmd����,soket��[%d]", tmp_sock);
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
				/*������*/
				Test_Log( "��������[%d]�ζ�ʧ��", tryNum );
				break; 
			}
			msg_lg = 3;
			sleep(10);
			ret = Test_TcpRecvDataInTime(newsock, buffer, &msg_lg, timeout );//TIME_INFINITE	
			if (ret < 0) {
				if (errno == 4) {  // time out
					Test_Log("���̳�ʱ,���յ���̨���� buffer[%s]\n" , Test_Asc2Hex(buffer, msg_lg) );//
					continue;
				} else if (errno == 11)  {  // maybe CD quit or restart, && (gpIfsf_shm->need_reconnect == 0))
					//gpIfsf_shm->need_reconnect = 1;
					Test_Log("Client need to reconnect to Server");
				}	
				Test_Log( "���̽�������ʧ��,errno:[%d][%s],buffer: %s", errno, strerror(errno), \
					Test_Asc2Hex(buffer, 2) );
				errcnt++;
				sleep(1);
				continue;
			} else {
				Test_Log( "<<<�����յ�����: [%s(%d)]", Test_Asc2Hex(buffer, msg_lg ), msg_lg);				
				if ( 0x22 != buffer[0] ){ //received data must be error!!(0 == (buffer[0]>>4) )||(0 ==( buffer[0]&0x0f) )
					Test_Log( "received data error" );
					errcnt++;
					sleep(1);
					continue;
				}

				if (0 == msg_lg){ //receivd data must be error!!
					Test_Log( "received size is  0 error" );
					errcnt++;
					//�ظ�����
					sBuffer[0] = 0x2f;
					sBuffer[1] = 0;
					//memcpy(&sBuffer[1],gpCSS_Shm->css._STATION_NO_BCD,5);
					ret = Test_TcpSendDataInTime(newsock, sBuffer, 2, timeout);
					if(ret <=0){				
						Test_Log("ʵʱ���̻ظ����� ʧ��");
					}
					
					sleep(1);
					continue;
				}

				
				
				//�����̨����.
				if (1 == buffer[1]){ 
					if(0x11 == buffer[2]){
						if(1 != FccNum) {
							Test_Log("PC�������� ����,������FCC");
							sBuffer[0] = 0x2f;
							sBuffer[1] = 0;
							//memcpy(&sBuffer[1],gpCSS_Shm->css._STATION_NO_BCD,5);
							ret = Test_TcpSendDataInTime(newsock, sBuffer, 2, timeout);
							if(ret <0){				
								Test_Log("���̻ظ����� ʧ��");
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
							Test_Log("���̻ظ����� ʧ��");
							//continue;
						}
						errcnt = 0;
						close(newsock);close(sock);
						return 1;//break;
					}else if(0x22 == buffer[2]){
						if(2 != FccNum) {
							Test_Log("PC�������� ����,������FCC");
							sBuffer[0] = 0x2f;
							sBuffer[1] = 0;
							//memcpy(&sBuffer[1],gpCSS_Shm->css._STATION_NO_BCD,5);
							ret = Test_TcpSendDataInTime(newsock, sBuffer, 2, timeout);
							if(ret <0){				
								Test_Log("���̻ظ����� ʧ��");
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
							Test_Log("���̻ظ����� ʧ��");
							//continue;
						}
						errcnt = 0;
						close(newsock);close(sock);
						return 1;//break;
					}else{
						Test_Log( "��������ʧ��,�յ�������ֵ����0x11��0x22" );
					}
				}  else{
					Test_Log( "��������ʧ��,�յ������ݵĳ����ֽڲ���1" );
				}				
				break;
				//sleep(10);			
			}
		}
		if ( errcnt > 40 ) {
			/*������*/
			Test_Log( "��ʼ��socket ʧ��.port=[%d]", TEST_TCP_PORT );
			break; 
		}
	}

	Test_Log("Test_AcceptPCCmd  return ......");
	close(sock);

	return  -1;
}


//ͨ���շ�����
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
		Test_Log( "serialPortNum < 0����" );
		return -1;
	}
	phyChnl = Test_GetChnPhy(chnlog,portChanel);
	if(phyChnl <=0){
		Test_Log( "phyChnl <=0����" );
		return -1;
	}
	fd = Test_OpenSerial(TEST_alltty[serialPortNum]);
	if(fd <=0){
		Test_Log( "Test_PortSendRecv�򿪴��ڴ���" );
		return -1;
	}
	Test_Log( "Test_PortSendRecv��ʼ����ͨ��[%d]" ,chnlog);
	//Test_Log("��PC��������");
	for(i=0;i<tryn;i++){
		memset(cmd,0x5a,sizeof(cmd));
		cmd[0] = 0x1B;
		cmd[1] = 0x30+phyChnl-1;
		
		if( sizeof(cmd) > MAX_PORT_CMD_LEN ) {
			Test_Log( "��˿�д�����ݳ���[%d]������󳤶�[%d]", sizeof(cmd), MAX_PORT_CMD_LEN );
			close(fd);
			return -1;
		}		 
		cmd[2] = 0x00+ (sizeof(cmd) -3 ) ;
		//memcpy( cmd+3, buff, len );
		cnt =sizeof(cmd);
		sleep(3);
		Test_Log("Test_SendRecv��������[%s]",Test_Asc2Hex(cmd, cnt));
		switch(serialPortNum){
			case 3:Test_Act_P(TEST_SEM_COM3);
				break;
			case 4:Test_Act_P(TEST_SEM_COM4);
				break;
			case 5:Test_Act_P(TEST_SEM_COM5);
				break;
			case 6:Test_Act_P(TEST_SEM_COM6);
				break;
			default:Test_Log( "���ں�[%d]����",serialPortNum );
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
			default:Test_Log( "���ں�[%d]����",serialPortNum );
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
			default:Test_Log( "���ں�[%d]����",serialPortNum );
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
			default:Test_Log( "���ں�[%d]����",serialPortNum );
				break;			
		}
		if(ret <=0){
			Test_Log( "��˿�д����ʧ��" );
			sleep(5);
			continue;
		}
		//break;
		sleep(3);//�ȴ��Է�ִ�����
		bzero(buff,sizeof(buff));
		cnt = RECLEN;//58
		ret = Test_PortRead(fd, serialPortNum, phyChnl, buff, &cnt);
		if(ret == 1){
			/*//�ɹ��ٶ�һ��,ȷ�����͵Ķ���ȡ��
			//sleep(1);//�ȴ��Է�ִ�����
			bzero(buff,sizeof(buff));
			cnt = RECLEN;//58
			ret = Test_PortRead(fd, serialPortNum, phyChnl, buff, &cnt);
			ret = 1;//�ָ��ɹ�״� */
			break;
		}else{
			Test_Log( "Test_PortRead��[����%d][ͨ��%02x]����ʧ��" ,serialPortNum, phyChnl );
			//���ɹ�����һ��
			//sleep(1);//�ȴ��Է�ִ�����
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
			default:Test_Log( "���ں�[%d]����",serialPortNum );
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
			default:Test_Log( "���ں�[%d]����",serialPortNum );
				break;			
		}		
		if(ret <=0){
			Test_Log( "�˿ڶ�����ʧ��" );
			sleep(1);
			continue;
		}
		Test_Log("Test_SendRecv��������[%s]",Test_Asc2Hex(buff, cnt));	
		if(  (0x30+phyChnl-1 == buff[0])||(0x30+phyChnl-1 == buff[1])){
			break;
		}else{
			sleep(1);
			continue;
		}*/
	}
	if( (ret <=0) ||(i >=tryn) ){
		Test_Log( "�Ӷ˿�[%d]������[%d]��ȫ��ʧ��" ,chnlog,tryn);
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
		Test_Log( "serialPortNum < 0����" );
		return -1;
	}
	phyChnl = Test_GetChnPhy(chnlog,portChanel);	
	if(phyChnl <=0){
		Test_Log( "phyChnl <=0����" );
		return -1;
	}
	fd = Test_OpenSerial(TEST_alltty[serialPortNum]);
	if(fd <=0){
		Test_Log( "Test_PortRecvSend�򿪴��ڴ���" );
		return -1;
	}
	Test_Log( "Test_PortRecvSend��ʼ����ͨ��[%d]" ,chnlog);
	 //��PC���շ�������
	//Test_Log("��PC���շ�������");
	for(i=0;i<tryn;i++){		
		sleep(1);//�ȴ��Է�ִ�����
		bzero(buff,sizeof(buff));
		cnt =RECLEN;//58
		ret = Test_PortRead(fd, serialPortNum, phyChnl, buff, &cnt);
		if(ret <0){
			Test_Log( "Test_PortRead������ʧ��" );
			//sleep(1);
			continue;	
		}
		memset(cmd,0x5a,sizeof(cmd));
		cmd[0] = 0x1B;
		cmd[1] = 0x30+phyChnl-1;
		
		if( sizeof(cmd) > MAX_PORT_CMD_LEN ) {
			Test_Log( "��˿�д�����ݳ���[%d]������󳤶�[%d]", sizeof(cmd), MAX_PORT_CMD_LEN );
			close(fd);
			return -1;
		}		 
		cmd[2] = 0x00+ (sizeof(cmd) -3 ) ;
		//memcpy( cmd+3, buff, len );
		cnt =sizeof(cmd);
		sleep(1);
		Test_Log("Test_SendRecv��������[%s]",Test_Asc2Hex(cmd, cnt));
		switch(serialPortNum){
			case 3:Test_Act_P(TEST_SEM_COM3);
				break;
			case 4:Test_Act_P(TEST_SEM_COM4);
				break;
			case 5:Test_Act_P(TEST_SEM_COM5);
				break;
			case 6:Test_Act_P(TEST_SEM_COM6);
				break;
			default:Test_Log( "���ں�[%d]����",serialPortNum );
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
			default:Test_Log( "���ں�[%d]����",serialPortNum );
				break;			
		}
		//ret = Test_WriteTty(fd, cmd, cnt);
		if(ret <=0){
			Test_Log( "��˿�д����ʧ��" );
			sleep(1);
			continue;
		}
		sleep(1);//�ٷ���һ��
		switch(serialPortNum){
			case 3:Test_Act_P(TEST_SEM_COM3);
				break;
			case 4:Test_Act_P(TEST_SEM_COM4);
				break;
			case 5:Test_Act_P(TEST_SEM_COM5);
				break;
			case 6:Test_Act_P(TEST_SEM_COM6);
				break;
			default:Test_Log( "���ں�[%d]����",serialPortNum );
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
			default:Test_Log( "���ں�[%d]����",serialPortNum );
				break;			
		}
		//ret = Test_WriteTty(fd, cmd, cnt);
		if(ret <=0){
			Test_Log( "��˿�д����ʧ��" );
			sleep(1);
			continue;
		}
		break;
	}
	if( (ret <=0) ||(i >=tryn) ){
		Test_Log( "�Ӷ˿�[%d]������[%d]��ȫ��ʧ��" ,chnlog, tryn);
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
		Test_Log( "serialPortNum < 0����" );
		return -1;
	}
	phyChnl = Test_GetChnPhy(chnlog,portChanel);
	if(phyChnl <=0){
		Test_Log( "phyChnl <=0����" );
		return -1;
	}
	fd = Test_OpenSerial(TEST_alltty[serialPortNum]);
	if(fd <=0){
		Test_Log( "Test_PortSendRecv�򿪴��ڴ���" );
		return -1;
	}
	Test_Log( "Test_PortSendRecv��ʼ����ͨ��[%d]" ,chnlog);
	//Test_Log("��PC��������");
	for(i=0;i<tryn;i++){
		memset(cmd,0x5a,sizeof(cmd));
		cmd[0] = 0x1B;
		cmd[1] = 0x30+phyChnl-1;
		
		if( sizeof(cmd) > MAX_PORT_CMD_LEN ) {
			Test_Log( "��˿�д�����ݳ���[%d]������󳤶�[%d]", sizeof(cmd), MAX_PORT_CMD_LEN );
			close(fd);
			return -1;
		}		 
		cmd[2] = 0x00+ (sizeof(cmd) -3 ) ;
		//memcpy( cmd+3, buff, len );
		cnt =sizeof(cmd);
		sleep(3);
		Test_Log("Test_SendRecv��������[%s]",Test_Asc2Hex(cmd, cnt));
		switch(serialPortNum){
			case 3:Test_Act_P(TEST_SEM_COM3);
				break;
			case 4:Test_Act_P(TEST_SEM_COM4);
				break;
			case 5:Test_Act_P(TEST_SEM_COM5);
				break;
			case 6:Test_Act_P(TEST_SEM_COM6);
				break;
			default:Test_Log( "���ں�[%d]����",serialPortNum );
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
			default:Test_Log( "���ں�[%d]����",serialPortNum );
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
			default:Test_Log( "���ں�[%d]����",serialPortNum );
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
			default:Test_Log( "���ں�[%d]����",serialPortNum );
				break;			
		}*/
		if(ret <=0){
			Test_Log( "��˿�д����ʧ��" );
			sleep(5);
			continue;
		}
		//break;
		sleep(10);//�ȴ��Է�ִ�����
		/*
		bzero(buff,sizeof(buff));
		cnt = RECLEN;//58
		ret = Test_PortRead(fd, serialPortNum, phyChnl, buff, &cnt);
		if(ret == 1){
			///�ɹ��ٶ�һ��,ȷ�����͵Ķ���ȡ��
			//sleep(1);//�ȴ��Է�ִ�����
			//bzero(buff,sizeof(buff));
			//cnt = RECLEN;//58
			//ret = Test_PortRead(fd, serialPortNum, phyChnl, buff, &cnt);
			//ret = 1;//�ָ��ɹ�״//
			break;
		}else{
			Test_Log( "Test_PortRead��[����%d][ͨ��%02x]����ʧ��" ,serialPortNum, phyChnl );
			//���ɹ�����һ��
			//sleep(1);//�ȴ��Է�ִ�����
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
			default:Test_Log( "���ں�[%d]����",serialPortNum );
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
			default:Test_Log( "���ں�[%d]����",serialPortNum );
				break;			
		}		
		if(ret <=0){
			Test_Log( "�˿ڶ�����ʧ��" );
			sleep(1);
			continue;
		}
		Test_Log("Test_SendRecv��������[%s]",Test_Asc2Hex(buff, cnt));	
		if(  (0x30+phyChnl-1 == buff[0])||(0x30+phyChnl-1 == buff[1])){
			break;
		}else{
			sleep(1);
			continue;
		}*/
	}
	if( (ret <=0) ||(i >=tryn) ){
		Test_Log( "�Ӷ˿�[%d]������[%d]��ȫ��ʧ��" ,chnlog,tryn);
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
		Test_Log( "serialPortNum < 0����" );
		return -1;
	}
	phyChnl = Test_GetChnPhy(chnlog,portChanel);	
	if(phyChnl <=0){
		Test_Log( "phyChnl <=0����" );
		return -1;
	}
	fd = Test_OpenSerial(TEST_alltty[serialPortNum]);
	if(fd <=0){
		Test_Log( "Test_PortRecv�򿪴��ڴ���" );
		return -1;
	}
	Test_Log( "Test_PortRecv��ʼ����ͨ��[%d]" ,chnlog);
	 //��PC���շ�������
	//Test_Log("��PC���շ�������");
	for(i=0;i<tryn;i++){		
		sleep(1);//�ȴ��Է�ִ�����
		bzero(buff,sizeof(buff));
		cnt =RECLEN;//58
		ret = Test_PortRead(fd, serialPortNum, phyChnl, buff, &cnt);
		if(ret <=0){
			Test_Log( "Test_PortRead������ʧ��" );
			sleep(10);
			continue;	
		}else{
			Test_Log( "Test_PortRead�����ݳɹ�" );
			return 1;
		}
		/*sleep(10);
		
		memset(cmd,0x5a,sizeof(cmd));
		cmd[0] = 0x1B;
		cmd[1] = 0x30+phyChnl-1;
		
		if( sizeof(cmd) > MAX_PORT_CMD_LEN ) {
			Test_Log( "��˿�д�����ݳ���[%d]������󳤶�[%d]", sizeof(cmd), MAX_PORT_CMD_LEN );
			close(fd);
			return -1;
		}		 
		cmd[2] = 0x00+ (sizeof(cmd) -3 ) ;
		//memcpy( cmd+3, buff, len );
		cnt =sizeof(cmd);
		sleep(1);
		Test_Log("Test_SendRecv��������[%s]",Test_Asc2Hex(cmd, cnt));
		switch(serialPortNum){
			case 3:Test_Act_P(TEST_SEM_COM3);
				break;
			case 4:Test_Act_P(TEST_SEM_COM4);
				break;
			case 5:Test_Act_P(TEST_SEM_COM5);
				break;
			case 6:Test_Act_P(TEST_SEM_COM6);
				break;
			default:Test_Log( "���ں�[%d]����",serialPortNum );
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
			default:Test_Log( "���ں�[%d]����",serialPortNum );
				break;			
		}
		//ret = Test_WriteTty(fd, cmd, cnt);
		if(ret <=0){
			Test_Log( "��˿�д����ʧ��" );
			sleep(1);
			continue;
		}
		sleep(1);//�ٷ���һ��
		switch(serialPortNum){
			case 3:Test_Act_P(TEST_SEM_COM3);
				break;
			case 4:Test_Act_P(TEST_SEM_COM4);
				break;
			case 5:Test_Act_P(TEST_SEM_COM5);
				break;
			case 6:Test_Act_P(TEST_SEM_COM6);
				break;
			default:Test_Log( "���ں�[%d]����",serialPortNum );
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
			default:Test_Log( "���ں�[%d]����",serialPortNum );
				break;			
		}
		//ret = Test_WriteTty(fd, cmd, cnt);
		if(ret <=0){
			Test_Log( "��˿�д����ʧ��" );
			sleep(1);
			continue;
		}
		break;*/
	}
	if( (ret <=0) ||(i >=tryn) ){
		Test_Log( "�Ӷ˿�[%d]������[%d]��ȫ��ʧ��" ,chnlog, tryn);
		close(fd);
		return -1;
	}	
	close(fd);
	return 0;
}
//�����շ�����
int Test_ComSendRecv(int serialPortNum){
	int i, ret, tmp,fd,n,k;//,, sockfd
	unsigned char buff[64] ;
	int timeout = 5, tryn = 3;
	char		cmd[16];
	int			cnt;
	
	
	fd = Test_OpenSerial(TEST_alltty[serialPortNum]);
	if(fd <=0){
		Test_Log( "Test_ComRecvSend�򿪴��ڴ���" );
		return -1;
	}
	//Test_Log("��PC��������");
	Test_Log( "Test_ComSendRecv��ʼ���Դ���[%d]" ,serialPortNum);
	for(i=0;i<tryn;i++){
		memset(cmd,0xa5,sizeof(cmd));
		if( sizeof(cmd) > MAX_PORT_CMD_LEN ) {
			Test_Log( "�򴮿�[%d]д�����ݳ���[%d]������󳤶�[%d]", serialPortNum,sizeof(cmd), MAX_PORT_CMD_LEN );
			close(fd);
			return -1;
		}		 
		//cmd[2] = 0x00+ (sizeof(cmd) -3 ) ;
		//memcpy( cmd+3, buff, len );
		cnt =sizeof(cmd);
		Test_Log("Test_ComRecvSend��������[%s]",Test_Asc2Hex(cmd, cnt));
		ret = Test_WriteTty(fd, cmd, cnt);
		if(ret <=0){
			Test_Log( "�򴮿�[%d]д����ʧ��" ,serialPortNum);
			sleep(5);
			continue;
		}
		//break;
		sleep(2);
		bzero(buff,sizeof(buff));
		cnt =sizeof(cmd);
		ret = Test_ReadTty(fd, buff, &cnt);
		if(ret <=0){
			Test_Log( "����[%d]������ʧ��",serialPortNum );
			sleep(5);
			continue;
		}
		Test_Log("Test_ComRecvSend��������[%s]",Test_Asc2Hex(buff, cnt));	
		break;
	}
	if( (ret <=0) ||(i >=tryn) ){
		Test_Log( "�Ӵ���[%d]������[%d]��ȫ��ʧ��" ,serialPortNum,tryn);
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
		Test_Log( "Test_ComRecvSend�򿪴��ڴ���" );
		return -1;
	}
	//Test_Log("��PC��������");
	Test_Log( "Test_ComRecvSend��ʼ���Դ���[%d]" ,serialPortNum);
	for(i=0;i<tryn;i++){		
		sleep(1);//�ȴ��Է�ִ�����
		bzero(buff,sizeof(buff));
		cnt =sizeof(cmd);
		ret = Test_ReadTty(fd, buff, &cnt);
		if(ret <=0){
			Test_Log( "����[%d]������ʧ��",serialPortNum );
			sleep(5);
			continue;
		}
		Test_Log("Test_ComRecvSend��������[%s]",Test_Asc2Hex(buff, cnt));
		sleep(5);
		//break;
		memset(cmd,0x5a,sizeof(cmd));
		if( sizeof(cmd) > MAX_PORT_CMD_LEN ) {
			Test_Log( "�򴮿�[%d]д�����ݳ���[%d]������󳤶�[%d]", serialPortNum,sizeof(cmd), MAX_PORT_CMD_LEN );
			close(fd);
			return -1;
		}		 
		//cmd[2] = 0x00+ (sizeof(cmd) -3 ) ;
		//memcpy( cmd+3, buff, len );
		cnt =sizeof(cmd);
		Test_Log("Test_ComRecvSend��������[%s]",Test_Asc2Hex(cmd, cnt));
		ret = Test_WriteTty(fd, cmd, cnt);
		if(ret <=0){
			Test_Log( "�򴮿�[%d]д����ʧ��" ,serialPortNum);
			sleep(5);
			continue;
		}
		sleep(2);//�ٷ���һ��
		cnt =sizeof(cmd);
		Test_Log("Test_ComRecvSend��������[%s]",Test_Asc2Hex(cmd, cnt));
		ret = Test_WriteTty(fd, cmd, cnt);
		if(ret <=0){
			Test_Log( "�򴮿�[%d]д����ʧ��" ,serialPortNum);
			sleep(5);
			continue;
		}
		break;
	}
	if( (ret <=0) ||(i >=tryn) ){
		Test_Log( "�Ӵ���[%d]������[%d]��ȫ��ʧ��" ,serialPortNum,tryn);
		close(fd);
		return -1;
	}
	
	close(fd);
	return 0;
}


//���ݼ�⵽��U�����ƹ���U��
static int Main_mountUsb(){

	unsigned char *fileName = "/proc/partitions";
	unsigned char buff[64],usbDev[4],devName[64],mountName[128],rmDev[64];
	FILE *fp;
	int i, j,ret, first_sdx;
	
	fp = fopen(fileName, "rt");
	if(fp == NULL){
		char pbuff[256];
		bzero(pbuff,sizeof(pbuff));
		sprintf( pbuff, "fileName[%s] ��ʧ��!",fileName);
		Test_Log(pbuff);
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
		char pbuff[256];
		bzero(pbuff,sizeof(pbuff));
		sprintf( pbuff, "��ȡU������ʧ��!buff[%s]", buff);
		Test_Log(pbuff);
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
		char pbuff[256];
		bzero(pbuff,sizeof(pbuff));
		sprintf( pbuff, "devName[%s] ����U���豸�ļ�ʧ��!", devName);
		Test_Log(pbuff);
		return -1;
	}

	//ж�ع�����
	system(Test_USB_UM);
	//����U��
	bzero(mountName,sizeof(mountName));
	sprintf(mountName,"/bin/mount -t vfat /dev/sd%s /home/ftp",usbDev);
	ret = system(mountName);
	if(ret < 0){
		char pbuff[256];
		bzero(pbuff,sizeof(pbuff));
		sprintf( pbuff, "mountName[%s] ����U��ʧ��!", mountName );
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
		Test_Log("Test_readUSB����ֵ����,ֵ[%d]",usbnum);
		return -1;
	}
	
	//�ҽ�U��
      	ret = Main_mountUsb();
	if( ret < 0 )
	{
		char pbuff[256];
		bzero(pbuff,sizeof(pbuff));
		sprintf( pbuff, "***����USB����ִ��ʧ��");
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
	//ȥ���ҽ�U��
	ret = system(Test_USB_UM);
	if( ret < 0 )
	{
		Test_Log( "***USB����[%s]ִ��ʧ��\n" ,Test_USB_UM);
		return -1;//exit(1);
	}	
	if(  0 != memcmp( my_data,Test_USB_Data,sizeof(Test_USB_Data) )  ){
		Test_Log( "��ȡ������[%s]��Ӧ����ȡ��������[%s]��ͬ\n" ,my_data,Test_USB_Data);
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
		Test_Log( "ʵʱ���̳�ʼ��socketʧ��.port=[%s]\n", TEST_TCP_PORT );
		//Free( cmd );
		return  ;
	}
	//fcntl(sock, F_SETFL, O_NONBLOCK);   // ������ģʽ
	
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
		tmp_sock = Test_TcpAccept(sock);//, NULL
		Test_Log("Test_AcceptPCCmd����,soket��[%d]", tmp_sock);
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
				Test_Log( "��������[%d]�ζ�ʧ��", tryNum );
				break; 
			}
			ret = Test_TcpRecvDataInTime(newsock, buffer, &msg_lg, timeout );//TIME_INFINITE	
			if (ret < 0) {
				if (errno == 4) {  // time out
					Test_Log("���̳�ʱ,���յ���̨���� buffer[%s]\n" , Test_Asc2Hex(buffer, 32) );//
					continue;
				} else if (errno == 11)  {  // maybe CD quit or restart, && (gpIfsf_shm->need_reconnect == 0))
					//gpIfsf_shm->need_reconnect = 1;
					Test_Log("Client need to reconnect to Server");
				}	
				Test_Log( "���̽�������ʧ��,errno:[%d][%s],buffer: %s", errno, strerror(errno), \
					Test_Asc2Hex(buffer, 2) );
				errcnt++;
				sleep(1);
				continue;
			} else {
				Test_Log( "<<<�����յ�����: [%s(%d)]", Test_Asc2Hex(buffer, msg_lg ), msg_lg);				
				if ( (0 == (buffer[0]>>4) )||(0 ==( buffer[0]&0x0f) )  ){ //received data must be error!!
					Test_Log( "received data error" );
					errcnt++;
					sleep(1);
					continue;
				}

				if (0 == msg_lg){ //receivd data must be error!!
					Test_Log( "received size is  0 error" );
					errcnt++;
					//�ظ�����
					sBuffer[0] = 0x1f;
					sBuffer[1] = 0;
					//memcpy(&sBuffer[1],gpCSS_Shm->css._STATION_NO_BCD,5);
					ret = Test_TcpSendDataInTime(newsock, sBuffer, 2, timeout);
					if(ret <=0){				
						Test_Log("ʵʱ���̻ظ����� ʧ��");
					}
					
					sleep(1);
					continue;
				}

				
				
				//�����̨����.
				Test_Log( "��ʼ�����յ�����: [%s(%d)]", Test_Asc2Hex(buffer, msg_lg ), msg_lg);
				if (0 == buffer[1]){ 
					if(0x99 == buffer[0]){
						Test_Log( "�յ��˳�����: [%s(%d)],�˳�", Test_Asc2Hex(buffer, msg_lg ), msg_lg);
						
					}
				}
				
				break;
				//sleep(10);			
			}
		}
		if ( errcnt > 40 ) {
			/*������*/
			Test_Log( "��ʼ��socket ʧ��.port=[%d]", TEST_TCP_PORT );
			break; 
		}
	}

	Test_Log("Test_AcceptPCCmd  Stop ......");
	close(sock);

	return  ;
}
/*//����FCCͬ��
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
		Test_Log( "***Fcc_SynStart��������,�����˳�" );
		exit(-1);
	}
	while(1){
		n = 3;
		ret = Test_TCPSendRecv(WaitSendBuff[NumFcc-1],5, WaitRecvBuff,&n);
		if(ret <0){
			Test_Log("���Ͳ����м�ͬ��ֵ�������");
			sleep(5);
			continue;
		}else{
			if( (0xff != WaitRecvBuff[0]) || (0x01 != WaitRecvBuff[1])|| (WaitFlag[NumFcc-1] != WaitRecvBuff[2]) ){//0 != memcmp(TestFlag,"2200",2)
				sleep(2);
				continue;
			}else{
				Test_Log("�����м�ͬ���ɹ�,��������");
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
		Test_Log("Test_Com_Thread����ֵ����,ֵ[%d],��ʾ���ں�",sn);
		if(2 == FccNum){
			Test_Log("***����[%d]����ʧ��",sn);
		}else{
			Test_Log("����[%d]����ʧ��",sn);
		}
		return;
	}
	if(2 == FccNum){
		Test_Log("***��ʼ����[%d]����",sn);		
		ret = Test_ComRecvSend(sn);
		if(ret <0){			
			Test_Log("***����[%d]����ʧ��",sn);				
		}else{
			Test_Result[2]  = Test_Result[2]  | (1<<(8-sn-1) );
			Test_Log("***����[%d]���Գɹ�",sn);
		}
	}else if(1 == FccNum){
		Test_Log("��ʼ����[%d]����",sn);
		ret = Test_ComSendRecv(sn);
		if(ret <0){			
			Test_Log("����[%d]����ʧ��",sn);				
		}else{
			Test_Result[2]  = Test_Result[2]  | (1<<(8-sn-1) );
			Test_Log("����[%d]���Գɹ�",sn);
		}
	}else{
		Test_Log("***����FCC��־����[%d],�˳�",FccNum);
		ret = Test_PortReadEnd();
		exit(0);
	}
	//return;
}
void Test_Port_Thread(void  *arg){//argͨ����-1
	int i,k,n,ret;//,spn
	int  sn;
	sn = (int *)arg;
	//sn = (int ) *arg;
	sn++;//snͨ����
	if(sn<=0 ||sn >64){//
		Test_Log("Test_Port_Thread����ֵ����,ֵ[%d],��ʾͨ����",sn);
		return;
	}
	//����ͨ����Ŀ	
	k=portChanel[3]+portChanel[4]+portChanel[5]+portChanel[6];
	/*//���㴮�ں�spn
	spn = sn;
	for(i=3;i<=6;i++){
		if((spn-=portChanel[i])<=0){
			spn = i;
			break;
		}
	}*/
	if(1 == FccNum){
		n = ( (sn-1) / 8) +1;
		Test_Log( "��ʼ����ͨ��[%d]" ,sn);
		ret = Test_PortSendRecv(sn);
		if(ret <0){
			Test_Log("ͨ��[%d]����ʧ��",sn);
		}else{
			Test_Act_P(TEST_SEM_RESULT);
			Test_Result[3+n-1] = Test_Result[3+n-1] | (1<<(8*n-sn) );
			Test_Act_V(TEST_SEM_RESULT);
			Test_Log("ͨ��[%d]���Գɹ�", sn);
		}				
	}else if(2 == FccNum){			
			n = ( (sn-1) / 8) +1;
			Test_Log( "***��ʼ����ͨ��[%d]" ,sn);
			ret = Test_PortRecvSend(sn);
			if(ret <0){
				Test_Log("***ͨ��[%d]����ʧ��",sn);
			}else{
				Test_Act_P(TEST_SEM_RESULT);
				Test_Result[3+n-1] = Test_Result[3+n-1] | (1<<(8*n-sn) );
				Test_Act_V(TEST_SEM_RESULT);
				Test_Log( "***ͨ��[%d]���Գɹ�" ,sn);
			}		
	}else{
		Test_Log("***����FCC��־����[%d],�˳�",FccNum);
		ret = Test_PortReadEnd();
		exit(0);
	}
}
//Gilbarcoר���߳�
void Test_Port_Thread_Grb(void  *arg){//argͨ����-1
	int i,k,n,ret;//,spn
	int  sn;
	sn = (int *)arg;
	//sn = (int )*arg;
	sn++;//snͨ����
	if(sn<=0 ||sn >64){//
		Test_Log("Test_Port_Thread����ֵ����,ֵ[%d],��ʾͨ����",sn);
		return;
	}
	//����ͨ����Ŀ	
	k=portChanel[3]+portChanel[4]+portChanel[5]+portChanel[6];
	/*//���㴮�ں�spn
	spn = sn;
	for(i=3;i<=6;i++){
		if((spn-=portChanel[i])<=0){
			spn = i;
			break;
		}
	}*/
	if(1 == FccNum){
		n = ( (sn-1) / 8) +1;
		Test_Log( "��ʼ����ͨ��[%d]" ,sn);
		ret = Test_PortSend(sn);
		if(ret <0){
			Test_Log("ͨ��[%d]����ʧ��",sn);
		}else{
			Test_Act_P(TEST_SEM_RESULT);
			Test_Result[3+n-1] = Test_Result[3+n-1] | (1<<(8*n-sn) );
			Test_Act_V(TEST_SEM_RESULT);
			Test_Log("ͨ��[%d]���Գɹ�", sn);
		}				
	}else if(2 == FccNum){			
			n = ( (sn-1) / 8) +1;
			Test_Log( "***��ʼ����ͨ��[%d]" ,sn);
			ret = Test_PortRecv(sn);
			if(ret <0){
				Test_Log("***ͨ��[%d]����ʧ��",sn);
			}else{
				Test_Act_P(TEST_SEM_RESULT);
				Test_Result[3+n-1] = Test_Result[3+n-1] | (1<<(8*n-sn) );
				Test_Act_V(TEST_SEM_RESULT);
				Test_Log( "***ͨ��[%d]���Գɹ�" ,sn);
			}		
	}else{
		Test_Log("***����FCC��־����[%d],�˳�",FccNum);
		ret = Test_PortReadEnd();
		exit(0);
	}
}


//����FCCͬ��
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
		Test_Log( "***Fcc_SynStart��������,�����˳�" );
		ret = Test_PortReadEnd();
		exit(-1);
	}
	while(1){
		n = 3;
		ret = Test_TCPSendRecv(WaitSendBuff[NumFcc-1],5, WaitRecvBuff,&n);
		if(ret <0){
			Test_Log("���Ͳ����м�ͬ��ֵ�������");
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


//������
int main( int argc, char* argv[] )
{
	//#define GUN_MAX_NUM 256 //���256��ǹ
	int i, j, n,k,len,ret,fd;//,pid,isChild
	unsigned char TestFlag[8];
	pthread_t ThreadChFlag[64];//ͨ���߳�����
	pthread_t ThreadSrFlag[8];//�����߳�����
	if (argc == 2) {
	       if (strcmp(argv[1], "-v") == 0) {
			printf( "%s version: %s\n", argv[0], PCD_VER);
	       } 
		return 0;
	}

	
	printf("��ʼ���в��Գ���\n");
	
	//��ʼ����־
	printf("��ʼ����־.\n");
	ret = Test_InitLog(1);//0: UDP��־,1:��ӡ��־
	if(ret < 0){
		printf("��־��ʼ������.\n");
		exit(-1);  
	}

		
	/*//��ʼ�������ڴ�
	ret = Test_InitTheKeyFile();
	if( ret < 0 )
	{
		Test_Log( "��ʼ��Key Fileʧ��\n" );
		exit(1);
	}
	*/
		
	//��ʼ������ͨ����Ŀ
	Test_Log("��ʼ��ʼ������ͨ����Ŀ");
	ret = Test_InitPortChn();
	if( ret < 0 )
	{
		Test_Log( "��ʼ������ͨ��ʧ��\n" );
		exit(-1);
	}
	
	//��ʼ�����ò���
	Test_Log("��ʼ��ʼ�����ò���");
	ret = Test_InitConfig();
	if(ret <0){
		Test_Log("���ò�����ʼ������.");
		exit(-1);
	}

	//��ʼ��ע��
	Test_Log("��ʼ��ʼ��ע��");
	ret = Test_Login();
	if(ret <0){
		Test_Log("��ʼ��ע�����.");
		exit(-1);
	}
	// ����һ���ź���.
	ret = Test_CreateSem( TEST_SEM_COM3 );
	if( ret < 0 ) {
		Test_Log( "�����ź���ʧ��" );
		exit(-1);//return -1;
	}
	ret = Test_CreateSem( TEST_SEM_COM4 );
	if( ret < 0 ) {
		Test_Log( "�����ź���ʧ��" );
		exit(-1);//return -1;
	}
	ret = Test_CreateSem( TEST_SEM_COM5 );
	if( ret < 0 ) {
		Test_Log( "�����ź���ʧ��" );
		exit(-1);//return -1;
	}
	ret = Test_CreateSem( TEST_SEM_COM6 );
	if( ret < 0 ) {
		Test_Log( "�����ź���ʧ��" );
		exit(-1);//return -1;
	}
	ret = Test_CreateSem( TEST_SEM_RESULT );
	if( ret < 0 ) {
		Test_Log( "�����ź���ʧ��" );
		exit(-1);//return -1;
	}
	
	//ͨ������ʼ��
	ret = Test_PortReadInit();
	if( ret <= 0 ) {
		Test_Log( "ͨ������ʼ��ʧ��" );
		exit(-1);//return -1;
	}
	
	//ѭ������
	while(1){
		//�ȴ����Կ�ʼ����
		if(2 == FccNum){
			Test_Log("***�ȴ����Կ�ʼ����");
		}else{
			Test_Log("�ȴ����Կ�ʼ����");
		}
		ret = Test_WaitStartCmd();
		if(ret <0){
			Test_Log("��ʼ�����������,�˳�");
			exit(-1);
		}
# if 0
		if(2 == FccNum){
			Test_Log("***��ʼ���ƿ�(����0)����",i);
			ret = Test_ComSendRecv(0);
			if(ret <0){
				Test_Log("***���ƿ�(����0)����ʧ��",i);
				//exit(0);
			}else{
				//TestFlag[0] = TestFlag[0] | (1<<7 );					
				Test_Log("***���ƿ�(����0)���Գɹ�",i);
			}
			ret = Test_SynStart(FccNum);
		}else{
			sleep(30);
			ret = Test_SynStart(FccNum);
		}
#endif		
		//-----------------------------
		//��ʼ����
		bzero(Test_Result,sizeof(Test_Result) );
		bzero(ThreadChFlag,sizeof(ThreadChFlag) );
		bzero(ThreadSrFlag,sizeof(ThreadSrFlag) );
		Test_Result[0] = 0x22;
		Test_Result[1] = 0x0a;
		
			
		
		//����ͨ����Ŀk = 0;	
		k=portChanel[3]+portChanel[4]+portChanel[5]+portChanel[6];
		if(k>64){
			Test_Log("***ͨ��(�������Ӻ�)��Ŀ̫��[%d],�˳�",k);
			exit(0);
		}
		//ͨ��(�������Ӻ�)����
		for(i=0;i<k;i++){
			if(0 == Gilbarco){
				if ( pthread_create( &ThreadChFlag[i], NULL, (void *)Test_Port_Thread, (void *)i ) )  {
					Test_Log("����ͨ��[%d] �̴߳���",i+1);
					abort();
				}
			}else if(1 == Gilbarco){
				if ( pthread_create( &ThreadChFlag[i], NULL, (void *)Test_Port_Thread_Grb, (void *)i ) )  {
					Test_Log("��������ͨ��[%d] �̴߳���",i+1);
					abort();
				}
			}else{
				Test_Log("������־ֵ[%d] ����",Gilbarco);
			}
			sleep(5);
			//sleep(10);
			//ret = Test_SynStart(FccNum);
		}
				
		//���ڲ���
		if(2 == FccNum){
			Test_Log("***��ʼ���ڲ���");
		}else{
			Test_Log("��ʼ���ڲ���");
		}
		bzero(TestFlag,sizeof(TestFlag));
		if(1 == FccNum){
			//����0������PC����
			for(i=1;i<3;i++){
				//Test_Log("��ʼ����[%d]����",i);
				if ( pthread_create( &ThreadSrFlag[i], NULL, (void *)Test_Com_Thread, (void *)i ) )  {
					Test_Log("��������[%d] �̴߳���",i+1);
					ret = Test_PortReadEnd();
					abort();
				}
				sleep(1);
				//sleep(10);
				//ret = Test_SynStart(FccNum);	
				}
		}else if(2 == FccNum){
			//����0������PC����
			for(i=1;i<3;i++){
				//Test_Log("��ʼ����[%d]����",i);
				if ( pthread_create( &ThreadSrFlag[i], NULL, (void *)Test_Com_Thread, (void *)i ) )  {
					Test_Log("***��������[%d] �̴߳���",i+1);
					ret = Test_PortReadEnd();
					abort();
				}
				sleep(1);
				//sleep(10);
				//ret = Test_SynStart(FccNum);
			}
		}else{
			Test_Log("***����FCC��־����[%d], �˳�",FccNum);
			ret = Test_PortReadEnd();
			exit(0);
		}
		//memcpy(&Test_Result[2],TestFlag,1);
		//Test_Result[2] = TestFlag[0];
		
		//USB�ڲ���,ֻ�ǲ��Դ�FCC
		if(2 == FccNum){
			Test_Log("***��ʼ���ƿ�(����0)����",i);
			ret = Test_ComSendRecv(0);
			if(ret <0){
				Test_Log("***���ƿ�(����0)����ʧ��",i);
				//exit(0);
			}else{
				TestFlag[0] = TestFlag[0] | (1<<7 );					
				Test_Log("***���ƿ�(����0)���Գɹ�",i);
			}
			Test_Result[2] =Test_Result[2] | TestFlag[0];
			Test_Log("***��ʼUSB�ڲ���");
			bzero(TestFlag,sizeof(TestFlag));
			//USB1�ڲ���
			Test_Log("***��ʼUSB��1����");
			ret = Test_readUSB(1);
			if(ret <0){
				Test_Log("***USB��[1]����ʧ��");
				sleep(1);
				//USB2�ڲ���
				Test_Log("***��ʼUSB��2����");
				ret = Test_readUSB(2);
				if(ret <0){
					Test_Log("***USB��[2]����ʧ��");
				}else{
					Test_Log("***USB��[2]���Գɹ�");
					TestFlag[0] = TestFlag[0] | 0xc0;//0x40
				}
					//exit(0);
			}else{
				Test_Log("***USB��[1]���Գɹ�");
				TestFlag[0] = TestFlag[0] | 0xc0;//0x80
			}
		

			Test_Result[11] = TestFlag[0];
		}else{
			Test_Log("���Ǵ�FCC[%d],�����Կ��ƿ�(����0)��USB��",FccNum);
		}
		for(i=0;i<k;i++){
			if ( pthread_join ( ThreadChFlag[i], NULL ) ) {
				Test_Log("joining ͨ��[%d]�̴߳���",i+1);
				ret = Test_PortReadEnd();
				abort();
			}else{
				Test_Log("joining ͨ��[%d]�߳̽���",i+1);
			}
		}
		for(i=1;i<3;i++){
			if(0==ThreadSrFlag[i]){
				continue;
			}
			else if ( pthread_join ( ThreadSrFlag[i], NULL ) ) {
				Test_Log("joining����[%d]�̴߳���",i);
				ret = Test_PortReadEnd();
				abort();
			}else{
				Test_Log("joining����[%d]�߳̽���",i);
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
		ret = Test_SynStart(FccNum);//ͬ��
		ret = Test_PortReadEnd();
		if( ret < 0 ) {
			Test_Log( "ͨ��������ʧ��" );
			return -1;
		}
		//���Ͳ��Խ��
		n = 2;
		 if(2 == FccNum){
		 	ret = Test_TCPSendRecv(Test_Result,12, TestFlag,&n);
			if(ret <0){
				Test_Log("���Ͳ��Խ������,�˳�");
				exit(-1);
			}else{
				if( (0x22 != TestFlag[0]) || (0x0 != TestFlag[1]) ){//0 != memcmp(TestFlag,"2200",2)
					Test_Log("���Ͳ��Խ��,�յ�������[%02x%02x]����,�˳�",TestFlag[0],TestFlag[1]);
					exit(-1);
				}
			}
			k = 0;//����ͨ����Ŀ	
			k=portChanel[3]+portChanel[4]+portChanel[5]+portChanel[6];
			//bzero(TestFlag,sizeof(TestFlag));
			TestFlag[0] =1;
			if((0xe0&Test_Result[2]) !=0xe0){//����ֻҪ0,1,2�����ɹ��Ϳ�����
				TestFlag[0] = 0;
				Test_Log("����0,1,2������һ��û�в���ͨ��[%02x]",Test_Result[2]);
				//exit(0);
			}
			for(i=0;i<k/8;i++){//ͨ��Ҫȫ��ͨ��
				if(Test_Result[i+3] != 0xff){
					TestFlag[0] = 0;
					Test_Log("ͨ��[%d~%d]����û��ͨ��",i*8+1,i*8 +8);
				}
			}
			if((0x80&Test_Result[11]) !=0x80){//USBֻҪ��һ���ɹ�����
				TestFlag[0] = 0;
				Test_Log("USB1����û��ͨ��[%02x]",Test_Result[11]);
			}
			if(1 != TestFlag[0] ){
				Test_Log("***��FCC����ʧ��,��رմ�FCC,������,�ȴ��ٴβ���");
				exit(-1);
			}else{
			 	Test_Log("***��FCC���Գɹ�,�ȴ���һ����FCC����");
				exit(0);
			 }
			 //Test_Log("***�Բ��Գ���һ�β������,�ȴ��´β���");
		 }else{
		  	Test_Log("�Բ��Գ���һ�β������,�ȴ��´β���");
		  }
	}
	 //Test_AcceptPCCmd();
	
	 Test_Log("***�Բ��Գ����쳣�˳�");
	return -1;
}







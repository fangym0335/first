/*
 * 2008-11-10 - Chen Fu-ming <chenfm@css-intelligent.com>
 Ë¼Â·:±¾³ÌĞòÔÚinittabÖĞÓÃwait×Ô¶¯Æô¶¯,È»ºó±¾³ÌĞò¸ù¾İÅäÖÃÎÄ¼ş ÅäÖÃµÄIPµØÖ·ºÍ¶Ë¿ÚºÅ
 Á¬½ÓPC»úÉÏµÄ²âÊÔ³ÌĞò,Èç¹û³ÖĞø1·ÖÖÓ¶à´ÎµÄÁ¬½Ó¶¼²»ÄÜ³É¹¦,ÄÇÃ´±¾³ÌĞò
 ×Ô¶¯ÍË³ö;Èç¹ûÁ¬½Ó³É¹¦¾Í¿ªÊ¼²âÊÔ.Òò´Ë±¾³ÌĞòÔÚ³ÉÆ·ÖĞÒ²¿ÉÒÔ´æÔÚ. 
 ±¾³ÌĞòÔËĞĞĞèÒªPCºÍ2¸öÍ¬ĞÍºÅµÄFCC,×îºÃÊÇÒ»¸öºÃµÄÒ»¸ö´ı²âÊÔµÄ,
 PC¸ù¾İIPµØÖ·Çø·Ö,µÚÒ»¸ö×¢²áµÄÊÇÖ÷FCC,µÚ¶ş¸öÊÇ´ÓFCC,Ê¹ÓÃÊ±×¢Òâ,½¨Òé´ı²âFCCÎª´Ó.
 * *******************************************************************************************************
 Ğ­Òé:
 Êı¾İ¸ñÊ½¶¼ÊÇHEX,³ı×¢²áÊı¾İÍâ,ÆäËüÊı¾İµÄ±êÖ¾×Ö½ÚµÄºó4Î»1Îª1ºÅFCC,2Îª2ºÅFCC
1. FCCÏòPC ×¢²á:
FCC·¢:1104+±¾»úIPµØÖ·+4¸ö´®¿ÚÍ¨µÀÊı,PC»Ø¸´:110111»òÕß 110122,ÆäÖĞ110111Îª1ºÅ,110122Îª2ºÅFCC.
2.FCCµÄËùÓĞµ÷ÊÔĞÅÏ¢¶¼ÓÃUDP·¢ËÍ¸øPCµÄ²âÊÔ³ÌĞò,¶¼ÔÚÒ»¸ö´°¿ÚÏÔÊ¾
3. ¿ªÊ¼È«²¿²âÊÔ:(µÈ´ı´Ó²âÊÔFCC×¢²áºóPC×Ô¶¯·¢)
  PCÏòÁ½¸öFCC·¢:220111ºÍ220122, 11µÄÊÇÖ÷²âÊÔ, 22µÄÊÇ´Ó²âÊÔ,FCCÊÕµ½»Ø¸´2200±íÊ¾¿ªÊ¼²âÊÔ
  ²âÊÔÍê³Éºó,FCC·¢ËÍ²âÊÔ½á¹û,²âÊÔ½á¹û°´ÕÕÎ»×ö±êÖ¾,0²âÊÔÊ§°Ü,1²âÊÔ³É¹¦:
  220a+1×Ö½Ú´®¿Ú³É¹¦±êÖ¾+8×Ö½Ú´®¿ÚÍ¨µÀ±êÖ¾+1×Ö½ÚUSB±êÖ¾×Ö½Ú,¹²12×Ö½Ú,PC»Ø¸´2200
  8×Ö½Ú´®¿ÚÍ¨µÀ±êÖ¾,Ã¿¸ö´®¿Ú2¸ö×Ö½Ú×î¶à16¸öÍ¨µÀ,Òò´Ë×î¶àÖ§³Ö64¸öÍ¨µÀ
4.Í¬²½²âÊÔÃüÁî:
    FCC·¢ËÍ0xff03110000»òÕß0xff03220000,FCCÊÕµ½0xff0111»òÕß0xff0122¼ÌĞø²âÊÔ, ·ñÔòÑ­»·µÈ´ı
5.Êı¾İ´íÎó´¦Àí:
  ËùÓĞÊı¾İ´íÎóµÄÊ±ºò,µÚÒ»¸ö×Ö½Ú,Ò²¾ÍÊÇÃüÁî×Ö½ÚµÄµÍ4Î»ÒªÖÃ1£¬¼´Öµf,
  µÚ¶ş¸ö×Ö½ÚÎª0,   Èç1100±ä³É1f00,
6. ÍË³ö²âÊÔ³ÌĞò:
    PCÏòÁ½¸öFCC·¢9900,FCCÊÕµ½»Ø¸´9900

-------------------------------------------------------------------------------------------
ÒÔÏÂ¹¦ÄÜÔİÊ±Ã»ÓĞÍê³É
7. ²¿·Ö²âÊÔ:(²¿·Ö²âÊÔ±ØĞëÔÚÈ«²¿²âÊÔ×öÍê³Éºó²ÅÄÜ½øĞĞ)
   1).½»²æÍ¨µÀ²âÊÔ: PCÏòÁ½¸öFCC·¢:3301XX3301YY,XXÊÇÖ÷²âÊÔµÄÍ¨µÀºÅ,YYÊÇ´Ó²âÊÔµÄÍ¨µÀºÅ
      Á½¸öÍ¨µÀÒªÎïÀíÁ¬½Ó.FCC»Ø¸´3300. ³É¹¦ºóFCC»Ø¸´3301XX»òÕß3301YY,PC»Ø¸´3300.
   2).ÆÕÍ¨´®¿Ú²âÊÔ:PCÏòÁ½¸öFCC·¢:4401XX4401YY,XXÊÇÖ÷²âÊÔµÄ´®¿ÚºÅ,YYÊÇ´Ó²âÊÔµÄ´®¿ÚºÅ
      Á½¸öÍ¨µÀÒªÎïÀíÁ¬½Ó.´®¿ÚºÅ´Ó0¿ªÊ¼.FCC»Ø¸´4400.³É¹¦ºó»Ø¸´4401XX»òÕß4401YY,PC»Ø¸´4400.
   3).USB²âÊÔ:PCÏòÁ½¸ö¸öFCC·¢:5501XX5501YY,XXÊÇÖ÷²âÊÔµÄUSBºÅ,YYÊÇ´Ó²âÊÔµÄUSBºÅ
      Á½¸öÍ¨µÀÒªÎïÀíÁ¬½Ó.USBºÅ´Ó1¿ªÊ¼.0±íÊ¾²»²âÊÔ
      FCC»Ø¸´5500. ³É¹¦ºóFCC»Ø¸´5501XX»òÕß5501YY,PC»Ø¸´5500.
  4).²âÊÔÖĞÌáĞÑ²åÈëUÅÌ:   FCC·¢6601XX, XXÊÇ²âÊÔµÄUSBºÅPC»Ø¸´6600.²åÈë½áÊø,PC·¢6601XX,
     FCC»Ø¸´6600,²¢¿ªÊ¼²âÊÔ.   ²âÊÔ½áÊøFCC·¢6602XX00»òÕß6602XX11, XXÊÇ²âÊÔµÄUSBºÅ,
     00±íÊ¾²âÊÔ³É¹¦,11±íÊ¾²âÊÔÊ§°Ü,PC»Ø¸´6600
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

//ËøĞÅºÅÁ¿Ë÷ÒıÖµ
#define TEST_SEM_FILE_INDEX 1000 
#define TEST_SEM_COM3 1003
#define TEST_SEM_COM4 1004
#define TEST_SEM_COM5 1005
#define TEST_SEM_COM6 1006
#define TEST_SEM_RESULT 1100

#define USB_DEV_SDA 0	//USB´ÓÉè±¸ºÅ
#define USB_DEV_SDB 16
#define USB_DEV_SDC 32
#define USB_DEV_SDD 48
//----------------------------------------------
//ÊµÊ±ÈÕÖ¾¶¨Òå
//===============================
#define TEST_MAX_VARIABLE_PARA_LEN 1024 
#define TEST_MAX_FILE_NAME_LEN 1024 
static char gsFccFileName[TEST_MAX_FILE_NAME_LEN + 1];	//¼ÇÂ¼µ÷ÊÔĞÅÏ¢µÄÎÄ¼şÃû
static int giFccLine;								//¼ÇÂ¼µ÷ÊÔĞÅÏ¢µÄĞĞ
static int giFccFd;                                                       //ÈÕÖ¾ÎÄ¼ş¾ä±ú
static char gcFccLogFlag;
static unsigned  char  gsLocalIP[16];
//×Ô¶¨Òå·Ö¼¶ÎÄ¼şÈÕÖ¾,¼û__Log.
#define Test_Log Test_SetFileAndLine( __FILE__, __LINE__ ), Test__Log
//#define Test_Log Test_SetFileAndLine( __FILE__, __LINE__ ),Test__FileLog
//--------------------------------------------------------------

//-------------------------------------------------------
//´®¿Ú¶¨Òå
//´®¿ÚÌØÕ÷,´®¿Ú´Ó0¿ªÊ¼
#define START_PORT 3
#define MAX_PORT 6
#define PORT_USED_COUNT 3
#define RECLEN 16
//´®¿Ú×Ö·û´®
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
//´®¿ÚÍ¨µÀÊıÄ¿,²»Á¬½ÓÍ¨µÀÖµÎª0.
unsigned char portChanel[MAX_PORT+1];
//----------------------------------------------------

//-------------------------------------------
//³õÊ¼»¯ÅäÖÃ²ÎÊı
//-------------------------------------------
static char PC_SERVER_IP[16]; //PC·şÎñÆ÷IPµØÖ·
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
//ÏòPC×¢²á
/*FCCÏòPC ×¢²á:
*FCC·¢:1104+±¾»úIPµØÖ·+4¸ö´®¿ÚÍ¨µÀÊı,PC»Ø¸´:110111»òÕß 110122,ÆäÖĞ110111Îª1ºÅ,110122Îª2ºÅFCC.*/
static unsigned char FccNum;
static unsigned char Gilbarco;
//-------------------------------------------------

//------------------------------------------------
static char Test_USB_UM[]="/bin/umount -l /home/ftp/";
static char Test_USB_File[]="/home/ftp/fcctest.txt";
static char Test_USB_Data[]="1234567890";
//---------------------------------------------

//------------------------------------------------
//½á¹û»Ø¸´Êı¾İ
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
//ÊµÊ±ÈÕÖ¾
//----------------------------------------------
void Test_SetLogFd( const int fd )
{
	giFccFd= fd;
	if(  giFccFd <=  0){
		printf( " {ÈÕÖ¾ÎÄ¼şÃèÊö·û´íÎó[%d] }\n", fd );
		
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
		printf("ĞÄÌøÊı¾İ·¢ËÍÊ§°Ü,error:[%s]", strerror(errno));	
	}*/		
	
}

//giLogLevel=0, no log;giLogLevel=1, only print log info; giLogLevel = 2, print log info and write log info to file 
void Test__Log(const char *fmt,...){//const int loglevel, const int fd, 
	va_list argptr;
	char sMessage[TEST_MAX_VARIABLE_PARA_LEN + 1];
	char fileName[TEST_MAX_FILE_NAME_LEN + 1];	/*¼ÇÂ¼µ÷ÊÔĞÅÏ¢µÄÎÄ¼şÃû*/
	int line;								/*¼ÇÂ¼µ÷ÊÔĞÅÏ¢µÄĞĞ*/
	//int level = giFccLogLevel;
	
	
	Test_GetFileAndLine(fileName, &line);	
	//sprintf(sMessage, "[%s]", gsLocalIP);
	sprintf(sMessage, "%s:%d  ", fileName, line);
	sprintf(sMessage+strlen(sMessage), "pid = [%s:%d]", gsLocalIP, getpid());

	va_start(argptr, fmt);
	vsprintf(sMessage+strlen(sMessage),fmt, argptr);
	va_end( argptr );
	sprintf(sMessage+strlen(sMessage), "\n");
	
	
	if(giFccFd <=0 )	/*ÈÕÖ¾ÎÄ¼şÎ´´ò¿ª?*/
	{
		printf( " {ÈÕÖ¾ÎÄ¼şÃèÊö·û´íÎó[%d] }\n", giFccFd );
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

int Test_InitLog(int flag){//flag---0: UDPÈÕÖ¾,1:´òÓ¡ÈÕÖ¾
	int sockfd;
	unsigned char *pIP;
	struct in_addr in_ip;
    	 /*if( ( fd = open( TEST_LOG_FILE, O_WRONLY|O_CREAT|O_TRUNC, 00644 ) ) <= 0 ) {
		printf( "´ò¿ªÈÕÖ¾ÎÄ¼ş[%s]Ê§°Ü\n", TEST_LOG_FILE );
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
	else printf("Test_InitLogº¯ÊıÈÕÖ¾³õÊ¼»¯´íÎó,flag[%d]\n",flag);
	//Test_SetLogLevel(loglevel);
	return 1;
}
//------------------------------------------------

//-----------------------------------------------------------------------
//¹«ÓÃº¯Êı
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
//»ñÈ¡±¾»úIP,·µ»Ø±¾»úIPµØÖ·£¬·µ»Ø-1Ê§°Ü£¬½â¾öDHCPÎÊÌâ
static unsigned  long  Test_GetLocalIP(){//²Î¼ûtcp.hºÍtcp.cÎÄ¼şÖĞÓĞ,¿ÉÒÔÑ¡ÔñÒ»¸ö.
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

	port = GetTcpPort( sPortName );	//¸ù¾İ¶Ë¿ÚºÅ»òÕß¶Ë¿ÚÃûÈ¡¶Ë¿Ú È¡³öµÄportÎªÍøÂç×Ö½ÚĞò//
	if( port == INPORT_NONE )	//GetTcpPortÒÑ¾­±¨¸æÁË´íÎó,´Ë´¦Ê¡ÂÔ//
		return -1;
	*/

	sockFd = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
	if( sockFd < 0 )
	{
		RptLibErr( errno );
		return -1;
	}

	//opt = 1;	//±êÖ¾´ò¿ª//
	//setsockopt( sockFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof( opt ) );	//ÉèÖÃ¶Ë¿Ú¿ÉÖØÓÃ//
	
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);	/*htonl Ö÷»ú×Ö½ÚĞòµ½ÍøÂç×Ö½ÚĞò.²»¹ıINADDR_ANYÎª0,ËùÒÔ¿ÉÒÔ²»ÓÃ*//*INADDR_ANY ÓÉÄÚºËÑ¡ÔñµØÖ·*/
	serverAddr.sin_port = htons(port);

	if( bind( sockFd, (SA *)&serverAddr, sizeof( serverAddr ) ) < 0 )
	{
		Test_Log( "´íÎóºÅ:%d", errno );
		return -1;
	}

	if( listen( sockFd, backLog ) < 0 )
	{
		Test_Log( "´íÎóºÅ:%d",errno );
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
		if( newsock < 0 && errno == EINTR ) /*±»ĞÅºÅÖĞ¶Ï*/
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

	Test_Log( "´íÎóºÅ:%d",errno );
	return -1;
}
/*µ±nTimeOutÉèÖÃÎªTIME_INFINITE»òÕßÉèÖÃµÄÊ±¼ä±ÈÏµÍ³È±Ê¡Ê±¼ä³¤Ê±,
 connectÔÚÍê³É·µ»ØÇ°¶¼Ö»µÈ´ıÏµÍ³È±Ê¡Ê±¼ä*/
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
		OldAction = SetAlrm( nTimeOut, NULL );  /*Èç¹ûÔÚalarmµ÷ÓÃºó,connectµ÷ÓÃÇ°³¬Ê±,ÔòconnectµÈ´ıÊ±¼äÎªÈ±Ê¡Ê±¼ä*/
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
	 0:	³É¹¦
	-1:	Ê§°Ü
	-2:	³¬Ê±
*/
static ssize_t Test_TcpSendDataInTime( int fd, const char *s, size_t len,  int timeout )
{
#if 1
	ssize_t	 retlen;
	size_t	sendlen;
	char	*head;
	char	*t;
	char	buf[TCP_MAX_SIZE];
	int	mflag; 	/*±êÊ¶ÊÇ·ñ·ÖÅäÁË¿Õ¼ä 0Î´·ÖÅä. 1ÒÑ·ÖÅä*/
	int	n;
	int     i;
	
	Assert( s != NULL && *s != 0 );
	Assert( len != 0  );
	Assert( timeout > 0 || timeout == TIME_INFINITE );

	mflag = 0;
	sendlen = len;

	if (sizeof(buf) >= sendlen ) {	/*Ô¤ÏÈ·ÖÅäµÄ±äÁ¿bufµÄ¿Õ¼ä×ã¹»ÈİÄÉËùÓĞÒª·¢ËÍµÄÊı¾İ*/
		t = buf;
	} else {			/*Ô¤ÏÈ·ÖÅäµÄ±äÁ¿bufµÄ¿Õ¼ä²»×ãÈİÄÉËùÓĞÒª·¢ËÍµÄÊı¾İ,¹Ê·ÖÅä¿Õ¼ä*/
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

	if ( n == sendlen ) {	/*Êı¾İÈ«²¿·¢ËÍ*/
		return 0;
	} else if ( n == -1 ) {/*³ö´í*/
		return -1;
	} else if ( n >= 0 && n < sendlen ) {
		if ( GetLibErrno() == ETIME ) {/*³¬Ê±*/
			return -2;
		} else {
			return -1;
		}
	}
#endif
#if 0
//´ø4×Ö½Ú³¤¶ÈµÄ´«ËÍ·½Ê½
	ssize_t		retlen;
	char		*head;
	char		*t;
	char		buf[TCP_MAX_SIZE];
	size_t		sendlen;
	int			mflag; 	/*±êÊ¶ÊÇ·ñ·ÖÅäÁË¿Õ¼ä 0Î´·ÖÅä. 1ÒÑ·ÖÅä*/
	int			n;
	int 		headlen=4;
	Assert( s != NULL && *s != 0 );
	Assert( len != 0 && headlen != 0 );
	Assert( timeout > 0 || timeout == TIME_INFINITE );

	mflag = 0;
	sendlen = headlen+len;

	if( sizeof(buf) >= sendlen )	/*Ô¤ÏÈ·ÖÅäµÄ±äÁ¿bufµÄ¿Õ¼ä×ã¹»ÈİÄÉËùÓĞÒª·¢ËÍµÄÊı¾İ*/
		t = buf;
	else							/*Ô¤ÏÈ·ÖÅäµÄ±äÁ¿bufµÄ¿Õ¼ä²»×ãÈİÄÉËùÓĞÒª·¢ËÍµÄÊı¾İ,¹Ê·ÖÅä¿Õ¼ä*/
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

	if( n == sendlen )	/*Êı¾İÈ«²¿·¢ËÍ*/
		return 0;
	else if( n == -1 ) /*³ö´í*/
		return -1;
	else if( n >= 0 && n < sendlen )
	{
		if( GetLibErrno() == ETIME ) /*³¬Ê±*/
			return -2;
		else
			return -1;
	}

#endif
}

/*
 * s:	±£´æ¶Áµ½µÄÊı¾İ
 * buflen:	Ö¸Õë¡£*buflenÔÚÊäÈëÊ±ÎªsÄÜ½ÓÊÜµÄ×î´ó³¤¶ÈµÄÊı¾İ£¨²»°üº¬Î²²¿µÄ0.Èçchar s[100], *buflen = sizeof(s) -1 )
 *		ÔÚÊä³öÊ±ÎªsÊµ¼Ê±£´æµÄÊı¾İµÄ³¤¶È(²»°üº¬Î²²¿µÄ0.)
 * ·µ»ØÖµ:
 *	0	³É¹¦
 *	1	³É¹¦.µ«Êı¾İ³¤¶È³¬¹ıÄÜ½ÓÊÜµÄ×î´ó³¤¶È.Êı¾İ±»½Ø¶Ì
 *	-1	Ê§°Ü
 *	-2	³¬Ê±»òÕßÁ¬½Ó¹ıÔç¹Ø±Õ
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
		printf("ÊÕµ½µÄ³¤¶È[%d]Ğ¡ÓÚÔ¤ÆÚÊÕµ½µÄ³¤¶È[%d],Öµ[%s]\n", n,len,Test_Asc2Hex(t, len));
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
//È¥³ı×Ö·û´®Á½²à¿Õ¸ñ¼°»Ø³µ»»ĞĞ·û,ĞèÒª×Ö·û´®´ø½áÊø·û
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
//³õÊ¼»¯ÅäÖÃ²ÎÊı
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
	//´ò¿ªÎÄ¼ş
	fd = open( fileName, O_WRONLY|O_CREAT|O_TRUNC, 00644 );
	if( fd < 0 )
	{
		//UserLog( "´ò¿ªµ±Ç°ÔËĞĞ¹¤×÷Â·¾¶ĞÅÏ¢ÎÄ¼ş[%s]Ê§°Ü.",fileName );
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
			//Test_Log("ÅäÖÃÊı¾İ³ö´í,  ²»ÊÇÔ¤ÆÚµÄÈı¸öÅäÖÃ,Ãû×ÖÊÇ[%s],ÖµÊÇ[%s]",fcc_ini[i].name,fcc_ini[i].value);
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
//´®¿ÚºÍÍ¨µÀ·ÃÎÊº¯Êı
//------------------------------------------------------------------------------


static int Test_OpenSerial( char *ttynam ){
	int fd;
	struct termios termios_new;

	fd = open (ttynam, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if( fd < 0 )
	{
		//UserLog( "´ò¿ªÉè±¸[%s]Ê§°Ü", ttynam );
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
		//Test_Log( "´ò¿ª´®¿ÚÊ§°Ü" );
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
 * func: ½«±³°åÁ¬½ÓºÅ(Âß¼­Í¨µÀºÅ)×ª»»ÎªÎïÀíÍ¨µÀºÅ
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
		Test_Log( "Test_GetPortChannel Ê§°Ü.Çë¼ì²é" );
		return -1;
	}
	Test_Log("Test_InitPortChn»ñÈ¡µ½portChnl[%s]",Test_Asc2Hex((unsigned char *)portChanel, MAX_PORT+1));
	return 0;
}

//¶ÁĞ´´®¿Ú
int Test_WriteTty(int fd, unsigned char *msg, int msgLen){
	int ret;
	int timeout = 5;
	//unsigned char buff[64],*result;
	
	//fd = OpenSerial( TEST_alltty[serial_num]);
	if( fd <= 0 )
	{
		Test_Log( "´®¿Úfd[%d]¸ºÖµ´íÎó",fd );
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

//Í¨µÀ¶ÁÈ¡Êı¾İ´¦Àí,´Ë´¦Ã»ÓĞÊ¹ÓÃÏûÏ¢¶ÓÁĞ,¸ÄÓÃÁÙÊ±ÎÄ¼ş.
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
		Test_Log( "´®¿Úfd[%d]¸ºÖµ´íÎó",fd );
		return -1;
	}
	//Test_Log( "after Test_Writetty and 0 to moved test!" );
	len = *recvLen;
	if( sizeof(buff) < len)
	{
		Test_Log( "FccReadTty´íÎó,buff³¤¶È[%d]Ğ¡ÓÚ´ı½ÓÊÕ³¤¶È[%d]",sizeof(buff), len);
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
			default:Test_Log( "´®¿ÚºÅ[%d]´íÎó",serialPortNum );
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
			default:Test_Log( "Test_PortRead´®¿ÚºÅ[%d]´íÎó",serialPortNum );
				break;			
		}		
		if(ret <=0){
			Test_Log( "Test_PortRead¶Ë¿Ú[%02x]¶Á[´®¿Ú%d]Êı¾İÊ§°Ü" ,phyChnl,serialPortNum);
			sleep(1);
			continue;
		}
		Test_Log("Test_PortRead[´®¿Ú%d][Í¨µÀ%02x]½ÓÊÕÊı¾İ[%s]",serialPortNum,phyChnl,Test_Asc2Hex(buff, len));	
		for(y=0;y<RECLEN;y++){
		       if(0x30+phyChnl-1 == buff[y]){
			Test_Log("Test_PortRead[´®¿Ú%d][Í¨µÀ%02x]½ÓÊÕÊı¾İ³É¹¦",serialPortNum,phyChnl);
			return 1;//¶ÁÈ¡³É¹¦//break;
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
			Test_Log("Ğ´ÁËÊı¾İÎÄ¼ş[%s]",sTemp);
			close(fd);
			sleep(1);
			continue;
	}
	
	/*if( (0x30+phyChnl-1 == msg[0])||(0x30+phyChnl-1 == msg[1])){
		return 1;//¶ÁÈ¡³É¹¦
	}*/	
	bzero(sTemp,sizeof(sTemp));
	sprintf(sTemp,"/tmp/testfcc/%02d%02x",serialPortNum,0x30+phyChnl-1);
	ret = Test_IsFileExist(sTemp);
	if(ret <= 0){
		Test_Log( "Ã»ÓĞ¸ÃÍ¨µÀ[´®¿Ú%d][Í¨µÀ%02x]Êı¾İ." ,serialPortNum,phyChnl);
		return -6;
	}else{
		unlink(sTemp);//É¾³ı¸ÃÎÄ¼ş
		Test_Log("É¾³ıÁËÊı¾İÎÄ¼ş[%s]",sTemp);
		return 1;//±£´æÓĞ¸ÃÍ¨µÀÊı¾İ
	}
}

//--------------------------------
//ÏòPC×¢²á
/*FCCÏòPC ×¢²á:
*FCC·¢:1104+±¾»úIPµØÖ·+4¸ö´®¿ÚÍ¨µÀÊı,PC»Ø¸´:110111»òÕß 110122,ÆäÖĞ110111Îª1ºÅ,110122Îª2ºÅFCC.*/
//static unsigned char FccNum;
//static char PC_SERVER_IP[16]; //PC·şÎñÆ÷IPµØÖ·
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
	
	
	//Í·²¿Êı¾İÉèÖÃ
	
	msgLength =6;
	sendBuffer[0] = 0x11;
	sendBuffer[1] = 0x04;	
	localIP = Test_GetLocalIP();
	
	//FCCµÄ IPµØÖ·
	memcpy(&sendBuffer[2],(unsigned char *)&localIP,4);

	//´®¿ÚÍ¨µÀÊıÄ¿
	memcpy(&sendBuffer[62],&portChanel[START_PORT],4);//(unsigned char *)&localIP
	//i=k;
	//ÏòPCÉÏ´«Êı¾İ
	Test_Log("ÏòPCÉÏ´«Êı¾İ[%s]",Test_Asc2Hex(sendBuffer, msgLength) );
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
				Test_Log("TcpSendDataInTime·¢ËÍÊı¾İ²»³É¹¦,Test_Login·¢ËÍÊı¾İ Ê§°Ü,IP[%s],port[%d]",sIPName,PC_SERVER_TCP_PORT);
				close(sockfd);
				sleep(1);//sleep(10000);
				continue;
			}else{				
				break;
			}
		} else {
			Test_Log("Tcp Connect ²»³É¹¦,Test_Login·¢ËÍÊı¾İ Ê§°Ü,IP[%s],port[%d]",sIPName,PC_SERVER_TCP_PORT);
			close(sockfd);
			sleep(1);//sleep(10000); 			
			continue;
			//return -1;
		}
	}
	 //¿ªÊ¼´ÓPC½ÓÊÕ·µ»ØÊı¾İ
	Test_Log("¿ªÊ¼´ÓPC½ÓÊÕ·µ»ØÊı¾İ");
	bzero(sendBuffer,sizeof(sendBuffer));
	for(i=0;i<tryn;i++){
		msgLength = 3;		
		ret =Test_TcpRecvDataInTime(sockfd, sendBuffer, &msgLength, timeout);			//Act_V(CD_SOCK_INDEX);			
		if (ret < 0 ){
			Test_Log("TcpRecvDataInTime½ÓÊÕÊı¾İ²»³É¹¦,Test_Login½ÓÊÕÊı¾İ Ê§°Ü");
			sleep(10);
			continue;
		}
		break;
	}
	
      if( (sendBuffer[0] != 0x11)||(sendBuffer[1] != 0x01) ){
	  	Test_Log("Tcp Connect ²»³É¹¦,Test_Login½ÓÊÕÊı¾İ Ê§°Ü,[%02x%02x%02x]",sendBuffer[0],sendBuffer[1],sendBuffer[2]);
		close(sockfd);
		return -1;
	}
	//CSS_LevelLog("gpCSS_Shm->css._TLG_SEND_INTERVAL¸³Öµ[%02x%02x]",sendBuffer[1],sendBuffer[2]);
	Gilbarco = 0;
      	if(sendBuffer[2] == 0x11) {
			FccNum = 1;
			Test_Log("FccNum¸³Öµ[%d],Ö÷FCC",FccNum);
	}else if(sendBuffer[2] == 0x22){ 
		FccNum = 2;
		Test_Log("FccNum¸³Öµ[%d],´ÓFCC",FccNum);
	}else if(sendBuffer[2] == 0x33){ 
		FccNum = 1;
		Gilbarco = 1;
		Test_Log("³¤¼ª×¨ÓÃ,FccNum¸³Öµ[%d],Ö÷FCC",FccNum);
	}else if(sendBuffer[2] == 0x44){ 
		FccNum = 2;
		Gilbarco = 1;
		Test_Log("³¤¼ª×¨ÓÃ,FccNum¸³Öµ[%d],´ÓFCC",FccNum);
	}else{
		Test_Log("Test_Login½ÓÊÕÊı¾İ´íÎó,Test_Login½ÓÊÕÊı¾İ Ê§°Ü");
		return -1;
	}
	close(sockfd);
	//Test_Log("FccNum¸³Öµ[%d]",FccNum);
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
				Test_Log("TcpSendDataInTime·¢ËÍÊı¾İ²»³É¹¦,IP[%s],port[%d]",sIPName,PC_SERVER_TCP_PORT);
				close(sockfd);
				sleep(10);//sleep(10000);
				continue;
			}else{				
				break;
			}
		} else {
			Test_Log("Tcp Connect ²»³É¹¦,IP[%s],port[%d]",sIPName,PC_SERVER_TCP_PORT);
			close(sockfd);
			sleep(60);//sleep(10000); 			
			continue;
			//return -1;
		}
	}
	 //´ÓPC½ÓÊÕ·µ»ØÊı¾İ
	Test_Log("´ÓPC½ÓÊÕ·µ»ØÊı¾İ");
	 //sleep(5);//µÈ´ı¶Ô·½Ö´ĞĞÍê±Ï
	//bzero(recvBuffer,sizeof(recvBuffer));
	for(i=0;i<tryn;i++){
		msgLength = *recvLength;		
		ret =Test_TcpRecvDataInTime(sockfd, recvBuffer, &msgLength, timeout);			//Act_V(CD_SOCK_INDEX);			
		if (ret < 0 ){
			Test_Log("TcpRecvDataInTime½ÓÊÕÊı¾İ²»³É¹¦");
			sleep(10);
			continue;
		}
		break;
	}
      if( (recvBuffer[0] == 0)&&(recvBuffer[1] == 0) ){
	  	Test_Log("Tcp Connect ²»³É¹¦,[%02x%02x%02x]",recvBuffer[0],recvBuffer[1],recvBuffer[2]);
		close(sockfd);
		return -1;
	}	
	close(sockfd);
	memcpy((char *)recvLength,(char *)&msgLength,sizeof(int));
	Test_Log("Test_SendRecv½ÓÊÕÊı¾İ[%s]",Test_Asc2Hex(recvBuffer, msgLength));	
	return 0;
}


//--------------------------------------------------
//Í¨Ñ¶·şÎñ³ÌĞò
//-----------------------------------------------
//static char PC_SERVER_IP[16]; //PC·şÎñÆ÷IPµØÖ·
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
		Test_Log( "ÊµÊ±½ø³Ì³õÊ¼»¯socketÊ§°Ü.port=[%s]\n", TEST_TCP_PORT );
		//Free( cmd );
		return  ;
	}
	//fcntl(sock, F_SETFL, O_NONBLOCK);   // ·Ç×èÈûÄ£Ê½
	
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
		if (newsock < 0) {  // Èç¹ûÃ»ÓĞÁ¬½Ó£¬Ôò¼àÌı
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
		Test_Log("Test_WaitStartCmdÁ¬½Ó,soketºÅ[%d]", tmp_sock);
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
				/*´íÎó´¦Àí*/
				Test_Log( "½ÓÊÕÊı¾İ[%d]´Î¶¼Ê§°Ü", tryNum );
				break; 
			}
			msg_lg = 3;
			sleep(10);
			ret = Test_TcpRecvDataInTime(newsock, buffer, &msg_lg, timeout );//TIME_INFINITE	
			if (ret < 0) {
				if (errno == 4) {  // time out
					Test_Log("½ø³Ì³¬Ê±,½ÓÊÕµ½ºóÌ¨ÃüÁî buffer[%s]\n" , Test_Asc2Hex(buffer, msg_lg) );//
					continue;
				} else if (errno == 11)  {  // maybe CD quit or restart, && (gpIfsf_shm->need_reconnect == 0))
					//gpIfsf_shm->need_reconnect = 1;
					Test_Log("Client need to reconnect to Server");
				}	
				Test_Log( "½ø³Ì½ÓÊÕÊı¾İÊ§°Ü,errno:[%d][%s],buffer: %s", errno, strerror(errno), \
					Test_Asc2Hex(buffer, 2) );
				errcnt++;
				sleep(1);
				continue;
			} else {
				Test_Log( "<<<½ø³ÌÊÕµ½ÃüÁî: [%s(%d)]", Test_Asc2Hex(buffer, msg_lg ), msg_lg);				
				if ( 0x22 != buffer[0] ){ //received data must be error!!(0 == (buffer[0]>>4) )||(0 ==( buffer[0]&0x0f) )
					Test_Log( "received data error" );
					errcnt++;
					sleep(1);
					continue;
				}

				if (0 == msg_lg){ //receivd data must be error!!
					Test_Log( "received size is  0 error" );
					errcnt++;
					//»Ø¸´´íÎó
					sBuffer[0] = 0x2f;
					sBuffer[1] = 0;
					//memcpy(&sBuffer[1],gpCSS_Shm->css._STATION_NO_BCD,5);
					ret = Test_TcpSendDataInTime(newsock, sBuffer, 2, timeout);
					if(ret <=0){				
						Test_Log("ÊµÊ±½ø³Ì»Ø¸´Êı¾İ Ê§°Ü");
					}
					
					sleep(1);
					continue;
				}

				
				
				//´¦ÀíºóÌ¨ÃüÁî.
				if (1 == buffer[1]){ 
					if(0x11 == buffer[2]){
						if(1 != FccNum) {
							Test_Log("PCÆô¶¯ÃüÁî ´íÎó,±¾»úÖ÷FCC");
							sBuffer[0] = 0x2f;
							sBuffer[1] = 0;
							//memcpy(&sBuffer[1],gpCSS_Shm->css._STATION_NO_BCD,5);
							ret = Test_TcpSendDataInTime(newsock, sBuffer, 2, timeout);
							if(ret <0){				
								Test_Log("½ø³Ì»Ø¸´Êı¾İ Ê§°Ü");
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
							Test_Log("½ø³Ì»Ø¸´Êı¾İ Ê§°Ü");
							//continue;
						}
						errcnt = 0;
						close(newsock);close(sock);
						return 1;//break;
					}else if(0x22 == buffer[2]){
						if(2 != FccNum) {
							Test_Log("PCÆô¶¯ÃüÁî ´íÎó,±¾»ú´ÓFCC");
							sBuffer[0] = 0x2f;
							sBuffer[1] = 0;
							//memcpy(&sBuffer[1],gpCSS_Shm->css._STATION_NO_BCD,5);
							ret = Test_TcpSendDataInTime(newsock, sBuffer, 2, timeout);
							if(ret <0){				
								Test_Log("½ø³Ì»Ø¸´Êı¾İ Ê§°Ü");
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
							Test_Log("½ø³Ì»Ø¸´Êı¾İ Ê§°Ü");
							//continue;
						}
						errcnt = 0;
						close(newsock);close(sock);
						return 1;//break;
					}else{
						Test_Log( "½âÎöÊı¾İÊ§°Ü,ÊÕµ½µÄÊı¾İÖµ²»ÊÇ0x11ºÍ0x22" );
					}
				}  else{
					Test_Log( "½âÎöÊı¾İÊ§°Ü,ÊÕµ½µÄÊı¾İµÄ³¤¶È×Ö½Ú²»ÊÇ1" );
				}				
				break;
				//sleep(10);			
			}
		}
		if ( errcnt > 40 ) {
			/*´íÎó´¦Àí*/
			Test_Log( "³õÊ¼»¯socket Ê§°Ü.port=[%d]", TEST_TCP_PORT );
			break; 
		}
	}

	Test_Log("Test_AcceptPCCmd  return ......");
	close(sock);

	return  -1;
}


//Í¨µÀÊÕ·¢º¯Êı
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
		Test_Log( "serialPortNum < 0´íÎó" );
		return -1;
	}
	phyChnl = Test_GetChnPhy(chnlog,portChanel);
	if(phyChnl <=0){
		Test_Log( "phyChnl <=0´íÎó" );
		return -1;
	}
	fd = Test_OpenSerial(TEST_alltty[serialPortNum]);
	if(fd <=0){
		Test_Log( "Test_PortSendRecv´ò¿ª´®¿Ú´íÎó" );
		return -1;
	}
	Test_Log( "Test_PortSendRecv¿ªÊ¼²âÊÔÍ¨µÀ[%d]" ,chnlog);
	//Test_Log("ÏòPC·¢ËÍÊı¾İ");
	for(i=0;i<tryn;i++){
		memset(cmd,0x5a,sizeof(cmd));
		cmd[0] = 0x1B;
		cmd[1] = 0x30+phyChnl-1;
		
		if( sizeof(cmd) > MAX_PORT_CMD_LEN ) {
			Test_Log( "Ïò¶Ë¿ÚĞ´µÄÊı¾İ³¤¶È[%d]³¬¹ı×î´ó³¤¶È[%d]", sizeof(cmd), MAX_PORT_CMD_LEN );
			close(fd);
			return -1;
		}		 
		cmd[2] = 0x00+ (sizeof(cmd) -3 ) ;
		//memcpy( cmd+3, buff, len );
		cnt =sizeof(cmd);
		sleep(3);
		Test_Log("Test_SendRecv·¢ËÍÊı¾İ[%s]",Test_Asc2Hex(cmd, cnt));
		switch(serialPortNum){
			case 3:Test_Act_P(TEST_SEM_COM3);
				break;
			case 4:Test_Act_P(TEST_SEM_COM4);
				break;
			case 5:Test_Act_P(TEST_SEM_COM5);
				break;
			case 6:Test_Act_P(TEST_SEM_COM6);
				break;
			default:Test_Log( "´®¿ÚºÅ[%d]´íÎó",serialPortNum );
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
			default:Test_Log( "´®¿ÚºÅ[%d]´íÎó",serialPortNum );
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
			default:Test_Log( "´®¿ÚºÅ[%d]´íÎó",serialPortNum );
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
			default:Test_Log( "´®¿ÚºÅ[%d]´íÎó",serialPortNum );
				break;			
		}
		if(ret <=0){
			Test_Log( "Ïò¶Ë¿ÚĞ´Êı¾İÊ§°Ü" );
			sleep(5);
			continue;
		}
		//break;
		sleep(3);//µÈ´ı¶Ô·½Ö´ĞĞÍê±Ï
		bzero(buff,sizeof(buff));
		cnt = RECLEN;//58
		ret = Test_PortRead(fd, serialPortNum, phyChnl, buff, &cnt);
		if(ret == 1){
			/*//³É¹¦ÔÙ¶ÁÒ»´Î,È·±£·¢ËÍµÄ¶¼¶ÁÈ¡µ½
			//sleep(1);//µÈ´ı¶Ô·½Ö´ĞĞÍê±Ï
			bzero(buff,sizeof(buff));
			cnt = RECLEN;//58
			ret = Test_PortRead(fd, serialPortNum, phyChnl, buff, &cnt);
			ret = 1;//»Ö¸´³É¹¦×´Ì */
			break;
		}else{
			Test_Log( "Test_PortRead¶Á[´®¿Ú%d][Í¨µÀ%02x]Êı¾İÊ§°Ü" ,serialPortNum, phyChnl );
			//²»³É¹¦ÔÙÊÔÒ»´Î
			//sleep(1);//µÈ´ı¶Ô·½Ö´ĞĞÍê±Ï
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
			default:Test_Log( "´®¿ÚºÅ[%d]´íÎó",serialPortNum );
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
			default:Test_Log( "´®¿ÚºÅ[%d]´íÎó",serialPortNum );
				break;			
		}		
		if(ret <=0){
			Test_Log( "¶Ë¿Ú¶ÁÊı¾İÊ§°Ü" );
			sleep(1);
			continue;
		}
		Test_Log("Test_SendRecv½ÓÊÕÊı¾İ[%s]",Test_Asc2Hex(buff, cnt));	
		if(  (0x30+phyChnl-1 == buff[0])||(0x30+phyChnl-1 == buff[1])){
			break;
		}else{
			sleep(1);
			continue;
		}*/
	}
	if( (ret <=0) ||(i >=tryn) ){
		Test_Log( "´Ó¶Ë¿Ú[%d]¶ÁÊı¾İ[%d]´ÎÈ«²¿Ê§°Ü" ,chnlog,tryn);
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
		Test_Log( "serialPortNum < 0´íÎó" );
		return -1;
	}
	phyChnl = Test_GetChnPhy(chnlog,portChanel);	
	if(phyChnl <=0){
		Test_Log( "phyChnl <=0´íÎó" );
		return -1;
	}
	fd = Test_OpenSerial(TEST_alltty[serialPortNum]);
	if(fd <=0){
		Test_Log( "Test_PortRecvSend´ò¿ª´®¿Ú´íÎó" );
		return -1;
	}
	Test_Log( "Test_PortRecvSend¿ªÊ¼²âÊÔÍ¨µÀ[%d]" ,chnlog);
	 //´ÓPC½ÓÊÕ·µ»ØÊı¾İ
	//Test_Log("´ÓPC½ÓÊÕ·µ»ØÊı¾İ");
	for(i=0;i<tryn;i++){		
		sleep(1);//µÈ´ı¶Ô·½Ö´ĞĞÍê±Ï
		bzero(buff,sizeof(buff));
		cnt =RECLEN;//58
		ret = Test_PortRead(fd, serialPortNum, phyChnl, buff, &cnt);
		if(ret <0){
			Test_Log( "Test_PortRead¶ÁÊı¾İÊ§°Ü" );
			//sleep(1);
			continue;	
		}
		memset(cmd,0x5a,sizeof(cmd));
		cmd[0] = 0x1B;
		cmd[1] = 0x30+phyChnl-1;
		
		if( sizeof(cmd) > MAX_PORT_CMD_LEN ) {
			Test_Log( "Ïò¶Ë¿ÚĞ´µÄÊı¾İ³¤¶È[%d]³¬¹ı×î´ó³¤¶È[%d]", sizeof(cmd), MAX_PORT_CMD_LEN );
			close(fd);
			return -1;
		}		 
		cmd[2] = 0x00+ (sizeof(cmd) -3 ) ;
		//memcpy( cmd+3, buff, len );
		cnt =sizeof(cmd);
		sleep(1);
		Test_Log("Test_SendRecv·¢ËÍÊı¾İ[%s]",Test_Asc2Hex(cmd, cnt));
		switch(serialPortNum){
			case 3:Test_Act_P(TEST_SEM_COM3);
				break;
			case 4:Test_Act_P(TEST_SEM_COM4);
				break;
			case 5:Test_Act_P(TEST_SEM_COM5);
				break;
			case 6:Test_Act_P(TEST_SEM_COM6);
				break;
			default:Test_Log( "´®¿ÚºÅ[%d]´íÎó",serialPortNum );
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
			default:Test_Log( "´®¿ÚºÅ[%d]´íÎó",serialPortNum );
				break;			
		}
		//ret = Test_WriteTty(fd, cmd, cnt);
		if(ret <=0){
			Test_Log( "Ïò¶Ë¿ÚĞ´Êı¾İÊ§°Ü" );
			sleep(1);
			continue;
		}
		sleep(1);//ÔÙ·¢ËÍÒ»´Î
		switch(serialPortNum){
			case 3:Test_Act_P(TEST_SEM_COM3);
				break;
			case 4:Test_Act_P(TEST_SEM_COM4);
				break;
			case 5:Test_Act_P(TEST_SEM_COM5);
				break;
			case 6:Test_Act_P(TEST_SEM_COM6);
				break;
			default:Test_Log( "´®¿ÚºÅ[%d]´íÎó",serialPortNum );
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
			default:Test_Log( "´®¿ÚºÅ[%d]´íÎó",serialPortNum );
				break;			
		}
		//ret = Test_WriteTty(fd, cmd, cnt);
		if(ret <=0){
			Test_Log( "Ïò¶Ë¿ÚĞ´Êı¾İÊ§°Ü" );
			sleep(1);
			continue;
		}
		break;
	}
	if( (ret <=0) ||(i >=tryn) ){
		Test_Log( "´Ó¶Ë¿Ú[%d]¶ÁÊı¾İ[%d]´ÎÈ«²¿Ê§°Ü" ,chnlog, tryn);
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
		Test_Log( "serialPortNum < 0´íÎó" );
		return -1;
	}
	phyChnl = Test_GetChnPhy(chnlog,portChanel);
	if(phyChnl <=0){
		Test_Log( "phyChnl <=0´íÎó" );
		return -1;
	}
	fd = Test_OpenSerial(TEST_alltty[serialPortNum]);
	if(fd <=0){
		Test_Log( "Test_PortSendRecv´ò¿ª´®¿Ú´íÎó" );
		return -1;
	}
	Test_Log( "Test_PortSendRecv¿ªÊ¼²âÊÔÍ¨µÀ[%d]" ,chnlog);
	//Test_Log("ÏòPC·¢ËÍÊı¾İ");
	for(i=0;i<tryn;i++){
		memset(cmd,0x5a,sizeof(cmd));
		cmd[0] = 0x1B;
		cmd[1] = 0x30+phyChnl-1;
		
		if( sizeof(cmd) > MAX_PORT_CMD_LEN ) {
			Test_Log( "Ïò¶Ë¿ÚĞ´µÄÊı¾İ³¤¶È[%d]³¬¹ı×î´ó³¤¶È[%d]", sizeof(cmd), MAX_PORT_CMD_LEN );
			close(fd);
			return -1;
		}		 
		cmd[2] = 0x00+ (sizeof(cmd) -3 ) ;
		//memcpy( cmd+3, buff, len );
		cnt =sizeof(cmd);
		sleep(3);
		Test_Log("Test_SendRecv·¢ËÍÊı¾İ[%s]",Test_Asc2Hex(cmd, cnt));
		switch(serialPortNum){
			case 3:Test_Act_P(TEST_SEM_COM3);
				break;
			case 4:Test_Act_P(TEST_SEM_COM4);
				break;
			case 5:Test_Act_P(TEST_SEM_COM5);
				break;
			case 6:Test_Act_P(TEST_SEM_COM6);
				break;
			default:Test_Log( "´®¿ÚºÅ[%d]´íÎó",serialPortNum );
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
			default:Test_Log( "´®¿ÚºÅ[%d]´íÎó",serialPortNum );
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
			default:Test_Log( "´®¿ÚºÅ[%d]´íÎó",serialPortNum );
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
			default:Test_Log( "´®¿ÚºÅ[%d]´íÎó",serialPortNum );
				break;			
		}*/
		if(ret <=0){
			Test_Log( "Ïò¶Ë¿ÚĞ´Êı¾İÊ§°Ü" );
			sleep(5);
			continue;
		}
		//break;
		sleep(10);//µÈ´ı¶Ô·½Ö´ĞĞÍê±Ï
		/*
		bzero(buff,sizeof(buff));
		cnt = RECLEN;//58
		ret = Test_PortRead(fd, serialPortNum, phyChnl, buff, &cnt);
		if(ret == 1){
			///³É¹¦ÔÙ¶ÁÒ»´Î,È·±£·¢ËÍµÄ¶¼¶ÁÈ¡µ½
			//sleep(1);//µÈ´ı¶Ô·½Ö´ĞĞÍê±Ï
			//bzero(buff,sizeof(buff));
			//cnt = RECLEN;//58
			//ret = Test_PortRead(fd, serialPortNum, phyChnl, buff, &cnt);
			//ret = 1;//»Ö¸´³É¹¦×´//
			break;
		}else{
			Test_Log( "Test_PortRead¶Á[´®¿Ú%d][Í¨µÀ%02x]Êı¾İÊ§°Ü" ,serialPortNum, phyChnl );
			//²»³É¹¦ÔÙÊÔÒ»´Î
			//sleep(1);//µÈ´ı¶Ô·½Ö´ĞĞÍê±Ï
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
			default:Test_Log( "´®¿ÚºÅ[%d]´íÎó",serialPortNum );
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
			default:Test_Log( "´®¿ÚºÅ[%d]´íÎó",serialPortNum );
				break;			
		}		
		if(ret <=0){
			Test_Log( "¶Ë¿Ú¶ÁÊı¾İÊ§°Ü" );
			sleep(1);
			continue;
		}
		Test_Log("Test_SendRecv½ÓÊÕÊı¾İ[%s]",Test_Asc2Hex(buff, cnt));	
		if(  (0x30+phyChnl-1 == buff[0])||(0x30+phyChnl-1 == buff[1])){
			break;
		}else{
			sleep(1);
			continue;
		}*/
	}
	if( (ret <=0) ||(i >=tryn) ){
		Test_Log( "´Ó¶Ë¿Ú[%d]¶ÁÊı¾İ[%d]´ÎÈ«²¿Ê§°Ü" ,chnlog,tryn);
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
		Test_Log( "serialPortNum < 0´íÎó" );
		return -1;
	}
	phyChnl = Test_GetChnPhy(chnlog,portChanel);	
	if(phyChnl <=0){
		Test_Log( "phyChnl <=0´íÎó" );
		return -1;
	}
	fd = Test_OpenSerial(TEST_alltty[serialPortNum]);
	if(fd <=0){
		Test_Log( "Test_PortRecv´ò¿ª´®¿Ú´íÎó" );
		return -1;
	}
	Test_Log( "Test_PortRecv¿ªÊ¼²âÊÔÍ¨µÀ[%d]" ,chnlog);
	 //´ÓPC½ÓÊÕ·µ»ØÊı¾İ
	//Test_Log("´ÓPC½ÓÊÕ·µ»ØÊı¾İ");
	for(i=0;i<tryn;i++){		
		sleep(1);//µÈ´ı¶Ô·½Ö´ĞĞÍê±Ï
		bzero(buff,sizeof(buff));
		cnt =RECLEN;//58
		ret = Test_PortRead(fd, serialPortNum, phyChnl, buff, &cnt);
		if(ret <=0){
			Test_Log( "Test_PortRead¶ÁÊı¾İÊ§°Ü" );
			sleep(10);
			continue;	
		}else{
			Test_Log( "Test_PortRead¶ÁÊı¾İ³É¹¦" );
			return 1;
		}
		/*sleep(10);
		
		memset(cmd,0x5a,sizeof(cmd));
		cmd[0] = 0x1B;
		cmd[1] = 0x30+phyChnl-1;
		
		if( sizeof(cmd) > MAX_PORT_CMD_LEN ) {
			Test_Log( "Ïò¶Ë¿ÚĞ´µÄÊı¾İ³¤¶È[%d]³¬¹ı×î´ó³¤¶È[%d]", sizeof(cmd), MAX_PORT_CMD_LEN );
			close(fd);
			return -1;
		}		 
		cmd[2] = 0x00+ (sizeof(cmd) -3 ) ;
		//memcpy( cmd+3, buff, len );
		cnt =sizeof(cmd);
		sleep(1);
		Test_Log("Test_SendRecv·¢ËÍÊı¾İ[%s]",Test_Asc2Hex(cmd, cnt));
		switch(serialPortNum){
			case 3:Test_Act_P(TEST_SEM_COM3);
				break;
			case 4:Test_Act_P(TEST_SEM_COM4);
				break;
			case 5:Test_Act_P(TEST_SEM_COM5);
				break;
			case 6:Test_Act_P(TEST_SEM_COM6);
				break;
			default:Test_Log( "´®¿ÚºÅ[%d]´íÎó",serialPortNum );
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
			default:Test_Log( "´®¿ÚºÅ[%d]´íÎó",serialPortNum );
				break;			
		}
		//ret = Test_WriteTty(fd, cmd, cnt);
		if(ret <=0){
			Test_Log( "Ïò¶Ë¿ÚĞ´Êı¾İÊ§°Ü" );
			sleep(1);
			continue;
		}
		sleep(1);//ÔÙ·¢ËÍÒ»´Î
		switch(serialPortNum){
			case 3:Test_Act_P(TEST_SEM_COM3);
				break;
			case 4:Test_Act_P(TEST_SEM_COM4);
				break;
			case 5:Test_Act_P(TEST_SEM_COM5);
				break;
			case 6:Test_Act_P(TEST_SEM_COM6);
				break;
			default:Test_Log( "´®¿ÚºÅ[%d]´íÎó",serialPortNum );
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
			default:Test_Log( "´®¿ÚºÅ[%d]´íÎó",serialPortNum );
				break;			
		}
		//ret = Test_WriteTty(fd, cmd, cnt);
		if(ret <=0){
			Test_Log( "Ïò¶Ë¿ÚĞ´Êı¾İÊ§°Ü" );
			sleep(1);
			continue;
		}
		break;*/
	}
	if( (ret <=0) ||(i >=tryn) ){
		Test_Log( "´Ó¶Ë¿Ú[%d]¶ÁÊı¾İ[%d]´ÎÈ«²¿Ê§°Ü" ,chnlog, tryn);
		close(fd);
		return -1;
	}	
	close(fd);
	return 0;
}
//´®¿ÚÊÕ·¢º¯Êı
int Test_ComSendRecv(int serialPortNum){
	int i, ret, tmp,fd,n,k;//,, sockfd
	unsigned char buff[64] ;
	int timeout = 5, tryn = 3;
	char		cmd[16];
	int			cnt;
	
	
	fd = Test_OpenSerial(TEST_alltty[serialPortNum]);
	if(fd <=0){
		Test_Log( "Test_ComRecvSend´ò¿ª´®¿Ú´íÎó" );
		return -1;
	}
	//Test_Log("ÏòPC·¢ËÍÊı¾İ");
	Test_Log( "Test_ComSendRecv¿ªÊ¼²âÊÔ´®¿Ú[%d]" ,serialPortNum);
	for(i=0;i<tryn;i++){
		memset(cmd,0xa5,sizeof(cmd));
		if( sizeof(cmd) > MAX_PORT_CMD_LEN ) {
			Test_Log( "Ïò´®¿Ú[%d]Ğ´µÄÊı¾İ³¤¶È[%d]³¬¹ı×î´ó³¤¶È[%d]", serialPortNum,sizeof(cmd), MAX_PORT_CMD_LEN );
			close(fd);
			return -1;
		}		 
		//cmd[2] = 0x00+ (sizeof(cmd) -3 ) ;
		//memcpy( cmd+3, buff, len );
		cnt =sizeof(cmd);
		Test_Log("Test_ComRecvSend·¢ËÍÊı¾İ[%s]",Test_Asc2Hex(cmd, cnt));
		ret = Test_WriteTty(fd, cmd, cnt);
		if(ret <=0){
			Test_Log( "Ïò´®¿Ú[%d]Ğ´Êı¾İÊ§°Ü" ,serialPortNum);
			sleep(5);
			continue;
		}
		//break;
		sleep(2);
		bzero(buff,sizeof(buff));
		cnt =sizeof(cmd);
		ret = Test_ReadTty(fd, buff, &cnt);
		if(ret <=0){
			Test_Log( "´®¿Ú[%d]¶ÁÊı¾İÊ§°Ü",serialPortNum );
			sleep(5);
			continue;
		}
		Test_Log("Test_ComRecvSend½ÓÊÕÊı¾İ[%s]",Test_Asc2Hex(buff, cnt));	
		break;
	}
	if( (ret <=0) ||(i >=tryn) ){
		Test_Log( "´Ó´®¿Ú[%d]¶ÁÊı¾İ[%d]´ÎÈ«²¿Ê§°Ü" ,serialPortNum,tryn);
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
		Test_Log( "Test_ComRecvSend´ò¿ª´®¿Ú´íÎó" );
		return -1;
	}
	//Test_Log("ÏòPC·¢ËÍÊı¾İ");
	Test_Log( "Test_ComRecvSend¿ªÊ¼²âÊÔ´®¿Ú[%d]" ,serialPortNum);
	for(i=0;i<tryn;i++){		
		sleep(1);//µÈ´ı¶Ô·½Ö´ĞĞÍê±Ï
		bzero(buff,sizeof(buff));
		cnt =sizeof(cmd);
		ret = Test_ReadTty(fd, buff, &cnt);
		if(ret <=0){
			Test_Log( "´®¿Ú[%d]¶ÁÊı¾İÊ§°Ü",serialPortNum );
			sleep(5);
			continue;
		}
		Test_Log("Test_ComRecvSend½ÓÊÕÊı¾İ[%s]",Test_Asc2Hex(buff, cnt));
		sleep(5);
		//break;
		memset(cmd,0x5a,sizeof(cmd));
		if( sizeof(cmd) > MAX_PORT_CMD_LEN ) {
			Test_Log( "Ïò´®¿Ú[%d]Ğ´µÄÊı¾İ³¤¶È[%d]³¬¹ı×î´ó³¤¶È[%d]", serialPortNum,sizeof(cmd), MAX_PORT_CMD_LEN );
			close(fd);
			return -1;
		}		 
		//cmd[2] = 0x00+ (sizeof(cmd) -3 ) ;
		//memcpy( cmd+3, buff, len );
		cnt =sizeof(cmd);
		Test_Log("Test_ComRecvSend·¢ËÍÊı¾İ[%s]",Test_Asc2Hex(cmd, cnt));
		ret = Test_WriteTty(fd, cmd, cnt);
		if(ret <=0){
			Test_Log( "Ïò´®¿Ú[%d]Ğ´Êı¾İÊ§°Ü" ,serialPortNum);
			sleep(5);
			continue;
		}
		sleep(2);//ÔÙ·¢ËÍÒ»´Î
		cnt =sizeof(cmd);
		Test_Log("Test_ComRecvSend·¢ËÍÊı¾İ[%s]",Test_Asc2Hex(cmd, cnt));
		ret = Test_WriteTty(fd, cmd, cnt);
		if(ret <=0){
			Test_Log( "Ïò´®¿Ú[%d]Ğ´Êı¾İÊ§°Ü" ,serialPortNum);
			sleep(5);
			continue;
		}
		break;
	}
	if( (ret <=0) ||(i >=tryn) ){
		Test_Log( "´Ó´®¿Ú[%d]¶ÁÊı¾İ[%d]´ÎÈ«²¿Ê§°Ü" ,serialPortNum,tryn);
		close(fd);
		return -1;
	}
	
	close(fd);
	return 0;
}


//¸ù¾İ¼ì²âµ½µÄUÅÌÃû³Æ¹ÒÔØUÅÌ
static int Main_mountUsb(){

	unsigned char *fileName = "/proc/partitions";
	unsigned char buff[64],usbDev[4],devName[64],mountName[128],rmDev[64];
	FILE *fp;
	int i, j,ret, first_sdx;
	
	fp = fopen(fileName, "rt");
	if(fp == NULL){
		char pbuff[256];
		bzero(pbuff,sizeof(pbuff));
		sprintf( pbuff, "fileName[%s] ´ò¿ªÊ§°Ü!",fileName);
		Test_Log(pbuff);
		return -1;
	}
	
	bzero(buff, sizeof(buff));	
	bzero(usbDev, sizeof(usbDev)); //¶ÁÈ¡  UÅÌÉè±¸Ãû³Æ
	
	i = 0;
	first_sdx = 0; //µÚÒ»´Î¶ÁÈ¡µ½sd£¬ÖÃ1
	while(i < 15){
		fgets(buff, sizeof(buff), fp);

		if(1 == first_sdx){
			//UÅÌÃû³Æ±£´æµ½usbDevÖĞ
			for(j =0; j < sizeof(buff); j++){
				if(buff[j] == 's'&& buff[++j] == 'd'){ //µÚ¶ş´Î¼ì²âµ½sdx
					usbDev[0] = buff[++j];
					usbDev[1] = buff[++j];
					break;
				}
			}
			break;
		}

		for(j =0; j < sizeof(buff); j++){
			if(buff[j] == 's'&& buff[++j] == 'd'){ //µÚ¶ş´Î¼ì²âµ½sdx
				first_sdx = 1;  //µÚÒ»´Î¼ì²âµ½sdx
			}
		}
		
		bzero(buff, sizeof(bzero));
		i++;
	}

	if(i == sizeof(buff)){
		char pbuff[256];
		bzero(pbuff,sizeof(pbuff));
		sprintf( pbuff, "¶ÁÈ¡UÅÌÃû³ÆÊ§°Ü!buff[%s]", buff);
		Test_Log(pbuff);
		return -1;
	}

	//´´½¨Éè±¸ÎÄ¼ş
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

	//³¢ÊÔÉ¾³ıÒÑÓĞÉè±¸ÎÄ¼ş£¬ÖØĞÂ´´½¨
	bzero(rmDev, sizeof(rmDev));
	sprintf(rmDev,"/bin/rm /dev/sd%s", usbDev);
	system(rmDev);

	ret = system(devName);
	if(ret < 0){
		char pbuff[256];
		bzero(pbuff,sizeof(pbuff));
		sprintf( pbuff, "devName[%s] ´´½¨UÅÌÉè±¸ÎÄ¼şÊ§°Ü!", devName);
		Test_Log(pbuff);
		return -1;
	}

	//Ğ¶ÔØ¹ÒÔØÏî
	system(Test_USB_UM);
	//¹ÒÔØUÅÌ
	bzero(mountName,sizeof(mountName));
	sprintf(mountName,"/bin/mount -t vfat /dev/sd%s /home/ftp",usbDev);
	ret = system(mountName);
	if(ret < 0){
		char pbuff[256];
		bzero(pbuff,sizeof(pbuff));
		sprintf( pbuff, "mountName[%s] ¹ÒÔØUÅÌÊ§°Ü!", mountName );
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
		Test_Log("Test_readUSB²ÎÊıÖµ´íÎó,Öµ[%d]",usbnum);
		return -1;
	}
	
	//¹Ò½ÓUÅÌ
      	ret = Main_mountUsb();
	if( ret < 0 )
	{
		char pbuff[256];
		bzero(pbuff,sizeof(pbuff));
		sprintf( pbuff, "***¹ÒÔØUSBÃüÁîÖ´ĞĞÊ§°Ü");
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
	//È¥³ı¹Ò½ÓUÅÌ
	ret = system(Test_USB_UM);
	if( ret < 0 )
	{
		Test_Log( "***USBÃüÁî[%s]Ö´ĞĞÊ§°Ü\n" ,Test_USB_UM);
		return -1;//exit(1);
	}	
	if(  0 != memcmp( my_data,Test_USB_Data,sizeof(Test_USB_Data) )  ){
		Test_Log( "¶ÁÈ¡µÄÊı¾İ[%s]ºÍÓ¦µ±¶ÁÈ¡µ½µÄÊı¾İ[%s]²»Í¬\n" ,my_data,Test_USB_Data);
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
		Test_Log( "ÊµÊ±½ø³Ì³õÊ¼»¯socketÊ§°Ü.port=[%s]\n", TEST_TCP_PORT );
		//Free( cmd );
		return  ;
	}
	//fcntl(sock, F_SETFL, O_NONBLOCK);   // ·Ç×èÈûÄ£Ê½
	
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
		if (newsock < 0) {  // Èç¹ûÃ»ÓĞÁ¬½Ó£¬Ôò¼àÌı
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
		Test_Log("Test_AcceptPCCmdÁ¬½Ó,soketºÅ[%d]", tmp_sock);
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
				/*´íÎó´¦Àí*/
				Test_Log( "½ÓÊÕÊı¾İ[%d]´Î¶¼Ê§°Ü", tryNum );
				break; 
			}
			ret = Test_TcpRecvDataInTime(newsock, buffer, &msg_lg, timeout );//TIME_INFINITE	
			if (ret < 0) {
				if (errno == 4) {  // time out
					Test_Log("½ø³Ì³¬Ê±,½ÓÊÕµ½ºóÌ¨ÃüÁî buffer[%s]\n" , Test_Asc2Hex(buffer, 32) );//
					continue;
				} else if (errno == 11)  {  // maybe CD quit or restart, && (gpIfsf_shm->need_reconnect == 0))
					//gpIfsf_shm->need_reconnect = 1;
					Test_Log("Client need to reconnect to Server");
				}	
				Test_Log( "½ø³Ì½ÓÊÕÊı¾İÊ§°Ü,errno:[%d][%s],buffer: %s", errno, strerror(errno), \
					Test_Asc2Hex(buffer, 2) );
				errcnt++;
				sleep(1);
				continue;
			} else {
				Test_Log( "<<<½ø³ÌÊÕµ½ÃüÁî: [%s(%d)]", Test_Asc2Hex(buffer, msg_lg ), msg_lg);				
				if ( (0 == (buffer[0]>>4) )||(0 ==( buffer[0]&0x0f) )  ){ //received data must be error!!
					Test_Log( "received data error" );
					errcnt++;
					sleep(1);
					continue;
				}

				if (0 == msg_lg){ //receivd data must be error!!
					Test_Log( "received size is  0 error" );
					errcnt++;
					//»Ø¸´´íÎó
					sBuffer[0] = 0x1f;
					sBuffer[1] = 0;
					//memcpy(&sBuffer[1],gpCSS_Shm->css._STATION_NO_BCD,5);
					ret = Test_TcpSendDataInTime(newsock, sBuffer, 2, timeout);
					if(ret <=0){				
						Test_Log("ÊµÊ±½ø³Ì»Ø¸´Êı¾İ Ê§°Ü");
					}
					
					sleep(1);
					continue;
				}

				
				
				//´¦ÀíºóÌ¨ÃüÁî.
				Test_Log( "¿ªÊ¼´¦ÀíÊÕµ½ÃüÁî: [%s(%d)]", Test_Asc2Hex(buffer, msg_lg ), msg_lg);
				if (0 == buffer[1]){ 
					if(0x99 == buffer[0]){
						Test_Log( "ÊÕµ½ÍË³öÃüÁî: [%s(%d)],ÍË³ö", Test_Asc2Hex(buffer, msg_lg ), msg_lg);
						
					}
				}
				
				break;
				//sleep(10);			
			}
		}
		if ( errcnt > 40 ) {
			/*´íÎó´¦Àí*/
			Test_Log( "³õÊ¼»¯socket Ê§°Ü.port=[%d]", TEST_TCP_PORT );
			break; 
		}
	}

	Test_Log("Test_AcceptPCCmd  Stop ......");
	close(sock);

	return  ;
}
/*//Ö÷´ÓFCCÍ¬²½
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
		Test_Log( "***Fcc_SynStart²ÎÊı´íÎó,³ÌĞòÍË³ö" );
		exit(-1);
	}
	while(1){
		n = 3;
		ret = Test_TCPSendRecv(WaitSendBuff[NumFcc-1],5, WaitRecvBuff,&n);
		if(ret <0){
			Test_Log("·¢ËÍ²âÊÔÖĞ¼äÍ¬²½Öµ´íÎó¼ÌĞø");
			sleep(5);
			continue;
		}else{
			if( (0xff != WaitRecvBuff[0]) || (0x01 != WaitRecvBuff[1])|| (WaitFlag[NumFcc-1] != WaitRecvBuff[2]) ){//0 != memcmp(TestFlag,"2200",2)
				sleep(2);
				continue;
			}else{
				Test_Log("²âÊÔÖĞ¼äÍ¬²½³É¹¦,¼ÌĞø²âÊÔ");
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
		Test_Log("Test_Com_Thread²ÎÊıÖµ´íÎó,Öµ[%d],±íÊ¾´®¿ÚºÅ",sn);
		if(2 == FccNum){
			Test_Log("***´®¿Ú[%d]²âÊÔÊ§°Ü",sn);
		}else{
			Test_Log("´®¿Ú[%d]²âÊÔÊ§°Ü",sn);
		}
		return;
	}
	if(2 == FccNum){
		Test_Log("***¿ªÊ¼´®¿Ú[%d]²âÊÔ",sn);		
		ret = Test_ComRecvSend(sn);
		if(ret <0){			
			Test_Log("***´®¿Ú[%d]²âÊÔÊ§°Ü",sn);				
		}else{
			Test_Result[2]  = Test_Result[2]  | (1<<(8-sn-1) );
			Test_Log("***´®¿Ú[%d]²âÊÔ³É¹¦",sn);
		}
	}else if(1 == FccNum){
		Test_Log("¿ªÊ¼´®¿Ú[%d]²âÊÔ",sn);
		ret = Test_ComSendRecv(sn);
		if(ret <0){			
			Test_Log("´®¿Ú[%d]²âÊÔÊ§°Ü",sn);				
		}else{
			Test_Result[2]  = Test_Result[2]  | (1<<(8-sn-1) );
			Test_Log("´®¿Ú[%d]²âÊÔ³É¹¦",sn);
		}
	}else{
		Test_Log("***Ö÷´ÓFCC±êÖ¾³ö´í[%d],ÍË³ö",FccNum);
		ret = Test_PortReadEnd();
		exit(0);
	}
	//return;
}
void Test_Port_Thread(void  *arg){//argÍ¨µÀºÅ-1
	int i,k,n,ret;//,spn
	int  sn;
	sn = (int *)arg;
	//sn = (int ) *arg;
	sn++;//snÍ¨µÀºÅ
	if(sn<=0 ||sn >64){//
		Test_Log("Test_Port_Thread²ÎÊıÖµ´íÎó,Öµ[%d],±íÊ¾Í¨µÀºÅ",sn);
		return;
	}
	//¼ÆËãÍ¨µÀÊıÄ¿	
	k=portChanel[3]+portChanel[4]+portChanel[5]+portChanel[6];
	/*//¼ÆËã´®¿ÚºÅspn
	spn = sn;
	for(i=3;i<=6;i++){
		if((spn-=portChanel[i])<=0){
			spn = i;
			break;
		}
	}*/
	if(1 == FccNum){
		n = ( (sn-1) / 8) +1;
		Test_Log( "¿ªÊ¼²âÊÔÍ¨µÀ[%d]" ,sn);
		ret = Test_PortSendRecv(sn);
		if(ret <0){
			Test_Log("Í¨µÀ[%d]²âÊÔÊ§°Ü",sn);
		}else{
			Test_Act_P(TEST_SEM_RESULT);
			Test_Result[3+n-1] = Test_Result[3+n-1] | (1<<(8*n-sn) );
			Test_Act_V(TEST_SEM_RESULT);
			Test_Log("Í¨µÀ[%d]²âÊÔ³É¹¦", sn);
		}				
	}else if(2 == FccNum){			
			n = ( (sn-1) / 8) +1;
			Test_Log( "***¿ªÊ¼²âÊÔÍ¨µÀ[%d]" ,sn);
			ret = Test_PortRecvSend(sn);
			if(ret <0){
				Test_Log("***Í¨µÀ[%d]²âÊÔÊ§°Ü",sn);
			}else{
				Test_Act_P(TEST_SEM_RESULT);
				Test_Result[3+n-1] = Test_Result[3+n-1] | (1<<(8*n-sn) );
				Test_Act_V(TEST_SEM_RESULT);
				Test_Log( "***Í¨µÀ[%d]²âÊÔ³É¹¦" ,sn);
			}		
	}else{
		Test_Log("***Ö÷´ÓFCC±êÖ¾³ö´í[%d],ÍË³ö",FccNum);
		ret = Test_PortReadEnd();
		exit(0);
	}
}
//Gilbarco×¨ÓÃÏß³Ì
void Test_Port_Thread_Grb(void  *arg){//argÍ¨µÀºÅ-1
	int i,k,n,ret;//,spn
	int  sn;
	sn = (int *)arg;
	//sn = (int )*arg;
	sn++;//snÍ¨µÀºÅ
	if(sn<=0 ||sn >64){//
		Test_Log("Test_Port_Thread²ÎÊıÖµ´íÎó,Öµ[%d],±íÊ¾Í¨µÀºÅ",sn);
		return;
	}
	//¼ÆËãÍ¨µÀÊıÄ¿	
	k=portChanel[3]+portChanel[4]+portChanel[5]+portChanel[6];
	/*//¼ÆËã´®¿ÚºÅspn
	spn = sn;
	for(i=3;i<=6;i++){
		if((spn-=portChanel[i])<=0){
			spn = i;
			break;
		}
	}*/
	if(1 == FccNum){
		n = ( (sn-1) / 8) +1;
		Test_Log( "¿ªÊ¼²âÊÔÍ¨µÀ[%d]" ,sn);
		ret = Test_PortSend(sn);
		if(ret <0){
			Test_Log("Í¨µÀ[%d]²âÊÔÊ§°Ü",sn);
		}else{
			Test_Act_P(TEST_SEM_RESULT);
			Test_Result[3+n-1] = Test_Result[3+n-1] | (1<<(8*n-sn) );
			Test_Act_V(TEST_SEM_RESULT);
			Test_Log("Í¨µÀ[%d]²âÊÔ³É¹¦", sn);
		}				
	}else if(2 == FccNum){			
			n = ( (sn-1) / 8) +1;
			Test_Log( "***¿ªÊ¼²âÊÔÍ¨µÀ[%d]" ,sn);
			ret = Test_PortRecv(sn);
			if(ret <0){
				Test_Log("***Í¨µÀ[%d]²âÊÔÊ§°Ü",sn);
			}else{
				Test_Act_P(TEST_SEM_RESULT);
				Test_Result[3+n-1] = Test_Result[3+n-1] | (1<<(8*n-sn) );
				Test_Act_V(TEST_SEM_RESULT);
				Test_Log( "***Í¨µÀ[%d]²âÊÔ³É¹¦" ,sn);
			}		
	}else{
		Test_Log("***Ö÷´ÓFCC±êÖ¾³ö´í[%d],ÍË³ö",FccNum);
		ret = Test_PortReadEnd();
		exit(0);
	}
}


//Ö÷´ÓFCCÍ¬²½
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
		Test_Log( "***Fcc_SynStart²ÎÊı´íÎó,³ÌĞòÍË³ö" );
		ret = Test_PortReadEnd();
		exit(-1);
	}
	while(1){
		n = 3;
		ret = Test_TCPSendRecv(WaitSendBuff[NumFcc-1],5, WaitRecvBuff,&n);
		if(ret <0){
			Test_Log("·¢ËÍ²âÊÔÖĞ¼äÍ¬²½Öµ´íÎó¼ÌĞø");
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


//Ö÷³ÌĞò
int main( int argc, char* argv[] )
{
	//#define GUN_MAX_NUM 256 //×î¶à256°ÑÇ¹
	int i, j, n,k,len,ret,fd;//,pid,isChild
	unsigned char TestFlag[8];
	pthread_t ThreadChFlag[64];//Í¨µÀÏß³ÌÊı×é
	pthread_t ThreadSrFlag[8];//´®¿ÚÏß³ÌÊı×é
	if (argc == 2) {
	       if (strcmp(argv[1], "-v") == 0) {
			printf( "%s version: %s\n", argv[0], PCD_VER);
	       } 
		return 0;
	}

	
	printf("¿ªÊ¼ÔËĞĞ²âÊÔ³ÌĞò\n");
	
	//³õÊ¼»¯ÈÕÖ¾
	printf("³õÊ¼»¯ÈÕÖ¾.\n");
	ret = Test_InitLog(1);//0: UDPÈÕÖ¾,1:´òÓ¡ÈÕÖ¾
	if(ret < 0){
		printf("ÈÕÖ¾³õÊ¼»¯³ö´í.\n");
		exit(-1);  
	}

		
	/*//³õÊ¼»¯¹²ÏíÄÚ´æ
	ret = Test_InitTheKeyFile();
	if( ret < 0 )
	{
		Test_Log( "³õÊ¼»¯Key FileÊ§°Ü\n" );
		exit(1);
	}
	*/
		
	//³õÊ¼»¯´®¿ÚÍ¨µÀÊıÄ¿
	Test_Log("¿ªÊ¼³õÊ¼»¯´®¿ÚÍ¨µÀÊıÄ¿");
	ret = Test_InitPortChn();
	if( ret < 0 )
	{
		Test_Log( "³õÊ¼»¯´®¿ÚÍ¨µÀÊ§°Ü\n" );
		exit(-1);
	}
	
	//³õÊ¼»¯ÅäÖÃ²ÎÊı
	Test_Log("¿ªÊ¼³õÊ¼»¯ÅäÖÃ²ÎÊı");
	ret = Test_InitConfig();
	if(ret <0){
		Test_Log("ÅäÖÃ²ÎÊı³õÊ¼»¯³ö´í.");
		exit(-1);
	}

	//³õÊ¼»¯×¢²á
	Test_Log("¿ªÊ¼³õÊ¼»¯×¢²á");
	ret = Test_Login();
	if(ret <0){
		Test_Log("³õÊ¼»¯×¢²á³ö´í.");
		exit(-1);
	}
	// ´´½¨Ò»¸öĞÅºÅÁ¿.
	ret = Test_CreateSem( TEST_SEM_COM3 );
	if( ret < 0 ) {
		Test_Log( "´´½¨ĞÅºÅÁ¿Ê§°Ü" );
		exit(-1);//return -1;
	}
	ret = Test_CreateSem( TEST_SEM_COM4 );
	if( ret < 0 ) {
		Test_Log( "´´½¨ĞÅºÅÁ¿Ê§°Ü" );
		exit(-1);//return -1;
	}
	ret = Test_CreateSem( TEST_SEM_COM5 );
	if( ret < 0 ) {
		Test_Log( "´´½¨ĞÅºÅÁ¿Ê§°Ü" );
		exit(-1);//return -1;
	}
	ret = Test_CreateSem( TEST_SEM_COM6 );
	if( ret < 0 ) {
		Test_Log( "´´½¨ĞÅºÅÁ¿Ê§°Ü" );
		exit(-1);//return -1;
	}
	ret = Test_CreateSem( TEST_SEM_RESULT );
	if( ret < 0 ) {
		Test_Log( "´´½¨ĞÅºÅÁ¿Ê§°Ü" );
		exit(-1);//return -1;
	}
	
	//Í¨µÀ¶Á³õÊ¼»¯
	ret = Test_PortReadInit();
	if( ret <= 0 ) {
		Test_Log( "Í¨µÀ¶Á³õÊ¼»¯Ê§°Ü" );
		exit(-1);//return -1;
	}
	
	//Ñ­»·²âÊÔ
	while(1){
		//µÈ´ı²âÊÔ¿ªÊ¼ÃüÁî
		if(2 == FccNum){
			Test_Log("***µÈ´ı²âÊÔ¿ªÊ¼ÃüÁî");
		}else{
			Test_Log("µÈ´ı²âÊÔ¿ªÊ¼ÃüÁî");
		}
		ret = Test_WaitStartCmd();
		if(ret <0){
			Test_Log("¿ªÊ¼²âÊÔÃüÁî´íÎó,ÍË³ö");
			exit(-1);
		}
# if 0
		if(2 == FccNum){
			Test_Log("***¿ªÊ¼¿ØÖÆ¿Ú(´®¿Ú0)²âÊÔ",i);
			ret = Test_ComSendRecv(0);
			if(ret <0){
				Test_Log("***¿ØÖÆ¿Ú(´®¿Ú0)²âÊÔÊ§°Ü",i);
				//exit(0);
			}else{
				//TestFlag[0] = TestFlag[0] | (1<<7 );					
				Test_Log("***¿ØÖÆ¿Ú(´®¿Ú0)²âÊÔ³É¹¦",i);
			}
			ret = Test_SynStart(FccNum);
		}else{
			sleep(30);
			ret = Test_SynStart(FccNum);
		}
#endif		
		//-----------------------------
		//¿ªÊ¼²âÊÔ
		bzero(Test_Result,sizeof(Test_Result) );
		bzero(ThreadChFlag,sizeof(ThreadChFlag) );
		bzero(ThreadSrFlag,sizeof(ThreadSrFlag) );
		Test_Result[0] = 0x22;
		Test_Result[1] = 0x0a;
		
			
		
		//¼ÆËãÍ¨µÀÊıÄ¿k = 0;	
		k=portChanel[3]+portChanel[4]+portChanel[5]+portChanel[6];
		if(k>64){
			Test_Log("***Í¨µÀ(±³°åÁ¬½ÓºÅ)ÊıÄ¿Ì«¶à[%d],ÍË³ö",k);
			exit(0);
		}
		//Í¨µÀ(±³°åÁ¬½ÓºÅ)²âÊÔ
		for(i=0;i<k;i++){
			if(0 == Gilbarco){
				if ( pthread_create( &ThreadChFlag[i], NULL, (void *)Test_Port_Thread, (void *)i ) )  {
					Test_Log("´´½¨Í¨µÀ[%d] Ïß³Ì´íÎó",i+1);
					abort();
				}
			}else if(1 == Gilbarco){
				if ( pthread_create( &ThreadChFlag[i], NULL, (void *)Test_Port_Thread_Grb, (void *)i ) )  {
					Test_Log("´´½¨³¤¼ªÍ¨µÀ[%d] Ïß³Ì´íÎó",i+1);
					abort();
				}
			}else{
				Test_Log("³¤¼ª±êÖ¾Öµ[%d] ´íÎó",Gilbarco);
			}
			sleep(5);
			//sleep(10);
			//ret = Test_SynStart(FccNum);
		}
				
		//´®¿Ú²âÊÔ
		if(2 == FccNum){
			Test_Log("***¿ªÊ¼´®¿Ú²âÊÔ");
		}else{
			Test_Log("¿ªÊ¼´®¿Ú²âÊÔ");
		}
		bzero(TestFlag,sizeof(TestFlag));
		if(1 == FccNum){
			//´®¿Ú0µ¥¶ÀÓëPC²âÊÔ
			for(i=1;i<3;i++){
				//Test_Log("¿ªÊ¼´®¿Ú[%d]²âÊÔ",i);
				if ( pthread_create( &ThreadSrFlag[i], NULL, (void *)Test_Com_Thread, (void *)i ) )  {
					Test_Log("´´½¨´®¿Ú[%d] Ïß³Ì´íÎó",i+1);
					ret = Test_PortReadEnd();
					abort();
				}
				sleep(1);
				//sleep(10);
				//ret = Test_SynStart(FccNum);	
				}
		}else if(2 == FccNum){
			//´®¿Ú0µ¥¶ÀÓëPC²âÊÔ
			for(i=1;i<3;i++){
				//Test_Log("¿ªÊ¼´®¿Ú[%d]²âÊÔ",i);
				if ( pthread_create( &ThreadSrFlag[i], NULL, (void *)Test_Com_Thread, (void *)i ) )  {
					Test_Log("***´´½¨´®¿Ú[%d] Ïß³Ì´íÎó",i+1);
					ret = Test_PortReadEnd();
					abort();
				}
				sleep(1);
				//sleep(10);
				//ret = Test_SynStart(FccNum);
			}
		}else{
			Test_Log("***Ö÷´ÓFCC±êÖ¾³ö´í[%d], ÍË³ö",FccNum);
			ret = Test_PortReadEnd();
			exit(0);
		}
		//memcpy(&Test_Result[2],TestFlag,1);
		//Test_Result[2] = TestFlag[0];
		
		//USB¿Ú²âÊÔ,Ö»ÊÇ²âÊÔ´ÓFCC
		if(2 == FccNum){
			Test_Log("***¿ªÊ¼¿ØÖÆ¿Ú(´®¿Ú0)²âÊÔ",i);
			ret = Test_ComSendRecv(0);
			if(ret <0){
				Test_Log("***¿ØÖÆ¿Ú(´®¿Ú0)²âÊÔÊ§°Ü",i);
				//exit(0);
			}else{
				TestFlag[0] = TestFlag[0] | (1<<7 );					
				Test_Log("***¿ØÖÆ¿Ú(´®¿Ú0)²âÊÔ³É¹¦",i);
			}
			Test_Result[2] =Test_Result[2] | TestFlag[0];
			Test_Log("***¿ªÊ¼USB¿Ú²âÊÔ");
			bzero(TestFlag,sizeof(TestFlag));
			//USB1¿Ú²âÊÔ
			Test_Log("***¿ªÊ¼USB¿Ú1²âÊÔ");
			ret = Test_readUSB(1);
			if(ret <0){
				Test_Log("***USB¿Ú[1]²âÊÔÊ§°Ü");
				sleep(1);
				//USB2¿Ú²âÊÔ
				Test_Log("***¿ªÊ¼USB¿Ú2²âÊÔ");
				ret = Test_readUSB(2);
				if(ret <0){
					Test_Log("***USB¿Ú[2]²âÊÔÊ§°Ü");
				}else{
					Test_Log("***USB¿Ú[2]²âÊÔ³É¹¦");
					TestFlag[0] = TestFlag[0] | 0xc0;//0x40
				}
					//exit(0);
			}else{
				Test_Log("***USB¿Ú[1]²âÊÔ³É¹¦");
				TestFlag[0] = TestFlag[0] | 0xc0;//0x80
			}
		

			Test_Result[11] = TestFlag[0];
		}else{
			Test_Log("²»ÊÇ´ÓFCC[%d],²»²âÊÔ¿ØÖÆ¿Ú(´®¿Ú0)ºÍUSB¿Ú",FccNum);
		}
		for(i=0;i<k;i++){
			if ( pthread_join ( ThreadChFlag[i], NULL ) ) {
				Test_Log("joining Í¨µÀ[%d]Ïß³Ì´íÎó",i+1);
				ret = Test_PortReadEnd();
				abort();
			}else{
				Test_Log("joining Í¨µÀ[%d]Ïß³Ì½áÊø",i+1);
			}
		}
		for(i=1;i<3;i++){
			if(0==ThreadSrFlag[i]){
				continue;
			}
			else if ( pthread_join ( ThreadSrFlag[i], NULL ) ) {
				Test_Log("joining´®¿Ú[%d]Ïß³Ì´íÎó",i);
				ret = Test_PortReadEnd();
				abort();
			}else{
				Test_Log("joining´®¿Ú[%d]Ïß³Ì½áÊø",i);
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
		ret = Test_SynStart(FccNum);//Í¬²½
		ret = Test_PortReadEnd();
		if( ret < 0 ) {
			Test_Log( "Í¨µÀ¶Á½áÊøÊ§°Ü" );
			return -1;
		}
		//·¢ËÍ²âÊÔ½á¹û
		n = 2;
		 if(2 == FccNum){
		 	ret = Test_TCPSendRecv(Test_Result,12, TestFlag,&n);
			if(ret <0){
				Test_Log("·¢ËÍ²âÊÔ½á¹û´íÎó,ÍË³ö");
				exit(-1);
			}else{
				if( (0x22 != TestFlag[0]) || (0x0 != TestFlag[1]) ){//0 != memcmp(TestFlag,"2200",2)
					Test_Log("·¢ËÍ²âÊÔ½á¹û,ÊÕµ½µÄÊı¾İ[%02x%02x]´íÎó,ÍË³ö",TestFlag[0],TestFlag[1]);
					exit(-1);
				}
			}
			k = 0;//¼ÆËãÍ¨µÀÊıÄ¿	
			k=portChanel[3]+portChanel[4]+portChanel[5]+portChanel[6];
			//bzero(TestFlag,sizeof(TestFlag));
			TestFlag[0] =1;
			if((0xe0&Test_Result[2]) !=0xe0){//´®¿ÚÖ»Òª0,1,2Èı¸ö³É¹¦¾Í¿ÉÒÔÁË
				TestFlag[0] = 0;
				Test_Log("´®¿Ú0,1,2ÖĞÖÁÉÙÒ»¸öÃ»ÓĞ²âÊÔÍ¨¹ı[%02x]",Test_Result[2]);
				//exit(0);
			}
			for(i=0;i<k/8;i++){//Í¨µÀÒªÈ«²¿Í¨¹ı
				if(Test_Result[i+3] != 0xff){
					TestFlag[0] = 0;
					Test_Log("Í¨µÀ[%d~%d]²âÊÔÃ»ÓĞÍ¨¹ı",i*8+1,i*8 +8);
				}
			}
			if((0x80&Test_Result[11]) !=0x80){//USBÖ»ÒªµÚÒ»¸ö³É¹¦¼´¿É
				TestFlag[0] = 0;
				Test_Log("USB1²âÊÔÃ»ÓĞÍ¨¹ı[%02x]",Test_Result[11]);
			}
			if(1 != TestFlag[0] ){
				Test_Log("***´ÓFCC²âÊÔÊ§°Ü,Çë¹Ø±Õ´ÓFCC,¼ì²éÇé¿ö,µÈ´ıÔÙ´Î²âÊÔ");
				exit(-1);
			}else{
			 	Test_Log("***´ÓFCC²âÊÔ³É¹¦,µÈ´ıÏÂÒ»¸ö´ÓFCC²âÊÔ");
				exit(0);
			 }
			 //Test_Log("***×Ô²âÊÔ³ÌĞòÒ»´Î²âÊÔÍê±Ï,µÈ´ıÏÂ´Î²âÊÔ");
		 }else{
		  	Test_Log("×Ô²âÊÔ³ÌĞòÒ»´Î²âÊÔÍê±Ï,µÈ´ıÏÂ´Î²âÊÔ");
		  }
	}
	 //Test_AcceptPCCmd();
	
	 Test_Log("***×Ô²âÊÔ³ÌĞòÒì³£ÍË³ö");
	return -1;
}







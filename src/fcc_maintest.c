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

//ËøĞÅºÅÁ¿Ë÷ÒıÖµ


//----------------------------------------------
//ÊµÊ±ÈÕÖ¾¶¨Òå
//===============================
#define MAIN_MAX_VARIABLE_PARA_LEN 1024 
#define MAIN_MAX_FILE_NAME_LEN 1024 
#define USB_DEV_SDA 0	//USB´ÓÉè±¸ºÅ
#define USB_DEV_SDB 16
#define USB_DEV_SDC 32
#define USB_DEV_SDD 48
static char gsFccFileName[MAIN_MAX_FILE_NAME_LEN + 1];	//¼ÇÂ¼µ÷ÊÔĞÅÏ¢µÄÎÄ¼şÃû
static int giFccLine;								//¼ÇÂ¼µ÷ÊÔĞÅÏ¢µÄĞĞ
static int giFccFd;                                                       //ÈÕÖ¾ÎÄ¼ş¾ä±ú
static char gcFccLogFlag;
static unsigned  char  gsLocalIP[16];
//×Ô¶¨Òå·Ö¼¶ÎÄ¼şÈÕÖ¾,¼û__Log.
#define Main_Log Main_SetFileAndLine( __FILE__, __LINE__ ), Main__Log
//#define Main_Log Main_SetFileAndLine( __FILE__, __LINE__ ),Main__FileLog
//--------------------------------------------------------------

//-------------------------------------------------------
//´®¿Ú¶¨Òå
//´®¿ÚÌØÕ÷,´®¿Ú´Ó0¿ªÊ¼
//#define START_PORT 3
#define MAX_PORT 6
//#define PORT_USED_COUNT 3
//´®¿Ú×Ö·û´®
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
//´®¿ÚÍ¨µÀÊıÄ¿,²»Á¬½ÓÍ¨µÀÖµÎª0.
//unsigned char portChanel[MAX_PORT+1];
//----------------------------------------------------

//-------------------------------------------
//³õÊ¼»¯ÅäÖÃ²ÎÊı
//-------------------------------------------
static char PC_SERVER_IP[16]; //PC·şÎñÆ÷IPµØÖ·
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
//ÏòPC×¢²á
/*FCCÏòPC ×¢²á:
*FCC·¢:1104+±¾»úIPµØÖ·+4¸ö´®¿ÚÍ¨µÀÊı,PC»Ø¸´:110111»òÕß 110122,ÆäÖĞ110111Îª1ºÅ,110122Îª2ºÅFCC.*/
//static unsigned char FccNum;
//-------------------------------------------------

//------------------------------------------------

static char Main_USB_UM[]="/bin/umount -l /home/ftp/";
static char Main_USB_File[]="/home/ftp/fcctest.txt";
static char Main_USB_Data[]="1234567890";
//---------------------------------------------

//------------------------------------------------
//½á¹û»Ø¸´Êı¾İ
static unsigned char Main_Result[8];
//---------------------------------------------




//------------------------------------------------
static unsigned  long  Main_GetLocalIP();

//-------------------------------------------------

//----------------------------------------------------

//--------------------------------------------------------------------------------
//ÊµÊ±ÈÕÖ¾
//----------------------------------------------
void Main_SetLogFd( const int fd )
{
	giFccFd= fd;
	if(  giFccFd <=  0){
		printf( " {ÈÕÖ¾ÎÄ¼şÃèÊö·û´íÎó[%d] }\n", fd );
		
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
		(struct sockaddr *)(&remoteSrv), sizeof(struct sockaddr));//·¢ËÍÊı¾İ
	/*if (ret < 0) {				
		printf("ĞÄÌøÊı¾İ·¢ËÍÊ§°Ü,error:[%s]", strerror(errno));	
	}*/		
	
}

//giLogLevel=0, no log;giLogLevel=1, only print log info; giLogLevel = 2, print log info and write log info to file 
void Main__Log(const char *fmt,...){//const int loglevel, const int fd, 
	va_list argptr;
	char sMessage[MAIN_MAX_VARIABLE_PARA_LEN + 1];
	char fileName[MAIN_MAX_FILE_NAME_LEN + 1];	/*¼ÇÂ¼µ÷ÊÔĞÅÏ¢µÄÎÄ¼şÃû*/
	int line;								/*¼ÇÂ¼µ÷ÊÔĞÅÏ¢µÄĞĞ*/
	//int level = giFccLogLevel;
	
	
	Main_GetFileAndLine(fileName, &line);	
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
	if(0==gcFccLogFlag)Main_SendUDP( giFccFd, sMessage, strlen(sMessage) );//write
	else{
		printf(sMessage);
		Main_SendUDP( giFccFd, sMessage, strlen(sMessage) );
	}
	//write( giFccFd, "\n", strlen("\n") );
	
	return;
}

int Main_InitLog(int flag){//flag---0: UDPÈÕÖ¾,1:´òÓ¡ÈÕÖ¾
	int sockfd;
	unsigned char *pIP;
	struct in_addr in_ip;
    	 /*if( ( fd = open( MAIN_LOG_FILE, O_WRONLY|O_CREAT|O_TRUNC, 00644 ) ) <= 0 ) {
		printf( "´ò¿ªÈÕÖ¾ÎÄ¼ş[%s]Ê§°Ü\n", MAIN_LOG_FILE );
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
	else printf("Main_InitLogº¯ÊıÈÕÖ¾³õÊ¼»¯´íÎó,flag[%d]\n",flag);
	//Main_SetLogLevel(loglevel);
	return 1;
}
//------------------------------------------------

//-----------------------------------------------------------------------
//¹«ÓÃº¯Êı
//-----------------------------------------------------------------------

unsigned char* Main_Asc2Hex( unsigned char *cmd, int cmdlen )
{
	static char retcmd[MAIN_MAX_ORDER_LEN*2+1];
	int	i;

	memset( retcmd, 0, sizeof(retcmd) );
	for( i = 0; i < cmdlen; i++ ) {
		sprintf(retcmd + i * 2, "%02x", cmd[i]);//ÎªÊ²Ã´Òªi * 2
	}

	return retcmd;
}
//»ñÈ¡±¾»úIP,·µ»Ø±¾»úIPµØÖ·£¬·µ»Ø-1Ê§°Ü£¬½â¾öDHCPÎÊÌâ
static unsigned  long  Main_GetLocalIP(){//²Î¼ûtcp.hºÍtcp.cÎÄ¼şÖĞÓĞ,¿ÉÒÔÑ¡ÔñÒ»¸ö.
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

	port = GetTcpPort( sPortName );	//¸ù¾İ¶Ë¿ÚºÅ»òÕß¶Ë¿ÚÃûÈ¡¶Ë¿Ú È¡³öµÄportÎªÍøÂç×Ö½ÚĞò//
	if( port == INPORT_NONE )	//GetTcpPortÒÑ¾­±¨¸æÁË´íÎó,´Ë´¦Ê¡ÂÔ//
		return -1;
	*/

	sockFd = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );//zhangjm IPPROTO_TCP ÊÇtcpĞ­Òé
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
		Main_Log( "´íÎóºÅ:%d", errno );
		return -1;
	}

	if( listen( sockFd, backLog ) < 0 )
	{
		Main_Log( "´íÎóºÅ:%d",errno );
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

		newsock = accept( sock, ( SA* )&ClientAddr, &len );//zhangjm¿Í»§µÄµØÖ·²»¹ØÏµ£¬ËùÒÔÇåÁã»òÎª¿Õ£¬ÆäÖĞsockÎªÇ°Ãæ¿Í»§Á¬½ÓÖ¸¶¨µÄstock
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

	Main_Log( "´íÎóºÅ:%d",errno );
	return -1;
}
/*µ±nTimeOutÉèÖÃÎªTIME_INFINITE»òÕßÉèÖÃµÄÊ±¼ä±ÈÏµÍ³È±Ê¡Ê±¼ä³¤Ê±,
 connectÔÚÍê³É·µ»ØÇ°¶¼Ö»µÈ´ıÏµÍ³È±Ê¡Ê±¼ä*/
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

	if( ( addr = GetTcpAddr( sIPName ) ) == INADDR_NONE )//zhangjmÊ²Ã´ÒâË¼å
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
static ssize_t Main_TcpSendDataInTime( int fd, const char *s, size_t len,  int timeout )
{
#if 1
	ssize_t	 retlen;
	size_t	sendlen;
	char	*head;
	char	*t;
	char	buf[MAIN_TCP_MAX_SIZE];//zhangjm TCP_MAX_SIZEÃ»ÓĞ¶¨Òå
	int	mflag; 	/*±êÊ¶ÊÇ·ñ·ÖÅäÁË¿Õ¼ä 0Î´·ÖÅä. 1ÒÑ·ÖÅä*/
	int	n;
	int     i;
	
	Assert( s != NULL && *s != 0 );//Ìõ¼şÎªÕæÔò¼ÌĞø£¬·ñÔòµ÷ÓÃabortÍ£Ö¹¡£
	Assert( len != 0  );
	Assert( timeout > 0 || timeout == TIME_INFINITE );//ÕâÀïµÄtimeoutÊÇÓÃÀ´¸ÉÊ²Ã´µÄÊ±¼äå

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
	
	Main_Log("before WirteLengthInTime -------------");
	n = WriteByLengthInTime( fd, t, sendlen, timeout );
	Main_Log("after WirteLengthInTime -------------");
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
	
	Main_Log("before WirteLengthInTime -------------");	
	n = WriteByLengthInTime( fd, t, sendlen, timeout );
	Main_Log("after WirteLengthInTime -----------length=[%d]",n);
	
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
		printf("ÊÕµ½µÄ³¤¶È[%d]Ğ¡ÓÚÔ¤ÆÚÊÕµ½µÄ³¤¶È[%d],Öµ[%s]\n", n,len,Main_Asc2Hex(t, len));
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
	ret = access( sFileName, F_OK );//³É¹¦·µ»Ø0.ÅĞ¶ÏÊÇ·ñ´æÔÚsFileName F_OKÎªÊÇ·ñ´æÔÚR_OK£¬W_OKÓëX_OKÓÃÀ´¼ì²éÎÄ¼şÊÇ·ñ¾ßÓĞ¶ÁÈ¡¡¢Ğ´ÈëºÍÖ´ĞĞµÄÈ¨ÏŞ¡£
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
//³õÊ¼»¯ÅäÖÃ²ÎÊı
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
	ret = Main_IsFileExist(fileName);//³É¹¦·µ»Ø1
	if(ret != 1){
		char pbuff[256];
		bzero(pbuff,sizeof(pbuff));
		sprintf( pbuff, "file[%s] not exist in readIni  error!! ", fileName );
		Main_Log(pbuff);
		exit(1);//zhangjmexit(1)¸ÉÊ²Ã´µÄ
		return -2;
	}
	f = fopen( fileName, "rt" );//·µ»ØÖ¸Ïò¸ÃÎÄ¼şµÄÖ¸Õë
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
		c = fgetc( f);//fËùÖ¸µÄÎÄ¼şÖĞ¶ÁÈ¡Ò»¸ö×Ö·û¡£Èô¶Áµ½ÎÄ¼şÎ²¶øÎŞÊı¾İÊ±±ã·µ»ØEOF
		if( c == EOF)
			break;
		my_data[i] = c;
	}
	my_data[i] = 0;	
	fclose( f );
	
	
	bzero(&(fcc_ini[0].name), sizeof(MAIN_INI_NAME_VALUE)*MAIN_INI_MAX_CNT);	
	i = 0;
	while (my_data[0] !='\0'){
		tmp = Main_split(my_data, '=');// split data to get field name.·ÖÀë³ö=ºÅÖ®Ç°µÄ¶«Î÷
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
	//´ò¿ªÎÄ¼ş
	fd = open( fileName, O_WRONLY|O_CREAT|O_TRUNC, 00644 );
	if( fd < 0 )
	{
		//UserLog( "´ò¿ªµ±Ç°ÔËĞĞ¹¤×÷Â·¾¶ĞÅÏ¢ÎÄ¼ş[%s]Ê§°Ü.",fileName );
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
	ret = Main_readIni(configFile);//¶ÁÅäÖÃÎÄ¼ş£¬½«IPºÍ¸÷¸ö¶Ë¿Ú¶Á³ö´æ·ÅÔÚ½á¹¹Ìåfcc_iniÖĞ·µ»ØÖµÎª¶Áµ½µÄ¸öÊı
	if(ret < 0){
		Main_Log("readIni error in _InitServerIP");
		return -1;
	}
	iniFlag = 0;
	//static int PC_SERVER_TCP_PORT,PC_SERVER_UDP_PORT,MAIN_TCP_PORT;
	for(i=0;i<ret;i++){
		if( memcmp(fcc_ini[i].name,"PC_SERVER_IP",11) == 0){
			memcpy(PC_SERVER_IP, fcc_ini[i].value,sizeof(PC_SERVER_IP ) );//sizeof(fcc_ini[i].value memcmpÁ½¸ö±È½ÏÒ»°ã´ó·µ»Ø0Ç°Ò»¸ö´ó·µ»ØÕıºóÒ»¸ö´ó·µ»Ø¸º
			Main_Log("PC_SERVER_IP[%s]",PC_SERVER_IP);
			iniFlag += 1;
		}else if( memcmp(fcc_ini[i].name,"PC_SERVER_TCP_PORT",18) == 0 ){
			PC_SERVER_TCP_PORT=atoi(fcc_ini[i].value);
			Main_Log("PC_SERVER_TCP_PORT[%d]",PC_SERVER_TCP_PORT);
			iniFlag += 1;
		}else if( memcmp(fcc_ini[i].name,"PC_SERVER_UDP_PORT",18) == 0 ){
			PC_SERVER_UDP_PORT=atoi(fcc_ini[i].value);//atoiÊÇ½«×Ö·û´®×ª»»ÎªÊı×ÖÈç"100"±ä³É100
			Main_Log("PC_SERVER_UDP_PORT[%d]",PC_SERVER_UDP_PORT);
			iniFlag += 1;
			//memcpy(gFcc._SERVER_UDP_PORT, fcc_ini[2].value, sizeof(fcc_ini[2].value) );
		}else if( memcmp(fcc_ini[i].name,"FCC_TCP_PORT",12) == 0 ){
			MAIN_TCP_PORT=atoi(fcc_ini[i].value);
			Main_Log("MAIN_TCP_PORT[%d]",MAIN_TCP_PORT);
			iniFlag += 1;
			//memcpy(gFcc._SERVER_UDP_PORT, fcc_ini[2].value, sizeof(fcc_ini[2].value) );
		}else{
			//Main_Log("ÅäÖÃÊı¾İ³ö´í,  ²»ÊÇÔ¤ÆÚµÄÈı¸öÅäÖÃ,Ãû×ÖÊÇ[%s],ÖµÊÇ[%s]",fcc_ini[i].name,fcc_ini[i].value);
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


static int Main_OpenSerial( char *ttynam ){
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


//¶ÁĞ´´®¿Ú
int Main_WriteTty(int fd, unsigned char *msg, int msgLen){
	int ret;
	int timeout = 5;
	//unsigned char buff[64],*result;
	
	//fd = OpenSerial( MAIN_alltty[serial_num]);
	if( fd <= 0 )
	{
		Main_Log( "´®¿Úfd[%d]¸ºÖµ´íÎó",fd );
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
		Main_Log( "´®¿Úfd[%d]¸ºÖµ´íÎó",fd );
		return -1;
	}
	//Main_Log( "after Main_Writetty and 0 to moved test!" );
	len = *recvLen;
	if( sizeof(buff) < len)
	{
		Main_Log( "FccReadTty´íÎó,buff³¤¶È[%d]Ğ¡ÓÚ´ı½ÓÊÕ³¤¶È[%d]",sizeof(buff), len);
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
//ÏòPC×¢²á
/*FCCÏòPC ×¢²á:
*FCC·¢:1104+±¾»úIPµØÖ·+4¸ö´®¿ÚÍ¨µÀÊı,PC»Ø¸´:110111»òÕß 110122,ÆäÖĞ110111Îª1ºÅ,110122Îª2ºÅFCC.*/
//static unsigned char FccNum;
//static char PC_SERVER_IP[16]; //PC·şÎñÆ÷IPµØÖ·
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
	
	
	//Í·²¿Êı¾İÉèÖÃ
	
	msgLength =6;
	sendBuffer[0] = 0x11;
	sendBuffer[1] = 0x04;	
	localIP = Main_GetLocalIP();
	
	//FCCµÄ IPµØÖ·
	memcpy(&sendBuffer[2],(unsigned char *)&localIP,4);//zhangjm(unsigned char *)&localIPÊ²Ã´ÒâË¼å

	//´®¿ÚÍ¨µÀÊıÄ¿
	//memcpy(&sendBuffer[6],&portChanel[START_PORT],4);//(unsigned char *)&localIP
	memset(&sendBuffer[6],0,4);
	//i=k;
	//ÏòPCÉÏ´«Êı¾İ
	Main_Log("ÏòPCÉÏ´«Êı¾İ[%s]",Main_Asc2Hex(sendBuffer, msgLength) );
	i = 0;
	sIPName =PC_SERVER_IP;//inet_ntoa(  *((struct in_addr *)(&gpIfsf_shm->recv_hb[i].host_ip))  );
	while(1){
		i++;
		if( i > LOGIN_TIME ) exit(-1);//zhangjmÕâÀïµÄiÊÇÎªÁË3´ÎÎÕÊÖÂï
		sockfd = Main_TcpConnect(sIPName,PC_SERVER_TCP_PORT , timeout);//(timeout+1)/2   //sPortName
		if (sockfd >0 ){
			//sleep(1);
			ret =Main_TcpSendDataInTime(sockfd, sendBuffer, msgLength, timeout);			//Act_V(CD_SOCK_INDEX);			
			if (ret < 0 ){
				Main_Log("TcpSendDataInTime·¢ËÍÊı¾İ²»³É¹¦,Main_Login·¢ËÍÊı¾İ Ê§°Ü,IP[%s],port[%d]",sIPName,PC_SERVER_TCP_PORT);
				close(sockfd);
				sleep(1);//sleep(10000);
				continue;
			}else{				
				break;
			}
		} else {
			Main_Log("Tcp Connect ²»³É¹¦,Main_Login·¢ËÍÊı¾İ Ê§°Ü,IP[%s],port[%d]",sIPName,PC_SERVER_TCP_PORT);
			close(sockfd);
			sleep(1);//sleep(10000); 			
			continue;
			//return -1;
		}
	}
	 //¿ªÊ¼´ÓPC½ÓÊÕ·µ»ØÊı¾İ
	Main_Log("¿ªÊ¼´ÓPC½ÓÊÕ·µ»ØÊı¾İ");
	bzero(sendBuffer,sizeof(sendBuffer));
	for(i=0;i<tryn;i++){
		msgLength = 3;		
		ret =Main_TcpRecvDataInTime(sockfd, sendBuffer, &msgLength, timeout);			//Act_V(CD_SOCK_INDEX);			
		if (ret < 0 ){
			Main_Log("TcpRecvDataInTime½ÓÊÕÊı¾İ²»³É¹¦,Main_Login½ÓÊÕÊı¾İ Ê§°Ü");
			sleep(10);
			continue;
		}
		break;
	}
	
      if( (sendBuffer[0] != 0x11)||(sendBuffer[1] != 0x01) ){
	  	Main_Log("Tcp Connect ²»³É¹¦,Main_Login½ÓÊÕÊı¾İ Ê§°Ü,[%02x%02x%02x]",sendBuffer[0],sendBuffer[1],sendBuffer[2]);
		close(sockfd);
		return -1;
	}
	//CSS_LevelLog("gpCSS_Shm->css._TLG_SEND_INTERVAL¸³Öµ[%02x%02x]",sendBuffer[1],sendBuffer[2]);
      	//if(sendBuffer[2] == 0x11) {
	//		FccNum = 1;
	//		Main_Log("FccNum¸³Öµ[%d],Ö÷FCC",FccNum);
	//}else if(sendBuffer[2] == 0x22){ 
	//	FccNum = 2;
	//	Main_Log("FccNum¸³Öµ[%d],´ÓFCC",FccNum);
	//}else{
	//	Main_Log("Main_Login½ÓÊÕÊı¾İ´íÎó,Main_Login½ÓÊÕÊı¾İ Ê§°Ü");
	//	return -1;
	//}
	close(sockfd);
	//Main_Log("FccNum¸³Öµ[%d]",FccNum);
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
				Main_Log("TcpSendDataInTime·¢ËÍÊı¾İ²»³É¹¦,IP[%s],port[%d]",sIPName,PC_SERVER_TCP_PORT);
				close(sockfd);
				sleep(10);//sleep(10000);
				continue;
			}else{				
				break;
			}
		} else {
			Main_Log("Tcp Connect ²»³É¹¦,IP[%s],port[%d]",sIPName,PC_SERVER_TCP_PORT);
			close(sockfd);
			sleep(60);//sleep(10000); 			
			continue;
			//return -1;
		}
	}
	 //´ÓPC½ÓÊÕ·µ»ØÊı¾İ
	Main_Log("´ÓPC½ÓÊÕ·µ»ØÊı¾İ");
	 //sleep(5);//µÈ´ı¶Ô·½Ö´ĞĞÍê±Ï
	//bzero(recvBuffer,sizeof(recvBuffer));
	for(i=0;i<tryn;i++){
		msgLength = *recvLength;		
		ret =Main_TcpRecvDataInTime(sockfd, recvBuffer, &msgLength, timeout);			//Act_V(CD_SOCK_INDEX);			
		if (ret < 0 ){
			Main_Log("TcpRecvDataInTime½ÓÊÕÊı¾İ²»³É¹¦");
			sleep(10);
			continue;
		}
		break;
	}
      if( (recvBuffer[0] == 0)&&(recvBuffer[1] == 0) ){
	  	Main_Log("Tcp Connect ²»³É¹¦,[%02x%02x%02x]",recvBuffer[0],recvBuffer[1],recvBuffer[2]);
		close(sockfd);
		return -1;
	}	
	close(sockfd);
	memcpy((char *)recvLength,(char *)&msgLength,sizeof(int));
	Main_Log("Main_SendRecv½ÓÊÕÊı¾İ[%s]",Main_Asc2Hex(recvBuffer, msgLength));	
	return 0;
}


//--------------------------------------------------
//Í¨Ñ¶·şÎñ³ÌĞò
//-----------------------------------------------
//static char PC_SERVER_IP[16]; //PC·şÎñÆ÷IPµØÖ·
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
		Main_Log( "ÊµÊ±½ø³Ì³õÊ¼»¯socketÊ§°Ü.port=[%s]\n", MAIN_TCP_PORT );
		//Free( cmd );
		return  ;
	}
	//fcntl(sock, F_SETFL, O_NONBLOCK);   // ·Ç×èÈûÄ£Ê½
	
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
		tmp_sock = Main_TcpAccept(sock);//, NULL
		Main_Log("Main_WaitStartCmdÁ¬½Ó,soketºÅ[%d]", tmp_sock);
		if (tmp_sock > 0) {
			if (newsock > 0) {
				close(newsock);
			}
			newsock = tmp_sock;
			tmp_sock = -1;//zhangjmÎªÊ²Ã´Òª¹Ø±ÕÎÄ¼şÃèÊö·ûnewsockÔÙ¸³Öµ
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
				/*´íÎó´¦Àí*/
				Main_Log( "½ÓÊÕÊı¾İ[%d]´Î¶¼Ê§°Ü", tryNum );
				break; 
			}
			msg_lg = 3;
			sleep(5);
			ret = Main_TcpRecvDataInTime(newsock, buffer, &msg_lg, timeout );//TIME_INFINITE	
			if (ret < 0) {
				if (errno == 4) {  // time out zhangjm errnoÊÇ´ÓÄÇÀï¼ÆÊıµÄ¡
					Main_Log("½ø³Ì³¬Ê±,½ÓÊÕµ½ºóÌ¨ÃüÁî buffer[%s]\n" , Main_Asc2Hex(buffer, msg_lg) );//
					continue;
				} else if (errno == 11)  {  // maybe CD quit or restart, && (gpIfsf_shm->need_reconnect == 0))
					//gpIfsf_shm->need_reconnect = 1;
					Main_Log("Client need to reconnect to Server");
				}	
				Main_Log( "½ø³Ì½ÓÊÕÊı¾İÊ§°Ü,errno:[%d][%s],buffer: %s", errno, strerror(errno), \
					Main_Asc2Hex(buffer, 2) );
				errcnt++;
				sleep(1);
				continue;
			} else {
				Main_Log( "<<<½ø³ÌÊÕµ½ÃüÁî: [%s(%d)]", Main_Asc2Hex(buffer, msg_lg ), msg_lg);				
				if ( 0x22 != buffer[0] ){ //received data must be error!!(0 == (buffer[0]>>4) )||(0 ==( buffer[0]&0x0f) )
					Main_Log( "received data error" );
					errcnt++;
					sleep(1);
					continue;
				}

				if (0 == msg_lg){ //receivd data must be error!!
					Main_Log( "received size is  0 error" );
					errcnt++;
					//»Ø¸´´íÎó
					sBuffer[0] = 0x2f;
					sBuffer[1] = 0;
					//memcpy(&sBuffer[1],gpCSS_Shm->css._STATION_NO_BCD,5);
					ret = Main_TcpSendDataInTime(newsock, sBuffer, 2, timeout);
					if(ret <=0){				
						Main_Log("ÊµÊ±½ø³Ì»Ø¸´Êı¾İ Ê§°Ü");
					}
					
					sleep(1);
					continue;
				}

				
				
				//´¦ÀíºóÌ¨ÃüÁî.
				if (1 == buffer[1]){ 
					if(0x11 == buffer[2]){
						/*if(1 != FccNum) {
							Main_Log("PCÆô¶¯ÃüÁî ´íÎó,±¾»úÖ÷FCC");
							sBuffer[0] = 0x2f;
							sBuffer[1] = 0;
							//memcpy(&sBuffer[1],gpCSS_Shm->css._STATION_NO_BCD,5);
							ret = Main_TcpSendDataInTime(newsock, sBuffer, 2, timeout);
							if(ret <0){				
								Main_Log("½ø³Ì»Ø¸´Êı¾İ Ê§°Ü");
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
							Main_Log("½ø³Ì»Ø¸´Êı¾İ Ê§°Ü");
							//continue;
						}
						errcnt = 0;
						close(newsock);close(sock);
						return 1;//break;
					}/*else if(0x22 == buffer[2]){
						if(2 != FccNum) {
							Main_Log("PCÆô¶¯ÃüÁî ´íÎó,±¾»ú´ÓFCC");
							sBuffer[0] = 0x2f;
							sBuffer[1] = 0;
							//memcpy(&sBuffer[1],gpCSS_Shm->css._STATION_NO_BCD,5);
							ret = Main_TcpSendDataInTime(newsock, sBuffer, 2, timeout);
							if(ret <0){				
								Main_Log("½ø³Ì»Ø¸´Êı¾İ Ê§°Ü");
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
							Main_Log("½ø³Ì»Ø¸´Êı¾İ Ê§°Ü");
							//continue;
						}
						errcnt = 0;
						close(newsock);close(sock);
						return 1;//break;
					}*/
					else{
						Main_Log( "½âÎöÊı¾İÊ§°Ü,ÊÕµ½µÄÊı¾İÖµ²»ÊÇ0x11ºÍ0x22" );
					}
				}  else{
					Main_Log( "½âÎöÊı¾İÊ§°Ü,ÊÕµ½µÄÊı¾İµÄ³¤¶È×Ö½Ú²»ÊÇ1" );
				}				
				break;
				//sleep(10);			
			}
		}
		if ( errcnt > 40 ) {
			/*´íÎó´¦Àí*/
			Main_Log( "³õÊ¼»¯socket Ê§°Ü.port=[%d]", MAIN_TCP_PORT );
			break; 
		}
	}

	Main_Log("Main_AcceptPCCmd  return ......");
	close(sock);

	return  -1;
}




//´®¿ÚÊÕ·¢º¯Êı
int Main_ComSendRecv(int serialPortNum){
	int i, ret, tmp,fd,n,k;//,, sockfd
	unsigned char buff[64] ;
	int timeout = 5, tryn = 5;
	char		cmd[32];
	int			cnt;
	
	
	fd = Main_OpenSerial(MAIN_alltty[serialPortNum]);
	if(fd <=0){
		Main_Log( "Main_ComRecvSend´ò¿ª´®¿Ú´íÎó" );
		return -1;
	}
	//Main_Log("ÏòPC·¢ËÍÊı¾İ");
	Main_Log( "Main_ComSendRecv¿ªÊ¼²âÊÔ´®¿Ú[%d]" ,serialPortNum);
	for(i=0;i<tryn;i++){
		memset(cmd,0xa5,sizeof(cmd));
		if( sizeof(cmd) > MAX_PORT_CMD_LEN ) {
			Main_Log( "Ïò´®¿Ú[%d]Ğ´µÄÊı¾İ³¤¶È[%d]³¬¹ı×î´ó³¤¶È[%d]", serialPortNum,sizeof(cmd), MAX_PORT_CMD_LEN );
			close(fd);
			return -1;
		}		 
		//cmd[2] = 0x00+ (sizeof(cmd) -3 ) ;
		//memcpy( cmd+3, buff, len );
		cnt =sizeof(cmd);
		Main_Log("Main_ComRecvSend·¢ËÍÊı¾İ[%s]",Main_Asc2Hex(cmd, cnt));
		ret = Main_WriteTty(fd, cmd, cnt);
		if(ret <=0){
			Main_Log( "Ïò´®¿Ú[%d]Ğ´Êı¾İÊ§°Ü" ,serialPortNum);
			sleep(5);
			continue;
		}
		//break;
		sleep(5);
		bzero(buff,sizeof(buff));
		cnt =sizeof(cmd);
		ret = Main_ReadTty(fd, buff, &cnt);
		if(ret <=0){
			Main_Log( "´®¿Ú[%d]¶ÁÊı¾İÊ§°Ü",serialPortNum );
			sleep(5);
			continue;
		}
		Main_Log("Main_ComRecvSend½ÓÊÕÊı¾İ[%s]",Main_Asc2Hex(buff, cnt));	
		break;
	}
	if( (ret <=0) ||(i >=tryn) ){
		Main_Log( "´Ó´®¿Ú[%d]¶ÁÊı¾İ[%d]´ÎÈ«²¿Ê§°Ü" ,serialPortNum,tryn);
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
		Main_Log( "Main_ComRecvSend´ò¿ª´®¿Ú´íÎó" );
		return -1;
	}
	//Main_Log("ÏòPC·¢ËÍÊı¾İ");
	Main_Log( "Main_ComRecvSend¿ªÊ¼²âÊÔ´®¿Ú[%d]" ,serialPortNum);
	for(i=0;i<tryn;i++){		
		sleep(1);//µÈ´ı¶Ô·½Ö´ĞĞÍê±Ï
		bzero(buff,sizeof(buff));
		cnt =sizeof(cmd);
		ret = Main_ReadTty(fd, buff, &cnt);
		if(ret <=0){
			Main_Log( "´®¿Ú[%d]¶ÁÊı¾İÊ§°Ü",serialPortNum );
			sleep(5);
			continue;
		}
		Main_Log("Main_ComRecvSend½ÓÊÕÊı¾İ[%s]",Main_Asc2Hex(buff, cnt));
		sleep(5);
		//break;
		memset(cmd,0x5a,sizeof(cmd));
		if( sizeof(cmd) > MAX_PORT_CMD_LEN ) {
			Main_Log( "Ïò´®¿Ú[%d]Ğ´µÄÊı¾İ³¤¶È[%d]³¬¹ı×î´ó³¤¶È[%d]", serialPortNum,sizeof(cmd), MAX_PORT_CMD_LEN );
			close(fd);
			return -1;
		}		 
		//cmd[2] = 0x00+ (sizeof(cmd) -3 ) ;
		//memcpy( cmd+3, buff, len );
		cnt =sizeof(cmd);
		Main_Log("Main_ComRecvSend·¢ËÍÊı¾İ[%s]",Main_Asc2Hex(cmd, cnt));
		ret = Main_WriteTty(fd, cmd, cnt);
		if(ret <=0){
			Main_Log( "Ïò´®¿Ú[%d]Ğ´Êı¾İÊ§°Ü" ,serialPortNum);
			sleep(5);
			continue;
		}
		sleep(5);//ÔÙ·¢ËÍÒ»´Î
		cnt =sizeof(cmd);
		Main_Log("Main_ComRecvSend·¢ËÍÊı¾İ[%s]",Main_Asc2Hex(cmd, cnt));
		ret = Main_WriteTty(fd, cmd, cnt);
		if(ret <=0){
			Main_Log( "Ïò´®¿Ú[%d]Ğ´Êı¾İÊ§°Ü" ,serialPortNum);
			sleep(5);
			continue;
		}
		break;
	}
	if( (ret <=0) ||(i >=tryn) ){
		Main_Log( "´Ó´®¿Ú[%d]¶ÁÊı¾İ[%d]´ÎÈ«²¿Ê§°Ü" ,serialPortNum,tryn);
		close(fd);
		return -1;
	}
	
	close(fd);
	return 0;
}


//¸ù¾İ¼ì²âµ½µÄUÅÌÃû³Æ¹ÒÔØUÅÌ
int Main_mountUsb(){

	unsigned char *fileName = "/proc/partitions";
	unsigned char buff[64],usbDev[4],devName[64],mountName[128],rmDev[64];
	FILE *fp;
	int i, j,ret, first_sdx;
	
	fp = fopen(fileName, "rt");
	if(fp == NULL){
		Main_Log("fileName[%s] ´ò¿ªÊ§°Ü!",fileName);
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
					Main_Log("buff[%s]  usbDev[%s]",buff, usbDev);
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
		Main_Log("¶ÁÈ¡UÅÌÃû³ÆÊ§°Ü!buff[%s]", buff);
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
		Main_Log("devName[%s] ´´½¨UÅÌÉè±¸ÎÄ¼şÊ§°Ü!", devName);
		return -1;
	}

	//Ğ¶ÔØ¹ÒÔØÏî
	system(Main_USB_UM);
	//¹ÒÔØUÅÌ
	bzero(mountName,sizeof(mountName));
	sprintf(mountName,"/bin/mount -t vfat /dev/sd%s /home/ftp",usbDev);
	ret = system(mountName);
	if(ret < 0){
		Main_Log("mountName[%s] ¹ÒÔØUÅÌÊ§°Ü!", mountName);
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
		Main_Log("Main_readUSB²ÎÊıÖµ´íÎó,Öµ[%d]",usbnum);
		return -1;
	}
	//¹Ò½ÓUÅÌ
      	ret = Main_mountUsb();
	if( ret < 0 )
	{
		Main_Log( "***¹ÒÔØUSBÃüÁîÖ´ĞĞÊ§°Ü\n" );
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
	//È¥³ı¹Ò½ÓUÅÌ
	ret = system(Main_USB_UM);
	if( ret < 0 )
	{
		Main_Log( "***USBÃüÁî[%s]Ö´ĞĞÊ§°Ü\n" ,Main_USB_UM);
		return -1;//exit(1);
	}	
	if(  0 != memcmp( my_data,Main_USB_Data,sizeof(Main_USB_Data) )  ){
		Main_Log( "¶ÁÈ¡µÄÊı¾İ[%s]ºÍÓ¦µ±¶ÁÈ¡µ½µÄÊı¾İ[%s]²»Í¬\n" ,my_data,Main_USB_Data);
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
		Main_Log( "ÊµÊ±½ø³Ì³õÊ¼»¯socketÊ§°Ü.port=[%s]\n", MAIN_TCP_PORT );
		//Free( cmd );
		return  ;
	}
	//fcntl(sock, F_SETFL, O_NONBLOCK);   // ·Ç×èÈûÄ£Ê½
	
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
		tmp_sock = Main_TcpAccept(sock);//, NULL
		Main_Log("Main_AcceptPCCmdÁ¬½Ó,soketºÅ[%d]", tmp_sock);
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
				Main_Log( "½ÓÊÕÊı¾İ[%d]´Î¶¼Ê§°Ü", tryNum );
				break; 
			}
			ret = Main_TcpRecvDataInTime(newsock, buffer, &msg_lg, timeout );//TIME_INFINITE	
			if (ret < 0) {
				if (errno == 4) {  // time out
					Main_Log("½ø³Ì³¬Ê±,½ÓÊÕµ½ºóÌ¨ÃüÁî buffer[%s]\n" , Main_Asc2Hex(buffer, 32) );//
					continue;
				} else if (errno == 11)  {  // maybe CD quit or restart, && (gpIfsf_shm->need_reconnect == 0))
					//gpIfsf_shm->need_reconnect = 1;
					Main_Log("Client need to reconnect to Server");
				}	
				Main_Log( "½ø³Ì½ÓÊÕÊı¾İÊ§°Ü,errno:[%d][%s],buffer: %s", errno, strerror(errno), \
					Main_Asc2Hex(buffer, 2) );
				errcnt++;
				sleep(1);
				continue;
			} else {
				Main_Log( "<<<½ø³ÌÊÕµ½ÃüÁî: [%s(%d)]", Main_Asc2Hex(buffer, msg_lg ), msg_lg);				
				if ( (0 == (buffer[0]>>4) )||(0 ==( buffer[0]&0x0f) )  ){ //received data must be error!!
					Main_Log( "received data error" );
					errcnt++;
					sleep(1);
					continue;
				}

				if (0 == msg_lg){ //receivd data must be error!!
					Main_Log( "received size is  0 error" );
					errcnt++;
					//»Ø¸´´íÎó
					sBuffer[0] = 0x1f;
					sBuffer[1] = 0;
					//memcpy(&sBuffer[1],gpCSS_Shm->css._STATION_NO_BCD,5);
					ret = Main_TcpSendDataInTime(newsock, sBuffer, 2, timeout);
					if(ret <=0){				
						Main_Log("ÊµÊ±½ø³Ì»Ø¸´Êı¾İ Ê§°Ü");
					}
					
					sleep(1);
					continue;
				}

				
				
				//´¦ÀíºóÌ¨ÃüÁî.
				Main_Log( "¿ªÊ¼´¦ÀíÊÕµ½ÃüÁî: [%s(%d)]", Main_Asc2Hex(buffer, msg_lg ), msg_lg);
				if (0 == buffer[1]){ 
					if(0x99 == buffer[0]){
						Main_Log( "ÊÕµ½ÍË³öÃüÁî: [%s(%d)],ÍË³ö", Main_Asc2Hex(buffer, msg_lg ), msg_lg);
						
					}
				}
				
				break;
				//sleep(10);			
			}
		}
		if ( errcnt > 40 ) {
			/*´íÎó´¦Àí*/
			Main_Log( "³õÊ¼»¯socket Ê§°Ü.port=[%d]", MAIN_TCP_PORT );
			break; 
		}
	}

	Main_Log("Main_AcceptPCCmd  Stop ......");
	close(sock);

	return  ;
}
/*//Ö÷´ÓFCCÍ¬²½
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
		Main_Log( "***Fcc_SynStart²ÎÊı´íÎó,³ÌĞòÍË³ö" );
		exit(-1);
	}
	while(1){
		n = 3;
		ret = Main_TCPSendRecv(WaitSendBuff[NumFcc-1],5, WaitRecvBuff,&n);
		if(ret <0){
			Main_Log("·¢ËÍ²âÊÔÖĞ¼äÍ¬²½Öµ´íÎó¼ÌĞø");
			sleep(5);
			continue;
		}else{
			if( (0xff != WaitRecvBuff[0]) || (0x01 != WaitRecvBuff[1])|| (WaitFlag[NumFcc-1] != WaitRecvBuff[2]) ){//0 != memcmp(TestFlag,"2200",2)
				sleep(2);
				continue;
			}else{
				Main_Log("²âÊÔÖĞ¼äÍ¬²½³É¹¦,¼ÌĞø²âÊÔ");
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
		Main_Log("Main_Com_Thread²ÎÊıÖµ´íÎó,Öµ[%d],±íÊ¾´®¿ÚºÅ",sn);
		//if(2 == FccNum){
			Main_Log("***´®¿Ú[%d]²âÊÔÊ§°Ü",sn);
		//}else{
		//	Main_Log("´®¿Ú[%d]²âÊÔÊ§°Ü",sn);
		//}
		return;
	}
	if(sn == 0){
		Main_Log("***¿ªÊ¼´®¿Ú[%d]²âÊÔ",sn);		
		ret = 1;//Main_ComSendRecv(sn);
		if(ret <0){			
			Main_Log("***´®¿Ú[%d]²âÊÔÊ§°Ü",sn);				
		}else{
			Main_Result[2]  = Main_Result[2]  | (1<<7 );
			Main_Log("***´®¿Ú[%d]²âÊÔ³É¹¦",sn);
		}
	}else if(sn%2 == 0){
		Main_Log("***¿ªÊ¼´®¿Ú[%d]²âÊÔ",sn);		
		ret = Main_ComRecvSend(sn);
		if(ret <0){			
			Main_Log("***´®¿Ú[%d]²âÊÔÊ§°Ü",sn);				
		}else{
			Main_Result[2]  = Main_Result[2]  | (1<<(8-sn-1) );
			Main_Log("***´®¿Ú[%d]²âÊÔ³É¹¦",sn);
		}
	}else if(sn%2 == 1){
		Main_Log("***¿ªÊ¼´®¿Ú[%d]²âÊÔ",sn);
		ret = Main_ComSendRecv(sn);
		if(ret <0){			
			Main_Log("***´®¿Ú[%d]²âÊÔÊ§°Ü",sn);				
		}else{
			Main_Result[2]  = Main_Result[2]  | (1<<(8-sn-1) );
			Main_Log("***´®¿Ú[%d]²âÊÔ³É¹¦",sn);
		}
	}else{
		Main_Log("***´®¿Ú³ö´í[%d],ÍË³ö",sn);
		exit(0);
	}
	//return;
}

//Ö÷´ÓFCCÍ¬²½
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
		Main_Log( "***Fcc_SynStart²ÎÊı´íÎó,³ÌĞòÍË³ö" );
		exit(-1);
	}
	while(1){
		n = 3;
		ret = Main_TCPSendRecv(WaitSendBuff[NumFcc-1],5, WaitRecvBuff,&n);
		if(ret <0){
			Main_Log("·¢ËÍ²âÊÔÍ¬²½Öµ´íÎó¼ÌĞø");
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


//Ö÷³ÌĞò
int main( int argc, char* argv[] )
{
	//#define GUN_MAX_NUM 256 //×î¶à256°ÑÇ¹
	int i, j, n,k,len,ret,fd;//,pid,isChild
	unsigned char TestFlag[8];
	//pthread_t ThreadChFlag[64];//Í¨µÀÏß³ÌÊı×é
	pthread_t ThreadSrFlag[8];//´®¿ÚÏß³ÌÊı×é
	if (argc == 2) {
	       if (strcmp(argv[1], "-v") == 0) {
			printf( "fcc_selftest. PCD version %s\n", PCD_VER);
	       } 
		return 0;
	}
	
	printf("¿ªÊ¼ÔËĞĞ²âÊÔ³ÌĞò\n");
	
	//³õÊ¼»¯ÈÕÖ¾
	printf("³õÊ¼»¯ÈÕÖ¾.\n");
	ret = Main_InitLog(1);//0: UDPÈÕÖ¾,1:´òÓ¡ÈÕÖ¾
	if(ret < 0){
		printf("ÈÕÖ¾³õÊ¼»¯³ö´í.\n");
		exit(-1);  
	}

		
	/*//³õÊ¼»¯¹²ÏíÄÚ´æ
	ret = Main_InitTheKeyFile();
	if( ret < 0 )
	{
		Main_Log( "³õÊ¼»¯Key FileÊ§°Ü\n" );
		exit(1);
	}
	*/
		
	//³õÊ¼»¯´®¿ÚÍ¨µÀÊıÄ¿
	
	
	//³õÊ¼»¯ÅäÖÃ²ÎÊı
	Main_Log("¿ªÊ¼³õÊ¼»¯ÅäÖÃ²ÎÊı");
	ret = Main_InitConfig();//³É¹¦·µ»Ø0²»³É·µ»Ø-1
	if(ret <0){
		Main_Log("ÅäÖÃ²ÎÊı³õÊ¼»¯³ö´í.");
		exit(-1);
	}

	//³õÊ¼»¯×¢²á
	Main_Log("¿ªÊ¼³õÊ¼»¯×¢²á");
	ret = Main_Login();
	if(ret <0){
		Main_Log("³õÊ¼»¯×¢²á³ö´í.");
		exit(-1);
	}
	Main_Log("³õÊ¼»¯×¢²á³É¹¦");
	sleep(5);
	// ´´½¨Ò»¸öĞÅºÅÁ¿.	
	//Í¨µÀ¶Á³õÊ¼»¯	
	
	//Ñ­»·²âÊÔ
	while(1){
		//µÈ´ı²âÊÔ¿ªÊ¼ÃüÁî
		//if(2 == FccNum){
			Main_Log("***µÈ´ı²âÊÔ¿ªÊ¼ÃüÁî");
		//}else{
		//	Main_Log("µÈ´ı²âÊÔ¿ªÊ¼ÃüÁî");
		//}
		ret = Main_WaitStartCmd();
		if(ret <0){
			Main_Log("¿ªÊ¼²âÊÔÃüÁî´íÎó,ÍË³ö");
			exit(-1);
		}
		Main_Log("²âÊÔ¿ªÊ¼ÃüÁî³É¹¦");
# if 0
		if(2 == FccNum){
			Main_Log("***¿ªÊ¼¿ØÖÆ¿Ú(´®¿Ú0)²âÊÔ",i);
			ret = Main_ComSendRecv(0);
			if(ret <0){
				Main_Log("***¿ØÖÆ¿Ú(´®¿Ú0)²âÊÔÊ§°Ü",i);
				//exit(0);
			}else{
				//TestFlag[0] = TestFlag[0] | (1<<7 );					
				Main_Log("***¿ØÖÆ¿Ú(´®¿Ú0)²âÊÔ³É¹¦",i);
			}
			ret = Main_SynStart(FccNum);
		}else{
			sleep(30);
			ret = Main_SynStart(FccNum);
		}
#endif		
		//-----------------------------
		//¿ªÊ¼²âÊÔ
		bzero(Main_Result,sizeof(Main_Result) );
		//bzero(ThreadChFlag,sizeof(ThreadChFlag) );
		bzero(ThreadSrFlag,sizeof(ThreadSrFlag) );
		Main_Result[0] = 0x22;
		Main_Result[1] = 0x01;
		
			
		
		//¼ÆËãÍ¨µÀÊıÄ¿k = 0;	
		
		//Í¨µÀ(±³°åÁ¬½ÓºÅ)²âÊÔ
		
		//´®¿Ú²âÊÔ
		Main_Log("***¿ªÊ¼´®¿Ú²âÊÔ");
		bzero(TestFlag,sizeof(TestFlag));
		for(i=0; i<=6; i++){
			//Main_Log("¿ªÊ¼´®¿Ú[%d]²âÊÔ",i);
			if ( pthread_create( &ThreadSrFlag[i], NULL, (void *)Main_Com_Thread, (void *)i ) )  {
				Main_Log("´´½¨´®¿Ú[%d] Ïß³Ì´íÎó",i);
				abort();
			}
			sleep(1);
		}		
			
		
		//memcpy(&Main_Result[2],TestFlag,1);
		//Main_Result[2] = TestFlag[0];
		
		//USB¿Ú²âÊÔ
		//if(2 == FccNum){
		/*Main_Log("***¿ªÊ¼¿ØÖÆ¿Ú(´®¿Ú0)²âÊÔ",i);
		ret = Main_ComSendRecv(0);
		if(ret <0){
			Main_Log("***¿ØÖÆ¿Ú(´®¿Ú0)²âÊÔÊ§°Ü",i);
			//exit(0);
		}else{
			TestFlag[0] = TestFlag[0] | (1<<7 );					
			Main_Log("***¿ØÖÆ¿Ú(´®¿Ú0)²âÊÔ³É¹¦",i);
		}
		Main_Result[2] =Main_Result[2] | TestFlag[0];*/
		Main_Log("***¿ªÊ¼USB¿Ú²âÊÔ");
		bzero(TestFlag,sizeof(TestFlag));
		//USB1¿Ú²âÊÔ
		Main_Log("***¿ªÊ¼USB¿Ú1²âÊÔ");
		ret = Main_readUSB(1);
		if(ret <0){
			Main_Log("***USB¿Ú[1]²âÊÔÊ§°Ü");
			//exit(0);
			sleep(1);
			//USB2¿Ú²âÊÔ
			Main_Log("***¿ªÊ¼USB¿Ú2²âÊÔ");
			ret = Main_readUSB(2);
			if(ret <0){
						
			}else{
				Main_Log("***USB¿Ú[2]²âÊÔ³É¹¦");
				TestFlag[0] = TestFlag[0] | 0x01;//0x40
			}			
		}else{
			Main_Log("***USB¿Ú[1]²âÊÔ³É¹¦");
			TestFlag[0] = TestFlag[0] | 0x01;//0x80
		}		
		Main_Result[2] = Main_Result[2] | TestFlag[0];
		//}else{
		//	Main_Log("²»ÊÇ´ÓFCC[%d],²»²âÊÔ¿ØÖÆ¿Ú(´®¿Ú0)ºÍUSB¿Ú",FccNum);
		//}
		/*for(i=0;i<k;i++){
			if ( pthread_join ( ThreadChFlag[i], NULL ) ) {
				Main_Log("joining Í¨µÀ[%d]Ïß³Ì´íÎó",i+1);
				ret = Main_PortReadEnd();
				abort();
			}else{
				Main_Log("joining Í¨µÀ[%d]Ïß³Ì½áÊø",i+1);
			}
		}*/
		for(i=0; i<=6; i++){
			if(0==ThreadSrFlag[i]){
				continue;
			}else if ( pthread_join ( ThreadSrFlag[i], NULL ) ) {
				Main_Log("joining´®¿Ú[%d]Ïß³Ì´íÎó",i);
				abort();
			}else{
				Main_Log("joining´®¿Ú[%d]Ïß³Ì½áÊø",i);
			}
		}
			
		
		/*ret = Main_SynStart(1);//Í¬²½
		if( ret < 0 ) {
			Main_Log( "Í¨µÀ¶Á½áÊøÊ§°Ü" );
			return -1;
		}*/
		//·¢ËÍ²âÊÔ½á¹û
		n = 2;
		// if(2 == FccNum){
	 	ret = Main_TCPSendRecv(Main_Result,3, TestFlag,&n);
		if(ret <0){
			Main_Log("·¢ËÍ²âÊÔ½á¹û´íÎó,ÍË³ö");
			exit(-1);
		}else{
			if( (0x22 != TestFlag[0]) || (0x0 != TestFlag[1]) ){//0 != memcmp(TestFlag,"2200",2)
				Main_Log("·¢ËÍ²âÊÔ½á¹û,ÊÕµ½µÄÊı¾İ[%02x%02x]´íÎó,ÍË³ö",TestFlag[0],TestFlag[1]);
				exit(-1);
			}
		}
		//k = 0;//¼ÆËãÍ¨µÀÊıÄ¿	
		//k=portChanel[3]+portChanel[4]+portChanel[5]+portChanel[6];
		bzero(TestFlag,sizeof(TestFlag));
		TestFlag[0] =1;
		if((0xff&Main_Result[2]) !=0xff){//´®¿ÚÖ»Òª0,1,2Èı¸ö³É¹¦¾Í¿ÉÒÔÁË
			TestFlag[0] = 0;
			Main_Log("´®¿Ú0,1,2,..,6ºÍUSBÖĞÖÁÉÙÒ»¸öÃ»ÓĞ²âÊÔÍ¨¹ı[%02x]",Main_Result[2]);
			//exit(0);
		}
		
		if(1 != TestFlag[0] ){
			Main_Log("***FCCÖ÷°å²âÊÔÊ§°Ü,Çë¹Ø±ÕFCCÖ÷°å,¼ì²éÇé¿ö,µÈ´ıÔÙ´Î²âÊÔ");
			exit(-1);
		}else{
		 	Main_Log("***FCCÖ÷°å²âÊÔ³É¹¦,µÈ´ıÏÂÒ»¸öFCCÖ÷°å²âÊÔ");
			exit(0);
		 }
		
	}
	 //Main_AcceptPCCmd();
	
	 Main_Log("***×Ô²âÊÔ³ÌĞòÒì³£ÍË³ö");
	return -1;
}








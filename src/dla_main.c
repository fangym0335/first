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
#define DEFAULT_WORK_DIR_4DLA  "/home/App/ifsf/"  //ǰͥ��������Ĭ�Ϲ���Ŀ¼,����GetWorkDir()����.
#define RUN_WORK_DIR_FILE           "/var/run/ifsf_main.wkd"  //Ӧ�ó�������Ŀ¼�ļ�,����GetWorkDir()����.
#define DLA_DSP_CFG_FILE                     "/home/App/ifsf/etc/dsp.cfg" 
#define MAX_PATH_NAME_LENGTH 1024
#define DLA_FIFO "/tmp/css_fifo.data"
//#define  DLA_SERVER_TCP_PORT_4LOGIN 9001
//#define  DLA_SERVER_TCP_PORT_4DATA  9002
//#define  DLA_SERVER_UDP_PORT_4REALTIME  9010
//#define  DLA_CSS_TCP_PORT_4REALTIME  9006

//��ǹ���ýṹ��,��Ӧ��ǹ�����ļ�dsp.cfg  modify by liys
typedef struct {
	unsigned char node;             //�ڵ��
	unsigned char chnLog;          //�߼�ͨ����
	unsigned char nodeDriver;      //������
	unsigned char phyfp;           //��������, 1+.
	unsigned char logfp;           //�߼�����, 1+.
	unsigned char physical_gun_no; //������ǹ��
	unsigned char logical_gun_no;  //�߼���ǹ��
	unsigned char oil_no[4];       //��Ʒ��
	unsigned char isused;          //�Ƿ�����
} DLA_GUN_CONFIG;
//��������ǹ�ż����ϵ 
typedef struct {
	unsigned char node;             //IFSF�ڵ��
	unsigned char fp;           //IFSF����, 1+.
	//unsigned char  physical_gun_no; //�ڵ�����ǹ��
	unsigned char  logical_gun_no;//���ǹ��
	unsigned char  big_gun_no; //��������ǹ��
	unsigned char  moved_gun_no; //����ǹ��IFSF��ǹ��
} DLA_BIG_GUN;
typedef struct{ //import!!
	/*Ϊ�˷����ȡ�������:*/
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
	/*UNSOLICITED DATA:��MakeTrMsg���������!!*/
	//unsigned char TR_Buff_States_Message[17]; //see IFSF 3-01,128
	/*MANUFACTURER/OIL COMPANY SPECIFIC*/
	unsigned char TR_Date[4]; // user defined, Data_Id 202, offline Transaction Date
	unsigned char TR_Time[3]; // user defined, Data_Id 203, offline Transaction Time
	unsigned char TR_Start_Total[7]; // user defined, Data_Id 204, Start Log_Noz_Vol_Total
	unsigned char TR_End_Total[7];  // user defined, Data_Id 205, End Log_Noz_Vol_Total
} DLA_FP_TR_DATA; //42 byte;
 typedef struct {
	unsigned char _SERVER_IP[16]; /*������IP��ַ*/
	int _SERVER_TCP_PORT;
	int _SERVER_UDP_PORT;
	int _REALTIME_FLAG; //ʵʱ�����ϴ���־,0���ϴ�,1�ϴ�
	 int _SOCKTCP,_SOCKUDP;//_SOCKTCP���ڽ��������ϴ�,_SOCKUDP����ʵʱ�����ϴ�
	unsigned char *sendBuffer;//,buff[16],*pBuff	
	int msgLength ;//, cfgLength unsigned long
	int fd_fifo ; //FIFO���ļ�������
	unsigned char  totalGun;//��ǹ��
	DLA_BIG_GUN *bigGun;//���256��ǹ
	//unsigned char _STATION_NO[11]; /*վ���������ַ���10λ����(ʡ2��2վ6)*/
	//unsigned char _STATION_NO_BCD[5]; /*BCD��,վ����10λ����(ʡ2��2վ6)*/
	// int _TLG_SEND_INTERVAL; /*Һλ���ϴ�ʱ����*/
	 
	 //struct sockaddr _SERVER_UDP_ADDR;//struct sockaddr_in _SERVER_UDP_ADDR;
	
	//unsigned char *readBuffer;//,buff[16],*pBuff	
	
} DLA_PUBLIC;


DLA_PUBLIC gDla;


GunShm *gpDlaGunShm;
IFSF_SHM *gpDlaIfsf_Shm;

//static char _SERVER_IP[16]; /*������IP��ַ*/
//static char _STATION_NO[11]; /*վ����10λ����(ʡ2��2վ6)*/
//static char _STATION_NO_BCD[5]; /*վ����10λ����(ʡ2��2վ6)*/
//static int _TLG_SEND_INTERVAL; /*Һλ���ϴ�ʱ����*/
//static int _REALTIME_FLAG; //ʵʱ�����ϴ���־,0���ϴ�,1�ϴ�
//static int _SOCKTCP,_SOCKUDP;//_SOCKTCP���ڽ��������ϴ�,_SOCKUDP����ʵʱ�����ϴ�
//static struct sockaddr_in _SERVER_UDP_ADDR;



//--------------------------------------------------------------------------------
//��־,����ǰ�����ʼ����־����_SetLogLevel����־�ļ����_SetLogFd
//------------------------------------------------------------------------------
#define DLA_DEFAULT_LOG_LEVEL 1                           //Ĭ����־����
#define DLA_MAX_VARIABLE_PARA_LEN 1024 
#define DLA_MAX_FILE_NAME_LEN 1024 
static char gsDlaFileName[DLA_MAX_FILE_NAME_LEN + 1];	/*��¼������Ϣ���ļ���*/
static int giDlaLine;								/*��¼������Ϣ����*/
 //�Զ�����־����,0:ʲôҲ����;1:ֻ��ӡ,����¼;2:��ӡ�Ҽ�¼;
static int giDlaLogLevel;                                             //��־���Լ���
static int giDlaFd;                                                       //��־�ļ����


//�Զ���ּ��ļ���־,��__LevelLog.
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
		RunLog( " {��־�ļ�����������[%d] }\n", fd );
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
	char fileName[DLA_MAX_FILE_NAME_LEN + 1];	/*��¼������Ϣ���ļ���*/
	int line;								/*��¼������Ϣ����*/
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
		if(giDlaFd <=0 )	/*��־�ļ�δ��?*/
		{
			RunLog( " {��־�ļ�����������[%d] }\n", giDlaFd );
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
	char fileName[DLA_MAX_FILE_NAME_LEN + 1];	/*��¼������Ϣ���ļ���*/
	int line;								/*��¼������Ϣ����*/
	
	Dla_GetFileAndLine(fileName, &line);
	sprintf(sMessage, "%s:%d	", fileName, line);
	sprintf(sMessage+strlen(sMessage), "pid = [%d]", getpid());

	va_start(argptr, fmt);
	vsprintf(sMessage+strlen(sMessage),fmt, argptr);
	va_end( argptr );

	printf( " {%s }\n", sMessage );
	if(giDlaFd <=0 )	/*��־�ļ�δ��?*/
	{
		RunLog( " {��־�ļ�����������[%d] }\n", giDlaFd );
		return;
	}
	
	write( giDlaFd, sMessage, strlen(sMessage) );
	write( giDlaFd, "\n", strlen("\n") );
	
	return;
}
int Dla_InitLog(int loglevel){
	int fd;
    	 if( ( fd = open( DLA_LOG_FILE, O_WRONLY|O_CREAT|O_TRUNC, 00644 ) ) <= 0 ) {
		RunLog( "����־�ļ�[%s]ʧ��\n", DLA_LOG_FILE );
		return -1;
	}
	giDlaFd = fd;
	Dla_SetLogLevel(loglevel);
	return 1;
}
//------------------------------------------------

//-----------------------------------------------------------------------
//���ú���
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
//BCD�����ݵĳ��Ȳ��ó���5���ֽڣ������ת�����ɹ�
//ֻ����Bin8��ռ�ֽ���,������С��λ��,ȫ��������������
int Dla_Bin8Bcd2Hex( unsigned char *bin8bcd, int lenb, unsigned char *hex, int lenh )
{
	unsigned char a[16];
	const unsigned char max_bcd[] = { 0x42, 0x94, 0x96, 0x72, 0x95, 0x00 };
	unsigned long b;
	int ret,k,i,first_flag,x;//

	//printf("ת������:");
	//for(i=0;i<lenb;i++)	printf("%02x",bin8bcd[i]);
	//printf("\n");

	/*����unsigned long �Ľ���*/
	k=lenb-6;
	i=0;
	while(k-i>0){
		if(0 !=bin8bcd[i+1]){
			//printf("ת������:0 !=bin8bcd[%d]\n",i+1);
			return -3;
		}
		i++;
	}
	if( lenb >= 6 && memcmp( bin8bcd+1+k, max_bcd,  5) > 0 )
	{
		//("ת������: lenb >= 6 && memcmp( bin8bcd+1+k, max_bcd,  5) > 0 \n");
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
			//printf("ת������: ����BCD���� 1\n");
			return -1;
		}
		x= bin8bcd[i+1]&0x0f;
		if( x >= 0 && x <=9 )
			a[2*i+1-k]= x+'0' ;
		else{
			//printf("ת������: ����BCD���� 2\n");
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

	/*����unsigned long �Ľ���*/
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
//��ȡ����IP,���ر���IP��ַ������-1ʧ�ܣ����DHCP����
static unsigned  long  Dla_GetLocalIP(){//�μ�tcp.h��tcp.c�ļ�����,����ѡ��һ��.
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
 * func: ��ȡ�㲥��ַ
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
		RunLog( "�����:%d", errno );
		return -1;
	}

	if( listen( sockFd, backLog ) < 0 )
	{
		RunLog( "�����:%d",errno );
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

	RunLog( "�����:%d",errno );
	return -1;
}
/*��nTimeOut����ΪTIME_INFINITE�������õ�ʱ���ϵͳȱʡʱ�䳤ʱ,
 connect����ɷ���ǰ��ֻ�ȴ�ϵͳȱʡʱ��*/
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
		OldAction = SetAlrm( nTimeOut, NULL );  /*�����alarm���ú�,connect����ǰ��ʱ,��connect�ȴ�ʱ��Ϊȱʡʱ��*/
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
	 0:	�ɹ�
	-1:	ʧ��
	-2:	��ʱ
*/
static ssize_t Dla_TcpSendDataInTime( int fd, const char *s, size_t len,  int timeout )
{
#if 0
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
	
	Dla_LevelLog("before WirteLengthInTime -------------");
	n = WriteByLengthInTime( fd, t, sendlen, timeout );
	Dla_LevelLog("after WirteLengthInTime -------------");
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
	
	//Dla_LevelLog("before WirteLengthInTime -------------");	
	n = WriteByLengthInTime( fd, t, sendlen, timeout );
	//Dla_LevelLog("after WirteLengthInTime -----------length=[%d]",n);
	
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
		RunLog("�յ��ĳ���[%d]С��Ԥ���յ��ĳ���[%d],ֵ[%s]\n", n,len,Dla_Asc2Hex(t, len));
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
//ȥ���ַ�������ո񼰻س����з�,��Ҫ�ַ�����������
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
//��ʼ�����ò���
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
	//���ļ�
	fd = open( fileName, O_WRONLY|O_CREAT|O_TRUNC, 00644 );
	if( fd < 0 )
	{
		//UserLog( "�򿪵�ǰ���й���·����Ϣ�ļ�[%s]ʧ��.",fileName );
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
			RunLog("�������ݳ���,  ����Ԥ�ڵ���������,������[%s],ֵ��[%s]",dla_ini[i].name,dla_ini[i].value);
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
//�����ڴ洦��
//------------------------------------------------
static char Dla_gKeyFile[MAX_PATH_NAME_LENGTH]="";

static int Dla_InitTheKeyFile() 
{
	char	*pFilePath, FilePath[]=DEFAULT_WORK_DIR_4DLA;
	char temp[128];
	int ret;
	
	//ȡkeyfile�ļ�·��//	
	pFilePath = getenv( "WORK_DIR" );	//WORK_DIR ����Ŀ¼�Ļ�������
	if( NULL ==  pFilePath){
		bzero(temp, sizeof(temp));
		ret = Dla_GetWorkDir(temp);
		if(ret >= 0 ){
			pFilePath = temp;
		}
		else{
			pFilePath = FilePath;
		}
		//cgi_error( "δ���廷������WORK_DIR" );
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
	#define DLA_MAX_GUN_CNT 64 //һ��վ�����ǹ��
	
	//unsigned char onlineFlag[2] ={0xaa,0x55};
	static unsigned char sendBuffer[512] ;//,buff[16],*pBuff
	unsigned char readBuffer[512] ;//,buff[16],*pBuff
	unsigned char  dla_node, dla_fp,moved_gun;
	int  ret, i, j, dataLen,readLength,len=sizeof(DLA_BIG_GUN);//, onlineFlagLen   = 2
	extern int errno;
	DLA_FP_TR_DATA  *pFp_tr_data;
	static unsigned char  gun_flag[DLA_MAX_GUN_CNT+1];//��ǹ��־
	
	/*if(  (NULL == ppIfsf_Shm) || (NULL == ppGunShm) ){
		_FileLog("_ReadTransData�����ڴ���˳�.");
		return -1;
	}*/	
	//*sendType = 0;
	if(gDla.fd_fifo<=0){
		gDla.fd_fifo = open(DLA_FIFO, O_RDONLY | O_NONBLOCK);
		if(gDla.fd_fifo <= 0){
			RunLog("FIFOֻ���򿪳���.");
			close(gDla.fd_fifo);
			return -1;
		}
		else{
			//RunLog( "gDla.fd_fifo�������");
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
				//RunLog( "�ն�gDla.fd_fifo[%d]��,û������",READ_TIMES );
				//if( (ret !=4 )|| (dataLen <=0) )
				//	_LevelLog("FIFO��ȡ�������ݵĳ���ֵ����.ret[%d],dataLen[%d]",ret,dataLen);
				if(readLength != dataLen)
					RunLog("FIFO��ȡ�������ݵĳ��ȳ���.readLength[%d],dataLen[%d]",readLength,dataLen);
				i = 0;
			}
			//_LevelLog( "��gDla.fd_fifo��ʼ");
			dataLen = -1;
			ret = read(gDla.fd_fifo, (char *)&dataLen, 4);//dataLen = 
			if( (ret !=4 )|| (dataLen <=0) ){
				dataLen = -1;
				//RunLog( "��gDla.fd_fifo����,û������,����[%d]",dataLen );
				continue;
			}
			if(dataLen>=512){
				RunLog("FIFO��ȡ�������ݵĳ��ȳ���.readLength[%d],dataLen[%d]",readLength,dataLen);
				continue;
			}
			for(j=0;j< READ_DATA_TIMES;j++){
				readLength = -1;
				readLength = read(gDla.fd_fifo, readBuffer, dataLen);
				RunLog( "��gDla.fd_fifo����[%s]",Dla_Asc2Hex(readBuffer,32) );
				if(readLength != dataLen){
					RunLog( "��gDla.fd_fifo����,readLength[%d], dataLen[%d],errno[%d]",readLength ,dataLen,errno);
					readLength = -1;
					continue;
				}else{
					//_LevelLog("FIFO��ȡ��������.readLength[%d],dataLen[%d]",readLength,dataLen);
					break;
				}
			}
			if((readLength == dataLen)&&(readLength != -1)){
				if(readLength<=512){
					//_LevelLog( "��gDla.fd_fifo����,��ʼ��������");
					//memcpy(gDla.readBuffer,readBuffer,readLength);
					//RunLog( "��gDla.fd_fifo��������[%s]",Dla_Asc2Hex(readBuffer,readLength) );
				}else{
					RunLog( "��gDla.fd_fifoʧ��,���ȴ���512[%s]",Dla_Asc2Hex(readBuffer,128) );
					}
				
				break;
			}
		}	
		
		
		//��������		
		ret = -1;
		//Dla_LevelLog("FIFO����������: ��ǹ��[%d]",gDla.totalGun);
		if( (readBuffer[0] == 0xff )  &&(readLength > 40 ) ){//Transaction data
			//RunLog("FIFO����������:");
			
			pFp_tr_data =(DLA_FP_TR_DATA  *)&readBuffer[1];
			dla_node = pFp_tr_data->node;
			dla_fp = pFp_tr_data->fp_ad-0x20;
			moved_gun = pFp_tr_data->TR_Log_Noz;
			
			sendBuffer[0] = (unsigned char) 0x08;//�������ݱ�־
			sendBuffer[1] = 0;//���ݳ���
			RunLog("FIFO ��������:�ڵ��[%d]����[%d]",dla_node,dla_fp);
			for(j=0;j<gDla.totalGun;j++){
				//Dla_LevelLog("gDla.bigGun[%d]�ڵ��[%d]����[%d]",j,gDla.bigGun[j].node,gDla.bigGun[j].fp);
				if( (gDla.bigGun[j].node==dla_node) &&(gDla.bigGun[j].fp==dla_fp) ){
					if(gDla.bigGun[j].moved_gun_no==moved_gun){
						sendBuffer[2] = gDla.bigGun[j].big_gun_no;
						gun_flag[gDla.bigGun[j].big_gun_no] = 0;//��ǹ��
						break;
					}else{
						RunLog("FIFO����������:�ڵ��[%d]����[%d]�� ��ǹ��[%d]���������ö�ǹ��[%d],", \
							dla_node,dla_fp, moved_gun, gDla.bigGun[j].moved_gun_no);
					}
				}
			}
			if( (j == gDla.totalGun)||(0 == sendBuffer[2]) ){
				//Dla_LevelLog("FIFO���������ݳ���:0 == sendBuffer[2],��ֵ������ǹ��1");
				//sendBuffer[2] = 1; //gDla.bigGun[0].big_gun_no
				RunLog("FIFO���������ݳ���:������ǹ���Ҳ�������0,�쳣����");
				return -1;
			}
			sendBuffer[3] = 0;
			sendBuffer[4] = 0;
			memcpy((char *)&(sendBuffer[5]), pFp_tr_data->TR_Seq_Nb,2);//BCD��HEXת����,�˴����ã���ת��
			
			ret = Dla_Bin8Bcd2Hex(pFp_tr_data->TR_Unit_Price, 4, &sendBuffer[7], 2);
			ret = Dla_Bin8Bcd2Hex(pFp_tr_data->TR_Volume, 5, &sendBuffer[9], 3);
			ret = Dla_Bin8Bcd2Hex(pFp_tr_data->TR_Amount, 5, &sendBuffer[12], 4);

			#if OLD_PROTOCOL	//�ɵ�Э��
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
			//Dla_LevelLog("���콻���������");
			ret = 1;
			break;
		}
		else if( (readBuffer[0] == 0xf0 )  &&(readLength < 9 ) &&(readLength > 3 ) ){//0����
			bzero(sendBuffer,sizeof(sendBuffer));
			sendBuffer[0] =0x02;
			sendBuffer[1] =0;
			dla_node =readBuffer[1];
			dla_fp = readBuffer[2]-0x20;
			moved_gun = readBuffer[3]; //��ǹ��,�˴�ֵ0,����
			//RunLog("FIFO����������:0  ��������");
			RunLog("FIFO  �� ��������:�ڵ��[%d]����[%d],��ǹ��[%d]",dla_node,dla_fp,moved_gun);
			for(j=0;j<gDla.totalGun;j++){
				//Dla_LevelLog("gDla.bigGun[%d]�ڵ��[%d]����[%d]",j,gDla.bigGun[j].node,gDla.bigGun[j].fp);
				if( (gDla.bigGun[j].node==dla_node) &&(gDla.bigGun[j].fp==dla_fp) ){
					if(gun_flag[gDla.bigGun[j].big_gun_no] == 1){
						sendBuffer[2] = gDla.bigGun[j].big_gun_no;
						gun_flag[gDla.bigGun[j].big_gun_no] = 0;//��ǹ��
						break;
					}else{
						Dla_LevelLog("FIFO����������:�ڵ��[%d]����[%d]�� ��ǹ��[%d]û��̧ǹ[%d], û��0����,", \
							dla_node,dla_fp,  gDla.bigGun[j].big_gun_no, gun_flag[gDla.bigGun[j].big_gun_no]);
					}
				}
			}
			if( (j == gDla.totalGun)||(0 == sendBuffer[2]) ){
				//Dla_LevelLog("FIFO���������ݳ���:0 == sendBuffer[2],��ֵ������ǹ��1");
				//sendBuffer[2] = 1;
				RunLog("FIFO���������ݳ���:������ǹ���Ҳ�������0,�쳣����");
				return -1;
			}
			#if OLD_PROTOCOL //�ɵ�Э��	
			gDla.msgLength = 14;
			#else		
			gDla.msgLength = 15;
			#endif
			
			//RunLog("FIFO����������:����0���׹�ǹ�������");
			ret = 2;
			break;
		}
		else if((readBuffer[2] != 1 ) ||(readBuffer[5] != 0x80 )  ){//dispenser, unsolicited data..
			RunLog("FIFO����������:  ����������");
			ret = -1;
			continue;
		}else if( (readBuffer[7] == 0x0e ) &&(readBuffer[10] == 100 )&&(readBuffer[14] == 4 )  ){//Realtime Data:state
		//readBuffer[7] == 0x0e������;readBuffer[10] == 100��������;readBuffer[14] == 4 ��ǹ
				//RunLog("FIFO����������:��ǹ״̬����");
				//ret = MakeStateData(resultBuff);//gDla.readBuffer
				dla_node =readBuffer[3];
				dla_fp = readBuffer[9]-0x20;
				moved_gun = readBuffer[17]; //��ǹ��
				
				gDla.msgLength =3 ;
				sendBuffer[0] = (unsigned char) 0x01;
				sendBuffer[1] =0;
				//RunLog("FIFO��ǹ״̬:�ڵ��[%d]����[%d]",dla_node,dla_fp);
				for(j=0;j<gDla.totalGun;j++){
					if( (gDla.bigGun[j].node==dla_node) &&(gDla.bigGun[j].fp==dla_fp) ){
						if(gDla.bigGun[j].moved_gun_no==moved_gun){
							sendBuffer[2] = gDla.bigGun[j].big_gun_no;
							gun_flag[gDla.bigGun[j].big_gun_no] = 1;//��ǹ��
							RunLog("FIFO��ǹ״̬:�ڵ��[%d]����[%d]�Ĵ�ǹ��[%d]",dla_node,dla_fp,gDla.bigGun[j].big_gun_no);
							break;
						}else{
							RunLog("FIFO��ǹ״̬:�ڵ��[%d]����[%d]�� ��ǹ��[%d]���������ö�ǹ��[%d],", \
								dla_node,dla_fp, moved_gun, gDla.bigGun[j].moved_gun_no);
						}
					}
				}
				if( (j == gDla.totalGun)||(0 == sendBuffer[2]) ){
					//Dla_LevelLog("FIFO���������ݳ���:0 == sendBuffer[2],��ֵ������ǹ��1");
					//sendBuffer[2] = 1;
					RunLog("FIFO���������ݳ���:������ǹ���Ҳ�������0,�쳣����");
					return -1;
				}
				//Dla_LevelLog("������ǹ״̬�������");
				ret = 2;
				break;
		}else if( (readBuffer[0] == 2 )&&(readBuffer[2] == 1 )&&(readBuffer[10] == 102 ) ){//Realtime Data:transaction
		//readBuffer[0] == 2����CD;readBuffer[2] == 1���ͻ�;readBuffer[10] == 102 ʵʱ����	
				//RunLog("FIFO����������: ʵʱ��������");
				dla_node =readBuffer[3];
				dla_fp = readBuffer[9]-0x20;
				moved_gun = readBuffer[28];
				
				gDla.msgLength =6 ;
				sendBuffer[0] = (unsigned char) 0x0a;
				sendBuffer[1] = (unsigned char) 0x00;//���׽���־
				//RunLog("FIFO������ ʵʱ��������:�����ڵ��[%d]����[%d],������[%d]",dla_node,dla_fp,gDla.totalGun);
				for(j=0;j<gDla.totalGun;j++){
					if( (gDla.bigGun[j].node==dla_node) &&(gDla.bigGun[j].fp==dla_fp) ){
						if(gDla.bigGun[j].moved_gun_no==moved_gun){
							sendBuffer[2] = gDla.bigGun[j].big_gun_no;
							gun_flag[gDla.bigGun[j].big_gun_no] = 1;//��ǹ��
							break;
						}else{
							RunLog("FIFO����������:�ڵ��[%d]����[%d]�� ��ǹ��[%d]���������ö�ǹ��[%d],", \
								dla_node,dla_fp, moved_gun, gDla.bigGun[j].moved_gun_no);
						}
					}
				}
				if( (j == gDla.totalGun)||(0 == sendBuffer[2]) ){
					//sendBuffer[2] = 1;
					RunLog("FIFO���������ݳ���:������ǹ���Ҳ�������0,�쳣����");
					return -1;
				}
				ret = Dla_Bin8Bcd2Hex(&readBuffer[14], 5, &sendBuffer[3], 3);//���
				//memcpy(&sendBuffer[3], &readBuffer[16], 3);	
				
				//�������
				ret = 2;
				break;
			
		}else{
			RunLog("FIFO����������:  ��������");
			ret = 0;
			continue;
		}
	}
	//������ϣ���������
	//resultBuff =sendBuffer; 
	memcpy(resultBuff, sendBuffer, gDla.msgLength);	//
	//memcpy(gDla.sendBuffer,sendBuffer,gDla.msgLength);
	//RunLog( "FIFO�������������[%s]",Dla_Asc2Hex(resultBuff, gDla.msgLength) );
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
	RunLog("���ͽ�������[%s]\n" , Dla_Asc2Hex(sendBuffer, sendLen) );
	for(i=0;i<tryN;i++){		
		ret = Dla_TcpSendDataInTime(gDla._SOCKTCP, sendBuffer,sendLen, timeout);		
		if(ret <0){//0��ʾ���ͳɹ�
			RunLog("Dla_SendTransData�������� [%s]ʧ��,����[%d]",Dla_Asc2Hex(sendBuffer, sendLen),ret);
			if(gDla._SOCKTCP <= 0){
				sIPName = gDla._SERVER_IP;//inet_ntoa(  *((struct in_addr *)(&gpIfsf_shm->recv_hb[i].host_ip))  );
				gDla._SOCKTCP = Dla_TcpConnect(sIPName, gDla._SERVER_TCP_PORT, timeout);//sPortName(timeout+1)/2
				if (gDla._SOCKTCP <= 0 ){
					RunLog("TcpConnect���Ӳ��ɹ�,DLA�������� ʧ��");
					continue;
				}
			}
		}else{
			//RunLog("Dla_SendTransData�������� [%s]�ɹ�,����[%d]",Dla_Asc2Hex(sendBuffer,sendLen),ret);
			memset(recvBuffer,0xff,sizeof(recvBuffer));
			for(i=0;i<tryN;i++){
				recvLength = 6;
				ret = Dla_TcpRecvDataInTime(gDla._SOCKTCP, recvBuffer, &recvLength, timeout);
				RunLog("Dla_SendTransData�������ݺ����ȷ�� ,����[%d],recvLength[%d], recvBufferǰ8�ֽ�:[%s]", \
					ret, recvLength,  Dla_Asc2Hex(recvBuffer, 8) );
				if(ret ==-2){
					if( (0x30 ==recvBuffer[0])&&(0x30 ==recvBuffer[1])&&(0x30 ==recvBuffer[2]) \
					  &&(0x32 ==recvBuffer[3])&&(0x08==recvBuffer[4]) ){ 
					  	if(0x10 ==recvBuffer[5]){
							return 0;
						}
						else{
							RunLog("Dla_SendTransData���յ���10���ݣ����ɹ�");							
							continue;
						}
					}
					break;
				}else if(ret < 0){				
					RunLog("Dla_SendTransData�������ݲ��ɹ�");
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
							RunLog("Dla_SendTransData���յ���10���ݣ����ɹ�");							
							continue;
						}
					
			}else if(ret < 0){
				RunLog("����, �ӷ�����û�н��յ�����");
				sleep(1);//���ⷢ�͹���Ƶ��
				continue;//�ط�����
				//return -1;			
			}else{
				RunLog("�ӷ��������յķ������ݴ���");
				return -1;
			}
			
		}
	}
	if( (ret <= 0) ||  (0x30 !=recvBuffer[0])||(0x30 !=recvBuffer[1])||(0x30 !=recvBuffer[2])  ){
			RunLog("DLA�շ��������� ʧ��");
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
				RunLog("UDP���ӽ������ɹ�,_setRealTime ʧ��");
				//close(_SOCKUDP); 
				gDla._SOCKUDP = -1;	
				//exit(0);
			}
			else{
				 for(i=0;i<tryNum;i++){
					ret = sendto(gDla._SOCKUDP, sendBuffer, gDla.msgLength, MSG_DONTWAIT, (struct sockaddr *)&_SERVER_UDP_ADDR, sizeof(struct sockaddr));//
					if (ret < 0) {				
						RunLog("ʵʱ���ݷ���ʧ��,error:[%s]", strerror(errno));	
					}else{
						/*Dla_LevelLog("ʵʱ���ݷ��ͳɹ�,��ʼ����ȷ��");
						recvLength = 0;//30
						bzero(recvBuffer,sizeof(recvBuffer));
						for(i=0;i<=tryNum;i++){
							//Dla_LevelLog("����ʵʱ���ݷ��ͷ���ֵ");
							ret = recvfrom(gDla._SOCKUDP, recvBuffer,sizeof(recvBuffer), 0,  \
								(struct sockaddr *)&_SERVER_UDP_ADDR,  &recvLength);
							if((ret <=0)||(recvLength<=0)){				
								Dla_LevelLog("Dla_SendRealtimeData�������ݲ��ɹ�");
								continue;		
							}
							if((recvBuffer[0] != 0x01)||(recvBuffer[0] != 0x02)||(recvBuffer[0] != 0x0a)){
							   	Dla_LevelLog("���������յ����ݴ���");
								continue;
							}else{
								Dla_LevelLog("ʵʱ���ݷ��ͳɹ������ȷ��");
								break;
							}
						}*/
					       RunLog("ʵʱ���ݷ��ͳɹ�");
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
				RunLog("ʵʱ���ݷ���ʧ��,error:[%s]", strerror(errno));	
			}else{
				/*Dla_LevelLog("ʵʱ���ݷ��ͳɹ�,��ʼ����ȷ��");
				recvLength = 0;//30
				bzero(recvBuffer,sizeof(recvBuffer));
				for(i=0;i<=tryNum;i++){
					//Dla_LevelLog("����ʵʱ���ݷ��ͷ���ֵ");
					ret = recvfrom(gDla._SOCKUDP, recvBuffer,sizeof(recvBuffer), MSG_DONTWAIT,  \
						(struct sockaddr *)&_SERVER_UDP_ADDR,  &recvLength);
					if((ret <=0)||(recvLength<=0)){				
						Dla_LevelLog("_TcpRecvDataInTime�������ݲ��ɹ�");
						continue;		
					}
					if((recvBuffer[0] != 0x01)||(recvBuffer[0] != 0x02)||(recvBuffer[0] != 0x0a)){
					   	Dla_LevelLog("���������յ����ݴ���");
						continue;
					}else{
						Dla_LevelLog("ʵʱ���ݷ��ͳɹ������ȷ��");
						break;
					}
				}*/
				RunLog("ʵʱ���ݷ��ͳɹ�");
				break;
			}
		}
		
		
	}	
 }

int main( int argc, char* argv[] )
{
	#define GUN_MAX_NUM 256 //���256��ǹ
	int i, j, n,k,len,ret,fd;//,pid,isChild
	unsigned char sPortName[5],*sIPName;	
 	int    tmp, timeout =5, tryn=10;
	//struct sockaddr_in  _SERVER_UDP_ADDR;
	unsigned char *resultBuff;
	unsigned char sendBuff[512] ;//,buff[16],*pBuff
	unsigned char realtimeBuff[32] ;//,buff[16],*pBuff
	//static unsigned char readBuff[512] ;//,buff[16],*pBuff
	DLA_BIG_GUN bigGun[GUN_MAX_NUM];	
	DLA_BIG_GUN tempGun;//����ͼ��㶯ǹ����
	unsigned char buff[16];
	DLA_GUN_CONFIG *pGunConfig;

	if (argc == 2) {
	       if (strcmp(argv[1], "-v") == 0) {
			printf( "Ifsf_main. PCD version 1.01\n");
	       } 
		return 0;
	}
	//��ʼ����־
	ret = InitLog();
	if(ret < 0){
		exit(0);  
	}

	
       RunLog("��ʼ���ж������......\n");
	
	sleep(30);//�ȴ�IFSF��������,����30��
	bzero(sendBuff, sizeof(sendBuff));
	bzero( (unsigned char *)&bigGun, sizeof(DLA_BIG_GUN)*GUN_MAX_NUM);
	gDla.sendBuffer = sendBuff;	
	//printf("��ʼ��bigGunΪ0.");
	//bzero((unsigned char *)bigGun, GUN_MAX_NUM*sizeof(DLA_BIG_GUN));
	gDla.bigGun = bigGun;


	
	//��ʼ��������ǹ��   //���ͻ��ڵ�����ʹ�����ǹ�Ź�ϵ����
	RunLog(" ��ʼ��������ǹ��.");
	fd = open( DLA_DSP_CFG_FILE, O_RDONLY );
	if(fd <=0){
		RunLog("  ��FCC���ͻ������ļ�[%s]����.");
		exit(0);
	}
	k = 0; // gun number
	len = sizeof(DLA_BIG_GUN);
	while (1) {
		n = read( fd, buff, sizeof(DLA_GUN_CONFIG) );
		if( n == 0 ) {	//�������ļ�β
			close(fd);
			break;
		} else if( n < 0 ) {
			RunLog( "��ȡ dsp.cfg �����ļ�����" );
			close(fd);
			return -1;
		}
		//RunLog("��[%d]�ζ�ȡ��dsp.cfg ����[%d]���ַ�\n",i+1, n);
		if( n != sizeof(DLA_GUN_CONFIG) ) {
			RunLog( "dsp.cfg ������Ϣ����" );
			close( fd );
			return -1;
		}
		pGunConfig = (DLA_GUN_CONFIG *) (&buff[0]);
		bigGun[k].node= pGunConfig->node;
		bigGun[k].fp = pGunConfig->logfp;
		//bigGun[k].physical_gun_no = pGunConfig->physical_gun_no;
		bigGun[k].logical_gun_no = pGunConfig->logical_gun_no;		
		RunLog( "��¼��[%d],�ڵ��[%d], ����[%d],�������ǹ��[%d]" ,k, \
				bigGun[k].node, bigGun[k].fp, bigGun[k].logical_gun_no );
		//gunBuffer[k*len+0] = pGunConfig->node;
		//gunBuffer[k*len+1] = pGunConfig->phyfp;
		//gunBuffer[k*len+2] = pGunConfig->physical_gun_no;
		//close(fd);
		//bigGun[k].big_gun_no = pGunConfig->logical_gun_no;	
		//if(bigGun[k].big_gun_no <= 0){
		//	Dla_FileLog( "����,��[%d]����¼��phygun[%d],������ǹ��lgun[%d]" ,k, \
		//		pGunConfig->physical_gun_no, pGunConfig->logical_gun_no);
		//	bigGun[k].big_gun_no = 1;
		//}
		k++;// gun number ++
	}
	gDla.totalGun = k;	
	RunLog( "������ǹ[%d]��" ,gDla.totalGun);
	
	//���սڵ��,����,���ǹ������
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
	
	
	//��ֵ������ǹ��
	for(i=0;i<gDla.totalGun;i++){
		bigGun[i].big_gun_no = i+1;
	}
	
	//����IFSF��ǹ��,�ٶ�ÿ��������8��ǹ
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
			default:Dla_LevelLog("��ͬ�ڵ�����ŵ�ǹ������8, �Ѿ���[%d]",bigGun[i].logical_gun_no);
				break;			
		}
		RunLog("��[%d]��ǹ�Ľڵ��[%d], ����[%d],IFSF��ǹ��[%x]",bigGun[i].big_gun_no, \
			bigGun[i].node, bigGun[i].fp, bigGun[i].moved_gun_no );		
	}
		
	RunLog("DLA_MAIN ��ʼ�������ڴ�...");
	//��ʼ�������ڴ�
	ret = Dla_InitTheKeyFile();
	if( ret < 0 )
	{
		RunLog( "��ʼ��Key Fileʧ��\n" );
		exit(1);
	}
	//#endif 
	
	RunLog("��ʼ�����ò���...");
	//��ʼ�����ò���
	ret = Dla_InitConfig();
	if(ret <0){
		RunLog("���ò�����ʼ������.");
		exit(0);
	}
	RunLog("���ӹ����ڴ�...");
	
	gpDlaGunShm = (GunShm*)Dla_AttachTheShm( GUN_SHM_INDEX );
	gpDlaIfsf_Shm = (IFSF_SHM*)Dla_AttachTheShm( IFSF_SHM_INDEX );
	if( (gpDlaGunShm == NULL)||(gpDlaIfsf_Shm == NULL) ){
		RunLog( "���ӹ����ڴ�ʧ��" );
		exit(1);
	}
	
	
	
	//ѭ���������ͽ������ݺ�ʵʱ����(���Ҫ�Ļ�)
	//�ȳ�ʼ��UDP���Ӻʹ�FIFO
	//��FIFO
	gDla.fd_fifo = -1;
	gDla.fd_fifo = open(DLA_FIFO, O_RDONLY | O_NONBLOCK);
	if(gDla.fd_fifo <= 0){
		RunLog("ֻ����FIFO�򿪳���.");
		close(gDla.fd_fifo);
		exit(0);
	}
	//��ʼ��UDP����
	gDla._SOCKUDP = -1;
	//bzero(&_SERVER_UDP_ADDR,sizeof(_SERVER_UDP_ADDR));
	//_SERVER_UDP_ADDR.sin_family=AF_INET;
	//_SERVER_UDP_ADDR.sin_addr.s_addr=inet_addr(gDla._SERVER_IP);
	//_SERVER_UDP_ADDR.sin_port = htons(gDla._SERVER_UDP_PORT);	
	gDla._SOCKUDP = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
	if (  gDla._SOCKUDP<=0 ){//connect(_SOCKUDP, (struct sockaddr *)_SERVER_UDP_ADDR, sizeof(_SERVER_UDP_ADDR))
		RunLog("UDP���ӽ������ɹ�,��ʼ��UDP����ʧ��");
		//close(_SOCKUDP); 
		gDla._SOCKUDP = -1;	
		exit(0);
	}
	gDla._SOCKTCP = -1;
	
	//ѭ����������
	gDla._REALTIME_FLAG ==1;  // 0
	gpDlaIfsf_Shm->css_data_flag = 2; // 1
	bzero(realtimeBuff, sizeof(realtimeBuff));
	bzero(sendBuff, sizeof(sendBuff));
	gDla.msgLength = 0;
	RunLog("��ʼѭ������ܵ�����.");
	while(1){
		//RunLog("ѭ����ȡFIFO����:��ǹ��[%d]",gDla.totalGun);
		ret = Dla_ReadTransData(sendBuff );//resultBuff
		RunLog( "ѭ����ȡFIFO��������:[%s]",Dla_Asc2Hex(sendBuff,gDla.msgLength) );
		if( ret  == 1){//��������
			//��֯�����͹�ǹ����
			realtimeBuff[0] =0x02;
			realtimeBuff[1] =0;
			
			
			realtimeBuff[2] = sendBuff[2];//ǹ��
			#if OLD_PROTOCOL	//�ɵ�Э��
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
			RunLog( "׼�����͹�ǹ����:[%s]",Dla_Asc2Hex(realtimeBuff,gDla.msgLength) );
			ret = Dla_SendRealtimeData(realtimeBuff);
			 RunLog("�ظ����͹�ǹ�źŵ�1 ��");
		        ret = Dla_SendRealtimeData(sendBuff);	
			 RunLog("�ظ����͹�ǹ�źŵ�2 ��");	
			  ret = Dla_SendRealtimeData(sendBuff);
			//���ͽ�������
			gDla.msgLength = tmp;
			//printf( "׼�����ͽ�������:[%s]",Dla_Asc2Hex(sendBuff, gDla.msgLength) );			
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
				RunLog("TcpConnect���Ӳ��ɹ�,DLA���ͽ������� ʧ��");
			}else{
				ret = Dla_SendTransData(sendBuff);
				close(gDla._SOCKTCP);
				if(ret>=0){
					RunLog( "���ͽ������ݳɹ�,����ֵ[%d]",ret);
				}
				else{
					RunLog( "���ͽ�������ʧ��,����ֵ[%d]",ret);
				}
			}
		}else if( ret == 2 ){//ʵʱ����(gDla._REALTIME_FLAG == 1)&& ()
			//Dla_LevelLog( "����ʵʱ����");
			RunLog( "׼������ʵʱ����:[%s]",Dla_Asc2Hex(sendBuff,gDla.msgLength) );
			ret = Dla_SendRealtimeData(sendBuff);
			//modify by liys 2008-09-25 ,�����ǹ�źŶ�ʧ
			if (sendBuff[0] == 0x02){	
				 RunLog("�ظ����͹�ǹ�źŵ�1 ��");
				 ret = Dla_SendRealtimeData(sendBuff);	
				 RunLog("�ظ����͹�ǹ�źŵ�2 ��");
			       ret = Dla_SendRealtimeData(sendBuff);
				  
			}	
			Dla_LevelLog( "����ʵʱ�������,����[%d]",ret);
		}
		else{
			RunLog( "��������ֵ����1��2");
		}
		
	}
	
	gpDlaIfsf_Shm->css_data_flag = 0;
	Dla_DetachTheShm((char**)(&gpDlaGunShm));
	Dla_DetachTheShm((char**)(&gpDlaIfsf_Shm));
	//#endif	
	exit(0);	
	return 0;
}



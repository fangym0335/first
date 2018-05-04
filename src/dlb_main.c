/*
 * 2008-06-16 - Chen Fu-ming <chenfm@css-intelligent.com>
 *    
 */

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <signal.h>
#include <sys/ipc.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "doshm.h"
#include <time.h>
#include <pthread.h> 
#include <poll.h> 
#include "log.h"
#include "ifsf_pub.h"
#include "ifsf_tlg.h"

#ifndef TRUE
#define TRUE			1
#endif
#ifndef FALSE
#define FALSE			0
#endif
#define DLB_VER          "v1.05.04   -  2010.11.30"
#define DLB_VERSION      "v1.05.0409 -  2013.01.09"

#define LOG_IFSF 1//是否共用IFSF的日志 
#define DLB_SHM_INDEX 2100    //产生访问整个共享内存css的关键字的索引值 
#define DLB_SEM_INDEX 2105
#define DLB_5M    5000000
#define DLB_500K  500000 
#define DLB_OFFLINE_MAXNUM  50000 //离线交易最大数

#define  DLB_CONFIG_FILE                     "/home/App/ifsf/etc/dlb.cfg" //配置文件
#define  DLB_CONFIG_TLG_FILE             "/home/App/ifsf/etc/dlbtlg.cfg"  //液位仪配置文件
#define DLB_LOG_FILE                            "/home/App/ifsf/log/dlb.log" //日志文件
#define DEFAULT_WORK_DIR_4DLB  "/home/App/ifsf/"  //前庭控制器的默认工作目录,用于GetWorkDir()函数.
#define RUN_WORK_DIR_FILE           "/var/run/ifsf_main.wkd"  //应用程序运行目录文件,用于GetWorkDir()函数.
#define DLB_DSP_CFG_FILE                     "/home/App/ifsf/etc/dsp.cfg"  //IFSF
#define  DLB_PRICE_CFG_FILE			 "/home/App/ifsf/etc/oil.cfg"  //IFSF
#define MAX_PATH_NAME_LENGTH 1024
#define OFFLINE_TRANS_FILE			 "/home/Data/css_ofline_trans.data"  //脱机交易文件
//三个FIFO文件
#define DLB_FIFO "/tmp/css_fifo.data"
#define SEND_FIFO "/tmp/send_fifo.data"
#define TLG_FIFO "/tmp/tlg_fifo.data"
#define OIL_FIFO "/tmp/oil_fifo.data"//use to send oilno message
#define  DLB_SERVER_TCP_PORT_4LOGIN 9001
#define  DLB_SERVER_TCP_PORT_4DATA  9002
#define  DLB_DLB_TCP_PORT_4REALTIME  9006
#define  DLB_HEART_BEAT_DUP_PORT 9009
#define  DLB_SERVER_UDP_PORT_4REALTIME  3485
#define  DLB_CLIENT_TCP_PORT_ST 9011
#define  DLB_CLIENT_TCP_PORT_EN 9150
#define  DLB_CLIENT_LIMIT_PORT 140
#define  OK_LOGIN  1
#define  FAILURE_LOGIN  0
#define  OK_SERVER9002_CONN  1
#define  FAILURE_SERVER9002_CONN  0
#define inttobcd(data) (((((data) -(data)%10)/10)&0x0f)<<4|(data)%10)

IFSF_SHM *phIfsf_Shm;

static unsigned char node_offline[MAX_CHANNEL_CNT + 1];//最后一个为液位仪的心跳

static int imstatic =0;
static int tank_cnt;
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
} DLB_GUN_CONFIG;
typedef struct   {//  __attribute__((__packed__))
       unsigned char oilno[4];
	unsigned char oilname[40];
	unsigned char price[3];        //BCD
} DLB_OIL_CONFIG;
//加油机编号+面板编号+枪号+油品号+油价
typedef struct    __attribute__((__packed__)){//
	unsigned char node;             //节点号
	unsigned char logfp;           //面板号, 1+.
	unsigned char logical_gun_no;  //逻辑油枪号
	 unsigned char oilno[4];
	unsigned char price[4];        //BCD
}DLB_LOGIN;
typedef struct  __attribute__((__packed__)){//__attribute__((__packed__))
	unsigned char tpno;     //油罐号
	unsigned char oilcanLe[5];  //油罐高度
	unsigned char oilcanVo[5]; //油罐容积	
       unsigned char highAlarm[5];  // 油高报警线高度
	unsigned char lowAlarm[5];    //油低报警线高度
       unsigned char oilNo[3];     /*油枪号*/	     
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//	  
} TLG_LOGIN;

typedef struct  __attribute__((__packed__)){//__attribute__((__packed__))
	unsigned char tpno;     //油罐号
	unsigned char oilcanLe[4];  //油罐高度
	unsigned char oilcanVo[4]; //油罐容积	
       unsigned char highAlarm[4];  // 油高报警线高度
	unsigned char lowAlarm[4];    //油低报警线高度
       unsigned char oilNo[4];     /*油品号*/	     
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//	  
} DLB_TLG_LOGIN;
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
} DLB_FP_TR_DATA; //42 byte;
 typedef struct  __attribute__((__packed__)) {//
	unsigned char flag; //标志=0x55
	unsigned char length; //后面的长度=9
	unsigned char _STATION_NO_BCD[5]; /*BCD码,站点编号10位数字(省2市2站6)*/
	unsigned char node; //
	unsigned char fp; //
	unsigned char moved_gun;
	unsigned char fp_state;//,buff[16],*pBuff	
	//unsigned char *readBuffer;//,buff[16],*pBuff	
} DLB_REAL_STATE;
typedef struct __attribute__((__packed__)) {// 
	unsigned char flag; //标志=0xff
	unsigned char length; //后面的长度=18
	unsigned char _STATION_NO_BCD[5]; /*BCD码,站点编号10位数字(省2市2站6)*/
	unsigned char node; //
	unsigned char fp; //	
	unsigned char amount[5];
	unsigned char vollume[5];
	unsigned char moved_gun;
} DLB_REAL_TRANS;
typedef struct  __attribute__((__packed__)) {// 
	unsigned char flag; //标志=0xaa
	unsigned char length; //后面的长度=47
	unsigned char _STATION_NO_BCD[5]; /*BCD码,站点编号10位数字(省2市2站6)*/
	unsigned char node; //
	unsigned char fp; //
	unsigned char moved_gun;	
	unsigned char TR_Seq_Nb[2];
	unsigned char amount[5];
	unsigned char vollume[5];   
	unsigned char TR_Unit_Price[4]; 
	unsigned char TR_Prod_Nb[4]; 
	unsigned char TR_Date[4]; // user defined, Data_Id 202, offline Transaction Date
	unsigned char TR_Time[3]; // user defined, Data_Id 203, offline Transaction Time
	unsigned char TR_Start_Total[7]; // user defined, Data_Id 204, Start Log_Noz_Vol_Total
	unsigned char TR_End_Total[7];  // user defined, Data_Id 205, End Log_Noz_Vol_Total
}DLB_END_TRANS;
typedef struct   {// __attribute__((__packed__))
	unsigned char flag; //标志=0xaa
	unsigned char length; //后面的长度=47
	unsigned char _STATION_NO_BCD[5]; /*BCD码,站点编号10位数字(省2市2站6)*/
	unsigned char node; //
	unsigned char fp; //
	unsigned char moved_gun;	
	unsigned char TR_Seq_Nb[2];
	unsigned char amount[5];
	unsigned char vollume[5];   
	unsigned char TR_Unit_Price[4]; 
	unsigned char TR_Prod_Nb[4]; 
	unsigned char TR_Date[4]; // user defined, Data_Id 202, offline Transaction Date
	unsigned char TR_Time[3]; // user defined, Data_Id 203, offline Transaction Time
	unsigned char TR_Start_Total[7]; // user defined, Data_Id 204, Start Log_Noz_Vol_Total
	unsigned char TR_End_Total[7];  // user defined, Data_Id 205, End Log_Noz_Vol_Total
	unsigned char send_flag;//已经发送标志
}DLB_OFFLINE_TRANS;
 typedef struct {
	unsigned char _SERVER_IP[16]; /*服务器IP地址*/
	//unsigned char _STATION_NO[11]; /*站点编号数字字符串10位数字(省2市2站6)*/
	unsigned char _STATION_NO_BCD[5]; /*BCD码,站点编号10位数字(省2市2站6)*/
	 int _TLG_SEND_INTERVAL; /*液位仪上传时间间隔*/
	 int _REALTIME_FLAG; //实时数据上传标志,0不上传,1上传
	 int _SOCKTCP,_SOCKUDP, _SOCKHUDP;//_SOCKTCP用于交易数据上传,_SOCKUDP用于实时数据上传
	 //struct sockaddr _SERVER_UDP_ADDR;//struct sockaddr_in _SERVER_UDP_ADDR;
	unsigned char sendBuffer[512];//*sendBuffer
	//unsigned char *readBuffer;//,buff[16],*pBuff	
	int msgLength ;//, cfgLength unsigned long
	int fd_fifo ; //
	int __LEAK_SEND_INTERVAL;//leak oil message send interval
	int __AFTER_TIME;
} DLB_DLB_PUBLIC;


typedef struct {
	int ppid;
	unsigned char login_ok;//登录成功标志
	unsigned char server9002_connect_ok; //9002连接成功标志	
	DLB_DLB_PUBLIC css;
       unsigned char NuPort[DLB_CLIENT_LIMIT_PORT]; //0是空闲状态，1是使用状态，2是close状态 3是锁定状态
       unsigned char tank_pr_no[MAX_TP_PER_TLG][4];//油罐和油品的对应关系
       unsigned char write_tlg;
	   unsigned char oilno[4];
} DLB_SHM;

typedef struct{
	pid_t 	pid;
	char	flag[4];	//进程标志,本软件中用来重启该进程.
}DLB_Pid;

/*全局变量区*/
unsigned char	DLB_cnt =0;		/*进程最大数*/
DLB_Pid*	DLB_gPidList;
char	DLB_isChild = 0;
extern int errno;
DLB_SHM * gpDLB_Shm;
int gSendFifoFd;
static int tport = 0;
pthread_mutex_t wrlock = PTHREAD_MUTEX_INITIALIZER;

//--------------------------------------------------------------------------------
//日志,调用前必须初始化日志级别_SetLogLevel和日志文件句柄_SetLogFd
//------------------------------------------------------------------------------
#define DLB_DEFAULT_LOG_LEVEL 1                           //默认日志级别
#define DLB_MAX_VARIABLE_PARA_LEN 1024 
#define DLB_MAX_FILE_NAME_LEN 1024 
#define DLB_DEBUG 0
static char gsFileName[DLB_MAX_FILE_NAME_LEN + 1];	/*记录调试信息的文件名*/
static int giLine;								/*记录调试信息的行*/
 //自定义日志级别,0:什么也不做;1:只打印,不记录;2:打印且记录;
static int giLogLevel;                                             //日志调试级别
static int giFd;                                                       //日志文件句柄

#ifndef LOG_IFSF
//自定义分级文件日志,见_DLB_LevelLog.
#define DLB_LevelLog DLB_SetFileAndLine( __FILE__, __LINE__ ),DLB___LevelLog
#define DLB_FileLog DLB_SetFileAndLine( __FILE__, __LINE__ ),DLB___FileLog
#else
#define DLB_LevelLog  SetFileAndLine( __FILE__, __LINE__ ),__RunLog       //__ShowMsg
#define DLB_FileLog     SetFileAndLine( __FILE__, __LINE__ ), __RunLog
#endif

time_t result;
struct tm *my_time;

void Login_Sleep();

void DLB_Ssleep( long usec )
{
    struct timeval timeout;
    ldiv_t d;

    d = ldiv( usec, 1000000L );
    timeout.tv_sec = d.quot;
    timeout.tv_usec = d.rem;
    select( 0, NULL, NULL, NULL, &timeout );
}

int DLB_getlocaltime(char *my_bcd)
{
	char tmp[4];

	time(&result);
//	my_time = gmtime(&result);
	my_time = localtime(&result);
	my_time->tm_year += 1900;
	my_time->tm_mon += 1;

	my_bcd[0] = my_time->tm_year /1000;		//year
	my_time->tm_year -= my_bcd[0]*1000;
	my_bcd[1] = my_time->tm_year /100;
	my_time->tm_year -= my_bcd[1] *100;
	my_bcd[2] = my_time->tm_year /10;
	my_time->tm_year -= my_bcd[2] *10;
	my_bcd[3] = my_time->tm_year %10;
	my_bcd[0] = my_bcd[0] << 4;
	my_bcd[0] = my_bcd[0] | my_bcd[1];
	my_bcd[2] = my_bcd[2] << 4;
	my_bcd[1] = my_bcd[2] | my_bcd[3];

	tmp[0] = my_time->tm_mon / 10;				//month
	tmp[1] = my_time->tm_mon % 10;
	tmp[0] = tmp[0] <<4;
	tmp[3] = tmp[0] | tmp[1];
	my_bcd[2] = tmp[3];

	tmp[0] = my_time->tm_mday / 10;				//day
	tmp[1] = my_time->tm_mday % 10;
	tmp[0] = tmp[0] <<4;
	tmp[3] = tmp[0] | tmp[1];
	my_bcd[3] = tmp[3];

	tmp[0] = my_time->tm_hour / 10;				//hour
	tmp[1] = my_time->tm_hour % 10;
	tmp[0] = tmp[0] <<4;
	tmp[3] = tmp[0] | tmp[1];
	my_bcd[4] = tmp[3];

	tmp[0] = my_time->tm_min / 10;				//minute
	tmp[1] = my_time->tm_min % 10;
	tmp[0] = tmp[0] <<4;
	tmp[3] = tmp[0] | tmp[1];
	my_bcd[5] = tmp[3];

	tmp[0] = my_time->tm_sec / 10;				//second
	tmp[1] = my_time->tm_sec % 10;
	tmp[0] = tmp[0] <<4;
	tmp[3] = tmp[0] | tmp[1];
	my_bcd[6] = tmp[3];
//printf("my year is : %x %x %x %x %x %x %x\n",my_bcd[0],my_bcd[1],my_bcd[2],my_bcd[3],my_bcd[4],my_bcd[5],my_bcd[6]);

	return 0;

}
void DLB_SetLogLevel( const int loglevel )
{
	giLogLevel = loglevel;
	if( (giLogLevel !=0) ||(giLogLevel !=1) || (giLogLevel !=2)   ){
		giLogLevel = DLB_DEFAULT_LOG_LEVEL;
	}
}
void DLB_SetLogFd( const int fd )
{
	giFd= fd;
	if(  giFd <=  0){
		printf( " {日志文件描述符错误[%d] }\n", fd );
	}
}
void DLB_SetFileAndLine( const char *filename, int line )
{
	assert( strlen( filename ) + 1 <= sizeof( gsFileName) );
	strcpy( gsFileName, filename );
	giLine = line;
}

void DLB_GetFileAndLine( char *fileName, int *line )
{
	strcpy( fileName, gsFileName );
	*line = giLine;
}


//giLogLevel=0, no log;giLogLevel=1, only print log info; giLogLevel = 2, print log info and write log info to file 
void DLB___LevelLog(const char *fmt,...){//const int loglevel, const int fd, 
	va_list argptr;
	char sMessage[DLB_MAX_VARIABLE_PARA_LEN + 1];
	char fileName[DLB_MAX_FILE_NAME_LEN + 1];	/*记录调试信息的文件名*/
	int line;								/*记录调试信息的行*/
	int level = giLogLevel;
	
	if(level == 0 ){
		return;
	}
	DLB_GetFileAndLine(fileName, &line);
	sprintf(sMessage, "%s:%d	", fileName, line);
	sprintf(sMessage+strlen(sMessage), "pid = [%d]", getpid());

	va_start(argptr, fmt);
	vsprintf(sMessage+strlen(sMessage),fmt, argptr);
	va_end( argptr );

	
	if( (giLogLevel !=0) ||(giLogLevel !=1) || (giLogLevel !=2)   ){
		level = DLB_DEFAULT_LOG_LEVEL;
	}
	if(level > 0 ){
		printf( " {%s }\n", sMessage );
	}
	if(level>= 2){
		if(giFd <=0 )	/*日志文件未打开?*/
		{
			printf( " {日志文件描述符错误[%d] }\n", giFd );
			return;
		}
		write( giFd, sMessage, strlen(sMessage) );
		write( giFd, "\n", strlen("\n") );
	}
	return;
}
void DLB___FileLog(const char *fmt,...){//const int loglevel, const int fd, 
	va_list argptr;
	char sMessage[DLB_MAX_VARIABLE_PARA_LEN + 1];
	char fileName[DLB_MAX_FILE_NAME_LEN + 1];	/*记录调试信息的文件名*/
	int line;								/*记录调试信息的行*/
	
	DLB_GetFileAndLine(fileName, &line);
	sprintf(sMessage, "%s:%d	", fileName, line);
	sprintf(sMessage+strlen(sMessage), "pid = [%d]", getpid());

	va_start(argptr, fmt);
	vsprintf(sMessage+strlen(sMessage),fmt, argptr);
	va_end( argptr );

	printf( " {%s }\n", sMessage );
	if(giFd <=0 )	/*日志文件未打开?*/
	{
		printf( " {日志文件描述符错误[%d] }\n", giFd );
		return;
	}
	write( giFd, sMessage, strlen(sMessage) );
	write( giFd, "\n", strlen("\n") );
	
	return;
}
int DLB_InitLog(int loglevel){
	int fd;
    	 if( ( fd = open( DLB_LOG_FILE, O_WRONLY|O_CREAT|O_TRUNC, 00644 ) ) <= 0 ) {
		printf( "打开日志文件[%s]失败\n", DLB_LOG_FILE );
		return -1;
	}
	giFd = fd;
	DLB_SetLogLevel(loglevel);
	return 1;
}
/*
 *  替换日志文件-将原文件拷贝到/home/Data/log_bak/ 下
 *  成功返回0, 失败返回 -1
 */
int DLB_BackupLog()
{
	int fd;
	int _fd;
	int ret;
	struct stat f_stat;
	unsigned char cmd[(2 * sizeof(DLB_LOG_FILE)) + 16];
	unsigned char date_time[7] = {0};

	DLB_getlocaltime(date_time);

	if((fd = open(DLB_LOG_FILE, O_RDONLY, 00644)) < 0) {
		return -1;
	}

	if (fstat(fd, &f_stat) != 0) {
		return -1;
	}
	
	if ((long)f_stat.st_size > DLB_500K) {
		RunLog("run.log 大小超过 500K, 系统备份日志");

		system("mkdir  /home/Data/log_backup");

		sprintf(cmd, "cp %s /home/Data/log_backup/run_log[%02x%02x%02x%02x%02x%02x%02x].bak",
						DLB_LOG_FILE, date_time[0], date_time[1], date_time[2],
						date_time[3], date_time[4], date_time[5], date_time[6]);
		system(cmd);

		truncate(DLB_LOG_FILE, 0);
	} else {
		close(fd);
		return 0;
	}
	
	close(fd);

	if((fd = open(DLB_LOG_FILE, O_CREAT , 00644)) < 0) {
		return -1;
	}

	close(fd);

	return 0;
}

//------------------------------------------------


//----------------------------------------------------------
//进程管理
//---------------------------------------------------------
int DLB_AddPidCtl( DLB_Pid *pidList, pid_t pid, char *flag, unsigned char *count )
{
	DLB_Pid *pFree;

	pFree = pidList + (*count);

	pFree->pid = pid;
	strcpy( pFree->flag, flag );

	(*count)++;

	return 0;
}

int DLB_GetPidCtl( DLB_Pid *pidList, pid_t pid, char *flag, unsigned char *count )
{
	int i;
	DLB_Pid *pFree;
	unsigned char tmp;
	for( i = 0; i < *count; i++ )
	{
		pFree = pidList + i;
		if( pFree->pid == pid )
		{
			strcpy( flag, pFree->flag );
			//*count = i;
			tmp = i;
			memcpy(count, &tmp, sizeof(unsigned char));
			return 0;
		}
	}
	return -1;

}

int DLB_ReplacePidCtl( DLB_Pid *pidList, pid_t pid, char *flag, unsigned char pos )
{
	DLB_Pid *pFree;
	pFree = pidList + pos;

	pFree->pid = pid;
	strcpy( pFree->flag, flag );

	return 0;
}

//--------------------------------------------------------------
//共享内存初始化
//-------------------------------------------------------------
//char DLB_isChild;
static char *DLB_AttachTheShm( int index );

static void DLB_RemoveIPC()
{
	//int i;
	printf( "清理IPC" );	
	RemoveShm( DLB_SHM_INDEX );	/*不判断返回码.有就销毁*/	
	//RemoveShm( IFSF_SHM_INDEX );
	
	//RemoveSem( PORT_SEM_INDEX );	
	
	//#endif
	return;

}
void DLB_WhenExit()
{
	RunLog( "进程[%d]退出,其父进程为[%d], 当前主进程的父进程为[%d]",  \
		getpid() ,getppid(), gpDLB_Shm->ppid);
	if( (DLB_isChild == 0) &&(gpDLB_Shm->ppid == getppid()) )	  /*父进程负责ipc的删除*/
		DLB_RemoveIPC(); 
	return;
}



/*系统初始化*/
int DLB_InitAll(  )
{
	//int fd;								/*文件描述符*/
	int ret;
	int size;
	
	//int ifsf_size ;
	
	
	//extern DLB_SHM * gpDLB_Shm; //该结构是全局的量，只有一个,在ifsf_main.c中定义.
	
	

	
	
	size = sizeof(DLB_SHM);
	//ifsf_size = sizeof(IFSF_SHM);
	
	/*创建共享内存*/	
	ret = DLB_CreateShm( DLB_SHM_INDEX, size );	
	if(ret < 0) {
		RunLog( "创建共享内存DLB_SHM 失败.请检查系统." );
		return -1;
	}
			
	gpDLB_Shm = (DLB_SHM *)DLB_AttachTheShm( DLB_SHM_INDEX );
	//gpIfsf_shm = (IFSF_SHM* )AttachShm( IFSF_SHM_INDEX );		
	//
	if (gpDLB_Shm == NULL) {//(gpIfsf_shm == NULL) && ()
		RunLog( "连接共享内存失败.请检查系统" );
		return -1;
	}
		
	
	//gpGunShm->sockCDPos = -1; //add in 2007.9.10
	//->chnCnt= 1;	
	atexit( DLB_WhenExit );

	

	/*创建一个信号量.用于对串口写时加锁
	ret = CreateSem( PORT_SEM_INDEX );
	if( ret < 0 ) {
		UserLog( "创建信号量失败" );
		return -1;
	}
	*/	

	return 0;

}
//----------------------------------------

//----------------------------------------------------------------
//信号处理
//----------------------------------------------------------------
typedef void DLB_Sigfunc( int );			/*Sigfunc表示一个返回值为void,参数为int的函数*/
										/*定义该类型可以简化signal函数的书写*/
//#define __DOSIANAL_STATIC__
int DLB_CatchSigAlrm;
int DLB_SetSigAlrm;
/*用于记录老的alarm函数的指针*/
DLB_Sigfunc 			*DLB_pfOldFunc=NULL;

/*缺省的alarm处理函数*/
static void DLB_WhenSigAlrm( int sig );

/*当收到SIGCHLD时,等待结束的子进程.防止产生僵尸进程*/
static void DLB_WhenSigChld(int sig);
DLB_Sigfunc *DLB_Signal( int signo, DLB_Sigfunc *func );


static void DLB_WhenSigAlrm( int sig )
{
	if( sig != SIGALRM )
		return;
	DLB_CatchSigAlrm = TRUE;
	DLB_Signal( sig, DLB_WhenSigAlrm);
}

DLB_Sigfunc *DLB_SetAlrm( uint sec, DLB_Sigfunc *pfWhenAlrm )
{

	DLB_CatchSigAlrm = FALSE;
	DLB_SetSigAlrm = FALSE;

	if( sec == TIME_INFINITE )
		return NULL;

	/*设置对SIGALRM信号的处理*/
	if( pfWhenAlrm == NULL )
		DLB_pfOldFunc = DLB_Signal( SIGALRM, DLB_WhenSigAlrm );
	else
		DLB_pfOldFunc = DLB_Signal( SIGALRM, pfWhenAlrm );

	if( DLB_pfOldFunc == (DLB_Sigfunc*)-1 )
	{
		return (DLB_Sigfunc*)-1;
	}

	/*设置闹钟*/
	alarm( sec );

	DLB_SetSigAlrm = TRUE;

	return DLB_pfOldFunc;
}

void DLB_UnSetAlrm( )
{
	DLB_CatchSigAlrm = FALSE;

	if( !DLB_SetSigAlrm )
		return;

	alarm( 0 );

	/*设置对SIGALRM信号的处理*/
	Signal( SIGALRM, DLB_pfOldFunc );
	DLB_pfOldFunc = NULL;

	DLB_SetSigAlrm = FALSE;

	return;
}


//-----------------------------------------------------------------------
//公用函数
//-----------------------------------------------------------------------
#define DLB_TCP_MAX_SIZE 1280
//
#define DLB_MAX_ORDER_LEN		128

unsigned char* DLB_Asc2Hex( unsigned char *cmd, int cmdlen )
{
	static char retcmd[DLB_MAX_ORDER_LEN*2+1];
	int	i;

	memset( retcmd, 0, sizeof(retcmd) );
	for( i = 0; i < cmdlen; i++ ) {
		sprintf(retcmd + i * 2, "%02x", cmd[i]);
	}

	return retcmd;
}
//获取本机IP,返回本机IP地址，返回-1失败，解决DHCP问题
static unsigned  long  DLB_GetLocalIP(){//参见tcp.h和tcp.c文件中有,可以选择一个.
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
static unsigned long  DLB_GetBroadCast()
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

int DLB_TcpServInit( int port )//const char *sPortName, int backLog
{

	struct sockaddr_in serverAddr;
	//in_port_t	port;
	int			sockFd;
	int 		backLog = 1;
	int reuse = 1;
	int i;

	for ( i = 0; i < 2; i ++){
		sockFd = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
		if( sockFd < 0 )
		{
			RptLibErr( errno );
			return -1;
		}

		if(setsockopt(sockFd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)) < 0){
				RptLibErr( errno );
				return -1;
		}
		
		serverAddr.sin_family = AF_INET;
		serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);	/*htonl 主机字节序到网络字节序.不过INADDR_ANY为0,所以可以不用*//*INADDR_ANY 由内核选择地址*/
		serverAddr.sin_port = htons(port);

		if( bind( sockFd, (SA *)&serverAddr, sizeof( serverAddr ) ) < 0 )
		{	
			if ( errno == EADDRINUSE ){
				RunLog("address reused ; try again");
				close(sockFd);
				sleep(15);
				continue;
			}
			
			RunLog( "DLB_TcpServInit错误号:%d", errno );
			return -1;
		}else {
			break;
		}

	}
	if ( 2 == i )
		return -1;
	if( listen( sockFd, backLog ) < 0 )
	{
		RunLog( "DLB_TcpServInit错误号:%d",errno );
		return -1;
	}

	return sockFd;
}

int DLB_TcpAccept( int sock )// , SockPair *pSockPair
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

int DLB_CheckInterruptConnect(int sock)
{
	struct pollfd unix_really_sucks;
	int some_more_junk;
	socklen_t yet_more_useless_junk;
	
	if ( errno != EINTR /* && errno != EINPROGRESS */ ) {
        	RunLog ("connect [%s]", strerror(errno));
        	return -1;
      	}
    	unix_really_sucks.fd = sock;
    	unix_really_sucks.events = POLLOUT;
    	while ( poll (&unix_really_sucks, 1, -1) == -1 ) {
		RunLog ("poll [%s]", strerror(errno));
      		if ( errno != EINTR ) {
          		RunLog ("poll [%s]", strerror(errno));
          		return -1;
        	}
    	}
	RunLog("poll revents is %d", unix_really_sucks.revents);
    	yet_more_useless_junk = sizeof(some_more_junk);
    	if ( getsockopt (sock, SOL_SOCKET, SO_ERROR,\
               &some_more_junk,\
               &yet_more_useless_junk) == -1 ) {
        	RunLog ("getsockopt [%s]", strerror(errno));
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
int DLB_TcpConnect( const char *sIPName,int Ports , int nTimeOut )//const char *sPortName
{
	struct sockaddr_in serverAddr, clientAddr;
	int sock;
	in_port_t port;
	in_addr_t addr;
	int ret,flag_port,i,j;
	
	RunLog("DLB_Send_data ---- In DLB_TcpConnect 1-1");

	Sigfunc *OldAction;

	Assert( sIPName != NULL && *sIPName != 0 );
	port = htons(Ports);

	if( ( addr = GetTcpAddr( sIPName ) ) == INADDR_NONE )
	{
		RptLibErr( TCP_ILL_IP );
		return -1;
	}

	RunLog("DLB_Send_data ---- In DLB_TcpConnect 1-2");
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = addr;
	serverAddr.sin_port = port;
	sock = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
	if( sock < 0 )
	{
		RptLibErr( errno );
		return -1;
	}
	RunLog("DLB_Send_data ---- In DLB_TcpConnect 1-3");
	j = 5;
	do{
	     
	RunLog("DLB_Send_data ---- In DLB_TcpConnect 2-1 sIPName:%s Ports:%d", sIPName, Ports);
			if( j != 5 )
			{
	RunLog("DLB_Send_data ---- In DLB_TcpConnect 2-2");
				close( sock );
				sock = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
				if( sock < 0 )
	RunLog("DLB_Send_data ---- In DLB_TcpConnect 2-3");
				{
					RptLibErr( errno );
					return -1;
				}
			}
	RunLog("DLB_Send_data ---- In DLB_TcpConnect 2-4");
			flag_port = tport - DLB_CLIENT_TCP_PORT_ST;
			if(flag_port >= 0){
			    pthread_mutex_lock(&wrlock);
			    gpDLB_Shm->NuPort[flag_port] = 2;// 2表示端口停止使用
			    pthread_mutex_unlock(&wrlock);
				
	RunLog("DLB_Send_data ---- In DLB_TcpConnect 2-5上次使用端口号tport: [%d] 状态:[%d]", tport, gpDLB_Shm->NuPort[flag_port]);
				RunLog("上次使用端口号tport: [%d] 状态:[%d]", tport, gpDLB_Shm->NuPort[flag_port]);

			}
			
/*
			for(i = 0; i < DLB_CLIENT_LIMIT_PORT; i++){
				RunLog("端口号NuPort[%d] = [%d]  flag_port:[%d]", i, gpDLB_Shm->NuPort[i],flag_port);
			}
*/			
			clientAddr.sin_family = AF_INET;
			clientAddr.sin_addr.s_addr = htonl(INADDR_ANY);
			//RunLog("dd first is %d", imstatic);
	RunLog("DLB_Send_data ---- In DLB_TcpConnect 2-6");
			
			if (!imstatic){
	RunLog("DLB_Send_data ---- In DLB_TcpConnect 2-7");
				//RunLog("first is %d", imstatic);
				imstatic =1;
				for(i = 0; i < DLB_CLIENT_LIMIT_PORT; i++){
					if(gpDLB_Shm->NuPort[i] == 0){
	RunLog("DLB_Send_data ---- In DLB_TcpConnect 2-8端口 is [%d]", i);
						
						RunLog("端口 is [%d]", i);
						
						tport = i + DLB_CLIENT_TCP_PORT_ST;
						pthread_mutex_lock(&wrlock);
						gpDLB_Shm->NuPort[i] = 1;
						 pthread_mutex_unlock(&wrlock);
						break;
					}
				}
			}
			else{
			
	RunLog("DLB_Send_data ---- In DLB_TcpConnect 2-9");
				for(i = ((flag_port+1) % DLB_CLIENT_LIMIT_PORT); i < DLB_CLIENT_LIMIT_PORT; i++){
					if(gpDLB_Shm->NuPort[i] == 0){
	RunLog("DLB_Send_data ---- In DLB_TcpConnect 2-10");
						tport = i + DLB_CLIENT_TCP_PORT_ST;
						pthread_mutex_lock(&wrlock);
						gpDLB_Shm->NuPort[i] = 1;
						 pthread_mutex_unlock(&wrlock);
						break;
					}
				}
			}
			clientAddr.sin_port = htons(tport);
			if(DLB_SERVER_TCP_PORT_4DATA == Ports) {
	RunLog("DLB_Send_data ---- In DLB_TcpConnect 2-11本次使用临时端口号tport: [%d]", tport);
				
				RunLog("本次使用临时端口号tport: [%d]", tport);

			}
			else
				
	RunLog("DLB_Send_data ---- In DLB_TcpConnect 2-12注册端口号Ports: [%d]", tport);
				RunLog("注册端口号Ports: [%d]", tport);
	
		--j;
		DLB_Ssleep(200000);
	}while( (bind( sock, (SA *)&clientAddr, sizeof( clientAddr ) ) < 0) && j > 0 );
	RunLog("DLB_Send_data ---- In DLB_TcpConnect 3-1");

	if( j == 0  )
	{
	RunLog("DLB_Send_data ---- In DLB_TcpConnect 3-2DLB_TcpConnect错误号:%d", errno );
		RunLog( "DLB_TcpConnect错误号:%d", errno );
		RunLog( "三次绑定IP错误重启dlb_main" );
		close( sock );
 		sleep(40);//重启前暂停40秒
		ret = system("/usr/bin/killall dlb_main");
		if( ret != 0){
			ret = system("/usr/bin/killall dlb_main");
		}
		return -1;
	}	

	RunLog("DLB_Send_data ---- In DLB_TcpConnect 3-3");
	if( nTimeOut == TIME_INFINITE )
	{
	RunLog("DLB_Send_data ---- In DLB_TcpConnect 3-4");
		ret = connect( sock, (struct sockaddr *)&serverAddr, sizeof( serverAddr ) );
		/* Start with fd just returned by socket(), blocking, SOCK_STREAM... */
		if ( ret == -1 ) {
	RunLog("DLB_Send_data ---- In DLB_TcpConnect 3-5");
			if (DLB_CheckInterruptConnect(sock) == -1 ) {
				RunLog("check interrupt connect failed %d", sock);
	RunLog("DLB_Send_data ---- In DLB_TcpConnect 3-6");
			}
  		}
	RunLog("DLB_Send_data ---- In DLB_TcpConnect 3-7connect TIME_INFINITE ret[%d],errno[%d],IP[%s], port[%d],port_ns[%d]\n",ret,errno,sIPName,Ports,port);
		/* At this point, fd is connected. */
		RunLog("connect TIME_INFINITE ret[%d],errno[%d],IP[%s], port[%d],port_ns[%d]\n",ret,errno,sIPName,Ports,port);
	}
	else
	{
	RunLog("DLB_Send_data ---- In DLB_TcpConnect 3-8");
		OldAction = SetAlrm( nTimeOut, NULL );  /*如果在alarm调用后,connect调用前超时,则connect等待时间为缺省时间*/
	RunLog("DLB_Send_data ---- In DLB_TcpConnect 3-9");
		ret = connect( sock, (struct sockaddr *)&serverAddr, sizeof( serverAddr ) );
		/* Start with fd just returned by socket(), blocking, SOCK_STREAM... */
		if ( ret == -1 ) {
	RunLog("DLB_Send_data ---- In DLB_TcpConnect 3-10");
			if (DLB_CheckInterruptConnect(sock) == -1 ) {
	RunLog("DLB_Send_data ---- In DLB_TcpConnect 3-11check interrupt connect failed %d", sock);
				RunLog("check interrupt connect failed %d", sock);
			}
  		}
	RunLog("DLB_Send_data ---- In DLB_TcpConnect 3-12connect secend ret[%d],sock[%d],errno[%d],IP[%s], port[%d],port_ns[%d]\n",ret, sock,errno,sIPName,Ports,port);
		/* At this point, fd is connected. */
		RunLog("connect secend ret[%d],sock[%d],errno[%d],IP[%s], port[%d],port_ns[%d]\n",ret, sock,errno,sIPName,Ports,port);
		UnSetAlrm();
	}

	RunLog("DLB_Send_data ---- In DLB_TcpConnect 3-13");
	if( ret == 0 )
		return sock;
	RunLog("DLB_Send_data ---- In DLB_TcpConnect 3-14");

	close( sock );

	RptLibErr( errno );

	return -1;

}

/*
	 0:	成功
	-1:	失败
	-2:	超时
*/
static ssize_t DLB_TcpSendDataInTime( int fd, const char *s, size_t len,  int timeout )
{
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
	
//	RunLog("before WirteLengthInTime -------------");
	n = WriteByLengthInTime( fd, t, sendlen, timeout );

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

static ssize_t DLB_TcpRecvDataInTime( int fd, char *s, size_t *buflen,  int timeout ) //size_t headlen,
{
	ssize_t	recvlen;
	size_t	len;
	char	buf[DLB_TCP_MAX_SIZE];
	char	*t;
	int	mflag;
	int 	n;
	int	ret;
	
	Assert( s != NULL );

	mflag = 0;

	memset( buf, 0, sizeof(buf) );

	len =*buflen;  //(unsigned short )head[6] *256+ (unsigned short )head[7];

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
	//RunLog("read begain");
	n = ReadByLengthInTime( fd,t, len, timeout );
	//RunLog("read over");
	if ( n == -1 ) {
		ret = -1;
	}else if ( n == 0  ) {
		ret = -1;
	}else if ( n > 0 && n <=len ) {
		memcpy( s, t, n);
		memcpy( (char *)buflen, (char *)&n, sizeof(size_t));
		ret = 2;
	} else {
		memcpy( s, t, *buflen);
		s[*buflen] = 0;
		ret = 1;
	}
	if ( mflag == 1 ) {
		Free( t );
	}

	return ret;
}

static int DLB_ReadALine( FILE *f, char *s, int MaxLen ){
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
static int   DLB_GetWorkDir( char *workdir) {
	FILE *f;
	int i, r,trytime = 3;
	char s[96] ;

	if(  workdir == NULL ){		
		return -1;
	}	
	f = fopen( RUN_WORK_DIR_FILE, "rt" );
	if( ! f ){	// == NULL
		memcpy( workdir,DEFAULT_WORK_DIR_4DLB, strlen(DEFAULT_WORK_DIR_4DLB) );
		return 0;
	}	
	bzero(s, sizeof(s));
	for(i=0; i<trytime; i++){
		r = DLB_ReadALine( f, s, sizeof(s) );		
		if( s[0] == '/' )
			break;
	}
	fclose( f );
	if(s[0] == '/' ){
		memcpy(workdir,s, strlen(s));	
		return 0;
	}
	else{
		memcpy( workdir,DEFAULT_WORK_DIR_4DLB, strlen(DEFAULT_WORK_DIR_4DLB) );
		return 0;
	}
}

static int DLB_IsFileExist(const char * sFileName ){
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
static char *DLB_TrimAll( char *buf )
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
static char *DLB_split(char *s, char stop){
	static char data[256];
	static char *sdata;
	char *tmp;
	int i, len, j;

	len = strlen(s);
	tmp = s;
	bzero(data, sizeof(data));
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
	sdata = &data[0];
	return sdata;
}
//--------------------------------------------------------------------------



//-------------------------------------------
//初始化配置参数
//-------------------------------------------


#define DLB_INI_FIELD_LEN 63
#define DLB_INI_MAX_CNT 30
typedef struct {//DLB_ini_name_value
char name[DLB_INI_FIELD_LEN + 1];
char value[DLB_INI_FIELD_LEN + 1];
}DLB_INI_NAME_VALUE;
static DLB_INI_NAME_VALUE DLB_ini[DLB_INI_MAX_CNT];

//first read ini file by readIni fuction , then change or add ini data, last write ini data to ini file by writeIni fuction.
static int DLB_readIni(char *fileName){

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
	ret = DLB_IsFileExist(fileName);
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
	
	
	bzero(&(DLB_ini[0].name), sizeof(DLB_INI_NAME_VALUE)*DLB_INI_MAX_CNT);	
	i = 0;
	while (my_data[0] !='\0'){
		tmp = DLB_split(my_data, '=');// split data to get field name.
		bzero(&(DLB_ini[i].name), sizeof(DLB_ini[i].name));
		strcpy(DLB_ini[i].name, DLB_TrimAll(tmp) );
	//	free(tmp);
		tmp = DLB_split(my_data, '\n');// split data to get value.
		bzero(&(DLB_ini[i].value), sizeof(DLB_ini[i].value));
		strcpy(DLB_ini[i].value, DLB_TrimAll(tmp));
	//	free(tmp);
		i ++;
	}
	//free(my_data);
	return i--;	
}

static int DLB_writeIni(const char *fileName){
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
	for(i=0;i<DLB_INI_MAX_CNT;i++){
		if( (DLB_ini[i].name[0] !=0)&&(DLB_ini[i].value[0] !=0) ){
			bzero(temp,sizeof(temp));
			sprintf(temp,"%s\t = %s  \n", DLB_ini[i].name,  DLB_ini[i].value);
			write( fd, temp, strlen(temp) );
		}
	}	
	close(fd);
	return 0;
}
static int DLB_addIni(const char *name, const char *value){
	int i,n;
	
	if(  (NULL== name)||(NULL== value)  ){
		return -1;
	}	
	n = strlen(name);
	for(i=0;i<DLB_INI_MAX_CNT;i++){
		if( memcmp(DLB_ini[i].name,name,n)  == 0 ){
			return -1;
			break;
		}
		else if(DLB_ini[i].name[0] == 0){
			bzero( DLB_ini[i].name, sizeof(DLB_ini[i].name)  );
			bzero( DLB_ini[i].value, sizeof(DLB_ini[i].value)  );			
			memcpy(DLB_ini[i].name, name, strlen(name) );
			memcpy(DLB_ini[i].value, value, strlen(value) );
			return 0;
		}
	}
		
		return -2;
}
static int DLB_changeIni(const char *name, const char *value){
	int i,n;
	int ret;
	
	if(  (NULL== name)||(NULL== value)  ){
		return -1;
	}	
	n = strlen(name);
	for(i=0;i<DLB_INI_MAX_CNT;i++){
		if( memcmp(DLB_ini[i].name,name,n)  == 0 ){
			bzero( DLB_ini[i].value, sizeof(DLB_ini[i].value)  );
			memcpy(DLB_ini[i].value, value, strlen(value) );
			break;
		}
	}
	if(i == DLB_INI_MAX_CNT){
		ret = DLB_addIni(name, value);
		if(ret < 0){
			return -2;
		}
	}		
	else
		return 0;
}


char *DLB_upper(char *str){
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
int DLB_InitConfig(){
	int i,ret,iniFlag;
	char configFile[256];
	
	bzero(configFile,sizeof(configFile));
	memcpy(configFile,DLB_CONFIG_FILE,sizeof(DLB_CONFIG_FILE));
	//RunLog("开始读配置参数文件");
	DLB_GetLocalIP();   //解决特定IP地址段错误!
	ret = DLB_readIni(configFile);
	if(ret < 0){
		RunLog("readIni error in _InitServerIP");
		return -1;
	}
	iniFlag = 0;
	//RunLog("开始查找配置参数");
	for(i=0;i<ret;i++){
		if( memcmp(DLB_ini[i].name,"SERVER_IP",9) == 0){
			memcpy(gpDLB_Shm->css._SERVER_IP, DLB_ini[i].value, sizeof(DLB_ini[i].value) );
			RunLog("SERVER_IP[%s]",gpDLB_Shm->css._SERVER_IP);
			iniFlag += 1;
		}else if( memcmp(DLB_ini[i].name,"STATION_NO",10) == 0 ){
			//memcpy(gpDLB_Shm->css._STATION_NO, DLB_ini[i].value, sizeof(DLB_ini[i].value) );
			gpDLB_Shm->css._STATION_NO_BCD[0] = ( (DLB_ini[i].value[0]-'0')&0x0f ) << 4 | ((DLB_ini[i].value[1]-'0')&0x0f);
			gpDLB_Shm->css._STATION_NO_BCD[1] = ( (DLB_ini[i].value[2]-'0')&0x0f ) << 4 | ((DLB_ini[i].value[3]-'0')&0x0f);
			gpDLB_Shm->css._STATION_NO_BCD[2] = ( (DLB_ini[i].value[4]-'0')&0x0f ) << 4 | ((DLB_ini[i].value[5]-'0')&0x0f);
			gpDLB_Shm->css._STATION_NO_BCD[3] = ( (DLB_ini[i].value[6]-'0')&0x0f ) << 4 | ((DLB_ini[i].value[7]-'0')&0x0f);
			gpDLB_Shm->css._STATION_NO_BCD[4] = ( (DLB_ini[i].value[8]-'0')&0x0f ) << 4 | ((DLB_ini[i].value[9]-'0')&0x0f);
			RunLog("STATION_NO[%s]", DLB_Asc2Hex(gpDLB_Shm->css._STATION_NO_BCD, 5) );
			iniFlag += 1;
		}else{			
		}		
	}

	gpDLB_Shm->write_tlg = 0;

	//RunLog("查找配置参数结束");
	if(iniFlag != 2 ) 
		return -1;
	else 
		return 0;	
 }


 
//-------------------------------------------





//------------------------------------------------
//共享内存处理
//------------------------------------------------
static char DLB_gKeyFile[MAX_PATH_NAME_LENGTH]="";

static int DLB_InitTheKeyFile() 
{
	char	*pFilePath, FilePath[]=DEFAULT_WORK_DIR_4DLB;
	char temp[128];
	int ret;
	
	//取keyfile文件路径//	
	pFilePath = getenv( "WORK_DIR" );	//WORK_DIR 工作目录的环境变量
	if( NULL ==  pFilePath){
		bzero(temp, sizeof(temp));
		ret = DLB_GetWorkDir(temp);
		if(ret >= 0 ){
			pFilePath = temp;
		}
		else{
			pFilePath = FilePath;
		}
		//cgi_error( "未定义环境变量WORK_DIR" );
	}
	if((NULL ==  pFilePath) ||(pFilePath[0] !=  '/') ){
		pFilePath = FilePath;
		}
	sprintf( DLB_gKeyFile, "%s/etc/keyfile.cfg", pFilePath );
	if( DLB_IsFileExist( DLB_gKeyFile ) == 1 )
		return 0;

	return -1;
}

static key_t DLB_GetTheKey( int index )
{
	key_t key;
	if( DLB_gKeyFile[0] == 0 ){
		printf( "gKeyFile[0] == 0" );
		exit(1);
	}
	key = ftok( DLB_gKeyFile, index );
	if ( key == (key_t)(-1) ){
		printf( " key == (key_t)(-1)" );
		exit(1);
	}
	return key;
}


static char *DLB_AttachTheShm( int index )
{
	int ShmId;
	char *p;

	ShmId = shmget( DLB_GetTheKey(index), 0, 0 );
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

static int DLB_DetachTheShm( char **p )
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


int DLB_IsShmExist( int index )
{
	int ShmId;

	ShmId = shmget( DLB_GetTheKey(index), 0, 0 );
	if( ShmId < 0 )
		return FALSE;

	return TRUE;
}



int DLB_RemoveShm( int index )
{
	int ShmId;
	int ret;

	ShmId = shmget( DLB_GetTheKey( index ), 1, IPC_EXCL );
	if( ShmId < 0 )
	{
		return -1;
	}
	ret = shmctl( ShmId, IPC_RMID, 0 );
	if( ret < 0 )
	{
		return -1;
	}
	return 0;
}

int DLB_CreateShm( int index, int size )
{
	int shmId;
	char * ptrShm;

	ptrShm = DLB_AttachTheShm( index );
	if( ptrShm != NULL )
	{
		if( DLB_DetachTheShm( &ptrShm ) < 0 || DLB_RemoveShm( index ) < 0 )
			return -1;
	}
	shmId = shmget( DLB_GetTheKey(index), size, IPC_CREAT|IPC_EXCL|PERMS );
	if( shmId < 0 )
	{
		printf( "errno = [%d]\n", errno );
		return -1;
	}
	ptrShm = ( char* )shmat( shmId, (char*)0, 0 ) ;
	shmdt( ptrShm );

	return 0;
}

int DLB_CreateNewShm( int index, int size )
{
	int shmId;
	char * ptrShm;

	shmId = shmget( DLB_GetTheKey(index), size, IPC_CREAT|IPC_EXCL|PERMS );
	if( shmId < 0 )
	{
		return -1;
	}
	ptrShm = ( char* )shmat( shmId, (char*)0, 0 ) ;
	shmdt( ptrShm );

	return 0;
}

//--------------------------------
//向中心注册
int DLB_Login(){
	int i, ret, tmp, sockfd,fd,n,k,g;//,
	//int sockflag =0;
	unsigned char sPortName[5],*sIPName;	
	unsigned char sendBuffer[2048] ,buff[64],*pBuff,*tlgBuff;	
	unsigned int msgLength , cfgLength , tlgcfgLength ,be_tlgcfgLength;
	int timeout = 5, tryn=10;
	DLB_GUN_CONFIG *pGunConfig;
	DLB_OIL_CONFIG *pOilConfig;
	DLB_TLG_LOGIN *tlgConfig;
	unsigned long localIP;
	DLB_LOGIN *psLogin;
       TLG_LOGIN *ptLogin;      

	
	bzero(sendBuffer,sizeof(sendBuffer));
	
	//加油机面板油品关系数据
	pBuff = &sendBuffer[18];
	msgLength = 13;
	cfgLength = 0;
	tlgcfgLength = 0;
	//RunLog(" 配置FCC加油机油枪");
	fd = open( DLB_DSP_CFG_FILE, O_RDONLY );
	if(fd <=0){
		RunLog("  打开FCC加油机配置文件出错.");
		exit(0);
	}
	i=0;//用作油枪数统计
	while (1) {
		n = read( fd, buff, sizeof(DLB_GUN_CONFIG) );
		if( n == 0 ) {	//读到了文件尾
			close(fd);
			break;
		} else if( n < 0 ) {
			RunLog( "读取 dsp.cfg 配置文件出错" );
			close(fd);
			return -1;
		}
		//RunLog("第[%d]次读取到dsp.cfg 数据[%d]个字符\n",i+1, n);

		if( n != sizeof(DLB_GUN_CONFIG) ) {
			RunLog( "dsp.cfg 配置信息有误" );
			close( fd );
			return -1;
		}
		psLogin = (DLB_LOGIN *)&pBuff[0];
		pGunConfig = (DLB_GUN_CONFIG *) (&buff[0]);
		//RunLog("油枪数据[%s]",DLB_Asc2Hex(buff, n) );
		psLogin->node= pGunConfig->node;
		psLogin->logfp= pGunConfig->logfp;
		psLogin->logical_gun_no = pGunConfig->logical_gun_no;
		memcpy(psLogin->oilno, pGunConfig->oil_no, 4);
		pBuff += sizeof(DLB_LOGIN);
		cfgLength +=sizeof(DLB_LOGIN);
		i++;  // gun number ++		
	}		
	k = i;//记录油枪个数
	RunLog("共有油枪[%d],油枪待注册数据[%s]", k, DLB_Asc2Hex(&sendBuffer[18], cfgLength) );
	
	//RunLog("配置 FCC加油机油价");
	fd = open( DLB_PRICE_CFG_FILE, O_RDONLY );
	if(fd <=0){
		RunLog("  打开FCC加油机油价配置文件出错.");
		exit(0);
	}
	while (1) {
		n = read( fd, buff, sizeof(DLB_OIL_CONFIG) );
		if( n == 0 ) {	//读到了文件尾
			close(fd);
			break;
		} else if( n < 0 ) {
			RunLog( "读取 oil.cfg 配置文件出错" );
			close(fd);
			return -1;
		}
		//RunLog("读取到oil.cfg 数据[%d]个字符",n);

		if( n != sizeof(DLB_OIL_CONFIG) ) {
			RunLog( "oil.cfg 配置信息有误" );
			close( fd );
			return -1;
		}
		pBuff = &sendBuffer[18];
		psLogin = (DLB_LOGIN *)&pBuff[0];
		pOilConfig = (DLB_OIL_CONFIG *) (&buff[0]);
		//RunLog(" 开始配置一个油价,共有油枪[%d], 配置前数据[%s]", \
		//	k, DLB_Asc2Hex(sendBuffer, 18+cfgLength));
		for(i=0;i<k;i++){
			psLogin = (DLB_LOGIN *)&pBuff[0];
			//RunLog("油枪数据[%s]",DLB_Asc2Hex(pBuff, sizeof(DLB_LOGIN)) );
			if(memcmp(psLogin->oilno, pOilConfig->oilno, 4) == 0){
				psLogin->price[0] = 4;
				memcpy(&psLogin->price[1], pOilConfig->price, 3);
			}
			pBuff += sizeof(DLB_LOGIN);		
		}
		//RunLog("一个油价配置完毕, 配置后数据[%s]", \
		//	k, DLB_Asc2Hex(sendBuffer, 18+cfgLength));		
	}


	//读取液位仪配置信息
	msgLength +=cfgLength;
	be_tlgcfgLength = msgLength + 9;  //加液位仪数据包长度4个字节，标志和长度5个字节
	tlgBuff = &sendBuffer[be_tlgcfgLength];
	fd = open( DLB_CONFIG_TLG_FILE, O_RDONLY );
	if(fd <=0){
		RunLog("  打开FCC加油机配置文件出错.");
		exit(0);
	}
	i=0;//用作液位仪探棒数统计
	while (1) {
		n = read( fd, buff, sizeof(DLB_TLG_LOGIN) );
		if( n == 0 ) {	//读到了文件尾
			close(fd);
			break;
		} else if( n < 0 ) {
			RunLog( "读取 dsp.cfg 配置文件出错" );
			close(fd);
			return -1;
		}
		//RunLog("第[%d]次读取到dsp.cfg 数据[%d]个字符\n",i+1, n);

		if( n != sizeof(DLB_TLG_LOGIN) ) {
			RunLog( "dsp.cfg 配置信息有误" );
			close( fd );
			return -1;
		}
		ptLogin = (TLG_LOGIN *)&tlgBuff[0];
		tlgConfig = (DLB_TLG_LOGIN*) (&buff[0]);
		//RunLog("油枪数据[%s]",DLB_Asc2Hex(buff, n) );
		ptLogin->tpno= tlgConfig->tpno;
		ptLogin->oilcanLe[0]= 0x06;
		memcpy(&ptLogin->oilcanLe[1], tlgConfig->oilcanLe, 4);
		ptLogin->oilcanVo[0]= 0x06;
		memcpy(&ptLogin->oilcanVo[1], tlgConfig->oilcanVo, 4);
		ptLogin->highAlarm[0]= 0x06;
		memcpy(&ptLogin->highAlarm[1], tlgConfig->highAlarm, 4);
		ptLogin->lowAlarm[0]= 0x06;
		memcpy(&ptLogin->lowAlarm[1], tlgConfig->lowAlarm, 4);
		ptLogin->oilNo[0] = (((tlgConfig->oilNo[1] & 0xf0)>> 4) *10 + (tlgConfig->oilNo[1] & 0x0f));
		RunLog("ptLogin->oilNo is %02x", ptLogin->oilNo[0]);
		memcpy(&ptLogin->oilNo[1], &tlgConfig->oilNo[2], 2);
		
		RunLog("油罐为[%d]的油品为[%02x%02x%02x]", ptLogin->tpno, tlgConfig->oilNo[1],tlgConfig->oilNo[2],tlgConfig->oilNo[3]);
		memcpy((char *)&(gpDLB_Shm->tank_pr_no[ptLogin->tpno][1]), ptLogin->oilNo, 3);
		tlgBuff += sizeof(TLG_LOGIN);
		tlgcfgLength +=sizeof(TLG_LOGIN);
		tank_cnt = i++;  // gun number ++		
	}		
	g = i;//记录油罐个数
	RunLog("共有油罐[%d],油罐待注册数据[%s]", g, DLB_Asc2Hex(&sendBuffer[be_tlgcfgLength], tlgcfgLength) );

	//头部数据设置
	msgLength +=tlgcfgLength;
	msgLength+=4; //加液位仪数据包长度4个字节
//	RunLog(" 头部数据设置: 总数据长[%d],内容长[%d],液位仪[%d],液位仪前[%d]",\
//		              msgLength, cfgLength,tlgcfgLength,be_tlgcfgLength);
	//mcpy(&sendBuffer[1], (char *)&msgLength,4);
	//mcpy(&sendBuffer[14], (char *)&cfgLength,4);
	sIPName = (unsigned char *)&msgLength;//暂时借用sIPName
	sendBuffer[1] = sIPName[3];
	sendBuffer[2] = sIPName[2];
	sendBuffer[3] = sIPName[1];
	sendBuffer[4] = sIPName[0];
	msgLength+=5;//总长度加前面的5个字节: 标志和长度
	sIPName = (unsigned char *)&cfgLength;//暂时借用sIPName
	sendBuffer[14] = sIPName[3];
	sendBuffer[15] = sIPName[2];
	sendBuffer[16] = sIPName[1];
	sendBuffer[17] = sIPName[0];

	sIPName = (unsigned char *)&tlgcfgLength;//暂时借用sIPName
	be_tlgcfgLength--;
	sendBuffer[be_tlgcfgLength] = sIPName[0];
	be_tlgcfgLength--;
	sendBuffer[be_tlgcfgLength] = sIPName[1];
	be_tlgcfgLength--;
	sendBuffer[be_tlgcfgLength] = sIPName[2];
	be_tlgcfgLength--;
	sendBuffer[be_tlgcfgLength] = sIPName[3];
	
	
	localIP = DLB_GetLocalIP();
	sendBuffer[0] = 0xfe;
	//站点编号
	memcpy(&sendBuffer[5], gpDLB_Shm->css._STATION_NO_BCD,5);
	
	//FCC的 IP地址
	memcpy(&sendBuffer[10],(char *)&localIP,4);

	//i=k;
	//向二配中心上传数据
	RunLog(" <Login> 向二配中心上传数据[%s]",DLB_Asc2Hex(sendBuffer, msgLength) );
	sIPName =gpDLB_Shm->css._SERVER_IP;//inet_ntoa(  *((struct in_addr *)(&gpIfsf_shm->recv_hb[i].host_ip))  );

	
	while(1){
		sockfd = DLB_TcpConnect(sIPName,DLB_SERVER_TCP_PORT_4LOGIN , timeout);//(timeout+1)/2   //sPortName
		if (sockfd >0 ){
			sleep(5);
			ret =DLB_TcpSendDataInTime(sockfd, sendBuffer, msgLength, timeout);			//Act_V(CD_SOCK_INDEX);			
			if (ret < 0 ){
				RunLog("<Login>向中心发送数据失败,IP[%s],port[%d]",sIPName,DLB_SERVER_TCP_PORT_4LOGIN);
				close(sockfd);
				sleep(60);//sleep(10000);
				continue;
			}else{				
				break;
			}
		} else {
			RunLog("<Login>与中心连接失败,IP[%s],port[%d]",sIPName,DLB_SERVER_TCP_PORT_4LOGIN);
			close(sockfd);
			sleep(60);//sleep(10000); 
			continue;
			//return -1;
		}
	}
	 //从二配中心接收液位仪时间间隔数据
	RunLog("<Login>从二配中心接收液位仪时间间隔数据");
	bzero(sendBuffer,sizeof(sendBuffer));
	k = timeout;
	timeout =2;
	for(i=0;i<tryn;i++){
		msgLength = 3;
		sleep(2);
		ret =DLB_TcpRecvDataInTime(sockfd, sendBuffer, &msgLength, timeout);			//Act_V(CD_SOCK_INDEX);			
		if (ret < 0 ){
			RunLog("<Login>发送注册数据成功，取返回信息失败!");
			close(sockfd);
			sleep(2);
			continue;
		}
			
		break;
	}
	timeout = k;
      if(sendBuffer[0] != 0xfe){
	  	RunLog("<Login>发送注册数据成功，取返回信息失败!,[%02x%02x%02x]",sendBuffer[0],sendBuffer[1],sendBuffer[2]);
		close(sockfd);
		return -1;
	}
	//RunLog("gpDLB_Shm->css._TLG_SEND_INTERVAL赋值[%02x%02x]",sendBuffer[1],sendBuffer[2]);
      gpDLB_Shm->css._TLG_SEND_INTERVAL =  \
	  	( (sendBuffer[1]&0xf0) >> 4 )*1000+(sendBuffer[1]&0x0f)*100+((sendBuffer[2]&0xf0)>>4)*10+sendBuffer[2]&0x0f;
	RunLog("注册成功收到的液位仪时间间隔为[%d]", gpDLB_Shm->css._TLG_SEND_INTERVAL);

	close(sockfd);
	return 0;
}

//注册失败不发送任何数据,此函数用做注册失败进程挂起
void Login_Sleep(){

	if ( gpDLB_Shm->login_ok != OK_LOGIN ){
		while(1){

			sleep(10);
			if ( gpDLB_Shm->login_ok == OK_LOGIN )
				break;
		}
	}
	
}

#define DLB_MAX_CHANNEL_CNT 64
#define DLB_MAX_TR_NB_PER_DISPENSER 256
#define READ_TIMES 1000
#define READ_DATA_TIMES 2
#define MAX_NODE_NUM 128
#define MAX_FP_NUM 4

 int  DLB_ReadTransData( unsigned char *resultBuff){
	unsigned char onlineFlag[2] ={0xaa,0x55};
	static unsigned char sendBuffer[512] ;//,buff[16],*pBuff
	unsigned char readBuffer[512] ;//,buff[16],*pBuff
	int  ret, i, j, dataLen,readLength, onlineFlagLen   = 2;
	extern int errno;
	DLB_FP_TR_DATA  *pFp_tr_data;
	static unsigned char Fulling_Flag[MAX_NODE_NUM][MAX_FP_NUM],node,fp;
	unsigned char lnz_allowed = 0;
	int zero = 0;
	char tmpbuf[512];
	
	if( gpDLB_Shm == NULL ){ //(ppGunShm == NULL)||()
		RunLog( "错误,DLB的共享内存为空" );
		return -1;
	}
	
	if(gpDLB_Shm->css.fd_fifo<=0){
		gpDLB_Shm->css.fd_fifo = open(DLB_FIFO, O_RDONLY | O_NONBLOCK);
		if(gpDLB_Shm->css.fd_fifo <= 0){
			RunLog("FIFO只读打开出错.");
			close(gpDLB_Shm->css.fd_fifo);
			return -1;
		}else{
			RunLog( "gpDLB_Shm->css.fd_fifo读打开完毕");
		}		
	}
	//bzero(Fulling_Flag, MAX_NODE_NUM * MAX_FP_NUM);
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
				//if( (ret !=4 )|| (dataLen <=0) )
				//	RunLog("FIFO读取出的数据的长度值出错.ret[%d],dataLen[%d]",ret,dataLen);
				if(readLength != dataLen)
					RunLog("FIFO读取出的数据的长度出错.readLength[%d],dataLen[%d]",readLength,dataLen);
				i = 0;
			}
			//RunLog( "读gpDLB_Shm->css.fd_fifo开始");
			dataLen = -1;
			ret = read(gpDLB_Shm->css.fd_fifo, (char *)&dataLen, 4);//dataLen = 
			if( (ret !=4 )|| (dataLen <=0) ){
				dataLen = -1;
				continue;
			}
			if(dataLen>=512){
				RunLog("FIFO读取出的数据的长度出错.readLength[%d],dataLen[%d]",readLength,dataLen);
				continue;

			}
			int k1=0,n1=0;//ReadByLength(int fd, void * vptr, size_t n)
			for(j=0;j< READ_DATA_TIMES;j++){
				while(1){
					n1 = -1;
					n1 = read(gpDLB_Shm->css.fd_fifo, &readBuffer[k1], dataLen-k1);
					//RunLog( "读gpDLB_Shm->css.fd_fifo结束[%02x%02x%02x]",readBuffer[0],readBuffer[1],readBuffer[2] );
					
					if(n1<dataLen-k1&&n1 > 0){		
						k1 = n1;
						continue;
					}else if(n1==dataLen-k1){
						readLength=dataLen;
						break;
					}
				}	
				if(readLength != dataLen){
					RunLog( "读gpDLB_Shm->css.fd_fifo错误,readLength[%d], dataLen[%d],errno[%d]",readLength ,dataLen,errno);
					readLength = -1;
					continue;
				}else{
					RunLog("FIFO读取出了数据.readLength[%d],dataLen[%d]",readLength,dataLen);
					break;
				}
			}
			if((readLength == dataLen)&&(readLength != -1)){
				if(readLength<=512){
					//RunLog( "读gpDLB_Shm->css.fd_fifo结束,开始复制数据");
					//memcpy(gpDLB_Shm->css.readBuffer,readBuffer,readLength);
					RunLog( "读gpDLB_Shm->css.fd_fifo正常结束[%s]",DLB_Asc2Hex(readBuffer,readLength) );
				}else{
					RunLog( "读gpDLB_Shm->css.fd_fifo失败,长度大于512[%s]",DLB_Asc2Hex(readBuffer,readLength) );
					}
				
				break;
			}
		}	
		
	/*	
		//处理数据		
		ret = -1;
		if (readBuffer[0] == 0x02 && readBuffer[1] == 0x01) {
			gpDLB_Shm->css.msgLength = readLength;
			bzero(tmpbuf, sizeof(tmpbuf));
			memcpy(tmpbuf, (char *)&(gpDLB_Shm->css.msgLength), sizeof(int));
			memcpy(&tmpbuf[4], readBuffer, gpDLB_Shm->css.msgLength);
			ret = write(gSendFifoFd, tmpbuf, gpDLB_Shm->css.msgLength + sizeof(int));
			RunLog("write to gSendFifoFd ret = %d", ret);
		}
		else if (readBuffer[0] == 0xca) {
			DLB_SendRealtimeData(&readBuffer[1]);
		}
	*/	
#if 1
		RunLog("DLB_Send_Data --- $$$_REALTIME_FLAG = %d", gpDLB_Shm->css._REALTIME_FLAG);
		if( 1 == gpDLB_Shm->css._REALTIME_FLAG ){
			 if( (readBuffer[7] == 0x0e ) &&(readBuffer[10] == 100 )  ){//Realtime Data:state,&&(readBuffer[14] == 4 ),(readBuffer[7] == 0x0e )
				RunLog("FIFO读处理数据:状态数据");
				//ret = MakeStateData(resultBuff);//gpDLB_Shm->css.readBuffer
				gpDLB_Shm->css.msgLength =11 ;
				sendBuffer[0] = (unsigned char) 0x55;
				sendBuffer[1] = gpDLB_Shm->css.msgLength-2;
				memcpy(&sendBuffer[2], gpDLB_Shm->css._STATION_NO_BCD,5);
				sendBuffer[7] = readBuffer[3];
				node = readBuffer[3];
				sendBuffer[8] = readBuffer[9]-0x20;
				fp = readBuffer[9];
				switch (readBuffer[17]) {
					case 0x00:
						lnz_allowed = 0x00; break;//此面所有枪空闲
					case 0x01:
						lnz_allowed = 0x01; break;
					case 0x02:
						lnz_allowed = 0x02; break;
					case 0x04:
						lnz_allowed = 0x03; break;
					case 0x08:
						lnz_allowed = 0x04; break;
					case 0x10:
						lnz_allowed = 0x05; break;
					case 0x20:
						lnz_allowed = 0x06; break;
					case 0x40:
						lnz_allowed = 0x07; break;
					case 0x80:
						lnz_allowed = 0x08; break;
					default:
						RunLog("Node[%d]FP[%d]未允许任何有效油枪加油readBuffer[17]:[%02x]",
							node, sendBuffer[8],readBuffer[17]);
				}
				sendBuffer[9] = lnz_allowed;//抢号
				sendBuffer[10] =readBuffer[14];
				if( readBuffer[14] > 3 ) //提枪以上的状态
				 {         Fulling_Flag[node -1][fp - 0x21] =readBuffer[16];//非挂枪状态赋值抢号
  				           //RunLog("Fulling_Flag[%d -1][%d] = %d",node,fp-0x21,readBuffer[16]);
				     }
				//RunLog("构造状态数据完毕,开始复制");
				//resultBuff =sendBuffer;//memcpy(gpDLB_Shm->css.sendBuffer,sendBuffer,gpDLB_Shm->css.msgLength);
				ret = 2;
				break;
			}else if( (readBuffer[0] == 2 )&&(readBuffer[2] == 1 )&&(readBuffer[10] == 102 ) ){//Realtime Data:transaction				
				RunLog("FIFO读处理数据: 实时交易数据REALTIME_FLAG[%d]", gpDLB_Shm->css._REALTIME_FLAG);
				gpDLB_Shm->css.msgLength =20 ;
				sendBuffer[0] = (unsigned char) 0xff;
				sendBuffer[1] = gpDLB_Shm->css.msgLength-2;
				memcpy(&sendBuffer[2], gpDLB_Shm->css._STATION_NO_BCD,5);
				sendBuffer[7] = readBuffer[3];
				sendBuffer[8] =readBuffer[9]-0x20;
				switch (readBuffer[28]) {
					case 0x00:
						lnz_allowed = 0x00; break;//此面所有枪空闲
					case 0x01:
						lnz_allowed = 0x01; break;
					case 0x02:
						lnz_allowed = 0x02; break;
					case 0x04:
						lnz_allowed = 0x03; break;
					case 0x08:
						lnz_allowed = 0x04; break;
					case 0x10:
						lnz_allowed = 0x05; break;
					case 0x20:
						lnz_allowed = 0x06; break;
					case 0x40:
						lnz_allowed = 0x07; break;
					case 0x80:
						lnz_allowed = 0x08; break;
					default:
						RunLog("Node[%d]FP[%d]未允许任何有效油枪加油readBuffer[28]:[%02x]",
							node, sendBuffer[8],readBuffer[28]);
				}
				sendBuffer[9] = lnz_allowed;//枪号
				memcpy(&(sendBuffer[10]) , &readBuffer[14],5);
				memcpy(&(sendBuffer[15]) ,&readBuffer[21],5);
				//memcpy(&(sendBuffer[9]) , &readBuffer[14],5);
				//memcpy(&(sendBuffer[14]) ,&readBuffer[21],5);
				//sendBuffer[19] = readBuffer[28];//枪号
				//RunLog("FIFO读处理数据: 实时交易数据完毕,开始复制");
				//处理完毕，返回
				//resultBuff =sendBuffer; //memcpy(gpDLB_Shm->css.sendBuffer,sendBuffer,gpDLB_Shm->css.msgLength);
				ret = 2;
				//memcpy(sendBuff, gpDLB_Shm->css.sendBuffer,gpDLB_Shm->css.msgLength);
				break;
			}
		}
		else if( 0 == gpDLB_Shm->css._REALTIME_FLAG ){
			if( (readBuffer[7] == 0x0e ) &&(readBuffer[10] == 100 )  ){
				RunLog("FIFO读处理数据:  状态数据");
			}else if( (readBuffer[0] == 2 )&&(readBuffer[2] == 1 )&&(readBuffer[10] == 102 ) ){
				RunLog("FIFO读处理数据:实时交易数据REALTIME_FLAG[%d]", gpDLB_Shm->css._REALTIME_FLAG);
			}else{RunLog("FIFO读处理数据:  其它数据");}
			ret = 0;
			continue;
		}else {			
			RunLog("FIFO读处理数据:  其它数据");
			ret = 0;
			continue;
		}
#endif
	}
	memcpy(resultBuff, sendBuffer, gpDLB_Shm->css.msgLength);		
	//RunLog( "FIFO读处理数据完毕[%s]",DLB_Asc2Hex(resultBuff, gpDLB_Shm->css.msgLength) );

	bzero(tmpbuf, sizeof(tmpbuf));
	memcpy(tmpbuf, (char *)&(gpDLB_Shm->css.msgLength), sizeof(int));
	memcpy(&tmpbuf[4], sendBuffer, gpDLB_Shm->css.msgLength);
	ret = write(gSendFifoFd, tmpbuf, gpDLB_Shm->css.msgLength + sizeof(int));
	RunLog("DLB_ReadTransData $$$$$$ write to gSendFifoFd ret = %d", ret);

	return ret;
 }

 int DLB_SendRealtimeData( unsigned char *sendBuffer){
	int i, j,ret,tryNum = 5;
	 struct sockaddr_in  DLB_SERVER_UDP_ADDR, clientUdpAddr;
	 int reuse = 1;
	 
	// if( gpDLB_Shm->css._SOCKUDP<=0){
		 for(i=0;i<tryNum;i++){
			bzero(&DLB_SERVER_UDP_ADDR,sizeof(DLB_SERVER_UDP_ADDR));
			DLB_SERVER_UDP_ADDR.sin_family=AF_INET;
			DLB_SERVER_UDP_ADDR.sin_addr.s_addr=inet_addr(gpDLB_Shm->css._SERVER_IP);
			DLB_SERVER_UDP_ADDR.sin_port = htons(DLB_SERVER_UDP_PORT_4REALTIME);
			gpDLB_Shm->css._SOCKUDP = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );

			if(setsockopt(gpDLB_Shm->css._SOCKUDP, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)) < 0)
			{
				RunLog( "setsockopt错误号:%d", errno );
				return -1;
			}
			clientUdpAddr.sin_family = AF_INET;
			clientUdpAddr.sin_addr.s_addr = htonl(INADDR_ANY);
			clientUdpAddr.sin_port = htons(DLB_SERVER_UDP_PORT_4REALTIME);
			if( bind( gpDLB_Shm->css._SOCKUDP, (SA *)&clientUdpAddr, sizeof( clientUdpAddr ) ) < 0 )
			{
				RunLog( "DLB_SendRealtimeData错误号:%d", errno );
				return -1;
			}	
			
			if (  gpDLB_Shm->css._SOCKUDP<=0 ){//connect(_SOCKUDP, (struct sockaddr *)_SERVER_UDP_ADDR, sizeof(_SERVER_UDP_ADDR))

				RunLog("UDP连接建立不成功,_setRealTime 失败");
				//close(_SOCKUDP); 
				gpDLB_Shm->css._SOCKUDP = -1;	
				//exit(0);
			}else{
				for(j=0;j<tryNum;j++){
					ret = sendto(gpDLB_Shm->css._SOCKUDP, sendBuffer, 10, 0, (struct sockaddr *)&DLB_SERVER_UDP_ADDR, sizeof(struct sockaddr));//
					if (ret < 0) {				
						RunLog("实时数据发送失败,error:[%s]", strerror(errno));	
					}else{
						RunLog( "心跳数据发送成功" );
						break;
					}
				}
				if( gpDLB_Shm->css._SOCKUDP >= 0){
					close(gpDLB_Shm->css._SOCKUDP);
					gpDLB_Shm->css._SOCKUDP = -1;	
				}
				break;
			}
		}
		 if( (i>= tryNum)&&(j>= tryNum))RunLog("实时数据发送[%d]次失败",tryNum);
 }

 void DLB_AcceptServerCmd( ){
	int sock;
	int newsock;
	static int tmp_sock;
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
		
	
	if( gpDLB_Shm == NULL ){
		RunLog( "错误,DLB的共享内存为空,接收服务进程退出." );
		exit(1);
	}
	gpDLB_Shm->css._REALTIME_FLAG = 1;  // 1 ,默认不发送实时数据
	
	sock = DLB_TcpServInit(DLB_DLB_TCP_PORT_4REALTIME);
	if ( sock < 0 ) {
		RunLog( "实时进程初始化socket失败.port=[%s]\n", DLB_DLB_TCP_PORT_4REALTIME );
		//Free( cmd );
		return  ;
	}
	//fcntl(sock, F_SETFL, O_NONBLOCK);   // 非阻塞模式
	
	newsock  = -1;
	tmp_sock = -1;
	while (1) {	
		if (errcnt > 32) {
			RunLog("out of ERR_CNT, close the tcp connection (DL.C --> FCC.S)");
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
				RunLog("TcpAccept new one, newsock: %d ", newsock);
				break;
			}
		}
#endif

#if 1
		tmp_sock = DLB_TcpAccept(sock);//, NULL
		RunLog("DLB_AcceptServerCmd连接,soket号[%d]", tmp_sock);
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

		errcnt = 0;
		msg_lg = sizeof(buffer);
		memset( buffer, 0, sizeof(buffer) );
		while(1){
			if ( errcnt > tryNum ) {
				/*错误处理*/
				RunLog( "接收数据[%d]次都失败", tryNum );
				break; 
			}
			ret = DLB_TcpRecvDataInTime(newsock, buffer, &msg_lg, timeout );//TIME_INFINITE	
			if (ret < 0) {
				if (errno == 4) {  // time out
					RunLog("实时进程超时,接收到后台命令 buffer[%s]\n" ,  DLB_Asc2Hex(buffer, 32) );//
					continue;
				} else if (errno == 11)  {  // maybe CD quit or restart, && (gpIfsf_shm->need_reconnect == 0))
					//gpIfsf_shm->need_reconnect = 1;
					RunLog("Client need to reconnect to Server");
				}	
				RunLog( "实时进程接收数据失败,errno:[%d][%s],buffer: %s", errno, strerror(errno), \
					DLB_Asc2Hex(buffer, 2) );
				errcnt++;
				sleep(1);
				continue;
			} else {
				RunLog( "<<<实时进程收到命令: [%s(%d)]", DLB_Asc2Hex(buffer, msg_lg ), msg_lg);				
				if ((0xee != buffer[0])&&(0x11 != buffer[0])&&(0xef != buffer[0])&&(0xea != buffer[0])&&(0xec != buffer[0])){ //received data must be error!!
					RunLog( "received data error" );
					errcnt++;
					sleep(1);
					continue;
					/*//回复错误
					sBuffer[0] = 0xe1;
					memcpy(&sBuffer[1],gpDLB_Shm->css._STATION_NO_BCD,5);
					ret = DLB_TcpSendDataInTime(newsock, sBuffer, 6, timeout);
					if(ret <=0){				
						RunLog("实时进程回复数据 失败");
					}
					continue;*/
				}

				if (0 == msg_lg){ //receivd data must be error!!
					RunLog( "received size is  0 error" );
					errcnt++;
					//回复错误
					sBuffer[0] = 0xe1;
					memcpy(&sBuffer[1],gpDLB_Shm->css._STATION_NO_BCD,5);
					ret = DLB_TcpSendDataInTime(newsock, sBuffer, 6, timeout);
					if(ret <=0){				
						RunLog("实时进程回复数据 失败");
					}
					
					sleep(1);
					continue;
				}

				
				RunLog("msg_lg is %d", msg_lg);
				//处理后台命令.
				if (msg_lg== 2 || msg_lg == 3){ 
					if(0xee == buffer[0]){
						gpDLB_Shm->css._REALTIME_FLAG = 1;
						RunLog("开启实时数据上传");
						sBuffer[0] = buffer[0];
						memcpy(&sBuffer[1],gpDLB_Shm->css._STATION_NO_BCD,5);
						ret = DLB_TcpSendDataInTime(newsock, sBuffer, 6, timeout);
						RunLog("ret = %d sBuffer is %s", ret, Asc2Hex(sBuffer, 6));
						if(ret <0){				
							RunLog("实时进程回复数据 失败");
							//continue;
						}
						errcnt = 0;
						break;
					}else if(0x11 == buffer[0]){
						gpDLB_Shm->css._REALTIME_FLAG = 0;
						RunLog("关闭实时数据上传");
						sBuffer[0] = buffer[0];
						memcpy(&sBuffer[1],gpDLB_Shm->css._STATION_NO_BCD,5);
						ret = DLB_TcpSendDataInTime(newsock, sBuffer, 6, timeout);
						if(ret <0){				
							RunLog("实时进程回复数据 失败");
							//continue;
						}
						errcnt = 0;
						break;
					}else if(0xef == buffer[0]){
					
						RunLog("设置液位仪数据传输间隔");
						gpDLB_Shm->css._TLG_SEND_INTERVAL = \
							((buffer[1]&0xf0) >> 4)* 1000 + (buffer[1]&0x0f) * 100 + ((buffer[2]&0xf0) >> 4)* 10 +buffer[2]&0x0f;
						RunLog("_TLG_SEND_INTERVAL is %d", gpDLB_Shm->css._TLG_SEND_INTERVAL);
						sBuffer[0] = buffer[0];
						sBuffer[1] = 0xff;
						ret = DLB_TcpSendDataInTime(newsock, sBuffer, 2, timeout);
						if(ret <0){				
							RunLog("实时进程回复数据 失败");
							//continue;
						}
						errcnt = 0;
					}else if(0xea == buffer[0]){
						RunLog("设置漏油检测数据传输间隔");
						//RunLog("buffer  2 is %02x %d  %d %d %d", buffer[2], ((buffer[1]&0xf0) >> 4)* 1000 ,  (buffer[1]&0x0f) * 100, ((buffer[2]&0xf0) >> 4)* 10, (buffer[2]&0x0f));
						gpDLB_Shm->css.__LEAK_SEND_INTERVAL = ((buffer[1]&0xf0) >> 4)* 1000 + (buffer[1]&0x0f) * 100 + ((buffer[2]&0xf0) >> 4)* 10 +(buffer[2]&0x0f);
						RunLog("__LEAK_SEND_INTERVAL is %d", gpDLB_Shm->css.__LEAK_SEND_INTERVAL);
						sBuffer[0] = buffer[0];
						sBuffer[1] = 0xff;
						ret = DLB_TcpSendDataInTime(newsock, sBuffer, 2, timeout);
						if(ret <0){				
							RunLog("实时进程回复数据 失败");
							//continue;
						}
						int fd = open("/home/App/ifsf/etc/timeval1.cfg", O_RDWR);
						if (fd == -1)
							RunLog("timeval1文件还未创建");
						else{
							write(fd, &(gpDLB_Shm->css.__LEAK_SEND_INTERVAL), 4);
							close(fd);
						}
						errcnt = 0;

 					}else if(0xec == buffer[0]){
						RunLog("设置漏油检测数据传输间隔");
						gpDLB_Shm->css.__AFTER_TIME = ((buffer[1]&0xf0) >> 4)* 1000 + (buffer[1]&0x0f) * 100 + ((buffer[2]&0xf0) >> 4)* 10 +(buffer[2]&0x0f);
						RunLog("__AFTER_TIME is %d", gpDLB_Shm->css.__AFTER_TIME);
						sBuffer[0] = buffer[0];
						sBuffer[1] = 0xff;
						ret = DLB_TcpSendDataInTime(newsock, sBuffer, 2, timeout);
						if(ret <0){				
							RunLog("实时进程回复数据 失败");
							//continue;
						}
						int fd = open("/home/App/ifsf/etc/timeval2.cfg", O_RDWR);
						if (fd == -1)
							RunLog("timeval2文件还未创建");
						else{
							write(fd, &(gpDLB_Shm->css.__AFTER_TIME), 4);
							close(fd);
						}
						errcnt = 0;

 					}else{
						RunLog( "解析数据失败,收到的数据值不正常" );
						sBuffer[1]=0x00;
						ret = DLB_TcpSendDataInTime(newsock, sBuffer, 2, timeout);
						if(ret <0){				
							RunLog("实时进程回复数据 失败");
							//continue;
						}
						errcnt = 0;
 					}
 				}  else{
					RunLog( "解析数据失败,收到的数据长度不正常" );
					sBuffer[1]=0x00;
					ret = DLB_TcpSendDataInTime(newsock, sBuffer, 2, timeout);
					if(ret <0){				
						RunLog("实时进程回复数据 失败");
						//continue;
					}
					errcnt = 0;
				}			
				break;
				//sleep(10);			
			}
		}
		close(newsock);
		close(tmp_sock);
		newsock = -1;
		if ( errcnt > 40 ) {
			/*错误处理*/
			RunLog( "初始化socket 失败.port=[%d]", DLB_DLB_TCP_PORT_4REALTIME );
			break; 
		}
	}

	RunLog("DLB_AcceptServerCmd  Stop ......");
	close(sock);

	return  ;
}
//-
//读写TP_DATA函数. 注意要构造的信息数据的类型,000读,001回答,010写,011主动需应答,100主动无需应答,111应答
//recvBuffer为收到的data,sendBuffer为要发送的data,msg_lg为生成的总长度.
//-
//-
//读取TP_DAT.TLG_DAT地址为1.构造的信息数据的类型为001回答
//-
int DLB_ReadTP_DAT(const IFSF_SHM*gpIfsf_shm, const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg){
	int i, recv_lg, tmp_lg = 0;
	int tp;
	int tp_stat;
	TLG_DATA *pTlg_data;
	TLG_DAT *pTlg_dat;
	TP_DAT  *pTp_dat;
	unsigned char node;
	unsigned char buffer[256];
	
	if( NULL == gpIfsf_shm ){		
		printf("共享内存值错误,液位仪ReadTP_DAT失败");
		return -1;
	}
	if( (NULL == recvBuffer) || (NULL == sendBuffer) || (NULL == msg_lg)  ){		
		printf("参数错误,液位仪ReadTP_DAT 失败");
		return -1;
	}
	
	node= recvBuffer[1];
	tp = recvBuffer[9] - 0x21;
	pTlg_data = (TLG_DATA*)( &(gpIfsf_shm->tlg_data) );
	pTlg_dat = (TLG_DAT*)( &(pTlg_data->tlg_dat) );
	pTp_dat =  (TP_DAT *)( &(pTlg_data->tp_dat[tp]) );
	tp_stat = pTp_dat->TP_Status;
	
	bzero(buffer, sizeof(buffer));
	if(  '\x09' != recvBuffer[0]  ){
		printf("参数错误,不是液位仪的数据,液位仪ReadTP_DAT 失败");
		return -1;
	}
	
	if( (node <= 0) || (node > 127 )  ){//||(node != gpIfsf_shm->node_tlg)
		printf("液位仪节点值错误,液位仪ReadTP_DAT 失败");
		return -1;
	}
	
	
	buffer[0] = recvBuffer[2];
	buffer[1] = recvBuffer[3];
	buffer[2] = recvBuffer[0];
	buffer[3] = recvBuffer[1];
	buffer[4] = '\0'; //recvBuffer[4];
	buffer[5] = ( (recvBuffer[5] &0x1f)  |0x20); //answer
	recv_lg = recvBuffer[6];
	recv_lg = recv_lg * 256 + recvBuffer[7];
	buffer[8] = '\x01'; //db_ad_lg. recvBuffer[8];
	buffer[9] = recvBuffer[9];//  db_ad.//'\x01';
	//buffer[10] = recvBuffer[10]; //It's Data_Id. Data begin.
	tmp_lg = 9;
	for(i=0; i < (recv_lg - 2); i++){
		switch( recvBuffer[10+i] ){
			case T_TP_Manufacturer_Id:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_TP_Manufacturer_Id;
				tmp_lg++;
				buffer[tmp_lg] =3;
				tmp_lg++;
				memcpy( &buffer[tmp_lg] , pTp_dat->TP_Manufacturer_Id, sizeof(pTp_dat->TP_Manufacturer_Id) );
				tmp_lg += sizeof(pTp_dat->TP_Manufacturer_Id) -1;
				break;
			case T_TP_Type: 
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_TP_Type;
				tmp_lg++;
				buffer[tmp_lg] =3;
				tmp_lg++;
				buffer[tmp_lg] = pTp_dat->TP_Type[0];
				tmp_lg++;
				buffer[tmp_lg] = pTp_dat->TP_Type[1];
				tmp_lg++;
				buffer[tmp_lg] = pTp_dat->TP_Type[2];
				break;
			case T_TP_Serial_Nb:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_TP_Serial_Nb;
				tmp_lg++;
				buffer[tmp_lg] ='\x12';
				tmp_lg++;
				memcpy( &buffer[tmp_lg] , pTp_dat->TP_Serial_Nb, sizeof(pTp_dat->TP_Serial_Nb) );
				tmp_lg += sizeof(pTp_dat->TP_Serial_Nb) -1;
				break;
			case T_TP_Model:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_TP_Model;
				tmp_lg++;
				buffer[tmp_lg] =3;
				tmp_lg++;
				buffer[tmp_lg] = pTp_dat->TP_Model[0];
				tmp_lg++;
				buffer[tmp_lg] = pTp_dat->TP_Model[1];
				tmp_lg++;
				buffer[tmp_lg] = pTp_dat->TP_Model[2];
				break;			
			case T_TP_Appl_Software_Ver:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_TP_Appl_Software_Ver;
				tmp_lg++;
				buffer[tmp_lg] ='\x12';
				tmp_lg++;
				memcpy( &buffer[tmp_lg] , pTp_dat->TP_Appl_Software_Ver, sizeof(pTp_dat->TP_Appl_Software_Ver) );
				tmp_lg += sizeof(pTp_dat->TP_Appl_Software_Ver) -1;
				break;
			case T_Prod_Nb:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Prod_Nb;
				tmp_lg++;
				buffer[tmp_lg] =4;
				tmp_lg++;
				buffer[tmp_lg] = pTp_dat->Prod_Nb[0];
				tmp_lg++;
				buffer[tmp_lg] = pTp_dat->Prod_Nb[1];
				tmp_lg++;
				buffer[tmp_lg] = pTp_dat->Prod_Nb[2];
				tmp_lg++;
				buffer[tmp_lg] = pTp_dat->Prod_Nb[3];
				break;
			case T_Prod_Description:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Prod_Description;
				tmp_lg++;
				buffer[tmp_lg] ='\x16';
				tmp_lg++;
				memcpy( &buffer[tmp_lg] , pTp_dat->Prod_Description, sizeof(pTp_dat->Prod_Description) );
				tmp_lg += sizeof(pTp_dat->Prod_Description) -1;
				break;
			case T_Prod_Group_Code:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Prod_Group_Code;
				tmp_lg++;
				buffer[tmp_lg] =1;
				tmp_lg++;
				buffer[tmp_lg] = pTp_dat->Prod_Group_Code;
				break;
			case T_Ref_Density:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Ref_Density;
				tmp_lg++;
				buffer[tmp_lg] =2;
				tmp_lg++;
				buffer[tmp_lg] = pTp_dat->Ref_Density[0];//bin16
				tmp_lg++;
				buffer[tmp_lg] = pTp_dat->Ref_Density[1];
				break;
			case T_Tank_Diameter:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Tank_Diameter;
				tmp_lg++;
				buffer[tmp_lg] =4;
				tmp_lg++;
				memcpy( &buffer[tmp_lg], pTp_dat->Tank_Diameter, sizeof(pTp_dat->Tank_Diameter) );
				tmp_lg += sizeof(pTp_dat->Tank_Diameter) -1;
				break;
			case T_Shell_Capacity:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Shell_Capacity;
				tmp_lg++;
				buffer[tmp_lg] =7;
				tmp_lg++;
				memcpy( &buffer[tmp_lg], pTp_dat->Shell_Capacity, sizeof(pTp_dat->Shell_Capacity) );
				tmp_lg += sizeof(pTp_dat->Shell_Capacity) -1;
				break;
			case T_Max_Safe_Fill_Capacity:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Max_Safe_Fill_Capacity;
				tmp_lg++;
				buffer[tmp_lg] =7;
				tmp_lg++;
				memcpy( &buffer[tmp_lg], pTp_dat->Max_Safe_Fill_Capacity, sizeof(pTp_dat->Max_Safe_Fill_Capacity) );
				tmp_lg += sizeof(pTp_dat->Max_Safe_Fill_Capacity) -1;
				break;
			case T_Low_Capacity:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Low_Capacity;
				tmp_lg++;
				buffer[tmp_lg] =7;
				tmp_lg++;
				memcpy( &buffer[tmp_lg], pTp_dat->Low_Capacity, sizeof(pTp_dat->Low_Capacity) );
				tmp_lg += sizeof(pTp_dat->Low_Capacity) -1;
				break;
			case T_Min_Operating_Capacity:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Min_Operating_Capacity;
				tmp_lg++;
				buffer[tmp_lg] =7;
				tmp_lg++;
				memcpy( &buffer[tmp_lg], pTp_dat->Min_Operating_Capacity, sizeof(pTp_dat->Min_Operating_Capacity) );
				tmp_lg += sizeof(pTp_dat->Min_Operating_Capacity) -1;
				break;
			case T_HiHi_Level_Setpoint:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_HiHi_Level_Setpoint;
				tmp_lg++;
				buffer[tmp_lg] =4;
				tmp_lg++;
				memcpy( &buffer[tmp_lg], pTp_dat->HiHi_Level_Setpoint, sizeof(pTp_dat->HiHi_Level_Setpoint) );
				tmp_lg += sizeof(pTp_dat->HiHi_Level_Setpoint) -1;
				break;
			case T_Hi_Level_Setpoint:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Hi_Level_Setpoint;
				tmp_lg++;
				buffer[tmp_lg] =4;
				tmp_lg++;
				memcpy( &buffer[tmp_lg], pTp_dat->Hi_Level_Setpoint, sizeof(pTp_dat->Hi_Level_Setpoint) );
				tmp_lg += sizeof(pTp_dat->Hi_Level_Setpoint) -1;
				break;
			case T_Lo_Level_Setpoint:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Lo_Level_Setpoint;
				tmp_lg++;
				buffer[tmp_lg] =4;
				tmp_lg++;
				memcpy( &buffer[tmp_lg], pTp_dat->Lo_Level_Setpoint, sizeof(pTp_dat->Lo_Level_Setpoint) );
				tmp_lg += sizeof(pTp_dat->Lo_Level_Setpoint) -1;
				break;
			case T_LoLo_Level_Setpoint:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_LoLo_Level_Setpoint;
				tmp_lg++;
				buffer[tmp_lg] =4;
				tmp_lg++;
				memcpy( &buffer[tmp_lg], pTp_dat->LoLo_Level_Setpoint, sizeof(pTp_dat->LoLo_Level_Setpoint) );
				tmp_lg += sizeof(pTp_dat->LoLo_Level_Setpoint) -1;
				break;
			case T_Hi_Water_Setpoint:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Hi_Water_Setpoint;
				tmp_lg++;
				buffer[tmp_lg] =4;
				tmp_lg++;
				memcpy( &buffer[tmp_lg], pTp_dat->Hi_Water_Setpoint, sizeof(pTp_dat->Hi_Water_Setpoint) );
				tmp_lg += sizeof(pTp_dat->Hi_Water_Setpoint) -1;
				break;
			case T_Water_Detection_Thresh:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Water_Detection_Thresh;
				tmp_lg++;
				buffer[tmp_lg] =4;
				tmp_lg++;
				memcpy( &buffer[tmp_lg], pTp_dat->Water_Detection_Thresh, sizeof(pTp_dat->Water_Detection_Thresh) );
				tmp_lg += sizeof(pTp_dat->Water_Detection_Thresh) -1;
				break;
			case T_Tank_Tilt_Offset:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Tank_Tilt_Offset;
				tmp_lg++;
				buffer[tmp_lg] =4;
				tmp_lg++;
				memcpy( &buffer[tmp_lg], pTp_dat->Tank_Tilt_Offset, sizeof(pTp_dat->Tank_Tilt_Offset) );
				tmp_lg += sizeof(pTp_dat->Tank_Tilt_Offset) -1;
				break;
			case T_Tank_Manifold_Partners:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Tank_Manifold_Partners;
				tmp_lg++;
				buffer[tmp_lg] =4;
				tmp_lg++;
				memcpy( &buffer[tmp_lg], pTp_dat->Tank_Manifold_Partners, sizeof(pTp_dat->Tank_Manifold_Partners) );
				tmp_lg += sizeof(pTp_dat->Tank_Manifold_Partners) -1;
				break;
			case T_TP_Measurement_Units:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_TP_Measurement_Units;
				tmp_lg++;
				buffer[tmp_lg] =1;
				tmp_lg++;
				buffer[tmp_lg] = pTp_dat->TP_Measurement_Units;
				break;
			case T_TP_Status:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_TP_Status;
				tmp_lg++;
				buffer[tmp_lg] =1;
				tmp_lg++;
				buffer[tmp_lg] = pTp_dat->TP_Status;
				break;
			case T_TP_Alarm:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_TP_Alarm;
				tmp_lg++;
				buffer[tmp_lg] =1;
				tmp_lg++;
				buffer[tmp_lg] = pTp_dat->TP_Alarm[0];
				tmp_lg++;
				buffer[tmp_lg] = pTp_dat->TP_Alarm[1];
				break;
			//follow r(2)
			case T_Product_Level:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Product_Level;
//				RunLog("ReadTP_DATA.case PR_Level, tp_stat: %d", tp_stat);
				if(tp_stat !=  2){
					tmp_lg++;
					buffer[tmp_lg] ='\0';
					break;
				}
				tmp_lg++;
				buffer[tmp_lg] =4;
				tmp_lg++;
				memcpy( &buffer[tmp_lg], pTp_dat->Product_Level, sizeof(pTp_dat->Product_Level) );
//				RunLog("PR_Level: %s", Asc2Hex(&buffer[tmp_lg], sizeof(pTp_dat->Product_Level)));
				tmp_lg += sizeof(pTp_dat->Product_Level) - 1;
				break;
			case T_Total_Observed_Volume:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Total_Observed_Volume;
				if(tp_stat  !=  2){
					tmp_lg++;
					buffer[tmp_lg] ='\0';
					break;
				}
				tmp_lg++;
				buffer[tmp_lg] =7;
				tmp_lg++;
#ifdef TANK_DEBUG
				RunLog("Total Observed Volume is %02x%02x%02x%02x", pTp_dat->Total_Observed_Volume[3],\
				pTp_dat->Total_Observed_Volume[4],\
				pTp_dat->Total_Observed_Volume[5],\
				pTp_dat->Total_Observed_Volume[6]);
				RunLog("Total_Observed_Volume ' address is %p", pTp_dat->Total_Observed_Volume);
#endif
				memcpy( &buffer[tmp_lg], pTp_dat->Total_Observed_Volume,
							sizeof(pTp_dat->Total_Observed_Volume) );

#ifdef TANK_DEBUG
				RunLog("Total Observed Volume is %02x%02x%02x%02x", pTp_dat->Total_Observed_Volume[3],\
				pTp_dat->Total_Observed_Volume[4],\
				pTp_dat->Total_Observed_Volume[5],\
				pTp_dat->Total_Observed_Volume[6]);
				RunLog("Total_Observed_Volume ' address is %p %p %p", pTp_dat->Total_Observed_Volume, pTlg_data, gpIfsf_shm);
#endif
				tmp_lg += sizeof(pTp_dat->Total_Observed_Volume) -1;
				break;
			case T_Gross_Standard_Volume:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Gross_Standard_Volume;
				if(tp_stat  !=  2){
					tmp_lg++;
					buffer[tmp_lg] ='\0';
					break;
				}
				tmp_lg++;
				buffer[tmp_lg] =7;
				tmp_lg++;
				memcpy( &buffer[tmp_lg], pTp_dat->Gross_Standard_Volume, sizeof(pTp_dat->Gross_Standard_Volume) );
				tmp_lg += sizeof(pTp_dat->Gross_Standard_Volume) -1;
				break;
			case T_Average_Temp:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Average_Temp;
				if(tp_stat  !=  2){
					tmp_lg++;
					buffer[tmp_lg] ='\0';
					break;
				}
				tmp_lg++;
				buffer[tmp_lg] =3;
				tmp_lg++;
				memcpy( &buffer[tmp_lg], pTp_dat->Average_Temp, sizeof(pTp_dat->Average_Temp) );
				tmp_lg += sizeof(pTp_dat->Average_Temp) -1;
				break;
			case T_Water_Level:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Water_Level;
				if(tp_stat  !=  2){
					tmp_lg++;
					buffer[tmp_lg] ='\0';
					break;
				}
				tmp_lg++;
				buffer[tmp_lg] =4;
				tmp_lg++;
				memcpy( &buffer[tmp_lg], pTp_dat->Water_Level, sizeof(pTp_dat->Water_Level) );
				tmp_lg += sizeof(pTp_dat->Water_Level) -1;
				break;
			case T_Observed_Density:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Observed_Density;
				if(tp_stat  !=  2){
					tmp_lg++;
					buffer[tmp_lg] ='\0';
					break;
				}
				tmp_lg++;
				buffer[tmp_lg] =2;
				tmp_lg++;
				buffer[tmp_lg] = pTp_dat->Observed_Density[0];//bin16
				tmp_lg++;
				buffer[tmp_lg] = pTp_dat->Observed_Density[1];
				break;
			case T_Last_Reading_Date:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Last_Reading_Date;
				if(tp_stat  !=  2){
					tmp_lg++;
					buffer[tmp_lg] ='\0';
					break;
				}
				tmp_lg++;
				buffer[tmp_lg] =4;
				tmp_lg++;
				memcpy( &buffer[tmp_lg], pTp_dat->Last_Reading_Date, sizeof(pTp_dat->Last_Reading_Date) );
				tmp_lg += sizeof(pTp_dat->Last_Reading_Date) -1;
				break;
			case T_Last_Reading_Time:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Last_Reading_Time;
				if(tp_stat  !=  2){
					tmp_lg++;
					buffer[tmp_lg] ='\0';
					break;
				}
				tmp_lg++;
				buffer[tmp_lg] =3;
				tmp_lg++;
				memcpy( &buffer[tmp_lg], pTp_dat->Last_Reading_Time, sizeof(pTp_dat->Last_Reading_Time) );
				tmp_lg += sizeof(pTp_dat->Last_Reading_Time) -1;
				break;
			default:
				tmp_lg++;
				buffer[tmp_lg] =recvBuffer[i+10] ;
				tmp_lg++;
				buffer[tmp_lg] ='\0';
				break;
		}
	}
	tmp_lg++;
	buffer[6] = (unsigned char )( (tmp_lg-8) / 256) ;
	buffer[7] = (unsigned char )((tmp_lg-8) %256);  //big end.
	*msg_lg= tmp_lg;
	memcpy(sendBuffer, buffer, tmp_lg);

	return 0;
}

static int asc_to_hex(const char *asc, char *hex, int lenasc)
{
	assert(hex!= NULL);
	int i;
	for(i = 0; i <lenasc; i++){
		if(asc[i]>= '0' && asc[i]<= '9')
			hex[i] = asc[i]-0x30;
		else if (asc[i] >= 'A' && asc[i] <='F')
			hex[i] = asc[i] -0x37;
		else if (asc[i] >= 'a' && asc[i] <='f')
			hex[i] = asc[i] -0x57;
		else{
			RunLog("THIS IS A INVALID CARACTOR");
			return -1;
		}
	}
	return i;
}

static int hex_to_bcd(const char *hex, char *bcd, int lenhex)
{
	assert(bcd!=NULL);
	int i, lenbcd;
	for(i = 0, lenbcd = 0; i < lenhex; i+=2, lenbcd++){
		bcd[lenbcd] = ((hex[i]&0x0f)<<4)|(hex[i+1]&0x0f);
	}
	return lenbcd;
}
void DLB_sendTlgData(){//int sig
	 //unsigned char ;	
	 unsigned char sPortName[5],*sIPName;	
	 unsigned char recvBuffer[8], sendBuffer[1280];
	 

	char together[1280]; 
	 unsigned int recvLength,msgLength;
	 int i, k, ret, tmp, tryN = 5, timeout = 10;
	 //GunShm *ppGunShm;
	IFSF_SHM *ppIfsf_Shm;

	char *tlgmsg = "090102010002000701214041424344";
	int len, prelen;
	char bcdtlgmsg[32];
	char hextlgmsg[32];
		
	RunLog("启动sendTlgData进程");
	//等待IFSF_MAIN启动
	sleep(30);//等待IFSF_MAIN启动完毕
	if( gpDLB_Shm == NULL ){ //(ppGunShm == NULL)||()
		RunLog( "错误,DLB的共享内存为空" );
		exit(1);
	}
	i = 0;

	while(gpDLB_Shm->css._TLG_SEND_INTERVAL<=0){//不发送TLG数据
		i++;
		if(i<60)sleep(60*i);
		else sleep(3600);
	}
	

	//gpDLB_Shm->css._TLG_SEND_INTERVAL = 1;
	
	ppIfsf_Shm = (IFSF_SHM*)DLB_AttachTheShm( IFSF_SHM_INDEX );	
	if( NULL == ppIfsf_Shm  ){
		RunLog("pIfsf_Shm空, DLB收发液位仪数据 失败");
		return;
	}
	//打开9002端口发送数据的管道 
	int duanTlgFifoFd = open( TLG_FIFO, O_RDWR| O_NONBLOCK);//O_WRONLY//gSendFifoFd
	if( duanTlgFifoFd< 0 ) {//gSendFifoFd
		RunLog( "DLB_sendTlgData写打开发送数据的管道文件[%s]失败errno[%d]\n", TLG_FIFO ,errno);
		//Free( pName );
		return ;
	}
	else{
		RunLog( "DLB_sendTlgData写打开发送数据的管道文件[%s]成功", TLG_FIFO );
	}
	
	len = strlen(tlgmsg);
	asc_to_hex(tlgmsg, hextlgmsg, len);
	hex_to_bcd(hextlgmsg, bcdtlgmsg, len);
	RunLog("bcdtlgmsg  %02x%02x%02x%02x", bcdtlgmsg[0],  bcdtlgmsg[1], bcdtlgmsg[2], bcdtlgmsg[3]);
	while(1){


		//注册失败，进程挂起
		//Login_Sleep();
		//RunLog("液位仪数据 写入管道成功");
		while(gpDLB_Shm->css._TLG_SEND_INTERVAL<=0){
			sleep(60);
		};
		sleep(gpDLB_Shm->css._TLG_SEND_INTERVAL);	//-1是前面代码的运行时间
		
		if( ppIfsf_Shm->node_online[MAX_CHANNEL_CNT] == 0 ){ //液位仪不在线延迟10秒在判断
			RunLog("DLB_sendTlgData 液位仪离线!");
			sleep(10);
			continue;
		}
		msgLength = 0;
		prelen = 0;
		for (i = 0; i < MAX_TP_PER_TLG; i++) {
			if(ppIfsf_Shm->tlg_data.tp_dat[i].TP_Status != 0){	
				bcdtlgmsg[9] = 0x20|(i+1);
				DLB_ReadTP_DAT(ppIfsf_Shm, bcdtlgmsg, &sendBuffer[prelen], &msgLength);			
				prelen += msgLength; 
				RunLog("Send buffer %s prelen %d", Asc2Hex(&sendBuffer[prelen], msgLength), prelen);
			}
		}
		msgLength = prelen;//总长度加前面的5个字节: 标志和长度
		//发送数据
		if( duanTlgFifoFd <=0  ){//gSendFifoFd
			RunLog("液位仪数据 写入管道duanTlgFifoFd<=0错误失败");
			return;
		}
		bzero(together, sizeof(together));
		//together= (char *)malloc(msgLength +4 );
		memcpy(together, (char *)&msgLength, 4);
		memcpy(together+4, sendBuffer, msgLength);
		RunLog("DDDDDDDDDDD %s", Asc2Hex(together, msgLength+4));
		if (gpDLB_Shm->write_tlg == 0){
			gpDLB_Shm->write_tlg = 1;
			ret=write(duanTlgFifoFd, together, msgLength +4);//gSendFifoFd
			gpDLB_Shm->write_tlg = 0;
			
			if( ret <0  ){
				
				RunLog("液位仪数据 写入管道失败 sleep 20s");

				continue;
			}
			else if (ret == msgLength +4){
				
				RunLog("液位仪数据 写入管道成功");

			}
			else {
				
				RunLog("液位仪数据 写入管道长度不对 ret = %d msgLength is %d", ret, msgLength);

			}
		}
		else {
			RunLog("由于抢占的问题该数据未能写入管道");
		}



	}
	//alarm(gpDLB_Shm->css._TLG_SEND_INTERVAL*60);	
	DLB_DetachTheShm((char**)(&ppIfsf_Shm));
	RunLog("DLB _sendTlgData异常退出");
	return;
}
//数据处理
void DLB_data(  )
{
	int i,ret,pid,isChild;
	unsigned char sPortName[5],*sIPName;	
 	int    tmp, timeout =5;
	unsigned char *resultBuff;
	IFSF_SHM *ppIfsf_Shm;
	int gTlgFifoFd;
	//打开9002端口发送数据的管道 
	gSendFifoFd = open( SEND_FIFO, O_RDWR|O_NONBLOCK);//O_WRONLY
	if( gSendFifoFd< 0 ) {
		RunLog( "DLB_data打开发送数据的管道文件[%s]失败errno[%d]\n", SEND_FIFO ,errno);
		return ;
	}
	else{
		RunLog( "DLB_data打开发送数据的管道文件[%s]成功", SEND_FIFO );
	}
/*	gTlgFifoFd = open( TLG_FIFO, O_RDWR|O_NONBLOCK);//O_WRONLY
	if( gTlgFifoFd< 0 ) {
		RunLog( "DLB_data打开发送数据的管道文件[%s]失败errno[%d]\n", TLG_FIFO ,errno);
		return ;
	}
	else{
		RunLog( "DLB_data打开发送数据的管道文件[%s]成功", TLG_FIFO );
	}*/
	
	//等待IFSF_MAIN启动
	sleep(30); 
	ppIfsf_Shm = (IFSF_SHM *)DLB_AttachTheShm( IFSF_SHM_INDEX );
	if( ppIfsf_Shm == NULL ){ //(ppGunShm == NULL)||()
	
		RunLog( "连接共享内存失败" );
		exit(1);
	}	
	if( gpDLB_Shm == NULL ){ //(ppGunShm == NULL)||()
	
		RunLog( "错误,DLB的共享内存为空" );
		exit(1);
	}

	bzero(gpDLB_Shm->css.sendBuffer, sizeof(gpDLB_Shm->css.sendBuffer));
			
	//循环处理并发送交易数据和实时数据(如果要的话)
	//先初始化打开FIFO		
	gpDLB_Shm->css.fd_fifo = -1;
	gpDLB_Shm->css.fd_fifo = open(DLB_FIFO, O_RDONLY|O_NONBLOCK);
	if(gpDLB_Shm->css.fd_fifo <= 0){
		
		RunLog("只读打开FIFO打开出错.");
		close(gpDLB_Shm->css.fd_fifo);
		exit(0);
	}	
	//循环处理数据	
	ppIfsf_Shm->css_data_flag = 2; // 为了减少错误,设置固定的2,2已经测试成功于dla_main
	
	gpDLB_Shm->css._REALTIME_FLAG = 1;
	while(1){		

		RunLog("DLb  css_data_flag %d", ppIfsf_Shm->css_data_flag);
		//注册失败，进程挂起
		//Login_Sleep();
		
		ret = DLB_ReadTransData( gpDLB_Shm->css.sendBuffer);//resultBuff
		
		RunLog( "DLB_ReadTransData完毕,状态数据[%s]",  \
			DLB_Asc2Hex(gpDLB_Shm->css.sendBuffer, gpDLB_Shm->css.msgLength));//,返回[%d]	, ret 

	}
	
	ppIfsf_Shm->css_data_flag = 0;
	DLB_DetachTheShm((char**)(&ppIfsf_Shm));
	exit(0);	
	return ;
}

//--------------------------------------------------
//离线交易处理函数

//离线交易是否处理
#define OFFLINE_SEND 0
#if OFFLINE_SEND
//#define DLB_OFFLINE_MAXNUM  50000
unsigned long head_4offline,tail_4offline;//交易数据头尾
unsigned int num_4offline;//交易记录数目,num_4offline最大值是DLB_OFFLINE_MAXNUM-1
int  DLB_OfflineInit(const unsigned char *offlineFileName) {	
	int fd;
	int len;
	int ret;
	unsigned char changFlag[2];
	int i;
	DLB_OFFLINE_TRANS *offTrans;
	unsigned char tansBuff[64];
	//extern int errno;	
	 //kk[0-2]用于环形交易记录文件找头尾.  kk[0]为首部是否有数据,
	 //kk[1]为第一个数据有无变化的地址,kk[2]为下一个数据有无变化的地址
	unsigned long kk[3];
	 
	if(NULL == offlineFileName ){
		RunLog("DLB_OfflineInit argument is null");
		return -1;//exit(1);
	}	
	
	//file is exist or not?
	ret = access( offlineFileName, F_OK );
	if(ret < 0){
		len = 0;
		RunLog("文件[%s]不存在.", offlineFileName );
		fd = open(offlineFileName,O_CREAT|O_APPEND);//O_RDWR|
		if(fd<=0){
			RunLog("创建文件[%s]失败.", offlineFileName );
			return -2;
			//exit(1);
		}
		bzero(tansBuff,sizeof(tansBuff));
		for(i=0;i<DLB_OFFLINE_MAXNUM;i++){
		 	if(  (write(fd,   &tansBuff,   sizeof(DLB_OFFLINE_TRANS) )) == -1) 
				RunLog("写入文件[%s]错误", offlineFileName);
		}
		close(fd);
		head_4offline = 0;
		tail_4offline = 0;
		num_4offline = 0;//记录数
		//return -2;	
	}else{
		num_4offline = 0;//记录数
		if (fd = open(offlineFileName,O_RDONLY)<= 0){//O_RDWR|
			RunLog("打开文件[%s]错误", offlineFileName );
			return -3;
		}
		bzero(tansBuff,sizeof(tansBuff));
		if(  read(fd,   &tansBuff,   sizeof(DLB_OFFLINE_TRANS) )== -1) 
				RunLog("读取文件[%s]错误", offlineFileName);
		offTrans =(DLB_OFFLINE_TRANS *)tansBuff;
		if(0 !=  offTrans->send_flag){//有交易
			num_4offline++;
			kk[0] =1;
		}else{
			kk[0] =0;
		}
		changFlag[0] = 0;
		changFlag[1] = 0;
		for(i=1;i<DLB_OFFLINE_MAXNUM;i++){
			if( read(fd,   &tansBuff,   sizeof(DLB_OFFLINE_TRANS) ) == -1) {
					RunLog("读取文件[%s]错误", offlineFileName);
					return -4;
			}
			offTrans =(DLB_OFFLINE_TRANS *)tansBuff;
			if(0 !=  offTrans->send_flag){//有交易
				num_4offline++;
			}
			if(kk[0] !=0){//首地址有交易数据
				if(0 == changFlag[0]){
					if(0==  offTrans->send_flag){						
						changFlag[0] =1;
						kk[1] =(i-1)*sizeof(DLB_OFFLINE_TRANS);
					}else{						
						//changFlag = 0;//kk[0] =0;
					}
				}else{
					if(0 !=  offTrans->send_flag){//有交易
						if(0 == changFlag[1]){
							changFlag[1] =1;
							kk[2] =i*sizeof(DLB_OFFLINE_TRANS);//break;//已经全部找到
						}else{}
						
					}else{
						if(1 == changFlag[1]){
							RunLog("文件[%s]交易数据有错误", offlineFileName);
							return -5;
						}
						//kk[0] =0;
					}
				
				}
			}else{//首地址没有交易数据
				if(0 == changFlag[0]){
					if(0 !=  offTrans->send_flag){//有交易
						changFlag[0] =1;
						kk[1] =i*sizeof(DLB_OFFLINE_TRANS);
					}else{
						//changFlag = 0;//kk[0] =0;
					}
				}else{
					if(0 ==  offTrans->send_flag){
						if(0 == changFlag[1]){
							changFlag[1] =1;
							kk[2] =(i-1)*sizeof(DLB_OFFLINE_TRANS);//break;//已经全部找到
						}else{}
					}else{
						if(1 == changFlag[1]){
							RunLog("文件[%s]交易数据有错误", offlineFileName);
							return -5;
						}
					}
				
				}
			}
		}
		head_4offline = 0;
		tail_4offline = 0;
		if(0 !=num_4offline){
			if((0!=kk[0])&&(0 != changFlag[0])&&(0 == changFlag[1])){
				head_4offline = 0;
				tail_4offline = kk[0]+sizeof(DLB_OFFLINE_TRANS);
			}else if((0!=kk[0])&&(0 != changFlag[0])&&(0 != changFlag[1])){
				head_4offline = kk[1];
				tail_4offline = kk[0]+sizeof(DLB_OFFLINE_TRANS);
			}else if((0 ==kk[0])&&(0 != changFlag[0])&&(0 == changFlag[1])){
				head_4offline = kk[0];
				tail_4offline =0;
			}else if((0==kk[0])&&(0 != changFlag[0])&&(0 != changFlag[1])){
				head_4offline =  kk[0];
				tail_4offline = kk[1]+sizeof(DLB_OFFLINE_TRANS);
			}
		}                                                                

		close(fd);
	}
	
	//get file size.
	fd = open(offlineFileName,O_RDONLY);//|O_CREAT O_RDWR
	if(fd<=0){
		RunLog("打开文件[%s]失败.", offlineFileName );
		return -1;
		//exit(1);
	}
	len =lseek(fd, 0, SEEK_END);
	//len = ftell(fd);//ftell
	close(fd);
	if(len != sizeof(DLB_OFFLINE_TRANS)*DLB_OFFLINE_MAXNUM){
		RunLog("文件长度异常,实际长度[%d],应当长度[%d]", len, sizeof(DLB_OFFLINE_TRANS)*DLB_OFFLINE_MAXNUM );
		return -7;
	}
	return len;	
}

//得到文件的字节数
int  DLB_GetFileSizeByFullName(const char *sFileName) {	
	FILE *fp;
	int len;
	int ret;
	int c;
	extern int errno;
	if(NULL == sFileName ){
		RunLog("GetLogFileSize argument is null");
		exit(1);
	}

	
	//file is exist or not?
	ret = access( sFileName, F_OK );
	if(ret < 0){
		len = 0;
		RunLog("文件[%s]不存在.", sFileName );
		return -2;	
	} 
	
	//get file size.
	fp = fopen(sFileName,"rb");
	if(!fp){
		RunLog("打开文件[%s]失败.", sFileName );
		return -1;
		//exit(1);
	}

	fseek(fp, 0, SEEK_END);
	len = ftell(fp);
	fclose(fp);

	return len;	
}
//获取一条脱机交易记录
//返回值: -1不成功,0成功,1没有记录了
int TakeOneTR_FILEDATA(const unsigned char *lsTrName, unsigned char *sendBuffer)//,const int *readpointint fd,  
{	
	unsigned char buffer[128];
	int i,fd,m,flength;
	unsigned char *mapped_mem;
	//DLB_END_TRANS *pCssTrans;
	DLB_OFFLINE_TRANS *pCssOfflineTrans;
	
	memset(buffer, 0, sizeof(buffer));

	
	if(NULL == sendBuffer){
		RunLog("参数空错误SendOneTailTR_FILEDATA 失败");
		return -1;
	}	
	if( NULL == lsTrName ){		
		RunLog("参数空错误SendOneTailTR_FILEDATA 失败");//RunLog("共享内存值错误,脱机交易数据SendOneTailTR_FILEDATA  失败");
		return -1;
	}
		
	m =DLB_GetFileSizeByFullName(lsTrName);	
	if( m == -1 ) {
		RunLog( "取脱机交易文件 失败." );
		return -1;
	} else if(m == -2) {	//-2文件不存在
		RunLog( "脱机交易 文件不存在" );
		return -1;
	}else if(m == 0) {	//-2文件没有记录
		RunLog( "脱机交易 文件没有记录" );
		return -1;
	}
	
	if(0 ==num_4offline){
		RunLog("没有脱机交易数据了");
		return 1;
	}
	
	fd = open(lsTrName, O_RDWR);
	if (fd < 0) {
		return -1;
	}

	flength = lseek(fd, 0, SEEK_END);
	if(flength == 0) {	//-2文件不存在
		RunLog( "脱机交易 文件没有记录" );
		return -1;
	}else if(flength != sizeof(DLB_OFFLINE_TRANS)*DLB_OFFLINE_MAXNUM){
		RunLog("文件长度异常,实际长度[%d],应当长度[%d]", flength, sizeof(DLB_OFFLINE_TRANS)*DLB_OFFLINE_MAXNUM );
		return -7;
	}		
	lseek(fd, 0, SEEK_SET);
	lseek(fd, head_4offline, SEEK_SET);
	if(  ( read(fd,   &buffer,   sizeof(DLB_OFFLINE_TRANS) ) ) == -1) {
		RunLog("读取文件[%s]错误", lsTrName);
		return -4;
	}
	pCssOfflineTrans =(DLB_OFFLINE_TRANS *)buffer;
	if(0 !=  pCssOfflineTrans->send_flag){
		num_4offline++;
	}
	//mapped_mem = mmap(NULL, flength, PROT_READ | PROT_WRITE, MAP_SHARED,fd, 0);
	//memcpy(buffer,mapped_mem+m - sizeof(DLB_OFFLINE_TRANS),sizeof(DLB_OFFLINE_TRANS));
	//pCssOfflineTrans = (DLB_OFFLINE_TRANS *)buffer;
      	
	//i = 1;//一个交易记录
	//while(pCssOfflineTrans->send_flag != 0) {	
//		RunLog(">>>>>>>> 清除对应的脱机交易记录");
		//memset(mapped_mem+m - sizeof(DLB_OFFLINE_TRANS),0,sizeof(DLB_OFFLINE_TRANS));   //删除文件的第一条记录 FIFO
		//memcpy(mapped_mem,mapped_mem + sizeof(DLB_OFFLINE_TRANS) ,flength - sizeof(DLB_OFFLINE_TRANS));
			 
//		RunLog(">>>>>>>> 文件同步<<<<<<<<");
		//msync(mapped_mem, flength,MS_SYNC);

		// 改变文件大小
		//if (ftruncate(fd, m - sizeof(DLB_OFFLINE_TRANS)) == -1) { //同时截断文件
		//	RunLog("ftruncate file error!");
		//	return -1;
		//}
		//i++;
		/*
		if(sizeof(DLB_OFFLINE_TRANS)>=m){//没有交易了
			close(fd);
			munmap(mapped_mem, flength); 
			return 1;
		}
		memcpy(buffer,mapped_mem+m - i*sizeof(DLB_OFFLINE_TRANS),sizeof(DLB_OFFLINE_TRANS));
		pCssOfflineTrans = (DLB_OFFLINE_TRANS *)buffer;*/	      		               
		
	//} 
	
	memcpy(sendBuffer, buffer, sizeof(DLB_OFFLINE_TRANS));

	close(fd);
	//munmap(mapped_mem, flength); 
	RunLog( "读取了一条脱机交易 记录" );
	/*if ( flength == sizeof(DLB_OFFLINE_TRANS)) { //无交易记录,脱机交易文件被删除
		unlink(lsTrName);
		RunLog("无交易记录,脱机交易文件被删除");
		//return -1;
	}*/	
	return 0;		
}



#endif


//only for HBSendServ.
static int  DLB_HBSend(const int time, const int sockfd, struct sockaddr *remoteSrv)
{
	int i,k,step;
	int startn, endn;
	int ret;	
	static int runcnt = 0;	
	extern int errno;
	static unsigned char sendHBbuff[1024];
	int count;
	bzero(sendHBbuff, sizeof(sendHBbuff));
	runcnt++;
	if (runcnt >= time){ 
		runcnt = 0;
	}
	if (time <=0){
		RunLog("DLB time error, time:[%d]",time);
		return -1;
	}

	startn = (MAX_CHANNEL_CNT + 1) / time * runcnt ;       // MAX_CHANNEL_CNT + 1:  all dispenser + tlg
	endn = (MAX_CHANNEL_CNT + 1) * (runcnt + 1) / time;	
	k = 0;
	//UserLog("HBSend  starting....runcnt:%d, startn:%d, endn:%d", runcnt, startn,endn);
	for (i = 0, count = 0; i < MAX_CHANNEL_CNT+1; i++) {
		sendHBbuff[0] = 0xbc;
		memcpy(&sendHBbuff[2 + count*9], gpDLB_Shm->css._STATION_NO_BCD,5);//站点编号
		if (((phIfsf_Shm->node_online[i] != 0)              // 节点在线
			&& (phIfsf_Shm->convert_hb[i].port != 0)) 
			 ||(node_offline[i] != 0)){   //找到了加油机节点
			//UserLog("HBSend  starting....[%d] is sending, ppid:[%d]", step,getppid() );
			//UserLog("HBSend  starting....[%d], pid:%d, ppid:%d, time:%d, send size:%d, ip:%x",  \
			//step,  getpid(), getppid(),  time, sizeof(IFSF_HEARTBEAT), remoteSrv->sin_addr.s_addr );
			//sendHBbuff[0 + count*9] = 0xBB; //标志位
			//sendHBbuff[1 + count*9] = 0x07;//长度
			if(i == MAX_CHANNEL_CNT)
			{
				sendHBbuff[8 + count*2] = 0xB1;
			}
			else{
				sendHBbuff[8 + count*2] = i +1;
			}

			sendHBbuff[9 + count*2] = node_offline[i] != 0 ?   0 :  1;
			
			node_offline[i] = 0x00;  //恢复离线标记
			count++;
		}		
	}	
	if (count == 0)
		return 0;
	sendHBbuff[7] = count;
	sendHBbuff[1] = count*2+6;
	
	ret = sendto(sockfd, sendHBbuff, sendHBbuff[1]+2,0, remoteSrv, sizeof(struct sockaddr));
	if (ret < 0) {				
		RunLog("DLB心跳数据发送失败,error:[%s]", strerror(errno));	
	}	
	else {
		RunLog("send heart %s count is %d", Asc2Hex(sendHBbuff, ret), count);
	}

	return 0;
}

//生成离线心跳
void DLB_MakeOffline()
{
	int i;
	unsigned char node;


#if 0
	if(	phIfsf_Shm->node_online[MAX_CHANNEL_CNT] == 0 )
		node_offline[MAX_CHANNEL_CNT] = 0x65;
	else
		node_offline[MAX_CHANNEL_CNT] = 0;
#endif
	//判断、设置液位仪离线标志
	node_offline[MAX_CHANNEL_CNT] = phIfsf_Shm->node_online[MAX_CHANNEL_CNT] == 0 ? 0x65 : 0;
	
	for( i = 0; i < MAX_GUN_CNT; i++){
		if( phIfsf_Shm->gun_config[i].isused == 1){  //用isused判断那些油机离线
			node = phIfsf_Shm->gun_config[i].node;
			if(phIfsf_Shm->node_online[node - 1] == 0){  // 节点离线
				node_offline[node - 1] = node;  //设置离线标记
			}
			
//			RunLog("@@@@@@@ Node[%02x] is offline port [%d]", node,phIfsf_Shm->convert_hb[node - 1].port);
//			RunLog("@@@@@@@ node_offline = [%s]",Asc2Hex(&node_offline[0], sizeof(node_offline)));
		}
	}

}


//启动本设备心跳发送服务，返回0正常启动，-1失败.
int DLB_HBSendServ()
{
	int time,ret;
	int sockfd;
	int broadcast = 1;
	//struct itimerval value;
	char	ip[16];
	struct sockaddr_in clientUdpAddr_Heart, DLB_HEART_UDP_ADDR;	
	int 	port;
	extern int errno;
	int reuse = 1;

	sleep(10);
	phIfsf_Shm = (IFSF_SHM*)DLB_AttachTheShm( IFSF_SHM_INDEX );	
	if( NULL == phIfsf_Shm  ){
		RunLog("phIfsf_Shm空, DLB发送油机液位仪心跳数据失败!");
		return;
	}
	RunLog("Dlb_mian Heartbeat Send Server-->");
	bzero(&DLB_HEART_UDP_ADDR,sizeof(DLB_HEART_UDP_ADDR));
	DLB_HEART_UDP_ADDR.sin_family=AF_INET;
	DLB_HEART_UDP_ADDR.sin_addr.s_addr=inet_addr(gpDLB_Shm->css._SERVER_IP);
	DLB_HEART_UDP_ADDR.sin_port = htons(DLB_HEART_BEAT_DUP_PORT);
	gpDLB_Shm->css._SOCKHUDP = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );

	if(setsockopt(gpDLB_Shm->css._SOCKHUDP, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)) < 0)
	{
		RunLog( "setsockopt错误号:%d", errno );
		return -1;
	}
	clientUdpAddr_Heart.sin_family = AF_INET;
	clientUdpAddr_Heart.sin_addr.s_addr = htonl(INADDR_ANY);
	clientUdpAddr_Heart.sin_port = htons(DLB_HEART_BEAT_DUP_PORT);
	if( bind( gpDLB_Shm->css._SOCKHUDP, (SA *)&clientUdpAddr_Heart, sizeof( clientUdpAddr_Heart ) ) < 0 )
	{
		RunLog( "DLB_HBSendServ错误号:%d", errno );
		return -1;
	}	
	
	if (  gpDLB_Shm->css._SOCKHUDP<=0 ){//connect(_SOCKUDP, (struct sockaddr *)_SERVER_UDP_ADDR, sizeof(_SERVER_UDP_ADDR))
		RunLog("UDP连接建立不成功,DLB_HBSendServ 失败");
		//close(_SOCKUDP); 
		gpDLB_Shm->css._SOCKHUDP = -1;	
		//exit(0);
	}
	while(1){		
		sleep(10);

		//注册失败，进程挂起
		Login_Sleep();

		//标记离线节点和液位仪
		DLB_MakeOffline();

		//实时上传心跳
		DLB_HBSend(10, gpDLB_Shm->css._SOCKHUDP, (struct sockaddr *)(&DLB_HEART_UDP_ADDR) );
	}
	DLB_DetachTheShm((char**)(&phIfsf_Shm));
	RunLog("DLB_HBSendServ异常退出");
	close(sockfd);

	return 0;
}

#define LEAKDEBUG 1


int DLB_SAM_TLG_CREATE()
{
	int semid;
	semget((key_t)123, 1, 0666|IPC_CREAT);
	
}
//typedef void Sigfunc( int );			/*Sigfunc表示一个返回值为void,参数为int的函数*/




unsigned char tank_pr_no[MAX_TP_PER_TLG][4];

IFSF_SHM *pifsf_shm = NULL;
GunShm *pgun_shm = NULL;
#if 0
static Sigfunc *MySetAlrm( uint sec, Sigfunc *pfWhenAlrm )
{

	if( sec == TIME_INFINITE )
		return NULL;
	Signal( SIGALRM, pfWhenAlrm );
	alarm( sec );
	RunLog("SET alarm success");
}
#endif									/*定义该类型可以简化signal函数的书写*/
int send_all_prno()
{
	int i;
	int ret;
	RunLog("MMMMMMMMMMMMMMMMMMM");
	for(i = 0; i < MAX_TP_PER_TLG; i++){
		if (tank_pr_no[i][3] != 0x00) {
			ret = write_tank_level(tank_pr_no[i]);
			if (ret < 0)
				break;
		}
	}
	return ret;
}
static void
sig_alrm(int signo)
{
 /* nothing to do, just returning wakes up sigsuspend() */
}
DLB_Sleep(unsigned int nsecs)
{
 struct sigaction newact, oldact;
 sigset_t newmask, oldmask, suspmask;
 unsigned int unslept;
 /* set our handler, save previous information */
 newact.sa_handler = sig_alrm;
 sigemptyset(&newact.sa_mask);
 newact.sa_flags = 0;
 sigaction(SIGALRM, &newact, &oldact);
 /* block SIGALRM and save current signal mask */
 sigemptyset(&newmask);
 sigaddset(&newmask, SIGALRM);
 sigprocmask(SIG_BLOCK, &newmask, &oldmask);
 alarm(nsecs);
 suspmask = oldmask;
 sigdelset(&suspmask, SIGALRM); /* make sure SIGALRM isn't blocked */
 sigsuspend(&suspmask); /* wait for any signal to be caught */
 /* some signal has been caught, SIGALRM is now blocked */
 unslept = alarm(0);
 sigaction(SIGALRM, &oldact, NULL); /* reset previous action */
 /* reset signal mask, which unblocks SIGALRM */
 sigprocmask(SIG_SETMASK, &oldmask, NULL);
 return(unslept);
}									
int write_tank_level(unsigned char *pr_no)
{
	unsigned char message[512];
	int i, j, k, ret;
	int pr_index =0;
	unsigned char time_buffer[7];
	int  flag = 0; 
	int stime = 0;
	int len;
	char together[2048];
	int LeakTlgFifoFd;

	bzero(together, sizeof(together));
	

	memset (message, 0, sizeof(message));
	while(stime < 10){	

		sleep(1);
		stime++;	
		RunLog("stime is %d", stime);
		if (stime == 5){
			message[0] = 0xfb;
			memcpy(&message[5], gpDLB_Shm->css._STATION_NO_BCD,sizeof(gpDLB_Shm->css._STATION_NO_BCD));
			/*add time_buffer to message*/		
	 		ret = DLB_getlocaltime(time_buffer);
			memcpy(&message[10], time_buffer, sizeof(time_buffer));
		}
		for ( i = 0; i < MAX_CHANNEL_CNT; i++){
			
			if ( flag == 1)
				break;
			for ( pr_index = 0 ; pr_index < FP_PR_CNT; pr_index++ ){
				if(memcmp(pifsf_shm->dispenser_data[i].fp_pr_data[pr_index].Prod_Nb, pr_no, 4) == 0){
					break;
				}
			}
			if (pr_index == FP_PR_CNT)
				continue;
			
			for ( j = 0; j < MAX_FP_CNT; j++ ){
				
				if ( flag == 1)
					break;
				for ( k = 0; k < MAX_FP_LN_CNT; k++){
					
					if(pifsf_shm->dispenser_data[i].fp_ln_data[j][k].PR_Id == pr_index +1 ){
						/*if the nozzle is not remove then goon else getout from gun polling*/
						if (pifsf_shm->dispenser_data[i].fp_data[j].FP_State<=3){
							continue;
						}else{
							flag = 1;
							RunLog("flag = %d , k = %d", flag, k);
							break;
						}
					}
				}
			}
		}
		flag = 0;
		
		if ( i == MAX_CHANNEL_CNT && j == MAX_FP_CNT && k == MAX_FP_LN_CNT){
			/*add pr_no to message*/
			if (stime ==5){
				RunLog("$$Stime 5");
				memcpy(&message[17], pr_no, 4);
			}
		}
		else{
			RunLog("i = %d, j = %d, k= %d , stime = %d", i, j, k, stime);
			return -1;
		}

		k = 0;
		for ( i = 0; i < MAX_TP_PER_TLG; i++){
			/*get tank which have the same pr_no all  nuzzles hunged*/
			
			if (tank_pr_no[i][3] != 0){
				if ( memcmp( tank_pr_no[i], pr_no, 4) == 0&& stime == 5 ){
					/*get tp message*/

					message[22 +k*16+0] = inttobcd(i);
				
					message[22 +k*16+1] = pifsf_shm->tlg_data.tp_dat[i-1].Product_Level[0];
					message[22 +k*16+2] = pifsf_shm->tlg_data.tp_dat[i-1].Product_Level[1];
					message[22 +k*16+3] = pifsf_shm->tlg_data.tp_dat[i-1].Product_Level[2];
					message[22 +k*16+4] = pifsf_shm->tlg_data.tp_dat[i-1].Product_Level[3];
				
					message[22 +k*16+5] = pifsf_shm->tlg_data.tp_dat[i-1].Water_Level[0];
					message[22 +k*16+6] = pifsf_shm->tlg_data.tp_dat[i-1].Water_Level[1];
					message[22 +k*16+7] = pifsf_shm->tlg_data.tp_dat[i-1].Water_Level[2];
					message[22 +k*16+8] = pifsf_shm->tlg_data.tp_dat[i-1].Water_Level[3];
					
					memcpy(&message[22 +k*16+9], pifsf_shm->tlg_data.tp_dat[i-1].Gross_Standard_Volume, 7);
					
					k++;
				}
			}
		}
		/*the cnt of tank with same pr_no*/
		if (stime == 5){
			
			RunLog("$#Stime 5");
			message[21] = inttobcd(k);
			/*message length */
			len = 22+ 16*k;
		}

	}			
	j = len;
	int dd= j -5;
	char *d= (char *)&dd;
	message[1] = *(d+3);
	message[2] = *(d+2);
	message[3] = *(d+1);
	message[4] = *d;
	/*send message to tlg pipe*/
	RunLog("LEAK message %s", Asc2Hex(message, j));

	
	if (sizeof(together) < j+4) {
		RunLog("together 分配空间小于[%d]", j+4);
		return -1;
	}
	memcpy(together, (char *)&j, 4);
	memcpy(together+4, message, j);
	
	LeakTlgFifoFd = open( TLG_FIFO, O_RDWR|O_NONBLOCK);//O_WRONLY
	if( LeakTlgFifoFd== -1 ) {
		RunLog( "DLB_data打开发送数据的管道文件[%s]失败errno[%d]\n", TLG_FIFO ,errno);
		return -1;
	}
	ret = 0;
	//RunLog("LeakTlgFifoFd is %d",LeakTlgFifoFd);
	if (gpDLB_Shm->write_tlg == 0){
		gpDLB_Shm->write_tlg = 1;
		ret=write(LeakTlgFifoFd, together, j +4);//gSendFifoFd
		gpDLB_Shm->write_tlg = 0;

		if( ret <0  ){
			RunLog("液位仪数据 写入管道失败");
			goto FAULT;
		}
		else if(ret == j +4){
			RunLog("液位仪数据 写入管道正常");
			return 0;
		}
		else {
			RunLog("液位仪数据 写入管道长度不对ret is %d j is %d", ret, j);
			goto FAULT;
		}
	}
	else{
		RunLog("由于抢占的问题该数据未能写入管道");
		goto FAULT;
	}
FAULT:		
	if (LeakTlgFifoFd > 0) {
		close(LeakTlgFifoFd);
		LeakTlgFifoFd = -1;
	}
	
	return -1;
}

/**
*the function makes a buffer which stores leaking message  to send and it is made followed the protocal ask
*the message sent is made up with time & pro_no & tlg_data
*author : duan 2012-11-06
*/

int DLB_SendLeak()
{
	int i, j, k, pr_index;
	unsigned char pr_no[4];
	unsigned char time_buffer[7];
	unsigned char message[512];
	unsigned char buff[256];
	unsigned char tank_same_pr_no[MAX_TP_PER_TLG];
	
	int fd;
	/*the flag indicate that there is one gun ,\
	configured with same oil,at least is not hung  */
	int  flag = 0; 
	int  diff;
	int  ret;
	int tank_num;
	DLB_TLG_LOGIN *tlgConfig;
	
	time_t t;
	static time_t old_time;
	static time_t pre_old_time;
	time_t new_time;
	static int first;
	static int first2;
	int zero = 0;	
	int guncnt;
	
	Login_Sleep();
      	
	pifsf_shm = (IFSF_SHM *)DLB_AttachTheShm(IFSF_SHM_INDEX);
	if (pifsf_shm ==NULL) {
		RunLog("同步共享内存失败");
		return -1;
	}
	pgun_shm= (GunShm*)DLB_AttachTheShm(GUN_SHM_INDEX);
	if (pgun_shm ==NULL) {
		RunLog("同步共享内存失败");
		return -1;
	}

	int tim = 15;
	int timefd = open("/home/App/ifsf/etc/timeval1.cfg", O_RDWR|O_CREAT, 00666);
	if (timefd == -1)
		RunLog("打开或创建timeval1.cfg失败");
	else{
		ret = read(timefd , &(gpDLB_Shm->css.__LEAK_SEND_INTERVAL), 4);
		RunLog("INIT __LEAK_SEND_INTERVAL is %d", gpDLB_Shm->css.__LEAK_SEND_INTERVAL);
		if (ret == 0){
			write(timefd, &tim, 4);
			gpDLB_Shm->css.__LEAK_SEND_INTERVAL = tim;
		}
		close(timefd);
	}
	tim = 20;
	timefd = open("/home/App/ifsf/etc/timeval2.cfg", O_RDWR|O_CREAT, 00666);
	if(timefd == -1)
		RunLog("打开或创建timeval2.cfg失败");
	else{
		ret = read(timefd , &(gpDLB_Shm->css.__AFTER_TIME), 4);
		RunLog("INIT __AFTER_TIME is %d", gpDLB_Shm->css.__AFTER_TIME);
		if (ret == 0){
			write(timefd, &tim, 4);
			gpDLB_Shm->css.__AFTER_TIME = tim;
		}
		close(timefd);
	}

	ret = 0;

	
	bzero(gpDLB_Shm->oilno, sizeof(gpDLB_Shm->oilno));
	bzero(tank_pr_no, sizeof(tank_pr_no));
	for ( i = 0; i < MAX_TP_PER_TLG; i++){
		for ( guncnt = 0;guncnt < MAX_GUN_CNT; guncnt ++){
			/*RunLog("guncnt is %d i is %d node is %02x logfp is %02x gunno is %02x tank 1 is [%02x] tank 2 is %02x tank3 is %02x\n\n", \
						guncnt, i, pifsf_shm->gun_config[guncnt].node,pifsf_shm->gun_config[guncnt].logfp, pifsf_shm->gun_config[guncnt].logical_gun_no,\
						 gpDLB_Shm->tank_pr_no[i][1] , gpDLB_Shm->tank_pr_no[i][2],  gpDLB_Shm->tank_pr_no[i][3]);*/
			if (pifsf_shm->gun_config[guncnt].node == gpDLB_Shm->tank_pr_no[i][1]  &&\
				pifsf_shm->gun_config[guncnt].logfp == gpDLB_Shm->tank_pr_no[i][2] &&\
				pifsf_shm->gun_config[guncnt].logical_gun_no == gpDLB_Shm->tank_pr_no[i][3]){
				memcpy(tank_pr_no[i], pifsf_shm->gun_config[guncnt].oil_no, 4);
				/*RunLog("dd  guncnt is %d i is %d node is %02x logfp is %02x gunno is %02x tank 1 is [%02x] tank 2 is %02x tank3 is %02x\n\n", \
					guncnt, i, pifsf_shm->gun_config[guncnt].node,pifsf_shm->gun_config[guncnt].logfp, pifsf_shm->gun_config[guncnt].logical_gun_no,\
					 tank_pr_no[i][1] , tank_pr_no[i][2],  tank_pr_no[i][3]);*/
					
			}
		}
	}
	RunLog("product level %02x%02x%02x%02x", pifsf_shm->tlg_data.tp_dat[0].Product_Level[0],\
							pifsf_shm->tlg_data.tp_dat[0].Product_Level[1],\
							pifsf_shm->tlg_data.tp_dat[0].Product_Level[2],\
							pifsf_shm->tlg_data.tp_dat[0].Product_Level[3]);


	while(1){
#ifdef LEAKDEBUG
		Login_Sleep();
#endif
#if 0
		if (!first2) {
			first2 = 1;
			MySetAlrm(gpDLB_Shm->css.__AFTER_TIME*60, (Sigfunc*)send_all_prno);
			RunLog("第一次设置定时");
		}
#endif
		if (pgun_shm->if_changed_prno != 0){
			RunLog("油品换号了，需要改变油罐油品对应信息");
			bzero(tank_pr_no, sizeof(tank_pr_no));
			for ( i = 0; i < MAX_TP_PER_TLG; i++){
				for ( guncnt = 0;guncnt < MAX_GUN_CNT; guncnt ++){
					if (pifsf_shm->gun_config[guncnt].node == gpDLB_Shm->tank_pr_no[i][1]  &&\
						pifsf_shm->gun_config[guncnt].logfp == gpDLB_Shm->tank_pr_no[i][2] &&\
						pifsf_shm->gun_config[guncnt].logical_gun_no == gpDLB_Shm->tank_pr_no[i][3]){
						memcpy(tank_pr_no[i], pifsf_shm->gun_config[guncnt].oil_no, 4);
					}
				}
			}
			RunLog("product level %02x%02x%02x%02x", pifsf_shm->tlg_data.tp_dat[0].Product_Level[0],\
							pifsf_shm->tlg_data.tp_dat[0].Product_Level[1],\
							pifsf_shm->tlg_data.tp_dat[0].Product_Level[2],\
							pifsf_shm->tlg_data.tp_dat[0].Product_Level[3]);
			pgun_shm->if_changed_prno = 0;
		}
		
		/*leak oil project message send*/
		time(&t);
		new_time = t;
		
		sleep(2);
		
		RunLog("product level %02x, newt %ld oldt %ld pret %ld", gpDLB_Shm->oilno[3], new_time,old_time, pre_old_time);

		
		if (memcmp(gpDLB_Shm->oilno, (char *)&zero, 4) != 0) {
			memcpy(pr_no, gpDLB_Shm->oilno, 4);
			bzero(gpDLB_Shm->oilno, sizeof(gpDLB_Shm->oilno));
			if (new_time < pre_old_time + gpDLB_Shm->css.__LEAK_SEND_INTERVAL*60) {
				
				RunLog("old time is less than new time [%d]", new_time - pre_old_time);
				
				continue;
			} 
			ret = write_tank_level(pr_no);
			if (ret == 0) {	
				pre_old_time = new_time;
				old_time = new_time;
				continue;
			}
		}
		
		if (new_time > old_time + gpDLB_Shm->css.__AFTER_TIME*60) {
			send_all_prno();
			old_time = new_time;
			pre_old_time = new_time;
		}
	}
	DLB_DetachTheShm((char**)(&pifsf_shm));
	DLB_DetachTheShm((char**)(&pgun_shm));
	RunLog("DLB _SendLeak异常退出");
	return;
	
}


void DLB_Send_data(  )
{	
	#define SEND_OFFLINE_MAX 100
	#define READ_TIMES_4SEND 10
	unsigned char recvBuffer[128] ;//,buff[16],*pBuff
	unsigned char readBuffer[512] ;//,buff[16],*pBuff
	int  ret, i, j, k,  dataLen,readLength,recvLength, tryN =3,timeout =5;
	extern int errno;
	int fifoFd,OfflineTransFd;	
	//int ret,timeout =5;
	unsigned char *sIPName = NULL;
	DLB_OFFLINE_TRANS *pOfflineTrans;
	unsigned char  data_type;//数据类型,1交易,0液位仪
	//发送数据失败标志,0成功,1回复数据错误,2接收不到数据,3发送失败, 4连接失败
	unsigned char  send_fail;
	int not_receive = 0;
	IFSF_SHM * gpIfsf_shm;

	sleep(10);//等待gSendFifoFd,gTlgFifoFd写打开完毕
	if( gpDLB_Shm == NULL ){ //(ppGunShm == NULL)||()
	
		RunLog( "错误,DLB的共享内存为空" );
		return ;
	}
	gpIfsf_shm = (IFSF_SHM* )DLB_AttachTheShm( IFSF_SHM_INDEX );	
	if( gpIfsf_shm == NULL ){ //(ppGunShm == NULL)||()
	
		RunLog( "错误,DLB的共享内存为空" );
		return ;
	}
	//Login_Sleep();
	//初始化端口
	gpDLB_Shm->server9002_connect_ok = FAILURE_SERVER9002_CONN;
	gpDLB_Shm->css._SOCKTCP = -1;
	while (1) {
		for (i = 0; i < MAX_RECV_CNT; i++ ) {
			//if (gpIfsf_shm->recv_hb[i].lnao.subnet == 2) {
				DLB_InitConfig();
			//	sIPName = inet_ntoa(*((struct in_addr *)(&gpIfsf_shm->recv_hb[i].host_ip)));
			//	sprintf(gpDLB_Shm->css._SERVER_IP, "%s", sIPName);
				RunLog("sIPName is %s", gpDLB_Shm->css._SERVER_IP);
				break;
			//}
		}
		if (strlen(gpDLB_Shm->css._SERVER_IP) == 0) {
			DLB_InitConfig();
			RunLog("sIPName is %s", gpDLB_Shm->css._SERVER_IP);
			sleep(5);
			continue;
		}
		else 
			break;
	}
	j = 0;

	//打开9002端口发送数据的管道 
	gSendFifoFd = open( SEND_FIFO, O_RDONLY|O_NONBLOCK);//O_WRONLY
	if( gSendFifoFd< 0 ) {
		
		RunLog( "读打开发送数据的管道文件[%s]失败errno[%d]\n", SEND_FIFO ,errno);
		//Free( pName );
		return;
	}
	else{
		
		RunLog( "读打开发送数据的管道文件[%s]成功", SEND_FIFO );
	}
	int gTlgFifoFd = open( TLG_FIFO, O_RDONLY|O_NONBLOCK);//O_WRONLY
	if( gTlgFifoFd< 0 ) {
		
		RunLog( "读打开发送数据的管道文件[%s]失败errno[%d]\n", TLG_FIFO ,errno);
		//Free( pName );
		return;
	}
	else{
		
		RunLog( "读打开发送数据的管道文件[%s]成功", TLG_FIFO );
	}
	
	//#if 1
	while(1) {
	RunLog("DLB_Send_data ---- check 0 connect before");
		gpDLB_Shm->css._SOCKTCP = DLB_TcpConnect(gpDLB_Shm->css._SERVER_IP, DLB_SERVER_TCP_PORT_4DATA, (timeout+1)/2);//sPortName
	RunLog("DLB_Send_data ---- check 0 connect after");
		if (gpDLB_Shm->css._SOCKTCP < 0 ){

			RunLog("DLB_TcpConnect连接不成功,DLB发送数据 失败");
			//break;//continue;//再试一次
			gpDLB_Shm->css._SOCKTCP = DLB_TcpConnect(gpDLB_Shm->css._SERVER_IP, DLB_SERVER_TCP_PORT_4DATA, (timeout+1)/2);//sPortName

			if (gpDLB_Shm->css._SOCKTCP < 0 ){
				
				RunLog("DLB_TcpConnect连接不成功,DLB发送数据 失败");
				gpDLB_Shm->server9002_connect_ok = FAILURE_SERVER9002_CONN;
				sleep(10);
			}else{
				gpDLB_Shm->server9002_connect_ok = OK_SERVER9002_CONN;
	RunLog("DLB_Send_data ---- check 0 connect ok");
				break;
			}
		}else{
			gpDLB_Shm->server9002_connect_ok = OK_SERVER9002_CONN;
	RunLog("DLB_Send_data ---- check 0 connect ok");
			break;
		}	
	}
	RunLog("DLB_Send_data ---- check 1");
	while(1){

	RunLog("DLB_Send_data ---- check 1");
		while(1){
	RunLog("DLB_Send_data ---- check 1");
			bzero(readBuffer, sizeof(readBuffer));
			dataLen =-1;
			readLength =-1;
			ret = 0;
			k = 0;
			while(1){		
	//RunLog("DLB_Send_data ---- check 2");
				k++;	
				if(k>=READ_TIMES_4SEND){			
	//RunLog("DLB_Send_data ---- check 2");
					if(readLength != dataLen) {
						
						RunLog("FIFO读取出的数据的长度出错.readLength[%d],dataLen[%d]",readLength,dataLen);

					}
					k = 0;
				}
				
				if(k%2 == 1){
	//RunLog("DLB_Send_data ---- check 3-1");
					fifoFd = gSendFifoFd;
					data_type=1;}
				else  {
	//RunLog("DLB_Send_data ---- check 3-2");
					fifoFd = gTlgFifoFd;
					data_type=0;}
				
	//RunLog("DLB_Send_data ---- check 3-3");
				dataLen = -1;
				ret = read(fifoFd, (char *)&dataLen, 4);//dataLen = 
				if( (ret !=4 )|| (dataLen <=0) ){
					dataLen = -1;
					//RunLog("read fifo to tcp: dataLen == %d",dataLen);
					continue;
				}
	RunLog("DLB_Send_data ---- check 4-1");
			
				RunLog("datatype is %d ret = %d", data_type, ret);
				if(dataLen>=512){
					
					RunLog("发送FIFO读取出的数据的长度出错.readLength[%d],dataLen[%d]",readLength,dataLen);
					continue;
				}
	RunLog("DLB_Send_data ---- check 4-2");
				RunLog( "有数据,读gSendFifoFd开始");
				for(j=0;j< READ_DATA_TIMES;j++){
	RunLog("DLB_Send_data ---- check 4-3");
					readLength = -1;
					readLength = read(fifoFd, readBuffer, dataLen);
					RunLog( "读gSendFifoFd结束前3字节[%02x%02x%02x]",readBuffer[0],readBuffer[1],readBuffer[2] );
					if(readLength != dataLen){
						RunLog( "读发送FIFO错误,readLength[%d], dataLen[%d],errno[%d]",readLength ,dataLen,errno);
						readLength = -1;
						continue;
					}else{
						RunLog("FIFO读取出了数据.readLength[%d],dataLen[%d]",readLength,dataLen);
						break;
					}
				}
				if((readLength == dataLen)&&(readLength != -1)){
					if(readLength<=512){
						
						RunLog( "读发送FIFO正常结束[%s]",DLB_Asc2Hex(readBuffer,readLength) );
					}else{
						RunLog( "读发送FIFO失败,长度大于512[%s]",DLB_Asc2Hex(readBuffer,readLength) );
					}				
					break;
				}
			}	
			
			//注册失败进程挂起
			//Login_Sleep();
			
			//发送数据----/
			send_fail = 0;
			
			for(j=0;j<=tryN;j++){
				ret = DLB_TcpSendDataInTime(gpDLB_Shm->css._SOCKTCP, readBuffer, dataLen, timeout);
				if(ret <0){

					RunLog("DLB_TcpSendDataInTime发送数据 失败");
					if(gpDLB_Shm->css._SOCKTCP >= 0){
						close(gpDLB_Shm->css._SOCKTCP);
						gpDLB_Shm->css._SOCKTCP = -1;
						
					}

					gpDLB_Shm->css._SOCKTCP = DLB_TcpConnect(gpDLB_Shm->css._SERVER_IP, DLB_SERVER_TCP_PORT_4DATA, (timeout+1)/2);//sPortName

					if (gpDLB_Shm->css._SOCKTCP < 0 ){
						
						RunLog("DLB_TcpConnect连接不成功,DLB发送数据 失败");
						//break;//continue;//再试一次
						gpDLB_Shm->css._SOCKTCP = DLB_TcpConnect(gpDLB_Shm->css._SERVER_IP, DLB_SERVER_TCP_PORT_4DATA, (timeout+1)/2);//sPortName

						if (gpDLB_Shm->css._SOCKTCP < 0 ){
							
							RunLog("DLB_TcpConnect连接不成功,DLB发送数据 失败");
							
							gpDLB_Shm->server9002_connect_ok = FAILURE_SERVER9002_CONN;
							break;//continue;
						}
					}
					else {
						gpDLB_Shm->server9002_connect_ok = OK_SERVER9002_CONN;
						continue;
					}
					
					sleep(3);//暂停一会儿,避免写入太频繁
				}
				else 
					break;
			}	
			
			
			if(gpDLB_Shm->server9002_connect_ok == FAILURE_SERVER9002_CONN){
				RunLog("9002连接不成功");
				if( gpDLB_Shm->css._SOCKTCP > 0){
					close(gpDLB_Shm->css._SOCKTCP);
					gpDLB_Shm->css._SOCKTCP = -1;
				}
				continue;
			}else if( (gpDLB_Shm->server9002_connect_ok == OK_SERVER9002_CONN)&&(0 == send_fail) ){
				//上传离线交易数据


			}		
			
			RunLog("DLB收发交易或者液位仪数据成功");
			
			//return 0;
			
		}
		
	if( gpDLB_Shm->css._SOCKTCP >= 0){
		close(gpDLB_Shm->css._SOCKTCP);
	}
	
	RunLog( "DLB_Send_data处理数据异常退出!!" );
	
	return ;		
	}
	//#endif
}

void *DLB_MANAGE_PORT(void *arg)
{
       int i,count = 0;
	pthread_t tid;
	tid = pthread_self();
	int Un_pid;
	Un_pid = (int)arg;
	RunLog("启动端口管理线程tid:[%u]",(unsigned int)tid);
	
	while(1)
	{
		sleep(1);
		for(i = 0; i < DLB_CLIENT_LIMIT_PORT; i++)
		{
			if(gpDLB_Shm->NuPort[i] == 1)
			{
				count++;
				if(count > 130)
				{
					count = 0;
					pthread_mutex_lock(&wrlock);
					for(i = 0; i <DLB_CLIENT_LIMIT_PORT; i++)
						gpDLB_Shm->NuPort[i] = 0;
					pthread_mutex_unlock(&wrlock);
				}
			} 
			else if(gpDLB_Shm->NuPort[i] == 2)  //找到停用端口
				{
					pthread_mutex_lock(&wrlock);
					gpDLB_Shm->NuPort[i] = 3;   //修改状态为释放状态
					pthread_mutex_unlock(&wrlock);
					sleep(45);
					pthread_mutex_lock(&wrlock);
					gpDLB_Shm->NuPort[i] = 0;   //释放端口
					pthread_mutex_unlock(&wrlock);
					RunLog("线程[%d]释放端口tport:[%d] 状态:[%d]",Un_pid, i + DLB_CLIENT_TCP_PORT_ST,  gpDLB_Shm->NuPort[i]);
				}
		}
	}

}

void DLB_manage_port( ){
	pthread_t ntid[8];
	int iTemp1 = 1, iTemp2 = 2;
	int iTemp3 = 3, iTemp4 = 4;
	
	sleep(15);
	RunLog("启动端口管理进程");
	
	//创建本地端口管理线程
	if(pthread_create(&ntid[0], NULL, DLB_MANAGE_PORT, (void *)iTemp1)){
		RunLog("创建端口管理线程错误");
		return;
	}
	if(pthread_create(&ntid[1], NULL, DLB_MANAGE_PORT, (void *)iTemp2)){
		RunLog("创建端口管理线程错误");
		return;
	}
	if(pthread_create(&ntid[2], NULL, DLB_MANAGE_PORT, (void *)iTemp3)){
		RunLog("创建端口管理线程错误");
		return;
	}
	if(pthread_create(&ntid[3], NULL, DLB_MANAGE_PORT, (void *)iTemp4)){
		RunLog("创建端口管理线程错误");
		return;
	}

	while(1){
		sleep(4);
		if (0 != pthread_kill(ntid[0],0)) {
			RunLog("thread manage port exit[%d].", pthread_kill(ntid[0],0));
			  
			pthread_create(&ntid[0], NULL, DLB_MANAGE_PORT, (void *)iTemp1);
			RunLog("重启manage port线程");
		
			//exit(-1);
		} else {
			#if DLB_DEBUG
			RunLog("thread manage port 1 online.");	
			#endif
		}

		if (0 != pthread_kill(ntid[1],0)) {
			RunLog("thread manage port exit[%d].", pthread_kill(ntid[1],0));
			  
			pthread_create(&ntid[1], NULL, DLB_MANAGE_PORT, (void *)iTemp2);
			RunLog("重启manage port线程");
		
			//exit(-1);
		} else {
			#if DLB_DEBUG
			RunLog("thread manage port 2 online.");	
			#endif		  
		}

		if (0 != pthread_kill(ntid[2],0)) {
			RunLog("thread manage port exit[%d].", pthread_kill(ntid[2],0));
			  
			pthread_create(&ntid[2], NULL, DLB_MANAGE_PORT, (void *)iTemp3);
			RunLog("重启manage port线程");
		
			//exit(-1);
		} else {
			#if DLB_DEBUG
			RunLog("thread manage port 3 online.");	
			#endif		  
		}

		if (0 != pthread_kill(ntid[3],0)) {
			RunLog("thread manage port exit[%d].", pthread_kill(ntid[3],0));
			  
			pthread_create(&ntid[3], NULL, DLB_MANAGE_PORT, (void *)iTemp4);
			RunLog("重启manage port线程");
		
			//exit(-1);
		} else {
			#if DLB_DEBUG
			RunLog("thread manage port 4 online.");	
			#endif		  
		}

	}
}
//------------------------------------------------------------------------
/*缺省的alarm处理函数*/
//static void DLB_WhenSigAlrm( int sig );
/*当收到SIGCHLD时,等待结束的子进程.防止产生僵尸进程*/
//static void DLB_WhenSigChld(int sig);
/*自定义一个signal函数.摘自<unix环境高级编程>( stevens )*/
/*如果系统支持，函数可以对被除SIGALRM信号中断的系统调用自动重新启动*/
DLB_Sigfunc *DLB_Signal( int signo, DLB_Sigfunc *func )
{
	struct sigaction 	act, oact;

	act.sa_handler = func;
	sigemptyset( &act.sa_mask );
	act.sa_flags = 0;
	if( signo == SIGALRM )
	{
#ifdef SA_INTERRUPT
		act.sa_flags |= SA_INTERRUPT;
#endif
	}
	else
	{
#ifdef SA_RESTART
		act.sa_flags |= SA_RESTART;
#endif
	}
	if( sigaction( signo, &act, &oact ) < 0 )
	{
		return ( DLB_Sigfunc* )( -1 );
	}
	return oact.sa_handler;
}

/*定义SIGCHLD信号的处理函数*/
static void DLB_WhenSigChld( int sig )
{
	pid_t	pid;
	int		ret;
	unsigned char	pos;
	char 	flag[4];
	DLB_Pid*	pidList;
	//DLB_Pid*	tmpPid;
	int		i;
	DLB_Pid *pFree;

	pidList = DLB_gPidList;
	pos = DLB_cnt;
	if(sig != SIGCHLD ) {
		return;
	}
	
	
	// 因为调用sigaction时,当前的信号会被阻塞,而被阻塞的信号以后只会发送一次,所以需要此处多处waitpid
	// 小于0表示出错.等于0表示无可等待的子进程
	while ((pid = waitpid( -1, NULL, WNOHANG)) > 0) {
		pos = DLB_cnt;

		/*子进程意外退出，查找子进程以重启*/
		RunLog( "进程[%d]退出, cnt:[%d], 当前pid[%d], ppid[%d]", pid ,pos, getpid(),getppid()); //remark only for test 
		
		//ret = GetPidCtl( pidList, pid, flag, &pos ); //arm 4 byte problem, write it under.
		
		for( i = 0; i < pos; i++ ){ 
			pFree = pidList + i;
			if( pFree->pid == pid ) {
				strcpy( flag, pFree->flag );
				pos= i;
				break;
			}
		}
		if(i == DLB_cnt){
			RunLog( "查找子进程失败" );
			Free(DLB_gPidList);
			exit( -1 );
		}
		/*fork进程*/
		pid = fork();
		if( pid < 0 ) {
			RunLog( "fork 子进程失败" );
			Free(DLB_gPidList);
			exit(-1);
		} else if( pid == 0 ) {
			DLB_isChild = 1;
			//Free( gPidList );

			/*根据flag重启进程*/
			if( flag[0] == 'D' )	/*数据进程*/
			{
				RunLog( "重启数据进程" );	
				DLB_data();
				DLB_DetachTheShm((char**)(&gpDLB_Shm));
				exit(0);
			}			
			else if( flag[0] == 'A' ) //上传加油记录进程
			{
				RunLog( "重启接受服务进程" );
				DLB_AcceptServerCmd();
				DLB_DetachTheShm((char**)(&gpDLB_Shm));
				exit(0);
			}
			else if( flag[0] == 'G' ) //上传TLG数据进程
			{
				RunLog( "重启上传TLG数据进程" );
				DLB_sendTlgData();
				DLB_DetachTheShm((char**)(&gpDLB_Shm));
				exit(0);
			}
			else if( flag[0] == 'S' )	/*发送交易和液位仪数据数据进程*/
			{
				RunLog( "重启发送交易和液位仪数据数据进程" );	
				DLB_Send_data();
				DLB_DetachTheShm((char**)(&gpDLB_Shm));
				exit(0);
			}			
			else if( flag[0] == 'P' )	/*发送交易和液位仪数据数据进程*/
			{
				RunLog( "重启端口管理进程" );	
				DLB_manage_port();
				DLB_DetachTheShm((char**)(&gpDLB_Shm));
				exit(0);
			}			
			else if( flag[0] == 'H' )	/*发送油机和液位仪心跳数据进程*/
			{
				RunLog( "重启发送油机和液位仪数据进程" );	
				DLB_HBSendServ();
				DLB_DetachTheShm((char**)(&gpDLB_Shm));
				exit(0);
			}							
		}
		else		/*父进程*/
		{
			DLB_ReplacePidCtl( pidList, pid, flag, pos );
			RunLog( "子进程号[%d]",pid );
		}

	}
	return;
}

/*定义对SIGCHLD信号的处理*/
int DLB_WaitChild( )
{
	sigset_t			set;
	sigset_t			oset;

	sigemptyset( &set );
	sigaddset( &set, SIGCHLD );

	if( sigprocmask( SIG_UNBLOCK, &set, &oset ) < 0 )	/*如果原先屏蔽字中有SIGCHLD,则解除该屏蔽*/
	{
		return -1;
	}

	if( DLB_Signal( SIGCHLD, DLB_WhenSigChld ) == ( DLB_Sigfunc* )( -1 ) )
		return -1;

	return 0;
}

#define CFGFILE "/home/App/ifsf/etc/tlg.cfg"
typedef struct INITCFG{
	unsigned char name[64];
	unsigned char value[64];
}init_cfg;

int ReadAllCfg(init_cfg *initcfg)
{
	FILE *fp;
	char buff[128];
	char *pbuff;
	int term_no = 0;
	int i = 0;
	RunLog("Read All Cfg");
	if (( fp= fopen(CFGFILE, "a+"))== NULL){
		RunLog("!!!fopen [%s] failed ", CFGFILE);
		return -1;
	}
	while(fgets(buff, 128, fp)){
		pbuff = DLB_TrimAll(buff);

		for (i = 0;*pbuff != '='; pbuff++, i++){
			initcfg[term_no].name[i] = *pbuff;
		}
		initcfg[term_no].name[i]= '\0';
		for (i = 0,  ++pbuff; *pbuff != '\0'; pbuff++, i++){
			initcfg[term_no].value[i] = *pbuff;
		}	
		initcfg[term_no].value[i]= '\0';
		RunLog("name is %s value is %s",initcfg[term_no].name, initcfg[term_no].value );
		term_no++;
	}
	bzero((initcfg+term_no), sizeof(init_cfg));
	fclose(fp);
	RunLog("Read All Cfg");	
	return term_no;
}

int ReadCfgByName(init_cfg *initcfg, const char *name)
{
	while(initcfg->name[0]!= 0){
		if (strcmp(initcfg->name, name) == 0){
			return atoi(initcfg->value);
		}
		initcfg++;
	}
	RunLog("there is no config term named [%s]", name);
	return -1;
}

int main( int argc, char* argv[] )
{
	int ret,pid,err;
	
	if (argc == 2) {
	       if (strcmp(argv[1], "-v") == 0) {
			printf( "dlb_main  %s \n",DLB_VER);
	       } else if(strcmp(argv[1], "--version") == 0){
			printf( "dlb_main  %s\n",DLB_VERSION);
		}
		return 0;
	}
	//初始化日志
	printf("日志初始化\n");
#ifndef LOG_IFSF
	ret = DLB_InitLog(1);
	if(ret < 0){
		printf("日志初始化出错.");
		exit(0);
	}	
#else
	ret = InitLog();
	if(ret < 0){
		printf("日志初始化出错.");
		exit(0);
	}	
#endif
	RunLog(" ");
	RunLog("dlb_main  (  %s  )  Startup......................",DLB_VERSION);
	//创建9002端口发送数据的FIFO管道	
	ret = DLB_IsFileExist(SEND_FIFO);	
	//unlink(_FIFO);
	if(ret <=0){
		ret = mkfifo(SEND_FIFO, 0777);//O_RDWR | O_NONBLOCK
		RunLog("发送交易数据的[%s]创建成功.",SEND_FIFO);
		if(ret <0){
			RunLog("发送交易数据的[%s]创建出错.",SEND_FIFO);
			exit(-1);
		}
	}
	else{
		RunLog("发送交易数据的[%s]存在.",SEND_FIFO);
	}
	ret = DLB_IsFileExist(TLG_FIFO);	
	//unlink(_FIFO);
	if(ret <=0){
		ret = mkfifo(TLG_FIFO, 0777);//O_RDWR | O_NONBLOCK
		RunLog("发送液位仪数据的[%s]创建成功.",TLG_FIFO);
		if(ret <0){
			RunLog("发送液位仪数据的[%s]创建出错.",TLG_FIFO);
			exit(-1);
		}
	}
	else{
		RunLog("发送液位仪数据的[%s]存在.",TLG_FIFO);
	}	

	//初始化共享内存Key	
	RunLog("初始化共享内存Key");
	ret = DLB_InitTheKeyFile();
	if( ret < 0 )
	{
		RunLog( "初始化Key File失败\n" );
		exit(1);
	}	
	//初始化共享内存等
	RunLog("初始化共享内存等");
	ret = DLB_InitAll();
	if(ret <0){
		RunLog("共享内存等初始化出错.");
		exit(0);
	}
	//初始化配置参数
	RunLog("初始化配置参数");
	ret = DLB_InitConfig();
	if(ret <0){
		RunLog("配置参数初始化出错.");
		exit(0);
	}
	
	//进程管理
	RunLog("开始启动Css_Main 的子进程");
	DLB_cnt = 6;
	DLB_gPidList = (DLB_Pid*)Calloc( DLB_cnt, sizeof(DLB_Pid) );
	if( DLB_gPidList == NULL ) {
		/*错误处理*/
		RunLog( "分配内存失败" );
		exit(0);//return -1;
	}

	DLB_cnt = 0; //统计已创建的进程数
	//启动进程
	pid = fork();/*启动端口管理进程*/
	{
		if( pid < 0 ) {
			RunLog( "fork 启动端口管理进程出错" );
			return -1;
		} else if( pid == 0 ) {
			DLB_isChild = 1;
			DLB_manage_port( );	
			DLB_DetachTheShm((char**)(&gpDLB_Shm));					
			exit(0);
		} else	{
			DLB_AddPidCtl( DLB_gPidList, pid, "P", &DLB_cnt );
			RunLog( "启动端口管理进程,进程号[%d]",pid);
		}
	}
	pid = fork();/*启动数据进程*/
	{
		if( pid < 0 ) {
			RunLog( "fork 启动数据进程出错" );
			return -1;
		} else if( pid == 0 ) {
			DLB_isChild = 1;
			DLB_data( );	
			DLB_DetachTheShm((char**)(&gpDLB_Shm));					
			exit(0);
		} else	{
			DLB_AddPidCtl( DLB_gPidList, pid, "D", &DLB_cnt );
			RunLog( "启动数据进程,进程号[%d]",pid);
		}
	}
		
	pid = fork();/*启动接收中心命令进程*/
	{
		if( pid < 0 ) {
			RunLog( "fork 启动接收中心命令进程出错" );
			return -1;
		} else if( pid == 0 ) {
			DLB_isChild = 1;
			//接收中心命令进程,启动实时发送SIGUSR1,停止实时发送SIGUSR2
			DLB_AcceptServerCmd( );
			DLB_DetachTheShm((char**)(&gpDLB_Shm));
			exit(0);
		} else	{
			DLB_AddPidCtl( DLB_gPidList, pid, "A", &DLB_cnt );
			RunLog( "启动接收中心命令进程,进程号[%d]",pid);
		}
	}
		
	pid = fork();/*启动TLG发送数据进程*/
	{
		if( pid < 0 ) {
			RunLog( "fork 启动TLG发送数据进程出错" );
			return -1;
		} else if( pid == 0 ) {
			DLB_isChild = 1;
			//接收中心命令进程,启动实时发送SIGUSR1,停止实时发送SIGUSR2
			DLB_sendTlgData( );
			DLB_DetachTheShm((char**)(&gpDLB_Shm));
			exit(0);
		} else	{
			DLB_AddPidCtl( DLB_gPidList, pid, "G", &DLB_cnt );
			RunLog( "启动TLG发送数据进程,进程号[%d]",pid);
		}
	}
	
	pid = fork();/*启动发送交易和液位仪数据进程*/
	{
		if( pid < 0 ) {
			RunLog( "fork 启动发送交易和液位仪数据进程出错" );
			return -1;
		} else if( pid == 0 ) {
			DLB_isChild = 1;
			DLB_Send_data( );	
			DLB_DetachTheShm((char**)(&gpDLB_Shm));					
			exit(0);
		} else	{
			DLB_AddPidCtl( DLB_gPidList, pid, "S", &DLB_cnt );
			RunLog( "启动发送交易和液位仪数据进程,进程号[%d]",pid);
		}
	}
	
#if 0
	pid = fork();/*启动发送油机和液位仪心跳数据进程*/
	{
		if( pid < 0 ) {
			RunLog( "fork 启动发送油机和液位仪心跳数据进程出错" );
			return -1;
		} else if( pid == 0 ) {
			DLB_isChild = 1;
			DLB_HBSendServ();	
			DLB_DetachTheShm((char**)(&gpDLB_Shm));					
			exit(0);
		} else	{
			DLB_AddPidCtl( DLB_gPidList, pid, "H", &DLB_cnt );
			RunLog( "启动发送油机和液位仪心跳数据进程,进程号[%d]",pid);
		}
	}
#endif	

	/*fork完毕*/
	//监控子进程退出
	RunLog( "开始监控Css_Main 子进程是否退出" );
	ret = DLB_WaitChild( );
	
	//向服务器注册
	RunLog( "创建进程完毕,开始向服务器注册" );
	
	#if 0
	gpDLB_Shm->login_ok = FAILURE_LOGIN;
	while(1){	
	       ret = DLB_Login();
		if(ret <0){
			RunLog("向服务器注册出错.");
			sleep(2);
			continue;//exit(0);
		}else{
			gpDLB_Shm->login_ok = OK_LOGIN;
			RunLog("注册成功!液位间隔[%d]分钟gpDLB_Shm->login_ok[%d]"
				,gpDLB_Shm->css._TLG_SEND_INTERVAL, gpDLB_Shm->login_ok);
			break;
		}
	}
	#endif
	
	gpDLB_Shm->css._TLG_SEND_INTERVAL = 5;
	gpDLB_Shm->login_ok = OK_LOGIN;
	
	while(1){
		//sleep(600);//主进程等待
		sleep(300);   // 0.5h
#ifndef LOG_IFSF
		ret = DLB_BackupLog();
		if (ret < 0) {
			RunLog("backup css.log failed");
		}
#endif
	}

	Free( DLB_gPidList );
	DLB_DetachTheShm( (char**)(&gpDLB_Shm) );
	RunLog( "主进程退出.请检查日志" );
	return 0;
	
}

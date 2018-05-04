#ifndef __PUB_H__
#define __PUB_H__

#include <stdio.h>
#include <stdlib.h>
#include <linux/unistd.h>
#include "mycomm.h"
#include "mystring.h"
#include "dotime.h"
#include "commipc.h"
#include "dofile.h"
#include "doshm.h"
#include "dosem.h"
#include "tcp.h"
#include "log.h"
#include "ifsf_def.h"
#include "ifsf_dsp.h"
#include "tcc_adjust.h"
#include "ifsf_tlg.h"

/* add other headers here********************/

#define DEBUG  1


#ifdef __NR_tkill
#undef __NR_tkill
#endif

#define lock_status()    syscall(349)
#define power_status()    syscall(348)

// The value of flowing extern char is writen in file pub.cfg, 
// dsp pcd used only 3 number,tlg pcd used 3 number. it's in file pubsock.c
extern char	PCD_TCP_PORT[6]; 
extern char	SERIAL_PORT_START[2];
extern char	SERIAL_PORT_USED_COUNT[2];
extern char	TLG_NODE[4];
extern char 	TLG_DRV[4];
extern char 	TLG_SERIAL_PORT[2];
extern char 	TLG_PROBER_NUM[4];
extern char 	TCC_ADJUST_DATA_FILE_SIZE[4];         // CSSI.校罐数据采集, 数据文件备份大小
extern char 	SHOW_FCC_SEND_DATA[2]; 		//0 不打印FCC发送给油机数据，1打印

#define TIME_OUT_WRITE_PORT	2
#define RET_TIME_OUT		-2
#define TIME_FOR_RECV_CMD	0	
#define TIME_FOR_TCP_CMD 	0

#define MAX_EXTEND_LEN		8		/*加油机泵码的最大长度*/
#define GUN_SHM_INDEX		100
#define GUN_SHM_TRAN		101
#define GUN_MSG_TRAN		102
#define ORD_MSG_TRAN		103
#define TOTAL_MSG_TRAN        119
#define TCP_MSG_TRAN        	120
#define TCP_MSG_TRAN2        	121
#define PORT_SEM_INDEX		104
#define ERR_SEM_INDEX		105
#define CD_SOCK_INDEX		106
#define CFG_SHM_INDEX		107
#define TANK_MSG_ID		111 
#define WAYNE_RCAP2L_SEM_INDEX  108             // 稳牌设置rcap2l使用
#define TCC_ADJUST_SEM_INDEX    109             // 校罐项目数据采集文件操作

#define MAX_GUN_CNT     	128             /*最大枪数*/ //modify by liys 2008-03-14
#define MAX_OIL_CNT             16              /*最大油品数*///modify by liys 2008-03-14

#define MAX_CHANNEL_CNT    	64		/*最大通道数*/  //=MAX_CHANNEL_CNT
#define MAX_CONVERT_CNT 	MAX_CHANNEL_CNT
#define MAX_PORT_CNT		6		/*最大串口数*/
#define MAX_CHANNEL_PER_PORT_CNT	16	/*每个串口最大通道数*/
#define MAX_PANEL_CNT		4		/*每个油机最大的面板数*/
#define MAX_ERR_IND		200
#define MAX_ORDER_LEN		160
#define MAX_LOG_LEN		160
#define MAX_PORT_CMDLEN		255	        /*向串口写的数据的最大长度*/

#define INDTYP			10000000
#define MAXADD		 	9999999
#define TCP_HEAD_LEN		4
#define TCP_TIME_OUT		3

#define RUN_PROC		0		/*正常状态*/
#define STOP_PROC 		1		/*系统停止*/
#define RESTART_PROC 		2		/*系统重启*/
#define PAUSE_PROC		3		/*暂停*/

#define PAUSE_FOR_TTY		1		/*暂停时tty暂停的时间*/
#define MAX_VOLUME		3		// volume length

#define DEFAULT_WORK_DIR        "/home/App/ifsf/"   //本软件默认工作目录. 
#define DEFAULT_DATA_DIR        "/home/Data/"       // 默认数据存放目录
#define NAND_FLASH_FILE	        "/home/Data/pwf_tr.dat" // 掉电时存放交易数据
#define BAK_TR_SEQ_NB_FILE      "/home/Data/pwf_tr_seq_nb.dat"   // 掉电时存放TR_SEQ_NB
#define BAK_LNZ_TOTAL_FILE      "/home/Data/pwf_lnz_total.dat"   // 掉电时存放油枪总累

#define UNPAIDLIMIT             2               // 允许的未支付交易笔数
#define MAX_FAIL_CNT            3               // 与油机通讯失败计数达到此值判定为油机离线
#define SENDSTATE               5               // 在CLOSED状态下, 用于调节给POS发送状态的频率
#define PRICE_CHANGE_RETRY_INTERVAL 2000000     // 变价失败重试间隔 2s (2000000微秒), 此时间由税控以及油机决定;

#define SYSLOGFILE	"log/sys.log"	        /*系统日志文件.准备用来记录温度,断网联网时间,开关状态及时间日志*/
#define TLGLOGFILE	"log/tlg.log"		/*液位仪运行日志.记录液位仪的运行信息.*/
#define DSPCFGFILE	"etc/dsp.cfg"    	/*加油机节点号,加油机型号,通道,串口等配置信息文件,IFSF加油机专用*/
#define PUBFILE		"etc/pub.cfg"		/*加油机,液位仪等的的 公共配置文件*/
#define TCCFILE		"etc/tcc.cfg"	/*校罐数据文件大小限制参数配置文件单位 K*/
#define IFSFFILE	"etc/dsp.cfg"		/*节点号,加油机,通道,串口等配置信息文件,IFSF专用*/
#define OILFILE         "etc/oil.cfg"            /*油品配置文件,modify by liys */
#define LOGFILE		"log/oil.log"		/*加油机日志文件.记录加油机启动和错误等普通日志*/
#define RUNFILE		"log/run.log"		/*加油机运行日志.记录加油机的运行信息.*/

#define PRODUCT_LEVEL_INTERVAL  100             // CSSI.校罐 液位至少变化 (PRODUCT_LEVEL_INTERVAL / 1000)mm 记录一次液位
#define PRODUCT_LEVEL_INTERVAL2  10             // CSSI.校罐 接近整厘米液位时的记录间隔

/*macro do segment must be interval seconds after last one*/
#define DO_BUSY_INTERVAL 1

static time_t last_time;
#define DO_INTERVAL(interval, segment)  do {\
	time_t tmp_time = time(NULL);\
	if (tmp_time > last_time + interval) {\
		{segment}\
		last_time = tmp_time;\
	}\
}while(0)

typedef struct{
	pid_t 	pid;
	char	flag[4];	//进程标志,本软件中用来重启该进程.
}Pid;

typedef struct{
	int	fd;
	int	port;
	unsigned char chn[MAX_PORT_CNT][MAX_CHANNEL_PER_PORT_CNT];
}Port;



/*定义共享内存的结构*/
typedef struct{
	unsigned char	chnLogUsed[MAX_CHANNEL_CNT];	/*显示逻辑通道是否使用*/
	unsigned char	portLogUsed[MAX_PORT_CNT];	/*显示串口是否使用*/
	unsigned char	chnLogSet[MAX_CHANNEL_CNT];	/*物理通道是否已经设置属性*/
	int		chnCnt;				/*总使用的通道数*/
	int		portCnt;			/*总使用的串口数*/
	unsigned char	wantLen[MAX_CHANNEL_CNT];       /*为通道在交易时使用.标识该逻辑通道希望后台返回的长度*/
	long	wantInd[MAX_CHANNEL_CNT];       /*为通道在交易时使用.标识在后台取得该逻辑通道的数据后,向消息队列写入消息的type*/
	long	minInd[MAX_CHANNEL_CNT];	/*每个通道的最小消息队列TYPE值*/
	long	maxInd[MAX_CHANNEL_CNT];	/*每个通道最大消息队列TYPE值*/
	long	useInd[MAX_CHANNEL_CNT];	/*为通道在交易时使用.标识该通道在交易中使用的消息队列type*/
						/*当通道接收后台数据超时时,useInd+1*/
	int	isCyc[MAX_CHANNEL_CNT];		/*通道的indtyp是否已经到达最大值并循环了*/
	int	errCnt;				/*此为程序中已超时队列type总数.用于回收程序回收*/
	long	errInd[MAX_ERR_IND];		/*此为程序中已超时队列type*/
	int	retryTim[MAX_ERR_IND];		/*回收程序回收已超时队列的次数*/
	int	sysStat;			/*系统运行标志. 0 正常运行 1系统终止 2更新配置文件(已经不用),3暂停*/
	unsigned char	addInf[MAX_CHANNEL_CNT][16];	/*附加信息.可以用来保存不同协议的一些个性数据*/
	int ppid;                               /*2007.8.27 parent proccess id of main proccess, used for remove ipc*/
	int sockCDPos;                          /*2007.9.10  position of socked fd of current  CD */
	unsigned char CDBuffer[1024];            /*2007.9.10  use  to send data to current  CD */
	size_t CDLen;                           /*2007.9.10  data length of to send data to current  CD */
	int CDtimeout;                          /*2007.9.10  time out to send data to current  CD */
	int LiquidAdjust;                       /*2009.04.13 Adjust for Liquid ,0: 不做校准, 1:做校准*/
	/* wayne_rcap2l_flag: 稳牌模块使用, 标记是否已经修改了串口采样中断周期 
	 * 0 - 否, 1 - 正在设置, 2 - 设置完成, 3 - IO程序版本低不支持修改命令
	 *
	 */
	int wayne_rcap2l_flag;
	int tr_fin_flag;                          // TCC: 交易结束标记, 标记有交易结束, 液位仪进程需要记录一次液位
	int tr_fin_flag_tmp;                      // TCC:
	int data_acquisition_4_TCC_adjust_flag;   // TCC: Tank capacity chart, !0 - start(1-first time), 0 - stop
	__u8 product_level[MAX_TP_PER_TLG][4];    // TCC: 校罐项目数据采集所需,记录上一次采集的液位高度
	int if_changed_prno;
} GunShm;


/*定义一个结构.用来维护运行时一个节点的信息*/
typedef struct{
	unsigned char	port;                             // 节点所在串口
	unsigned char	chnLog;                           // 面板所在的通道号(逻辑通道号)
	unsigned char	chnPhy;                           // 物理通道号 1-8 / 1-16, 针对同意串口
	unsigned char	panelCnt;                         // 面板数
	unsigned char	panelLog[MAX_FP_CNT];             // 逻辑面板号1-MAX
	unsigned char	panelPhy[MAX_FP_CNT];             // FP 的物理地址, 实际赋值对应油机协议中的枪号、机号 
	unsigned char 	cmd[MAX_ORDER_LEN+1];
	// 正在使用的逻辑枪号(1,2..), 挂枪不清零，不同于FP_DB的Current_Log_Noz, 作为索引要减一
	unsigned char	gunLog[MAX_FP_CNT];               
	unsigned char	gunPhy[MAX_FP_CNT];               // 正在使用的物理枪号
	unsigned char 	gunCnt[MAX_FP_CNT];               // 每个面板的枪数
	unsigned char	wantLen;                          // 需要后台返回的长度
	unsigned char	addInf[MAX_FP_CNT][16];           // 附加信息，各个模块用途不同
	unsigned char   newPrice[FP_PR_NB_CNT][4];        // Bin8 + Bcd6
			// 变价命令的新油价缓存于此，变价完成后清零
	size_t		cmdlen;
	int		fd;                               // serial port fd 
	int		indtyp;                           // 从后台取命令的消息队列,type值不会发生变化
	time_t          time_offline;                     // 记录油机离线恢复的最小时间点(保证CD能够知道油机离线过)
	int             safe_resume;            // 标记, 快速恢复在线还是确保CD知道油机离线了之后再上线, 1-安全, 0-快速
} PUMP;

extern GunShm	*gpGunShm;                                // 共享内存中的gpGunShm

#define  NODE_OFFLINE  0
#define  NODE_ONLINE   1
int ChangeNode_Status(int node, __u8 status);
int Update_Prod_No();
#endif


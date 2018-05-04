/*
 * 2017-08-25 - Chen Fu-ming <chenfm@css-intelligent.com>
 *    
 */
#include <sys/epoll.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <signal.h>
#include <sys/ipc.h>
#include <fcntl.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <sys/vfs.h>    /* or <sys/statfs.h> */
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <pthread.h> 
#include <poll.h> 
#include <jansson.h>
#include "log.h"
//#include "tty.h"
#include "dosignal.h"
#include "dosem.h"
#include "doshm.h"
#include "doio.h"
#include "domsgq.h"
#include "dostring.h"
#include "dotime.h"
#include "dodigital.h"
#include "dofile.h"
#include "cgidecoder.h"

#ifndef TRUE
#define TRUE			1
#endif
#ifndef FALSE
#define FALSE			0
#endif
/*-- 结构定义 ----------------------------------------------------------------*/
typedef unsigned char      __u8;
typedef unsigned short     __u16;
typedef unsigned int       __u32;
typedef unsigned long long __u64;

#define SINOPEC_VER          "v1.00.0005 - 2018.04.27"
#define SINOPEC_VERSION      "v1.00.0005 - 2018.04.27"


//epoll
#define  MAX_PC_NUM             4 //最多上位机个数
#define  BACKLOG             1
#define  MAX_EPOLL_EVENTS    (MAX_PC_NUM * 2)
#define  EPOLL_WAIT_TIMEOUT  -1                     // epoll_wait超时(ms), 无限期
#define  SNP_HEAD_LEN       8
#define  TCP_SEND_TRY_CNT    3
#define  TCP_RECV_TIMEOUT    3                      // TCP接收超时时间(秒)

/*-- 宏定义 ------------------------------------------------------------------*/
/*!
 * \brief  包裹宏，不处理errno, 若func执行返回负值则: 打印错误信息，并执行opt
 */
#define NOERR_FUNC(func, opt)    if ((func) < 0) {                  \
	LOG_ERROR("%s 执行失败[err:%s]", #func, strerror(errno));   \
	opt;                                                        \
}
//socket
#define SOCKERR_IO             -1
#define SOCKERR_CLOSED         -2
#define SOCKERR_INVARG         -3
#define SOCKERR_TIMEOUT        -4
#define SOCKERR_OK             0
#define SOCKET_INVALID         -1

//msg:
#define MAX_MSG_LEN       1024    //!< TCP发送缓冲区长度
#define MSG_HEAD_LEN  6 //8      //!< 消息头最大长度[]

//share mem:
#define SNP_SHM_INDEX 170    //产生访问整个共享内存css的关键字的索引值 
#define SNP_SEND_PC_SEM_INDEX  171 //send to pc share memary sem.
#define SNP_PORT_SEM_INDEX 104
#define SNP_TTY2TCP_INDEX   110
//#define SNP_SEM_INDEX 2105

#define SNP_5M    5000000
#define SNP_2M    2000000
#define SNP_500K  500000 
#define SNP_1K     1000
#define SNP_OFFLINE_MAXNUM  50000 //离线交易最大数
#define  INTERVAL 	2
#define  MIN_DISK_SPACE 	36 //MB//50

//configure:
#define SNP_WORK_DIR               "/home/App/ifsf/"                   //!< 应用工作目录
#define  SNP_CONFIG_FILE                     "/home/App/ifsf/etc/sinopec.cfg" //配置文件
#define  SNP_TCPPORT_FILE                     "/home/App/ifsf/etc/pub.cfg" //配置文件
//#define  SNP_CONFIG_TLG_FILE             "/home/App/ifsf/etc/dlbtlg.cfg"  //液位仪配置文件
#define SNP_LOG_FILE                            "/home/App/ifsf/log/sinopec.log" //日志文件
//#define RUN_WORK_DIR_FILE           "/var/run/sinopec_main.wkd"  //应用程序运行目录文件,用于GetWorkDir()函数.
//#define SNP_DSP_CFG_FILE                     "/home/App/ifsf/etc/dsp.cfg"  //IFSF
//#define  SNP_PRICE_CFG_FILE			 "/home/App/ifsf/etc/oil.cfg"  //IFSF
#define MAX_PATH_NAME_LENGTH 1024
//#define OFFLINE_TRANS_FILE			 "/home/Data/css_ofline_trans.data"  //脱机交易文件

#define get_sinopec_general_config(key, value)    SNP_get_general_config(SNP_CONFIG_FILE, key, value)

#define get_sinopec_general_config2(key, value)    SNP_get_general_config2(SNP_TCPPORT_FILE, key, value)




//--------------------------------------------------
//FCC channel
#define SNP_TIME_OUT_WRITE_PORT 2
#define MAX_CHANNEL_CNT    	64		/*最大通道数*/  //=MAX_CHANNEL_CNT
//#define MAX_CONVERT_CNT 	MAX_CHANNEL_CNT
#define MAX_PORT_CNT		6		/*最大串口数*/
#define MAX_CHANNEL_PER_PORT_CNT	16	/*每个串口最大通道数*/
//#define MAX_PANEL_CNT		4		/*每个油机最大的面板数*/
#define MAX_ERR_IND		200
#define MAX_ORDER_LEN		160
#define MAX_LOG_LEN		160
#define MAX_PORT_CMDLEN		255	        /*向串口写的数据的最大长度*/
#define MAX_GUN_CNT     	128             /*最大枪数*/

#define INDTYP			10000000
#define MAXADD		 	9999999
//#define TCP_HEAD_LEN		4
//#define TCP_TIME_OUT		3
//system status
#define RUN_PROC		0		/*正常状态*/
#define STOP_PROC 		1		/*系统停止*/
#define RESTART_PROC 		2		/*系统重启*/
#define PAUSE_PROC		3		/*暂停*/

#define PAUSE_FOR_TTY		1		/*暂停时tty暂停的时间*/

//--------------------------------------------------------------------------------
//日志,调用前必须初始化日志级别_SetLogLevel和日志文件句柄_SetLogFd
//------------------------------------------------------------------------------
#define SNP_DEFAULT_LOG_LEVEL 1                           //默认日志级别
#define SNP_MAX_VARIABLE_PARA_LEN 1024 
#define SNP_MAX_FILE_NAME_LEN 1024 
#define SNP_DEBUG 0
static char gsFileName[SNP_MAX_FILE_NAME_LEN + 1];	/*记录调试信息的文件名*/
static int giLine;								/*记录调试信息的行*/
 //自定义日志级别,0:什么也不做;1:只打印,不记录;2:打印且记录;
static int giLogLevel;                                             //日志调试级别
static int giFd;                                                       //日志文件句柄


//自定义分级文件日志,见_SNP_LevelLog.
#define SNP_LevelLog  RunLog //SetFileAndLine( __FILE__, __LINE__ ),          //__ShowMsg
#define SNP_FileLog   RunLog  //SetFileAndLine( __FILE__, __LINE__ ), 


time_t result;
struct tm *my_time;

/*-- 结构定义 ----------------------------------------------------------------*/
typedef struct {
	int msg_len;
	int  sock_fd;
	__u8 pc_id;
	__u8 msg[MAX_MSG_LEN];
} SNP_SEND_INFO;
typedef struct {
	int  sock_fd;        // TCP socket fd
	__u8 pc_id;         // PC ID
	__u8 logon_flag;    //
} SNP_CONN_TIEM;
/*
 * Guojie Zhu @ 8.20
 * 修改ReadTTY中调用AddJustLen时传入的参数ChnCmd[].len为 &ChmCmd[].len
 */
char SNP_alltty[][11] =
{
	"/dev/ttyS1",
	"/dev/ttyS2",
	"/dev/ttyS3",
	"/dev/ttyS4",
	"/dev/ttyS5",
	"/dev/ttyS6"
};
typedef struct {
	unsigned char	wantlen;
	int indtyp;
	int len;
	unsigned char	cmd[MAX_ORDER_LEN+1];
}SNP_CHNCMD;
typedef struct {
	__u8 head;
	__u8 dst;
	__u8 src;
	__u8 token;
	__u8 lenBcd[2];
	unsigned short int  len;
//	__u8 res1;//for 8bytes
//	__u8 res2;//for 8bytes
} SNP_DATA_HEAD;
typedef struct{
	unsigned char node;
	//logical channel number in a PCD, the number is no. of the linked tag which signed  at back of FCC.	
	unsigned char chnLog; 
	unsigned char nodeDriver; //dispenser driver type,this determine how to initial and wich lischnxxx to use.	
	//unsigned char cntOfFps; //count of fuelling points
	//unsigned char cntOfNoz; //all count of Nozzles  in the dispenser.
	//unsigned char cntOfProd;//all count of oil products in the dispenser.
	//count of Nozzles  in each FP of the dispenser, if there is not  2nd,3rd,4th FP then which set 0.
	//unsigned char cntOfFpNoz[4]; 
	//runtime information
	unsigned char serialPort; //use which serial port in main board, it's zero in file, only used it in runtime.
	unsigned char chnPhy;  //phisical channel number at a serial port , it's zero in file, only used it in runtime.
	unsigned char isUse;
} SNP_NODE_CHANNEL;
typedef struct {
	int ppid;
	int sysStat;/*系统运行标志. 0 正常运行 1系统终止 2更新配置文件(已经不用),3暂停*/

	int config_version;//
	SNP_SEND_INFO  snp_fcc_msg; // TODO 以后根据并发需要改为队列
	SNP_CONN_TIEM snp_conn[MAX_PC_NUM];	
	
	//for fcc channel
	unsigned char	chnLogUsed[MAX_CHANNEL_CNT];	/*显示逻辑通道是否使用*/
	unsigned char	portLogUsed[MAX_PORT_CNT];	/*显示串口是否使用*/
	unsigned char	chnLogSet[MAX_CHANNEL_CNT];	/*物理通道是否已经设置属性*/
	unsigned char portsChns[MAX_CHANNEL_CNT];  /*1~6每个串口所拥有的通道数*/
	int		chnCnt;				/*总使用的通道数*/
	int		portCnt;			/*总使用的串口数*/
	//unsigned char	wantLen[MAX_CHANNEL_CNT];       /*为通道在交易时使用.标识该逻辑通道希望后台返回的长度*/
	//long	wantInd[MAX_CHANNEL_CNT];       /*为通道在交易时使用.标识在后台取得该逻辑通道的数据后,向消息队列写入消息的type*/
	//long	minInd[MAX_CHANNEL_CNT];	/*每个通道的最小消息队列TYPE值*/
	//long	maxInd[MAX_CHANNEL_CNT];	/*每个通道最大消息队列TYPE值*/
	//long	useInd[MAX_CHANNEL_CNT];	/*为通道在交易时使用.标识该通道在交易中使用的消息队列type*/
						/*当通道接收后台数据超时时,useInd+1*/
	//int	isCyc[MAX_CHANNEL_CNT];		/*通道的indtyp是否已经到达最大值并循环了*/
	//int	errCnt;				/*此为程序中已超时队列type总数.用于回收程序回收*/
	//long	errInd[MAX_ERR_IND];		/*此为程序中已超时队列type*/
	//int	retryTim[MAX_ERR_IND];		/*回收程序回收已超时队列的次数*/
	//unsigned char	addInf[MAX_CHANNEL_CNT][16];	/*附加信息.可以用来保存不同协议的一些个性数据*/
	
	//节点,逻辑通道,串口和物理通道.
	SNP_NODE_CHANNEL   node_channel[MAX_CHANNEL_CNT];   // 以 Node - 1 为索引
	//标志下面的设备数组哪个在线，在线节点号,离线0, 本PCD设备不用心跳.   
	unsigned char node_online[MAX_CHANNEL_CNT + 1];//最后一个为液位仪的心跳
	//串口运行时数据for tcp:
	unsigned char serialPort[MAX_PORT_CNT];
	int serialPort_fd[MAX_PORT_CNT];
} SNP_SHM;

typedef struct{
	pid_t 	pid;
	char	flag[4];	//进程标志,本软件中用来重启该进程.
}SNP_Pid;



/*全局变量区*/
//proc
unsigned char	SNP_cnt =0;		/*进程最大数*/
SNP_Pid*	SNP_gPidList;
char	SNP_isChild = 0;
static unsigned char flag[31];
extern int errno;
SNP_SHM * gpSNP_shm;
//epoll
static int tcp_listen_fd = 0;
static int epfd = -1;
static struct epoll_event evs, events[MAX_EPOLL_EVENTS];

extern int errno;



#define CHILD_PROC_CNT 3
static int SNP_StartProc();
//static void SNP_WhenSigChld( int sig );
static int SNP_WaitMyChild( /*int sig */);
static int SNP_save_version();
int SNP_WriteToPort( int fd, int chnLog, int chnPhy, const __u8 *buff, size_t len, int flag, char wantLen, int timeout );
static char *SNP_AttachTheShm( int index );
void SNP_Misc_Process();

#if 0
/*附加长度判断.判断已取得的长度是否足够返回给前端*/
/*在所有返回不固定长度数据的交易中,需要在此处增加处理*/
/*系统设计时认为所有交易均返回固定长度的数据.后来发现未必如此,不得不加此补丁*/
/*造化如斯,如之奈何............*/

/*
 * Guojie zhu @ 8.20
 * 将AddJustLen的参数cmdlen由int类型改为int *; cmd 改为非const类型.
 */

/*
 * 返回码:
 *      0   取到了所有数据.可以返回.
 *      1   未取到数据.此时会更改需要的长度字段
 */

//int AddJustLen(int chnLog, const unsigned char *cmd, int cmdlen, int wantInd, int mark  )
int SNP_AddJustLen(int chnLog, unsigned char *cmd, int *cmdlen, int wantInd, int mark)
{
	int ret;
	int wantlen = 0;   // for case 0x07
	static int idx = 0;   // for case 0x08
	static int left_fa = 0;  // for case 0x08, 如果 left_fa == 1 表示数据中出现了一个FA(非转义FA)
	static int real_wantlen = 0;  // 真正应该返回的数据长度, 不包括转义FA

	switch( mark ) {
		case 0x01:	/*长吉加油机取泵码的交易.返回的长度是油机(或面板)总枪数*30+4.*/
			if (cmd[(*cmdlen) - 1] != 0xf0 ) {
				if (wantInd == gpSNP_shm->wantInd[chnLog-1] ) {
					/*如果此时前端已经超时并且已经更改成了下一个交易的返回值......*/
					gpSNP_shm->wantLen[chnLog-1] += 30;
					return 1;
				}
			}
			break;
		case 0x03:	/*太空协议查询油机状态用*//*第二位06表示非加油状态,返回8位.第二位0C为加油状态,返回14位*/
			if (wantInd == gpSNP_shm->wantInd[chnLog-1]) {	/*消息没超时*/
				if (gpSNP_shm->wantLen[chnLog - 1] < cmd[1] + 2) {
					gpSNP_shm->wantLen[chnLog - 1] = cmd[1] + 2;
					gpSNP_shm->addInf[chnLog - 1][0] = 0x00;
					return 1;
				} 
			}
			break;
		case 0x04: /*榕兴协议。取油品累计时用。第三位是数据长度.后面＋2是crc*/
			if (wantInd == gpSNP_shm->wantInd[chnLog-1]) {	/*消息没超时*/
				gpSNP_shm->wantLen[chnLog-1] = 3 + cmd[2] + 2;
				gpSNP_shm->addInf[chnLog - 1][0] = 0x00;
				return 1;
			}
			break;
		case 0x06:		//2006.02.17 for 35/39 return characters
			if( cmd[2] == 0x1e ) {
				if(wantInd == gpSNP_shm->wantInd[chnLog-1] ) {	/*消息没超时*/
					if( gpSNP_shm->wantLen[chnLog-1] == 35 ) {
						return 0;
					} else {
						gpSNP_shm->wantLen[chnLog-1] = 35;
						return 1;
					}
				}
			} else {
		      // 	if( cmd[2] == 0x22 ) {  // 移除这个判断,避免错误数据影响接收,GuojieZhu@2009.12.14
				if(wantInd == gpSNP_shm->wantInd[chnLog-1] )	/*消息没超时*/
				{
					if( gpSNP_shm->wantLen[chnLog-1] == 39 ) {
						return 0;
					} else {
						gpSNP_shm->wantLen[chnLog-1] = 39;
						return 1;
					}
				}
			}
			break;
		case 0x07:             // 稳牌
			switch (cmd[1] & 0xF0) {  // 判断 CTRL
			case 0x50:     // NAK
			case 0x70:     // EOT
			case 0xc0:     // ACK
//				if (cmd[2] == 0xFA) {        // SF
					gpSNP_shm->addInf[chnLog - 1][0] = 0x00;
					return 0;
//				}
				break;
			case 0x30:
				if (gpSNP_shm->addInf[chnLog - 1][1] == 0x00) {
//					RunLog("-1---------- %s ", Asc2Hex(cmd, gpGunShm->wantLen[chnLog - 1]));
					(gpSNP_shm->wantLen[chnLog - 1])++;      // 取第一块DATA的长度 
					gpSNP_shm->addInf[chnLog - 1][1] = 0x0F;
					return 1;
				} else if (gpSNP_shm->addInf[chnLog - 1][1] == 0x0F) {
//					RunLog("-2---------- %s ", Asc2Hex(cmd, gpGunShm->wantLen[chnLog - 1]));
					gpSNP_shm->wantLen[chnLog - 1] += cmd[3] + 4;
					gpSNP_shm->addInf[chnLog - 1][1] = 0xF0; // 取第一块DATA
					return 1;    // 继续接收
				} else {
//					RunLog("-3---------- %s, ", Asc2Hex(cmd, gpGunShm->wantLen[chnLog - 1]));
					wantlen = gpSNP_shm->wantLen[chnLog - 1];
					// 这里要判断数据中是否有0xFA, 如果有要把前面的数据连接符去掉
					// 因稳牌上传数据都是BCD格式, 所以0xFA只可能出现在CRC校验中
					if (cmd[wantlen - 3] == 0xFA &&  cmd[wantlen - 4] == 0x10) {
						// 跳过数据连接码0x10(SF 覆盖 DLE 储存缓冲区)
						cmd[wantlen - 4] = cmd[wantlen - 3];
						cmd[wantlen - 3] = cmd[wantlen - 2];
						cmd[wantlen - 2] = cmd[wantlen - 1];
						(*cmdlen)--;   // 加收一个字节
//					RunLog("-------------cmdlen: %d -------", *cmdlen);
//					RunLog("-4---------- %s, ", Asc2Hex(cmd, gpGunShm->wantLen[chnLog - 1]));
						return 1;
					} else if (cmd[wantlen - 2] == 0xFA && cmd[wantlen - 3] == 0x10) {
						// 跳过数据连接码0x10(SF 覆盖 DLE 储存缓冲区)
						cmd[wantlen - 3] = cmd[wantlen - 2];
						cmd[wantlen - 2] = cmd[wantlen - 1];
						(*cmdlen)--;   // 加收一个字节
//					RunLog("-------------cmdlen: %d -------", *cmdlen);
//					RunLog("-4---------- %s, ", Asc2Hex(cmd, gpGunShm->wantLen[chnLog - 1]));
						return 1;
					} else if (cmd[wantlen - 1] == 0xFA && cmd[wantlen - 2] == 0x03) {
						gpSNP_shm->addInf[chnLog - 1][0] = 0x00;
						gpSNP_shm->addInf[chnLog - 1][1] = 0x00;
						return 0;    // 接收完毕
					} else {
						gpSNP_shm->wantLen[chnLog - 1] += cmd[wantlen - 3] + 2;
						return 1;    // 继续接收
					}
				}
				break; 
			default:
				RunLog("控制字符错误 [%02x]", cmd[1]);
				gpSNP_shm->addInf[chnLog - 1][0] = 0x00;
				return 0;
			}
			break;
		case 0x08:             // 恒山自助油机
			if (wantInd == gpSNP_shm->wantInd[chnLog - 1]) {
//					RunLog("-1111111111 left_fa: %d, cmdlen: %d, %s, ", 
//						left_fa, *cmdlen, Asc2Hex(cmd, gpGunShm->wantLen[chnLog - 1]));
				if (gpSNP_shm->wantLen[chnLog - 1] == 6) {
					real_wantlen = cmd[4] + 7;
					gpSNP_shm->wantLen[chnLog - 1]++;
//					RunLog("-----------real_wantLen: %d --------", real_wantlen);
//					RunLog("-1111111111 %s, ", Asc2Hex(cmd, gpGunShm->wantLen[chnLog - 1]));
					return 1;
				} else if ((*cmdlen) == real_wantlen && 
					((cmd[*cmdlen - 1] != 0xFA) || 
					 (cmd[*cmdlen - 1] == 0xFA && left_fa == 1))) {
//					RunLog("-=-=-=-=-=-=-==cmdlen: %d, idx: %d, wantLen: %d -=-=--==-=-=-=",
//						       	*cmdlen, idx, gpGunShm->wantLen[chnLog - 1]);
					gpSNP_shm->addInf[chnLog - 1][0] = 0x00;
					left_fa = 0;
					real_wantlen = 0;
					return 0;
				} else {
//					RunLog("----------idx: %d,  %02x----------------, ", idx, cmd[idx]);
//					RunLog("----------idx: %d,  %02x----------------, ", *cmdlen, cmd[*cmdlen - 1]);
//					RunLog("-1111111111 %s, ", Asc2Hex(cmd, gpGunShm->wantLen[chnLog - 1]));
					if (cmd[*cmdlen - 1] == 0xFA) {
//					RunLog("-1-left_fa: %d--------- %s, ", left_fa,
//						       	Asc2Hex(cmd, gpGunShm->wantLen[chnLog - 1]));
						if (left_fa == 0) {
							left_fa = 1;
							(*cmdlen)--;   // ignore this byte
							/*
					RunLog("-2-left_fa: %d, cmdlen: %d, wantLen: %d -=-=--==-=-=-=",
							left_fa, *cmdlen, gpGunShm->wantLen[chnLog - 1]);
							*/
							return 1;      // 直接返回, wantLen不能增加
						} else {
							left_fa = 0;
							/*
					RunLog("-3-left_fa: %d, cmdlen: %d, wantLen: %d -=-=--==-=-=-=",
							left_fa, *cmdlen, gpGunShm->wantLen[chnLog - 1]);
							*/
						}
					}

					gpSNP_shm->wantLen[chnLog - 1]++;
					return 1;
				}
			}
			left_fa = 0;
			break;
		case 0x09:             // 豪升变长
			if (wantInd == gpSNP_shm->wantInd[chnLog - 1]) {
//				gpSNP_shm->wantLen[chnLog - 1] = (cmd[4] * 256) + cmd[5] + 8;
				gpSNP_shm->wantLen[chnLog - 1] = cmd[5] + 8; // 长度不会大于255, 所以只用cmd[5]
				gpSNP_shm->addInf[chnLog - 1][0] = 0x00;
				return 1;
			}
			break;
		case 0x0F:             // 佳力佳变长
			if (wantInd == gpSNP_shm->wantInd[chnLog - 1]) {
				gpSNP_shm->wantLen[chnLog - 1] = cmd[2] + 3;
				gpSNP_shm->addInf[chnLog - 1][0] = 0x00;
				return 1;
			}
			break;
		case 0x10:             // 老长空卡机联动
			if (wantInd == gpSNP_shm->wantInd[chnLog - 1]) {
				if (gpSNP_shm->wantLen[chnLog - 1] == 2) {
					real_wantlen = cmd[1] + 2;
					gpSNP_shm->wantLen[chnLog - 1]++;
					return 1;
				} else if ((*cmdlen) == real_wantlen && 
					((cmd[*cmdlen - 1] != 0xFA) || 
					 (cmd[*cmdlen - 1] == 0xFA && left_fa == 1))) {
					gpSNP_shm->addInf[chnLog - 1][0] = 0x00;
					left_fa = 0;
					real_wantlen = 0;
					return 0;
				} else {
					if (cmd[*cmdlen - 1] == 0xFA) {
						if (left_fa == 0) {
							left_fa = 1;
							(*cmdlen)--;   // 忽略这个字节
							return 1;      // 直接返回, wantLen不能增加
						} else {
							left_fa = 0;
						}
					}

					gpSNP_shm->wantLen[chnLog - 1]++;
					return 1;
				}
			}
			left_fa = 0;
			break;
		default:
			break;
	}
	return 0;
}
#endif
//fcc channel:
#define SNP_REOPENTTY()	{                                         \
	gpSNP_shm->sysStat = PAUSE_PROC;	/*让通道处理暂停*/        \
                                                                  \
	sleep(PAUSE_FOR_TTY);	/*停止一段时间*/                  \
                                                                  \
	close(fd);	/*关闭打开的tty*/                         \
	fd = SNP_OpenSerial_Read( ttynam ); /*重新打开tty*/                \
	if (fd < 0) {                                             \
		UserLog("打开设备[%s]失败", ttynam);              \
		return -1;                                        \
	}                                                         \
	UserLog("重新打开串口[%s].fd=[%d]", ttynam, fd);          \
                                                                  \
	maxFd = 0;                                                \
	FD_ZERO(&set);                                            \
	FD_SET(fd, &set);                                         \
                                                                  \
	if (fd >= maxFd) {                                        \
		maxFd = fd + 1;                                   \
	}                                                         \
	memcpy( &setbak, &set, sizeof(fd_set) );                  \
                                                                  \
	gpSNP_shm->sysStat = RUN_PROC;	/*恢复通道处理*/          \
}

int SNP_getSerialPortFd(unsigned char chn){
	int i,j;
	for(i=0;i<MAX_CHANNEL_CNT;i++){
		if(gpSNP_shm->node_channel[i].chnPhy!=chn)
			continue;

		return gpSNP_shm->serialPort_fd[gpSNP_shm->node_channel[i].serialPort-1];
		//gpSNP_shm->serialPort_fd[gpSNP_shm->node_channel[i].serialPort-1] 
			
	}
	return -1;
	
}


/*
 * func: 设定串口属性—通讯速率、校验位等
 */
static int set_chn_boardrate(int fd, unsigned char chnLog, unsigned char chnPhy, int boardrate)
{
	int ret;
	unsigned char cmdlen = 7;
	unsigned char cmd[MAX_ORDER_LEN+1];

	cmd[0] = 0x1C;
	cmd[1] = 0x52;
	cmd[2] = 0x30|(chnPhy-1);
	switch(boardrate){
		case 9600:
			cmd[3] = 0x03;	//9600bps
			break;
		case 4800:
			cmd[3] = 0x06;
			break;
		default:
			RunLog("boardrate[%d] 错误，仅支持9600/4800", boardrate);
			break;
	}
	//cmd[4] = 0x01<< ((chnPhy - 1) % 8);	//EVEN parity
	cmd[4] = 0x10;
	cmd[5] = 0x10;
	cmd[6] = 0x00;
	//cmd[6] = 0x01<< ((chnPhy - 1) % 8);
	cmdlen = 7;

	ret = SNP_WriteToPort(fd, chnLog, chnPhy, cmd, cmdlen, 0, 0,SNP_TIME_OUT_WRITE_PORT);
	if( ret < 0 ) {
		return -1;
	}

	RunLog( "<加油机>通道[%d], 设置串口属性成功(偶校验) fd=%d", chnPhy, fd);
	return 0;
}

void SNP_initSerialPortForTcp(){
	int i,j, fd, ret;
	for(i=0;i<MAX_CHANNEL_CNT;i++){
		if(gpSNP_shm->node_channel[i].serialPort<=0)
			continue;
		if(gpSNP_shm->node_channel[i].serialPort > MAX_PORT_CNT){
			RunLog("串口号[%s]错误", gpSNP_shm->node_channel[i].serialPort);
			continue;
		}
		gpSNP_shm->serialPort[gpSNP_shm->node_channel[i].serialPort-1] 
			= gpSNP_shm->node_channel[i].serialPort;
		//for(j=0;j<MAX_PORT_CNT;j++){}
	}
	for(i=0;i<MAX_PORT_CNT;i++){
		if(gpSNP_shm->serialPort[i] <= 0)
			continue;
		if(gpSNP_shm->serialPort[i] > MAX_PORT_CNT){
			RunLog("串口号[%s]错误", gpSNP_shm->serialPort[i]);
			continue;
		}
		gpSNP_shm->serialPort_fd[i]= SNP_OpenSerial_Write(SNP_alltty[gpSNP_shm->serialPort[i]- 1]);
		if (gpSNP_shm->serialPort_fd[i] < 0) {
			RunLog("打开串口[%s]失败", SNP_alltty[gpSNP_shm->serialPort[i]- 1]);
			//return -1;
		} else {
			RunLog("打开串口[%s]成功 fd=%d", SNP_alltty[gpSNP_shm->serialPort[i]- 1], gpSNP_shm->serialPort_fd[i]);
		}
	}
/*
	for (i = 0; i < MAX_CHANNEL_CNT; i++) {
		if (gpSNP_shm->chnLogUsed[i]) {
			fd = SNP_getSerialPortFd(i+1);
			ret = set_chn_boardrate(fd, i+1, 9600);
			if (ret == 0)
				gpSNP_shm->chnLogSet[i] = 1;
		}
	}
*/
}

//int send_xml_msg(gpSNP_shm->snp_fcc_msg.sock_fd, gpSNP_shm->snp_fcc_msg.msg, gpSNP_shm->snp_fcc_msg.msg_len);
int SNP_TcpSendDataToChannel( const __u8 *str, size_t len, char node )//, char chnLog, long indtyp
{
	int fd;
	int rc;
	int ret;
	unsigned char chnlog, chnphy;

#ifndef MAX_NODE_CNT
#define MAX_NODE_CNT 64
#endif

	int nodecnt;

	for (nodecnt = 0; nodecnt < MAX_NODE_CNT; nodecnt++) {
		if (gpSNP_shm->node_channel[nodecnt].node == node) {
			break;
		}
	}
	if (nodecnt >= MAX_NODE_CNT) {
		LOG_ERROR("查找节点号[%d]失败", node);
		return -1;
	}

	chnlog = gpSNP_shm->node_channel[nodecnt].chnLog;
	chnphy = gpSNP_shm->node_channel[nodecnt].chnPhy;
	fd = gpSNP_shm->serialPort_fd[gpSNP_shm->node_channel[nodecnt].serialPort-1];

	LOG_ERROR("node:%d, chnlog:%d, chnphy:%d, fd:%d", node, chnlog, chnphy, fd);
	
	//fd = SNP_getSerialPortFd(chn);
	if(fd<0) {
		RunLog("通道[%d]查找fd失败", chnlog);
		return -1;
	} else {
		RunLog("通道[%d]查找到fd=%d", chnlog, fd);
	}

	if (gpSNP_shm->chnLogSet[chnlog-1] == 0) {
		ret = set_chn_boardrate(fd, chnlog, chnphy, 9600);
		if (ret == 0)
			gpSNP_shm->chnLogSet[chnlog-1] = 1;
	}

	rc = SNP_WriteToPort(fd, chnlog, chnphy, str,len, 1, 0, SNP_TIME_OUT_WRITE_PORT);
	//RunLog( "SendDataToChannel	[%s]", Asc2Hex(str,len) );
	//RunLog( "senddatatochannel.通道号[%d]逻辑通道号[%d],数据[%s]", chn, chnLog, Asc2Hex(str,len) );
	return rc;//SendMsg( GUN_MSG_TRAN, str, len,indtyp, 3 );
}
int SNP_TtySendDataToTcp( char *str, size_t len)
{
	//RunLog( "SendDataToChannel	[%s]", Asc2Hex(str,len) );
	//RunLog( "senddatatochannel.通道号[%d]逻辑通道号[%d],数据[%s]", chn, chnLog, Asc2Hex(str,len) );
	if(gpSNP_shm->snp_fcc_msg.msg_len<=0){
		Act_P(SNP_SEND_PC_SEM_INDEX);
		memcpy(gpSNP_shm->snp_fcc_msg.msg,str,len);
		gpSNP_shm->snp_fcc_msg.msg[len] = 0;
		gpSNP_shm->snp_fcc_msg.msg_len = len;
		Act_V(SNP_SEND_PC_SEM_INDEX);
		return len;//SendMsg( GUN_MSG_TRAN, str, len,indtyp, 3 );
	}
	return -1;
}

int SNP_TtySendDataToTcp_msq( char *str, size_t len)
{
	return SendMsg(SNP_TTY2TCP_INDEX, str, len, 1, 3);
}


int SNP_OpenSerial_Read( char *ttynam )
{
	int fd;
	struct termios termios_new;

	fd = open (ttynam, O_RDONLY | O_NOCTTY | O_NONBLOCK);
	if( fd < 0 )
	{
		UserLog( "打开设备[%s]失败", ttynam );
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

int SNP_OpenSerial_Write( char *ttynam )
{
	int fd;
	struct termios termios_new;

	fd = open (ttynam, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if( fd < 0 )
	{
		UserLog( "打开设备[%s]失败", ttynam );
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


int SNP_ClearSerial( int fd )
{
	int n;
	char	str[3];

	str[2] = 0;

	while(1)
	{
		n = read( fd, str, 2 );
		if( n == 0 )
			return 0;
		else if( n < 0 && errno == EAGAIN ) /*无数据.返回*/
			return 0;
		else if( n < 0 && n != EINTR ) /*出错并且不是被信号打断*/
			return -1;
		/*其他情况继续读*/
	}
}

ssize_t SNP_Read2( int fd, char *vptr )
{
	size_t	nleft;
	ssize_t	nread;
	char	*ptr;

	ssize_t n;
	n = 0;

	/*读第一位*/
	ptr = vptr;
	while(1)
	{
		if ( (nread = read(fd, ptr, 1)) < 0 )
		{
			if( errno == EAGAIN )	/*无数据*/
				continue;
			else if (errno == EINTR)	/*被信号打断*/
				nread = 0;
			else
				return(-1);
		}
		else if (nread == 0)
			return n;				/* EOF */

		if( ptr[0] != 0x00 )	/*0x00抛弃。其他认为是正常数据*/
		{
			n = 1;
			break;
		}
		UserLog( "throw 0x00 away" );
	}

	/*读第二位*/
	ptr = vptr+1;
	while(1)
	{
		if ( (nread = read(fd, ptr, 1)) < 0 )
		{
			if( errno == EAGAIN )	/*无数据*/
				continue;
			if (errno == EINTR)	/*被信号打断*/
				nread = 0;
			else
				return(-1);
		}
		else if (nread == 0)
			return n;				/* EOF */

		n = 2;
		break;

	}
	return n;
}

int SNP_WriteToPort( int fd, int chnLog, int chnPhy, const __u8 *buff, size_t len, int flag, char wantLen, int timeout )
{
	char	cmd[MAX_ORDER_LEN+1];
	int	cnt;
	int	ret;

	if (flag == 1) {	/*flag=1表示需要发送时组包*/
		cmd[0] = 0x1B;
		cmd[1] = 0x30+chnPhy-1;
		if( len > MAX_PORT_CMDLEN ) {
			RunLog( "向端口写的数据超过最大长度[%d]", MAX_PORT_CMDLEN );
			return -1;
		}
		cmd[2] = 0x00+len;
		memcpy( cmd+3, buff, len );
		cnt = len+3;
	} else {
		cnt = len;
		memcpy( cmd, buff, len );
	}

	//if( 1 == atoi(SHOW_FCC_SEND_DATA)){
		RunLog("CHN[%02d]-->Pump: %s(%d)", chnLog,Asc2Hex(buff, len), len);
		RunLog("cmd[0-2] : %02x%02x%02x", cmd[0], cmd[1], cmd[2]);
	//}

	if (wantLen != 0 ) {	/*期待返回值*/
		//gpSNP_shm->wantLen[chnLog-1] = wantLen;
		//gpSNP_shm->wantInd[chnLog-1] = gpSNP_shm->useInd[chnLog-1];
	}

	Act_P( SNP_PORT_SEM_INDEX );	/*对端口的写不知道是不是原子操作。用PV原语保险点*/
	ret = WriteByLengthInTime(fd, cmd, cnt, timeout );
	Act_V( SNP_PORT_SEM_INDEX );

	return ret;
}



/*监听端口*/
int SNP_ReadTTY( int portnum )
{
	int		i;
	int		j;
	int		k;
	int		n;
	int		fd;
	char	ttynam[30];
	char	chn;
	int	len;
	long	indtyp;
	char	retlen;	         /*返回长度*/
	int		ret;
	SNP_DATA_HEAD head;
	//int		gunCnt;	/*总枪数*/
	unsigned char	buf[3];
	unsigned char	chnPhy;	/*物理通道号*/
	unsigned char	chnLog;	/*逻辑通道号*/
	unsigned char port[MAX_CHANNEL_PER_PORT_CNT];
	SNP_CHNCMD ChnCmd[MAX_CHANNEL_CNT];	/*每个通道对应一个结构*/
	int		maxFd;
	fd_set	set;
	fd_set	setbak;	/*select返回时set被修改，这是在select之前保存set的值*/

	memset( ChnCmd, 0, sizeof(ChnCmd) );

	//gunCnt = gpGunShm->gunCnt;	/*取枪总数*/

	j = 0;

	strcpy( ttynam, SNP_alltty[portnum-1] );			/*取设备名*/
	fd = SNP_OpenSerial_Read( ttynam );
	if( fd < 0 ) {
		/*错误处理*/
		UserLog( "打开设备[%s]失败", ttynam );
		return -1;
	}

	UserLog( "打开串口ReadOnly[%s].fd=[%d]", ttynam, fd );

	/*搜索共享内存,查找该串口下的物理通道号chnPhy和逻辑通道号chnLog*/
	/*每个串口的物理通道号chnPhy从1到8.这里用物理通道号chnPhy-1作为索引.存放该物理通道号对应的逻辑通道号chnLog*/
	
	for(i=0; i<MAX_CHANNEL_CNT; i++){
		if(gpSNP_shm->node_channel[i].serialPort == (unsigned char )portnum){
			chnLog = gpSNP_shm->node_channel[i].chnLog;
			chnPhy = gpSNP_shm->node_channel[i].chnPhy;
			port[chnPhy-1] = chnLog;
			UserLog( "串口[%d]监听逻辑通道[%d]物理通道[%d]的数据", portnum, chnLog, chnPhy );
		}
	}	

	len = 0;
	chn = 0;
	chnLog = 0;

	/*将fd加入到要监听的描述符*/
	maxFd = 0;
	FD_ZERO( &set );
	FD_SET( fd, &set );

	if (fd >= maxFd) {
		maxFd = fd + 1;
	}
	memcpy( &setbak, &set, sizeof(fd_set) );

	while(1) {
		ret = select( maxFd, &set, NULL, NULL, NULL );  /*等到至少一个描述符准备好*/
		if( ret < 0 ) {
			if( errno == EINTR ) {
			    /*被信号打断*/
			    continue;
			} else {
			    UserLog( "监听串口出错" );
			    return -1;
			}
		} else if( ret == 0 ) {     /*超时？不会吧？*/
			continue;
		}
		indtyp = 0;
		while (1) {
			n = ReadByLength( fd, buf, 2 );
			buf[n] = 0;
			if( n < 0 ) {
				RunLog( "读串口数据失败" );
				SNP_REOPENTTY();

				break;
			} else if(n != 2) {
				RunLog( "接收数据有误n=[%d]",n );
				SNP_REOPENTTY();

				break;
			} else {		/*读到了数据.*/
				if (buf[0] < 0x30 || buf[0] > 0x3f) {
					RunLog( "Get Data buf[0] = [%02x], buf[1] = [%02x]", buf[0], buf[1] );
					RunLog( "非法数据" );

					SNP_REOPENTTY();

					break;
				}
				RunLog( "Get Data buf[0] = [%02x], buf[1] = [%02x]", buf[0], buf[1] );

 				chn = buf[0] - 0x30 + 1;

				chnLog = port[chn-1];
				//retlen = 8;//gpSNP_shm->wantLen[chnLog-1];			/*应返回的长度*/
				//indtyp = 0;//gpSNP_shm->wantInd[chnLog-1];			/*返回的消息队列键值*/

				//if( retlen == 0 ) {
					//RunLog( "异常数据.Read[0x%02x][0x%02x].wantLen[%d].getLen[%d]", \
					//	buf[0], buf[1], ChnCmd[chnLog-1].wantlen, ChnCmd[chnLog-1].len );
					//continue;
				//}

				/*检查该通道是否已经有数据.有则将数据保存,无则从共享内存管理信息*/
				//if (ChnCmd[chnLog-1].wantlen != 0 && ChnCmd[chnLog-1].indtyp != indtyp) { /*保存的消息已超时*/
				//	memset( &ChnCmd[chnLog-1], 0, sizeof(ChnCmd[chnLog-1]));
				//}
				if(ChnCmd[chnLog-1].wantlen == 0){
					ChnCmd[chnLog-1].wantlen = MSG_HEAD_LEN;
					ChnCmd[chnLog-1].indtyp = 1;
					//data errer, data frame is cut?
					if(buf[1]!=0xfa){
						RunLog("通道(%d)数据错误,数据帧可能被截断? 本字节为(%02x)", chnLog, buf[1]);
						continue;
					}
				}else if(ChnCmd[chnLog-1].wantlen == MSG_HEAD_LEN && ChnCmd[chnLog-1].len == MSG_HEAD_LEN){
					//head = (SNP_DATA_HEAD *)ChnCmd[chnLog-1].cmd;
					memcpy((unsigned char*)&head, ChnCmd[chnLog-1].cmd, sizeof(SNP_DATA_HEAD));
					head.len = (head.lenBcd[0]>>4)*1000 + (head.lenBcd[0]&0x0f)*100 + (head.lenBcd[1]>>4)*10 + (head.lenBcd[1]&0x0f);
					ChnCmd[chnLog-1].wantlen = MSG_HEAD_LEN+head.len+2;
					ChnCmd[chnLog-1].indtyp = 2;
				}else{//after 8 bytes data					
				}
				ChnCmd[chnLog-1].cmd[ChnCmd[chnLog-1].len] = buf[1];
				ChnCmd[chnLog-1].len++;
#if 0
				RunLog("##chnLog:%d, wantlen: %d, len: %d, received: %s(%d)",
					       	chnLog, ChnCmd[chnLog-1].wantlen, ChnCmd[chnLog-1].len,
					       	Asc2Hex(ChnCmd[chnLog-1].cmd, ChnCmd[chnLog-1].len), ChnCmd[chnLog-1].len);
#endif

				if (ChnCmd[chnLog-1].wantlen == ChnCmd[chnLog-1].len && ChnCmd[chnLog-1].wantlen!= MSG_HEAD_LEN ) {
					/*addInf[0]=00时不需要特殊处理*/
					//if ((gpSNP_shm->addInf[chnLog-1][0] == 0x00) ||
					//	((gpSNP_shm->addInf[chnLog-1][0] != 0x00) //&& 
						  //(AddJustLen(chnLog, ChnCmd[chnLog-1].cmd, &(ChnCmd[chnLog-1].len),
							//ChnCmd[chnLog-1].indtyp, gpSNP_shm->addInf[chnLog-1][0]) == 0)
					//		)) {
						//gpSNP_shm->wantLen[chnLog-1] = 0;
						RunLog( "senddatatochannel.通道号[%d]逻辑通道号[%d]", chn, chnLog );
						ret = SNP_TtySendDataToTcp_msq( ChnCmd[chnLog-1].cmd, ChnCmd[chnLog-1].len);//, chn, chnLog, indtyp 
						//ret = SNP_SendDataToChannel( ChnCmd[chnLog-1].cmd, 
						//	ChnCmd[chnLog-1].len, chn, chnLog, indtyp );
						if( ret < 0 ) {
							RunLog("发送通道(%02d)数据到消息队列失败(%d)", chnLog, ret);
						//	continue;   
						/* This line removed by jie @ 2009.5.13
						 * 错误了也要清零ChnCmd,否则错误不能恢复
						 */
						}
						RunLog("写通道(%02d)数据到消息队列:%s(%d)", chnLog, Asc2Hex( ChnCmd[chnLog-1].cmd, ChnCmd[chnLog-1].len), ChnCmd[chnLog-1].len);
						memset(&ChnCmd[chnLog-1], 0, sizeof(ChnCmd[chnLog-1]));	/*初始化之*/
					//}
				}
			}
		}
		memcpy( &set, &setbak, sizeof(fd_set) );
	}
	return 0;
}


/*!
 * \brief 读取通用配置
 * \param[in]  file  配置文件路径
 * \param[in]  key   配置项名称
 * \param[out] value 配置值(仅支持字符串和整型值，不管是字符串类还是整数均传入变量地址)
 * \return int 成功返回0，失败返回-1
 */
static int SNP_get_general_config(const char *file, const char *key, void *value)
{
	json_t *root = NULL;
	json_t *child = NULL;
	json_t *v = NULL;
	json_error_t error;
	int rc, flag = -1;
	const char *k = NULL;

	root = json_load_file(file, 0, &error);
	if (root == NULL) {
		LOG_ERROR("load %s failed, err:%s", file, error.text);
		return -1;
	}

	child = json_object_get(root, "general");
	if (child == NULL) {
		LOG_ERROR("try to get general failed");
		rc = -1;
		goto EXIT_LOAD_CFG;
	}

	json_object_foreach(child, k, v) {
		if (v == NULL) {
			LOG_ERROR("parsing general error, key:%s", k);
			rc = -1;
			goto EXIT_LOAD_CFG;
		} 
		if (strcmp(key, k) == 0) {
			switch (json_typeof(v)) {
			case JSON_STRING:
				strcpy((char *)value, json_string_value(v));
				LOG_INFO("%s , [general]  %s = %s ", file,key, json_string_value(v));
				flag = 0;
				break;
			case JSON_INTEGER:
				*(int *)value = json_integer_value(v);
				LOG_INFO("%s, [general] %s= %d ",file, key, *(int*)value);
				flag = 0;
				break;
			case JSON_TRUE:
			case JSON_FALSE:
				*(int *)value = json_is_true(v) ? 1 : 0;
				LOG_INFO("%s, [general] %s= %d ",file, key, *(int*)value);
				flag = 0;
				break;
			default:
				LOG_DEBUG("%s, [general] %s: type error",file, key);
				break;
			}
			if (flag == 0)
				break;
		} else {
			//LOG_WARN("general.unparsed: %s,  but  key = %s", k,key);
		}
	}
	if (flag == -1){
		LOG_ERROR("%s 中没有找到参数%s",file, key);
	}

	rc = flag;

EXIT_LOAD_CFG:
	//JSON_DECREF(root);
	json_decref(root);
	return rc;
}


static int SNP_get_general_config2(const char *fileName, const char *key, void *value)
{
	
#define INI_FIELD_LEN 63
#define INI_MAX_CNT 30
	
typedef struct	__attribute__((__packed__)){
	char name[INI_FIELD_LEN + 1];
	char value[INI_FIELD_LEN + 1];
}INI_NAME_VALUE;
	
INI_NAME_VALUE ini[INI_MAX_CNT];
	

		int tcpport;
		char  my_data[1024];
	
		char  *tmp_ptr,*tmp;//
	
		int data_len;
	
		int i,j,ret,g;
	
		int c;
	
		FILE *f;

	
		if(  NULL== fileName ){
	
			return -1;
	
		}
	
		ret = IsTheFileExist(fileName);
	
		if(ret != 1){
	
			char pbuff[256];
	
			bzero(pbuff,sizeof(pbuff));
	
			sprintf( pbuff, "file[%s] not exist in readIni	error!! ", fileName );
	
			cgi_error(pbuff);
	
			exit(1);
	
			return -2;
	
		}
	
		f = fopen( fileName, "rt" );
	
		if( !f	){	
	
			char pbuff[256];
	
			bzero(pbuff,sizeof(pbuff));
	
			sprintf( pbuff, "file[%s] open error in readIni!! ", fileName );
	
			cgi_error(pbuff);
	
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
	
		
	
		tmp_ptr = my_data;
	
		bzero(&(ini[0].name), sizeof(INI_NAME_VALUE)*INI_MAX_CNT);	
	
		i = 0;

		while (tmp_ptr[0] !='\0'){
	
			tmp = split(tmp_ptr,'=');// split data to get field name.
	
			bzero(&(ini[i].value),sizeof(ini[i].name));
	
			strcpy(ini[i].name,TrimAll(tmp) );
	
			tmp = split(tmp_ptr,'\n');// split data to get value.
	
			bzero(&(ini[i].value),sizeof(ini[i].value));
	
			strcpy(ini[i].value,TrimAll(tmp));		   
	
	
			if (memcmp(ini[i].name, "SINOPEC_TCP_PORT", strlen("SINOPEC_TCP_PORT")) == 0) {
				
				tcpport = 0;
				for (j = 0; (ini[i].value[j] != '\0') && (j <INI_FIELD_LEN+1);j++ ){
					tcpport *= 10;
					tcpport += ini[i].value[j]-'0';

				}
				memcpy((char*)value, &tcpport, sizeof(int));
				LOG_ERROR("公共配置----tcpport=%d",tcpport);
				return i;
			}
			i++;
		}

		return -1;	
	
	}
	



int disk_space(char *filename)
{
    struct statfs sfs;
    int ret = statfs(filename, &sfs);//"/home/Data"
    int avilable =sfs.f_bavail/1024*sfs.f_bsize/1024;
    return avilable;

}
void SNP_SetLogLevel( const int loglevel )
{
	giLogLevel = loglevel;
	if( (giLogLevel !=0) ||(giLogLevel !=1) || (giLogLevel !=2)   ){
		giLogLevel = SNP_DEFAULT_LOG_LEVEL;
	}
}
void SNP_SetLogFd( const int fd )
{
	giFd= fd;
	if(  giFd <=  0){
		printf( " {日志文件描述符错误[%d] }\n", fd );
	}
}
void SNP_SetFileAndLine( const char *filename, int line )
{
	assert( strlen( filename ) + 1 <= sizeof( gsFileName) );
	strcpy( gsFileName, filename );
	giLine = line;
}

void SNP_GetFileAndLine( char *fileName, int *line )
{
	strcpy( fileName, gsFileName );
	*line = giLine;
}


//giLogLevel=0, no log;giLogLevel=1, only print log info; giLogLevel = 2, print log info and write log info to file 
void SNP___LevelLog(const char *fmt,...){//const int loglevel, const int fd, 
	va_list argptr;
	char sMessage[SNP_MAX_VARIABLE_PARA_LEN + 1];
	char fileName[SNP_MAX_FILE_NAME_LEN + 1];	/*记录调试信息的文件名*/
	int line;								/*记录调试信息的行*/
	int level = giLogLevel;
	
	if(level == 0 ){
		return;
	}
	SNP_GetFileAndLine(fileName, &line);
	sprintf(sMessage, "%s:%d	", fileName, line);
	sprintf(sMessage+strlen(sMessage), "pid = [%d]", getpid());

	va_start(argptr, fmt);
	vsprintf(sMessage+strlen(sMessage),fmt, argptr);
	va_end( argptr );

	
	if( (giLogLevel !=0) ||(giLogLevel !=1) || (giLogLevel !=2)   ){
		level = SNP_DEFAULT_LOG_LEVEL;
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
void SNP___FileLog(const char *fmt,...){//const int loglevel, const int fd, 
	va_list argptr;
	char sMessage[SNP_MAX_VARIABLE_PARA_LEN + 1];
	char fileName[SNP_MAX_FILE_NAME_LEN + 1];	/*记录调试信息的文件名*/
	int line;								/*记录调试信息的行*/
	
	SNP_GetFileAndLine(fileName, &line);
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
int SNP_InitLog(int loglevel){
	int fd;
    	 if( ( fd = open( SNP_LOG_FILE, O_WRONLY|O_CREAT|O_TRUNC, 00644 ) ) <= 0 ) {
		printf( "打开日志文件[%s]失败\n", SNP_LOG_FILE );
		return -1;
	}
	giFd = fd;
	SNP_SetLogLevel(loglevel);
	return 1;
}
int SNP_IsFileExist(const char * sFileName ){
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

int SNP_getlocaltime(char *my_bcd)
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

/*
 *  替换日志文件-将原文件拷贝到/home/Data/log_bak/ 下
 *  成功返回0, 失败返回 -1
 */
int SNP_BackupLog()
{
	int fd;
	int _fd;
	int ret;
	struct stat f_stat;
	unsigned char cmd[(2 * sizeof(SNP_LOG_FILE)) + 16];
	unsigned char date_time[7] = {0};

	SNP_getlocaltime(date_time);

	if((fd = open(SNP_LOG_FILE, O_RDONLY, 00644)) < 0) {
		return -1;
	}

	if (fstat(fd, &f_stat) != 0) {
		return -1;
	}
	
	if ((long)f_stat.st_size > SNP_500K) {
		SNP_LevelLog("run.log 大小超过 500K, 系统备份日志");

		system("mkdir  /home/Data/log_backup");

		sprintf(cmd, "cp %s /home/Data/log_backup/run_log[%02x%02x%02x%02x%02x%02x%02x].bak",
						SNP_LOG_FILE, date_time[0], date_time[1], date_time[2],
						date_time[3], date_time[4], date_time[5], date_time[6]);
		system(cmd);

		truncate(SNP_LOG_FILE, 0);
	} else {
		close(fd);
		return 0;
	}
	
	close(fd);

	if((fd = open(SNP_LOG_FILE, O_CREAT , 00644)) < 0) {
		return -1;
	}

	close(fd);

	return 0;
}

//------------------------------------------------


//----------------------------------------------------------
//进程管理
//---------------------------------------------------------
int SNP_AddPidCtl( SNP_Pid *pidList, pid_t pid, char *flag, unsigned char *count )
{
	SNP_Pid *pFree;

	pFree = pidList + (*count);

	pFree->pid = pid;
	strcpy( pFree->flag, flag );

	(*count)++;

	return 0;
}

int SNP_GetPidCtl( SNP_Pid *pidList, pid_t pid, char *flag, unsigned char *count )
{
	int i;
	SNP_Pid *pFree;
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

int SNP_ReplacePidCtl( SNP_Pid *pidList, pid_t pid, char *flag, unsigned char pos )
{
	SNP_Pid *pFree;
	pFree = pidList + pos;

	pFree->pid = pid;
	strcpy( pFree->flag, flag );

	return 0;
}

//--------------------------------------------------------------
//共享内存初始化
//-------------------------------------------------------------
//char SNP_isChild;
//static char *SNP_AttachTheShm( int index );

static void SNP_RemoveIPC()
{
	//int i;
	printf( "清理IPC" );	
	RemoveShm( SNP_SHM_INDEX );	/*不判断返回码.有就销毁*/	
	//RemoveShm( SNP_SHM_INDEX );
	
	//RemoveSem( PORT_SEM_INDEX );	
	
	//#endif
	return;

}
void SNP_WhenExit()
{
	SNP_LevelLog( "进程[%d]退出,其父进程为[%d], 当前主进程的父进程为[%d]",  \
		getpid() ,getppid(), gpSNP_shm->ppid);
	if( (SNP_isChild == 0) &&(gpSNP_shm->ppid == getppid()) )	  /*父进程负责ipc的删除*/
		SNP_RemoveIPC(); 
	return;
}



/*系统初始化*/
int SNP_InitAll(  )
{
	//int fd;								/*文件描述符*/
	int ret;
	int size;
	unsigned char cmd[128];
	//int SNP_size ;
	
	
	//extern SNP_SHM * gpSNP_shm; //该结构是全局的量，只有一个,在SNP_main.c中定义.
	
	

	
	
	size = sizeof(SNP_SHM);
	//SNP_size = sizeof(SNP_SHM);
	
	/*创建共享内存*/	
	ret = SNP_CreateShm( SNP_SHM_INDEX, size );	
	if(ret < 0) {
		SNP_LevelLog( "创建共享内存SNP_SHM 失败.请检查系统." );
		return -1;
	}
	gpSNP_shm = (SNP_SHM *)SNP_AttachTheShm( SNP_SHM_INDEX );
	//gpSNP_shm = (SNP_SHM* )AttachShm( SNP_SHM_INDEX );		
	//
	if (gpSNP_shm == NULL) {//(gpSNP_shm == NULL) && ()
		SNP_LevelLog( "连接共享内存失败.请检查系统" );
		return -1;
	}
	//sprintf(cmd, "rm -f %s ",STOP_TANK);
	//system(cmd);
	//gpGunShm->sockCDPos = -1; //add in 2007.9.10
	//->chnCnt= 1;	
	atexit( SNP_WhenExit );


	//创建一个信号量.用于对串口写时加锁
	ret = CreateSem( SNP_PORT_SEM_INDEX );
	if( ret < 0 ) {
		UserLog( "创建信号量失败" );
		return -1;
	}
	//创建一个信号量.用于对共享内存读写时加锁
	ret = CreateSem( SNP_SEND_PC_SEM_INDEX );
	if( ret < 0 ) {
		UserLog( "创建信号量失败" );
		return -1;
	}
	//创建一个消息队列，用于监听TTY的程序和TCP上行线程通讯
	ret = CreateMsq( SNP_TTY2TCP_INDEX);
	if( ret < 0 ) {
		UserLog( "创建消息队列失败" );
		return -1;
	}

	return 0;

}
//----------------------------------------

//----------------------------------------------------------------
//信号处理
//----------------------------------------------------------------
typedef void SNP_Sigfunc( int );			/*Sigfunc表示一个返回值为void,参数为int的函数*/
										/*定义该类型可以简化signal函数的书写*/
//#define __DOSIANAL_STATIC__
int SNP_CatchSigAlrm;
int SNP_SetSigAlrm;
/*用于记录老的alarm函数的指针*/
SNP_Sigfunc 			*SNP_pfOldFunc=NULL;

/*缺省的alarm处理函数*/
static void SNP_WhenSigAlrm( int sig );

/*当收到SIGCHLD时,等待结束的子进程.防止产生僵尸进程*/
static void SNP_WhenSigChld(int sig);
//SNP_Sigfunc *SNP_Signal( int signo, SNP_Sigfunc *func );


static void SNP_WhenSigAlrm( int sig )
{
	if( sig != SIGALRM )
		return;
	SNP_CatchSigAlrm = TRUE;
	Signal( sig, SNP_WhenSigAlrm);
}

SNP_Sigfunc *SNP_SetAlrm( uint sec, SNP_Sigfunc *pfWhenAlrm )
{

	SNP_CatchSigAlrm = FALSE;
	SNP_SetSigAlrm = FALSE;

	if( sec == TIME_INFINITE )
		return NULL;

	/*设置对SIGALRM信号的处理*/
	if( pfWhenAlrm == NULL )
		SNP_pfOldFunc = Signal( SIGALRM, SNP_WhenSigAlrm );
	else
		SNP_pfOldFunc = Signal( SIGALRM, pfWhenAlrm );

	if( SNP_pfOldFunc == (SNP_Sigfunc*)-1 )
	{
		return (SNP_Sigfunc*)-1;
	}

	/*设置闹钟*/
	alarm( sec );

	SNP_SetSigAlrm = TRUE;

	return SNP_pfOldFunc;
}

void SNP_UnSetAlrm( )
{
	SNP_CatchSigAlrm = FALSE;

	if( !SNP_SetSigAlrm )
		return;

	alarm( 0 );

	/*设置对SIGALRM信号的处理*/
	Signal( SIGALRM, SNP_pfOldFunc );
	SNP_pfOldFunc = NULL;

	SNP_SetSigAlrm = FALSE;

	return;
}

static int   SNP_GetWorkDir( char *workdir) {
	FILE *f;
	int i, r,trytime = 3;
	char s[96] ;

	if(  workdir == NULL ){		
		return -1;
	}	
	f = fopen( RUN_WORK_DIR_FILE, "rt" );
	if( ! f ){	// == NULL
		memcpy( workdir,DEFAULT_WORK_DIR, strlen(DEFAULT_WORK_DIR) );
		return 0;
	}	
	bzero(s, sizeof(s));
	for(i=0; i<trytime; i++){
		r = ReadALine( f, s, sizeof(s) );		
		if( s[0] == '/' )
			break;
	}
	fclose( f );
	if(s[0] == '/' ){
		memcpy(workdir,s, strlen(s));	
		return 0;
	}
	else{
		memcpy( workdir,DEFAULT_WORK_DIR, strlen(DEFAULT_WORK_DIR) );
		return 0;
	}
}

//------------------------------------------------
//共享内存处理
//------------------------------------------------
static char SNP_gKeyFile[MAX_PATH_NAME_LENGTH]="";

static int SNP_InitKeyFile() 
{
	char	*pFilePath, FilePath[]=SNP_WORK_DIR;
	char temp[128];
	int ret;
	
	//取keyfile文件路径//	
	pFilePath = getenv( "WORK_DIR" );	//WORK_DIR 工作目录的环境变量
	if( NULL ==  pFilePath){
		bzero(temp, sizeof(temp));
		ret = SNP_GetWorkDir(temp);
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
	sprintf( SNP_gKeyFile, "%s/etc/keyfile.cfg", pFilePath );
	if( SNP_IsFileExist( SNP_gKeyFile ) == 1 )
		return 0;

	return -1;
}

static key_t SNP_GetTheKey( int index )
{
	key_t key;
	if( SNP_gKeyFile[0] == 0 ){
		printf( "gKeyFile[0] == 0" );
		exit(1);
	}
	key = ftok( SNP_gKeyFile, index );
	if ( key == (key_t)(-1) ){
		printf( " key == (key_t)(-1)" );
		exit(1);
	}
	return key;
}


static char *SNP_AttachTheShm( int index )
{
	int ShmId;
	char *p;

	ShmId = shmget( SNP_GetTheKey(index), 0, 0 );
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

static int SNP_DetachTheShm( char **p )
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


int SNP_IsShmExist( int index )
{
	int ShmId;

	ShmId = shmget( SNP_GetTheKey(index), 0, 0 );
	if( ShmId < 0 )
		return FALSE;

	return TRUE;
}



int SNP_RemoveShm( int index )
{
	int ShmId;
	int ret;

	ShmId = shmget( SNP_GetTheKey( index ), 1, IPC_EXCL );
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

int SNP_CreateShm( int index, int size )
{
	int shmId;
	char * ptrShm;

	ptrShm = SNP_AttachTheShm( index );
	if( ptrShm != NULL )
	{
		if( SNP_DetachTheShm( &ptrShm ) < 0 || SNP_RemoveShm( index ) < 0 )
			return -1;
	}
	shmId = shmget( SNP_GetTheKey(index), size, IPC_CREAT|IPC_EXCL|PERMS );
	if( shmId < 0 )
	{
		printf( "errno = [%d]\n", errno );
		return -1;
	}
	ptrShm = ( char* )shmat( shmId, (char*)0, 0 ) ;
	shmdt( ptrShm );

	return 0;
}

int SNP_CreateNewShm( int index, int size )
{
	int shmId;
	char * ptrShm;

	shmId = shmget( SNP_GetTheKey(index), size, IPC_CREAT|IPC_EXCL|PERMS );
	if( shmId < 0 )
	{
		return -1;
	}
	ptrShm = ( char* )shmat( shmId, (char*)0, 0 ) ;
	shmdt( ptrShm );

	return 0;
}

//--------------------------------
/*
 * \brief  获取IP地址
 * \param[in]  interface    网络接口名称(eth0, eth0:1 etc.)
 * \param[out] ip_addr_str  IP地址(点分十进制字符串)
 * \return     int 成功返回0，失败返回-1
 */
static int ifconfig_get_ip(const __u8 *interface, __u8 *ip_addr_str)
{
	int fd;
	int if_cnt;
	int i = -1;
	struct ifconf ifc;
	struct ifreq ifr[8];

	ifc.ifc_len = sizeof(ifr);
	ifc.ifc_buf = (__caddr_t)ifr;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd == -1) {
		LOG_ERROR("%s:create socket failed", __FUNCTION__);
		return -1;
	}

	if (ioctl(fd, SIOCGIFCONF, &ifc) < 0) {
		LOG_ERROR("%s:get SIOCGIFCONF failed", __FUNCTION__);
		return -1;
	}

	if_cnt = ifc.ifc_len / sizeof(struct ifreq);
	LOG_DEBUG("For HeartBeat %s:if count: %d, req interface:%s", __FUNCTION__, if_cnt, interface);

	while (if_cnt-- > 0) {
		//LOG_DEBUG("   if[%d].name:%s", if_cnt, ifr[if_cnt].ifr_name);
		if (strcmp(ifr[if_cnt].ifr_name, interface) == 0) {
			i = if_cnt;
		}

		if ((ioctl(fd, SIOCGIFFLAGS, &ifr[if_cnt])) < 0) {
			LOG_ERROR("%s:get SIOCGIFFLAGS failed", __FUNCTION__);
			return -1;
		}
		if (ifr[if_cnt].ifr_flags & IFF_UP) {
			LOG_DEBUG("   if[%d].status: UP", if_cnt);
		} else {
			LOG_DEBUG("   if[%d].status: DOWN", if_cnt);
		}

		if ((ioctl(fd, SIOCGIFADDR, &ifr[if_cnt])) < 0) {
			LOG_ERROR("%s:get SIOCGIFADDR failed", __FUNCTION__);
			return -1;
		}
		LOG_DEBUG("   if[%d].ip:%s", if_cnt,
			(char*)inet_ntoa(((struct sockaddr_in*)(&ifr[if_cnt].ifr_addr))->sin_addr));
	};
	if (i == -1) {
		return -1;
	}
	LOG_INFO("For HeartBeat %s.IP:%s\n", interface, (char*)inet_ntoa(((struct sockaddr_in*)(&ifr[i].ifr_addr))->sin_addr));
	strcpy(ip_addr_str, inet_ntoa(((struct sockaddr_in*)(&ifr[i].ifr_addr))->sin_addr));

	close(fd);
	return 0;
}
/*
 * \brief  设置socket为非阻塞
 * \param  sock  带设置socket fd
 * \return int 成功返回0，失败返回-1
 */
static int set_non_blocking(int sock)
{
	int flags;

	flags = fcntl(sock, F_GETFL, 0);
	if (flags == -1) {
		LOG_ERROR("get flags of socket %d failed[err:%s]",
						sock, strerror(errno));
		return -1;
	}

	flags |= O_NONBLOCK;
	if (fcntl(sock, F_SETFL, flags) == -1) {
		LOG_ERROR("set nonblock of socket %d failed[err:%s]",
						sock, strerror(errno));
		return -1;
	}

	return 0;
}
/*
 * \brief 根据PC ID添加或更新共享内存snp_conn中的条目
 * \param[in] pc_id     必须有效(若logon_flag赋为0，则处理完后对应条目的pc_id会被清零)
 * \param[in] sock_fd    若不修改则赋值-1
 * \param[in] logon_flag 1-签到状态，0-签退状态, 若不修改则赋值-1
 * \return    int  成功返回0，失败返回-1
 */
int add_update_pc_conn(int pc_id, int sock_fd, int logon_flag)//int pos_hb_cnt, 
{
	int t = -1, idx;

	for (idx = 0; idx < MAX_PC_NUM; ++idx) {
		if (gpSNP_shm->snp_conn[idx].pc_id == pc_id) {
			// 条目已存在, 更新
			t = idx;
			break;
		} else if (gpSNP_shm->snp_conn[idx].pc_id == 0) {
			t = (t == -1 ? idx : t);   // 记录第一个空的位置
		}
	}

	if (t == -1) {
		LOG_ERROR("snp_conn中无空余空间存放新连接信息");
		return -1;
	}

	if (sock_fd != -1) {
		gpSNP_shm->snp_conn[t].sock_fd = sock_fd;
		LOG_INFO("DDDDDDDDDDDDDDDDDD  t %d  gpSNP_shm->snp_conn[t].sock_fd %d", t, gpSNP_shm->snp_conn[t].sock_fd);
	}
	if (logon_flag != -1) {
		gpSNP_shm->snp_conn[t].logon_flag = logon_flag;
	}
	gpSNP_shm->snp_conn[t].pc_id = pc_id;

	return 1;
}

/*
 * \brief 根据POS ID或者SOCK_FD清除共享内存pos_conn中的条目
 * \param[in] pos_id     0或者非0,若sock_fd为0则必须为非0
 * \param[in] sock_fd    0或者非0,若pos_id为0则必须为非0
 * \return    int  成功返回1，失败返回-1
 */
int del_update_pc_conn(int pc_id, int sock_fd)
{
	int t = -1, idx;
	if(pc_id>0){
		for (idx = 0; idx < MAX_PC_NUM; ++idx) {
			if (gpSNP_shm->snp_conn[idx].pc_id == pc_id) {
				t = idx;
				break;
			} 
		}
	}else if(sock_fd>0){
		for (idx = 0; idx < MAX_PC_NUM; ++idx) {
			if (gpSNP_shm->snp_conn[idx].sock_fd == sock_fd) {
				t = idx;
				break;
			} 
		}
	}
	if (t == -1) {
		LOG_ERROR("pos_conn中无此连接信息");
		return -1;
	}
	memset((void *)(&gpSNP_shm->snp_conn[t]),0,sizeof(SNP_CONN_TIEM));
	return 1;
}

/*
 * \brief  初始创建socket，并设置监听
 * \return 成功返回fd，失败返回-1
 */
static int SNP_tcp_serv_init()
{
	int fd, opt;
	int port;
	struct sockaddr_in serv_addr;
	
//	conn_info_init();

	//-- 读取端口配置
	if (get_sinopec_general_config2("sinopec_tcp_port", &port) < 0) {
		LOG_ERROR("公共配置----读取PC-FCC服务器端口配置sinopec_tcpc_port失败:port=%d",port);
		return -1;
	}
	LOG_INFO("公共配置----PC-FCC服务器端口sinopec_tcp_port:%d", port);

	//-- create a INET stream socket
	if ((fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		LOG_ERROR("create socket failed, err:%s", strerror(errno));
		return -1;
	}

	//-- bind the name to the descriptor
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(port);
	
	opt = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
		LOG_ERROR("setsocketopt failed, err:%s", strerror(errno));
		return -1;
	}


	if (bind(fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		LOG_ERROR("bind failed, err:%s", strerror(errno));
		goto error;
	}

	//-- tell kernel we're a server
	if (listen(fd, BACKLOG) < 0) {
		LOG_ERROR("listen failed, err:%s", strerror(errno));
		goto error;
	}

	return fd;

error:
	close(fd);
	return -1;
}

/*
 * \brief  添加socket到epoll监听集
 * \param[in]  fd  待监听socket fd
 * \param[in]  ev  epoll监听集
 * \return     int 成功返回0，失败返回-1
 */
static int epoll_ctl_add(int fd, struct epoll_event *ev)
{
	//-- 设置非阻塞socket
	if (set_non_blocking(fd) < 0) {
		LOG_ERROR("设置socket非阻塞失败");
		return -1;
	}

	ev->data.fd = fd;
	ev->events = EPOLLIN | EPOLLHUP | 0x2000;
	if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, ev) < 0) {
		LOG_ERROR("epoll_ctl add socket failed[err:%s]", strerror(errno));
		return -1;
	}

	return 0;
}
/*
 * \brief  从epoll监听集中删除socket
 * \param[in]  fd  待删除的socket fd
 * \param[in]  ev  epoll监听集
 * \return     int 成功返回0，失败返回-1
 */
static int epoll_ctl_del(int fd, struct epoll_event *ev)
{
	ev->data.fd = fd;
	if (epoll_ctl(epfd, EPOLL_CTL_DEL, fd, ev) < 0) {
		LOG_ERROR("epoll_ctl add socket failed[err:%s]", strerror(errno));
		return -1;
	}

	return 0;
}

inline int SNP_GetMsg()
{
	int send_signal_flag;
	int ret = -1, k;
	
	//NOERR_FUNC(thread_P(&mutex_send_msg_buffer), return -1);
	send_signal_flag = (gpSNP_shm->snp_fcc_msg.msg_len != 0);
	if (send_signal_flag) {
		for(k=0;k<MAX_PC_NUM;k++){
			if(gpSNP_shm->snp_conn[k].sock_fd > 0){
				gpSNP_shm->snp_fcc_msg.sock_fd = gpSNP_shm->snp_conn[k].sock_fd;
				break;
			}
		}
		Act_P(SNP_SEND_PC_SEM_INDEX);
		ret = send_pc_msg(gpSNP_shm->snp_fcc_msg.sock_fd, gpSNP_shm->snp_fcc_msg.msg, gpSNP_shm->snp_fcc_msg.msg_len);
		if (ret < 0) {
			LOG_ERROR("send pc msg failed , bucause %s", strerror(errno));
		}
		else {
			LOG_INFO("PC->>FCC  %s(%d)", Asc2Hex(gpSNP_shm->snp_fcc_msg.msg, gpSNP_shm->snp_fcc_msg.msg_len), gpSNP_shm->snp_fcc_msg.msg_len);
		}
		memset(&gpSNP_shm->snp_fcc_msg, 0, sizeof(SNP_SEND_INFO));
		Act_V(SNP_SEND_PC_SEM_INDEX);
	}
	//NOERR_FUNC(thread_V(&mutex_send_msg_buffer), return -1);

	//if (send_signal_flag) {
	//	pthread_cond_broadcast(&cond_len_wait);
	//}
	return ret;
}

inline int SNP_GetMsg_msq()
{
	int ret, k, fd;
	unsigned char msg[1024];
	int len = 1023;
	long indtype = 1;
	int timeout = 3;
	
	ret = RecvMsg(SNP_TTY2TCP_INDEX, &msg, &len, &indtype, timeout);
	if (ret == 0) {
		// 有消息
		for(k=0;k<MAX_PC_NUM;k++){
			if(gpSNP_shm->snp_conn[k].sock_fd > 0){
				gpSNP_shm->snp_fcc_msg.sock_fd = gpSNP_shm->snp_conn[k].sock_fd;
				break;
			}
		}
		fd = gpSNP_shm->snp_fcc_msg.sock_fd;
		// 发送到TCP
		ret = send_pc_msg(fd, msg, len);
		if (ret == 0) {
			RunLog("发送消息到TCP: %s(%d)", Asc2Hex(msg, len), len);
		} else {
			RunLog("发送消息到TCP失败: %s(%d)", Asc2Hex(msg, len), len);
		}
		
		return ret;
	}
	
	// 无消息
	return 0;
}


/*
 * \brief  发送报文
 * \param  fd   socket fd
 * \param  xml  发送数据流
 * \param  len  待发送数据长度
 * \return int  成功返回0，失败返回-1
 */
int send_pc_msg(int fd, const __u8 *xml, int len)
{
	int nwrite, nleft;
	const __u8 *pbuf = NULL;
	int cnt;
	/*union {
		__u8  xml_len[MSG_HEAD_LEN];
		__u32 len;
	} head;
	//暂时不用MD5, 前2字节为0
	//MD5_CTX context;
	//unsigned char digest [ 16 ]={0};
	unsigned char digest [ 2 ]={0,0};
	head.len = htonl(len);

	//MD5Init(context);
	//MD5Update(context, xml, len);
	//MD5Final( digest , context);
	
	// -- 发送XML报文长度
	pbuf = head.xml_len;
	nleft = MSG_HEAD_LEN;

	cnt = 5;
	while (nleft > 0 && cnt--) {
		if ((nwrite = write(fd, pbuf, nleft)) < 0) {
			LOG_DEBUG("---- write fail, errno:%d, %s", errno, strerror(errno));
			if (errno == EAGAIN) {
				continue;	
			} else if (errno == EINTR) {
				nwrite = 0;
			} else {
				LOG_ERROR("write data to (fd:%d) failed[err:%s]",
								fd, strerror(errno));
				return -1;
			}
		}
		nleft -= nwrite;
		pbuf += nwrite;
	}
	if (cnt == -1) {
				LOG_ERROR("write data to (fd:%d) failed[err:%s]",
								fd, strerror(errno));
				return -1;
	}
	// -- 发送报文MD5
	pbuf = digest;
	nleft = sizeof(digest);

	cnt = 5;
	while (nleft > 0 && cnt--) {
		if ((nwrite = write(fd, pbuf, nleft)) < 0) {
			LOG_DEBUG("---- write fail, errno:%d, %s", errno, strerror(errno));
			if (errno == EAGAIN) {
				continue;	
			} else if (errno == EINTR) {
				nwrite = 0;
			} else {
				LOG_ERROR("write data to (fd:%d) failed[err:%s]",
								fd, strerror(errno));
				return -1;
			}
		}
		nleft -= nwrite;
		pbuf += nwrite;
	}
	if (cnt == -1) {
				LOG_ERROR("write data to (fd:%d) failed[err:%s]",
								fd, strerror(errno));
				return -1;
	}
	*/
	// -- 发送XML报文
	pbuf = xml;
	nleft = len;

	cnt = 5;
	while (nleft > 0 && cnt--) {
		if ((nwrite = write(fd, pbuf, nleft)) < 0) {
			LOG_DEBUG("---- write fail, errno:%d, %s", errno, strerror(errno));
			if (errno == EAGAIN ) {
				continue;	
			} else if (errno == EINTR) {
				nwrite = 0;
			} else {
				LOG_ERROR("write data to (fd:%d) failed[err:%s]",
								fd, strerror(errno));
				return -1;
			}
		}
		nleft -= nwrite;
		pbuf += nwrite;
	}
	if (cnt == -1) {
				LOG_ERROR("write data to (fd:%d) failed[err:%s]",
								fd, strerror(errno));
				return -1;
	}
	return 0;
}

/*!
 * \brief  毫秒睡眠
 * \param  msec  睡眠时间长度，单位毫秒
 */
void msleep(long msec)
{
    struct timeval timeout;
    ldiv_t d;

    d = ldiv(msec * 1000, 1000000L);
    timeout.tv_sec = d.quot;
    timeout.tv_usec = d.rem;
    select(0, NULL, NULL, NULL, &timeout);
}

/*
 * \brief  接收PC报文
 * \param  fd      socket fd
 * \param  xml_buf XML接收缓冲区
 * \param  buflen  传入为缓冲区长度,返回为接收数据长度
 * \param  timeout 接收超时时间
 * \return int     成功返回0，读取失败返回-1，数据错误返回-2，对方关闭连接返回-3
 */
int recv_pc_msg(int fd, __u8 *xml_buf, int *buflen, int timeout)
{
	int nleft, nread;
	int rc;
	__u8 *pbuf = NULL;
	__u8  data_head[MSG_HEAD_LEN+1]={0};
	SNP_DATA_HEAD *head;
	
	
	memset(xml_buf, 0, *buflen);
	msleep(100);
	SetAlrm(timeout, NULL);
	if(*buflen<MSG_HEAD_LEN){
		LOG_ERROR("ERROR, buflen:%d<%d错误", nleft,MSG_HEAD_LEN);
		return -1;
	}
	

	//-- 读PC报文头部
	pbuf = data_head;
	nleft = MSG_HEAD_LEN;
	while (nleft > 0) {
		if ((nread = read(fd, pbuf, nleft)) < 0) {
			LOG_DEBUG("---- read fail, errno:%s", strerror(errno));

			if (CatchSigAlrm) {
				rc = -1;       // 超时返回
				goto RECV_DATA_FAILED_RET;
			}

			if (errno == EAGAIN) {
				LOG_DEBUG("---- EAGAIN----------------");
				msleep(100);
				continue;     // No data was available
			} else if (errno == EINTR) {
				LOG_DEBUG("---- EINTR----------------");
				nread = 0;    // Interrupted by a signal
				continue;
			}

			LOG_ERROR("read data from (fd:%d) failed[err:%d,%s]",
						fd, errno, strerror(errno));
			UnSetAlrm();
			return -3;
		} else if (nread == 0) {
			UnSetAlrm();
			return -3;            // connection is graceful closed by peer
		}

		pbuf += nread;
		nleft -= nread;
	}
	
	memcpy(xml_buf,data_head,MSG_HEAD_LEN);
	//-- 读取PC报文
	head = (SNP_DATA_HEAD *)data_head;
	head->len = (head->lenBcd[0]>>4)*1000 + (head->lenBcd[0]&0x0f)*100 + (head->lenBcd[1]>>4)*10 + (head->lenBcd[1]&0x0f);
	nleft = head->len + 2; // CRClen
	LOG_ERROR("data_head:%02x%02x%02x%02x%02x%02x", data_head[0], data_head[1], data_head[2], data_head[3], data_head[4],data_head[5]);
	LOG_ERROR("head->len = %d", head->len);
	if(nleft>MAX_MSG_LEN){
		LOG_ERROR("ERROR, 待接收数据长度为:%d,太长, 可能出现数据错位错误", nleft);
		return -3;
	}
	if (*buflen < nleft) {
		LOG_ERROR("ERROR:接收缓冲区太小, 待接收数据长度为:%d", nleft);
		goto RECV_DATA_CLEAR_ERR_DATA;
	}

	*buflen = MSG_HEAD_LEN + nleft;

	nread = 0;
	pbuf = xml_buf+MSG_HEAD_LEN;
	while (nleft > 0) {
		if ((nread = read(fd, pbuf, nleft)) < 0) {
			LOG_DEBUG("---- read fail, errno:%s", strerror(errno));
			if (CatchSigAlrm) {
				rc = -1;       // 超时返回
				goto RECV_DATA_FAILED_RET;
			}

			if (errno == EAGAIN) {
				LOG_DEBUG("---- EAGAIN----------------");
				msleep(100);
				continue;     // No data was available
			} else if (errno == EINTR) {
				LOG_DEBUG("---- EINTR----------------");
				nread = 0;    // Interrupted by a signal
				continue;
			}

			LOG_ERROR("read data from (fd:%d) failed[err:%d,%s]",
						fd, errno, strerror(errno));
			rc = -3;             // connection is graceful closed by peer
			goto RECV_DATA_FAILED_RET;
		} else if (nread == 0) {
			rc = -3;             // connection is graceful closed by peer
			goto RECV_DATA_FAILED_RET;
		}

		pbuf += nread;
		nleft -= nread;
	}

	LOG_ERROR("TCP Recv ............. head->len = %d, buflen = %d", head->len, *buflen);
	LOG_ERROR("TCP Recv: %s", Asc2Hex(xml_buf, *buflen));
	UnSetAlrm();
	return 0;

RECV_DATA_CLEAR_ERR_DATA:
	// -- 接收剩余错误数据以免一直接收错误
	LOG_DEBUG("clear error pc tcp data");
	rc = -2;
	pbuf = xml_buf;
	while (1) {
		nleft = 10;
		if ((nread = read(fd, pbuf, nleft)) < 0) {
			LOG_DEBUG("---- read fail, errno:%s", strerror(errno));
			if (errno == EAGAIN) {
				LOG_DEBUG("---- EAGAIN----------------");
				break;
			} else if (errno == EINTR) {
				continue;
			}
			LOG_ERROR("read data from (fd:%d) failed[err:%d,%s]",
						fd, errno, strerror(errno));
			rc = -3;
		} else if (nread == 0) {
			rc = -3;
		}
	}

RECV_DATA_FAILED_RET:

	UnSetAlrm();
	return rc;
}

/*
 * \brief  接受连接请求
 * \param  listenfd  监听socket
 * \return 成功返回连接fd，失败返回-1
 */
static int SNP_serv_accept(int listenfd, struct sockaddr_in *cli_addr)
{
	int clifd, len;

	while (1) {
		len = sizeof(struct sockaddr_in);
		memset(cli_addr, 0, len);
		clifd = accept(listenfd, (struct sockaddr *)cli_addr, &len);
		if (clifd < 0 && errno == EINTR) {
			continue;
		} else if (clifd >= 0) {
			break;
		}
		LOG_ERROR("接受新连接失败, err:%s", strerror(errno));
		return -1;
	}

	LOG_INFO("建立(接受)新连接, IP: %s , PORT: %ld , socket:%d", 
		inet_ntoa(cli_addr->sin_addr), ntohs(cli_addr->sin_port), clifd);

	return clifd;
}

/*
 * \brief  完成TCP监听服务的启动和初始化
 * \return int 成功返回socket fd，失败返回-1
 */
static int SNP_tcp_server_startup()
{
	int fd;
	fd = SNP_tcp_serv_init();
	if (fd < 0) {
		return -1; 
	}
	if (set_non_blocking(fd) == -1)
		return -1;
	
	epfd = epoll_create(MAX_EPOLL_EVENTS);
	if (epfd == -1) {
		LOG_ERROR("epoll_create failed[err:%s]", strerror(errno));
		goto STARTUP_RET_FAIL;
	}

	if (epoll_ctl_add(fd, &evs) < 0) {
		goto STARTUP_RET_FAIL;
	} else {
		return fd;
	}

STARTUP_RET_FAIL:
	close(fd);
	return -1;
}
inline int SNP_ClearMsg()
{
	int send_signal_flag;
	int ret = -1;
	
	//NOERR_FUNC(thread_P(&mutex_send_msg_buffer), return -1);
	send_signal_flag = (gpSNP_shm->snp_fcc_msg.msg_len != 0);
	if (send_signal_flag) {
		Act_P(SNP_SEND_PC_SEM_INDEX);		
		memset(&gpSNP_shm->snp_fcc_msg, 0, sizeof(SNP_SEND_INFO));
		Act_P(SNP_SEND_PC_SEM_INDEX);
		ret = 0;
		LOG_INFO(">>>>Clear snp_fcc_msg ok.");
	}
	//NOERR_FUNC(thread_V(&mutex_send_msg_buffer), return -1);

	//if (send_signal_flag) {
	//	pthread_cond_broadcast(&cond_len_wait);
	//}
	return ret;
}
/*!
 * \brief  解析处理POS的XML请求
 * \param[in] req_xml  请求的xml报文
 * \param[in] len      xml报文长度
 * \return    int 成功时，若是签到请求返回正整数POS ID, 其他请求返回0，失败返回-1
 */
int SNP_handle_received_req(const __u8 *req_data, int len){
	int rc = -1;
	char data_head[10]={0};
	SNP_DATA_HEAD *head;
	memcpy(data_head,req_data, MSG_HEAD_LEN);
	head = (SNP_DATA_HEAD *)data_head;
	head->len = (head->lenBcd[0]>>4)*1000 + (head->lenBcd[0]&0x0f)*100 + (head->lenBcd[1]>>4)*10 + (head->lenBcd[1]&0x0f);
	LOG_ERROR("snp .................. head->len=%d, all-len=%d", head->len, len);
	if(head->len+MSG_HEAD_LEN+2==len){//ok data:收到正确的数据
		rc = SNP_TcpSendDataToChannel(req_data,len,head->dst);
	}
	return rc;
}
/**
*
*/
void SNP_accpet_pc_proc()
{
	int fd = -1, nfds;
	int send_msg_flag=0;
	int i,j, rc, len=0, trycnt = 0;
	struct sockaddr_in cli_addr;
	__u8 buffer[1024];
	//__u8 node;
	time_t t;
	//int iDeviceID;
	char msg[MAX_MSG_LEN];
	int msglen = 0;
	int litype;
	//struct timeval now;  
  	//struct timespec outtime; 
	//WAIT_INIT(WAIT_POS_NODE);
	SNP_initSerialPortForTcp();
	
	int pc_sock_break_flag=0;
	Signal(SIGPIPE, SIG_IGN);
	time(&t);
	//pthread_detach(pthread_self());
	LOG_INFO("fdc_accpet_pc_proc is starting....");
	while(1){
		//for (i = 0; i < MAX_POS_NUM; ++i) {
		//	LOG_DEBUG("pos_id:%d,sock_fd:%d",gpSNP_shm->pos_conn[i].pos_id ,gpIfsf_shm->pos_conn[i].sock_fd) ;
		//}
		//pthread_testcancel();
		nfds = epoll_wait(epfd, events, MAX_EPOLL_EVENTS, -1);
		if (nfds < 0) {
			LOG_ERROR("epoll_wait failed[err:%s]", strerror(errno));
			continue;
		} 
		for (i = 0; i < nfds; ++i) {
			//LOG_DEBUG("Recv server events[%d].events: %x,events[i].data.fd:%d,tcp_listen_fd4cd:%d", i, events[i].events, 
				//events[i].data.fd,	tcp_listen_fd4cd);
			if (events[i].data.fd == tcp_listen_fd) {	LOG_DEBUG("call fdc_pc_serv_accept");

				//-- accept connection
				fd = SNP_serv_accept(tcp_listen_fd, &cli_addr);
				if (fd < 0) {
					continue;
				}
				//-- 添加epoll事件监听
				if (epoll_ctl_add(fd, &evs) < 0) {
					LOG_ERROR("epoll_ctl_add failed");
					close(fd);
					continue;
				}

				LOG_INFO("connection created fd:%d FCC<-------------------------------PC", fd);
				pc_sock_break_flag=0;
			} else if (events[i].events & EPOLLIN) {
					len = sizeof(buffer);
					//rc =recv_xml_msg(events[i].data.fd, buffer, &len, 0);
					rc =recv_pc_msg(events[i].data.fd, buffer, &len, TCP_RECV_TIMEOUT);//TCP_RECV_TIMEOUT
					LOG_DEBUG("after call recv_pc_msg,rc:%d",rc);
					if (rc == 0) {//received ok.
						//logon:
						//add_update_pc_conn(0, events[i].data.fd, 1);//rc
						//TO DO?????
						rc = SNP_handle_received_req(buffer, len);//,events[i].data.fd
						if (rc >= 0) {
							//LOG_INFO("执行add_update_pos_conn 函数, sock_fd:%d",events[i].data.fd);
							add_update_pc_conn(0, events[i].data.fd, 1);//rc							
						}else if(rc<0){//==0 heartbeat
							LOG_ERROR("处理PC data失败,rc:%d",rc);
						}
												
						break;
					} else if ((rc == -1) || (rc == -2)) {
						LOG_DEBUG("接收PC data失败 rc %d", rc);
					} else if (rc == -3) {
						LOG_DEBUG("connection closed by peer,msg.sock_fd:%d,events[].data.fd:%d",gpSNP_shm->snp_fcc_msg.sock_fd,events[i].data.fd);
						if(gpSNP_shm->snp_fcc_msg.sock_fd == events[i].data.fd){//清除数据以免死锁
							LOG_DEBUG("events[i].data.fd:%d,msg will be clear.", events[i].data.fd);	
							LOG_INFO("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
							SNP_ClearMsg();//GetMsg(msg, &msglen);
							bzero(msg, sizeof(msg));
							msglen = 0;
						}
						pc_sock_break_flag=events[i].data.fd;
						rc=del_update_pc_conn(0, pc_sock_break_flag);
						epoll_ctl_del(events[i].data.fd, &evs);
						close(events[i].data.fd);
						events[i].data.fd = SOCKET_INVALID;
						LOG_DEBUG("fd:%d PC<---------------x----------------FCC", events[i].data.fd);						
						break;
					}
			}


PFS_EPOLL_ERROR:
			if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) \
							|| (events[i].events & 0x2000)) {LOG_DEBUG("EPOLL error or hup");
			
				pc_sock_break_flag=events[i].data.fd;
				rc=del_update_pc_conn(0, pc_sock_break_flag);
				epoll_ctl_del(events[i].data.fd, &evs);
				LOG_ERROR("fd:%d PC<---------------x----------------FCC closed", events[i].data.fd);
				close(events[i].data.fd);
				events[i].data.fd = SOCKET_INVALID;
			}
			continue;
		}

		//未找到合适的就发送数据
		if (i ==  nfds && gpSNP_shm->snp_fcc_msg.msg_len != 0) {	
			LOG_ERROR("未找到合适的pc id  %d ", gpSNP_shm->snp_fcc_msg.pc_id);
			SNP_GetMsg();//msg, &msglen
		}
		
		//--Task2 离线PC处理:注销&关闭连接
	}
	close(epfd);
	//WAIT_DESTROY(WAIT_POS_NODE);
}

//线程等待锁用于线程等待
pthread_mutex_t mutex_thread;
pthread_cond_t cond_thread;
#define WAIT_DEF struct timeval now;  struct timespec outtime; 
#define WAIT_INIT pthread_mutex_init(&mutex_thread,NULL);\
	pthread_cond_init(&cond_thread,NULL);
#define WAIT_TIME(timeout) pthread_mutex_lock(&mutex_thread);\
		gettimeofday(&now, NULL);\
		outtime.tv_sec = now.tv_sec + (timeout);\
		outtime.tv_nsec = now.tv_usec * 1000;\
		pthread_cond_timedwait(&cond_thread, &mutex_thread, &outtime);\
		pthread_mutex_unlock(&mutex_thread);	
#define WAIT_DESTROY pthread_mutex_destroy(&mutex_thread); \
	pthread_cond_destroy(&cond_thread);

/*
 * func: 设定串口属性-通讯速率、校验位等 */
static int SetPort023()
{
	// COPY FROM : int SNP_TcpSendDataToChannel( const __u8 *str, size_t len, char node )
	
	int fd;
	int rc;
	int ret;
	unsigned char chnlog, chnphy;
	
#ifndef MAX_NODE_CNT
#define MAX_NODE_CNT 64
#endif
	
	int nodecnt;
	
	for (nodecnt = 0; nodecnt < MAX_NODE_CNT; nodecnt++) {

		if (gpSNP_shm->node_channel[nodecnt].nodeDriver != 23) {
			continue;
		}
		if (gpSNP_shm->node_channel[nodecnt].isUse == 0) {
			continue;
		}
		
		LOG_ERROR("SP node_channel[%d]:node[%02x],nodeDriver[%d],chnlog[%02x],serialPort[%02x],chnPhy[%02x],isused[%02x]", nodecnt,\
		gpSNP_shm->node_channel[nodecnt].node, \
		gpSNP_shm->node_channel[nodecnt].nodeDriver, \
		gpSNP_shm->node_channel[nodecnt].chnLog, \
		gpSNP_shm->node_channel[nodecnt].serialPort, \
		gpSNP_shm->node_channel[nodecnt].chnPhy, \
		gpSNP_shm->node_channel[nodecnt].isUse);
		
		chnlog = gpSNP_shm->node_channel[nodecnt].chnLog;
		chnphy = gpSNP_shm->node_channel[nodecnt].chnPhy;
		fd = gpSNP_shm->serialPort_fd[gpSNP_shm->node_channel[nodecnt].serialPort-1];
	
		LOG_ERROR("node:%d, chnlog:%d, chnphy:%d, fd:%d", nodecnt+1, chnlog, chnphy, fd);
		
		//fd = SNP_getSerialPortFd(chn);
		if(fd<0) {
			RunLog("通道[%d]查找fd失败", chnlog);
			return -1;
		} else {
			RunLog("通道[%d]查找到fd=%d", chnlog, fd);
		}
	
		if (gpSNP_shm->chnLogSet[chnlog-1] == 0) {
			ret = set_chn_boardrate(fd, chnlog, chnphy, 9600);
			if (ret == 0) {
				gpSNP_shm->chnLogSet[chnlog-1] = 1;
				RunLog( "<加油机>节点[%d], 设置串口属性成功(偶校验) fd=%d", gpSNP_shm->node_channel[nodecnt].node, fd);
			}
		} else
			continue;
	
		//RunLog( "SendDataToChannel	[%s]", Asc2Hex(str,len) );
		//RunLog( "senddatatochannel.通道号[%d]逻辑通道号[%d],数据[%s]", chn, chnLog, Asc2Hex(str,len) );
		//return rc;//SendMsg( GUN_MSG_TRAN, str, len,indtyp, 3 );
	}

	if (nodecnt >= MAX_NODE_CNT) {
		LOG_ERROR("节点查找完毕");
		return 0;
	}






#if 0
	int ret;

	if( gpGunShm->chnLogSet[pump.chnLog-1] == 0 ) {
	pump.cmd[0] = 0x1C;
	pump.cmd[1] = 0x52;
	pump.cmd[2] = 0x30|(pump.chnPhy-1);
	pump.cmd[3] = 0x03;	//9600bps
	pump.cmd[4] = 0x01<< ((pump.chnPhy - 1) % 8);	//EVEN parity
	pump.cmd[5] = 0x00;
	pump.cmd[6] = 0x01<< ((pump.chnPhy - 1) % 8);
	pump.cmdlen = 7;
	pump.wantLen = 0;

	ret =  WriteToPort( pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			pump.cmdlen, 0, pump.wantLen, TIME_OUT_WRITE_PORT );
	if( ret < 0 ) {
		return -1;
	}
	gpGunShm->chnLogSet[pump.chnLog-1] = 1;
	Ssleep(INTERVAL);
	}	

	RunLog( "<加油机>节点[%d],设置串口属性成功(偶校验)", node);
	return 0;
#endif
}


void SNP_send_pc_thread()
{
	WAIT_DEF;
	WAIT_INIT;

	SetPort023();
	
	while (1) {
		 //WAIT_TIME(2);
		//LOG_INFO("################send pos thread running ");
		//SNP_GetMsg();
		SNP_GetMsg_msq();
		//Sleep();
	}
	WAIT_DESTROY;
	
}

/*
 * \brief  FCC-PC 数据收发服务
*为PC启动的Tcp服务器, 用于处理PC主动发送过来的数据 
*/
int SNP_TcpToPcServ()
{
	
	int i,j,rc;//,n
	int tmp_node;
	int cnt_thread=0;//线程数	
	pthread_t thread_pc_svr[2]  ;
	
	char thread_name[ ][32] = { 
			"PC数据发送线程", 
			"empty",
		};//TCP_POS_SVR_THREAD_NUM // "监听PC和数据接收线程",   
	
	/*struct timeval now;  
  	struct timespec outtime; */
	LOG_INFO("FCC-PC TCP Serv startup....");

	assert(gpSNP_shm != NULL);	
	//WAIT_INIT(0) ;
	
	//初始化运行时结构体等
	//SNP_initSerialPortForTcp();
	//step1---启动监听PC服务端
	tcp_listen_fd = SNP_tcp_server_startup();
	if (tcp_listen_fd < 0) {
		LOG_ERROR("启动FDC-PC TCP服务失败");
		return -1;
	}
	//step2---启动数据发送线程 TTY->(MSQ) TO TCP
	i++;
	if(pthread_create(&thread_pc_svr[i], NULL,(void *)SNP_send_pc_thread,NULL)) {
		LOG_ERROR("启动TcpToPcServ的[%s]失败", thread_name[i]);	
		return -1;
	}
	cnt_thread++;
	//step3---启动PC监听和数据接收部分(TCP TO TTY)
	SNP_accpet_pc_proc();//主进程中启动
	/*i=0;
	if(pthread_create(&thread_pc_svr[i], NULL,(void *)SNP_accpet_pc_thread ,NULL)) {//fdc_accpet_pos_thread
		LOG_ERROR("启动TcpPcServ的[%s]失败", thread_name[i]);	
		return -1;
	}
	cnt_thread++;*/
	
	/*//WAIT_TIME(0,1) ;	
	while (1) {
		//sleep(1);
		for(i=0;i<cnt_thread;i++){ //TCP_POS_SVR_THREAD_NUM
			// 检查线程是否退出,若退出则重启之
			if (0 == pthread_kill(thread_pc_svr[i], 0))
			{
				//LOG_DEBUG("TcpPosServ的[%s]运行正常", thread_name[i]);
				continue;
			}
			LOG_ERROR("TcpPcServ的[%s]已退出,重启该线程", thread_name[i]);
			pthread_cancel(thread_pc_svr[i]); 
			//WAIT_TIME(0,3) ;//
			sleep(3);//等待一会儿重启
			//thread_pos_svr[i]=NULL;
			if (0==i)
			{
				if(pthread_create(&thread_pc_svr[i], NULL,(void *)SNP_accpet_pc_thread,NULL)) {
					LOG_ERROR("重启TcpPosServ的[%s]失败", thread_name[i]);	
					return -1;
				}
			}else if (1==i)
			{
				if(pthread_create(&thread_pc_svr[i], NULL,(void *)SNP_send_pc_thread,NULL)) {
					LOG_ERROR("重启TcpPcServ的[%s]失败", thread_name[i]);	
					return -1;
				}
			}else{
				LOG_ERROR("TcpPcServ的[线程%d]不存在",i );	//thread_name[i]
				return -1;
			}
		}
		
		
		
	}
	*/
	return 0;
}

//----------------------------------main proc-----------------------------

/*
 * \brief 加载配置
 * \return int 成功返回0，失败返回-1
 */
int load_sinopec_config_2()
{
	//油枪配置结构体,对应油枪配置文件dsp.cfg  modify by liys
	typedef struct {
		unsigned char node; 			//节点号
		unsigned char chnLog;		   //逻辑通道号
		unsigned char nodeDriver;	   //驱动号
		unsigned char phyfp;		   //物理面板号, 1+.
		unsigned char logfp;		   //逻辑面板号, 1+.
		unsigned char physical_gun_no; //物理油枪号 在此表示串口号
		unsigned char logical_gun_no;  //逻辑油枪号
		unsigned char oil_no[4];	   //油品号
		unsigned char isused;		   //是否启用
	} GUN_CONFIG;
	GUN_CONFIG *pGun_Config;

	int i, n, portfd, fd, guncnt;
	//unsigned char portschns[6]; // 重新定义为全局量
	unsigned char buff[sizeof(GUN_CONFIG)+1];

	// 读串口配置
	portfd = open("/home/App/ifsf/etc/portchnl.cfg", O_RDONLY);
	if (fd < 0) {
		LOG_ERROR( "打开配置文件[portchnl.cfg]失败.请检查是否已配置串口" );
		return -1;
	}
	memset(gpSNP_shm->portsChns, 0, 6);
	n = read(portfd, gpSNP_shm->portsChns, 6);
	if (n != 6) {
		LOG_ERROR("读配置文件[portchnl.cfg]失败");
		return -1;
	}
	close(portfd);
	
	// 读枪配置
	fd = open( "/home/App/ifsf/etc/dsp.cfg", O_RDONLY );
	if( fd < 0 ) {
		LOG_ERROR( "打开配置文件[dsp.cfg]失败.请检查是否已配置油枪" );
		return -1;
	}

	i = 0;
	guncnt = 0;
	while (1) {	
		memset( buff, 0, sizeof(buff) );
		n = read( fd, buff, sizeof(GUN_CONFIG) );
		if( n == 0 ) {	
			break;
		} else if( n < 0 ) {
			LOG_ERROR( "读取 dsp.cfg 配置文件出错" );
			close(fd);
			return -1;
		}

		//RunLog("第[%d]次读取到dsp.cfg 数据[%d]个字符\n",i+1, n);

		if( n != sizeof(GUN_CONFIG) ) {
			LOG_ERROR( "dsp.cfg 配置信息有误" );
			close( fd );
			return -1;
		}
		
		pGun_Config = (GUN_CONFIG*)(&buff[0]);
		if (pGun_Config->nodeDriver != 0x17) {
			LOG_ERROR("node_channel[%d]->nodeDriver = %d , continue", i+1, pGun_Config->nodeDriver);
			i++;
			continue;
		}

		gpSNP_shm->node_channel[i].nodeDriver = pGun_Config->nodeDriver;


		gpSNP_shm->node_channel[i].node = pGun_Config->node;
		gpSNP_shm->node_channel[i].chnLog = pGun_Config->chnLog;

		if (pGun_Config->chnLog > (gpSNP_shm->portsChns[0] + gpSNP_shm->portsChns[1] + gpSNP_shm->portsChns[2] + gpSNP_shm->portsChns[3])) {
			LOG_ERROR("卡机node[%d]背板连接号配置错误:%d", pGun_Config->node,  pGun_Config->chnLog);
		} else if (pGun_Config->chnLog > (gpSNP_shm->portsChns[0] + gpSNP_shm->portsChns[1] + gpSNP_shm->portsChns[2])) {
			gpSNP_shm->node_channel[i].serialPort = 6; 
			gpSNP_shm->node_channel[i].chnPhy = pGun_Config->chnLog - (gpSNP_shm->portsChns[0] + gpSNP_shm->portsChns[1] + gpSNP_shm->portsChns[2]);
		} else if (pGun_Config->chnLog > (gpSNP_shm->portsChns[0] + gpSNP_shm->portsChns[1])) {
			gpSNP_shm->node_channel[i].serialPort = 5; 
			gpSNP_shm->node_channel[i].chnPhy = pGun_Config->chnLog - (gpSNP_shm->portsChns[0] + gpSNP_shm->portsChns[1]);
		} else if (pGun_Config->chnLog > gpSNP_shm->portsChns[0]) {
			gpSNP_shm->node_channel[i].serialPort = 4; 
			gpSNP_shm->node_channel[i].chnPhy = pGun_Config->chnLog - gpSNP_shm->portsChns[0];
		} else if (pGun_Config->chnLog > 0) {
			gpSNP_shm->node_channel[i].serialPort = 3; 
			gpSNP_shm->node_channel[i].chnPhy = pGun_Config->chnLog;
		} else {
			LOG_ERROR("卡机node[%d]背板连接号配置错误:%d", pGun_Config->node,  pGun_Config->chnLog);
		}
		
		LOG_ERROR("卡机node[%d]背板连接号:%d", pGun_Config->node, pGun_Config->chnLog);




		gpSNP_shm->node_channel[i].isUse = pGun_Config->isused;
		gpSNP_shm->portLogUsed[gpSNP_shm->node_channel[i].serialPort -1] = 1;
		gpSNP_shm->chnLogUsed[pGun_Config->chnLog-1] = 1;
		
		LOG_ERROR("shm->node_channel[%d]: node[%02x], chnlog[%02x], serialPort[%02x], chnPhy[%02x], isused[%02x]", i,\
		gpSNP_shm->node_channel[i].node, \
		gpSNP_shm->node_channel[i].chnLog, \
		gpSNP_shm->node_channel[i].serialPort, \
		gpSNP_shm->node_channel[i].chnPhy, \
		gpSNP_shm->node_channel[i].isUse);
		LOG_ERROR("node[%d]->nodeDriver = %d ", gpSNP_shm->node_channel[i].node, gpSNP_shm->node_channel[i].nodeDriver);

		i++;
		guncnt++;  // gun number ++
	}
	
	gpSNP_shm->config_version = 1;
	gpSNP_shm->chnCnt = guncnt;	// dsp.cfg 中油枪配置条数/gun_config 的记录数

	gpSNP_shm->portCnt = 0;
	for(i = 0; i < MAX_PORT_CNT; i++) {
		if (gpSNP_shm->portLogUsed[i] == 1)
			gpSNP_shm->portCnt++;
	}

	return 0;	
}



/*
 * \brief 加载配置
 * \return int 成功返回0，失败返回-1
 */
int load_sinopec_config()
{
//初始化gpSNP_shm的如下内容
#if 0
//------------------
	unsigned char	chnLogUsed[MAX_CHANNEL_CNT];	/*显示逻辑通道是否使用*/
	unsigned char	portLogUsed[MAX_PORT_CNT];	/*显示串口是否使用*/
	unsigned char	chnLogSet[MAX_CHANNEL_CNT];	/*物理通道是否已经设置属性*/
	int		chnCnt;				/*总使用的通道数*/
	int		portCnt;			/*总使用的串口数*/
	SNP_NODE_CHANNEL   node_channel[MAX_CHANNEL_CNT];   // 以 Node - 1 为索引
//-------------------------
typedef struct{
	unsigned char node
	unsigned char chnLog; 
	unsigned char serialPort; //use which serial port in main board, it's zero in file, only used it in runtime.
	unsigned char chnPhy;  //phisical channel number at a serial port , it's zero in file, only used it in runtime.
	unsigned char isUse;
} SNP_NODE_CHANNEL;
#endif
	json_t *root = NULL;
	json_t *child = NULL;
	json_t *value = NULL;
	json_error_t error;
	size_t index;
	int node, chnLog, serialPort, chnPhy,isUse;
	int mode_no, auto_auth, rc;
	const char *key = NULL;
	const char *prod_name;
	const char *mode_name;
	const char *price;
	int tmp;

	assert(gpSNP_shm != NULL);

	LOG_INFO("开始加载配置");

	root = json_load_file(SNP_CONFIG_FILE, 0, &error);
	if (root == NULL) {
		LOG_ERROR("load %sfailed, err:%s", SNP_CONFIG_FILE,error.text);
		return -1;
	}

	// -- version
	child = json_object_get(root, "version");
	if (child == NULL) {
		LOG_ERROR("try to get version failed");
		rc = -1;
		goto EXIT_LOAD_SNP_CFG;
	}
	rc = json_unpack(child, "i", &tmp);
	LOG_INFO("config version: %d", tmp);
	gpSNP_shm->config_version = tmp;

	// -- chnCnt
	child = json_object_get(root, "chnCnt");
	if (child == NULL) {
		LOG_ERROR("try to get chnCnt failed");
		rc = -1;
		goto EXIT_LOAD_SNP_CFG;
	}
	rc = json_unpack(child, "i", &tmp);
	LOG_INFO("config chnCnt: %d", tmp);
	gpSNP_shm->chnCnt = tmp;

	// -- portCnt
	child = json_object_get(root, "portCnt");
	if (child == NULL) {
		LOG_ERROR("try to get portCnt failed");
		rc = -1;
		goto EXIT_LOAD_SNP_CFG;
	}
	rc = json_unpack(child, "i", &tmp);
	LOG_INFO("config portCnt: %d", tmp);
	gpSNP_shm->portCnt = tmp;

	// -- node channel	
	child = json_object_get(root, "node_channel");
	if (child == NULL) {
		LOG_ERROR("try to get channel failed");
		rc = -1;
		goto EXIT_LOAD_SNP_CFG;
	} else if (!json_is_array(child)) {
		LOG_ERROR("got channel, but it's not array");
		rc = -1;
		goto EXIT_LOAD_SNP_CFG;
	}

	json_array_foreach(child, index, value) {
		if (value == NULL) {
			LOG_ERROR("parsing channel error");
			rc = -1;
			goto EXIT_LOAD_SNP_CFG;
		}
		rc = json_unpack(value, "{s:i, s:i, s:i, s:i, s:i}",
				"node", &node, "chnLog", &chnLog,
				"serialPort", &serialPort, "chnPhy", &chnPhy, "isUse", &isUse);
		if (rc != 0) {
			LOG_ERROR("parsing dispenser, unpack failed");
			//JSON_DECREF(value);
			rc = -1;
			goto EXIT_LOAD_SNP_CFG;
		}

		LOG_INFO("index[%d]:{node:%d, chnLog:%d, serialPort:%d, chnPhy:%d, isUse:%d}",
						index, node, chnLog, serialPort, chnPhy, isUse);
		if (index >= MAX_GUN_CNT) {
			LOG_ERROR("加油机配置数量超限, %d vs %d", MAX_GUN_CNT, index + 1);
			continue;
		}
		gpSNP_shm->node_channel[index].node = node;
		gpSNP_shm->node_channel[index].chnLog = chnLog;
		gpSNP_shm->node_channel[index].serialPort = serialPort;
		gpSNP_shm->node_channel[index].chnPhy = chnPhy;
		gpSNP_shm->node_channel[index].isUse = isUse;
		//JSON_DECREF(value);
		gpSNP_shm->portLogUsed[serialPort-1] = 1;
		gpSNP_shm->chnLogUsed[chnLog-1] = 1;
	}
	LOG_INFO("channel cnt: %d\n", gpSNP_shm->chnCnt);
	//JSON_DECREF(child);

/*	
	// -- general -------------------------------------------------------------------------
	child = json_object_get(root, "general");
	if (child == NULL) {
		LOG_ERROR("try to get general failed");
		rc = -1;
		goto EXIT_LOAD_SNP_CFG;
	}

	json_object_foreach(child, key, value) {
		if (value == NULL) {
			LOG_ERROR("parsing general error, key:%s", key);
			rc = -1;
			goto EXIT_LOAD_SNP_CFG;
		}

		if (strcmp("log_level", key) == 0) {
			LOG_INFO("general.log_level:  %d", json_integer_value(value));
			log_set_level(json_integer_value(value));
		} else if (strcmp("cd_node", key) == 0) {
			LOG_INFO("general.cd_node:    %d", json_integer_value(value));
			gpIfsf_shm->cfg.cd_node = json_integer_value(value);
		} else if (strcmp("fdc_pos_port", key) == 0) {
			LOG_INFO("general.fdc_pos_port:    %d", json_integer_value(value));
		} else if (strcmp("ifsf_tcp_srv_port", key) == 0) {
			LOG_INFO("general.ifsf_tcp_srv_port:    %d", json_integer_value(value));
		} else {
			LOG_ERROR("general.unparsed:  %s", key);
		}

		//JSON_DECREF(value);
	}
*/

	rc = 0;
EXIT_LOAD_SNP_CFG:
	//JSON_DECREF(child);
	//JSON_DECREF(root);
	json_decref(root);

	return rc;
}


/*!
 * \brief  写进程pid到/var/run/appname.pid文件中
 * \param[in] appname 进程/程序名
 * \return    int     成功返回0，打开/创建文件失败返回-1, 写入失败返回-2
 */
int write_pidfile(const char *appname)
{
	int fd;
	int rc;
	char buf[40];

	snprintf(buf, sizeof(buf), "/var/run/%s.pid", appname);
	fd = open(buf, O_WRONLY|O_CREAT|O_TRUNC, 0666);
	if (fd < 0) {
		return -1;
	}

	sprintf(buf, "%d", getpid());
	rc = write(fd, buf, strlen(buf));
	if (rc != strlen(buf)) {
		return -2;
	}

	close(fd);
	return 0;
}

/*初始化共享内存gpSNP_shm,*/
int SNP_InitSNPShm()
{
	if(gpSNP_shm != NULL ) {
		memset(gpSNP_shm,  0,  sizeof(SNP_SHM));
	} else {
		return -1;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	int ret;
	if (argc == 2) {
	       if (strcmp(argv[1], "-v") == 0) {
			printf( "sinopec_main. Sinopec Machine Drive Version %s\n", SINOPEC_VER);
			return 0;
	       } else if (strcmp(argv[1], "--version") == 0) {
			printf( "sinopec_main. Sinopec Machine Drive Version %s\n", SINOPEC_VER);
			return 0;
	       } else {
		       printf("Usage: sinopec_main [-v |]\n");
		       return -1;
	       }
	}


	log_init(SNP_LOG_FILE);

	LOG_INFO("");
	LOG_INFO("Sinopec Machine Drive Main ( %s ) Startup ................ ", SINOPEC_VER);

	sleep(5);

	if (setenv("WORK_DIR", SNP_WORK_DIR, 1) != 0) {
		LOG_ERROR("设置环境变量 WORK_DIR 失败");
		return -1;
	}

	ret = SNP_InitKeyFile(); //this function is'nt changed.
	if (ret < 0) {
		LOG_ERROR( "初始化Key File失败" );
		return -1;
	}
	ret = InitKeyFile(); //this function is'nt changed.
	if (ret < 0) {
		LOG_ERROR( "初始化Key File失败" );
		return -1;
	}
	LOG_INFO("-----------------2--------------");
	ret = SNP_InitAll();
	if (ret < 0) {
		LOG_ERROR( "初始化失败");
		return -1;
	}
	LOG_INFO("-----------------3--------------");

	
	
	if (SNP_save_version() < 0) {
		LOG_ERROR("保存运行版本信息到sinopec_main.ver失败");
		return -1;
	}
	LOG_INFO("-----------------4--------------");

	if (write_pidfile("sinopec_main") < 0) {
		LOG_ERROR("写sinopec_main.pid失败");
		return -1;
	}
	LOG_ERROR("系统启动中，准备加载配置");
	sleep(1);
	LOG_ERROR("..............................");
	sleep(1);
	LOG_ERROR("..............................");
	sleep(1);
	//ret = load_sinopec_config();
	ret = load_sinopec_config_2();
	if( ret < 0 ) {
		LOG_ERROR("初始化配置失败.请检查配置文件");
		return -1;
	}
	LOG_INFO("-----------------5--------------");
	ret = SNP_StartProc();
	if( ret < 0 ) {
		LOG_ERROR("启动进程失败.请检查日志");
		return -1;
	}

	// do something 

	pause();

	DetachShm( (char**)(&gpSNP_shm) );
	LOG_INFO( "主进程退出.请检查日志" );
	
	return 0;

}


/*用于启动所有的服务进程*/
static int SNP_StartProc()
{
	pid_t	pid;
	int i;
	gpSNP_shm = (SNP_SHM*)AttachShm(SNP_SHM_INDEX );
	if (gpSNP_shm == NULL) {
		LOG_ERROR("连接共享内存SNP_SHM_INDEX失败");
		return -1;
	}	
	
	SNP_gPidList = (SNP_Pid*)Calloc(CHILD_PROC_CNT, sizeof(SNP_Pid));
	if( SNP_gPidList == NULL ) {
		LOG_ERROR( "分配内存失败");
		return -1;
	}


	SNP_cnt = 0;	/*用作统计已创建的子进程的数目*/

	SNP_WaitMyChild();

	//------------------------------------------------
	
#if 0	
	/*启动ARM<-->51串口监听程序*/
	for( i = 0; i < MAX_PORT_CNT; i++ ) {
		if( gpSNP_shm->portLogUsed[i] == 1 ) {	/*该串口使用*/
			pid = fork();
			{
				if( pid < 0 ) {
					Free( SNP_gPidList );
					UserLog( "分配串口子进程失败" );
					return -1;
				} else if( pid == 0 ) {	        /*子进程*/
					Free( SNP_gPidList );	/*子进程用不着这个，释放以节省空间*/
					SNP_isChild = 1;
					//SNP_ReadTTY( i+1 );
					while(1){sleep(1000);}
					
					DetachShm((char**)(&gpSNP_shm));					
					//DetachShm((char**)(&gpGunShm));
					
					exit(0);	/*子进程退出*/
				} else {
					sprintf( flag, "P%02d", i+1 );    /*add "%" by chenfm 2007-06-18*/
					SNP_AddPidCtl( SNP_gPidList, pid, flag, &SNP_cnt );	/*加入管理*/
					UserLog( "启动串口监听进程[%02d]", i+1 );
				}
			}
			//SNP_cnt++;
		}
	}
#endif
	
	// 后台监听Pc进程, TCP Pc Server
	pid = fork();
	{
		if( pid < 0 ) {
			LOG_ERROR( "fork TcpOptServ error" );
			Free( SNP_gPidList );
			return -1;
		} else if( pid == 0 ) {
			//Free( SNP_gPidList );
			SNP_isChild = 1;
			
			//fdc_pos_comm_srv_demo();
			//fdc_pos_demo();  // TEST
			SNP_TcpToPcServ();
						
			DetachShm((char**)(&gpSNP_shm));		
			exit(0);
		} else {  /*父进程*/
			SNP_AddPidCtl( SNP_gPidList, pid, "S", &SNP_cnt );
			LOG_INFO( "启动 TCP POS Server");
		}
	}

	// 日志检测备份
	pid = fork();
	{
		if( pid < 0 ) {
			Free( SNP_gPidList );
			LOG_ERROR( "fork error" );
			return -1;
		} else if( pid == 0 ) {
			Free( SNP_gPidList );
			SNP_isChild = 1;			
			SNP_Misc_Process();
						
			DetachShm((char**)(&gpSNP_shm));		
			exit(0);
		} else	{
			SNP_AddPidCtl( SNP_gPidList, pid, "F", &SNP_cnt );
			LOG_INFO( "启动日志备份进程");
		}
	}
	//-------------------------------------

	// 掉电由主进程监控, 把日志备份处理改为使用子进程
	pause();
	//StartPowerFailureServ();
	
	Free(SNP_gPidList);

	return 0;
}

static void SNP_WhenSigChld( int sig ) //this function is changed!!
{
	pid_t	pid;
	int     pc;
	char 	flag[4];
	SNP_Pid*	pidList;
	int	i;
	SNP_Pid *pFree;

	pidList = SNP_gPidList;
	pc = SNP_cnt;

	if(sig != SIGCHLD ) {
		return;
	}
	
	// 因为调用sigaction时,当前的信号会被阻塞,而被阻塞的信号以后只会发送一次,所以需要此处多处waitpid
	// 小于0表示出错.等于0表示无可等待的子进程
	while ((pid = waitpid( -1, NULL, WNOHANG)) > 0) {
		pc = SNP_cnt;

		/*子进程意外退出，查找子进程以重启*/
		LOG_ERROR( "进程[%d]退出, cnt:[%d], 当前pid[%d], ppid[%d]", pid ,pc, getpid(),getppid()); //remark only for test 
		
		for (i = 0; i < pc; i++) {
			pFree = pidList + i;
			if( pFree->pid == pid ) {
				strcpy( flag, pFree->flag );
				pc= i;
				break;
			}
		}

		if (i == SNP_cnt) {
			LOG_ERROR( "查找子进程失败" );
			Free(SNP_gPidList);
			exit( -1 );
		}

		pid = fork();
		if (pid < 0) {
			LOG_ERROR( "fork error, error:%s", strerror(errno));
			Free(SNP_gPidList);
			exit(-1);
		} else if (pid == 0) { /*根据flag重启进程*/
			SNP_isChild = 1;
			//Free( gPidList );
			if (flag[0] == 'S') {          // 数据接收进程
				LOG_INFO( "重启Tcp数据接收进程" );
				SNP_TcpToPcServ();

				DetachShm((char**)(&gpSNP_shm));					
				exit(0);
			} else if (flag[0] == 'P') {   // 串口监听进程
				UserLog( "重启串口监听进程[%s]", flag );
				SNP_ReadTTY(atoi(&flag[1]));

				DetachShm((char**)(&gpSNP_shm));
				exit(0);
			} else if (flag[0] == 'F') {   // 日志备份进程
				LOG_INFO( "重启日志备份进程" );
				SNP_Misc_Process();

				//DetachShm((char**)(&gpSNP_shm));					
				exit(0);
			} 
		} else {        // 父进程
			SNP_ReplacePidCtl( pidList, pid, flag, pc );
		}

	}
	return;
}

static int SNP_WaitMyChild(  int sig )
{

	sigset_t set;
	sigset_t oset;

	sigemptyset( &set );
	sigaddset( &set, SIGCHLD );

	if( sigprocmask( SIG_UNBLOCK, &set, &oset ) < 0 )	/*如果原先屏蔽字中有SIGCHLD,则解除该屏蔽*/
	{
		return -1;
	}

	if( Signal( SIGCHLD, SNP_WhenSigChld ) == ( Sigfunc* )( -1 ) )
		return -1;

	return 0;

}


/*
 * func:  保存当前所运行ifsf_main的版本信息到/var/run/ifsf_main.ver
 */
static int SNP_save_version()
{
	int fd;
	ssize_t ret;
	//unsigned char info[64] = {0};

	fd = open("/var/run/sinopec_main.ver", O_CREAT | O_WRONLY | O_TRUNC, 00444);
	if (fd < 0) {
		LOG_ERROR("Open /var/run/sinopec_main.ver failed!");
		return -1;
	}

	ret = write(fd, SINOPEC_VER, sizeof(SINOPEC_VER));
	if (ret != sizeof(SINOPEC_VER)) {
		LOG_ERROR("write version info to cd_main.ver failed!");
		return -1;
	}


	return 0;
}


/*
 * func: 处理一些琐事 (日志备份, 配置更新同步等)
 */

void SNP_Misc_Process()
{
	int cnt = 0;
	int cnt_tcc = 0;
	int i;
//	gpIfsf_shm->cfg_changed = 0;

	while (1) {
		// 1. 每隔10s检查一次标记 cfg_changed
		sleep(10);
		cnt++;
		cnt_tcc++;

		
		//Update_Prod_No();
/*		
		if (0x0F == gpIfsf_shm->cfg_changed) {
			SyncCfg2File();
		}
*/
		// 2. 每隔 10 分钟, 检查一次日志是否需要备份
		//if (cnt >= 60) {
		if (cnt >= 6) {
			cnt = 0;
			if (backup_log_file() < 0) {
				RunLog("exec backup_log_file failed");
			}
		}
	}
}




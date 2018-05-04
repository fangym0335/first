/*******************************************************************
本设计中协议转换是在oil_main的基础上进行的,除了以ifsf_开头的文件外,
其他相关文件也要修改.
注: oil_main也要做两处改变,首先是SRAM改为共享内存,同时在内核中增加掉电系统调用syscall0,
应用程序增加掉电写共享内存到nand flash的进程.
-------------------------------------------------------------------
IFSF_SHM,协议转换软件要使用的共享内存,
包括以心跳表的,待协议转换的加油机的记录表的,
待发送的数据信息表的共享内存.
-------------------------------------------------------------------
2007-06-26 by chenfm
2008-03-06 - Guojie Zhu <zhugj@css-intelligent.com>
    NODE_CHANNEL.convertUsed[]改为node_online[]
*******************************************************************/
#ifndef __IFSF_PUB_H__
#define __IFSF_PUB_H__

#include "pub.h"
#include "ifsf_def.h"
#include "ifsf_dspad.h"
#include "ifsf_tcpip.h"
#include "ifsf_dsp.h"
#include "ifsf_tlg.h"


// 条件编译--加入二配相关处理
#define SECOND_DISTRIBUTION  0


//前庭控制器3种授权模式,0和1只初始化各加油机时起作用,初始化后由上位机控制各加油机.
#define POS_CTRL_FCC  0 //上位机主控模式
#define POS_LISTEN_FCC 1 //上位机监视模式.上位机在线时前庭控制器自动授权.
#define FCC_SWITCH_AUTH 2  //本机授权开关控制模式,上位机不在线的时候自动置位.

#define DEFAULT_AUTH_MODE 0  //默认授权模式,只上位机在线时起作用,上位机不在线时自动转模式2,见IFSF_SHM->auth_flag

#define FP_TR_CONVERT_CNT  19456L //所有待协议转换的加油机的事务总数值.IFSF每个加油点9999个.每个加油机最多是256个
#define TEMP_TR_FILE  "/home/Data/dsptr"    //交易数据临时存放文件的前面部分,后面为两个字母的节点号.

//下面定义一个IFSF设备可能连接的其他IFSF设备的最大数，
//最大数根据IFSF_FP2_1.01附录1可得
#define MAX_RECV_CNT  64  // IFSF OVER LONWORK,LONWORK中的通讯数据库要求为64 
//#define MAX_TLG_CNT 1 //max count of  tlg in a fcc.  only one. 
//#define MAX_NO_RECV_TIMES  -6 //think a device such as CD is dead when it has not  heartbeat more HEARTBEAT_TIME  than MAX_NO_RECV_TIMES times 
#define MAX_NO_RECV_TIMES  -1 //think a device such as CD is dead when it has not  heartbeat more HEARTBEAT_TIME  than MAX_NO_RECV_TIMES times 

#define ACK       1
#define NACK      -1


// Mark in progress offline transaction. Guojie Zhu@2008.7.9
	// There is an in progress offline transaction (Start Of OFFLINE_TR)
#define SO_OFFLINE_TR(node, idx_fp)  (gpIfsf_shm->fp_offline_flag[(node - 1)][(idx_fp)] |= 0x0F)
	// Then in progress offline tr finished (End Of OFFLINE_TR)
#define EO_OFFLINE_TR(node, idx_fp)  (gpIfsf_shm->fp_offline_flag[(node - 1)][(idx_fp)] &= 0xF0)
	// Mark There is more than 1 offline tr of this node in TR_Buffer
#define SET_FP_OFFLINE_TR(node, idx_fp)  (gpIfsf_shm->fp_offline_flag[(node - 1)][(idx_fp)] |= 0xE0)
	// Mark There is no offline tr of this node in TR_Buffer
#define CLR_FP_OFFLINE_TR(node, idx_fp)  (gpIfsf_shm->fp_offline_flag[(node - 1)][(idx_fp)] &= 0x0F)


/*-------------------------------------------------------------------*/
//relation of node and logical channel, include serial port and physical channel. 
//and also include  4 import attributes of a dispenser, that is count of fuelling poits, nozzles,
//products and nozzles of each fuelling point. 12 bytes.
typedef struct{
	unsigned char node;
	//logical channel number in a PCD, the number is no. of the linked tag which signed  at back of FCC.	
	unsigned char chnLog; 
	unsigned char nodeDriver; //dispenser driver type,this determine how to initial and wich lischnxxx to use.	
	unsigned char cntOfFps; //count of fuelling points
	unsigned char cntOfNoz; //all count of Nozzles  in the dispenser.
	unsigned char cntOfProd;//all count of oil products in the dispenser.
	//count of Nozzles  in each FP of the dispenser, if there is not  2nd,3rd,4th FP then which set 0.
	unsigned char cntOfFpNoz[4]; 
	//runtime information
	unsigned char serialPort; //use which serial port in main board, it's zero in file, only used it in runtime.
	unsigned char chnPhy;  //phisical channel number at a serial port , it's zero in file, only used it in runtime.
} NODE_CHANNEL;

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
} GUN_CONFIG;


typedef struct{
        unsigned char oilno[4];
	unsigned char oilname[40];
	unsigned char price[3];        //BCD
} OIL_CONFIG;

//--------------------------------------------------------------------//
//IFSF_SHM,协议转换软件要使用的共享内存,
typedef struct{
	//授权模式标志,2为上位机离线模式,由开关控制是否可加油;默认初始化值为0或1,即上位机在线模式
	unsigned char auth_flag;//0:上位机控制加油,1:上位机监视下提枪可加油,2:由前庭控制器开关决定是否自动授权加油

	//二配数据标志，0:没有启动，1:启动但是不要实是数据，2:启动并要实时数据。
	unsigned char  css_data_flag; 
	
	//节点,逻辑通道,驱动号,面板数,总抢数,油品数以及每个面板油枪数的关系,包括串口和物理通道.
	NODE_CHANNEL   node_channel[MAX_CHANNEL_CNT];   // 以 Node - 1 为索引
	
	////通讯数据区:----------------------------------------------------	
	FCC_COM_SV_DATA  fcc_com_sv_data;//含自定义数据,广播地址,Data_Id=200.
	
	//心跳数据接收区:----------------------------------------------------		
	//标志下面的IFSF设备数组哪个有效，每次收到心跳时赋值为HEARTBEAT_TIME的值,
	//定时到收不到HEARTBEAT_INTERVAL信号减1,无效0.负值到-6?以上表示可以彻底删除.
	short  recvUsed[MAX_RECV_CNT];
	//IFSF设备数组 
	IFSF_HEARTBEAT recv_hb[MAX_RECV_CNT]; 
	
	//连接的CD的fd
	int recvSockfd[MAX_RECV_CNT];
		
	//心跳数据发送区:----------------------------------------------------	
	//标志下面的设备数组哪个在线，在线节点号,离线0, 本PCD设备不用心跳.   
	unsigned char node_online[MAX_CHANNEL_CNT + 1];//最后一个为液位仪的心跳
	//待协议转换的设备数组
	IFSF_HEARTBEAT convert_hb[MAX_CHANNEL_CNT + 1]; //最后一个为液位仪的心跳

	//液位仪--------------------------------------------------------------------------
	//液位仪,仅一个,Nb_Tanks值(TP个数)驱动程序计算得到.	
	TLG_DATA tlg_data;//[MAX_TLG_CNT]
	
	//待协议转换的所有加油机的结构体:-------------------------
	DISPENSER_DATA  dispenser_data[MAX_CHANNEL_CNT];  //预计不到1M.
	FP_TR_DATA fp_tr_data[FP_TR_CONVERT_CNT];         //import!! 预计达2M.	

	GUN_CONFIG gun_config[MAX_GUN_CNT];      // 油枪配置信息modify by liys 2008-03-14
	OIL_CONFIG oil_config[MAX_OIL_CNT];      // 油品配置信息modify by liys 2008-03-14
	unsigned char gunCnt;                    // 总枪数 added by jie @ 2008-03-24
	unsigned char oilCnt;                    // 总油品数 added by jie @ 2008-11-10
	unsigned char need_reconnect;            // Tcp.C 需要重连Server
	unsigned char is_new_kernel;             // 1 - new version kernel(2.6.21-fcc-x.x), 0 - old version kernel(2.6.21)
	unsigned char log_level;                 // 日志等级
	// fp_offline_flag 标记缓冲区中是否有脱机交易或者该fp是否有未完成的脱机交易
	unsigned char fp_offline_flag[MAX_CHANNEL_CNT][MAX_FP_CNT];
	unsigned char msg_cnt_in_queue[MAX_CHANNEL_CNT]; // 消息队列ORD_MSG_TRAN中的消息条数 // added by jie @2008-10-27
	unsigned char cfg_changed;               // 标记油品配置是否有改变, 0x0F-有改变, 0xFF-正在同步, 0-无改变
	unsigned char get_total_flag;
	unsigned char tank_heartbeat;          //0:关闭液位仪心跳发送 1:打开
} IFSF_SHM;

int VrPanelNo[MAX_CHANNEL_CNT][MAX_PANEL_CNT];//Videer-Root液位仪校准所需要的大排序面板号
int OpwGunNo[MAX_CHANNEL_CNT][MAX_PANEL_CNT][8];  //opw-pv4 液位仪校准所需的大排序枪号

extern IFSF_SHM *gpIfsf_shm; //该结构是全局的量，只有一个,在ifsf_main.h中定义.
void show_all_dispenser();
void show_dispenser(unsigned char node);
void show_pump(PUMP *ppump);
int price_change_track();

#endif


/*
 * Filename: lischn021.c
 *
 * Description: 加油机协议转换模块—美国长吉,2Line CL / RS485, 5760,E,8,1 
 *
 * History:
 *
 * 2008/02/25  - Guojie Zhu <zhugj@css-intelligent.com> 
 *     基于陈博士200802021830版本修改
 *     增加 InitGunLog(), GetCurrentLNZ(), ChangePrice()
 *     增加 油机离线判断, 增加FP State Error的处理, 增加Started状态
 *     删除 GetLogicGun(), 删除冗余代码
 *     修改 DoPrice(), 将其分离为DoPrice() 和 ChangePrice(0 两部分	
 *     若干其他细节的修改(可对照bak/lischn021.c查看)
 *
 * 2008/03/06  - Guojie Zhu <zhugj@css-intelligent.com> 
 *     增加未支付交易笔数是否达到限制的判断(ReachedUnPaidLimit)
 *     BeginTr4Fp移至第一次DoBusy之前
 *     convertUsed[]改为node_online[]
 * 2008/03/30 - Guojie Zhu <zhugj@css-intelligent.com>
 *     修改 DoCmd, 错误判断会放入HandleStateErr
 *     修改 AckFirst 处理方式
 *     将未支付交易笔数达到限制判断放入HandleStateErr
 * 2008/05/23 - Guojie Zhu <zhugj@css-intelligent.com>
 *     定额加油命令改为6位模式
 *
 * 010命令增加了判断小数位({ 0xff, 0xe9, 0xfe, 0xe0, 0xe1, 0xe0, 0xfb, 0xee, 0xf0 };)
 *回复:// 1枪抬起 ret: ba b0 b3  B1  b0 b1 b0 b0 b0  b0 b1 b0 B1 b1 B1  c3 c1 8d 8a
 *		// 无枪抬起ret: ba b0 b3  B1  b0 b1 b0 b0 b0  b0 b1 b0 B0 b1 B0  c3 c3 8d 8a
 *老的机器都是传0
 *ret[9] = 0
 *ret[9]为0 表示1为小数 不需要判断百分位小数
 *
 *新机器传1
 *ret[9] = 1				                                           1枪     2枪     3枪     4枪
 *需要取010命令 判断百分位小数位 ret[10] ret[11] ret[12] ret[13] 
 *读取泵码时需要发送010命令 判断百分位小数 注意根据面号判断
 *
 *新机器传2
 *ret[9] = 2
 *泵码读取的是两位小数，没有最前的百万位
 *
 */

#include "pub.h"
#include "doshm.h"
#include "tcp.h"
#include "dosem.h"
#include "domsgq.h"
#include "commipc.h"
#include "tty.h"
#include "ifsf_def.h"
#include "ifsf_pub.h"
#include "ifsf_dspad.h"

#define GUN_EXTEND_LEN  8

// FP State
#define PUMP_ERROR	0x00
#define PUMP_OFF	0x06
#define PUMP_CALL	0x07
#define PUMP_AUTH	0x08
#define PUMP_BUSY	0x09
#define PUMP_PEOT	0x0A
#define PUMP_FEOT	0x0B
#define PUMP_STOP	0x0C

#define TRY_TIME	5
#define INTERVAL	10000    // 命令发送间隔, 数值视具体油机而定, 10ms
#define INTERVAL_TOTAL	75000    // 命令发送间隔, 数值视具体油机而定, 10ms


//存长吉百分位数据
typedef struct{
	unsigned char lnzOne;
	unsigned char lnzTwo;
	unsigned char lnzThr;
	unsigned char lnzFou;
}GILPERCENTILE; 

static int DoCmd();
static void DoPolling();
static int DoAuth(int pos);
static int DoStarted(int i);
static int DoBusy(int pos);
static int DoCall(int pos);
static int DoEot(int pos);
static int DoOpen(int pos, __u8 *msgbuffer);
static int DoClose(int pos, __u8 *msgbuffer);
static int DoRation(int pos);
static int DoStop(int pos);
static int DoMode();
static int DoPrice();
static int ChangePrice();
static int GetGunTotal(int pos, __u8 gunLog, __u8 *vol_total, __u8 *amount_total);
static int GetGunPrice(int pos, __u8 gunLog, __u8 *price);
static int GetTransData(int pos);
static int GetPumpStatus(int pos, __u8 *status);
static int GetPumpOnline(int pos, __u8 *status);
static int CancelRation(int pos);
static int InitPumpTotal();
static int InitCurrentPrice();
static int InitGunLog();
static int GetCurrentLNZ(int pos);
static int keep_in_touch_xs(int x_seconds);
static int SetPort001();
static int ReDoSetPort001();
static int DoRation_Idle(int pos);
static int CheckPriceErr(int pos);
static int GetPumpPoint();
static int GetPumpPercentile(const int pos);


static PUMP pump;
static int point_flag = 0; /*长吉泵码小数点位标记
					0 :一位小数点  
					1 :一位小数点，百分位小数在010命令中
					2 :两位小数点，一条泵码命令就够了
					*/



static GILPERCENTILE GilPercentile[MAX_FP_CNT];
					
static int node;
static int IFSFpos;
static DISPENSER_DATA *lpDispenser_data;
static NODE_CHANNEL *lpNode_channel;
static int fp_state[MAX_FP_CNT];   // 0 - IDLE, 1 - CALLING , 2 - DoStarted成功, 3 - FUELLING
static int dostop_flag[MAX_FP_CNT] = {0};
static __u8 ration_flag[MAX_FP_CNT] = {0};          // 标识是否有预置要下发
static __u8 ration_type[MAX_FP_CNT] = {0};          // 标识要下发预置的类型
static __u8 ration_value[MAX_FP_CNT][3] = {{0,0}};  // 缓存预置量
static __u8 ration_noz_mask[MAX_FP_CNT] = {0};      // 标识预置允许枪号

int ListenChn001(int chnLog)
{
	long	indtyp;
	int	ret;
	int	fd;
	int	j;
	int	i,icount;
	__u8 status;
	
	sleep(5);   //防止此进程退出,而主进程没有启动完毕. 每个lischnxxx都需要
	RunLog("Listen Channel  001 ,logic channel[%d]", chnLog);
	memset(&pump, 0, sizeof(pump));
	pump.chnLog = chnLog;
	pump.panelCnt = 0;

	// ============ Part.1 初始化 pump 结构  ======================================
	// 扫描共享内存, 检查该通道连接的串口, 面板
	
	for (i = 0; i < MAX_CHANNEL_CNT; i++) {
		if (chnLog == gpIfsf_shm->node_channel[i].chnLog) {
			IFSFpos = i;
			lpNode_channel = (NODE_CHANNEL *)(&(gpIfsf_shm->node_channel[i]));
			// node_channel和dispenser_data必须一一对应
			lpDispenser_data = (DISPENSER_DATA *)(&gpIfsf_shm->dispenser_data[i]);
			node = lpNode_channel->node;
			pump.port =lpNode_channel->serialPort;
			pump.chnPhy = lpNode_channel->chnPhy;
			pump.panelCnt = lpNode_channel->cntOfFps;
			
			int idx;
			for (j = 0; j < pump.panelCnt; j++) {
				// initial panelPhy - addr of fp for communication 
				for (idx = 0; idx < gpIfsf_shm->gunCnt; idx++) {
					if ((gpIfsf_shm->gun_config[idx].node == node) &&
						(gpIfsf_shm->gun_config[idx].isused == 1) &&
							(gpIfsf_shm->gun_config[idx].logfp == j + 1)) {
						// 1 -- Pump ID 1, ...... ,0 -- Pump ID 16
						pump.panelPhy[j] = (gpIfsf_shm->gun_config[idx].phyfp) % 16;
						break;
					}
				}
				pump.gunCnt[j] =  gpIfsf_shm->node_channel[i].cntOfFpNoz[j];
				// 用做标识是否抬枪后第一次取实时加油数据
				// 长吉油机在Fuelling状态第一次取实时数据时,要求等的时间较长.故用addinf[0]做一个标识
				pump.addInf[j][0] = 0;	      // Note. 用途视具体机型而定
			// 用做标识是否从busy状态直接转换为了call状态.busy时置1,off或eot时恢复为0.如果call时为1,则取加油结果
				pump.addInf[j][1] = 0;	      // Note. 用途视具体机型而定
			}
				
			break;
		}
	}
	pump.indtyp = chnLog;
	bzero(&pump.newPrice, FP_PR_NB_CNT * 4);
	
#if 0
	show_dispenser(node); 
	show_pump(&pump);
#endif

	// ============= Part.2 打开 & 设置串口属性 -- 波特率/校验位等 ===================

	strcpy(pump.cmd, alltty[pump.port - 1]);
	pump.fd = OpenSerial(pump.cmd);
	if (pump.fd < 0) {
		RunLog("节点[%d], 打开串口[%s]失败", node, pump.cmd);
		return -1;
	}
//	RunLog("节点[%d]打开串口[%s]成功",node, pump.cmd);

	ret = SetPort001();
	if (ret < 0) {
		RunLog("节点[%d], 设置串口属性失败",node);
		return -1;
	}
	
	// ============== Part.3 判断加油机是否在线 ====================================
	// Check wether dispenser is online, if it's not online, wait  here.
	while (1) {
		ret = GetPumpOnline(0, &status);
		if (ret == 0) {
			break;
		}

		gpIfsf_shm->node_online[IFSFpos] = 0; //不在线,不产生心跳.
		UserLog("<长吉> 节点号[%d] , 不在线", node);
		
		sleep(1);       // 固定轮询间隔1s
	}

	//解决多面油机断电重起初始化泵码失败，造成油机通讯不畅的问题
	for (icount = 0; icount < pump.panelCnt; icount++) {
		ret = GetPumpOnline(icount, &status);
	}	

	
	// ============= Part.4 初始化与加油机当前状态相关的数据库相关项 =======================
#if 0
	// commented by jie @ 2009.4.23 , 已经不再需要
	ret = InitGunLog();    // Note.InitGunLog 并不是每种品牌油机都必须,长吉油机预置加油需要
	if (ret < 0) {
		RunLog("节点[%d], 初始化默认当前逻辑枪号失败",node);
		return -1;
	}
#endif

	/*读取泵码小数位数*/
	ret = GetPumpPoint();
	if (ret < 0) {
		RunLog("节点[%d], 读取泵码小数位失败!默认一位小数",node);
		point_flag = 0;
	}

	ret = InitCurrentPrice(); //  初始化fp数据库的当前油价
	if (ret < 0) {
		RunLog("节点[%d], 初始化油品单价到数据库失败",node);
		return -1;
	}

	ret = InitPumpTotal();    // 初始化泵码到数据库
	if (ret < 0) {
		RunLog("节点[%d], 初始化泵码到数据库失败",node);
		return -1;
	}

	


	// 一切就绪, 上线
//	gpIfsf_shm->node_online[IFSFpos] = node;
	ChangeNode_Status(node, NODE_ONLINE);
	UserLog("节点[%d]在线,状态[%02x]!", node, status);
	keep_in_touch_xs(12);               // 确保CD已经收到心跳之后再做后续操作

	// =============== Part.5 油机模块初始化完毕,转换FP状态为 Idle ==================
	// when dispenser is initialized , the status is Closed. it's waiting for set command from CD. 
	for (i = 0; i < lpNode_channel->cntOfFps; i++) {
		if ((__u8)FP_State_Closed != lpDispenser_data->fp_data[i].FP_State) {
			ret = ChangeFP_State(node, 0x21 + i, FP_State_Closed, NULL);
		}
	}

	// =============== Part.6 运行时主体程序 ========================================
	while (1) {
		if (gpGunShm->sysStat == RESTART_PROC ||
			gpGunShm->sysStat == STOP_PROC) {
			RunLog("系统退出");
			exit(0);
		}

		while (gpGunShm->sysStat == PAUSE_PROC) {
			__u8 status;
			GetPumpStatus(0, &status);              // Note. 与油机保持联系
			Ssleep(INTERVAL);
			RunLog("系统暂停");
		}

		indtyp = pump.indtyp;

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		ret = -1;

		// ---------- Part.6.1 后台在线情况下的处理流程 ----------------------
		if (gpIfsf_shm->auth_flag - FCC_SWITCH_AUTH != 0) {
		       	/* 
			 * 检查是否有后台命令, 如果有则转入DoCmd执行该命令; 如果没有, 则继续轮询
			 */
			ret = RecvMsg(ORD_MSG_TRAN, pump.cmd, &pump.cmdlen, &indtyp, TIME_FOR_RECV_CMD); 
			if (ret == 0) {
				ret = DoCmd();
				if (ret != 0) {
					RunLog("Node %d 处理后台命令失败", node);
				}
			}

			DoPolling();
			Ssleep(500000);       // 正常轮询间隔为100ms + INTERVAL
		} else { // ------------- Part.6.2 后台离线情况下的处理流程 ------------ 
			Ssleep(200000);
			DoPolling();
		}
	} // end of main endless loop

}

/*
 * Func: 执行上位机命令
 * 命令格式: 命令代码(2Bytes) + FP_Id(1Byte) + 标准命令代码在"Ack Message"中的位置(1Byte) +
 *           后续数据长度(1Byte) + Ack Message(nBytes)
 */
static int DoCmd()
{
	int ret;
	int fp_id;
	int pos;    //pos is index of  fuelling poit(panel) of a dispenser: 0, 1, 2, 3
	unsigned int cnt;
	__u8 msgbuffer[48];

	if (pump.cmd[0] != 0x2F) {
		// 提取调用命令执行函数前的共有操作
		fp_id = pump.cmd[2];
		if (fp_id == 0x00) {  // fp_id == 0x00 表示对所有fp操作, 对应 pos 置为 255
			/*
			 * Note!!! pos 不能赋值为255,
			 * 因为与FP相关的数组大小只有4Bytes, 输入255会越界
			 * 如果以后确实要支持针对全部FP的操作, 那么需要检查修改相关处理;
			 * 目前的状态使用pos=255的模式肯定会出问题
			 *  Guojie Zhu @ 2009.03.18
			 */
			pos = 255;
		} else if ((fp_id >= 0x21) && (fp_id <= 0x24)) {
			pos = fp_id - 0x21;
		} else {
			if (pump.cmd[0] != 0x18 && pump.cmd[0] != 0x19) {  // DoPrice & DoMode不需要处理面板号
				RunLog("节点[%d] , 非法的面板地址",node);
				return -1;
			}
		}				

		bzero(msgbuffer, sizeof(msgbuffer));
		memcpy(msgbuffer, &pump.cmd[3], pump.cmd[4] + 2);  // 本次操作的 Acknowledge Message
	}

	ret = -1;
	switch (pump.cmd[0]) {
	case 0x18:	//  Change Product Price
		ret = DoPrice();		
		break;		
	case 0x19:	//  更改油机的控制模式: 上位机控制 或 自主加油
//			ret = DoMode();	      // Unsupported
		break;
	case 0x1C:	// Terminate_FP
		// Step.1 先进行错误处理, 检查当前状态是否允许进行该操作
		ret = HandleStateErr(node, pos, Terminate_FP,
			lpDispenser_data->fp_data[pos].FP_State, msgbuffer); 
		if (ret < 0) {
			break;
		}
		// Step.2 执行命令, 若成功改变 FP 状态, 否则返回 NACK
		if ((__u8)FP_State_Calling == lpDispenser_data->fp_data[pos].FP_State) {
			ret = 0;  // 抬枪状态执行停止会失败, 所以直接不执行, 仅转换FP状态
		} else {
			ret = DoStop(pos);
		}

		if (ret == 0) {
			ration_flag[pos] = 0;           //预置标志置为未预置0
			ration_type[pos] = 0;
			memset(&ration_value[pos], 0, 3);
			ret = ChangeFP_State(node, pos + 0x21, FP_State_Idle, msgbuffer);
		} else {
			SendCmdAck(NACK, msgbuffer);
			ChangeFP_State(node, pos + 0x21,
				lpDispenser_data->fp_data[pos].FP_State, NULL);
		}

		break;
	case 0x1D:	// Release_FP
		ret = HandleStateErr(node, pos, Do_Auth,     // Note. 这里用Do_Auth 而不是 Release_FP 
			lpDispenser_data->fp_data[pos].FP_State, msgbuffer); 
		if (ret < 0) {
			break;
		}
		ret = DoAuth(pos);				
		if (ret == 0) {
			ret = ChangeFP_State(node, pos + 0x21, FP_State_Started, msgbuffer);
		} else {
			SendCmdAck(NACK, msgbuffer);
			ChangeFP_State(node, pos + 0x21,
				lpDispenser_data->fp_data[pos].FP_State, NULL);
		}

		break;
	case 0x3D:      // Preset  Amount or Volume
	case 0x4D:
		ret = HandleStateErr(node, pos, Do_Ration,   // Note. 这里用Do_Ration
			lpDispenser_data->fp_data[pos].FP_State, msgbuffer); 
		if (ret < 0) {
			//lpDispenser_data->fp_data[pos].Log_Noz_Mask = 0xFF;
			bzero(&lpDispenser_data->fp_data[pos].Remote_Volume_Preset, 5);
			bzero(&lpDispenser_data->fp_data[pos].Remote_Amount_Prepay, 5);
			break;
		}
		RunLog("HandleStateErr OK, go to PreSet:");


		/* Guojie Zhu @ 2009.7.19
		 * 因为空闲时下发预置因为油机本身的设计问题需要连发三次才能避免预置失败,
		 * 但是这样会导致预置处理时间过长, 所以尝试改为抬枪后下发
		 *
		 */
		/*
		 * 抬枪之后下发预置所使用的标记变量和缓冲区:
		 * ration_flag[pos]: 预置标记, 标识是否抬枪后是否有预置需要下发
		 * ration_type[pos]: 预置方式标记字节
		 * ration_value[pos]:  预置量缓存
		 * ration_noz_mask[pos]: 预置允许油枪, 0xFF - 允许全部
		 */
		ration_flag[pos] = 1;            // 预置标记, 标记有预置需要下发
		ration_type[pos] = pump.cmd[0];  // 预置方式
		ration_noz_mask[pos] = lpDispenser_data->fp_data[pos].Log_Noz_Mask;
		RunLog("ration_noz_mask:%02x", ration_noz_mask[pos]);

		if (pump.cmd[0] == 0x3D) {
			memcpy(&ration_value[pos], 
				&lpDispenser_data->fp_data[pos].Remote_Volume_Preset[2], 3);
			ret = 0;
		} else if (pump.cmd[0] == 0x4D) {
			memcpy(&ration_value[pos], 
				&lpDispenser_data->fp_data[pos].Remote_Amount_Prepay[1], 3); //MianDian, Amount is big, 06YYYYYY.00
			ret = 0;
		} else {
			ret = -1;
		}

		if (ret == 0) {
			if ((__u8)FP_State_Calling == lpDispenser_data->fp_data[pos].FP_State) {
				ret = SendData2CD(&msgbuffer[2], msgbuffer[1], 5);
				if (ret < 0) {
					RunLog("Send Ack Msg [%s] to CD failed", 
						Asc2Hex(&msgbuffer[2], msgbuffer[1]));
				}
			} else {
				ret = ChangeFP_State(node, pos + 0x21, FP_State_Authorised, msgbuffer);
			}
		} else {
			SendCmdAck(NACK, msgbuffer);
			ChangeFP_State(node, pos + 0x21,
				lpDispenser_data->fp_data[pos].FP_State, NULL);
		}

		bzero(&lpDispenser_data->fp_data[pos].Remote_Volume_Preset, 5);
		bzero(&lpDispenser_data->fp_data[pos].Remote_Amount_Prepay, 5);
		break;	
	case 0x1E:     // Open_FP
		ret = HandleStateErr(node, pos, Open_FP,
			lpDispenser_data->fp_data[pos].FP_State, msgbuffer); 
		if (ret < 0) {
			break;
		}
		ret = DoOpen(pos, msgbuffer);
		break;
	case 0x1F:     // Close_FP
		ret = HandleStateErr(node, pos, Close_FP,
			lpDispenser_data->fp_data[pos].FP_State, msgbuffer); 
		if (ret < 0) {
			break;
		}
		ret = DoClose(pos, msgbuffer);
		break;	
	case 0x2F:
		ret = upload_offline_tr(node, &pump.cmd[3]);
		break;
	default:       // Unknow
		RunLog("节点[%d], 接受到未定义命令[0x%02x]",node, pump.cmd[0]);
		break;
	}

	return ret;
}

/*
 * func: 对FP进行状态轮询, 根据状态做相应处理
 */
static void DoPolling()
{
	int i, j;
	int ret;
	__u8 status;
	int tmp_lnz;
	int pump_err_cnt[MAX_FP_CNT] = {0};
	static __u8 pre_status[MAX_FP_CNT] = {0};
	static int times = 0;
	static int fail_cnt = 0;  // 记录GetPumpStatus 失败次数, 用于判定离线
	int trynum;
	
	for (i = 0; i < pump.panelCnt; i++) {
		ret = GetPumpStatus(i, &status);
		if (ret < 0) {
			if (gpIfsf_shm->node_online[IFSFpos] == node) {
				if ((++fail_cnt) < MAX_FAIL_CNT) {  // 加油机离线判断
					/*
					 * 2008.12.29增加此操作, 解决因电压不稳造成
					 * 单片机复位后串口设置复位最终导致通讯不通的问题
					 */
					/*
					 * 因重设串口属性会导致ReadTTY接收到"非法数据", 故暂时移除
					 * 2009.02.19
					if (fail_cnt >= (MAX_FAIL_CNT / 2)) {
						if (SetPort001() >= 0) {
							RunLog("节点%d重设串口属性成功", node);
						}
					}
					*/
					continue;
				}

				// Step.1
				memset(fp_state, 0, sizeof(fp_state));
				memset(dostop_flag, 0, sizeof(dostop_flag));

				// Step.2
//				gpIfsf_shm->node_online[IFSFpos] = 0;
				ChangeNode_Status(node, NODE_OFFLINE);
				pump.time_offline = (3 * gpIfsf_shm->fcc_com_sv_data.Heartbeat_Interval) + time(NULL);
				RunLog("======节点[%d] 离线! ===========================", node);

				// Step.3
				for (j = 0; j < pump.panelCnt; ++j) {
					lpDispenser_data->fp_data[j].FP_State = FP_State_Inoperative;
					lpDispenser_data->fp_data[j].Log_Noz_State = 0;
				}
			}

			// 重设串口属性
			//ReDoSetPort021(); 2009.02.19
			continue;
		} else {
			if (gpIfsf_shm->node_online[IFSFpos] == 0) {
				if ((--fail_cnt) > 0) { // 排除干扰
					fail_cnt = 1;
					continue;
				} else if (time(NULL) < pump.time_offline) {
					RunLog("====================== wait for 30s, ==============");
					continue;
				}

				// fail_cnt复位
				fail_cnt = 0;

				/*读取泵码小数位数*/
				ret = GetPumpPoint();
				if (ret < 0) {
					RunLog("节点[%d], 读取泵码小数位失败!默认一位小数",node);
					point_flag = 0;
				}

				// Step.1
				ret = InitPumpTotal();    // 初始化泵码到数据库
				if (ret < 0) {
					RunLog("节点[%d], 更新泵码到数据库失败",node);
					continue;
				}

				// Step.2
				for (j = 0; j < pump.panelCnt; ++j) {
					lpDispenser_data->fp_data[j].FP_State = FP_State_Closed;
				}

				// Step.3
//				gpIfsf_shm->node_online[IFSFpos] = node;
				ChangeNode_Status(node, NODE_ONLINE);
				RunLog("====== 节点[%d] 恢复联线! =======================", node);

				
				
				bzero(ration_flag,sizeof(ration_flag));     //回复连线后将所有面的预置标志置为0
				bzero(ration_type,sizeof(ration_type));
				keep_in_touch_xs(12);               // 确保CD已经收到心跳之后再做后续操作
				// Step.4                 // 主动上传当前FP状态
				for (j = 0; j < pump.panelCnt; ++j) {
					ChangeFP_State(node, j + 0x21, FP_State_Closed, NULL);
				}
				continue;
			}

			fail_cnt = 0;
			if (status != PUMP_ERROR) {
				pump_err_cnt[i] = 0;
			}
		}

		if (status != pre_status[i]) {
			pre_status[i] = status;
			RunLog("节点[%d]面板[%d]当前状态[%02x]", node, i + 1, status);
		}

		if (gpIfsf_shm->auth_flag - FCC_SWITCH_AUTH != 0) {  //如果有后台
			if ((__u8)FP_State_Closed == lpDispenser_data->fp_data[i].FP_State) {
				if (i == 0) {
					times += pump.panelCnt;					
				}
				if (times > (SENDSTATE *  pump.panelCnt)) {
					if (i >= pump.panelCnt -1)
						times = 0;
					RunLog("节点[%d]面板[%d] 处于Closed状态", node, i + 1);
				}

				// Closed 状态下也要能够变价
				for (j = 0; j < pump.panelCnt; ++j) {
					if ((__u8)FP_State_Idle <  lpDispenser_data->fp_data[j].FP_State) {
						break;
					}
				}
				// 所有面都空闲才变价
				if (j >= pump.panelCnt) {
					ret = ChangePrice();            // 查看是否有油品需要变价,有则执行
				}
				continue; 
			}
		} else {
			if ((__u8)FP_State_Closed == lpDispenser_data->fp_data[i].FP_State) {
				RunLog("节点[%d]面板[%d], DoPolling, 没有后台改变状态为IDLE", node,i + 1);
				ret = SetHbStatus(node, 0);   // IFSF_HEARTBEAT.status set to 0
				ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
			}
		}

		// 若FP状态为Inoperative, 要求人工干预,程序不能改变其状态,也不能在该状态下做任何操作
		if ((__u8)FP_State_Inoperative == lpDispenser_data->fp_data[i].FP_State) {
			RunLog("节点[%d]面板[%d] 处于Inoperative状态!", node, i + 1);
			// Inoperative 状态下也要能够变价
			for (j = 0; j < pump.panelCnt; ++j) {
				if ((__u8)FP_State_Idle <  lpDispenser_data->fp_data[j].FP_State) {
					break;
				}
			}
			// 所有面都空闲才变价
			if (j >= pump.panelCnt) {
				ret = ChangePrice();            // 查看是否有油品需要变价,有则执行
			}

			CheckPriceErr(i);
			continue;
		}

		switch (status) {
		case PUMP_ERROR:				// Do nothing
			break;
		case PUMP_OFF:					// 挂枪、空闲
			if (ration_flag[i] != 0) {              // 有预置待下发
				// 清除油枪状态, 避免抬错枪的情况下重新抬枪DoStarted无法读取枪号
				lpDispenser_data->fp_data[i].Log_Noz_State = 0;
				break; 	
			}
					 
			dostop_flag[i] = 0;
			pump.addInf[i][0] = 0;

			if (fp_state[i] >= 2) {                 // Note.对没有采到挂枪信号(EOT)的补充处理
				// 若有加过油, 则进行状态改变和交易处理
				// 1. ChangeFP_State to IDLE first
				tmp_lnz = lpDispenser_data->fp_data[i].Log_Noz_State;
				lpDispenser_data->fp_data[i].Log_Noz_State = 0x00;     
				if ((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State) {
					ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
				}
				// Step2. Get & Save TR to TR_BUFFER
				lpDispenser_data->fp_data[i].Log_Noz_State = tmp_lnz;
				ret = DoEot(i);
			}

			lpDispenser_data->fp_data[i].Log_Noz_State = 0x00;
			fp_state[i] = 0;

			if ((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State) {
				ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
			}

			EO_OFFLINE_TR(node, i);
			if (gpIfsf_shm->fp_offline_flag[node - 1][i] != 0) {
				RunLog("Dopolling, call upload_offline_tr");
				upload_offline_tr(node, NULL);
			}

			// 有单独的已授权状态, 无需附加判断状态
			for (j = 0; j < pump.panelCnt; ++j) {
				if ((__u8)FP_State_Idle != lpDispenser_data->fp_data[j].FP_State) {
					break;
				}
			}
			// 所有面都空闲才变价
			if (j >= pump.panelCnt) {
				ret = ChangePrice();            // 查看是否有油品需要变价,有则执行
			}
			break;
		case PUMP_CALL:					// 抬枪
			RunLog("FP_State: %02x", lpDispenser_data->fp_data[i].FP_State);
			// 1. PAK Calling
			if ((__u8)FP_State_Calling > lpDispenser_data->fp_data[i].FP_State) {
				fp_state[i] = 1;

				if (DoCall(i) < 0) {
					break;
				}

				if (DoStarted(i) < 0) {
					break;
				}
			}
			//挂枪空交易之前授权，定额量为0，油机不控制，增加特殊处理 
			if (ration_flag[i] != 0 && ration_value[i][0] == 0 && ration_value[i][1] == 0 && ration_value[i][2] == 0) {
				RunLog("挂枪过程中预授权，再次抬枪预授权取消");
				ration_flag[i] = 0;          // 标识是否有预置要下发
				ration_type[i] = 0;          // 标识要下发预置的类型
				ration_noz_mask[i] = 0;      // 标识预置允许枪号
				lpDispenser_data->fp_data[i].Log_Noz_Mask = 0xFF;
				break;
			}
			
			if (ration_flag[i] != 0) {           // 有预置量未下发
				fp_state[i] = 1;

				if (DoStarted(i) < 0) {
					break;
				}
				/* Note! DoStarted 要在DoRation之前执行,
				 * DoRation需要当前抬枪的枪号, 需要在抬枪状态下发预置量
				 */

				// 判断抬起的是否是需要预置的油枪, 如果不是则忽略
				if ((lpDispenser_data->fp_data[i].Log_Noz_State !=
								ration_noz_mask[i]) && 
								(ration_noz_mask[i] != 0xFF)) {
					RunLog("Log_Noz_State:%02x != ration_noz_mask:%02x", lpDispenser_data->fp_data[i].Log_Noz_State, ration_noz_mask[i]);
					break;
				}

				ration_flag[i] = 0;
				ret = DoRation(i);
				if (ret == 0) {
					ChangeFP_State(node, i + 0x21, FP_State_Started, NULL);	
				} else if (ret == 1) {
					ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);	
				}
			
				// 预置处理完成, 回复允许全部油枪加油
				lpDispenser_data->fp_data[i].Log_Noz_Mask = 0xFF;
				break;
			}

			break;
		case PUMP_AUTH:
			// 不管是PAK还是预置, 均是抬枪后授权, 所以PUMP_AUTH状态时一定是FP_Started
			if ((__u8)FP_State_Started > lpDispenser_data->fp_data[i].FP_State) {
				ret = ChangeFP_State(node, i + 0x21, FP_State_Started, NULL);
			}
		case PUMP_BUSY:					// 加油
			// 补充处理
			if (fp_state[i] < 2) {
				DoStarted(i);
			}

			// 补充处理
			if ((__u8)FP_State_Started > lpDispenser_data->fp_data[i].FP_State) {
				ret = ChangeFP_State(node, i + 0x21, FP_State_Started, NULL);
			}

			//DO_INTERVAL(DO_BUSY_INTERVAL,  ret = DoBusy(i););
			DoBusy(i);

			if ((__u8)FP_State_Fuelling > lpDispenser_data->fp_data[i].FP_State) {
				if (fp_state[i] == 3) {
					ChangeFP_State(node, i + 0x21, FP_State_Fuelling, NULL);
				}
			}

			break;
		case PUMP_STOP:					// 暂停加油 
			if ((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State) {
				ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
			}
			break;
		case PUMP_PEOT:
		case PUMP_FEOT:					// 加油结束
			// 1. ChangeFP_State to IDLE first
			tmp_lnz = lpDispenser_data->fp_data[i].Log_Noz_State;
			lpDispenser_data->fp_data[i].Log_Noz_State = 0x00;
			if ((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State) {
				ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
			}
			// Step2. Get & Save TR to TR_BUFFER
			lpDispenser_data->fp_data[i].Log_Noz_State = tmp_lnz;
			ret = DoEot(i);
			lpDispenser_data->fp_data[i].Log_Noz_State = 0x00;
			fp_state[i] = 0;
			break;
		default:
			break;
		}
		
	}
}

/*  
 *  Note.
 * 因调用对油机的读写操作函数之前已进行过状态判断和错误预处理,
 * 所以在具体的命令执行函数中就无需再做状态检查了
 * 因为有的油机模块在每次下发命令之前都做状态判断的操作, 特此说明
 *
 */

/*
 * func: Open_FP
 */
static int DoOpen(int pos, __u8 *msgbuffer) {
	int ret;
	unsigned int i;

	ret = SetHbStatus(node, 0);   // IFSF_HEARTBEAT.status set to 0
	if (ret<0) {
		RunLog("节点[%d] 心跳状态改变错误",node);
		SendCmdAck(NACK, msgbuffer);
		return -1;
	}

	if (pos == 255) {
		SendCmdAck(ACK, msgbuffer);
		for (i = 0; i < pump.panelCnt; i++) {
			ret = ChangeFP_State(node, 0x21 + i, FP_State_Idle, NULL);
		}
	} else {
		ret = ChangeFP_State(node, pos + 0x21, FP_State_Idle, msgbuffer);
	}

	return ret;
}

/*
 * func: Close_FP
 */
static int DoClose(int pos, __u8 *msgbuffer) 
{
	int i, ret;

	if (pos == 255) {
		SendCmdAck(ACK, msgbuffer);
		for (i = 0; i < pump.panelCnt; i++) {			
			ret = ChangeFP_State(node, 0x21 + i, FP_State_Closed, NULL);
		}
	} else {
		ret = ChangeFP_State(node, pos + 0x21, FP_State_Closed, msgbuffer);
	}
	
	return ret;
}

static int DoStarted(int i)
{
	int ret;

	if ((lpDispenser_data->fp_data[i].Log_Noz_State == 0) ) {
		ret = GetCurrentLNZ(i);
		if (ret < 0) {
			return -1;
		}
	}

	ret = BeginTr4Fp(node, i + 0x21);
	if (ret < 0) {
		ret = BeginTr4Fp(node, i + 0x21);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d],BeingTr4FP Failed", node, i + 1);
			return -1;
		}
	}

	// GetCurrentLNZ & BeginTr4Fp 成功, 则置位 fp_state[i] 为 2
	fp_state[i] = 2;

	return 0;
}

// Only full mode? cann't be changed.
static int DoMode() {
	return -1;
}

/*
 * func: PreSet/PrePay & Release_FP
 * return: -1 - 失败; 0 - 成功; 1 - 已经挂枪, 预置取消
 */
static int DoRation(int pos)  
{
#define  PRE_VOL  0xF1
#define  PRE_AMO  0xF2
	int ret;
	int trynum;
	__u8 status;
	__u8 volume[3];
	__u8 amount[6];
	__u8 preset_type;
	__u8 lnz_allowed = 0;      // 油机枪号从0开始

	if (lpDispenser_data->fp_data[pos].Log_Noz_Mask == 0) {
	//if (ration_noz_mask[pos] == 0) {
		RunLog("Node[%d]FP[%d]未允许任何油枪加油", node, pos + 1);
		return -1;
	}

	// Step.0 获得允许枪号
	for (ret = 0x01; ret <= 0x80; ret = (ret << 1), lnz_allowed++) {
		if ((lpDispenser_data->fp_data[pos].Log_Noz_Mask & ret) != 0) {
		//if ((ration_noz_mask[pos] & ret) != 0) {
			break;
		}
	}

	// 正常的流程
	if (ration_type[pos] == 0x3D) { // MianDian volume have 3 point, amount have 0 point
		preset_type = PRE_VOL;
		
	amount[0] = 0xe0 | (ration_value[pos][2] & 0x0f);
	amount[1] = 0xe0 | (ration_value[pos][2] >> 4);
	amount[2] = 0xe0 | (ration_value[pos][1] & 0x0f);
	amount[3] = 0xe0 | (ration_value[pos][1] >> 4);
	amount[4] = 0xe0 | (ration_value[pos][0] & 0x0f);
	amount[5] = 0xe0 | (ration_value[pos][0] >> 4);
	RunLog("amount[0-5],PRE_Volume: %02x%02x%02x%02x%02x%02x",amount[0],amount[1],amount[2],amount[3],amount[4],amount[5]);
	} else {
		preset_type = PRE_AMO;
	amount[0] = 0xe0 | (ration_value[pos][2] & 0x0f);
	amount[1] = 0xe0 | (ration_value[pos][2] >> 4);
	amount[2] = 0xe0 | (ration_value[pos][1] & 0x0f);
	amount[3] = 0xe0 | (ration_value[pos][1] >> 4);
	amount[4] = 0xe0 | (ration_value[pos][0] & 0x0f);
	amount[5] = 0xe0 | (ration_value[pos][0] >> 4);
	RunLog("amount[0-5],PRE_Amount: %02x%02x%02x%02x%02x%02x",amount[0],amount[1],amount[2],amount[3],amount[4],amount[5]);
	}


	/*
	 * 下发预置之前判断一下当这么状态是否还是抬枪, 如果已经挂枪那么就取消这笔预置,
 	 * 避免抬枪后预置量未下发就已经挂枪, 导致抬挂枪无法取消预置. Guojie Zhu @ 2009.8.6
	 */
	ret = GetPumpStatus(pos, &status);
	if (ret < 0) {
		return -1;
	} else if (status != PUMP_CALL) {
		RunLog("Node[%d].FP[%d]当前状态不宜预置", node, pos + 1);
		return 1;
	}


	for (trynum = 0; trynum < TRY_TIME; trynum++) {
		// Step.1.1 发送预置命令
		pump.cmd[0] = 0x20 | pump.panelPhy[pos];
		pump.cmdlen = 1;
		pump.wantLen = 1;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d], 写预置加油命令失败", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd)-1;

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);

		if (ret < 0) {
			RunLog("节点[%d]面板[%d], 写预置加油命令成功, 取返回信息失败", node, pos + 1);
			Ssleep(2 * INTERVAL);
			continue;
		} else if (pump.cmd[0] != (0xD0 | pump.panelPhy[pos])) { 
			Ssleep(2000000);            // 2x命令间隔要求为2s
			continue;
		}							
		RunLog("节点[%d]面板[%d] Step.1 写预置加油命令成功", node, pos + 1);
		
		// Step.1.2 发送预置量
		pump.cmd[0] = 0xff;
		pump.cmd[1] = 0xe3;
		pump.cmd[2] = preset_type;
		pump.cmd[3] = 0xf4;      // Price Level 1

		if ((lpDispenser_data->fp_data[pos].Log_Noz_Mask != 0xFF) ||(preset_type == PRE_VOL) ||(lpNode_channel->cntOfFpNoz[pos] == 1)) { 
			// 只有油量预置能够指定到枪
			//(lpNode_channel->cntOfFpNoz[pos] == 1)) { 
			//定金额定量在缅甸试用
			pump.cmd[4] = 0xf6;      // Grade data next
			pump.cmd[5] = 0xe0 | lnz_allowed;
			pump.cmd[6] = 0xf8;
			pump.cmd[7] = amount[0];
			pump.cmd[8] = amount[1];
			pump.cmd[9] = amount[2];
			pump.cmd[10] = amount[3];
			pump.cmd[11] = amount[4];		
			pump.cmd[12] = amount[5];		
			pump.cmd[13] = 0xfb;
			pump.cmd[14] = 0xe0 | (LRC(pump.cmd, 14) & 0x0f);
			pump.cmd[15] = 0xf0;
			pump.cmdlen = 16;
			pump.wantLen = 0;
		} else { //0xFF 代表上位机下发预置命令时没有指定油枪, add by liys @ 2009.02.06
			pump.cmd[4] = 0xf8;
			pump.cmd[5] = amount[0];
			pump.cmd[6] = amount[1];
			pump.cmd[7] = amount[2];
			pump.cmd[8] = amount[3];
			pump.cmd[9] = amount[4];		
			pump.cmd[10] = amount[5];		
			pump.cmd[11] = 0xfb;
			pump.cmd[12] = 0xe0 | (LRC(pump.cmd, 12) & 0x0f);
			pump.cmd[13] = 0xf0;
			pump.cmdlen = 14;
			pump.wantLen = 0;		   	
		}	


		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d], 写预置加油量失败", node, pos + 1);
			// WriteToPort 失败已经延时2秒, 所以不再需要设置延时再continue
			continue;
		}
              
		RunLog("节点[%d]面板[%d] Step.2 写预置加油量成功[%s]",
			node, pos + 1, Asc2Hex(pump.cmd, pump.cmdlen));
		
		// Step.2 授权加油
		//根据厂商要求，增加延时，以便加油机来得及对预置命令做处理, Added by liys @2009.02.06
		Ssleep(100000);  // 100ms
		ret = DoAuth(pos);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]预置加油授权失败!", node, pos + 1);
			continue;				
		}			
			  
		return 0; 
	}


	return -1;
}

/*
 * func: 通过下发预置量0000.00的方式取消定量
 */
static int CancelRation(int pos)
{
	int ret;
	int trynum;

	RunLog("Node[%d].FP[%d] Do CancelRation", node, pos + 1);

	for (trynum = 0; trynum < TRY_TIME; trynum++) {
		// Step.1.1 发送预置命令
		pump.cmd[0] = 0x20 | pump.panelPhy[pos];
		pump.cmdlen = 1;
		pump.wantLen = 1;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]发取消预置加油命令失败", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd)-1;

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);

		if  (pump.cmd[0] != (0xD0 | pump.panelPhy[pos])) { 
			RunLog("节点[%d]面板[%d]写取消预置加油命令成功, 取返回信息失败", node, pos + 1);
			sleep(2);
			continue;
		}							
		RunLog("节点[%d]面板[%d] Step.1.1 写取消预置加油命令成功", node, pos + 1);
		
		// Step.1.2 发送预置量
		pump.cmd[0] = 0xff;
		pump.cmd[1] = 0xe3;
		pump.cmd[2] = 0xf2;
		pump.cmd[3] = 0xf4;      // Price Level 1
		pump.cmd[4] = 0xf8;
		pump.cmd[5] = 0xe0;
		pump.cmd[6] = 0xe0;
		pump.cmd[7] = 0xe0;
		pump.cmd[8] = 0xe0;
		pump.cmd[9] = 0xe0;
		pump.cmd[10] = 0xe0;
		pump.cmd[11] = 0xfb;
		pump.cmd[12] = 0xe0 | (LRC(pump.cmd, 12) & 0x0f);
		pump.cmd[13] = 0xf0;
		pump.cmdlen = 14;
		pump.wantLen = 0;		   	

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("发取消预置量到节点[%d]面板[%d]失败", node, pos + 1);
			continue;
		}
              
		RunLog("节点[%d]面板[%d]写取消预置加油量成功[%s]",
			node, pos + 1, Asc2Hex(pump.cmd, pump.cmdlen));
		
		return 0; 
	}

	return -1;
}

/*
 * func: 接收油品单价, 存入newPrice[][], 在IDLE状态执行变价
 */
static int DoPrice()
{
	__u8 pr_no;
	__u8 times_print;
	static __u8 times = 0;
	
	pr_no  = pump.cmd[2] - 0x41; //油品地址到油品编号0,1,...7.
	if ((0x04 != pump.cmd[8] && 0x06 != pump.cmd[8]) || pr_no > 7) {
		RunLog("变价信息错误,不能下发, pump.cmd[8] = %d, pr_no = %d", pump.cmd[8], pr_no);
		return -1;
	}
	
	if ((pump.newPrice[pr_no][1] | pump.newPrice[pr_no][2] | pump.newPrice[pr_no][3]) == 0) {
		// 上次执行变价(ChangePrice)后第一次收到WriteFM发来的变价命令
		//pump.newPrice[pr_no][0] = 1;
		times_print = 1;
	} else {
		//pump.newPrice[pr_no][0]++;
		++times;
		times_print = times;
	}

	// 不管是否是连续收到同种油品的变价命令, 也不管单价是否一致, 均使用新单价覆盖老单价
	pump.newPrice[pr_no][0] = pump.cmd[8]; // add by weihp, for MianDian
	pump.newPrice[pr_no][1] = pump.cmd[9];
	pump.newPrice[pr_no][2] = pump.cmd[10];
	pump.newPrice[pr_no][3] = pump.cmd[11];
#ifdef DEBUG
	RunLog("Recv New Price-PR_ID[%d]TIMES[%d] %02x%02x%02x",
		pr_no + 1, times_print, pump.newPrice[pr_no][1], pump.newPrice[pr_no][2], pump.newPrice[pr_no][3]);
#endif

	return 0;
}


/*
 * func: 执行变价, 失败改变FP状态为INOPERATIVE, 并写错误数据库
 */
static int ChangePrice() 
{
	int i, j;
	int ret, fm_idx, trynum;
	__u8 price[4] = {0};
	__u8 tmp_price[4] = {0};
	__u8 pr_no;
	__u8 lnz;
	__u8 err_code;
	__u8 err_code_tmp;
	__u8 status;

/*
 * 遍历新单价数组newPrice[FP_PR_NB_CNT], 如果有油品需要变价--数组值不为零,
 * 则遍历所有FP, 如果油品匹配就下发变价命令
 */

//	RunLog(">> exec ChangePrice Now !");
	for (pr_no = 0; pr_no < FP_PR_NB_CNT; pr_no++) {
//		RunLog(">>>1 newPrice[%d]: %02x.%02x", pr_no, pump.newPrice[pr_no][2], pump.newPrice[pr_no][3]);;
		if (pump.newPrice[pr_no][1] != 0x00 || pump.newPrice[pr_no][2] != 0x00 || pump.newPrice[pr_no][3] != 0x00) {
			for (i = 0; i < pump.panelCnt; i++) {
				//1.##ChangePrice 
				err_code = 0x99;
				for (j = 0; j < MAX_FP_LN_CNT; j++) { // 不能用gunCnt,1枪可能不用
#if DEBUG
					RunLog(">>> fp_ln_data[%d][%d].PR_Id: %d", i, j,
							lpDispenser_data->fp_ln_data[i][j].PR_Id);
#endif
					if (lpDispenser_data->fp_ln_data[i][j].PR_Id - 1 == pr_no) {
						//2.##ChangePrice 
						err_code_tmp = 0x99;
				CriticalLog("下发变价命令给节点[%02d]面[%d]枪[%d]......", node, i + 1, j + 1);

					//price is little endian mode, so 1 to 0
					if (0x04 == pump.newPrice[pr_no][0]) {
						price[0] = 0xe0 | (pump.newPrice[pr_no][2] & 0x0f);
						price[1] = 0xe0 | (pump.newPrice[pr_no][2] >> 4);
						price[2] = 0xe0 | (pump.newPrice[pr_no][1] & 0x0f);
						price[3] = 0xe0 | (pump.newPrice[pr_no][1] >> 4);
					} else if (0x06 == pump.newPrice[pr_no][0]) {
						price[0] = 0xe0 | (pump.newPrice[pr_no][3] & 0x0f);
						price[1] = 0xe0 | (pump.newPrice[pr_no][3] >> 4);
						price[2] = 0xe0 | (pump.newPrice[pr_no][2] & 0x0f);
						price[3] = 0xe0 | (pump.newPrice[pr_no][2] >> 4);
					}

					for (trynum = 0; trynum < TRY_TIME; trynum++) {
                                            // Step.1.1 发变价命令;
						Ssleep(INTERVAL);
						pump.cmd[0] = 0x20 | pump.panelPhy[i];
						pump.cmdlen = 1;
						pump.wantLen = 1;

						ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd, 
								pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
						if (ret < 0) {
							RunLog("节点[%d]面板[%d], 下发变价命令失败",node ,i+1);
							continue;
						}

						memset(pump.cmd, 0, sizeof(pump.cmd));
						pump.cmdlen = sizeof(pump.cmd) - 1;
						ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);

						if (pump.cmd[0] != (0xD0 | pump.panelPhy[i])) {
							RunLog("节点[%d]面板[%d]取回复信息失败",node,i+1);
							continue;
						}
						
						// Setp.1.2 发单价数据
						pump.cmd[0] = 0xff;
						pump.cmd[1] = 0xe5;
						pump.cmd[2] = 0xf4;
						pump.cmd[3] = 0xf6;
						pump.cmd[4] = 0xe0 | j; // j == lnz - 1;
						pump.cmd[5] = 0xf7;
						pump.cmd[6] = price[0];
						pump.cmd[7] = price[1];
						pump.cmd[8] = price[2];
						pump.cmd[9] = price[3];
						pump.cmd[10] = 0xfb;
						pump.cmd[11] = 0xe0 | (LRC(pump.cmd, 11) & 0x0f);
						pump.cmd[12] = 0xf0;
						pump.cmdlen = 13;
						pump.wantLen = 0;

						Ssleep(INTERVAL*5);
						ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
								pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
						if (ret < 0) {
							RunLog("节点[%d]面板[%d]下发变价命令失败",node,i+1);
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL * 6);
							continue;
						}


						RunLog("Node[%d].FP[%d].PriceChange_CMD: %s",
							node, i + 1, Asc2Hex(pump.cmd, pump.cmdlen));
										  
						// Step.2 根据命令返回信息或者重读油机单价确认变价是否成功
						
						// 避免税控忙导致处理延时, 所以延时1s再读单价
						sleep(1);

						ret = GetGunPrice(i, j + 1, tmp_price);
						if (ret < 0) {
							/* 如果GetGunPrice失败,
							 * 那么超时时间一定多余2s, 所以直接 continue;
							 */
							continue;
						}
					      
						if (memcmp(tmp_price, price, 4) != 0) {
							RunLog("节点[%d]面板[%d]下发变价命令成功,执行失败", node, i + 1);
							err_code_tmp = 0x37; // 变价执行失败
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - 1000000);
							continue;
						}
						 
						// Step.3  清零单价缓存
						
						fm_idx = lpDispenser_data->fp_data[i].Default_Fuelling_Mode - 1;

					RunLog("节点[%d]面板[%d]发送变价命令成功,新单价fp_fm_data[%d][%d].Prode_Price: %s",
							node, i + 1, pr_no, fm_idx,
							Asc2Hex(lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price, 4));

					CriticalLog("节点[%02d]面[%d]枪[%d]变价成功,新单价为%02x.%02x", node, i + 1, j + 1,
							lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2],
							lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3]);

						err_code_tmp = 0x99;
						break;
					}

					// Step.4 错误判断, 若出错则写错误数据库/置FP状态为 Inoperative
					if (trynum >= TRY_TIME) {
						// Write Error Code: 0x36/0x37 - Price Change Error (Spare)
						//5.##ChangePrice 
						if (0x37 != err_code_tmp) {
							err_code_tmp = 0x36;    // 通讯失败
				CriticalLog("节点[%02d]面[%d]枪[%d]变价失败[通讯问题],改变面状态为[不可用]",
											node, i + 1, j + 1);
						} else {
				CriticalLog("节点[%02d]面[%d]枪[%d]变价失败[油机执行失败],改变面状态为[不可用]",
											node, i + 1, j + 1);
						}

						PriceChangeErrLog("%02x.%02x.%02x %02x.%02x %02x",
							       	node, i + 1, j + 1,
							       	pump.newPrice[pr_no][2], 
							       	pump.newPrice[pr_no][3], err_code);
					}
					//6.##ChangePrice 
					if (err_code == 0x99) {
						// 该面有任何油枪变价失败, 则算整个FP变价失败.
						err_code = err_code_tmp;
					}
					
					} // end of if.PR_Id match
				//	break;  // 如果加油机能够根据指令信息处理多枪同油品的情况，那这里需要break.
				} // end of for.j < LN_CNT
				//7.##ChangePrice 
				RunLog("---------- Node[%d]FP[%d] ER_DATA, error code: %02x -----", node, i + 1, err_code);

				int tmp_idx;
				for (tmp_idx = 0; tmp_idx < pump.newPrice[pr_no][0]; tmp_idx++) {
					ChangeER_DATA(node, 0x21 + i, err_code, NULL); // Translater已经回复了ACK
				}

				if (err_code != 0x99) {
					ChangeFP_State(node, 0x21 + i, FP_State_Inoperative, NULL);
				}

				// 保证 2x 命令的间隔达到PRICE_CHANGE_RETRY_INTERVAL(2s) (读单价前 1s + 这里 1s)
				Ssleep(PRICE_CHANGE_RETRY_INTERVAL - 1000000);
			} // end of for.i < panelCnt
			bzero(&pump.newPrice[pr_no], 4);  // 该油品变价结束
		} // end of if.newPrice
	} // end of for.pr_no

	return 0;
}


/*
 * func: 加油结束，进行交易记录的读取和处理
 *       零交易的判断和处理由DoEot来做, DoPoling 不关心
 * ret : 0 - 成功; -1 - 失败
 *
 */
/*
 * 修改建议&实现说明:  需要从油机读取的数据项有 当次加油量 & 加油金额 &
 *                     单价 & 油量总累 [& 金额总累] , 分别更新相应的数据库项
 *                     
 * mdf to America Gilbarco
 */
static int DoEot(int pos)
{
	int i;
	int ret,lnz;
	int pr_no;
	int err_cnt;
	__u8 price[4] = {0};
	__u8 volume[5] = {0};
	__u8 amount[5] = {0};
	__u8 total[7] = {0};
	__u8 vol_total[GUN_EXTEND_LEN] = {0};
	__u8 amount_total[GUN_EXTEND_LEN] = {0};
	__u8 pre_total[7] = {0};

	RunLog("节点[%d]面板[%d]处理新的交易", node, pos + 1);

	pump.addInf[pos][0] = 0;


	err_cnt = 0;
	do {
		ret = GetGunTotal(pos, pump.gunLog[pos], vol_total, amount_total);	// 取油枪泵码
		if (ret < 0) {
			RunLog("节点[%d]面板[%d] 取加油机泵码失败", node, pos + 1);
			err_cnt++;
		}
	} while (ret < 0 && err_cnt <=5);
	
	if (ret < 0) {
		return -1;
	}
#if 0
	if(point_flag == 1){ //需要去百分位
		ret = GetPumpPercentile(pos);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]读取百分位失败", node,i+1);
			return -1;
		}
	}
#endif
	pr_no = (lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).PR_Id - 1;  // 0 - 7	
	if ((pr_no < 0) || (pr_no > 7)) {
		RunLog("DoEot PR_Id error, PR_Id[%d] <ERR>", pr_no + 1);
	}
	
	memcpy(pre_total, (lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Vol_Total, 7);

	lnz = pump.gunLog[pos] - 1;
	/*Beijing长吉泵码分1位小数和2位小数两种, America Gilbarco no need this*/
	//if(point_flag == 0){
	if (1) {
		total[0] = 0x0A;
		total[1] = 0x00;
		total[2] = 0x00;
		total[3] = (vol_total[7] << 4) | (vol_total[6] & 0x0F); // modfy by weihp volum total point << 1
		total[4] = (vol_total[5] << 4) | (vol_total[4] & 0x0F);
		total[5] = (vol_total[3] << 4) | (vol_total[2] & 0x0F);
		total[6] = (vol_total[1] << 4) | (vol_total[0] & 0x0F);
		RunLog("DoEot volume total : %02x%02x%02x%02x%02x%02x%02x", total[0],total[1],total[2],total[3],total[4],total[5],total[6]);
	}else if(point_flag == 1){
		total[0] = 0x0A;
		total[1] = 0x00;
		total[2] = vol_total[7] & 0x0f;
		total[3] = (vol_total[6] << 4) | (vol_total[5] & 0x0F);
		total[4] = (vol_total[4] << 4) | (vol_total[3] & 0x0F);
		total[5] = (vol_total[2] << 4) | (vol_total[1] & 0x0F);
		
		switch(lnz){
			case 0:
				total[6] = (vol_total[0] << 4)|(GilPercentile[pos].lnzOne& 0x0F);
				break;
			case 1:
				total[6] = (vol_total[0] << 4)|(GilPercentile[pos].lnzTwo& 0x0F);
				break;
			case 2:
				total[6] = (vol_total[0] << 4)|(GilPercentile[pos].lnzThr& 0x0F);
				break;
			case 3:
				total[6] = (vol_total[0] << 4)|(GilPercentile[pos].lnzFou& 0x0F);
				break;
			default:
				break;
		}

	}else{
		total[0] = 0x0A;
		total[1] = 0x00;
		total[2] = 0x00;
		total[3] = (vol_total[7] << 4) | (vol_total[6] & 0x0F);
		total[4] = (vol_total[5] << 4) | (vol_total[4] & 0x0F);
		total[5] = (vol_total[3] << 4) | (vol_total[2] & 0x0F);
		total[6] = (vol_total[1] << 4) | (vol_total[0] & 0x0F);
	}


	if (memcmp(total, pre_total, sizeof(total)) != 0) {
		err_cnt = 0;
		do {
			ret = GetTransData(pos);	// 取最后一笔加油记录
			if (ret < 0) {
				RunLog("节点[%d]面板[%d]取交易记录失败",node,pos+1);
				err_cnt++;
			}
		} while (ret < 0 && err_cnt <= 5);

		if (ret < 0) {
			return -1;
		}

#if 0 // change volum[0]
		volume[0] = 0x05;
		volume[1] = 0x00;
		volume[2] = (pump.cmd[22] << 4) | (pump.cmd[21] & 0x0f);
		volume[3] = (pump.cmd[20] << 4) | (pump.cmd[19] & 0x0f);
		volume[4] = (pump.cmd[18] << 4) | (pump.cmd[17] & 0x0f);
		memcpy(lpDispenser_data->fp_data[pos].Current_Volume, volume, 5);

		amount[0] = 0x07;
		amount[1] = 0x00;
		amount[2] = (pump.cmd[29] << 4) | (pump.cmd[28] & 0x0f);
		amount[3] = (pump.cmd[27] << 4) | (pump.cmd[26] & 0x0f);
		amount[4] = (pump.cmd[25] << 4) | (pump.cmd[24] & 0x0f);
		memcpy(lpDispenser_data->fp_data[pos].Current_Amount, amount, 5);
#else //change volume 1-4
		volume[0] = 0x06;
		volume[1] = 0x00;
		volume[2] = (pump.cmd[22] & 0x0f);
		volume[3] = (pump.cmd[21] << 4) | (pump.cmd[20] & 0x0f);
		volume[4] = (pump.cmd[19] << 4) | (pump.cmd[18] & 0x0f);
		memcpy(lpDispenser_data->fp_data[pos].Current_Volume, volume, 5);
		RunLog("GetTrans Volume : %02x%02x%02x%02x%02x", volume[0],volume[1],volume[2],volume[3],volume[4]);

		amount[0] = 0x06;
		amount[1] = (pump.cmd[29] << 4) | (pump.cmd[28] & 0x0f);
		amount[2] = (pump.cmd[27] << 4) | (pump.cmd[26] & 0x0f);
		amount[3] = (pump.cmd[25] << 4) | (pump.cmd[24] & 0x0f);
		amount[4] = 0x00;
		memcpy(lpDispenser_data->fp_data[pos].Current_Amount, amount, 5);
		RunLog("GetTrans Amount : %02x%02x%02x%02x%02x", amount[0],amount[1],amount[2],amount[3],amount[4]);

#endif

		price[0] = 0x04;
		price[1] = (pump.cmd[15] << 4) | (pump.cmd[14] & 0x0f);
		price[2] = (pump.cmd[13] << 4) | (pump.cmd[12] & 0x0f);
		price[3] = 0x00;
		memcpy(lpDispenser_data->fp_data[pos].Current_Unit_Price, price, 4);
		
		memcpy(lpDispenser_data->fp_m_data[pr_no].Meter_Total, total, 7);
		memcpy((lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Vol_Total, total, 7);


#if 0
              RunLog("节点[%02d]面板[%d]枪[%d] ## Amount: %02x%02x.%02x Volume: %02x%02x.%02x Price: %02x.%02x Total: %02x%02x%02x%02x.%02x", node, pos + 1, pump.gunLog[pos], amount[2], 
					amount[3], amount[4], volume[2], volume[3], volume[4], price[2],
						price[3], total[2], total[3], total[4], total[5], total[6]);
#endif
		
		// Update Amount_Total
		total[0] = 0x0a; 
		total[1] = 0x00;
		total[2] = 0x00;
		total[3] = (amount_total[7] & 0x0f); //modefy by weihp , point << 1
		total[4] = (amount_total[6] << 4) | (amount_total[5] & 0x0f);
		total[5] = (amount_total[4] << 4) | (amount_total[3] & 0x0f);
		total[6] = (amount_total[2] << 4) | (amount_total[1] & 0x0f);
		RunLog("DoEot amount total : %02x%02x%02x%02x%02x%02x%02x", total[0],total[1],total[2],total[3],total[4],total[5],total[6]);

		memcpy((lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Amount_Total, total, 7);
		
		memcpy(lpDispenser_data->fp_data[pos].Current_Prod_Nb, lpDispenser_data->fp_pr_data[pr_no].Prod_Nb, 4);

		err_cnt = 0;
		do {
			ret = EndTr4Ln(node, pos + 0x21, pump.gunLog[pos] + 0x10);
			if (ret < 0) {
				RunLog("节点[%d]面板[%d]DoEot.EndTr4Ln 失败, ret: %d <ERR>", node,pos +1 ,ret);
				err_cnt++;
			}
		} while (ret < 0 && err_cnt <= 5);

		err_cnt = 0;
		do {
			ret = EndTr4Fp(node, pos + 0x21, pre_total,
				(lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Vol_Total);
			if (ret < 0) {
				RunLog("节点[%d]面板[%d] DoEot.EndTr4Fp 失败, ret: %d <ERR>", node,pos + 1,ret);
				err_cnt++;
			}
		} while (ret < 0 && err_cnt <= 8);

	} else {
		ret = EndZeroTr4Fp(node, pos + 0x21);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]DoEot.EndZeroTr4Fp 失败, ret: %d <ERR>", node,pos+1,ret);
		}
	}

	
	return ret;
}


/*
 * func: 获取该FP所有枪的泵码
 */
static int GetPumpTotal(int pos)
{
	int ret;
	int trynum;
	__u8 lrc;

	for (trynum = 0; trynum <TRY_TIME ; trynum++) {
		Ssleep(INTERVAL);
		pump.cmd[0] = 0x50 | pump.panelPhy[pos];
		pump.cmdlen = 1;
		pump.wantLen = pump.gunCnt[pos] * 30 + 4;
		
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]下发取泵码命令失败",node,pos +1);
			gpGunShm->addInf[pump.chnLog - 1][0] = 0x00;
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		//变长特征.如果该变量不为0x00,则tty程序在取得足够的长度时,需要用该特征码判断是否还有后续数据
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x01;
		//判断特征码的程序在spec.c中
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x00;	//取完数据后, 复位标记
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]下发取泵码命令成功, 取返回信息失败",node,pos +1);
			continue;
		}

		if (pump.cmd[pump.cmdlen - 1] != 0xf0) {
			RunLog("节点[%d]面板[%d] 接收数据不完整",node,pos +1);              //继续接收
			continue;
		}

		//lrc校验失败//
		lrc = LRC(pump.cmd, pump.cmdlen - 2);
		if ((lrc & 0x0f) != (pump.cmd[pump.cmdlen - 2] & 0x0f)) {
			RunLog("节点[%d]面板[%d] LRC校验失败",node,pos + 1);
			continue;
		}

		return 0;
	}

	return -1;
}

/*
 * func: 取某条枪的泵码
 */ 
static int GetGunTotal(int pos, __u8 gunLog, __u8 *vol_total, __u8 *amount_total)
{
	int ret ;
	int i;
	int  num;
	__u8 *p;

	ret = GetPumpTotal(pos);
	if (ret < 0) {
		RunLog("节点[%d]面板[%d] 取枪的泵码失败", node,pos + 1);
		return -1;
	}

	num = (pump.cmdlen - 4) / 30;	// 每条记录30字节,4个字节为其他包头,包尾
	p = pump.cmd + 1;

	for (i = 0; i < num; i++) {
		if ((p[1] & 0x0F) == gunLog - 1) {
			memcpy(vol_total, p + 3, GUN_EXTEND_LEN);
			memcpy(amount_total, p + 12, GUN_EXTEND_LEN);
			break;
		}
		p = p + 30;
	}

	return 0;
}


/*
 * func: 取某条枪的泵码
 */ 
static int GetGunPrice(int pos, __u8 gunLog, __u8 *price)
{
	int ret ;
	int i;
	int  num;
	__u8 *p;

	ret = GetPumpTotal(pos);
	if (ret < 0) {
		RunLog("节点[%d]面板[%d] 取枪的油品单价失败", node,pos + 1);
		return -1;
	}

	num = (pump.cmdlen - 4) / 30;	// 每条记录30字节,4个字节为其他包头,包尾
	p = pump.cmd + 1;

	for (i = 0; i < num; i++) {
		if ((p[1] & 0x0F) == gunLog - 1) {
			memcpy(price, p + 21, 4);
			break;
		}
		p = p + 30;
	}

	return 0;
}

/*
 * func: 读取当次交易记录
 */ 
static int GetTransData(int pos)
{
	int ret;
	int fp_pos;
	int trynum;
	__u8 lrc;

	for (trynum = 0; trynum < TRY_TIME; ++trynum) { 
		pump.cmd[0] = 0x40 | pump.panelPhy[pos];
		pump.cmdlen = 1;
		pump.wantLen = 33;

		Ssleep(INTERVAL);	/*停10毫秒,防止写入太频繁*/
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]下发读交易命令失败",node ,pos +1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]下发读交易命令成功, 取返回信息失败",node ,pos +1);
			continue;
		}

		if (pump.cmd[pump.cmdlen - 1] != 0xf0) {
			RunLog("节点[%d]面板[%d] 确认信息失败",node,pos+1);
			continue;
		}

		/*lrc校验失败*/
		lrc = LRC(pump.cmd, pump.cmdlen - 2);
		if ((lrc&0x0f) != (pump.cmd[pump.cmdlen - 2] & 0x0f)) {
			RunLog("节点[%d]面板[%d],LRC校验失败", node, pos +1);
			continue;
		}


		fp_pos = pump.cmd[4] & 0x0f;	/*其他各命令都是16号面板为0,其他为对应值.但这里16是f,1是0...*/
		if (fp_pos == 0x0f) {
			fp_pos = 0x00;
		} else {
			fp_pos += 1;
		}

		if (fp_pos == pump.panelPhy[pos]) {
			pump.gunLog[pos] = (pump.cmd[9] & 0x0F) + 1;
			return 0;
		}
	}

	return -1;
}

/*
 * func: 暂停加油，在此做Terminate_FP用
 */
static int DoStop(int pos)
{
	int ret;
	int trynum;
	__u8 status;

	RunLog("Node[%d].FP[%d] DoStop", node, pos + 1);

	for (trynum = 0; trynum < TRY_TIME; ++trynum) {
		if (pos == 255) {
			pump.cmd[0] = 0xfc;
		} else {
			pump.cmd[0] = 0x30 | pump.panelPhy[pos];
		}
		pump.cmdlen = 1;
		pump.wantLen = 0;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]下发停止加油命令失败", node, pos + 1);
			continue;
		}

		Ssleep(200000);  // 200ms, 延迟检查
		ret = GetPumpStatus(pos, &status);
		if (ret < 0) {
			continue;
		}

		// PUMP_CALL 状态下执行DoStop状态不变
		// PUMP_AUTH 状态下执行 DoStop 会变为 PUMP_OFF
		// PUMP_BUSY 状态下执行DoSTop会变为 PUMP_STOP
		if (status == PUMP_STOP || status == PUMP_OFF || status == PUMP_CALL) {
			return 0;
		}
	}

	return -1;
}

/*
 * func: Release_FP
 */
static int DoAuth(int pos)
{
	int i;
	int maxCnt;
	int trynum;
	int ret;
	__u8 status;

	if (pos == 255)	{
		i = 0;
		maxCnt = pump.panelCnt;
	} else {
		i = pos;
		maxCnt = pos + 1;
	}

	for (; i < maxCnt; i++) {
		RunLog("Node %d FP %d DoAuth", node, i + 1);
		for (trynum = 0; trynum < TRY_TIME; trynum++) {
			pump.cmd[0] = 0x10 | pump.panelPhy[i];
			pump.cmdlen = 1;
			pump.wantLen = 0;

			Ssleep(INTERVAL);
			ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
			if (ret < 0) {
				RunLog("节点[%d]面板[%d]下发授权命令失败",node,pos +1);
				continue;
			}

			Ssleep(60000);   // 发命令到轮询状态之间至少需要间隔68ms
			ret = GetPumpStatus(i, &status);
			if (ret < 0) {
				continue;
			}
			if (status != PUMP_BUSY && status != PUMP_AUTH) {
				continue;
			}

#if 0
			if (status == PUMP_BUSY) {
				if (fp_state[i] < 2) {
					DoStarted(i);
				}
			}
#endif
			break;
		}

		if (trynum == TRY_TIME) {
			return -1;
		}
	}

	return 0;
}

/*
 * func: 读取并上传实时加油数据
 */
static int DoBusy(int pos)
{
	int i, j, freq;
	int ret;
	int trynum;
	int pr_no;
	__u8 tmp1[5];
	__u8 tmp2[5];
	__u8 status;
	__u8  ifsf_amount[5],ifsf_volume[5];
	time_t time_tmp;
	static time_t running_msg_time[MAX_FP_CNT] = {0};
	const __u8 arr_zero[3] = {0};
	long price, amount, volume;

	for (trynum = 0; trynum < 2; trynum++) {
		// Step.1 读取实时数据
		pump.cmd[0] = 0x60 | pump.panelPhy[pos];
		pump.cmdlen = 1;
		pump.wantLen = 6;

		if (pump.addInf[pos][0] == 0) {  // 第一次
			Ssleep(INTERVAL * 150);
			pump.addInf[pos][0] = 1;
		} else {
			Ssleep(INTERVAL * 2);
		}

		Ssleep(INTERVAL); 
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]发读实时数据命令失败",node,pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]发读实时数据命令成功, 取返回信息失败",node,pos + 1);
			continue;
		}

		// Step.2  Exec ChangeFP_Run_Trans()
		memset(tmp1, 0, sizeof(tmp1));
		tmp1[0] = (pump.cmd[5] << 4) | (pump.cmd[4] & 0x0f);
		tmp1[1] = (pump.cmd[3] << 4) | (pump.cmd[2] & 0x0f);
		tmp1[2] = (pump.cmd[1] << 4) | (pump.cmd[0] & 0x0f);
		ifsf_amount[0] = 0x06;
		ifsf_amount[1] = tmp1[0]; // MianDian amount not point
		ifsf_amount[2] = tmp1[1];
		ifsf_amount[3] = tmp1[2];
		ifsf_amount[4] = 0x00;
		ifsf_volume[0] = 0x06;
		ifsf_volume[1] = 0x00;

		// 只有当加油量不为零才能将FP状态改变为Fuelling
		if (fp_state[pos] != 3) {
		       if (memcmp(tmp1, arr_zero, 3) == 0) { 
				return 0;
		       }
			fp_state[pos] = 3;
			return 0;
		}

		memset(tmp2, 0, sizeof(tmp2));
		pr_no = (lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).PR_Id - 1;  // 0-7	
		memcpy(tmp2, lpDispenser_data->fp_fm_data[pr_no][lpDispenser_data->fp_data[pos].Default_Fuelling_Mode - 1].Prod_Price + 1, 3);

		amount = 0;
		price = 0;
		for (i = 0; i < 3; i++) {
			amount = amount * 100 + (tmp1[i] >> 4) * 10 + (tmp1[i] & 0x0f);
			price = price * 100 + (tmp2[i] >> 4) * 10 + (tmp2[i] & 0x0f);
		}
		if (price == 0) {
			UserLog("节点[%d]面板[%d] DoBusy.Price is 0 error, Default FM: %d", node, pos + 1, 
							lpDispenser_data->fp_data[pos].Default_Fuelling_Mode);
			return -1;
		}
		//volume = amount * 100 / price; //都有两位小数,做除法注意100
		volume = amount *10000  / price; //缅甸不用乘100
		if (0 == volume) {
			UserLog("节点[%d]面板[%d], Volume is zero",node,pos + 1);
		}
#if 0
		RunLog("节点[%d]面板[%d] ## Amount: %ld, Volume: %d, Price: %ld",node,pos + 1, amount, volume, price);
#endif

		memset(tmp2, 0, sizeof(tmp2));
		for (i= 3; i >= 0; i--) {
			tmp2[i] = (volume % 10) | (((volume / 10)  % 10) << 4);
			volume /= 100;
		}
		if (volume != 0) {
			UserLog("节点[%d]面板[%d], Current_Volume caculate error",node,pos +1);
		}

		memcpy(&(ifsf_volume[1]), tmp2, 4);

		if ((__u8)FP_State_Fuelling == lpDispenser_data->fp_data[pos].FP_State) {
			time_tmp = time(NULL);
			freq =	(((lpDispenser_data->fp_data[pos].Running_Transaction_Message_Frequency[1] >> 4) * 100) +
				((lpDispenser_data->fp_data[pos].Running_Transaction_Message_Frequency[1] & 0x0F) * 10) +
				(lpDispenser_data->fp_data[pos].Running_Transaction_Message_Frequency[0] >> 4));
			if ((time_tmp >= running_msg_time[pos] + freq) && freq != 0) {
				running_msg_time[pos] = time_tmp;

				ret = ChangeFP_Run_Trans(node, 0x21 + pos, ifsf_amount, ifsf_volume);
			}
		}

		return 0;
	}

	return -1;
}

/*
 * func: 抬枪处理 
 * 获取当前抬起的逻辑枪号,设置fp_data[].Log_Noz_State & 改变FP状态为Calling
 */
static int DoCall(int pos)
{
	int ret;
	int trynum;
	__u8 ln_status;

	pump.addInf[pos][0] = 0; 

	ret = GetCurrentLNZ(pos);
	if (ret < 0) {
		RunLog("节点[%d]面板[%d], 获取当前油枪枪号失败",node,pos + 1);
		return -1;
	}

	ret = ChangeFP_State(node, pos + 0x21, FP_State_Calling, NULL);

	//上位机在线,但是允许自动授权;上位机不在线, 授权开关置于开的状态,此时自动授权
	if ((gpIfsf_shm->auth_flag == FCC_SWITCH_AUTH) && (lock_status() == 1)) {
		ret = DoAuth(pos);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]自动授权失败",node,pos +1);
		}
		RunLog("节点[%d]面板[%d]自动授权成功",node ,pos +1);
		SO_OFFLINE_TR(node, pos);
	}

	return ret;
}


/*
 * func: 读取FP状态
 */
static int GetPumpStatus(int pos, __u8 *status)
{
	int i;
	int ret;

	for (i = 0; i < 2; ++i) {         // Note. GetPumpStatus 循环变量最好用本地的,不要用trynum
		pump.cmd[0] = 0x00 | pump.panelPhy[pos];
		pump.cmdlen = 1;
		pump.wantLen = 1;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]发读状态命令失败",node,pos +1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd)-1;

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]发读状态命令成功, 取返回信息失败",node,pos +1);
			continue;
		} 

		if ((pump.cmd[0] & 0x0f) != pump.panelPhy[pos]) {
			RunLog("Node[%d]FP[%d]Message Error.PF_Id not match", node, pos + 1);
			continue;
		}

		*status = pump.cmd[0] >> 4;

		return 0;
	}

	return -1;
}

//test wether the dispenser is online, we only use fp 1 to test.
/*
 * func: 判断Dispenser是否在线
 */
static int GetPumpOnline(int pos, __u8 *status)
{
	int i;
	int ret;

	for (i = 0; i < TRY_TIME; ++i) {
		pump.cmd[0] = 0x00 | pump.panelPhy[pos];
		pump.cmdlen = 1;
		pump.wantLen = 1;

		Ssleep(INTERVAL * 10);   // 100ms
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]发读状态命令失败",node,pos +1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]发读状态命令成功, 取返回信息失败",node,pos +1);
			continue;
		} 

		if ((pump.cmd[0] & 0x0f) != pump.panelPhy[pos]) {
			RunLog("Message Error.PF_Id not match");
			continue;
		}

		*status = pump.cmd[0] >> 4;
		return 0;
	}

	return -1;
}

/*
 * func: 获取当前抬起油枪的枪号, 结果写入pump.gunLog[] & fp_data[].Log_Noz_State
 * ret : 0 - 成功, -1 - 失败 
 * Note: 这个函数必须在油枪抬起的状态下执行,比如PUMP_CALL/PUMP_BUSY/PUMP_STOP,否则无响应
 */
static int GetCurrentLNZ(int pos)
{
	int ret;
	int trynum;
	__u8 ln_status;
	__u8 sfc_cmd[] = { 0xff, 0xe9, 0xfe, 0xe0, 0xe1, 0xe0, 0xfb, 0xee, 0xf0 };

	for(trynum = 0; trynum < TRY_TIME; trynum++) {
		pump.cmd[0] = 0x20 | pump.panelPhy[pos];
		pump.cmdlen = 1;
		pump.wantLen = 1;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d] 向串口写数据失败",node,pos +1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d] 取返回信息失败",node,pos + 1);
			continue;
		}

		if (pump.cmd[0] != (0xd0 | pump.panelPhy[pos])) {
			RunLog("节点[%d]面板[%d] 信息确认失败",node,pos +1);
			Ssleep(2000000);   // 间隔2s
			continue;
		}

		memcpy(pump.cmd, sfc_cmd, 9);
		pump.cmdlen = 9;
		pump.wantLen = 19;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT) ;
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]发取枪号命令失败",node,pos +1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]发取枪号命令成功, 取返回信息失败",node,pos +1);
			continue;
		}
		
		//                        ID                  Msgs:  In/Out  No  Cksum CR LF
		// 1枪抬起 ret: ba b0 b3  B1  b0 b1 b0 b0 b0  b0 b1 b0 B1 b1 B1  c3 c1 8d 8a
		// 无枪抬起ret: ba b0 b3  B1  b0 b1 b0 b0 b0  b0 b1 b0 B0 b1 B0  c3 c3 8d 8a
				
		if (pump.cmdlen != 19) {
			RunLog("节点[%d]面板[%d]返回信息有误",node,pos + 1);
			continue;
		}

		point_flag = pump.cmd[9] & 0x0f;

		if(point_flag < 0 || point_flag >2){
			RunLog("节点[%d]小数点位读取错误point_flag[%d]",node, point_flag);
			point_flag = 0;
		}

		switch(point_flag){
			case 0:
				RunLog("节点[%d]point_flag[%d] 一位泵码小数位",node, point_flag);
				break;
			case 1:
				RunLog("节点[%d]point_flag[%d] 一位泵码小数位,010命令取百分位"
					,node, point_flag);
				break;
			case 2:
				RunLog("节点[%d]point_flag[%d] 两位泵码小数位",node, point_flag);
				break;
			default:
				RunLog("节点[%d]point_flag[%d] 错误!",node, point_flag);
				break;
		}

		if ( ((pump.cmd[3] & 0xf0) == 0xb0 && 
			(pump.cmd[3] & 0x0f) == pump.panelPhy[pos]) ||
			((pump.cmd[3]&0xf0)==0xc0 &&
			 ((pump.cmd[3] & 0x0f) + 0x09 == pump.panelPhy[pos])) ) {

			if ((pump.cmd[14] & 0x0f) != 0x00) {
				pump.gunLog[pos] = pump.cmd[14] & 0x0f;	// 当前抬起的油枪
				ln_status = 1 << (pump.gunLog[pos] - 1);
				RunLog("节点[%d]面板[%d] %d 号枪抬起", node, pos + 1, pump.gunLog[pos]);
				lpDispenser_data->fp_data[pos].Log_Noz_State = ln_status;
				return 0;
			} else {
				RunLog("节点[%d]面板[%d] 数据有误",node,pos + 1);
				continue;
			}
		}


	}

	return -1;
}

/*
 * func: 初始的pump.gunLog, 预置加油指令需要; 不修改 Log_Noz_State
 */
static int InitGunLog()
{
	int i;
	int ret;

	for (i = 0; i < pump.panelCnt; ++i) {
		ret = GetTransData(i);
		if (ret < 0) {
			return -1;
		}

		pump.gunLog[i] = (pump.cmd[9] & 0x0F) + 1;   
	}

	return 0;
}

/*
 * 初始化FP的当前油价. 从配置文件中读取单价数据,
 * 写入 fp_data[].Current_Unit_Price
 * 假设 PR_Id 与 PR_Nb 是一一对应的
 */
static int InitCurrentPrice() {
	int i, j;
	int ret;
	int pr_no;
	int fm_idx;
	__u8 price[4];

	for (i = 0; i < pump.panelCnt; i++) {
		for (j = 0; j < pump.gunCnt[i]; j++) {
			pr_no = (lpDispenser_data->fp_ln_data[i][j]).PR_Id - 1;  // 0-7	
			if ((pr_no < 0) || (pr_no > 7)) {
				RunLog("InitPumpPrice PR_Id error, PR_Id[%d]", pr_no + 1);
				return -1;
			}

			fm_idx = lpDispenser_data->fp_data[i].Default_Fuelling_Mode - 1;
			memcpy(lpDispenser_data->fp_data[i].Current_Unit_Price, 
				lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price, 4);
#ifdef DEBUG
			RunLog("节点[%d]面板[%d] LNZ %d PR_Id %d,Prod_Price: %02x%02x%02x%02x",
				node,i + 1, j, pr_no + 1,
				lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[0],
				lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[1],
				lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2],
				lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3]);
#endif
		}
	}


	return 0;
}

/*
 * func: 初始化油枪泵码
 */
static int InitPumpTotal()
{
	int ret;
	int i, j;
	int  num;
	int pr_no;
	__u8 *p;
	__u8 status;
	__u8 lnz;
	__u8 meter_total[7] = {0};
	__u8 extend[GUN_EXTEND_LEN] = {0};
	__u8 amount_total[7] = {0};
	__u8 price_bcd[2] = {0};
	__u8 price_hex[2] = {0};
	__u64 total;

	// 错开各个通道初始化泵码的时间, 减轻IO程序压力
	Ssleep(pump.chnLog * 20 * INTERVAL);  // added by jie @ 2009.4.18

	for (i = 0; i < pump.panelCnt; i++) {
		ret = GetPumpStatus(i, &status);
		if (ret < 0) {
			return -1;
		}

		if (status != PUMP_OFF && status != PUMP_CALL &&
			status != PUMP_STOP && status != PUMP_PEOT &&
						status != PUMP_FEOT) {
			RunLog("节点[%d]面板[%d]当前FP状态不能查询泵码,Status[%02x]", node, i + 1, status);
			if (status == PUMP_AUTH) {
				// 如果是已授权状态, 则把它取消掉, 避免通讯正常, 但是无法上线
				DoStop(i);
			}
			return -1;
		}
		if(point_flag == 1){ //需要去百分位
			ret = GetPumpPercentile(i);
			if (ret < 0) {
				RunLog("节点[%d]面板[%d]读取百分位失败", node,i+1);
				return -1;
			}
		}
		
		Ssleep(INTERVAL);
		ret = GetPumpTotal(i);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]取泵码失败", node,i+1);
			return -1;
		}

		
		num = (pump.cmdlen - 4) / 30; //每条记录30字节,4个字节为其他包头,包尾
		p = pump.cmd + 1;

		for (j = 0; j < num; j++) {
			// Volume_Total
			memcpy(extend, p + 3, GUN_EXTEND_LEN);
			lnz = p[1] & 0x0F;
			pr_no = (lpDispenser_data->fp_ln_data[i][lnz]).PR_Id - 1;  // 0-7	
			RunLog("节点[%d]面板[%d], p[1]: %02x,j: %d, lnz: %d, pr_no:%d",node,i+1,p[1], j, lnz, pr_no);
			if ((pr_no < 0) || (pr_no > 7)) {
				RunLog("节点[%d]面板[%d], InitPumpTotal,FP %d, LNZ %d, PR_Id %d",
									node,i+1, i, lnz + 1, pr_no + 1);
				p = p + 30;
				continue;
			}

			/*长吉泵码分1位小数和2位小数两种*/
		/*	if(point_flag == 0){
				meter_total[0] = 0x0A;
				meter_total[1] = 0x00;
				meter_total[2] = extend[7] & 0x0f;
				meter_total[3] = (extend[6] << 4) | (extend[5] & 0x0F);
				meter_total[4] = (extend[4] << 4) | (extend[3] & 0x0F);
				meter_total[5] = (extend[2] << 4) | (extend[1] & 0x0F);
				meter_total[6] = (extend[0] << 4);
			}else if(point_flag == 1){
				meter_total[0] = 0x0A;
				meter_total[1] = 0x00;
				meter_total[2] = extend[7] & 0x0f;
				meter_total[3] = (extend[6] << 4) | (extend[5] & 0x0F);
				meter_total[4] = (extend[4] << 4) | (extend[3] & 0x0F);
				meter_total[5] = (extend[2] << 4) | (extend[1] & 0x0F);
				
				switch(lnz){
					case 0:
						meter_total[6] = (extend[0] << 4)|(GilPercentile[i].lnzOne& 0x0F);
						break;
					case 1:
						meter_total[6] = (extend[0] << 4)|(GilPercentile[i].lnzTwo& 0x0F);
						break;
					case 2:
						meter_total[6] = (extend[0] << 4)|(GilPercentile[i].lnzThr& 0x0F);
						break;
					case 3:
						meter_total[6] = (extend[0] << 4)|(GilPercentile[i].lnzFou& 0x0F);
						break;
					default:
						break;
				}

			}*/
			
			{
				meter_total[0] = 0x0A;
				meter_total[1] = 0x00;
				meter_total[2] = 0x00;
				meter_total[3] = (extend[7] << 4) | (extend[6] & 0x0F);
				meter_total[4] = (extend[5] << 4) | (extend[4] & 0x0F);
				meter_total[5] = (extend[3] << 4) | (extend[2] & 0x0F);
				meter_total[6] = (extend[1] << 4) | (extend[0] & 0x0F);
			}

			memcpy(lpDispenser_data->fp_m_data[pr_no].Meter_Total, meter_total, 7);
			memcpy((lpDispenser_data->fp_ln_data[i][lnz]).Log_Noz_Vol_Total, meter_total, 7);
#if DEBUG
			RunLog("节点[%d]面板[%d] Init.fp_ln_data[%d][%d].Vol_Total: %s",
				node,i+1, i, lnz, Asc2Hex(lpDispenser_data->fp_ln_data[i][lnz].Log_Noz_Vol_Total, 7));
			RunLog("节点[%d]面板[%d] Init.fp_m_data[%d].Meter_Total[%s]",
									node,i+1,pr_no, Asc2Hex(meter_total, 7));
#endif
			// Amount_Total
			memcpy(extend, p + 12, GUN_EXTEND_LEN);
			meter_total[0] = 0x0A;
			meter_total[1] = 0x00;
			meter_total[2] = 0x00;
			meter_total[3] = (extend[7] << 4) | (extend[6] & 0x0F);
			meter_total[4] = (extend[5] << 4) | (extend[4] & 0x0F);
			meter_total[5] = (extend[3] << 4) | (extend[2] & 0x0F);
			meter_total[6] = (extend[1] << 4) | (extend[0] & 0x0F);
			memcpy((lpDispenser_data->fp_ln_data[i][lnz]).Log_Noz_Amount_Total, meter_total, 7);
			RunLog("节点[%d]面板[%d] Init.fp_ln_data[%d][%d].Amount_Total: %s ", node,i+1, i, lnz,
				       	Asc2Hex(lpDispenser_data->fp_ln_data[i][lnz].Log_Noz_Amount_Total, 7));

			p = p + 30;
		}

	}

	return 0;
}



/*
 * func: 与油机保持通讯 x 秒, 了解在等待期间油机的状态, 对于某些油机还有保持主控模式的作用
 * return: GetPumpStatus失败的次数
 */
static int keep_in_touch_xs(int x)
{
	int i;
	int fail_times = 0;
	time_t return_time = 0;
	__u8 status;

	return_time = time(NULL) + (x);
	RunLog("==================return_time: %ld =======", return_time);
	do {
		Ssleep(200000);  // 200ms
		for (i = 0; i < pump.panelCnt; ++i) {
			if (GetPumpStatus(i, &status) < 0) {
				fail_times++;
			}
		}
	} while (time(NULL) < return_time);

	return fail_times;
}


/*
 * func: 设定串口属性—通讯速率、校验位等
 */
static int SetPort001()
{
	int ret;

	pump.cmd[0] = 0x1C;
	pump.cmd[1] = 0x52;
	pump.cmd[2] = 0x30 | (pump.chnPhy - 1);
	pump.cmd[3] = 0x05;	// 5760bps
	pump.cmd[4] = 0x01 << ((pump.chnPhy - 1) % 8);    // check
	pump.cmd[5] = 0x00;	// even
	pump.cmd[6] = 0x01 << ((pump.chnPhy - 1) % 8);    // check
	pump.cmdlen = 7;
	pump.wantLen = 0;

	ret =  WriteToPort(pump.fd, pump.chnPhy, pump.chnLog, pump.cmd,
			pump.cmdlen, 0, pump.wantLen, TIME_OUT_WRITE_PORT);
	if (ret < 0) {
		return -1;
	}

	gpGunShm->chnLogSet[pump.chnLog - 1] = 1;
	Ssleep(50*INTERVAL);      // 确保进行数据通讯之前串口属性已经设置好

	RunLog("通道 %d 设置串口属性成功", pump.chnLog);

	return 0;
}


/*
 * func:  通讯不通时轮询过程中隔一段时间重设一下串口属性
 *        避免因意外的电压不稳导致单片机复位--串口属性设置恢复预设9600,8,n,1
 */
static int ReDoSetPort001()
{
#define RE_DO_FREQUENCY   10         //  10次轮询失败设置一下串口属性

	int static polling_fail_cnt = 0;
	int ret;

	if ((++polling_fail_cnt) >= RE_DO_FREQUENCY) {
		ret = SetPort001();
		if (ret < 0) {
			RunLog("节点 %d 重设串口属性失败", node);
		}
		polling_fail_cnt = 0;
	}

	return ret;
}


static int DoRation_Idle(int pos)  
{
#define  PRE_VOL  0xF1
#define  PRE_AMO  0xF2
	int ret;
	int trynum;
	__u8 amount[6];
	__u8 preset_type;
	__u8 lnz_allowed = 0;      // 油机枪号从0开始

	if (lpDispenser_data->fp_data[pos].Log_Noz_Mask == 0) {
		RunLog("Node[%d]FP[%d]未允许任何油枪加油", node, pos + 1);
		return -1;
	}

	// Step.0 获得允许枪号
	for (ret = 0x01; ret <= 0x80; ret = (ret << 1), lnz_allowed++) {
		if ((lpDispenser_data->fp_data[pos].Log_Noz_Mask & ret) != 0) {
			break;
		}
	}

	// 正常的流程
	if (ration_type[pos] == 0x3D) { 
		preset_type = PRE_VOL;
	} else {
		preset_type = PRE_AMO;
	}

	amount[0] = 0xe0 | (ration_value[pos][2] & 0x0f);
	amount[1] = 0xe0 | (ration_value[pos][2] >> 4);
	amount[2] = 0xe0 | (ration_value[pos][1] & 0x0f);
	amount[3] = 0xe0 | (ration_value[pos][1] >> 4);
	amount[4] = 0xe0 | (ration_value[pos][0] & 0x0f);
	amount[5] = 0xe0 | (ration_value[pos][0] >> 4);

	for (trynum = 0; trynum < 3; trynum++) {
		// Step.1.1 发送预置命令
		pump.cmd[0] = 0x20 | pump.panelPhy[pos];
		pump.cmdlen = 1;
		pump.wantLen = 1;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d], 预写预置加油命令失败", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd)-1;

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);

		if (ret < 0) {
			RunLog("节点[%d]面板[%d], 预写预置加油命令成功, 取返回信息失败", node, pos + 1);
			Ssleep(2 * INTERVAL);
			continue;
		} else if (pump.cmd[0] != (0xD0 | pump.panelPhy[pos])) { 
			Ssleep(2000000);            // 2x命令间隔要求为2s
			continue;
		}							
		RunLog("节点[%d]面板[%d] Step.1 预写预置加油命令成功", node, pos + 1);
		
		// Step.1.2 发送预置量
		pump.cmd[0] = 0xff;
		pump.cmd[1] = 0xe3;
		pump.cmd[2] = preset_type;
		pump.cmd[3] = 0xf4;      // Price Level 1

		if ((lpDispenser_data->fp_data[pos].Log_Noz_Mask != 0xFF) ||
			(preset_type == PRE_VOL) ||(lpNode_channel->cntOfFpNoz[pos] == 1)) { 
			// 只有油量预置能够指定到枪
			pump.cmd[4] = 0xf6;      // Grade data next
			pump.cmd[5] = 0xe0 | lnz_allowed;
			pump.cmd[6] = 0xf8;
			pump.cmd[7] = amount[0];
			pump.cmd[8] = amount[1];
			pump.cmd[9] = amount[2];
			pump.cmd[10] = amount[3];
			pump.cmd[11] = amount[4];		
			pump.cmd[12] = amount[5];		
			pump.cmd[13] = 0xfb;
			pump.cmd[14] = 0xe0 | (LRC(pump.cmd, 14) & 0x0f);
			pump.cmd[15] = 0xf0;
			pump.cmdlen = 16;
			pump.wantLen = 0;
		} else { //0xFF 代表上位机下发预置命令时没有指定油枪, add by liys @ 2009.02.06
			pump.cmd[4] = 0xf8;
			pump.cmd[5] = amount[0];
			pump.cmd[6] = amount[1];
			pump.cmd[7] = amount[2];
			pump.cmd[8] = amount[3];
			pump.cmd[9] = amount[4];		
			pump.cmd[10] = amount[5];		
			pump.cmd[11] = 0xfb;
			pump.cmd[12] = 0xe0 | (LRC(pump.cmd, 12) & 0x0f);
			pump.cmd[13] = 0xf0;
			pump.cmdlen = 14;
			pump.wantLen = 0;		   	
		}	


		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d], 预写预置加油量失败", node, pos + 1);
			// WriteToPort 失败已经延时2秒, 所以不再需要设置延时再continue
			continue;
		}
              
		RunLog("节点[%d]面板[%d] Step.2 预写预置加油量成功[%s]",
			node, pos + 1, Asc2Hex(pump.cmd, pump.cmdlen));
		
		return 0; 
	}


	return -1;
}


/*
 * 检查FP上各枪的油品单价是否符合正确(符合实际), 如果全部正确则改变状态为Closed
 */
static int CheckPriceErr(int pos)
{
	int ret;
	int lnz, pr_no;
	int fm_idx;
	__u8 status;
	__u8 msp[4] = {0};
	__u8 price[4] = {0};

	if ((__u8)FP_State_Inoperative != lpDispenser_data->fp_data[pos].FP_State) {
		return -1;
	}


	for (lnz = 0; lnz < pump.gunCnt[pos]; lnz++) {
		pr_no = (lpDispenser_data->fp_ln_data[pos][lnz]).PR_Id - 1;  // 0 - 7	
		if ((pr_no < 0) || (pr_no > 7)) {
			RunLog("CheckPrice ERR PR_Id error, FP[%d]LNZ[%d] PR_Id[%d] <ERR>",
								pos + 1, lnz + 1, pr_no + 1);
			return -1;
		}

		memset( price, 0, sizeof(price));
		memset( msp, 0, sizeof(msp));
		
		ret = GetGunPrice(pos, lnz + 1, msp);
		if(ret < 0){
			return -1;
		}

		price[2] = (msp[3] << 4) | (msp[2] & 0x0f);
		price[3] = (msp[1] << 4) | (msp[0] & 0x0f);
		fm_idx = lpDispenser_data->fp_data[pos].Default_Fuelling_Mode - 1;

		RunLog("Node %d FP %d LNZ %d Now price is %02x.%02x; DST Price: %02x.%02x",
			node, pos + 1, lnz + 1, price[2], price[3],	
			lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2], 
		       	lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3]);
		

		if ((price[2] != lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2]) ||
			(price[3] != lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3])) {
			return -2;
		}
	}
	
	ChangeFP_State(node, 0x21 + pos, FP_State_Closed, NULL);
	RunLog("Node[%d]FP[%d]单价已变更正确, 切换状态为Closed", node, pos + 1);

	return 0;
}

/*
 * func: 读取长吉油机小数点位数
 */
static int GetPumpPoint()
{
	int i;
	int ret;
	__u8 sfc_cmd[] = { 0xff, 0xe9, 0xfe, 0xe0, 0xe1, 0xe0, 0xfb, 0xee, 0xf0 };


	for (i = 0; i < 5; ++i) {         // Note. GetPumpStatus 循环变量最好用本地的,不要用trynum
		pump.cmd[0] = 0x20 | pump.panelPhy[0];
		pump.cmdlen = 1;
		pump.wantLen = 1;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d] 向串口写数据失败",node,1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d] 取返回信息失败",node, 1);
			continue;
		}

		if (pump.cmd[0] != (0xd0 | pump.panelPhy[0])) {
			RunLog("节点[%d]面板[%d] 信息确认失败",node,1);
			Ssleep(2000000);   // 间隔2s
			continue;
		}

		memcpy(pump.cmd, sfc_cmd, 9);
		pump.cmdlen = 9;
		pump.wantLen = 19;

		Ssleep(INTERVAL_TOTAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT) ;
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]发读泵码小数点位数失败",node,1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]发取泵码位数成功, 取返回信息失败",node,1);
			continue;
		}
		

		if ((pump.cmd[9] & 0xf0) != 0xb0) {
			RunLog("Node[%d]Message Error.PF_Id not match", node);
			continue;
		}

		point_flag = pump.cmd[9] & 0x0f;

		if(point_flag < 0 || point_flag >2){
			RunLog("节点[%d]小数点位读取错误point_flag[%d]",node, point_flag);
			point_flag = 0;
		}

		switch(point_flag){
			case 0:
				RunLog("节点[%d]point_flag[%d] 一位泵码小数位",node, point_flag);
				break;
			case 1:
				RunLog("节点[%d]point_flag[%d] 一位泵码小数位,010命令取百分位"
					,node, point_flag);
				break;
			case 2:
				RunLog("节点[%d]point_flag[%d] 两位泵码小数位",node, point_flag);
				break;
			default:
				RunLog("节点[%d]point_flag[%d] 错误!",node, point_flag);
				break;
		}
			
		return 0;
	}

	return -1;
}


/*
 * func: 读取长吉油机百分位数据
 */
static int GetPumpPercentile(const int pos)
{
	int i;
	int ret;
	__u8 sfc_cmd[] = { 0xff, 0xe9, 0xfe, 0xe0, 0xe1, 0xe0, 0xfb, 0xee, 0xf0 };

	if(pos < 0 || pos > MAX_FP_CNT){
		RunLog("GetPumpPercentile  pos[%d]错误", pos);
		return -1;
	}
	
	for (i = 0; i < 5; ++i) {         // Note. GetPumpStatus 循环变量最好用本地的,不要用trynum
		pump.cmd[0] = 0x20 | pump.panelPhy[pos];
		pump.cmdlen = 1;
		pump.wantLen = 1;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d] 向串口写数据失败",node,1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d] 取返回信息失败",node, 1);
			continue;
		}

		if (pump.cmd[0] != (0xd0 | pump.panelPhy[pos])) {
			RunLog("节点[%d]面板[%d] 信息确认失败",node,1);
			Ssleep(2000000);   // 间隔2s
			continue;
		}

		memcpy(pump.cmd, sfc_cmd, 9);
		pump.cmdlen = 9;
		pump.wantLen = 19;

		Ssleep(INTERVAL_TOTAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT) ;
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]发读泵码小数点位数失败",node,1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]发取泵码位数成功, 取返回信息失败",node,1);
			continue;
		}
		
		//ret: ba b0 b3  B1  b0 b1 b0 b0 b0  b0 b1 b0 B1 b1 B1  c3 c1 8d 8a
		if ((pump.cmd[10] & 0xf0) != 0xb0) {
			RunLog("Node[%d]Message Error.PF_Id not match", node);
			continue;
		}

		GilPercentile[pos].lnzOne = pump.cmd[10] & 0x0f;

		if ((pump.cmd[10] & 0xf0) != 0xb0) {
			RunLog("Node[%d]Message Error.PF_Id not match", node);
			continue;
		}

		GilPercentile[pos].lnzTwo= pump.cmd[11] & 0x0f;

		if ((pump.cmd[10] & 0xf0) != 0xb0) {
			RunLog("Node[%d]Message Error.PF_Id not match", node);
			continue;
		}

		GilPercentile[pos].lnzThr= pump.cmd[12] & 0x0f;

		if ((pump.cmd[10] & 0xf0) != 0xb0) {
			RunLog("Node[%d]Message Error.PF_Id not match", node);
			continue;
		}

		GilPercentile[pos].lnzFou= pump.cmd[13] & 0x0f;

		RunLog("###Node[%d]FP[%d] LNZ Percentile 1[%02x] 2[%02x] 3[%02x] 4[%02x]",
			node, pos+1, GilPercentile[pos].lnzOne, GilPercentile[pos].lnzTwo, GilPercentile[pos].lnzThr,
			GilPercentile[pos].lnzFou);
			
		return 0;
	}

	return -1;
}


/* ----------------- End ----------------------------- */








/*
 * Filename: lischn019.c
 *
 * Description: 加油机协议转换模块 -- 恒山UDC, 3 Line CL, 9600,8,n,1
 *
 * 协议文档: UDC Protocol P1 & P2 
 * 联调油机: TH30系列
 *
 * 2009.06 Created by Guojie Zhu <zhugj@css-intelligent.com>
 * 
 * Note:
 *     1.
 *     2. D0状态挂枪, 读到的交易可能为零,泵码正常 (油机缺陷)
 *
 * History:   
 *
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

#define NOZ_TOTAL_LEN  5

// FP State
#define FP_INOPERATIVE  0x2F      // Uninitialized

#define FP_IDLE         0x20
#define FP_CALLING1     0xA0      // cash sale
#define FP_CALLING2     0xA1      // credit sale
#define FP_AUTHORISED   0x90
#define FP_FUELLING1    0xD0      // 小阀开 
#define FP_FUELLING2    0xF0      // 大阀开
#define FP_SPFUELLING   0x98      // Suspend Fuelling

#define TRY_TIME	5
#define INTERVAL	150000    // 150ms命令发送间隔, 数值视具体油机而定

typedef struct {
	__u8 a1_rsp_len;   // 18 or more...
	__u8 a2_rsp_len;   // 2 or more...
	__u8 a3_rsp_len;   // 2 or 4
	__u8 a4_rsp_len;   // 2 or 4
	__u8 a5_rsp_len;   // 2 or 4
	__u8 a6_rsp_len;   // 2 or 4
}CMD_VAR_PARAM;

static int DoCmd();
static void DoPolling();
static int DoAuth(int pos);
static int DoBusy(int pos);
static int DoCall(int pos);
static int DoEot(int pos);
static int DoOpen(int pos, __u8 *msgbuffer);
static int DoClose(int pos, __u8 *msgbuffer);
static int DoRation(int pos);
static int DoStarted(int i);
static int DoStop(int pos);
static int DoMode();
static int DoPrice();
static int ChangePrice();
static int GetPrice(int pos);
static int GetGunTotal(int pos, __u8 gunPhy, __u8 *vol_total, __u8 *amt_total);
static int GetPumpTotal(int pos);
static int GetPumpStatus(int pos, __u8 *status);
static int InitPumpTotal();
static int InitCurrentPrice();
static int InitGunLog();
static int GetCurrentLNZ(int pos);
static int keep_in_touch_xs(int x_seconds);
static int SetDisplayData(int pos);
static int ReDoSetPort019();
static int GetDisplayData(int pos);
static int GetPumpID(int pos, __u8 *id);
static int AckDeactivedHose(int pos);
static int Wait4PumpReady();
static int ReWritePrice(int pos);
static int DoOnline();
static int DoOffline();
static int SetPort019();
static int DoubleTalkCheck(const __u8 *buf, int len);
static int CheckPriceErr(int pos);

static PUMP pump;
static int node;
static int IFSFpos;
static DISPENSER_DATA *lpDispenser_data;
static NODE_CHANNEL *lpNode_channel;
static int fp_state[MAX_FP_CNT];     // 0 - IDLE, 1 - CALLING , 2 - DoStarted 成功 3 - FUELLING
static int dostop_flag[MAX_FP_CNT];  // 1 - 成功执行过 Terminate_FP
/* ack_deactived_hose_flag标记是否需要执行挂枪确认和读当次交易. 1 - 是, 0 - 否; 
 * 初始就要执行一次是避免考虑此程序启动之前油机有未被确认的挂枪和未读取的交易.
 */
static int ack_deactived_hose_flag[MAX_FP_CNT] = { 1, 1, 1, 1};
static int nb_prods[MAX_FP_CNT];     // 记录油机上各个FP配置的油品数量是多少, 与变价相关
static __u8 display_data[MAX_FP_CNT][16] = {      // 存放主显数据用于2F状态时初始化油机
	{0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff,
		0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff}, 
	{0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff,
		0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff}, 
	{0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff,
		0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff}, 
	{0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff,
		0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff}, 
};


int ListenChn019(int chnLog)
{
	long	indtyp;
	int	ret;
	int	fd;
	int	i, j;
	__u8    id = 0, status = 0;
	
	sleep(5);   //防止此进程退出,而主进程没有启动完毕. 每个lischnxxx都需要
	RunLog("Listen Channel  019 ,logic channel[%d]", chnLog);
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
						// 地址映射: 0x00 - 0x0F 对应FP地址 1-16
						pump.panelPhy[j] = gpIfsf_shm->gun_config[idx].phyfp - 1;
						break;
					}
				}
				//pump.gunCnt[j] =  gpIfsf_shm->node_channel[i].cntOfFpNoz[j];
				pump.gunCnt[j] = 4;   // fix value, modified by Guojie Zhu @ 2009.11.07
			}
				
			break;
		}
	}
	pump.indtyp = chnLog;
	memset(&pump.newPrice, 0, FP_PR_NB_CNT * 4);
	
	// ============= Part.2 打开 & 设置串口属性 -- 波特率/校验位等 ===================

	strcpy(pump.cmd, alltty[pump.port - 1]);
	pump.fd = OpenSerial(pump.cmd);
	if (pump.fd < 0) {
		RunLog("节点[%d]打开串口[%s]失败", node, pump.cmd);
		return -1;
	}

	ret = SetPort019();
	if (ret < 0) {
		RunLog("节点[%d]设置串口属性失败", node);
		return -1;
	}

	
	// ============== Part.3 判断加油机是否在线 ====================================
	// Check wether dispenser is online, if it's not online, wait  here.
	while (1) {		
		// 用返回数据较长的功能测试通讯, 否则在有错误数据的情况下会判断出错.
		ret = GetDisplayData(0);
		if (ret == 0) {		
			break;
		}
		
		gpIfsf_shm->node_online[IFSFpos] = 0; //不在线,不产生心跳.
		UserLog("节点 %d 可能不在线", node);
		
		sleep(1);
	}
	
	ret = GetPumpID(0, &id);
	if (ret < 0) {
		return -1;
	}

	ret = Wait4PumpReady();
	if (ret < 0) {
		RunLog("Node[%d] 状态未准备好, 等待重试", node);
		return -1;
	}

	// ============= Part.4 初始化与加油机当前状态相关的数据库相关项 =======================
	ret = InitCurrentPrice(); //  初始化fp数据库的当前油价
	if (ret < 0) {
		RunLog("初始化油品单价到数据库失败");
		return -1;
	}

	ret = InitPumpTotal();    // 初始化泵码到数据库
	if (ret < 0) {
		RunLog("初始化泵码到数据库失败");
		return -1;
	}


     //+++++++++++++++  一切就绪, 上线+++++++++++++++++++++++//
       
//	gpIfsf_shm->node_online[IFSFpos] = node;  
	ChangeNode_Status(node, NODE_ONLINE);
	UserLog("节点 %d 在线!", node);
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
//			Ssleep(100000);       // 正常轮询间隔为100ms + INTERVAL
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
	__u8 msgbuffer[48];

	// 提取调用命令执行函数前的共有操作
	if (pump.cmd[0] != 0x2F) {
		fp_id = pump.cmd[2];
		if (fp_id == 0x00) {  // fp_id == 0x00 表示对所有fp操作, 对应 pos 置为 255
			pos = 255;
		} else if ((fp_id >= 0x21) && (fp_id <= 0x24)) {
			pos = fp_id - 0x21;
		} else {
			if (pump.cmd[0] != 0x18 && pump.cmd[0] != 0x19) {  // DoPrice & DoMode不需要处理面板号
				RunLog("非法的面板地址");
				return -1;
			}
		}				

		memset(msgbuffer, 0, sizeof(msgbuffer));
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
			memset(pump.addInf[pos], 0, sizeof(pump.addInf[pos]));
			if ((__u8)FP_State_Calling == lpDispenser_data->fp_data[pos].FP_State) {
				ret = 0;
			} else {
				ret = DoStop(pos);
			}

			if (ret == 0) {
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
			}else if (ret == -2) {
				SendCmdAck(NACK, msgbuffer);
				ChangeFP_State(node, pos + 0x21,
					FP_State_Idle, NULL);

			}else{
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
				lpDispenser_data->fp_data[pos].Log_Noz_Mask = 0xFF;
				bzero(&lpDispenser_data->fp_data[pos].Remote_Volume_Preset, 5);
				bzero(&lpDispenser_data->fp_data[pos].Remote_Amount_Prepay, 5);
				break;
			}

			/*
			 * 抬枪之后下发预置所使用的标记变量和缓冲区:
			 * pump.addInf[pos][0]: 预置标记, 标识是否抬枪后是否有预置需要下发
			 * pump.addInf[pos][1]: 预置方式标记字节
			 * pump.addInf[pos][2]: 预置允许油枪, 0xFF - 允许全部
			 * pump.addInf[pos][3-5]:  预置量缓存
			 */
			pump.addInf[pos][0] = 1;            // 预置标记, 标记有预置需要下发
			pump.addInf[pos][1] = pump.cmd[0];  // 预置方式
			pump.addInf[pos][2] = lpDispenser_data->fp_data[pos].Log_Noz_Mask;

			if (pump.cmd[0] == 0x3D) {
				memcpy(&(pump.addInf[pos][3]), 
					&lpDispenser_data->fp_data[pos].Remote_Volume_Preset[2], 3);
				ret = 0;
			} else if (pump.cmd[0] == 0x4D) {
				memcpy(&(pump.addInf[pos][3]), 
					&lpDispenser_data->fp_data[pos].Remote_Amount_Prepay[2], 3);
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

			memset(&lpDispenser_data->fp_data[pos].Remote_Volume_Preset, 0, 5);
			memset(&lpDispenser_data->fp_data[pos].Remote_Amount_Prepay, 0, 5);
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
			RunLog("节点[%d]接受到未定义命令[0x%02x]", node, pump.cmd[0]);
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
	int tmp_lnz;
	static int times = 0;
	static int fail_cnt = 0;  // 记录GetPumpStatus 失败次数, 用于判定离线
	static __u8 pre_status[MAX_FP_CNT] = {0};
	__u8 status;
	
	for (i = 0; i < pump.panelCnt; i++) {
		ret = GetPumpStatus(i, &status);
		if (ret < 0) {
			if (gpIfsf_shm->node_online[IFSFpos] == node) {
				if ((++fail_cnt) < MAX_FAIL_CNT) {  // 加油机离线判断
					/*
					 * 因重设串口属性会导致ReadTTY接收到"非法数据", 故暂时移除
					 * 2009.02.19
					if (fail_cnt >= (MAX_FAIL_CNT / 2)) {
						if (SetPort019() >= 0) {
							RunLog("节点%d重设串口属性成功", node);
						}
					}
					*/
					continue;
				}

				DoOffline();
			}

			// 重设串口属性
			//ReDoSetPort019();
			continue;
		} else {
			if (gpIfsf_shm->node_online[IFSFpos] == 0) {
				if ((--fail_cnt) > 0) { 
					fail_cnt = 1;   // 两次OK即判断油机联线
					continue;
				} else if (time(NULL) < pump.time_offline) {
					RunLog("================ waiting for 3 Heartbeat Interval ================");
					continue;
				}

				// fail_cnt复位
				fail_cnt = 0;

				if (DoOnline() < 0) {
					fail_cnt = 1;  // 重试联机处理
				}

				continue;
			}
			fail_cnt = 0;
		}
		
		if (status != pre_status[i]) {
			pre_status[i] = status;
			RunLog("节点[%d]的面板[%d]当前状态字为[%02x]", node, i + 1, status);
		}

		if (gpIfsf_shm->auth_flag - FCC_SWITCH_AUTH != 0) {  //如果有后台
			if ((__u8)FP_State_Closed == lpDispenser_data->fp_data[i].FP_State) {
				if (i == 0) {
					times += pump.panelCnt;					
				}
				if (times > (SENDSTATE *  pump.panelCnt)) {
					if (i >= pump.panelCnt -1)
						times = 0;
					UserLog("节点[%d]的面板[%d]处于Closed状态", node, i + 1);
				}

				// Closed 状态下也要能够变价
				ChangePrice();
				continue; 
			}
		} else {
			if ((__u8)FP_State_Closed == lpDispenser_data->fp_data[i].FP_State) {
				RunLog("DoPolling, 没有后台改变FP[%02x]的状态为IDLE", i + 0x21);
				ret = SetHbStatus(node, 0);   // IFSF_HEARTBEAT.status set to 0
				ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);			
			}
		}

		// 若FP状态为Inoperative, 要求人工干预,程序不能改变其状态,也不能在该状态下做任何操作
		if ((__u8)FP_State_Inoperative == lpDispenser_data->fp_data[i].FP_State) {
			RunLog("Node[%d].FP[%d]处于Inoperative状态!", node, i + 1);
			// Inoperative 状态下也要能够变价
			ChangePrice();

			CheckPriceErr(i);
			continue;
		}

		switch (status & 0xF9) {
		case FP_IDLE:
			// 1.
			if (fp_state[i] >= 1) {
				// 若有加过油, 则进行状态改变和交易处理
				// 1. ChangeFP_State to IDLE first
				tmp_lnz = lpDispenser_data->fp_data[i].Log_Noz_State;
				if (((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State) ||
						 (lpDispenser_data->fp_data[i].Log_Noz_State != 0)) {
					lpDispenser_data->fp_data[i].Log_Noz_State = 0;
					ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
				}
				// Step2. Get & Save TR to TR_BUFFER
				lpDispenser_data->fp_data[i].Log_Noz_State = tmp_lnz;
				ret = DoEot(i);
			}

			// 2. 必须在 "1."之后, 在交易处理完成之前屏蔽抬枪信号
			if (ack_deactived_hose_flag[i] != 0) {
				ret = AckDeactivedHose(i);
				if (ret == 0 && GetDisplayData(i) == 0) {
					ack_deactived_hose_flag[i] = 0;
				}
			}

			// 3.
			if (pump.addInf[i][0] != 0) { // 有预置量未下发, FP是Authorised状态
				/* 清除油枪状态Log_Noz_State,
				 * 避免抬错枪的情况下重新抬枪后DoStarted中无法重新读取当前枪号
				 */
				lpDispenser_data->fp_data[i].Log_Noz_State = 0;
				break;
			}


			// 4.
			lpDispenser_data->fp_data[i].Log_Noz_State = 0x00;
			fp_state[i] = 0;

			if ((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State) {
				ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
			}

			EO_OFFLINE_TR(node, i);
			if (gpIfsf_shm->fp_offline_flag[node - 1][i] != 0) {
				upload_offline_tr(node, NULL);
			}

			// 5.当FP状态为Idle的时候才下发变价
			for (j = 0; j < pump.panelCnt; ++j) {
				if ((__u8)FP_State_Idle != lpDispenser_data->fp_data[j].FP_State) {
					break;
				}
			}
			// 所有面都空闲才下发变价
			if (j >= pump.panelCnt) {
				ret = ChangePrice();            // 查看是否有油品需要变价,有则执行
			}
			break;
		case FP_CALLING1:
		case FP_CALLING2:
			// 预置处理--抬枪下发 
			if (pump.addInf[i][0] != 0) {           // 有预置量未下发
				fp_state[i] = 1;
				DoStarted(i);                   
				/* Note! DoStarted 要在DoRation之前执行,
				 * DoRation需要当前抬枪的枪号, 需要在抬枪状态下发预置量
				 */

				// 判断抬起的是否是需要预置的油枪, 如果不是则忽略
				if ((lpDispenser_data->fp_data[i].Log_Noz_State !=
					pump.addInf[i][2]) && pump.addInf[i][2] != 0xFF) {
					fp_state[i] = 0;
					break;
				}

				pump.addInf[i][0] = 0;
				ret = DoRation(i);
				// 预置处理完成, 回复允许全部油枪加油
				lpDispenser_data->fp_data[i].Log_Noz_Mask = 0xFF;
				break;
			}

			// 抬枪处理
			if ((__u8)FP_State_Calling > lpDispenser_data->fp_data[i].FP_State) {
				ret = DoCall(i);
				fp_state[i] = 1;

				DoStarted(i);
			}

			break;
		case FP_AUTHORISED:
			if ((__u8)FP_State_Calling == lpDispenser_data->fp_data[i].FP_State) {
				ret = ChangeFP_State(node, i + 0x21, FP_State_Started, NULL);
			} else if ((__u8)FP_State_Authorised > lpDispenser_data->fp_data[i].FP_State) {
				ret = ChangeFP_State(node, i + 0x21, FP_State_Authorised, NULL);
			}
			break;
		case FP_FUELLING1:
		case FP_FUELLING2:
			// 补充处理
			if (fp_state[i] < 2) {
				if (DoStarted(i) < 0) {
					continue;
				}
			}
			// 补充处理
			if ((__u8)FP_State_Started > lpDispenser_data->fp_data[i].FP_State) {
				ret = ChangeFP_State(node, i + 0x21, FP_State_Started, NULL);
			}

			DO_INTERVAL(DO_BUSY_INTERVAL, ret = DoBusy(i););

			if ((__u8)FP_State_Fuelling > lpDispenser_data->fp_data[i].FP_State) {
				if (fp_state[i] == 3) {
					ret = ChangeFP_State(node, i + 0x21, FP_State_Fuelling, NULL);
					if (ret < 0) {
						break;
					}
				}
			}

			break;
		case FP_SPFUELLING:
			if ((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State) {
				ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
			}
			break;
		default:
			break;
		}
		
	}
}


/*
 * func: Open_FP
 */
static int DoOpen(int pos, __u8 *msgbuffer) 
{
	int ret;
	int i;

	ret = SetHbStatus(node, 0);   // IFSF_HEARTBEAT.status set to 0
	if (ret < 0) {
		RunLog("心跳状态改变错误");
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

	if (lpDispenser_data->fp_data[i].Log_Noz_State == 0) {
		ret = GetCurrentLNZ(i);
		if (ret < 0) {
			return -1;
		}
	}

	ret = BeginTr4Fp(node, i + 0x21);
	if (ret < 0) {
		ret = BeginTr4Fp(node, i + 0x21);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d], BeginTr4Fp Failed", node, i + 1);
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
 */
static int DoRation(int pos)  
{
	int ret;
	int pr_no, fm_idx;
	int trycnt;
	int fail_cnt = 0;
	unsigned long pre_value;
	__u8 price[2] = {0};
	__u8 volume[3] = {0};
	__u8 amount[3] = {0};

	// 0. 复位预置标记
	pump.addInf[pos][0] = 0;

	if (pump.gunLog[pos] == 0) {  // 判断抬起枪号是否已经取得
		return -1;
	}

	// 1. 获取该枪单价
	pr_no = (lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).PR_Id - 1;  // 0 - 7	
	if ((pr_no < 0) || (pr_no > 7)) {
		RunLog("Node[%d] DoRation PR_Id error, PR_Id[%d] <ERR>", node, pr_no + 1);
		return -1;
	}

	fm_idx = lpDispenser_data->fp_data[pos].Default_Fuelling_Mode - 1;
	if (fm_idx < 0 || fm_idx > 7) {
		RunLog("Node[%d] DoRation default FM error, fm[%d] <ERR>", node, fm_idx + 1);
		return -1;
	}
	price[0] = lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2];
	price[1] = lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3];

	// 2. 计算对应的油量或者金额
	if (pump.addInf[pos][1] == 0x4D) {          // Prepay (机械式油机以金额作为限制
		memcpy(amount, &pump.addInf[pos][3], 3);
		pre_value = (Bcd2Long(amount, 3) * 10000) / Bcd2Long(price, 2);
		pre_value = (pre_value + 50) / 100; // 进位

		RunLog("-------------pre_volume: %u -----------", pre_value);
		if (Long2BcdR(pre_value, volume, 3) != 0) {
			RunLog("Node[%d].FP[%d]Preset, Exec Long2Bcd Failed", node, pos + 1);
			return -1;
		}
	} else if (pump.addInf[pos][1] == 0x3D) {   // Preset (电子式油机以油量作为限制)
		memcpy(volume, &pump.addInf[pos][3], 3);
		pre_value = Bcd2Long(volume, 3) * Bcd2Long(price, 2) / 100;
		/*
		 * Note! 实际测试结果显示此油机(TH30系列)金额小数不进位
		 */
		RunLog("-------------pre_amount: %u -----------", pre_value);

		if (Long2BcdR(pre_value, amount, 3) != 0) {
			RunLog("Node[%d].FP[%d]Preset, Exec Long2Bcd Failed", node, pos + 1);
			return -1;
		}
	} else {
		return -1;
	}
	

	// 3. 下发
	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {
		pump.cmd[0] = 0xF0 | pump.panelPhy[pos];
		pump.cmd[1] = ~pump.cmd[0];                       // double talk byte
		pump.cmd[2] = 0xA5;
		pump.cmd[3] = 0x5A;                               // ~pump.cmd[2]
		pump.cmd[4] = 0x05;                               // Slow Flow Offset
		pump.cmd[5] = 0xFA;

		pump.cmd[6] = price[1];                           // Unit_Price LSB
		pump.cmd[7] = ~pump.cmd[6];
		pump.cmd[8] = price[0];                           // Unit_Price MSB
		pump.cmd[9] = ~pump.cmd[8];
		pump.cmd[10] = amount[2];                         // Money LSB
		pump.cmd[11] = ~pump.cmd[10];
		pump.cmd[12] = amount[1];                         // Money 2SB     
		pump.cmd[13] = ~pump.cmd[12];
		pump.cmd[14] = amount[0];                         // Money MSB     
		pump.cmd[15] = ~pump.cmd[14];
		pump.cmd[16] = volume[2];                         // Volume LSB
		pump.cmd[17] = ~pump.cmd[16];
		pump.cmd[18] = volume[1];                         // Volume 2SB
		pump.cmd[19] = ~pump.cmd[18];
		pump.cmd[20] = volume[0];                         // Volume MSB
		pump.cmd[21] = ~pump.cmd[20];
		pump.cmdlen = 22;
		pump.wantLen = 2;

		Ssleep(INTERVAL); 
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]下发预置命令失败", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]下发预置命令成功, 取返回信息失败", node, pos + 1);
			continue;
		} 

		if (pump.cmd[1] != (__u8)~pump.cmd[0]) {
			RunLog("Node[%d].FP[%d]返回数据校验错误", node, pos + 1);
			continue;
		}

		if (pump.cmd[0] != FP_AUTHORISED) {
			RunLog("Node[%d].FP[%d]预置命令执行失败", node, pos + 1);
			fail_cnt++;
			if (fail_cnt >= 2) {
				fail_cnt = 0;
				ReWritePrice(pos);
			}
			continue;
		}

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
	
	pr_no  = pump.cmd[2] - 0x41; //油品地址到油品编号0,1,...7.
	if ((0x04 != pump.cmd[8]) || (0x00 != pump.cmd[9])) {
		RunLog("单价错误,不能下发");
		return -1;
	}
	
	if ((pump.newPrice[pr_no][2] | pump.newPrice[pr_no][3]) == 0) {
		// 上次执行变价(ChangePrice)后第一次收到WriteFM发来的变价命令
		pump.newPrice[pr_no][0] = 1;
	} else {
		pump.newPrice[pr_no][0]++;
	}

	// 不管是否是连续收到同种油品的变价命令, 也不管单价是否一致, 均使用新单价覆盖老单价
	pump.newPrice[pr_no][2] = pump.cmd[10];
	pump.newPrice[pr_no][3] = pump.cmd[11];
#ifdef DEBUG
	RunLog("Recv New Price-PR_ID[%d]TIMES[%d] %02x.%02x",
		pr_no + 1, pump.newPrice[pr_no][0], pump.newPrice[pr_no][2], pump.newPrice[pr_no][3]);
#endif

	return 0;
}


/*
 * func: 执行变价, 失败改变FP状态为INOPERATIVE, 并写错误数据库
 * Note: TMC油机变价命令需指定该面所有油枪的单价, 为方便执行提高变价速度,
 *       此实现在收到一条Fuel的变价命令后会做一定时间的等待(8次轮询),
 *       若还有后续的变价命令, 则一起执行;
 */
static int ChangePrice() 
{
#define WAIT_CNT  8
	int i, ret, trycnt;
	int pr_no, tmp_pr_no;
	int lnz_idx, idx;
	int price_change_flag = 0;  // 标记是否有油品需要变价, 0 - 不需要, 非零 - 需要
	int fm_idx;
	int error_code_msg_cnt = 0; // 需要反馈给Fuel的error code信息的条数
	static int wait_cnt = WAIT_CNT;    // 等待后续变价命令的时间, 以便一个面的新单价一起下发.
	__u8 err_code = 0x99;       // 0x99-成功, 0x37-执行失败, 0x36-通讯问题变价失败
	__u8 price_bak[64] = {0};   // 备份下发的单价数据部分, 用于比较变价结果
	__u8 status;
	
/*
 * 遍历新单价数组newPrice[FP_PR_NB_CNT], 如果有油品需要变价--数组值不为零,
 * 则遍历所有FP, 如果油品匹配就下发变价命令
 */
       
	price_change_flag = 0;
	for (pr_no = FP_PR_NB_CNT - 1; pr_no >= 0; pr_no--) {
//		RunLog(">>>1 newPrice[%d]: %02x.%02x", pr_no, pump.newPrice[pr_no][2], pump.newPrice[pr_no][3]);;
		if (pump.newPrice[pr_no][3] != 0x00 || pump.newPrice[pr_no][2] != 0x00) {
			RunLog("Node[%d].Pr_Id[%d]需要变价", node, pr_no);
			price_change_flag = 1;
			break;
		}
	}

	if (price_change_flag == 0) { 
		return 0;
	}
	
	if (wait_cnt > 0) {
		wait_cnt--;
		return 0;
	} else {
		wait_cnt = WAIT_CNT;
	}

	for (i = 0; i < pump.panelCnt; i++) {
		tmp_pr_no = 0;
		CriticalLog("下发变价命令给节点[%02d]面[%d]枪[*]......", node, i + 1);
		fm_idx = lpDispenser_data->fp_data[i].Default_Fuelling_Mode - 1;
		if (fm_idx < 0 || fm_idx > 7) {
			RunLog("Node[%d]FP[%d] Default FM Error[%d]", node, i + 1, fm_idx + 1);
			return -1;
		}

		err_code = 0x99;
		error_code_msg_cnt = 0;

		for (trycnt = 0; trycnt < TRY_TIME; trycnt++) {
			// 检查状态是否为2F,如果是则先初始化, 否则变价不成功
			GetPumpStatus(i, &status);              

			// 下发变价
			price_change_flag = 0;          // 复位标记
			Ssleep(INTERVAL * 2);
			pump.cmd[0] = 0xC0 | pump.panelPhy[i];
			pump.cmd[1] = ~pump.cmd[0];     // double talk byte
			pump.cmd[2] = 0xA3;
			pump.cmd[3] = 0x5C;             // ~pump.cmd[2]

			idx = 4;
			for (lnz_idx = 0; lnz_idx < pump.gunCnt[i]; ++lnz_idx) {
				pr_no = lpDispenser_data->fp_ln_data[i][lnz_idx].PR_Id - 1;
				if ((pr_no < 0) || (pr_no > 7)) {
					RunLog("ChangePrice PR_Id, PR_Id[%d] <ERR>", pr_no + 1);
					pr_no = 0;
					/*
					 * 在油机中配置的油枪, 但实际未使用的情况会发生此情况.
					 * 例: 油机该面配置了两条枪,但实际只接出了1#枪,那么WEB界面上不会配置2#枪的油品,
					 *     PR_ID默认为0,但是变价命令中这条枪的单价又不能为零,所以pr_no赋为0,
					 *     指定一种油品;
					 */
				}

				RunLog("--- pr_no: %d, newPrice:%02x%02x, idx: %d", pr_no, 
						pump.newPrice[pr_no][2], pump.newPrice[pr_no][3], idx);
				/*
				 * 说明: 此油机一面总共4枪4油品, 如果油机上配置了只使用 n 中油品(n<4),
				 *       那么 1-n 号枪单价一定不能为零, 不管这些枪实际上是否都有接出来使用;
				 *       另一方面, (n+1) - 4 号枪的单价则必须为零, 否则变价失败.
				 */
				// cash price
				if (lnz_idx >= nb_prods[i]) {
					// a.未配置油品的油枪(单价设置为零)
					pump.cmd[idx + 0] = 0x00;
					pump.cmd[idx + 1] = 0xff;
					pump.cmd[idx + 2] = 0x00;
					pump.cmd[idx + 3] = 0xff;
				} else if (pump.newPrice[pr_no][3] != 0x00 || pump.newPrice[pr_no][2] != 0x00) {
					// b. 已配置油品, 且需要变价的油枪
					pump.cmd[idx + 0] = pump.newPrice[pr_no][2];        // MSB First
					pump.cmd[idx + 1] = ~pump.cmd[idx + 0];
					pump.cmd[idx + 2] = pump.newPrice[pr_no][3];
					pump.cmd[idx + 3] = ~pump.cmd[idx + 2];

					tmp_pr_no = pr_no;
					error_code_msg_cnt += pump.newPrice[pr_no][0];
					price_change_flag = 1;
				} else {
					// c. 已配置油品, 但无需变价的油枪
					pump.cmd[idx + 0] = lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2];
					pump.cmd[idx + 1] = ~pump.cmd[idx + 0];
					pump.cmd[idx + 2] = lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3];
					pump.cmd[idx + 3] = ~pump.cmd[idx + 2];

				}

				// credit price
				memcpy(&pump.cmd[idx + 4], &pump.cmd[idx], 4);

				// 备份单价数据用于变价完成后读取单价进行比较 
				price_bak[lnz_idx * 4 + 0] = pump.cmd[idx + 2];
				price_bak[lnz_idx * 4 + 1] = pump.cmd[idx + 3];
				price_bak[lnz_idx * 4 + 2] = pump.cmd[idx + 0];
				price_bak[lnz_idx * 4 + 3] = pump.cmd[idx + 1];

				idx += 8;
			}

			if (price_change_flag == 0) {
				RunLog("Node[%d].FP[%d]无需变价", node, i + 1);
				break;    // 处理下一个FP
			}
			
			pump.cmdlen = idx;
			pump.wantLen = 2;


			ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd, 
					pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
			if (ret < 0) {
				RunLog("Node[%d].FP[%d]下发变价命令失败", node, i + 1);
				continue;
			}

			RunLog("Node[%d].FP[%d].ChangePrice_CMD: %s",
					node, i + 1, Asc2Hex(pump.cmd, pump.cmdlen));

			memset(pump.cmd, 0, sizeof(pump.cmd));
			pump.cmdlen = sizeof(pump.cmd) - 1;

			ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
			if (ret < 0) {
				RunLog("Node[%d].FP[%d]发变价命令成功, 取返回信息失败", node, i + 1);
				continue;
			}

			if (pump.cmd[1] != (__u8)~pump.cmd[0]) {
				RunLog("Node[%d].FP[%d]返回数据校验错误", node, i + 1);
				Ssleep(PRICE_CHANGE_RETRY_INTERVAL - 3 * INTERVAL);
				continue;
			}

			GetPumpStatus(i, &status);   // 变价之后状态会变为2F
			sleep(1);                    // 等待, 确保税控已经处理完变价命令

			ret = GetPrice(i);
			if (ret < 0) {
				RunLog("Node[%d].FP[%d]读单价失败", node, i + 1);
				continue;
			}

			if (memcmp(pump.cmd, price_bak, pump.gunCnt[i] * 4) != 0) {
				RunLog("Node[%d].FP[%d]执行变价失败", node, i + 1);
				err_code = 0x37;
				Ssleep(PRICE_CHANGE_RETRY_INTERVAL - 1000000 - (4 * INTERVAL));
				continue;
			}

			// 打印日志, 更新FM数据库
			for (lnz_idx = 0; lnz_idx < pump.gunCnt[i]; ++lnz_idx) {
				pr_no = lpDispenser_data->fp_ln_data[i][lnz_idx].PR_Id - 1;
				if ((pr_no < 0) || (pr_no > 7)) {
					RunLog("ChangePrice PR_Id, PR_Id[%d] <ERR>", pr_no + 1);
					pr_no = 0;
					continue;
				}
				if ((pump.newPrice[pr_no][2] == 0x00) && (pump.newPrice[pr_no][3]) == 0) {
					continue;
				}
			

				memcpy(&(lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price),
									&pump.newPrice[pr_no], 4);

				RunLog("Node[%d]变价成功,新单价fp_fm_data[%d][%d].Prode_Price: %s", node, pr_no,
					fm_idx, Asc2Hex(lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price, 4));

				CriticalLog("节点[%02d]面[%d]枪[%d]变价成功,新单价为%02x.%02x",
									node, i + 1, lnz_idx + 1,
					lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2],
					lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3]);

			}

			break;						
		} // end of for trycnt < TRY_TIME

		// Step.4 错误判断, 若出错则写错误数据库/置FP状态为 Inoperative
		if (trycnt >= TRY_TIME) {
			// Write Error Code: 0x36/0x37 - Price Change Error (Spare)
			if (0x37 != err_code) {
				err_code = 0x36;    // 通讯失败
				CriticalLog("节点[%02d]面[%d]枪[x]变价失败[通讯问题],改变面状态为[不可用]",
											node, i + 1);
			} else {
				CriticalLog("节点[%02d]面[%d]枪[x]变价失败[油机执行失败],改变面状态为[不可用]",
											node, i + 1);
			}

			PriceChangeErrLog("%02x.%02x.** %02x.%02x %02x",
					node, i + 1,
					pump.newPrice[tmp_pr_no][2], 
					pump.newPrice[tmp_pr_no][3], err_code);
		}

		RunLog("---------- Node[%d]FP[%d] ER_DATA, error code: %02x -----", node, i + 1, err_code);

		int tmp_idx;
		for (tmp_idx = 0; tmp_idx < error_code_msg_cnt; tmp_idx++) {
			ChangeER_DATA(node, 0x21 + i, err_code, NULL); // Translater已经回复了ACK
		}

		if (err_code != 0x99) {
			ChangeFP_State(node, 0x21 + i, FP_State_Inoperative, NULL);
		}

		Ssleep(PRICE_CHANGE_RETRY_INTERVAL - 1000000 - (4 * INTERVAL));
	} // end of for (i = 0; i < pump.panelCnt; )

	//待全部面板都执行变价后再清零
	memset(&pump.newPrice, 0, FP_PR_NB_CNT * 4);

	return 0;
}

/*
 * func: 读取某个FP所有枪的油品单价, LSB First
 */
static int GetPrice(int pos)
{
	int ret;
	int trycnt;

	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {
		pump.cmd[0] = 0xF0 | pump.panelPhy[pos];
		pump.cmd[1] = ~pump.cmd[0];     // double talk byte
		pump.cmd[2] = 0xAC;
		pump.cmd[3] = 0x53;             // ~pump.cmd[2]
		pump.cmdlen = 4;
		pump.wantLen = (pump.gunCnt[pos] * 2 * 2) + 2;

		Ssleep(INTERVAL); 
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]下发读单价命令失败", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]下发读单价命令成功, 取返回信息失败", node, pos + 1);
			continue;
		} 

		if (DoubleTalkCheck(pump.cmd, pump.cmdlen) != 0) {
			RunLog("Node[%d].FP[%d]返回数据校验错误", node, pos + 1);
			continue;
		}

		return 0;
	}

	return -1;
}

/*
* func: 加油结束，进行交易记录的读取和处理
*       零交易的判断和处理由DoEot来做, DoPoling 不关心
* ret : 0 - 成功; -1 - 失败
*
*/
static int DoEot(int pos)
{
	int i, ret;
	int pr_no;
	int err_cnt;
	__u8 price[5];
	__u8 volume[5];
	__u8 amount[5];
	__u8 pre_total[7];
	__u8 total[7];
	__u8 vol_total[NOZ_TOTAL_LEN];
	__u8 amount_total[NOZ_TOTAL_LEN];

	RunLog("节点[%d]面板[%d]处理新交易", node, pos + 1);

	err_cnt = 0;
	do {
		ret = GetGunTotal(pos, pump.gunLog[pos], vol_total, amount_total);	// 取油枪泵码
		if (ret < 0) {
			err_cnt++;
			RunLog("Node[%d].FP[%d]读取泵码失败 <ERR>", node, pos + 1);
		}
	} while (ret < 0 && err_cnt <= 5);
	
	if (ret < 0) {
		return -1;
	}

	pr_no = (lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).PR_Id - 1;  // 0 - 7	
	if ((pr_no < 0) || (pr_no > 7)) {
		RunLog("DoEot PR_Id error, PR_Id[%d] <ERR>", pr_no + 1);
		pr_no = 0;
		/*
		 * 发生了异常
		 */
	}
	
	memcpy(pre_total, (lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Vol_Total, 7);

	//vol_total
	total[0] = 0x0a; 
	total[1] = 0x00;	
	memcpy(&total[2], vol_total, NOZ_TOTAL_LEN);

	ret = memcmp(total, pre_total, sizeof(total));
	if (ret != 0) {
		err_cnt= 0;
		do {
			ret = GetDisplayData(pos);	// 取最后一笔加油记录
			if (ret != 0) {
				err_cnt++;
				RunLog("Node[%d].FP[%d]读取交易记录失败 <ERR>", node, pos + 1);
			}
		} while (ret < 0 && err_cnt <=5);

		if (ret < 0) {
			return -1;
		}

		volume[0] = 0x06;               
		volume[1] = 0x00;
		volume[2] = pump.cmd[14];
		volume[3] = pump.cmd[12];
		volume[4] = pump.cmd[10];
		
		memcpy(lpDispenser_data->fp_data[pos].Current_Volume, volume, 5);

		amount[0] = 0x06;
		amount[1] = 0x00;
		amount[2] = pump.cmd[8];
		amount[3] = pump.cmd[6];
		amount[4] = pump.cmd[4];
		
		memcpy(lpDispenser_data->fp_data[pos].Current_Amount, amount, 5);

		price[0] = 0x04;		
		price[1] = 0x00;
		price[2] = pump.cmd[2];
		price[3] = pump.cmd[0];
		
		memcpy(lpDispenser_data->fp_data[pos].Current_Unit_Price, price, 4);

		memcpy(lpDispenser_data->fp_m_data[pr_no].Meter_Total, total, 7);
		memcpy((lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Vol_Total, total, 7);

		// Update Amount_Total
		total[0] = 0x0a; 
		total[1] = 0x00;	
		memcpy(&total[2], amount_total, NOZ_TOTAL_LEN);
		
		memcpy((lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Amount_Total, total, 7);
		memcpy(lpDispenser_data->fp_data[pos].Current_Prod_Nb, lpDispenser_data->fp_pr_data[pr_no].Prod_Nb, 4);

		err_cnt = 0;
		do {
			ret = EndTr4Ln(node, pos + 0x21, pump.gunLog[pos] + 0x10);
			if (ret < 0) {
				RunLog("节点[%d]面板[%d]DoEot.EndTr4Ln失败, ret: %d <ERR>", node, pos + 1, ret);
				err_cnt++;
			}
		} while (ret < 0 && err_cnt <= 5);

		err_cnt = 0;
		do {
			ret = EndTr4Fp(node, pos + 0x21, pre_total, 
				(lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Vol_Total);
			if (ret < 0) {
				RunLog("节点[%d]面板[%d] DoEot.EndTr4Fp失败, ret: %d <ERR>", node, pos + 1, ret);
				err_cnt++;
			}
		} while (ret < 0 && err_cnt <= 8);

	} else {
		ret = EndZeroTr4Fp(node, pos + 0x21);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d] DoEot.EndZeroTr4Fp 失败, ret: %d <ERR>", node, pos + 1, ret);
		}
	}
	
	
	return 0;
}


/*
 * func: 获取该FP所有枪的泵码
 */
static int GetPumpTotal(int pos)
{
	return -1;
}

/*
 * func: 取某条枪的泵码
 */ 
static int GetGunTotal(int pos, __u8 gunLog, __u8 *vol_total, __u8 *amt_total)
{
	int i;
	int ret;

	for (i = 0; i < TRY_TIME; ++i) {
		pump.cmd[0] = 0xF0 | pump.panelPhy[pos];
		pump.cmd[1] = ~pump.cmd[0];                 // double talk byte
		pump.cmd[2] = 0xA9;
		pump.cmd[3] = 0x56;                         // ~pump.cmd[2]
		pump.cmd[4] = 0x40 | ((gunLog - 1) << 1);   // single hose,running total,cash
		pump.cmd[5] = ~pump.cmd[4];
		pump.cmdlen = 6;
		pump.wantLen = 22;

		Ssleep(INTERVAL); 
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]下发读泵码命令失败", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]下发读泵码命令成功, 取返回信息失败", node, pos + 1);
			continue;
		} 

		if (DoubleTalkCheck(pump.cmd, pump.cmdlen) != 0) {
			RunLog("Node[%d].FP[%d]返回数据校验错误", node, pos + 1);
			continue;
		}

		vol_total[0] = pump.cmd[8];
		vol_total[1] = pump.cmd[6];
		vol_total[2] = pump.cmd[4];
		vol_total[3] = pump.cmd[2];
		vol_total[4] = pump.cmd[0];

		amt_total[0] = pump.cmd[18];
		amt_total[1] = pump.cmd[16];
		amt_total[2] = pump.cmd[14];
		amt_total[3] = pump.cmd[12];
		amt_total[4] = pump.cmd[10];

		return 0;
	}

	return -1;
}


/*
 * func: 暂停加油，在此做Terminate_FP用
 *       可以取消授权
 */
static int DoStop(int pos)
{
	int ret;
	int trycnt;

	RunLog("Node %d FP %d DoStop", node, pos + 1);

	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {
		pump.cmd[0] = 0xF0 | pump.panelPhy[pos];
		pump.cmd[1] = ~pump.cmd[0];     // double talk byte
		pump.cmd[2] = 0xA3;
		pump.cmd[3] = 0x5C;             // ~pump.cmd[2]
		pump.cmdlen = 4;
		pump.wantLen = 2;

		Ssleep(INTERVAL); 
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]下发停止命令失败", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]下发停止命令成功, 取返回信息失败", node, pos + 1);
			continue;
		} 

		if (pump.cmd[1] != (__u8)~pump.cmd[0]) {
			RunLog("Node[%d].FP[%d]返回数据校验错误", node, pos + 1);
			continue;
		}

		if ((pump.cmd[0] != FP_IDLE) && (pump.cmd[0] != FP_SPFUELLING)) {
			RunLog("Node[%d].FP[%d]执行停止加油命令失败", node, pos + 1);
			continue;
		}

		return 0;
	}

	return -1;
}

/*
 * func: Release_FP
 */
static int DoAuth(int pos)
{
	int trycnt, ret;
	int pr_no, fm_idx;
	int fail_cnt = 0;

	RunLog("Node %d FP %d Do Auth", node, pos + 1);

	pr_no = (lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).PR_Id - 1;  // 0 - 7	
	if ((pr_no < 0) || (pr_no > 7)) {
		RunLog("Node[%d] Do Auth PR_Id error, PR_Id[%d] <ERR>", node, pr_no + 1);
		return -1;
	}

	fm_idx = lpDispenser_data->fp_data[pos].Default_Fuelling_Mode - 1;
	if (fm_idx < 0 || fm_idx > 7) {
		RunLog("Node[%d] Do Auth default FM error, fm[%d] <ERR>", node, fm_idx + 1);
		return -1;
	}

	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {
		pump.cmd[0] = 0xF0 | pump.panelPhy[pos];
		pump.cmd[1] = ~pump.cmd[0];     // double talk byte
		pump.cmd[2] = 0xA5;
		pump.cmd[3] = 0x5A;             // ~pump.cmd[2]
		pump.cmd[4] = 0x05;
		pump.cmd[5] = 0xFA;
		pump.cmd[6] = lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3];
		pump.cmd[7] = ~pump.cmd[6];
		pump.cmd[8] = lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2];
		pump.cmd[9] = ~pump.cmd[8];
		pump.cmd[10] = 0x00;
		pump.cmd[11] = 0xff;
		pump.cmd[12] = 0x00;
		pump.cmd[13] = 0xff;
		pump.cmd[14] = 0x00;
		pump.cmd[15] = 0xff;
		pump.cmd[16] = 0x00;
		pump.cmd[17] = 0xff;
		pump.cmd[18] = 0x00;
		pump.cmd[19] = 0xff;
		pump.cmd[20] = 0x00;
		pump.cmd[21] = 0xff;
		pump.cmdlen = 22;
		pump.wantLen = 2;

		Ssleep(INTERVAL); 
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]下发授权命令失败", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]下发授权命令成功, 取返回信息失败", node, pos + 1);
			continue;
		} 

		if (pump.cmd[1] != (__u8)~pump.cmd[0]) {
			RunLog("Node[%d].FP[%d]返回数据校验错误", node, pos + 1);
			continue;
		}
		
		if (pump.cmd[0] == FP_IDLE) {
			return -2;
		}
		
		if ((pump.cmd[0] != FP_AUTHORISED) &&(pump.cmd[0] != FP_FUELLING1)&&(pump.cmd[0] != FP_FUELLING2)) {
			RunLog("Node[%d].FP[%d]执行授权加油命令失败", node, pos + 1);
			fail_cnt++;
			if (fail_cnt >= 2) {
				fail_cnt = 0;
				ReWritePrice(pos);
			}
			continue;
		}
		

		return 0;
	}

	return -1;
}

/*
 * func: 读取并上传实时加油数据
 */
static int DoBusy(int pos)
{
	int ret;
	__u8 amount[5] = {0};
	__u8 volume[5] = {0};
	const __u8 arr_zero[3] = {0};

	ret = GetDisplayData(pos);
	if (ret < 0) {
		return -1;
	}

	volume[0] = 0x06;
	volume[1] = 0x00;
	volume[2] = pump.cmd[14];
	volume[3] = pump.cmd[12];
	volume[4] = pump.cmd[10];

	// 只有当加油量不为零了才能将FP状态改变为Fuelling
	if (fp_state[pos] != 3) {
		if (memcmp(&volume[2], arr_zero, 3) == 0) {
			return 0;
		}
		fp_state[pos] = 3;
		return 0;
	}

	amount[0] = 0x06;
	amount[1] = 0x00;
	amount[2] = pump.cmd[8];
	amount[3] = pump.cmd[6];
	amount[4] = pump.cmd[4];

	if ((__u8)FP_State_Fuelling == lpDispenser_data->fp_data[pos].FP_State) {
		ret = ChangeFP_Run_Trans(node, 0x21 + pos, amount,  volume);
	}

	return 0;
}

/*
 * func: 抬枪处理 
 * 获取当前抬起的逻辑枪号,设置fp_data[].Log_Noz_State & 改变FP状态为Calling
 */
static int DoCall(int pos)
{
	int ret;
	int trycnt;
	__u8 ln_status;

	ret = GetCurrentLNZ(pos);
	if (ret < 0) {
	//	RunLog("获取当前油枪枪号失败");
		return -1;
	}

	ret = ChangeFP_State(node, pos + 0x21, FP_State_Calling, NULL);

	//上位机在线,但是允许自动授权;上位机不在线, 授权开关置于开的状态,此时自动授权
	if ((gpIfsf_shm->auth_flag == FCC_SWITCH_AUTH) && (lock_status() == 1)) {
		ret = DoAuth(pos);
		if (ret < 0) {
			RunLog("Node %d FP %d 自动授权失败", node, pos + 1);
		}
		ret = ChangeFP_State(node, pos + 0x21, FP_State_Started, NULL);
		RunLog("Node %d FP %d 自动授权成功", node, pos + 1);
		SO_OFFLINE_TR(node, pos);
	}

	return ret;
}

/*
 * func: 读取FP标识, 用于判断油机类型
 *       连续读取两次, 取到的ID值一致才返回成功
 *
 * return: 0 -成功; -1 - 失败.
 */
static int GetPumpID(int pos, __u8 *id)
{
	int trycnt, ret;
	__u8 pre_id = 0;

	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {
		pump.cmd[0] = 0xF0 | pump.panelPhy[pos];
		pump.cmd[1] = ~pump.cmd[0];     // double talk byte
		pump.cmd[2] = 0xA0;
		pump.cmd[3] = 0x5F;             // ~pump.cmd[2]
		pump.cmdlen = 4;
		pump.wantLen = 2;

		Ssleep(INTERVAL); 
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]下发读FP标识ID失败", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]下发读FP标识ID成功, 取返回信息失败", node, pos + 1);
			continue;
		} 

		if (pump.cmd[1] != (__u8)~pump.cmd[0]) {
			RunLog("Node[%d].FP[%d]返回数据校验错误", node, pos + 1);
			continue;
		}

		if (pre_id == 0) {
			pre_id = pump.cmd[0];
			continue;
		} else if (pre_id != pump.cmd[0]) {
			return -1;
		}

		*id = pump.cmd[0];
		RunLog("Node[%d].FP[%d] ID为[%02x]", node, pos + 1, *id);

		return 0;
	}

	return -1;
}

/*
 * func: 读取FP状态
 */
static int GetPumpStatus(int pos, __u8 *status)
{
	int trycnt, ret;

	for (trycnt = 0; trycnt < 2; ++trycnt) {
		pump.cmd[0] = 0xF0 | pump.panelPhy[pos];
		pump.cmd[1] = ~pump.cmd[0];     // double talk byte
		pump.cmd[2] = 0xA2;
		pump.cmd[3] = 0x5D;             // ~pump.cmd[2]
		pump.cmdlen = 4;
		pump.wantLen = 2;

		Ssleep(INTERVAL); 
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]下发读状态命令失败", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]下发读状态命令成功, 取返回信息失败",
									node, pos + 1);
			continue;
		} 

		if (pump.cmd[1] != (__u8)~pump.cmd[0]) {
			RunLog("Node[%d].FP[%d]返回数据校验错误", node, pos + 1);
			continue;
		}

		*status = pump.cmd[0];

		if (*status == FP_INOPERATIVE) {
			ret = SetDisplayData(pos);
		} else if (*status != FP_IDLE) {
			ack_deactived_hose_flag[pos] = 1;
		}

		return 0;
	}

	return -1;
}

/*
 * func: 读取FP显示数据(实时数据&交易)
 *       交易处理时相当于 GetTransData
 */
static int GetDisplayData(int pos)
{
	int trycnt, ret;

	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {
		pump.cmd[0] = 0xF0 | pump.panelPhy[pos];
		pump.cmd[1] = ~pump.cmd[0];     // double talk byte
		pump.cmd[2] = 0xA1;
		pump.cmd[3] = 0x5E;             // ~pump.cmd[2]
		pump.cmdlen = 4;
		pump.wantLen = 18;

		Ssleep(INTERVAL); 
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]下发读FP显示数据失败", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]下发读FP显示数据成功, 取返回信息失败", node, pos + 1);
			continue;
		} 

		if (DoubleTalkCheck(pump.cmd, pump.cmdlen) != 0) {
			RunLog("Node[%d].FP[%d]返回数据校验错误", node, pos + 1);
			continue;
		}

#if 0
		/*
		 * UDC协议要求2F状态要设置最近一笔交易的数据到主显, 但按个人理解,
		 * 油机离线以及变价之后主显示清零较好, 
		 * 因为离线期间若加油那么交易值不可知, 
		 * 而变价后单价与交易数据可能不匹配,容易造成客户误解,
	 	 * 所以这里不更新display_data, Guojie Zhu @ 2009.06.15
		 *
		 * SetDisplayData只是用全零设置主显.
		 * 变价后油机上查当次交易也会显示全零.
		 *
		 */
		// 缓存当次交易, 用于初始化时设置主显
		/*
		if (pump.cmd[16] == FP_IDLE) {
			memcpy(&display_data[pos], pump.cmd, 16);
		}
		*/
#endif

		return 0;
	}

	return -1;
}

/*
 * func: 设置FP主显数据
 */
static int SetDisplayData(int pos)
{
	int trycnt, ret;

	RunLog("Node[%d].FP[%d]设置主显示数据", node, pos + 1);

	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {
		pump.cmd[0] = 0xF0 | pump.panelPhy[pos];
		pump.cmd[1] = ~pump.cmd[0];     // double talk byte
		pump.cmd[2] = 0xA6;
		pump.cmd[3] = 0x59;             // ~pump.cmd[2]

		memcpy(&pump.cmd[4], display_data[pos], 16);
		pump.cmdlen = 20;
		pump.wantLen = 2;

		Ssleep(INTERVAL); 
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]下发设置主显示数据失败", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]下发设置主显示数据成功, 取返回信息失败", node, pos + 1);
			continue;
		} 

		if (pump.cmd[1] != (__u8)~pump.cmd[0]) {
			RunLog("Node[%d].FP[%d]返回数据校验错误", node, pos + 1);
			continue;
		}

		if (pump.cmd[0] == FP_INOPERATIVE) {
			continue;
		}

		return 0;
	}

	return -1;
}

//test wether the dispenser is online, we only use fp 1 to test.
/*
 * func: 判断Dispenser是否在线
 *       此模块使用GetDisplayData替代
 * static int GetPumpOnline(int pos);
 */


/*
 * func: 获取当前抬起油枪的枪号, 结果写入pump.gunLog[] & fp_data[].Log_Noz_State
 * ret : 0 - 成功; -1 - 失败 ; -2 - 未选择油枪
 */
static int GetCurrentLNZ(int pos)
{
	int ret, trycnt;

	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {
		pump.cmd[0] = 0xC0 | pump.panelPhy[pos];
		pump.cmd[1] = ~pump.cmd[0];     // double talk byte
		pump.cmd[2] = 0xA1;
		pump.cmd[3] = 0x5E;             // ~pump.cmd[2]
		pump.cmdlen = 4;
		pump.wantLen = 2;

		Ssleep(INTERVAL); 
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]下发取枪号命令失败", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]下发取枪号命令成功, 取返回信息失败", node, pos + 1);
			continue;
		} 

		if (pump.cmd[1] != (__u8)~pump.cmd[0]) {
			RunLog("Node[%d].FP[%d]返回数据校验错误", node, pos + 1);
			continue;
		}

		pump.gunLog[pos] = (pump.cmd[0] & 0x7F);  // 当前抬起的油枪	
		if (pump.gunLog[pos] == 0) {
			RunLog("Node[%d].FP[%d]未选择油枪", node, pos + 1);
			return -2;
		}

		RunLog("Node %d FP %d 的 %d 号枪抬起", node, pos + 1, pump.gunLog[pos]);
		lpDispenser_data->fp_data[pos].Log_Noz_State = (1 << (pump.gunLog[pos] - 1));

		return 0;
	}

	return -1;
}


/*
 * 初始化FP的当前油价. 从配置文件中读取单价数据,
 * 写入 fp_data[].Current_Unit_Price
 * 假设 PR_Id 与 PR_Nb 是一一对应的
 */
static int InitCurrentPrice() 
{
	int i, j;
	int pr_no, fm_idx;
	int ret;

	for (i = 0; i < pump.panelCnt; i++) {
		ret = GetPrice(i);
		if (ret < 0) {
			return -1;
		}

		nb_prods[i] = pump.cmd[pump.cmdlen - 2];
		RunLog("Node[%d].FP[%d]油品数量为%d", node, i + 1, nb_prods[i]);

		fm_idx = lpDispenser_data->fp_data[i].Default_Fuelling_Mode - 1;
		if (fm_idx < 0 || fm_idx > 7) {
			RunLog("Node[%d] default FM error, fm[%d] <ERR>", node, fm_idx + 1);
			return -1;
		}

		//for (j = 0; j < pump.gunCnt[i]; j++) {
		for (j = 0; j < nb_prods[i]; j++) {
			pr_no = (lpDispenser_data->fp_ln_data[i][j]).PR_Id - 1;  // 0-7	
			if ((pr_no < 0) || (pr_no > 7)) {
				RunLog("InitPumpPrice PR_Id error, PR_Id[%d]", pr_no + 1);
				pr_no = 0;
				continue;
			}

			// 因变价需要使用fp_fm_data中的现有数据, 所以必须保证fp_fm_data中数据正确
			if ((pump.cmd[j * 4 + 2] != 
				lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2]) ||
			     (pump.cmd[j * 4 + 0] != 
				lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3])) {
				lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2] = pump.cmd[j * 4 + 2];
				lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3] = pump.cmd[j * 4 + 0];
				RunLog("注意! Node[%d].FP[%d]Noz[%d]的油品单价配置与实际不匹配", node, i + 1, j + 1);
			}

			memcpy(lpDispenser_data->fp_data[i].Current_Unit_Price, 
					lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price, 4);
#ifdef DEBUG
		RunLog("Init.FP %d LNZ %d PR_Id %d,Prod_Price: %02x%02x%02x%02x", i + 1, j + 1, pr_no + 1,
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
	int pr_no;
	__u8 total[7] = {0};
	__u8 vol_total[NOZ_TOTAL_LEN] = {0};
	__u8 amt_total[NOZ_TOTAL_LEN] = {0};

	for (i = 0; i < pump.panelCnt; i++) {
		for (j = 0; j < pump.gunCnt[i]; ++j) {
			ret = GetGunTotal(i, j + 1, vol_total, amt_total);
			if (ret < 0) {
				RunLog("取 Node %d FP %d LNZ %d 的泵码失败", node, i, j);
				return -1;
			}

			pr_no = (lpDispenser_data->fp_ln_data[i][j]).PR_Id - 1;  // 0-7	
			if ((pr_no < 0) || (pr_no > 7)) {
				RunLog("InitPumpTotal pr_no error, FP %d, LNZ %d, PR_Id %d", i, j + 1, pr_no + 1);
				pr_no = 0;
				continue;
			}
			
			// Volume_Total
			total[0] = 0x0a;
			total[1] = 0x00;
			memcpy(&total[2], vol_total, NOZ_TOTAL_LEN);
			memcpy(lpDispenser_data->fp_m_data[pr_no].Meter_Total, total, 7);
			memcpy((lpDispenser_data->fp_ln_data[i][j]).Log_Noz_Vol_Total, total, 7);

			RunLog("Init.fp_ln_data[%d][%d].Vol_Total: %s ", i, j,
					Asc2Hex(lpDispenser_data->fp_ln_data[i][j].Log_Noz_Vol_Total, 7));
			RunLog("Init.fp_m_data[%d].Meter_Total[%s] ", pr_no, Asc2Hex(total, 7));
			RunLog("节点[%d]面[%d]逻辑枪号[%d]的泵码为[%s]", node, i + 1, 
				j + 1, Asc2Hex(vol_total, NOZ_TOTAL_LEN));
			
			// Amount_Total
			total[0] = 0x0a;
			total[1] = 0x00;
			memcpy(&total[2], amt_total, NOZ_TOTAL_LEN);
			memcpy((lpDispenser_data->fp_ln_data[i][j]).Log_Noz_Amount_Total, total, 7);
			RunLog("Init.fp_ln_data[%d][%d].Amount_Total: %s ", i, j,
				Asc2Hex(lpDispenser_data->fp_ln_data[i][j].Log_Noz_Amount_Total, 7));
		}
	}


	return 0;
}

/*
 * func: 检测等待油机处于空闲状态, 以进行后续处理
 * return: -1 - 失败, 需要重试; 0 - 成功, 节点可以上线
 */
static int Wait4PumpReady() 
{
	int i, ret;
	__u8 status = 0;

	// 1. 等待所有FP状态为空闲
	do {
		for (i = 0; i < pump.panelCnt; ++i) {
			ret = GetPumpStatus(i, &status);
			if (ret < 0) {
				break;
			}

			if (status != FP_IDLE) {
				ret = -1;
				break;
			}
		}
		// 间隔1s再重试
		sleep(1);
	} while (ret < 0);

	// 2. 发送挂枪确认
	for (i = 0; i < pump.panelCnt; ++i) {
		ret = AckDeactivedHose(i);
		if (ret < 0) {
			return -1;
		}
	}

	// 3. 读取最后一笔交易
	for (i = 0; i < pump.panelCnt; ++i) {
		ret = GetDisplayData(i);
		if (ret < 0) {
			return -1;
		}
	}

	// 4. 同步设置单价 (加油机断电重启等情况下, 需要重发单价才能授权成功)
	for (i = 0; i < pump.panelCnt; i++) {
		ret = ReWritePrice(i);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]发送初始化单价失败", node, i + 1);
			return -1;
		}
	}

	return 0;
}


/*
 * func: 挂枪确认 - 告诉油机确认收到挂枪信息
 */
static int AckDeactivedHose(int pos)
{
	int ret;
	int trycnt;

	RunLog("Node[%d].FP[%d]发送挂枪确认", node, pos + 1);

	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {
		pump.cmd[0] = 0xC0 | pump.panelPhy[pos];
		pump.cmd[1] = ~pump.cmd[0];     // double talk byte
		pump.cmd[2] = 0xA2;
		pump.cmd[3] = 0x5D;             // ~pump.cmd[2]
		pump.cmdlen = 4;
		pump.wantLen = 2;

		Ssleep(INTERVAL); 
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]下发挂枪确认命令失败", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]下发挂枪确认命令成功, 取返回信息失败", node, pos + 1);
			continue;
		} 

		if (pump.cmd[1] != (__u8)~pump.cmd[0]) {
			RunLog("Node[%d].FP[%d]返回数据校验错误", node, pos + 1);
			continue;
		}
		
		if (pump.cmd[0] != 0xB0) {
			continue;
		}

		return 0;
	}

	return -1;
}


/*
 * func: 重写单价--初始化时使用,
 *       有时意外导致加油机授权总是执行失败通过重发单价可以解决
 */
static int ReWritePrice(int pos)
{
	int ret, cmd_len;
	int idx1, idx2, lnz;
	int trycnt;
	__u8 status = 0;
	__u8 buf[64] = {0};

	RunLog("Node[%d].FP[%d]发送初始化单价", node, pos + 1);

	ret = GetPrice(pos);
	if (ret < 0) {
		return -1;
	}

	cmd_len = (pump.cmdlen - 2) * 2;
	for (lnz = 0; lnz < (pump.cmdlen - 2) / 4; lnz++) {
		idx1 = lnz * 8;
		idx2 = lnz * 4;

		// cash price
		buf[idx1 + 0] = pump.cmd[idx2 + 2];
		buf[idx1 + 1] = pump.cmd[idx2 + 3];
		buf[idx1 + 2] = pump.cmd[idx2 + 0];
		buf[idx1 + 3] = pump.cmd[idx2 + 1];

		// credit price
		buf[idx1 + 4] = buf[idx1 + 0];
		buf[idx1 + 5] = buf[idx1 + 1];
		buf[idx1 + 6] = buf[idx1 + 2];
		buf[idx1 + 7] = buf[idx1 + 3];
	}

	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {
		GetPumpStatus(pos, &status);              

		pump.cmd[0] = 0xC0 | pump.panelPhy[pos];
		pump.cmd[1] = ~pump.cmd[0];     // double talk byte
		pump.cmd[2] = 0xA3;
		pump.cmd[3] = 0x5C;             // ~pump.cmd[2]
		memcpy(&pump.cmd[4], buf, cmd_len);
		pump.cmdlen = 4 + cmd_len;
		pump.wantLen = 2;

		Ssleep(INTERVAL * 2); 
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]初始化单价数据失败", node, pos + 1);
			continue;
		}

		RunLog("Node[%d].FP[%d].PriceData: %s",
				node, pos + 1, Asc2Hex(pump.cmd, pump.cmdlen));

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]初始化单价数据成功, 取返回信息失败", node, pos + 1);
			continue;
		} 

		if (pump.cmd[1] != (__u8)~pump.cmd[0]) {
			RunLog("Node[%d].FP[%d]返回数据校验错误", node, pos + 1);
			continue;
		}
		
		if (pump.cmd[0] != 0xB0) {
			continue;
		}

		return 0;
	}

	return -1;
}

/*
 * func: 离线处理--复位全局标记/数据, 记录离线时间
 */

static int DoOffline()
{
	int j, tmp;

	// Step.1
	memset(fp_state, 0, sizeof(fp_state));
	memset(dostop_flag, 0, sizeof(dostop_flag));
	memset(pump.addInf, 0, sizeof(pump.addInf));

	// Step.2
	/*
	 * 主显设置数据恢复为全零, 因为油机离线期间可能加过油,
	 * 如果仍然设置为之前保存的加油数据会不准确
	 */
	for (j = 0; j < pump.panelCnt; ++j) {
		tmp = 0;
		do {
			display_data[j][tmp] = 0x00;
			display_data[j][tmp + 1] = 0xFF;
			tmp += 2;
		} while (tmp < sizeof(display_data[0]));
	}

	// Step.3
//	gpIfsf_shm->node_online[IFSFpos] = 0;
	ChangeNode_Status(node, NODE_OFFLINE);
	pump.time_offline = (3 * gpIfsf_shm->fcc_com_sv_data.Heartbeat_Interval) + time(NULL);
	RunLog("====== Node %d 离线! =============================", node);

	// Step.4
	for (j = 0; j < pump.panelCnt; ++j) {
		lpDispenser_data->fp_data[j].FP_State = FP_State_Inoperative;
		lpDispenser_data->fp_data[j].Log_Noz_State = 0;
	}

	return 0;
}

/*
 * func: 油机恢复联线的处理--检查油机状态是否Ready,
 *                           初始化总累, 初始化FP状态, 设置节点上线
 * return: -1 - 执行失败, 0 - 执行成功.
 */

static int DoOnline()
{
	int j, ret;
				
	// Step.1
	ret = Wait4PumpReady();
	if (ret < 0) {
		RunLog("Node[%d]状态未准备好, 重试", node);
		return -1;
	}

	// Step.2
	ret = InitPumpTotal();   // 更新泵码到数据库
	if (ret < 0) {
		RunLog("Node[%d]更新泵码到数据库失败", node);
		return -1;
	}

	// Step.3
	for (j = 0; j < pump.panelCnt; ++j) {
		lpDispenser_data->fp_data[j].FP_State = FP_State_Closed;
	}

//	gpIfsf_shm->node_online[IFSFpos] = node;
	ChangeNode_Status(node, NODE_ONLINE);
	RunLog("====== Node %d 恢复联线! =========================", node);
	keep_in_touch_xs(12);   // 确保上位机收到心跳后再上传状态

	// Step.4               // 主动上传当前FP状态
	for (j = 0; j < pump.panelCnt; ++j) {
		ChangeFP_State(node, j + 0x21, FP_State_Closed, NULL);
	}

	return 0;
}

/*
 * func: 与油机保持通讯 x秒, 了解在等待期间油机的状态, 对于某些油机还有保持主控模式的作用
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
		Ssleep(300000);
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
static int SetPort019()
{
	int ret;

	pump.cmd[0] = 0x1C;
	pump.cmd[1] = 0x52;
	pump.cmd[2] = 0x30 | (pump.chnPhy - 1);
	pump.cmd[3] = 0x03;	                          //波特率 9600
	pump.cmd[4] = 0x00;
	pump.cmd[5] = 0x00;
	pump.cmd[6] = 0x00;
	pump.cmdlen = 7;
	pump.wantLen = 0;

	ret = WriteToPort(pump.fd, pump.chnPhy, pump.chnLog, pump.cmd,
			pump.cmdlen, 0, pump.wantLen, TIME_OUT_WRITE_PORT);
	if (ret < 0) {			
		return -1;
	}

	Ssleep(INTERVAL);

	return 0;
}


/*
 * func:  通讯不通时轮询过程中隔一段时间重设一下串口属性
 *        避免因意外的电压不稳导致单片机复位--串口属性设置恢复预设9600,8,n,1
 */
static int ReDoSetPort019()
{
#define RE_DO_FREQUENCY   10         //  10次轮询失败设置一下串口属性

	int static polling_fail_cnt = 0;
	int ret;

	if ((++polling_fail_cnt) >= RE_DO_FREQUENCY) {
		ret = SetPort019();
		if (ret < 0) {
			RunLog("节点 %d 重设串口属性失败", node);
		}
		polling_fail_cnt = 0;
	}

	return ret;
}

/*
 * func: DoubleTalk 校验
 * return: 0 - 成功; -1 - 失败
 */
static int DoubleTalkCheck(const __u8 *buf, int len)
{
	int i;

	if ((len % 2) != 0) {
		return -2;
	}

	for (i = len - 1; i >= 0; i -= 2) {
		if (buf[i - 1] != (__u8)~buf[i]) {
			return -1;
		}
	}

	return 0;
}

/*
 * 检查FP上各枪的油品单价是否符合正确(符合实际), 如果全部正确则改变状态为Closed
 */
static int CheckPriceErr(int pos)
{
	int ret;
	int lnz, pr_no;
	int fm_idx;
	__u8 price_tmp[64] = {0};

	if ((__u8)FP_State_Inoperative != lpDispenser_data->fp_data[pos].FP_State) {
		return -1;
	}

	fm_idx = lpDispenser_data->fp_data[pos].Default_Fuelling_Mode - 1;
	if ((fm_idx < 0) || (fm_idx > 7)) {
		RunLog("CheckPrice ERR Default FM error, Node[%d]FP[%d] FM[%d] <ERR>",
							node, pos + 1, fm_idx + 1);
		return -1;
	}

	for (lnz = 0; lnz < pump.gunCnt[pos]; ++lnz) {
		/*
		 * 说明: 此油机一面总共4枪4油品, 如果油机上配置了只使用 n 中油品(n<4),
		 *       那么 1-n 号枪单价一定不能为零, 不管这些枪实际上是否都有接出来使用;
		 *       另一方面, (n+1) - 4 号枪的单价则必须为零, 否则变价失败.
		 */
		if (lnz < nb_prods[pos]) {
			// a. 已配置油品
			pr_no = lpDispenser_data->fp_ln_data[pos][lnz].PR_Id - 1;
			if ((pr_no < 0) || (pr_no > 7)) {
				RunLog("Node[%d] CheckPrice PR_Id, PR_Id[%d] <ERR>", node, pr_no + 1);
				pr_no = 0;
			}

			price_tmp[(lnz << 2) + 0] = lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3];
			price_tmp[(lnz << 2) + 1] = ~pump.cmd[(lnz << 2) + 0];
			price_tmp[(lnz << 2) + 2] = lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2];
			price_tmp[(lnz << 2) + 3] = ~pump.cmd[(lnz << 2) + 2];
		} else {
			// b.未配置油品的油枪(单价设置为零)
			price_tmp[(lnz << 2) + 0] = 0x00;
			price_tmp[(lnz << 2) + 1] = 0xff;
			price_tmp[(lnz << 2) + 2] = 0x00;
			price_tmp[(lnz << 2) + 3] = 0xff;
		}
	}
	
	ret = GetPrice(pos);
	if (ret < 0){
		RunLog("Node[%d].FP[%d]读单价失败", node, pos + 1);
		return -1;
	}

	for (lnz = 0; lnz < pump.gunCnt[pos]; ++lnz) {
		pr_no = lpDispenser_data->fp_ln_data[pos][lnz].PR_Id - 1;

		RunLog("Node %d FP %d LNZ %d 当前单价: %02x.%02x; 目标单价: %02x.%02x",
			node, pos + 1,lnz + 1, pump.cmd[(lnz << 2) + 2], pump.cmd[(lnz << 2) + 0],	
			lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2], 
			lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3]);
	}
		
	if (memcmp(pump.cmd, price_tmp, (pump.gunCnt[pos] << 2)) != 0) {
		return -2;
	}

	ChangeFP_State(node, 0x21 + pos, FP_State_Closed, NULL);
	RunLog("Node[%d]FP[%d]单价已变更正确, 切换状态为Closed", node, pos + 1);

	return 0;
}


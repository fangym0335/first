/*
 * Filename: lischn008.c
 *
 * Description: 加油机协议转换模块 - 恒山自助加油机, CL, 9600,N,8,1 
 *
 * History:
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

#define GUN_EXTEND_LEN  4

#define TRY_TIME	5
#define INTERVAL	100000  // 100ms , 视油机而定

#define FP_IDLE        0x00
#define FP_CALLING     0x01
#define FP_AUTHORISED  0x02
#define FP_FUELLING    0x03
#define FP_SPFUELLING  0x04    // 加油结束/达到最大值, 未保存交易
#define FP_EOT         0x05    // 交易结束
#define FP_LOCKED      0xFF    // 锁枪


static int DoCmd();
static void DoPolling();
static int DoAuth(int pos);
static int DoBusy(int pos);
static int DoCall(int pos);
static int DoEot(int pos);
static int InitGunLog();
static int DoOpen(int pos, __u8 *msgbuffer);
static int DoClose(int pos, __u8 *msgbuffer);
static int DoRation(int pos);
static int DoStop(int pos);
static int DoStarted(int i);
static int DoMode();
static int DoPrice();
static int ChangePrice();
static int GetTransData(int pos);
static int GetPumpStatus(int pos, __u8 *status);
static int GetPumpOnline(int pos, __u8 *status);
static int GetPrice(int pos);
static int GetPumpTotal(int pos);
static int InitPumpTotal();
static int InitCurrentPrice();
static inline int GetCurrentLNZ(int pos);
static int GetPumpInfo(int pos);
static int Ack4GetTrans(int pos);
static int clr_history_trans(int pos);
static int keep_in_touch_xs(int x_seconds);
static int SetPort008();
static int CheckPriceErr(int pos);

static PUMP pump;
static int node;
static int IFSFpos;
static unsigned short int crc;
static __u8 current_ttc[MAX_FP_CNT][4];
static DISPENSER_DATA *lpDispenser_data;
static NODE_CHANNEL *lpNode_channel;
static __u8 frame_no[MAX_FP_CNT] = {0};
static int dostop_flag[MAX_FP_CNT];
static int fp_state[MAX_FP_CNT];   // 0 - IDLE, 1 - CALLING , 2 - 已完成DoStarted, 3 - FUELLING

int ListenChn008(int chnLog)
{
	long	indtyp;
	int	ret;
	int	fd;
	int	j;
	int	i;
	__u8 status;
	
	sleep(5);   //防止此进程退出,而主进程没有启动完毕. 每个lischn008都需要
	RunLog("Listen Channel  008 ,logic channel[%d]", chnLog);
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
						pump.panelPhy[j] = gpIfsf_shm->gun_config[idx].phyfp;
						break;
					}
				}
				pump.gunCnt[j] =  gpIfsf_shm->node_channel[i].cntOfFpNoz[j];
			}
				
			break;
		}
	}
	bzero(&pump.newPrice, FP_PR_NB_CNT * 4);
	pump.indtyp = chnLog;
	
#if DEBUG
//	show_dispenser(node); 
//	show_pump(&pump);
#endif

	// ============= Part.2 打开 & 设置串口属性 -- 波特率/校验位等 ===================

	strcpy(pump.cmd, alltty[pump.port - 1]);
	pump.fd = OpenSerial(pump.cmd);
	if (pump.fd < 0) {
		RunLog("打开串口[%s]失败", pump.cmd);
		return -1;
	}

	ret = SetPort008();
	if (ret < 0) {
		RunLog("设置串口属性失败");
		return -1;
	}

	Ack4GetTrans(0);
	// ============== Part.3 判断加油机是否在线 ====================================
	// Check wether dispenser is online, if it's not online, wait  here.
	while (1) {
		ret = GetPumpOnline(0, &status);
		if (ret == 0) {
			break;
		}

		gpIfsf_shm->node_online[IFSFpos] = 0; //不在线,不产生心跳.
		UserLog("Node %d 不在线", node);
		
		sleep(1);
	}
	
	// ============= Part.4 初始化与加油机当前状态相关的数据库相关项 =======================
	
	for (i = 0; i < pump.panelCnt; ++i) {
		ret = GetPumpInfo(i);
	}

	// 等待状态
	for (i = 0; i < pump.panelCnt; ++i) {
		ret = GetPumpStatus(i, &status);
		if (ret < 0) {
			return -1;
		} else if (FP_AUTHORISED == status || FP_FUELLING == status) {
			return -1;
		}
	}

	ret = InitGunLog();    // Note.InitGunLog 并不是每种品牌油机都必须,长吉油机预置加油需要
	if (ret < 0) {
		RunLog("初始化默认当前逻辑枪号失败");
		return -1;
	}

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

//	gpIfsf_shm->node_online[IFSFpos] = node;  // 一切就绪, 上线
	ChangeNode_Status(node, NODE_ONLINE);
	RunLog("节点 %d 在线,状态[%02x]!", node, status);
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
			Ssleep(200000);  // 200ms, 根据具体情况调整
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
			pos = 255;
		} else if ((fp_id >= 0x21) && (fp_id <= 0x24)) {
			pos = fp_id - 0x21;
		} else {
			if (pump.cmd[0] != 0x18 && pump.cmd[0] != 0x19) {  // DoPrice & DoMode不需要处理面板号
				RunLog("非法的面板地址");
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
				dostop_flag[pos] = 1;
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
				lpDispenser_data->fp_data[pos].Log_Noz_Mask = 0xFF;
				bzero(&lpDispenser_data->fp_data[pos].Remote_Volume_Preset, 5);
				bzero(&lpDispenser_data->fp_data[pos].Remote_Amount_Prepay, 5);
				break;
			}
			ret = DoRation(pos);
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
			RunLog("接受到未定义命令[0x%02x]", pump.cmd[0]);
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
					continue;
				}

				memset(dostop_flag, 0, sizeof(dostop_flag));
				memset(fp_state, 0, sizeof(fp_state));

//				gpIfsf_shm->node_online[IFSFpos] = 0;
				ChangeNode_Status(node, NODE_OFFLINE);
				pump.time_offline = (3 * gpIfsf_shm->fcc_com_sv_data.Heartbeat_Interval) + time(NULL);
				RunLog("====== Node %d 离线! =============================", node);

				for (j = 0; j < pump.panelCnt; ++j) {
					lpDispenser_data->fp_data[j].FP_State = FP_State_Inoperative;
					lpDispenser_data->fp_data[j].Log_Noz_State = 0;
				}
			}
			continue;
		} else {
			if (gpIfsf_shm->node_online[IFSFpos] == 0) {
				if ((--fail_cnt) > 0) { // 排除干扰/双线电流环数据采集Bug造成的影响
					fail_cnt = 1;
					continue;
				} else if (time(NULL) < pump.time_offline) {
					RunLog("====================== wait for 30s, ==============");
					continue;
				}

				// fail_cnt复位
				fail_cnt = 0;

				ret = InitPumpTotal();    // 更新泵码到数据库
				if (ret < 0) {
					RunLog("更新泵码到数据库失败");
					fail_cnt = 1;     // Note.恢复过程中任何一个步骤失败都要重试
					continue;
				}

				for (j = 0; j < pump.panelCnt; ++j) {
					lpDispenser_data->fp_data[j].FP_State = FP_State_Closed;
				}

				// 一起都准备好了, 上线
//				gpIfsf_shm->node_online[IFSFpos] = node;
				ChangeNode_Status(node, NODE_ONLINE);
				RunLog("====== Node %d 恢复联线! =========================", node);
				keep_in_touch_xs(12);               // 确保CD已经收到心跳之后再做后续操作
				// 主动上传当前FP状态
				for (j = 0; j < pump.panelCnt; ++j) {
					ChangeFP_State(node, j + 0x21, FP_State_Closed, NULL);
				}
				continue;
			}
			
			fail_cnt = 0;
		}

		// 仅在状态字有变化的时候才打印(写日志)
		if (status != pre_status[i]) {
			pre_status[i] = status;
			RunLog("节点[%d]面板[%d]当前状态字为[%02x]", node, i + 1, status);
		}

		if (gpIfsf_shm->auth_flag != FCC_SWITCH_AUTH) {  //如果有后台
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

		switch (status) {
		case FP_IDLE:                                   // 挂枪、空闲
		case FP_EOT:
		case FP_LOCKED:
			dostop_flag[i] = 0;
			if (fp_state[i] >= 2) {
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

			// 脱机交易处理
			EO_OFFLINE_TR(node, i);
			if (gpIfsf_shm->fp_offline_flag[node - 1][i] != 0) {
				upload_offline_tr(node, NULL);
			}

			// 若状态不是IDLE, 则切换为Idle
			if (((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State )) {
				ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
			}

			ret = ChangePrice();                    // 查看是否有油品需要变价,有则执行
			break;
		case FP_CALLING:                                // 抬枪
			if (dostop_flag[i] == 1) {
				break;
			}
			if ((__u8)FP_State_Calling > lpDispenser_data->fp_data[i].FP_State) {
				ret = DoCall(i);
				fp_state[i] = 1;

				ret = DoStarted(i);
			}

			break;
		case FP_AUTHORISED:
			if ((__u8)FP_State_Authorised != lpDispenser_data->fp_data[i].FP_State) {
				ret = ChangeFP_State(node, i + 0x21, FP_State_Authorised, NULL);
			}
			break;
		case FP_FUELLING:                               // 加油
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

			if ((__u8)FP_State_Fuelling != lpDispenser_data->fp_data[i].FP_State) {
				if (fp_state[i] == 3) {
					ret = ChangeFP_State(node, i + 0x21, FP_State_Fuelling, NULL);
				}
			}
			break;
		case FP_SPFUELLING:
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
	int idx, trycnt;
	int ration_type;
	__u8 tmp[3] = {0};
	__u8 pre_volume[3] = {0};

	if (pump.cmd[0] == 0x3D) {   // Preset
		ration_type = 0x01;
		memcpy(pre_volume, &lpDispenser_data->fp_data[pos].Remote_Volume_Preset[2], 3);
	} else {
		ration_type = 0x00;  // Prepay
		memcpy(pre_volume, &lpDispenser_data->fp_data[pos].Remote_Amount_Prepay[2], 3);
	}
		
	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {
		frame_no[pos] = (++frame_no[pos]) % 17;
		pump.cmd[0] = 0xFA;
		pump.cmd[1] = 0x00;
		pump.cmd[2] = pump.panelPhy[pos];
		pump.cmd[3] = frame_no[pos];
		pump.cmd[4] = 0x06;    // len
		pump.cmd[5] = 0x11;    // cmd
		pump.cmd[6] = ration_type;
		pump.cmd[7] = 0x00;
		Bcd2Hex(pre_volume, 3, tmp, 3);
		pump.cmd[8] = tmp[0];
		pump.cmd[9] = tmp[1];
		idx = 10;
		if (tmp[1] == 0xFA) {
			pump.cmd[idx++] = 0xFA;
		}
		
		pump.cmd[idx++] = tmp[2];
		if (tmp[2] == 0xFA) {
			pump.cmd[idx++] = 0xFA;
		}

		crc = CRC_16_HS(&pump.cmd[1], idx - 1);
		pump.cmd[idx++] = crc & 0x00FF;
		if (pump.cmd[idx - 1] == 0xFA) {
			pump.cmd[idx++] = 0xFA;
		}
		pump.cmd[idx++] = crc >> 8;
		if (pump.cmd[idx - 1] == 0xFA) {
			pump.cmd[idx++] = 0xFA;
		}

		pump.cmdlen = idx;
		pump.wantLen = 6;        // 变长

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog( "Node[%d]FP[%d]下发预置加油命令失败",node,pos+1 );
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x08;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x00;
		if (ret < 0) {
			RunLog( "Node[%d]FP[%d]下发预置加油命令成功, 取返回信息失败", node, pos + 1);
			continue;
		} 

		crc = CRC_16_HS(&pump.cmd[1], pump.cmdlen - 3);
		if ((__u8)(crc & 0x00FF) != pump.cmd[pump.cmdlen - 2] ||
				(__u8)(crc >> 8) != pump.cmd[pump.cmdlen - 1]) {
			RunLog( "Node[%d]FP[%d]返回数据校验错误", node, pos + 1);
			continue;
		}
		
		if (pump.cmd[6] != 0x55) {
			RunLog("Node[%d]FP[%d] 执行预置加油命令失败", node, pos + 1);
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
	if ((0x04 != pump.cmd[8]) || (0x00 != pump.cmd[9]) || pr_no > 7) {
		RunLog("变价信息错误,不能下发");
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
 */
static int ChangePrice() 
{
	int i, j, k;
	int idx, ret, fm_idx;
	int trycnt;
	__u8 pr_no;
	__u8 lnz, status;
	__u8 price_tmp[2] = {0};
	__u8 err_code, err_code_tmp;

/*
 * 遍历新单价数组newPrice[FP_PR_NB_CNT], 如果有油品需要变价--数组值不为零,
 * 则遍历所有FP, 如果油品匹配就下发变价命令
 */

//	RunLog(">> exec ChangePrice Now !");
	for (pr_no = 0; pr_no < FP_PR_NB_CNT; pr_no++) {
//		RunLog(">>>1 newPrice[%d]: %02x.%02x", pr_no, pump.newPrice[pr_no][2], pump.newPrice[pr_no][3]);;
		if (pump.newPrice[pr_no][2] != 0x00 || pump.newPrice[pr_no][3] != 0x00) {
			for (i = 0; i < pump.panelCnt; i++) {
				err_code = 0x99;
				for (j = 0; j < MAX_FP_LN_CNT; j++) { // 不能用gunCnt,1枪可能不用
#if DEBUG
					RunLog(">>> fp_ln_data[%d][%d].PR_Id: %d", i, j,
							lpDispenser_data->fp_ln_data[i][j].PR_Id);
#endif
					if (lpDispenser_data->fp_ln_data[i][j].PR_Id - 1 == pr_no) {
					err_code_tmp = 0x99;
				CriticalLog("下发变价命令给节点[%02d]面[%d]枪[%d]......", node, i + 1, j + 1);

					for (trycnt = 0; trycnt < TRY_TIME; trycnt++) {
						frame_no[i] = frame_no[i] % 17;
						// Step.1 发变价命令
						pump.cmd[0] = 0xFA;
						pump.cmd[1] = 0x00;
						pump.cmd[2] = pump.panelPhy[i];
						pump.cmd[3] = frame_no[i];
						pump.cmd[4] = 0x06;    // len
						pump.cmd[5] = 0x13;    // cmd
						pump.cmd[6] = lpDispenser_data->fp_ln_data[i][j].Physical_Noz_Id;

						Bcd2Hex(&pump.newPrice[pr_no][2], 2, price_tmp, 2);
						idx = 7;
						pump.cmd[idx++] = price_tmp[0];  // unit_price
						if (price_tmp[0] == 0xFA) {
							pump.cmd[idx++] = 0xFA;
						}
						pump.cmd[idx++] = price_tmp[1];
						if (price_tmp[1] == 0xFA) {
							pump.cmd[idx++] = 0xFA;
						}
						pump.cmd[idx++] = 0x00;  // prod_code
						pump.cmd[idx++] = 0x00;

						crc = CRC_16_HS(&pump.cmd[1], idx - 1);
						pump.cmd[idx++] = crc & 0x00FF;
						if (pump.cmd[idx - 1] == 0xFA) {
							pump.cmd[idx++] = 0xFA;
						}
						pump.cmd[idx++] = crc >> 8;
						if (pump.cmd[idx - 1] == 0xFA) {
							pump.cmd[idx++] = 0xFA;
						}
						pump.cmdlen = idx;
						pump.wantLen = 6;      // 变长

						Ssleep(INTERVAL);
						ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
								pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
						if (ret < 0) {
							RunLog("Node[%d]FP[%d]下发变价命令失败", node, i + 1);
							continue;
						}


						RunLog("Node[%d].FP[%d].PriceChange_CMD: %s",
							node, i + 1, Asc2Hex(pump.cmd, pump.cmdlen));

						// Step.2 根据命令返回信息或者重读油机单价确认变价是否成功
						memset(pump.cmd, 0, sizeof(pump.cmd));
						pump.cmdlen = sizeof(pump.cmd) - 1;
						gpGunShm->addInf[pump.chnLog - 1][0] = 0x08;
						ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
						gpGunShm->addInf[pump.chnLog - 1][0] = 0x00;
						if (ret < 0) {
							RunLog("Node[%d]FP[%d]下发变价命令成功, 取返回信息失败",
												node, i + 1);
							continue;
						} 

						crc = CRC_16_HS(&pump.cmd[1], pump.cmdlen - 3);
						if ((__u8)(crc & 0x00FF) != pump.cmd[pump.cmdlen - 2] ||
								(__u8)(crc >> 8) != pump.cmd[pump.cmdlen - 1]) {
							RunLog( "Node[%d]FP[%d]返回数据校验错误", node, i + 1);
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
							continue;
						}

						if (pump.cmd[6] != 0x55) {
							RunLog("Node[%d]FP[%d] 执行变价命令失败", node, i + 1);
							err_code_tmp = 0x37;
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
							continue;
						}

						ret = GetPrice(i);
						if (ret < 0) {
							continue;
						}

						if (memcmp(&pump.cmd[7], &pump.newPrice[pr_no][2], 2) != 0) {
							RunLog("Node[%d]FP[%d] 执行变价命令失败", node, i + 1);
							err_code_tmp = 0x37;
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
							continue;
						}

										
						// Step.3 写新单价到油品数据库 & 清零单价缓存
						fm_idx = lpDispenser_data->fp_data[i].Default_Fuelling_Mode - 1;
						RunLog("Node[%d]发送变价命令成功,新单价fp_fm_data[%d][%d].Prode_Price: %s",
							node, pr_no, fm_idx,
							Asc2Hex(lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price, 4));

						CriticalLog("节点[%02d]面[%d]枪[%d]变价成功,新单价为%02x.%02x",
							node, i + 1, j + 1,
							lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2],
							lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3]);

						err_code_tmp = 0x99;
						break;
						
					}

					// Step.4 错误判断, 若出错则写错误数据库/置FP状态为 Inoperative
					if (trycnt >= TRY_TIME) {
						// Write Error Code: 0x36/0x37 - Price Change Error (Spare)
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
					if (err_code == 0x99) {
						// 该面有任何油枪变价失败, 则算整个FP变价失败.
						err_code = err_code_tmp;
					}
					} // end of if.PR_Id match
				//	break;  // 如果加油机能够根据指令信息处理多枪同油品的情况，那这里需要break.
				} // end of for.j < LN_CNT
				RunLog("---------- Node[%d]FP[%d] ER_DATA, error code: %02x -----", node, i + 1, err_code);

				int tmp_idx;
				for (tmp_idx = 0; tmp_idx < pump.newPrice[pr_no][0]; tmp_idx++) {
					ChangeER_DATA(node, 0x21 + i, err_code, NULL); // Translater已经回复了ACK
				}

				if (err_code != 0x99) {
					ChangeFP_State(node, 0x21 + i, FP_State_Inoperative, NULL);
				}

				Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
			} // end of for.i < panelCnt
			bzero(&pump.newPrice[pr_no], 4);
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
 */
static int DoEot(int pos)
{
	int i;
	int ret;
	int pr_no;
	int err_cnt;
	__u8 price[4];
	__u8 volume[5];
	__u8 amount[5];
	__u8 total[7] = {0};
	__u8 pre_total[7];

	RunLog("节点[%d]面板[%d]处理新的交易", node, pos + 1);

	// 1. Get Current TTC  & Volume_Total
	err_cnt = 0;
	do {
		ret = GetPumpTotal(pos);
		if (ret < 0) {
			err_cnt++;
			RunLog("节点[%d]面板[%d]读取泵码失败", node, pos + 1);
		}
	} while (err_cnt <= 5 && ret < 0);

	if (ret < 0) {
		return -1;
	}

//	memcpy(&current_ttc[pos], &pump.cmd[9], 4);       // 更新当前TTC
	RunLog("## Current TTC: %s", Asc2Hex(&current_ttc[pos][0], 4));

	total[0] = 0x0A;
	total[1] = 0x00;
	Hex2Bcd(&pump.cmd[13], GUN_EXTEND_LEN, &total[2], 5);

	pr_no = (lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).PR_Id - 1;  // 0 - 7	
	if ((pr_no < 0) || (pr_no > 7)) {
		RunLog("DoEot PR_Id error, PR_Id[%d] <ERR>", pr_no + 1);
	}
	
	memcpy(pre_total, (lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Vol_Total, 7);

//	RunLog("## Pre Total: %s", Asc2Hex(pre_total, 7));
//	RunLog("## Current Total: %s", Asc2Hex(total, 7));

	if (memcmp(pre_total, total, 7) != 0) {
		err_cnt = 0;
		do {
			ret = GetTransData(pos);
			if (ret != 0) {
				err_cnt++;
				RunLog("节点[%d]面板[%d]读取交易记录失败", node, pos + 1);
			}
		} while (err_cnt <= 5 && ret != 0);
		if (ret != 0) {
			return -1;
		}

		// Volume
		volume[0] = 0x06;
		volume[1] = 0x00;
		Hex2Bcd(&pump.cmd[24], 3, &volume[2], 3);
		memcpy(lpDispenser_data->fp_data[pos].Current_Volume, volume, 5);

		// Amount
		amount[0] = 0x06;
		amount[1] = 0x00;
		Hex2Bcd(&pump.cmd[28], 3, &amount[2], 3);
		memcpy(lpDispenser_data->fp_data[pos].Current_Amount, amount, 5);

		price[0] = 0x04;
		Hex2Bcd(&pump.cmd[12], 2, &price[1], 3);
		memcpy(lpDispenser_data->fp_data[pos].Current_Unit_Price, price, 4);

		// 更新 Volume_Total
		memcpy(lpDispenser_data->fp_m_data[pr_no].Meter_Total, total, 7);
		memcpy((lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Vol_Total, total, 7);

#if 0
		RunLog("节点[%02d]面板[%d]枪[%d] ## Amount: %02x%02x.%02x Volume: %02x%02x.%02x Price: %02x.%02x Total: %02x%02x%02x.%02x", node, pos + 1, pump.gunLog[pos], amount[2], 
					amount[3], amount[4], volume[2], volume[3], volume[4], price[2],
							price[3], total[3], total[4], total[5], total[6]);
#endif

		// 更新 Amount_Total--没有金额总累
		memset(total, 0, 7);
		total[0] = 0x0A;
		memcpy((lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Amount_Total, total, 7);
		memcpy(lpDispenser_data->fp_data[pos].Current_Prod_Nb, lpDispenser_data->fp_pr_data[pr_no].Prod_Nb, 4);

		err_cnt = 0;
		do {
			ret = EndTr4Ln(node, pos + 0x21, pump.gunLog[pos] + 0x10);
			if (ret < 0) {
				err_cnt++;
				RunLog("DoEot.EndTr4Ln 失败, ret: %d <ERR>", ret);
			}
		} while (ret < 0 && err_cnt <= 5);

		err_cnt = 0;
		do {
			ret = EndTr4Fp(node, pos + 0x21, pre_total,
				(lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Vol_Total);
			if (ret < 0) {
				err_cnt++;
				RunLog("DoEot.EndTr4Fp失败, ret: %d <ERR>", ret);
			}
		} while (ret < 0 && err_cnt <= 8);

	} else {
		ret = EndZeroTr4Fp(node, pos + 0x21);
		if (ret < 0) {
			RunLog("DoEot.EndZeroTr4Fp失败, ret: %d <ERR>", ret);
		}
	}

	return ret;
}


/*
 * func: 获取该FP所有枪的泵码
 *      pump.cmd[13] - pump.cmd[16]
 */
static int GetPumpTotal(int pos)
{
	int i, idx, ret;

	for (i = 0; i < TRY_TIME; ++i) {
		frame_no[pos] = (++frame_no[pos]) % 17;
		pump.cmd[0] = 0xFA;
		pump.cmd[1] = 0x00;
		pump.cmd[2] = pump.panelPhy[pos];
		pump.cmd[3] = frame_no[pos];
		pump.cmd[4] = 0x01;    // len
		pump.cmd[5] = 0x06;    // cmd
		crc = CRC_16_HS(&pump.cmd[1], 5);
		// pump.cmd[2] 为 0x03, pump.cmd[3] 为0x06的时候校验第二字节为FA
		idx = 6;   
		pump.cmd[idx++] = crc & 0x00FF;
		if (pump.cmd[idx - 1] == 0xFA) {
			pump.cmd[idx++] = 0xFA;
		}
		pump.cmd[idx++] = crc >> 8;
		if (pump.cmd[idx - 1] == 0xFA) {
			pump.cmd[idx++] = 0xFA;
		}
		pump.cmdlen = idx;
		pump.wantLen = 6;      // 变长,  先接收到命令字,因命令字之前不会有FA的转义

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog( "Node[%d]FP[%d]下发取泵码命令失败",node,pos+1 );
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x08;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x00;
		if (ret < 0) {
			RunLog( "Node[%d]FP[%d]下发取泵码命令成功, 取返回信息失败",node,pos+1 );
			continue;
		} 

		crc = CRC_16_HS(&pump.cmd[1], pump.cmdlen - 3);
		if ((__u8)(crc & 0x00FF) != pump.cmd[pump.cmdlen - 2] ||
				(__u8)(crc >> 8) != pump.cmd[pump.cmdlen - 1]) {
			RunLog( "Node[%d]FP[%d]返回数据校验错误", node, pos + 1);
			continue;
		}

		memcpy(&current_ttc[pos][0], &pump.cmd[9], 4);       // 更新当前TTC
		return 0;
	}

	return -1;

}

/*
 * func: 读取当次交易记录
 * return: 0 - 成功, 1 - 成功但无交易, -1 - 失败
 */ 
static int GetTransData(int pos)
{
	int i, idx, ret;

	for (i = 0; i < TRY_TIME; ++i) {
		frame_no[pos] = (++frame_no[pos]) % 17;
		pump.cmd[0] = 0xFA;
		pump.cmd[1] = 0x00;
		pump.cmd[2] = pump.panelPhy[pos];
		pump.cmd[3] = frame_no[pos];
		pump.cmd[4] = 0x05;    // len
		pump.cmd[5] = 0x03;    // cmd
		memcpy(&pump.cmd[6], &current_ttc[pos][0], 4);

		idx = 6;
		pump.cmd[idx++] = current_ttc[pos][0];
		if (current_ttc[pos][0] == 0xFA) {
			pump.cmd[idx++] = 0xFA;
		}
		pump.cmd[idx++] = current_ttc[pos][1];
		if (current_ttc[pos][1] == 0xFA) {
			pump.cmd[idx++] = 0xFA;
		}
		pump.cmd[idx++] = current_ttc[pos][2];
		if (current_ttc[pos][2] == 0xFA) {
			pump.cmd[idx++] = 0xFA;
		}
		pump.cmd[idx++] = current_ttc[pos][3];
		if (current_ttc[pos][3] == 0xFA) {
			pump.cmd[idx++] = 0xFA;
		}

		crc = CRC_16_HS(&pump.cmd[1], idx - 1);
		pump.cmd[idx++] = crc & 0x00FF;
		if (pump.cmd[idx - 1] == 0xFA) {
			pump.cmd[idx++] = 0xFA;
		}
		pump.cmd[idx++] = crc >> 8;
		if (pump.cmd[idx - 1] == 0xFA) {
			pump.cmd[idx++] = 0xFA;
		}
		pump.cmdlen = idx;
		pump.wantLen = 6;       // 变长

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]下发读交易命令失败",node,pos+1 );
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x08;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x00;
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]下发读交易命令成功, 取返回信息失败", node, pos + 1);
			continue;
		} 

		crc = CRC_16_HS(&pump.cmd[1], pump.cmdlen - 3);
		if ((__u8)(crc & 0x00FF) != pump.cmd[pump.cmdlen - 2] ||
				(__u8)(crc >> 8) != pump.cmd[pump.cmdlen - 1]) {
			RunLog("Node[%d]FP[%d]返回数据校验错误", node, pos + 1);
			continue;
		}

		if (pump.cmd[4] == 0x05) {
			RunLog("Node[%d]FP[%d]没有该序号交易", node, pos + 1);
			return 1;
		}

		// 发送交易确认
		Ack4GetTrans(pos);

		return 0;
	}

	return -1;
}


/*
 * func: 交易确认
 *      得到交易确认之后油枪状态才会由加油结束转变为空闲
 */
static int Ack4GetTrans(int pos)
{
	int i, idx, ret;
	int cmdlen , wantLen;
	__u8 cmd[32] = {0};

	for (i = 0; i < TRY_TIME; ++i) {
		frame_no[pos] = (++frame_no[pos]) % 17;
		cmd[0] = 0xFA;
		cmd[1] = 0x00;
		cmd[2] = pump.panelPhy[pos];
		cmd[3] = frame_no[pos];
		cmd[4] = 0x01;    // len
		cmd[5] = 0x14;    // cmd

		crc = CRC_16_HS(&cmd[1], 5);
		// pump.cmd[2] == 0x02, pump.cmd[3] == 0x07, 校验第一字节为 FA
		idx = 6;
		cmd[idx++] = crc & 0x00FF;
		if (cmd[idx - 1] == 0xFA) {
			cmd[idx++] = 0xFA;
		}
		cmd[idx++] = crc >> 8;
		if (cmd[idx - 1] == 0xFA) {
			cmd[idx++] = 0xFA;
		}
		cmdlen = idx;
		wantLen = 6;      // 变长

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, cmd,
			       	cmdlen, 1, wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]下发交易确认失败", node, pos + 1);
			continue;
		}

		memset(cmd, 0, sizeof(cmd));
		cmdlen = sizeof(cmd) - 1;
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x08;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, cmd, &cmdlen);
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x00;
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]下发交易确认成功, 取返回信息失败", node, pos + 1);
			continue;
		} 

		crc = CRC_16_HS(&cmd[1], cmdlen - 3);
		if ((__u8)(crc & 0x00FF) != cmd[cmdlen - 2] ||
				(__u8)(crc >> 8) != cmd[cmdlen - 1]) {
			RunLog( "Node[%d]FP[%d]返回数据校验错误", node, pos + 1);
			continue;
		}

		if (cmd[6] != 0x55) {
			RunLog( "Node[%d]FP[%d]执行交易确认失败", node, pos + 1);
			continue;
		}

		return 0;
	}

	return -1;
}


/*
 * func: Terminate_FP
 */ 
static int DoStop(int pos)
{
	int i, idx, ret;

	RunLog("Node %d FP %d Do Stop", node, pos + 1);

	for (i = 0; i < TRY_TIME; ++i) {
		frame_no[pos] = (++frame_no[pos]) % 17;
		pump.cmd[0] = 0xFA;
		pump.cmd[1] = 0x00;
		pump.cmd[2] = pump.panelPhy[pos];
		pump.cmd[3] = frame_no[pos];
		pump.cmd[4] = 0x01;    // len
		pump.cmd[5] = 0x12;    // cmd

		crc = CRC_16_HS(&pump.cmd[1], 5);
		// pump.cmd[2] == 0x02, pump.cmd[3] == 0x0f, 校验第二字节为 FA
		idx = 6;
		pump.cmd[idx++] = crc & 0x00FF;
		if (pump.cmd[idx - 1] == 0xFA) {
			pump.cmd[idx++] = 0xFA;
		}
		pump.cmd[idx++] = crc >> 8;
		if (pump.cmd[idx - 1] == 0xFA) {
			pump.cmd[idx++] = 0xFA;
		}
		pump.cmdlen = idx;
		pump.wantLen = 6;      // 变长

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog( "Node[%d]FP[%d]下发停机命令失败",node,pos+1 );
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x08;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x00;
		if (ret < 0) {
			RunLog( "Node[%d]FP[%d]下发停机命令成功, 取返回信息失败", node, pos + 1);
			continue;
		} 

		crc = CRC_16_HS(&pump.cmd[1], pump.cmdlen - 3);
		if ((__u8)(crc & 0x00FF) != pump.cmd[pump.cmdlen - 2] ||
				(__u8)(crc >> 8) != pump.cmd[pump.cmdlen - 1]) {
			RunLog( "Node[%d]FP[%d]返回数据校验错误", node, pos + 1);
			continue;
		}

		if (pump.cmd[6] != 0x55) {
			RunLog( "Node[%d]FP[%d] 执行停机命令失败", node, pos + 1);
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
	int i, idx, ret;

	RunLog("Node %d FP %d Do Auth", node, pos + 1);
		
	for (i = 0; i < TRY_TIME; ++i) {
		frame_no[pos] = (++frame_no[pos]) % 17;
		pump.cmd[0] = 0xFA;
		pump.cmd[1] = 0x00;
		pump.cmd[2] = pump.panelPhy[pos];
		pump.cmd[3] = frame_no[pos];
		pump.cmd[4] = 0x06;    // len
		pump.cmd[5] = 0x11;    // cmd
		pump.cmd[6] = 0x02;
		pump.cmd[7] = 0x00;
		pump.cmd[8] = 0x00;
		pump.cmd[9] = 0x00;
		pump.cmd[10] = 0x00;
		crc = CRC_16_HS(&pump.cmd[1], 10);
		// pump.cmd[2] == 0x02, pump.cmd[3] == 0x04, 校验第二字节为 FA
		idx = 11;
		pump.cmd[idx++] = crc & 0x00FF;
		if (pump.cmd[idx - 1] == 0xFA) {
			pump.cmd[idx++] = 0xFA;
		}
		pump.cmd[idx++] = crc >> 8;
		if (pump.cmd[idx - 1] == 0xFA) {
			pump.cmd[idx++] = 0xFA;
		}
		pump.cmdlen = idx;
		pump.wantLen = 6;      // 变长

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog( "Node[%d]FP[%d]下发授权命令失败",node,pos+1 );
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x08;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x00;
		if (ret < 0) {
			RunLog( "Node[%d]FP[%d]下发授权命令成功, 取返回信息失败", node, pos + 1);
			continue;
		} 

		crc = CRC_16_HS(&pump.cmd[1], pump.cmdlen - 3);
		if ((__u8)(crc & 0x00FF) != pump.cmd[pump.cmdlen - 2] ||
				(__u8)(crc >> 8) != pump.cmd[pump.cmdlen - 1]) {
			RunLog( "Node[%d]FP[%d]返回数据校验错误", node, pos + 1);
			continue;
		}
		
		if (pump.cmd[6] != 0x55) {
			RunLog( "Node[%d]FP[%d] 执行授权命令失败", node, pos + 1);
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
	__u8 volume[5], amount[5];
	const __u8 arr_zero[4] = {0};

	// 加油量为零不转变为Fuelling状态 , 不上传Run_Trans
	if (fp_state[pos] != 3) {
		if (memcmp(arr_zero, &pump.cmd[8], 4) == 0) {
			return 0;
		}
		fp_state[pos] = 3;
		return 0;
	}

	//  取得实时加油量 ........
	
	volume[0] = 0x06;
	Hex2Bcd(&pump.cmd[7], 4, &volume[1], 4);

	amount[0] = 0x06;
	Hex2Bcd(&pump.cmd[11], 4, &amount[1], 4);

	if ((__u8)FP_State_Fuelling == lpDispenser_data->fp_data[pos].FP_State) {
		ret = ChangeFP_Run_Trans(node, 0x21+ pos, amount,  volume); //只有一个面板
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

	ret = GetCurrentLNZ(pos);
	if (ret < 0) {
		RunLog("获取当前油枪枪号失败");
		return -1;
	}

	ret = ChangeFP_State(node, pos + 0x21, FP_State_Calling, NULL);

	//上位机在线,但是允许自动授权;上位机不在线, 授权开关置于开的状态,此时自动授权
	if ((gpIfsf_shm->auth_flag == FCC_SWITCH_AUTH) && (lock_status() == 1)) {
		ret = DoAuth(pos);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]自动授权失败", node, pos + 1);
		}
		ret = ChangeFP_State(node, pos + 0x21, FP_State_Started, NULL);
		RunLog("节点[%d]面板[%d]自动授权成功", node, pos + 1);
		SO_OFFLINE_TR(node, pos);
	}

	return ret;
}

/*
 * func: 读取油机单价
 *      pump.cmd[7]-pump.cmd[8]
 */
static int GetPrice(int pos)
{
	int i, idx, ret;

	for (i = 0; i < TRY_TIME; ++i) {
		frame_no[pos] = (++frame_no[pos]) % 17;
		pump.cmd[0] = 0xFA;
		pump.cmd[1] = 0x00;
		pump.cmd[2] = pump.panelPhy[pos];
		pump.cmd[3] = frame_no[pos];
		pump.cmd[4] = 0x01;    // len
		pump.cmd[5] = 0x06;    // cmd
		crc = CRC_16_HS(&pump.cmd[1], 5);
		// pump.cmd[2] 为 0x03, pump.cmd[3] 为0x06的时候校验第二字节为FA
		idx = 6;
		pump.cmd[idx++] = crc & 0x00FF;
		if (pump.cmd[idx - 1] == 0xFA) {
			pump.cmd[idx++] = 0xFA;
		}
		pump.cmd[idx++] = crc >> 8;
		if (pump.cmd[idx - 1] == 0xFA) {
			pump.cmd[idx++] = 0xFA;
		}
		pump.cmdlen = idx;
		pump.wantLen = 6;      // 变长,  先接收到命令字,因命令字之前不会有FA的转义

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog( "Node[%d]FP[%d]下发取单价命令失败",node,pos+1 );
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x08;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x00;
		if (ret < 0) {
			RunLog( "Node[%d]FP[%d]下发取单价命令成功, 取返回信息失败",node,pos+1 );
			continue;
		} 

		crc = CRC_16_HS(&pump.cmd[1], pump.cmdlen - 3);
		if ((__u8)(crc & 0x00FF) != pump.cmd[pump.cmdlen - 2] ||
				(__u8)(crc >> 8) != pump.cmd[pump.cmdlen - 1]) {
			RunLog( "Node[%d]FP[%d]返回数据校验错误", node, pos + 1);
			continue;
		}

		return 0;
	}

	return -1;
}



/*
 * func: 读取实时状态 (失败只重试一次)
 *
 */
static int GetPumpStatus(int pos, __u8 * status)
{
	int i, idx, ret;
		
	for (i = 0; i < 2; ++i) {
		frame_no[pos] = (++frame_no[pos]) % 17;
		memset(pump.cmd, 0, 30);
		pump.cmd[0] = 0xFA;
		pump.cmd[1] = 0x00;
		pump.cmd[2] = pump.panelPhy[pos];
		pump.cmd[3] = frame_no[pos];
		pump.cmd[4] = 0x08;    // len
		pump.cmd[5] = 0x02;    // cmd
		getlocaltime(&pump.cmd[6]);

		crc = CRC_16_HS(&pump.cmd[1], 12);
		idx = 13;
		pump.cmd[idx++] = crc & 0x00FF;
		if (pump.cmd[idx - 1] == 0xFA) {
			pump.cmd[idx++] = 0xFA;
		}
		pump.cmd[idx++] = crc >> 8;
		if (pump.cmd[idx - 1] == 0xFA) {
			pump.cmd[idx++] = 0xFA;
		}
		pump.cmdlen = idx;
		pump.wantLen = 6;      // 变长

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog( "Node[%d]FP[%d]下发取状态命令失败",node,pos+1 );
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x08;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x00;
		if (ret < 0) {
			RunLog( "Node[%d]FP[%d]下发取状态命令成功, 取返回信息失败", node, pos + 1);
			continue;
		} 

		crc = CRC_16_HS(&pump.cmd[1], pump.cmdlen - 3);
		if ((__u8)(crc & 0x00FF) != pump.cmd[pump.cmdlen - 2] ||
				(__u8)(crc >> 8) != pump.cmd[pump.cmdlen - 1]) {
			RunLog( "Node[%d]FP[%d]返回数据校验错误", node, pos + 1);
			continue;
		}

		*status = pump.cmd[6];
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
	int i, idx, ret;
		
	for (i = 0; i < TRY_TIME; ++i) {
		frame_no[pos] = (++frame_no[pos]) % 17;
		memset(pump.cmd, 0, 30);
		pump.cmd[0] = 0xFA;
		pump.cmd[1] = 0x00;
		pump.cmd[2] = pump.panelPhy[pos];
		pump.cmd[3] = frame_no[pos];
		pump.cmd[4] = 0x08;    // len
		pump.cmd[5] = 0x02;    // cmd
		getlocaltime(&pump.cmd[6]);

		crc = CRC_16_HS(&pump.cmd[1], 12);
		idx = 13;
		pump.cmd[idx++] = crc & 0x00FF;
		if (pump.cmd[idx - 1] == 0xFA) {
			pump.cmd[idx++] = 0xFA;
		}
		pump.cmd[idx++] = crc >> 8;
		if (pump.cmd[idx - 1] == 0xFA) {
			pump.cmd[idx++] = 0xFA;
		}
		pump.cmdlen = idx;
		pump.wantLen = 6;      // 变长

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog( "Node[%d]FP[%d]下发取状态命令失败",node,pos+1 );
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x08;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x00;
		if (ret < 0) {
			RunLog( "Node[%d]FP[%d]下发取状态命令成功, 取返回信息失败", node, pos + 1);
			continue;
		} 

		crc = CRC_16_HS(&pump.cmd[1], pump.cmdlen - 3);
		if ((__u8)(crc & 0x00FF) != pump.cmd[pump.cmdlen - 2] ||
				(__u8)(crc >> 8) != pump.cmd[pump.cmdlen - 1]) {
			RunLog( "Node[%d]FP[%d]返回数据校验错误", node, pos + 1);
			continue;
		}

		*status = pump.cmd[6];
		return 0;
	}

	return -1;
}

/*
 * func: 读取实时状态 (失败只重试一次)
 *
 */
static int GetPumpInfo(int pos)
{
	int i, idx, ret;

	for (i = 0; i < TRY_TIME; ++i) {
		frame_no[pos] = (++frame_no[pos]) % 17;
		pump.cmd[0] = 0xFA;
		pump.cmd[1] = 0x00;
		pump.cmd[2] = pump.panelPhy[pos];
		pump.cmd[3] = frame_no[pos];  //  帧号 0-16 循环
		pump.cmd[4] = 0x01;    // len
		pump.cmd[5] = 0x01;    // cmd
		crc = CRC_16_HS(&pump.cmd[1], 5);
		/*
		 *  pump.cmd[2]: 1 - 4, pump.cmd[3]: 0-16 的情况下
		 *  校验不会出现 FA,这里不需要判断校验是否有FA,
		 *  为统一处理且GetPumpInfo很少使用, 所以增加判断;
		 */
		idx = 6;
		pump.cmd[idx++] = crc & 0x00FF;
		if (pump.cmd[idx - 1] == 0xFA) {
			pump.cmd[idx++] = 0xFA;
		}
		pump.cmd[idx++] = crc >> 8;
		if (pump.cmd[idx - 1] == 0xFA) {
			pump.cmd[idx++] = 0xFA;
		}
		pump.cmdlen = idx;
		pump.wantLen = 6;      // 变长,  先接收到命令字,因命令字之前不会有FA的转义

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog( "Node[%d]FP[%d]下发取设备信息命令失败",node,pos+1 );
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x08;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x00;
		if (ret < 0) {
			RunLog( "Node[%d]FP[%d]下发取设备信息命令成功, 取返回信息失败",node,pos+1 );
			continue;
		} 

		crc = CRC_16_HS(&pump.cmd[1], pump.cmdlen - 3);
		if ((__u8)(crc & 0x00FF) != pump.cmd[pump.cmdlen - 2] ||
				(__u8)(crc >> 8) != pump.cmd[pump.cmdlen - 1]) {
			RunLog( "Node[%d]FP[%d]返回数据校验错误", node, pos + 1);
			continue;
		}

		return 0;
	}

	return -1;
}


/*
 * func: 油机变为加油状态, 改变FP状态为Started
 */
static int DoStarted(int i)
{
	int ret;

	if (lpDispenser_data->fp_data[i].Log_Noz_State == 0) {
		__u8 ln_status = 0;
		ln_status = 1 << (pump.gunLog[i] - 1);
		lpDispenser_data->fp_data[i].Log_Noz_State = ln_status;
		RunLog("节点[%d]面板[%d] %d 号枪抬起", node, i + 1, pump.gunLog[i]);
	}

	ret = BeginTr4Fp(node, i + 0x21);
	if (ret < 0) {
		ret = BeginTr4Fp(node, i + 0x21);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d],BeingTr4FP Failed", node, i + 1);
			return -1;
		}
	}

	//  Log_Noz_State & BeginTr4Fp 成功, 则置位 fp_state[i] 为 2
	fp_state[i] = 2;

	return 0;
}


/*
 * func: 获取当前抬起油枪的枪号, 结果写入pump.gunLog[] & fp_data[].Log_Noz_State
 * ret : 0 - 成功, -1 - 失败 
 */
static inline int GetCurrentLNZ(int pos)
{
	RunLog("Node %d FP %d 的 %d 号枪抬起", node, pos + 1, pump.gunLog[pos]);
	lpDispenser_data->fp_data[pos].Log_Noz_State = 1 << (pump.gunLog[pos] - 1);

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
	__u8 price[4];

	for (i = 0; i < pump.panelCnt; i++) {
		for (j = 0; j < pump.gunCnt[i]; j++) {
			pr_no = (lpDispenser_data->fp_ln_data[i][j]).PR_Id - 1;  // 0-7	
			if ((pr_no < 0) || (pr_no > 7)) {
				RunLog("InitPumpPrice PR_Id error, PR_Id[%d]", pr_no + 1);
				return -1;
			}

			memcpy(lpDispenser_data->fp_data[i].Current_Unit_Price, 
					lpDispenser_data->fp_fm_data[pr_no][lpDispenser_data->fp_data[i].Default_Fuelling_Mode - 1].Prod_Price, 4);
#ifdef DEBUG
		RunLog("Init.FP %d LNZ %d PR_Id %d,Prod_Pricd: %02x%02x%02x%02x", i + 1, pump.gunLog[i], pr_no + 1,
				lpDispenser_data->fp_fm_data[pr_no][lpDispenser_data->fp_data[i].Default_Fuelling_Mode - 1].Prod_Price[0],
				lpDispenser_data->fp_fm_data[pr_no][lpDispenser_data->fp_data[i].Default_Fuelling_Mode - 1].Prod_Price[1],
				lpDispenser_data->fp_fm_data[pr_no][lpDispenser_data->fp_data[i].Default_Fuelling_Mode - 1].Prod_Price[2],
				lpDispenser_data->fp_fm_data[pr_no][lpDispenser_data->fp_data[i].Default_Fuelling_Mode - 1].Prod_Price[3]);
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
	int i;
	int pr_no;
	__u8 lnz = 0;     // fix value
	__u8 total[7] = {0};

	for (i = 0; i < pump.panelCnt; i++) {
		ret = GetPumpTotal(i);
		if (ret < 0) {
			RunLog("取 Node %d FP %d 的泵码失败", node, i);
			return -1;
		}

		// Volume_Total
		total[0] = 0x0a;
		total[1] = 0x00;
		Hex2Bcd(&pump.cmd[13], GUN_EXTEND_LEN, &total[2], GUN_EXTEND_LEN + 1);
		pr_no = (lpDispenser_data->fp_ln_data[i][lnz]).PR_Id - 1;  // 0-7	
		if ((pr_no < 0) || (pr_no > 7)) {
			RunLog("InitPumpTotal,FP %d, LNZ %d, PR_Id %d", i, lnz + 1, pr_no + 1);
		}

		memcpy(lpDispenser_data->fp_m_data[pr_no].Meter_Total, total, 7);
		memcpy((lpDispenser_data->fp_ln_data[i][pump.gunLog[i] - 1]).Log_Noz_Vol_Total, total, 7);
#if DEBUG
		RunLog("Init.fp_ln_data[%d][%d].Vol_Total: %s ", i, pump.gunLog[i] - 1,
				Asc2Hex(lpDispenser_data->fp_ln_data[i][0].Log_Noz_Vol_Total, 7));
		RunLog("Init.fp_m_data[%d].Meter_Total[%s] ", pr_no, Asc2Hex(total, 7));
		RunLog("Node[%d].FP[%d]LNZ[%d]的泵码为[%s]", node, i + 1,
			lnz + 0x01, Asc2Hex(&pump.cmd[12], GUN_EXTEND_LEN));
#endif
		// Amount_Total
		memset(total, 0, sizeof(total));
		total[0] = 0x0a;
		memcpy((lpDispenser_data->fp_ln_data[i][pump.gunLog[i] - 1]).Log_Noz_Amount_Total, total, 7);
		RunLog("Init.fp_ln_data[%d][%d].Amount_Total: %s ", i, pump.gunLog[i] - 1,
				Asc2Hex(lpDispenser_data->fp_ln_data[0][0].Log_Noz_Amount_Total, 7));
	}

	return 0;
}


static int InitGunLog()
{
	int i;

	for (i = 0; i < pump.panelCnt; ++i) {
		pump.gunLog[i] = 1;
	}

	return 0;
}


/*
 * func: 设置控制模式
 *	恒山自助加油机不需要, 通过键盘设置
 *
 */




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
		sleep(1);
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
static int SetPort008()
{
	int ret;

	if (gpGunShm->chnLogSet[pump.chnLog - 1] == 0) {
		pump.cmd[0] = 0x1C;
		pump.cmd[1] = 0x52;
		pump.cmd[2] = 0x30 | (pump.chnPhy - 1);
		pump.cmd[3] = 0x03;	// 9600bps
		pump.cmd[4] = 0x00;     // none
		pump.cmd[5] = 0x00;
		pump.cmd[6] = 0x00;
		pump.cmdlen = 7;
		pump.wantLen = 0;

		ret =  WriteToPort(pump.fd, pump.chnPhy, pump.chnLog, pump.cmd,
			       	pump.cmdlen, 0, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("设置通道 %d 串口属性失败", pump.chnLog);
			return -1;
		}
		gpGunShm->chnLogSet[pump.chnLog - 1] = 1;
	}

	return 0;
}




/*
 * 检查FP上各枪的油品单价是否符合正确(符合实际), 如果全部正确则改变状态为Closed
 */
static int CheckPriceErr(int pos)
{
	int ret, fm_idx;
	int lnz, pr_no;
	__u8 status;

	if ((__u8)FP_State_Inoperative != lpDispenser_data->fp_data[pos].FP_State) {
		return -1;
	}

	ret = GetPrice(pos);
	if (ret < 0) {
		return -1;
	}

	for (lnz = 0; lnz < pump.gunCnt[pos]; lnz++) {
		pr_no = (lpDispenser_data->fp_ln_data[pos][lnz]).PR_Id - 1;  // 0 - 7	
		if ((pr_no < 0) || (pr_no > 7)) {
			RunLog("CheckPrice ERR PR_Id error, FP[%d]LNZ[%d] PR_Id[%d] <ERR>",
								pos + 1, lnz + 1, pr_no + 1);
			return -1;
		}

		fm_idx = lpDispenser_data->fp_data[pos].Default_Fuelling_Mode - 1;
		RunLog("Node %d FP %d LNZ %d Now price is %02x.%02x; DST Price: %02x.%02x",
			node,pos+1,lnz+1, pump.cmd[7], pump.cmd[8],
		       	lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2], 
		       	lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3]);

		if ((pump.cmd[7] != lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2]) ||
			(pump.cmd[8] != lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3])) {
			return -2;
		}
	}
	
	ChangeFP_State(node, 0x21 + pos, FP_State_Closed, NULL);
	RunLog("Node[%d]FP[%d]单价已变更正确, 切换状态为Closed", node, pos + 1);

	return 0;
}


/* ----------------- End of lischn008.c ------------------- */

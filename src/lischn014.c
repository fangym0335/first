/*
 * Filename: lischn014.c
 *
 * Description: 加油机协议转换模块 - RS485, 4800,E,8,1 
 *              对应<<中意单枪通讯协议2.0>>
 *
 * History:
 * 	Created by Guojie Zhu @ 2008/09/24
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

#define VOL_TOTAL_LEN   5
#define FP_CLOSE        0
#define FP_IDLE         1
#define FP_CALLING      2
#define FP_AUTHORISED   3
#define FP_FUELLING     4
#define FP_EOT          5       // 预置加油达最大值或者被停止

#define TRY_TIME	5
#define TRY_TIME_CMD	5
#define INTERVAL	100000  // 100ms , 视油机而定

static int DoCmd();
static void DoPolling();
static int DoAuth(int pos);
static int DoBusy(int pos);
static int DoCall(int pos);
static int DoEot(int pos);
static int DoOpen(int pos, __u8 *msgbuffer);
static int DoClose(int pos, __u8 *msgbuffer);
static int DoRation(int pos);
static int DoStop(int pos);
static int DoMode();
static int GetPrice(int pos , __u8 *price);
static int DoPrice();
static int DoStarted(int pos);
static int ChangePrice();
static int GetTransData(int pos);
static int GetPumpStatus(int pos, __u8 *status, __u8 *msg_type);
static int GetPumpOnline(int pos, __u8 *status, __u8 *msg_type);
static int InitPumpTotal();
static int InitCurrentPrice();
static inline int GetCurrentLNZ(int pos);
static int SetCtrlMode(int pos);
static int clr_offline_trs(int pos);
static int keep_in_touch_xs(int x_seconds);
static int SetPort014();
static int ReDoSetPort014();
static int CheckPriceErr(int pos);

static PUMP pump;
static int node;
static int IFSFpos;
static DISPENSER_DATA *lpDispenser_data;
static NODE_CHANNEL *lpNode_channel;
static int fp_state[MAX_FP_CNT];   // 0 - IDLE, 1 - CALLING , 2 - 已完成DoStarted, 3 - FUELLING
static int dostop_flag;

int ListenChn014(int chnLog)
{
	long	indtyp;
	int	ret;
	int	fd;
	int	j;
	int	i;
	__u8 status;
	__u8 msg_type;
	
	sleep(5);   //防止此进程退出,而主进程没有启动完毕. 每个lischn014都需要
	RunLog("Listen Channel  014 ,logic channel[%d]", chnLog);
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
				pump.gunLog[j] = 1;    // 初始化gunLog, 单枪机,所以始终为1
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

	ret = SetPort014();
	if (ret < 0) {
		RunLog("设置串口属性失败");
		return -1;
	}
	
	// ============== Part.3 判断加油机是否在线 ====================================
	// Check wether dispenser is online, if it's not online, wait  here.
	while (1) {
		ret = GetPumpOnline(0, &status, &msg_type);
		if (ret == 0) {
			break;
		}

		gpIfsf_shm->node_online[IFSFpos] = 0; //不在线,不产生心跳.
		UserLog("Node %d 不在线", node);
		
		sleep(1);
	}
	
	// ============= Part.4 初始化与加油机当前状态相关的数据库相关项 =======================
	
	for (i = 0;  i < pump.panelCnt; ++i) {
		if (SetCtrlMode(i < 0)) {   // 设置为开放模式
			RunLog("Node[%d]FP[%d]设置开放模式失败", node, i + 1);
			return -1;
		}
	}

	ret = InitCurrentPrice(); //  初始化fp数据库的当前油价
	if (ret < 0) {
		RunLog("初始化油品单价到数据库失败");
		return -1;
	}

	for (i = 0;  i < pump.panelCnt; ++i) {
		ret = clr_offline_trs(i);
		if (ret < 0) {
			RunLog("清除历史交易失败");
			return -1;
		}
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
			GetPumpStatus(0, &status, &msg_type);              // Note. 与油机保持联系
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
			if (lpDispenser_data->fp_data[pos].FP_State < FP_State_Authorised ||
									dostop_flag == -1) {
			
				dostop_flag = 1;
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
			ret = HandleStateErr(node, pos, Do_Auth,
				lpDispenser_data->fp_data[pos].FP_State, msgbuffer); 
			if (ret < 0) {
				break;
			}
			ret = DoAuth(pos);				
			if (ret == 0) {       // exec success, change fp state
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
	__u8 status, msg_type;
	
	for (i = 0; i < pump.panelCnt; i++) {
		ret = GetPumpStatus(i, &status, &msg_type);
		if (ret < 0) {
			if (gpIfsf_shm->node_online[IFSFpos] == node) {
				if ((++fail_cnt) < MAX_FAIL_CNT) {  // 加油机离线判断
					/*
					 * 因电压不稳出现掉电会导致单片机复位, 增加此处理解决该问题
					 */
					/*
					 * 因重设串口属性会导致ReadTTY接收到"非法数据", 故暂时移除
					 * 2009.02.19
					if (fail_cnt >= (MAX_FAIL_CNT / 2)) {
						if (SetPort014() >= 0) {
							RunLog("节点%d重设串口属性成功", node);
						}
					}
					*/
					continue;
				}

				// Step.1
				dostop_flag = 0;
				memset(fp_state, 0, sizeof(fp_state));

				// Step.2
//				gpIfsf_shm->node_online[IFSFpos] = 0;
				ChangeNode_Status(node, NODE_OFFLINE);
				pump.time_offline = (3 * gpIfsf_shm->fcc_com_sv_data.Heartbeat_Interval) + time(NULL);
				RunLog("====== Node %d 离线! =============================", node);

				// 油机离线, 无需上传状态改变信息, 所以不使用ChangeFP_State, 直接操作FP_State
				for (j = 0; j < pump.panelCnt; ++j) {
					lpDispenser_data->fp_data[j].FP_State = FP_State_Inoperative;
					lpDispenser_data->fp_data[j].Log_Noz_State = 0;
				}
			}

			// 重设串口属性(防止离线期间掉电导致单片机复位)
			//ReDoSetPort014();
			continue;
		} else {
			if (gpIfsf_shm->node_online[IFSFpos] == 0) {
				if ((--fail_cnt) > 0) { // 排除干扰/双线电流环数据采集Bug造成的影响
					fail_cnt = 1;
					continue;
				} else if (time(NULL) < pump.time_offline) {
					RunLog("====================== wait for 30s ===============");
					continue;
				}

				// fail_cnt复位
				fail_cnt = 0;

				// Step.1
				if (SetCtrlMode(i) < 0) {   // 设置为FCC控制模式
					RunLog("Node[%d]FP[%d]设置主控模式失败", node, i + 1);
					continue;
				}

				// Step.2
				ret = InitPumpTotal();    // 更新泵码到数据库
				if (ret < 0) {
					RunLog("更新泵码到数据库失败");
					continue;
				}

				// Step.3
				clr_offline_trs(i);

				// Step.4
				for (j = 0;  j < pump.panelCnt; ++j) {
					lpDispenser_data->fp_data[j].FP_State = FP_State_Closed;
				}

				// Step.5 一起都准备好了, 上线
//				gpIfsf_shm->node_online[IFSFpos] = node;
				ChangeNode_Status(node, NODE_ONLINE);
				RunLog("====== Node %d 恢复联线! =========================", node);
				keep_in_touch_xs(12);               // 确保CD已经收到心跳之后再做后续操作
				// Step.6                 // 主动上传当前FP状态
				for (j = 0; j < pump.panelCnt; ++j) {
					ChangeFP_State(node, j + 0x21, FP_State_Closed, NULL);
				}
				continue;
			}
			
			// Note. 检查控制模式是否仍然为主控
			if (status < 0x10) {
				if (SetCtrlMode(i) < 0) {   // 设置为主控模式
					RunLog("Node[%d]FP[%d]设置主控模式失败", node, i + 1);
				} else {
					fail_cnt = 0;
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

		pump.cmdlen = 0;

		switch (status & 0x0F) {
		case FP_CLOSE:
			break;
		case FP_EOT:
			dostop_flag = -1;
			if (fp_state[i] < 2) {
				DoStarted(i);
			}

			if (msg_type == 0x02||msg_type == 0x00) {// && lpDispenser_data->fp_data[i].FP_State == FP_State_Authorised)) {
				if ((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State ) {
					ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
				}
				
//				RunLog("---------i : %d ---------", i);
				if (DoEot(i) == 0) {
					fp_state[i] = 0;
//			RunLog("---------fp_state[%d] : %d ---------", i, fp_state[i]);
				}
			}

			break;
		case FP_IDLE:                                   // 挂枪、空闲
			dostop_flag = 0;
			Ssleep(100000);
//			RunLog("---------fp_state[%d] : %d ---------", i, fp_state[i]);
			if (fp_state[i] >= 2 && (msg_type == 0x02 ||msg_type == 0x00 )) {
			       //由于是先发送状态后保存交易记录，所以
			       //需要先用tmp_lnz把当前用的油枪存起来，供保存记录时用，
			       //否则保存的记录枪号就是0了
				// 1. ChangeFP_State to IDLE first
				tmp_lnz = lpDispenser_data->fp_data[i].Log_Noz_State;  // just for test offline TR
				lpDispenser_data->fp_data[i].Log_Noz_State = 0x00;     
				if ((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State ) {
					ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
				}
				// Step2. Get & Save TR to TR_BUFFER
				lpDispenser_data->fp_data[i].Log_Noz_State = tmp_lnz;  // just for test offline TR
				ret = DoEot(i);
			}

			lpDispenser_data->fp_data[i].Log_Noz_State = 0x00;
			fp_state[i] = 0; //IDLE
			if ((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State) {
				ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
			}

			EO_OFFLINE_TR(node, i);
			if (gpIfsf_shm->fp_offline_flag[node - 1][i] != 0) {
				upload_offline_tr(node, NULL);
			}

			//ret = ChangePrice();                    // 查看是否有油品需要变价,有则执行
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
		case FP_CALLING:                                // 抬枪
			if (dostop_flag == 1) {
				break;
			}
			if ((__u8)FP_State_Calling > lpDispenser_data->fp_data[i].FP_State) {

				fp_state[i] = 1;
				
				ret = DoCall(i);

				ret = DoStarted(i);
			}

			break;
#if 0                        
		case FP_AUTHORISED:
			if ((__u8)FP_State_Authorised != lpDispenser_data->fp_data[i].FP_State) {
				ret = ChangeFP_State(node, i + 0x21, FP_State_Authorised, NULL);
			}

			break;
#endif
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

			if ((__u8)FP_State_Fuelling > lpDispenser_data->fp_data[i].FP_State) {
				if (fp_state[i] == 3) {
					ret = ChangeFP_State(node, i + 0x21, FP_State_Fuelling, NULL);
					if (ret < 0) {
						break;
					}
				}
			}
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
#if 0
		// 化繁为简, 若有必要再把锁枪下发到油机, GuojieZhu @ 2009.7.18
		for (i = 0; i < TRY_TIME; ++i) {
			pump.cmd[0] = 0xAA;
			pump.cmd[1] = pump.panelPhy[pos];
			pump.cmd[2] = 0x05;  // len
			pump.cmd[3] = 0xfa;  // cmd
			pump.cmd[4] = 0x01;  // close
			pump.cmd[5] = 0xfe ^ pump.cmd[1];
			pump.cmdlen = 6;
			pump.wantLen = 6;

			Ssleep(INTERVAL);
			ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy,
					pump.cmd, pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
			if (ret < 0) {
				RunLog("下发开机命令到Node[%d]FP[%d]失败", node, pos + 1);
				continue;
			}

			pump.cmdlen = pump.wantLen;
			ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
			if (ret < 0) {
				RunLog("下发开机命令到Node[%d]FP[%d]成功, 取返回信息失败", node, pos + 1);
				continue;
			}

			if (Check_xor(&pump.cmd[1], pump.cmdlen - 2) != pump.cmd[pump.cmdlen - 1]) {
				RunLog("Node[%d].FP[%d]返回信息校验错误", node, pos + 1);
				continue;
			}

			if (pump.cmd[pump.cmdlen - 2] == 0xCC) {
				ret = ChangeFP_State(node, pos + 0x21, FP_State_Idle, msgbuffer);
				return 0;
			}
		}
#endif
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
			/*
			 * 单枪机, 这部分其实不需要
			 */
			ret = ChangeFP_State(node, 0x21 + i, FP_State_Closed, NULL);
		}
	} else {
		ret = ChangeFP_State(node, pos + 0x21, FP_State_Closed, msgbuffer);
#if 0
		// 化繁为简, 若有必要再把锁枪下发到油机, GuojieZhu @ 2009.7.18
		for (i = 0; i < TRY_TIME; ++i) {
			pump.cmd[0] = 0xAA;
			pump.cmd[1] = pump.panelPhy[pos];
			pump.cmd[2] = 0x05;  // len
			pump.cmd[3] = 0xfa;  // cmd
			pump.cmd[4] = 0x00;  // close
			pump.cmd[5] = 0xff ^ pump.cmd[1];
			pump.cmdlen = 6;
			pump.wantLen = 6;

			Ssleep(INTERVAL);
			ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy,
					pump.cmd, pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
			if (ret < 0) {
				RunLog("下发锁机命令到Node[%d]FP[%d]失败", node, pos + 1);
				continue;
			}

			pump.cmdlen = pump.wantLen;
			ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
			if (ret < 0) {
				RunLog("下发锁机命令到Node[%d]FP[%d]成功, 取返回信息失败", node, pos + 1);
				continue;
			}

			if (Check_xor(&pump.cmd[1], pump.cmdlen - 2) != pump.cmd[pump.cmdlen - 1]) {
				RunLog("Node[%d].FP[%]返回信息校验错误", node, pos + 1);
				continue;
			}

			if (pump.cmd[pump.cmdlen - 2] == 0xCC) {
				ret = ChangeFP_State(node, pos + 0x21, FP_State_Closed, msgbuffer);
				return 0;
			}
		}
#endif
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
	int i, ret;
	int ration_type;
	__u8 pre_volume[3] = {0};

	RunLog("Node %d FP %d DoRation", node, pos + 1);

	if (pump.cmd[0] == 0x3D) {   // Preset
		ration_type = 0x01;
		memcpy(pre_volume, &lpDispenser_data->fp_data[pos].Remote_Volume_Preset[2], 3);
	} else {
		ration_type = 0x02;
		memcpy(pre_volume, &lpDispenser_data->fp_data[pos].Remote_Amount_Prepay[2], 3);
	}

	for (i = 0; i < TRY_TIME_CMD; ++i) {
		pump.cmd[0] = 0xAA;
		pump.cmd[1] = pump.panelPhy[pos];
		pump.cmd[2] = 0x07;  // len
		pump.cmd[3] = 0xf7;  // cmd
		pump.cmd[4] = ration_type;
		pump.cmd[5] = pre_volume[0];
		pump.cmd[6] = pre_volume[1];
		pump.cmd[7] = Check_xor(&pump.cmd[1], 6);
		pump.cmdlen = 8;
		pump.wantLen = 6;

		Ssleep(INTERVAL * 3);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy,
				pump.cmd, pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("下发预置命令到Node[%d]FP[%d]失败", node, pos + 1);
			continue;
		}

		pump.cmdlen = pump.wantLen;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("下发预置命令到Node[%d]FP[%d]成功, 取返回信息失败", node, pos + 1);
			continue;
		}

		if (Check_xor(&pump.cmd[1], pump.cmdlen - 2) != pump.cmd[pump.cmdlen - 1]) {
			RunLog("Node[%d]返回信息校验错误", node);
			continue;
		}

		if (pump.cmd[pump.cmdlen - 2] == 0xCC) {
			return 0;
		}
	}

	return -1;
}
/*
*   读取油机单价
*/
static int GetPrice(int pos , __u8 *price)
{
   	int ret;
	int i;

	for (i = 0; i < 2; ++i) {
		pump.cmd[0] = 0xAA;
		pump.cmd[1] = pump.panelPhy[pos];
		pump.cmd[2] = 0x04;  // len
		pump.cmd[3] = 0xf0;  // cmd
		pump.cmd[4] = 0xf4 ^ pump.cmd[1];
		pump.cmdlen = 5;
		pump.wantLen = 23;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy,
				pump.cmd, pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("下发读状态命令到Node[%d]FP[%d]失败", node, pos + 1);
			continue;
		}

		pump.cmdlen = pump.wantLen;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("下发读状态命令到Node[%d]FP[%d]成功, 取返回信息失败", node, pos + 1);
			continue;
		}

		if (Check_xor(&pump.cmd[1], 21) != pump.cmd[22]) {
			RunLog("Node[%d]返回信息校验错误", node);
			continue;
		}

		if (pump.cmd[0] != 0xbb) {
			RunLog("Node[%d]返回信息格式错误", node);
			continue;
		}

		memcpy( price, &pump.cmd[13] , 2);
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
	int i, j;
	int ret, fm_idx;
	int trycnt;
	__u8 pr_no;
	__u8 lnz;
	__u8 price[2] = {0};
	__u8 err_code;
	__u8 err_code_tmp;

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
					RunLog(">>>2 lnz: %d, fp_ln_data[%d][j].PR_Id: %d", j + 1,
							lpDispenser_data->fp_ln_data[i][j].PR_Id);
#endif
					if (lpDispenser_data->fp_ln_data[i][j].PR_Id - 1 == pr_no) {
					err_code_tmp = 0x99;
				CriticalLog("下发变价命令给节点[%02d]面[%d]枪[%d]......", node, i + 1, j + 1);

					for (trycnt = 0; trycnt < TRY_TIME_CMD; trycnt++) {
						// Step.1 发变价命令
						pump.cmd[0] = 0xAA;
						pump.cmd[1] = pump.panelPhy[i];
						pump.cmd[2] = 0x06;  // len
						pump.cmd[3] = 0xf4;  // cmd
						pump.cmd[4] = pump.newPrice[pr_no][2];
						pump.cmd[5] = pump.newPrice[pr_no][3];
						pump.cmd[6] = Check_xor(&pump.cmd[1], 5);
						pump.cmdlen = 7;
						pump.wantLen = 6;

						Ssleep(INTERVAL);
						ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy,
								pump.cmd, pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
						if (ret < 0) {
							RunLog("下发变价命令到Node[%d]FP[%d]失败", node, i + 1);
							continue;
						}

						RunLog("Node[%d].FP[%d].PriceChange_CMD: %s",
							node, i + 1, Asc2Hex(pump.cmd, pump.cmdlen));

						// Step.2 根据命令返回信息或者重读油机单价确认变价是否成功
						pump.cmdlen = pump.wantLen;
						ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
						if (ret < 0) {
							RunLog("下发变价命令到Node[%d]FP[%d]成功, 取返回信息失败",
													node, i + 1);
							continue;
						}

						if (Check_xor(&pump.cmd[1], pump.cmdlen - 2) != pump.cmd[pump.cmdlen - 1]) {
							RunLog("Node[%d]返回信息校验错误", node);
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
							continue;
						}

						if (pump.cmd[4] != 0xCC) {
							err_code_tmp = 0x37;     // 执行失败
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
							continue;
						}
						Ssleep(INTERVAL); //第一次取返回信息失败，所以加个延迟
						
						ret = GetPrice(i, price);
						if(ret < 0){
							continue;
						}
						
						if(memcmp(price , &pump.newPrice[pr_no][2] , 2) != 0){
							RunLog("节点[%d]面板[%d]变价失败 %d 次",node,i+1, trycnt+1);
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
							err_code_tmp = 0x37;
							continue;		
						}
						// Step.3 写新单价到油品数据库 & 清零单价缓存
						fm_idx = lpDispenser_data->fp_data[i].Default_Fuelling_Mode - 1;
						RunLog("Node[%d]变价成功,新单价fp_fm_data[%d][%d].Prode_Price: %s", 
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
						// Write Error Code: 0x36/0x37 - price Change Error (Spare)
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
					if (0x99 == err_code) {
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
static int DoEot(int pos)
{
	int i, ret;
	int pr_no;
	int err_cnt;
	__u8 price[5];
	__u8 volume[5];
	__u8 amount[5];
	__u8 total[7];
	__u8 pre_total[7];
	__u8 status;
	
	RunLog("节点[%d]面板[%d]处理新交易", node, pos + 1);
	
	err_cnt = 0;
	do {
		ret = GetTransData(pos);
		if (ret < 0) {
			err_cnt++;
		}
	} while (ret < 0 && err_cnt <= 5);
	
	if (ret < 0) {
		return -1;
	}

	total[0] = 0x0A;
	total[1] = 0x00;
	memcpy(&total[2], &pump.cmd[17], VOL_TOTAL_LEN);

	memcpy(pre_total, (lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Vol_Total, 7);
//	RunLog("pre_total: %s, ", Asc2Hex(pre_total, 7));
//	RunLog("total: %s, ", Asc2Hex(total, 7));
	
	//3. 得到油品id ,应该为0	
	pr_no = (lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).PR_Id - 1;  // 0 - 7	
	if ((pr_no < 0) || (pr_no > 7)) {
		RunLog("DoEot PR_Id error, PR_Id[%d] <ERR>", pr_no + 1);
	}

	//4. 检查泵码是否有变化,如果有则保存交易记录
	if (memcmp(pre_total, total, 7) != 0) {
		//Current_Volume 
		volume[0] = 0x06;
		volume[1] = 0x00;
		memcpy(&volume[2], &pump.cmd[7], 3);
		memcpy(lpDispenser_data->fp_data[pos].Current_Volume, volume, 5);

		//Current_Amount
		amount[0] = 0x06;
		amount[1] = 0x00;
		memcpy(&amount[2], &pump.cmd[10], 3);
		memcpy(lpDispenser_data->fp_data[pos].Current_Amount, amount, 5);

		//Current_Unit_Price
		price[0] = 0x04;
		price[1] = 0x00;
		memcpy(&price[2], &pump.cmd[13], 2);
		memcpy(lpDispenser_data->fp_data[pos].Current_Unit_Price, price, 4);
				
		//chinese tax dispenser transimit in XXXXXXX.X, so use 0b as first byte.
		memcpy(lpDispenser_data->fp_m_data[pr_no].Meter_Total, total, 7 );		
		memcpy((lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Vol_Total, total, 7);

#if 0
		RunLog("节点[%02d]面板[%d]枪[1] ## Amount: %02x%02x.%02x Volume: %02x%02x.%02x Price: %02x.%02x Total: %02x%02x%02x.%02x", 
			node, pos + 1, amount[2], amount[3], amount[4], volume[2], volume[3], volume[4],
		       	price[2], price[3], total[3], total[4], total[5], total[6]);
#endif
		
		// Update Amount_Total
		memset(total, 0, sizeof(total));
		total[0] = 0x0A;
		memcpy((lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Amount_Total, total, 7);
		memcpy(lpDispenser_data->fp_data[pos].Current_Prod_Nb, lpDispenser_data->fp_pr_data[pr_no].Prod_Nb, 4);

		err_cnt = 0;
		do {
			ret = EndTr4Ln(node, pos + 0x21, pump.gunLog[pos] + 0x10);
			if (ret < 0) {
				err_cnt++;
				RunLog("节点[%d]面板[%d] DoEot.EndTr4Ln failed, ret: %d <ERR>", node, pos+1, ret);
			}
		} while (ret < 0 && err_cnt <= 5);

		err_cnt = 0;
		do {
			ret = EndTr4Fp(node, pos + 0x21, pre_total, 
				(lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Vol_Total);
			if (ret < 0) {
				err_cnt++;
				RunLog("节点[%d]面板[%d]DoEot.EndTr4Fp failed, ret: %d <ERR>", node, pos + 1, ret);
			}
		} while (ret < 0 && err_cnt <= 8);

	} else {
		ret = EndZeroTr4Fp(node, pos + 0x21);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d] DoEot.EndZeroTr4Fp failed, ret: %d <ERR>",node,pos+1,ret);
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
	int i;

	for (i = 0; i < TRY_TIME; ++i) {
		pump.cmd[0] = 0xAA;
		pump.cmd[1] = pump.panelPhy[pos];
		pump.cmd[2] = 0x04;  // len
		pump.cmd[3] = 0xf0;  // cmd
		pump.cmd[4] = 0xf4 ^ pump.cmd[1];
		pump.cmdlen = 5;
		pump.wantLen = 23;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy,
				pump.cmd, pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("下发读泵码泵码命令到Node[%d]FP[%d]失败", node, pos + 1);
			continue;
		}

		pump.cmdlen = pump.wantLen;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("下发读泵码泵码命令到Node[%d]FP[%d]成功, 取返回信息失败", node, pos + 1);
			continue;
		}

		if (Check_xor(&pump.cmd[1], 21) != pump.cmd[22]) {
			RunLog("Node[%d]返回信息校验错误", node);
			continue;
		}

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
	int i, ret;
	int err_cnt = 5;
	int cmdlen, wantLen;
	__u8 status, msg_type;
	__u8 cmd[7] = {0};

	do {
		ret = GetPumpStatus(pos, &status, &msg_type);
		if (ret < 0) {
			continue;
		}

		cmd[0] = 0xAA;
		cmd[1] = pump.panelPhy[pos];
		cmd[2] = 0x06;  // len
		cmd[3] = 0xf1;  // cmd
		cmd[4] = pump.cmd[15]; // seq_nb byte 1
		cmd[5] = pump.cmd[16]; // seq_nb byte 2
		cmd[6] = Check_xor(&cmd[1], 5);
		cmdlen = 7;
		wantLen = 0;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy,
				cmd, cmdlen, 1, wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("下发交易数据应答命令到Node[%d]FP[%d]失败", node, pos + 1);
			continue;
		}

		// 确认读到的是最新一笔交易, 并且油机已经删除该交易
		if (0x02 != msg_type) {
			return 0;
		}
	} while (err_cnt <= 5);

	return 0;
}

/*
* func: 读取历史交易记录
*/


/*
* func: Terminate_FP
*/
static int DoStop(int pos)
{
	int ret;
	int i;

	RunLog("Node %d FP %d DoStop", node, pos + 1);

	for (i = 0; i < TRY_TIME_CMD; ++i) {
		pump.cmd[0] = 0xAA;
		pump.cmd[1] = pump.panelPhy[pos];
		pump.cmd[2] = 0x04;  // len
		pump.cmd[3] = 0xf8;  // cmd
		pump.cmd[4] = 0xfc ^ pump.cmd[1];
		pump.cmdlen = 5;
		pump.wantLen = 6;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy,
				pump.cmd, pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("下发停止加油命令到Node[%d]FP[%d]失败", node, pos + 1);
			continue;
		}

		pump.cmdlen = pump.wantLen;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("下发停止加油命令到Node[%d]FP[%d]成功, 取返回信息失败", node, pos + 1);
			continue;
		}

		if (Check_xor(&pump.cmd[1], pump.cmdlen - 2) != pump.cmd[5]) {
			RunLog("Node[%d]返回信息校验错误", node);
			continue;
		}

		if (pump.cmd[pump.cmdlen - 2] == 0xCC) {
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
	int ret;
	int i;

	RunLog("Node %d FP %d DoAuth", node, pos + 1);

	for (i = 0; i < TRY_TIME_CMD; ++i) {
		pump.cmd[0] = 0xAA;
		pump.cmd[1] = pump.panelPhy[pos];
		pump.cmd[2] = 0x07;  // len
		pump.cmd[3] = 0xf7;  // cmd
		pump.cmd[4] = 0x00;  // Release
		pump.cmd[5] = 0x00;
		pump.cmd[6] = 0x00;
		pump.cmd[7] = 0xf0 ^ pump.cmd[1];
		pump.cmdlen = 8;
		pump.wantLen = 6;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy,
				pump.cmd, pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("下发授权命令到Node[%d]FP[%d]失败", node, pos + 1);
			continue;
		}

		pump.cmdlen = pump.wantLen;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("下发授权命令到Node[%d]FP[%d]成功, 取返回信息失败", node, pos + 1);
			continue;
		}

		if (Check_xor(&pump.cmd[1], pump.cmdlen - 2) != pump.cmd[5]) {
			RunLog("Node[%d]返回信息校验错误", node);
			continue;
		}

		if (pump.cmd[pump.cmdlen - 2] == 0xCC) {
			return 0;
		}
	}

	return -1;
}

/*
 * func: 设置主控模式
 */
static int SetCtrlMode(int pos)
{
	int ret;
	int i;

	RunLog("Node %d FP %d SetCtrlMode", node, pos + 1);

	for (i = 0; i < TRY_TIME; ++i) {
		pump.cmd[0] = 0xAA;
		pump.cmd[1] = pump.panelPhy[pos];
		pump.cmd[2] = 0x05;  // len
		pump.cmd[3] = 0xf9;  // cmd
		pump.cmd[4] = 0x01;  // 主控
		pump.cmd[5] = 0xfd ^ pump.cmd[1];
		pump.cmdlen = 6;
		pump.wantLen = 6;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy,
				pump.cmd, pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("下发设置主控命令到Node[%d]FP[%d]失败", node, pos + 1);
			continue;
		}

		pump.cmdlen = pump.wantLen;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("下发设置主控命令到Node[%d]FP[%d]成功, 取返回信息失败", node, pos + 1);
			continue;
		}

		if (Check_xor(&pump.cmd[1], pump.cmdlen - 2) != pump.cmd[5]) {
			RunLog("Node[%d]返回信息校验错误", node);
			continue;
		}
		
		if (pump.cmd[pump.cmdlen - 2] == 0xCC) {
			return 0;
		}
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
	const __u8 arr_zero[3] = {0};

	// 加油量为零不转变为Fuelling状态 , 不上传Run_Trans
	if (fp_state[pos] != 3) {
		if (memcmp(arr_zero, &pump.cmd[7], 3) == 0) {
			return 0;
		}
		fp_state[pos] = 3;
		return 0;
	}

	//  取得实时加油量 ........
	
	volume[0] = 0x06;
	volume[1] = 0x00;
	memcpy(&volume[2],  &pump.cmd[7], 3);

	amount[0] = 0x06;
	amount[1] = 0x00;
	memcpy(&amount[2],  &pump.cmd[10], 3);

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
 * func: 油机变为加油状态, 改变FP状态为Started
 */
static int DoStarted(int i)
{
	int ret;

	if (lpDispenser_data->fp_data[i].Log_Noz_State == 0) {
		lpDispenser_data->fp_data[i].Log_Noz_State = 0x01;
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
 * func: 读取实时状态 (失败只重试一次)
 *
 */
static int GetPumpStatus(int pos, __u8 *status, __u8 *msg_type)
{
	int ret;
	int i;

	for (i = 0; i < 2; ++i) {
		pump.cmd[0] = 0xAA;
		pump.cmd[1] = pump.panelPhy[pos];
		pump.cmd[2] = 0x04;  // len
		pump.cmd[3] = 0xf0;  // cmd
		pump.cmd[4] = 0xf4 ^ pump.cmd[1];
		pump.cmdlen = 5;
		pump.wantLen = 23;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy,
				pump.cmd, pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("下发读状态命令到Node[%d]FP[%d]失败", node, pos + 1);
			continue;
		}

		pump.cmdlen = pump.wantLen;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("下发读状态命令到Node[%d]FP[%d]成功, 取返回信息失败", node, pos + 1);
			continue;
		}

		if (Check_xor(&pump.cmd[1], 21) != pump.cmd[22]) {
			RunLog("Node[%d]返回信息校验错误", node);
			continue;
		}

		if (pump.cmd[0] != 0xbb) {
			RunLog("Node[%d]返回信息格式错误", node);
			continue;
		}

		*status = pump.cmd[5] | (pump.cmd[4] << 4); // 合并两个状态字节, 控制模式体现在高4位
		*msg_type = pump.cmd[6];
		return 0;
	}

	return -1;

}


//test wether the dispenser is online, we only use fp 1 to test.
/*
 * func: 判断Dispenser是否在线
 */
static int GetPumpOnline(int pos, __u8 *status, __u8 *msg_type)
{
	int ret;
	int i;

	for (i = 0; i < TRY_TIME; ++i) {
		pump.cmd[0] = 0xAA;
		pump.cmd[1] = pump.panelPhy[pos];
		pump.cmd[2] = 0x04;  // len
		pump.cmd[3] = 0xf0;  // cmd
		pump.cmd[4] = 0xf4 ^ pump.cmd[1];
		pump.cmdlen = 5;
		pump.wantLen = 23;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy,
				pump.cmd, pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("下发读状态命令到Node[%d]FP[%d]失败", node, pos + 1);
			continue;
		}

		pump.cmdlen = pump.wantLen;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("下发读状态命令到Node[%d]FP[%d]成功, 取返回信息失败", node, pos + 1);
			continue;
		}

		if (Check_xor(&pump.cmd[1], 21) != pump.cmd[22]) {
			RunLog("Node[%d]返回信息校验错误", node);
			continue;
		}

		if (pump.cmd[0] != 0xbb) {
			RunLog("Node[%d]返回信息格式错误", node);
			continue;
		}

		*status = pump.cmd[5] | (pump.cmd[4] << 4); // 合并两个状态字节, 控制模式体现在高4位
		*msg_type = pump.cmd[6];
		return 0;
	}

	return -1;

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
			pr_no = (lpDispenser_data->fp_ln_data[i][0]).PR_Id - 1;  // 0-7	
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
		pr_no = (lpDispenser_data->fp_ln_data[i][lnz]).PR_Id - 1;  // 0-7	
		if ((pr_no < 0) || (pr_no > 7)) {
			RunLog("InitPumpTotal,FP %d, LNZ %d, PR_Id %d", i, lnz + 1, pr_no + 1);
		}

		total[0] = 0x0a;
		total[1] = 0x00;
		memcpy(&total[2], &pump.cmd[17], VOL_TOTAL_LEN);
		memcpy(lpDispenser_data->fp_m_data[pr_no].Meter_Total, total, 7);
		memcpy((lpDispenser_data->fp_ln_data[i][pump.gunLog[i] - 1]).Log_Noz_Vol_Total, total, 7);
#if DEBUG
		RunLog("Init.fp_ln_data[%d][%d].Vol_Total: %s ", i, pump.gunLog[i] - 1,
				Asc2Hex(lpDispenser_data->fp_ln_data[0][0].Log_Noz_Vol_Total, 7));
		RunLog("Init.fp_m_data[%d].Meter_Total[%s] ", pr_no, Asc2Hex(total, 7));
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


static int clr_offline_trs(int pos)
{
	int ret;
	__u8 status, msg_type;
	__u8 seq_nb[2] = {0};

	do {
		ret = GetPumpStatus(pos, &status, &msg_type);
		if (ret < 0) {
			continue;
		}

		seq_nb[0] = pump.cmd[15];
		seq_nb[1] = pump.cmd[16];

		RunLog("seq_nb: %02x%02x", pump.cmd[15], pump.cmd[16]);

		if (0x02 == msg_type) {
			pump.cmd[0] = 0xAA;
			pump.cmd[1] = pump.panelPhy[pos];
			pump.cmd[2] = 0x06;  // len
			pump.cmd[3] = 0xf1;  // cmd
			pump.cmd[4] = seq_nb[0];
			pump.cmd[5] = seq_nb[1];
			pump.cmd[6] = Check_xor(&pump.cmd[1], 5);
			pump.cmdlen = 7;

			Ssleep(INTERVAL);
			ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy,
					pump.cmd, pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
			if (ret < 0) {
				RunLog("下发交易数据应答命令到Node[%d]FP[%d]失败", node, pos + 1);
				continue;
			}

		} else {
			break;
		}
	} while (1);

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
	__u8 msg_type;

	return_time = time(NULL) + (x);
	RunLog("==================return_time: %ld =======", return_time);
	do {
		sleep(1);
		if (GetPumpStatus(0, &status, &msg_type) < 0) {
			fail_times++;
		}
	} while (time(NULL) < return_time);

	return fail_times;
}

/*
 * func: 设定串口属性—通讯速率、校验位等
 */
static int SetPort014()
{
	int ret;

	pump.cmd[0] = 0x1C;
	pump.cmd[1] = 0x52;
	pump.cmd[2] = 0x30 | (pump.chnPhy - 1);
	pump.cmd[3] = 0x06;	// 4800bps
	pump.cmd[4] = 0x01 << ((pump.chnPhy - 1) % 8);    // check
	pump.cmd[5] = 0x00;	// even
	pump.cmd[6] = 0x01 << ((pump.chnPhy - 1) % 8);    // check
	pump.cmdlen = 7;
	pump.wantLen = 0;

	ret =  WriteToPort(pump.fd, pump.chnPhy, pump.chnLog, pump.cmd,
			pump.cmdlen, 0, pump.wantLen, TIME_OUT_WRITE_PORT);
	if (ret < 0) {
		RunLog("设置通道 %d 串口属性失败", pump.chnLog);
		return -1;
	}

	gpGunShm->chnLogSet[pump.chnLog - 1] = 1;
	Ssleep(INTERVAL);

	return 0;
}



/*
 * func:  通讯不通时轮询过程中隔一段时间重设一下串口属性
 *        避免因意外的电压不稳导致单片机复位--串口属性设置恢复预设9600,8,n,1
 */
static int ReDoSetPort014()
{
#define RE_DO_FREQUENCY   10         //  10次轮询失败设置一下串口属性

	int static polling_fail_cnt = 0;
	int ret;

	if ((++polling_fail_cnt) >= RE_DO_FREQUENCY) {
		ret = SetPort014();
		if (ret < 0) {
			RunLog("节点 %d 重设串口属性失败", node);
		}
		polling_fail_cnt = 0;
	}

	return ret;
}



/*
 * 检查FP上各枪的油品单价是否符合正确(符合实际), 如果全部正确则改变状态为Closed
 */
static int CheckPriceErr(int pos)
{
	int ret, fm_idx;
	int lnz, pr_no;
	__u8 status;
	__u8 price[2] = {0};

	if ((__u8)FP_State_Inoperative != lpDispenser_data->fp_data[pos].FP_State) {
		return -1;
	}

	ret = GetPrice(pos, price);
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
			node,pos+1,lnz+1,price[0], price[1],
		       	lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2], 
		       	lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3]);

		if ((price[0] != lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2]) ||
			(price[1] != lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3])) {
			return -2;
		}
	}
	
	ChangeFP_State(node, 0x21 + pos, FP_State_Closed, NULL);
	RunLog("Node[%d]FP[%d]单价已变更正确, 切换状态为Closed", node, pos + 1);

	return 0;
}

/* ----------------- End of lischn014.c ------------------- */

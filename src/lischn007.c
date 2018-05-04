/*
 * Filename: lischn007.c
 *
 * Description: 加油机协议转换模块 - 上海德莱塞稳牌, RS485, 9600,O,8,1 
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

#define GUN_EXTEND_LEN  5

#define FP_INOPERATIVE  0x00    // 未被编程
#define FP_IDLE         0x01    // 已被总清
#define FP_AUTHORISED   0x02    // 已授权
#define FP_FUELLING     0x04    // 正在加油
#define FP_EOT          0x05    // 加油完毕
#define FP_SPFUELLING   0x06    // 加油到达限定值
#define FP_CLOSED       0x07    // 关机


#define _ACK            0xC0    
#define _NAK            0x50
#define _EOT            0x70
#define _ACKPOLL        0xE0

#define TRY_TIME	5
#define INTERVAL        150000  // 150ms , 视油机而定

// TX#加一操作
#define    PLUS_TX_NO(idx)    (tx_no[idx] = (++tx_no[idx]) & 0x0F);

static int poll();
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
static int DoPrice();
static int DoStarted(int i);
static int ChangePrice();
static int AckPoll(int pos, __u8 type, __u8 tx_no);
static int GetTransData(int pos);
static int GetPumpMsg(int pos);
static int GetPumpStatus(int pos);
static int GetPumpOnline(int pos);
static int GetGunTotal(int pos, __u8 gunLog, __u8 *vol_total, __u8 *amo_total);
static int InitPumpTotal();
static int InitCurrentPrice();
static int GetCurrentLNZ(int pos);
static int SetNozMask(int pos, __u8 lnz_mask);
static int ReSet(int pos);
static int ClosePump(int pos);
static int StopPump(int pos);
static int SetPort007();
static int ReDoSetPort007();
static int keep_in_touch_xs(int x_seconds);
static int SetRCAP2L();
static int WriteRCAP2L(__u8 rcap2l);
static int TestRCAP2L_GetTotal();
static int TestRCAP2L_SetNozMask();

static PUMP pump;
static int node;
static int IFSFpos;
static DISPENSER_DATA *lpDispenser_data;
static NODE_CHANNEL *lpNode_channel;
static __u8 tx_no[MAX_FP_CNT] = {0x0F, 0x0F, 0x0F, 0x0F};   // TX#, 数据块顺序编号
static int dostop_flag[MAX_FP_CNT] = {0};  // 标记是否成功执行过 Terminate_FP
static int fp_state[MAX_FP_CNT] = {0}; 
static int pre_flg[MAX_FP_CNT] = {0};      // 1 - 抬枪后需要下发预置; 2 - 已经下发预置未加油
static __u8 glb_pre_volume[MAX_FP_CNT][5] = {0};
static __u8 glb_pre_amount[MAX_FP_CNT][5] = {0};
static int pre_ration_type[MAX_FP_CNT] = {0};

// -1 - 加油后未挂枪, 0 - IDLE, 1 - CALLING , 2 - 已完成DoStarted, 3 - FUELLING

typedef struct {
	__u8 status;             // 基本状态, 对应 DC1.STATUS
	__u8 lnz_state;          // 油枪状态, 对应 DC3.NOZIO
	__u8 current_vol[4];     // 实时加油量, 对应 DC2.VAL
	__u8 current_amo[4];     // 实时加油金额, 对应 DC2.AMO
	__u8 current_pri[3];     // 当前单价, 对应 DC3.PRI
	__u8 vol_total[GUN_EXTEND_LEN];  // 该次所取油量总累
	__u8 amo_total[GUN_EXTEND_LEN];  // 该次所取金额总累
} PUMP_MSG;

typedef struct {
	unsigned int lnz1:1;
	unsigned int lnz2:1;
	unsigned int lnz3:1;
	unsigned int lnz4:1;
	unsigned int lnz5:1;
	unsigned int lnz6:1;
	unsigned int lnz7:1;
	unsigned int lnz8:1;
} BITLNZ;

PUMP_MSG pump_msg[MAX_FP_CNT];

int ListenChn007(int chnLog)
{
	long	indtyp;
	int	ret;
	int	fd;
	int	j;
	int	i;
	__u8 status;
	
	sleep(5);   //防止此进程退出,而主进程没有启动完毕.
	RunLog("Listen Channel  007 ,logic channel[%d]", chnLog);
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
						// !!!Note.稳牌通讯地址为 79 + 面板上的显示的地址值
						pump.panelPhy[j] = gpIfsf_shm->gun_config[idx].phyfp + 79;
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
	bzero(&pump_msg, sizeof(PUMP_MSG) * MAX_FP_CNT);
	
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

	ret = SetPort007();
	if (ret < 0) {
		RunLog("设置串口属性失败");
		return -1;
	}
	
	// ============== Part.3 判断加油机是否在线 ====================================
	// Check wether dispenser is online, if it's not online, wait  here.
	while (1) {
		ret = GetPumpOnline(0);
		if (ret == 0) {
			break;
		}

		gpIfsf_shm->node_online[IFSFpos] = 0; //不在线,不产生心跳.
		UserLog("Node %d 不在线", node);
		
		sleep(1);
	}
	
	ret = SetRCAP2L();
	if (ret < 0) {
		return -1;
	}

	// 复位TX#
	for (i = 0; i < pump.panelCnt; ++i) {
		if (GetPumpOnline(i) < 0) {
			return -1;
		}
	}
	// ============= Part.4 初始化与加油机当前状态相关的数据库相关项 =======================
	
	// 联机即进入主控模式, 无需设置

	ret = InitCurrentPrice(); //  初始化fp数据库的当前油价
	if (ret < 0) {
		RunLog("初始化油品单价到数据库失败");
		return -1;
	}

	for (i = 0; i < pump.panelCnt; ++i) { // 允许所有油枪加油
		ret = SetNozMask(i, 0xFF);
		if (ret < 0) {
			return -1;
		}
	}

	for (i = 0; i < pump.panelCnt; ++i) {
		// Step.1 更新PUMP_MSG结构
		ret = GetPumpStatus(i);
		if (ret < 0) {
			RunLog("初始化pump_msg失败");
			return -1;
		}
		// Step.2 判断当前状态是否可以进入正常 DoPolling 流程
		// 如果是加油中或者已授权, 那就等待... 
		RunLog("status: %02x, lnz_state: %02x", pump_msg[i].status, pump_msg[i].lnz_state);
		if (pump_msg[i].status == FP_FUELLING ||
			pump_msg[i].lnz_state != 0) {
			sleep(1);
			i = -1;
		}

		if (pump_msg[i].status == FP_AUTHORISED) {
			DoStop(i);
			i = -1;
		}
	}

	// 泵码要到挂枪后才会更新
	ret = InitPumpTotal();    // 初始化泵码到数据库
	if (ret < 0) {
		RunLog("初始化泵码到数据库失败");
		return -1;
	}

//	gpIfsf_shm->node_online[IFSFpos] = node;  // 一切就绪, 上线
	ChangeNode_Status(node, NODE_ONLINE);
	RunLog("节点 %d 在线!", node);
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
			GetPumpStatus(0);              // Note. 与油机保持联系
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
			Ssleep(100000);  // 100ms, 根据具体情况调整
			DoPolling();
		}
	} // end of main endless loop

}



/*
 * func: 获取当前抬起油枪的枪号, 结果写入pump.gunLog[] & fp_data[].Log_Noz_State
 * ret : 0 - 成功, -1 - 失败 
 */
static inline int GetCurrentLNZ(int pos)
{
	pump.gunLog[pos] = pump_msg[pos].lnz_state;
	RunLog("Node %d FP %d 的 %d 号枪抬起", node, pos + 1, pump.gunLog[pos]);
	lpDispenser_data->fp_data[pos].Log_Noz_State = ((__u8)1 << (pump_msg[pos].lnz_state - 1));

	return 0;
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
			ret = DoStop(pos);
			if (ret == 0) {
				ret = ChangeFP_State(node, pos + 0x21, FP_State_Idle, msgbuffer);
				dostop_flag[pos] = 1;
				pre_flg[pos] =0;
				pre_ration_type[pos] = 0;				
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

			GetPumpStatus(pos);

			// 检查一下油枪是否还是抬起状态
			if (pump_msg[pos].lnz_state == 0) {
				ret = -1;
			} else {
				ret = DoAuth(pos);				
			}

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
			// 恢复允许所有油枪
			if (pre_flg[pos] != 1) {
			    lpDispenser_data->fp_data[pos].Log_Noz_Mask = 0xFF;
			}
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
	int tmp_lnz,pr_no;
	int fm_idx;
	__u8 status;
	static __u8 pre_status[MAX_FP_CNT] = {0};
	static int times = 0;
	static int fail_cnt = 0;  // 记录GetPumpStatus 失败次数, 用于判定离线
	
	for (i = 0; i < pump.panelCnt; i++) {
		ret = GetPumpStatus(i);
		if (ret < 0) {
			if (gpIfsf_shm->node_online[IFSFpos] == node) {
				if ((++fail_cnt) < 3) {  // 加油机离线判断, 稳重启时间比较短
					/*
					 * 因重设串口属性会导致ReadTTY接收到"非法数据", 故暂时移除
					 *
					if (fail_cnt >= 2) {
						if (SetPort007() >= 0) {
							RunLog("节点%d重设串口属性成功", node);
						}
					}
					*/
					continue;
				}

				// Step.1
				memset(dostop_flag, 0, sizeof(dostop_flag));
				memset(fp_state, 0, sizeof(fp_state));
				bzero(glb_pre_volume, sizeof(glb_pre_volume));
				bzero(glb_pre_amount, sizeof(glb_pre_amount));

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
			// ReDoSetPort007();
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

				for (j = 0; j < pump.panelCnt; ++j) {
					lpDispenser_data->fp_data[j].FP_State = FP_State_Closed;
				}

				for (j = 0; j < pump.panelCnt; ++j) { // 默认允许所有油枪加油
					ret = SetNozMask(j, 0xFF);
//					SetNozMask(j, lpDispenser_data->fp_data[j].Log_Noz_Mask);
					if (ret < 0) {
						return;
					}
				}

				for (j = 0; j < pump.panelCnt; ++j) { // 状态准备
					// Step.1 更新PUMP_MSG结构
					ret = GetPumpStatus(j);
					if (ret < 0) {
						RunLog("初始化pump_msg失败");
						return;
					}
					if (pump_msg[j].lnz_state != 0) {
						RunLog("油枪为抬起状态");
						// 油枪抬起状态下读取泵码可能不准确, 所以等待...
						return;                // waiting
					}
#if 1
					// Step.2 判断当前状态是否可以进入正常 DoPolling 流程
					// 如果是加油中或者已授权, 那就等待... 
					if (pump_msg[j].status == FP_FUELLING) {
						Ssleep(INTERVAL);
						j = 0;   // 重新
					} else if (pump_msg[j].status == FP_AUTHORISED) {
						DoStop(j);
						j = 0;
					}
#endif
				}

				// 确认油枪为挂枪状态才读取泵码
				ret = InitPumpTotal();    // 更新泵码到数据库
				if (ret < 0) {
					RunLog("更新泵码到数据库失败");
					continue;
				}


				// 一起都准备好了, 上线
//				gpIfsf_shm->node_online[IFSFpos] = node;
				ChangeNode_Status(node, NODE_ONLINE);
				bzero(pre_flg,sizeof(pre_flg));
				bzero(pre_ration_type,sizeof(pre_ration_type));
				RunLog("====== Node %d 恢复联线! =========================", node);
				keep_in_touch_xs(12);               // 确保CD已经收到心跳之后再做后续操作
				// Step.                 // 主动上传当前FP状态
				for (j = 0; j < pump.panelCnt; ++j) {
					ChangeFP_State(node, j + 0x21, FP_State_Closed, NULL);
				}
				continue;
			}
			
			// Note. 如果油机没有收到后台命令若干秒后会自动转换为自主加油模式
			//       那么恢复通讯后判断通讯失败时间是否超过油机的超时时间值, 
			//       若超过则需重设主控模式
			//       ## 稳牌一联机即主控, 无需设置
			if (fail_cnt >= 1) {
			//  断电重启后一定要重设允许油枪
				for (j = 0; j < pump.panelCnt; ++j) { // 默认允许所有油枪加油
					ret = SetNozMask(j, 0xFF);
//					ret = SetNozMask(j, lpDispenser_data->fp_data[j].Log_Noz_Mask);
					if (ret < 0) {
						return;
					}
				}

			//  断电重启很快, 一定要更新状态
				for (j = 0; j < pump.panelCnt; ++j) {
					// Step.1 更新PUMP_MSG结构
					ret = GetPumpStatus(j);
					if (ret < 0) {
						RunLog("初始化pump_msg失败");
						return;
					}
				}
			}
			fail_cnt = 0;
		}

		// 仅在状态字有变化的时候才打印(写日志)
		if (pump_msg[i].status != pre_status[i]) {
			pre_status[i] = pump_msg[i].status;
			RunLog("节点[%d]面板[%d]当前状态字为[%02x]", node, i + 1, pump_msg[i].status);
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
			continue;
		}

		
		switch (pump_msg[i].status) {
		case FP_INOPERATIVE:
			//Modify by liys  加油机上电时为未编程状态，需要进行一次下发油价			
			fm_idx = lpDispenser_data->fp_data[i].Default_Fuelling_Mode - 1;
			for (j = 0; j < pump.gunCnt[i]; ++j) {
					pr_no = (lpDispenser_data->fp_ln_data[i][j]).PR_Id - 1;  // 0 - 7	
					if (lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2] != 0x00 ||
						lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3] != 0x00) {
						memcpy(&pump.newPrice[pr_no],
							&(lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price), 4);
					}
			}
			
			ChangePrice();          // 未被编程状态可能需要变价

			GetPumpStatus(i);
			break;
		case FP_IDLE:                   // 已总清
			//- 是否 Calling ?
			/*
			RunLog("Idle ---------  [%d].lnz_state: %02x, dostop_flag: %d", 
 						i, pump_msg[i].lnz_state, dostop_flag[i]);
			*/
			if (pump_msg[i].lnz_state != 0 && dostop_flag[i] == 0) {  //Modify by liys
			       
			       if (pre_flg[i] == 1){	
					GetCurrentLNZ(i);
					// 判断是否预置该枪加油, 如果不是则break
					if ((lpDispenser_data->fp_data[i].Log_Noz_Mask != pump_msg[i].lnz_state) &&
								(lpDispenser_data->fp_data[i].Log_Noz_Mask != 0xFF)) {
					    break;
					}
					
					memcpy(lpDispenser_data->fp_data[i].Remote_Volume_Preset,&glb_pre_volume[i],5);
					memcpy(lpDispenser_data->fp_data[i].Remote_Amount_Prepay,&glb_pre_amount[i],5);
					ret = DoRation(i);
					if (ret == 0) {
						pre_flg[i] = 2;
					} else {
						pre_flg[i] = 0;
					}
					bzero(&lpDispenser_data->fp_data[i].Remote_Volume_Preset, 5);
					bzero(&lpDispenser_data->fp_data[i].Remote_Amount_Prepay, 5);
					bzero(glb_pre_volume[i], sizeof(glb_pre_volume[i]));
					bzero(glb_pre_amount[i], sizeof(glb_pre_amount[i]));
					pre_ration_type[i] = 0;

					break;
				} else if (pre_flg[i] != 2) {
					goto DO_CALLING;
				}
			}

			// 以下是挂枪状态下的处理
			// 若状态不是IDLE, 则切换为Idle 
			if (pump_msg[i].lnz_state == 0) {
				lpDispenser_data->fp_data[i].Log_Noz_State = 0;
			}

			if (((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State) &&
				((lpDispenser_data->fp_data[i].FP_State != FP_State_Authorised) ||
						(pump_msg[i].lnz_state == 0 && pre_flg[i] == 2))) {
				// 如果已经下发了预置,并且现在是挂枪的状态, 那么表明预置已经结束或者取消
				// 这里改变状态为Idle
				pre_flg[i] = 0;
				ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
			}

			for (j = 0; j < pump.panelCnt; ++j) {
				if ((__u8)FP_State_Idle != lpDispenser_data->fp_data[j].FP_State) {
					break;
				}
			}
			if (j >= pump.panelCnt) {
				ret = ChangePrice();          // 查看是否有油品需要变价,有则执行
			}
			break;
		case FP_EOT:                     // 交易完毕, 执行停止之后也是此状态, 或重启后的初始状态
			pre_flg[i] = 0;

			//- 是否 Calling ?
			if (pump_msg[i].lnz_state != 0) {
				goto DO_CALLING;
			}

			if (pump_msg[i].lnz_state == 0) {
				dostop_flag[i] = 0; // (FP_EOT && lnz_state == 0), 所以dostop_flag可以清零了
			} else if (dostop_flag[i] != 0) {
						// 执行了停止命令的情况下, 要等挂枪泵码才会更新, 所以..
				break;          // 继续轮询
			}

			//- 交易处理
			if (fp_state[i] >= 2) {
				// 若有加过油, 则进行状态改变和交易处理
				// 1. ChangeFP_State to IDLE first
				//   Log_Noz_State 反映真是油枪状态(这里唯一的可能是挂枪又快速抬枪)
				if (lpDispenser_data->fp_data[i].Log_Noz_State != pump_msg[i].lnz_state) {
					tmp_lnz = lpDispenser_data->fp_data[i].Log_Noz_State;
					lpDispenser_data->fp_data[i].Log_Noz_State = pump_msg[i].lnz_state;
					if ((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State){
					    ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
					}
				}

				if ((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State) {
					ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
				}

				// Step2. Get & Save TR to TR_BUFFER
				lpDispenser_data->fp_data[i].Log_Noz_State = tmp_lnz;
				ret = DoEot(i);
				lpDispenser_data->fp_data[i].Log_Noz_State = pump_msg[i].lnz_state;
			}

			fp_state[i] = 0;
			if ((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State) {
				ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
			}

			//- 脱机交易处理
			EO_OFFLINE_TR(node, i);
			if (gpIfsf_shm->fp_offline_flag[node - 1][i] != 0) {
				upload_offline_tr(node, NULL);
			}


			//- 变价操作
			for (j = 0; j < pump.panelCnt; ++j) {
				if ((__u8)FP_State_Idle != lpDispenser_data->fp_data[j].FP_State) {
					break;
				}
			}

			if (j >= pump.panelCnt) {
				ret = ChangePrice();          // 查看是否有油品需要变价,有则执行
			}

			break;

//----------------------------------------------------------------------------------------------//
DO_CALLING:             // 抬枪处理

			if (((__u8)FP_State_Calling > lpDispenser_data->fp_data[i].FP_State) &&
					(dostop_flag[i] ==0)) {
				// add dostop_flag 主要是避免在预制后抬错枪，之后下发停止命令的情况
				//- 不管是刚初始化还是交易结束还是执行了停止命令, 都进行总清
				// 如果执行过停止命令, 到这一步如果枪是抬着的, 那一定是挂枪重新抬枪了

				ret = ReSet(i);
				if (ret < 0 || (ret == 0 && pump_msg[i].lnz_state == 0)) {
					// 总清失败或者油枪已挂
					break;
				}

				ret = DoCall(i);
				
				fp_state[i] = 1;

				ret = DoStarted(i);
			}

			break;  // DO_CALLING 结束
//----------------------------------------------------------------------------------------------//
		case FP_AUTHORISED:
			if (pump_msg[i].lnz_state == 0) {
				ret = DoStop(i);   // 达到抬挂枪取消授权的效果
				if (ret == 0) {
					lpDispenser_data->fp_data[i].Log_Noz_State = 0;
				}
				break;
			} 
			
			if ((__u8)FP_State_Authorised > lpDispenser_data->fp_data[i].FP_State) {
				ret = ChangeFP_State(node, i + 0x21, FP_State_Authorised, NULL);
			}

			// 已授权状态提枪转Started
			if (pump_msg[i].lnz_state != 0 &&
				(lpDispenser_data->fp_data[i].Log_Noz_State == 0)) {
				if ((__u8)FP_State_Authorised == lpDispenser_data->fp_data[i].FP_State) {
					if (fp_state[i] < 2) {
						if (DoStarted(i) < 0) {
							continue;
						}
					}

					ret = ChangeFP_State(node, i + 0x21, FP_State_Started, NULL);
				}
				break;
			}

			break;
		case FP_FUELLING:                               // 加油
DO_BUSY:		
			if (dostop_flag[i] != 0) {              // add by liys
				break;
			}
			   
			if (fp_state[i] < 2) {
				if (DoStarted(i) < 0) {
					continue;
				}
			}

			if ((__u8)FP_State_Started > lpDispenser_data->fp_data[i].FP_State) {
				ret = ChangeFP_State(node, i + 0x21, FP_State_Started, NULL);
			}

			DO_INTERVAL(DO_BUSY_INTERVAL, ret = DoBusy(i););

			if ((__u8)FP_State_Fuelling > lpDispenser_data->fp_data[i].FP_State) {
				if (fp_state[i] == 3) {
					ret = ChangeFP_State(node, i + 0x21, FP_State_Fuelling, NULL);
					if (ret < 0) {
						RunLog("Node %d Change FP %d State to Fuelling failed", node, i + 1);
						break;
					}
				}
			}

			break;
		case FP_SPFUELLING:
			DO_INTERVAL(DO_BUSY_INTERVAL, ret = DoBusy(i););
#if 0
			if ((__u8)FP_State_Suspended_Fuelling != lpDispenser_data->fp_data[i].FP_State) {
				ret = ChangeFP_State(node, i + 0x21, FP_State_Suspended_Fuelling, NULL);
			}
#endif
			break;
		case FP_CLOSED:
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
static int DoOpen(int pos, __u8 *msgbuffer) 
{
	int ret;
	int i;

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
	int idx, ret;
	int trycnt;
	int ration_type = 0;
	__u16 crc = 0;
	__u8 pre_volume[4] = {0};
	//if ((lpNode_channel->cntOfFpNoz[pos] == 1)||pre_flg[pos] == 1){ //如果为单面多枪机情况的话，则在抬枪的时候还需要再次下发DoRation
	if (pre_flg[pos] == 1) { //根据厂家人员描述,所有有型号的油枪预置流程都为抬枪后再预置授权
		ration_type = pre_ration_type[pos];
		if (ration_type == 0x03) {
			memcpy(&pre_volume[1], &lpDispenser_data->fp_data[pos].Remote_Volume_Preset[2], 3);
		} else {
			memcpy(&pre_volume[1], &lpDispenser_data->fp_data[pos].Remote_Amount_Prepay[2], 3);
		}

		// Setp.0 确保总清, 否则预置不成功(不能在执行DoAuth的时候才总清, 因为那时候总清会取消预置量)
		if (pump_msg[pos].status != FP_IDLE ) {
			if (ReSet(pos) < 0) {
				return -1;
			}
		}

		// Step.1  设置允许枪号, 这步一定要在判断了 pump.cmd[0] 之后进行
		RunLog("Node[%d].FP[%d].Log_Noz_Mask: %d", node, pos + 1, lpDispenser_data->fp_data[pos].Log_Noz_Mask);
		SetNozMask(pos, lpDispenser_data->fp_data[pos].Log_Noz_Mask);
		lpDispenser_data->fp_data[pos].Log_Noz_Mask = 0xFF;
		//RunLog("============= ration_type: %02x , pre_volume: %s=============", ration_type, Asc2Hex(pre_volume, 4));
		// Step.2
		PLUS_TX_NO(pos);

		for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {
			pump.cmd[0] = pump.panelPhy[pos];   // ADR
			pump.cmd[1] = 0x30 | tx_no[pos];    // CTRL
			pump.cmd[2] = ration_type;
			pump.cmd[3] = 0x04;   // LNG
			memcpy(&pump.cmd[4], pre_volume, 4);
			crc = CRC_16(&pump.cmd[0], 8);
			idx = 8;
			if ((crc & 0x00FF) == 0xFA) {
				pump.cmd[idx++] = 0x10;   // 数据连接码 DEL(0x10)
			}
			pump.cmd[idx++] = crc & 0x00FF;

			if ((crc >> 8) == 0xFA) {
				pump.cmd[idx++] = 0x10;
			}
			pump.cmd[idx++] = crc >> 8;
			pump.cmd[idx++] = 0x03;
			pump.cmd[idx++] = 0xfa;
			pump.cmdlen = idx;                // 12 or 13
			pump.wantLen = 3;

			Ssleep(INTERVAL);
			ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
					pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
			if (ret < 0) {
				RunLog("发预置加油命令到Node[%d].FP[%d]失败", node, pos + 1);
				continue;
			}

			RunLog("Node[%d].FP[%d].Pre_xxx:%s", node, pos + 1, Asc2Hex(pump.cmd, pump.cmdlen));

			memset(pump.cmd, 0, sizeof(pump.cmd));
			pump.cmdlen = pump.wantLen;
			ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
			if (ret < 0) {
				RunLog("发预置加油命令到Node[%d].FP[%d]成功, 取返回信息失败", node, pos + 1);
				continue;
			} 

			// 不管是返回 NAK 还是 TX# 不符, 均重试
			if (pump.cmd[1] == (_NAK | tx_no[pos])) {
				tx_no[pos] = 0x00;   // 错误恢复
				continue;
			} else if ((pump.cmd[1] & 0x0F) != tx_no[pos]) {
				tx_no[pos] = 0x00;   // 错误恢复
				continue;
			}

				  
			ret = DoAuth(pos);
			if (ret < 0) {
				return -1;
			}	       
		
			return 0;
		}
	} else {

		memcpy(glb_pre_volume[pos],lpDispenser_data->fp_data[pos].Remote_Volume_Preset,5);
		memcpy(glb_pre_amount[pos],lpDispenser_data->fp_data[pos].Remote_Amount_Prepay,5);
		pre_flg[pos] = 1;			  

		if (pump.cmd[0] == 0x3D) {   // Preset
			ration_type = 0x03;
		} else {
			ration_type = 0x04;
		}	  
 		pre_ration_type[pos] = ration_type;

		// Setp.0 确保总清, 否则预置不成功(不能在执行DoAuth的时候才总清, 因为那时候总清会取消预置量)
		if (pump_msg[pos].status != FP_IDLE ) {
		     if (ReSet(pos) < 0) {
			   return -1;
		     }
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
 *       因稳的变价命令不同于其他油机协议, 此ChangePrice与其他模块有较大差别
 */
static int ChangePrice() 
{
	int i, j;
	int ret;
	int trycnt;
	int pr_no;
	int fm_idx;
	int need_change = 0;          // 标识这个面的所有油品中是否有需要变价的
	int error_code_msg_cnt = 0;   // 需要反馈给Fuel的error code信息的条数;
	__u8 lnz;
	__u8 err_code = 0;
	__u16 crc;

/*
 * 遍历新单价数组newPrice[FP_PR_NB_CNT], 如果有油品需要变价--数组值不为零,
 * 则遍历所有FP, 如果油品匹配就下发变价命令
 */

	for (pr_no = 0; pr_no < FP_PR_NB_CNT; pr_no++) {
		if (pump.newPrice[pr_no][2] != 0x00 || pump.newPrice[pr_no][3] != 0x00) {
			RunLog(">>>1 newPrice[%d]: %02x.%02x", pr_no,
					pump.newPrice[pr_no][2], pump.newPrice[pr_no][3]);
			for (i = 0; i < pump.panelCnt; i++) {
				CriticalLog("下发变价命令给节点[%02d]面[%d]枪[*]......", node, i + 1);
				PLUS_TX_NO(i);
				fm_idx = lpDispenser_data->fp_data[i].Default_Fuelling_Mode - 1;
				if (fm_idx < 0 || fm_idx > 7) {
					RunLog("Node[%d]FP[%d] Default FM Error[%d]", node, i + 1, fm_idx + 1);
					return -1;
				}

				err_code = 0x99;
				error_code_msg_cnt = 0;
				for (trycnt = 0; trycnt < TRY_TIME; trycnt++) {
					memset(&pump.cmd, 0, sizeof(pump.cmd));
					need_change = 0;                  // clear
					pump.cmd[0] = pump.panelPhy[i];   // ADR
					pump.cmd[1] = 0x30 | tx_no[i];    // CTRL
					pump.cmd[2] = 0x05;               // TRANS
					for (j = 0; j < pump.gunCnt[i]; ++j) {
						pr_no = (lpDispenser_data->fp_ln_data[i][j]).PR_Id - 1;  // 0 - 7	
						if ((pr_no < 0) || (pr_no > 7)) {
							RunLog("ChangePrice PR_Id error, PR_Id[%d] <ERR>", pr_no + 1);
							return -1;
						}
						if (pump.newPrice[pr_no][2] != 0x00 || pump.newPrice[pr_no][3] != 0x00) {
							memcpy(&pump.cmd[4 + j * 3], &pump.newPrice[pr_no][1], 3);
							need_change = 1;
							error_code_msg_cnt += pump.newPrice[pr_no][0];
						} else {
							memcpy(&pump.cmd[4 + j * 3],
								&lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[1], 3);
						}
					}

					if (need_change == 0) {
						RunLog("FP %d 无需变价", i + 1);
						tx_no[i]--;  // Note! 不减1会导致TX不连续, 以致变价失败;
						break;       // 处理下一个面
					}

					pump.cmd[3] = pump.gunCnt[i] * 3;
					j = 4 + pump.gunCnt[i] * 3;
					crc = CRC_16(&pump.cmd[0], j);
					if ((crc & 0x00FF) == 0xFA) {
						pump.cmd[j++] = 0x10;  // 数据连接码 DEL
					}
					pump.cmd[j++] = crc & 0x00FF;

					if ((crc >> 8) == 0xFA) {
						pump.cmd[j++] = 0x10;
					}
					pump.cmd[j++] = crc >> 8;
					pump.cmd[j++] = 0x03;
					pump.cmd[j++] = 0xfa;
					pump.cmdlen = j;
					pump.wantLen = 3;

					Ssleep(INTERVAL);
					ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
							pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
					if (ret < 0) {
						RunLog("发变价命令到Node[%d].FP[%d]失败", node, i + 1);
						continue;
					}

					memset(pump.cmd, 0, sizeof(pump.cmd));
					pump.cmdlen = sizeof(pump.cmd) - 1;
					ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
					if (ret < 0) {
						RunLog("发变价命令到Node[%d].FP[%d]成功, 取返回信息失败",
												node, i + 1);
						Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
						continue;
					} 

					// 不管是返回 NAK 还是 TX# 不符, 均重试
					if (pump.cmd[1] == (_NAK | tx_no[i])) {
						tx_no[i] = 0x00;
						Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
						continue;
					} else if ((pump.cmd[1] & 0x0F) != tx_no[i]) {
						tx_no[i] = 0x00;   // 错误恢复
						Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
						continue;
					}


					// Step.2 根据命令返回信息或者重读油机单价确认变价是否成功
					
					// Step.3 写新单价到油品数据库 & 清零单价缓存
					for (j = 0; j < pump.gunCnt[i]; ++j) {
						pr_no = (lpDispenser_data->fp_ln_data[i][j]).PR_Id - 1;  // 0 - 7	
						if (pump.newPrice[pr_no][2] != 0x00 || pump.newPrice[pr_no][3] != 0x00) {

					RunLog("Node[%d]发送变价命令成功,新单价fp_fm_data[%d][%d].Prode_Price: %s",
						node, pr_no, fm_idx,
						Asc2Hex(lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price, 4));
					CriticalLog("节点[%02d]面[%d]枪[%d]变价成功,新单价为%02x.%02x",
					       	node, i + 1, j + 1,
						lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2],
						lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3]);
						}
					}

					break;
				}

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
						pump.newPrice[pr_no][2], 
						pump.newPrice[pr_no][3], err_code);
				}

				RunLog("---------- Node[%d]FP[%d] ER_DATA, error code: %02x -----", node, i + 1, err_code);

				int tmp_idx;
				for (tmp_idx = 0; tmp_idx < error_code_msg_cnt; tmp_idx++) {
					ChangeER_DATA(node, 0x21 + i, err_code, NULL); // Translater已经回复了ACK
				}

				if (err_code != 0x99) {
					ChangeFP_State(node, 0x21 + i, FP_State_Inoperative, NULL);
				}

				Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
			} // end of for.i < panelCnt
			bzero(&pump.newPrice, FP_PR_NB_CNT * 4);   // Note. 清零全部
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
	__u8 vol_total[5] = {0};
	__u8 amo_total[5] = {0};
	__u8 total[7];
	__u8 pre_total[7];

	RunLog("节点[%d]面板[%d]处理新的交易", node, pos + 1);

	err_cnt = 0;
	do {
		ret = GetGunTotal(pos, pump.gunLog[pos], vol_total, amo_total);
		if (ret < 0) {
			err_cnt++;
			RunLog("节点[%d]面板[%d]读取总累记录失败", node, pos + 1);
		}
	} while (err_cnt <= 5 && ret < 0);

	if (ret < 0) {
		return -1;
	}
	total[0] = 0x0A;
	total[1] = 0x00;
	memcpy(&total[2], vol_total, 5);

	pr_no = (lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).PR_Id - 1;  // 0 - 7	
	if ((pr_no < 0) || (pr_no > 7)) {
		RunLog("DoEot PR_Id error, PR_Id[%d] LNZ[%d] <ERR>", pr_no + 1, pump.gunLog[pos]);
	}
	
	memcpy(pre_total, (lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Vol_Total, 7);
       
	if (memcmp(pre_total, total, 7) != 0) {
		err_cnt = 0;
		do {
			ret = GetTransData(pos);
			if (ret < 0) {
				err_cnt++;
				RunLog("节点[%d]面板[%d]读取交易记录失败", node, pos + 1);
			}
		} while (err_cnt <= 5 && ret < 0);

		if (ret < 0) {
			return -1;
		}
		// Volume
		volume[0] = 0x06;
		memcpy(&volume[1], pump_msg[pos].current_vol, 4);		
		bzero(pump_msg[pos].current_vol,sizeof(pump_msg[pos].current_vol));
		memcpy(lpDispenser_data->fp_data[pos].Current_Volume, volume, 5);

		// Amount
		amount[0] = 0x06;
		memcpy(&amount[1], pump_msg[pos].current_amo, 4);
		bzero(pump_msg[pos].current_amo,sizeof(pump_msg[pos].current_amo));
		memcpy(lpDispenser_data->fp_data[pos].Current_Amount, amount, 5);

		// Price
		price[0] = 0x04;
		memcpy(&price[1], pump_msg[pos].current_pri, 3);
		bzero(pump_msg[pos].current_pri,sizeof(pump_msg[pos].current_pri));
		memcpy(lpDispenser_data->fp_data[pos].Current_Unit_Price, price, 4);

		// 更新 Volume_Total
		memcpy(lpDispenser_data->fp_m_data[pr_no].Meter_Total, total, 7);
		memcpy((lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Vol_Total, total, 7);

#if 0
		RunLog("节点[%02d]面板[%d]枪[%d] ## Amount: %02x%02x.%02x Volume: %02x%02x.%02x Price: %02x.%02x Total: %02x%02x%02x.%02x", node, pos + 1, pump.gunLog[pos], amount[2], 
					amount[3], amount[4], volume[2], volume[3], volume[4], price[2],
							price[3], total[3], total[4], total[5], total[6]);
#endif

		// 更新 Amount_Total
		memcpy(&total[2], amo_total, 5);
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
 * func:设置允许枪号
 */

static int SetNozMask(int pos, __u8 lnz_mask)
{
	int i;
	int ret;
	int trycnt;
	__u16 crc = 0;
	BITLNZ *plnz = NULL;

	plnz = (BITLNZ*)&lnz_mask;

	RunLog("Node[%d].FP[%d] set LNZ mask (%02x)", node, pos + 1, lnz_mask);

	PLUS_TX_NO(pos);
	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {
		pump.cmd[0] = pump.panelPhy[pos];   // ADR
		pump.cmd[1] = 0x30 | tx_no[pos];    // CTRL
		pump.cmd[2] = 0x02;   // TRANS

		// 枪号
		i = 0;

		if (plnz->lnz1 != 0 && pump.gunCnt[pos] > i) {
			pump.cmd[i + 4] = 0x01;
			i++;
		}
		if (plnz->lnz2 != 0 && pump.gunCnt[pos] > i) {
			pump.cmd[i + 4] = 0x02;
			i++;
		}
		if (plnz->lnz3 != 0 && pump.gunCnt[pos] > i) {
			pump.cmd[i + 4] = 0x03;
			i++;
		}
		if (plnz->lnz4 != 0 && pump.gunCnt[pos] > i) {
			pump.cmd[i + 4] = 0x04;
			i++;
		}
		if (plnz->lnz5 != 0 && pump.gunCnt[pos] > i) {
			pump.cmd[i + 4] = 0x05;
			i++;
		}
		if (plnz->lnz6 != 0 && pump.gunCnt[pos] > i) {
			pump.cmd[i + 4] = 0x06;
			i++;
		}
		if (plnz->lnz7 != 0 && pump.gunCnt[pos] > i) {
			pump.cmd[i + 4] = 0x07;
			i++;
		}
		if (plnz->lnz8 != 0 && pump.gunCnt[pos] > i) {
			pump.cmd[i + 4] = 0x08;
			i++;
		}


		pump.cmd[3] = i;          // 油枪数量
		i += 4;
		crc = CRC_16(&pump.cmd[0], i);
		if ((crc & 0x00FF) == 0xFA) {
			pump.cmd[i++] = 0x10;       // 数据连接码 DEL
		}
		pump.cmd[i++] = crc & 0x00FF;

		if ((crc >> 8) == 0xFA) {
			pump.cmd[i++] = 0x10;
		}
		pump.cmd[i++] = crc >> 8;
		pump.cmd[i++] = 0x03;
		pump.cmd[i++] = 0xfa;
		pump.cmdlen = i;
		pump.wantLen = 3;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("发允许油枪设置命令到Node[%d].FP[%d]失败", node, pos + 1);
			continue;
		}

		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("发允许油枪设置命令到Node[%d].FP[%d]成功, 取返回信息失败", node, pos + 1);
			continue;
		} 

		// 不管是返回 NAK 还是 TX# 不符, 均重试
		if (pump.cmd[1] == (_NAK | tx_no[pos])) {
			tx_no[pos] = 0x00;   // 错误恢复
			continue;
		} else if ((pump.cmd[1] & 0x0F) != tx_no[pos]) {
			tx_no[pos] = 0x00;   // 错误恢复
			continue;
		}

		return 0;
	}
	return -1;
}


/*
 * func: 获取该FP所有枪的泵码
 */
static int GetPumpTotal(int pos)
{
	return 0;
}

/*
 * func: 读取当次交易记录
 * return: 0 - 成功, 1 - 成功但无交易, -1 - 失败
 */ 
static int GetTransData(int pos)
{
	int idx,ret;
	int trycnt;
	__u16 crc = 0;

	PLUS_TX_NO(pos);
	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {
		// Step.1 请求返回加油信息
		pump.cmd[0] = pump.panelPhy[pos];   // ADR
		pump.cmd[1] = 0x30 | tx_no[pos];    // CTRL
		pump.cmd[2] = 0x01;   // TRANS
		pump.cmd[3] = 0x01;   // LNG
		pump.cmd[4] = 0x04;   // DCC
		crc = CRC_16(&pump.cmd[0], 5);
		idx = 5;
		if ((crc & 0x00FF) == 0xFA) {
			pump.cmd[idx++] = 0x10;   // 数据连接码 DEL(0x10)
		}
		pump.cmd[idx++] = crc & 0x00FF;

		if ((crc >> 8) == 0xFA) {
			pump.cmd[idx++] = 0x10;
		}
		pump.cmd[idx++] = crc >> 8;
		pump.cmd[idx++] = 0x03;
		pump.cmd[idx++] = 0xfa;
		pump.cmdlen = idx;                


		pump.wantLen = 3;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("发取交易命令到Node[%d].FP[%d]失败", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("发取交易命令到Node[%d].FP[%d]成功, 取返回信息失败", node, pos + 1);
			continue;
		} 

		// 不管是返回 NAK 还是 TX# 不符, 均重试
		if (pump.cmd[1] == (_NAK | tx_no[pos])) {
			tx_no[pos] = 0x00;   // 错误恢复
			continue;
		} else if ((pump.cmd[1] & 0x0F) != tx_no[pos]) {
			tx_no[pos] = 0x00;   // 错误恢复
			continue;
		}

		// Step.2 轮询接收加油信息
		ret = GetPumpMsg(pos);
		RunLog("Node[%d].FP[%d] Get Trans Data, ret: %d", node, pos + 1, ret);
		if (ret < 0 || (ret & 0x02) == 0) {
			continue;
		}

		// 交易信息存储于pump_msg[].current_xxx

		return 0;
	}

	return -1;
}


/*
 * func: Terminate_FP
 */
static int DoStop(int pos)
{
	int idx,ret;
	int trycnt;
	__u16 crc = 0;

	RunLog("Node[%d].FP[%d] DoStop", node, pos + 1);

	PLUS_TX_NO(pos);
	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {
		pump.cmd[0] = pump.panelPhy[pos];   // ADR
		pump.cmd[1] = 0x30 | tx_no[pos];    // CTRL
		pump.cmd[2] = 0x01;   // TRANS
		pump.cmd[3] = 0x01;   // LNG
		pump.cmd[4] = 0x08;   // DCC
		crc = CRC_16(&pump.cmd[0], 5);
		idx = 5;
		if ((crc & 0x00FF) == 0xFA) {
			pump.cmd[idx++] = 0x10;   // 数据连接码 DEL(0x10)
		}
		pump.cmd[idx++] = crc & 0x00FF;

		if ((crc >> 8) == 0xFA) {
			pump.cmd[idx++] = 0x10;
		}
		pump.cmd[idx++] = crc >> 8;
		pump.cmd[idx++] = 0x03;
		pump.cmd[idx++] = 0xfa;
		pump.cmdlen = idx;           
		pump.wantLen = 3;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("发停止命令到Node[%d].FP[%d]失败", node, pos + 1);
			continue;
		}

		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("发停止命令到Node[%d].FP[%d]成功, 取返回信息失败", node, pos + 1);
			continue;
		} 

		// 不管是返回 NAK 还是 TX# 不符, 均重试
		if (pump.cmd[1] == (_NAK | tx_no[pos])) {
			tx_no[pos] = 0x00;   // 错误恢复
			continue;
		} else if ((pump.cmd[1] & 0x0F) != tx_no[pos]) {
			tx_no[pos] = 0x00;   // 错误恢复
			continue;
		}

		// 查看状态是否变化
		if (GetPumpStatus(pos) < 0) {
			continue;
		}

		if (pump_msg[pos].status != FP_EOT &&
			pump_msg[pos].status != FP_FUELLING) { //Modify by liys 
			PLUS_TX_NO(pos);
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
	int idx,ret,icount;
	int trycnt;
	__u16 crc = 0;

	RunLog("Node[%d].FP[%d] Do Auth", node, pos + 1);

	// Step.1 确保已经处于总清状态
	// 预置加油DoRation之前一定要先进行总清, 否则这里执行总清会导致预置量被取消
	if (pump_msg[pos].status != FP_IDLE) {
		if (ReSet(pos) < 0) {
			return -1;
		}
	}

	// Setp.2 下发授权
	PLUS_TX_NO(pos);
	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {
		pump.cmd[0] = pump.panelPhy[pos];   // ADR
		pump.cmd[1] = 0x30 | tx_no[pos];    // CTRL
		pump.cmd[2] = 0x01;   // TRANS
		pump.cmd[3] = 0x01;   // LNG
		pump.cmd[4] = 0x06;   // DCC
		crc = CRC_16(&pump.cmd[0], 5);
		idx = 5;
		if ((crc & 0x00FF) == 0xFA) {
			pump.cmd[idx++] = 0x10;   // 数据连接码 DEL(0x10)
		}
		pump.cmd[idx++] = crc & 0x00FF;

		if ((crc >> 8) == 0xFA) {
			pump.cmd[idx++] = 0x10;
		}
		pump.cmd[idx++] = crc >> 8;
		pump.cmd[idx++] = 0x03;
		pump.cmd[idx++] = 0xfa;
		pump.cmdlen = idx;
		pump.wantLen = 3;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("发授权命令到Node[%d].FP[%d]失败", node, pos + 1);
			continue;
		}

		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("发授权命令到Node[%d].FP[%d]成功, 取返回信息失败", node, pos + 1);
			continue;
		} 

		// 不管是返回 NAK 还是 TX# 不符, 均重试
		if (pump.cmd[1] == (_NAK | tx_no[pos])) {
			tx_no[pos] = 0x00;   // 错误恢复
			continue;
		} else if ((pump.cmd[1] & 0x0F) != tx_no[pos]) {
			tx_no[pos] = 0x00;   // 错误恢复
			continue;
		}

		/*
		 * 20系列油机在转变为已授权状态需要很长时间,
		 * 所以在此不适合在状态判断--容易导致FCC给CD的反馈时间过长
		 */

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
	static __u8 volume[5], amount[5];
	const __u8 arr_zero[4] = {0};

	// 加油量为零不转变为Fuelling状态 , 不上传Run_Trans
	if (fp_state[pos] != 3) {
		if (memcmp(arr_zero, pump_msg[pos].current_vol, 4) == 0) {
			return 0;
		}
		fp_state[pos] = 3;
		return 0;
	}

	//  取得实时加油量 ........
	
	volume[0] = 0x06;
	memcpy(&volume[1], pump_msg[pos].current_vol, 4);

	amount[0] = 0x06;
	memcpy(&amount[1], pump_msg[pos].current_amo, 4);

	if ((__u8)FP_State_Fuelling == lpDispenser_data->fp_data[pos].FP_State) {
		//只有一个面板Modify by liys 09.01.12 0x21 --> pos + 0x21
		ret = ChangeFP_Run_Trans(node, pos + 0x21, amount,  volume); 
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

	//  Log_Noz_State & BeginTr4Fp 成功, 则置位 fp_state[i] 为 2
	fp_state[i] = 2;

	return 0;
}

/*
 * func:  发送确认信息 ACK/NAK
 */

static int AckPoll(int pos, __u8 type, __u8 tx_no)
{
	int ret;
	int trycnt;
	int cmdlen;
	__u8 ack_cmd[3] = {0};

	for (trycnt = 0; trycnt < 2; ++trycnt) {
		ack_cmd[0] = pump.panelPhy[pos];   // ADR
		ack_cmd[1] = type | tx_no;         // CTRL
		ack_cmd[2] = 0xFA;                 // SF
		cmdlen = 3;

//		RunLog("-------- Send Ack: %02x ------------", ack_cmd[1]);
		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, ack_cmd,
					cmdlen, 1, 0, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("发ACK/NAK到Node[%d]失败", node);
			continue;
		}

		return 0;
	}

	return -1;
}

/*
 * func: 轮询并分析轮询得到的数据, 更新 pump_msg 结构,
 *       得到不同信息返回值不同以便调用者判断是否读到期望的数据;
 * return: -1 - 无返回信息;
 *          0 - 返回3字节(ACK/NAK/EOT);
 *          Bit1 为1 - 读到面状态;
 *          Bit2 为1 - 读到加油数据(实时or交易);
 *          Bit3 为1 - 读到油枪状态;
 *          Bit4 为1 - 读到总累;
 *
 */
static int GetPumpMsg(int pos)
{
	int ret = 0;
	int lnz_state;
	__u8 *p;

	if (poll(pos) < 0) {
		return -1;
	}

	if (pump.cmdlen == 3) {
		return 0;
	}

	ret = 0;
	p = pump.cmd + 2;
	do {
		switch (p[0]) {
		case 0x01:        // DC1, 油机状态
			ret |= 0x01;
			pump_msg[pos].status = p[2];
			break;
		case 0x02:        // DC2, 实时加油信息
			ret |= 0x02;
			memcpy(pump_msg[pos].current_vol, &p[2], 4);
			memcpy(pump_msg[pos].current_amo, &p[6], 4);
			break;
		case 0x03:        // DC3, 油枪状态和单价
			ret |= 0x04;
			memcpy(pump_msg[pos].current_pri, &p[2], 3);
			lnz_state = (p[5] & 0x0F);    // 抬挂状态
			if (((p[5] & 0x10) != 0) && lnz_state != 0) {
				if (pump_msg[pos].lnz_state != lnz_state) {
					pump_msg[pos].lnz_state = lnz_state;
					RunLog("Node %d FP %d LNZ %d up......, gunLog: %02x", 
						node, pos + 1, pump_msg[pos].lnz_state, pump.gunLog[pos]);
				}
			} else {
				if (pump_msg[pos].lnz_state != 0) {
					RunLog("Node %d FP %d LNZ %d replace...... gunLog: %02x", 
						node, pos + 1, pump_msg[pos].lnz_state, pump.gunLog[pos]);
					pump_msg[pos].lnz_state = 0;
				}
				dostop_flag[pos] = 0;  // 油枪状态变化的时候才会执行到这里,
					               // 所以不能保证在lnz_state在枪=挂着的时候总是为0
			}
			break;
		case 0x65:
			ret |= 0x08;
			RunLog("Node %d FP %d 取泵码成功 ....", node , pos + 1);
			// Note. 稳牌油机有两种格式的泵码数据, 长度不一样, 区别处理
			if (p[1] == 13) {  // 国内卡机格式, 有金额总累
				// p + 4, p + 10: 忽略最高字节
				memcpy(pump_msg[pos].vol_total, p + 4, GUN_EXTEND_LEN);
				memcpy(pump_msg[pos].amo_total, p + 10, GUN_EXTEND_LEN);
			} else {
				// p[1] == 16, 原装机格式, 无金额总累
				memcpy(pump_msg[pos].vol_total, p + 3, GUN_EXTEND_LEN);
				bzero(pump_msg[pos].amo_total, GUN_EXTEND_LEN);
			}

			break;
		default:
			break;
		}

		p = p + p[1] + 2;  // 下一个数据块, 2 为p[0]p[1]的长度

	} while (p[2] != 0x03 || p[3] != 0xFA);

	return ret;
}

/*
 * func: 一次轮询
 *       返回数据若为 ACK/NAK/EOT 则返回0, 如果是其他数据, 即pump.cmdlen大于3, 则返回 1
 */

static int poll(int pos)
{
	int ret;
	int trycnt;
	int ackpoll_flag = 0; // 标记是否需要回复ACKPOLL
	__u16 crc;

	for (trycnt = 0; trycnt < 2; ++trycnt) {
		// Step.1 send command
		pump.cmd[0] = pump.panelPhy[pos];   // ADR
		pump.cmd[1] = 0x20;                 // CTRL
		pump.cmd[2] = 0xFA;                 // SF
		pump.cmdlen = 3;
		pump.wantLen = 3;                   // 变长

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("发取轮询命令到Node[%d].FP[%d]失败", node, pos + 1);
			return -1;                  // 取返回信息失败由GetPumpMsg的调用者负责重试
		}

RETRY:
		if (trycnt > 3) {
			// 重试最多3次
			return -1;
		}
		// Step.2  receive data
		pump.cmdlen = sizeof(pump.cmd) -  1;
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x07;
		gpGunShm->addInf[pump.chnLog - 1][1] = 0x00;  // spec.c中使用, 标识数据接受进度
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x00;
		if (ret < 0) {
			RunLog("发取轮询命令到Node[%d].FP[%d]成功, 取返回信息失败", node, pos + 1);
			continue;
			/*
			 * 油机在加油过程中或者状态变化的时候poll存在无返回的情况,
			 * 重新poll一次才能得到信息
			 */
		} 

		if (pump.cmdlen == 3) {
			if (pump.cmd[2] != 0xfa) {
				return -1;    // 防止错误数据干扰
			}
			// EOT 无需回复
			return 0;
		}

		crc = CRC_16(&pump.cmd[0], pump.cmdlen - 4);
		if ((__u8)(crc & 0x00FF) != pump.cmd[pump.cmdlen - 4] ||
			(__u8)(crc >> 8) != pump.cmd[pump.cmdlen - 3]) {
			RunLog("Node[%d].FP[%d]返回数据校验错误", node, pos + 1);
			if (AckPoll(pos, _NAK, pump.cmd[1] & 0x0F) < 0) {   // 要求油机重传
				continue;
			}
			ackpoll_flag = 1;
			++trycnt;
			goto RETRY;                    // 重新接收
		}

		if (ackpoll_flag == 0) {
			AckPoll(pos, _ACK, pump.cmd[1] & 0x0F);            // 要回复 ACK, 否则下次会重传
		} else {
			AckPoll(pos, _ACKPOLL, pump.cmd[1] & 0x0F);        // 要回复 ACKPOLL, 否则下次会重传
		}

		ackpoll_flag = 0;
		return 1;
	}

	return -1;
}

/*
 * func: 读取实时状态 (失败只重试一次)
 *       仅在没有收到回复信息时才返回-1;
 *       返回信息为NAK等情况返回1, 避免影响离线判断的准确性
 *
 */
static int GetPumpStatus(int pos)
{
	int idx,ret;
	int trycnt;
	int no_response_cnt = 0;    // 没有收到回复信息计数
	__u16 crc = 0;

	PLUS_TX_NO(pos);
	for (trycnt = 0; trycnt < 2; ++trycnt) {
		pump.cmd[0] = pump.panelPhy[pos];   // ADR
		pump.cmd[1] = 0x30 | tx_no[pos];    // CTRL
		pump.cmd[2] = 0x01;   // TRANS
		pump.cmd[3] = 0x01;   // LNG
		pump.cmd[4] = 0x00;   // DCC
		crc = CRC_16(&pump.cmd[0], 5);
		idx = 5;
		if ((crc & 0x00FF) == 0xFA) {
			pump.cmd[idx++] = 0x10;   // 数据连接码 DEL(0x10)
		}
		pump.cmd[idx++] = crc & 0x00FF;

		if ((crc >> 8) == 0xFA) {
			pump.cmd[idx++] = 0x10;
		}
		pump.cmd[idx++] = crc >> 8;
		pump.cmd[idx++] = 0x03;
		pump.cmd[idx++] = 0xfa;
		pump.cmdlen = idx;
		pump.wantLen = 3;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("发取状态命令到Node[%d].FP[%d]失败", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("发取状态命令到Node[%d].FP[%d]成功, 取返回信息失败", node, pos + 1);
			no_response_cnt++;
			continue;
		} 

		// 不管是返回 NAK 还是 TX# 不符, 均重试
		if (pump.cmd[1] == (_NAK | tx_no[pos])) {
			no_response_cnt = 0;
			tx_no[pos] = 0x00;
			continue;
		} else if ((pump.cmd[1] & 0x0F) != tx_no[pos]) {
			no_response_cnt = 0;
			tx_no[pos] = 0x00;   // 错误恢复
			continue;
		}

		// GetPumpOnline & GetPumpStatus返回结果长度一定大于3, 否则错误
		// 所以判断条件为 <= 0
		ret = GetPumpMsg(pos);
		if (ret < 0) {                      // 无返回结果
			no_response_cnt++;
			continue;
		} else if ((ret & 0x01) != 0x01) {  // 返回结果中不包含状态
			no_response_cnt = 0;
			continue;
		}
#if 0
		if (ret == 0) {
			no_response_cnt = 0;
			continue;
		} else if (ret < 0) {
			no_response_cnt++;
			continue;
		}
#endif

		return 0;
	}

	if (no_response_cnt == 0) {
		return 1;         // 有回复,是NAK
	} else {
		return -1;
	}
}


//test wether the dispenser is online, we only use fp 1 to test.
/*
 * func: 判断Dispenser是否在线
 */
static int GetPumpOnline(int pos)
{
	int idx,ret;
	int trycnt;
	__u16 crc = 0;

	RunLog("Node[%d].FP[%d] Online ?", node, pos + 1);

	PLUS_TX_NO(pos);
	tx_no[pos] = 0;
	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {
		pump.cmd[0] = pump.panelPhy[pos];   // ADR
		pump.cmd[1] = 0x30 | tx_no[pos];    // CTRL
		pump.cmd[2] = 0x01;   // TRANS
		pump.cmd[3] = 0x01;   // LNG
		pump.cmd[4] = 0x00;   // DCC
		crc = CRC_16(&pump.cmd[0], 5);
		idx = 5;
		if ((crc & 0x00FF) == 0xFA) {
			pump.cmd[idx++] = 0x10;   // 数据连接码 DEL(0x10)
		}
		pump.cmd[idx++] = crc & 0x00FF;

		if ((crc >> 8) == 0xFA) {
			pump.cmd[idx++] = 0x10;
		}
		pump.cmd[idx++] = crc >> 8;
		pump.cmd[idx++] = 0x03;
		pump.cmd[idx++] = 0xfa;
		pump.cmdlen = idx;
		pump.wantLen = 3;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("发取状态命令到Node[%d].FP[%d]失败", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("发取状态命令到Node[%d].FP[%d]成功, 取返回信息失败", node, pos + 1);
			continue;
		} 

		// 不管是返回 NAK 还是 TX# 不符, 均重试
		if ((pump.cmd[1] & 0x0F) == _NAK) {
			tx_no[pos] = 0x00;   // 错误恢复
			continue;
		} else if ((pump.cmd[1] & 0x0F) != tx_no[pos]) {
			tx_no[pos] = 0x00;   // 错误恢复
			continue;
		}

		// GetPumpOnline & GetPumpStatus返回结果长度一定大于3, 否则错误
		// 所以判断条件为 <= 0
		if (GetPumpMsg(pos) <= 0) {
			continue;
		}

		return 0;
	}

	return -1;
}

/*
 * func: 获取某条枪泵码
 *       稳牌返回的泵码数据有两个不同的格式长度分别为5和6, 注意区别对待
 */
static int GetGunTotal(int pos, __u8 gunLog, __u8 *vol_total, __u8 *amo_total)
{
	int idx;
	int ret;
	int trycnt;
	__u16 crc = 0;

	PLUS_TX_NO(pos);
	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {
		pump.cmd[0] = pump.panelPhy[pos];   // ADR
		pump.cmd[1] = 0x30 | tx_no[pos];    // CTRL
		pump.cmd[2] = 0x65;   // TRANS
		pump.cmd[3] = 0x01;   // LNG
		pump.cmd[4] = gunLog; // LNZ 1.2..
		crc = CRC_16(&pump.cmd[0], 5);
		idx = 5;
		if ((crc & 0x00FF) == 0xFA) {
			pump.cmd[idx++] = 0x10;
		}
		pump.cmd[idx++] = crc & 0x00FF;

		if ((crc >> 8) == 0xFA) {
			pump.cmd[idx++] = 0x10;
		}
		pump.cmd[idx++] = crc >> 8;
		pump.cmd[idx++] = 0x03;
		pump.cmd[idx++] = 0xfa;
		pump.cmdlen = idx;    // 9 or 10
		pump.wantLen = 3;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("发取泵码命令到Node[%d].FP[%d]失败", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("发取泵码命令到Node[%d].FP[%d]成功, 取返回信息失败", node, pos + 1);
			continue;
		} 

		// 不管是返回 NAK 还是 TX# 不符, 均重试
		if (pump.cmd[1] == (_NAK | tx_no[pos])) {
			tx_no[pos] = 0x00;   // 错误恢复
			continue;
		} else if ((pump.cmd[1] & 0x0F) != tx_no[pos]) {
			tx_no[pos] = 0x00;   // 错误恢复
			continue;
		}

		ret = GetPumpMsg(pos);
		if (ret < 0) {
			RunLog("读取Node[%d].FP[%d].LNZ[%d]的泵码失败", node, pos + 1, gunLog);
			continue;
		} else if (ret < 0x08) {
			RunLog("读取Node[%d].FP[%d].LNZ[%d]的泵码失败", node, pos + 1, gunLog);
			// 这种情况需要增加TX, 简单起见使用复位TX
			tx_no[pos] = 0x00;
			continue;
		}

		memcpy(vol_total, pump_msg[pos].vol_total, GUN_EXTEND_LEN);
		memcpy(amo_total, pump_msg[pos].amo_total, GUN_EXTEND_LEN);

		return 0;
	}

	return -1;
}


/*
 * func: 总清
 */
static int ReSet(int pos)
{
	int idx,ret;
	int trycnt;
	__u16 crc = 0;

	RunLog("Node[%d].FP[%d] do reset ", node, pos + 1);
	
	PLUS_TX_NO(pos);
	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {
		pump.cmd[0] = pump.panelPhy[pos];   // ADR
		pump.cmd[1] = 0x30 | tx_no[pos];    // CTRL
		pump.cmd[2] = 0x01;   // TRANS
		pump.cmd[3] = 0x01;   // LNG
		pump.cmd[4] = 0x05;   // DCC
		crc = CRC_16(&pump.cmd[0], 5);
		idx = 5;
		if ((crc & 0x00FF) == 0xFA) {
			pump.cmd[idx++] = 0x10;   // 数据连接码 DEL(0x10)
		}
		pump.cmd[idx++] = crc & 0x00FF;

		if ((crc >> 8) == 0xFA) {
			pump.cmd[idx++] = 0x10;
		}
		pump.cmd[idx++] = crc >> 8;
		pump.cmd[idx++] = 0x03;
		pump.cmd[idx++] = 0xfa;
		pump.cmdlen = idx;
		pump.wantLen = 3;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("发总清命令到Node[%d].FP[%d]失败", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("发总清命令到Node[%d].FP[%d]成功, 取返回信息失败", node, pos + 1);
			continue;
		} 

		// 不管是返回 NAK 还是 TX# 不符, 均重试
		if (pump.cmd[1] == (_NAK | tx_no[pos])) {
			tx_no[pos] = 0x00;   // 错误恢复
			continue;
		} else if ((pump.cmd[1] & 0x0F) != tx_no[pos]) {
			tx_no[pos] = 0x00;   // 错误恢复
			continue;
		}

		if (GetPumpMsg(pos) < 0) {
			continue;
		}

		if (pump_msg[pos].status != FP_IDLE) {  // 总清没有成功
			PLUS_TX_NO(pos);
			continue;
		}

		return 0;
	}

	return -1;
}

/*
 * func: 关机
 */
static int ClosePump(int pos)
{
	int ret;
	int trycnt;
	__u16 crc = 0;

	RunLog("Node[%d].FP[%d] do ClosePump", node, pos + 1);
	
	PLUS_TX_NO(pos);
	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {
		pump.cmd[0] = pump.panelPhy[pos];   // ADR
		pump.cmd[1] = 0x30 | tx_no[pos];    // CTRL
		pump.cmd[2] = 0x01;   // TRANS
		pump.cmd[3] = 0x01;   // LNG
		pump.cmd[4] = 0x0A;   // DCC
		crc = CRC_16(&pump.cmd[0], 5);
		pump.cmd[5] = crc & 0x00FF;         // 校验值不会出现 SF (0xFA)
		pump.cmd[6] = crc >> 8;
		pump.cmd[7] = 0x03;
		pump.cmd[8] = 0xfa;
		pump.cmdlen = 9;
		pump.wantLen = 3;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("发关机命令到Node[%d].FP[%d]失败", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("发关机命令到Node[%d].FP[%d]成功, 取返回信息失败", node, pos + 1);
			continue;
		} 

		// 不管是返回 NAK 还是 TX# 不符, 均重试
		if (pump.cmd[1] == (_NAK | tx_no[pos])) {
			tx_no[pos] = 0x00;   // 错误恢复
			continue;
		} else if ((pump.cmd[1] & 0x0F) != tx_no[pos]) {
			tx_no[pos] = 0x00;   // 错误恢复
			continue;
		}

		if (GetPumpMsg(pos) < 0) {
			continue;
		}

		if (pump_msg[pos].status != FP_CLOSED) {  // 关机没有成功
			PLUS_TX_NO(pos);
			continue;
		}

		return 0;
	}

	return -1;
}

/*
 * func: 停止
 */
static int StopPump(int pos)
{
	int ret;
	int trycnt;
	__u16 crc = 0;

	RunLog("Node[%d].FP[%d] do Stop ", node, pos + 1);
	
	PLUS_TX_NO(pos);
	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {
		pump.cmd[0] = pump.panelPhy[pos];   // ADR
		pump.cmd[1] = 0x30 | tx_no[pos];    // CTRL
		pump.cmd[2] = 0x01;   // TRANS
		pump.cmd[3] = 0x01;   // LNG
		pump.cmd[4] = 0x08;   // DCC
		crc = CRC_16(&pump.cmd[0], 5);
		pump.cmd[5] = crc & 0x00FF;         // 校验值不会出现 SF (0xFA)
		pump.cmd[6] = crc >> 8;
		pump.cmd[7] = 0x03;
		pump.cmd[8] = 0xfa;
		pump.cmdlen = 9;
		pump.wantLen = 3;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("发关机命令到Node[%d].FP[%d]失败", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("发关机命令到Node[%d].FP[%d]成功, 取返回信息失败", node, pos + 1);
			continue;
		} 

		// 不管是返回 NAK 还是 TX# 不符, 均重试
		if (pump.cmd[1] == (_NAK | tx_no[pos])) {
			tx_no[pos] = 0x00;   // 错误恢复
			continue;
		} else if ((pump.cmd[1] & 0x0F) != tx_no[pos]) {
			tx_no[pos] = 0x00;   // 错误恢复
			continue;
		}

		if (GetPumpMsg(pos) < 0) {
			continue;
		}
/*
		if (pump_msg[pos].status != FP_CLOSED) {  // 关机没有成功
			PLUS_TX_NO(pos);
			continue;
		}
*/
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
	int ret;
	int pr_no;
	int fm_idx;
	__u8 price[4];

	for (i = 0; i < pump.panelCnt; i++) {
		fm_idx = lpDispenser_data->fp_data[i].Default_Fuelling_Mode - 1;
		if (fm_idx < 0 || fm_idx > 7) {
			RunLog("Node %02x FP %02x Default Fuelling Mode Error[%d]", node, i + 1, fm_idx + 1);
			return -1;
		}
		for (j = 0; j < pump.gunCnt[i]; j++) {
			pr_no = (lpDispenser_data->fp_ln_data[i][j]).PR_Id - 1;  // 0-7	
			if ((pr_no < 0) || (pr_no > 7)) {
				RunLog("InitPumpPrice PR_Id error, PR_Id[%d]", pr_no + 1);
				return -1;
			}

			RunLog("InitPumpPrice PR_Id[%d]", pr_no + 1);

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
	__u8 lnz = 0;     // fix value
	__u8 total[7] = {0};
	__u8 vol_total[GUN_EXTEND_LEN] = {0};
	__u8 amo_total[GUN_EXTEND_LEN] = {0};

	for (i = 0; i < pump.panelCnt; i++) {
		for (j = 0; j < pump.gunCnt[i]; ++j) {
			ret = GetGunTotal(i, j + 1, vol_total, amo_total);
			if (ret < 0) {
				RunLog("取 Node %d FP %d LNZ %d 的泵码失败", node, i, j);
				return -1;
			}

			pr_no = (lpDispenser_data->fp_ln_data[i][j]).PR_Id - 1;  // 0-7	
			if ((pr_no < 0) || (pr_no > 7)) {
				RunLog("InitPumpTotal,FP %d, LNZ %d, PR_Id %d", i, j + 1, pr_no + 1);
			}
			
			// Volume_Total
			total[0] = 0x0a;
			total[1] = 0x00;
			memcpy(&total[2], vol_total, GUN_EXTEND_LEN);
			memcpy(lpDispenser_data->fp_m_data[pr_no].Meter_Total, total, 7);
			memcpy((lpDispenser_data->fp_ln_data[i][j]).Log_Noz_Vol_Total, total, 7);
#if DEBUG
			RunLog("Init.fp_ln_data[%d][%d].Vol_Total: %s ", i, j,
					Asc2Hex(lpDispenser_data->fp_ln_data[i][j].Log_Noz_Vol_Total, 7));
			RunLog("Init.fp_m_data[%d].Meter_Total[%s] ", pr_no, Asc2Hex(total, 7));
			RunLog("面[%d]逻辑枪号[%d]的泵码为[%s]", i + 1, 
				j + 1, Asc2Hex(vol_total, GUN_EXTEND_LEN));
#endif
			// Amount_Total
			total[0] = 0x0a;
			total[1] = 0x00;
			memcpy(&total[2], amo_total, GUN_EXTEND_LEN);
			memcpy((lpDispenser_data->fp_ln_data[i][j]).Log_Noz_Amount_Total, total, 7);
			RunLog("Init.fp_ln_data[%d][%d].Amount_Total: %s ", i, j,
				Asc2Hex(lpDispenser_data->fp_ln_data[i][j].Log_Noz_Amount_Total, 7));
		}
	}

	return 0;
}

/*
 * func: 设置控制模式, 稳牌不需要设置, 一旦有后台命令即转换为主控
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

	return_time = time(NULL) + (x);
	RunLog("==================return_time: %ld =======", return_time);
	do {
		sleep(1);
		for (i = 0; i < pump.panelCnt; ++i) {
			if (GetPumpMsg(i) < 0) {
				fail_times++;
			}
		}
	} while (time(NULL) < return_time);

	return fail_times;
}

/*
 * func: 设定串口属性—通讯速率、校验位等
 */
static int SetPort007()
{
	int ret;

	pump.cmd[0] = 0x1C;
	pump.cmd[1] = 0x52;
	pump.cmd[2] = 0x30 | (pump.chnPhy - 1);
	pump.cmd[3] = 0x03;	                          // 9600bps
	pump.cmd[4] = 0x01 << ((pump.chnPhy - 1) % 8);    // check
	pump.cmd[5] = 0x01 << ((pump.chnPhy - 1) % 8);    // odd
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
	Ssleep(INTERVAL);

	return 0;
}

/*
 * func:  通讯不通时轮询过程中隔一段时间重设一下串口属性
 *        避免因意外的电压不稳导致单片机复位--串口属性设置恢复预设9600,8,n,1
 */
static int ReDoSetPort007()
{
#define RE_DO_FREQUENCY   10         //  10次轮询失败设置一下串口属性

	int static polling_fail_cnt = 0;
	int ret;

	if ((++polling_fail_cnt) >= RE_DO_FREQUENCY) {
		ret = SetPort007();
		if (ret < 0) {
			RunLog("节点 %d 重设串口属性失败", node);
		}
		polling_fail_cnt = 0;
	}

	return ret;
}

int NodeOnline(int IFSFpos)
{
	int fp_cnt;
	int node;
	int i;
	DISPENSER_DATA *lpDispenser_data;
	NODE_CHANNEL *lpNode_channel;

	node = ((NODE_CHANNEL *)(&(gpIfsf_shm->node_channel[IFSFpos])))->node;
	fp_cnt = ((NODE_CHANNEL *)(&(gpIfsf_shm->node_channel[IFSFpos])))->cntOfFps;
	lpDispenser_data = (DISPENSER_DATA *)(&gpIfsf_shm->dispenser_data[IFSFpos]);
	
	// 状态准备
	for (i = 0; i < fp_cnt; i++) {
		lpDispenser_data->fp_data[i].FP_State = FP_State_Closed;
	}

//	gpIfsf_shm->node_online[IFSFpos] = node;  // 一切就绪, 上线
	ChangeNode_Status(node, NODE_ONLINE);
	RunLog("节点 %d 在线!", node);
	keep_in_touch_xs(12);               // 确保CD已经收到心跳之后再做后续操作

	// when dispenser is initialized , the status is Closed. it's waiting for set command from CD. 
	// Retalix 要求上线时主动上传当前状态
	for (i = 0; i < fp_cnt; i++) {
		if ((__u8)FP_State_Closed != lpDispenser_data->fp_data[i].FP_State) {
			ChangeFP_State(node, 0x21 + i, FP_State_Closed, NULL);
		}
	}
}

/*
 * func: 设定串口采样中断周期RCAP2L
 *
 *       稳牌六枪机需要较长的串口采样周期, 且适应性比较差,
 *       所以碰上此种机型需要动态修改单片机采样中断周期
 *
 * Note: 此功能要求IO程序版本大于等于1006
 *
 * return:
 *       0 - 成功; -1 - 其他进程正在设置; -2 - 未找到合适值设置失败;
 */
static int SetRCAP2L()
{
	int i, ret;
	int fail_cnt;
	int test_cnt;
	__u8 rcap2l = 0x80;   // IO板程序设定的默认值

	if (gpGunShm->wayne_rcap2l_flag == 3) {
		/* 
		 * 此版本IO程序不支持 1c45 命令不能设置,
		 * 若下发1c45会导致IO错误, 所以直接返回.
		 */
		return 0;
	}

	// Step.0 确保已经能够通讯
	if (GetPumpOnline(0) < 0) {
		return -1;
	}
	// Setp.1 设置操作只能由其中一个进程进行
	
	RunLog("------ 开始进行RCAP2L设置, rcap2l_flag[%02x] -------------", gpGunShm->wayne_rcap2l_flag);

	if (gpGunShm->wayne_rcap2l_flag == 2) {
		RunLog("------- RCAP2L 已经设置, 直接返回 --------------");
		return 0;
	} else if (gpGunShm->wayne_rcap2l_flag == 1) {
		RunLog("------- 其他进程正在设置 RCAP2L, 等待... -------------");
		sleep(15);

		if (gpGunShm->wayne_rcap2l_flag == 2) {
			return 0;
		} 
		return -1;
	} 

	Act_P(WAYNE_RCAP2L_SEM_INDEX);
	gpGunShm->wayne_rcap2l_flag = 1;    // @1 标识开始设置
	Act_V(WAYNE_RCAP2L_SEM_INDEX);
	RemoveSem(WAYNE_RCAP2L_SEM_INDEX);

	RunLog("------ SetRCAP2L flag: %02x ------------------", gpGunShm->wayne_rcap2l_flag);

	/*
	 * Step.2 通过读取总累和设置允许油枪操作找出合适的rcap2l值, 并设置
	 * 测试(查找)方式:
	 * 	如果读取总累或者设置允许油枪有任何一次失败,
	 * 	则调整rcap2l(下限0x65)重新测试,
	 * 	直到读取泵码15次均成功,设置允许油枪12次均成功
	 */

	do {
		// 1. 
		fail_cnt = 2;   // 失败两次就调整rcap2l值重新测试.

		if (TestRCAP2L_GetTotal() < 0) {
			fail_cnt = 0;
		}

		if (TestRCAP2L_SetNozMask() < 0) {
			fail_cnt = 0;
		}

		// 2.
		if (fail_cnt == 2) {  
		   	// 没有失败
			RunLog("------ 完成RCAP2L设置,当前值为[%02x]", rcap2l);
			gpGunShm->wayne_rcap2l_flag = 2;  // @2 标识设置成功
			return 0;
		} else {
			// 测试下一个值
			rcap2l--;
			RunLog("------- 修改RCAP2L为[%02x]", rcap2l);
			if (WriteRCAP2L(rcap2l) < 0) {
				gpGunShm->wayne_rcap2l_flag = 0;  // @3 失败返回要标识复位
				return -1;
			}
		}
	} while (rcap2l > 0x65);

	WriteRCAP2L(0x80);                // 恢复默认设置
	gpGunShm->wayne_rcap2l_flag = 0;  // @4 失败返回要标识复位
	return -2;
}

/*
 * Note: 此功能要求IO程序版本大于等于1006
 */
static int WriteRCAP2L(__u8 rcap2l)
{
	int ret;

	pump.cmd[0] = 0x1C;
	pump.cmd[1] = 0x54;
	pump.cmd[2] = rcap2l;
	pump.cmdlen = 3;
	pump.wantLen = 0;

	ret = WriteToPort(pump.fd, pump.chnPhy, pump.chnLog, pump.cmd,
			pump.cmdlen, 0, pump.wantLen, TIME_OUT_WRITE_PORT);
	if (ret < 0) {
		RunLog("写RCAP2L为[%02x]失败", rcap2l);
		return -1;
	}

	Ssleep(200000);       // 延时200ms, 确保新的rcap2l已经生效再发送数据.

	return 0;
}

/*
 * 使用读取总累操作测试RCAP2L
 */
static int TestRCAP2L_GetTotal()
{
	int idx;
	int ret;
	int trycnt;
	int fail_cnt = 0;
	__u16 crc = 0;

	RunLog("Node[%d].FP[%d] Get Total[TEST]", node, 1);

	// 1. 5次读总累测试, 只要失败两次就退出
	PLUS_TX_NO(0);
	for (trycnt = 0; trycnt < 15 && fail_cnt < 2; ++trycnt) {
		pump.cmd[0] = pump.panelPhy[0];   // ADR
		pump.cmd[1] = 0x30 | tx_no[0];    // CTRL
		pump.cmd[2] = 0x65;   // TRANS
		pump.cmd[3] = 0x01;   // LNG
		pump.cmd[4] = 0x01;   // LNZ 1.2..
		crc = CRC_16(&pump.cmd[0], 5);
		idx = 5;
		if ((crc & 0x00FF) == 0xFA) {
			pump.cmd[idx++] = 0x10;
		}
		pump.cmd[idx++] = crc & 0x00FF;
		if ((crc >> 8) == 0xFA) {
			pump.cmd[idx++] = 0x10;
		}
		pump.cmd[idx++] = crc >> 8;
		pump.cmd[idx++] = 0x03;
		pump.cmd[idx++] = 0xfa;
		pump.cmdlen = idx;    // 9 or 10
		pump.wantLen = 3;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("发取泵码命令到Node[%d].FP[%d]失败", node, 1);
			continue;
		}

		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("发取泵码命令到Node[%d].FP[%d]成功, 取返回信息失败", node, 1);
			fail_cnt++;
			continue;
		} 

		// 不管是返回 NAK 还是 TX# 不符, 均重试
		if (pump.cmd[1] == (_NAK | tx_no[0])) {
			tx_no[0] = 0x00;   // 错误恢复
	//		fail_cnt++;
			continue;
		} else if ((pump.cmd[1] & 0x0F) != tx_no[0]) {
			tx_no[0] = 0x00;   // 错误恢复
	//		fail_cnt++;
			continue;
		}


		if (GetPumpMsg(0) < 0x08) {
			RunLog("读取Node[%d].FP[%d].LNZ[%d]的泵码失败", node, 1, 1);
			tx_no[0] = 0x00;   // 错误恢复
	//		fail_cnt++;
			continue;
		}
		PLUS_TX_NO(0);
		continue;
	}

	RunLog("===================== Total-fail-cnt: %d ", fail_cnt);
	if (fail_cnt > 0) {
		return -1;
	}

	tx_no[0]--;       // 最后一次循环PLUS_TX_NO增加了tx_no, 但是没有使用
	return 0;
}

/*
 * 使用设置允许枪号测试, 失败两次就退出
 */
static int TestRCAP2L_SetNozMask()
{
	int i;
	int ret;
	int trycnt;
	int fail_cnt = 0;
	__u8 lnz_mask = 0xff;
	__u16 crc = 0;

	BITLNZ *plnz = NULL;

	plnz = (BITLNZ*)&lnz_mask;

	RunLog("Node[%d].FP[%d] set LNZ mask (%02x)[TEST]", node, 1, lnz_mask);

	PLUS_TX_NO(0);
	for (trycnt = 0; trycnt < 12 && fail_cnt < 2; ++trycnt) {
		pump.cmd[0] = pump.panelPhy[0];   // ADR
		pump.cmd[1] = 0x30 | tx_no[0];    // CTRL
		pump.cmd[2] = 0x02;   // TRANS

		// 枪号
		i = 0;

		if (plnz->lnz1 != 0 && pump.gunCnt[0] > i) {
			pump.cmd[i + 4] = 0x01;
			i++;
		}
		if (plnz->lnz2 != 0 && pump.gunCnt[0] > i) {
			pump.cmd[i + 4] = 0x02;
			i++;
		}
		if (plnz->lnz3 != 0 && pump.gunCnt[0] > i) {
			pump.cmd[i + 4] = 0x03;
			i++;
		}
		if (plnz->lnz4 != 0 && pump.gunCnt[0] > i) {
			pump.cmd[i + 4] = 0x04;
			i++;
		}
		if (plnz->lnz5 != 0 && pump.gunCnt[0] > i) {
			pump.cmd[i + 4] = 0x05;
			i++;
		}
		if (plnz->lnz6 != 0 && pump.gunCnt[0] > i) {
			pump.cmd[i + 4] = 0x06;
			i++;
		}
		if (plnz->lnz7 != 0 && pump.gunCnt[0] > i) {
			pump.cmd[i + 4] = 0x07;
			i++;
		}
		if (plnz->lnz8 != 0 && pump.gunCnt[0] > i) {
			pump.cmd[i + 4] = 0x08;
			i++;
		}


		pump.cmd[3] = i;          // 油枪数量
		i += 4;
		crc = CRC_16(&pump.cmd[0], i);
		if ((crc & 0x00FF) == 0xFA) {
			pump.cmd[i++] = 0x10;       // 数据连接码 DEL
		}
		pump.cmd[i++] = crc & 0x00FF;
		pump.cmd[i++] = crc >> 8;
		pump.cmd[i++] = 0x03;
		pump.cmd[i++] = 0xfa;
		pump.cmdlen = i;
		pump.wantLen = 3;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("发允许油枪设置命令到Node[%d].FP[%d]失败", node, 1);
			continue;
		}

		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("发允许油枪设置命令到Node[%d].FP[%d]成功, 取返回信息失败", node, 1);
			fail_cnt++;
			continue;
		} 

		// 不管是返回 NAK 还是 TX# 不符, 均重试
		if (pump.cmd[1] == (_NAK | tx_no[0])) {
			tx_no[0] = 0x00;   // 错误恢复
//			fail_cnt++;
			continue;
		} else if ((pump.cmd[1] & 0x0F) != tx_no[0]) {
			tx_no[0] = 0x00;   // 错误恢复
//			fail_cnt++;
			continue;
		}
		PLUS_TX_NO(0);
		continue;
	}

	RunLog("===================== Noz-fail-cnt: %d ", fail_cnt);
	if (fail_cnt > 0) {
		return -1;
	}

	tx_no[0]--;       // 最后一次循环PLUS_TX_NO增加了tx_no, 但是没有使用
	return 0;
}


/* ----------------- End of lischn007.c ------------------- */

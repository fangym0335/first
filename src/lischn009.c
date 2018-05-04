/*
 * Filename: lischn009.c
 *
 * Description: 加油机协议转换模块 - 富仁豪升, RS422, 9600,N,8,1 
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

#define PUMP_IDLE    0x00
#define PUMP_CALL    0xAA
#define PUMP_BUSY    0xBB
#define PUMP_STOP    0xEE
#define PUMP_EOT     0xFF

#define TRY_TIME     5
#define INTERVAL     100000  // 100ms

static int DoCmd();
static void DoPolling();
static int DoAuth(int pos);
static int DoBusy(int pos);
static int DoCall(int pos);
static int DoEot(int pos);
static int DoOpen(int pos, __u8 *msgbuffer);
static int DoClose(int pos, __u8 *msgbuffer);
static int DoRation(int pos);
static int DoStarted(int pos);
static int DoStop(int pos);
static int DoMode();
static int DoPrice();
static int ChangePrice();
static int CancelRation(int pos);
static int CancelRation_Spec(int pos);
static int GetTransData(int pos, __u8 lnz);
static int GetPumpStatus(int pos, __u8 *status);
static int GetPumpOnline(int pos, __u8 *status);
static int GetPrice(int pos, __u8 lnz);
static int GetGunTotal(int pos, __u8 lnz);
static int InitPumpTotal();
static inline int GetCurrentLNZ(int pos);
static int SetCtrlMode(int pos, __u8 mode);
static int keep_in_touch_xs(int x_seconds);
static int SetPort009();
static int CheckPriceErr(int pos);

static PUMP pump;
static int node;
static int IFSFpos;
static DISPENSER_DATA *lpDispenser_data;
static NODE_CHANNEL *lpNode_channel;
static int fp_state[MAX_FP_CNT];   // 0 - IDLE, 1 - CALLING , 2 - DoStart执行成功, 3 - FUELLING
static unsigned short crc;
static int dostop_flag[MAX_FP_CNT] = {0};           // 标记是否成功执行了DoStop
static unsigned char pump_type = 0;                 // 标记油机类型, 0xFF - 多枪机(一面多枪), 0 - 单枪(一面单枪)

int ListenChn009(int chnLog)
{
	long	indtyp;
	int	ret;
	int	fd;
	int	j;
	int	i;
	__u8 status;
	
	sleep(5);   //防止此进程退出,而主进程没有启动完毕. 每个lischnxxx都需要
	RunLog("Listen Channel  009 ,logic channel[%d]", chnLog);
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
				pump.addInf[j][0] = 0;   // 标记是否做过预置
				pump.addInf[j][1] = 0;   // 标记Terminate_FP调用的是DoStop还是CancelRation
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

	ret = SetPort009();
	if (ret < 0) {
		RunLog("设置串口属性失败");
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
		UserLog("Node %d 不在线", node);
		
		sleep(1);
	}
	
	// ============= Part.4 初始化与加油机当前状态相关的数据库相关项 =======================
	
	for (i = 0;  i < pump.panelCnt; ++i) {
		while(SetCtrlMode(i, 0xAA) < 0) {   // 设置为开放模式
			RunLog("Node[%d]FP[%d]设置主控模式失败", node, i + 1);
			sleep(2);
		}
	}
	RunLog("节点[%02d]设置主控模式成功", node);


	// IDLE了再进入正常流程 , 要放在初始化泵码之前
	i = 30;
	do {
		ret = GetPumpStatus(0, &status);
		if (ret == 0 && (PUMP_IDLE == status || PUMP_CALL == status)) {
				break;
		}
		i--;
		sleep(1);
	} while (i > 0);

	if (i <= 0) {
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
			GetPumpStatus(0, &status);              // Note.  保持轮询
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
			if (lpDispenser_data->fp_data[pos].FP_State == ((__u8)FP_State_Authorised)) {
//				ret = CancelRation(pos);
				ret = DoStop(pos);
			} else if (lpDispenser_data->fp_data[pos].FP_State == ((__u8)FP_State_Calling)) {
				ret = 0;                  // 仅改变状态
				pump.addInf[pos][1] = 1;  // 需要置位 dostop_flag
			} else {
				ret = DoStop(pos);
				pump.addInf[pos][1] = 1;  // 需要置位 dostop_flag, 因没有停止加油状态
			}

			if (ret == 0) {
				pump.addInf[pos][0] = 0;
				ret = ChangeFP_State(node, pos + 0x21, FP_State_Idle, msgbuffer);
				if (pump.addInf[pos][1] == 1) {
					dostop_flag[pos] = 1;
					pump.addInf[pos][1] = 0;
				}
			} else {
				SendCmdAck(NACK, msgbuffer);
				ChangeFP_State(node,  pos + 0x21,
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
				//----特殊处理
				if (fp_state[pos] < 2) {
					DoStarted(pos);
				}
				//-----
				ChangeFP_State(node,  pos + 0x21, FP_State_Started, msgbuffer);
			} else {
				SendCmdAck(NACK, msgbuffer);
				ChangeFP_State(node,  pos + 0x21,
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
				pump.addInf[pos][0] = 1;
			} else {
				SendCmdAck(NACK, msgbuffer);
				ChangeFP_State(node,  pos + 0x21,
					lpDispenser_data->fp_data[pos].FP_State, NULL);
			}
			bzero(&lpDispenser_data->fp_data[pos].Remote_Volume_Preset, 5);
			bzero(&lpDispenser_data->fp_data[pos].Remote_Amount_Prepay, 5);
			lpDispenser_data->fp_data[pos].Log_Noz_Mask = 0xFF;

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

				// Step.1
				memset(dostop_flag, 0, sizeof(dostop_flag));
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
			continue;
		} else {
			if (gpIfsf_shm->node_online[IFSFpos] == 0) {
				if ((--fail_cnt) > 0) { // 排除干扰
					fail_cnt = 1;   // 两次成功就认为连接已经稳定
					continue;
				} else if (time(NULL) < pump.time_offline) {
					RunLog("====================== wait for 30s, ==============");
					continue;
				}

				// fail_cnt复位
				fail_cnt = 0;

				for (j = 0;  j < pump.panelCnt; ++j) {
					if (SetCtrlMode(j, 0xAA) < 0) {
						RunLog("Node[%d]FP[%d]设置主控模式失败", node, j + 1);
						fail_cnt = 1;
						return;
					}
				}

				ret = InitPumpTotal();    // 更新泵码到数据库
				if (ret < 0) {
					RunLog("更新泵码到数据库失败");
					fail_cnt = 1;     // Note.恢复过程中任何一个步骤失败都要重试
					continue;
				}

				for (j = 0;  j < pump.panelCnt; ++j) {
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
			
			
			// Note. 如果油机没有收到后台命令若干秒后会自动转换为自主加油模式
			//       那么恢复通讯后判断通讯失败时间是否超过油机的超时时间值, 
			//       若超过则需重设主控模式
			if (fail_cnt >= 2) {
				ret = 0;
				for (j = 0;  j < pump.panelCnt; ++j) {
					if (SetCtrlMode(j, 0xAA) < 0) {   // 设置为主控模式
						RunLog("Node[%d]FP[%d]设置主控模式失败", node, j + 1);
						ret--;
					}
				}

				if (ret >= 0) {
					fail_cnt = 0;
				}
				/*
				 * 因SetCtrlMode返回结果将
				 * GetPumpStatus返回结果覆盖, 会影响DoBusy处理,
				 * 所以不管SetCtrlMode成功与否均continue
				 */
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

		switch (status) {
		case PUMP_IDLE:                                   // 挂枪、空闲
			dostop_flag[i] = 0;
			if (fp_state[i] >= 2) {
				// 若有加过油, 则进行状态改变和交易处理
				// 1. ChangeFP_State to IDLE first
				tmp_lnz = lpDispenser_data->fp_data[i].Log_Noz_State;
				lpDispenser_data->fp_data[i].Log_Noz_State = 0x00;
				// 增加tmp_lnz != 0 是为判断抬枪Terminate_FP后挂枪上传挂枪状态
				if ((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State ||
											tmp_lnz != 0) {
					ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
				}

				// Step2. Get & Save TR to TR_BUFFER
				lpDispenser_data->fp_data[i].Log_Noz_State = tmp_lnz;
				ret = DoEot(i);
			} else {
				if (lpDispenser_data->fp_data[i].Log_Noz_State != 0) {
					lpDispenser_data->fp_data[i].Log_Noz_State = 0;
					ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
				}
			}

			lpDispenser_data->fp_data[i].Log_Noz_State = 0x00;
			fp_state[i] = 0;
			// 若状态不是IDLE, 则切换为Idle
			if ( ((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State) &&
				( ((__u8)FP_State_Authorised != lpDispenser_data->fp_data[i].FP_State) ||
				 ( ((__u8)FP_State_Authorised == lpDispenser_data->fp_data[i].FP_State) &&
										 (pump.addInf[i][0] == 0))) ) {
				// 在Authorised状态, 如果执行Terminate_FP失败,
				// 那么通过抬挂枪一次应该能够取消Authorised, 改变FP状态为 Idle,
				// 所以这里需要判断pump.addInf[i][0];
				ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
			}

			// 脱机交易处理
			EO_OFFLINE_TR(node, i);
			if (gpIfsf_shm->fp_offline_flag[node - 1][i] != 0) {
				upload_offline_tr(node, NULL);
			}

			// 排除Authorised状态
			if ((__u8)FP_State_Authorised != lpDispenser_data->fp_data[i].FP_State) {
				ret = ChangePrice();           // 查看是否有油品需要变价,有则执行
			}
			break;
		case PUMP_STOP:                                // 停止加油, 不做处理
			break;
		case PUMP_EOT:                                 // 加油结束, 未挂枪
			/*
			 * Terminate_FP 之后的交易才能走这个流程; 
			 * 如果是预置加油到点按照IFSF协议这个时候不应转换状态和上传交易;
			 * 考虑这个时候就上传交易效率比较好, 所以增加这个状态, 暂时不启用, 
			 * 需要的时候再启用 (移除dostop_flag[i] == 0 的判断即可);
			 * 如果实施之后证明这个是不需要的, 那么移除该状态, 移除GetPumpStatus中state相关处理.
			 * Guojie Zhu@8.19
			 */
			if (dostop_flag[i] == 0) {
				break;        // 如果预置加油到点, 等待挂枪后再处理交易
			}
			dostop_flag[i] = 0;

			if (fp_state[i] >= 2) {
				// 若有加过油, 则进行状态改变和交易处理
				// 1. ChangeFP_State to IDLE first
				// 增加tmp_lnz != 0 是为判断抬枪Terminate_FP后挂枪上传挂枪状态
				if ((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State) {
					ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
				}

				// Step2. Get & Save TR to TR_BUFFER
				ret = DoEot(i);
			}

			fp_state[i] = 0;
			// 脱机交易处理
			EO_OFFLINE_TR(node, i);
			if (gpIfsf_shm->fp_offline_flag[node - 1][i] != 0) {
				upload_offline_tr(node, NULL);
			}

			break;
		case PUMP_CALL:                                // 抬枪
			if (dostop_flag[i] != 0) {
				break;
			}

			if ((__u8)FP_State_Calling > lpDispenser_data->fp_data[i].FP_State) {
				ret = DoCall(i);
				fp_state[i] = 1;

				DoStarted(i);
			}

			// 因豪升油机已授权状态与预置加油加到点以及挂枪状态无异,
			// 所以如果抬枪枪过快或者预置量很小, 很可能采不到抬枪状态,
			// 这容易丢交易, 所以改为抬枪后再授权
			if (pump.addInf[i][0] != 0) {
				pump.addInf[i][0] = 0;
				if (DoStarted(i) < 0) {
					if (DoStarted(i) < 0) {
						return;         // 如果交易序列号设置出错, 不授权
					}
				}

				ret = DoAuth(i);

				if ((__u8)FP_State_Started > lpDispenser_data->fp_data[i].FP_State) {
					ret = ChangeFP_State(node, i + 0x21, FP_State_Started, NULL);
				}
			}
			pump.addInf[i][0] = 0;

			break;
		case PUMP_BUSY:                               // 加油
			if (dostop_flag[i] != 0) {
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
					ret = ChangeFP_State(node, 0x21 + i, FP_State_Fuelling, NULL);
					if (ret < 0) {
						RunLog("Node %d Change FP %d State to Fuelling failed", node, i + 1);
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
	int ret;
	int trycnt;
	int fm_idx = 0;
	__u8 pr_no;
	__u8 lnz, status;
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
					RunLog(">>> fp_ln_data[%d][%d].PR_Id: %d", i, j,
							lpDispenser_data->fp_ln_data[i][j].PR_Id);
#endif
					if (lpDispenser_data->fp_ln_data[i][j].PR_Id - 1 == pr_no) {
					err_code_tmp = 0x99;
				CriticalLog("下发变价命令给节点[%02d]面[%d]枪[%d]......", node, i + 1, j + 1);

					for (trycnt = 0; trycnt < TRY_TIME; trycnt++) {
						// Step.1 发变价命令
						pump.cmd[0] = 0xFA;  // command header
						pump.cmd[1] = 0x00;
						pump.cmd[2] = 0x00;
						pump.cmd[3] = 0xF0 | j;     // lnz
						pump.cmd[4] = 0x00;  // date_len
						pump.cmd[5] = 0x04;  // date_len
						pump.cmd[6] = 0x51;  // command
						pump.cmd[7] = pump.newPrice[pr_no][2];
						pump.cmd[8] = pump.newPrice[pr_no][3];
						pump.cmd[9] = 0x01;
						crc = CRC_16(&pump.cmd[1], pump.cmd[5] + 5);
						pump.cmd[10] = crc >> 8;     // CRC-1
						pump.cmd[11] = crc & 0x00FF; // CRC-2
						pump.cmdlen = 12;
						pump.wantLen = 10;

						Ssleep(INTERVAL);
						ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
								pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
						if (ret < 0) {
							RunLog("发变价命令到Node[%d].FP[%d]失败", node, i + 1);
							continue;
						}

						RunLog("Node[%d].FP[%d].PriceChange_CMD: %s",
							node, i + 1, Asc2Hex(pump.cmd, pump.cmdlen));

						bzero(pump.cmd, pump.wantLen);
						pump.cmdlen = pump.wantLen;
						ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
						if (ret < 0) {
							RunLog("发变价命令到Node[%d].FP[%d]成功, 取返回信息失败",
												node, i + 1);
							continue;
						} 

						crc = CRC_16(&pump.cmd[1], pump.cmdlen - 3);
						if ((__u8)(crc & 0x00FF) != pump.cmd[pump.cmdlen - 1] ||
							(__u8)(crc >> 8) != pump.cmd[pump.cmdlen - 2]) {
							RunLog("Node[%d].FP[%d]返回数据校验错误", node, i + 1);
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
							continue;
						}

						if (pump_type == 0xFF && (0xF0 | j) != pump.cmd[3]) {
							RunLog("Node[%d].FP[%d]返回数据枪号错误", node, i + 1);
							continue;
						}

						if (pump.cmd[7] != 0x01) {
							err_code_tmp = 0x37;
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
							continue;
						}

						// Step.2 根据命令返回信息或者重读油机单价确认变价是否成功
						ret = GetPrice(i, j);
						if (ret < 0) {
							continue;
						}

						if (memcmp(&pump.cmd[7], &pump.newPrice[pr_no][2], 2) != 0) {
							RunLog("Node[%d].FP[%d]下发变价成功, 执行失败", node, i + 1);
							err_code_tmp = 0x37; // 变价执行失败
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
	int lnz;
	__u8 price[4];
	__u8 volume[5] = {0};
	__u8 amount[5] = {0};
	__u8 total[7] = {0};
	__u8 pre_total[7] = {0};

	RunLog("节点[%d]面板[%d]处理新的交易", node, pos + 1);


	switch (lpDispenser_data->fp_data[pos].Log_Noz_State) {
	case 0x01:
		lnz = 0x01; break;
	case 0x02:
		lnz = 0x02; break;
	case 0x04:
		lnz = 0x03; break;
	case 0x08:
		lnz = 0x04; break;
	case 0x10:
		lnz = 0x05; break;
	case 0x20:
		lnz = 0x06; break;
	case 0x40:
		lnz = 0x07; break;
	case 0x80:
		lnz = 0x08; break;
	default:
		RunLog("Node[%d]FP[%d]og_Noz_State错误[%02x]",
			node, pos + 1, lpDispenser_data->fp_data[pos].Log_Noz_State);
		return -1;
	}

	err_cnt = 0;
	do {
		ret = GetTransData(pos, lnz - 1);
		if (ret < 0) {
			err_cnt++;
			RunLog("节点[%d]面板[%d]读取交易记录失败", node, pos + 1);
		}
	} while (err_cnt <= 5 && ret < 0);

	if (ret < 0) {
		return -1;
	}

	total[0] = 0x0A;
	total[1] = 0x00;
	memcpy(&total[2], &pump.cmd[15], 5);

	pr_no = (lpDispenser_data->fp_ln_data[pos][lnz - 1]).PR_Id - 1;  // 0 - 7	
	if ((pr_no < 0) || (pr_no > 7)) {
		RunLog("DoEot PR_Id error, PR_Id[%d], pump.gunLog[%d] <ERR>", pr_no + 1, lnz);
	}
	
	memcpy(pre_total, (lpDispenser_data->fp_ln_data[pos][lnz - 1]).Log_Noz_Vol_Total, 7);

	RunLog("STOTAL: %s, gunLog: %02x", Asc2Hex(pre_total, 7), lnz);
	RunLog("ETOTAL: %s, gunLog: %02x", Asc2Hex(total, 7), lnz);

	if (memcmp(pre_total, total, 7) != 0) {
		// Volume
		volume[0] = 0x06;
		volume[1] = 0x00;
		volume[2] = pump.cmd[7];
		volume[3] = pump.cmd[8];
		volume[4] = pump.cmd[9];
		memcpy(lpDispenser_data->fp_data[pos].Current_Volume, volume, 5);

		// Amount
		amount[0] = 0x06;
		amount[1] = 0x00;
		amount[2] = pump.cmd[10];
		amount[3] = pump.cmd[11];
		amount[4] = pump.cmd[12];
		memcpy(lpDispenser_data->fp_data[pos].Current_Amount, amount, 5);

		price[0] = 0x04;
		price[1] = 0x00;
		price[2] = pump.cmd[13];
		price[3] = pump.cmd[14];
		memcpy(lpDispenser_data->fp_data[pos].Current_Unit_Price, price, 4);

		// Update Volume_Total
		memcpy(lpDispenser_data->fp_m_data[pr_no].Meter_Total, total, 7);
		memcpy((lpDispenser_data->fp_ln_data[pos][lnz - 1]).Log_Noz_Vol_Total, total, 7);

		// Update Amount_Total
		memcpy(&total[2], &pump.cmd[20], 7);
		memcpy((lpDispenser_data->fp_ln_data[pos][lnz - 1]).Log_Noz_Amount_Total, total, 7);
		memcpy(lpDispenser_data->fp_data[pos].Current_Prod_Nb, lpDispenser_data->fp_pr_data[pr_no].Prod_Nb, 4);

		err_cnt = 0;
		do {
			ret = EndTr4Ln(node, pos + 0x21, lnz + 0x10);
			if (ret < 0) {
				err_cnt++;
				RunLog("DoEot.EndTr4Ln 失败, ret: %d <ERR>", ret);
			}
		} while (ret < 0 && err_cnt <= 5);

		err_cnt = 0;
		do {
			ret = EndTr4Fp(node, pos + 0x21, pre_total,
				(lpDispenser_data->fp_ln_data[pos][lnz - 1]).Log_Noz_Vol_Total);
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
 * Volume_total: pump.cmd[7] - pump.cmd[11]
 * Amount_total: pump.cmd[12] - pump.cmd[16]
 */

static int GetGunTotal(int pos, __u8 lnz)
{
	int ret;
	int trycnt;

	for (trycnt = 0; trycnt <TRY_TIME ; trycnt++) {
		pump.cmd[0] = 0xFA;  // command header
		pump.cmd[1] = 0x00;
		pump.cmd[2] = 0x00;
		pump.cmd[3] = 0xF0 | lnz;
		pump.cmd[4] = 0x00;  // date_len
		pump.cmd[5] = 0x01;  // date_len
		pump.cmd[6] = 0x58;  // command
		crc = CRC_16(&pump.cmd[1], pump.cmd[5] + 5);
		pump.cmd[7] = crc >> 8;
		pump.cmd[8] = crc & 0x00FF;
		pump.cmdlen = 9;
		pump.wantLen = 19;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("发取Node[%d].FP[%d].LNZ[%d]泵码命令失败", node, pos + 1, lnz + 1);
			continue;
		}

		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("发取Node[%d].FP[%d].LNZ[%d]泵码命令成功, 取返回信息失败",
								node, pos + 1, lnz + 1);
			continue;
		} 

		crc = CRC_16(&pump.cmd[1], pump.cmdlen - 3);
		if ((__u8)(crc & 0x00FF) != pump.cmd[pump.cmdlen - 1] ||
			(__u8)(crc >> 8) != pump.cmd[pump.cmdlen - 2]) {
			RunLog("Node[%d].FP[%d]返回数据校验错误", node, pos + 1);
			continue;
		}

		if (pump_type == 0xFF && (0xF0 | lnz) != pump.cmd[3]) {
			RunLog("Node[%d].FP[%d]返回数据枪号错误", node, pos + 1);
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

static int GetTransData(int pos, __u8 lnz) 
{
	int ret;
	int trycnt;

	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {
		pump.cmd[0] = 0xFA;  // command header
		pump.cmd[1] = 0x00;
		pump.cmd[2] = 0x00;
		pump.cmd[3] = 0xF0 | lnz;   // lnz
		pump.cmd[4] = 0x00;  // date_len
		pump.cmd[5] = 0x01;  // date_len
		pump.cmd[6] = 0x56;  // command
		crc = CRC_16(&pump.cmd[1], pump.cmd[5] + 5);
		pump.cmd[7] = crc >> 8;
		pump.cmd[8] = crc & 0x00FF;
		pump.cmdlen = 9;
		pump.wantLen = 27;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("发读交易命令到Node[%d].FP[%d]失败", node, pos + 1);
			continue;
		}

		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("发读交易命令到Node[%d].FP[%d]成功, 取返回信息失败", node, pos + 1);
			continue;
		} 

		crc = CRC_16(&pump.cmd[1], pump.cmdlen - 3);
		if ((__u8)(crc & 0x00FF) != pump.cmd[pump.cmdlen - 1] ||
			(__u8)(crc >> 8) != pump.cmd[pump.cmdlen - 2]) {
			RunLog("Node[%d].FP[%d]返回数据校验错误", node, pos + 1);
			continue;
		}

		if (pump_type == 0xFF && (0xF0 | lnz) != pump.cmd[3]) {
			RunLog("Node[%d].FP[%d]返回数据枪号错误", node, pos + 1);
			continue;
		}

		return 0;
	}

	return -1;
}


/*
 * func: Terminate_FP
 *       空闲状态下发授权后再下发此命令可以取消授权
 *       Note. 加油中停止加油后, 若未挂枪再授权将开始新的交易而不是继续加油
 */

static int DoStop(int pos)
{
	int ret;
	int trycnt;

	RunLog("Node %d FP %d DoStop", node, pos + 1);

	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {
		pump.cmd[0] = 0xFA;  // command header
		pump.cmd[1] = 0x00;
		pump.cmd[2] = 0x00;
		pump.cmd[3] = 0x00;
		pump.cmd[4] = 0x00;  // date_len
		pump.cmd[5] = 0x02;  // date_len
		pump.cmd[6] = 0x62;  // command
		pump.cmd[7] = 0x01;
		crc = CRC_16(&pump.cmd[1], pump.cmd[5] + 5);
		pump.cmd[8] = crc >> 8;
		pump.cmd[9] = crc & 0x00FF;
		pump.cmdlen = 10;
		pump.wantLen = 6;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("发停止加油命令到Node[%d].FP[%d]失败", node, pos + 1);
			continue;
		}

		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = sizeof(pump.cmd);
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x09;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x00;
		if (ret < 0) {
			RunLog("发停止加油命令到Node[%d].FP[%d]成功, 取返回信息失败", node, pos + 1);
			continue;
		} 

		crc = CRC_16(&pump.cmd[1], pump.cmdlen - 3);
		if ((__u8)(crc & 0x00FF) != pump.cmd[pump.cmdlen - 1] ||
			(__u8)(crc >> 8) != pump.cmd[pump.cmdlen - 2]) {
			RunLog("Node[%d].FP[%d]返回数据校验错误", node, pos + 1);
			continue;
		}

		if (pump.cmd[7] != 0x01) {
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
	int ret;
	int trycnt;

	RunLog("Node %d FP %d DoAuth", node, pos + 1);

	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {
		pump.cmd[0] = 0xFA;  // command header
		pump.cmd[1] = 0x00;
		pump.cmd[2] = 0x00;
		pump.cmd[3] = 0x00;
		pump.cmd[4] = 0x00;  // date_len
		pump.cmd[5] = 0x02;  // date_len
		pump.cmd[6] = 0x61;  // command
		pump.cmd[7] = 0x01;
		crc = CRC_16(&pump.cmd[1], pump.cmd[5] + 5);
		pump.cmd[8] = crc >> 8;
		pump.cmd[9] = crc & 0x00FF;
		pump.cmdlen = 10;
		pump.wantLen = 6;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("发授权命令到Node[%d].FP[%d]失败", node, pos + 1);
			continue;
		}

		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = sizeof(pump.cmd);
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x09;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x00;
		if (ret < 0) {
			RunLog("发授权命令到Node[%d].FP[%d]成功, 取返回信息失败", node, pos + 1);
			continue;
		} 

		crc = CRC_16(&pump.cmd[1], pump.cmdlen - 3);
		if ((__u8)(crc & 0x00FF) != pump.cmd[pump.cmdlen - 1] ||
			(__u8)(crc >> 8) != pump.cmd[pump.cmdlen - 2]) {
			RunLog("Node[%d].FP[%d]返回数据校验错误", node, pos + 1);
			continue;
		}
		
		if (pump.cmd[7] != 0x01) {
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
	const __u8 arr_zero[3] = {0};
	
//	RunLog("----------------- DoBusy ---------------");
	if (fp_state[pos] != 3) {
		if (memcmp(arr_zero, &pump.cmd[7], 3) == 0) {
			return 0;
		}
		fp_state[pos] = 3;
		return 0;
	}

	volume[0] = 0x06;
	volume[1] = 0x00;
	memcpy(&volume[2], &pump.cmd[7], 3);

	amount[0] = 0x06;
	amount[1] = 0x00;
	memcpy(&amount[2], &pump.cmd[10], 3);

	if ((__u8)FP_State_Fuelling == lpDispenser_data->fp_data[pos].FP_State) {
		ret = ChangeFP_Run_Trans(node, 0x21 + pos, amount,  volume); //只有一个面板
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
 * func: 读取油机单价
 * price: pump.cmd[7]-pump.cmd[8]
 */

static int GetPrice(int pos, __u8 lnz) 
{
	int ret;
	int trycnt;

	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {
		pump.cmd[0] = 0xFA;    // command header
		pump.cmd[1] = 0x00;
		pump.cmd[2] = 0x00;
		pump.cmd[3] = 0xF0 | lnz;     // lnz
		pump.cmd[4] = 0x00;    // date_len
		pump.cmd[5] = 0x01;    // date_len
		pump.cmd[6] = 0x50;    // command
		crc = CRC_16(&pump.cmd[1], pump.cmd[5] + 5);
		pump.cmd[7] = crc >> 8;
		pump.cmd[8] = crc & 0x00FF;
		pump.cmdlen = 9;
		pump.wantLen = 11;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("发读Node[%d].FP[%d].LNZ[%d]单价命令失败", node, pos + 1, lnz + 1);
			continue;
		}

		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("发读Node[%d].FP[%d].LNZ[%d]单价命令成功, 取返回信息失败", node, pos + 1, lnz + 1);
			continue;
		} 

		crc = CRC_16(&pump.cmd[1], pump.cmdlen - 3);
		if ((__u8)(crc & 0x00FF) != pump.cmd[pump.cmdlen - 1] ||
			(__u8)(crc >> 8) != pump.cmd[pump.cmdlen - 2]) {
			RunLog("Node[%d].FP[%d]返回数据校验错误", node, pos + 1);
			continue;
		}

		if (pump_type == 0xFF && (0xF0 | lnz) != pump.cmd[3]) {
			RunLog("Node[%d].FP[%d]返回数据枪号错误", node, pos + 1);
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

static int GetPumpStatus(int pos, __u8 *status)
{
	int trycnt;
	int ret;
	__u8 dt_str[7];
	static __u8 state[MAX_FP_CNT] = {0};
	/* state:
	 *    0: 挂枪.空闲
	 *    1: 抬枪.未加油
	 *    2: 抬枪.加油中
	 *    3: 加油结束.未挂枪
	 */

	for (trycnt = 0; trycnt < 2; ++trycnt) {
		bzero(pump.cmd, 25);
		getlocaltime(dt_str);
		pump.cmd[0] = 0xFA;  // command header
		pump.cmd[1] = 0x00;
		pump.cmd[2] = 0x00;
		pump.cmd[3] = 0x00;
		pump.cmd[4] = 0x00;  // date_len
		pump.cmd[5] = 0x08;  // date_len
		pump.cmd[6] = 0x30;  // command
		memcpy(&pump.cmd[7], dt_str, 7);
		crc = CRC_16(&pump.cmd[1], pump.cmd[5] + 5);
		pump.cmd[14] = crc >> 8;
		pump.cmd[15] = crc & 0x00FF;
		pump.cmdlen = 16;
		pump.wantLen = 6;   // 变长

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("发取状态命令到Node[%d].FP[%d]失败", node, pos + 1);
			continue;
		}

		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = sizeof(pump.cmd);
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x09;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x00;
		if (ret < 0) {
			RunLog("发取状态命令到Node[%d].FP[%d]成功, 取返回信息失败", node, pos + 1);
			continue;
		} 

		crc = CRC_16(&pump.cmd[1], pump.cmdlen - 3);
		if ((__u8)(crc & 0x00FF) != pump.cmd[pump.cmdlen - 1] ||
			(__u8)(crc >> 8) != pump.cmd[pump.cmdlen - 2]) {
			RunLog("Node[%d].FP[%d]返回数据校验错误", node, pos + 1);
			continue;
		}

		if (pump.cmd[6] == 0x5B) {
			state[pos] = PUMP_BUSY;         // 抬枪.加油中
			*status = PUMP_BUSY;            // Fuelling
		} else if (pump.cmd[6] == 0x5A) {
			if (pump.cmd[2] == 0x01) {
				if (pump.cmd[7] == 0x00) {
					if (state[pos] == PUMP_IDLE) {
						state[pos] = PUMP_CALL;  // 抬枪.未加油
//						*status = PUMP_CALL;     // Calling
					} else if (state[pos] == PUMP_BUSY) {
						state[pos] = PUMP_EOT;   // 加油完成.未挂枪
//						*status = PUMP_EOT;
					}
					*status = state[pos];
//					*status = PUMP_CALL;      // Calling
				} else if (pump.cmd[7] == 0xFF) {
					state[pos] = PUMP_EOT;    // 加油完成.未挂枪
					*status = PUMP_STOP;
				} else {
					continue;       // 断电重启开始会有错误的状态 5a01
					//*status = state[pos];   // 如果不赋值可能会导致 status为不确定值
				}
			} else {
				state[pos] = PUMP_IDLE; // 挂枪.空闲
				*status = PUMP_IDLE;    // Idle
			}
		} else {
			continue;       // 断电重启开始会有错误的状态 5d0101
		}


		if ((pump.cmd[3] & 0xF0) == 0xF0) {
			pump_type = 0xFF;                                      // 多枪机 (一面多枪)
		}

		if (pump.cmd[2] != 0) {
			if (pump_type == 0xFF) {
				pump.gunLog[pos] = (pump.cmd[3] & 0x0F) + 1;   // 1面多枪油机, 枪号 0xF0 - 0xF7: LNZ 1 - 8
			} else {
				pump.gunLog[pos] = 0x01;                       // 1面单枪油机
			}
		} else {
			pump.gunLog[pos] = 0;
		}
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
	int trycnt;
	int ret;
	__u8 dt_str[7];

	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {
		bzero(pump.cmd, 25);
		getlocaltime(dt_str);
		pump.cmd[0] = 0xFA;  // command header
		pump.cmd[1] = 0x00;
		pump.cmd[2] = 0x00;
		pump.cmd[3] = 0x00;
		pump.cmd[4] = 0x00;  // date_len
		pump.cmd[5] = 0x08;  // date_len
		pump.cmd[6] = 0x30;  // command
		memcpy(&pump.cmd[7], dt_str, 7);
		crc = CRC_16(&pump.cmd[1], pump.cmd[5] + 5);
		pump.cmd[14] = crc >> 8;
		pump.cmd[15] = crc & 0x00FF;
		pump.cmdlen = 16;
		pump.wantLen = 6;   // 变长

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("发取状态命令到Node[%d].FP[%d]失败", node, pos + 1);
			continue;
		}

		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = sizeof(pump.cmd);
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x09;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x00;
		if (ret < 0) {
			RunLog("发取状态命令到Node[%d].FP[%d]成功, 取返回信息失败", node, pos + 1);
			continue;
		} 

		crc = CRC_16(&pump.cmd[1], pump.cmdlen - 3);
		if ((__u8)(crc & 0x00FF) != pump.cmd[pump.cmdlen - 1] ||
			(__u8)(crc >> 8) != pump.cmd[pump.cmdlen - 2]) {
			RunLog("Node[%d].FP[%d]返回数据校验错误", node, pos + 1);
			continue;
		}

		if (pump.cmd[6] == 0x5B) {
			*status = 0xBB;                   // Fuelling
		} else {
			if (pump.cmd[2] == 0x01) {
				if (pump.cmd[7] == 0x00) {
					*status = 0xAA;   // Calling
				} else if (pump.cmd[7] == 0xFF) {
					*status = 0xFF;   // Stoped
				}
			} else {
				*status = 0x00;           // Idle
			}
		}

		return 0;
	}

	return -1;
}

/*
 * func: PreSet/PrePay & Release_FP
 */
static int DoRation(int pos)
{
	int ret;
	int trycnt;
	int ration_type;
	__u8 pre_volume[3] = {0};
	__u8 lnz_allowed = 0;        // 预置枪号1/2或整个FP

	if (pump.cmd[0] == 0x3D) {   // Preset
		ration_type = 0x02;
		memcpy(pre_volume, &lpDispenser_data->fp_data[pos].Remote_Volume_Preset[2], 3);
	} else {
		ration_type = 0x01;
		memcpy(pre_volume, &lpDispenser_data->fp_data[pos].Remote_Amount_Prepay[2], 3);
	}

	// 指定油枪
	if (lpDispenser_data->fp_data[pos].Log_Noz_Mask == 0xFF) {
		lnz_allowed = 0x08; // 允许任意油枪加油
	} else {
		switch (lpDispenser_data->fp_data[pos].Log_Noz_Mask) {
		case 0x01:
			lnz_allowed = 0x00; break;
		case 0x02:
			lnz_allowed = 0x01; break;
		case 0x04:
			lnz_allowed = 0x02; break;
		case 0x08:
			lnz_allowed = 0x03; break;
		case 0x10:
			lnz_allowed = 0x04; break;
		case 0x20:
			lnz_allowed = 0x05; break;
		case 0x40:
			lnz_allowed = 0x06; break;
		case 0x80:
			lnz_allowed = 0x07; break;
		default:
			RunLog("Node[%d]FP[%d]未允许任何有效油枪加油Log_Noz_Mask[%02x]",
				node, pos + 1, lpDispenser_data->fp_data[pos].Log_Noz_Mask);
			return -1;
		}
	}

	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {
		pump.cmd[0] = 0xFA;  // command header
		pump.cmd[1] = 0x00;
		pump.cmd[2] = 0x00;
		pump.cmd[3] = 0xF0 | lnz_allowed;
		pump.cmd[4] = 0x00;  // date_len
		pump.cmd[5] = 0x05;  // date_len
		if (ration_type == 0x01) {
			pump.cmd[6] = 0x59;  // command, Prepay
		} else {
			pump.cmd[6] = 0x60;  // command, Preset
		}
		memcpy(&pump.cmd[7], pre_volume, 3);
		pump.cmd[10] = 0x01;
		crc = CRC_16(&pump.cmd[1], pump.cmd[5] + 5);
		pump.cmd[11] = crc >> 8;
		pump.cmd[12] = crc & 0x00FF;
		pump.cmdlen = 13;
		pump.wantLen = 10;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("发预置加油命令到Node[%d].FP[%d]失败", node, pos + 1);
			continue;
		}

		RunLog("发预置命令[%s]到Node[%d].FP[%d]成功",
			       	Asc2Hex(pump.cmd, pump.cmdlen), node, pos + 1);

		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("发预置加油命令到Node[%d].FP[%d]成功, 取返回信息失败", node, pos + 1);
			continue;
		} 

		crc = CRC_16(&pump.cmd[1], pump.cmdlen - 3);
		if ((__u8)(crc & 0x00FF) != pump.cmd[pump.cmdlen - 1] ||
			(__u8)(crc >> 8) != pump.cmd[pump.cmdlen - 2]) {
			RunLog("Node[%d].FP[%d]返回数据校验错误", node, pos + 1);
			continue;
		}

		if (pump_type == 0xFF && (0xF0 | lnz_allowed) != pump.cmd[3]) {
			RunLog("Node[%d].FP[%d]返回数据枪号错误", node, pos + 1);
			continue;
		}

		if (pump.cmd[7] != 0x01) {
			continue;
		}
#if 0
		ret = DoAuth(pos);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]预置加油授权失败", node, pos + 1);
			continue;
		}
#endif
		return 0;
	}


	return -1;
}

static int CancelRation(int pos)
{
	int trycnt;
	int ret;

	RunLog("Cancel Ration.......");

	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {
		pump.cmd[0] = 0xFA;  // command header
		pump.cmd[1] = 0x00;
		pump.cmd[2] = 0x00;
		pump.cmd[3] = 0x00;
		pump.cmd[4] = 0x00;  // date_len
		pump.cmd[5] = 0x05;  // date_len
		pump.cmd[6] = 0x59;  // command, Preset
//		bzero(&pump.cmd[7], 3);
		pump.cmd[7] = 0x99;
		pump.cmd[8] = 0x99;
		pump.cmd[9] = 0x99;
		pump.cmd[10] = 0x01;
		crc = CRC_16(&pump.cmd[1], pump.cmd[5] + 5);
		pump.cmd[11] = crc >> 8;      // 0x41
		pump.cmd[12] = crc & 0x00FF;  // 0xd9
		pump.cmdlen = 13;
		pump.wantLen = 10;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("发取消预置加油命令到Node[%d].FP[%d]失败", node, pos + 1);
			continue;
		}

		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("发取消预置加油命令到Node[%d].FP[%d]成功, 取返回信息失败", node, pos + 1);
			continue;
		} 

		crc = CRC_16(&pump.cmd[1], pump.cmdlen - 3);
		if ((__u8)(crc & 0x00FF) != pump.cmd[pump.cmdlen - 1] ||
			(__u8)(crc >> 8) != pump.cmd[pump.cmdlen - 2]) {
			RunLog("Node[%d].FP[%d]返回数据校验错误", node, pos + 1);
			continue;
		}

		if (pump.cmd[7] != 0x01) {
			continue;
		}
		return 0;
	}
	return -1;
}

static int CancelRation_Spec(int pos)
{
	int trycnt;
	int ret;

	RunLog("Cancel Ration spec.......");

	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {
		pump.cmd[0] = 0xFA;  // command header
		pump.cmd[1] = 0x00;
		pump.cmd[2] = 0x00;
		pump.cmd[3] = 0x00;
		pump.cmd[4] = 0x00;  // date_len
		pump.cmd[5] = 0x05;  // date_len
		pump.cmd[6] = 0x59;  // command, Preset
		pump.cmd[7] = 0x99;
		pump.cmd[8] = 0x99;
		pump.cmd[9] = 0x99;
		pump.cmd[10] = 0x01;
		crc = CRC_16(&pump.cmd[1], pump.cmd[5] + 5);
		pump.cmd[11] = crc >> 8;      // 0x41
		pump.cmd[12] = crc & 0x00FF;  // 0xd9
		pump.cmdlen = 13;
		pump.wantLen = 10;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("发取消预置加油命令到Node[%d].FP[%d]失败", node, pos + 1);
			continue;
		}

		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("发取消预置加油命令到Node[%d].FP[%d]成功, 取返回信息失败", node, pos + 1);
			continue;
		} 

		crc = CRC_16(&pump.cmd[1], pump.cmdlen - 3);
		if ((__u8)(crc & 0x00FF) != pump.cmd[pump.cmdlen - 1] ||
			(__u8)(crc >> 8) != pump.cmd[pump.cmdlen - 2]) {
			RunLog("Node[%d].FP[%d]返回数据校验错误", node, pos + 1);
			continue;
		}

		if (pump.cmd[7] != 0x01) {
			continue;
		}
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
 * func: 初始化油枪泵码
 */
static int InitPumpTotal()
{
	int ret;
	int i;
	int pr_no;
	__u8 lnz = 0;
	__u8 total[7] = {0};

	for (i = 0; i < pump.panelCnt; i++) {
		for (lnz = 0; lnz < pump.gunCnt[i]; ++lnz) {
			ret = GetGunTotal(i, lnz);
			if (ret < 0) {
				RunLog("取Node[%d]FP[%d]LNZ[%d]的泵码失败", node, i + 1, lnz + 1);
				return -1;
			}

			// Volume_Total
			memcpy(&total[2], &pump.cmd[7], GUN_EXTEND_LEN);
			pr_no = (lpDispenser_data->fp_ln_data[i][lnz]).PR_Id - 1;  // 0-7	
			if ((pr_no < 0) || (pr_no > 7)) {
				RunLog("InitPumpTotal,FP %d, LNZ %d, PR_Id %d", i, lnz + 1, pr_no + 1);
			}
			total[0] = 0x0a;
			total[1] = 0x00;
			memcpy(lpDispenser_data->fp_m_data[pr_no].Meter_Total, total, 7);
			memcpy((lpDispenser_data->fp_ln_data[i][lnz]).Log_Noz_Vol_Total, total, 7);
#if DEBUG
			RunLog("Init.fp_ln_data[%d][%d].Vol_Total: %s ", i, lnz,
					Asc2Hex(lpDispenser_data->fp_ln_data[i][lnz].Log_Noz_Vol_Total, 7));
			RunLog("Init.fp_m_data[%d].Meter_Total[%s] ", pr_no, Asc2Hex(total, 7));
			RunLog("逻辑枪号[%d]的泵码为[%s]", 
				lnz + 0x01, Asc2Hex(lpDispenser_data->fp_ln_data[i][lnz].Log_Noz_Vol_Total + 2, 5));
#endif
			// Amount_Total
			memcpy(&total[2], &pump.cmd[12], GUN_EXTEND_LEN);
			total[0] = 0x0a;
			total[1] = 0x00;
			memcpy((lpDispenser_data->fp_ln_data[i][lnz]).Log_Noz_Amount_Total, total, 7);
			RunLog("Init.fp_ln_data[%d][%d].Amount_Total: %s ", i, lnz,
					Asc2Hex(lpDispenser_data->fp_ln_data[i][lnz].Log_Noz_Amount_Total, 7));
		}
	}

	return 0;
}


/*
 * func: 设置控制模式
 *
 */

static int SetCtrlMode(int pos, __u8 mode)
{
	int ret;
	int trycnt;

	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {
		pump.cmd[0] = 0xFA;  // command header
		pump.cmd[1] = 0x00;
		pump.cmd[2] = 0x00;
		pump.cmd[3] = 0x00;
		pump.cmd[4] = 0x00;  // date_len
		pump.cmd[5] = 0x03;  // date_len
		pump.cmd[6] = 0x5D;  // command
		if (mode == 0xAA) {
			pump.cmd[7] = 0x01; // 主控
		} else {
			pump.cmd[7] = 0x00; // 监控
		}

		pump.cmd[8] = 0x01;
		crc = CRC_16(&pump.cmd[1], pump.cmd[5] + 5);
		pump.cmd[9] = crc >> 8;
		pump.cmd[10] = crc & 0x00FF;
		pump.cmdlen = 11;
		pump.wantLen = 11;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("发设置控制模式命令到Node[%d].FP[%d]失败", node, pos + 1);
			continue;
		}

		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("发设置控制模式命令到Node[%d].FP[%d]成功, 取返回信息失败", node, pos + 1);
			continue;
		} 

		crc = CRC_16(&pump.cmd[1], pump.cmdlen - 3);
		if ((__u8)(crc & 0x00FF) != pump.cmd[pump.cmdlen - 1] ||
			(__u8)(crc >> 8) != pump.cmd[pump.cmdlen - 2]) {
			RunLog("Node[%d].FP[%d]返回数据校验错误", node, pos + 1);
			continue;
		}

		if (pump.cmd[7] != 0x01) {
			continue;
		}

		return 0;
	}

	return -1;
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
		sleep(1);
		if (GetPumpStatus(0, &status) < 0) {
			fail_times++;
		}
	} while (time(NULL) < return_time);

	return fail_times;
}


/*
 * func: 设定串口属性—通讯速率、校验位等
 */
static int SetPort009()
{
	int ret;

	if (gpGunShm->chnLogSet[pump.chnLog - 1] == 0) {
		pump.cmd[0] = 0x1C;
		pump.cmd[1] = 0x52;
		pump.cmd[2] = 0x30 | (pump.chnPhy - 1);
		pump.cmd[3] = 0x03;	// 9600bps
		pump.cmd[4] = 0x00;     // None
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
	int ret;
	int pr_no;
	int fm_idx = 0;
	__u8 lnz;
	__u8 status;

	if ((__u8)FP_State_Inoperative != lpDispenser_data->fp_data[pos].FP_State) {
		return -1;
	}

	for (lnz = 0; lnz < pump.gunCnt[pos]; lnz++) {
		ret = GetPrice(pos, lnz);
		if (ret < 0) {
			return -1;
		}

		pr_no = (lpDispenser_data->fp_ln_data[pos][lnz]).PR_Id - 1;  // 0 - 7	
		if ((pr_no < 0) || (pr_no > 7)) {
			RunLog("CheckPrice ERR PR_Id error, FP[%d]LNZ[%d] PR_Id[%d] <ERR>",
								pos + 1, lnz + 1, pr_no + 1);
			return -1;
		}

		fm_idx = lpDispenser_data->fp_data[pos].Default_Fuelling_Mode - 1;

		RunLog("Node %d FP %d LNZ %d New price is %02x.%02x; DST Price: %02x.%02x",
			node, pos + 1, lnz + 1, pump.cmd[7], pump.cmd[8],
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

#if 0
static int CheckLastTrans(int pos, __u8 lnz)
{
	__u8 total[7] = {0};

	for (lnz = 0; lnz < pump.gunCnt[pos]; ++lnz) {
		ret = GetGunTotal(pos, lnz);
		if (ret < 0) {
			RunLog("取Node[%d]FP[%d]LNZ[%d]的泵码失败", node, pos + 1, lnz + 1);
			return -1;
		}

		// Volume_Total
		memcpy(&total[2], &pump.cmd[7], GUN_EXTEND_LEN);
		pr_no = (lpDispenser_data->fp_ln_data[pos][lnz]).PR_Id - 1;  // 0-7	
		if ((pr_no < 0) || (pr_no > 7)) {
			RunLog("InitPumpTotal,FP %d, LNZ %d, PR_Id %d", pos, lnz + 1, pr_no + 1);
		}
		total[0] = 0x0a;
		total[1] = 0x00;
		memcpy(lpDispenser_data->fp_m_data[pr_no].Meter_Total, total, 7);
		memcpy((lpDispenser_data->fp_ln_data[i][lnz]).Log_Noz_Vol_Total, total, 7);
	}

}
#endif

/* ----------------- End of lischn009.c ------------------- */

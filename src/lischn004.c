/*
 * Filename: lischn004.c
 *
 * Description: 加油机协议转换模块—榕兴, RS422,9600,n,8,1
 *
 * 2008.07 by Lei Li <lil@css-intelligent.com>
 *
 * History:
 * 
 * 处理说明:
 * 榕兴协议只能在提抢状态下预置加油。无金额总累。
 * 当前油量，当前金额需除10为正确数值。
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

// FP State
#define PUMP_OFF        0x30
#define PUMP_CALL	0x32
#define PUMP_BUSY	0x31
#define PUMP_PEOT	0x34
#define PUMP_STOP       0x35
#define TRY_TIME	5
#define INTERVAL 	200000  // 200ms
#define INTERVAL_10     150000  //写入是停止150ms

static int DoCmd();
static void DoPolling();
static int DoAuth(int pos);
static int DoBusy(int pos);
static int DoCall(int pos);
static int ShutDown(int pos);
static int DoEot(int pos);
static int DoOpen(int pos, __u8 *msgbuffer);
static int DoClose(int pos, __u8 *msgbuffer);
static int DoStarted(int i);
static int DoRation(int pos);
static int CancelRation(int pos);
static int DoStop(int pos);
static int DoMode();
static int DoPrice();
static int ChangePrice();
static int Get_price(int pos);
static int GetPumpStatus(int pos, __u8 *status);
static int GetPumpOnline(int pos, __u8 *status);
static int GetPumpTotal(int pos);
static int GetCurrentLNZ(int pos);
static int InitPumpTotal();
static int InitCurrentPrice();
static int InitGunLog();
static int SetSaleMode(int pos,int mode);
static int SetPort004();
static int keep_in_touch_xs(int x_seconds);
static int CheckPriceErr(int pos);


static PUMP pump;
static int node;
static int IFSFpos;
static DISPENSER_DATA *lpDispenser_data;
static NODE_CHANNEL *lpNode_channel;
static int fp_state[MAX_FP_CNT];       // 0 - IDLE, 1 - CALLING , 2 - DoStarted成功, 3 - Fuelling
static int dostop_flag[MAX_FP_CNT];    // 标记是否成功执行过DoStop, 1 - 是
static int doauth_flag[MAX_FP_CNT];    // 标记是否成功做过DoRation, 1 - 是
static int fail_cnt = 0;               // 记录跟油机通讯失败次数, 用于判定离线

int ListenChn004(int chnLog)
{
	long	indtyp;
	int	ret;
	int	fd;
	int	j;
	int	i;
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
						pump.panelPhy[j] = gpIfsf_shm->gun_config[idx].phyfp;
						break;
					}
				}
				pump.gunCnt[j] =  gpIfsf_shm->node_channel[i].cntOfFpNoz[j];
				pump.addInf[j][0] = 0;	      // 标识是否做了预置
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
		RunLog("节点[%d]打开串口[%s]失败",node, pump.cmd);
		return -1;
	}

	ret = SetPort004();
	if (ret < 0) {
		RunLog("节点[%d]设置串口属性失败",node);
		return -1;
	}

	// 错开各个通道初始化读取油机数据是的时间, 减轻IO板负担
	Ssleep(2 * INTERVAL * pump.chnLog);
	
	// ============== Part.3 判断加油机是否在线 ====================================
	// Check wether dispenser is online, if it's not online, wait  here.
	while (1) {
		ret = GetPumpOnline(0, &status);
		if (ret == 0) {
			break;
		}

		gpIfsf_shm->node_online[IFSFpos] = 0; //不在线,不产生心跳.
		RunLog("<榕兴> 节点[%d]不在线", node);
		
		sleep(1);
	}
	
	// ============= Part.4 初始化与加油机当前状态相关的数据库相关项 =======================
	for (i = 0; i < pump.panelCnt; ++i) {
		ret = SetSaleMode(i, 1);   //初始化面板为主控模式
		if (ret < 0) {
			RunLog("节点[%d]设置面板[i] 主控模式失败!",node,i + 1);
			return -1;
		}
	}
	
	ret = InitGunLog();    // Note.InitGunLog 并不是每种品牌油机都必须
	if (ret < 0) {
		RunLog("节点[%d]初始化默认当前逻辑枪号失败",node);
		return -1;
	}

	ret = InitCurrentPrice(); //  初始化fp数据库的当前油价
	if (ret < 0) {
		RunLog("节点[%d]初始化油品单价到数据库失败",node);
		return -1;
	}

	ret = InitPumpTotal();    // 初始化泵码到数据库
	if (ret < 0) {
		RunLog("节点[%d]初始化泵码到数据库失败",node);
		return -1;
	}


//	gpIfsf_shm->node_online[IFSFpos] = node;
	ChangeNode_Status(node, NODE_ONLINE);
	RunLog("节点[ %d] 上线,状态[%02x]!", node, status);
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
			RunLog("系统暂停Fp_State = %d",lpDispenser_data->fp_data[0].FP_State);
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
			Ssleep(200000);  //200ms

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
				RunLog("节点[%d]非法的面板地址",node);
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
				dostop_flag[pos] = 1;
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
						RunLog("Send Ack Msg[%s] to CD failed", 
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
	int i,j;
	int ret;
	int tmp_lnz;
	static int times = 0;
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
				bzero(fp_state, sizeof(fp_state));
				bzero(dostop_flag, sizeof(dostop_flag));
				bzero(doauth_flag, sizeof(doauth_flag));

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

				// Step.1  把设置控制放在第一位
				for (j = 0; j < pump.panelCnt; ++j) {
					ret = SetSaleMode(j, 1);   //初始化面板为主控模式
					if (ret < 0) {
						RunLog("设置面板[%d] 主控模式失败!", j + 1);
						return;   // 不能用continue, 用continue这个面的设置将被略
					} else{
						RunLog("设置面板[%d] 主控模式成功!", j + 1);
					}
				}
	
				// Step.2
				ret = InitPumpTotal();    // 初始化泵码到数据库
				if (ret < 0) {
					RunLog("节点[%d], 更新泵码到数据库失败",node);
					continue;
				}

				// Step.3
				for (j = 0; j < pump.panelCnt; ++j) {
					lpDispenser_data->fp_data[j].FP_State = FP_State_Closed;
				}

				// Step.4
//				gpIfsf_shm->node_online[IFSFpos] = node;
				ChangeNode_Status(node, NODE_ONLINE);
				RunLog("====== Node %d 恢复联线! =======================", node);
				// Step.5                 // 主动上传当前FP状态
				for (j = 0; j < pump.panelCnt; ++j) {
					ChangeFP_State(node, j + 0x21, FP_State_Closed, NULL);
				}
				continue;
			}

			if (fail_cnt >= 1) {
				if (status != PUMP_BUSY) { // 避免加油中设置失败影响交易处理流程
					for (j = 0; j < pump.panelCnt; ++j) {
						if (SetSaleMode(j, 1)!= 0) {
							return;
						}
					}
					fail_cnt = 0;
					/*
					 * 因SetCtrlMode的返回结果会覆盖GetPumpStatus的返回数据,
					 * 若在结束交易时发生, 将导致交易处理出错
					 * 所以这里不管SetCtrlMode成功与否均要continue
					 */
					continue;
				}
			} else {
				fail_cnt = 0;
			}
		}
		
                if (status != pre_status[i]) {
			pre_status[i] = status;
			RunLog("节点[%d]面板[%d]当前状态字为[%02x]", node, i + 1, status);
		}
		if (gpIfsf_shm->auth_flag - FCC_SWITCH_AUTH != 0) {  //如果有后台
			if ((__u8)FP_State_Closed == lpDispenser_data->fp_data[i].FP_State) {
				if (i == 0) {
					times += pump.panelCnt;					
				}
			
				if (times > (SENDSTATE *  pump.panelCnt)) {
					if (i >= pump.panelCnt -1) {
						times = 0;
					}
					UserLog("节点[%d]面板[%d],处于Closed状态", node, i + 1);
				}

				// Closed 状态下也要能够变价
				ChangePrice();
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
			RunLog("Node[%d].FP[%d]处于Inoperative状态!", node, i + 1);
			// Inoperative 状态下也要能够变价
			ChangePrice();

			CheckPriceErr(i);
			continue;
		}
		
		switch (status) {
		case PUMP_OFF:					// 挂枪、空闲
			dostop_flag[i]= 0;
			if (fp_state[i] >= 2) {                 // Note.对没有采到挂枪信号(EOT)的补充处理
				// 若有加过油, 则进行状态改变和交易处理
				// 1. ChangeFP_State to IDLE first
				tmp_lnz = lpDispenser_data->fp_data[i].Log_Noz_State;
				lpDispenser_data->fp_data[i].Log_Noz_State = 0x00;     
				if ((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State) {
					ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
				}
				RunLog("Node %d FP %d 的 %d 号枪挂枪", node, i + 1, pump.gunLog[i]);
				// Step2. Get & Save TR to TR_BUFFER
				lpDispenser_data->fp_data[i].Log_Noz_State = tmp_lnz;
				ret = DoEot(i);
			}

			lpDispenser_data->fp_data[i].Log_Noz_State = 0x00;
			fp_state[i] = 0;
			
			if (((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State) && 
				((__u8)FP_State_Authorised != lpDispenser_data->fp_data[i].FP_State)) {
				ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
			}

			EO_OFFLINE_TR(node, i);
			if (gpIfsf_shm->fp_offline_flag[node - 1][i] != 0) {
				upload_offline_tr(node, NULL);
			}

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
		case PUMP_CALL: // 抬枪
			dostop_flag[i]= 0;

			if ((__u8)FP_State_Calling > lpDispenser_data->fp_data[i].FP_State) {
				ret = DoCall(i);
				fp_state[i] = 1;

				DoStarted(i);
			}

			break;
		case PUMP_BUSY:					// 加油
			if (dostop_flag[i] != 0) { 
				break;
			}
				
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
		case PUMP_STOP:					// 暂停加油 
			if ((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State) {
				ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
			}
			break;
		case PUMP_PEOT:
			ShutDown(i);
			Ssleep(200000);      // 等待油机处理数据
			if (fp_state[i] >= 2) {  //如果加油中油机脱机不做交易处理
				if (GetPumpStatus(i, &status) < 0) {
					break;
				}
				if (status != PUMP_OFF) {
					/* 做交易处理前必须确保油机状态为 PUMP_OFF,
					 * 否则油机可能还没有完成数据处理.
					 */
					break;
				}

				/*
				 * Note: 之所以在case PUMP_PEOT下增加PUMP_OFF状态判断,
				 *       并做交易处理DoEot, 是为了增加交易上传速度;
				 *       如果下一个轮询周期再做DoEot会延长交易上传时间(1-2s)
				 * 
				 */

				// 1. ChangeFP_State to IDLE first
				tmp_lnz = lpDispenser_data->fp_data[i].Log_Noz_State;
				lpDispenser_data->fp_data[i].Log_Noz_State = 0x00;
				if ((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State) {
					ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
				}
				RunLog("Node %d FP %d 的 %d 号枪挂枪", node, i + 1, pump.gunLog[i]);
				// Step2. Get & Save TR to TR_BUFFER
				lpDispenser_data->fp_data[i].Log_Noz_State = tmp_lnz;
				ret = DoEot(i);
				lpDispenser_data->fp_data[i].Log_Noz_State = 0x00;
				fp_state[i] = 0;
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
		RunLog("节点[%d]心跳状态改变错误",node);
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
 */
static int DoRation(int pos)  
{
#define  PRE_VOL  1
#define  PRE_AMO  2
	int t;
	int ret;
	__u8 tmp[3] = {0};
	__u8 amount[6] = {0};
	__u8 preset_type;
	__u8 CrcHi;
	__u8 CrcLo;

	if (pump.cmd[0] == 0x3D) { 
		preset_type = PRE_VOL;
		tmp[0] = lpDispenser_data->fp_data[pos].Remote_Volume_Preset[2];	
		tmp[1] = lpDispenser_data->fp_data[pos].Remote_Volume_Preset[3];
		tmp[2] = lpDispenser_data->fp_data[pos].Remote_Volume_Preset[4];

	} else if (pump.cmd[0] == 0x4D) {//定额
		preset_type = PRE_AMO;
		tmp[0] = lpDispenser_data->fp_data[pos].Remote_Amount_Prepay[2];	
		tmp[1] = lpDispenser_data->fp_data[pos].Remote_Amount_Prepay[3];
		tmp[2] = lpDispenser_data->fp_data[pos].Remote_Amount_Prepay[4];
	}
	

	amount[0] = 0x30 | (tmp[0] >> 4);
	amount[1] = 0x30 | (tmp[0] & 0x0f);
	amount[2] = 0x30 | (tmp[1] >> 4);
	amount[3] = 0x30 | (tmp[1] & 0x0f);
	amount[4] = 0x30 | (tmp[2] >> 4);
	amount[5] = 0x30 | (tmp[2] & 0x0f);

	for (t = 0; t < TRY_TIME; t++) {		
		Ssleep(INTERVAL_10);
		pump.cmd[0] = 0xAA;
		pump.cmd[1] = 0x55;		/*aa55报头*/
		pump.cmd[2] = 0x0a;		/*数据长度 从地址（包括）到Crc（不包括）之间的长度*/
		pump.cmd[3] = 0x00|(pump.panelPhy[pos]-1);	/*设备地址。用物理面板号保存*/
		pump.cmd[4] = 0x02;				/*命令：02是授权命令*/
		pump.cmd[5] = '1';				/*授权*/
		if (preset_type == PRE_VOL)			/*定量还是定额*/
			pump.cmd[6] = '1';
		else if (preset_type == PRE_AMO)
			pump.cmd[6] = '2';
		
		pump.cmd[7] = amount[0];
		pump.cmd[8] = amount[1];
		pump.cmd[9] = amount[2];
		pump.cmd[10] = amount[3];
		pump.cmd[11] = amount[4];
		pump.cmd[12] = amount[5];

		pump.cmdlen = 13;
		CRC_16_RX(pump.cmd, pump.cmdlen, &CrcHi, &CrcLo);	/*计算Crc值*/
		pump.cmd[13] = CrcHi;
		pump.cmd[14] = CrcLo;
		pump.cmdlen = 15;
		pump.wantLen = 7;
		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]下发预置加油命令失败", node, pos + 1);
			continue;
		}

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]下发预置加油命令成功, 取返回信息失败", node, pos + 1);
			continue;
		}

		CRC_16_RX(pump.cmd,pump.cmdlen-2,&CrcHi, &CrcLo);
		if (pump.cmd[pump.cmdlen -2] != CrcHi || pump.cmd[pump.cmdlen -1] != CrcLo) {
			RunLog("节点[%d]面板[%d]返回数据CRC校验失败。",node,pos + 1);
			continue;
		}
		
		if (pump.cmd[4] !='A') {
			RunLog("节点[%d]面板[%d] 取回复信息失败", node,pos + 1);
			continue;
		} else {
			return 0;
		}
	}

	return -1;
}

/*
 * func: PreSet/PrePay & DoReleaseRation
 *       @油机厂商已经应中油要求将取消预置和停止加油合并, 所以此函数已不再需要
 */
static int CancelRation(int pos)  
{

	int t;
	int ret;
	__u8 CrcHi;
	__u8 CrcLo;

	for (t = 0; t < TRY_TIME; t++) {		
		Ssleep(INTERVAL_10);
		pump.cmd[0] = 0xAA;
		pump.cmd[1] = 0x55;
		pump.cmd[2] = 0x0a;
		pump.cmd[3] = 0x00|(pump.panelPhy[pos]-1);
		pump.cmd[4] = 0x02;     // command
		pump.cmd[5] = '1';
		pump.cmd[6] = '1';
		pump.cmd[7] = 0x30;
		pump.cmd[8] = 0x30;
		pump.cmd[9] = 0x30;
		pump.cmd[10] =0x30;
		pump.cmd[11] =0x30;
		pump.cmd[12] = 0x30;
		pump.cmdlen = 13;
		CRC_16_RX(pump.cmd, pump.cmdlen, &CrcHi, &CrcLo);
		pump.cmd[13] = CrcHi;
		pump.cmd[14] = CrcLo;
		pump.cmdlen = 15;
		pump.wantLen = 7;
		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]下发预置加油命令失败", node, pos + 1);
			continue;
		}

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret<0) {
			RunLog("节点[%d]面板[%d]下发预置加油命令成功, 取返回信息失败", node, pos + 1);
			continue;
		}

		CRC_16_RX(pump.cmd,pump.cmdlen-2,&CrcHi, &CrcLo);
		if (pump.cmd[pump.cmdlen -2] != CrcHi || pump.cmd[pump.cmdlen -1] != CrcLo) {
			RunLog("节点[%d]面板[%d]返回数据CRC校验失败。",node,pos + 1);
			continue;
		}
		
		if (pump.cmd[4] !='A') {
			RunLog("节点[%d]面板[%d] 取回复信息失败", node,pos + 1);
			continue;
		} else {
			return 0;
		}
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
/*关机命令*/
static int ShutDown(int pos)
{
	int t;
	int ret;
	__u8 CrcHi,CrcLo;
	
	for (t=0;t < TRY_TIME;t++) {
		pump.cmd[0] = 0xAA;
		pump.cmd[1] = 0x55;
		pump.cmd[2] = 0x02;
		pump.cmd[3] = 0x00|(pump.panelPhy[pos]-1);
		pump.cmd[4] = 0x03;
		pump.cmdlen = 5;
		CRC_16_RX(pump.cmd,pump.cmdlen,&CrcHi,&CrcLo);
		pump.cmd[5] = CrcHi;
		pump.cmd[6] = CrcLo;
		pump.cmdlen = 7;
		pump.wantLen = 7;

		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]向端口写数据失败",node,pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd)-1;

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]取回复信息失败.", node,pos + 1);
			continue;
		} 

		CRC_16_RX(pump.cmd,pump.cmdlen-2,&CrcHi, &CrcLo);
		if (pump.cmd[pump.cmdlen -2] != CrcHi || pump.cmd[pump.cmdlen -1] != CrcLo) {
			RunLog("节点[%d]面板[%d]返回数据CRC校验失败。",node,pos + 1);
			continue;
		}
		
		if (pump.cmd[4] !='A') {
			RunLog("节点[%d]面板[%d]发送关机命令失败.", node,pos + 1);
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
/*
 * 修改建议&实现说明:  需要从油机读取的数据项有 当次加油量 & 加油金额 &
 *                     单价 & 油量总累 [& 金额总累] , 分别更新相应的数据库项
 *                     
 */
static int DoEot(int pos)
{
	int ret;
	int pr_no;
	int n;
	int err_cnt;
	long zero = 0;
	__u8 price[4];
	__u8 volume[5];
	__u8 amount[5];
	__u8 addfive[4] = {0};
	__u8 msg[21] = {0};
	__u8 pre_total[7];
	__u8 meter_total[7] = {0};
	__u8 amount_total[7] = {0};	

	doauth_flag[pos]= 0;
	RunLog("节点[%d]面板[%d]处理新交易", node, pos + 1);

	memcpy(msg, pump.cmd, 20);   // 交易数据备份 -- 油量&金额

#if 0
	/*
	 * 这部分处理独立出来在 PUMP_PEOT状态做, 
	 * 等状态变为PUMP_OFF确定加油机已经完成交易数据处理后再读取泵码和交易
	 *
	 * Guojie Zhu @ 2009.8.25
	 */
	err_cnt = 0;
	do {
		ret = ShutDown(pos);
		if (ret < 0) {
			err_cnt++;
			RunLog("节点[%d]面板[%d]关机命令失败<ERR>" ,node, pos+1);
		}
	} while (ret < 0 && err_cnt <=5);

	if (ret < 0) {
		return -1;
	}


	//2. 获得泵码，和累积加油金额
	bzero(pump.cmd,sizeof(pump.cmd));
	Ssleep(200000); //挂枪后马上取泵码，泵码可能还没更新。
#endif
	err_cnt = 0;
	do {
		ret = GetPumpTotal(pos);
		if (ret < 0) {
			err_cnt++;
			RunLog("节点[%d]面板[%d]GetPumpTotal 失败 <ERR>" ,node, pos+1);
		}
	} while (ret < 0 && err_cnt <=5);

	if (ret < 0) {
		return -1;
	}

	n = 6;
	n += (pump.gunLog[pos] - 1) * 4;
	meter_total[0] = 0x0A;
	Hex2Bcd(pump.cmd + n, 4, &meter_total[1],6);

	amount_total[0] = 0x0A;
	amount_total[1] = 0x00;
	amount_total[2] = 0x00;
	amount_total[3] = 0x00;
	amount_total[4] = 0x00;
	amount_total[5] = 0x00;
	amount_total[6] = 0x00;

	memcpy(pre_total, (lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Vol_Total, 7);

	//3. 得到油品id 
	pr_no = (lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).PR_Id - 1;  // 0 - 7	
	if ((pr_no < 0) || (pr_no > 7)) {
		RunLog("节点[%d]面板[%d]DoEot PR_Id error, PR_Id[%d]<ERR>",node, pos + 1, pr_no + 1);
	}


	//4. 检查泵码是否有变化,如果有则保存交易记录
	if (memcmp(pre_total, meter_total, 7) != 0) {
#if 0
		RunLog("节点[%d]面板[%d]原泵码为 %s",node,pos,Asc2Hex(pre_total,7));
		RunLog("节点[%d]面板[%d]当前泵码为 %s",node,pos,Asc2Hex(meter_total,7));	  
#endif

		//Current_Unit_Price bin8 + bcd8
		ret = Get_price(pos);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]读取单价失败",node,pos + 1);
			// 获取失败就用数据库里的, 尽可能保证交易能上传, 不到不得已, 不能放弃退出
			memcpy(price, lpDispenser_data->fp_fm_data[pr_no][lpDispenser_data->fp_data[pos].Default_Fuelling_Mode - 1].Prod_Price, 4);
		} else {
			price[0] = 0x04;
			price[1] = 0x00;
			price[2] = (pump.cmd[6] << 4) | (pump.cmd[7] & 0x0f);
			price[3] = (pump.cmd[8] << 4) | (pump.cmd[9] & 0x0f);
		}
				
		memcpy(lpDispenser_data->fp_data[pos].Current_Unit_Price, price, 4);
		//榕兴小数点后第三位可能出现9，加油机上显示的为进位后的数。
		//上传的数据没有进位需要四舍五入。

		zero = Hex2Long(msg + 8, 4);
		if (zero % 10 == 0) {
			Hex2Bcd(msg + 8, 4, &volume[1], 4);
		} else {
			zero += 5;
			Long2Hex(zero, &addfive[0], 4);
			Hex2Bcd(&addfive[0], 4, &volume[1], 4);
		}


		//Current_Volume  bin8 + bcd8
		volume[0] = 0x06;
		volume[4] = (volume[4]>>4)|((volume[3]&0x0f)<<4);
		volume[3] = (volume[3]>>4)|((volume[2]&0x0f)<<4);
		volume[2] = (volume[2]>>4)|((volume[1]&0x0f)<<4);
		volume[1] = volume[1]>>4;
		memcpy(lpDispenser_data->fp_data[pos].Current_Volume, volume, 5);

		//Current_Amount  bin8 + bcd 8
		zero = Hex2Long(msg + 12, 4);
		if (zero % 10 == 0) {
			Hex2Bcd(msg + 12, 4, &amount[1], 4);
		} else {
			zero += 5;
			Long2Hex(zero, &addfive[0], 4);
			Hex2Bcd(&addfive[0], 4, &amount[1], 4);
		}

		amount[0] = 0x06;
		amount[4]= (amount[4]>>4)|((amount[3]&0x0f)<<4);
		amount[3]= (amount[3]>>4)|((amount[2]&0x0f)<<4);
		amount[2]= (amount[2]>>4)|((amount[1]&0x0f)<<4);
		amount[1]= amount[1]>>4;
		memcpy(lpDispenser_data->fp_data[pos].Current_Amount, amount, 5);

		memcpy(lpDispenser_data->fp_m_data[pr_no].Meter_Total, meter_total, 7);		
		memcpy((lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Vol_Total, meter_total, 7);

#if 0
		RunLog("节点[%02d]面板[%d]枪[%d] ## Amount: %02x%02x.%02x, Volume: %02x%02x.%02x, Price: %02x.%02x, Total: %02x%02x%02x%02x.%02x", 
			node, pos + 1, pump.gunLog[pos], amount[2], amount[3], amount[4], 
			volume[2], volume[3], volume[4], price[2], price[3], meter_total[2],
		       	meter_total[3], meter_total[4], meter_total[5], meter_total[6]);
#endif

		memcpy((lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Amount_Total, amount_total, 7);
		memcpy(lpDispenser_data->fp_data[pos].Current_Prod_Nb, lpDispenser_data->fp_pr_data[pr_no].Prod_Nb, 4);
		err_cnt = 0;
		do {
			ret = EndTr4Ln(node, pos + 0x21, pump.gunLog[pos] + 0x10);
			if (ret < 0) {
				err_cnt++;
				RunLog("节点[%d]面板[%d] DoEot.EndTr4Ln failed, ret: %d <ERR>", node,pos+1,ret);
			}
		} while (ret < 0 && err_cnt <= 5);

		err_cnt = 0;
		do {
			ret = EndTr4Fp(node, pos + 0x21, pre_total, 
				(lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Vol_Total);
			if (ret < 0) {
				err_cnt++;
				RunLog("节点[%d]面板[%d]DoEot.EndTr4Fp failed, ret: %d <ERR>", node,pos+1,ret);
			}
		} while (ret < 0 && err_cnt <= 8);
	} else {
		ret = EndZeroTr4Fp(node, pos + 0x21);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d] DoEot.EndZeroTr4Fp failed, ret: %d <ERR>",node,pos+1,ret);
		}
	}	
	return 0;
}


/*
 * func: 停止加油或者取消预置授权
 */
static int DoStop(int pos)
{
	int t,ret;
	__u8  CrcHi,CrcLo;

	RunLog("Node %d FP %d DoStop", node, pos + 1);

	for (t=0;t < TRY_TIME;t++) {
		pump.cmd[0] = 0xAA;
		pump.cmd[1] = 0x55;
		pump.cmd[2] = 0x02;
		pump.cmd[3] = 0x00|(pump.panelPhy[pos]-1);
		pump.cmd[4] = 0x03;
		pump.cmdlen = 5;
		CRC_16_RX(pump.cmd,pump.cmdlen,&CrcHi,&CrcLo);
		pump.cmd[5] = CrcHi;
		pump.cmd[6] = CrcLo;
		pump.cmdlen = 7;
		pump.wantLen = 7;

		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d] 发暂停加油命令失败",node,pos+1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd)-1;

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d] 发暂停加油命令成功, 取返回信息失败",node,pos+1);
			continue;
		}

		CRC_16_RX(pump.cmd,pump.cmdlen-2,&CrcHi, &CrcLo);
		if (pump.cmd[pump.cmdlen -2] != CrcHi || pump.cmd[pump.cmdlen -1] != CrcLo) {
			RunLog("节点[%d]面板[%d]返回数据CRC校验失败。",node,pos + 1);
			continue;
		}

		if (pump.cmd[4] !='A') {
			RunLog("节点[%d]面板[%d]发送关机命令失败", node,pos + 1);
			continue;
		}

		return 0;
	}
	return -1;
}


/*
 * func: 授权
 */
static int DoAuth(int pos)
{
	int ret;
	int t;
	__u8 status;
	__u8 CrcHi;
	__u8 CrcLo;

	RunLog("Node %d FP %d DoAuth", node, pos + 1);

	for (t = 0; t < TRY_TIME; t++) {
		pump.cmd[0] = 0xAA;
		pump.cmd[1] = 0x55;	/*aa55报头*/
		pump.cmd[2] = 0x0a;	/*数据长度 从地址（包括）到Crc（不包括）之间的长度*/
		pump.cmd[3] = 0x00|(pump.panelPhy[pos]-1);	/*设备地址。用物理面板号保存*/
		pump.cmd[4] = 0x02;	/*命令：02是授权命令*/
		pump.cmd[5] = '1';	/*授权*/
		pump.cmd[6] = '0';	/*非定值加油*/
		pump.cmd[7] = '0';
		pump.cmd[8] = '0';
		pump.cmd[9] = '0';
		pump.cmd[10] = '0';
		pump.cmd[11] = '0';
		pump.cmd[12] = '0';	/*非定值加油.补齐位*/
		pump.cmdlen = 13;
		CRC_16_RX(pump.cmd, pump.cmdlen, &CrcHi, &CrcLo);
		pump.cmd[13] = CrcHi;
		pump.cmd[14] = CrcLo;
		pump.cmdlen = 15;
		pump.wantLen = 7;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]发授权命令失败",node,pos+1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd)-1;

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]发授权命令成功, 取返回信息失败",node,pos+1);
			continue;
		}

		CRC_16_RX(pump.cmd,pump.cmdlen-2,&CrcHi, &CrcLo);
		if (pump.cmd[pump.cmdlen -2] != CrcHi || pump.cmd[pump.cmdlen -1] != CrcLo) {
			RunLog("节点[%d]面板[%d]返回数据CRC校验失败。",node,pos + 1);
			continue;
		}

		if (pump.cmd[4] != 'A' && pump.cmd[4] != 'W') {
			RunLog("Node[%d].FP[%d] 授权失败",node, pos + 1);
			continue;
		}

#if 0
		// 通过A/W判断是否成功已足够, 故不再判断状态
		ret = GetPumpStatus(pos, &status);
		if (ret < 0) {
			continue;
		}

		if (status != PUMP_BUSY) {
			continue;
		}
#endif

		return 0;
	}
	return -1;
}

/*
 * func: 上传实时加油数据
 */
static int DoBusy(int pos)
{
	int ret;
	__u8 amount[5] = {0};
	__u8 volume[5] = {0};
	static __u8 arr_zero[3] = {0};

	if (fp_state[pos] != 3) {
		if (memcmp(arr_zero, &pump.cmd[11], 4) == 0) {
			return 0;
		}
		fp_state[pos] = 3;
		return 0;
	}

	volume[0] = 0x06;
	Hex2Bcd(&pump.cmd[8], 4, &volume[1],4);

	volume[4] = (volume[4]>>4)|((volume[3]&0x0f)<<4);
	volume[3] = (volume[3]>>4)|((volume[2]&0x0f)<<4);
	volume[2] = (volume[2]>>4)|((volume[1]&0x0f)<<4);
	volume[1] = volume[1]>>4;
	
	amount[0] = 0x06;
	Hex2Bcd(&pump.cmd[12], 4, &amount[1], 4);
	amount[4]= (amount[4]>>4)|((amount[3]&0x0f)<<4);
	amount[3]= (amount[3]>>4)|((amount[2]&0x0f)<<4);
	amount[2]= (amount[2]>>4)|((amount[1]&0x0f)<<4);
	amount[1]= amount[1]>>4;
	
	ret = ChangeFP_Run_Trans(node, 0x21+pos, amount,  volume);

	return 0;
}


/*
 * func: 抬枪处理 
 * 获取当前抬起的逻辑枪号,设置fp_data[].Log_Noz_State & 改变FP状态为Calling
 */
static int DoCall(int pos)
{
	int ret;
	int t;
	__u8 ln_status;

	ret = GetCurrentLNZ(pos);
	if (ret < 0) {
		RunLog("节点[%d]面板[%d]获取当前油枪枪号失败",node,pos + 1);
		return -1;
	}

	ret = ChangeFP_State(node, pos + 0x21, FP_State_Calling, NULL);

	//上位机在线,但是允许自动授权;上位机不在线, 授权开关置于开的状态,此时自动授权
	if ((gpIfsf_shm->auth_flag == FCC_SWITCH_AUTH) && (lock_status() == 1)) {
		ret = DoAuth(pos);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d],自动授权失败",node,pos+1);
		}
		ret = ChangeFP_State(node, pos + 0x21, FP_State_Started, NULL);
		RunLog("节点[%d]面板[%d]自动授权成功",node,pos+1);
		SO_OFFLINE_TR(node, pos);
	}

	return ret;
}

/*
 * func: 执行变价, 失败改变FP状态为INOPERATIVE, 并写错误数据库
 */
static int ChangePrice() 
{
	int i, j,t;
	int ret, fm_idx;
	int pos;
	__u8 price[4] = {0};
	__u8 pr_no;
	__u8 lnz;
	__u8 msp[10]={0};
	__u8 CrcHi;
	__u8 CrcLo;
	__u8 status;
	__u8 err_code;
	__u8 err_code_tmp;
/*
 * 遍历新单价数组newPrice[FP_PR_NB_CNT], 如果有油品需要变价--数组值不为零,
 * 则遍历所有FP, 如果油品匹配就下发变价命令
 */

//	RunLog(">> exec ChangePrice Now !");
	for (pr_no = 0; pr_no < FP_PR_NB_CNT; pr_no++) {
		if (pump.newPrice[pr_no][2] != 0x00 || pump.newPrice[pr_no][3] != 0x00) {
			for (i = 0; i < pump.panelCnt; i++) {
				err_code = 0x99;
				for (j = 0; j < pump.gunCnt[i]; j++) { // 不能用gunCnt,1枪可能不用
#if DEBUG
					RunLog(">>> fp_ln_data[%d][%d].PR_Id: %d", i, j,
							lpDispenser_data->fp_ln_data[i][j].PR_Id);
#endif
					if (lpDispenser_data->fp_ln_data[i][j].PR_Id - 1 == pr_no) {
					err_code_tmp = 0x99;
				CriticalLog("下发变价命令给节点[%02d]面[%d]枪[%d]......", node, i + 1, j + 1);

					//price is little endian mode, so 1 to 0
					price[0] = 0x30 |(pump.newPrice[pr_no][2] >>4);
					price[1] = 0x30 |(pump.newPrice[pr_no][2] & 0x0f);
					price[2] = 0x30 |(pump.newPrice[pr_no][3] >>4);
					price[3] = 0x30 |(pump.newPrice[pr_no][3] & 0x0f);
			
					for (t = 0; t < TRY_TIME; t++) {
						// Step.1.1 发变价命令
						Ssleep(INTERVAL_10);	/*停10毫秒,防止写入太频繁*/
						ret=GetPumpStatus(i, &status);
						if (ret < 0) {
							RunLog("节点[%d]面板[%d]取状态信息失败",node,pos + 1);
						}
						

						/*组织更改油价命令
						aa5509 01 05 31 3933 30353837 
						*/
						pump.cmd[0] = 0xAA;
						pump.cmd[1] = 0x55;
						pump.cmd[2] = 0x09;
						pump.cmd[3] = 0x00|(pump.panelPhy[i]-1);
						pump.cmd[4] = 0x05;
						pump.cmd[5] = 0x30|(j + 1);
						pump.cmd[6] = 0x30;
						pump.cmd[7] = 0x30;
						pump.cmd[8] = price[0];
						pump.cmd[9] = price[1];
						pump.cmd[10] = price[2];
						pump.cmd[11] = price[3];
						pump.cmdlen = 12;
						CRC_16_RX(pump.cmd, pump.cmdlen, &CrcHi, &CrcLo);
						pump.cmd[12] = CrcHi;
						pump.cmd[13] = CrcLo;
						pump.cmdlen = 14;
						pump.wantLen = 7;
					
						// Setp.1.2 发单价数据
						Ssleep(INTERVAL);
						ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
								pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
						if (ret < 0) {
							RunLog("节点[%d]面板[%d],下发变价失败!" ,node,i+1);
							continue;
						}

						RunLog("Node[%d].FP[%d].PriceChange_CMD: %s",
							node, i + 1, Asc2Hex(pump.cmd, pump.cmdlen));

						memset(pump.cmd, 0, sizeof(pump.cmd));
						ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
						if (ret < 0) {
							RunLog("节点[%d]面板[%d],下发变价命令成功,取返回信息失败",node,i+1);
							continue;
						}

						/*校验Crc值*/
						CRC_16_RX(pump.cmd,pump.cmdlen-2,&CrcHi, &CrcLo);
						if (pump.cmd[pump.cmdlen -2] != CrcHi || pump.cmd[pump.cmdlen -1] != CrcLo) {
							RunLog("节点[%d]面板[%d]返回数据CRC校验失败。",node,i + 1);
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL_10 - INTERVAL);
							continue;
						}

						if (pump.cmd[4] != 'A' && pump.cmd[4] != 'W') {
							ret = Get_price(i);
							if (ret < 0) {
								continue;
							}

							memset(msp, 0, sizeof(msp));
							msp[0] = pump.cmd[6]<<4|pump.cmd[7]&0x0f;
							msp[1] = pump.cmd[8]<<4|pump.cmd[9]&0x0f;
							if (memcmp(&pump.newPrice[pr_no][2], &msp[0], 2) != 0) {
								err_code_tmp = 0x37; // 变价执行失败
								Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL_10 - INTERVAL);
								continue;
							}
						}
		
						// Step.3 写新单价到油品数据库 & 清零单价缓存
						fm_idx = lpDispenser_data->fp_data[i].Default_Fuelling_Mode - 1;
						RunLog("节点[%d]面板[%d]下发变价命令成功", node, i + 1);
						RunLog("新单价fp_fm_data[%d][%d].Prode_Price: %s", pr_no, fm_idx,
							Asc2Hex(lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price, 4));

						CriticalLog("节点[%02d]面[%d]枪[%d]变价成功,新单价为%02x.%02x",
							       	node, i + 1, j + 1,
								lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2],
								lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3]);
				
						err_code_tmp = 0x99;
						break;
					}

					// Step.4 错误判断, 若出错则写错误数据库/置FP状态为 Inoperative
					if (t >= TRY_TIME) {
						RunLog("节点[%d]面板[%d],变价命令执行失败,改变FP为不可操作状态",node,i+1);
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
				} // end of for.j < LN_CNT
				RunLog("---------- Node[%d]FP[%d] ER_DATA, error code: %02x -----", node, i + 1, err_code);

				int tmp_idx;
				for (tmp_idx = 0; tmp_idx < pump.newPrice[pr_no][0]; tmp_idx++) {
					ChangeER_DATA(node, 0x21 + i, err_code, NULL); // Translater已经回复了ACK
				}

				if (err_code != 0x99) {
					ChangeFP_State(node, 0x21 + i, FP_State_Inoperative, NULL);
				}

				Ssleep(2000000 - INTERVAL_10 - INTERVAL);
			} // end of for.i < panelCnt
			bzero(&pump.newPrice[pr_no], 4);
		} // end of if.newPrice
	} // end of for.pr_no

	return 0;
}

static int Get_price(int pos)
{
	int ret;
	int t;
	__u8 CrcHi;
	__u8 CrcLo;

	for (t = 0; t < TRY_TIME; t++) {
		pump.cmd[0] = 0xAA;
		pump.cmd[1] = 0x55;
		pump.cmd[2] = 0x03;
		pump.cmd[3] = 0x00|(pump.panelPhy[pos]-1);
		pump.cmd[4] = 0x04;
		pump.cmd[5] = 0x30|pump.gunLog[pos];
		pump.cmdlen = 6;
		CRC_16_RX(pump.cmd, pump.cmdlen, &CrcHi, &CrcLo);
		pump.cmd[6] = CrcHi;
		pump.cmd[7] = CrcLo;
		pump.cmdlen = 8;
		pump.wantLen = 12;
		
		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]向串口写数据失败",node,pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd)-1;

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]取回复信息失败",node,pos + 1);
			continue;
		} 

		CRC_16_RX(pump.cmd,pump.cmdlen-2,&CrcHi, &CrcLo);
		if (pump.cmd[pump.cmdlen -2] != CrcHi || pump.cmd[pump.cmdlen -1] != CrcLo) {
			RunLog("节点[%d]面板[%d]返回数据CRC校验失败。",node,pos + 1);
			continue;
		}

		return 0;
	}
	return -1;
}



//Inoperative 时获取所有枪单价
static int _Get_price(int pos,int _gun)
{
	int ret;
	int t;
	__u8 CrcHi;
	__u8 CrcLo;

	for (t = 0; t < TRY_TIME; t++) {
		pump.cmd[0] = 0xAA;
		pump.cmd[1] = 0x55;
		pump.cmd[2] = 0x03;
		pump.cmd[3] = 0x00|(pump.panelPhy[pos]-1);
		pump.cmd[4] = 0x04;
		pump.cmd[5] = 0x30|(_gun + 1);
		pump.cmdlen = 6;
		CRC_16_RX(pump.cmd, pump.cmdlen, &CrcHi, &CrcLo);
		pump.cmd[6] = CrcHi;
		pump.cmd[7] = CrcLo;
		pump.cmdlen = 8;
		pump.wantLen = 12;
		
		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]向串口写数据失败",node,pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd)-1;

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]取回复信息失败",node,pos + 1);
			continue;
		} 

		CRC_16_RX(pump.cmd,pump.cmdlen-2,&CrcHi, &CrcLo);
		if (pump.cmd[pump.cmdlen -2] != CrcHi || pump.cmd[pump.cmdlen -1] != CrcLo) {
			RunLog("节点[%d]面板[%d]返回数据CRC校验失败。",node,pos + 1);
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
	int ret,t;
	__u8 CrcHi;
	__u8 CrcLo;

	for (t = 0; t < TRY_TIME; t++) {
		pump.cmd[0] = 0xAA;
		pump.cmd[1] = 0x55;
		pump.cmd[2] = 0x02;
		pump.cmd[3] = 0x00|(pump.panelPhy[pos]-1);
		pump.cmd[4] = 0x07;
		pump.cmdlen = 5;
		CRC_16_RX(pump.cmd, pump.cmdlen, &CrcHi, &CrcLo);	/*计算Crc值*/
		pump.cmd[5] = CrcHi;
		pump.cmd[6] = CrcLo;
		pump.cmdlen = 7;
		pump.wantLen = 3;
		gpGunShm->addInf[pump.chnLog-1][0] = 0x04;	/*变长特征.*/

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]向串口写数据失败",node,pos + 1);
			gpGunShm->addInf[pump.chnLog-1][0] = 0x00;	/*恢复变长特征.*/
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd)-1;

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		gpGunShm->addInf[pump.chnLog-1][0] = 0x00;	/*恢复变长特征.*/
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]取回复信息失败",node,pos + 1);
			continue;
		}

		if (pump.cmd[4] != 'A') {
			RunLog("节点[%d]面板[%d]取泵码失败",node,pos + 1);
			continue;
		}

		CRC_16_RX(pump.cmd,pump.cmdlen-2,&CrcHi, &CrcLo);
		if (pump.cmd[pump.cmdlen -2] != CrcHi || pump.cmd[pump.cmdlen -1] != CrcLo) {
			RunLog("节点[%d]面板[%d]返回数据CRC校验失败。",node,pos + 1);
			continue;
		}
		
		return 0;
	}

	return -1;
}


static int GetPumpOnline(int pos, __u8 *status)
{
	int i,t;
	int ret;

	__u8 CrcHi;
	__u8 CrcLo;
	int chnLog = pump.chnLog;
	
	for (t = 0; t < TRY_TIME; t++) {
		pump.cmd[0] = 0xAA;
		pump.cmd[1] = 0x55;              // aa55报头
		pump.cmd[2] = 0x02;              // 数据长度 从地址（包括）到Crc（不包括）之间的长度
		pump.cmd[3] = 0x00 | (pump.panelPhy[pos] - 1);   //设备地址。用物理面板号保存
		pump.cmd[4] = 0x00;              // 命令：00是读状态命令
		pump.cmdlen = 5;
		CRC_16_RX(pump.cmd, pump.cmdlen, &CrcHi, &CrcLo);
		pump.cmd[5] = CrcHi;
		pump.cmd[6] = CrcLo;
		pump.cmdlen = 7;
		pump.wantLen = 3;

		gpGunShm ->addInf[chnLog - 1][0] = 0x06;
		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]下发取状态命令失败",node,pos+1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		//sleep(2);
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		gpGunShm ->addInf[chnLog - 1][0] = 0x00;
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]下发取状态命令成功, 取返回信息失败", node, pos + 1);
			continue;
		}
		// 防止干扰信号, 确认数据是加油机传来的
		CRC_16_RX(pump.cmd, pump.cmdlen - 2, &CrcHi, &CrcLo);
		if ((CrcHi != pump.cmd[pump.cmdlen - 2]) || (CrcLo != pump.cmd[pump.cmdlen - 1])) {
			continue;
		} 

		*status = pump.cmd[6];
		return 0;
	}
	return -1;
}


/*
 *  测试与加油机的连接是否正常, 测试该节点任意有枪即可
 *  成功返回 0; 失败返回 -1
 */



static int GetPumpStatus(int pos, __u8 *status)
{
	int ret;
	int t;
	__u8 CrcHi;
	__u8 CrcLo;

	for (t = 0; t < 2; t++) {
		pump.cmd[0] = 0xAA;
		pump.cmd[1] = 0x55;	/*aa55报头*/
		pump.cmd[2] = 0x02;	/*数据长度 从地址（包括）到Crc（不包括）之间的长度*/
		pump.cmd[3] = 0x00|(pump.panelPhy[pos]-1);/*设备地址。用物理面板号保存*/
		pump.cmd[4] = 0x00;	/*命令：00是读状态命令*/
		pump.cmdlen = 5;
		CRC_16_RX(pump.cmd, pump.cmdlen, &CrcHi, &CrcLo);
		pump.cmd[5] = CrcHi;
		pump.cmd[6] = CrcLo;
		pump.cmdlen = 7;
		pump.wantLen = 3;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]下发取状态命令失败",node,pos+1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd)-1;

		gpGunShm->addInf[pump.chnLog-1][0] = 0x06;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		gpGunShm->addInf[pump.chnLog-1][0] = 0x00;
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]下发取状态命令成功, 取返回信息失败", node, pos + 1);
			continue;
		}

		if (pump.cmd[4] != 'A') {
			RunLog("节点[%d]面板[%d]取状态失败",node,pos + 1);
			continue;
		}

		CRC_16_RX(pump.cmd,pump.cmdlen-2,&CrcHi, &CrcLo);
		if (pump.cmd[pump.cmdlen -2] != CrcHi || pump.cmd[pump.cmdlen -1] != CrcLo) {
			RunLog("节点[%d]面板[%d]返回数据CRC校验失败。",node,pos + 1);
			continue;
		}

		*status = pump.cmd[6];
		return 0;
	}
	return -1;
}


/*
 * func: 获取当前抬起油枪的枪号, 结果写入pump.gunLog[] & fp_data[].Log_Noz_State
 * ret : 0 - 成功, -1 - 失败 
 */
static int GetCurrentLNZ(int pos)
{
	int ret;
	int t;
	__u8 ln_status;
	__u8 status;
	
	for (t = 0; t < TRY_TIME; t++) {
		ret = GetPumpStatus(pos,&status);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]GetPumpStatus Failed",node,pos +1);
			continue;
		} else if (pump.cmd[4]=='A') {
			pump.gunLog[pos] = pump.cmd[7] & 0x0f;	// 当前抬起的油枪
			ln_status = 1 << (pump.gunLog[pos] - 1);
			RunLog("Node %d FP %d 的 %d 号枪抬起", node, pos + 1, pump.gunLog[pos]);
			lpDispenser_data->fp_data[pos].Log_Noz_State = ln_status;

			return 0;
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

	for (i = 0; i < pump.panelCnt; ++i) {
		pump.gunLog[i]=1;
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
		RunLog("Init.FP %d LNZ %d PR_Id %d,Prod_Price: %02x%02x%02x%02x", i + 1, j+1, pr_no + 1,
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
	int ret,i,j;
	int n;
	int pr_no;
	__u8 status;
	__u8 meter_total[7] = {0};
	__u8 amount_total[7] = {0};
	   	
	for (i = 0; i < pump.panelCnt; i++) {
		ret = GetPumpTotal(i);
		if (ret < 0) {
			RunLog("取 Node %d FP %d LNZ %d 的泵码失败", node, i, j);
			return -1;
		}
		for (j = 0; j < pump.gunCnt[i]; ++j) {
			pr_no = (lpDispenser_data->fp_ln_data[i][j]).PR_Id - 1;  // 0-7	
			if ((pr_no < 0) || (pr_no > 7)) {
				RunLog("InitGunTotal,FP %d, LNZ %d, PR_Id %d", i, j + 1, pr_no + 1);
				return -1;
			}

			n = 6;
			n += j * 4;
			// Volume_Total
			meter_total[0] = 0x0A;
			Hex2Bcd(pump.cmd+n,4,&meter_total[1],6);

			memcpy(lpDispenser_data->fp_m_data[pr_no].Meter_Total, meter_total, 7);
			memcpy((lpDispenser_data->fp_ln_data[i][j]).Log_Noz_Vol_Total, meter_total, 7);
#if DEBUG
			RunLog("节点[%d]面板[%d]Init.fp_ln_data[%d][%d].Vol_Total: %s ",node,i+1,i,j,
				Asc2Hex(lpDispenser_data->fp_ln_data[i][pump.gunLog[i] - 1].Log_Noz_Vol_Total, 7));
			RunLog("节点[%d]面板[%d]Init.fp_m_data[%d].Meter_Total[%s] ",node,i+1, pr_no, Asc2Hex(meter_total, 7));
			RunLog("面[%d]逻辑枪号[%d]的泵码为[%s]", i + 1, 
					j + 1, Asc2Hex(meter_total, 7));		
#endif
			// Amount_Total
			memset(amount_total, 0, sizeof(amount_total));
			amount_total[0] = 0x0A;
			memcpy((lpDispenser_data->fp_ln_data[i][pump.gunLog[i] - 1]).Log_Noz_Amount_Total, amount_total, 7);
		}
	}

	return 0;
}
	
static int SetSaleMode(int pos,int mode)
{
	int i;
	int ret;
	int t;

	__u8 CrcHi,CrcLo;

	RunLog("节点[%d]面板[%d]SetSaleMode...",node,pos + 1);	
	
	for (t = 0; t < TRY_TIME; t++) {
		pump.cmd[0] = 0xAA;
		pump.cmd[1] = 0x55;	/*aa55报头*/
		pump.cmd[2] = 0x03;	/*数据长度 从地址（包括）到Crc（不包括）之间的长度*/
		pump.cmd[3] = 0x00|(pump.panelPhy[pos]-1);	/*设备地址。用物理面板号保存*/	
		pump.cmd[4] = 0x01;	/*命令：01是设定通信模式*/
		if (mode == 1) { /*主控*/
			pump.cmd[5] = '1';   /*数据: "1"表示上位机控制*/
		} else if (mode == 2) {/*监控*/
			pump.cmd[5] = '0';						
		}
		pump.cmdlen = 6;
		CRC_16_RX(pump.cmd, pump.cmdlen, &CrcHi, &CrcLo);	/*计算Crc值*/
		pump.cmd[6] = CrcHi;
		pump.cmd[7] = CrcLo;
		pump.cmdlen = 8;
		pump.wantLen = 7;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]下发设置主控模式命令失败", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd)-1;

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]下发设置主控模式命令成功, 取返回信息失败", node, pos + 1);
			continue;
		} 

		CRC_16_RX(pump.cmd, pump.cmdlen-2, &CrcHi, &CrcLo);
//		RunLog("取得数据[%s].cmdlen=[%d],CrcHi=[%02x],CrcLo=[%02x]",
//				Asc2Hex(pump.cmd,pump.cmdlen),pump.cmdlen,CrcHi,CrcLo); 
		if ((CrcHi != pump.cmd[pump.cmdlen-2]) || (CrcLo != pump.cmd[pump.cmdlen-1])) {
			RunLog("节点[%d]面板[%d]Crc校验失败!",node,pos + 1);
			continue;
		}

		if (pump.cmd[4] != 'A') {
			RunLog("节点[%d]主控模式设置失败!",node);
			continue;
		} else {
			RunLog("节点[%d]主控模式设置成功!",node);
			return 0;				
		}
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
		for (i = 0; i < pump.panelCnt; ++i) {
			if (GetPumpStatus(i, &status) < 0) {
				fail_times++;
			}
		}
	} while (time(NULL) < return_time);

	return fail_times;
}


/*
 * func: 设置串口属性
 */
static int SetPort004()
{
	int ret;

	if (gpGunShm->chnLogSet[pump.chnLog - 1] == 0) {
		pump.cmd[0] = 0x1C;
		pump.cmd[1] = 0x52;
		pump.cmd[2] = 0x30 | (pump.chnPhy - 1);
		pump.cmd[3] = 0x03;	// 9600bps
		pump.cmd[4] = 0x00;
		pump.cmd[5] = 0x00;
		pump.cmd[6] = 0x00; 
		pump.cmdlen = 7;
		pump.wantLen = 0;

		ret =  WriteToPort(pump.fd, pump.chnPhy, pump.chnLog, pump.cmd,
			       	pump.cmdlen, 0, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			return -1;
		}
		gpGunShm->chnLogSet[pump.chnLog - 1] = 1;
	}
	RunLog("<榕兴>节点[%d], 设置串口属性成功", node);

	Ssleep(INTERVAL);

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
	__u8 msp[10] = {0};

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
		ret = _Get_price(pos, lnz);
		if(ret < 0){
			return -1;
		}
		memset(msp, 0, sizeof(msp));
		msp[0] = pump.cmd[6]<<4|pump.cmd[7]&0x0f;
		msp[1] = pump.cmd[8]<<4|pump.cmd[9]&0x0f;

		fm_idx = lpDispenser_data->fp_data[pos].Default_Fuelling_Mode - 1;
		RunLog("Node %d FP %d LNZ %d Now price is %02x.%02x; DST Price: %02x.%02x",
			node,pos+1,lnz+1, msp[0], msp[1],	
			lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2], 
		       	lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3]);
		

		if ((msp[0] != lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2]) ||
			(msp[1] != lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3])) {
			return -2;
		}
	}
	
	ChangeFP_State(node, 0x21 + pos, FP_State_Closed, NULL);
	RunLog("Node[%d]FP[%d]单价已变更正确, 切换状态为Closed", node, pos + 1);

	return 0;
}


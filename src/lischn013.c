/*
 *    Filename:  lischn013.c
 *
 * Description:  加油机协议转换模块 -- 鸿洋, 2400,8,n,1
 *
 *     Created:  2009.03.16
 *      Author:  Guojie Zhu <zhugj@css-intelligent.com>
 *
 *     History:   
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

#define GUN_EXTEND_LEN  4

#define FP_CLOSE        0xE0   // 油机处于设置模式时会变为该状态
#define FP_IDLE         0xE1
#define FP_CALLING      0xE2
#define FP_AUTHORISED   0xE3
#define FP_FUELLING     0xE4
#define FP_STOP         0xE5
#define FP_EOT          0xE6
#define FP_OFFLINE      0xE7

#define TRY_TIME	5
#define INTERVAL	200000  // 200ms

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
static int GetPrice(int pos);
static int ChangePrice();
static int GetTransData(int pos);
static int GetPumpStatus(int pos, __u8 *status);
static int GetPumpOnline(int pos, __u8 *status);
static int GetPumpTotal(int pos);
static int GetCurrentLNZ(int pos);
static int InitPumpTotal();
static int InitCurrentPrice();
static int InitGunLog();
static int SetPort013();
static int ReDoSetPort013();
static int SetCtrlMode(int pos);
static void clr_his_trans(int pos);
static int is_all_fp_ready();
static int GetRunningData(int pos);
static int keep_in_touch_xs(int x_seconds);
static __u8 checksum_hongyang(const __u8 *cmd, int cmd_len,  __u8 * result);
static int CheckPriceErr(int pos);

static PUMP pump;
static int node;
static int IFSFpos;
static DISPENSER_DATA *lpDispenser_data;
static NODE_CHANNEL *lpNode_channel;
static int fp_state[MAX_FP_CNT];        // 0 - IDLE, 1 - CALLING , 2 - DoStarted成功, 3 - Fuelling
static int dostop_flag = 0;             // 标记成功执行过 Terminate_FP

int ListenChn013(int chnLog)
{
	long	indtyp;
	int	ret;
	int	fd;
	int	j;
	int	i;
	__u8    status;
	__u8    cmd[8];
	
	sleep(5);   //防止此进程退出,而主进程没有启动完毕. 每个lischnxxx都需要
	RunLog("Listen Channel 013 ,logic channel[%d]", chnLog);
	memset(&pump, 0, sizeof(pump));
	pump.chnLog = chnLog;
	pump.panelCnt = 0;


	//1.   扫描共享内存, 检查该通道连接的串口, FP		
	for (i = 0; i < MAX_CHANNEL_CNT; i++) {
		if (chnLog == gpIfsf_shm->node_channel[i].chnLog) {	// 逻辑FP号相同
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
	
	//2.打开串口(连接在串口上的扩展口)，设置串口属性
	
	strcpy(pump.cmd, alltty[pump.port-1]);
	pump.fd = OpenSerial(pump.cmd);
	if (pump.fd < 0) {
		RunLog("Node[%d]打开串口[%s]失败", node, pump.cmd);
		return -1;
	}

	ret = SetPort013();
	if (ret < 0) {
		RunLog("Node[%d]设置串口属性失败",node);
		return -1;
	}
	
	//3.  测试加油机是否在线,如果不在线则等待
	while (1) {
		sleep(1);
		if (GetPumpOnline(0, &status) == 0) {
			for (i = pump.panelCnt - 1; i >= 0; --i) {
				if (0 != SetCtrlMode(i)) {
					return -1;
				}
			}

			break;   // Note!
		}

		gpIfsf_shm->node_online[IFSFpos] = 0; //不在线,不产生心跳.
		UserLog("Node %d 可能不在线", node);
	}


	// 3.5检查是否能够进入正常流程
	if (!is_all_fp_ready()) {
		// 未具备进入正常流程的条件, 退出重来
		return -1;
	}

	// Clear TRs of all FP
	clr_his_trans(255);

	//4.初始化默认当前逻辑枪号
	ret = InitGunLog();
	if (ret < 0) {
		RunLog("Node[%d], 初始化默认当前逻辑枪号失败",node);
		return -1;
	}

	 //5.初始化油品单价到数据库
	ret = InitCurrentPrice();
	if (ret < 0) {
		RunLog("Node[%d], 初始化油品单价到数据库失败",node);
		return -1;
	}

	//6.初始化泵码到数据库
	ret = InitPumpTotal();
	if (ret < 0) {
		RunLog("Node[%d], 初始化泵码到数据库失败",node);
		return -1;
	}

	
//	gpIfsf_shm->node_online[IFSFpos] = node;
	ChangeNode_Status(node, NODE_ONLINE);
	UserLog("Node %d 在线,状态[%02x]!", node, status);
	keep_in_touch_xs(12);               // 确保CD已经收到心跳之后再做后续操作

	// =============== Part.7 油机模块初始化完毕,转换FP状态为 Idle ==================
	// when dispenser is initialized , the status is Closed. it's waiting for set command from CD. 
	for (i = 0; i < lpNode_channel->cntOfFps; i++) {
		if ((__u8)FP_State_Closed != lpDispenser_data->fp_data[i].FP_State) {
			ret = ChangeFP_State(node, 0x21 + i, FP_State_Closed, NULL);
		}
	}

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
			RunLog("系统暂停sysStat = %d, fp_data[0].FP_State = %02x",
				gpGunShm->sysStat,lpDispenser_data->fp_data[0].FP_State);
		}

		indtyp = pump.indtyp;

		// 首先检查是否有命令
		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		ret = -1;

		if (gpIfsf_shm->auth_flag - FCC_SWITCH_AUTH != 0) {  // 后台在线
			// 查看是否有自己的消息
			ret = RecvMsg(ORD_MSG_TRAN, pump.cmd, &pump.cmdlen, &indtyp, TIME_FOR_RECV_CMD); 
			if (ret == 0) {
				ret = DoCmd();
				if (ret != 0) {
					RunLog("Node %d 处理后台命令失败", node);
				}
			}

			DoPolling();
//			Ssleep(100000);       // 正常轮询间隔为100ms + INTERVAL
		} else {
			Ssleep(200000);
			DoPolling();
		}
	}

}

/*
 * func: 执行上位机命令
 */
static int DoCmd()
{
	int ret;
	int fp_id;
	int pos;    //pos is No. of  fuelling poit(panel) of a dispenser: 0, 1, 2, 3
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
			if (pump.cmd[0] != 0x18 && pump.cmd[0] != 0x19) {  // DoPrice & DoMode不需要处理FP号
				RunLog("Node[%d] , 非法的FP地址",node);
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
		case 0x19:	// 更改控制模式
//			ret = DoMode();	      // Unsupported
			break;
		case 0x1C:	// Terminate_FP
			ret = HandleStateErr(node, pos, Terminate_FP,
				lpDispenser_data->fp_data[pos].FP_State, msgbuffer); 
			if (ret < 0) {       // 发生错误，当前状态不允许做该操作
				break;
			}

			if ((lpDispenser_data->fp_data[pos].FP_State) == ((__u8)FP_State_Calling)) {
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
		case 0x3D:      // Preset  amount or volume
		case 0x4D:
			ret = HandleStateErr(node, pos, Do_Ration,
				lpDispenser_data->fp_data[pos].FP_State, msgbuffer); 
			if (ret < 0) {
				lpDispenser_data->fp_data[pos].Log_Noz_Mask = 0xFF;
				bzero(&lpDispenser_data->fp_data[pos].Remote_Volume_Preset, 5);
				bzero(&lpDispenser_data->fp_data[pos].Remote_Amount_Prepay, 5);
				break;
			}
			ret = DoRation(pos);
			if (ret == 0) {       // exec success, change fp state
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
			RunLog("Node[%d], 接受到未定义命令[0x%02x]",node, pump.cmd[0]);
			break;
	}

	return ret;
}



/*
 * func: 对FP进行状态轮询, 根据状态做相应处理
 */
static void DoPolling()
{
	int i;
	int j;
	int err_cnt;
	int ret;
	int tmp_lnz;
	int pump_err_cnt[MAX_FP_CNT] = {0};
	static int times = 0;
	static int fail_cnt = 0;  // 记录GetPumpStatus 失败次数, 用于判定离线
	static __u8 flag_soft_offline = 0;

	__u8 status;
	
	for (i = 0; i < pump.panelCnt; i++) {
		ret = GetPumpStatus(i, &status);
		if (ret < 0 || (0xFF == flag_soft_offline)) {
			flag_soft_offline = 0;

			if (gpIfsf_shm->node_online[IFSFpos] == node) {
				if ((++fail_cnt) < MAX_FAIL_CNT) {  // 加油机离线判断
					/*
					 * 因重设串口属性会导致ReadTTY接收到"非法数据", 故暂时移除
					 * 2009.02.19
					if (fail_cnt >= (MAX_FAIL_CNT / 2)) {
						if (SetPort013() >= 0) {
							RunLog("Node %d 重设串口属性成功", node);
						}
					}
					*/
					continue;
				}

				// Step.1
				memset(fp_state, 0, sizeof(fp_state));
				dostop_flag = 0;

				// Step.2
//				gpIfsf_shm->node_online[IFSFpos] = 0;
				ChangeNode_Status(node, NODE_OFFLINE);
				pump.time_offline = (3 * gpIfsf_shm->fcc_com_sv_data.Heartbeat_Interval) + time(NULL);
				RunLog("====== Node %d 离线! =============================", node);

				// Step.3 油机离线, 无需上传状态改变信息, 所以不使用ChangeFP_State, 直接操作FP_State
				for (j = 0; j < pump.panelCnt; ++j) {
					lpDispenser_data->fp_data[j].FP_State = FP_State_Inoperative;
					lpDispenser_data->fp_data[j].Log_Noz_State = 0;
				}
			}
			
			// 重设串口属性
			//ReDoSetPort013();
			continue;
		} else {
			if (gpIfsf_shm->node_online[IFSFpos] == 0) {
				if ((--fail_cnt) > 0) {
					fail_cnt = 1;   // 加快上线速度
					continue;
				} else if (time(NULL) < pump.time_offline) {
					RunLog("============== waiting for 30s =============");
					continue;
				} 

				// fail_cnt复位
				fail_cnt = 0;

				// Step.1                // 清除历史交易,转变状态位FP_IDLE
				clr_his_trans(255);

				// Step.2
				ret = InitPumpTotal();   // 更新泵码到数据库
				if (ret < 0) {
					RunLog("Node[%d]更新泵码到数据库失败", node);
					continue;
				}

				// Step.3
				for (j = 0; j < pump.panelCnt; ++j) {
					lpDispenser_data->fp_data[j].FP_State = FP_State_Closed;
				}

//				gpIfsf_shm->node_online[IFSFpos] = node;
				ChangeNode_Status(node, NODE_ONLINE);
				RunLog("====== Node %d 恢复联线! =============================", node);
				keep_in_touch_xs(12);               // 确保CD已经收到心跳之后再做后续操作
				// Step.4                 // 主动上传当前FP状态
				for (j = 0; j < pump.panelCnt; ++j) {
					ChangeFP_State(node, j + 0x21, FP_State_Closed, NULL);
				}
				continue;
			}

			fail_cnt = 0;
		}


		if (gpIfsf_shm->auth_flag - FCC_SWITCH_AUTH != 0) {  //如果有后台
			if ((__u8)FP_State_Closed == lpDispenser_data->fp_data[i].FP_State) {
				if (i == 0) {
					times += pump.panelCnt;					
				}
				if (times > (SENDSTATE *  pump.panelCnt)) {
					if (i >= pump.panelCnt -1)
						times = 0;
					UserLog("Node[%d]的FP[%d]处于Closed状态", node, i + 1);
				}

				// Closed 状态下也要能够变价
				ChangePrice();
				continue; 
			}
		} else {
			if ((__u8)FP_State_Closed == lpDispenser_data->fp_data[i].FP_State) {
				RunLog("Node[%d]FP[%d], DoPolling, 没有后台改变状态为IDLE", node,i + 1);
				ret = SetHbStatus(node, 0);
				ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
			}
		}

		// 若FP状态为Inoperative, 要求人工干预,程序不能改变其状态,也不能在该状态下做任何操作
		if ((__u8)FP_State_Inoperative == lpDispenser_data->fp_data[i].FP_State) {
			RunLog("Node[%d]FP[%d] 处于Inoperative状态!", node, i + 1);
			// Inoperative 状态下也要能够变价
			ChangePrice();

			CheckPriceErr(i);
			continue;
		}

		switch (status) {
		case FP_IDLE:
			if (fp_state[i] >= 2) {            // 对没有采到EOT状态的补充处理
				// 1. ChangeFP_State to IDLE first
				//由于是先发送状态后保存交易记录，所以
			       //需要先用tmp_lnz把当前用的油枪存起来，供保存记录时用，
			       //否则保存的记录枪号就是0了
				tmp_lnz = lpDispenser_data->fp_data[i].Log_Noz_State;
				lpDispenser_data->fp_data[i].Log_Noz_State = 0x00;     
				if (((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State) && \
					((__u8)FP_State_Inoperative != lpDispenser_data->fp_data[i].FP_State)) {
					ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
				}
				RunLog("Node %d FP %d 的 %d 号枪挂枪", node, i + 1, pump.gunLog[i]);
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
				upload_offline_tr(node, NULL);
			}

			ret = ChangePrice();
			break;
		case FP_CALLING:			// 抬枪
			if ((__u8)FP_State_Calling > lpDispenser_data->fp_data[i].FP_State) {
				ret = DoCall(i);
				fp_state[i] = 1;
				DoStarted(i);
			}
			break;
		case FP_AUTHORISED:
			if ((__u8)FP_State_Authorised > lpDispenser_data->fp_data[i].FP_State) {
				ChangeFP_State(node, i + 0x21, FP_State_Authorised, NULL);
			}
			break;
		case FP_FUELLING:
			// 补充处理
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
						break;
					}
				}
			}
			break;
		case FP_EOT:
			// 补充处理
			if (fp_state[i] < 2) {
				if (DoStarted(i) < 0) {
					continue;
				}
			}

			if (fp_state[i] >= 2) {
				// 1. ChangeFP_State to IDLE first
				//由于是先发送状态后保存交易记录，所以
				//需要先用tmp_lnz把当前用的油枪存起来，供保存记录时用，
				//否则保存的记录枪号就是0了
				tmp_lnz = lpDispenser_data->fp_data[i].Log_Noz_State;
				lpDispenser_data->fp_data[i].Log_Noz_State = 0x00;     
				if (((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State) && \
					((__u8)FP_State_Inoperative != lpDispenser_data->fp_data[i].FP_State)) {
					ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
				}
				RunLog("Node %d FP %d 的 %d 号枪挂枪", node, i + 1, pump.gunLog[i]);
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
				upload_offline_tr(node, NULL);
			}

			break;
		case FP_STOP:
			if ((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State) {
				ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
			}
			break;
		case FP_OFFLINE:
			SetCtrlMode(i);
			fail_cnt = MAX_FAIL_CNT;
			flag_soft_offline = 0xFF;   // 若联线期间油机上做了脱机操作, 那么模拟油机离线, 并做相应处理;
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
	unsigned int i;
      
	ret = SetHbStatus(node, 0);
	if (ret<0) {
		RunLog("Node[%d] 心跳状态改变错误",node);
		SendCmdAck(NACK, msgbuffer);
		ret = ChangeFP_State(node, 0x21 + pos, FP_State_Idle, NULL);
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
			RunLog("Node[%d]FP[%d],BeingTr4FP Failed", node, i + 1);
			return -1;
		}
	}

	// GetCurrentLNZ & BeginTr4Fp 成功, 则置位 fp_state[i] 为 2
	fp_state[i] = 2;

	return 0;
}

/*
 * func: PreSet/PrePay & Release_FP
 */
static int DoRation(int pos)  
{
	int ret;
	int trynum;
	__u8 pre_type;
	
	if (pump.cmd[0] == 0x3D) {
		pre_type = 0xA5;
		memcpy(&pump.cmd[3], &(lpDispenser_data->fp_data[pos].Remote_Volume_Preset[2]), 3);
	} else if (pump.cmd[0] == 0x4D) {
		pre_type = 0xA4;
		memcpy(&pump.cmd[3], &(lpDispenser_data->fp_data[pos].Remote_Amount_Prepay[2]), 3);
	} else {
		return -1;
	}
	
	for (trynum = 0; trynum < TRY_TIME; ++trynum) {
		pump.cmd[0] = 0xFC;
		pump.cmd[1] = pre_type;
		pump.cmd[2] = pump.panelPhy[pos];
		checksum_hongyang(&pump.cmd[1], 5, &pump.cmd[6]);
		pump.cmd[8] = 0xFF;
		pump.cmdlen = 9;
		pump.wantLen = 7;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]写预置加油命令失败", node, pos + 1);
			continue;
		}

		RunLog("发预置加油命令[%s]到Node[%d].FP[%d]成功",
			Asc2Hex(pump.cmd, pump.cmdlen), node, pos + 1);

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]写预置加油命令成功, 取返回信息失败", node, pos + 1);
			continue;
		}

		if (0xFC != pump.cmd[0] || 0xFF != pump.cmd[6]) {
			RunLog("Node[%d]FP[%d]返回数据错误", node, pos + 1);
			continue;
		}

		if (((pump.cmd[4] << 4) | pump.cmd[5]) != 
			checksum_hongyang(&pump.cmd[1], pump.wantLen - 4, NULL)) {
			RunLog("Node[%d]FP[%d]返回数据校验错误", node, pos + 1);
			continue;
		}

		if (0x99 != pump.cmd[3]) {
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
 * func: 加油结束，进行交易记录的读取和处理
 *       零交易的判断和处理由DoEot来做, DoPoling 不关心
 * ret : 0 - 成功; -1 - 失败
 *
 */
static int DoEot(int pos)
{
	int i;
	int ret;
	int pr_no;
	int err_cnt;
	__u8 price[5];
	__u8 volume[5];
	__u8 amount[5];
	__u8 total[7];
	__u8 pre_total[7];
	__u8 meter_total[7] = {0};
	__u8 amount_total[7] = {0};	
	
	RunLog("Node[%d]FP[%d]处理新的交易", node, pos + 1);

	//1. 获得泵码，和累积加油金额
	err_cnt = 0;
	do {
		ret = GetPumpTotal(pos);
		if (ret < 0) {
			err_cnt++;
			RunLog("Node[%d]FP[%d] 取加油机泵码失败",node,pos+1);
		}
	} while (ret < 0 && err_cnt <= 5);

	if (ret < 0) {
		return -1;
	}

	meter_total[0] = 0x0A;
	memcpy(&meter_total[1], &pump.cmd[3], 6);

	amount_total[0] = 0x0A;
	memcpy(&amount_total[1], &pump.cmd[9], 6);

	memcpy(pre_total, (lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Vol_Total, 7);

	//2. 检查泵码是否有变化,如果有则保存交易记录
	if (memcmp(pre_total, meter_total, 7) != 0) {
#if 0
		RunLog("原泵码为 %s",Asc2Hex(lpDispenser_data->fp_m_data[pr_no].Meter_Total,7));
		RunLog("当前泵码为 %s",Asc2Hex(meter_total,7));	  
#endif
		err_cnt = 0;
		do {
			ret = GetTransData(pos);	
			if (ret != 0) {
				err_cnt++;
				RunLog("Node[%d]FP[%d]取交易记录失败", node, pos + 1);
			}

			// 比较泵码判断取道的交易是否是最新的交易
			if (memcmp(&pump.cmd[11], &meter_total[1], 6) != 0) {
				ret = -1;
			}
		} while (ret < 0 && err_cnt <= 5);

		if (ret < 0) {
			return -1;
		}

		//Current_Volume  bin8 + bcd8
		volume[0] = 0x06;
		volume[1] = 0x00;
		memcpy(&volume[2],  &pump.cmd[6], 3);
		memcpy(lpDispenser_data->fp_data[pos].Current_Volume, volume, 5);

		//Current_Amount  bin8 + bcd 8
		amount[0] = 0x06;
		amount[1] = 0x00;
		memcpy(&amount[2], &pump.cmd[3], 3);
		memcpy(lpDispenser_data->fp_data[pos].Current_Amount, amount, 5);

		//Current_Unit_Price bin8 + bcd8
		price[0] = 0x04;
		price[1] = 0x00;
		memcpy(&price[2], &pump.cmd[9], 2);
		memcpy(lpDispenser_data->fp_data[pos].Current_Unit_Price, price, 4);

		//3. 得到油品id 
		pr_no = (lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).PR_Id - 1;  // 0 - 7	
		if ((pr_no < 0) || (pr_no > 7)) {
			RunLog("DoEot PR_Id error, PR_Id[%d] <ERR>", pr_no + 1);
		}
				
		memcpy(lpDispenser_data->fp_m_data[pr_no].Meter_Total, meter_total, 7);		
		memcpy((lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Vol_Total, meter_total, 7);

#if 0
		RunLog("Node[%02d]FP[%d]枪[%d]## Amount: %02x%02x.%02x, Volume: %02x%02x.%02x, Price: %02x.%02x, Total: %02x%02x%02x%02x.%02x",
			node, pos+1, pump.gunLog[pos], amount[2], amount[3], amount[4], volume[2], volume[3], volume[4],
		       	price[2], price[3], meter_total[2], meter_total[3], meter_total[4], meter_total[5], meter_total[6]);
#endif
		
		// Update Amount_Total
		memcpy((lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Amount_Total, amount_total, 7);

		memcpy(lpDispenser_data->fp_data[pos].Current_Prod_Nb, lpDispenser_data->fp_pr_data[pr_no].Prod_Nb, 4);

		err_cnt = 0;
		do {
			ret = EndTr4Ln(node, pos + 0x21, pump.gunLog[pos] + 0x10);
			if (ret < 0) {
				RunLog("Node[%d]FP[%d]DoEot.EndTr4Ln 失败, ret: %d <ERR>", node,pos +1 ,ret);
				err_cnt++;
			}
		} while (ret < 0 && err_cnt <= 5);

		err_cnt = 0;
		do {
			ret = EndTr4Fp(node, pos + 0x21, pre_total,
				(lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Vol_Total);
			if (ret < 0) {
				RunLog("Node[%d]FP[%d] DoEot.EndTr4Fp 失败, ret: %d <ERR>", node,pos + 1,ret);
				err_cnt++;
			}
		} while (ret < 0 && err_cnt <= 8);

	}else {
		GetTransData(pos);    // 用于转换状态
	
		ret = EndZeroTr4Fp(node, pos + 0x21);
		if (ret < 0) {
			RunLog("DoEot.EndZeroTr4Fp 失败, ret: %d <ERR>", ret);
		}
	}
	
	
	return 0;
}

/*
 * func: 暂停加油，在此做Terminate_FP用
 */
static int DoStop(int pos)
{
	int ret;
	int trynum;

	RunLog("Node %d FP %d DoStop", node, pos + 1);

	for (trynum = 0; trynum < TRY_TIME; trynum++) {
		pump.cmd[0] = 0xFC;
		pump.cmd[1] = 0xA6;
		pump.cmd[2] = pump.panelPhy[pos];
		checksum_hongyang(&pump.cmd[1], 2, &pump.cmd[3]);
		pump.cmd[5] = 0xFF;
		pump.cmdlen = 6;
		pump.wantLen = 7;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]下发停止加油命令失败", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd)-1;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]下发停止加油命令成功, 取返回信息失败", node, pos + 1);
			continue;
		}
		
		if (0xFC != pump.cmd[0] || 0xFF != pump.cmd[6]) {
			RunLog("Node[%d]FP[%d]返回数据错误", node, pos + 1);
			continue;
		}

		if (((pump.cmd[4] << 4) | pump.cmd[5]) != 
			checksum_hongyang(&pump.cmd[1], pump.wantLen - 4, NULL)) {
			RunLog("Node[%d]FP[%d]返回数据校验错误", node, pos + 1);
			continue;
		}

		if (0x99 != pump.cmd[3]) {
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
	int  trynum;
	__u8 status;

	RunLog("Node %d FP %d DoAuth", node, pos + 1);

	for (trynum = 0; trynum < TRY_TIME; trynum++) {
		pump.cmd[0] = 0xFC;
		pump.cmd[1] = 0xA3;
		pump.cmd[2] = pump.panelPhy[pos];
		checksum_hongyang(&pump.cmd[1], 2, &pump.cmd[3]);
		pump.cmd[5] = 0xFF;
		pump.cmdlen = 6;
		pump.wantLen = 7;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]下发授权命令失败",node,pos +1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd)-1;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]授权失败",node,pos+1);
			continue;
		}
		
		if (0xFC != pump.cmd[0] || 0xFF != pump.cmd[6]) {
			RunLog("Node[%d]FP[%d]返回数据错误", node, pos + 1);
			continue;
		}

		if (((pump.cmd[4] << 4) | pump.cmd[5]) != 
			checksum_hongyang(&pump.cmd[1], pump.wantLen - 4, NULL)) {
			RunLog("Node[%d]FP[%d]返回数据校验错误", node, pos + 1);
			continue;
		}

		if (0x99 != pump.cmd[3]) {
			continue;
		}

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
	int  trynum;
	__u8 amount[5],volume[5];
	const __u8 arr_zero[3] = {0};

	ret = GetRunningData(pos);
	if (ret < 0) {
		return -1;
	}

	if (fp_state[pos] != 3) {
		if (memcmp(&pump.cmd[7], arr_zero, 3) == 0) {
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
	memcpy(&amount[2], &pump.cmd[4], 3);

	if ((__u8)FP_State_Fuelling == lpDispenser_data->fp_data[pos].FP_State) {
		ret = ChangeFP_Run_Trans(node, 0x21 + pos, amount,  volume);
	}

	return 0;
}

/*
 * func: 抬枪处理 - 报告抬枪信号
 */
static int DoCall(int pos)
{
	int ret;
	__u8 ln_status;

	ret = GetCurrentLNZ(pos);
	if (ret < 0) {
		RunLog("Node[%d]FP[%d], 获取当前油枪枪号失败",node,pos + 1);
		return -1;
	}

	ret = ChangeFP_State(node, pos + 0x21, FP_State_Calling, NULL);

	//上位机在线,但是允许自动授权;上位机不在线, 授权开关置于开的状态,此时自动授权
	if ((gpIfsf_shm->auth_flag == FCC_SWITCH_AUTH) && (lock_status() == 1)) {
		ret = DoAuth(pos);
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]自动授权失败",node,pos +1);
		}
		ret = ChangeFP_State(node, pos + 0x21, FP_State_Started, NULL);
		RunLog("Node[%d]FP[%d]自动授权成功",node ,pos +1);
		SO_OFFLINE_TR(node, pos);
	}

	return ret;
}



/*
 * func: 执行变价, 失败改变FP状态为INOPERATIVE, 并写错误数据库
 */
static int ChangePrice() 
{
	int i, j;
	int ret, fm_idx;
	int trynum;
	__u8 pr_no;
	__u8 err_code;
	__u8 err_code_tmp;
	__u8 status = 0;


	for (pr_no = 0; pr_no < FP_PR_NB_CNT; pr_no++) {    
              //一台加油机只有一个油品也只有一个油价
		if (pump.newPrice[pr_no][2] != 0x00 || pump.newPrice[pr_no][3] != 0x00) {	
			for (i = 0; i < pump.panelCnt; i++) {
				// 1. 先判断油机所有枪状态
				err_code = 0x99;
				for (j = pump.panelCnt - 1; j >= 0; --j) {
					ret = GetPumpStatus(i, &status);
					if (ret != 0 || status != FP_IDLE) {
						return 1;
					}
				}

				// 2. 下发变价命令
				for (j = 0; j < MAX_FP_LN_CNT; j++) { // 不能用gunCnt,1枪可能不用
#if DEBUG
					RunLog(">>> fp_ln_data[%d][%d].PR_Id: %d", i, j,
							lpDispenser_data->fp_ln_data[i][j].PR_Id);
#endif
					if (lpDispenser_data->fp_ln_data[i][j].PR_Id - 1 == pr_no) {
					err_code_tmp = 0x99;
				CriticalLog("下发变价命令给节点[%02d]面[%d]枪[%d]......", node, i + 1, j + 1);

					for (trynum = 0; trynum < TRY_TIME; trynum++) {
						pump.cmd[0] = 0xFC;
						pump.cmd[1] = 0xA0;
						pump.cmd[2] = pump.panelPhy[i];
						pump.cmd[3] = pump.newPrice[pr_no][2];
						pump.cmd[4] = pump.newPrice[pr_no][3];
						checksum_hongyang(&pump.cmd[1], 4, &pump.cmd[5]);
						pump.cmd[7] = 0xFF;
						pump.cmdlen = 8;
						pump.wantLen = 7;

						Ssleep(INTERVAL);
						ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
								pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
						if (ret < 0) {
							RunLog("Node[%d]FP[%d]下发变价命令失败", node,i + 1);
							continue;
						} 

						RunLog("Node[%d].FP[%d].PriceChange_CMD: %s",
							node, i + 1, Asc2Hex(pump.cmd, pump.cmdlen));

						// 变价成功，新单价写入数据库 & 清零缓存

						memset(pump.cmd, 0, sizeof(pump.cmd));
						pump.cmdlen = sizeof(pump.cmd) - 1;

						ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
						if (ret < 0) {
							RunLog("Node[%d]FP[%d]发变价命令成功, 取回复信息失败", node, i + 1);
							continue;
						}

						if (0xFC != pump.cmd[0] || 0xFF != pump.cmd[6]) {
							RunLog("Node[%d]FP[%d]返回数据错误", node, i + 1);
							err_code_tmp = 0x37;
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
							continue;
						}

						if (((pump.cmd[4] << 4) | pump.cmd[5]) != 
							checksum_hongyang(&pump.cmd[1], pump.wantLen - 4, NULL)) {
							RunLog("Node[%d]FP[%d]返回数据校验错误", node, i + 1);
							err_code_tmp = 0x37;
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
							continue;
						}

						if (0x99 != pump.cmd[3]) {
							RunLog("Node[%d]FP[%d]执行变价命令失败", node, i + 1);
							err_code_tmp = 0x37;
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
							continue;
						} 

						ret = GetPrice(i);
						if (ret == 0 && (memcmp(&pump.cmd[3], &pump.newPrice[pr_no][2], 2) != 0)) {
							RunLog("Node[%d]FP[%d]执行变价成功, 但当前单价与新单价不同",
														node, i + 1);
							err_code_tmp = 0x37; // 变价执行失败
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
							continue;
						}

						fm_idx = lpDispenser_data->fp_data[i].Default_Fuelling_Mode - 1;
					RunLog("Node[%d]FP[%d]发送变价命令成功,新单价fp_fm_data[%d][%d].Prode_Price: %s",
							node, i + 1, pr_no, fm_idx,
						       	Asc2Hex(lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price, 4));
					CriticalLog("节点[%02d]面[%d]枪[%d]变价成功,新单价为%02x.%02x",
						       	node, i + 1, j + 1,
							lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2],
							lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3]);

						err_code_tmp = 0x99;
						break;
					}

					if (trynum >= TRY_TIME) {
						// Write Error Code:  price Change Error (Spare)
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

				Ssleep(2000000 - INTERVAL);
			} // end of for.i < panelCnt
			bzero(&pump.newPrice[pr_no], 4);
		} // end of if.newPrice
	} // end of for.pr_no

	return 0;
}


/*
 * func: 获取该FP所有枪的泵码
 */
static int GetPumpTotal(int pos)
{
	int ret;
	int trynum;

	for (trynum = 0; trynum <TRY_TIME ; trynum++) {
		Ssleep(INTERVAL);
		pump.cmd[0] = 0xFC;
		pump.cmd[1] = 0xA7;
		pump.cmd[2] = pump.panelPhy[pos];
		checksum_hongyang(&pump.cmd[1], 2, &pump.cmd[3]);
		pump.cmd[5] = 0xFF;
		pump.cmdlen = 6;
		pump.wantLen = 18;
		
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]下发取泵码命令失败",node,pos +1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]下发取泵码命令成功, 取返回信息失败",node,pos +1);
			continue;
		}

		if (0xFC != pump.cmd[0] || 0xFF != pump.cmd[17]) {
			RunLog("Node[%d]FP[%d]返回数据错误", node, pos + 1);
			continue;
		}

		if (((pump.cmd[15] << 4) | pump.cmd[16]) != 
			checksum_hongyang(&pump.cmd[1], pump.wantLen - 4, NULL)) {
			RunLog("Node[%d]FP[%d]状态数据校验错误", node, pos + 1);
			continue;
		}

		return 0;
	}

	return -1;
}


/*
 * func: 读取当次交易记录
 */ 
static int GetTransData(int pos)
{
	int ret;
	int trynum;

	for (trynum = 0; trynum < TRY_TIME; ++trynum) { 
		pump.cmd[0] = 0xFC;
		pump.cmd[1] = 0xA8;
		pump.cmd[2] = pump.panelPhy[pos];
		checksum_hongyang(&pump.cmd[1], 2, &pump.cmd[3]);
		pump.cmd[5] = 0xFF;
		pump.cmdlen = 6;
		pump.wantLen = 26;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]下发读交易命令失败",node ,pos +1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]下发读交易命令成功, 取返回信息失败",node ,pos +1);
			continue;
		}

		if (0xFC != pump.cmd[0] || 0xFF != pump.cmd[25]) {
			RunLog("Node[%d]FP[%d]返回数据错误", node, pos + 1);
			continue;
		}

		if (((pump.cmd[23] << 4) | pump.cmd[24]) != 
			checksum_hongyang(&pump.cmd[1], pump.wantLen - 4, NULL)) {
			RunLog("Node[%d]FP[%d]返回数据校验错误", node, pos + 1);
			continue;
		}
		return 0;
	}

	return -1;
}

/*
 * func: 读取FP状态
 */
static int GetPumpStatus(int pos, __u8 *status)
{
	int i;
	int ret;
	static __u8 pre_status[MAX_FP_CNT] = {0};
	*status = 0x00;

	for (i = 0; i < 2; ++i) {
		pump.cmd[0] = 0xFC;
		pump.cmd[1] = 0xAA;
		pump.cmd[2] = pump.panelPhy[pos];
		checksum_hongyang(&pump.cmd[1], 2, &pump.cmd[3]);
		pump.cmd[5] = 0xFF;
		pump.cmdlen = 6;
		pump.wantLen = 7;

		Ssleep(INTERVAL); 
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]发读状态命令失败",node,pos +1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]发读状态命令成功, 取返回信息失败",node,pos +1);
			continue;
		} 

		if (0xFC != pump.cmd[0] || 0xFF != pump.cmd[6]) {
			RunLog("Node[%d]FP[%d]返回数据错误", node, pos + 1);
			continue;
		}

		if (((pump.cmd[4] << 4) | pump.cmd[5]) != 
			checksum_hongyang(&pump.cmd[1], pump.wantLen - 4, NULL)) {
			RunLog("Node[%d]FP[%d]返回数据校验错误", node, pos + 1);
			continue;
		}

		*status = pump.cmd[3];

		if (*status != pre_status[pos]) {
			pre_status[pos] = *status;
			RunLog("Node[%d]FP[%d]当前状态[%02x]", node, pos + 1, *status);
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
	int i;
	int ret;
	*status = 0x00;

	for (i = 0; i < TRY_TIME; ++i) {
		pump.cmd[0] = 0xFC;
		pump.cmd[1] = 0xAA;
		pump.cmd[2] = pump.panelPhy[pos];
		checksum_hongyang(&pump.cmd[1], 2, &pump.cmd[3]);
		pump.cmd[5] = 0xFF;
		pump.cmdlen = 6;
		pump.wantLen = 7;

		Ssleep(INTERVAL); 
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]发读状态命令失败",node,pos +1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]发读状态命令成功, 取返回信息失败",node,pos +1);
			continue;
		} 

		if (0xFC != pump.cmd[0] || 0xFF != pump.cmd[6]) {
			RunLog("Node[%d]FP[%d]返回数据错误", node, pos + 1);
			continue;
		}

		if (((pump.cmd[4] << 4) | pump.cmd[5]) != 
			checksum_hongyang(&pump.cmd[1], pump.wantLen - 4, NULL)) {
			RunLog("Node[%d]FP[%d]返回数据校验错误", node, pos + 1);
			continue;
		}

		*status = pump.cmd[3];
		return 0;
	}

	return -1;
}

/*
 * func: 读取FP状态
 */
static int GetRunningData(int pos)
{
	int i;
	int ret;

	for (i = 0; i < 2; ++i) {
		pump.cmd[0] = 0xFC;
		pump.cmd[1] = 0xA2;
		pump.cmd[2] = pump.panelPhy[pos];
		checksum_hongyang(&pump.cmd[1], 2, &pump.cmd[3]);
		pump.cmd[5] = 0xFF;
		pump.cmdlen = 6;
		pump.wantLen = 15;

		Ssleep(INTERVAL); 
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]发读状态命令失败",node,pos +1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]发读状态命令成功, 取返回信息失败",node,pos +1);
			continue;
		} 

		if (0xFC != pump.cmd[0] || 0xFF != pump.cmd[14]) {
			RunLog("Node[%d]FP[%d]返回数据错误", node, pos + 1);
			continue;
		}

		if (((pump.cmd[12] << 4) | pump.cmd[13]) != 
			checksum_hongyang(&pump.cmd[1], pump.wantLen - 4, NULL)) {
			RunLog("Node[%d]FP[%d]返回数据校验错误", node, pos + 1);
			continue;
		}

		return 0;
	}

	return -1;
}

/*
 * func: 读取油品单价
 */
static int GetPrice(int pos)
{
	int ret;
	int trynum;

	for (trynum = 0; trynum < TRY_TIME; ++trynum) {
		pump.cmd[0] = 0xFC;
		pump.cmd[1] = 0xA1;
		pump.cmd[2] = pump.panelPhy[pos];
		checksum_hongyang(&pump.cmd[1], 2, &pump.cmd[3]);
		pump.cmd[5] = 0xFF;
		pump.cmdlen = 6;
		pump.wantLen = 8;

		Ssleep(INTERVAL); 
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]发读单价命令失败", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]发读单价命令成功, 取返回信息失败",node,pos +1);
			continue;
		} 

		if (0xFC != pump.cmd[0] || 0xFF != pump.cmd[7]) {
			RunLog("Node[%d]FP[%d]返回数据错误", node, pos + 1);
			continue;
		}

		if (((pump.cmd[5] << 4) | pump.cmd[6]) != 
			checksum_hongyang(&pump.cmd[1], pump.wantLen - 4, NULL)) {
			RunLog("Node[%d]FP[%d]返回数据校验错误", node, pos + 1);
			continue;
		}

		return 0;
	}

	return -1;
}

/*
 * func: 获取当前抬起油枪的枪号, 结果写入pump.gunLog[]，并ChangeLog_Noz_State
 * 单枪机
 * ret : 0 - 成功, -1 - 失败 
 */
static int GetCurrentLNZ(int pos)
{
	  RunLog("Node[%d]FP[%d] %d 号枪抬起", node, pos + 1, pump.gunLog[pos]);
	  lpDispenser_data->fp_data[pos].Log_Noz_State = 1 << pump.gunLog[pos] - 1;	// 当前抬起的油枪	

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
		RunLog("Node[%d]FP[%d] LNZ %d PR_Id %d,Prod_Price: %02x%02x%02x%02x", node,i + 1, j, pr_no + 1,
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
	__u8 status;
	__u8 meter_total[7] = {0};
	__u8 amount_total[7] = {0};

	for (i = 0; i < pump.panelCnt; i++) {
		ret = GetPumpTotal(i);
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]取泵码失败", node,i+1);
			return -1;
		}

	       // Volume_Total
		pr_no = (lpDispenser_data->fp_ln_data[i][pump.gunLog[i] - 1]).PR_Id - 1;  // 0-7	
		
		meter_total[0] = 0x0A;
		memcpy(&meter_total[1], &pump.cmd[3], 6);
		
		memcpy(lpDispenser_data->fp_m_data[pr_no].Meter_Total, meter_total, 7);
		memcpy((lpDispenser_data->fp_ln_data[i][pump.gunLog[i] - 1]).Log_Noz_Vol_Total, meter_total, 7);
#if DEBUG
		RunLog("Node[%d]FP[%d], Init.fp_ln_data[%d][%d].Vol_Total: %s ",
				node,i+1,i,pump.gunLog[i] - 1,
			       	Asc2Hex(lpDispenser_data->fp_ln_data[i][pump.gunLog[i] - 1].Log_Noz_Vol_Total, 7));
		RunLog("Node[%d]FP[%d], Init.fp_m_data[%d].Meter_Total[%s] ",
					node, i + 1, pr_no, Asc2Hex(meter_total, 7));
				
#endif

	       // Amount_Total
		amount_total[0] = 0x0A;
		memcpy(&amount_total[1], &pump.cmd[9], 6);
		memcpy((lpDispenser_data->fp_ln_data[i][pump.gunLog[i] - 1]).Log_Noz_Amount_Total, amount_total, 7);
		RunLog("Node[%d]FP[%d] Init.fp_ln_data[%d][%d].Amount_Total: %s ",
			node, i + 1, i,pump.gunLog[i] - 1,
			Asc2Hex(lpDispenser_data->fp_ln_data[i][pump.gunLog[i] - 1].Log_Noz_Amount_Total, 7));

	}

	return 0;
}

/*
 *  func: 初始的gunLog, 预置加油指令需要; 不修改 Log_Noz_State
 *  三金加油机一条通道对应一条枪
 */
static int InitGunLog()
{
	int i;
	int ret;

	for (i = 0; i < pump.panelCnt; ++i) {
		pump.gunLog[i] = 1;
	}

	return 0;
}



/*
 * func:  通讯不通时轮询过程中隔一段时间重设一下串口属性
 *        避免因意外的电压不稳导致单片机复位--串口属性设置恢复预设9600,8,n,1
 */
static int ReDoSetPort013()
{
#define RE_DO_FREQUENCY   10         //  10次轮询失败设置一下串口属性

	int static polling_fail_cnt = 0;
	int ret;

	if ((++polling_fail_cnt) >= RE_DO_FREQUENCY) {
		ret = SetPort013();
		if (ret < 0) {
			RunLog("Node %d 重设串口属性失败", node);
		}
		polling_fail_cnt = 0;
	}

	return ret;
}

/*
 * func: 设定主控模式
 */
static int SetCtrlMode(int pos)
{
	int ret;
	int trynum;

	for (trynum = 0; trynum < TRY_TIME; ++trynum) {
		pump.cmd[0] = 0xFC;
		pump.cmd[1] = 0xA9;
		pump.cmd[2] = pump.panelPhy[pos];
		checksum_hongyang(&pump.cmd[1], 2, &pump.cmd[3]);
		pump.cmd[5] = 0xFF;
		pump.cmdlen = 6;
		pump.wantLen = 7;

		Ssleep(INTERVAL); 
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]下发设置主控模式命令失败", node, pos + 1);
			continue;
		}
		
		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]下发设置主控模式命令成功, 取返回信息失败", node, pos + 1);
			continue;
		}

		if (0xFC != pump.cmd[0] || 0xFF != pump.cmd[6]) {
			RunLog("Node[%d]FP[%d]返回数据错误", node, pos + 1);
			continue;
		}

		if (((pump.cmd[4] << 4) | pump.cmd[5]) != 
			checksum_hongyang(&pump.cmd[1], pump.wantLen - 4, NULL)) {
			RunLog("Node[%d]FP[%d]返回数据校验错误", node, pos + 1);
			continue;
		}

		if (0x99 != pump.cmd[3]) {
			RunLog("Node[%d]主控模式设置失败!", node);
			continue;
		} 

		RunLog("Node[%d]主控模式设置成功!", node);
		return 0;
	}

	return -1;
}

/*
 * func: 清除某个FP或者全部FP的历史交易, 令其状态转变为FP_IDLE
 *       pos: 255 表示操作全部FP
 */
static void clr_his_trans(int pos)
{
	int fp_cnt;
	int i;
	int ret;
	__u8 status;

	if (255 == pos) {
		fp_cnt = pump.panelCnt;
		i = 0;
	} else {
		fp_cnt = pos;
		i = pos;
	}

	for (; i < fp_cnt; ++i) {
		do {
			ret = GetPumpStatus(i, &status);
			if (ret == 0 && status == FP_EOT) {
				GetTransData(i);
			} else {
				break;
			}
		} while (1);
	}

}

/*
 * func: 设定串口属性—通讯速率、校验位等
 */
static int SetPort013()
{
	int ret;

	pump.cmd[0] = 0x1C;
	pump.cmd[1] = 0x52;
	pump.cmd[2] = 0x30 | (pump.chnPhy - 1);
	pump.cmd[3] = 0x0C;
	pump.cmd[4] = 0x00;
	pump.cmd[5] = 0x00;
	pump.cmd[6] = 0x00;
	pump.cmdlen = 7;
	pump.wantLen = 0;

	ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			pump.cmdlen, 0, pump.wantLen, TIME_OUT_WRITE_PORT);
	if (ret < 0) {
		return -1;
	}

	Ssleep(INTERVAL);       // 确保使用前串口属性已经设置好

	RunLog("Node[%d]设置串口属性成功", node);

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
 * func: 计算鸿洋命令的校验和(BCC)
 *       return 返回未经拆分的校验和
 *       result 返回拆分后的梁字节校验结果
 */ 
static __u8 checksum_hongyang(const __u8 *cmd, int cmd_len,  __u8 * result)
{
	int i;
	__u8 ret = 0x00;

	for (i = cmd_len - 1; i >= 0; --i) {
		ret += cmd[i];
	}

	ret = ~ret;
	ret++;

	if (NULL != result) {
		result[0] = (ret >> 4);
		result[1] = (ret & 0x0F);
	}

	return ret;
}

/*
 *  func: 检查所有FP的状态, 看是否允许进入正常流程
 *        若有已授权/正在加油/加油停止等状态则等待
 *  return:  0 - 不允许; 1 - 允许
 */
static int is_all_fp_ready()
{
	int i;
	int idx;
	int ret;
	__u8 status;

	i = 30;
	do {
		for (idx = 0; idx < pump.panelCnt; ++idx) {
			ret = GetPumpStatus(idx, &status);
			if ((ret == 0 && (FP_AUTHORISED <= status && status <= FP_STOP)) || (ret < 0)) {
				if (FP_AUTHORISED == status) {
					DoStop(idx);  // 取消预授权
				}
				break;         // 条件不符合, 退出重新等待
			} else {
				continue;      // 检查下一个FP
			}
		}

		if (idx >= pump.panelCnt) {
			break;                 // 全部满足条件, 则退出循环进入正常流程
		}

		i--;
		sleep(1);
	} while (i > 0);


	if (i <= 0) {
		return 0;
	} else {
		return 1;
	}
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
			node,pos+1,lnz+1, pump.cmd[3], pump.cmd[4],
		       	lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2], 
		       	lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3]);

		if ((pump.cmd[3] != lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2]) ||
			(pump.cmd[4] != lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3])) {
			return -2;
		}
	}
	
	ChangeFP_State(node, 0x21 + pos, FP_State_Closed, NULL);
	RunLog("Node[%d]FP[%d]单价已变更正确, 切换状态为Closed", node, pos + 1);

	return 0;
}


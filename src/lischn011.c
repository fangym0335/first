/*
 * Filename: lischn011.c
 *
 * Description: 加油机协议转换模块—长空( Uninfo协议,  2Line CL,9600,e,8,1
 *
 * 2008.04 by Yongsheng Li <Liys@css-intelligent.com>
 * 
 *
 *  History:   
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

#define GUN_EXTEND_LEN  9

// FP State
#define PUMP_ERROR	0x00 //错误
#define PUMP_OFF        0x01 //空闲
#define PUMP_CALL	0x02 //等待授权, 即抬枪
#define PUMP_AUTH	0x03 //授权未加油
#define PUMP_BUSY	0x04 //加油中
#define PUMP_STOP	0x05 //暂停加油
#define PUMP_EOT	0x06 //交易结束


#define TRY_TIME	5
#define INTERVAL	80000    // 80ms命令发送间隔, 数值视具体油机而定

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
static int GetPrice(int pos);
static int ChangePrice();
static int GetGunTotal(int pos, __u8 gunPhy, __u8 *vol_total, __u8 *amount_total);
static int GetTransData(int pos);
static int GetPumpTotal(int pos);
static int GetPumpStatus(int pos, __u8 *status);
static int GetPumpOnline(int pos, __u8 *status); static int InitPumpTotal();
static int InitCurrentPrice();
static int InitGunLog();
static int GetCurrentLNZ(int pos);
static int keep_in_touch_xs(int x_seconds);
static int SetPort011();
static int ReDoSetPort011();
static int CheckPriceErr(int pos);

static PUMP pump;
static int node;
static int IFSFpos;
static DISPENSER_DATA *lpDispenser_data;
static NODE_CHANNEL *lpNode_channel;
static int fp_state[MAX_FP_CNT];     // 0 - IDLE, 1 - CALLING , 2 - DoStarted 成功 3 - FUELLING
static int dostop_flag[MAX_FP_CNT];  // 1 - 成功执行过 Terminate_FP
static int first_time = 0;           // 标记是否第一次DoBusy , 1 - first_time

int ListenChn011(int chnLog)
{
	long	indtyp;
	int	ret;
	int	fd;
	int	j;
	int	i;
	__u8    status;
	
	sleep(3);   //防止此进程退出,而主进程没有启动完毕. 每个lischnxxx都需要
	RunLog("Listen Channel  011 ,logic channel[%d]", chnLog);
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
			}
				
			break;
		}
	}
	pump.indtyp = chnLog;
	bzero(&pump.newPrice, FP_PR_NB_CNT * 4);
	
#if DEBUG
//	show_dispenser(node); 
//	show_pump(&pump);
#endif

	// ============= Part.2 打开 & 设置串口属性 -- 波特率/校验位等 ===================

	strcpy(pump.cmd, alltty[pump.port - 1]);
	pump.fd = OpenSerial(pump.cmd);
	if (pump.fd < 0) {
		RunLog("节点[%d]打开串口[%s]失败", node, pump.cmd);
		return -1;
	}

	ret = SetPort011();
	if (ret < 0) {
		RunLog("节点[%d]设置串口属性失败", node);
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
		UserLog("节点 %d 可能不在线,请检查!ppid:[%d]", node,getppid());
		
		sleep(1);
	}
	
#if 0
	// "打通任督二脉"
	for (i = 0; i < pump.panelCnt; ++i) {
		ret = GetPumpOnline(i, &status);
	}
#endif

	// ============= Part.4 初始化与加油机当前状态相关的数据库相关项 =======================
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

     //+++++++++++++++  一切就绪, 上线+++++++++++++++++++++++//
       
//	gpIfsf_shm->node_online[IFSFpos] = node;  
	ChangeNode_Status(node, NODE_ONLINE);
	UserLog("节点 %d 在线,状态[%02x]!", node, status);
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
	unsigned int cnt;
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
					ret = 0;
			} else {
				ret = DoStop(pos);
			}
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
						if (SetPort011() >= 0) {
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
				RunLog("====== Node %d 离线! =============================", node);

				// Step.3
				for (j = 0; j < pump.panelCnt; ++j) {
					lpDispenser_data->fp_data[j].FP_State = FP_State_Inoperative;
					lpDispenser_data->fp_data[j].Log_Noz_State = 0;
				}
			}

			// 重设串口属性
			//ReDoSetPort011();
			continue;
		} else {
			if (gpIfsf_shm->node_online[IFSFpos] == 0) {
				if ((--fail_cnt) > 0) { 
					fail_cnt = 1;   // 两次OK即判断油机联线
					continue;
				} else if (time(NULL) < pump.time_offline) {
					RunLog("================ waiting for 30s ================");
					continue;
				}

				// fail_cnt复位
				fail_cnt = 0;

				// Step.1
				ret = InitPumpTotal();   // 更新泵码到数据库
				if (ret < 0) {
					RunLog("节点[%d]更新泵码到数据库失败", node);
					fail_cnt = 1;
					continue;
				}

				// Step.2
				for (j = 0; j < pump.panelCnt; ++j) {
					lpDispenser_data->fp_data[j].FP_State = FP_State_Closed;
				}

//				gpIfsf_shm->node_online[IFSFpos] = node;
				ChangeNode_Status(node, NODE_ONLINE);
				RunLog("====== Node %d 恢复联线! =========================", node);
				keep_in_touch_xs(12);   // 确保上位机收到心跳后再上传状态
				// Step.3                 // 主动上传当前FP状态
				for (j = 0; j < pump.panelCnt; ++j) {
					ChangeFP_State(node, j + 0x21, FP_State_Closed, NULL);
				}
				continue;
			}
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

		switch (status) {
		case PUMP_ERROR:				// Do nothing
			ret = ChangeFP_State(node, i + 0x21, FP_State_Inoperative, NULL);
			break;
		case PUMP_OFF:					// 挂枪、空闲
			first_time = 0;
			dostop_flag[i] = 0;
			if (fp_state[i] >= 2) {                    // Note.对没有采到挂枪信号(EOT)的补充处理
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

			lpDispenser_data->fp_data[i].Log_Noz_State = 0x00;
			fp_state[i] = 0;

			if (((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State) &&
				(__u8)FP_State_Authorised != lpDispenser_data->fp_data[i].FP_State) {
				// 新长空在预置成功之后状态并不会转换为已授权为加油
				// 所以仍要加是否为已授权的判断
				ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
			}

			EO_OFFLINE_TR(node, i);
			if (gpIfsf_shm->fp_offline_flag[node - 1][i] != 0) {
				upload_offline_tr(node, NULL);
			}

			// 当FP状态为Idle的时候才下发变价
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
		case PUMP_CALL:					// 抬枪
			if (dostop_flag[i] != 0) {
				break;
			}
			if ((__u8)FP_State_Calling > lpDispenser_data->fp_data[i].FP_State) {
				ret = DoCall(i);
				fp_state[i] = 1;

				DoStarted(i);
			}

			break;
		case PUMP_AUTH:
			if ((__u8)FP_State_Authorised != lpDispenser_data->fp_data[i].FP_State) {
				ret = ChangeFP_State(node, i + 0x21, FP_State_Authorised, NULL);
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
		case PUMP_EOT:					// 加油结束
			if (fp_state[i] == 0) {
				break;
			}

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
#define  PRE_VOL  1
#define  PRE_AMO  2
	int ret;
	__u8 pre_volume[3] = {0};
	__u8 value[8];
	__u8 lrc;
	__u8 preset_type;
	int trycnt;

	if (pump.cmd[0] == 0x3D) { 
		preset_type = PRE_VOL;
	} else {
		preset_type = PRE_AMO;
	}
	
	if (preset_type == PRE_AMO) {
		memcpy(pre_volume, &lpDispenser_data->fp_data[pos].Remote_Amount_Prepay[2], 5);
	} else {
		memcpy(pre_volume, &lpDispenser_data->fp_data[pos].Remote_Volume_Preset[2], 5);
	}

	value[0] = 0xe0|(pre_volume[2]&0x0f);
	value[1] = 0xe0|(pre_volume[2]>>4);
	value[2] = 0xe0|(pre_volume[1]&0x0f);
	value[3] = 0xe0|(pre_volume[1]>>4);
	value[4] = 0xe0|(pre_volume[0]&0x0f);
	value[5] = 0xe0|(pre_volume[0]>>4);
	value[6] = 0xe0;
	value[7] = 0xe0;
	
	for (trycnt = 0; trycnt < TRY_TIME; trycnt++) {		
		pump.cmd[0] = 0xd0 | pump.panelPhy[pos];
		pump.cmd[1] = 0xff;

		// e1 e1  = 17  从FD开始到FC前一个字节结束
		pump.cmd[2] = 0xe1;
		pump.cmd[3] = 0xe1;
		
		pump.cmd[4] = 0xfd;

		//002	――	送預置量
		pump.cmd[5] = 0xe0;
		pump.cmd[6] = 0xe0;
		pump.cmd[7] = 0xe2;

		//油品号（多枪机的枪号）
		pump.cmd[8] = 0xf5;		
		pump.cmd[9] = 0xe0 | (pump.gunLog[pos] - 1);
	

		//預置量（F1油量预置，F2金额预置）
		if (preset_type == PRE_VOL)	{
			pump.cmd[10] = 0xf1;
		} else if (preset_type == PRE_AMO) {
			pump.cmd[10] = 0xf2;
		}	


		//预置数据（BCD）
		memcpy(pump.cmd + 11, value, 8);

	/*	RunLog("预制加油数据: %02x%02x%02x%02x%02x%02x%02x%02x",amount[7],amount[6],
			       amount[5],amount[4],amount[3],amount[2],amount[1],amount[0]); */

		//2位小数
		pump.cmd[19] = 0xe2;

		//单价使用:F3 正常结算单价； F4:优惠价格
		pump.cmd[20] = 0xf3;

		pump.cmd[21] = 0xfc;
		pump.cmd[22] = 0xe0 | LRC_CK(pump.cmd + 1, 21);
		pump.cmd[23] = 0xf0;
		pump.cmdlen = 24;
		pump.wantLen = 12;

//		RunLog("doration: %s", Asc2Hex(pump.cmd, pump.cmdlen));
		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("写预置加油命令到Node %d FP %d 失败", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd)-1;

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);

		if  (!( pump.cmd[0] == (0xd0|pump.panelPhy[pos]) && (pump.cmd[8] == 0xe4 || pump.cmd[8] == 0xe0))){
			RunLog("写预置加油命令到Node %d FP %d 成功, 取返回信息失败", node, pos + 1);
			continue;
		}							
		RunLog("写预置加油命令到Node %d FP %d 成功", node, pos + 1);
		
			
		return 0; 
	}

	return -1;
}


static int GetPrice(int pos)
{
	int ret;
	int trycnt;
	__u8 lrc;
	   
	for (trycnt = 0; trycnt <TRY_TIME ; trycnt++) {
		pump.cmd[0] = 0xB0|pump.panelPhy[pos];
		pump.cmdlen = 1;
		pump.wantLen = pump.gunCnt[pos] * 40 + 4;
		
		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]下发读取单价失败!", node, pos + 1);
			gpGunShm->addInf[pump.chnLog - 1][0] = 0x00;
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		//变长特征.如果该变量不为0x00,则tty程序在取得足够的长度时,需要用该特征码判断是否还有后续数据
		gpGunShm->addInf[pump.chnLog-1][0] = 0x01;
		//判断特征码的程序在spec.c中
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x00;	//取完数据后, 复位标记
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]下发读取单价命令成功, 取返回信息失败", node, pos + 1);
			continue;
		}

		if (pump.cmd[pump.cmdlen - 1] != 0xf0) {
			RunLog("接收数据不完整");
			continue;
		}

		//lrc校验失败
		lrc = LRC(pump.cmd, pump.cmdlen - 2);
		if ((lrc & 0x0f) != (pump.cmd[pump.cmdlen - 2] & 0x0f)) {
			RunLog("节点[%d]面板[%d]单价数据校验失败");
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
	int i, j;
	int ret, fm_idx;
	int trycnt;
	__u8 price[6] = {0};
	__u8 price_tmp[2] = {0};
	__u8 pr_no;
	__u8 lnz, status;
	__u8 err_code;
	__u8 err_code_tmp;
	
/*
 * 遍历新单价数组newPrice[FP_PR_NB_CNT], 如果有油品需要变价--数组值不为零,
 * 则遍历所有FP, 如果油品匹配就下发变价命令
 */
       
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

					//price is little endian mode, so 1 to 0
					price[0] = 0xe0 | (pump.newPrice[pr_no][3] & 0x0f);
					price[1] = 0xe0 | (pump.newPrice[pr_no][3] >> 4);
					price[2] = 0xe0 | (pump.newPrice[pr_no][2] & 0x0f);
					price[3] = 0xe0 | (pump.newPrice[pr_no][2] >> 4);  
					price[4] = 0xe0;
					price[5] = 0xe0;

					for (trycnt = 0; trycnt < TRY_TIME; trycnt++) {
						// Step.1.1 发变价命令
						pump.cmd[0] = 0xD0|pump.panelPhy[i];
						pump.cmd[1] = 0xff;

						//e1 e6 = 22  从FD开始到FC前一个字节结束的长度
						pump.cmd[2] = 0xe1;
						pump.cmd[3] = 0xe6;

						//fd 数据交换命令				  
						pump.cmd[4] = 0xfd;

						//e0 e0 e1 = 001 更改单价
						pump.cmd[5] = 0xe0;
						pump.cmd[6] = 0xe0;
						pump.cmd[7] = 0xe1;

						//f5  油品数据				   
						pump.cmd[8] = 0xf5;						
						pump.cmd[9] = 0xe0;  // ******************** 是否应为 0xe0 | j ?

						//f3  正常结算单价
						pump.cmd[10] = 0xf3;
						memcpy(pump.cmd + 11,price,6);
						pump.cmd[17] = 0xe2;

						//f4 优惠单价
						pump.cmd[18] = 0xf4;
						memcpy(pump.cmd + 19,price,6);
						pump.cmd[25] = 0xe2;

						//fc 校验
						pump.cmd[26] = 0xfc;

						//ex  0xFF - 0xFC 之间的数据算校验
						pump.cmd[27] = 0xe0|LRC_CK(pump.cmd+1,26);

						//f0   数据结束
						pump.cmd[28] = 0xf0;
							
						pump.cmdlen = 29;
						pump.wantLen = 12;

						Ssleep(INTERVAL);
						ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd, 
								pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
						if (ret < 0) {
							RunLog("节点[%d]面板[%d]下发变价命令失败", node, i + 1);
							continue;
						}

						RunLog("Node[%d].FP[%d].ChangePrice_CMD: %s",
							node, i + 1, Asc2Hex(pump.cmd, pump.cmdlen));

						memset(pump.cmd, 0, sizeof(pump.cmd));
						pump.cmdlen = sizeof(pump.cmd) - 1;

						ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
						 if (ret < 0) {
							RunLog("发变价命令到Node[%d].FP[%d]成功, 取返回信息失败", node, i + 1);
							continue;
						}
										 
						if (pump.cmd[0] != (0xD0 | pump.panelPhy[i])) {
							RunLog("Node[%d].FP[%d]发变价命令成功, 取返回信息失败", node, i + 1);
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
							continue;
						}

						switch (pump.cmd[8] & 0x0F) {
						case 0x01:
							RunLog("Node[%d].FP[%d]变价校验或数据长度错", node, i + 1);
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
							continue;
						case 0x02:
							RunLog("Node[%d].FP[%d]变价数据项错误", node, i + 1);
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
							continue;
						case 0x03:
							RunLog("Node[%d].FP[%d]此刻不允许执行变价", node, i + 1);
							err_code_tmp = 0x37;
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
							continue;
						case 0x04:
							RunLog("Node[%d].FP[%d]变价执行中", node, i + 1);
							sleep(1);
							break;
						case 0x00:
							break;
						default:
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
							continue;
						}

						ret = GetPrice(i);
						if( ret < 0 ){
							continue;
						}
						price_tmp[0] = (pump.cmd[29] << 4) | (pump.cmd[28] & 0x0f);
						price_tmp[1] = (pump.cmd[27] << 4) | (pump.cmd[26] & 0x0f);

						if(memcmp( &price_tmp[0] , &pump.newPrice[pr_no][2] , 2) != 0 ){
							RunLog("节点[%d]面板[%d]变价失败 %d 次",node,i+1, trycnt+1);
							err_code_tmp = 0x37;
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
							continue;
						}

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
                     //待全部面板都执行变价后再清零
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
	__u8 price[5];
	__u8 volume[5];
	__u8 amount[5];
	__u8 pre_total[7];
	__u8 total[7];
	__u8 vol_total[GUN_EXTEND_LEN];
	__u8 amount_total[GUN_EXTEND_LEN];

	RunLog("节点[%d]面板[%d]处理新交易", node, pos + 1);

	err_cnt = 0;
	do {
		ret = GetGunTotal(pos, pump.gunLog[pos], vol_total, amount_total);	// 取油枪泵码
		if (ret < 0) {
			err_cnt++;
			RunLog("Node[%d].FP[%d]读取总累失败 <ERR>", node, pos + 1);
		}
	} while (ret < 0 && err_cnt <= 5);
	
	if (ret < 0) {
		return -1;
	}


	pr_no = (lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).PR_Id - 1;  // 0 - 7	
	if ((pr_no < 0) || (pr_no > 7)) {
		RunLog("DoEot PR_Id error, PR_Id[%d] <ERR>", pr_no + 1);
	}
	
	memcpy(pre_total, (lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Vol_Total, 7);

	//vol_total
	total[0] = 0x0a; 
	total[1] = 0x00;	
	total[2] = 0x00 | (vol_total[8] << 4);
	total[3] = (vol_total[7] << 4) | (vol_total[6] & 0x0f);
	total[4] = (vol_total[5] << 4) | (vol_total[4] & 0x0f);
	total[5] = (vol_total[3] << 4) | (vol_total[2] & 0x0f);
	total[6] = (vol_total[1] << 4) | (vol_total[0] & 0x0f);

	ret = memcmp(total, pre_total, sizeof(total));
	if (ret != 0) {
//		RunLog("----- 原泵码= [%s] ",Asc2Hex(pre_total,7));
//		RunLog("----- 新泵码= [%s] ",Asc2Hex(total,7));
		err_cnt= 0;
		do {
			ret = GetTransData(pos);	// 取最后一笔加油记录
			if (ret != 0) {
				RunLog("Node[%d].FP[%d]读取交易记录失败 <ERR>", node, pos + 1);
				err_cnt++;
			}
		} while (ret < 0 && err_cnt <=5);

		if (ret < 0) {
			return -1;
		}

		volume[0] = 0x06;               
		volume[1] = (pump.cmd[34] << 4) | (pump.cmd[33] & 0x0f);
		volume[2] = (pump.cmd[32] << 4) | (pump.cmd[31] & 0x0f);
		volume[3] = (pump.cmd[30] << 4) | (pump.cmd[29] & 0x0f);
		volume[4] = (pump.cmd[28] << 4) | (pump.cmd[27] & 0x0f);
		
		memcpy(lpDispenser_data->fp_data[pos].Current_Volume, volume, 5);

		amount[0] = 0x06;
		amount[1] = (pump.cmd[44] << 4) | (pump.cmd[43] & 0x0f);
		amount[2] = (pump.cmd[42] << 4) | (pump.cmd[41] & 0x0f);
		amount[3] = (pump.cmd[40] << 4) | (pump.cmd[39] & 0x0f);
		amount[4] = (pump.cmd[38] << 4) | (pump.cmd[37] & 0x0f);
		
		memcpy(lpDispenser_data->fp_data[pos].Current_Amount, amount, 5);

		price[0] = 0x04;		
		price[1] = (pump.cmd[24] << 4) | (pump.cmd[23] & 0x0f);
		price[2] = (pump.cmd[22] << 4) | (pump.cmd[21] & 0x0f);
		price[3] = (pump.cmd[20] << 4) | (pump.cmd[19] & 0x0f);
		
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
		total[2] = 0x00 | (amount_total[8] << 4);
		total[3] = (amount_total[7] << 4) | (amount_total[6] & 0x0f);
		total[4] = (amount_total[5] << 4) | (amount_total[4] & 0x0f);
		total[5] = (amount_total[3] << 4) | (amount_total[2] & 0x0f);
		total[6] = (amount_total[1] << 4) | (amount_total[0] & 0x0f);
		
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
	int ret;
	int trycnt;
	__u8 lrc;
	   
	for (trycnt = 0; trycnt <TRY_TIME ; trycnt++) {
		pump.cmd[0] = 0xB0|pump.panelPhy[pos];
		pump.cmdlen = 1;
		pump.wantLen = pump.gunCnt[pos] * 40 + 4;
		
		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]下发读取泵码命令失败", node, pos + 1);
			gpGunShm->addInf[pump.chnLog - 1][0] = 0x00;
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		//变长特征.如果该变量不为0x00,则tty程序在取得足够的长度时,需要用该特征码判断是否还有后续数据
		gpGunShm->addInf[pump.chnLog-1][0] = 0x01;
		//判断特征码的程序在spec.c中
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x00;	//取完数据后, 复位标记
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]下发读取泵码命令成功, 取返回信息失败", node, pos + 1);
			continue;
		}

		if (pump.cmd[pump.cmdlen - 1] != 0xf0) {
			RunLog("接收数据不完整");
			continue;
		}

		//lrc校验失败
		lrc = LRC(pump.cmd, pump.cmdlen - 2);
		if ((lrc & 0x0f) != (pump.cmd[pump.cmdlen - 2] & 0x0f)) {
			RunLog("节点[%d]面板[%d]泵码数据校验失败");
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
		RunLog("取FP %d 中枪的泵码失败", pos + 1);
		return -1;
	}

	num = (pump.cmdlen - 4) / 30;	// 每条记录30字节,4个字节为其他包头,包尾
	p = pump.cmd + 1;

	for (i = 0; i < num; i++) {
		if ((p[1] & 0x0F) == gunLog - 1) {
			memcpy(vol_total, p + 3, GUN_EXTEND_LEN);
			memcpy(amount_total, p + 14, GUN_EXTEND_LEN);
			break;
		}
		p = p + 40;
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
	int trycnt;
	__u8 lrc;
	__u8 status;

	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) { 
		pump.cmd[0] = 0xc0 | pump.panelPhy[pos];
		pump.cmdlen = 1;
		pump.wantLen = 73;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]下发读取交易记录命令失败", node , pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]下发读取交易记录命令成功, 取返回信息失败", node , pos + 1);
			continue;
		}

		if (pump.cmd[pump.cmdlen - 1] != 0xf0) {
			RunLog("节点[%d]面板[%d]返回信息结束符错误", node, pos + 1);
			continue;
		}

		/*lrc校验失败*/
		lrc = LRC(pump.cmd, pump.cmdlen - 2);
		if ((lrc & 0x0f) != (pump.cmd[pump.cmdlen - 2] & 0x0f)) {
			RunLog("节点[%d]面板[%d]交易数据校验失败", node , pos + 1);
			continue;
		}
		
		//验证
		if ((pump.cmd[2] & 0x0f) != pump.panelPhy[pos] ){
			RunLog("交易数据面板号错误");
			continue;
		} 

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
	__u8 status;

	RunLog("Node %d FP %d DoStop", node, pos + 1);

	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {
		if (pos == 255) {
			pump.cmd[0] = 0xfe;
		} else {
			pump.cmd[0] = 0x70 | pump.panelPhy[pos];
		}
		pump.cmdlen = 1;
		pump.wantLen = 0;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]下发暂停加油命令失败", node, pos + 1);
			continue;
		}

		Ssleep(INTERVAL);
		ret = GetPumpStatus(pos, &status);
		if (ret < 0) {
			continue;
		}

		if (status == PUMP_STOP || status == PUMP_OFF) {
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
	int ret;
	int trycnt;
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
		for (trycnt = 0; trycnt < TRY_TIME; trycnt++) {
			pump.cmd[0] = 0x90 | pump.panelPhy[i];
			pump.cmdlen = 1;
			pump.wantLen = 0;

			Ssleep(INTERVAL);
			ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
			if (ret < 0) {
				RunLog("节点[%d]面板[%d]下发授权命令失败", node, pos + 1);
				continue;
			}

			Ssleep(INTERVAL);
			ret = GetPumpStatus(pos, &status);
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

		if (trycnt == TRY_TIME) {
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
	int ret;
	__u8 amount[5] = {0};
	__u8 volume[5] = {0};
	const __u8 arr_zero[3] = {0};
	int trycnt;

	if (first_time == 0) {
		// 第一次取实时数据容易失败, 时间长一点
		Ssleep(INTERVAL * 5);
		first_time = 1;
	}

	for( trycnt = 0; trycnt < 2; trycnt++ ) {
		Ssleep(INTERVAL);
		pump.cmd[0] = 0x80 | pump.panelPhy[pos];
		pump.cmdlen = 1;
		pump.wantLen = 25;

		ret = WriteToPort( pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT );
		if( ret < 0 ) {
			RunLog( "发 取实时加油数据命令 到通道 %d 失败", pump.chnLog);
			continue;
		}

		memset( pump.cmd, 0, sizeof(pump.cmd) );
		pump.cmdlen = sizeof(pump.cmd) - 1;

		ret = GetReturnMsg( GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen );
		if( ret < 0 ) {
			RunLog( "发 取实时加油数据命令 到通道 %d 成功, 取返回信息失败", pump.chnLog);
			continue;
		}

		volume[0] = 0x06;
		volume[1] = (pump.cmd[16] << 4) | (pump.cmd[15] & 0x0F);
		volume[2] = (pump.cmd[14] << 4) | (pump.cmd[13] & 0x0F);
		volume[3] = (pump.cmd[12] << 4) | (pump.cmd[11] & 0x0F);
		volume[4] = (pump.cmd[10] << 4) | (pump.cmd[9] & 0x0F);

		// 只有当加油量不为零了才能将FP状态改变为Fuelling
		if (fp_state[pos] != 3) {
			if (memcmp(&volume[2], arr_zero, 3) == 0) {
				return 0;
			}
			fp_state[pos] = 3;
			return 0;
		}

		amount[0] = 0x06;
		amount[1] = (pump.cmd[7] << 4) | (pump.cmd[6] & 0x0F);
		amount[2] = (pump.cmd[5] << 4) | (pump.cmd[4] & 0x0F);
		amount[3] = (pump.cmd[3] << 4) | (pump.cmd[2] & 0x0F);
		amount[4] = (pump.cmd[1] << 4) | (pump.cmd[0] & 0x0F);

		if ((__u8)FP_State_Fuelling == lpDispenser_data->fp_data[pos].FP_State) {
			ret = ChangeFP_Run_Trans(node, 0x21 + pos, amount,  volume);
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
	int trycnt;
	__u8 ln_status;

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
			RunLog("Node %d FP %d 自动授权失败", node, pos + 1);
		}
		ret = ChangeFP_State(node, pos + 0x21, FP_State_Started, NULL);
		RunLog("Node %d FP %d 自动授权成功", node, pos + 1);
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

	for (i = 0; i < 2; ++i) {
		pump.cmd[0] = 0xA0|pump.panelPhy[pos];
		pump.cmdlen = 1;
		pump.wantLen = 1;

		Ssleep(INTERVAL); 
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]读取FP状态失败", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]读取FP状态成功, 取返回信息失败", node, pos + 1);
			continue;
		} 

		if ((pump.cmd[0] & 0x0f) != pump.panelPhy[pos]) {
			RunLog("Message Error.FP_Id not match");
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
		pump.cmd[0] = 0xA0|pump.panelPhy[pos];
		pump.cmdlen = 1;
		pump.wantLen = 1;

		Ssleep(INTERVAL); 
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]读取FP状态失败", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("节点[%d]面板[%d]读取FP状态成功, 取返回信息失败", node, pos + 1);
			continue;
		} 

		if ((pump.cmd[0] & 0x0f) != pump.panelPhy[pos]) {
			RunLog("节点[%d]面板[%d]读取FP状态成功, 取返回信息失败", node, pos + 1);
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
 */
static int GetCurrentLNZ(int pos)
{
	  pump.gunLog[pos] = 1;	// 当前抬起的油枪	
	  RunLog("Node %d FP %d 的 %d 号枪抬起", node, pos + 1, pump.gunLog[pos]);
	  lpDispenser_data->fp_data[pos].Log_Noz_State = 1;
	  return 0;
}

/*
 * func: 初始的pump.gunLog, 预置加油指令需要; 不修改 Log_Noz_State
 */
static int InitGunLog()
{
	int i;

	for (i = 0; i < pump.panelCnt; ++i) {
	       pump.gunLog[i] = 1;
	}

	return 0;
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
	int i, j;
	int  num;
	int pr_no;
	__u8 *p;
	__u8 status;
	__u8 lnz;
	__u8 meter_total[7] = {0};
	__u8 extend[GUN_EXTEND_LEN];

	for (i = 0; i < pump.panelCnt; i++) {
		ret = GetPumpStatus(i, &status);
		if (ret < 0) {
			return -1;
		}

		if (status != PUMP_OFF && status != PUMP_CALL &&
			status != PUMP_STOP && status != PUMP_EOT ) {
			RunLog("节点[%d]面板[%d]当前状态不能查询泵码(%02x)", node, i + 1, status);
			return -1;
		}


		Ssleep(INTERVAL);
		ret = GetPumpTotal(i);
		if (ret < 0) {
			RunLog("取 FP%d 的泵码失败", i);
			return -1;
		}

		num = (pump.cmdlen-4)/40; /*每条记录40字节,4个字节为其他包头,包尾*/
		p = pump.cmd + 1;

		for (j = 0; j < num; j++) {
			bzero(extend,sizeof(extend));
			memcpy(extend, p + 3, GUN_EXTEND_LEN);
			lnz = p[1] & 0x0F;
			pr_no = (lpDispenser_data->fp_ln_data[i][lnz]).PR_Id - 1;  // 0-7

			RunLog("lpDispenser_data->fp_ln_data[%d][%d].PR_Id = %d",
					i, lnz, (lpDispenser_data->fp_ln_data[i][lnz]).PR_Id);
			
			RunLog("p[1]: %02x,j: %d, lnz: %d, pr_no:%d",p[1], j, lnz, pr_no);
			if ((pr_no < 0) || (pr_no > 7)) {
				RunLog("InitPumpTotal,非法的油品地址FP %d, LNZ %d, PR_Id %d", i, lnz + 1, pr_no + 1);
				p = p + 40;
				continue;
			}
			meter_total[0] = 0x0A;
			meter_total[1] = 0x00;
			meter_total[2]= 0x00|( extend[8]<<4);
			meter_total[3]=( extend[7]<<4)|( extend[6]&0x0f);
			meter_total[4]=( extend[5]<<4)|( extend[4]&0x0f);
			meter_total[5]=( extend[3]<<4)|( extend[2]&0x0f);
			meter_total[6]=( extend[1]<<4)|( extend[0]&0x0f);

			memcpy(lpDispenser_data->fp_m_data[pr_no].Meter_Total, meter_total, 7);
			memcpy((lpDispenser_data->fp_ln_data[i][pump.gunLog[i] - 1]).Log_Noz_Vol_Total, meter_total, 7);
			
#if DEBUG
			RunLog("Init.fp_ln_data[%d][%d].Log_Noz_Vol_Total: %s ", i, pump.gunLog[i] - 1,
				       	Asc2Hex(lpDispenser_data->fp_ln_data[i][pump.gunLog[i] - 1].Log_Noz_Vol_Total, 7));
			RunLog("Init.fp_m_data[%d].Meter_Total[%s] ", pr_no, Asc2Hex(meter_total, 7));
#endif
			// Amount_Total
			bzero(extend,sizeof(extend));
			memcpy(extend, p + 14, GUN_EXTEND_LEN);
			meter_total[0] = 0x0A;
			meter_total[1] = 0x00;
			meter_total[2]= 0x00|( extend[8]<<4);
			meter_total[3]=( extend[7]<<4)|( extend[6]&0x0f);
			meter_total[4]=( extend[5]<<4)|( extend[4]&0x0f);
			meter_total[5]=( extend[3]<<4)|( extend[2]&0x0f);
			meter_total[6]=( extend[1]<<4)|( extend[0]&0x0f);
			   
			memcpy((lpDispenser_data->fp_ln_data[i][pump.gunLog[i] - 1]).Log_Noz_Amount_Total, meter_total, 7);
			RunLog("Init.fp_ln_data[%d][%d].Amount_Total: %s ", i, pump.gunLog[i] - 1,
				       	Asc2Hex(lpDispenser_data->fp_ln_data[i][pump.gunLog[i] - 1].Log_Noz_Amount_Total, 7));

			p = p + 40;
		}

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
static int SetPort011()
{
	int ret;

//	if (gpGunShm->chnLogSet[pump.chnLog - 1] == 0) {	
	pump.cmd[0] = 0x1C;
	pump.cmd[1] = 0x52;
	pump.cmd[2] = 0x30|(pump.chnPhy-1);
	pump.cmd[3] = 0x03;	//波特率 9600
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
	Ssleep(INTERVAL);
	
//	}

	return 0;
}


/*
 * func:  通讯不通时轮询过程中隔一段时间重设一下串口属性
 *        避免因意外的电压不稳导致单片机复位--串口属性设置恢复预设9600,8,n,1
 */
static int ReDoSetPort011()
{
#define RE_DO_FREQUENCY   10         //  10次轮询失败设置一下串口属性

	int static polling_fail_cnt = 0;
	int ret;

	if ((++polling_fail_cnt) >= RE_DO_FREQUENCY) {
		ret = SetPort011();
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

	ret = GetPrice(pos);
	if (ret < 0) {
		return -1;
	}

	price[0] = (pump.cmd[29] << 4) | (pump.cmd[28] & 0x0f);
	price[1] = (pump.cmd[27] << 4) | (pump.cmd[26] & 0x0f);

	for (lnz = 0; lnz < pump.gunCnt[pos]; lnz++) {
		pr_no = (lpDispenser_data->fp_ln_data[pos][lnz]).PR_Id - 1;  // 0 - 7	
		if ((pr_no < 0) || (pr_no > 7)) {
			RunLog("CheckPrice ERR PR_Id error, FP[%d]LNZ[%d] PR_Id[%d] <ERR>",
								pos + 1, lnz + 1, pr_no + 1);
			return -1;
		}

		fm_idx = lpDispenser_data->fp_data[pos].Default_Fuelling_Mode - 1;
		RunLog("Node %d FP %d LNZ %d Now price is %02x.%02x; DST Price: %02x.%02x",
			node,pos+1,lnz+1,price[0],price[1],
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


/*--------------------------end of lischn011.c-------------------------------*/

/*
 * Filename: lischnxxx.c
 *
 * Description: 加油机协议转换模块 - XXXX, CL, 9600,E,8,1 
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
static int ChangePrice();
static int GetTransData(int pos);
static int GetPumpStatus(int pos, __u8 *status);
static int GetPumpOnline(int pos, __u8 *status);
static int GetPrice(int pos);
static int InitPumpTotal();
static int InitCurrentPrice();
static inline int GetCurrentLNZ(int pos);
static int SetCtrlMode(int pos, __u8 mode);
static int SetPortxxx();

static PUMP pump;
static int node;
static int IFSFpos;
static DISPENSER_DATA *lpDispenser_data;
static NODE_CHANNEL *lpNode_channel;
static int fp_state[MAX_FP_CNT];   // 0 - IDLE, 1 - CALLING , 2 - 已完成DoStarted, 3 - FUELLING

int ListenChnxxx(int chnLog)
{
	long	indtyp;
	int	ret;
	int	fd;
	int	j;
	int	i;
	__u8 status;
	
	sleep(5);   //防止此进程退出,而主进程没有启动完毕. 每个lischnxxx都需要
	RunLog("Listen Chenel  xxx ,logic chennel[%d]", chnLog);
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

	ret = SetPortxxx();
	if (ret < 0) {
		RunLog("设置串口属性失败");
		return -1;
	}
	
	// ============== Part.3 判断加油机是否在线 ====================================
	// Check wether dispenser is online, if it's not online, wait  here.
	while (1) {
		static int n = 0;
		ret = GetPumpOnline(0, &status);
		if (ret == 0) {
			break;
		}

		gpIfsf_shm->node_online[IFSFpos] = 0; //不在线,不产生心跳.
		UserLog("Node %d 不在线", node);
		
		if (n < 60) {
			n++;
			sleep(n);
		} else {
			sleep(60);;
		}
	}
	
	// ============= Part.4 初始化与加油机当前状态相关的数据库相关项 =======================
	
	for (i = 0;  i < pump.panelCnt; ++i) {
		if (SetCtrlMode(i, 0xAA) < 0) {   // 设置为开放模式
			RunLog("Node[%d]FP[%d]设置开放模式失败", node, i + 1);
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

	gpIfsf_shm->node_online[IFSFpos] = node;  // 一切就绪, 上线
	RunLog("节点 %d 在线,状态[%02x]!", node, status);
	sleep(10);     // 确保上位机收到心跳后再上传状态
	

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
			if (ret < 0) {
				DoPolling();
			} else {
				ret = DoCmd();
				if (ret != 0) {
					RunLog("处理后台命令失败");
				}
			}
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
			ret = DoStop(pos);
			if (ret == 0) {
				ret = ChangeFP_State(node, pos + 0x21, FP_State_Idle, msgbuffer);
			} else {
				SendCmdAck(NACK, msgbuffer);
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
				// 正星特殊处理, 因为正星有Started状态
				// 当油机本身具有该状态时, 状态切换按油机实际情况来做
				ret = SendData2CD(&msgbuffer[2], msgbuffer[1], 5);
				if (ret < 0) {
					RunLog("Send ack message [%s] to CD failed", Asc2Hex(&msgbuffer[2], msgbuffer[1]));
				}
			} else {
				SendCmdAck(NACK, msgbuffer);
			}

			break;
		case 0x3D:      // Preset  Amount or Volume
		case 0x4D:
			ret = HandleStateErr(node, pos, Do_Ration,   // Note. 这里用Do_Ration
				lpDispenser_data->fp_data[pos].FP_State, msgbuffer); 
			if (ret < 0) {
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
	__u8 status;
	static __u8 pre_status[MAX_FP_CNT] = {0};
	static int times = 0;
	static int fail_cnt = 0;  // 记录GetPumpStatus 失败次数, 用于判定离线
	__u8 Tankcmd[3] = {0};
	
	for (i = 0; i < pump.panelCnt; i++) {
		ret = GetPumpStatus(i, &status);
		if (ret < 0) {
			if (gpIfsf_shm->node_online[IFSFpos] == node) {
				if ((++fail_cnt) < MAX_FAIL_CNT) {  // 加油机离线判断
					continue;
				}

				ret = ChangeFP_State(node, i + 0x21, FP_State_Closed, NULL);
				ret = ChangeFP_State(node, i + 0x21, FP_State_Inoperative, NULL);
				if (ret < 0) {
					fail_cnt--;
					continue;
				}
				gpIfsf_shm->node_online[IFSFpos] = 0;
				RunLog("====== Node %d 离线! =============================", node);
			}
		} else {
			if (gpIfsf_shm->node_online[IFSFpos] == 0) {
				if ((--fail_cnt) != 0) { // 排除干扰/双线电流环数据采集Bug造成的影响
					continue;
				}

				ret = InitPumpTotal();    // 更新泵码到数据库
				if (ret < 0) {
					RunLog("更新泵码到数据库失败");
					fail_cnt = 1;     // Note.恢复过程中任何一个步骤失败都要重试
					continue;
				}

				for (j = 0;  j < pump.panelCnt; ++j) {
					if (SetCtrlMode(j, 0xAA) < 0) {   // 设置为FCC控制模式
						RunLog("Node[%d]FP[%d]设置主控模式失败", node, j + 1);
						fail_cnt = 1;
						continue;
					}
				}

				ret = ChangeFP_State(node, i + 0x21, FP_State_Closed, NULL);
				if (ret < 0) {
					fail_cnt = 1;
					continue;
				}

				// 一起都准备好了, 上线
				gpIfsf_shm->node_online[IFSFpos] = node;
				RunLog("====== Node %d 恢复联线! =========================", node);
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

				if (ret < 0) {
					continue;
				}
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
#if 0
					// 不应该这么做, 移除这部分处理
					ret = ChangeFP_State(node, i + 0x21, FP_State_Closed, NULL);
#endif
					if (i >= pump.panelCnt -1)
						times = 0;
					UserLog("节点[%d]的面板[%d]处于Closed状态", node, i + 1);
				}
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
			continue;
		}

		pump.cmdlen = 0;

		switch (status & 0x0F) {
		case FP_IDLE:                                   // 挂枪、空闲
			if (fp_state[i] >= 2) {
				// 若有加过油, 则进行状态改变和交易处理
				// 1. ChangeFP_State to IDLE first

				// Step2. Get & Save TR to TR_BUFFER
			}

			fp_state[i] = 0;
			// 若状态不是IDLE, 则切换为Idle

			// 脱机交易处理
			EO_OFFLINE_TR(node, i);
			if (gpIfsf_shm->fp_offline_flag[node - 1][i] != 0) {
				upload_offline_tr(node, NULL);
			}

			ret = ChangePrice();                    // 查看是否有油品需要变价,有则执行
			break;
		case FP_CALLING:                                // 抬枪
			if ((__u8)FP_State_Calling > lpDispenser_data->fp_data[i].FP_State) {
				ret = DoCall(i);

				ret = DoStarted(i);

				// For ATG Adjust
				if (atoi(TLG_DRV) == 1){	//vr - 350r			
					//向350R 液位仪发送抬抢命令0100 01
					Tankcmd[0] = 0x01;
					Tankcmd[1] = 0x00;
					Tankcmd[2] = VrPanelNo[node-1][i];
					
					SendMsg( TANK_MSG_ID, Tankcmd, 3 ,1, 3 );
				}
			}

			break;

		case FP_FUELLING:                               // 加油
			if ((__u8)FP_State_Fuelling != lpDispenser_data->fp_data[i].FP_State) {
				// Do Something
			}
			DO_INTERVAL(DO_BUSY_INTERVAL, ret = DoBusy(i););
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
static int DoRation(int pos);
{
	int ret;
	int trycnt;
	int ration_type;
	__u8 pre_volume[3] = {0};

	if (pump.cmd[0] == 0x3D) {   // Preset
		ration_type = 0x02;
		memcpy(pre_volume, &lpDispenser_data->fp_data[pos].Remote_Volume_Preset[2], 3);
	} else {
		ration_type = 0x01;
		memcpy(pre_volume, &lpDispenser_data->fp_data[pos].Remote_Amount[2], 3);
	}




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
	
	memcpy(&pump.newPrice[pr_no], &pump.cmd[8], 4);
#ifdef DEBUG
	RunLog("Recv New Price: %02x.%02x", pump.newPrice[pr_no][2], pump.newPrice[pr_no][3]);
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
	__u8 pr_no;
	__u8 lnz;

/*
 * 遍历新单价数组newPrice[FP_PR_NB_CNT], 如果有油品需要变价--数组值不为零,
 * 则遍历所有FP, 如果油品匹配就下发变价命令
 */

//	RunLog(">> exec ChangePrice Now !");
	for (pr_no = 0; pr_no < FP_PR_NB_CNT; pr_no++) {
//		RunLog(">>>1 newPrice[%d]: %02x.%02x", pr_no, pump.newPrice[pr_no][2], pump.newPrice[pr_no][3]);;
		if (pump.newPrice[pr_no][2] != 0x00 || pump.newPrice[pr_no][3] != 0x00) {
			for (i = 0; i < pump.panelCnt; i++) {
				for (j = 0; j < MAX_FP_LN_CNT; j++) { // 不能用gunCnt,1枪可能不用
#if DEBUG
					RunLog(">>>2 lnz: %d, fp_ln_data[%d][j].PR_Id: %d", j + 1,
							lpDispenser_data->fp_ln_data[i][j].PR_Id);
#endif
					if (lpDispenser_data->fp_ln_data[i][j].PR_Id - 1 == pr_no) {
					for (trycnt = 0; trycnt < TRY_TIME; trycnt++) {
						// Step.1 发变价命令

						// Step.2 根据命令返回信息或者重读油机单价确认变价是否成功
						
						// Step.3 写新单价到油品数据库 & 清零单价缓存
#if DEBUG
						RunLog("发送变价命令成功,原单价fp_fm_data[%d][0].Prode_Price: %s", pr_no,
							Asc2Hex(lpDispenser_data->fp_fm_data[pr_no][0].Prod_Price, 4));
#endif
						memcpy(&(lpDispenser_data->fp_fm_data[pr_no][0].Prod_Price),
												&pump.newPrice[pr_no], 4);
#if DEBUG
						RunLog("发送变价命令成功,新单价fp_fm_data[%d][0].Prode_Price: %s", pr_no,
							Asc2Hex(lpDispenser_data->fp_fm_data[pr_no][0].Prod_Price, 4));
#endif
						break;
						
					}

					// Step.4 错误判断, 若出错则写错误数据库/置FP状态为 Inoperative
					if (trycnt >= TRY_TIME) {
						// Write Error Code: 0x13 - Change price Error (Spare)
						ret = ChangeER_DATA(node, 0x21 + i, 0x13, NULL); // Translater已经回复了ACK
						ret = ChangeFP_State(node, 0x21 + i, FP_State_Inoperative, NULL);
					}

					} // end of if.PR_Id match
				//	break;  // 如果加油机能够根据指令信息处理多枪同油品的情况，那这里需要break.
				} // end of for.j < LN_CNT
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
	__u8 total[7];
	__u8 pre_total[7];
	__u8 Tankcmd[12] = {0};

	RunLog("节点[%d]面板[%d]处理新的交易", node, pos + 1);

	err_cnt = 0;
	do {
		ret = GetTransData(pos);
		if (ret < 0) {
			err_cnt++;
			RunLog("节点[%d]面板[%d]读取交易记录失败", node, pos + 1);
		}
	} while (err_cnt <= 5 && ret < 0);

	total[0] = 0x0A;
	total[1] = 0x00;
	memcpy(&total[2], &pump.cmd[15], 5);

	pr_no = (lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).PR_Id - 1;  // 0 - 7	
	if ((pr_no < 0) || (pr_no > 7)) {
		RunLog("DoEot PR_Id error, PR_Id[%d] <ERR>", pr_no + 1);
	}
	
	memcpy(pre_total, (lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Vol_Total, 7);
	

	if (memcmp(pre_total, total, 7) != 0) {
		// Volume
		volume[0] = 0x06;
		volume[1] = 0x00;
		memcpy(lpDispenser_data->fp_data[pos].Current_Volume, volume, 5);

		// Amount
		amount[0] = 0x06;
		amount[1] = 0x00;
		memcpy(lpDispenser_data->fp_data[pos].Current_Amount, amount, 5);

		price[0] = 0x04;
		price[1] = 0x00;
		memcpy(lpDispenser_data->fp_data[pos].Current_Unit_Price, price, 4);

		// 更新 Volume_Total
		memcpy(lpDispenser_data->fp_m_data[pr_no].Meter_Total, total, 7);
		memcpy((lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Vol_Total, total, 7);


		if (atoi(TLG_DRV) == 1) {
			Tankcmd[0] = 0x02;
			Tankcmd[1] = 0x00;
			Tankcmd[2] = VrPanelNo[node - 1][pos];
			Tankcmd[3] = pump.gunLog[pos] - 1;
			memcpy(&Tankcmd[4], &total[2], 5);
			memcpy(&Tankcmd[9], &volume[2], 3);
			SendMsg(TANK_MSG_ID, Tankcmd, 12, 1, 3);
		}


		RunLog("节点[%02d]面板[%d]枪[%d] ## Amount: %02x%02x.%02x Volume: %02x%02x.%02x Price: %02x.%02x Total: %02x%02x%02x.%02x", node, pos + 1, pump.gunLog[pos], amount[2], 
					amount[3], amount[4], volume[2], volume[3], volume[4], price[2],
							price[3], total[3], total[4], total[5], total[6]);

		// 更新 Amount_Total
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
				(lpDispener_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Vol_Total);
			if (ret < 0) {
				err_cnt++;
				RunLog("DoEot.EndTr4Fp失败, ret: %d <ERR>", ret);
			}
		} while (ret < 0 && err_cnt <= 8);

	} else {
		if (atoi(TLG_DRV) == 1) {
			Tankcmd[0] = 0x02;
			Tankcmd[1] = 0x00;
			Tankcmd[2] = VrPanelNo[node - 1][pos];
			Tankcmd[3] = pump.gunLog[pos] - 1;
			memcpy(&Tankcmd[4], &total[2], 5);
			memset(&Tankcmd[9], 0, 3);
			SendMsg(TANK_MSG_ID, Tankcmd, 12, 1, 3);
		}

		ret = EndZeroTr4Fp(node, pos + 0x21);
		if (ret < 0) {
			RunLog("DoEot.EndZeroTr4Fp失败, ret: %d <ERR>", ret);
		}
	}

	return ret;
}


/*
 * func: 获取该FP所有枪的泵码
 */
static int GetPumpTotal(int pos)
{

}

/*
 * func: 读取当次交易记录
 * return: 0 - 成功, 1 - 成功但无交易, -1 - 失败
 */ 
static int GetTransData(int pos)
{

}

/*
 * func: 读取历史交易记录
 */


/*
 * func: Terminate_FP
 */
static int DoStop(int pos)
{

}

/*
 * func: Release_FP
 */
static int DoAuth(int pos)
{

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
		if (memcmp(arr_zero, &pump.cmd[5], 3) == 0) {
			return 0;
		}
		fp_state[pos] = 3;
		return 0;
	}

	//  取得实时加油量 ........
	
	volume[0] = 0x06;
	volume[1] = 0x00;
	memcpy(&volume[2],  &pump.cmd[5], 3);

	amount[0] = 0x06;
	amount[1] = 0x00;
	memcpy(&amount[2],  &pump.cmd[8], 3);

	if ((__u8)FP_State_Fuelling == lpDispenser_data->fp_data[pos].FP_State) {
		ret = ChangeFP_Run_Trans(node, 0x21, amount,  volume); //只有一个面板
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
		RunLog("节点[%d]面板[%d]自动授权成功", node, pos + 1);
		SO_OFFLINE_TR(node, pos);
	}

	return ret;
}

/*
 * func: 读取油机单价
 */



/*
 * func: 读取实时状态 (失败只重试一次)
 *
 */
static int GetPumpStatus(int pos)
{

}


//test wether the dispenser is online, we only use fp 1 to test.
/*
 * func: 判断Dispenser是否在线
 */
static int GetPumpOnline(int pos)
{

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
					lpDispenser_data->fp_fm_data[pr_no][0].Prod_Price, 4);
#ifdef DEBUG
		RunLog("Init.FP %d LNZ %d PR_Id %d,Prod_Pricd: %02x%02x%02x%02x", i + 1, pump.gunLog[i], pr_no + 1,
				lpDispenser_data->fp_fm_data[pr_no][0].Prod_Price[0],
				lpDispenser_data->fp_fm_data[pr_no][0].Prod_Price[1],
				lpDispenser_data->fp_fm_data[pr_no][0].Prod_Price[2],
				lpDispenser_data->fp_fm_data[pr_no][0].Prod_Price[3]);
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
		Hex2Bcd(&pump.cmd[8], GUN_EXTEND_LEN, &total[3], GUN_EXTEND_LEN);
		pr_no = (lpDispenser_data->fp_ln_data[i][lnz]).PR_Id - 1;  // 0-7	
		if ((pr_no < 0) || (pr_no > 7)) {
			RunLog("InitPumpTotal,FP %d, LNZ %d, PR_Id %d", i, lnz + 1, pr_no + 1);
		}
		total[0] = 0x0a;
		total[1] = 0x00;
		total[2] = 0x00;
		memcpy(lpDispenser_data->fp_m_data[pr_no].Meter_Total, total, 7);
		memcpy((lpDispenser_data->fp_ln_data[i][pump.gunLog[i] - 1]).Log_Noz_Vol_Total, total, 7);
#if DEBUG
		RunLog("Init.fp_ln_data[%d][%d].Vol_Total: %s ", i, pump.gunLog[i] - 1,
				Asc2Hex(lpDispenser_data->fp_ln_data[0][0].Log_Noz_Vol_Total, 7));
		RunLog("Init.fp_m_data[%d].Meter_Total[%s] ", pr_no, Asc2Hex(total, 7));
		RunLog("逻辑枪号[%d]的泵码为[%s]", 
			lnz + 0x01, Asc2Hex(&pump.cmd[4], GUN_EXTEND_LEN));
#endif
		// Amount_Total
		Hex2Bcd(&pump.cmd[4], GUN_EXTEND_LEN, &total[3], GUN_EXTEND_LEN);
		total[0] = 0x0a;
		total[1] = 0x00;
		total[2] = 0x00;
		memcpy((lpDispenser_data->fp_ln_data[i][pump.gunLog[i] - 1]).Log_Noz_Amount_Total, total, 7);
		RunLog("Init.fp_ln_data[%d][%d].Amount_Total: %s ", i, pump.gunLog[i] - 1,
				Asc2Hex(lpDispenser_data->fp_ln_data[0][0].Log_Noz_Amount_Total, 7));
	}

	return 0;
}

/*
 * func: 设置控制模式
 *
 */

/*
 * func: 设定串口属性—通讯速率、校验位等
 */
static int SetPortxxx()
{
	int ret;

	if (gpGunShm->chnLogSet[pump.chnLog - 1] == 0) {
		pump.cmd[0] = 0x1C;
		pump.cmd[1] = 0x52;
		pump.cmd[2] = 0x30 | (pump.chnPhy - 1);
		pump.cmd[3] = 0x03;	// 9600bps
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
	}

	return 0;
}


/* ----------------- End of lischnxxx.c ------------------- */

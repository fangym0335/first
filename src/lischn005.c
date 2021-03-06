/*
 * Filename: lischn005.c
 *
 * Description: 加油机协议转换模块 - 河南正星, CL, 9600,E,8,1 
 *
 * History:
 *
 * 2008/05/09 - Guojie Zhu <zhugj@css-intelligent.com>
 * 修正授权命令没有恢复以至于Fuel Server 不下发 Clear TR 命令,最终导致后续加油不授权的问题
 * 2008/04/08 - Guojie Zhu <zhugj@css-intelligent.com>
 *
 * 处理说明:
 *   对于回复ACK的命令, 更加注重BB/CC的判断, 数据校验错误只进行记录
 *   而对于回复是数据的命令, 则更加注重校验的判断, 校验错误必须重试
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

#define FP_INOPERATIVE  0x01
#define FP_CLOSED       0x02
#define FP_IDLE         0x03
#define FP_CALLING      0x04
#define FP_AUTHORISED	0x05
#define FP_STARTED      0x06
#define FP_FUELLING	0x08
#define FP_SPSTARTED    0x07    // Suspended Started
#define FP_SPFUELLING   0x09    // Suspended Fuelling

#define TRY_TIME	5
#define INTERVAL	100000  // 100ms

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
static int GetPumpTotal(int pos);
static int GetPrice(int pos);
static int InitPumpTotal();
static int InitCurrentPrice();
static int InitGunLog();
static inline int GetCurrentLNZ(int pos);
static int SetCtrlMode(int pos, __u8 mode);
static int clr_offline_trans();
static int SetPort005();
static int ReDoSetPort005();
static int keep_in_touch_xs(int x_seconds);
static int CheckPriceErr(int pos);

static PUMP pump;
static int node;
static int IFSFpos;
static DISPENSER_DATA *lpDispenser_data;
static NODE_CHANNEL *lpNode_channel;
static int fp_state[MAX_FP_CNT];   // 0 - IDLE, 1 - CALLING , 2 - FUELLING

int ListenChn005(int chnLog)
{
	long	indtyp;
	int	ret;
	int	fd;
	int	j;
	int	i;
	__u8 status;
	
	sleep(5);   //防止此进程退出,而主进程没有启动完毕. 每个lischnxxx都需要
	RunLog("Listen Channel  005 ,logic channel[%d]", chnLog);
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
				pump.addInf[j][0] = 0;    // 标识读取交易后是否有交易未清除成功
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

//	RunLog("pump.port = [%d]", pump.port);
	strcpy(pump.cmd, alltty[pump.port - 1]);
	pump.fd = OpenSerial(pump.cmd);
	if (pump.fd < 0) {
		RunLog("打开串口[%s]失败", pump.cmd);
		return -1;
	}
//	RunLog("打开串口[%s].fd=[%d]", pump.cmd, pump.fd);

	ret = SetPort005();
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
		if (SetCtrlMode(i, 0xAA) < 0) {   // 设置为开放模式
			RunLog("Node[%d]FP[%d]设置开放模式失败", node, i + 1);
			return -1;
		}
	}

	// IDLE了再进入正常流程 , 要放在初始化泵码之前
	i = 30;
	do {
		ret = GetPumpStatus(0, &status);
		if (ret == 0 && ((status & 0x0F) < FP_AUTHORISED)) {
				break;
		}
		i--;
		sleep(1);
	} while (i > 0);

	if (i <= 0) {
		return -1;
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
	
	// clean history TR   // for test
	// 重新联机的时候也需要这么做
	
	if (clr_offline_trans() < 0) {
		return -1;
	}

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
				// 正星特殊处理, 因为正星有Started状态
				// 当油机本身具有该状态时, 状态切换按油机实际情况来做
				ret = SendData2CD(&msgbuffer[2], msgbuffer[1], 5);
				if (ret < 0) {
					RunLog("Send ack message [%s] to CD failed", Asc2Hex(&msgbuffer[2], msgbuffer[1]));
				}
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
	__u8 status;
	static int times = 0;
	static int fail_cnt = 0;  // 记录GetPumpStatus 失败次数, 用于判定离线
	
	for (i = 0; i < pump.panelCnt; i++) {
		ret = GetPumpStatus(i, &status);
		if (ret < 0) {
			if (gpIfsf_shm->node_online[IFSFpos] == node) {
				if ((++fail_cnt) < MAX_FAIL_CNT) {  // 加油机离线判断
					// 防止电压不稳造成单片机串口属性设置复位
					/*
					 * 因重设串口属性会导致ReadTTY接收到"非法数据", 故暂时移除
					 * 2009.02.19
					if (fail_cnt >= (MAX_FAIL_CNT / 2)) {
						if (SetPort005() >= 0) {
							RunLog("节点 %d 重设串口属性成功", node);
						}
					}
					*/
					continue;
				}

				// Step.1
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

			// 重设串口属性
			//ReDoSetPort005();
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

				// Step.1
				for (j = 0;  j < pump.panelCnt; ++j) {
					if (SetCtrlMode(j, 0xAA) < 0) {   // 设置为开放模式
						RunLog("Node[%d]FP[%d]设置开放模式失败", node, j + 1);
						fail_cnt = 1;
						return;
					}
				}

				// Step.2
				if (clr_offline_trans() < 0) {
					fail_cnt = 1;
					continue;
				}

				// Step.3
				ret = InitPumpTotal();    // 初始化泵码到数据库
				if (ret < 0) {
					RunLog("更新泵码到数据库失败");
					fail_cnt = 1;
					continue;
				}

				// Step.4
				for (j = 0;  j < pump.panelCnt; ++j) {
					lpDispenser_data->fp_data[j].FP_State = FP_State_Closed;
				}

//				gpIfsf_shm->node_online[IFSFpos] = node;
				ChangeNode_Status(node, NODE_ONLINE);
				RunLog("====== Node %d 恢复联线! =========================", node);
				keep_in_touch_xs(12);               // 确保CD已经收到心跳之后再做后续操作
				// Step.5                 // 主动上传当前FP状态
				for (j = 0; j < pump.panelCnt; ++j) {
					ChangeFP_State(node, j + 0x21, FP_State_Closed, NULL);
				}
				continue;
			}
			
			if (fail_cnt >= 2) {
				ret = 0;
				for (j = 0;  j < pump.panelCnt; ++j) {
					if (SetCtrlMode(j, 0xAA) < 0) {   // 设置为开放模式
						RunLog("Node[%d]FP[%d]设置开放模式失败", node, j + 1);
						ret--;
					}
				}

				if (ret >= 0) {
					fail_cnt = 0;
				}
				// 不管成否与否均continue
				continue;
			}
			fail_cnt = 0;
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

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = 0;
		
		switch (status & 0x0F) {
		case FP_INOPERATIVE:
			// 此状态不作为 
			break;
		case FP_IDLE:                                   // 挂枪、空闲
			if (fp_state[i] != 0) {   // Note.对没有采到挂枪信号(EOT)的补充处理
				// 若有加过油, 则进行状态改变和交易处理
				// 1. ChangeFP_State to IDLE first
				tmp_lnz = lpDispenser_data->fp_data[i].Log_Noz_State;
				if ((status & 0x80) == 0) {   // 交易结束，但是未挂枪
					lpDispenser_data->fp_data[i].Log_Noz_State = 0x00;     
				}
				if ((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State) {
					ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
				}
				RunLog("Node %d FP %d 的 %d 号枪挂枪", node, i + 1, pump.gunLog[i]);

				// Step2. Get & Save TR to TR_BUFFER
				lpDispenser_data->fp_data[i].Log_Noz_State = tmp_lnz;
				if ((status & 0x10) != 0) {
					ret = DoEot(i);
				} else {
					EndZeroTr4Fp(node, i + 0x21);
				}

				if ((status & 0x80) == 0) {   // 交易结束, 并且已挂枪
					lpDispenser_data->fp_data[i].Log_Noz_State = 0x00;
				} 
			} else {
				// 处理授权超时的情况, 授权超时会形成一笔交易, 这里把它清掉
				// 63 -> 23 -> 25 -> 73
				if ((status & 0x10) != 0) {
				       if (clr_offline_trans() < 0) {
						pump.addInf[i][0] = 1;
				       }
					pump.addInf[i][0] = 0;
				}
			}

			fp_state[i] = 0;
			// Terminate_FP 后FP状态变为Idle, 但是油枪仍为抬起状态, 补充处理
			if (((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State) ||
				(lpDispenser_data->fp_data[i].Log_Noz_State != 0 && ((status & 0x80) == 0))) {
				lpDispenser_data->fp_data[i].Log_Noz_State = 0x00;
				ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
				RunLog("Node %d FP %d 的 %d 号枪挂枪", node, i + 1, pump.gunLog[i]);
			}


			EO_OFFLINE_TR(node, i);
			if (gpIfsf_shm->fp_offline_flag[node - 1][i] != 0) {
				upload_offline_tr(node, NULL);
			}

			ret = ChangePrice();                    // 查看是否有油品需要变价,有则执行

			// spec.防止上一笔交易没有清除
			if (pump.addInf[i][0] != 0) {
			       if (clr_offline_trans() < 0) {
				       return;
			       }
				pump.addInf[i][0] = 0;
			}
			break;
		case FP_CALLING:                                // 抬枪
			if ((status & 0x10) != 0) {             // 如果有交易未清除绝对不允许加油
				// Note! 这里不能做 clr_offline_trans
				break;
			}

			if ((__u8)FP_State_Calling > lpDispenser_data->fp_data[i].FP_State) {
				ret = DoCall(i);
				fp_state[i] = 1;
				ret = BeginTr4Fp(node, i + 0x21);
				if (ret < 0) {
					RunLog("DoPolling.PUMP_BUSY: BeingTr4FP 失败");
				}
			}

			break;
		case FP_AUTHORISED:
			if ((__u8)FP_State_Authorised != lpDispenser_data->fp_data[i].FP_State) {
				ret = ChangeFP_State(node, i + 0x21, FP_State_Authorised, NULL);
			}
			break;
		case FP_STARTED:
			if ((__u8)FP_State_Started != lpDispenser_data->fp_data[i].FP_State) {
				if (lpDispenser_data->fp_data[i].Log_Noz_State == 0) {
					__u8 ln_status = 0;
					ln_status = 1 << (pump.gunLog[i] - 1);
					lpDispenser_data->fp_data[i].Log_Noz_State = ln_status;
					RunLog("Node %d FP %d 的 %d 号枪抬起", node, i + 1, pump.gunLog[i]);
				}
				ret = ChangeFP_State(node, i + 0x21, FP_State_Started, NULL);
				ret = BeginTr4Fp(node, i + 0x21);
				if (ret < 0) {
					RunLog("DoPolling.PUMP_BUSY: BeingTr4FP 失败");
				}
				fp_state[i] = 1;
			}
			break;
		case FP_FUELLING:                               // 加油
			if ((__u8)FP_State_Fuelling != lpDispenser_data->fp_data[i].FP_State) {
				if (lpDispenser_data->fp_data[i].Log_Noz_State == 0) {
					__u8 ln_status = 0; 
					ln_status = 1 << (pump.gunLog[i] - 1);
					lpDispenser_data->fp_data[i].Log_Noz_State = ln_status;
					RunLog("Node %d FP %d 的 %d 号枪抬起", node, i + 1, pump.gunLog[i]);
				}
				if ((__u8)FP_State_Started != lpDispenser_data->fp_data[i].FP_State) {
					ret = ChangeFP_State(node, i + 0x21, FP_State_Started, NULL);
					if (ret < 0) {
						break;
					}
					ret = BeginTr4Fp(node, i + 0x21);
					if (ret < 0) {
						RunLog("DoPolling.PUMP_BUSY: BeingTr4FP 失败");
					}
				}

				ret = ChangeFP_State(node, i + 0x21, FP_State_Fuelling, NULL);
				if (ret < 0) {
					break;
				}

				fp_state[i] = 2;
			}
			DO_INTERVAL(DO_BUSY_INTERVAL, ret = DoBusy(i););
			break;
		case FP_SPSTARTED:
			break;     // CPPEI 不要求此状态
			if ((__u8)FP_State_Suspended_Started != lpDispenser_data->fp_data[i].FP_State) {
				ret = ChangeFP_State(node, i + 0x21, FP_State_Suspended_Started, NULL);
			}
			break;
		case FP_SPFUELLING:                            // Limit-Reached
			break;     // CPPEI 不要求此状态
			if ((__u8)FP_State_Suspended_Fuelling != lpDispenser_data->fp_data[i].FP_State) {
				ret = ChangeFP_State(node, i + 0x21, FP_State_Suspended_Fuelling, NULL);
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
 * func: PreSet/PrePay & Release_FP
 */
static int DoRation(int pos)  
{
	int ret;
	int trycnt;
	int ration_type;
	__u8 pre_volume[3] = {0};
	static __u8 chain_no[2] = {0, 0};  // 链号

	// 若有为成功清除的交易, 不允许再次加油, 不许先清除
	if (pump.addInf[pos][0] != 0) {
		return -1;
	}

	if (pump.cmd[0] == 0x3D) {   // Preset
		ration_type = 0x02;
		memcpy(pre_volume, &lpDispenser_data->fp_data[pos].Remote_Volume_Preset[2], 3);
	} else {                     // Prepay
		ration_type = 0x01;
		memcpy(pre_volume, &lpDispenser_data->fp_data[pos].Remote_Amount_Prepay[2], 3);
	}

	BBcdInc(chain_no, 2);  // 链号增1, 0001 - 9999
	for (trycnt = 0; trycnt < TRY_TIME; trycnt++) {		
		// Step.1 发送预置命令
		pump.cmd[0] = 0xFC;                // command head
		pump.cmd[1] = ration_type;         // command code
		pump.cmd[2] = pump.panelPhy[pos];  // FP_Nb
		memcpy(&pump.cmd[3], pre_volume, 3);
		if (ration_type == 0x01) {
			pump.cmd[6] = 0x00;                // type
			pump.cmd[7] = chain_no[0];
			pump.cmd[8] = chain_no[1];
			pump.cmd[9] = Check_xor(&pump.cmd[1], 8);
			pump.cmdlen = 10;
		} else {
			pump.cmd[6] = chain_no[0];
			pump.cmd[7] = chain_no[1];
			pump.cmd[8] = Check_xor(&pump.cmd[1], 7);
			pump.cmdlen = 9;
		}
		pump.wantLen = 6;

		RunLog("Do ration: %s ", Asc2Hex(pump.cmd, 9));
		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("写预置量到Node[%d].FP[%d]失败", node, pos + 1);
			continue;
		}
		
		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("写预置量到Node[%d].FP[%d]成功, 取返回信息失败", node, pos + 1);
			continue;
		} 

		if (pump.cmd[0] != 0xFC || pump.cmd[1] != ration_type ||
					pump.cmd[2] != pump.panelPhy[pos]) {
			RunLog("返回数据有误");
			continue;
		}
		
		if (pump.cmd[4] != 0xBB) {
			RunLog("执行预置加油命令失败");
			continue;
		}

		if (pump.cmd[pump.wantLen - 1] != Check_xor(&pump.cmd[1], pump.wantLen - 2)) {
			RunLog("Return Message XOR ERROR");
			if ((pump.cmd[3] & 0x60 != 0x00) || pump.cmd[3] & 0x60 != 0x01) {
				continue;
			}
		}

		RunLog("写预置量到Node[%d].FP[%d]成功", node, pos + 1);
		// [Step.2 授权加油]
		/*
		 * 实现建议&修改说明: 有的油机下发了预置量之后无需另外授权既可以加油, 无需Step.2
		 */
		ret = DoAuth(pos);
		if (ret < 0) {
			RunLog("预置加油授权失败!");
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
	__u8 pr_no, lnz;
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
						// Step.1 发变价命令
						pump.cmd[0] = 0xFC;
						pump.cmd[1] = 0x03;
						pump.cmd[2] = pump.panelPhy[i];
						pump.cmd[3] = pump.newPrice[pr_no][2];
						pump.cmd[4] = pump.newPrice[pr_no][3];
						pump.cmd[5] = Check_xor(&pump.cmd[1], 4);
						pump.cmdlen = 6;
						pump.wantLen = 6;

						Ssleep(INTERVAL);
						ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd, 
								pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
						if (ret < 0) {
							RunLog("发变价命令到Node[%d].FP[%d]失败", node, i + 1);
							continue;
						}

						RunLog("Node[%d].FP[%d].PriceChange_CMD: %s",
							node, i + 1, Asc2Hex(pump.cmd, pump.cmdlen));

						// Step.2 根据命令返回信息或者重读油机单价确认变价是否成功
						bzero(pump.cmd, pump.wantLen);
						pump.cmdlen = pump.wantLen;
						ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
						if (ret < 0) {
							RunLog("发变价命令到Node[%d].FP[%d]成功, 取返回信息失败", node, i + 1);
							continue;
						}


						if (pump.cmd[pump.wantLen - 1] != Check_xor(&pump.cmd[1], pump.wantLen - 2)) {
							RunLog("Node[%d].FP[%d]Return Message XOR ERROR", node, i + 1);
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
							continue;
						}

						if (pump.cmd[0] != 0xFC || pump.cmd[1] != 0x03 ||
								pump.cmd[2] != pump.panelPhy[i]) {
							RunLog("Node[%d].FP[%d]返回数据有误", node, i + 1);
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
							continue;
						}
						
						if (pump.cmd[4] != 0xBB) {
							RunLog("Node[%d].FP[%d]执行变价失败", node, i + 1);
							err_code_tmp = 0x37;
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
							continue;
						}

						//  变价成功了也要读单价进行验证
						ret = GetPrice(i);
						if (ret < 0) {
							continue;
						}

						if (memcmp(&pump.newPrice[pr_no][2], &pump.cmd[4], 2) != 0) {
							err_code_tmp = 0x37;
							// 避免税控忙导致变价失败, 所以延迟2s再重试 
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
							continue;
						}

						
						// Step.3 写新单价到油品数据库 & 清零单价缓存
						fm_idx = lpDispenser_data->fp_data[i].Default_Fuelling_Mode - 1;
					RunLog("Node[%d].FP[%d]发送变价命令成功,新单价fp_fm_data[%d][%d].Prode_Price: %s",
							node, i + 1, pr_no, fm_idx,
							Asc2Hex(lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price, 4));

					CriticalLog("节点[%02d]面[%d]枪[%d]变价成功,新单价为%02x.%02x",
						node, i + 1, j + 1,
						lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2],
						lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3]);
						err_code_tmp = 0x99;
						break;
						
					//	Ssleep(INTERVAL*10);
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
					if(0x99 == err_code){
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
	__u8 price[5];
	__u8 volume[5];
	__u8 amount[5];
	__u8 total[7];
	__u8 pre_total[7];
	__u8 amt_total[7];
	__u8 buffer[3];

	RunLog("节点[%d]面板[%d]处理新交易", node, pos + 1);

	/*
	 * 正星读交易命令返回的数据中虽然也有泵码, 但是那个泵码到999999.99的时候会归零, 但是实际的泵码不会, 
	 * 这个时候就会出现两个命令获得的泵码值不一样的问题, 相差x百万
	 */
	err_cnt = 0;
	do {
		ret = GetPumpTotal(pos);	// 取当次交易记录
		if (ret != 0) {
			err_cnt++;
			RunLog("Node %d FP %d 读取泵码失败 <ERR>", node, pos + 1);
		}
	} while (ret < 0 && err_cnt <= 5);
	
	if (ret < 0) {
		return -1;
	}

	total[0] = 0x0a;
	total[1] = 0x00;
	Hex2Bcd(&pump.cmd[8], GUN_EXTEND_LEN, &total[2], GUN_EXTEND_LEN + 1);

	amt_total[0] = 0x0a;
	amt_total[1] = 0x00;
	Hex2Bcd(&pump.cmd[4], GUN_EXTEND_LEN, &amt_total[2], GUN_EXTEND_LEN + 1);

	pr_no = (lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).PR_Id - 1;  // 0 - 7	
	if ((pr_no < 0) || (pr_no > 7)) {
		RunLog("DoEot PR_Id error, PR_Id[%d] <ERR>", pr_no + 1);
	}
	memcpy(pre_total, (lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Vol_Total, 7);

	// 读交易(一定要在比较泵码之前做, 因为如果是零交易也需要清除)
	err_cnt = 0;
	do {
		ret = GetTransData(pos);	// 取当次交易记录
		if (ret != 0) {
			err_cnt++;
			RunLog("Node %d FP %d 读取交易数据失败 <ERR>", node, pos + 1);
		}
	} while (ret < 0 && err_cnt <= 5);
	
	if (ret < 0) {
		return -1;
	}

	if (memcmp(total, pre_total, 7) != 0) {
		// Amount
		buffer[0] = pump.cmd[6] >> 4;
		buffer[1] = (pump.cmd[6] << 4) | (pump.cmd[7] >> 4);
		buffer[2] = (pump.cmd[7] << 4) | (pump.cmd[8] >> 4);
		amount[0] = 0x06;
		Hex2Bcd(buffer, 3, &amount[1], 4);
		memcpy(lpDispenser_data->fp_data[pos].Current_Amount, amount, 5);

		// Volume
		buffer[0] = pump.cmd[8] & 0x0F;
		buffer[1] = pump.cmd[9];
		buffer[2] = pump.cmd[10];
		volume[0] = 0x06;
		Hex2Bcd(buffer, 3, &volume[1], 4);
		memcpy(lpDispenser_data->fp_data[pos].Current_Volume, volume, 5);

		// Price
		price[0] = 0x04;
		price[1] = 0x00;
		price[2] = pump.cmd[19];
		price[3] = pump.cmd[20];
		memcpy(lpDispenser_data->fp_data[pos].Current_Unit_Price, price, 4);
		
		// Update Vol_total
		memcpy(lpDispenser_data->fp_m_data[pr_no].Meter_Total, total, 7);
		memcpy((lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Vol_Total, total, 7);

		// Update Amount_total
		memcpy((lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Amount_Total, amt_total, 7);

#if 0
		RunLog("节点[%02d]面板[%d]枪[%d] ## Amount: %02x%02x.%02x Volume: %02x%02x.%02x Price: %02x.%02x Total: %02x%02x%02x%02x.%02x", node, pos + 1, pump.gunLog[pos], amount[2], 
					amount[3], amount[4], volume[2], volume[3], volume[4], price[2],
							price[3], total[2], total[3], total[4], total[5], total[6]);
#endif

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
				RunLog("DoEot.EndTr4Fp 失败, ret: %d <ERR>", ret);
			}
		} while (ret < 0 && err_cnt <= 8);

	} else {
		ret = EndZeroTr4Fp(node, pos + 0x21);
		if (ret < 0) {
			RunLog("DoEot.EndZeroTr4Fp 失败, ret: %d <ERR>", ret);
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
	int trycnt;

	for (trycnt = 0; trycnt <TRY_TIME ; trycnt++) {
		pump.cmd[0] = 0xFC;
		pump.cmd[1] = 0x0C;
		pump.cmd[2] = pump.panelPhy[pos];
		pump.cmd[3] = pump.cmd[1] ^ pump.cmd[2];
		pump.cmdlen = 4;
		pump.wantLen = 17;
		
		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("发 取泵码命令 到Node[%d].FP[%d]失败", node, pos + 1);
			continue;
		}

		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("发 取泵码命令 到Node[%d].FP[%d]成功, 取返回信息失败", node, pos + 1);
			continue;
		}

		if (pump.cmd[pump.wantLen - 1] != Check_xor(&pump.cmd[1], pump.wantLen - 2)) {
			RunLog("Return Message XOR ERROR");
			continue;
		}

		if (pump.cmd[0] != 0xFC || pump.cmd[1] != 0x0C ||
			       	pump.cmd[2] != pump.panelPhy[pos]) {
			RunLog("返回数据有误");
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
	int ret;
	int trycnt;
	int ack_cmdlen;
	int ack_wantLen;
	__u8 status;
	__u8 ack_cmd[6];
	__u8 data_backup[40] = {0};
	__u8 got_flag = 0;       // 标记是否成功取得过交易数据, 非零 -- 是; 0 - 否

	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) { 
		// Step.1 Read Transaction Data
		pump.cmd[0] = 0xFC;
		pump.cmd[1] = 0x15;
		pump.cmd[2] = pump.panelPhy[pos];
		pump.cmd[3] = pump.cmd[1] ^ pump.cmd[2];
		pump.cmdlen = 4;
		pump.wantLen = 36;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("发读交易数据命令到Node[%d].FP[%d]失败", node, pos + 1);
			continue;
		}

		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("发读交易数据命令到Node[%d].FP[%d]成功, 取返回信息失败", node, pos + 1);
			continue;
		}

		if (pump.cmd[pump.wantLen - 1] != Check_xor(&pump.cmd[1], pump.wantLen - 2)) {
			RunLog("Return Message XOR ERROR");
			continue;
		}

		if (pump.cmd[0] != 0xFC || pump.cmd[1] != 0x15 ||
			       	pump.cmd[2] != pump.panelPhy[pos]) {
			RunLog("返回数据有误");
			continue;
		}


		if (pump.cmd[1] == 0x16) {     // Exec Successful, but have no transaction
#if 1
			if (0 == got_flag) {
				return 1;      // 无交易直接返回
			} else {
				/*
				 * Step.2 没有正确完成--实际上交易已经被清除
				 */

				goto EO_GETRANSDATA;
			}
#endif
		}
		
		// 备份本次读到的数据
		memcpy(data_backup, pump.cmd, pump.wantLen);
		got_flag = 1;

		// Step.2 Answer pump -- remove this transaction from pump's buffer
		ack_cmd[0] = 0xFC;
		ack_cmd[1] = 0x17;
		ack_cmd[2] = pump.panelPhy[pos];
		ack_cmd[3] = pump.cmd[23];
		ack_cmd[4] = pump.cmd[24];
		ack_cmd[5] = Check_xor(&ack_cmd[1], 4);
		ack_cmdlen = 6;
		ack_wantLen = 5;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, ack_cmd,
			       	ack_cmdlen, 1, ack_wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("发读交易数据回复到Node[%d].FP[%d]失败", node, pos + 1);
			continue;
		}

		bzero(ack_cmd, ack_wantLen);
		ack_cmdlen = ack_wantLen;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, ack_cmd, &ack_cmdlen);
		if (ret < 0) {
			RunLog("发读交易数据回复到Node[%d].FP[%d]成功, 取返回信息失败", node, pos + 1);
			continue;
			// 防止油机删除了交易, 但是FCC没有收到的情况, 做完交易后再来清除
//			pump.addInf[pos][0] = 1;  // jie @ 2008.8.6
		}

		if (ack_cmd[ack_wantLen - 1] != Check_xor(&ack_cmd[1], ack_wantLen - 2)) {
			RunLog("Return Message XOR ERROR");
			continue;
//			pump.addInf[pos][0] = 1;
		}

		if (ack_cmd[0] != 0xFC || ack_cmd[1] != 0x17 ||
			       	ack_cmd[2] != pump.panelPhy[pos]) {
			RunLog("返回数据有误");
			continue;
//			pump.addInf[pos][0] = 1;
		}

		if ((ack_cmd[3] & 0x10) != 0) {
			RunLog("交易未成功清除");
			continue;
//			pump.addInf[pos][0] = 1;
		}


		return 0;
	}

	
EO_GETRANSDATA:
	if (0 != got_flag) {
		memcpy(pump.cmd, data_backup, pump.wantLen);
		pump.cmdlen = 36;
		return 0;
	}

	return -1;
}

/*
 * func: 读取历史交易记录
 */
static int GetHisTransData(int pos, int seq_nb)
{
	int ret;
	int trycnt;
	int ack_cmdlen;
	int ack_wantLen;
	static int Tr_Seq_Nb = 0;
	__u8 ack_cmd[6];

	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) { 
		// Step.1 Read Transaction Data
		pump.cmd[0] = 0xFC;
		pump.cmd[1] = 0x13;
		pump.cmd[2] = pump.panelPhy[pos];
		pump.cmd[3] = pump.cmd[1] ^ pump.cmd[2];
		pump.cmdlen = 4;
		pump.wantLen = 36;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("发读交易数据命令到Node[%d].FP[%d]失败", node, pos + 1);
			continue;
		}

		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("发读交易数据命令到Node[%d].FP[%d]成功, 取返回信息失败", node, pos + 1);
			continue;
		}

		if (pump.cmd[pump.wantLen - 1] != Check_xor(&pump.cmd[1], pump.wantLen - 2)) {
			RunLog("Return Message XOR ERROR");
			continue;
		}

		if (pump.cmd[0] != 0xFC || pump.cmd[1] != 0x13 ||
			       	pump.cmd[2] != pump.panelPhy[pos]) {
			RunLog("返回数据有误");
			continue;
		}

		// Step.2 Answer pump -- remove this transaction
		ack_cmd[0] = 0xFC;
		ack_cmd[1] = 0x17;
		ack_cmd[2] = pump.panelPhy[pos];
		ack_cmd[3] = pump.cmd[23];
		ack_cmd[4] = pump.cmd[24];
		ack_cmd[5] = Check_xor(&ack_cmd[1], 4);
		ack_cmdlen = 6;
		ack_wantLen = 5;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, ack_cmd,
			       	ack_cmdlen, 1, ack_wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("发读交易数据回复到Node[%d].FP[%d]失败", node, pos + 1);
			continue;
		}

		bzero(ack_cmd, ack_wantLen);
		ack_cmdlen = ack_wantLen;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, ack_cmd, &ack_cmdlen);
		if (ret < 0) {
			RunLog("发读交易数据回复到Node[%d].FP[%d]成功, 取返回信息失败", node, pos + 1);
			continue;
		}

		if (ack_cmd[ack_wantLen - 1] != Check_xor(&ack_cmd[1], ack_wantLen - 1)) {
			RunLog("Return Message XOR ERROR");
//			continue;   // 这失败了怎么办
		}

		if (ack_cmd[0] != 0xFC || ack_cmd[1] != 0x17 ||
			       	ack_cmd[2] != pump.panelPhy[pos]) {
			RunLog("返回数据有误");
			continue;
		}

		return 0;
	}

	return -1;
}


/*
 * func: 暂停加油，在此做Terminate_FP用
 */
static int DoStop(int pos)
{
	int i;
	int ret;
	int trycnt;
	__u8 status;

	RunLog("Node %d FP %d DoStop", node, pos + 1);

	for (trycnt = 0; trycnt < TRY_TIME; trycnt++) {
		pump.cmd[0] = 0xFC;
		pump.cmd[1] = 0x05;
		pump.cmd[2] = pump.panelPhy[pos];
		pump.cmd[3] = pump.cmd[1] ^ pump.cmd[2];
		pump.cmdlen = 4;
		pump.wantLen = 5;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("发 终止加油命令 到Node[%d].FP[%d]失败", node, pos + 1);
			continue;
		}

		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("发 终止加油命令 到Node[%d].FP[%d]成功, 取返回信息失败", node, pos + 1);
			continue;
		}

		if (pump.cmd[pump.wantLen - 1] != Check_xor(&pump.cmd[1], pump.wantLen - 2)) {
			RunLog("Return Message XOR ERROR");
			continue;
		}

		if (pump.cmd[0] != 0xFC || pump.cmd[1] != 0x05 ||
			       	pump.cmd[2] != pump.panelPhy[pos]) {
			RunLog("返回数据有误");
			continue;
		}

		if ((pump.cmd[3] & 0x0F) == FP_IDLE) {
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
	int ret;
	int trycnt;
	__u8 status;

	RunLog("Node %d FP %d DoAuth", node, pos + 1);

	for (trycnt = 0; trycnt < TRY_TIME; trycnt++) {
		pump.cmd[0] = 0xFC;
		pump.cmd[1] = 0x06;
		pump.cmd[2] = pump.panelPhy[pos];
		pump.cmd[3] = pump.cmd[1] ^ pump.cmd[2];
		pump.cmdlen = 4;
		pump.wantLen = 5;

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

		if (pump.cmd[pump.wantLen - 1] != Check_xor(&pump.cmd[1], pump.wantLen - 2)) {
			RunLog("Return Message XOR ERROR");
			continue;
		}

		if (pump.cmd[0] != 0xFC || pump.cmd[1] != 0x06 ||
			       	pump.cmd[2] != pump.panelPhy[pos]) {
			RunLog("返回数据有误");
			continue;
		}

		if (((pump.cmd[3] & 0x0F) == FP_STARTED) ||
			((pump.cmd[3] & 0x0F) == FP_AUTHORISED)) {
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
	int trycnt;
	__u8 amount[5] = {0};
	__u8 volume[5] = {0};

	for (trycnt = 0; trycnt < 2; ++trycnt) {
		// Step.1 读取实时数据
		pump.cmd[0] = 0xFC;
		pump.cmd[1] = 0x18;
		pump.cmd[2] = pump.panelPhy[pos];
		pump.cmd[3] = pump.cmd[1] ^ pump.cmd[2];
		pump.cmdlen = 4;
		pump.wantLen = 13;

		Ssleep(INTERVAL); 
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("发 取实时加油数据 命令到Node[%d].FP[%d]失败", node, pos + 1);
			continue;
		}

		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("发 取实时加油数据 命令到Node[%d].FP[%d]成功, 取返回信息失败", node, pos + 1);
			continue;
		}

		if (pump.cmd[pump.wantLen - 1] != Check_xor(&pump.cmd[1], pump.wantLen - 2)) {
			RunLog("Return Message XOR ERROR");
			continue;
		}

		if (pump.cmd[0] != 0xFC || pump.cmd[1] != 0x18 ||
			       	pump.cmd[2] != pump.panelPhy[pos]) {
			RunLog("返回数据有误");
			continue;
		}

		// Step.2  Exec ChangeFP_Run_Trans()

		memcpy(&volume[2], &pump.cmd[4], 3);
		volume[0] = 0x06;
		volume[1] = 0x00;
		memcpy(&amount[2], &pump.cmd[7], 3);
		amount[0] = 0x06;
		amount[1] = 0x00;

		ret = ChangeFP_Run_Trans(node, 0x21 + pos, amount, volume);

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
 * func: 读取油机单价 密度 时间
 * 	prince:pump.cmd[4]pump.cmd[5] (BCD)
 */

static int GetPrice(int pos)
{
	int ret;
	int trycnt;

	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {
		pump.cmd[0] = 0xFC;
		pump.cmd[1] = 0x12;
		pump.cmd[2] = pump.panelPhy[pos];
		pump.cmd[3] = pump.cmd[1] ^ pump.cmd[2];
		pump.cmdlen = 4;
		pump.wantLen = 18;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("发读取油机单价命令到Node[%d].FP[%d]失败", node, pos + 1);
			continue;
		}

		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("发读取油机单价命令到Node[%d].FP[%d]成功, 取返回信息失败", node, pos + 1);
			continue;
		} 

		if (pump.cmd[pump.wantLen - 1] != Check_xor(&pump.cmd[1], pump.wantLen - 2)) {
			RunLog("Return Message XOR ERROR");
			continue;
		}

		if (pump.cmd[0] != 0xFC || pump.cmd[1] != 0x12 ||
			       	pump.cmd[2] != pump.panelPhy[pos]) {
			RunLog("返回数据有误");
			continue;
		}

		return 0;
	}

	return -1;
}

/*
 * func: 读取实时状态
 */

static int GetPumpStatus(int pos, __u8 *status)
{
	int trycnt;
	int ret;
	static __u8 pre_status[MAX_FP_CNT] = {0};

	for (trycnt = 0; trycnt < 2; ++trycnt) {
		pump.cmd[0] = 0xFC;
		pump.cmd[1] = 0x0D;
		pump.cmd[2] = pump.panelPhy[pos];
		pump.cmd[3] = pump.cmd[1] ^ pump.cmd[2];
		pump.cmdlen = 4;
		pump.wantLen = 5;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("发取状态命令到Node[%d].FP[%d]失败", node, pos + 1);
			continue;
		}

		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("发取状态命令到Node[%d].FP[%d]成功, 取返回信息失败", node, pos + 1);
			continue;
		} 

		if (pump.cmd[pump.wantLen - 1] != Check_xor(&pump.cmd[1], pump.wantLen - 2)) {
			RunLog("Return Message XOR ERROR");
			continue;
		}

		if (pump.cmd[0] != 0xFC || pump.cmd[1] != 0x0D ||
			       	pump.cmd[2] != pump.panelPhy[pos]) {
			RunLog("返回数据有误");
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
	int ret;
	int trycnt;

	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {         // Note. GetPumpStatus 循环变量最好用本地的,不要用trycnt
		pump.cmd[0] = 0xFC;
		pump.cmd[1] = 0x0D;
		pump.cmd[2] = pump.panelPhy[pos];
		pump.cmd[3] = pump.cmd[1] ^ pump.cmd[2];
		pump.cmdlen = 4;
		pump.wantLen = 5;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("发取状态命令到Node[%d].FP[%d]失败", node, pos + 1);
			continue;
		}

		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("发取状态命令到Node[%d].FP[%d]成功, 取返回信息失败", node, pos + 1);
			continue;
		} 

		if (pump.cmd[pump.wantLen - 1] != Check_xor(&pump.cmd[1], pump.wantLen - 2)) {
			RunLog("Return Message XOR ERROR");
			continue;
		}

		if (pump.cmd[0] != 0xFC || pump.cmd[1] != 0x0D ||
			       	pump.cmd[2] != pump.panelPhy[pos]) {
			RunLog("加油点号错误");
			continue;
		}

		*status = pump.cmd[3];

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
		Hex2Bcd(&pump.cmd[8], GUN_EXTEND_LEN, &total[2], GUN_EXTEND_LEN + 1);
		pr_no = (lpDispenser_data->fp_ln_data[i][lnz]).PR_Id - 1;  // 0-7	
		if ((pr_no < 0) || (pr_no > 7)) {
			RunLog("InitPumpTotal,FP %d, LNZ %d, PR_Id %d", i, lnz + 1, pr_no + 1);
		}

		memcpy(lpDispenser_data->fp_m_data[pr_no].Meter_Total, total, 7);
		memcpy((lpDispenser_data->fp_ln_data[i][pump.gunLog[i] - 1]).Log_Noz_Vol_Total, total, 7);
#if DEBUG
		RunLog("逻辑枪号[%d]的泵码为[%s]", 
			lnz + 0x01, Asc2Hex(&pump.cmd[8], GUN_EXTEND_LEN));
		RunLog("Init.fp_ln_data[%d][%d].Vol_Total: %s ", i, pump.gunLog[i] - 1,
				Asc2Hex(lpDispenser_data->fp_ln_data[0][0].Log_Noz_Vol_Total, 7));
		RunLog("Init.fp_m_data[%d].Meter_Total[%s] ", pr_no, Asc2Hex(total, 7));
#endif
		// Amount_Total
		total[0] = 0x0a;
		total[1] = 0x00;
		Hex2Bcd(&pump.cmd[4], GUN_EXTEND_LEN, &total[2], GUN_EXTEND_LEN + 1);
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

static int SetCtrlMode(int pos, __u8 mode)
{
	int trycnt;
	int ret;

	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {         // Note. GetPumpStatus 循环变量最好用本地的,不要用trycnt
		pump.cmd[0] = 0xFC;
		pump.cmd[1] = 0x08;
		pump.cmd[2] = pump.panelPhy[pos];
		pump.cmd[3] = mode;
		pump.cmd[4] = pump.cmd[1] ^ pump.cmd[2] ^ pump.cmd[3];
		pump.cmdlen = 5;
		pump.wantLen = 5;

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

		if (pump.cmd[2] != pump.panelPhy[pos]) {
			RunLog("加油点号错误");
			continue;
		}

		if (pump.cmd[pump.wantLen - 1] != Check_xor(&pump.cmd[1], pump.wantLen - 2)) {
			RunLog("Return Message XOR ERROR");
			continue;
		}

		RunLog("Node[%d]FP[%d]设置控制模式成功", node, pos);
		return 0;
	}

	RunLog("Node[%d]FP[%d]设置控制模式失败", node, pos);
	return -1;
}



/*
 * func: 清除油机脱机交易
 */
static int clr_offline_trans()
{
	int i;
	int ret;
	__u8 status;

	for (i = 0; i < pump.panelCnt; ++i) {
		while (1) {
			ret = GetPumpStatus(i, &status);
			if (ret == 0 && (status & 0x10) != 0) {
				ret = GetTransData(i);
				continue;
			} else if (ret < 0) {
				return -1;
			}
			break;
		}; 
	}

	return 0;
}
/*
 * func: 设定串口属性—通讯速率、校验位等
 */
static int SetPort005()
{
	int ret;

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
		RunLog("节点 %d 设置串口属性失败", node);
		return -1;
	}

	gpGunShm->chnLogSet[pump.chnLog - 1] = 1;
	Ssleep(INTERVAL);      // 使用之前串口属性已经设置好

	return 0;
}


/*
 * func:  通讯不通时轮询过程中隔一段时间重设一下串口属性
 *        避免因意外的电压不稳导致单片机复位--串口属性设置恢复预设9600,8,n,1
 */
static int ReDoSetPort005()
{
#define RE_DO_FREQUENCY   10         //  10次轮询失败设置一下串口属性

	int static polling_fail_cnt = 0;
	int ret;

	if ((++polling_fail_cnt) >= RE_DO_FREQUENCY) {
		ret = SetPort005();
		if (ret < 0) {
			RunLog("节点 %d 重设串口属性失败", node);
		}
		polling_fail_cnt = 0;
	}

	return ret;
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
			node,pos+1,lnz+1,pump.cmd[4], pump.cmd[5],
		       	lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2], 
		       	lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3]);

		if ((pump.cmd[4] != lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2]) ||
			(pump.cmd[5] != lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3])) {
			return -2;
		}
	}
	
	ChangeFP_State(node, 0x21 + pos, FP_State_Closed, NULL);
	RunLog("Node[%d]FP[%d]单价已变更正确, 切换状态为Closed", node, pos + 1);

	return 0;
}

/* ----------------- End of lischn005.c ------------------- */

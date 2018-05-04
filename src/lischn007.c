/*
 * Filename: lischn007.c
 *
 * Description: ���ͻ�Э��ת��ģ�� - �Ϻ�����������, RS485, 9600,O,8,1 
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

#define FP_INOPERATIVE  0x00    // δ�����
#define FP_IDLE         0x01    // �ѱ�����
#define FP_AUTHORISED   0x02    // ����Ȩ
#define FP_FUELLING     0x04    // ���ڼ���
#define FP_EOT          0x05    // �������
#define FP_SPFUELLING   0x06    // ���͵����޶�ֵ
#define FP_CLOSED       0x07    // �ػ�


#define _ACK            0xC0    
#define _NAK            0x50
#define _EOT            0x70
#define _ACKPOLL        0xE0

#define TRY_TIME	5
#define INTERVAL        150000  // 150ms , ���ͻ�����

// TX#��һ����
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
static __u8 tx_no[MAX_FP_CNT] = {0x0F, 0x0F, 0x0F, 0x0F};   // TX#, ���ݿ�˳����
static int dostop_flag[MAX_FP_CNT] = {0};  // ����Ƿ�ɹ�ִ�й� Terminate_FP
static int fp_state[MAX_FP_CNT] = {0}; 
static int pre_flg[MAX_FP_CNT] = {0};      // 1 - ̧ǹ����Ҫ�·�Ԥ��; 2 - �Ѿ��·�Ԥ��δ����
static __u8 glb_pre_volume[MAX_FP_CNT][5] = {0};
static __u8 glb_pre_amount[MAX_FP_CNT][5] = {0};
static int pre_ration_type[MAX_FP_CNT] = {0};

// -1 - ���ͺ�δ��ǹ, 0 - IDLE, 1 - CALLING , 2 - �����DoStarted, 3 - FUELLING

typedef struct {
	__u8 status;             // ����״̬, ��Ӧ DC1.STATUS
	__u8 lnz_state;          // ��ǹ״̬, ��Ӧ DC3.NOZIO
	__u8 current_vol[4];     // ʵʱ������, ��Ӧ DC2.VAL
	__u8 current_amo[4];     // ʵʱ���ͽ��, ��Ӧ DC2.AMO
	__u8 current_pri[3];     // ��ǰ����, ��Ӧ DC3.PRI
	__u8 vol_total[GUN_EXTEND_LEN];  // �ô���ȡ��������
	__u8 amo_total[GUN_EXTEND_LEN];  // �ô���ȡ�������
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
	
	sleep(5);   //��ֹ�˽����˳�,��������û���������.
	RunLog("Listen Channel  007 ,logic channel[%d]", chnLog);
	memset(&pump, 0, sizeof(pump));
	pump.chnLog = chnLog;
	pump.panelCnt = 0;

	// ============ Part.1 ��ʼ�� pump �ṹ  ======================================
	// ɨ�蹲���ڴ�, ����ͨ�����ӵĴ���, ���
	
	for (i = 0; i < MAX_CHANNEL_CNT; i++) {
		if (chnLog == gpIfsf_shm->node_channel[i].chnLog) {
			IFSFpos = i;
			lpNode_channel = (NODE_CHANNEL *)(&(gpIfsf_shm->node_channel[i]));
			// node_channel��dispenser_data����һһ��Ӧ
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
						// !!!Note.����ͨѶ��ַΪ 79 + ����ϵ���ʾ�ĵ�ֵַ
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

	// ============= Part.2 �� & ���ô������� -- ������/У��λ�� ===================

	strcpy(pump.cmd, alltty[pump.port - 1]);
	pump.fd = OpenSerial(pump.cmd);
	if (pump.fd < 0) {
		RunLog("�򿪴���[%s]ʧ��", pump.cmd);
		return -1;
	}

	ret = SetPort007();
	if (ret < 0) {
		RunLog("���ô�������ʧ��");
		return -1;
	}
	
	// ============== Part.3 �жϼ��ͻ��Ƿ����� ====================================
	// Check wether dispenser is online, if it's not online, wait  here.
	while (1) {
		ret = GetPumpOnline(0);
		if (ret == 0) {
			break;
		}

		gpIfsf_shm->node_online[IFSFpos] = 0; //������,����������.
		UserLog("Node %d ������", node);
		
		sleep(1);
	}
	
	ret = SetRCAP2L();
	if (ret < 0) {
		return -1;
	}

	// ��λTX#
	for (i = 0; i < pump.panelCnt; ++i) {
		if (GetPumpOnline(i) < 0) {
			return -1;
		}
	}
	// ============= Part.4 ��ʼ������ͻ���ǰ״̬��ص����ݿ������ =======================
	
	// ��������������ģʽ, ��������

	ret = InitCurrentPrice(); //  ��ʼ��fp���ݿ�ĵ�ǰ�ͼ�
	if (ret < 0) {
		RunLog("��ʼ����Ʒ���۵����ݿ�ʧ��");
		return -1;
	}

	for (i = 0; i < pump.panelCnt; ++i) { // ����������ǹ����
		ret = SetNozMask(i, 0xFF);
		if (ret < 0) {
			return -1;
		}
	}

	for (i = 0; i < pump.panelCnt; ++i) {
		// Step.1 ����PUMP_MSG�ṹ
		ret = GetPumpStatus(i);
		if (ret < 0) {
			RunLog("��ʼ��pump_msgʧ��");
			return -1;
		}
		// Step.2 �жϵ�ǰ״̬�Ƿ���Խ������� DoPolling ����
		// ����Ǽ����л�������Ȩ, �Ǿ͵ȴ�... 
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

	// ����Ҫ����ǹ��Ż����
	ret = InitPumpTotal();    // ��ʼ�����뵽���ݿ�
	if (ret < 0) {
		RunLog("��ʼ�����뵽���ݿ�ʧ��");
		return -1;
	}

//	gpIfsf_shm->node_online[IFSFpos] = node;  // һ�о���, ����
	ChangeNode_Status(node, NODE_ONLINE);
	RunLog("�ڵ� %d ����!", node);
	keep_in_touch_xs(12);               // ȷ��CD�Ѿ��յ�����֮��������������

	// =============== Part.5 �ͻ�ģ���ʼ�����,ת��FP״̬Ϊ Idle ==================
	// when dispenser is initialized , the status is Closed. it's waiting for set command from CD. 
	for (i = 0; i < lpNode_channel->cntOfFps; i++) {
		if ((__u8)FP_State_Closed != lpDispenser_data->fp_data[i].FP_State) {
			ret = ChangeFP_State(node, 0x21 + i, FP_State_Closed, NULL);
		}
	}

	// =============== Part.6 ����ʱ������� ========================================
	while (1) {
		if (gpGunShm->sysStat == RESTART_PROC ||
			gpGunShm->sysStat == STOP_PROC) {
			RunLog("ϵͳ�˳�");
			exit(0);
		}

		while (gpGunShm->sysStat == PAUSE_PROC) {
			__u8 status;
			GetPumpStatus(0);              // Note. ���ͻ�������ϵ
			Ssleep(INTERVAL);
			RunLog("ϵͳ��ͣ");
		}

		indtyp = pump.indtyp;

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		ret = -1;

		// ---------- Part.6.1 ��̨��������µĴ������� ----------------------
		if (gpIfsf_shm->auth_flag - FCC_SWITCH_AUTH != 0) { 
		       	/* 
			 * ����Ƿ��к�̨����, �������ת��DoCmdִ�и�����; ���û��, �������ѯ
			 */
			ret = RecvMsg(ORD_MSG_TRAN, pump.cmd, &pump.cmdlen, &indtyp, TIME_FOR_RECV_CMD); 
			if (ret == 0) {
				ret = DoCmd();
				if (ret != 0) {
					RunLog("Node %d �����̨����ʧ��", node);
				}
			}

			DoPolling();
//			Ssleep(100000);       // ������ѯ���Ϊ100ms + INTERVAL
		} else { // ------------- Part.6.2 ��̨��������µĴ������� ------------ 
			Ssleep(100000);  // 100ms, ���ݾ����������
			DoPolling();
		}
	} // end of main endless loop

}



/*
 * func: ��ȡ��ǰ̧����ǹ��ǹ��, ���д��pump.gunLog[] & fp_data[].Log_Noz_State
 * ret : 0 - �ɹ�, -1 - ʧ�� 
 */
static inline int GetCurrentLNZ(int pos)
{
	pump.gunLog[pos] = pump_msg[pos].lnz_state;
	RunLog("Node %d FP %d �� %d ��ņ̀��", node, pos + 1, pump.gunLog[pos]);
	lpDispenser_data->fp_data[pos].Log_Noz_State = ((__u8)1 << (pump_msg[pos].lnz_state - 1));

	return 0;
}


/*
 * Func: ִ����λ������
 * �����ʽ: �������(2Bytes) + FP_Id(1Byte) + ��׼���������"Ack Message"�е�λ��(1Byte) +
 *           �������ݳ���(1Byte) + Ack Message(nBytes)
 */
static int DoCmd()
{
	int ret;
	int fp_id;
	int pos;    //pos is index of  fuelling poit(panel) of a dispenser: 0, 1, 2, 3
	unsigned int cnt;
	__u8 msgbuffer[48];

	if (pump.cmd[0] != 0x2F) {
		// ��ȡ��������ִ�к���ǰ�Ĺ��в���
		fp_id = pump.cmd[2];
		if (fp_id == 0x00) {  // fp_id == 0x00 ��ʾ������fp����, ��Ӧ pos ��Ϊ 255
			pos = 255;
		} else if ((fp_id >= 0x21) && (fp_id <= 0x24)) {
			pos = fp_id - 0x21;
		} else {
			if (pump.cmd[0] != 0x18 && pump.cmd[0] != 0x19) {  // DoPrice & DoMode����Ҫ��������
				RunLog("�Ƿ�������ַ");
				return -1;
			}
		}				

		bzero(msgbuffer, sizeof(msgbuffer));
		memcpy(msgbuffer, &pump.cmd[3], pump.cmd[4] + 2);  // ���β����� Acknowledge Message
	}

	ret = -1;
	switch (pump.cmd[0]) {
		case 0x18:	//  Change Product Price
			ret = DoPrice();		
			break;		
		case 0x19:	//  �����ͻ��Ŀ���ģʽ: ��λ������ �� ��������
//			ret = DoMode();	      // Unsupported
			break;
		case 0x1C:	// Terminate_FP
			// Step.1 �Ƚ��д�����, ��鵱ǰ״̬�Ƿ�������иò���
			ret = HandleStateErr(node, pos, Terminate_FP,
				lpDispenser_data->fp_data[pos].FP_State, msgbuffer); 
			if (ret < 0) {
				break;
			}
			// Step.2 ִ������, ���ɹ��ı� FP ״̬, ���򷵻� NACK
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
			ret = HandleStateErr(node, pos, Do_Auth,     // Note. ������Do_Auth ������ Release_FP 
				lpDispenser_data->fp_data[pos].FP_State, msgbuffer); 
			if (ret < 0) {
				break;
			}

			GetPumpStatus(pos);

			// ���һ����ǹ�Ƿ���̧��״̬
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
			ret = HandleStateErr(node, pos, Do_Ration,   // Note. ������Do_Ration
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
			// �ָ�����������ǹ
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
			RunLog("���ܵ�δ��������[0x%02x]", pump.cmd[0]);
			break;
	}

	return ret;
}

/*
 * func: ��FP����״̬��ѯ, ����״̬����Ӧ����
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
	static int fail_cnt = 0;  // ��¼GetPumpStatus ʧ�ܴ���, �����ж�����
	
	for (i = 0; i < pump.panelCnt; i++) {
		ret = GetPumpStatus(i);
		if (ret < 0) {
			if (gpIfsf_shm->node_online[IFSFpos] == node) {
				if ((++fail_cnt) < 3) {  // ���ͻ������ж�, ������ʱ��Ƚ϶�
					/*
					 * �����贮�����Իᵼ��ReadTTY���յ�"�Ƿ�����", ����ʱ�Ƴ�
					 *
					if (fail_cnt >= 2) {
						if (SetPort007() >= 0) {
							RunLog("�ڵ�%d���贮�����Գɹ�", node);
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
				RunLog("======�ڵ�[%d] ����! ===========================", node);

				// Step.3
				for (j = 0; j < pump.panelCnt; ++j) {
					lpDispenser_data->fp_data[j].FP_State = FP_State_Inoperative;
					lpDispenser_data->fp_data[j].Log_Noz_State = 0;
				}
			}

			// ���贮������
			// ReDoSetPort007();
			continue;
		} else {
			if (gpIfsf_shm->node_online[IFSFpos] == 0) {
				if ((--fail_cnt) > 0) { // �ų�����
					fail_cnt = 1;
					continue;
				} else if (time(NULL) < pump.time_offline) {
					RunLog("====================== wait for 30s, ==============");
					continue;
				}

				// fail_cnt��λ
				fail_cnt = 0;

				for (j = 0; j < pump.panelCnt; ++j) {
					lpDispenser_data->fp_data[j].FP_State = FP_State_Closed;
				}

				for (j = 0; j < pump.panelCnt; ++j) { // Ĭ������������ǹ����
					ret = SetNozMask(j, 0xFF);
//					SetNozMask(j, lpDispenser_data->fp_data[j].Log_Noz_Mask);
					if (ret < 0) {
						return;
					}
				}

				for (j = 0; j < pump.panelCnt; ++j) { // ״̬׼��
					// Step.1 ����PUMP_MSG�ṹ
					ret = GetPumpStatus(j);
					if (ret < 0) {
						RunLog("��ʼ��pump_msgʧ��");
						return;
					}
					if (pump_msg[j].lnz_state != 0) {
						RunLog("��ǹΪ̧��״̬");
						// ��ņ̀��״̬�¶�ȡ������ܲ�׼ȷ, ���Եȴ�...
						return;                // waiting
					}
#if 1
					// Step.2 �жϵ�ǰ״̬�Ƿ���Խ������� DoPolling ����
					// ����Ǽ����л�������Ȩ, �Ǿ͵ȴ�... 
					if (pump_msg[j].status == FP_FUELLING) {
						Ssleep(INTERVAL);
						j = 0;   // ����
					} else if (pump_msg[j].status == FP_AUTHORISED) {
						DoStop(j);
						j = 0;
					}
#endif
				}

				// ȷ����ǹΪ��ǹ״̬�Ŷ�ȡ����
				ret = InitPumpTotal();    // ���±��뵽���ݿ�
				if (ret < 0) {
					RunLog("���±��뵽���ݿ�ʧ��");
					continue;
				}


				// һ��׼������, ����
//				gpIfsf_shm->node_online[IFSFpos] = node;
				ChangeNode_Status(node, NODE_ONLINE);
				bzero(pre_flg,sizeof(pre_flg));
				bzero(pre_ration_type,sizeof(pre_ration_type));
				RunLog("====== Node %d �ָ�����! =========================", node);
				keep_in_touch_xs(12);               // ȷ��CD�Ѿ��յ�����֮��������������
				// Step.                 // �����ϴ���ǰFP״̬
				for (j = 0; j < pump.panelCnt; ++j) {
					ChangeFP_State(node, j + 0x21, FP_State_Closed, NULL);
				}
				continue;
			}
			
			// Note. ����ͻ�û���յ���̨�������������Զ�ת��Ϊ��������ģʽ
			//       ��ô�ָ�ͨѶ���ж�ͨѶʧ��ʱ���Ƿ񳬹��ͻ��ĳ�ʱʱ��ֵ, 
			//       ������������������ģʽ
			//       ## ����һ����������, ��������
			if (fail_cnt >= 1) {
			//  �ϵ�������һ��Ҫ����������ǹ
				for (j = 0; j < pump.panelCnt; ++j) { // Ĭ������������ǹ����
					ret = SetNozMask(j, 0xFF);
//					ret = SetNozMask(j, lpDispenser_data->fp_data[j].Log_Noz_Mask);
					if (ret < 0) {
						return;
					}
				}

			//  �ϵ������ܿ�, һ��Ҫ����״̬
				for (j = 0; j < pump.panelCnt; ++j) {
					// Step.1 ����PUMP_MSG�ṹ
					ret = GetPumpStatus(j);
					if (ret < 0) {
						RunLog("��ʼ��pump_msgʧ��");
						return;
					}
				}
			}
			fail_cnt = 0;
		}

		// ����״̬���б仯��ʱ��Ŵ�ӡ(д��־)
		if (pump_msg[i].status != pre_status[i]) {
			pre_status[i] = pump_msg[i].status;
			RunLog("�ڵ�[%d]���[%d]��ǰ״̬��Ϊ[%02x]", node, i + 1, pump_msg[i].status);
		}

		if (gpIfsf_shm->auth_flag != FCC_SWITCH_AUTH) {  //����к�̨
			if ((__u8)FP_State_Closed == lpDispenser_data->fp_data[i].FP_State) {
				if (i == 0) {
					times += pump.panelCnt;					
				}
				if (times > (SENDSTATE *  pump.panelCnt)) {
					if (i >= pump.panelCnt -1)
						times = 0;
					UserLog("�ڵ�[%d]�����[%d]����Closed״̬", node, i + 1);
				}

				// Closed ״̬��ҲҪ�ܹ����
				ChangePrice();
				continue; 
			}
		} else {
			if ((__u8)FP_State_Closed == lpDispenser_data->fp_data[i].FP_State) {
				RunLog("DoPolling, û�к�̨�ı�FP[%02x]��״̬ΪIDLE", i + 0x21);
				ret = SetHbStatus(node, 0);   // IFSF_HEARTBEAT.status set to 0
				ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
			}
		}
		
		// ��FP״̬ΪInoperative, Ҫ���˹���Ԥ,�����ܸı���״̬,Ҳ�����ڸ�״̬�����κβ���
		if ((__u8)FP_State_Inoperative == lpDispenser_data->fp_data[i].FP_State) {
			RunLog("Node[%d].FP[%d]����Inoperative״̬!", node, i + 1);
			// Inoperative ״̬��ҲҪ�ܹ����
			ChangePrice();
			continue;
		}

		
		switch (pump_msg[i].status) {
		case FP_INOPERATIVE:
			//Modify by liys  ���ͻ��ϵ�ʱΪδ���״̬����Ҫ����һ���·��ͼ�			
			fm_idx = lpDispenser_data->fp_data[i].Default_Fuelling_Mode - 1;
			for (j = 0; j < pump.gunCnt[i]; ++j) {
					pr_no = (lpDispenser_data->fp_ln_data[i][j]).PR_Id - 1;  // 0 - 7	
					if (lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2] != 0x00 ||
						lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3] != 0x00) {
						memcpy(&pump.newPrice[pr_no],
							&(lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price), 4);
					}
			}
			
			ChangePrice();          // δ�����״̬������Ҫ���

			GetPumpStatus(i);
			break;
		case FP_IDLE:                   // ������
			//- �Ƿ� Calling ?
			/*
			RunLog("Idle ---------  [%d].lnz_state: %02x, dostop_flag: %d", 
 						i, pump_msg[i].lnz_state, dostop_flag[i]);
			*/
			if (pump_msg[i].lnz_state != 0 && dostop_flag[i] == 0) {  //Modify by liys
			       
			       if (pre_flg[i] == 1){	
					GetCurrentLNZ(i);
					// �ж��Ƿ�Ԥ�ø�ǹ����, ���������break
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

			// �����ǹ�ǹ״̬�µĴ���
			// ��״̬����IDLE, ���л�ΪIdle 
			if (pump_msg[i].lnz_state == 0) {
				lpDispenser_data->fp_data[i].Log_Noz_State = 0;
			}

			if (((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State) &&
				((lpDispenser_data->fp_data[i].FP_State != FP_State_Authorised) ||
						(pump_msg[i].lnz_state == 0 && pre_flg[i] == 2))) {
				// ����Ѿ��·���Ԥ��,���������ǹ�ǹ��״̬, ��ô����Ԥ���Ѿ���������ȡ��
				// ����ı�״̬ΪIdle
				pre_flg[i] = 0;
				ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
			}

			for (j = 0; j < pump.panelCnt; ++j) {
				if ((__u8)FP_State_Idle != lpDispenser_data->fp_data[j].FP_State) {
					break;
				}
			}
			if (j >= pump.panelCnt) {
				ret = ChangePrice();          // �鿴�Ƿ�����Ʒ��Ҫ���,����ִ��
			}
			break;
		case FP_EOT:                     // �������, ִ��ֹ֮ͣ��Ҳ�Ǵ�״̬, ��������ĳ�ʼ״̬
			pre_flg[i] = 0;

			//- �Ƿ� Calling ?
			if (pump_msg[i].lnz_state != 0) {
				goto DO_CALLING;
			}

			if (pump_msg[i].lnz_state == 0) {
				dostop_flag[i] = 0; // (FP_EOT && lnz_state == 0), ����dostop_flag����������
			} else if (dostop_flag[i] != 0) {
						// ִ����ֹͣ����������, Ҫ�ȹ�ǹ����Ż����, ����..
				break;          // ������ѯ
			}

			//- ���״���
			if (fp_state[i] >= 2) {
				// ���мӹ���, �����״̬�ı�ͽ��״���
				// 1. ChangeFP_State to IDLE first
				//   Log_Noz_State ��ӳ������ǹ״̬(����Ψһ�Ŀ����ǹ�ǹ�ֿ���̧ǹ)
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

			//- �ѻ����״���
			EO_OFFLINE_TR(node, i);
			if (gpIfsf_shm->fp_offline_flag[node - 1][i] != 0) {
				upload_offline_tr(node, NULL);
			}


			//- ��۲���
			for (j = 0; j < pump.panelCnt; ++j) {
				if ((__u8)FP_State_Idle != lpDispenser_data->fp_data[j].FP_State) {
					break;
				}
			}

			if (j >= pump.panelCnt) {
				ret = ChangePrice();          // �鿴�Ƿ�����Ʒ��Ҫ���,����ִ��
			}

			break;

//----------------------------------------------------------------------------------------------//
DO_CALLING:             // ̧ǹ����

			if (((__u8)FP_State_Calling > lpDispenser_data->fp_data[i].FP_State) &&
					(dostop_flag[i] ==0)) {
				// add dostop_flag ��Ҫ�Ǳ�����Ԥ�ƺ�̧��ǹ��֮���·�ֹͣ��������
				//- �����Ǹճ�ʼ�����ǽ��׽�������ִ����ֹͣ����, ����������
				// ���ִ�й�ֹͣ����, ����һ�����ǹ��̧�ŵ�, ��һ���ǹ�ǹ����̧ǹ��

				ret = ReSet(i);
				if (ret < 0 || (ret == 0 && pump_msg[i].lnz_state == 0)) {
					// ����ʧ�ܻ�����ǹ�ѹ�
					break;
				}

				ret = DoCall(i);
				
				fp_state[i] = 1;

				ret = DoStarted(i);
			}

			break;  // DO_CALLING ����
//----------------------------------------------------------------------------------------------//
		case FP_AUTHORISED:
			if (pump_msg[i].lnz_state == 0) {
				ret = DoStop(i);   // �ﵽ̧��ǹȡ����Ȩ��Ч��
				if (ret == 0) {
					lpDispenser_data->fp_data[i].Log_Noz_State = 0;
				}
				break;
			} 
			
			if ((__u8)FP_State_Authorised > lpDispenser_data->fp_data[i].FP_State) {
				ret = ChangeFP_State(node, i + 0x21, FP_State_Authorised, NULL);
			}

			// ����Ȩ״̬��ǹתStarted
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
		case FP_FUELLING:                               // ����
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
 * ����ö��ͻ��Ķ�д��������֮ǰ�ѽ��й�״̬�жϺʹ���Ԥ����,
 * �����ھ��������ִ�к����о���������״̬�����
 * ��Ϊ�е��ͻ�ģ����ÿ���·�����֮ǰ����״̬�жϵĲ���, �ش�˵��
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
		RunLog("����״̬�ı����");
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
	//if ((lpNode_channel->cntOfFpNoz[pos] == 1)||pre_flg[pos] == 1){ //���Ϊ�����ǹ������Ļ�������̧ǹ��ʱ����Ҫ�ٴ��·�DoRation
	if (pre_flg[pos] == 1) { //���ݳ�����Ա����,�������ͺŵ���ǹԤ�����̶�Ϊ̧ǹ����Ԥ����Ȩ
		ration_type = pre_ration_type[pos];
		if (ration_type == 0x03) {
			memcpy(&pre_volume[1], &lpDispenser_data->fp_data[pos].Remote_Volume_Preset[2], 3);
		} else {
			memcpy(&pre_volume[1], &lpDispenser_data->fp_data[pos].Remote_Amount_Prepay[2], 3);
		}

		// Setp.0 ȷ������, ����Ԥ�ò��ɹ�(������ִ��DoAuth��ʱ�������, ��Ϊ��ʱ�������ȡ��Ԥ����)
		if (pump_msg[pos].status != FP_IDLE ) {
			if (ReSet(pos) < 0) {
				return -1;
			}
		}

		// Step.1  ��������ǹ��, �ⲽһ��Ҫ���ж��� pump.cmd[0] ֮�����
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
				pump.cmd[idx++] = 0x10;   // ���������� DEL(0x10)
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
				RunLog("��Ԥ�ü������Node[%d].FP[%d]ʧ��", node, pos + 1);
				continue;
			}

			RunLog("Node[%d].FP[%d].Pre_xxx:%s", node, pos + 1, Asc2Hex(pump.cmd, pump.cmdlen));

			memset(pump.cmd, 0, sizeof(pump.cmd));
			pump.cmdlen = pump.wantLen;
			ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
			if (ret < 0) {
				RunLog("��Ԥ�ü������Node[%d].FP[%d]�ɹ�, ȡ������Ϣʧ��", node, pos + 1);
				continue;
			} 

			// �����Ƿ��� NAK ���� TX# ����, ������
			if (pump.cmd[1] == (_NAK | tx_no[pos])) {
				tx_no[pos] = 0x00;   // ����ָ�
				continue;
			} else if ((pump.cmd[1] & 0x0F) != tx_no[pos]) {
				tx_no[pos] = 0x00;   // ����ָ�
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

		// Setp.0 ȷ������, ����Ԥ�ò��ɹ�(������ִ��DoAuth��ʱ�������, ��Ϊ��ʱ�������ȡ��Ԥ����)
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
 * func: ������Ʒ����, ����newPrice[][], ��IDLE״ִ̬�б��
 */
static int DoPrice()
{
	__u8 pr_no;
	
	pr_no  = pump.cmd[2] - 0x41; //��Ʒ��ַ����Ʒ���0,1,...7.
	if ((0x04 != pump.cmd[8]) || (0x00 != pump.cmd[9])) {
		RunLog("���۴���,�����·�");
		return -1;
	}
	
	if ((pump.newPrice[pr_no][2] | pump.newPrice[pr_no][3]) == 0) {
		// �ϴ�ִ�б��(ChangePrice)���һ���յ�WriteFM�����ı������
		pump.newPrice[pr_no][0] = 1;
	} else {
		pump.newPrice[pr_no][0]++;
	}

	// �����Ƿ��������յ�ͬ����Ʒ�ı������, Ҳ���ܵ����Ƿ�һ��, ��ʹ���µ��۸����ϵ���
	pump.newPrice[pr_no][2] = pump.cmd[10];
	pump.newPrice[pr_no][3] = pump.cmd[11];
#ifdef DEBUG
	RunLog("Recv New Price-PR_ID[%d]TIMES[%d] %02x.%02x",
		pr_no + 1, pump.newPrice[pr_no][0], pump.newPrice[pr_no][2], pump.newPrice[pr_no][3]);
#endif

	return 0;
}


/*
 * func: ִ�б��, ʧ�ܸı�FP״̬ΪINOPERATIVE, ��д�������ݿ�
 *       ���ȵı�����ͬ�������ͻ�Э��, ��ChangePrice������ģ���нϴ���
 */
static int ChangePrice() 
{
	int i, j;
	int ret;
	int trycnt;
	int pr_no;
	int fm_idx;
	int need_change = 0;          // ��ʶ������������Ʒ���Ƿ�����Ҫ��۵�
	int error_code_msg_cnt = 0;   // ��Ҫ������Fuel��error code��Ϣ������;
	__u8 lnz;
	__u8 err_code = 0;
	__u16 crc;

/*
 * �����µ�������newPrice[FP_PR_NB_CNT], �������Ʒ��Ҫ���--����ֵ��Ϊ��,
 * ���������FP, �����Ʒƥ����·��������
 */

	for (pr_no = 0; pr_no < FP_PR_NB_CNT; pr_no++) {
		if (pump.newPrice[pr_no][2] != 0x00 || pump.newPrice[pr_no][3] != 0x00) {
			RunLog(">>>1 newPrice[%d]: %02x.%02x", pr_no,
					pump.newPrice[pr_no][2], pump.newPrice[pr_no][3]);
			for (i = 0; i < pump.panelCnt; i++) {
				CriticalLog("�·����������ڵ�[%02d]��[%d]ǹ[*]......", node, i + 1);
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
						RunLog("FP %d ������", i + 1);
						tx_no[i]--;  // Note! ����1�ᵼ��TX������, ���±��ʧ��;
						break;       // ������һ����
					}

					pump.cmd[3] = pump.gunCnt[i] * 3;
					j = 4 + pump.gunCnt[i] * 3;
					crc = CRC_16(&pump.cmd[0], j);
					if ((crc & 0x00FF) == 0xFA) {
						pump.cmd[j++] = 0x10;  // ���������� DEL
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
						RunLog("��������Node[%d].FP[%d]ʧ��", node, i + 1);
						continue;
					}

					memset(pump.cmd, 0, sizeof(pump.cmd));
					pump.cmdlen = sizeof(pump.cmd) - 1;
					ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
					if (ret < 0) {
						RunLog("��������Node[%d].FP[%d]�ɹ�, ȡ������Ϣʧ��",
												node, i + 1);
						Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
						continue;
					} 

					// �����Ƿ��� NAK ���� TX# ����, ������
					if (pump.cmd[1] == (_NAK | tx_no[i])) {
						tx_no[i] = 0x00;
						Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
						continue;
					} else if ((pump.cmd[1] & 0x0F) != tx_no[i]) {
						tx_no[i] = 0x00;   // ����ָ�
						Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
						continue;
					}


					// Step.2 �����������Ϣ�����ض��ͻ�����ȷ�ϱ���Ƿ�ɹ�
					
					// Step.3 д�µ��۵���Ʒ���ݿ� & ���㵥�ۻ���
					for (j = 0; j < pump.gunCnt[i]; ++j) {
						pr_no = (lpDispenser_data->fp_ln_data[i][j]).PR_Id - 1;  // 0 - 7	
						if (pump.newPrice[pr_no][2] != 0x00 || pump.newPrice[pr_no][3] != 0x00) {

					RunLog("Node[%d]���ͱ������ɹ�,�µ���fp_fm_data[%d][%d].Prode_Price: %s",
						node, pr_no, fm_idx,
						Asc2Hex(lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price, 4));
					CriticalLog("�ڵ�[%02d]��[%d]ǹ[%d]��۳ɹ�,�µ���Ϊ%02x.%02x",
					       	node, i + 1, j + 1,
						lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2],
						lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3]);
						}
					}

					break;
				}

				// Step.4 �����ж�, ��������д�������ݿ�/��FP״̬Ϊ Inoperative
				if (trycnt >= TRY_TIME) {
					// Write Error Code: 0x36/0x37 - Price Change Error (Spare)
					if (0x37 != err_code) {
						err_code = 0x36;    // ͨѶʧ��
			CriticalLog("�ڵ�[%02d]��[%d]ǹ[x]���ʧ��[ͨѶ����],�ı���״̬Ϊ[������]",
										node, i + 1);
					} else {
			CriticalLog("�ڵ�[%02d]��[%d]ǹ[x]���ʧ��[�ͻ�ִ��ʧ��],�ı���״̬Ϊ[������]",
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
					ChangeER_DATA(node, 0x21 + i, err_code, NULL); // Translater�Ѿ��ظ���ACK
				}

				if (err_code != 0x99) {
					ChangeFP_State(node, 0x21 + i, FP_State_Inoperative, NULL);
				}

				Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
			} // end of for.i < panelCnt
			bzero(&pump.newPrice, FP_PR_NB_CNT * 4);   // Note. ����ȫ��
		} // end of if.newPrice
	} // end of for.pr_no

	return 0;
}


/*
 * func: ���ͽ��������н��׼�¼�Ķ�ȡ�ʹ���
 *       �㽻�׵��жϺʹ�����DoEot����, DoPoling ������
 * ret : 0 - �ɹ�; -1 - ʧ��
 *
 */
/*
 * �޸Ľ���&ʵ��˵��:  ��Ҫ���ͻ���ȡ���������� ���μ����� & ���ͽ�� &
 *                     ���� & �������� [& �������] , �ֱ������Ӧ�����ݿ���
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

	RunLog("�ڵ�[%d]���[%d]�����µĽ���", node, pos + 1);

	err_cnt = 0;
	do {
		ret = GetGunTotal(pos, pump.gunLog[pos], vol_total, amo_total);
		if (ret < 0) {
			err_cnt++;
			RunLog("�ڵ�[%d]���[%d]��ȡ���ۼ�¼ʧ��", node, pos + 1);
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
				RunLog("�ڵ�[%d]���[%d]��ȡ���׼�¼ʧ��", node, pos + 1);
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

		// ���� Volume_Total
		memcpy(lpDispenser_data->fp_m_data[pr_no].Meter_Total, total, 7);
		memcpy((lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Vol_Total, total, 7);

#if 0
		RunLog("�ڵ�[%02d]���[%d]ǹ[%d] ## Amount: %02x%02x.%02x Volume: %02x%02x.%02x Price: %02x.%02x Total: %02x%02x%02x.%02x", node, pos + 1, pump.gunLog[pos], amount[2], 
					amount[3], amount[4], volume[2], volume[3], volume[4], price[2],
							price[3], total[3], total[4], total[5], total[6]);
#endif

		// ���� Amount_Total
		memcpy(&total[2], amo_total, 5);
		memcpy((lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Amount_Total, total, 7);
		memcpy(lpDispenser_data->fp_data[pos].Current_Prod_Nb, lpDispenser_data->fp_pr_data[pr_no].Prod_Nb, 4);

		err_cnt = 0;
		do {
			ret = EndTr4Ln(node, pos + 0x21, pump.gunLog[pos] + 0x10);
			if (ret < 0) {
				err_cnt++;
				RunLog("DoEot.EndTr4Ln ʧ��, ret: %d <ERR>", ret);
			}
		} while (ret < 0 && err_cnt <= 5);

		err_cnt = 0;
		do {
			ret = EndTr4Fp(node, pos + 0x21, pre_total,
				(lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Vol_Total);
			if (ret < 0) {
				err_cnt++;
				RunLog("DoEot.EndTr4Fpʧ��, ret: %d <ERR>", ret);
			}
		} while (ret < 0 && err_cnt <= 8);

	} else {
		ret = EndZeroTr4Fp(node, pos + 0x21);
		if (ret < 0) {
			RunLog("DoEot.EndZeroTr4Fpʧ��, ret: %d <ERR>", ret);
		}
	}

	return ret;
}



/*
 * func:��������ǹ��
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

		// ǹ��
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


		pump.cmd[3] = i;          // ��ǹ����
		i += 4;
		crc = CRC_16(&pump.cmd[0], i);
		if ((crc & 0x00FF) == 0xFA) {
			pump.cmd[i++] = 0x10;       // ���������� DEL
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
			RunLog("��������ǹ�������Node[%d].FP[%d]ʧ��", node, pos + 1);
			continue;
		}

		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("��������ǹ�������Node[%d].FP[%d]�ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		} 

		// �����Ƿ��� NAK ���� TX# ����, ������
		if (pump.cmd[1] == (_NAK | tx_no[pos])) {
			tx_no[pos] = 0x00;   // ����ָ�
			continue;
		} else if ((pump.cmd[1] & 0x0F) != tx_no[pos]) {
			tx_no[pos] = 0x00;   // ����ָ�
			continue;
		}

		return 0;
	}
	return -1;
}


/*
 * func: ��ȡ��FP����ǹ�ı���
 */
static int GetPumpTotal(int pos)
{
	return 0;
}

/*
 * func: ��ȡ���ν��׼�¼
 * return: 0 - �ɹ�, 1 - �ɹ����޽���, -1 - ʧ��
 */ 
static int GetTransData(int pos)
{
	int idx,ret;
	int trycnt;
	__u16 crc = 0;

	PLUS_TX_NO(pos);
	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {
		// Step.1 ���󷵻ؼ�����Ϣ
		pump.cmd[0] = pump.panelPhy[pos];   // ADR
		pump.cmd[1] = 0x30 | tx_no[pos];    // CTRL
		pump.cmd[2] = 0x01;   // TRANS
		pump.cmd[3] = 0x01;   // LNG
		pump.cmd[4] = 0x04;   // DCC
		crc = CRC_16(&pump.cmd[0], 5);
		idx = 5;
		if ((crc & 0x00FF) == 0xFA) {
			pump.cmd[idx++] = 0x10;   // ���������� DEL(0x10)
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
			RunLog("��ȡ�������Node[%d].FP[%d]ʧ��", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("��ȡ�������Node[%d].FP[%d]�ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		} 

		// �����Ƿ��� NAK ���� TX# ����, ������
		if (pump.cmd[1] == (_NAK | tx_no[pos])) {
			tx_no[pos] = 0x00;   // ����ָ�
			continue;
		} else if ((pump.cmd[1] & 0x0F) != tx_no[pos]) {
			tx_no[pos] = 0x00;   // ����ָ�
			continue;
		}

		// Step.2 ��ѯ���ռ�����Ϣ
		ret = GetPumpMsg(pos);
		RunLog("Node[%d].FP[%d] Get Trans Data, ret: %d", node, pos + 1, ret);
		if (ret < 0 || (ret & 0x02) == 0) {
			continue;
		}

		// ������Ϣ�洢��pump_msg[].current_xxx

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
			pump.cmd[idx++] = 0x10;   // ���������� DEL(0x10)
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
			RunLog("��ֹͣ���Node[%d].FP[%d]ʧ��", node, pos + 1);
			continue;
		}

		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("��ֹͣ���Node[%d].FP[%d]�ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		} 

		// �����Ƿ��� NAK ���� TX# ����, ������
		if (pump.cmd[1] == (_NAK | tx_no[pos])) {
			tx_no[pos] = 0x00;   // ����ָ�
			continue;
		} else if ((pump.cmd[1] & 0x0F) != tx_no[pos]) {
			tx_no[pos] = 0x00;   // ����ָ�
			continue;
		}

		// �鿴״̬�Ƿ�仯
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

	// Step.1 ȷ���Ѿ���������״̬
	// Ԥ�ü���DoRation֮ǰһ��Ҫ�Ƚ�������, ��������ִ������ᵼ��Ԥ������ȡ��
	if (pump_msg[pos].status != FP_IDLE) {
		if (ReSet(pos) < 0) {
			return -1;
		}
	}

	// Setp.2 �·���Ȩ
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
			pump.cmd[idx++] = 0x10;   // ���������� DEL(0x10)
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
			RunLog("����Ȩ���Node[%d].FP[%d]ʧ��", node, pos + 1);
			continue;
		}

		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("����Ȩ���Node[%d].FP[%d]�ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		} 

		// �����Ƿ��� NAK ���� TX# ����, ������
		if (pump.cmd[1] == (_NAK | tx_no[pos])) {
			tx_no[pos] = 0x00;   // ����ָ�
			continue;
		} else if ((pump.cmd[1] & 0x0F) != tx_no[pos]) {
			tx_no[pos] = 0x00;   // ����ָ�
			continue;
		}

		/*
		 * 20ϵ���ͻ���ת��Ϊ����Ȩ״̬��Ҫ�ܳ�ʱ��,
		 * �����ڴ˲��ʺ���״̬�ж�--���׵���FCC��CD�ķ���ʱ�����
		 */

		return 0;
	}

	return -1;
}

/*
 * func: ��ȡ���ϴ�ʵʱ��������
 */
static int DoBusy(int pos)
{
	int ret;
	static __u8 volume[5], amount[5];
	const __u8 arr_zero[4] = {0};

	// ������Ϊ�㲻ת��ΪFuelling״̬ , ���ϴ�Run_Trans
	if (fp_state[pos] != 3) {
		if (memcmp(arr_zero, pump_msg[pos].current_vol, 4) == 0) {
			return 0;
		}
		fp_state[pos] = 3;
		return 0;
	}

	//  ȡ��ʵʱ������ ........
	
	volume[0] = 0x06;
	memcpy(&volume[1], pump_msg[pos].current_vol, 4);

	amount[0] = 0x06;
	memcpy(&amount[1], pump_msg[pos].current_amo, 4);

	if ((__u8)FP_State_Fuelling == lpDispenser_data->fp_data[pos].FP_State) {
		//ֻ��һ�����Modify by liys 09.01.12 0x21 --> pos + 0x21
		ret = ChangeFP_Run_Trans(node, pos + 0x21, amount,  volume); 
	}

	return 0;
}

/*
 * func: ̧ǹ���� 
 * ��ȡ��ǰ̧����߼�ǹ��,����fp_data[].Log_Noz_State & �ı�FP״̬ΪCalling
 */
static int DoCall(int pos)
{
	int ret;

	ret = GetCurrentLNZ(pos);
	if (ret < 0) {
		RunLog("��ȡ��ǰ��ǹǹ��ʧ��");
		return -1;
	}

	ret = ChangeFP_State(node, pos + 0x21, FP_State_Calling, NULL);

	//��λ������,���������Զ���Ȩ;��λ��������, ��Ȩ�������ڿ���״̬,��ʱ�Զ���Ȩ
	if ((gpIfsf_shm->auth_flag == FCC_SWITCH_AUTH) && (lock_status() == 1)) {
		ret = DoAuth(pos);
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d]�Զ���Ȩʧ��", node, pos + 1);
		}
		ret = ChangeFP_State(node, pos + 0x21, FP_State_Started, NULL);
		RunLog("�ڵ�[%d]���[%d]�Զ���Ȩ�ɹ�", node, pos + 1);
		SO_OFFLINE_TR(node, pos);
	}

	return ret;
}


/*
 * func: �ͻ���Ϊ����״̬, �ı�FP״̬ΪStarted
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
			RunLog("�ڵ�[%d]���[%d],BeingTr4FP Failed", node, i + 1);
			return -1;
		}
	}

	//  Log_Noz_State & BeginTr4Fp �ɹ�, ����λ fp_state[i] Ϊ 2
	fp_state[i] = 2;

	return 0;
}

/*
 * func:  ����ȷ����Ϣ ACK/NAK
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
			RunLog("��ACK/NAK��Node[%d]ʧ��", node);
			continue;
		}

		return 0;
	}

	return -1;
}

/*
 * func: ��ѯ��������ѯ�õ�������, ���� pump_msg �ṹ,
 *       �õ���ͬ��Ϣ����ֵ��ͬ�Ա�������ж��Ƿ��������������;
 * return: -1 - �޷�����Ϣ;
 *          0 - ����3�ֽ�(ACK/NAK/EOT);
 *          Bit1 Ϊ1 - ������״̬;
 *          Bit2 Ϊ1 - ������������(ʵʱor����);
 *          Bit3 Ϊ1 - ������ǹ״̬;
 *          Bit4 Ϊ1 - ��������;
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
		case 0x01:        // DC1, �ͻ�״̬
			ret |= 0x01;
			pump_msg[pos].status = p[2];
			break;
		case 0x02:        // DC2, ʵʱ������Ϣ
			ret |= 0x02;
			memcpy(pump_msg[pos].current_vol, &p[2], 4);
			memcpy(pump_msg[pos].current_amo, &p[6], 4);
			break;
		case 0x03:        // DC3, ��ǹ״̬�͵���
			ret |= 0x04;
			memcpy(pump_msg[pos].current_pri, &p[2], 3);
			lnz_state = (p[5] & 0x0F);    // ̧��״̬
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
				dostop_flag[pos] = 0;  // ��ǹ״̬�仯��ʱ��Ż�ִ�е�����,
					               // ���Բ��ܱ�֤��lnz_state��ǹ=���ŵ�ʱ������Ϊ0
			}
			break;
		case 0x65:
			ret |= 0x08;
			RunLog("Node %d FP %d ȡ����ɹ� ....", node , pos + 1);
			// Note. �����ͻ������ָ�ʽ�ı�������, ���Ȳ�һ��, ������
			if (p[1] == 13) {  // ���ڿ�����ʽ, �н������
				// p + 4, p + 10: ��������ֽ�
				memcpy(pump_msg[pos].vol_total, p + 4, GUN_EXTEND_LEN);
				memcpy(pump_msg[pos].amo_total, p + 10, GUN_EXTEND_LEN);
			} else {
				// p[1] == 16, ԭװ����ʽ, �޽������
				memcpy(pump_msg[pos].vol_total, p + 3, GUN_EXTEND_LEN);
				bzero(pump_msg[pos].amo_total, GUN_EXTEND_LEN);
			}

			break;
		default:
			break;
		}

		p = p + p[1] + 2;  // ��һ�����ݿ�, 2 Ϊp[0]p[1]�ĳ���

	} while (p[2] != 0x03 || p[3] != 0xFA);

	return ret;
}

/*
 * func: һ����ѯ
 *       ����������Ϊ ACK/NAK/EOT �򷵻�0, �������������, ��pump.cmdlen����3, �򷵻� 1
 */

static int poll(int pos)
{
	int ret;
	int trycnt;
	int ackpoll_flag = 0; // ����Ƿ���Ҫ�ظ�ACKPOLL
	__u16 crc;

	for (trycnt = 0; trycnt < 2; ++trycnt) {
		// Step.1 send command
		pump.cmd[0] = pump.panelPhy[pos];   // ADR
		pump.cmd[1] = 0x20;                 // CTRL
		pump.cmd[2] = 0xFA;                 // SF
		pump.cmdlen = 3;
		pump.wantLen = 3;                   // �䳤

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("��ȡ��ѯ���Node[%d].FP[%d]ʧ��", node, pos + 1);
			return -1;                  // ȡ������Ϣʧ����GetPumpMsg�ĵ����߸�������
		}

RETRY:
		if (trycnt > 3) {
			// �������3��
			return -1;
		}
		// Step.2  receive data
		pump.cmdlen = sizeof(pump.cmd) -  1;
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x07;
		gpGunShm->addInf[pump.chnLog - 1][1] = 0x00;  // spec.c��ʹ��, ��ʶ���ݽ��ܽ���
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x00;
		if (ret < 0) {
			RunLog("��ȡ��ѯ���Node[%d].FP[%d]�ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
			/*
			 * �ͻ��ڼ��͹����л���״̬�仯��ʱ��poll�����޷��ص����,
			 * ����pollһ�β��ܵõ���Ϣ
			 */
		} 

		if (pump.cmdlen == 3) {
			if (pump.cmd[2] != 0xfa) {
				return -1;    // ��ֹ�������ݸ���
			}
			// EOT ����ظ�
			return 0;
		}

		crc = CRC_16(&pump.cmd[0], pump.cmdlen - 4);
		if ((__u8)(crc & 0x00FF) != pump.cmd[pump.cmdlen - 4] ||
			(__u8)(crc >> 8) != pump.cmd[pump.cmdlen - 3]) {
			RunLog("Node[%d].FP[%d]��������У�����", node, pos + 1);
			if (AckPoll(pos, _NAK, pump.cmd[1] & 0x0F) < 0) {   // Ҫ���ͻ��ش�
				continue;
			}
			ackpoll_flag = 1;
			++trycnt;
			goto RETRY;                    // ���½���
		}

		if (ackpoll_flag == 0) {
			AckPoll(pos, _ACK, pump.cmd[1] & 0x0F);            // Ҫ�ظ� ACK, �����´λ��ش�
		} else {
			AckPoll(pos, _ACKPOLL, pump.cmd[1] & 0x0F);        // Ҫ�ظ� ACKPOLL, �����´λ��ش�
		}

		ackpoll_flag = 0;
		return 1;
	}

	return -1;
}

/*
 * func: ��ȡʵʱ״̬ (ʧ��ֻ����һ��)
 *       ����û���յ��ظ���Ϣʱ�ŷ���-1;
 *       ������ϢΪNAK���������1, ����Ӱ�������жϵ�׼ȷ��
 *
 */
static int GetPumpStatus(int pos)
{
	int idx,ret;
	int trycnt;
	int no_response_cnt = 0;    // û���յ��ظ���Ϣ����
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
			pump.cmd[idx++] = 0x10;   // ���������� DEL(0x10)
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
			RunLog("��ȡ״̬���Node[%d].FP[%d]ʧ��", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("��ȡ״̬���Node[%d].FP[%d]�ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			no_response_cnt++;
			continue;
		} 

		// �����Ƿ��� NAK ���� TX# ����, ������
		if (pump.cmd[1] == (_NAK | tx_no[pos])) {
			no_response_cnt = 0;
			tx_no[pos] = 0x00;
			continue;
		} else if ((pump.cmd[1] & 0x0F) != tx_no[pos]) {
			no_response_cnt = 0;
			tx_no[pos] = 0x00;   // ����ָ�
			continue;
		}

		// GetPumpOnline & GetPumpStatus���ؽ������һ������3, �������
		// �����ж�����Ϊ <= 0
		ret = GetPumpMsg(pos);
		if (ret < 0) {                      // �޷��ؽ��
			no_response_cnt++;
			continue;
		} else if ((ret & 0x01) != 0x01) {  // ���ؽ���в�����״̬
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
		return 1;         // �лظ�,��NAK
	} else {
		return -1;
	}
}


//test wether the dispenser is online, we only use fp 1 to test.
/*
 * func: �ж�Dispenser�Ƿ�����
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
			pump.cmd[idx++] = 0x10;   // ���������� DEL(0x10)
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
			RunLog("��ȡ״̬���Node[%d].FP[%d]ʧ��", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("��ȡ״̬���Node[%d].FP[%d]�ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		} 

		// �����Ƿ��� NAK ���� TX# ����, ������
		if ((pump.cmd[1] & 0x0F) == _NAK) {
			tx_no[pos] = 0x00;   // ����ָ�
			continue;
		} else if ((pump.cmd[1] & 0x0F) != tx_no[pos]) {
			tx_no[pos] = 0x00;   // ����ָ�
			continue;
		}

		// GetPumpOnline & GetPumpStatus���ؽ������һ������3, �������
		// �����ж�����Ϊ <= 0
		if (GetPumpMsg(pos) <= 0) {
			continue;
		}

		return 0;
	}

	return -1;
}

/*
 * func: ��ȡĳ��ǹ����
 *       ���Ʒ��صı���������������ͬ�ĸ�ʽ���ȷֱ�Ϊ5��6, ע������Դ�
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
			RunLog("��ȡ�������Node[%d].FP[%d]ʧ��", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("��ȡ�������Node[%d].FP[%d]�ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		} 

		// �����Ƿ��� NAK ���� TX# ����, ������
		if (pump.cmd[1] == (_NAK | tx_no[pos])) {
			tx_no[pos] = 0x00;   // ����ָ�
			continue;
		} else if ((pump.cmd[1] & 0x0F) != tx_no[pos]) {
			tx_no[pos] = 0x00;   // ����ָ�
			continue;
		}

		ret = GetPumpMsg(pos);
		if (ret < 0) {
			RunLog("��ȡNode[%d].FP[%d].LNZ[%d]�ı���ʧ��", node, pos + 1, gunLog);
			continue;
		} else if (ret < 0x08) {
			RunLog("��ȡNode[%d].FP[%d].LNZ[%d]�ı���ʧ��", node, pos + 1, gunLog);
			// ���������Ҫ����TX, �����ʹ�ø�λTX
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
 * func: ����
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
			pump.cmd[idx++] = 0x10;   // ���������� DEL(0x10)
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
			RunLog("���������Node[%d].FP[%d]ʧ��", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("���������Node[%d].FP[%d]�ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		} 

		// �����Ƿ��� NAK ���� TX# ����, ������
		if (pump.cmd[1] == (_NAK | tx_no[pos])) {
			tx_no[pos] = 0x00;   // ����ָ�
			continue;
		} else if ((pump.cmd[1] & 0x0F) != tx_no[pos]) {
			tx_no[pos] = 0x00;   // ����ָ�
			continue;
		}

		if (GetPumpMsg(pos) < 0) {
			continue;
		}

		if (pump_msg[pos].status != FP_IDLE) {  // ����û�гɹ�
			PLUS_TX_NO(pos);
			continue;
		}

		return 0;
	}

	return -1;
}

/*
 * func: �ػ�
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
		pump.cmd[5] = crc & 0x00FF;         // У��ֵ������� SF (0xFA)
		pump.cmd[6] = crc >> 8;
		pump.cmd[7] = 0x03;
		pump.cmd[8] = 0xfa;
		pump.cmdlen = 9;
		pump.wantLen = 3;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("���ػ����Node[%d].FP[%d]ʧ��", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("���ػ����Node[%d].FP[%d]�ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		} 

		// �����Ƿ��� NAK ���� TX# ����, ������
		if (pump.cmd[1] == (_NAK | tx_no[pos])) {
			tx_no[pos] = 0x00;   // ����ָ�
			continue;
		} else if ((pump.cmd[1] & 0x0F) != tx_no[pos]) {
			tx_no[pos] = 0x00;   // ����ָ�
			continue;
		}

		if (GetPumpMsg(pos) < 0) {
			continue;
		}

		if (pump_msg[pos].status != FP_CLOSED) {  // �ػ�û�гɹ�
			PLUS_TX_NO(pos);
			continue;
		}

		return 0;
	}

	return -1;
}

/*
 * func: ֹͣ
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
		pump.cmd[5] = crc & 0x00FF;         // У��ֵ������� SF (0xFA)
		pump.cmd[6] = crc >> 8;
		pump.cmd[7] = 0x03;
		pump.cmd[8] = 0xfa;
		pump.cmdlen = 9;
		pump.wantLen = 3;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("���ػ����Node[%d].FP[%d]ʧ��", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("���ػ����Node[%d].FP[%d]�ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		} 

		// �����Ƿ��� NAK ���� TX# ����, ������
		if (pump.cmd[1] == (_NAK | tx_no[pos])) {
			tx_no[pos] = 0x00;   // ����ָ�
			continue;
		} else if ((pump.cmd[1] & 0x0F) != tx_no[pos]) {
			tx_no[pos] = 0x00;   // ����ָ�
			continue;
		}

		if (GetPumpMsg(pos) < 0) {
			continue;
		}
/*
		if (pump_msg[pos].status != FP_CLOSED) {  // �ػ�û�гɹ�
			PLUS_TX_NO(pos);
			continue;
		}
*/
		return 0;
	}

	return -1;
}



/*
 * ��ʼ��FP�ĵ�ǰ�ͼ�. �������ļ��ж�ȡ��������,
 * д�� fp_data[].Current_Unit_Price
 * ���� PR_Id �� PR_Nb ��һһ��Ӧ��
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
 * func: ��ʼ����ǹ����
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
				RunLog("ȡ Node %d FP %d LNZ %d �ı���ʧ��", node, i, j);
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
			RunLog("��[%d]�߼�ǹ��[%d]�ı���Ϊ[%s]", i + 1, 
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
 * func: ���ÿ���ģʽ, ���Ʋ���Ҫ����, һ���к�̨���ת��Ϊ����
 *
 */



/*
 * func: ���ͻ�����ͨѶ x��, �˽��ڵȴ��ڼ��ͻ���״̬, ����ĳЩ�ͻ����б�������ģʽ������
 * return: GetPumpStatusʧ�ܵĴ���
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
 * func: �趨�������ԡ�ͨѶ���ʡ�У��λ��
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
		RunLog("����ͨ�� %d ��������ʧ��", pump.chnLog);
		return -1;
	}

	gpGunShm->chnLogSet[pump.chnLog - 1] = 1;
	Ssleep(INTERVAL);

	return 0;
}

/*
 * func:  ͨѶ��ͨʱ��ѯ�����и�һ��ʱ������һ�´�������
 *        ����������ĵ�ѹ���ȵ��µ�Ƭ����λ--�����������ûָ�Ԥ��9600,8,n,1
 */
static int ReDoSetPort007()
{
#define RE_DO_FREQUENCY   10         //  10����ѯʧ������һ�´�������

	int static polling_fail_cnt = 0;
	int ret;

	if ((++polling_fail_cnt) >= RE_DO_FREQUENCY) {
		ret = SetPort007();
		if (ret < 0) {
			RunLog("�ڵ� %d ���贮������ʧ��", node);
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
	
	// ״̬׼��
	for (i = 0; i < fp_cnt; i++) {
		lpDispenser_data->fp_data[i].FP_State = FP_State_Closed;
	}

//	gpIfsf_shm->node_online[IFSFpos] = node;  // һ�о���, ����
	ChangeNode_Status(node, NODE_ONLINE);
	RunLog("�ڵ� %d ����!", node);
	keep_in_touch_xs(12);               // ȷ��CD�Ѿ��յ�����֮��������������

	// when dispenser is initialized , the status is Closed. it's waiting for set command from CD. 
	// Retalix Ҫ������ʱ�����ϴ���ǰ״̬
	for (i = 0; i < fp_cnt; i++) {
		if ((__u8)FP_State_Closed != lpDispenser_data->fp_data[i].FP_State) {
			ChangeFP_State(node, 0x21 + i, FP_State_Closed, NULL);
		}
	}
}

/*
 * func: �趨���ڲ����ж�����RCAP2L
 *
 *       ������ǹ����Ҫ�ϳ��Ĵ��ڲ�������, ����Ӧ�ԱȽϲ�,
 *       �������ϴ��ֻ�����Ҫ��̬�޸ĵ�Ƭ�������ж�����
 *
 * Note: �˹���Ҫ��IO����汾���ڵ���1006
 *
 * return:
 *       0 - �ɹ�; -1 - ����������������; -2 - δ�ҵ�����ֵ����ʧ��;
 */
static int SetRCAP2L()
{
	int i, ret;
	int fail_cnt;
	int test_cnt;
	__u8 rcap2l = 0x80;   // IO������趨��Ĭ��ֵ

	if (gpGunShm->wayne_rcap2l_flag == 3) {
		/* 
		 * �˰汾IO����֧�� 1c45 ���������,
		 * ���·�1c45�ᵼ��IO����, ����ֱ�ӷ���.
		 */
		return 0;
	}

	// Step.0 ȷ���Ѿ��ܹ�ͨѶ
	if (GetPumpOnline(0) < 0) {
		return -1;
	}
	// Setp.1 ���ò���ֻ��������һ�����̽���
	
	RunLog("------ ��ʼ����RCAP2L����, rcap2l_flag[%02x] -------------", gpGunShm->wayne_rcap2l_flag);

	if (gpGunShm->wayne_rcap2l_flag == 2) {
		RunLog("------- RCAP2L �Ѿ�����, ֱ�ӷ��� --------------");
		return 0;
	} else if (gpGunShm->wayne_rcap2l_flag == 1) {
		RunLog("------- ���������������� RCAP2L, �ȴ�... -------------");
		sleep(15);

		if (gpGunShm->wayne_rcap2l_flag == 2) {
			return 0;
		} 
		return -1;
	} 

	Act_P(WAYNE_RCAP2L_SEM_INDEX);
	gpGunShm->wayne_rcap2l_flag = 1;    // @1 ��ʶ��ʼ����
	Act_V(WAYNE_RCAP2L_SEM_INDEX);
	RemoveSem(WAYNE_RCAP2L_SEM_INDEX);

	RunLog("------ SetRCAP2L flag: %02x ------------------", gpGunShm->wayne_rcap2l_flag);

	/*
	 * Step.2 ͨ����ȡ���ۺ�����������ǹ�����ҳ����ʵ�rcap2lֵ, ������
	 * ����(����)��ʽ:
	 * 	�����ȡ���ۻ�������������ǹ���κ�һ��ʧ��,
	 * 	�����rcap2l(����0x65)���²���,
	 * 	ֱ����ȡ����15�ξ��ɹ�,����������ǹ12�ξ��ɹ�
	 */

	do {
		// 1. 
		fail_cnt = 2;   // ʧ�����ξ͵���rcap2lֵ���²���.

		if (TestRCAP2L_GetTotal() < 0) {
			fail_cnt = 0;
		}

		if (TestRCAP2L_SetNozMask() < 0) {
			fail_cnt = 0;
		}

		// 2.
		if (fail_cnt == 2) {  
		   	// û��ʧ��
			RunLog("------ ���RCAP2L����,��ǰֵΪ[%02x]", rcap2l);
			gpGunShm->wayne_rcap2l_flag = 2;  // @2 ��ʶ���óɹ�
			return 0;
		} else {
			// ������һ��ֵ
			rcap2l--;
			RunLog("------- �޸�RCAP2LΪ[%02x]", rcap2l);
			if (WriteRCAP2L(rcap2l) < 0) {
				gpGunShm->wayne_rcap2l_flag = 0;  // @3 ʧ�ܷ���Ҫ��ʶ��λ
				return -1;
			}
		}
	} while (rcap2l > 0x65);

	WriteRCAP2L(0x80);                // �ָ�Ĭ������
	gpGunShm->wayne_rcap2l_flag = 0;  // @4 ʧ�ܷ���Ҫ��ʶ��λ
	return -2;
}

/*
 * Note: �˹���Ҫ��IO����汾���ڵ���1006
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
		RunLog("дRCAP2LΪ[%02x]ʧ��", rcap2l);
		return -1;
	}

	Ssleep(200000);       // ��ʱ200ms, ȷ���µ�rcap2l�Ѿ���Ч�ٷ�������.

	return 0;
}

/*
 * ʹ�ö�ȡ���۲�������RCAP2L
 */
static int TestRCAP2L_GetTotal()
{
	int idx;
	int ret;
	int trycnt;
	int fail_cnt = 0;
	__u16 crc = 0;

	RunLog("Node[%d].FP[%d] Get Total[TEST]", node, 1);

	// 1. 5�ζ����۲���, ֻҪʧ�����ξ��˳�
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
			RunLog("��ȡ�������Node[%d].FP[%d]ʧ��", node, 1);
			continue;
		}

		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("��ȡ�������Node[%d].FP[%d]�ɹ�, ȡ������Ϣʧ��", node, 1);
			fail_cnt++;
			continue;
		} 

		// �����Ƿ��� NAK ���� TX# ����, ������
		if (pump.cmd[1] == (_NAK | tx_no[0])) {
			tx_no[0] = 0x00;   // ����ָ�
	//		fail_cnt++;
			continue;
		} else if ((pump.cmd[1] & 0x0F) != tx_no[0]) {
			tx_no[0] = 0x00;   // ����ָ�
	//		fail_cnt++;
			continue;
		}


		if (GetPumpMsg(0) < 0x08) {
			RunLog("��ȡNode[%d].FP[%d].LNZ[%d]�ı���ʧ��", node, 1, 1);
			tx_no[0] = 0x00;   // ����ָ�
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

	tx_no[0]--;       // ���һ��ѭ��PLUS_TX_NO������tx_no, ����û��ʹ��
	return 0;
}

/*
 * ʹ����������ǹ�Ų���, ʧ�����ξ��˳�
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

		// ǹ��
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


		pump.cmd[3] = i;          // ��ǹ����
		i += 4;
		crc = CRC_16(&pump.cmd[0], i);
		if ((crc & 0x00FF) == 0xFA) {
			pump.cmd[i++] = 0x10;       // ���������� DEL
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
			RunLog("��������ǹ�������Node[%d].FP[%d]ʧ��", node, 1);
			continue;
		}

		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("��������ǹ�������Node[%d].FP[%d]�ɹ�, ȡ������Ϣʧ��", node, 1);
			fail_cnt++;
			continue;
		} 

		// �����Ƿ��� NAK ���� TX# ����, ������
		if (pump.cmd[1] == (_NAK | tx_no[0])) {
			tx_no[0] = 0x00;   // ����ָ�
//			fail_cnt++;
			continue;
		} else if ((pump.cmd[1] & 0x0F) != tx_no[0]) {
			tx_no[0] = 0x00;   // ����ָ�
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

	tx_no[0]--;       // ���һ��ѭ��PLUS_TX_NO������tx_no, ����û��ʹ��
	return 0;
}


/* ----------------- End of lischn007.c ------------------- */

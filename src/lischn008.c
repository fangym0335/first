/*
 * Filename: lischn008.c
 *
 * Description: ���ͻ�Э��ת��ģ�� - ��ɽ�������ͻ�, CL, 9600,N,8,1 
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
#define INTERVAL	100000  // 100ms , ���ͻ�����

#define FP_IDLE        0x00
#define FP_CALLING     0x01
#define FP_AUTHORISED  0x02
#define FP_FUELLING    0x03
#define FP_SPFUELLING  0x04    // ���ͽ���/�ﵽ���ֵ, δ���潻��
#define FP_EOT         0x05    // ���׽���
#define FP_LOCKED      0xFF    // ��ǹ


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
static int fp_state[MAX_FP_CNT];   // 0 - IDLE, 1 - CALLING , 2 - �����DoStarted, 3 - FUELLING

int ListenChn008(int chnLog)
{
	long	indtyp;
	int	ret;
	int	fd;
	int	j;
	int	i;
	__u8 status;
	
	sleep(5);   //��ֹ�˽����˳�,��������û���������. ÿ��lischn008����Ҫ
	RunLog("Listen Channel  008 ,logic channel[%d]", chnLog);
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

	// ============= Part.2 �� & ���ô������� -- ������/У��λ�� ===================

	strcpy(pump.cmd, alltty[pump.port - 1]);
	pump.fd = OpenSerial(pump.cmd);
	if (pump.fd < 0) {
		RunLog("�򿪴���[%s]ʧ��", pump.cmd);
		return -1;
	}

	ret = SetPort008();
	if (ret < 0) {
		RunLog("���ô�������ʧ��");
		return -1;
	}

	Ack4GetTrans(0);
	// ============== Part.3 �жϼ��ͻ��Ƿ����� ====================================
	// Check wether dispenser is online, if it's not online, wait  here.
	while (1) {
		ret = GetPumpOnline(0, &status);
		if (ret == 0) {
			break;
		}

		gpIfsf_shm->node_online[IFSFpos] = 0; //������,����������.
		UserLog("Node %d ������", node);
		
		sleep(1);
	}
	
	// ============= Part.4 ��ʼ������ͻ���ǰ״̬��ص����ݿ������ =======================
	
	for (i = 0; i < pump.panelCnt; ++i) {
		ret = GetPumpInfo(i);
	}

	// �ȴ�״̬
	for (i = 0; i < pump.panelCnt; ++i) {
		ret = GetPumpStatus(i, &status);
		if (ret < 0) {
			return -1;
		} else if (FP_AUTHORISED == status || FP_FUELLING == status) {
			return -1;
		}
	}

	ret = InitGunLog();    // Note.InitGunLog ������ÿ��Ʒ���ͻ�������,�����ͻ�Ԥ�ü�����Ҫ
	if (ret < 0) {
		RunLog("��ʼ��Ĭ�ϵ�ǰ�߼�ǹ��ʧ��");
		return -1;
	}

	ret = InitCurrentPrice(); //  ��ʼ��fp���ݿ�ĵ�ǰ�ͼ�
	if (ret < 0) {
		RunLog("��ʼ����Ʒ���۵����ݿ�ʧ��");
		return -1;
	}

	ret = InitPumpTotal();    // ��ʼ�����뵽���ݿ�
	if (ret < 0) {
		RunLog("��ʼ�����뵽���ݿ�ʧ��");
		return -1;
	}

//	gpIfsf_shm->node_online[IFSFpos] = node;  // һ�о���, ����
	ChangeNode_Status(node, NODE_ONLINE);
	RunLog("�ڵ� %d ����,״̬[%02x]!", node, status);
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
			GetPumpStatus(0, &status);              // Note. ���ͻ�������ϵ
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
			Ssleep(200000);  // 200ms, ���ݾ����������
			DoPolling();
		}
	} // end of main endless loop

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
			ret = HandleStateErr(node, pos, Do_Auth,     // Note. ������Do_Auth ������ Release_FP 
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
	int tmp_lnz;
	static int times = 0;
	static int fail_cnt = 0;  // ��¼GetPumpStatus ʧ�ܴ���, �����ж�����
	static __u8 pre_status[MAX_FP_CNT] = {0};
	__u8 status;
	
	for (i = 0; i < pump.panelCnt; i++) {
		ret = GetPumpStatus(i, &status);
		if (ret < 0) {
			if (gpIfsf_shm->node_online[IFSFpos] == node) {
				if ((++fail_cnt) < MAX_FAIL_CNT) {  // ���ͻ������ж�
					continue;
				}

				memset(dostop_flag, 0, sizeof(dostop_flag));
				memset(fp_state, 0, sizeof(fp_state));

//				gpIfsf_shm->node_online[IFSFpos] = 0;
				ChangeNode_Status(node, NODE_OFFLINE);
				pump.time_offline = (3 * gpIfsf_shm->fcc_com_sv_data.Heartbeat_Interval) + time(NULL);
				RunLog("====== Node %d ����! =============================", node);

				for (j = 0; j < pump.panelCnt; ++j) {
					lpDispenser_data->fp_data[j].FP_State = FP_State_Inoperative;
					lpDispenser_data->fp_data[j].Log_Noz_State = 0;
				}
			}
			continue;
		} else {
			if (gpIfsf_shm->node_online[IFSFpos] == 0) {
				if ((--fail_cnt) > 0) { // �ų�����/˫�ߵ��������ݲɼ�Bug��ɵ�Ӱ��
					fail_cnt = 1;
					continue;
				} else if (time(NULL) < pump.time_offline) {
					RunLog("====================== wait for 30s, ==============");
					continue;
				}

				// fail_cnt��λ
				fail_cnt = 0;

				ret = InitPumpTotal();    // ���±��뵽���ݿ�
				if (ret < 0) {
					RunLog("���±��뵽���ݿ�ʧ��");
					fail_cnt = 1;     // Note.�ָ��������κ�һ������ʧ�ܶ�Ҫ����
					continue;
				}

				for (j = 0; j < pump.panelCnt; ++j) {
					lpDispenser_data->fp_data[j].FP_State = FP_State_Closed;
				}

				// һ��׼������, ����
//				gpIfsf_shm->node_online[IFSFpos] = node;
				ChangeNode_Status(node, NODE_ONLINE);
				RunLog("====== Node %d �ָ�����! =========================", node);
				keep_in_touch_xs(12);               // ȷ��CD�Ѿ��յ�����֮��������������
				// �����ϴ���ǰFP״̬
				for (j = 0; j < pump.panelCnt; ++j) {
					ChangeFP_State(node, j + 0x21, FP_State_Closed, NULL);
				}
				continue;
			}
			
			fail_cnt = 0;
		}

		// ����״̬���б仯��ʱ��Ŵ�ӡ(д��־)
		if (status != pre_status[i]) {
			pre_status[i] = status;
			RunLog("�ڵ�[%d]���[%d]��ǰ״̬��Ϊ[%02x]", node, i + 1, status);
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

			CheckPriceErr(i);
			continue;
		}

		switch (status) {
		case FP_IDLE:                                   // ��ǹ������
		case FP_EOT:
		case FP_LOCKED:
			dostop_flag[i] = 0;
			if (fp_state[i] >= 2) {
				// ���мӹ���, �����״̬�ı�ͽ��״���
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

			// �ѻ����״���
			EO_OFFLINE_TR(node, i);
			if (gpIfsf_shm->fp_offline_flag[node - 1][i] != 0) {
				upload_offline_tr(node, NULL);
			}

			// ��״̬����IDLE, ���л�ΪIdle
			if (((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State )) {
				ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
			}

			ret = ChangePrice();                    // �鿴�Ƿ�����Ʒ��Ҫ���,����ִ��
			break;
		case FP_CALLING:                                // ̧ǹ
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
		case FP_FUELLING:                               // ����
			// ���䴦��
			if (fp_state[i] < 2) {
				if (DoStarted(i) < 0) {
					continue;
				}
			}

			// ���䴦��
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
 * ����ö��ͻ��Ķ�д��������֮ǰ�ѽ��й�״̬�жϺʹ���Ԥ����,
 * �����ھ��������ִ�к����о���������״̬�����
 * ��Ϊ�е��ͻ�ģ����ÿ���·�����֮ǰ����״̬�жϵĲ���, �ش�˵��
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
		pump.wantLen = 6;        // �䳤

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog( "Node[%d]FP[%d]�·�Ԥ�ü�������ʧ��",node,pos+1 );
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x08;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x00;
		if (ret < 0) {
			RunLog( "Node[%d]FP[%d]�·�Ԥ�ü�������ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		} 

		crc = CRC_16_HS(&pump.cmd[1], pump.cmdlen - 3);
		if ((__u8)(crc & 0x00FF) != pump.cmd[pump.cmdlen - 2] ||
				(__u8)(crc >> 8) != pump.cmd[pump.cmdlen - 1]) {
			RunLog( "Node[%d]FP[%d]��������У�����", node, pos + 1);
			continue;
		}
		
		if (pump.cmd[6] != 0x55) {
			RunLog("Node[%d]FP[%d] ִ��Ԥ�ü�������ʧ��", node, pos + 1);
			continue;
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
	if ((0x04 != pump.cmd[8]) || (0x00 != pump.cmd[9]) || pr_no > 7) {
		RunLog("�����Ϣ����,�����·�");
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
 * �����µ�������newPrice[FP_PR_NB_CNT], �������Ʒ��Ҫ���--����ֵ��Ϊ��,
 * ���������FP, �����Ʒƥ����·��������
 */

//	RunLog(">> exec ChangePrice Now !");
	for (pr_no = 0; pr_no < FP_PR_NB_CNT; pr_no++) {
//		RunLog(">>>1 newPrice[%d]: %02x.%02x", pr_no, pump.newPrice[pr_no][2], pump.newPrice[pr_no][3]);;
		if (pump.newPrice[pr_no][2] != 0x00 || pump.newPrice[pr_no][3] != 0x00) {
			for (i = 0; i < pump.panelCnt; i++) {
				err_code = 0x99;
				for (j = 0; j < MAX_FP_LN_CNT; j++) { // ������gunCnt,1ǹ���ܲ���
#if DEBUG
					RunLog(">>> fp_ln_data[%d][%d].PR_Id: %d", i, j,
							lpDispenser_data->fp_ln_data[i][j].PR_Id);
#endif
					if (lpDispenser_data->fp_ln_data[i][j].PR_Id - 1 == pr_no) {
					err_code_tmp = 0x99;
				CriticalLog("�·����������ڵ�[%02d]��[%d]ǹ[%d]......", node, i + 1, j + 1);

					for (trycnt = 0; trycnt < TRY_TIME; trycnt++) {
						frame_no[i] = frame_no[i] % 17;
						// Step.1 ���������
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
						pump.wantLen = 6;      // �䳤

						Ssleep(INTERVAL);
						ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
								pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
						if (ret < 0) {
							RunLog("Node[%d]FP[%d]�·��������ʧ��", node, i + 1);
							continue;
						}


						RunLog("Node[%d].FP[%d].PriceChange_CMD: %s",
							node, i + 1, Asc2Hex(pump.cmd, pump.cmdlen));

						// Step.2 �����������Ϣ�����ض��ͻ�����ȷ�ϱ���Ƿ�ɹ�
						memset(pump.cmd, 0, sizeof(pump.cmd));
						pump.cmdlen = sizeof(pump.cmd) - 1;
						gpGunShm->addInf[pump.chnLog - 1][0] = 0x08;
						ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
						gpGunShm->addInf[pump.chnLog - 1][0] = 0x00;
						if (ret < 0) {
							RunLog("Node[%d]FP[%d]�·��������ɹ�, ȡ������Ϣʧ��",
												node, i + 1);
							continue;
						} 

						crc = CRC_16_HS(&pump.cmd[1], pump.cmdlen - 3);
						if ((__u8)(crc & 0x00FF) != pump.cmd[pump.cmdlen - 2] ||
								(__u8)(crc >> 8) != pump.cmd[pump.cmdlen - 1]) {
							RunLog( "Node[%d]FP[%d]��������У�����", node, i + 1);
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
							continue;
						}

						if (pump.cmd[6] != 0x55) {
							RunLog("Node[%d]FP[%d] ִ�б������ʧ��", node, i + 1);
							err_code_tmp = 0x37;
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
							continue;
						}

						ret = GetPrice(i);
						if (ret < 0) {
							continue;
						}

						if (memcmp(&pump.cmd[7], &pump.newPrice[pr_no][2], 2) != 0) {
							RunLog("Node[%d]FP[%d] ִ�б������ʧ��", node, i + 1);
							err_code_tmp = 0x37;
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
							continue;
						}

										
						// Step.3 д�µ��۵���Ʒ���ݿ� & ���㵥�ۻ���
						fm_idx = lpDispenser_data->fp_data[i].Default_Fuelling_Mode - 1;
						RunLog("Node[%d]���ͱ������ɹ�,�µ���fp_fm_data[%d][%d].Prode_Price: %s",
							node, pr_no, fm_idx,
							Asc2Hex(lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price, 4));

						CriticalLog("�ڵ�[%02d]��[%d]ǹ[%d]��۳ɹ�,�µ���Ϊ%02x.%02x",
							node, i + 1, j + 1,
							lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2],
							lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3]);

						err_code_tmp = 0x99;
						break;
						
					}

					// Step.4 �����ж�, ��������д�������ݿ�/��FP״̬Ϊ Inoperative
					if (trycnt >= TRY_TIME) {
						// Write Error Code: 0x36/0x37 - Price Change Error (Spare)
						if (0x37 != err_code_tmp) {
							err_code_tmp = 0x36;    // ͨѶʧ��
				CriticalLog("�ڵ�[%02d]��[%d]ǹ[%d]���ʧ��[ͨѶ����],�ı���״̬Ϊ[������]",
											node, i + 1, j + 1);
						} else {
				CriticalLog("�ڵ�[%02d]��[%d]ǹ[%d]���ʧ��[�ͻ�ִ��ʧ��],�ı���״̬Ϊ[������]",
											node, i + 1, j + 1);
						}

						PriceChangeErrLog("%02x.%02x.%02x %02x.%02x %02x",
							node, i + 1, j + 1,
							pump.newPrice[pr_no][2], 
							pump.newPrice[pr_no][3], err_code);

					}
					if (err_code == 0x99) {
						// �������κ���ǹ���ʧ��, ��������FP���ʧ��.
						err_code = err_code_tmp;
					}
					} // end of if.PR_Id match
				//	break;  // ������ͻ��ܹ�����ָ����Ϣ�����ǹͬ��Ʒ���������������Ҫbreak.
				} // end of for.j < LN_CNT
				RunLog("---------- Node[%d]FP[%d] ER_DATA, error code: %02x -----", node, i + 1, err_code);

				int tmp_idx;
				for (tmp_idx = 0; tmp_idx < pump.newPrice[pr_no][0]; tmp_idx++) {
					ChangeER_DATA(node, 0x21 + i, err_code, NULL); // Translater�Ѿ��ظ���ACK
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
	__u8 total[7] = {0};
	__u8 pre_total[7];

	RunLog("�ڵ�[%d]���[%d]�����µĽ���", node, pos + 1);

	// 1. Get Current TTC  & Volume_Total
	err_cnt = 0;
	do {
		ret = GetPumpTotal(pos);
		if (ret < 0) {
			err_cnt++;
			RunLog("�ڵ�[%d]���[%d]��ȡ����ʧ��", node, pos + 1);
		}
	} while (err_cnt <= 5 && ret < 0);

	if (ret < 0) {
		return -1;
	}

//	memcpy(&current_ttc[pos], &pump.cmd[9], 4);       // ���µ�ǰTTC
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
				RunLog("�ڵ�[%d]���[%d]��ȡ���׼�¼ʧ��", node, pos + 1);
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

		// ���� Volume_Total
		memcpy(lpDispenser_data->fp_m_data[pr_no].Meter_Total, total, 7);
		memcpy((lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Vol_Total, total, 7);

#if 0
		RunLog("�ڵ�[%02d]���[%d]ǹ[%d] ## Amount: %02x%02x.%02x Volume: %02x%02x.%02x Price: %02x.%02x Total: %02x%02x%02x.%02x", node, pos + 1, pump.gunLog[pos], amount[2], 
					amount[3], amount[4], volume[2], volume[3], volume[4], price[2],
							price[3], total[3], total[4], total[5], total[6]);
#endif

		// ���� Amount_Total--û�н������
		memset(total, 0, 7);
		total[0] = 0x0A;
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
 * func: ��ȡ��FP����ǹ�ı���
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
		// pump.cmd[2] Ϊ 0x03, pump.cmd[3] Ϊ0x06��ʱ��У��ڶ��ֽ�ΪFA
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
		pump.wantLen = 6;      // �䳤,  �Ƚ��յ�������,��������֮ǰ������FA��ת��

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog( "Node[%d]FP[%d]�·�ȡ��������ʧ��",node,pos+1 );
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x08;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x00;
		if (ret < 0) {
			RunLog( "Node[%d]FP[%d]�·�ȡ��������ɹ�, ȡ������Ϣʧ��",node,pos+1 );
			continue;
		} 

		crc = CRC_16_HS(&pump.cmd[1], pump.cmdlen - 3);
		if ((__u8)(crc & 0x00FF) != pump.cmd[pump.cmdlen - 2] ||
				(__u8)(crc >> 8) != pump.cmd[pump.cmdlen - 1]) {
			RunLog( "Node[%d]FP[%d]��������У�����", node, pos + 1);
			continue;
		}

		memcpy(&current_ttc[pos][0], &pump.cmd[9], 4);       // ���µ�ǰTTC
		return 0;
	}

	return -1;

}

/*
 * func: ��ȡ���ν��׼�¼
 * return: 0 - �ɹ�, 1 - �ɹ����޽���, -1 - ʧ��
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
		pump.wantLen = 6;       // �䳤

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]�·�����������ʧ��",node,pos+1 );
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x08;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x00;
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]�·�����������ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		} 

		crc = CRC_16_HS(&pump.cmd[1], pump.cmdlen - 3);
		if ((__u8)(crc & 0x00FF) != pump.cmd[pump.cmdlen - 2] ||
				(__u8)(crc >> 8) != pump.cmd[pump.cmdlen - 1]) {
			RunLog("Node[%d]FP[%d]��������У�����", node, pos + 1);
			continue;
		}

		if (pump.cmd[4] == 0x05) {
			RunLog("Node[%d]FP[%d]û�и���Ž���", node, pos + 1);
			return 1;
		}

		// ���ͽ���ȷ��
		Ack4GetTrans(pos);

		return 0;
	}

	return -1;
}


/*
 * func: ����ȷ��
 *      �õ�����ȷ��֮����ǹ״̬�Ż��ɼ��ͽ���ת��Ϊ����
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
		// pump.cmd[2] == 0x02, pump.cmd[3] == 0x07, У���һ�ֽ�Ϊ FA
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
		wantLen = 6;      // �䳤

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, cmd,
			       	cmdlen, 1, wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]�·�����ȷ��ʧ��", node, pos + 1);
			continue;
		}

		memset(cmd, 0, sizeof(cmd));
		cmdlen = sizeof(cmd) - 1;
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x08;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, cmd, &cmdlen);
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x00;
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]�·�����ȷ�ϳɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		} 

		crc = CRC_16_HS(&cmd[1], cmdlen - 3);
		if ((__u8)(crc & 0x00FF) != cmd[cmdlen - 2] ||
				(__u8)(crc >> 8) != cmd[cmdlen - 1]) {
			RunLog( "Node[%d]FP[%d]��������У�����", node, pos + 1);
			continue;
		}

		if (cmd[6] != 0x55) {
			RunLog( "Node[%d]FP[%d]ִ�н���ȷ��ʧ��", node, pos + 1);
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
		// pump.cmd[2] == 0x02, pump.cmd[3] == 0x0f, У��ڶ��ֽ�Ϊ FA
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
		pump.wantLen = 6;      // �䳤

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog( "Node[%d]FP[%d]�·�ͣ������ʧ��",node,pos+1 );
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x08;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x00;
		if (ret < 0) {
			RunLog( "Node[%d]FP[%d]�·�ͣ������ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		} 

		crc = CRC_16_HS(&pump.cmd[1], pump.cmdlen - 3);
		if ((__u8)(crc & 0x00FF) != pump.cmd[pump.cmdlen - 2] ||
				(__u8)(crc >> 8) != pump.cmd[pump.cmdlen - 1]) {
			RunLog( "Node[%d]FP[%d]��������У�����", node, pos + 1);
			continue;
		}

		if (pump.cmd[6] != 0x55) {
			RunLog( "Node[%d]FP[%d] ִ��ͣ������ʧ��", node, pos + 1);
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
		// pump.cmd[2] == 0x02, pump.cmd[3] == 0x04, У��ڶ��ֽ�Ϊ FA
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
		pump.wantLen = 6;      // �䳤

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog( "Node[%d]FP[%d]�·���Ȩ����ʧ��",node,pos+1 );
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x08;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x00;
		if (ret < 0) {
			RunLog( "Node[%d]FP[%d]�·���Ȩ����ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		} 

		crc = CRC_16_HS(&pump.cmd[1], pump.cmdlen - 3);
		if ((__u8)(crc & 0x00FF) != pump.cmd[pump.cmdlen - 2] ||
				(__u8)(crc >> 8) != pump.cmd[pump.cmdlen - 1]) {
			RunLog( "Node[%d]FP[%d]��������У�����", node, pos + 1);
			continue;
		}
		
		if (pump.cmd[6] != 0x55) {
			RunLog( "Node[%d]FP[%d] ִ����Ȩ����ʧ��", node, pos + 1);
			continue;
		}


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
	__u8 volume[5], amount[5];
	const __u8 arr_zero[4] = {0};

	// ������Ϊ�㲻ת��ΪFuelling״̬ , ���ϴ�Run_Trans
	if (fp_state[pos] != 3) {
		if (memcmp(arr_zero, &pump.cmd[8], 4) == 0) {
			return 0;
		}
		fp_state[pos] = 3;
		return 0;
	}

	//  ȡ��ʵʱ������ ........
	
	volume[0] = 0x06;
	Hex2Bcd(&pump.cmd[7], 4, &volume[1], 4);

	amount[0] = 0x06;
	Hex2Bcd(&pump.cmd[11], 4, &amount[1], 4);

	if ((__u8)FP_State_Fuelling == lpDispenser_data->fp_data[pos].FP_State) {
		ret = ChangeFP_Run_Trans(node, 0x21+ pos, amount,  volume); //ֻ��һ�����
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
 * func: ��ȡ�ͻ�����
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
		// pump.cmd[2] Ϊ 0x03, pump.cmd[3] Ϊ0x06��ʱ��У��ڶ��ֽ�ΪFA
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
		pump.wantLen = 6;      // �䳤,  �Ƚ��յ�������,��������֮ǰ������FA��ת��

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog( "Node[%d]FP[%d]�·�ȡ��������ʧ��",node,pos+1 );
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x08;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x00;
		if (ret < 0) {
			RunLog( "Node[%d]FP[%d]�·�ȡ��������ɹ�, ȡ������Ϣʧ��",node,pos+1 );
			continue;
		} 

		crc = CRC_16_HS(&pump.cmd[1], pump.cmdlen - 3);
		if ((__u8)(crc & 0x00FF) != pump.cmd[pump.cmdlen - 2] ||
				(__u8)(crc >> 8) != pump.cmd[pump.cmdlen - 1]) {
			RunLog( "Node[%d]FP[%d]��������У�����", node, pos + 1);
			continue;
		}

		return 0;
	}

	return -1;
}



/*
 * func: ��ȡʵʱ״̬ (ʧ��ֻ����һ��)
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
		pump.wantLen = 6;      // �䳤

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog( "Node[%d]FP[%d]�·�ȡ״̬����ʧ��",node,pos+1 );
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x08;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x00;
		if (ret < 0) {
			RunLog( "Node[%d]FP[%d]�·�ȡ״̬����ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		} 

		crc = CRC_16_HS(&pump.cmd[1], pump.cmdlen - 3);
		if ((__u8)(crc & 0x00FF) != pump.cmd[pump.cmdlen - 2] ||
				(__u8)(crc >> 8) != pump.cmd[pump.cmdlen - 1]) {
			RunLog( "Node[%d]FP[%d]��������У�����", node, pos + 1);
			continue;
		}

		*status = pump.cmd[6];
		return 0;
	}

	return -1;
}


//test wether the dispenser is online, we only use fp 1 to test.
/*
 * func: �ж�Dispenser�Ƿ�����
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
		pump.wantLen = 6;      // �䳤

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog( "Node[%d]FP[%d]�·�ȡ״̬����ʧ��",node,pos+1 );
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x08;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x00;
		if (ret < 0) {
			RunLog( "Node[%d]FP[%d]�·�ȡ״̬����ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		} 

		crc = CRC_16_HS(&pump.cmd[1], pump.cmdlen - 3);
		if ((__u8)(crc & 0x00FF) != pump.cmd[pump.cmdlen - 2] ||
				(__u8)(crc >> 8) != pump.cmd[pump.cmdlen - 1]) {
			RunLog( "Node[%d]FP[%d]��������У�����", node, pos + 1);
			continue;
		}

		*status = pump.cmd[6];
		return 0;
	}

	return -1;
}

/*
 * func: ��ȡʵʱ״̬ (ʧ��ֻ����һ��)
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
		pump.cmd[3] = frame_no[pos];  //  ֡�� 0-16 ѭ��
		pump.cmd[4] = 0x01;    // len
		pump.cmd[5] = 0x01;    // cmd
		crc = CRC_16_HS(&pump.cmd[1], 5);
		/*
		 *  pump.cmd[2]: 1 - 4, pump.cmd[3]: 0-16 �������
		 *  У�鲻����� FA,���ﲻ��Ҫ�ж�У���Ƿ���FA,
		 *  Ϊͳһ������GetPumpInfo����ʹ��, ���������ж�;
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
		pump.wantLen = 6;      // �䳤,  �Ƚ��յ�������,��������֮ǰ������FA��ת��

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog( "Node[%d]FP[%d]�·�ȡ�豸��Ϣ����ʧ��",node,pos+1 );
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x08;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x00;
		if (ret < 0) {
			RunLog( "Node[%d]FP[%d]�·�ȡ�豸��Ϣ����ɹ�, ȡ������Ϣʧ��",node,pos+1 );
			continue;
		} 

		crc = CRC_16_HS(&pump.cmd[1], pump.cmdlen - 3);
		if ((__u8)(crc & 0x00FF) != pump.cmd[pump.cmdlen - 2] ||
				(__u8)(crc >> 8) != pump.cmd[pump.cmdlen - 1]) {
			RunLog( "Node[%d]FP[%d]��������У�����", node, pos + 1);
			continue;
		}

		return 0;
	}

	return -1;
}


/*
 * func: �ͻ���Ϊ����״̬, �ı�FP״̬ΪStarted
 */
static int DoStarted(int i)
{
	int ret;

	if (lpDispenser_data->fp_data[i].Log_Noz_State == 0) {
		__u8 ln_status = 0;
		ln_status = 1 << (pump.gunLog[i] - 1);
		lpDispenser_data->fp_data[i].Log_Noz_State = ln_status;
		RunLog("�ڵ�[%d]���[%d] %d ��ņ̀��", node, i + 1, pump.gunLog[i]);
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
 * func: ��ȡ��ǰ̧����ǹ��ǹ��, ���д��pump.gunLog[] & fp_data[].Log_Noz_State
 * ret : 0 - �ɹ�, -1 - ʧ�� 
 */
static inline int GetCurrentLNZ(int pos)
{
	RunLog("Node %d FP %d �� %d ��ņ̀��", node, pos + 1, pump.gunLog[pos]);
	lpDispenser_data->fp_data[pos].Log_Noz_State = 1 << (pump.gunLog[pos] - 1);

	return 0;
}

/*
 * ��ʼ��FP�ĵ�ǰ�ͼ�. �������ļ��ж�ȡ��������,
 * д�� fp_data[].Current_Unit_Price
 * ���� PR_Id �� PR_Nb ��һһ��Ӧ��
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
 * func: ��ʼ����ǹ����
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
			RunLog("ȡ Node %d FP %d �ı���ʧ��", node, i);
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
		RunLog("Node[%d].FP[%d]LNZ[%d]�ı���Ϊ[%s]", node, i + 1,
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
 * func: ���ÿ���ģʽ
 *	��ɽ�������ͻ�����Ҫ, ͨ����������
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
 * func: �趨�������ԡ�ͨѶ���ʡ�У��λ��
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
			RunLog("����ͨ�� %d ��������ʧ��", pump.chnLog);
			return -1;
		}
		gpGunShm->chnLogSet[pump.chnLog - 1] = 1;
	}

	return 0;
}




/*
 * ���FP�ϸ�ǹ����Ʒ�����Ƿ������ȷ(����ʵ��), ���ȫ����ȷ��ı�״̬ΪClosed
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
	RunLog("Node[%d]FP[%d]�����ѱ����ȷ, �л�״̬ΪClosed", node, pos + 1);

	return 0;
}


/* ----------------- End of lischn008.c ------------------- */

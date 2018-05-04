/*
 * Filename: lischn019.c
 *
 * Description: ���ͻ�Э��ת��ģ�� -- ��ɽUDC, 3 Line CL, 9600,8,n,1
 *
 * Э���ĵ�: UDC Protocol P1 & P2 
 * �����ͻ�: TH30ϵ��
 *
 * 2009.06 Created by Guojie Zhu <zhugj@css-intelligent.com>
 * 
 * Note:
 *     1.
 *     2. D0״̬��ǹ, �����Ľ��׿���Ϊ��,�������� (�ͻ�ȱ��)
 *
 * History:   
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

#define NOZ_TOTAL_LEN  5

// FP State
#define FP_INOPERATIVE  0x2F      // Uninitialized

#define FP_IDLE         0x20
#define FP_CALLING1     0xA0      // cash sale
#define FP_CALLING2     0xA1      // credit sale
#define FP_AUTHORISED   0x90
#define FP_FUELLING1    0xD0      // С���� 
#define FP_FUELLING2    0xF0      // �󷧿�
#define FP_SPFUELLING   0x98      // Suspend Fuelling

#define TRY_TIME	5
#define INTERVAL	150000    // 150ms����ͼ��, ��ֵ�Ӿ����ͻ�����

typedef struct {
	__u8 a1_rsp_len;   // 18 or more...
	__u8 a2_rsp_len;   // 2 or more...
	__u8 a3_rsp_len;   // 2 or 4
	__u8 a4_rsp_len;   // 2 or 4
	__u8 a5_rsp_len;   // 2 or 4
	__u8 a6_rsp_len;   // 2 or 4
}CMD_VAR_PARAM;

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
static int ChangePrice();
static int GetPrice(int pos);
static int GetGunTotal(int pos, __u8 gunPhy, __u8 *vol_total, __u8 *amt_total);
static int GetPumpTotal(int pos);
static int GetPumpStatus(int pos, __u8 *status);
static int InitPumpTotal();
static int InitCurrentPrice();
static int InitGunLog();
static int GetCurrentLNZ(int pos);
static int keep_in_touch_xs(int x_seconds);
static int SetDisplayData(int pos);
static int ReDoSetPort019();
static int GetDisplayData(int pos);
static int GetPumpID(int pos, __u8 *id);
static int AckDeactivedHose(int pos);
static int Wait4PumpReady();
static int ReWritePrice(int pos);
static int DoOnline();
static int DoOffline();
static int SetPort019();
static int DoubleTalkCheck(const __u8 *buf, int len);
static int CheckPriceErr(int pos);

static PUMP pump;
static int node;
static int IFSFpos;
static DISPENSER_DATA *lpDispenser_data;
static NODE_CHANNEL *lpNode_channel;
static int fp_state[MAX_FP_CNT];     // 0 - IDLE, 1 - CALLING , 2 - DoStarted �ɹ� 3 - FUELLING
static int dostop_flag[MAX_FP_CNT];  // 1 - �ɹ�ִ�й� Terminate_FP
/* ack_deactived_hose_flag����Ƿ���Ҫִ�й�ǹȷ�ϺͶ����ν���. 1 - ��, 0 - ��; 
 * ��ʼ��Ҫִ��һ���Ǳ��⿼�Ǵ˳�������֮ǰ�ͻ���δ��ȷ�ϵĹ�ǹ��δ��ȡ�Ľ���.
 */
static int ack_deactived_hose_flag[MAX_FP_CNT] = { 1, 1, 1, 1};
static int nb_prods[MAX_FP_CNT];     // ��¼�ͻ��ϸ���FP���õ���Ʒ�����Ƕ���, �������
static __u8 display_data[MAX_FP_CNT][16] = {      // ���������������2F״̬ʱ��ʼ���ͻ�
	{0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff,
		0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff}, 
	{0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff,
		0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff}, 
	{0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff,
		0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff}, 
	{0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff,
		0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff}, 
};


int ListenChn019(int chnLog)
{
	long	indtyp;
	int	ret;
	int	fd;
	int	i, j;
	__u8    id = 0, status = 0;
	
	sleep(5);   //��ֹ�˽����˳�,��������û���������. ÿ��lischnxxx����Ҫ
	RunLog("Listen Channel  019 ,logic channel[%d]", chnLog);
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
						// ��ַӳ��: 0x00 - 0x0F ��ӦFP��ַ 1-16
						pump.panelPhy[j] = gpIfsf_shm->gun_config[idx].phyfp - 1;
						break;
					}
				}
				//pump.gunCnt[j] =  gpIfsf_shm->node_channel[i].cntOfFpNoz[j];
				pump.gunCnt[j] = 4;   // fix value, modified by Guojie Zhu @ 2009.11.07
			}
				
			break;
		}
	}
	pump.indtyp = chnLog;
	memset(&pump.newPrice, 0, FP_PR_NB_CNT * 4);
	
	// ============= Part.2 �� & ���ô������� -- ������/У��λ�� ===================

	strcpy(pump.cmd, alltty[pump.port - 1]);
	pump.fd = OpenSerial(pump.cmd);
	if (pump.fd < 0) {
		RunLog("�ڵ�[%d]�򿪴���[%s]ʧ��", node, pump.cmd);
		return -1;
	}

	ret = SetPort019();
	if (ret < 0) {
		RunLog("�ڵ�[%d]���ô�������ʧ��", node);
		return -1;
	}

	
	// ============== Part.3 �жϼ��ͻ��Ƿ����� ====================================
	// Check wether dispenser is online, if it's not online, wait  here.
	while (1) {		
		// �÷������ݽϳ��Ĺ��ܲ���ͨѶ, �������д������ݵ�����»��жϳ���.
		ret = GetDisplayData(0);
		if (ret == 0) {		
			break;
		}
		
		gpIfsf_shm->node_online[IFSFpos] = 0; //������,����������.
		UserLog("�ڵ� %d ���ܲ�����", node);
		
		sleep(1);
	}
	
	ret = GetPumpID(0, &id);
	if (ret < 0) {
		return -1;
	}

	ret = Wait4PumpReady();
	if (ret < 0) {
		RunLog("Node[%d] ״̬δ׼����, �ȴ�����", node);
		return -1;
	}

	// ============= Part.4 ��ʼ������ͻ���ǰ״̬��ص����ݿ������ =======================
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


     //+++++++++++++++  һ�о���, ����+++++++++++++++++++++++//
       
//	gpIfsf_shm->node_online[IFSFpos] = node;  
	ChangeNode_Status(node, NODE_ONLINE);
	UserLog("�ڵ� %d ����!", node);
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
			Ssleep(200000);
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
	__u8 msgbuffer[48];

	// ��ȡ��������ִ�к���ǰ�Ĺ��в���
	if (pump.cmd[0] != 0x2F) {
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

		memset(msgbuffer, 0, sizeof(msgbuffer));
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
			memset(pump.addInf[pos], 0, sizeof(pump.addInf[pos]));
			if ((__u8)FP_State_Calling == lpDispenser_data->fp_data[pos].FP_State) {
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
			}else if (ret == -2) {
				SendCmdAck(NACK, msgbuffer);
				ChangeFP_State(node, pos + 0x21,
					FP_State_Idle, NULL);

			}else{
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

			/*
			 * ̧ǹ֮���·�Ԥ����ʹ�õı�Ǳ����ͻ�����:
			 * pump.addInf[pos][0]: Ԥ�ñ��, ��ʶ�Ƿ�̧ǹ���Ƿ���Ԥ����Ҫ�·�
			 * pump.addInf[pos][1]: Ԥ�÷�ʽ����ֽ�
			 * pump.addInf[pos][2]: Ԥ��������ǹ, 0xFF - ����ȫ��
			 * pump.addInf[pos][3-5]:  Ԥ��������
			 */
			pump.addInf[pos][0] = 1;            // Ԥ�ñ��, �����Ԥ����Ҫ�·�
			pump.addInf[pos][1] = pump.cmd[0];  // Ԥ�÷�ʽ
			pump.addInf[pos][2] = lpDispenser_data->fp_data[pos].Log_Noz_Mask;

			if (pump.cmd[0] == 0x3D) {
				memcpy(&(pump.addInf[pos][3]), 
					&lpDispenser_data->fp_data[pos].Remote_Volume_Preset[2], 3);
				ret = 0;
			} else if (pump.cmd[0] == 0x4D) {
				memcpy(&(pump.addInf[pos][3]), 
					&lpDispenser_data->fp_data[pos].Remote_Amount_Prepay[2], 3);
				ret = 0;
			} else {
				ret = -1;
			}

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

			memset(&lpDispenser_data->fp_data[pos].Remote_Volume_Preset, 0, 5);
			memset(&lpDispenser_data->fp_data[pos].Remote_Amount_Prepay, 0, 5);
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
			RunLog("�ڵ�[%d]���ܵ�δ��������[0x%02x]", node, pump.cmd[0]);
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
					/*
					 * �����贮�����Իᵼ��ReadTTY���յ�"�Ƿ�����", ����ʱ�Ƴ�
					 * 2009.02.19
					if (fail_cnt >= (MAX_FAIL_CNT / 2)) {
						if (SetPort019() >= 0) {
							RunLog("�ڵ�%d���贮�����Գɹ�", node);
						}
					}
					*/
					continue;
				}

				DoOffline();
			}

			// ���贮������
			//ReDoSetPort019();
			continue;
		} else {
			if (gpIfsf_shm->node_online[IFSFpos] == 0) {
				if ((--fail_cnt) > 0) { 
					fail_cnt = 1;   // ����OK���ж��ͻ�����
					continue;
				} else if (time(NULL) < pump.time_offline) {
					RunLog("================ waiting for 3 Heartbeat Interval ================");
					continue;
				}

				// fail_cnt��λ
				fail_cnt = 0;

				if (DoOnline() < 0) {
					fail_cnt = 1;  // ������������
				}

				continue;
			}
			fail_cnt = 0;
		}
		
		if (status != pre_status[i]) {
			pre_status[i] = status;
			RunLog("�ڵ�[%d]�����[%d]��ǰ״̬��Ϊ[%02x]", node, i + 1, status);
		}

		if (gpIfsf_shm->auth_flag - FCC_SWITCH_AUTH != 0) {  //����к�̨
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

		switch (status & 0xF9) {
		case FP_IDLE:
			// 1.
			if (fp_state[i] >= 1) {
				// ���мӹ���, �����״̬�ı�ͽ��״���
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

			// 2. ������ "1."֮��, �ڽ��״������֮ǰ����̧ǹ�ź�
			if (ack_deactived_hose_flag[i] != 0) {
				ret = AckDeactivedHose(i);
				if (ret == 0 && GetDisplayData(i) == 0) {
					ack_deactived_hose_flag[i] = 0;
				}
			}

			// 3.
			if (pump.addInf[i][0] != 0) { // ��Ԥ����δ�·�, FP��Authorised״̬
				/* �����ǹ״̬Log_Noz_State,
				 * ����̧��ǹ�����������̧ǹ��DoStarted���޷����¶�ȡ��ǰǹ��
				 */
				lpDispenser_data->fp_data[i].Log_Noz_State = 0;
				break;
			}


			// 4.
			lpDispenser_data->fp_data[i].Log_Noz_State = 0x00;
			fp_state[i] = 0;

			if ((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State) {
				ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
			}

			EO_OFFLINE_TR(node, i);
			if (gpIfsf_shm->fp_offline_flag[node - 1][i] != 0) {
				upload_offline_tr(node, NULL);
			}

			// 5.��FP״̬ΪIdle��ʱ����·����
			for (j = 0; j < pump.panelCnt; ++j) {
				if ((__u8)FP_State_Idle != lpDispenser_data->fp_data[j].FP_State) {
					break;
				}
			}
			// �����涼���в��·����
			if (j >= pump.panelCnt) {
				ret = ChangePrice();            // �鿴�Ƿ�����Ʒ��Ҫ���,����ִ��
			}
			break;
		case FP_CALLING1:
		case FP_CALLING2:
			// Ԥ�ô���--̧ǹ�·� 
			if (pump.addInf[i][0] != 0) {           // ��Ԥ����δ�·�
				fp_state[i] = 1;
				DoStarted(i);                   
				/* Note! DoStarted Ҫ��DoRation֮ǰִ��,
				 * DoRation��Ҫ��ǰ̧ǹ��ǹ��, ��Ҫ��̧ǹ״̬�·�Ԥ����
				 */

				// �ж�̧����Ƿ�����ҪԤ�õ���ǹ, ������������
				if ((lpDispenser_data->fp_data[i].Log_Noz_State !=
					pump.addInf[i][2]) && pump.addInf[i][2] != 0xFF) {
					fp_state[i] = 0;
					break;
				}

				pump.addInf[i][0] = 0;
				ret = DoRation(i);
				// Ԥ�ô������, �ظ�����ȫ����ǹ����
				lpDispenser_data->fp_data[i].Log_Noz_Mask = 0xFF;
				break;
			}

			// ̧ǹ����
			if ((__u8)FP_State_Calling > lpDispenser_data->fp_data[i].FP_State) {
				ret = DoCall(i);
				fp_state[i] = 1;

				DoStarted(i);
			}

			break;
		case FP_AUTHORISED:
			if ((__u8)FP_State_Calling == lpDispenser_data->fp_data[i].FP_State) {
				ret = ChangeFP_State(node, i + 0x21, FP_State_Started, NULL);
			} else if ((__u8)FP_State_Authorised > lpDispenser_data->fp_data[i].FP_State) {
				ret = ChangeFP_State(node, i + 0x21, FP_State_Authorised, NULL);
			}
			break;
		case FP_FUELLING1:
		case FP_FUELLING2:
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

			if ((__u8)FP_State_Fuelling > lpDispenser_data->fp_data[i].FP_State) {
				if (fp_state[i] == 3) {
					ret = ChangeFP_State(node, i + 0x21, FP_State_Fuelling, NULL);
					if (ret < 0) {
						break;
					}
				}
			}

			break;
		case FP_SPFUELLING:
			if ((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State) {
				ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
			}
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
	int i;

	ret = SetHbStatus(node, 0);   // IFSF_HEARTBEAT.status set to 0
	if (ret < 0) {
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
			RunLog("�ڵ�[%d]���[%d], BeginTr4Fp Failed", node, i + 1);
			return -1;
		}
	}

	// GetCurrentLNZ & BeginTr4Fp �ɹ�, ����λ fp_state[i] Ϊ 2
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
	int ret;
	int pr_no, fm_idx;
	int trycnt;
	int fail_cnt = 0;
	unsigned long pre_value;
	__u8 price[2] = {0};
	__u8 volume[3] = {0};
	__u8 amount[3] = {0};

	// 0. ��λԤ�ñ��
	pump.addInf[pos][0] = 0;

	if (pump.gunLog[pos] == 0) {  // �ж�̧��ǹ���Ƿ��Ѿ�ȡ��
		return -1;
	}

	// 1. ��ȡ��ǹ����
	pr_no = (lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).PR_Id - 1;  // 0 - 7	
	if ((pr_no < 0) || (pr_no > 7)) {
		RunLog("Node[%d] DoRation PR_Id error, PR_Id[%d] <ERR>", node, pr_no + 1);
		return -1;
	}

	fm_idx = lpDispenser_data->fp_data[pos].Default_Fuelling_Mode - 1;
	if (fm_idx < 0 || fm_idx > 7) {
		RunLog("Node[%d] DoRation default FM error, fm[%d] <ERR>", node, fm_idx + 1);
		return -1;
	}
	price[0] = lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2];
	price[1] = lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3];

	// 2. �����Ӧ���������߽��
	if (pump.addInf[pos][1] == 0x4D) {          // Prepay (��еʽ�ͻ��Խ����Ϊ����
		memcpy(amount, &pump.addInf[pos][3], 3);
		pre_value = (Bcd2Long(amount, 3) * 10000) / Bcd2Long(price, 2);
		pre_value = (pre_value + 50) / 100; // ��λ

		RunLog("-------------pre_volume: %u -----------", pre_value);
		if (Long2BcdR(pre_value, volume, 3) != 0) {
			RunLog("Node[%d].FP[%d]Preset, Exec Long2Bcd Failed", node, pos + 1);
			return -1;
		}
	} else if (pump.addInf[pos][1] == 0x3D) {   // Preset (����ʽ�ͻ���������Ϊ����)
		memcpy(volume, &pump.addInf[pos][3], 3);
		pre_value = Bcd2Long(volume, 3) * Bcd2Long(price, 2) / 100;
		/*
		 * Note! ʵ�ʲ��Խ����ʾ���ͻ�(TH30ϵ��)���С������λ
		 */
		RunLog("-------------pre_amount: %u -----------", pre_value);

		if (Long2BcdR(pre_value, amount, 3) != 0) {
			RunLog("Node[%d].FP[%d]Preset, Exec Long2Bcd Failed", node, pos + 1);
			return -1;
		}
	} else {
		return -1;
	}
	

	// 3. �·�
	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {
		pump.cmd[0] = 0xF0 | pump.panelPhy[pos];
		pump.cmd[1] = ~pump.cmd[0];                       // double talk byte
		pump.cmd[2] = 0xA5;
		pump.cmd[3] = 0x5A;                               // ~pump.cmd[2]
		pump.cmd[4] = 0x05;                               // Slow Flow Offset
		pump.cmd[5] = 0xFA;

		pump.cmd[6] = price[1];                           // Unit_Price LSB
		pump.cmd[7] = ~pump.cmd[6];
		pump.cmd[8] = price[0];                           // Unit_Price MSB
		pump.cmd[9] = ~pump.cmd[8];
		pump.cmd[10] = amount[2];                         // Money LSB
		pump.cmd[11] = ~pump.cmd[10];
		pump.cmd[12] = amount[1];                         // Money 2SB     
		pump.cmd[13] = ~pump.cmd[12];
		pump.cmd[14] = amount[0];                         // Money MSB     
		pump.cmd[15] = ~pump.cmd[14];
		pump.cmd[16] = volume[2];                         // Volume LSB
		pump.cmd[17] = ~pump.cmd[16];
		pump.cmd[18] = volume[1];                         // Volume 2SB
		pump.cmd[19] = ~pump.cmd[18];
		pump.cmd[20] = volume[0];                         // Volume MSB
		pump.cmd[21] = ~pump.cmd[20];
		pump.cmdlen = 22;
		pump.wantLen = 2;

		Ssleep(INTERVAL); 
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]�·�Ԥ������ʧ��", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]�·�Ԥ������ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		} 

		if (pump.cmd[1] != (__u8)~pump.cmd[0]) {
			RunLog("Node[%d].FP[%d]��������У�����", node, pos + 1);
			continue;
		}

		if (pump.cmd[0] != FP_AUTHORISED) {
			RunLog("Node[%d].FP[%d]Ԥ������ִ��ʧ��", node, pos + 1);
			fail_cnt++;
			if (fail_cnt >= 2) {
				fail_cnt = 0;
				ReWritePrice(pos);
			}
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
 * Note: TMC�ͻ����������ָ������������ǹ�ĵ���, Ϊ����ִ����߱���ٶ�,
 *       ��ʵ�����յ�һ��Fuel�ı����������һ��ʱ��ĵȴ�(8����ѯ),
 *       �����к����ı������, ��һ��ִ��;
 */
static int ChangePrice() 
{
#define WAIT_CNT  8
	int i, ret, trycnt;
	int pr_no, tmp_pr_no;
	int lnz_idx, idx;
	int price_change_flag = 0;  // ����Ƿ�����Ʒ��Ҫ���, 0 - ����Ҫ, ���� - ��Ҫ
	int fm_idx;
	int error_code_msg_cnt = 0; // ��Ҫ������Fuel��error code��Ϣ������
	static int wait_cnt = WAIT_CNT;    // �ȴ�������������ʱ��, �Ա�һ������µ���һ���·�.
	__u8 err_code = 0x99;       // 0x99-�ɹ�, 0x37-ִ��ʧ��, 0x36-ͨѶ������ʧ��
	__u8 price_bak[64] = {0};   // �����·��ĵ������ݲ���, ���ڱȽϱ�۽��
	__u8 status;
	
/*
 * �����µ�������newPrice[FP_PR_NB_CNT], �������Ʒ��Ҫ���--����ֵ��Ϊ��,
 * ���������FP, �����Ʒƥ����·��������
 */
       
	price_change_flag = 0;
	for (pr_no = FP_PR_NB_CNT - 1; pr_no >= 0; pr_no--) {
//		RunLog(">>>1 newPrice[%d]: %02x.%02x", pr_no, pump.newPrice[pr_no][2], pump.newPrice[pr_no][3]);;
		if (pump.newPrice[pr_no][3] != 0x00 || pump.newPrice[pr_no][2] != 0x00) {
			RunLog("Node[%d].Pr_Id[%d]��Ҫ���", node, pr_no);
			price_change_flag = 1;
			break;
		}
	}

	if (price_change_flag == 0) { 
		return 0;
	}
	
	if (wait_cnt > 0) {
		wait_cnt--;
		return 0;
	} else {
		wait_cnt = WAIT_CNT;
	}

	for (i = 0; i < pump.panelCnt; i++) {
		tmp_pr_no = 0;
		CriticalLog("�·����������ڵ�[%02d]��[%d]ǹ[*]......", node, i + 1);
		fm_idx = lpDispenser_data->fp_data[i].Default_Fuelling_Mode - 1;
		if (fm_idx < 0 || fm_idx > 7) {
			RunLog("Node[%d]FP[%d] Default FM Error[%d]", node, i + 1, fm_idx + 1);
			return -1;
		}

		err_code = 0x99;
		error_code_msg_cnt = 0;

		for (trycnt = 0; trycnt < TRY_TIME; trycnt++) {
			// ���״̬�Ƿ�Ϊ2F,��������ȳ�ʼ��, �����۲��ɹ�
			GetPumpStatus(i, &status);              

			// �·����
			price_change_flag = 0;          // ��λ���
			Ssleep(INTERVAL * 2);
			pump.cmd[0] = 0xC0 | pump.panelPhy[i];
			pump.cmd[1] = ~pump.cmd[0];     // double talk byte
			pump.cmd[2] = 0xA3;
			pump.cmd[3] = 0x5C;             // ~pump.cmd[2]

			idx = 4;
			for (lnz_idx = 0; lnz_idx < pump.gunCnt[i]; ++lnz_idx) {
				pr_no = lpDispenser_data->fp_ln_data[i][lnz_idx].PR_Id - 1;
				if ((pr_no < 0) || (pr_no > 7)) {
					RunLog("ChangePrice PR_Id, PR_Id[%d] <ERR>", pr_no + 1);
					pr_no = 0;
					/*
					 * ���ͻ������õ���ǹ, ��ʵ��δʹ�õ�����ᷢ�������.
					 * ��: �ͻ���������������ǹ,��ʵ��ֻ�ӳ���1#ǹ,��ôWEB�����ϲ�������2#ǹ����Ʒ,
					 *     PR_IDĬ��Ϊ0,���Ǳ������������ǹ�ĵ����ֲ���Ϊ��,����pr_no��Ϊ0,
					 *     ָ��һ����Ʒ;
					 */
				}

				RunLog("--- pr_no: %d, newPrice:%02x%02x, idx: %d", pr_no, 
						pump.newPrice[pr_no][2], pump.newPrice[pr_no][3], idx);
				/*
				 * ˵��: ���ͻ�һ���ܹ�4ǹ4��Ʒ, ����ͻ���������ֻʹ�� n ����Ʒ(n<4),
				 *       ��ô 1-n ��ǹ����һ������Ϊ��, ������Щǹʵ�����Ƿ��нӳ���ʹ��;
				 *       ��һ����, (n+1) - 4 ��ǹ�ĵ��������Ϊ��, ������ʧ��.
				 */
				// cash price
				if (lnz_idx >= nb_prods[i]) {
					// a.δ������Ʒ����ǹ(��������Ϊ��)
					pump.cmd[idx + 0] = 0x00;
					pump.cmd[idx + 1] = 0xff;
					pump.cmd[idx + 2] = 0x00;
					pump.cmd[idx + 3] = 0xff;
				} else if (pump.newPrice[pr_no][3] != 0x00 || pump.newPrice[pr_no][2] != 0x00) {
					// b. ��������Ʒ, ����Ҫ��۵���ǹ
					pump.cmd[idx + 0] = pump.newPrice[pr_no][2];        // MSB First
					pump.cmd[idx + 1] = ~pump.cmd[idx + 0];
					pump.cmd[idx + 2] = pump.newPrice[pr_no][3];
					pump.cmd[idx + 3] = ~pump.cmd[idx + 2];

					tmp_pr_no = pr_no;
					error_code_msg_cnt += pump.newPrice[pr_no][0];
					price_change_flag = 1;
				} else {
					// c. ��������Ʒ, �������۵���ǹ
					pump.cmd[idx + 0] = lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2];
					pump.cmd[idx + 1] = ~pump.cmd[idx + 0];
					pump.cmd[idx + 2] = lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3];
					pump.cmd[idx + 3] = ~pump.cmd[idx + 2];

				}

				// credit price
				memcpy(&pump.cmd[idx + 4], &pump.cmd[idx], 4);

				// ���ݵ����������ڱ����ɺ��ȡ���۽��бȽ� 
				price_bak[lnz_idx * 4 + 0] = pump.cmd[idx + 2];
				price_bak[lnz_idx * 4 + 1] = pump.cmd[idx + 3];
				price_bak[lnz_idx * 4 + 2] = pump.cmd[idx + 0];
				price_bak[lnz_idx * 4 + 3] = pump.cmd[idx + 1];

				idx += 8;
			}

			if (price_change_flag == 0) {
				RunLog("Node[%d].FP[%d]������", node, i + 1);
				break;    // ������һ��FP
			}
			
			pump.cmdlen = idx;
			pump.wantLen = 2;


			ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd, 
					pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
			if (ret < 0) {
				RunLog("Node[%d].FP[%d]�·��������ʧ��", node, i + 1);
				continue;
			}

			RunLog("Node[%d].FP[%d].ChangePrice_CMD: %s",
					node, i + 1, Asc2Hex(pump.cmd, pump.cmdlen));

			memset(pump.cmd, 0, sizeof(pump.cmd));
			pump.cmdlen = sizeof(pump.cmd) - 1;

			ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
			if (ret < 0) {
				RunLog("Node[%d].FP[%d]���������ɹ�, ȡ������Ϣʧ��", node, i + 1);
				continue;
			}

			if (pump.cmd[1] != (__u8)~pump.cmd[0]) {
				RunLog("Node[%d].FP[%d]��������У�����", node, i + 1);
				Ssleep(PRICE_CHANGE_RETRY_INTERVAL - 3 * INTERVAL);
				continue;
			}

			GetPumpStatus(i, &status);   // ���֮��״̬���Ϊ2F
			sleep(1);                    // �ȴ�, ȷ��˰���Ѿ�������������

			ret = GetPrice(i);
			if (ret < 0) {
				RunLog("Node[%d].FP[%d]������ʧ��", node, i + 1);
				continue;
			}

			if (memcmp(pump.cmd, price_bak, pump.gunCnt[i] * 4) != 0) {
				RunLog("Node[%d].FP[%d]ִ�б��ʧ��", node, i + 1);
				err_code = 0x37;
				Ssleep(PRICE_CHANGE_RETRY_INTERVAL - 1000000 - (4 * INTERVAL));
				continue;
			}

			// ��ӡ��־, ����FM���ݿ�
			for (lnz_idx = 0; lnz_idx < pump.gunCnt[i]; ++lnz_idx) {
				pr_no = lpDispenser_data->fp_ln_data[i][lnz_idx].PR_Id - 1;
				if ((pr_no < 0) || (pr_no > 7)) {
					RunLog("ChangePrice PR_Id, PR_Id[%d] <ERR>", pr_no + 1);
					pr_no = 0;
					continue;
				}
				if ((pump.newPrice[pr_no][2] == 0x00) && (pump.newPrice[pr_no][3]) == 0) {
					continue;
				}
			

				memcpy(&(lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price),
									&pump.newPrice[pr_no], 4);

				RunLog("Node[%d]��۳ɹ�,�µ���fp_fm_data[%d][%d].Prode_Price: %s", node, pr_no,
					fm_idx, Asc2Hex(lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price, 4));

				CriticalLog("�ڵ�[%02d]��[%d]ǹ[%d]��۳ɹ�,�µ���Ϊ%02x.%02x",
									node, i + 1, lnz_idx + 1,
					lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2],
					lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3]);

			}

			break;						
		} // end of for trycnt < TRY_TIME

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
					pump.newPrice[tmp_pr_no][2], 
					pump.newPrice[tmp_pr_no][3], err_code);
		}

		RunLog("---------- Node[%d]FP[%d] ER_DATA, error code: %02x -----", node, i + 1, err_code);

		int tmp_idx;
		for (tmp_idx = 0; tmp_idx < error_code_msg_cnt; tmp_idx++) {
			ChangeER_DATA(node, 0x21 + i, err_code, NULL); // Translater�Ѿ��ظ���ACK
		}

		if (err_code != 0x99) {
			ChangeFP_State(node, 0x21 + i, FP_State_Inoperative, NULL);
		}

		Ssleep(PRICE_CHANGE_RETRY_INTERVAL - 1000000 - (4 * INTERVAL));
	} // end of for (i = 0; i < pump.panelCnt; )

	//��ȫ����嶼ִ�б�ۺ�������
	memset(&pump.newPrice, 0, FP_PR_NB_CNT * 4);

	return 0;
}

/*
 * func: ��ȡĳ��FP����ǹ����Ʒ����, LSB First
 */
static int GetPrice(int pos)
{
	int ret;
	int trycnt;

	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {
		pump.cmd[0] = 0xF0 | pump.panelPhy[pos];
		pump.cmd[1] = ~pump.cmd[0];     // double talk byte
		pump.cmd[2] = 0xAC;
		pump.cmd[3] = 0x53;             // ~pump.cmd[2]
		pump.cmdlen = 4;
		pump.wantLen = (pump.gunCnt[pos] * 2 * 2) + 2;

		Ssleep(INTERVAL); 
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]�·�����������ʧ��", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]�·�����������ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		} 

		if (DoubleTalkCheck(pump.cmd, pump.cmdlen) != 0) {
			RunLog("Node[%d].FP[%d]��������У�����", node, pos + 1);
			continue;
		}

		return 0;
	}

	return -1;
}

/*
* func: ���ͽ��������н��׼�¼�Ķ�ȡ�ʹ���
*       �㽻�׵��жϺʹ�����DoEot����, DoPoling ������
* ret : 0 - �ɹ�; -1 - ʧ��
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
	__u8 pre_total[7];
	__u8 total[7];
	__u8 vol_total[NOZ_TOTAL_LEN];
	__u8 amount_total[NOZ_TOTAL_LEN];

	RunLog("�ڵ�[%d]���[%d]�����½���", node, pos + 1);

	err_cnt = 0;
	do {
		ret = GetGunTotal(pos, pump.gunLog[pos], vol_total, amount_total);	// ȡ��ǹ����
		if (ret < 0) {
			err_cnt++;
			RunLog("Node[%d].FP[%d]��ȡ����ʧ�� <ERR>", node, pos + 1);
		}
	} while (ret < 0 && err_cnt <= 5);
	
	if (ret < 0) {
		return -1;
	}

	pr_no = (lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).PR_Id - 1;  // 0 - 7	
	if ((pr_no < 0) || (pr_no > 7)) {
		RunLog("DoEot PR_Id error, PR_Id[%d] <ERR>", pr_no + 1);
		pr_no = 0;
		/*
		 * �������쳣
		 */
	}
	
	memcpy(pre_total, (lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Vol_Total, 7);

	//vol_total
	total[0] = 0x0a; 
	total[1] = 0x00;	
	memcpy(&total[2], vol_total, NOZ_TOTAL_LEN);

	ret = memcmp(total, pre_total, sizeof(total));
	if (ret != 0) {
		err_cnt= 0;
		do {
			ret = GetDisplayData(pos);	// ȡ���һ�ʼ��ͼ�¼
			if (ret != 0) {
				err_cnt++;
				RunLog("Node[%d].FP[%d]��ȡ���׼�¼ʧ�� <ERR>", node, pos + 1);
			}
		} while (ret < 0 && err_cnt <=5);

		if (ret < 0) {
			return -1;
		}

		volume[0] = 0x06;               
		volume[1] = 0x00;
		volume[2] = pump.cmd[14];
		volume[3] = pump.cmd[12];
		volume[4] = pump.cmd[10];
		
		memcpy(lpDispenser_data->fp_data[pos].Current_Volume, volume, 5);

		amount[0] = 0x06;
		amount[1] = 0x00;
		amount[2] = pump.cmd[8];
		amount[3] = pump.cmd[6];
		amount[4] = pump.cmd[4];
		
		memcpy(lpDispenser_data->fp_data[pos].Current_Amount, amount, 5);

		price[0] = 0x04;		
		price[1] = 0x00;
		price[2] = pump.cmd[2];
		price[3] = pump.cmd[0];
		
		memcpy(lpDispenser_data->fp_data[pos].Current_Unit_Price, price, 4);

		memcpy(lpDispenser_data->fp_m_data[pr_no].Meter_Total, total, 7);
		memcpy((lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Vol_Total, total, 7);

		// Update Amount_Total
		total[0] = 0x0a; 
		total[1] = 0x00;	
		memcpy(&total[2], amount_total, NOZ_TOTAL_LEN);
		
		memcpy((lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Amount_Total, total, 7);
		memcpy(lpDispenser_data->fp_data[pos].Current_Prod_Nb, lpDispenser_data->fp_pr_data[pr_no].Prod_Nb, 4);

		err_cnt = 0;
		do {
			ret = EndTr4Ln(node, pos + 0x21, pump.gunLog[pos] + 0x10);
			if (ret < 0) {
				RunLog("�ڵ�[%d]���[%d]DoEot.EndTr4Lnʧ��, ret: %d <ERR>", node, pos + 1, ret);
				err_cnt++;
			}
		} while (ret < 0 && err_cnt <= 5);

		err_cnt = 0;
		do {
			ret = EndTr4Fp(node, pos + 0x21, pre_total, 
				(lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Vol_Total);
			if (ret < 0) {
				RunLog("�ڵ�[%d]���[%d] DoEot.EndTr4Fpʧ��, ret: %d <ERR>", node, pos + 1, ret);
				err_cnt++;
			}
		} while (ret < 0 && err_cnt <= 8);

	} else {
		ret = EndZeroTr4Fp(node, pos + 0x21);
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d] DoEot.EndZeroTr4Fp ʧ��, ret: %d <ERR>", node, pos + 1, ret);
		}
	}
	
	
	return 0;
}


/*
 * func: ��ȡ��FP����ǹ�ı���
 */
static int GetPumpTotal(int pos)
{
	return -1;
}

/*
 * func: ȡĳ��ǹ�ı���
 */ 
static int GetGunTotal(int pos, __u8 gunLog, __u8 *vol_total, __u8 *amt_total)
{
	int i;
	int ret;

	for (i = 0; i < TRY_TIME; ++i) {
		pump.cmd[0] = 0xF0 | pump.panelPhy[pos];
		pump.cmd[1] = ~pump.cmd[0];                 // double talk byte
		pump.cmd[2] = 0xA9;
		pump.cmd[3] = 0x56;                         // ~pump.cmd[2]
		pump.cmd[4] = 0x40 | ((gunLog - 1) << 1);   // single hose,running total,cash
		pump.cmd[5] = ~pump.cmd[4];
		pump.cmdlen = 6;
		pump.wantLen = 22;

		Ssleep(INTERVAL); 
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]�·�����������ʧ��", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]�·�����������ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		} 

		if (DoubleTalkCheck(pump.cmd, pump.cmdlen) != 0) {
			RunLog("Node[%d].FP[%d]��������У�����", node, pos + 1);
			continue;
		}

		vol_total[0] = pump.cmd[8];
		vol_total[1] = pump.cmd[6];
		vol_total[2] = pump.cmd[4];
		vol_total[3] = pump.cmd[2];
		vol_total[4] = pump.cmd[0];

		amt_total[0] = pump.cmd[18];
		amt_total[1] = pump.cmd[16];
		amt_total[2] = pump.cmd[14];
		amt_total[3] = pump.cmd[12];
		amt_total[4] = pump.cmd[10];

		return 0;
	}

	return -1;
}


/*
 * func: ��ͣ���ͣ��ڴ���Terminate_FP��
 *       ����ȡ����Ȩ
 */
static int DoStop(int pos)
{
	int ret;
	int trycnt;

	RunLog("Node %d FP %d DoStop", node, pos + 1);

	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {
		pump.cmd[0] = 0xF0 | pump.panelPhy[pos];
		pump.cmd[1] = ~pump.cmd[0];     // double talk byte
		pump.cmd[2] = 0xA3;
		pump.cmd[3] = 0x5C;             // ~pump.cmd[2]
		pump.cmdlen = 4;
		pump.wantLen = 2;

		Ssleep(INTERVAL); 
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]�·�ֹͣ����ʧ��", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]�·�ֹͣ����ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		} 

		if (pump.cmd[1] != (__u8)~pump.cmd[0]) {
			RunLog("Node[%d].FP[%d]��������У�����", node, pos + 1);
			continue;
		}

		if ((pump.cmd[0] != FP_IDLE) && (pump.cmd[0] != FP_SPFUELLING)) {
			RunLog("Node[%d].FP[%d]ִ��ֹͣ��������ʧ��", node, pos + 1);
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
	int trycnt, ret;
	int pr_no, fm_idx;
	int fail_cnt = 0;

	RunLog("Node %d FP %d Do Auth", node, pos + 1);

	pr_no = (lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).PR_Id - 1;  // 0 - 7	
	if ((pr_no < 0) || (pr_no > 7)) {
		RunLog("Node[%d] Do Auth PR_Id error, PR_Id[%d] <ERR>", node, pr_no + 1);
		return -1;
	}

	fm_idx = lpDispenser_data->fp_data[pos].Default_Fuelling_Mode - 1;
	if (fm_idx < 0 || fm_idx > 7) {
		RunLog("Node[%d] Do Auth default FM error, fm[%d] <ERR>", node, fm_idx + 1);
		return -1;
	}

	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {
		pump.cmd[0] = 0xF0 | pump.panelPhy[pos];
		pump.cmd[1] = ~pump.cmd[0];     // double talk byte
		pump.cmd[2] = 0xA5;
		pump.cmd[3] = 0x5A;             // ~pump.cmd[2]
		pump.cmd[4] = 0x05;
		pump.cmd[5] = 0xFA;
		pump.cmd[6] = lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3];
		pump.cmd[7] = ~pump.cmd[6];
		pump.cmd[8] = lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2];
		pump.cmd[9] = ~pump.cmd[8];
		pump.cmd[10] = 0x00;
		pump.cmd[11] = 0xff;
		pump.cmd[12] = 0x00;
		pump.cmd[13] = 0xff;
		pump.cmd[14] = 0x00;
		pump.cmd[15] = 0xff;
		pump.cmd[16] = 0x00;
		pump.cmd[17] = 0xff;
		pump.cmd[18] = 0x00;
		pump.cmd[19] = 0xff;
		pump.cmd[20] = 0x00;
		pump.cmd[21] = 0xff;
		pump.cmdlen = 22;
		pump.wantLen = 2;

		Ssleep(INTERVAL); 
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]�·���Ȩ����ʧ��", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]�·���Ȩ����ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		} 

		if (pump.cmd[1] != (__u8)~pump.cmd[0]) {
			RunLog("Node[%d].FP[%d]��������У�����", node, pos + 1);
			continue;
		}
		
		if (pump.cmd[0] == FP_IDLE) {
			return -2;
		}
		
		if ((pump.cmd[0] != FP_AUTHORISED) &&(pump.cmd[0] != FP_FUELLING1)&&(pump.cmd[0] != FP_FUELLING2)) {
			RunLog("Node[%d].FP[%d]ִ����Ȩ��������ʧ��", node, pos + 1);
			fail_cnt++;
			if (fail_cnt >= 2) {
				fail_cnt = 0;
				ReWritePrice(pos);
			}
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
	__u8 amount[5] = {0};
	__u8 volume[5] = {0};
	const __u8 arr_zero[3] = {0};

	ret = GetDisplayData(pos);
	if (ret < 0) {
		return -1;
	}

	volume[0] = 0x06;
	volume[1] = 0x00;
	volume[2] = pump.cmd[14];
	volume[3] = pump.cmd[12];
	volume[4] = pump.cmd[10];

	// ֻ�е���������Ϊ���˲��ܽ�FP״̬�ı�ΪFuelling
	if (fp_state[pos] != 3) {
		if (memcmp(&volume[2], arr_zero, 3) == 0) {
			return 0;
		}
		fp_state[pos] = 3;
		return 0;
	}

	amount[0] = 0x06;
	amount[1] = 0x00;
	amount[2] = pump.cmd[8];
	amount[3] = pump.cmd[6];
	amount[4] = pump.cmd[4];

	if ((__u8)FP_State_Fuelling == lpDispenser_data->fp_data[pos].FP_State) {
		ret = ChangeFP_Run_Trans(node, 0x21 + pos, amount,  volume);
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
	int trycnt;
	__u8 ln_status;

	ret = GetCurrentLNZ(pos);
	if (ret < 0) {
	//	RunLog("��ȡ��ǰ��ǹǹ��ʧ��");
		return -1;
	}

	ret = ChangeFP_State(node, pos + 0x21, FP_State_Calling, NULL);

	//��λ������,���������Զ���Ȩ;��λ��������, ��Ȩ�������ڿ���״̬,��ʱ�Զ���Ȩ
	if ((gpIfsf_shm->auth_flag == FCC_SWITCH_AUTH) && (lock_status() == 1)) {
		ret = DoAuth(pos);
		if (ret < 0) {
			RunLog("Node %d FP %d �Զ���Ȩʧ��", node, pos + 1);
		}
		ret = ChangeFP_State(node, pos + 0x21, FP_State_Started, NULL);
		RunLog("Node %d FP %d �Զ���Ȩ�ɹ�", node, pos + 1);
		SO_OFFLINE_TR(node, pos);
	}

	return ret;
}

/*
 * func: ��ȡFP��ʶ, �����ж��ͻ�����
 *       ������ȡ����, ȡ����IDֵһ�²ŷ��سɹ�
 *
 * return: 0 -�ɹ�; -1 - ʧ��.
 */
static int GetPumpID(int pos, __u8 *id)
{
	int trycnt, ret;
	__u8 pre_id = 0;

	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {
		pump.cmd[0] = 0xF0 | pump.panelPhy[pos];
		pump.cmd[1] = ~pump.cmd[0];     // double talk byte
		pump.cmd[2] = 0xA0;
		pump.cmd[3] = 0x5F;             // ~pump.cmd[2]
		pump.cmdlen = 4;
		pump.wantLen = 2;

		Ssleep(INTERVAL); 
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]�·���FP��ʶIDʧ��", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]�·���FP��ʶID�ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		} 

		if (pump.cmd[1] != (__u8)~pump.cmd[0]) {
			RunLog("Node[%d].FP[%d]��������У�����", node, pos + 1);
			continue;
		}

		if (pre_id == 0) {
			pre_id = pump.cmd[0];
			continue;
		} else if (pre_id != pump.cmd[0]) {
			return -1;
		}

		*id = pump.cmd[0];
		RunLog("Node[%d].FP[%d] IDΪ[%02x]", node, pos + 1, *id);

		return 0;
	}

	return -1;
}

/*
 * func: ��ȡFP״̬
 */
static int GetPumpStatus(int pos, __u8 *status)
{
	int trycnt, ret;

	for (trycnt = 0; trycnt < 2; ++trycnt) {
		pump.cmd[0] = 0xF0 | pump.panelPhy[pos];
		pump.cmd[1] = ~pump.cmd[0];     // double talk byte
		pump.cmd[2] = 0xA2;
		pump.cmd[3] = 0x5D;             // ~pump.cmd[2]
		pump.cmdlen = 4;
		pump.wantLen = 2;

		Ssleep(INTERVAL); 
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]�·���״̬����ʧ��", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]�·���״̬����ɹ�, ȡ������Ϣʧ��",
									node, pos + 1);
			continue;
		} 

		if (pump.cmd[1] != (__u8)~pump.cmd[0]) {
			RunLog("Node[%d].FP[%d]��������У�����", node, pos + 1);
			continue;
		}

		*status = pump.cmd[0];

		if (*status == FP_INOPERATIVE) {
			ret = SetDisplayData(pos);
		} else if (*status != FP_IDLE) {
			ack_deactived_hose_flag[pos] = 1;
		}

		return 0;
	}

	return -1;
}

/*
 * func: ��ȡFP��ʾ����(ʵʱ����&����)
 *       ���״���ʱ�൱�� GetTransData
 */
static int GetDisplayData(int pos)
{
	int trycnt, ret;

	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {
		pump.cmd[0] = 0xF0 | pump.panelPhy[pos];
		pump.cmd[1] = ~pump.cmd[0];     // double talk byte
		pump.cmd[2] = 0xA1;
		pump.cmd[3] = 0x5E;             // ~pump.cmd[2]
		pump.cmdlen = 4;
		pump.wantLen = 18;

		Ssleep(INTERVAL); 
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]�·���FP��ʾ����ʧ��", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]�·���FP��ʾ���ݳɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		} 

		if (DoubleTalkCheck(pump.cmd, pump.cmdlen) != 0) {
			RunLog("Node[%d].FP[%d]��������У�����", node, pos + 1);
			continue;
		}

#if 0
		/*
		 * UDCЭ��Ҫ��2F״̬Ҫ�������һ�ʽ��׵����ݵ�����, �����������,
		 * �ͻ������Լ����֮������ʾ����Ϻ�, 
		 * ��Ϊ�����ڼ���������ô����ֵ����֪, 
		 * ����ۺ󵥼��뽻�����ݿ��ܲ�ƥ��,������ɿͻ����,
	 	 * �������ﲻ����display_data, Guojie Zhu @ 2009.06.15
		 *
		 * SetDisplayDataֻ����ȫ����������.
		 * ��ۺ��ͻ��ϲ鵱�ν���Ҳ����ʾȫ��.
		 *
		 */
		// ���浱�ν���, ���ڳ�ʼ��ʱ��������
		/*
		if (pump.cmd[16] == FP_IDLE) {
			memcpy(&display_data[pos], pump.cmd, 16);
		}
		*/
#endif

		return 0;
	}

	return -1;
}

/*
 * func: ����FP��������
 */
static int SetDisplayData(int pos)
{
	int trycnt, ret;

	RunLog("Node[%d].FP[%d]��������ʾ����", node, pos + 1);

	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {
		pump.cmd[0] = 0xF0 | pump.panelPhy[pos];
		pump.cmd[1] = ~pump.cmd[0];     // double talk byte
		pump.cmd[2] = 0xA6;
		pump.cmd[3] = 0x59;             // ~pump.cmd[2]

		memcpy(&pump.cmd[4], display_data[pos], 16);
		pump.cmdlen = 20;
		pump.wantLen = 2;

		Ssleep(INTERVAL); 
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]�·���������ʾ����ʧ��", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]�·���������ʾ���ݳɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		} 

		if (pump.cmd[1] != (__u8)~pump.cmd[0]) {
			RunLog("Node[%d].FP[%d]��������У�����", node, pos + 1);
			continue;
		}

		if (pump.cmd[0] == FP_INOPERATIVE) {
			continue;
		}

		return 0;
	}

	return -1;
}

//test wether the dispenser is online, we only use fp 1 to test.
/*
 * func: �ж�Dispenser�Ƿ�����
 *       ��ģ��ʹ��GetDisplayData���
 * static int GetPumpOnline(int pos);
 */


/*
 * func: ��ȡ��ǰ̧����ǹ��ǹ��, ���д��pump.gunLog[] & fp_data[].Log_Noz_State
 * ret : 0 - �ɹ�; -1 - ʧ�� ; -2 - δѡ����ǹ
 */
static int GetCurrentLNZ(int pos)
{
	int ret, trycnt;

	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {
		pump.cmd[0] = 0xC0 | pump.panelPhy[pos];
		pump.cmd[1] = ~pump.cmd[0];     // double talk byte
		pump.cmd[2] = 0xA1;
		pump.cmd[3] = 0x5E;             // ~pump.cmd[2]
		pump.cmdlen = 4;
		pump.wantLen = 2;

		Ssleep(INTERVAL); 
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]�·�ȡǹ������ʧ��", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]�·�ȡǹ������ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		} 

		if (pump.cmd[1] != (__u8)~pump.cmd[0]) {
			RunLog("Node[%d].FP[%d]��������У�����", node, pos + 1);
			continue;
		}

		pump.gunLog[pos] = (pump.cmd[0] & 0x7F);  // ��ǰ̧�����ǹ	
		if (pump.gunLog[pos] == 0) {
			RunLog("Node[%d].FP[%d]δѡ����ǹ", node, pos + 1);
			return -2;
		}

		RunLog("Node %d FP %d �� %d ��ņ̀��", node, pos + 1, pump.gunLog[pos]);
		lpDispenser_data->fp_data[pos].Log_Noz_State = (1 << (pump.gunLog[pos] - 1));

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
	int pr_no, fm_idx;
	int ret;

	for (i = 0; i < pump.panelCnt; i++) {
		ret = GetPrice(i);
		if (ret < 0) {
			return -1;
		}

		nb_prods[i] = pump.cmd[pump.cmdlen - 2];
		RunLog("Node[%d].FP[%d]��Ʒ����Ϊ%d", node, i + 1, nb_prods[i]);

		fm_idx = lpDispenser_data->fp_data[i].Default_Fuelling_Mode - 1;
		if (fm_idx < 0 || fm_idx > 7) {
			RunLog("Node[%d] default FM error, fm[%d] <ERR>", node, fm_idx + 1);
			return -1;
		}

		//for (j = 0; j < pump.gunCnt[i]; j++) {
		for (j = 0; j < nb_prods[i]; j++) {
			pr_no = (lpDispenser_data->fp_ln_data[i][j]).PR_Id - 1;  // 0-7	
			if ((pr_no < 0) || (pr_no > 7)) {
				RunLog("InitPumpPrice PR_Id error, PR_Id[%d]", pr_no + 1);
				pr_no = 0;
				continue;
			}

			// ������Ҫʹ��fp_fm_data�е���������, ���Ա��뱣֤fp_fm_data��������ȷ
			if ((pump.cmd[j * 4 + 2] != 
				lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2]) ||
			     (pump.cmd[j * 4 + 0] != 
				lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3])) {
				lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2] = pump.cmd[j * 4 + 2];
				lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3] = pump.cmd[j * 4 + 0];
				RunLog("ע��! Node[%d].FP[%d]Noz[%d]����Ʒ����������ʵ�ʲ�ƥ��", node, i + 1, j + 1);
			}

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
	__u8 total[7] = {0};
	__u8 vol_total[NOZ_TOTAL_LEN] = {0};
	__u8 amt_total[NOZ_TOTAL_LEN] = {0};

	for (i = 0; i < pump.panelCnt; i++) {
		for (j = 0; j < pump.gunCnt[i]; ++j) {
			ret = GetGunTotal(i, j + 1, vol_total, amt_total);
			if (ret < 0) {
				RunLog("ȡ Node %d FP %d LNZ %d �ı���ʧ��", node, i, j);
				return -1;
			}

			pr_no = (lpDispenser_data->fp_ln_data[i][j]).PR_Id - 1;  // 0-7	
			if ((pr_no < 0) || (pr_no > 7)) {
				RunLog("InitPumpTotal pr_no error, FP %d, LNZ %d, PR_Id %d", i, j + 1, pr_no + 1);
				pr_no = 0;
				continue;
			}
			
			// Volume_Total
			total[0] = 0x0a;
			total[1] = 0x00;
			memcpy(&total[2], vol_total, NOZ_TOTAL_LEN);
			memcpy(lpDispenser_data->fp_m_data[pr_no].Meter_Total, total, 7);
			memcpy((lpDispenser_data->fp_ln_data[i][j]).Log_Noz_Vol_Total, total, 7);

			RunLog("Init.fp_ln_data[%d][%d].Vol_Total: %s ", i, j,
					Asc2Hex(lpDispenser_data->fp_ln_data[i][j].Log_Noz_Vol_Total, 7));
			RunLog("Init.fp_m_data[%d].Meter_Total[%s] ", pr_no, Asc2Hex(total, 7));
			RunLog("�ڵ�[%d]��[%d]�߼�ǹ��[%d]�ı���Ϊ[%s]", node, i + 1, 
				j + 1, Asc2Hex(vol_total, NOZ_TOTAL_LEN));
			
			// Amount_Total
			total[0] = 0x0a;
			total[1] = 0x00;
			memcpy(&total[2], amt_total, NOZ_TOTAL_LEN);
			memcpy((lpDispenser_data->fp_ln_data[i][j]).Log_Noz_Amount_Total, total, 7);
			RunLog("Init.fp_ln_data[%d][%d].Amount_Total: %s ", i, j,
				Asc2Hex(lpDispenser_data->fp_ln_data[i][j].Log_Noz_Amount_Total, 7));
		}
	}


	return 0;
}

/*
 * func: ���ȴ��ͻ����ڿ���״̬, �Խ��к�������
 * return: -1 - ʧ��, ��Ҫ����; 0 - �ɹ�, �ڵ��������
 */
static int Wait4PumpReady() 
{
	int i, ret;
	__u8 status = 0;

	// 1. �ȴ�����FP״̬Ϊ����
	do {
		for (i = 0; i < pump.panelCnt; ++i) {
			ret = GetPumpStatus(i, &status);
			if (ret < 0) {
				break;
			}

			if (status != FP_IDLE) {
				ret = -1;
				break;
			}
		}
		// ���1s������
		sleep(1);
	} while (ret < 0);

	// 2. ���͹�ǹȷ��
	for (i = 0; i < pump.panelCnt; ++i) {
		ret = AckDeactivedHose(i);
		if (ret < 0) {
			return -1;
		}
	}

	// 3. ��ȡ���һ�ʽ���
	for (i = 0; i < pump.panelCnt; ++i) {
		ret = GetDisplayData(i);
		if (ret < 0) {
			return -1;
		}
	}

	// 4. ͬ�����õ��� (���ͻ��ϵ������������, ��Ҫ�ط����۲�����Ȩ�ɹ�)
	for (i = 0; i < pump.panelCnt; i++) {
		ret = ReWritePrice(i);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]���ͳ�ʼ������ʧ��", node, i + 1);
			return -1;
		}
	}

	return 0;
}


/*
 * func: ��ǹȷ�� - �����ͻ�ȷ���յ���ǹ��Ϣ
 */
static int AckDeactivedHose(int pos)
{
	int ret;
	int trycnt;

	RunLog("Node[%d].FP[%d]���͹�ǹȷ��", node, pos + 1);

	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {
		pump.cmd[0] = 0xC0 | pump.panelPhy[pos];
		pump.cmd[1] = ~pump.cmd[0];     // double talk byte
		pump.cmd[2] = 0xA2;
		pump.cmd[3] = 0x5D;             // ~pump.cmd[2]
		pump.cmdlen = 4;
		pump.wantLen = 2;

		Ssleep(INTERVAL); 
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]�·���ǹȷ������ʧ��", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]�·���ǹȷ������ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		} 

		if (pump.cmd[1] != (__u8)~pump.cmd[0]) {
			RunLog("Node[%d].FP[%d]��������У�����", node, pos + 1);
			continue;
		}
		
		if (pump.cmd[0] != 0xB0) {
			continue;
		}

		return 0;
	}

	return -1;
}


/*
 * func: ��д����--��ʼ��ʱʹ��,
 *       ��ʱ���⵼�¼��ͻ���Ȩ����ִ��ʧ��ͨ���ط����ۿ��Խ��
 */
static int ReWritePrice(int pos)
{
	int ret, cmd_len;
	int idx1, idx2, lnz;
	int trycnt;
	__u8 status = 0;
	__u8 buf[64] = {0};

	RunLog("Node[%d].FP[%d]���ͳ�ʼ������", node, pos + 1);

	ret = GetPrice(pos);
	if (ret < 0) {
		return -1;
	}

	cmd_len = (pump.cmdlen - 2) * 2;
	for (lnz = 0; lnz < (pump.cmdlen - 2) / 4; lnz++) {
		idx1 = lnz * 8;
		idx2 = lnz * 4;

		// cash price
		buf[idx1 + 0] = pump.cmd[idx2 + 2];
		buf[idx1 + 1] = pump.cmd[idx2 + 3];
		buf[idx1 + 2] = pump.cmd[idx2 + 0];
		buf[idx1 + 3] = pump.cmd[idx2 + 1];

		// credit price
		buf[idx1 + 4] = buf[idx1 + 0];
		buf[idx1 + 5] = buf[idx1 + 1];
		buf[idx1 + 6] = buf[idx1 + 2];
		buf[idx1 + 7] = buf[idx1 + 3];
	}

	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {
		GetPumpStatus(pos, &status);              

		pump.cmd[0] = 0xC0 | pump.panelPhy[pos];
		pump.cmd[1] = ~pump.cmd[0];     // double talk byte
		pump.cmd[2] = 0xA3;
		pump.cmd[3] = 0x5C;             // ~pump.cmd[2]
		memcpy(&pump.cmd[4], buf, cmd_len);
		pump.cmdlen = 4 + cmd_len;
		pump.wantLen = 2;

		Ssleep(INTERVAL * 2); 
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]��ʼ����������ʧ��", node, pos + 1);
			continue;
		}

		RunLog("Node[%d].FP[%d].PriceData: %s",
				node, pos + 1, Asc2Hex(pump.cmd, pump.cmdlen));

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]��ʼ���������ݳɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		} 

		if (pump.cmd[1] != (__u8)~pump.cmd[0]) {
			RunLog("Node[%d].FP[%d]��������У�����", node, pos + 1);
			continue;
		}
		
		if (pump.cmd[0] != 0xB0) {
			continue;
		}

		return 0;
	}

	return -1;
}

/*
 * func: ���ߴ���--��λȫ�ֱ��/����, ��¼����ʱ��
 */

static int DoOffline()
{
	int j, tmp;

	// Step.1
	memset(fp_state, 0, sizeof(fp_state));
	memset(dostop_flag, 0, sizeof(dostop_flag));
	memset(pump.addInf, 0, sizeof(pump.addInf));

	// Step.2
	/*
	 * �����������ݻָ�Ϊȫ��, ��Ϊ�ͻ������ڼ���ܼӹ���,
	 * �����Ȼ����Ϊ֮ǰ����ļ������ݻ᲻׼ȷ
	 */
	for (j = 0; j < pump.panelCnt; ++j) {
		tmp = 0;
		do {
			display_data[j][tmp] = 0x00;
			display_data[j][tmp + 1] = 0xFF;
			tmp += 2;
		} while (tmp < sizeof(display_data[0]));
	}

	// Step.3
//	gpIfsf_shm->node_online[IFSFpos] = 0;
	ChangeNode_Status(node, NODE_OFFLINE);
	pump.time_offline = (3 * gpIfsf_shm->fcc_com_sv_data.Heartbeat_Interval) + time(NULL);
	RunLog("====== Node %d ����! =============================", node);

	// Step.4
	for (j = 0; j < pump.panelCnt; ++j) {
		lpDispenser_data->fp_data[j].FP_State = FP_State_Inoperative;
		lpDispenser_data->fp_data[j].Log_Noz_State = 0;
	}

	return 0;
}

/*
 * func: �ͻ��ָ����ߵĴ���--����ͻ�״̬�Ƿ�Ready,
 *                           ��ʼ������, ��ʼ��FP״̬, ���ýڵ�����
 * return: -1 - ִ��ʧ��, 0 - ִ�гɹ�.
 */

static int DoOnline()
{
	int j, ret;
				
	// Step.1
	ret = Wait4PumpReady();
	if (ret < 0) {
		RunLog("Node[%d]״̬δ׼����, ����", node);
		return -1;
	}

	// Step.2
	ret = InitPumpTotal();   // ���±��뵽���ݿ�
	if (ret < 0) {
		RunLog("Node[%d]���±��뵽���ݿ�ʧ��", node);
		return -1;
	}

	// Step.3
	for (j = 0; j < pump.panelCnt; ++j) {
		lpDispenser_data->fp_data[j].FP_State = FP_State_Closed;
	}

//	gpIfsf_shm->node_online[IFSFpos] = node;
	ChangeNode_Status(node, NODE_ONLINE);
	RunLog("====== Node %d �ָ�����! =========================", node);
	keep_in_touch_xs(12);   // ȷ����λ���յ����������ϴ�״̬

	// Step.4               // �����ϴ���ǰFP״̬
	for (j = 0; j < pump.panelCnt; ++j) {
		ChangeFP_State(node, j + 0x21, FP_State_Closed, NULL);
	}

	return 0;
}

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
		Ssleep(300000);
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
static int SetPort019()
{
	int ret;

	pump.cmd[0] = 0x1C;
	pump.cmd[1] = 0x52;
	pump.cmd[2] = 0x30 | (pump.chnPhy - 1);
	pump.cmd[3] = 0x03;	                          //������ 9600
	pump.cmd[4] = 0x00;
	pump.cmd[5] = 0x00;
	pump.cmd[6] = 0x00;
	pump.cmdlen = 7;
	pump.wantLen = 0;

	ret = WriteToPort(pump.fd, pump.chnPhy, pump.chnLog, pump.cmd,
			pump.cmdlen, 0, pump.wantLen, TIME_OUT_WRITE_PORT);
	if (ret < 0) {			
		return -1;
	}

	Ssleep(INTERVAL);

	return 0;
}


/*
 * func:  ͨѶ��ͨʱ��ѯ�����и�һ��ʱ������һ�´�������
 *        ����������ĵ�ѹ���ȵ��µ�Ƭ����λ--�����������ûָ�Ԥ��9600,8,n,1
 */
static int ReDoSetPort019()
{
#define RE_DO_FREQUENCY   10         //  10����ѯʧ������һ�´�������

	int static polling_fail_cnt = 0;
	int ret;

	if ((++polling_fail_cnt) >= RE_DO_FREQUENCY) {
		ret = SetPort019();
		if (ret < 0) {
			RunLog("�ڵ� %d ���贮������ʧ��", node);
		}
		polling_fail_cnt = 0;
	}

	return ret;
}

/*
 * func: DoubleTalk У��
 * return: 0 - �ɹ�; -1 - ʧ��
 */
static int DoubleTalkCheck(const __u8 *buf, int len)
{
	int i;

	if ((len % 2) != 0) {
		return -2;
	}

	for (i = len - 1; i >= 0; i -= 2) {
		if (buf[i - 1] != (__u8)~buf[i]) {
			return -1;
		}
	}

	return 0;
}

/*
 * ���FP�ϸ�ǹ����Ʒ�����Ƿ������ȷ(����ʵ��), ���ȫ����ȷ��ı�״̬ΪClosed
 */
static int CheckPriceErr(int pos)
{
	int ret;
	int lnz, pr_no;
	int fm_idx;
	__u8 price_tmp[64] = {0};

	if ((__u8)FP_State_Inoperative != lpDispenser_data->fp_data[pos].FP_State) {
		return -1;
	}

	fm_idx = lpDispenser_data->fp_data[pos].Default_Fuelling_Mode - 1;
	if ((fm_idx < 0) || (fm_idx > 7)) {
		RunLog("CheckPrice ERR Default FM error, Node[%d]FP[%d] FM[%d] <ERR>",
							node, pos + 1, fm_idx + 1);
		return -1;
	}

	for (lnz = 0; lnz < pump.gunCnt[pos]; ++lnz) {
		/*
		 * ˵��: ���ͻ�һ���ܹ�4ǹ4��Ʒ, ����ͻ���������ֻʹ�� n ����Ʒ(n<4),
		 *       ��ô 1-n ��ǹ����һ������Ϊ��, ������Щǹʵ�����Ƿ��нӳ���ʹ��;
		 *       ��һ����, (n+1) - 4 ��ǹ�ĵ��������Ϊ��, ������ʧ��.
		 */
		if (lnz < nb_prods[pos]) {
			// a. ��������Ʒ
			pr_no = lpDispenser_data->fp_ln_data[pos][lnz].PR_Id - 1;
			if ((pr_no < 0) || (pr_no > 7)) {
				RunLog("Node[%d] CheckPrice PR_Id, PR_Id[%d] <ERR>", node, pr_no + 1);
				pr_no = 0;
			}

			price_tmp[(lnz << 2) + 0] = lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3];
			price_tmp[(lnz << 2) + 1] = ~pump.cmd[(lnz << 2) + 0];
			price_tmp[(lnz << 2) + 2] = lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2];
			price_tmp[(lnz << 2) + 3] = ~pump.cmd[(lnz << 2) + 2];
		} else {
			// b.δ������Ʒ����ǹ(��������Ϊ��)
			price_tmp[(lnz << 2) + 0] = 0x00;
			price_tmp[(lnz << 2) + 1] = 0xff;
			price_tmp[(lnz << 2) + 2] = 0x00;
			price_tmp[(lnz << 2) + 3] = 0xff;
		}
	}
	
	ret = GetPrice(pos);
	if (ret < 0){
		RunLog("Node[%d].FP[%d]������ʧ��", node, pos + 1);
		return -1;
	}

	for (lnz = 0; lnz < pump.gunCnt[pos]; ++lnz) {
		pr_no = lpDispenser_data->fp_ln_data[pos][lnz].PR_Id - 1;

		RunLog("Node %d FP %d LNZ %d ��ǰ����: %02x.%02x; Ŀ�굥��: %02x.%02x",
			node, pos + 1,lnz + 1, pump.cmd[(lnz << 2) + 2], pump.cmd[(lnz << 2) + 0],	
			lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2], 
			lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3]);
	}
		
	if (memcmp(pump.cmd, price_tmp, (pump.gunCnt[pos] << 2)) != 0) {
		return -2;
	}

	ChangeFP_State(node, 0x21 + pos, FP_State_Closed, NULL);
	RunLog("Node[%d]FP[%d]�����ѱ����ȷ, �л�״̬ΪClosed", node, pos + 1);

	return 0;
}


/*
 * Filename: lischn004.c
 *
 * Description: ���ͻ�Э��ת��ģ�顪����, RS422,9600,n,8,1
 *
 * 2008.07 by Lei Li <lil@css-intelligent.com>
 *
 * History:
 * 
 * ����˵��:
 * ����Э��ֻ��������״̬��Ԥ�ü��͡��޽�����ۡ�
 * ��ǰ��������ǰ������10Ϊ��ȷ��ֵ��
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
#define INTERVAL_10     150000  //д����ֹͣ150ms

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
static int fp_state[MAX_FP_CNT];       // 0 - IDLE, 1 - CALLING , 2 - DoStarted�ɹ�, 3 - Fuelling
static int dostop_flag[MAX_FP_CNT];    // ����Ƿ�ɹ�ִ�й�DoStop, 1 - ��
static int doauth_flag[MAX_FP_CNT];    // ����Ƿ�ɹ�����DoRation, 1 - ��
static int fail_cnt = 0;               // ��¼���ͻ�ͨѶʧ�ܴ���, �����ж�����

int ListenChn004(int chnLog)
{
	long	indtyp;
	int	ret;
	int	fd;
	int	j;
	int	i;
	__u8 status;
	
	sleep(5);   //��ֹ�˽����˳�,��������û���������. ÿ��lischnxxx����Ҫ
	RunLog("Listen Channel  001 ,logic channel[%d]", chnLog);

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
				pump.addInf[j][0] = 0;	      // ��ʶ�Ƿ�����Ԥ��
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
		RunLog("�ڵ�[%d]�򿪴���[%s]ʧ��",node, pump.cmd);
		return -1;
	}

	ret = SetPort004();
	if (ret < 0) {
		RunLog("�ڵ�[%d]���ô�������ʧ��",node);
		return -1;
	}

	// ������ͨ����ʼ����ȡ�ͻ������ǵ�ʱ��, ����IO�帺��
	Ssleep(2 * INTERVAL * pump.chnLog);
	
	// ============== Part.3 �жϼ��ͻ��Ƿ����� ====================================
	// Check wether dispenser is online, if it's not online, wait  here.
	while (1) {
		ret = GetPumpOnline(0, &status);
		if (ret == 0) {
			break;
		}

		gpIfsf_shm->node_online[IFSFpos] = 0; //������,����������.
		RunLog("<����> �ڵ�[%d]������", node);
		
		sleep(1);
	}
	
	// ============= Part.4 ��ʼ������ͻ���ǰ״̬��ص����ݿ������ =======================
	for (i = 0; i < pump.panelCnt; ++i) {
		ret = SetSaleMode(i, 1);   //��ʼ�����Ϊ����ģʽ
		if (ret < 0) {
			RunLog("�ڵ�[%d]�������[i] ����ģʽʧ��!",node,i + 1);
			return -1;
		}
	}
	
	ret = InitGunLog();    // Note.InitGunLog ������ÿ��Ʒ���ͻ�������
	if (ret < 0) {
		RunLog("�ڵ�[%d]��ʼ��Ĭ�ϵ�ǰ�߼�ǹ��ʧ��",node);
		return -1;
	}

	ret = InitCurrentPrice(); //  ��ʼ��fp���ݿ�ĵ�ǰ�ͼ�
	if (ret < 0) {
		RunLog("�ڵ�[%d]��ʼ����Ʒ���۵����ݿ�ʧ��",node);
		return -1;
	}

	ret = InitPumpTotal();    // ��ʼ�����뵽���ݿ�
	if (ret < 0) {
		RunLog("�ڵ�[%d]��ʼ�����뵽���ݿ�ʧ��",node);
		return -1;
	}


//	gpIfsf_shm->node_online[IFSFpos] = node;
	ChangeNode_Status(node, NODE_ONLINE);
	RunLog("�ڵ�[ %d] ����,״̬[%02x]!", node, status);
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
			RunLog("ϵͳ��ͣFp_State = %d",lpDispenser_data->fp_data[0].FP_State);
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
			Ssleep(200000);  //200ms

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
				RunLog("�ڵ�[%d]�Ƿ�������ַ",node);
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
				dostop_flag[pos] = 1;
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
				if ((++fail_cnt) < MAX_FAIL_CNT) {  // ���ͻ������ж�
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
				RunLog("======�ڵ�[%d] ����! ===========================", node);

				// Step.3
				for (j = 0; j < pump.panelCnt; ++j) {
					lpDispenser_data->fp_data[j].FP_State = FP_State_Inoperative;
					lpDispenser_data->fp_data[j].Log_Noz_State = 0;
				}
			}
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

				// Step.1  �����ÿ��Ʒ��ڵ�һλ
				for (j = 0; j < pump.panelCnt; ++j) {
					ret = SetSaleMode(j, 1);   //��ʼ�����Ϊ����ģʽ
					if (ret < 0) {
						RunLog("�������[%d] ����ģʽʧ��!", j + 1);
						return;   // ������continue, ��continue���������ý�����
					} else{
						RunLog("�������[%d] ����ģʽ�ɹ�!", j + 1);
					}
				}
	
				// Step.2
				ret = InitPumpTotal();    // ��ʼ�����뵽���ݿ�
				if (ret < 0) {
					RunLog("�ڵ�[%d], ���±��뵽���ݿ�ʧ��",node);
					continue;
				}

				// Step.3
				for (j = 0; j < pump.panelCnt; ++j) {
					lpDispenser_data->fp_data[j].FP_State = FP_State_Closed;
				}

				// Step.4
//				gpIfsf_shm->node_online[IFSFpos] = node;
				ChangeNode_Status(node, NODE_ONLINE);
				RunLog("====== Node %d �ָ�����! =======================", node);
				// Step.5                 // �����ϴ���ǰFP״̬
				for (j = 0; j < pump.panelCnt; ++j) {
					ChangeFP_State(node, j + 0x21, FP_State_Closed, NULL);
				}
				continue;
			}

			if (fail_cnt >= 1) {
				if (status != PUMP_BUSY) { // �������������ʧ��Ӱ�콻�״�������
					for (j = 0; j < pump.panelCnt; ++j) {
						if (SetSaleMode(j, 1)!= 0) {
							return;
						}
					}
					fail_cnt = 0;
					/*
					 * ��SetCtrlMode�ķ��ؽ���Ḳ��GetPumpStatus�ķ�������,
					 * ���ڽ�������ʱ����, �����½��״������
					 * �������ﲻ��SetCtrlMode�ɹ�����Ҫcontinue
					 */
					continue;
				}
			} else {
				fail_cnt = 0;
			}
		}
		
                if (status != pre_status[i]) {
			pre_status[i] = status;
			RunLog("�ڵ�[%d]���[%d]��ǰ״̬��Ϊ[%02x]", node, i + 1, status);
		}
		if (gpIfsf_shm->auth_flag - FCC_SWITCH_AUTH != 0) {  //����к�̨
			if ((__u8)FP_State_Closed == lpDispenser_data->fp_data[i].FP_State) {
				if (i == 0) {
					times += pump.panelCnt;					
				}
			
				if (times > (SENDSTATE *  pump.panelCnt)) {
					if (i >= pump.panelCnt -1) {
						times = 0;
					}
					UserLog("�ڵ�[%d]���[%d],����Closed״̬", node, i + 1);
				}

				// Closed ״̬��ҲҪ�ܹ����
				ChangePrice();
				continue; 
			}
		} else {
			if ((__u8)FP_State_Closed == lpDispenser_data->fp_data[i].FP_State) {
				RunLog("�ڵ�[%d]���[%d], DoPolling, û�к�̨�ı�״̬ΪIDLE", node,i + 1);
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
		case PUMP_OFF:					// ��ǹ������
			dostop_flag[i]= 0;
			if (fp_state[i] >= 2) {                 // Note.��û�вɵ���ǹ�ź�(EOT)�Ĳ��䴦��
				// ���мӹ���, �����״̬�ı�ͽ��״���
				// 1. ChangeFP_State to IDLE first
				tmp_lnz = lpDispenser_data->fp_data[i].Log_Noz_State;
				lpDispenser_data->fp_data[i].Log_Noz_State = 0x00;     
				if ((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State) {
					ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
				}
				RunLog("Node %d FP %d �� %d ��ǹ��ǹ", node, i + 1, pump.gunLog[i]);
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
			// �����涼���вű��
			if (j >= pump.panelCnt) {
				ret = ChangePrice();            // �鿴�Ƿ�����Ʒ��Ҫ���,����ִ��
			}
			break;
		case PUMP_CALL: // ̧ǹ
			dostop_flag[i]= 0;

			if ((__u8)FP_State_Calling > lpDispenser_data->fp_data[i].FP_State) {
				ret = DoCall(i);
				fp_state[i] = 1;

				DoStarted(i);
			}

			break;
		case PUMP_BUSY:					// ����
			if (dostop_flag[i] != 0) { 
				break;
			}
				
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
		case PUMP_STOP:					// ��ͣ���� 
			if ((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State) {
				ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
			}
			break;
		case PUMP_PEOT:
			ShutDown(i);
			Ssleep(200000);      // �ȴ��ͻ���������
			if (fp_state[i] >= 2) {  //����������ͻ��ѻ��������״���
				if (GetPumpStatus(i, &status) < 0) {
					break;
				}
				if (status != PUMP_OFF) {
					/* �����״���ǰ����ȷ���ͻ�״̬Ϊ PUMP_OFF,
					 * �����ͻ����ܻ�û��������ݴ���.
					 */
					break;
				}

				/*
				 * Note: ֮������case PUMP_PEOT������PUMP_OFF״̬�ж�,
				 *       �������״���DoEot, ��Ϊ�����ӽ����ϴ��ٶ�;
				 *       �����һ����ѯ��������DoEot���ӳ������ϴ�ʱ��(1-2s)
				 * 
				 */

				// 1. ChangeFP_State to IDLE first
				tmp_lnz = lpDispenser_data->fp_data[i].Log_Noz_State;
				lpDispenser_data->fp_data[i].Log_Noz_State = 0x00;
				if ((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State) {
					ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
				}
				RunLog("Node %d FP %d �� %d ��ǹ��ǹ", node, i + 1, pump.gunLog[i]);
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
		RunLog("�ڵ�[%d]����״̬�ı����",node);
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
			RunLog("�ڵ�[%d]���[%d],BeingTr4FP Failed", node, i + 1);
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

	} else if (pump.cmd[0] == 0x4D) {//����
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
		pump.cmd[1] = 0x55;		/*aa55��ͷ*/
		pump.cmd[2] = 0x0a;		/*���ݳ��� �ӵ�ַ����������Crc����������֮��ĳ���*/
		pump.cmd[3] = 0x00|(pump.panelPhy[pos]-1);	/*�豸��ַ�����������ű���*/
		pump.cmd[4] = 0x02;				/*���02����Ȩ����*/
		pump.cmd[5] = '1';				/*��Ȩ*/
		if (preset_type == PRE_VOL)			/*�������Ƕ���*/
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
		CRC_16_RX(pump.cmd, pump.cmdlen, &CrcHi, &CrcLo);	/*����Crcֵ*/
		pump.cmd[13] = CrcHi;
		pump.cmd[14] = CrcLo;
		pump.cmdlen = 15;
		pump.wantLen = 7;
		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d]�·�Ԥ�ü�������ʧ��", node, pos + 1);
			continue;
		}

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d]�·�Ԥ�ü�������ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		}

		CRC_16_RX(pump.cmd,pump.cmdlen-2,&CrcHi, &CrcLo);
		if (pump.cmd[pump.cmdlen -2] != CrcHi || pump.cmd[pump.cmdlen -1] != CrcLo) {
			RunLog("�ڵ�[%d]���[%d]��������CRCУ��ʧ�ܡ�",node,pos + 1);
			continue;
		}
		
		if (pump.cmd[4] !='A') {
			RunLog("�ڵ�[%d]���[%d] ȡ�ظ���Ϣʧ��", node,pos + 1);
			continue;
		} else {
			return 0;
		}
	}

	return -1;
}

/*
 * func: PreSet/PrePay & DoReleaseRation
 *       @�ͻ������Ѿ�Ӧ����Ҫ��ȡ��Ԥ�ú�ֹͣ���ͺϲ�, ���Դ˺����Ѳ�����Ҫ
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
			RunLog("�ڵ�[%d]���[%d]�·�Ԥ�ü�������ʧ��", node, pos + 1);
			continue;
		}

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret<0) {
			RunLog("�ڵ�[%d]���[%d]�·�Ԥ�ü�������ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		}

		CRC_16_RX(pump.cmd,pump.cmdlen-2,&CrcHi, &CrcLo);
		if (pump.cmd[pump.cmdlen -2] != CrcHi || pump.cmd[pump.cmdlen -1] != CrcLo) {
			RunLog("�ڵ�[%d]���[%d]��������CRCУ��ʧ�ܡ�",node,pos + 1);
			continue;
		}
		
		if (pump.cmd[4] !='A') {
			RunLog("�ڵ�[%d]���[%d] ȡ�ظ���Ϣʧ��", node,pos + 1);
			continue;
		} else {
			return 0;
		}
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
/*�ػ�����*/
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
			RunLog("�ڵ�[%d]���[%d]��˿�д����ʧ��",node,pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd)-1;

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d]ȡ�ظ���Ϣʧ��.", node,pos + 1);
			continue;
		} 

		CRC_16_RX(pump.cmd,pump.cmdlen-2,&CrcHi, &CrcLo);
		if (pump.cmd[pump.cmdlen -2] != CrcHi || pump.cmd[pump.cmdlen -1] != CrcLo) {
			RunLog("�ڵ�[%d]���[%d]��������CRCУ��ʧ�ܡ�",node,pos + 1);
			continue;
		}
		
		if (pump.cmd[4] !='A') {
			RunLog("�ڵ�[%d]���[%d]���͹ػ�����ʧ��.", node,pos + 1);
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
/*
 * �޸Ľ���&ʵ��˵��:  ��Ҫ���ͻ���ȡ���������� ���μ����� & ���ͽ�� &
 *                     ���� & �������� [& �������] , �ֱ������Ӧ�����ݿ���
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
	RunLog("�ڵ�[%d]���[%d]�����½���", node, pos + 1);

	memcpy(msg, pump.cmd, 20);   // �������ݱ��� -- ����&���

#if 0
	/*
	 * �ⲿ�ִ������������ PUMP_PEOT״̬��, 
	 * ��״̬��ΪPUMP_OFFȷ�����ͻ��Ѿ���ɽ������ݴ�����ٶ�ȡ����ͽ���
	 *
	 * Guojie Zhu @ 2009.8.25
	 */
	err_cnt = 0;
	do {
		ret = ShutDown(pos);
		if (ret < 0) {
			err_cnt++;
			RunLog("�ڵ�[%d]���[%d]�ػ�����ʧ��<ERR>" ,node, pos+1);
		}
	} while (ret < 0 && err_cnt <=5);

	if (ret < 0) {
		return -1;
	}


	//2. ��ñ��룬���ۻ����ͽ��
	bzero(pump.cmd,sizeof(pump.cmd));
	Ssleep(200000); //��ǹ������ȡ���룬������ܻ�û���¡�
#endif
	err_cnt = 0;
	do {
		ret = GetPumpTotal(pos);
		if (ret < 0) {
			err_cnt++;
			RunLog("�ڵ�[%d]���[%d]GetPumpTotal ʧ�� <ERR>" ,node, pos+1);
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

	//3. �õ���Ʒid 
	pr_no = (lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).PR_Id - 1;  // 0 - 7	
	if ((pr_no < 0) || (pr_no > 7)) {
		RunLog("�ڵ�[%d]���[%d]DoEot PR_Id error, PR_Id[%d]<ERR>",node, pos + 1, pr_no + 1);
	}


	//4. �������Ƿ��б仯,������򱣴潻�׼�¼
	if (memcmp(pre_total, meter_total, 7) != 0) {
#if 0
		RunLog("�ڵ�[%d]���[%d]ԭ����Ϊ %s",node,pos,Asc2Hex(pre_total,7));
		RunLog("�ڵ�[%d]���[%d]��ǰ����Ϊ %s",node,pos,Asc2Hex(meter_total,7));	  
#endif

		//Current_Unit_Price bin8 + bcd8
		ret = Get_price(pos);
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d]��ȡ����ʧ��",node,pos + 1);
			// ��ȡʧ�ܾ������ݿ����, �����ܱ�֤�������ϴ�, ����������, ���ܷ����˳�
			memcpy(price, lpDispenser_data->fp_fm_data[pr_no][lpDispenser_data->fp_data[pos].Default_Fuelling_Mode - 1].Prod_Price, 4);
		} else {
			price[0] = 0x04;
			price[1] = 0x00;
			price[2] = (pump.cmd[6] << 4) | (pump.cmd[7] & 0x0f);
			price[3] = (pump.cmd[8] << 4) | (pump.cmd[9] & 0x0f);
		}
				
		memcpy(lpDispenser_data->fp_data[pos].Current_Unit_Price, price, 4);
		//����С��������λ���ܳ���9�����ͻ�����ʾ��Ϊ��λ�������
		//�ϴ�������û�н�λ��Ҫ�������롣

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
		RunLog("�ڵ�[%02d]���[%d]ǹ[%d] ## Amount: %02x%02x.%02x, Volume: %02x%02x.%02x, Price: %02x.%02x, Total: %02x%02x%02x%02x.%02x", 
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
				RunLog("�ڵ�[%d]���[%d] DoEot.EndTr4Ln failed, ret: %d <ERR>", node,pos+1,ret);
			}
		} while (ret < 0 && err_cnt <= 5);

		err_cnt = 0;
		do {
			ret = EndTr4Fp(node, pos + 0x21, pre_total, 
				(lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Vol_Total);
			if (ret < 0) {
				err_cnt++;
				RunLog("�ڵ�[%d]���[%d]DoEot.EndTr4Fp failed, ret: %d <ERR>", node,pos+1,ret);
			}
		} while (ret < 0 && err_cnt <= 8);
	} else {
		ret = EndZeroTr4Fp(node, pos + 0x21);
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d] DoEot.EndZeroTr4Fp failed, ret: %d <ERR>",node,pos+1,ret);
		}
	}	
	return 0;
}


/*
 * func: ֹͣ���ͻ���ȡ��Ԥ����Ȩ
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
			RunLog("�ڵ�[%d]���[%d] ����ͣ��������ʧ��",node,pos+1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd)-1;

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d] ����ͣ��������ɹ�, ȡ������Ϣʧ��",node,pos+1);
			continue;
		}

		CRC_16_RX(pump.cmd,pump.cmdlen-2,&CrcHi, &CrcLo);
		if (pump.cmd[pump.cmdlen -2] != CrcHi || pump.cmd[pump.cmdlen -1] != CrcLo) {
			RunLog("�ڵ�[%d]���[%d]��������CRCУ��ʧ�ܡ�",node,pos + 1);
			continue;
		}

		if (pump.cmd[4] !='A') {
			RunLog("�ڵ�[%d]���[%d]���͹ػ�����ʧ��", node,pos + 1);
			continue;
		}

		return 0;
	}
	return -1;
}


/*
 * func: ��Ȩ
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
		pump.cmd[1] = 0x55;	/*aa55��ͷ*/
		pump.cmd[2] = 0x0a;	/*���ݳ��� �ӵ�ַ����������Crc����������֮��ĳ���*/
		pump.cmd[3] = 0x00|(pump.panelPhy[pos]-1);	/*�豸��ַ�����������ű���*/
		pump.cmd[4] = 0x02;	/*���02����Ȩ����*/
		pump.cmd[5] = '1';	/*��Ȩ*/
		pump.cmd[6] = '0';	/*�Ƕ�ֵ����*/
		pump.cmd[7] = '0';
		pump.cmd[8] = '0';
		pump.cmd[9] = '0';
		pump.cmd[10] = '0';
		pump.cmd[11] = '0';
		pump.cmd[12] = '0';	/*�Ƕ�ֵ����.����λ*/
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
			RunLog("�ڵ�[%d]���[%d]����Ȩ����ʧ��",node,pos+1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd)-1;

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d]����Ȩ����ɹ�, ȡ������Ϣʧ��",node,pos+1);
			continue;
		}

		CRC_16_RX(pump.cmd,pump.cmdlen-2,&CrcHi, &CrcLo);
		if (pump.cmd[pump.cmdlen -2] != CrcHi || pump.cmd[pump.cmdlen -1] != CrcLo) {
			RunLog("�ڵ�[%d]���[%d]��������CRCУ��ʧ�ܡ�",node,pos + 1);
			continue;
		}

		if (pump.cmd[4] != 'A' && pump.cmd[4] != 'W') {
			RunLog("Node[%d].FP[%d] ��Ȩʧ��",node, pos + 1);
			continue;
		}

#if 0
		// ͨ��A/W�ж��Ƿ�ɹ����㹻, �ʲ����ж�״̬
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
 * func: �ϴ�ʵʱ��������
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
 * func: ̧ǹ���� 
 * ��ȡ��ǰ̧����߼�ǹ��,����fp_data[].Log_Noz_State & �ı�FP״̬ΪCalling
 */
static int DoCall(int pos)
{
	int ret;
	int t;
	__u8 ln_status;

	ret = GetCurrentLNZ(pos);
	if (ret < 0) {
		RunLog("�ڵ�[%d]���[%d]��ȡ��ǰ��ǹǹ��ʧ��",node,pos + 1);
		return -1;
	}

	ret = ChangeFP_State(node, pos + 0x21, FP_State_Calling, NULL);

	//��λ������,���������Զ���Ȩ;��λ��������, ��Ȩ�������ڿ���״̬,��ʱ�Զ���Ȩ
	if ((gpIfsf_shm->auth_flag == FCC_SWITCH_AUTH) && (lock_status() == 1)) {
		ret = DoAuth(pos);
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d],�Զ���Ȩʧ��",node,pos+1);
		}
		ret = ChangeFP_State(node, pos + 0x21, FP_State_Started, NULL);
		RunLog("�ڵ�[%d]���[%d]�Զ���Ȩ�ɹ�",node,pos+1);
		SO_OFFLINE_TR(node, pos);
	}

	return ret;
}

/*
 * func: ִ�б��, ʧ�ܸı�FP״̬ΪINOPERATIVE, ��д�������ݿ�
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
 * �����µ�������newPrice[FP_PR_NB_CNT], �������Ʒ��Ҫ���--����ֵ��Ϊ��,
 * ���������FP, �����Ʒƥ����·��������
 */

//	RunLog(">> exec ChangePrice Now !");
	for (pr_no = 0; pr_no < FP_PR_NB_CNT; pr_no++) {
		if (pump.newPrice[pr_no][2] != 0x00 || pump.newPrice[pr_no][3] != 0x00) {
			for (i = 0; i < pump.panelCnt; i++) {
				err_code = 0x99;
				for (j = 0; j < pump.gunCnt[i]; j++) { // ������gunCnt,1ǹ���ܲ���
#if DEBUG
					RunLog(">>> fp_ln_data[%d][%d].PR_Id: %d", i, j,
							lpDispenser_data->fp_ln_data[i][j].PR_Id);
#endif
					if (lpDispenser_data->fp_ln_data[i][j].PR_Id - 1 == pr_no) {
					err_code_tmp = 0x99;
				CriticalLog("�·����������ڵ�[%02d]��[%d]ǹ[%d]......", node, i + 1, j + 1);

					//price is little endian mode, so 1 to 0
					price[0] = 0x30 |(pump.newPrice[pr_no][2] >>4);
					price[1] = 0x30 |(pump.newPrice[pr_no][2] & 0x0f);
					price[2] = 0x30 |(pump.newPrice[pr_no][3] >>4);
					price[3] = 0x30 |(pump.newPrice[pr_no][3] & 0x0f);
			
					for (t = 0; t < TRY_TIME; t++) {
						// Step.1.1 ���������
						Ssleep(INTERVAL_10);	/*ͣ10����,��ֹд��̫Ƶ��*/
						ret=GetPumpStatus(i, &status);
						if (ret < 0) {
							RunLog("�ڵ�[%d]���[%d]ȡ״̬��Ϣʧ��",node,pos + 1);
						}
						

						/*��֯�����ͼ�����
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
					
						// Setp.1.2 ����������
						Ssleep(INTERVAL);
						ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
								pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
						if (ret < 0) {
							RunLog("�ڵ�[%d]���[%d],�·����ʧ��!" ,node,i+1);
							continue;
						}

						RunLog("Node[%d].FP[%d].PriceChange_CMD: %s",
							node, i + 1, Asc2Hex(pump.cmd, pump.cmdlen));

						memset(pump.cmd, 0, sizeof(pump.cmd));
						ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
						if (ret < 0) {
							RunLog("�ڵ�[%d]���[%d],�·��������ɹ�,ȡ������Ϣʧ��",node,i+1);
							continue;
						}

						/*У��Crcֵ*/
						CRC_16_RX(pump.cmd,pump.cmdlen-2,&CrcHi, &CrcLo);
						if (pump.cmd[pump.cmdlen -2] != CrcHi || pump.cmd[pump.cmdlen -1] != CrcLo) {
							RunLog("�ڵ�[%d]���[%d]��������CRCУ��ʧ�ܡ�",node,i + 1);
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
								err_code_tmp = 0x37; // ���ִ��ʧ��
								Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL_10 - INTERVAL);
								continue;
							}
						}
		
						// Step.3 д�µ��۵���Ʒ���ݿ� & ���㵥�ۻ���
						fm_idx = lpDispenser_data->fp_data[i].Default_Fuelling_Mode - 1;
						RunLog("�ڵ�[%d]���[%d]�·��������ɹ�", node, i + 1);
						RunLog("�µ���fp_fm_data[%d][%d].Prode_Price: %s", pr_no, fm_idx,
							Asc2Hex(lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price, 4));

						CriticalLog("�ڵ�[%02d]��[%d]ǹ[%d]��۳ɹ�,�µ���Ϊ%02x.%02x",
							       	node, i + 1, j + 1,
								lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2],
								lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3]);
				
						err_code_tmp = 0x99;
						break;
					}

					// Step.4 �����ж�, ��������д�������ݿ�/��FP״̬Ϊ Inoperative
					if (t >= TRY_TIME) {
						RunLog("�ڵ�[%d]���[%d],�������ִ��ʧ��,�ı�FPΪ���ɲ���״̬",node,i+1);
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
				} // end of for.j < LN_CNT
				RunLog("---------- Node[%d]FP[%d] ER_DATA, error code: %02x -----", node, i + 1, err_code);

				int tmp_idx;
				for (tmp_idx = 0; tmp_idx < pump.newPrice[pr_no][0]; tmp_idx++) {
					ChangeER_DATA(node, 0x21 + i, err_code, NULL); // Translater�Ѿ��ظ���ACK
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
			RunLog("�ڵ�[%d]���[%d]�򴮿�д����ʧ��",node,pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd)-1;

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d]ȡ�ظ���Ϣʧ��",node,pos + 1);
			continue;
		} 

		CRC_16_RX(pump.cmd,pump.cmdlen-2,&CrcHi, &CrcLo);
		if (pump.cmd[pump.cmdlen -2] != CrcHi || pump.cmd[pump.cmdlen -1] != CrcLo) {
			RunLog("�ڵ�[%d]���[%d]��������CRCУ��ʧ�ܡ�",node,pos + 1);
			continue;
		}

		return 0;
	}
	return -1;
}



//Inoperative ʱ��ȡ����ǹ����
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
			RunLog("�ڵ�[%d]���[%d]�򴮿�д����ʧ��",node,pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd)-1;

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d]ȡ�ظ���Ϣʧ��",node,pos + 1);
			continue;
		} 

		CRC_16_RX(pump.cmd,pump.cmdlen-2,&CrcHi, &CrcLo);
		if (pump.cmd[pump.cmdlen -2] != CrcHi || pump.cmd[pump.cmdlen -1] != CrcLo) {
			RunLog("�ڵ�[%d]���[%d]��������CRCУ��ʧ�ܡ�",node,pos + 1);
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
		CRC_16_RX(pump.cmd, pump.cmdlen, &CrcHi, &CrcLo);	/*����Crcֵ*/
		pump.cmd[5] = CrcHi;
		pump.cmd[6] = CrcLo;
		pump.cmdlen = 7;
		pump.wantLen = 3;
		gpGunShm->addInf[pump.chnLog-1][0] = 0x04;	/*�䳤����.*/

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d]�򴮿�д����ʧ��",node,pos + 1);
			gpGunShm->addInf[pump.chnLog-1][0] = 0x00;	/*�ָ��䳤����.*/
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd)-1;

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		gpGunShm->addInf[pump.chnLog-1][0] = 0x00;	/*�ָ��䳤����.*/
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d]ȡ�ظ���Ϣʧ��",node,pos + 1);
			continue;
		}

		if (pump.cmd[4] != 'A') {
			RunLog("�ڵ�[%d]���[%d]ȡ����ʧ��",node,pos + 1);
			continue;
		}

		CRC_16_RX(pump.cmd,pump.cmdlen-2,&CrcHi, &CrcLo);
		if (pump.cmd[pump.cmdlen -2] != CrcHi || pump.cmd[pump.cmdlen -1] != CrcLo) {
			RunLog("�ڵ�[%d]���[%d]��������CRCУ��ʧ�ܡ�",node,pos + 1);
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
		pump.cmd[1] = 0x55;              // aa55��ͷ
		pump.cmd[2] = 0x02;              // ���ݳ��� �ӵ�ַ����������Crc����������֮��ĳ���
		pump.cmd[3] = 0x00 | (pump.panelPhy[pos] - 1);   //�豸��ַ�����������ű���
		pump.cmd[4] = 0x00;              // ���00�Ƕ�״̬����
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
			RunLog("�ڵ�[%d]���[%d]�·�ȡ״̬����ʧ��",node,pos+1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		//sleep(2);
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		gpGunShm ->addInf[chnLog - 1][0] = 0x00;
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d]�·�ȡ״̬����ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		}
		// ��ֹ�����ź�, ȷ�������Ǽ��ͻ�������
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
 *  ��������ͻ��������Ƿ�����, ���Ըýڵ�������ǹ����
 *  �ɹ����� 0; ʧ�ܷ��� -1
 */



static int GetPumpStatus(int pos, __u8 *status)
{
	int ret;
	int t;
	__u8 CrcHi;
	__u8 CrcLo;

	for (t = 0; t < 2; t++) {
		pump.cmd[0] = 0xAA;
		pump.cmd[1] = 0x55;	/*aa55��ͷ*/
		pump.cmd[2] = 0x02;	/*���ݳ��� �ӵ�ַ����������Crc����������֮��ĳ���*/
		pump.cmd[3] = 0x00|(pump.panelPhy[pos]-1);/*�豸��ַ�����������ű���*/
		pump.cmd[4] = 0x00;	/*���00�Ƕ�״̬����*/
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
			RunLog("�ڵ�[%d]���[%d]�·�ȡ״̬����ʧ��",node,pos+1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd)-1;

		gpGunShm->addInf[pump.chnLog-1][0] = 0x06;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		gpGunShm->addInf[pump.chnLog-1][0] = 0x00;
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d]�·�ȡ״̬����ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		}

		if (pump.cmd[4] != 'A') {
			RunLog("�ڵ�[%d]���[%d]ȡ״̬ʧ��",node,pos + 1);
			continue;
		}

		CRC_16_RX(pump.cmd,pump.cmdlen-2,&CrcHi, &CrcLo);
		if (pump.cmd[pump.cmdlen -2] != CrcHi || pump.cmd[pump.cmdlen -1] != CrcLo) {
			RunLog("�ڵ�[%d]���[%d]��������CRCУ��ʧ�ܡ�",node,pos + 1);
			continue;
		}

		*status = pump.cmd[6];
		return 0;
	}
	return -1;
}


/*
 * func: ��ȡ��ǰ̧����ǹ��ǹ��, ���д��pump.gunLog[] & fp_data[].Log_Noz_State
 * ret : 0 - �ɹ�, -1 - ʧ�� 
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
			RunLog("�ڵ�[%d]���[%d]GetPumpStatus Failed",node,pos +1);
			continue;
		} else if (pump.cmd[4]=='A') {
			pump.gunLog[pos] = pump.cmd[7] & 0x0f;	// ��ǰ̧�����ǹ
			ln_status = 1 << (pump.gunLog[pos] - 1);
			RunLog("Node %d FP %d �� %d ��ņ̀��", node, pos + 1, pump.gunLog[pos]);
			lpDispenser_data->fp_data[pos].Log_Noz_State = ln_status;

			return 0;
		}
	}

	return -1;
}

/*
 * func: ��ʼ��pump.gunLog, Ԥ�ü���ָ����Ҫ; ���޸� Log_Noz_State
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
 * func: ��ʼ����ǹ����
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
			RunLog("ȡ Node %d FP %d LNZ %d �ı���ʧ��", node, i, j);
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
			RunLog("�ڵ�[%d]���[%d]Init.fp_ln_data[%d][%d].Vol_Total: %s ",node,i+1,i,j,
				Asc2Hex(lpDispenser_data->fp_ln_data[i][pump.gunLog[i] - 1].Log_Noz_Vol_Total, 7));
			RunLog("�ڵ�[%d]���[%d]Init.fp_m_data[%d].Meter_Total[%s] ",node,i+1, pr_no, Asc2Hex(meter_total, 7));
			RunLog("��[%d]�߼�ǹ��[%d]�ı���Ϊ[%s]", i + 1, 
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

	RunLog("�ڵ�[%d]���[%d]SetSaleMode...",node,pos + 1);	
	
	for (t = 0; t < TRY_TIME; t++) {
		pump.cmd[0] = 0xAA;
		pump.cmd[1] = 0x55;	/*aa55��ͷ*/
		pump.cmd[2] = 0x03;	/*���ݳ��� �ӵ�ַ����������Crc����������֮��ĳ���*/
		pump.cmd[3] = 0x00|(pump.panelPhy[pos]-1);	/*�豸��ַ�����������ű���*/	
		pump.cmd[4] = 0x01;	/*���01���趨ͨ��ģʽ*/
		if (mode == 1) { /*����*/
			pump.cmd[5] = '1';   /*����: "1"��ʾ��λ������*/
		} else if (mode == 2) {/*���*/
			pump.cmd[5] = '0';						
		}
		pump.cmdlen = 6;
		CRC_16_RX(pump.cmd, pump.cmdlen, &CrcHi, &CrcLo);	/*����Crcֵ*/
		pump.cmd[6] = CrcHi;
		pump.cmd[7] = CrcLo;
		pump.cmdlen = 8;
		pump.wantLen = 7;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d]�·���������ģʽ����ʧ��", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd)-1;

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d]�·���������ģʽ����ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		} 

		CRC_16_RX(pump.cmd, pump.cmdlen-2, &CrcHi, &CrcLo);
//		RunLog("ȡ������[%s].cmdlen=[%d],CrcHi=[%02x],CrcLo=[%02x]",
//				Asc2Hex(pump.cmd,pump.cmdlen),pump.cmdlen,CrcHi,CrcLo); 
		if ((CrcHi != pump.cmd[pump.cmdlen-2]) || (CrcLo != pump.cmd[pump.cmdlen-1])) {
			RunLog("�ڵ�[%d]���[%d]CrcУ��ʧ��!",node,pos + 1);
			continue;
		}

		if (pump.cmd[4] != 'A') {
			RunLog("�ڵ�[%d]����ģʽ����ʧ��!",node);
			continue;
		} else {
			RunLog("�ڵ�[%d]����ģʽ���óɹ�!",node);
			return 0;				
		}
	}

	return -1;
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
 * func: ���ô�������
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
	RunLog("<����>�ڵ�[%d], ���ô������Գɹ�", node);

	Ssleep(INTERVAL);

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
	RunLog("Node[%d]FP[%d]�����ѱ����ȷ, �л�״̬ΪClosed", node, pos + 1);

	return 0;
}


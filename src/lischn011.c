/*
 * Filename: lischn011.c
 *
 * Description: ���ͻ�Э��ת��ģ�顪����( UninfoЭ��,  2Line CL,9600,e,8,1
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
#define PUMP_ERROR	0x00 //����
#define PUMP_OFF        0x01 //����
#define PUMP_CALL	0x02 //�ȴ���Ȩ, ��̧ǹ
#define PUMP_AUTH	0x03 //��Ȩδ����
#define PUMP_BUSY	0x04 //������
#define PUMP_STOP	0x05 //��ͣ����
#define PUMP_EOT	0x06 //���׽���


#define TRY_TIME	5
#define INTERVAL	80000    // 80ms����ͼ��, ��ֵ�Ӿ����ͻ�����

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
static int fp_state[MAX_FP_CNT];     // 0 - IDLE, 1 - CALLING , 2 - DoStarted �ɹ� 3 - FUELLING
static int dostop_flag[MAX_FP_CNT];  // 1 - �ɹ�ִ�й� Terminate_FP
static int first_time = 0;           // ����Ƿ��һ��DoBusy , 1 - first_time

int ListenChn011(int chnLog)
{
	long	indtyp;
	int	ret;
	int	fd;
	int	j;
	int	i;
	__u8    status;
	
	sleep(3);   //��ֹ�˽����˳�,��������û���������. ÿ��lischnxxx����Ҫ
	RunLog("Listen Channel  011 ,logic channel[%d]", chnLog);
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

	// ============= Part.2 �� & ���ô������� -- ������/У��λ�� ===================

	strcpy(pump.cmd, alltty[pump.port - 1]);
	pump.fd = OpenSerial(pump.cmd);
	if (pump.fd < 0) {
		RunLog("�ڵ�[%d]�򿪴���[%s]ʧ��", node, pump.cmd);
		return -1;
	}

	ret = SetPort011();
	if (ret < 0) {
		RunLog("�ڵ�[%d]���ô�������ʧ��", node);
		return -1;
	}
	
	// ============== Part.3 �жϼ��ͻ��Ƿ����� ====================================
	// Check wether dispenser is online, if it's not online, wait  here.
	while (1) {		
		ret = GetPumpOnline(0, &status);
		if (ret == 0) {		
			break;
		}
		
		gpIfsf_shm->node_online[IFSFpos] = 0; //������,����������.
		UserLog("�ڵ� %d ���ܲ�����,����!ppid:[%d]", node,getppid());
		
		sleep(1);
	}
	
#if 0
	// "��ͨ�ζ�����"
	for (i = 0; i < pump.panelCnt; ++i) {
		ret = GetPumpOnline(i, &status);
	}
#endif

	// ============= Part.4 ��ʼ������ͻ���ǰ״̬��ص����ݿ������ =======================
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

     //+++++++++++++++  һ�о���, ����+++++++++++++++++++++++//
       
//	gpIfsf_shm->node_online[IFSFpos] = node;  
	ChangeNode_Status(node, NODE_ONLINE);
	UserLog("�ڵ� %d ����,״̬[%02x]!", node, status);
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
	unsigned int cnt;
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
						if (SetPort011() >= 0) {
							RunLog("�ڵ�%d���贮�����Գɹ�", node);
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
				RunLog("====== Node %d ����! =============================", node);

				// Step.3
				for (j = 0; j < pump.panelCnt; ++j) {
					lpDispenser_data->fp_data[j].FP_State = FP_State_Inoperative;
					lpDispenser_data->fp_data[j].Log_Noz_State = 0;
				}
			}

			// ���贮������
			//ReDoSetPort011();
			continue;
		} else {
			if (gpIfsf_shm->node_online[IFSFpos] == 0) {
				if ((--fail_cnt) > 0) { 
					fail_cnt = 1;   // ����OK���ж��ͻ�����
					continue;
				} else if (time(NULL) < pump.time_offline) {
					RunLog("================ waiting for 30s ================");
					continue;
				}

				// fail_cnt��λ
				fail_cnt = 0;

				// Step.1
				ret = InitPumpTotal();   // ���±��뵽���ݿ�
				if (ret < 0) {
					RunLog("�ڵ�[%d]���±��뵽���ݿ�ʧ��", node);
					fail_cnt = 1;
					continue;
				}

				// Step.2
				for (j = 0; j < pump.panelCnt; ++j) {
					lpDispenser_data->fp_data[j].FP_State = FP_State_Closed;
				}

//				gpIfsf_shm->node_online[IFSFpos] = node;
				ChangeNode_Status(node, NODE_ONLINE);
				RunLog("====== Node %d �ָ�����! =========================", node);
				keep_in_touch_xs(12);   // ȷ����λ���յ����������ϴ�״̬
				// Step.3                 // �����ϴ���ǰFP״̬
				for (j = 0; j < pump.panelCnt; ++j) {
					ChangeFP_State(node, j + 0x21, FP_State_Closed, NULL);
				}
				continue;
			}
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

		switch (status) {
		case PUMP_ERROR:				// Do nothing
			ret = ChangeFP_State(node, i + 0x21, FP_State_Inoperative, NULL);
			break;
		case PUMP_OFF:					// ��ǹ������
			first_time = 0;
			dostop_flag[i] = 0;
			if (fp_state[i] >= 2) {                    // Note.��û�вɵ���ǹ�ź�(EOT)�Ĳ��䴦��
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

			lpDispenser_data->fp_data[i].Log_Noz_State = 0x00;
			fp_state[i] = 0;

			if (((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State) &&
				(__u8)FP_State_Authorised != lpDispenser_data->fp_data[i].FP_State) {
				// �³�����Ԥ�óɹ�֮��״̬������ת��Ϊ����ȨΪ����
				// ������Ҫ���Ƿ�Ϊ����Ȩ���ж�
				ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
			}

			EO_OFFLINE_TR(node, i);
			if (gpIfsf_shm->fp_offline_flag[node - 1][i] != 0) {
				upload_offline_tr(node, NULL);
			}

			// ��FP״̬ΪIdle��ʱ����·����
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
		case PUMP_CALL:					// ̧ǹ
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
		case PUMP_EOT:					// ���ͽ���
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

		// e1 e1  = 17  ��FD��ʼ��FCǰһ���ֽڽ���
		pump.cmd[2] = 0xe1;
		pump.cmd[3] = 0xe1;
		
		pump.cmd[4] = 0xfd;

		//002	�D�D	���A����
		pump.cmd[5] = 0xe0;
		pump.cmd[6] = 0xe0;
		pump.cmd[7] = 0xe2;

		//��Ʒ�ţ���ǹ����ǹ�ţ�
		pump.cmd[8] = 0xf5;		
		pump.cmd[9] = 0xe0 | (pump.gunLog[pos] - 1);
	

		//�A������F1����Ԥ�ã�F2���Ԥ�ã�
		if (preset_type == PRE_VOL)	{
			pump.cmd[10] = 0xf1;
		} else if (preset_type == PRE_AMO) {
			pump.cmd[10] = 0xf2;
		}	


		//Ԥ�����ݣ�BCD��
		memcpy(pump.cmd + 11, value, 8);

	/*	RunLog("Ԥ�Ƽ�������: %02x%02x%02x%02x%02x%02x%02x%02x",amount[7],amount[6],
			       amount[5],amount[4],amount[3],amount[2],amount[1],amount[0]); */

		//2λС��
		pump.cmd[19] = 0xe2;

		//����ʹ��:F3 �������㵥�ۣ� F4:�Żݼ۸�
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
			RunLog("дԤ�ü������Node %d FP %d ʧ��", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd)-1;

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);

		if  (!( pump.cmd[0] == (0xd0|pump.panelPhy[pos]) && (pump.cmd[8] == 0xe4 || pump.cmd[8] == 0xe0))){
			RunLog("дԤ�ü������Node %d FP %d �ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		}							
		RunLog("дԤ�ü������Node %d FP %d �ɹ�", node, pos + 1);
		
			
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
			RunLog("�ڵ�[%d]���[%d]�·���ȡ����ʧ��!", node, pos + 1);
			gpGunShm->addInf[pump.chnLog - 1][0] = 0x00;
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		//�䳤����.����ñ�����Ϊ0x00,��tty������ȡ���㹻�ĳ���ʱ,��Ҫ�ø��������ж��Ƿ��к�������
		gpGunShm->addInf[pump.chnLog-1][0] = 0x01;
		//�ж�������ĳ�����spec.c��
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x00;	//ȡ�����ݺ�, ��λ���
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d]�·���ȡ��������ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		}

		if (pump.cmd[pump.cmdlen - 1] != 0xf0) {
			RunLog("�������ݲ�����");
			continue;
		}

		//lrcУ��ʧ��
		lrc = LRC(pump.cmd, pump.cmdlen - 2);
		if ((lrc & 0x0f) != (pump.cmd[pump.cmdlen - 2] & 0x0f)) {
			RunLog("�ڵ�[%d]���[%d]��������У��ʧ��");
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
 * �����µ�������newPrice[FP_PR_NB_CNT], �������Ʒ��Ҫ���--����ֵ��Ϊ��,
 * ���������FP, �����Ʒƥ����·��������
 */
       
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

					//price is little endian mode, so 1 to 0
					price[0] = 0xe0 | (pump.newPrice[pr_no][3] & 0x0f);
					price[1] = 0xe0 | (pump.newPrice[pr_no][3] >> 4);
					price[2] = 0xe0 | (pump.newPrice[pr_no][2] & 0x0f);
					price[3] = 0xe0 | (pump.newPrice[pr_no][2] >> 4);  
					price[4] = 0xe0;
					price[5] = 0xe0;

					for (trycnt = 0; trycnt < TRY_TIME; trycnt++) {
						// Step.1.1 ���������
						pump.cmd[0] = 0xD0|pump.panelPhy[i];
						pump.cmd[1] = 0xff;

						//e1 e6 = 22  ��FD��ʼ��FCǰһ���ֽڽ����ĳ���
						pump.cmd[2] = 0xe1;
						pump.cmd[3] = 0xe6;

						//fd ���ݽ�������				  
						pump.cmd[4] = 0xfd;

						//e0 e0 e1 = 001 ���ĵ���
						pump.cmd[5] = 0xe0;
						pump.cmd[6] = 0xe0;
						pump.cmd[7] = 0xe1;

						//f5  ��Ʒ����				   
						pump.cmd[8] = 0xf5;						
						pump.cmd[9] = 0xe0;  // ******************** �Ƿ�ӦΪ 0xe0 | j ?

						//f3  �������㵥��
						pump.cmd[10] = 0xf3;
						memcpy(pump.cmd + 11,price,6);
						pump.cmd[17] = 0xe2;

						//f4 �Żݵ���
						pump.cmd[18] = 0xf4;
						memcpy(pump.cmd + 19,price,6);
						pump.cmd[25] = 0xe2;

						//fc У��
						pump.cmd[26] = 0xfc;

						//ex  0xFF - 0xFC ֮���������У��
						pump.cmd[27] = 0xe0|LRC_CK(pump.cmd+1,26);

						//f0   ���ݽ���
						pump.cmd[28] = 0xf0;
							
						pump.cmdlen = 29;
						pump.wantLen = 12;

						Ssleep(INTERVAL);
						ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd, 
								pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
						if (ret < 0) {
							RunLog("�ڵ�[%d]���[%d]�·��������ʧ��", node, i + 1);
							continue;
						}

						RunLog("Node[%d].FP[%d].ChangePrice_CMD: %s",
							node, i + 1, Asc2Hex(pump.cmd, pump.cmdlen));

						memset(pump.cmd, 0, sizeof(pump.cmd));
						pump.cmdlen = sizeof(pump.cmd) - 1;

						ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
						 if (ret < 0) {
							RunLog("��������Node[%d].FP[%d]�ɹ�, ȡ������Ϣʧ��", node, i + 1);
							continue;
						}
										 
						if (pump.cmd[0] != (0xD0 | pump.panelPhy[i])) {
							RunLog("Node[%d].FP[%d]���������ɹ�, ȡ������Ϣʧ��", node, i + 1);
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
							continue;
						}

						switch (pump.cmd[8] & 0x0F) {
						case 0x01:
							RunLog("Node[%d].FP[%d]���У������ݳ��ȴ�", node, i + 1);
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
							continue;
						case 0x02:
							RunLog("Node[%d].FP[%d]������������", node, i + 1);
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
							continue;
						case 0x03:
							RunLog("Node[%d].FP[%d]�˿̲�����ִ�б��", node, i + 1);
							err_code_tmp = 0x37;
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
							continue;
						case 0x04:
							RunLog("Node[%d].FP[%d]���ִ����", node, i + 1);
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
							RunLog("�ڵ�[%d]���[%d]���ʧ�� %d ��",node,i+1, trycnt+1);
							err_code_tmp = 0x37;
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
							continue;
						}

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
                     //��ȫ����嶼ִ�б�ۺ�������
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
	__u8 price[5];
	__u8 volume[5];
	__u8 amount[5];
	__u8 pre_total[7];
	__u8 total[7];
	__u8 vol_total[GUN_EXTEND_LEN];
	__u8 amount_total[GUN_EXTEND_LEN];

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
//		RunLog("----- ԭ����= [%s] ",Asc2Hex(pre_total,7));
//		RunLog("----- �±���= [%s] ",Asc2Hex(total,7));
		err_cnt= 0;
		do {
			ret = GetTransData(pos);	// ȡ���һ�ʼ��ͼ�¼
			if (ret != 0) {
				RunLog("Node[%d].FP[%d]��ȡ���׼�¼ʧ�� <ERR>", node, pos + 1);
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
		RunLog("�ڵ�[%02d]���[%d]ǹ[%d] ## Amount: %02x%02x.%02x Volume: %02x%02x.%02x Price: %02x.%02x Total: %02x%02x%02x%02x.%02x", node, pos + 1, pump.gunLog[pos], amount[2], 
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
			RunLog("�ڵ�[%d]���[%d]�·���ȡ��������ʧ��", node, pos + 1);
			gpGunShm->addInf[pump.chnLog - 1][0] = 0x00;
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		//�䳤����.����ñ�����Ϊ0x00,��tty������ȡ���㹻�ĳ���ʱ,��Ҫ�ø��������ж��Ƿ��к�������
		gpGunShm->addInf[pump.chnLog-1][0] = 0x01;
		//�ж�������ĳ�����spec.c��
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x00;	//ȡ�����ݺ�, ��λ���
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d]�·���ȡ��������ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		}

		if (pump.cmd[pump.cmdlen - 1] != 0xf0) {
			RunLog("�������ݲ�����");
			continue;
		}

		//lrcУ��ʧ��
		lrc = LRC(pump.cmd, pump.cmdlen - 2);
		if ((lrc & 0x0f) != (pump.cmd[pump.cmdlen - 2] & 0x0f)) {
			RunLog("�ڵ�[%d]���[%d]��������У��ʧ��");
			continue;
		}

		return 0;
	}

	return -1;
}

/*
 * func: ȡĳ��ǹ�ı���
 */ 
static int GetGunTotal(int pos, __u8 gunLog, __u8 *vol_total, __u8 *amount_total)
{
	int ret ;
	int i;
	int  num;
	__u8 *p;

	ret = GetPumpTotal(pos);
	if (ret < 0) {
		RunLog("ȡFP %d ��ǹ�ı���ʧ��", pos + 1);
		return -1;
	}

	num = (pump.cmdlen - 4) / 30;	// ÿ����¼30�ֽ�,4���ֽ�Ϊ������ͷ,��β
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
 * func: ��ȡ���ν��׼�¼
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
			RunLog("�ڵ�[%d]���[%d]�·���ȡ���׼�¼����ʧ��", node , pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d]�·���ȡ���׼�¼����ɹ�, ȡ������Ϣʧ��", node , pos + 1);
			continue;
		}

		if (pump.cmd[pump.cmdlen - 1] != 0xf0) {
			RunLog("�ڵ�[%d]���[%d]������Ϣ����������", node, pos + 1);
			continue;
		}

		/*lrcУ��ʧ��*/
		lrc = LRC(pump.cmd, pump.cmdlen - 2);
		if ((lrc & 0x0f) != (pump.cmd[pump.cmdlen - 2] & 0x0f)) {
			RunLog("�ڵ�[%d]���[%d]��������У��ʧ��", node , pos + 1);
			continue;
		}
		
		//��֤
		if ((pump.cmd[2] & 0x0f) != pump.panelPhy[pos] ){
			RunLog("�����������Ŵ���");
			continue;
		} 

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
			RunLog("�ڵ�[%d]���[%d]�·���ͣ��������ʧ��", node, pos + 1);
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
				RunLog("�ڵ�[%d]���[%d]�·���Ȩ����ʧ��", node, pos + 1);
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
 * func: ��ȡ���ϴ�ʵʱ��������
 */
static int DoBusy(int pos)
{
	int ret;
	__u8 amount[5] = {0};
	__u8 volume[5] = {0};
	const __u8 arr_zero[3] = {0};
	int trycnt;

	if (first_time == 0) {
		// ��һ��ȡʵʱ��������ʧ��, ʱ�䳤һ��
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
			RunLog( "�� ȡʵʱ������������ ��ͨ�� %d ʧ��", pump.chnLog);
			continue;
		}

		memset( pump.cmd, 0, sizeof(pump.cmd) );
		pump.cmdlen = sizeof(pump.cmd) - 1;

		ret = GetReturnMsg( GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen );
		if( ret < 0 ) {
			RunLog( "�� ȡʵʱ������������ ��ͨ�� %d �ɹ�, ȡ������Ϣʧ��", pump.chnLog);
			continue;
		}

		volume[0] = 0x06;
		volume[1] = (pump.cmd[16] << 4) | (pump.cmd[15] & 0x0F);
		volume[2] = (pump.cmd[14] << 4) | (pump.cmd[13] & 0x0F);
		volume[3] = (pump.cmd[12] << 4) | (pump.cmd[11] & 0x0F);
		volume[4] = (pump.cmd[10] << 4) | (pump.cmd[9] & 0x0F);

		// ֻ�е���������Ϊ���˲��ܽ�FP״̬�ı�ΪFuelling
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
		RunLog("��ȡ��ǰ��ǹǹ��ʧ��");
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
 * func: ��ȡFP״̬
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
			RunLog("�ڵ�[%d]���[%d]��ȡFP״̬ʧ��", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d]��ȡFP״̬�ɹ�, ȡ������Ϣʧ��", node, pos + 1);
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
 * func: �ж�Dispenser�Ƿ�����
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
			RunLog("�ڵ�[%d]���[%d]��ȡFP״̬ʧ��", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d]��ȡFP״̬�ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		} 

		if ((pump.cmd[0] & 0x0f) != pump.panelPhy[pos]) {
			RunLog("�ڵ�[%d]���[%d]��ȡFP״̬�ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		}

		*status = pump.cmd[0] >> 4;
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
	  pump.gunLog[pos] = 1;	// ��ǰ̧�����ǹ	
	  RunLog("Node %d FP %d �� %d ��ņ̀��", node, pos + 1, pump.gunLog[pos]);
	  lpDispenser_data->fp_data[pos].Log_Noz_State = 1;
	  return 0;
}

/*
 * func: ��ʼ��pump.gunLog, Ԥ�ü���ָ����Ҫ; ���޸� Log_Noz_State
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
 * ��ʼ��FP�ĵ�ǰ�ͼ�. �������ļ��ж�ȡ��������,
 * д�� fp_data[].Current_Unit_Price
 * ���� PR_Id �� PR_Nb ��һһ��Ӧ��
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
 * func: ��ʼ����ǹ����
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
			RunLog("�ڵ�[%d]���[%d]��ǰ״̬���ܲ�ѯ����(%02x)", node, i + 1, status);
			return -1;
		}


		Ssleep(INTERVAL);
		ret = GetPumpTotal(i);
		if (ret < 0) {
			RunLog("ȡ FP%d �ı���ʧ��", i);
			return -1;
		}

		num = (pump.cmdlen-4)/40; /*ÿ����¼40�ֽ�,4���ֽ�Ϊ������ͷ,��β*/
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
				RunLog("InitPumpTotal,�Ƿ�����Ʒ��ַFP %d, LNZ %d, PR_Id %d", i, lnz + 1, pr_no + 1);
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
static int SetPort011()
{
	int ret;

//	if (gpGunShm->chnLogSet[pump.chnLog - 1] == 0) {	
	pump.cmd[0] = 0x1C;
	pump.cmd[1] = 0x52;
	pump.cmd[2] = 0x30|(pump.chnPhy-1);
	pump.cmd[3] = 0x03;	//������ 9600
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
 * func:  ͨѶ��ͨʱ��ѯ�����и�һ��ʱ������һ�´�������
 *        ����������ĵ�ѹ���ȵ��µ�Ƭ����λ--�����������ûָ�Ԥ��9600,8,n,1
 */
static int ReDoSetPort011()
{
#define RE_DO_FREQUENCY   10         //  10����ѯʧ������һ�´�������

	int static polling_fail_cnt = 0;
	int ret;

	if ((++polling_fail_cnt) >= RE_DO_FREQUENCY) {
		ret = SetPort011();
		if (ret < 0) {
			RunLog("�ڵ� %d ���贮������ʧ��", node);
		}
		polling_fail_cnt = 0;
	}

	return ret;
}



/*
 * ���FP�ϸ�ǹ����Ʒ�����Ƿ������ȷ(����ʵ��), ���ȫ����ȷ��ı�״̬ΪClosed
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
	RunLog("Node[%d]FP[%d]�����ѱ����ȷ, �л�״̬ΪClosed", node, pos + 1);

	return 0;
}


/*--------------------------end of lischn011.c-------------------------------*/

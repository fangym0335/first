/*
 *    Filename:  lischn013.c
 *
 * Description:  ���ͻ�Э��ת��ģ�� -- ����, 2400,8,n,1
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

#define FP_CLOSE        0xE0   // �ͻ���������ģʽʱ���Ϊ��״̬
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
static int fp_state[MAX_FP_CNT];        // 0 - IDLE, 1 - CALLING , 2 - DoStarted�ɹ�, 3 - Fuelling
static int dostop_flag = 0;             // ��ǳɹ�ִ�й� Terminate_FP

int ListenChn013(int chnLog)
{
	long	indtyp;
	int	ret;
	int	fd;
	int	j;
	int	i;
	__u8    status;
	__u8    cmd[8];
	
	sleep(5);   //��ֹ�˽����˳�,��������û���������. ÿ��lischnxxx����Ҫ
	RunLog("Listen Channel 013 ,logic channel[%d]", chnLog);
	memset(&pump, 0, sizeof(pump));
	pump.chnLog = chnLog;
	pump.panelCnt = 0;


	//1.   ɨ�蹲���ڴ�, ����ͨ�����ӵĴ���, FP		
	for (i = 0; i < MAX_CHANNEL_CNT; i++) {
		if (chnLog == gpIfsf_shm->node_channel[i].chnLog) {	// �߼�FP����ͬ
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
	
	//2.�򿪴���(�����ڴ����ϵ���չ��)�����ô�������
	
	strcpy(pump.cmd, alltty[pump.port-1]);
	pump.fd = OpenSerial(pump.cmd);
	if (pump.fd < 0) {
		RunLog("Node[%d]�򿪴���[%s]ʧ��", node, pump.cmd);
		return -1;
	}

	ret = SetPort013();
	if (ret < 0) {
		RunLog("Node[%d]���ô�������ʧ��",node);
		return -1;
	}
	
	//3.  ���Լ��ͻ��Ƿ�����,�����������ȴ�
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

		gpIfsf_shm->node_online[IFSFpos] = 0; //������,����������.
		UserLog("Node %d ���ܲ�����", node);
	}


	// 3.5����Ƿ��ܹ�������������
	if (!is_all_fp_ready()) {
		// δ�߱������������̵�����, �˳�����
		return -1;
	}

	// Clear TRs of all FP
	clr_his_trans(255);

	//4.��ʼ��Ĭ�ϵ�ǰ�߼�ǹ��
	ret = InitGunLog();
	if (ret < 0) {
		RunLog("Node[%d], ��ʼ��Ĭ�ϵ�ǰ�߼�ǹ��ʧ��",node);
		return -1;
	}

	 //5.��ʼ����Ʒ���۵����ݿ�
	ret = InitCurrentPrice();
	if (ret < 0) {
		RunLog("Node[%d], ��ʼ����Ʒ���۵����ݿ�ʧ��",node);
		return -1;
	}

	//6.��ʼ�����뵽���ݿ�
	ret = InitPumpTotal();
	if (ret < 0) {
		RunLog("Node[%d], ��ʼ�����뵽���ݿ�ʧ��",node);
		return -1;
	}

	
//	gpIfsf_shm->node_online[IFSFpos] = node;
	ChangeNode_Status(node, NODE_ONLINE);
	UserLog("Node %d ����,״̬[%02x]!", node, status);
	keep_in_touch_xs(12);               // ȷ��CD�Ѿ��յ�����֮��������������

	// =============== Part.7 �ͻ�ģ���ʼ�����,ת��FP״̬Ϊ Idle ==================
	// when dispenser is initialized , the status is Closed. it's waiting for set command from CD. 
	for (i = 0; i < lpNode_channel->cntOfFps; i++) {
		if ((__u8)FP_State_Closed != lpDispenser_data->fp_data[i].FP_State) {
			ret = ChangeFP_State(node, 0x21 + i, FP_State_Closed, NULL);
		}
	}

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
			RunLog("ϵͳ��ͣsysStat = %d, fp_data[0].FP_State = %02x",
				gpGunShm->sysStat,lpDispenser_data->fp_data[0].FP_State);
		}

		indtyp = pump.indtyp;

		// ���ȼ���Ƿ�������
		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		ret = -1;

		if (gpIfsf_shm->auth_flag - FCC_SWITCH_AUTH != 0) {  // ��̨����
			// �鿴�Ƿ����Լ�����Ϣ
			ret = RecvMsg(ORD_MSG_TRAN, pump.cmd, &pump.cmdlen, &indtyp, TIME_FOR_RECV_CMD); 
			if (ret == 0) {
				ret = DoCmd();
				if (ret != 0) {
					RunLog("Node %d �����̨����ʧ��", node);
				}
			}

			DoPolling();
//			Ssleep(100000);       // ������ѯ���Ϊ100ms + INTERVAL
		} else {
			Ssleep(200000);
			DoPolling();
		}
	}

}

/*
 * func: ִ����λ������
 */
static int DoCmd()
{
	int ret;
	int fp_id;
	int pos;    //pos is No. of  fuelling poit(panel) of a dispenser: 0, 1, 2, 3
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
			if (pump.cmd[0] != 0x18 && pump.cmd[0] != 0x19) {  // DoPrice & DoMode����Ҫ����FP��
				RunLog("Node[%d] , �Ƿ���FP��ַ",node);
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
		case 0x19:	// ���Ŀ���ģʽ
//			ret = DoMode();	      // Unsupported
			break;
		case 0x1C:	// Terminate_FP
			ret = HandleStateErr(node, pos, Terminate_FP,
				lpDispenser_data->fp_data[pos].FP_State, msgbuffer); 
			if (ret < 0) {       // �������󣬵�ǰ״̬���������ò���
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
			RunLog("Node[%d], ���ܵ�δ��������[0x%02x]",node, pump.cmd[0]);
			break;
	}

	return ret;
}



/*
 * func: ��FP����״̬��ѯ, ����״̬����Ӧ����
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
	static int fail_cnt = 0;  // ��¼GetPumpStatus ʧ�ܴ���, �����ж�����
	static __u8 flag_soft_offline = 0;

	__u8 status;
	
	for (i = 0; i < pump.panelCnt; i++) {
		ret = GetPumpStatus(i, &status);
		if (ret < 0 || (0xFF == flag_soft_offline)) {
			flag_soft_offline = 0;

			if (gpIfsf_shm->node_online[IFSFpos] == node) {
				if ((++fail_cnt) < MAX_FAIL_CNT) {  // ���ͻ������ж�
					/*
					 * �����贮�����Իᵼ��ReadTTY���յ�"�Ƿ�����", ����ʱ�Ƴ�
					 * 2009.02.19
					if (fail_cnt >= (MAX_FAIL_CNT / 2)) {
						if (SetPort013() >= 0) {
							RunLog("Node %d ���贮�����Գɹ�", node);
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
				RunLog("====== Node %d ����! =============================", node);

				// Step.3 �ͻ�����, �����ϴ�״̬�ı���Ϣ, ���Բ�ʹ��ChangeFP_State, ֱ�Ӳ���FP_State
				for (j = 0; j < pump.panelCnt; ++j) {
					lpDispenser_data->fp_data[j].FP_State = FP_State_Inoperative;
					lpDispenser_data->fp_data[j].Log_Noz_State = 0;
				}
			}
			
			// ���贮������
			//ReDoSetPort013();
			continue;
		} else {
			if (gpIfsf_shm->node_online[IFSFpos] == 0) {
				if ((--fail_cnt) > 0) {
					fail_cnt = 1;   // �ӿ������ٶ�
					continue;
				} else if (time(NULL) < pump.time_offline) {
					RunLog("============== waiting for 30s =============");
					continue;
				} 

				// fail_cnt��λ
				fail_cnt = 0;

				// Step.1                // �����ʷ����,ת��״̬λFP_IDLE
				clr_his_trans(255);

				// Step.2
				ret = InitPumpTotal();   // ���±��뵽���ݿ�
				if (ret < 0) {
					RunLog("Node[%d]���±��뵽���ݿ�ʧ��", node);
					continue;
				}

				// Step.3
				for (j = 0; j < pump.panelCnt; ++j) {
					lpDispenser_data->fp_data[j].FP_State = FP_State_Closed;
				}

//				gpIfsf_shm->node_online[IFSFpos] = node;
				ChangeNode_Status(node, NODE_ONLINE);
				RunLog("====== Node %d �ָ�����! =============================", node);
				keep_in_touch_xs(12);               // ȷ��CD�Ѿ��յ�����֮��������������
				// Step.4                 // �����ϴ���ǰFP״̬
				for (j = 0; j < pump.panelCnt; ++j) {
					ChangeFP_State(node, j + 0x21, FP_State_Closed, NULL);
				}
				continue;
			}

			fail_cnt = 0;
		}


		if (gpIfsf_shm->auth_flag - FCC_SWITCH_AUTH != 0) {  //����к�̨
			if ((__u8)FP_State_Closed == lpDispenser_data->fp_data[i].FP_State) {
				if (i == 0) {
					times += pump.panelCnt;					
				}
				if (times > (SENDSTATE *  pump.panelCnt)) {
					if (i >= pump.panelCnt -1)
						times = 0;
					UserLog("Node[%d]��FP[%d]����Closed״̬", node, i + 1);
				}

				// Closed ״̬��ҲҪ�ܹ����
				ChangePrice();
				continue; 
			}
		} else {
			if ((__u8)FP_State_Closed == lpDispenser_data->fp_data[i].FP_State) {
				RunLog("Node[%d]FP[%d], DoPolling, û�к�̨�ı�״̬ΪIDLE", node,i + 1);
				ret = SetHbStatus(node, 0);
				ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
			}
		}

		// ��FP״̬ΪInoperative, Ҫ���˹���Ԥ,�����ܸı���״̬,Ҳ�����ڸ�״̬�����κβ���
		if ((__u8)FP_State_Inoperative == lpDispenser_data->fp_data[i].FP_State) {
			RunLog("Node[%d]FP[%d] ����Inoperative״̬!", node, i + 1);
			// Inoperative ״̬��ҲҪ�ܹ����
			ChangePrice();

			CheckPriceErr(i);
			continue;
		}

		switch (status) {
		case FP_IDLE:
			if (fp_state[i] >= 2) {            // ��û�вɵ�EOT״̬�Ĳ��䴦��
				// 1. ChangeFP_State to IDLE first
				//�������ȷ���״̬�󱣴潻�׼�¼������
			       //��Ҫ����tmp_lnz�ѵ�ǰ�õ���ǹ���������������¼ʱ�ã�
			       //���򱣴�ļ�¼ǹ�ž���0��
				tmp_lnz = lpDispenser_data->fp_data[i].Log_Noz_State;
				lpDispenser_data->fp_data[i].Log_Noz_State = 0x00;     
				if (((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State) && \
					((__u8)FP_State_Inoperative != lpDispenser_data->fp_data[i].FP_State)) {
					ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
				}
				RunLog("Node %d FP %d �� %d ��ǹ��ǹ", node, i + 1, pump.gunLog[i]);
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
		case FP_CALLING:			// ̧ǹ
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
			// ���䴦��
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
			// ���䴦��
			if (fp_state[i] < 2) {
				if (DoStarted(i) < 0) {
					continue;
				}
			}

			if (fp_state[i] >= 2) {
				// 1. ChangeFP_State to IDLE first
				//�������ȷ���״̬�󱣴潻�׼�¼������
				//��Ҫ����tmp_lnz�ѵ�ǰ�õ���ǹ���������������¼ʱ�ã�
				//���򱣴�ļ�¼ǹ�ž���0��
				tmp_lnz = lpDispenser_data->fp_data[i].Log_Noz_State;
				lpDispenser_data->fp_data[i].Log_Noz_State = 0x00;     
				if (((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State) && \
					((__u8)FP_State_Inoperative != lpDispenser_data->fp_data[i].FP_State)) {
					ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
				}
				RunLog("Node %d FP %d �� %d ��ǹ��ǹ", node, i + 1, pump.gunLog[i]);
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
			flag_soft_offline = 0xFF;   // �������ڼ��ͻ��������ѻ�����, ��ôģ���ͻ�����, ������Ӧ����;
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
		RunLog("Node[%d] ����״̬�ı����",node);
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

	// GetCurrentLNZ & BeginTr4Fp �ɹ�, ����λ fp_state[i] Ϊ 2
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
			RunLog("Node[%d]FP[%d]дԤ�ü�������ʧ��", node, pos + 1);
			continue;
		}

		RunLog("��Ԥ�ü�������[%s]��Node[%d].FP[%d]�ɹ�",
			Asc2Hex(pump.cmd, pump.cmdlen), node, pos + 1);

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]дԤ�ü�������ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		}

		if (0xFC != pump.cmd[0] || 0xFF != pump.cmd[6]) {
			RunLog("Node[%d]FP[%d]�������ݴ���", node, pos + 1);
			continue;
		}

		if (((pump.cmd[4] << 4) | pump.cmd[5]) != 
			checksum_hongyang(&pump.cmd[1], pump.wantLen - 4, NULL)) {
			RunLog("Node[%d]FP[%d]��������У�����", node, pos + 1);
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
 * func: ���ͽ��������н��׼�¼�Ķ�ȡ�ʹ���
 *       �㽻�׵��жϺʹ�����DoEot����, DoPoling ������
 * ret : 0 - �ɹ�; -1 - ʧ��
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
	
	RunLog("Node[%d]FP[%d]�����µĽ���", node, pos + 1);

	//1. ��ñ��룬���ۻ����ͽ��
	err_cnt = 0;
	do {
		ret = GetPumpTotal(pos);
		if (ret < 0) {
			err_cnt++;
			RunLog("Node[%d]FP[%d] ȡ���ͻ�����ʧ��",node,pos+1);
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

	//2. �������Ƿ��б仯,������򱣴潻�׼�¼
	if (memcmp(pre_total, meter_total, 7) != 0) {
#if 0
		RunLog("ԭ����Ϊ %s",Asc2Hex(lpDispenser_data->fp_m_data[pr_no].Meter_Total,7));
		RunLog("��ǰ����Ϊ %s",Asc2Hex(meter_total,7));	  
#endif
		err_cnt = 0;
		do {
			ret = GetTransData(pos);	
			if (ret != 0) {
				err_cnt++;
				RunLog("Node[%d]FP[%d]ȡ���׼�¼ʧ��", node, pos + 1);
			}

			// �Ƚϱ����ж�ȡ���Ľ����Ƿ������µĽ���
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

		//3. �õ���Ʒid 
		pr_no = (lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).PR_Id - 1;  // 0 - 7	
		if ((pr_no < 0) || (pr_no > 7)) {
			RunLog("DoEot PR_Id error, PR_Id[%d] <ERR>", pr_no + 1);
		}
				
		memcpy(lpDispenser_data->fp_m_data[pr_no].Meter_Total, meter_total, 7);		
		memcpy((lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Vol_Total, meter_total, 7);

#if 0
		RunLog("Node[%02d]FP[%d]ǹ[%d]## Amount: %02x%02x.%02x, Volume: %02x%02x.%02x, Price: %02x.%02x, Total: %02x%02x%02x%02x.%02x",
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
				RunLog("Node[%d]FP[%d]DoEot.EndTr4Ln ʧ��, ret: %d <ERR>", node,pos +1 ,ret);
				err_cnt++;
			}
		} while (ret < 0 && err_cnt <= 5);

		err_cnt = 0;
		do {
			ret = EndTr4Fp(node, pos + 0x21, pre_total,
				(lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Vol_Total);
			if (ret < 0) {
				RunLog("Node[%d]FP[%d] DoEot.EndTr4Fp ʧ��, ret: %d <ERR>", node,pos + 1,ret);
				err_cnt++;
			}
		} while (ret < 0 && err_cnt <= 8);

	}else {
		GetTransData(pos);    // ����ת��״̬
	
		ret = EndZeroTr4Fp(node, pos + 0x21);
		if (ret < 0) {
			RunLog("DoEot.EndZeroTr4Fp ʧ��, ret: %d <ERR>", ret);
		}
	}
	
	
	return 0;
}

/*
 * func: ��ͣ���ͣ��ڴ���Terminate_FP��
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
			RunLog("Node[%d]FP[%d]�·�ֹͣ��������ʧ��", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd)-1;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]�·�ֹͣ��������ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		}
		
		if (0xFC != pump.cmd[0] || 0xFF != pump.cmd[6]) {
			RunLog("Node[%d]FP[%d]�������ݴ���", node, pos + 1);
			continue;
		}

		if (((pump.cmd[4] << 4) | pump.cmd[5]) != 
			checksum_hongyang(&pump.cmd[1], pump.wantLen - 4, NULL)) {
			RunLog("Node[%d]FP[%d]��������У�����", node, pos + 1);
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
			RunLog("Node[%d]FP[%d]�·���Ȩ����ʧ��",node,pos +1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd)-1;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]��Ȩʧ��",node,pos+1);
			continue;
		}
		
		if (0xFC != pump.cmd[0] || 0xFF != pump.cmd[6]) {
			RunLog("Node[%d]FP[%d]�������ݴ���", node, pos + 1);
			continue;
		}

		if (((pump.cmd[4] << 4) | pump.cmd[5]) != 
			checksum_hongyang(&pump.cmd[1], pump.wantLen - 4, NULL)) {
			RunLog("Node[%d]FP[%d]��������У�����", node, pos + 1);
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
 * func: �ϴ�ʵʱ��������
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
 * func: ̧ǹ���� - ����̧ǹ�ź�
 */
static int DoCall(int pos)
{
	int ret;
	__u8 ln_status;

	ret = GetCurrentLNZ(pos);
	if (ret < 0) {
		RunLog("Node[%d]FP[%d], ��ȡ��ǰ��ǹǹ��ʧ��",node,pos + 1);
		return -1;
	}

	ret = ChangeFP_State(node, pos + 0x21, FP_State_Calling, NULL);

	//��λ������,���������Զ���Ȩ;��λ��������, ��Ȩ�������ڿ���״̬,��ʱ�Զ���Ȩ
	if ((gpIfsf_shm->auth_flag == FCC_SWITCH_AUTH) && (lock_status() == 1)) {
		ret = DoAuth(pos);
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]�Զ���Ȩʧ��",node,pos +1);
		}
		ret = ChangeFP_State(node, pos + 0x21, FP_State_Started, NULL);
		RunLog("Node[%d]FP[%d]�Զ���Ȩ�ɹ�",node ,pos +1);
		SO_OFFLINE_TR(node, pos);
	}

	return ret;
}



/*
 * func: ִ�б��, ʧ�ܸı�FP״̬ΪINOPERATIVE, ��д�������ݿ�
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
              //һ̨���ͻ�ֻ��һ����ƷҲֻ��һ���ͼ�
		if (pump.newPrice[pr_no][2] != 0x00 || pump.newPrice[pr_no][3] != 0x00) {	
			for (i = 0; i < pump.panelCnt; i++) {
				// 1. ���ж��ͻ�����ǹ״̬
				err_code = 0x99;
				for (j = pump.panelCnt - 1; j >= 0; --j) {
					ret = GetPumpStatus(i, &status);
					if (ret != 0 || status != FP_IDLE) {
						return 1;
					}
				}

				// 2. �·��������
				for (j = 0; j < MAX_FP_LN_CNT; j++) { // ������gunCnt,1ǹ���ܲ���
#if DEBUG
					RunLog(">>> fp_ln_data[%d][%d].PR_Id: %d", i, j,
							lpDispenser_data->fp_ln_data[i][j].PR_Id);
#endif
					if (lpDispenser_data->fp_ln_data[i][j].PR_Id - 1 == pr_no) {
					err_code_tmp = 0x99;
				CriticalLog("�·����������ڵ�[%02d]��[%d]ǹ[%d]......", node, i + 1, j + 1);

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
							RunLog("Node[%d]FP[%d]�·��������ʧ��", node,i + 1);
							continue;
						} 

						RunLog("Node[%d].FP[%d].PriceChange_CMD: %s",
							node, i + 1, Asc2Hex(pump.cmd, pump.cmdlen));

						// ��۳ɹ����µ���д�����ݿ� & ���㻺��

						memset(pump.cmd, 0, sizeof(pump.cmd));
						pump.cmdlen = sizeof(pump.cmd) - 1;

						ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
						if (ret < 0) {
							RunLog("Node[%d]FP[%d]���������ɹ�, ȡ�ظ���Ϣʧ��", node, i + 1);
							continue;
						}

						if (0xFC != pump.cmd[0] || 0xFF != pump.cmd[6]) {
							RunLog("Node[%d]FP[%d]�������ݴ���", node, i + 1);
							err_code_tmp = 0x37;
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
							continue;
						}

						if (((pump.cmd[4] << 4) | pump.cmd[5]) != 
							checksum_hongyang(&pump.cmd[1], pump.wantLen - 4, NULL)) {
							RunLog("Node[%d]FP[%d]��������У�����", node, i + 1);
							err_code_tmp = 0x37;
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
							continue;
						}

						if (0x99 != pump.cmd[3]) {
							RunLog("Node[%d]FP[%d]ִ�б������ʧ��", node, i + 1);
							err_code_tmp = 0x37;
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
							continue;
						} 

						ret = GetPrice(i);
						if (ret == 0 && (memcmp(&pump.cmd[3], &pump.newPrice[pr_no][2], 2) != 0)) {
							RunLog("Node[%d]FP[%d]ִ�б�۳ɹ�, ����ǰ�������µ��۲�ͬ",
														node, i + 1);
							err_code_tmp = 0x37; // ���ִ��ʧ��
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
							continue;
						}

						fm_idx = lpDispenser_data->fp_data[i].Default_Fuelling_Mode - 1;
					RunLog("Node[%d]FP[%d]���ͱ������ɹ�,�µ���fp_fm_data[%d][%d].Prode_Price: %s",
							node, i + 1, pr_no, fm_idx,
						       	Asc2Hex(lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price, 4));
					CriticalLog("�ڵ�[%02d]��[%d]ǹ[%d]��۳ɹ�,�µ���Ϊ%02x.%02x",
						       	node, i + 1, j + 1,
							lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2],
							lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3]);

						err_code_tmp = 0x99;
						break;
					}

					if (trynum >= TRY_TIME) {
						// Write Error Code:  price Change Error (Spare)
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

				Ssleep(2000000 - INTERVAL);
			} // end of for.i < panelCnt
			bzero(&pump.newPrice[pr_no], 4);
		} // end of if.newPrice
	} // end of for.pr_no

	return 0;
}


/*
 * func: ��ȡ��FP����ǹ�ı���
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
			RunLog("Node[%d]FP[%d]�·�ȡ��������ʧ��",node,pos +1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]�·�ȡ��������ɹ�, ȡ������Ϣʧ��",node,pos +1);
			continue;
		}

		if (0xFC != pump.cmd[0] || 0xFF != pump.cmd[17]) {
			RunLog("Node[%d]FP[%d]�������ݴ���", node, pos + 1);
			continue;
		}

		if (((pump.cmd[15] << 4) | pump.cmd[16]) != 
			checksum_hongyang(&pump.cmd[1], pump.wantLen - 4, NULL)) {
			RunLog("Node[%d]FP[%d]״̬����У�����", node, pos + 1);
			continue;
		}

		return 0;
	}

	return -1;
}


/*
 * func: ��ȡ���ν��׼�¼
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
			RunLog("Node[%d]FP[%d]�·�����������ʧ��",node ,pos +1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]�·�����������ɹ�, ȡ������Ϣʧ��",node ,pos +1);
			continue;
		}

		if (0xFC != pump.cmd[0] || 0xFF != pump.cmd[25]) {
			RunLog("Node[%d]FP[%d]�������ݴ���", node, pos + 1);
			continue;
		}

		if (((pump.cmd[23] << 4) | pump.cmd[24]) != 
			checksum_hongyang(&pump.cmd[1], pump.wantLen - 4, NULL)) {
			RunLog("Node[%d]FP[%d]��������У�����", node, pos + 1);
			continue;
		}
		return 0;
	}

	return -1;
}

/*
 * func: ��ȡFP״̬
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
			RunLog("Node[%d]FP[%d]����״̬����ʧ��",node,pos +1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]����״̬����ɹ�, ȡ������Ϣʧ��",node,pos +1);
			continue;
		} 

		if (0xFC != pump.cmd[0] || 0xFF != pump.cmd[6]) {
			RunLog("Node[%d]FP[%d]�������ݴ���", node, pos + 1);
			continue;
		}

		if (((pump.cmd[4] << 4) | pump.cmd[5]) != 
			checksum_hongyang(&pump.cmd[1], pump.wantLen - 4, NULL)) {
			RunLog("Node[%d]FP[%d]��������У�����", node, pos + 1);
			continue;
		}

		*status = pump.cmd[3];

		if (*status != pre_status[pos]) {
			pre_status[pos] = *status;
			RunLog("Node[%d]FP[%d]��ǰ״̬[%02x]", node, pos + 1, *status);
		}
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
			RunLog("Node[%d]FP[%d]����״̬����ʧ��",node,pos +1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]����״̬����ɹ�, ȡ������Ϣʧ��",node,pos +1);
			continue;
		} 

		if (0xFC != pump.cmd[0] || 0xFF != pump.cmd[6]) {
			RunLog("Node[%d]FP[%d]�������ݴ���", node, pos + 1);
			continue;
		}

		if (((pump.cmd[4] << 4) | pump.cmd[5]) != 
			checksum_hongyang(&pump.cmd[1], pump.wantLen - 4, NULL)) {
			RunLog("Node[%d]FP[%d]��������У�����", node, pos + 1);
			continue;
		}

		*status = pump.cmd[3];
		return 0;
	}

	return -1;
}

/*
 * func: ��ȡFP״̬
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
			RunLog("Node[%d]FP[%d]����״̬����ʧ��",node,pos +1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]����״̬����ɹ�, ȡ������Ϣʧ��",node,pos +1);
			continue;
		} 

		if (0xFC != pump.cmd[0] || 0xFF != pump.cmd[14]) {
			RunLog("Node[%d]FP[%d]�������ݴ���", node, pos + 1);
			continue;
		}

		if (((pump.cmd[12] << 4) | pump.cmd[13]) != 
			checksum_hongyang(&pump.cmd[1], pump.wantLen - 4, NULL)) {
			RunLog("Node[%d]FP[%d]��������У�����", node, pos + 1);
			continue;
		}

		return 0;
	}

	return -1;
}

/*
 * func: ��ȡ��Ʒ����
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
			RunLog("Node[%d]FP[%d]������������ʧ��", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]������������ɹ�, ȡ������Ϣʧ��",node,pos +1);
			continue;
		} 

		if (0xFC != pump.cmd[0] || 0xFF != pump.cmd[7]) {
			RunLog("Node[%d]FP[%d]�������ݴ���", node, pos + 1);
			continue;
		}

		if (((pump.cmd[5] << 4) | pump.cmd[6]) != 
			checksum_hongyang(&pump.cmd[1], pump.wantLen - 4, NULL)) {
			RunLog("Node[%d]FP[%d]��������У�����", node, pos + 1);
			continue;
		}

		return 0;
	}

	return -1;
}

/*
 * func: ��ȡ��ǰ̧����ǹ��ǹ��, ���д��pump.gunLog[]����ChangeLog_Noz_State
 * ��ǹ��
 * ret : 0 - �ɹ�, -1 - ʧ�� 
 */
static int GetCurrentLNZ(int pos)
{
	  RunLog("Node[%d]FP[%d] %d ��ņ̀��", node, pos + 1, pump.gunLog[pos]);
	  lpDispenser_data->fp_data[pos].Log_Noz_State = 1 << pump.gunLog[pos] - 1;	// ��ǰ̧�����ǹ	

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
 * func: ��ʼ����ǹ����
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
			RunLog("Node[%d]FP[%d]ȡ����ʧ��", node,i+1);
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
 *  func: ��ʼ��gunLog, Ԥ�ü���ָ����Ҫ; ���޸� Log_Noz_State
 *  ������ͻ�һ��ͨ����Ӧһ��ǹ
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
 * func:  ͨѶ��ͨʱ��ѯ�����и�һ��ʱ������һ�´�������
 *        ����������ĵ�ѹ���ȵ��µ�Ƭ����λ--�����������ûָ�Ԥ��9600,8,n,1
 */
static int ReDoSetPort013()
{
#define RE_DO_FREQUENCY   10         //  10����ѯʧ������һ�´�������

	int static polling_fail_cnt = 0;
	int ret;

	if ((++polling_fail_cnt) >= RE_DO_FREQUENCY) {
		ret = SetPort013();
		if (ret < 0) {
			RunLog("Node %d ���贮������ʧ��", node);
		}
		polling_fail_cnt = 0;
	}

	return ret;
}

/*
 * func: �趨����ģʽ
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
			RunLog("Node[%d]FP[%d]�·���������ģʽ����ʧ��", node, pos + 1);
			continue;
		}
		
		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]�·���������ģʽ����ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		}

		if (0xFC != pump.cmd[0] || 0xFF != pump.cmd[6]) {
			RunLog("Node[%d]FP[%d]�������ݴ���", node, pos + 1);
			continue;
		}

		if (((pump.cmd[4] << 4) | pump.cmd[5]) != 
			checksum_hongyang(&pump.cmd[1], pump.wantLen - 4, NULL)) {
			RunLog("Node[%d]FP[%d]��������У�����", node, pos + 1);
			continue;
		}

		if (0x99 != pump.cmd[3]) {
			RunLog("Node[%d]����ģʽ����ʧ��!", node);
			continue;
		} 

		RunLog("Node[%d]����ģʽ���óɹ�!", node);
		return 0;
	}

	return -1;
}

/*
 * func: ���ĳ��FP����ȫ��FP����ʷ����, ����״̬ת��ΪFP_IDLE
 *       pos: 255 ��ʾ����ȫ��FP
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
 * func: �趨�������ԡ�ͨѶ���ʡ�У��λ��
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

	Ssleep(INTERVAL);       // ȷ��ʹ��ǰ���������Ѿ����ú�

	RunLog("Node[%d]���ô������Գɹ�", node);

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
 * func: ������������У���(BCC)
 *       return ����δ����ֵ�У���
 *       result ���ز�ֺ�����ֽ�У����
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
 *  func: �������FP��״̬, ���Ƿ����������������
 *        ��������Ȩ/���ڼ���/����ֹͣ��״̬��ȴ�
 *  return:  0 - ������; 1 - ����
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
					DoStop(idx);  // ȡ��Ԥ��Ȩ
				}
				break;         // ����������, �˳����µȴ�
			} else {
				continue;      // �����һ��FP
			}
		}

		if (idx >= pump.panelCnt) {
			break;                 // ȫ����������, ���˳�ѭ��������������
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
			node,pos+1,lnz+1, pump.cmd[3], pump.cmd[4],
		       	lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2], 
		       	lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3]);

		if ((pump.cmd[3] != lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2]) ||
			(pump.cmd[4] != lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3])) {
			return -2;
		}
	}
	
	ChangeFP_State(node, 0x21 + pos, FP_State_Closed, NULL);
	RunLog("Node[%d]FP[%d]�����ѱ����ȷ, �л�״̬ΪClosed", node, pos + 1);

	return 0;
}


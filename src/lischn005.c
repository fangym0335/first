/*
 * Filename: lischn005.c
 *
 * Description: ���ͻ�Э��ת��ģ�� - ��������, CL, 9600,E,8,1 
 *
 * History:
 *
 * 2008/05/09 - Guojie Zhu <zhugj@css-intelligent.com>
 * ������Ȩ����û�лָ�������Fuel Server ���·� Clear TR ����,���յ��º������Ͳ���Ȩ������
 * 2008/04/08 - Guojie Zhu <zhugj@css-intelligent.com>
 *
 * ����˵��:
 *   ���ڻظ�ACK������, ����ע��BB/CC���ж�, ����У�����ֻ���м�¼
 *   �����ڻظ������ݵ�����, �����ע��У����ж�, У������������
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
	
	sleep(5);   //��ֹ�˽����˳�,��������û���������. ÿ��lischnxxx����Ҫ
	RunLog("Listen Channel  005 ,logic channel[%d]", chnLog);
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
				pump.addInf[j][0] = 0;    // ��ʶ��ȡ���׺��Ƿ��н���δ����ɹ�
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

//	RunLog("pump.port = [%d]", pump.port);
	strcpy(pump.cmd, alltty[pump.port - 1]);
	pump.fd = OpenSerial(pump.cmd);
	if (pump.fd < 0) {
		RunLog("�򿪴���[%s]ʧ��", pump.cmd);
		return -1;
	}
//	RunLog("�򿪴���[%s].fd=[%d]", pump.cmd, pump.fd);

	ret = SetPort005();
	if (ret < 0) {
		RunLog("���ô�������ʧ��");
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
		UserLog("Node %d ������", node);
		
		sleep(1);
	}
	

	// ============= Part.4 ��ʼ������ͻ���ǰ״̬��ص����ݿ������ =======================
	
	for (i = 0;  i < pump.panelCnt; ++i) {
		if (SetCtrlMode(i, 0xAA) < 0) {   // ����Ϊ����ģʽ
			RunLog("Node[%d]FP[%d]���ÿ���ģʽʧ��", node, i + 1);
			return -1;
		}
	}

	// IDLE���ٽ����������� , Ҫ���ڳ�ʼ������֮ǰ
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
	
	// clean history TR   // for test
	// ����������ʱ��Ҳ��Ҫ��ô��
	
	if (clr_offline_trans() < 0) {
		return -1;
	}

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
					RunLog("Node %d ������̨����ʧ��", node);
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
			// Step.1 �Ƚ��д�����, ��鵱ǰ״̬�Ƿ��������иò���
			ret = HandleStateErr(node, pos, Terminate_FP,
				lpDispenser_data->fp_data[pos].FP_State, msgbuffer); 
			if (ret < 0) {
				break;
			}
			// Step.2 ִ������, ���ɹ��ı� FP ״̬, ���򷵻� NACK
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
			ret = HandleStateErr(node, pos, Do_Auth,     // Note. ������Do_Auth ������ Release_FP 
				lpDispenser_data->fp_data[pos].FP_State, msgbuffer); 
			if (ret < 0) {
				break;
			}
			ret = DoAuth(pos);				
			if (ret == 0) {
				// �������⴦��, ��Ϊ������Started״̬
				// ���ͻ��������и�״̬ʱ, ״̬�л����ͻ�ʵ���������
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
	__u8 status;
	static int times = 0;
	static int fail_cnt = 0;  // ��¼GetPumpStatus ʧ�ܴ���, �����ж�����
	
	for (i = 0; i < pump.panelCnt; i++) {
		ret = GetPumpStatus(i, &status);
		if (ret < 0) {
			if (gpIfsf_shm->node_online[IFSFpos] == node) {
				if ((++fail_cnt) < MAX_FAIL_CNT) {  // ���ͻ������ж�
					// ��ֹ��ѹ������ɵ�Ƭ�������������ø�λ
					/*
					 * �����贮�����Իᵼ��ReadTTY���յ�"�Ƿ�����", ����ʱ�Ƴ�
					 * 2009.02.19
					if (fail_cnt >= (MAX_FAIL_CNT / 2)) {
						if (SetPort005() >= 0) {
							RunLog("�ڵ� %d ���贮�����Գɹ�", node);
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
				RunLog("====== Node %d ����! =============================", node);

				// �ͻ�����, �����ϴ�״̬�ı���Ϣ, ���Բ�ʹ��ChangeFP_State, ֱ�Ӳ���FP_State
				for (j = 0; j < pump.panelCnt; ++j) {
					lpDispenser_data->fp_data[j].FP_State = FP_State_Inoperative;
					lpDispenser_data->fp_data[j].Log_Noz_State = 0;
				}
			}

			// ���贮������
			//ReDoSetPort005();
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

				// Step.1
				for (j = 0;  j < pump.panelCnt; ++j) {
					if (SetCtrlMode(j, 0xAA) < 0) {   // ����Ϊ����ģʽ
						RunLog("Node[%d]FP[%d]���ÿ���ģʽʧ��", node, j + 1);
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
				ret = InitPumpTotal();    // ��ʼ�����뵽���ݿ�
				if (ret < 0) {
					RunLog("���±��뵽���ݿ�ʧ��");
					fail_cnt = 1;
					continue;
				}

				// Step.4
				for (j = 0;  j < pump.panelCnt; ++j) {
					lpDispenser_data->fp_data[j].FP_State = FP_State_Closed;
				}

//				gpIfsf_shm->node_online[IFSFpos] = node;
				ChangeNode_Status(node, NODE_ONLINE);
				RunLog("====== Node %d �ָ�����! =========================", node);
				keep_in_touch_xs(12);               // ȷ��CD�Ѿ��յ�����֮��������������
				// Step.5                 // �����ϴ���ǰFP״̬
				for (j = 0; j < pump.panelCnt; ++j) {
					ChangeFP_State(node, j + 0x21, FP_State_Closed, NULL);
				}
				continue;
			}
			
			if (fail_cnt >= 2) {
				ret = 0;
				for (j = 0;  j < pump.panelCnt; ++j) {
					if (SetCtrlMode(j, 0xAA) < 0) {   // ����Ϊ����ģʽ
						RunLog("Node[%d]FP[%d]���ÿ���ģʽʧ��", node, j + 1);
						ret--;
					}
				}

				if (ret >= 0) {
					fail_cnt = 0;
				}
				// ���ܳɷ�����continue
				continue;
			}
			fail_cnt = 0;
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

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = 0;
		
		switch (status & 0x0F) {
		case FP_INOPERATIVE:
			// ��״̬����Ϊ 
			break;
		case FP_IDLE:                                   // ��ǹ������
			if (fp_state[i] != 0) {   // Note.��û�вɵ���ǹ�ź�(EOT)�Ĳ��䴦��
				// ���мӹ���, �����״̬�ı�ͽ��״���
				// 1. ChangeFP_State to IDLE first
				tmp_lnz = lpDispenser_data->fp_data[i].Log_Noz_State;
				if ((status & 0x80) == 0) {   // ���׽���������δ��ǹ
					lpDispenser_data->fp_data[i].Log_Noz_State = 0x00;     
				}
				if ((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State) {
					ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
				}
				RunLog("Node %d FP %d �� %d ��ǹ��ǹ", node, i + 1, pump.gunLog[i]);

				// Step2. Get & Save TR to TR_BUFFER
				lpDispenser_data->fp_data[i].Log_Noz_State = tmp_lnz;
				if ((status & 0x10) != 0) {
					ret = DoEot(i);
				} else {
					EndZeroTr4Fp(node, i + 0x21);
				}

				if ((status & 0x80) == 0) {   // ���׽���, �����ѹ�ǹ
					lpDispenser_data->fp_data[i].Log_Noz_State = 0x00;
				} 
			} else {
				// ������Ȩ��ʱ�����, ��Ȩ��ʱ���γ�һ�ʽ���, ����������
				// 63 -> 23 -> 25 -> 73
				if ((status & 0x10) != 0) {
				       if (clr_offline_trans() < 0) {
						pump.addInf[i][0] = 1;
				       }
					pump.addInf[i][0] = 0;
				}
			}

			fp_state[i] = 0;
			// Terminate_FP ��FP״̬��ΪIdle, ������ǹ��Ϊ̧��״̬, ���䴦��
			if (((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State) ||
				(lpDispenser_data->fp_data[i].Log_Noz_State != 0 && ((status & 0x80) == 0))) {
				lpDispenser_data->fp_data[i].Log_Noz_State = 0x00;
				ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
				RunLog("Node %d FP %d �� %d ��ǹ��ǹ", node, i + 1, pump.gunLog[i]);
			}


			EO_OFFLINE_TR(node, i);
			if (gpIfsf_shm->fp_offline_flag[node - 1][i] != 0) {
				upload_offline_tr(node, NULL);
			}

			ret = ChangePrice();                    // �鿴�Ƿ�����Ʒ��Ҫ���,����ִ��

			// spec.��ֹ��һ�ʽ���û�����
			if (pump.addInf[i][0] != 0) {
			       if (clr_offline_trans() < 0) {
				       return;
			       }
				pump.addInf[i][0] = 0;
			}
			break;
		case FP_CALLING:                                // ̧ǹ
			if ((status & 0x10) != 0) {             // ����н���δ������Բ���������
				// Note! ���ﲻ���� clr_offline_trans
				break;
			}

			if ((__u8)FP_State_Calling > lpDispenser_data->fp_data[i].FP_State) {
				ret = DoCall(i);
				fp_state[i] = 1;
				ret = BeginTr4Fp(node, i + 0x21);
				if (ret < 0) {
					RunLog("DoPolling.PUMP_BUSY: BeingTr4FP ʧ��");
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
					RunLog("Node %d FP %d �� %d ��ņ̀��", node, i + 1, pump.gunLog[i]);
				}
				ret = ChangeFP_State(node, i + 0x21, FP_State_Started, NULL);
				ret = BeginTr4Fp(node, i + 0x21);
				if (ret < 0) {
					RunLog("DoPolling.PUMP_BUSY: BeingTr4FP ʧ��");
				}
				fp_state[i] = 1;
			}
			break;
		case FP_FUELLING:                               // ����
			if ((__u8)FP_State_Fuelling != lpDispenser_data->fp_data[i].FP_State) {
				if (lpDispenser_data->fp_data[i].Log_Noz_State == 0) {
					__u8 ln_status = 0; 
					ln_status = 1 << (pump.gunLog[i] - 1);
					lpDispenser_data->fp_data[i].Log_Noz_State = ln_status;
					RunLog("Node %d FP %d �� %d ��ņ̀��", node, i + 1, pump.gunLog[i]);
				}
				if ((__u8)FP_State_Started != lpDispenser_data->fp_data[i].FP_State) {
					ret = ChangeFP_State(node, i + 0x21, FP_State_Started, NULL);
					if (ret < 0) {
						break;
					}
					ret = BeginTr4Fp(node, i + 0x21);
					if (ret < 0) {
						RunLog("DoPolling.PUMP_BUSY: BeingTr4FP ʧ��");
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
			break;     // CPPEI ��Ҫ���״̬
			if ((__u8)FP_State_Suspended_Started != lpDispenser_data->fp_data[i].FP_State) {
				ret = ChangeFP_State(node, i + 0x21, FP_State_Suspended_Started, NULL);
			}
			break;
		case FP_SPFUELLING:                            // Limit-Reached
			break;     // CPPEI ��Ҫ���״̬
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
	int trycnt;
	int ration_type;
	__u8 pre_volume[3] = {0};
	static __u8 chain_no[2] = {0, 0};  // ����

	// ����Ϊ�ɹ�����Ľ���, �������ٴμ���, ���������
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

	BBcdInc(chain_no, 2);  // ������1, 0001 - 9999
	for (trycnt = 0; trycnt < TRY_TIME; trycnt++) {		
		// Step.1 ����Ԥ������
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
			RunLog("дԤ������Node[%d].FP[%d]ʧ��", node, pos + 1);
			continue;
		}
		
		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("дԤ������Node[%d].FP[%d]�ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		} 

		if (pump.cmd[0] != 0xFC || pump.cmd[1] != ration_type ||
					pump.cmd[2] != pump.panelPhy[pos]) {
			RunLog("������������");
			continue;
		}
		
		if (pump.cmd[4] != 0xBB) {
			RunLog("ִ��Ԥ�ü�������ʧ��");
			continue;
		}

		if (pump.cmd[pump.wantLen - 1] != Check_xor(&pump.cmd[1], pump.wantLen - 2)) {
			RunLog("Return Message XOR ERROR");
			if ((pump.cmd[3] & 0x60 != 0x00) || pump.cmd[3] & 0x60 != 0x01) {
				continue;
			}
		}

		RunLog("дԤ������Node[%d].FP[%d]�ɹ�", node, pos + 1);
		// [Step.2 ��Ȩ����]
		/*
		 * ʵ�ֽ���&�޸�˵��: �е��ͻ��·���Ԥ����֮������������Ȩ�ȿ��Լ���, ����Step.2
		 */
		ret = DoAuth(pos);
		if (ret < 0) {
			RunLog("Ԥ�ü�����Ȩʧ��!");
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
	__u8 pr_no, lnz;
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
						// Step.1 ���������
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
							RunLog("��������Node[%d].FP[%d]ʧ��", node, i + 1);
							continue;
						}

						RunLog("Node[%d].FP[%d].PriceChange_CMD: %s",
							node, i + 1, Asc2Hex(pump.cmd, pump.cmdlen));

						// Step.2 �����������Ϣ�����ض��ͻ�����ȷ�ϱ���Ƿ�ɹ�
						bzero(pump.cmd, pump.wantLen);
						pump.cmdlen = pump.wantLen;
						ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
						if (ret < 0) {
							RunLog("��������Node[%d].FP[%d]�ɹ�, ȡ������Ϣʧ��", node, i + 1);
							continue;
						}


						if (pump.cmd[pump.wantLen - 1] != Check_xor(&pump.cmd[1], pump.wantLen - 2)) {
							RunLog("Node[%d].FP[%d]Return Message XOR ERROR", node, i + 1);
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
							continue;
						}

						if (pump.cmd[0] != 0xFC || pump.cmd[1] != 0x03 ||
								pump.cmd[2] != pump.panelPhy[i]) {
							RunLog("Node[%d].FP[%d]������������", node, i + 1);
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
							continue;
						}
						
						if (pump.cmd[4] != 0xBB) {
							RunLog("Node[%d].FP[%d]ִ�б��ʧ��", node, i + 1);
							err_code_tmp = 0x37;
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
							continue;
						}

						//  ��۳ɹ���ҲҪ�����۽�����֤
						ret = GetPrice(i);
						if (ret < 0) {
							continue;
						}

						if (memcmp(&pump.newPrice[pr_no][2], &pump.cmd[4], 2) != 0) {
							err_code_tmp = 0x37;
							// ����˰��æ���±��ʧ��, �����ӳ�2s������ 
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
							continue;
						}

						
						// Step.3 д�µ��۵���Ʒ���ݿ� & ���㵥�ۻ���
						fm_idx = lpDispenser_data->fp_data[i].Default_Fuelling_Mode - 1;
					RunLog("Node[%d].FP[%d]���ͱ������ɹ�,�µ���fp_fm_data[%d][%d].Prode_Price: %s",
							node, i + 1, pr_no, fm_idx,
							Asc2Hex(lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price, 4));

					CriticalLog("�ڵ�[%02d]��[%d]ǹ[%d]��۳ɹ�,�µ���Ϊ%02x.%02x",
						node, i + 1, j + 1,
						lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2],
						lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3]);
						err_code_tmp = 0x99;
						break;
						
					//	Ssleep(INTERVAL*10);
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
					if(0x99 == err_code){
						// �������κ���ǹ���ʧ��, ��������FP���ʧ��.
						err_code = err_code_tmp;
					}
						
					} // end of if.PR_Id match
				//	break;  // ������ͻ��ܹ�����ָ����Ϣ������ǹͬ��Ʒ���������������Ҫbreak.
				} // end of for.j < LN_CNT
				RunLog("---------- Node[%d]FP[%d] ER_DATA, error code: %02x -----", node, i + 1, err_code);

				int tmp_idx;
				for (tmp_idx = 0; tmp_idx < pump.newPrice[pr_no][0]; tmp_idx++) {
					ChangeER_DATA(node, 0x21 + i, err_code, NULL); // Translater�Ѿ��ظ���ACK
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
	__u8 total[7];
	__u8 pre_total[7];
	__u8 amt_total[7];
	__u8 buffer[3];

	RunLog("�ڵ�[%d]���[%d]�����½���", node, pos + 1);

	/*
	 * ���Ƕ���������ص���������ȻҲ�б���, �����Ǹ����뵽999999.99��ʱ������, ����ʵ�ʵı��벻��, 
	 * ���ʱ��ͻ�������������õı���ֵ��һ��������, ���x����
	 */
	err_cnt = 0;
	do {
		ret = GetPumpTotal(pos);	// ȡ���ν��׼�¼
		if (ret != 0) {
			err_cnt++;
			RunLog("Node %d FP %d ��ȡ����ʧ�� <ERR>", node, pos + 1);
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

	// ������(һ��Ҫ�ڱȽϱ���֮ǰ��, ��Ϊ������㽻��Ҳ��Ҫ���)
	err_cnt = 0;
	do {
		ret = GetTransData(pos);	// ȡ���ν��׼�¼
		if (ret != 0) {
			err_cnt++;
			RunLog("Node %d FP %d ��ȡ��������ʧ�� <ERR>", node, pos + 1);
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
		RunLog("�ڵ�[%02d]���[%d]ǹ[%d] ## Amount: %02x%02x.%02x Volume: %02x%02x.%02x Price: %02x.%02x Total: %02x%02x%02x%02x.%02x", node, pos + 1, pump.gunLog[pos], amount[2], 
					amount[3], amount[4], volume[2], volume[3], volume[4], price[2],
							price[3], total[2], total[3], total[4], total[5], total[6]);
#endif

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
				RunLog("DoEot.EndTr4Fp ʧ��, ret: %d <ERR>", ret);
			}
		} while (ret < 0 && err_cnt <= 8);

	} else {
		ret = EndZeroTr4Fp(node, pos + 0x21);
		if (ret < 0) {
			RunLog("DoEot.EndZeroTr4Fp ʧ��, ret: %d <ERR>", ret);
		}
	}
	
	return ret;
}


/*
 * func: ��ȡ��FP����ǹ�ı���
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
			RunLog("�� ȡ�������� ��Node[%d].FP[%d]ʧ��", node, pos + 1);
			continue;
		}

		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("�� ȡ�������� ��Node[%d].FP[%d]�ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		}

		if (pump.cmd[pump.wantLen - 1] != Check_xor(&pump.cmd[1], pump.wantLen - 2)) {
			RunLog("Return Message XOR ERROR");
			continue;
		}

		if (pump.cmd[0] != 0xFC || pump.cmd[1] != 0x0C ||
			       	pump.cmd[2] != pump.panelPhy[pos]) {
			RunLog("������������");
			continue;
		}

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
	int ret;
	int trycnt;
	int ack_cmdlen;
	int ack_wantLen;
	__u8 status;
	__u8 ack_cmd[6];
	__u8 data_backup[40] = {0};
	__u8 got_flag = 0;       // ����Ƿ�ɹ�ȡ�ù���������, ���� -- ��; 0 - ��

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
			RunLog("���������������Node[%d].FP[%d]ʧ��", node, pos + 1);
			continue;
		}

		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("���������������Node[%d].FP[%d]�ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		}

		if (pump.cmd[pump.wantLen - 1] != Check_xor(&pump.cmd[1], pump.wantLen - 2)) {
			RunLog("Return Message XOR ERROR");
			continue;
		}

		if (pump.cmd[0] != 0xFC || pump.cmd[1] != 0x15 ||
			       	pump.cmd[2] != pump.panelPhy[pos]) {
			RunLog("������������");
			continue;
		}


		if (pump.cmd[1] == 0x16) {     // Exec Successful, but have no transaction
#if 1
			if (0 == got_flag) {
				return 1;      // �޽���ֱ�ӷ���
			} else {
				/*
				 * Step.2 û����ȷ���--ʵ���Ͻ����Ѿ������
				 */

				goto EO_GETRANSDATA;
			}
#endif
		}
		
		// ���ݱ��ζ���������
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
			RunLog("�����������ݻظ���Node[%d].FP[%d]ʧ��", node, pos + 1);
			continue;
		}

		bzero(ack_cmd, ack_wantLen);
		ack_cmdlen = ack_wantLen;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, ack_cmd, &ack_cmdlen);
		if (ret < 0) {
			RunLog("�����������ݻظ���Node[%d].FP[%d]�ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
			// ��ֹ�ͻ�ɾ���˽���, ����FCCû���յ������, ���꽻�׺��������
//			pump.addInf[pos][0] = 1;  // jie @ 2008.8.6
		}

		if (ack_cmd[ack_wantLen - 1] != Check_xor(&ack_cmd[1], ack_wantLen - 2)) {
			RunLog("Return Message XOR ERROR");
			continue;
//			pump.addInf[pos][0] = 1;
		}

		if (ack_cmd[0] != 0xFC || ack_cmd[1] != 0x17 ||
			       	ack_cmd[2] != pump.panelPhy[pos]) {
			RunLog("������������");
			continue;
//			pump.addInf[pos][0] = 1;
		}

		if ((ack_cmd[3] & 0x10) != 0) {
			RunLog("����δ�ɹ����");
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
 * func: ��ȡ��ʷ���׼�¼
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
			RunLog("���������������Node[%d].FP[%d]ʧ��", node, pos + 1);
			continue;
		}

		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("���������������Node[%d].FP[%d]�ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		}

		if (pump.cmd[pump.wantLen - 1] != Check_xor(&pump.cmd[1], pump.wantLen - 2)) {
			RunLog("Return Message XOR ERROR");
			continue;
		}

		if (pump.cmd[0] != 0xFC || pump.cmd[1] != 0x13 ||
			       	pump.cmd[2] != pump.panelPhy[pos]) {
			RunLog("������������");
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
			RunLog("�����������ݻظ���Node[%d].FP[%d]ʧ��", node, pos + 1);
			continue;
		}

		bzero(ack_cmd, ack_wantLen);
		ack_cmdlen = ack_wantLen;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, ack_cmd, &ack_cmdlen);
		if (ret < 0) {
			RunLog("�����������ݻظ���Node[%d].FP[%d]�ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		}

		if (ack_cmd[ack_wantLen - 1] != Check_xor(&ack_cmd[1], ack_wantLen - 1)) {
			RunLog("Return Message XOR ERROR");
//			continue;   // ��ʧ������ô��
		}

		if (ack_cmd[0] != 0xFC || ack_cmd[1] != 0x17 ||
			       	ack_cmd[2] != pump.panelPhy[pos]) {
			RunLog("������������");
			continue;
		}

		return 0;
	}

	return -1;
}


/*
 * func: ��ͣ���ͣ��ڴ���Terminate_FP��
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
			RunLog("�� ��ֹ�������� ��Node[%d].FP[%d]ʧ��", node, pos + 1);
			continue;
		}

		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("�� ��ֹ�������� ��Node[%d].FP[%d]�ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		}

		if (pump.cmd[pump.wantLen - 1] != Check_xor(&pump.cmd[1], pump.wantLen - 2)) {
			RunLog("Return Message XOR ERROR");
			continue;
		}

		if (pump.cmd[0] != 0xFC || pump.cmd[1] != 0x05 ||
			       	pump.cmd[2] != pump.panelPhy[pos]) {
			RunLog("������������");
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

		if (pump.cmd[pump.wantLen - 1] != Check_xor(&pump.cmd[1], pump.wantLen - 2)) {
			RunLog("Return Message XOR ERROR");
			continue;
		}

		if (pump.cmd[0] != 0xFC || pump.cmd[1] != 0x06 ||
			       	pump.cmd[2] != pump.panelPhy[pos]) {
			RunLog("������������");
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
 * func: ��ȡ���ϴ�ʵʱ��������
 */
static int DoBusy(int pos)
{
	int ret;
	int trycnt;
	__u8 amount[5] = {0};
	__u8 volume[5] = {0};

	for (trycnt = 0; trycnt < 2; ++trycnt) {
		// Step.1 ��ȡʵʱ����
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
			RunLog("�� ȡʵʱ�������� ���Node[%d].FP[%d]ʧ��", node, pos + 1);
			continue;
		}

		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("�� ȡʵʱ�������� ���Node[%d].FP[%d]�ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		}

		if (pump.cmd[pump.wantLen - 1] != Check_xor(&pump.cmd[1], pump.wantLen - 2)) {
			RunLog("Return Message XOR ERROR");
			continue;
		}

		if (pump.cmd[0] != 0xFC || pump.cmd[1] != 0x18 ||
			       	pump.cmd[2] != pump.panelPhy[pos]) {
			RunLog("������������");
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
 * func: ��ȡ�ͻ����� �ܶ� ʱ��
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
			RunLog("����ȡ�ͻ��������Node[%d].FP[%d]ʧ��", node, pos + 1);
			continue;
		}

		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("����ȡ�ͻ��������Node[%d].FP[%d]�ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		} 

		if (pump.cmd[pump.wantLen - 1] != Check_xor(&pump.cmd[1], pump.wantLen - 2)) {
			RunLog("Return Message XOR ERROR");
			continue;
		}

		if (pump.cmd[0] != 0xFC || pump.cmd[1] != 0x12 ||
			       	pump.cmd[2] != pump.panelPhy[pos]) {
			RunLog("������������");
			continue;
		}

		return 0;
	}

	return -1;
}

/*
 * func: ��ȡʵʱ״̬
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
			RunLog("��ȡ״̬���Node[%d].FP[%d]ʧ��", node, pos + 1);
			continue;
		}

		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("��ȡ״̬���Node[%d].FP[%d]�ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		} 

		if (pump.cmd[pump.wantLen - 1] != Check_xor(&pump.cmd[1], pump.wantLen - 2)) {
			RunLog("Return Message XOR ERROR");
			continue;
		}

		if (pump.cmd[0] != 0xFC || pump.cmd[1] != 0x0D ||
			       	pump.cmd[2] != pump.panelPhy[pos]) {
			RunLog("������������");
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
	int ret;
	int trycnt;

	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {         // Note. GetPumpStatus ѭ����������ñ��ص�,��Ҫ��trycnt
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
			RunLog("��ȡ״̬���Node[%d].FP[%d]ʧ��", node, pos + 1);
			continue;
		}

		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("��ȡ״̬���Node[%d].FP[%d]�ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		} 

		if (pump.cmd[pump.wantLen - 1] != Check_xor(&pump.cmd[1], pump.wantLen - 2)) {
			RunLog("Return Message XOR ERROR");
			continue;
		}

		if (pump.cmd[0] != 0xFC || pump.cmd[1] != 0x0D ||
			       	pump.cmd[2] != pump.panelPhy[pos]) {
			RunLog("���͵�Ŵ���");
			continue;
		}

		*status = pump.cmd[3];

		return 0;
	}

	return -1;
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
		Hex2Bcd(&pump.cmd[8], GUN_EXTEND_LEN, &total[2], GUN_EXTEND_LEN + 1);
		pr_no = (lpDispenser_data->fp_ln_data[i][lnz]).PR_Id - 1;  // 0-7	
		if ((pr_no < 0) || (pr_no > 7)) {
			RunLog("InitPumpTotal,FP %d, LNZ %d, PR_Id %d", i, lnz + 1, pr_no + 1);
		}

		memcpy(lpDispenser_data->fp_m_data[pr_no].Meter_Total, total, 7);
		memcpy((lpDispenser_data->fp_ln_data[i][pump.gunLog[i] - 1]).Log_Noz_Vol_Total, total, 7);
#if DEBUG
		RunLog("�߼�ǹ��[%d]�ı���Ϊ[%s]", 
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
 * func: ���ÿ���ģʽ
 *
 */

static int SetCtrlMode(int pos, __u8 mode)
{
	int trycnt;
	int ret;

	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {         // Note. GetPumpStatus ѭ����������ñ��ص�,��Ҫ��trycnt
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
			RunLog("�����ÿ���ģʽ���Node[%d].FP[%d]ʧ��", node, pos + 1);
			continue;
		}

		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("�����ÿ���ģʽ���Node[%d].FP[%d]�ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		} 

		if (pump.cmd[2] != pump.panelPhy[pos]) {
			RunLog("���͵�Ŵ���");
			continue;
		}

		if (pump.cmd[pump.wantLen - 1] != Check_xor(&pump.cmd[1], pump.wantLen - 2)) {
			RunLog("Return Message XOR ERROR");
			continue;
		}

		RunLog("Node[%d]FP[%d]���ÿ���ģʽ�ɹ�", node, pos);
		return 0;
	}

	RunLog("Node[%d]FP[%d]���ÿ���ģʽʧ��", node, pos);
	return -1;
}



/*
 * func: ����ͻ��ѻ�����
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
 * func: �趨�������ԡ�ͨѶ���ʡ�У��λ��
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
		RunLog("�ڵ� %d ���ô�������ʧ��", node);
		return -1;
	}

	gpGunShm->chnLogSet[pump.chnLog - 1] = 1;
	Ssleep(INTERVAL);      // ʹ��֮ǰ���������Ѿ����ú�

	return 0;
}


/*
 * func:  ͨѶ��ͨʱ��ѯ�����и�һ��ʱ������һ�´�������
 *        ����������ĵ�ѹ���ȵ��µ�Ƭ����λ--�����������ûָ�Ԥ��9600,8,n,1
 */
static int ReDoSetPort005()
{
#define RE_DO_FREQUENCY   10         //  10����ѯʧ������һ�´�������

	int static polling_fail_cnt = 0;
	int ret;

	if ((++polling_fail_cnt) >= RE_DO_FREQUENCY) {
		ret = SetPort005();
		if (ret < 0) {
			RunLog("�ڵ� %d ���贮������ʧ��", node);
		}
		polling_fail_cnt = 0;
	}

	return ret;
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
		if (GetPumpStatus(0, &status) < 0) {
			fail_times++;
		}
	} while (time(NULL) < return_time);

	return fail_times;
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
			node,pos+1,lnz+1,pump.cmd[4], pump.cmd[5],
		       	lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2], 
		       	lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3]);

		if ((pump.cmd[4] != lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2]) ||
			(pump.cmd[5] != lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3])) {
			return -2;
		}
	}
	
	ChangeFP_State(node, 0x21 + pos, FP_State_Closed, NULL);
	RunLog("Node[%d]FP[%d]�����ѱ����ȷ, �л�״̬ΪClosed", node, pos + 1);

	return 0;
}

/* ----------------- End of lischn005.c ------------------- */
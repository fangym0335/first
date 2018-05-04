/*
 * Filename: lischn018.c
 *
 * Description: ���ͻ�Э��ת��ģ�� -- �ϳ��տ�������, 3 Line CL, 4800,8,e,1
 *
 * �ο��ĵ�<<���տ����������ͻ�ͨѶЭ��(2.5)>>
 *
 * 2009.04 by Zhang Hua <zhanghua@css-intelligent.com>
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

#define PUMP_OFF	0x00
#define PUMP_CALL	0x40 
#define PUMP_BUSY	0x60

#define TRY_TIME	5

#define INTERVAL	150000

static int DoCmd();
static void DoPolling();
static int DoAuth(int pos);
static int GetKeystokePreset (int pos);
static int DoStarted(int i);
static int DoBusy(int pos);
static int DoCall(int pos);
static int DoEot(int pos);
static int DoOpen(int pos, __u8 *msgbuffer);
static int DoClose(int pos, __u8 *msgbuffer);
static int DoRation(int pos);
static int DoKeystokeRation(int pos);
static int DoStop(int pos);
static int DoMode();
static int DoPrice();
static int ChangePrice();
static int GetTransDataL(int pos);
static int GetTransDataP(int pos);
static int GetPumpStatus(int pos, __u8 *status);
static int GetPumpOnline(int pos, __u8 *status);
static int GetPumpTotalL(int pos);
static int GetPumpTotalP(int pos);
static int GetPrice(int pos);
static int InitPumpTotal();
static int InitCurrentPrice();
static int InitGunLog();
static int GetCurrentLNZ(int pos);
static int SetPort018();
static int ReDoSetPort018();
static int SetCtrlMode(int pos);
static int DoEot_Additional(int pos);
static int keep_in_touch_xs(int x_seconds);
static int CheckPriceErr(int pos);

static PUMP pump;
static int node;
static int IFSFpos;
static DISPENSER_DATA *lpDispenser_data;
static NODE_CHANNEL *lpNode_channel;
static __u8 cmdbak[MAX_ORDER_LEN+1];          // Ԥ�������,����̧ǹ��Ԥ��
static int fp_state[MAX_FP_CNT];              // 0 - IDLE, 1 - CALLING , 2 - FUELLING
static int dostop_flag[MAX_FP_CNT] = {0};     // ����Ƿ�ɹ�������ͣ����

int ListenChn018(int chnLog)
{
	long	indtyp;
	int	ret;
	int	fd;
	int	j; 
	int	i;
	__u8    cmd[8];
	__u8    status;
	
	sleep(5);   //��ֹ�˽����˳�,��������û���������. ÿ��lischnxxx����Ҫ
	RunLog("Listen Channel  018 ,logic channel[%d]", chnLog);
	memset(&pump, 0, sizeof(pump));
	pump.chnLog = chnLog;
	pump.panelCnt = 0;


	//1.   ɨ�蹲���ڴ�, ����ͨ�����ӵĴ���, ���		
	for (i = 0; i < MAX_CHANNEL_CNT; i++) {
		if (chnLog == gpIfsf_shm->node_channel[i].chnLog) {	// �߼�������ͬ
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
						pump.panelPhy[j] =(((gpIfsf_shm->gun_config[idx].phyfp) - 1) * 0x08);
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
	
	//2.�򿪴���(�����ڴ����ϵ���չ��)�����ô�������
	
	strcpy(pump.cmd, alltty[pump.port-1]);
	pump.fd = OpenSerial(pump.cmd);
	if (pump.fd < 0) {
		RunLog("�򿪴���[%s]ʧ��", pump.cmd);
		return -1;
	}

	//RunLog("�򿪴���[%s].fd=[%d]", pump.cmd, pump.fd);

	ret = SetPort018();
	if (ret < 0) {
		return -1;
	}
	

	//3.  ���Լ��ͻ��Ƿ�����,�����������ȴ�
	while (1) {		
		ret = GetPumpOnline(0, &status);
		if (ret == 0) {
			//���ü��ͻ�������ģʽ	
			for (j = 0; j < pump.panelCnt; ++j) {		
				if (SetCtrlMode(j) != 0) {						
					continue;
				}
			}
	  		
			break;
		}

		gpIfsf_shm->node_online[IFSFpos] = 0; //������,����������.
		UserLog("�ڵ� %d ������", node);
		
		sleep(1);
	}

	//4.��ʼ��Ĭ�ϵ�ǰ�߼�ǹ��
	ret = InitGunLog();
	if (ret < 0) {
		RunLog("��ʼ��Ĭ�ϵ�ǰ�߼�ǹ��ʧ��");
		return -1;
	}

	 //5.��ʼ����Ʒ���۵����ݿ�
	ret = InitCurrentPrice();
	if (ret < 0) {
		RunLog("��ʼ����Ʒ���۵����ݿ�ʧ��");
		return -1;
	}

	//6.��ʼ�����뵽���ݿ�
	ret = InitPumpTotal();
	if (ret < 0) {
		RunLog("��ʼ�����뵽���ݿ�ʧ��");
		return -1;
	}


       //+++++++++++++++ 7. һ�о���, ����+++++++++++++++++++++++//
       
//	gpIfsf_shm->node_online[IFSFpos] = node;  
	ChangeNode_Status(node, NODE_ONLINE);
	UserLog("�ڵ� %d ����,״̬[%02x]!", node, status);
	keep_in_touch_xs(12);               // ȷ��CD�Ѿ��յ�����֮��������������
	

	//8. �ͻ�ģ���ʼ�����,ת��FP״̬Ϊ Close 
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
			RunLog("ϵͳ��ͣ");
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
			if (pump.cmd[0] != 0x18 && pump.cmd[0] != 0x19) {  // DoPrice & DoMode����Ҫ��������
				RunLog("�ڵ�[%d] �Ƿ�������ַ",node);
				return -1;
			}
		}				

		bzero(msgbuffer, sizeof(msgbuffer));
		memcpy(msgbuffer, &pump.cmd[3], pump.cmd[4] + 2);  // ���β����� Acknowledge Message
	}

	ret = -1;
	
	
	switch (pump.cmd[0]) {
		case 0x18:	// �����ͼ�
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

			ret = DoStop(pos);
			if (ret == 0) {
				ret = ChangeFP_State(node, pos + 0x21, FP_State_Idle, msgbuffer);
				if ((__u8)FP_State_Calling != lpDispenser_data->fp_data[pos].FP_State) {
					dostop_flag[pos] = 1;
				}
				/*
				 * ��Calling��״̬��ִ�д˲���, ����Э�鷵��ACK,���Ҹı�״̬ΪIdle
				 * ֮����Polling��ʱ������ͻ�״̬ת��FP_StateΪCalling,
				 * ������̧ǹ״̬�·�Ԥ������״̬ת������
				 * Guojie Zhu @ 2009.03.13
				 */
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
				lpDispenser_data->fp_data[pos].Log_Noz_Mask = 0xFF;
				bzero(&lpDispenser_data->fp_data[pos].Remote_Volume_Preset, 5);
				bzero(&lpDispenser_data->fp_data[pos].Remote_Amount_Prepay, 5);
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
				break;
			}
			cmdbak[0]= pump.cmd[0];
			if (pump.cmd[0] == 0x3D) {     //����
				cmdbak[1] = lpDispenser_data->fp_data[pos].Remote_Volume_Preset[2];	
				cmdbak[2] = lpDispenser_data->fp_data[pos].Remote_Volume_Preset[3];
				cmdbak[3] = lpDispenser_data->fp_data[pos].Remote_Volume_Preset[4];
			} else if (pump.cmd[0] == 0x4D) {//����
				cmdbak[1] = lpDispenser_data->fp_data[pos].Remote_Amount_Prepay[2];	
				cmdbak[2] = lpDispenser_data->fp_data[pos].Remote_Amount_Prepay[3];
				cmdbak[3] = lpDispenser_data->fp_data[pos].Remote_Amount_Prepay[4];	  	
			}

			pump.addInf[pos][15] = 1; //Ԥ�ñ�־,���ں���Ȩ		

			if ((__u8)FP_State_Calling == lpDispenser_data->fp_data[pos].FP_State) {
				pump.addInf[pos][15] = 0 ;//Ԥ�ñ�־,���ں���Ȩ	
				ret = SendData2CD(&msgbuffer[2], msgbuffer[1], 5);
				if (ret < 0) {
					RunLog("Send Ack Msg[%s] to CD failed",
						Asc2Hex(&msgbuffer[2], msgbuffer[1]));
				}
			} else {
				ret = ChangeFP_State(node, pos + 0x21, FP_State_Authorised, msgbuffer);
			}

			bzero(&lpDispenser_data->fp_data[pos].Remote_Amount_Prepay, 5);
			bzero(&lpDispenser_data->fp_data[pos].Remote_Volume_Preset, 5);

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
 * func: Open_FP
 */
static int DoOpen(int pos, __u8 *msgbuffer) {
	int ret;
	unsigned int i;
      
	ret = SetHbStatus(node, 0);
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
	int kind;
	int ret;
	int trynum;
	
	RunLog("Node[%d].FP[%d] Do Ration", node, pos + 1);
	
	if (cmdbak[0] == 0x3D) {
		kind = 0x00;
	} else if (cmdbak[0] == 0x4D) { 
		kind = 0x01;
	} else {
		return -1;
	}
	

	for(trynum = 0; trynum < TRY_TIME; trynum++) {		
		Ssleep(INTERVAL);
		pump.cmd[0] = 0xfa;
		pump.cmd[1] = 0x07;
		pump.cmd[2] = pump.panelPhy[pos];
		pump.cmd[3] = 0x32;	
		pump.cmd[4] = kind;
		pump.cmd[5] = cmdbak[1];
		pump.cmd[6] = cmdbak[2];
		pump.cmd[7] = cmdbak[3];
		pump.cmd[8] = ((0x77 + pump.cmd[1]  + pump.cmd[2] + pump.cmd[3] + \
				pump.cmd[4] + pump.cmd[5] + pump.cmd[6] + pump.cmd[7]) & 0xff);
		if (pump.cmd[8] == 0xfa) {
			pump.cmd[9] = 0xfa;
			pump.cmdlen = 10;
		} else {
			pump.cmdlen = 9;
		}
		pump.wantLen = 4;

		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy,
			       	pump.cmd, pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]�·�Ԥ������ʧ��", node, pos + 1);
			continue;
		}

		RunLog("Node[%d].FP[%d]�·�Ԥ������[%s]�ɹ�", 
			node, pos + 1, Asc2Hex(pump.cmd, pump.cmdlen));

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]�·�Ԥ������ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		} 

		if (0xFA != pump.cmd[0]) {
			RunLog("Node[%d].FP[%d]�������ݴ���", node, pos + 1);
			continue;
		}

		if (pump.cmd[3]!=((0x77 + pump.cmd[1] + pump.cmd[2])&0xFF)) {
			RunLog("Node[%d].FP[%d]��������У�����", node, pos + 1);
			continue;
		}
		
		return 0;
	}

	return -1;
}


static int DoKeystokeRation(int pos)  
{
	int kind;
	int ret;
	int trynum;

	ret = GetKeystokePreset(pos);
	if (ret < 0) {
		RunLog("Node[%d].FP[%d]��ȡ����Ԥ����ʧ��", node, pos + 1);
		return -1;
	}

	if ((pump.cmd[4] | pump.cmd[5] | pump.cmd[6]) == 0x00) {
		return 1;         // �޼���Ԥ��
	} else {
				  // �м���Ԥ����
		kind = pump.cmd[3];
		memcpy(&cmdbak[1], &pump.cmd[4], 3);
	}

	for(trynum = 0; trynum < TRY_TIME; trynum++) {		
		Ssleep(INTERVAL);
		pump.cmd[0] = 0xfa;
		pump.cmd[1] = 0x07;
		pump.cmd[2] = pump.panelPhy[pos];
		pump.cmd[3] = 0x32;	
		pump.cmd[4] = kind;
		pump.cmd[5] = cmdbak[1];
		pump.cmd[6] = cmdbak[2];
		pump.cmd[7] = cmdbak[3];
		pump.cmd[8] = ((0x77 + pump.cmd[1]  + pump.cmd[2] + pump.cmd[3] + \
				pump.cmd[4] + pump.cmd[5] + pump.cmd[6] + pump.cmd[7]) & 0xff);
		if (pump.cmd[8] == 0xfa) {
			pump.cmd[9] = 0xfa;
			pump.cmdlen = 10;
		} else {
			pump.cmdlen = 9;
		}
		pump.wantLen = 4;

		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy,
			       	pump.cmd, pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]�·�Ԥ������ʧ��", node, pos + 1);
			continue;
		}

		RunLog("Node[%d].FP[%d]�·�Ԥ������[%s]�ɹ�", 
			node, pos + 1, Asc2Hex(pump.cmd, pump.cmdlen));

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]�·�Ԥ������ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		} 

		if (0xFA != pump.cmd[0]) {
			RunLog("Node[%d].FP[%d]�������ݴ���", node, pos + 1);
			continue;
		}

		if (pump.cmd[3]!=((0x77 + pump.cmd[1] + pump.cmd[2])&0xFF)) {
			RunLog("Node[%d].FP[%d]��������У�����", node, pos + 1);
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
	int ret;
	int trynum;
	int idx, fm_idx;
	__u8 pr_no;
	__u8 err_code;
	__u8 err_code_tmp;
	__u8 newPrice1[2] = {0};
	__u8 tmpPrice[2] = {0};

	for (pr_no = 0; pr_no < FP_PR_NB_CNT; pr_no++) {    
              //һ̨���ͻ�ֻ��һ����ƷҲֻ��һ���ͼ�
		if (pump.newPrice[pr_no][2] != 0x00 || pump.newPrice[pr_no][3] != 0x00) {	
			for (i = 0; i < pump.panelCnt; i++) {
				err_code = 0x99;
				for (j = 0; j < MAX_FP_LN_CNT; j++) {
#if DEBUG
					RunLog(">>> fp_ln_data[%d][%d].PR_Id: %d", i, j,
							lpDispenser_data->fp_ln_data[i][j].PR_Id);
#endif
					
					if (lpDispenser_data->fp_ln_data[i][j].PR_Id - 1 == pr_no) {
					err_code_tmp = 0x99;
					Bcd2Hex(&pump.newPrice[pr_no][2], 2, newPrice1, 2);
				
				CriticalLog("�·����������ڵ�[%02d]��[%d]ǹ[%d]......", node, i + 1, j + 1);
				
					for (trynum = 0; trynum < TRY_TIME; trynum++) {
						pump.cmd[0] = 0xfa;
						pump.cmd[1] = 0x05;
						pump.cmd[2] = pump.panelPhy[i];
						
						pump.cmd[3] = 0x20;
						pump.cmd[4] = newPrice1[0];
						
						idx = 4;
						if (newPrice1[0] == 0xfa) {
							pump.cmd[++idx] = 0xfa;
						}
						pump.cmd[++idx] = newPrice1[1];
						if (newPrice1[1] == 0xfa) {
							pump.cmd[++idx] = 0xfa;
						}
						 
						pump.cmd[++idx] = ((0x77 + pump.cmd[1] + pump.cmd[2]+ \
								pump.cmd[3] + newPrice1[0] + newPrice1[1]) & 0xff);
						if (pump.cmd[idx] == 0xfa) {
							pump.cmd[++idx]=0xfa;
						}

						pump.cmdlen = idx+1;
						pump.wantLen = 4;
						
						Ssleep(INTERVAL);
						ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
								pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
						if (ret < 0) {
							RunLog("Node[%d].FP[%d]�·��������ʧ��", node, i + 1);
							continue;
						} else {  // ��۳ɹ����µ���д�����ݿ� & ���㻺��

						RunLog("Node[%d].FP[%d].PriceChange_CMD: %s",
							node, i + 1, Asc2Hex(pump.cmd, pump.cmdlen));
						

						memset(pump.cmd, 0, sizeof(pump.cmd));
						pump.cmdlen = sizeof(pump.cmd)-1;

						ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
						if (ret < 0) {
							RunLog("Node[%d].FP[%d]�·��������ɹ�,ȡ������Ϣʧ��",
												node, i + 1);
							continue;
						}

						if (pump.cmd[3]!=((0x77 + pump.cmd[1] + pump.cmd[2]) & 0xFF)) {
							RunLog("Node[%d].FP[%d]��������У�����", node, i + 1);
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
							continue;
						}

						if (0xFA != pump.cmd[0]) {
							RunLog("Node[%d].FP[%d]�������ݴ���", node, i + 1);
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
							continue;
						}

						if (pump.cmd[2] == 0x01) {
							if (pump.cmd[3] == 0x03) {
								RunLog("����ͥ������û�м���");
							} else if (pump.cmd[3] == 0x02) {
								RunLog("FCC ����У���");
							} else if (pump.cmd[3] == 0x08) {
								RunLog("��ͥ������Ų���");
							}
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
							continue;
						} else if (pump.cmd[2] != 0x20) {
							RunLog("Node[%d].FP[%d]�������ݴ���", node, i + 1);
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
							continue;
						}

						// �������жϵ���
						ret = GetPrice(i);
						if (ret < 0) {
							continue;
						}

						if (memcmp(newPrice1, pump.cmd + 3, 2) != 0) {
							RunLog("Node[%d]FP[%d] ִ�б������ʧ�� ", node, i + 1);
							err_code_tmp = 0x37; // ���ִ��ʧ��
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
					}

//					trynum = TRY_TIME; for test Error
					if (trynum >= TRY_TIME) {
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
					if (0x99 == err_code) {
						// �������κ���ǹ���ʧ��, ��������FP���ʧ��.
						err_code = err_code_tmp;
					}
					} // end of if.PR_Id match
				//	break;  // ������ͻ��ܹ�����ָ����Ϣ�����ǹͬ��Ʒ���������������Ҫbreak.
				}
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
 * func: ��FP����״̬��ѯ, ����״̬����Ӧ����
 */
static void DoPolling()
{
	int i,j;
	int ret;
	int tmp_lnz; // realy temp 3.27
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
					 * 2008.12.29���Ӵ˲���, ������ѹ�������
					 * ��Ƭ����λ�󴮿ڸ�λ���յ���ͨѶ��ͨ������
					 */
					/*
					 * �����贮�����Իᵼ��ReadTTY���յ�"�Ƿ�����", ����ʱ�Ƴ�
					 * 2009.02.19
					if (fail_cnt >= (MAX_FAIL_CNT / 2)) {
						if (SetPort018() >= 0) {
							RunLog("�ڵ�%d���贮�����Գɹ�", node);
						}
					}
					*/
					continue;
				}


				// Step.1
				memset(dostop_flag, 0, sizeof(dostop_flag));
				memset(fp_state, 0, sizeof(fp_state));
				pump.addInf[i][15] = 0;

				// Step.2
//				gpIfsf_shm->node_online[IFSFpos] = 0;
				ChangeNode_Status(node, NODE_OFFLINE);
				pump.time_offline = (3 * gpIfsf_shm->fcc_com_sv_data.Heartbeat_Interval) + time(NULL);
				RunLog("====== Node %d ����! =============================", node);

				// Step.3
				// �ͻ�����, �����ϴ�״̬�ı���Ϣ, ���Բ�ʹ��ChangeFP_State, ֱ�Ӳ���FP_State
				for (j = 0; j < pump.panelCnt; ++j) {
					lpDispenser_data->fp_data[j].FP_State = FP_State_Inoperative;
					lpDispenser_data->fp_data[j].Log_Noz_State = 0;
				}
			}

			// ���������ڼ����,���������ڼ�Ҳ��ʱ���贮������
			//ReDoSetPort018();
			continue;
		} else {
			if (gpIfsf_shm->node_online[IFSFpos] == 0) {
				if ((--fail_cnt) > 0) { // �ų�����/˫�ߵ��������ݲɼ�Bug��ɵ�Ӱ��
					fail_cnt = 1;
					continue;
				} else if (time(NULL) < pump.time_offline) {
					RunLog("==================== wait for 30s, ==============");
					continue;
				}

				// fail_cnt��λ
				fail_cnt = 0;

				// Step.1
				for (j = 0; j < pump.panelCnt; ++j) {		
					if (SetCtrlMode(j) != 0) {
						fail_cnt = 1;
						return;
					}
			      	}

				// Step.2
				ret = InitPumpTotal();    // ��ʼ�����뵽���ݿ�
				if (ret < 0) {
					RunLog("�ڵ�[%d], ���±��뵽���ݿ�ʧ��", node);
					fail_cnt = 1;
					continue;
				}

				// Step.3
				for (j = 0;  j < pump.panelCnt; ++j) {
					lpDispenser_data->fp_data[j].FP_State = FP_State_Closed;
				}

				// Step.4
//				gpIfsf_shm->node_online[IFSFpos] = node;
				ChangeNode_Status(node, NODE_ONLINE);
				RunLog("====== Node[%d]�ָ�����! =========================", node);
				keep_in_touch_xs(12);     // ȷ��CD�Ѿ��յ�����֮��������������
				// Step.5                 // �����ϴ���ǰFP״̬
				for (j = 0; j < pump.panelCnt; ++j) {
					ChangeFP_State(node, j + 0x21, FP_State_Closed, NULL);
				}
				continue;                 // Note!
			}
				
			// ����1�ξ���������һ������ģʽ, ��ʽ�ͻ�תΪ��������ģʽ
			if (((status & 0x08) == 0) || fail_cnt >= 2) {
				if ((status & 0x60) != PUMP_BUSY) { // �������������ʧ��Ӱ�콻�״�������
					for (j = 0; j < pump.panelCnt; ++j) {
						if (SetCtrlMode(j) != 0) {
							return;
						}
					}
					fail_cnt = 0;
					continue;
				}
			} else {
				fail_cnt = 0;
			}		
		}

		if (status != pre_status[i]) {
			pre_status[i] = status;
			RunLog("�ڵ�[%d]���[%d]��ǰ״̬[%02x], status & 0x60 = [%02x]",
							node, i + 1, status, status & 0x60);
		}

		if (gpIfsf_shm->auth_flag - FCC_SWITCH_AUTH != 0) {  //����к�̨
			if ((__u8)FP_State_Closed == lpDispenser_data->fp_data[i].FP_State) {
				GetPumpStatus(i, &status);   // �������ͻ�����,��ֹĳЩ�ͻ��Զ��л�Ϊ��������ģʽ
				if (i == 0) {
					times += pump.panelCnt;					
				}
				if (times > (SENDSTATE *  pump.panelCnt)) {
					if (i >= pump.panelCnt -1)
						times = 0;
					UserLog("Node[%d].FP[%d]����Closed״̬", node, i + 1);
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

		switch (status & 0x60) {
		case PUMP_OFF:					// ��ǹ
			dostop_flag[i] = 0;
			if (fp_state[i] >= 2) {                 // Note.��û�вɵ���ǹ�ź�(EOT)�Ĳ��䴦��
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

			if (((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State) && 
				((__u8)FP_State_Authorised != lpDispenser_data->fp_data[i].FP_State)) {
				ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
			}

			//  clear in_progress_offline_tr_flag
			EO_OFFLINE_TR(node, i);
			if (gpIfsf_shm->fp_offline_flag[node - 1][i] != 0) {
				upload_offline_tr(node, NULL);
			}

			for (j = 0; j < pump.panelCnt; j++) {		 
				 if ((__u8)FP_State_Idle != lpDispenser_data->fp_data[j].FP_State) {		 
				           break;
				 }
			}

			if (j >= pump.panelCnt) {
				ret = ChangePrice();
			} 
			break;
		case PUMP_CALL:					// ̧ǹ
			if (dostop_flag[i] != 0) {    // �����ͣ�ɹ��򱣳�IDLE״̬
				break;
			}

			if (1 == pump.addInf[i][15]) {          //Ԥ�ñ�־,���ں���Ȩ
				GetCurrentLNZ(i);

				pump.addInf[i][15] = 0;

				ret = DoRation(i);
				if (ret == 0) {
					ChangeFP_State(node, i + 0x21, FP_State_Started, NULL);
				}
				
				fp_state[i] = 1;				
				
				DoStarted(i);
			} else if ((__u8)FP_State_Calling > lpDispenser_data->fp_data[i].FP_State) {
				ret = DoCall(i);
				fp_state[i] = 1;

				DoStarted(i);
			} else if (FP_State_Authorised == lpDispenser_data->fp_data[i].FP_State) {
				fp_state[i] = 1;
				DoStarted(i);						  
			}

			break;
		case PUMP_BUSY:					// ����
		       if (dostop_flag[i] != 0) {    // �����ͣ�ɹ��򱣳�IDLE״̬
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
		default:
			break;
		}
		
	}
}


/*
 * func: ��ȡ��FP����ǹ�ı���
 */
static int GetPumpTotalL(int pos)
{
	int ret;
	int trynum;

	for (trynum = 0; trynum <TRY_TIME ; trynum++) {
		Ssleep(INTERVAL);
		pump.cmd[0] = 0xfa;
		pump.cmd[1] = 0x03;
		pump.cmd[2] = pump.panelPhy[pos];
		pump.cmd[3] = 0x15;
		pump.cmd[4] = 0x77 + pump.cmd[1]  + pump.cmd[2]  + pump.cmd[3];
		pump.cmdlen = 5;
		pump.wantLen = 2;
		
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]�·�����������ʧ��", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x10;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		gpGunShm->addInf[pump.chnLog - 1][0] = 0;

		if (ret < 0) {
			RunLog("Node[%d].FP[%d]�·�����������ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		}

		if (pump.cmd[2] != 0x15 || pump.cmd[0] != 0xFA) {
			RunLog("Node[%d].FP[%d]������Ϣ����", node, pos + 1);
			continue;
		}

		if (pump.cmd[9]!=((0x77 + pump.cmd[1] + pump.cmd[2] + pump.cmd[3] + \
			pump.cmd[4] + pump.cmd[5] + pump.cmd[6] + pump.cmd[7] + pump.cmd[8]) & 0xFF)) {
			RunLog("Node[%d].FP[%d]������ϢУ�����", node, pos + 1);
			continue;
		}

		return 0;
	}

	return -1;
}

static int GetPumpTotalP(int pos)
{
	int ret;
	int trynum;

	for (trynum = 0; trynum <TRY_TIME ; trynum++) {
		Ssleep(INTERVAL);
		pump.cmd[0] = 0xfa;
		pump.cmd[1] = 0x03;
		pump.cmd[2] = pump.panelPhy[pos];
		pump.cmd[3] = 0x16;
		pump.cmd[4] = 0x77 + pump.cmd[1]  + pump.cmd[2]  + pump.cmd[3];
		pump.cmdlen = 5;
		pump.wantLen = 2;
		
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]�·�����������ʧ��", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x10;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		gpGunShm->addInf[pump.chnLog - 1][0] = 0;
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]�·�����������ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		}

		if (pump.cmd[2] != 0x16 || pump.cmd[0] != 0xFA) {
			RunLog("Node[%d].FP[%d]������Ϣ����", node, pos + 1);
			continue;
		}

		if (pump.cmd[9]!=((0x77 + pump.cmd[1] + pump.cmd[2] + pump.cmd[3] + \
			pump.cmd[4] + pump.cmd[5] + pump.cmd[6] + pump.cmd[7] + pump.cmd[8])&0xFF)) {
			RunLog("Node[%d].FP[%d]������ϢУ�����", node, pos + 1);
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
	int ret;
	int pr_no;
	int err_cnt;
	__u8 price[4];
	__u8 volume[5];
	__u8 amount[5];
	__u8 total[7];
	__u8 meter_total[7] = {0};
	__u8 amount_total[7] = {0};
	__u8 pre_total[7];
	
	RunLog("�ڵ�[%d]���[%d]�����½���", node, pos + 1);
	
	//1. ��ñ��룬���ۻ����ͽ��
	err_cnt = 0;
	do {
		ret = GetPumpTotalL(pos);
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d]��ȡ����ʧ��", node, pos + 1);
			err_cnt++;
		}
	} while (ret < 0 && err_cnt <= 5);

	if (ret < 0) {
		return -1;
	}

	meter_total[0] = 0x0A;
	memcpy(&meter_total[1], pump.cmd + 3, 6);

	err_cnt = 0;
	do {
		ret = GetPumpTotalP(pos);
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d]��ȡ����ʧ��", node, pos + 1);
			err_cnt++;
		}
	} while (ret < 0 && err_cnt <= 5);

	if (ret < 0) {
		return -1;
	}


	amount_total[0] = 0x0A;
	memcpy(&amount_total[1], pump.cmd +3, 6);

	memcpy(pre_total, (lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Vol_Total, 7);
	
	//3. �õ���Ʒid ,Ӧ��Ϊ0	
	pr_no = (lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).PR_Id - 1;  // 0 - 7	
	if ((pr_no < 0) || (pr_no > 7)) {
		RunLog("DoEot PR_Id error, PR_Id[%d] <ERR>", pr_no + 1);
		// ֻ��¼�����˳�
	}
	

	//4. �������Ƿ��б仯,������򱣴潻�׼�¼
	if (memcmp(pre_total, meter_total, 7) != 0) {
//---------------�����в���ʱ�Ż�ȥȡ��Ӧ�Ľ��׼�¼-----------------------//
	       //1. ��ü��ͼ�¼
		err_cnt = 0;
		do {
			ret = GetTransDataL(pos);
			if (ret != 0) {
				err_cnt++;
				RunLog("�ڵ�[%d]���[%d]��ȡ���׼�¼ʧ��", node, pos + 1);
			}
		} while (ret < 0 && err_cnt <= 5);
		
		if (ret < 0) {
			return -1;
		}

		//Current_Volume  bin8 + bcd8
		volume[0] = 0x06;
		volume[1] = 0x00;
		memcpy(&volume[2],  pump.cmd + 3, 3);
		memcpy(lpDispenser_data->fp_data[pos].Current_Volume, volume, 5);

		err_cnt = 0;
		memset(pump.cmd, 0, sizeof(pump.cmd));
		do {
			ret = GetTransDataP(pos);
			if (ret != 0) {
				err_cnt++;
				RunLog("�ڵ�[%d]���[%d]��ȡ���׼�¼ʧ��", node, pos + 1);
			}
		} while (ret < 0 && err_cnt <= 5);
		
		if (ret < 0) {
			return -1;
		}


		//Current_Amount  bin8 + bcd 8
		amount[0] = 0x06;
		amount[1] = 0x00;
		memcpy(&amount[2],  pump.cmd + 3, 3);
		memcpy(lpDispenser_data->fp_data[pos].Current_Amount, amount, 5);

		//2.����ͼ�
		ret = GetPrice(pos);	
		if (ret != 0) {
			RunLog("Node[%d].FP[%d]ȡ�ͼ�ʧ��", node, pos + 1);
			memcpy(price, lpDispenser_data->fp_fm_data[pr_no][lpDispenser_data->fp_data[pos].Default_Fuelling_Mode - 1].Prod_Price, 4);
		} else {
			price[0] = 0x04;
			price[1] = 0x00;
			Hex2Bcd(&pump.cmd[3], 2, &price[2] , 2);
		}
					
		//Current_Unit_Price bin8 + bcd8
		memcpy(lpDispenser_data->fp_data[pos].Current_Unit_Price, price, 4);
		
		//chinese tax dispenser transimit in XXXXXXX.X, so use 0b as first byte.
		memcpy(lpDispenser_data->fp_m_data[pr_no].Meter_Total, meter_total, 7);		
		memcpy((lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Vol_Total, meter_total, 7);

#if 0
		RunLog("�ڵ�[%02d]���[%d]ǹ[%d] ## Amount: %02x%02x.%02x Volume: %02x%02x.%02x Price: %02x.%02x Total: %02x%02x%02x%02x.%02x", node, pos + 1, pump.gunLog[pos], amount[2], 
			amount[3], amount[4], volume[2], volume[3], volume[4], price[2],
			price[3], meter_total[2], meter_total[3], meter_total[4], meter_total[5], meter_total[6]);
#endif
		
		// Update Amount_Total
		memcpy((lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Amount_Total, amount_total, 7);
		memcpy(lpDispenser_data->fp_data[pos].Current_Prod_Nb, lpDispenser_data->fp_pr_data[pr_no].Prod_Nb, 4);

		err_cnt = 0;
		do {
			ret = EndTr4Ln(node, pos + 0x21, pump.gunLog[pos] + 0x10);
			if (ret < 0) {
				RunLog("�ڵ�[%d]���[%d]DoEot.EndTr4Ln ʧ��, ret: %d <ERR>", node,pos +1 ,ret);
				err_cnt++;
			}
		} while (ret < 0 && err_cnt <= 5);

		err_cnt = 0;
		do {
			ret = EndTr4Fp(node, pos + 0x21, pre_total,
				(lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Vol_Total);
			if (ret < 0) {
				RunLog("�ڵ�[%d]���[%d] DoEot.EndTr4Fp ʧ��, ret: %d <ERR>", node,pos + 1,ret);
				err_cnt++;
			}
		} while (ret < 0 && err_cnt <= 8);

	}else {
		ret = EndZeroTr4Fp(node, pos + 0x21);
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d]DoEot.EndZeroTr4Fp ʧ��, ret: %d <ERR>", node,pos+1,ret);
		}
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
			pr_no = (lpDispenser_data->fp_ln_data[i][0]).PR_Id - 1;  // 0-7	
			if ((pr_no < 0) || (pr_no > 7)) {
				RunLog("InitPumpPrice PR_Id error, PR_Id[%d]", pr_no + 1);
				return -1;
			}

			memcpy(lpDispenser_data->fp_data[i].Current_Unit_Price, 
					lpDispenser_data->fp_fm_data[pr_no][lpDispenser_data->fp_data[i].Default_Fuelling_Mode - 1].Prod_Price, 4);
#ifdef DEBUG
		RunLog("Init.FP %d LNZ %d PR_Id %d,Prod_Price: %02x%02x%02x%02x", i + 1, pump.gunLog[i], pr_no + 1,
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
	__u8 meter_total[7] = {0};
	__u8 amount_total[7] = {0};

	for (i = 0; i < pump.panelCnt; i++) {
		ret = GetPumpTotalL(i);
		if (ret < 0) {
			RunLog("ȡNode[%d].FP[%d]�ı���ʧ��", node, i);
			return -1;
		}

		// Volume_Total
		pr_no = (lpDispenser_data->fp_ln_data[i][pump.gunLog[i] - 1]).PR_Id - 1;  // 0-7	
		
		meter_total[0] = 0x0A;
		memcpy(&meter_total[1], pump.cmd + 3, 6);
		
		memcpy(lpDispenser_data->fp_m_data[pr_no].Meter_Total, meter_total, 7);
		memcpy((lpDispenser_data->fp_ln_data[i][pump.gunLog[i] - 1]).Log_Noz_Vol_Total, meter_total, 7);
#if DEBUG
		RunLog("Init.fp_ln_data[%d][%d].Vol_Total: %s ", i, pump.gunLog[i] - 1,
			Asc2Hex(lpDispenser_data->fp_ln_data[i][pump.gunLog[i] - 1].Log_Noz_Vol_Total, 7));
		RunLog("Init.fp_m_data[%d].Meter_Total[%s] ", pr_no, Asc2Hex(meter_total, 7));
				
#endif

	       // Amount_Total
	       ret = GetPumpTotalP(i);
		if (ret < 0) {
			RunLog("ȡNode[%d].FP[%d]�ı���ʧ��", node, i);
			return -1;
		}

		
		pr_no = (lpDispenser_data->fp_ln_data[i][pump.gunLog[i] - 1]).PR_Id - 1;  // 0-7	
		amount_total[0] = 0x0A;
		memcpy(&amount_total[1], pump.cmd +3, 6);
		memcpy((lpDispenser_data->fp_ln_data[i][pump.gunLog[i] - 1]).Log_Noz_Amount_Total, amount_total, 7);
		RunLog("Init.fp_ln_data[%d][%d].Amount_Total: %s ", i, pump.gunLog[i] - 1,
			Asc2Hex(lpDispenser_data->fp_ln_data[i][pump.gunLog[i] - 1].Log_Noz_Amount_Total, 7));

	}

	return 0;
}



/*
 * func: ��ȡ���ν��׼�¼
 */ 
static int GetTransDataP(int pos)
{
	int ret;
	int trynum;

	for (trynum = 0; trynum < TRY_TIME; ++trynum) { 
		Ssleep(INTERVAL);
		pump.cmd[0] = 0xfa;
		pump.cmd[1] = 0x03;
		pump.cmd[2] = pump.panelPhy[pos];
		pump.cmd[3] = 0x14;
		pump.cmd[4] = 0x77+ pump.cmd[1]+ pump.cmd[2]+ pump.cmd[3];
		pump.cmdlen = 5;
		pump.wantLen = 2;

		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]�·�����������ʧ��", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x10;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		gpGunShm->addInf[pump.chnLog - 1][0] = 0;
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]�·�����������ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		}

		if (pump.cmd[6]!=((0x77 + pump.cmd[1] + pump.cmd[2] + pump.cmd[3] + 
						pump.cmd[4] + pump.cmd[5]) & 0xFF)) {
                 	RunLog("Node[%d].FP[%d]��������У�����", node, pos + 1);
			continue;
		}		

		if (0xFA != pump.cmd[0]) {
			RunLog("Node[%d].FP[%d]�������ݴ���", node, pos + 1);
			continue;
		}

		return 0;
	}

	return -1;
}



/*
 * func: ��ȡ���ν��׼�¼
 */ 
static int GetTransDataL(int pos)
{
	int ret;
	int trynum;

	for (trynum = 0; trynum < TRY_TIME; ++trynum) { 
		pump.cmd[0] = 0xfa;
		pump.cmd[1] = 0x03;
		pump.cmd[2] = pump.panelPhy[pos];
		pump.cmd[3] = 0x13;
		pump.cmd[4] = 0x77+ pump.cmd[1]+ pump.cmd[2]+ pump.cmd[3];
		pump.cmdlen = 5;
		pump.wantLen = 2;

		Ssleep(INTERVAL);

		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]�·�����������ʧ��", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x10;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		gpGunShm->addInf[pump.chnLog - 1][0] = 0;
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]�·�����������ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		}

		if (pump.cmd[6]!=((0x77 + pump.cmd[1] + pump.cmd[2] + pump.cmd[3] + \
						pump.cmd[4] + pump.cmd[5]) & 0xFF)) {
                 	RunLog("Node[%d].FP[%d]��������У�����", node, pos + 1);
			continue;
		}		
			
		if (0xFA != pump.cmd[0]) {
			RunLog("Node[%d].FP[%d]�������ݴ���", node, pos + 1);
			continue;
		}

		return 0;
	}

	return -1;
}


/*
 * func: ��ȡ����Ԥ����, �����ж���Ȩ��ʽ
 */
static int GetKeystokePreset (int pos)
{
	int ret;
	int trynum;

	RunLog("Node[%d].FP[%d] Get KB Preset", node, pos + 1);

	for (trynum = 0; trynum < TRY_TIME; trynum++) {
		pump.cmd[0] = 0xfa;
		pump.cmd[1] = 0x03;
		pump.cmd[2] = pump.panelPhy[pos];
		pump.cmd[3] = 0x1A;
		pump.cmd[4] = 0x77 + pump.cmd[1]  + pump.cmd[2]  + pump.cmd[3] ;
		pump.cmdlen = 5;
		pump.wantLen = 2;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]�·�������Ԥ�����ɹ�, ȡ������Ϣʧ��",
							node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd)-1;
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x10;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		gpGunShm->addInf[pump.chnLog - 1][0] = 0;
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]������Ԥ����ʧ��", node, pos + 1);
			continue;
		}

		if (pump.cmd[7] != ((0x77 + pump.cmd[1] + pump.cmd[2] + pump.cmd[3] + \
					pump.cmd[4] + pump.cmd[5] + pump.cmd[6]) & 0xFF)) {
			RunLog("Node[%d].FP[%d]��������У�����", node, pos + 1);
			continue;
		}

		if (pump.cmd[3] != 0x00 && pump.cmd[3] != 0x01) {
			RunLog("Node[%d].FP[%d]������������", node, pos + 1);
			continue;
		}

		if (pump.cmd[2] != 0x1A || pump.cmd[0] != 0xFA) {
			RunLog("Node[%d].FP[%d]������������", node, pos + 1);
			continue;
		}

		return 0;
	}

	return -1;
}

static int DoAuth(int pos)
{
	int i;
	int ret;
	int trynum;

	RunLog("Node[%d].FP[%d] DoAuth", node, pos + 1);

	// 1. �ж��Ƿ��м���Ԥ��, ��������ж�����Ȩ
	ret = DoKeystokeRation(pos);
	if (ret == 0) {
		RunLog("ִ�����Ԥ�ü��ͳɹ�");
		return 0;
	}

	if (ret < 0) {
		RunLog("��ȡ���Ԥ�ü���ʱ���ִ���");
		return -1;
	}
		
	// 2. ���û�м���Ԥ��, ��ִ��δ������Ȩ
	for (trynum = 0; trynum < TRY_TIME; trynum++) {
		pump.cmd[0] = 0xfa;
		pump.cmd[1] = 0x03;
		pump.cmd[2] = pump.panelPhy[pos];
		pump.cmd[3] = 0x33;
		pump.cmd[4] = 0x77 + pump.cmd[1]  + pump.cmd[2]  + pump.cmd[3] ;
		pump.cmdlen = 5;
		pump.wantLen = 4;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]�·���Ȩʧ��",
							node, pos + 1);
			continue;
		  }

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd)-1;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]�·���Ȩ�ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		}

		if (pump.cmd[3]!=((0x77 + pump.cmd[1] + pump.cmd[2]) & 0xFF)) {
			RunLog("Node[%d].FP[%d]��������У�����", node, pos + 1);
			continue;
		}

		if (pump.cmd[2] != 0x33 || pump.cmd[0] != 0xFA) {
			RunLog("Node[%d].FP[%d]�������ݴ���", node, pos + 1);
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
	static __u8 arr_zero[3] = {0};
	__u8 amount[5],volume[5];

	
	//1. ��ü������ͼ��ͽ��
	ret = GetTransDataL(pos);
	if (ret < 0) {
		RunLog("GetTransDataL Failed");
		return -1;
	}
	
	volume[0] = 0x06;
	volume[1] = 0x00;
	memcpy(&volume[2], &pump.cmd[3], 3);

	ret = GetTransDataP(pos);
	if (ret < 0) {
		RunLog("GetTransDataP Failed");
		return -1;
	}		
	amount[0] = 0x06;
	amount[1] = 0x00;
	memcpy(&amount[2], &pump.cmd[3], 3);

	if (fp_state[pos] != 3) {
		if (memcmp(arr_zero, &volume[2], 3) == 0) {
			return 0;
		}
		fp_state[pos] = 3;
		return 0;
	}

	if ((__u8)FP_State_Fuelling == lpDispenser_data->fp_data[pos].FP_State) {	 
		ret = ChangeFP_Run_Trans(node, pos + 0x21, amount,  volume);
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
		RunLog("�ڵ�[%d]���[%d], ��ȡ��ǰ��ǹǹ��ʧ��", node, pos + 1);
		return -1;
	}

	ret = ChangeFP_State(node, pos + 0x21, FP_State_Calling, NULL);

	//��λ������,���������Զ���Ȩ;��λ��������, ��Ȩ�������ڿ���״̬,��ʱ�Զ���Ȩ
	if ((gpIfsf_shm->auth_flag == FCC_SWITCH_AUTH) && (lock_status() == 1)) {
		ret = DoAuth(pos);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]�Զ���Ȩʧ��", node, pos + 1);
		}
		ret = ChangeFP_State(node, pos + 0x21, FP_State_Started, NULL);
		RunLog("Node[%d].FP[%d]�Զ���Ȩ�ɹ�", node, pos + 1);
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
	*status = 0x99;
	
	for (i = 0; i < 2; ++i) {
		pump.cmd[0] = 0xfa;		
		pump.cmd[1] = 0x03;
		pump.cmd[2] = pump.panelPhy[pos];		
		pump.cmd[3] = 0x19;
		pump.cmd[4] = 0x77 + pump.cmd[1]  + pump.cmd[2]  + pump.cmd[3] ;
		pump.cmdlen = 5;
		pump.wantLen = 6;

		Ssleep(INTERVAL); 
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]�·���״̬����ʧ��", node, pos + 1);
			continue;;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]�·���״̬����ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		} 
		

		if (pump.cmd[5] != ((0x77 + pump.cmd[1] + pump.cmd[2] + \
					pump.cmd[3] + pump.cmd[4]) & 0xFF)) {
			RunLog("Node[%d].FP[%d]��������У�����", node, pos + 1);
			continue;
		}		
			  	
		if (0xFA != pump.cmd[0]) {
			RunLog("Node[%d].FP[%d]�������ݴ���", node, pos + 1);
			continue;
		}

		*status = pump.cmd[3];
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
	*status = 0x99;
	
	for (i = 0; i < 2; ++i) {
		pump.cmd[0] = 0xfa;		
		pump.cmd[1] = 0x03;
		pump.cmd[2] = pump.panelPhy[pos];		
		pump.cmd[3] = 0x19;
		pump.cmd[4] = 0x77 + pump.cmd[1]  + pump.cmd[2]  + pump.cmd[3] ;
		pump.cmdlen = 5;
		pump.wantLen = 6;

		Ssleep(INTERVAL); 
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]�·���״̬����ʧ��", node, pos + 1);
			continue;;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]�·���״̬����ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		} 

		if (0xFA != pump.cmd[0]) {
			RunLog("Node[%d].FP[%d]�������ݴ���", node, pos + 1);
			continue;
		}
		
		if (pump.cmd[5]!=((0x77 + pump.cmd[1] + pump.cmd[2] + pump.cmd[3] + pump.cmd[4])&0xFF)) {
			RunLog("Node[%d].FP[%d]��������У�����", node, pos + 1);
			continue;
		}		
			  	
		*status = pump.cmd[3];
		return 0;
	}

	return -1;

}


/*
 *  func: ��ʼ��gunLog, Ԥ�ü���ָ����Ҫ; ���޸� Log_Noz_State
 */
static int InitGunLog()
{
	int i;
	int ret;

	for (i = 0; i < pump.panelCnt; ++i) {
		pump.gunLog[i] =  1;   
	}

	return 0;
}


static int GetCurrentLNZ(int pos)
{	
	  pump.gunLog[pos] = 1;	// ��ǰ̧�����ǹ	
	  RunLog("Node %d FP %d �� %d ��ņ̀��", node, pos + 1, pump.gunLog[pos]);
	  lpDispenser_data->fp_data[pos].Log_Noz_State = 1;
	  return 0;
}

/*
 * func: �趨�������ԡ�ͨѶ���ʡ�У��λ��
 */
static int SetPort018()
{
	int ret;

	pump.cmd[0] = 0x1C;
	pump.cmd[1] = 0x52;
	pump.cmd[2] = 0x30 | (pump.chnPhy - 1);
	pump.cmd[3] = 0x06;	                        //4800bps
	pump.cmd[4] = 0x01<<((pump.chnPhy - 1) % 8);	//EVEN parity
	pump.cmd[5] = 0x00;
	pump.cmd[6] = 0x01<<((pump.chnPhy - 1) % 8);	//EVEN parity
	pump.cmdlen = 7;
	pump.wantLen = 0;

	ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy,
		pump.cmd, pump.cmdlen, 0, pump.wantLen, TIME_OUT_WRITE_PORT);
	if (ret < 0) {
		UserLog("ͨ��%d���ô�������ʧ��", pump.chnLog);
		return -1;
	}
	
	Ssleep(INTERVAL);

	return 0;
}


/*
 * func:  ͨѶ��ͨʱ��ѯ�����и�һ��ʱ������һ�´�������
 *        ����������ĵ�ѹ���ȵ��µ�Ƭ����λ--�����������ûָ�Ԥ��9600,8,n,1
 *        [�����ڼ�ʹ��, ���������ڼ������ɵ�Ƭ����λ]
 */
static int ReDoSetPort018()
{
#define RE_DO_FREQUENCY   10         //  10����ѯʧ������һ�´�������

	int static polling_fail_cnt = 0;
	int ret;

	if ((+ + polling_fail_cnt) >= RE_DO_FREQUENCY) {
		ret = SetPort018();
		if (ret < 0) {
			RunLog("�ڵ� %d ���贮������ʧ��", node);
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
	int i;
	int ret;

	for (i = 0; i < TRY_TIME; i++) {		
		memset(pump.cmd,0,pump.cmdlen);
		pump.cmd[0] = 0xfa;
		pump.cmd[1] = 0x03;
		pump.cmd[2] = pump.panelPhy[pos];
		pump.cmd[3] = 0x30;
		pump.cmd[4] = 0x77 + pump.cmd[1] + pump.cmd[2] + pump.cmd[3];
		pump.cmdlen = 5;
		pump.wantLen = 4;

		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy,
			       	pump.cmd, pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]�·�������������ʧ��", node, pos + 1);
			continue;
		}
		
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]�·�������������ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		}

		if (0xFA != pump.cmd[0]) {
			RunLog("Node[%d].FP[%d]�������ݴ���", node, pos + 1);
			continue;
		}

		if (pump.cmd[3]!=((0x77 + pump.cmd[1] + pump.cmd[2])&0xFF)) {
			RunLog("Node[%d].FP[%d]��������У�����", node, pos + 1);
			continue;
		}

		RunLog("Node[%d].FP[%d]����ģʽ���óɹ�!", node, pos + 1);
		return 0;
	}

	return -1;
}


static int GetPrice(int pos)
{
	int ret;
	int trynum;
	
	for(trynum = 0; trynum < TRY_TIME; trynum++) {
		pump.cmd[0] = 0xfa;
		pump.cmd[1] = 0x03;
		pump.cmd[2] = pump.panelPhy[pos];
		pump.cmd[3] = 0x10;
		pump.cmd[4] =0x77 + pump.cmd[1] + pump.cmd[2] + pump.cmd[3];
		pump.cmdlen = 5;
		pump.wantLen = 2;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, 
				pump.cmd, pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]�·�����������ʧ��", node, pos + 1);
			continue;
		}
		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd)-1;

		gpGunShm->addInf[pump.chnLog - 1][0] = 0x10;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		gpGunShm->addInf[pump.chnLog - 1][0] = 0;
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]�·�����������ɹ�, ȡ������Ϣʧ��",
								node, pos + 1);
			continue;
		}
		
		if (0xFA != pump.cmd[0]) {
			RunLog("Node[%d].FP[%d]�������ݴ���", node, pos + 1);
			continue;
		}

		if (pump.cmd[5]!=((0x77 + pump.cmd[1] + pump.cmd[2] + pump.cmd[3] + pump.cmd[4]) & 0xFF)) {
			RunLog("Node[%d].FP[%d]��������У�����", node, pos + 1);
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
	int ret;
	int trynum;
	__u8 status;

	RunLog("Node[%d].FP[%d] DoStop", node, pos + 1);

	// ȡ��Ԥ����
	pump.addInf[pos][15] = 0 ;

	for(trynum = 0; trynum < TRY_TIME; trynum++) {
		pump.cmd[0] = 0xfa;
		pump.cmd[1] = 0x03;
		pump.cmd[2] = pump.panelPhy[pos];
		pump.cmd[3] = 0x34;
		pump.cmd[4] = 0x77 + pump.cmd[1]  + pump.cmd[2]  + pump.cmd[3] ;
		pump.cmdlen = 5;
		pump.wantLen = 4;
		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]�·�ֹͣ��������ʧ��", node, pos + 1);
			continue;
		}
		
		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd)-1;

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]�·�ֹͣ��������ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		}

		if (0xFA != pump.cmd[0]) {
			RunLog("Node[%d].FP[%d]�������ݴ���", node, pos + 1);
			continue;
		}

		if (pump.cmd[3] != ((0x77 + pump.cmd[1] + pump.cmd[2]) & 0xFF)) {
			RunLog("Node[%d].FP[%d]��������У�����", node, pos + 1);
			continue;
		}
		
		ret = GetPumpStatus(pos, &status);
		if (ret < 0) {
			continue;
		}

		if (((status & 0x60) == PUMP_CALL) || ((status & 0x60) == PUMP_OFF)) {
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
	Hex2Bcd( &pump.cmd[3] , 2, &price[0], 2);
	   
	for (lnz = 0; lnz < pump.gunCnt[pos]; lnz++) {
		pr_no = (lpDispenser_data->fp_ln_data[pos][lnz]).PR_Id - 1;  // 0 - 7	
		if ((pr_no < 0) || (pr_no > 7)) {
			RunLog("CheckPrice ERR PR_Id error, FP[%d]LNZ[%d] PR_Id[%d] <ERR>",
								pos + 1, lnz + 1, pr_no + 1);
			return -1;
		}

		fm_idx = lpDispenser_data->fp_data[pos].Default_Fuelling_Mode - 1;
		RunLog("Node %d FP %d LNZ %d Now price is %02x.%02x; DST Price: %02x.%02x",
			node,pos+1,lnz+1, price[0], price[1],
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


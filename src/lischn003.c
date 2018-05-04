/*
 * Filename: lischn003.c
 *
 * Description: ���ͻ�Э��ת��ģ�顪̫�� CK_CL 9600,8,e,1
 *
 * 2008.04 by Yongsheng Li <Liys@css-intelligent.com>
 * 
 *
 * History:   
 * 2008��9��11���ͻ��ѻ���ϸ������
 *        ���ͻ����ڼ���ʱ��FCC�ѻ�����������ʱ���ȼ���ͻ��Ƿ�Ϊ����״̬
 *        ����ǣ�����Ҫ�ѵ�ʱ���͵�ʵʱ���ݰ�һ���ѻ����״�����������Ϊ0
 *        ������ǣ�����Ҫ�ȼ���״̬������ȡ���룬���Ƿ�Ϊ�жϵ���һ��
 *        ����ǣ���һ���������״�������������ǣ����ǰѵ�ʱ��һ��
 *        ��ʵʱ��������һ���ѻ����״�����������Ϊ0
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


#define PUMP_OFF	0x00    //���
#define PUMP_CALL	0x01
#define PUMP_BUSY	0x02
#define PUMP_AUTH       0x03   //δ��ǹ��Ȩ״̬  


#define TRY_TIME	3
#define INTERVAL	100000

static int DoCmd();
static void DoPolling();
static int DoAuth(int pos);
static int DoStarted(int i);
static int DoBusy(int pos, __u8 status);
static int DoCall(int pos);
static int DoEot(int pos);
static int DoOpen(int pos, __u8 *msgbuffer);
static int DoClose(int pos, __u8 *msgbuffer);
static int DoRation(int pos1);
static int DoStop(int pos);
static int DoRecovery(int pos);
static int DoMode();
static int DoPrice();
static int ChangePrice();
static int GetTransData(int pos);
static int GetPumpStatus(int pos, __u8 *status);
static int GetPumpOnline(int pos, __u8 *status);
static int GetPumpTotal(int pos);
static int InitPumpTotal();
static int InitCurrentPrice();
static int InitGunLog();
static int GetCurrentLNZ(int pos);
static int SetPort003();
static int ReDoSetPort003();
static int SetCtrlMode(int pos);
static int keep_in_touch_xs(int x_seconds);
static int CheckPriceErr(int pos);


static PUMP pump;
static int node;
static int IFSFpos;
static DISPENSER_DATA *lpDispenser_data;
static NODE_CHANNEL *lpNode_channel;
static int fp_state[MAX_FP_CNT];   // 0 - IDLE, 1 - CALLING , 2 - DoStarted, 3 - Fueling
static int dostop_flag = 0;     // ����Ƿ�ɹ�������ͣ����


int ListenChn003(int chnLog)
{
	long	indtyp;
	int	ret;
	int	fd;
	int	j;
	int	i;
	__u8    cmd[8];
	__u8 status;
	
	sleep(5);   //��ֹ�˽����˳�,��������û���������. ÿ��lischnxxx����Ҫ
	RunLog("Listen Channel  003 ,logic channel[%d]", chnLog);
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
						pump.panelPhy[j] = gpIfsf_shm->gun_config[idx].phyfp;
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
	
	strcpy(pump.cmd, alltty[pump.port-1]);
	pump.fd = OpenSerial(pump.cmd);
	if (pump.fd < 0) {
		RunLog("�򿪴���[%s]ʧ��", pump.cmd);
		return -1;
	}

	ret = SetPort003();
	if (ret < 0) {
		RunLog("���ô�������ʧ��");
		return -1;
	}
	
	//3.  ���Լ��ͻ��Ƿ�����,�����������ȴ�
	while (1) {
		ret = GetPumpOnline(0, &status);
		if (ret == 0) {
			ret = SetCtrlMode(0);
			if (ret !=0) 
				continue;		
			break;
		}

		gpIfsf_shm->node_online[IFSFpos] = 0; //������,����������.
		UserLog(" �ڵ��[%d] , ������", node);
		
		sleep(1);
	}


	// IDLE���ٽ����������� , Ҫ���ڳ�ʼ������֮ǰ
	// ����̧ǹʱ�ܷ�õ�����
	i = 30;
	do {
		ret = GetPumpStatus(0, &status);
		if (ret == 0 && ((status & 0x03) != PUMP_BUSY)) {
				break;
		}
		i--;
		sleep(1);
	} while (i > 0);

	if (i <= 0) {
		return -1;
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


	// һ�о���, ����
//	gpIfsf_shm->node_online[IFSFpos] = node;
	ChangeNode_Status(node, NODE_ONLINE);
	UserLog("�ڵ� %d ����,״̬[%02x]!", node, status);
	keep_in_touch_xs(12);

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
				sleep(1);
				DoRecovery(pos);
				dostop_flag = 1;
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
				if ((__u8)FP_State_Calling == lpDispenser_data->fp_data[pos].FP_State ) {
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

	//return 0;
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
		/*
		__u8 ln_status = 0;     // û�а취��GetCurrentLNZ���̧���ǹ��:(
		ln_status = 1 << (pump.gunLog[i] - 1);
		lpDispenser_data->fp_data[i].Log_Noz_State = ln_status;
		RunLog("Node %d FP %d �� %d ��ņ̀��", node, i + 1, pump.gunLog[i]);
		*/
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
static int DoMode() 
{
	return -1;
}

/*
 * func: PreSet/PrePay & Release_FP
 */
static int DoRation(int pos)  
{
	int i;
	int j;
	int ret;
	int trynum;
	__u8 preset_type;         // 0x01 - PreSet; 0x02 - PrePay
	__u8 amount[6];
	

	if (pump.cmd[0] == 0x3D) { 
		preset_type = 0x01;
	} else {
		preset_type = 0x02;
	}
	
	if(pump.cmd[0] == 0x3D){//����
		amount[0] = lpDispenser_data->fp_data[pos].Remote_Volume_Preset[2];	
		amount[1] = lpDispenser_data->fp_data[pos].Remote_Volume_Preset[3];
		amount[2] = lpDispenser_data->fp_data[pos].Remote_Volume_Preset[4];			      	
	} else if(pump.cmd[0] == 0x4D){//����
		amount[0] = lpDispenser_data->fp_data[pos].Remote_Amount_Prepay[2];	
		amount[1] = lpDispenser_data->fp_data[pos].Remote_Amount_Prepay[3];
		amount[2] = lpDispenser_data->fp_data[pos].Remote_Amount_Prepay[4];	  	
	} else {
		RunLog( "�Ƿ�������" );
	}

	memset( pump.cmd, 0, sizeof(pump.cmd) );
	pump.cmdlen = 0;	
	for (trynum = 0; trynum < TRY_TIME; trynum++ ) {		
		Ssleep(INTERVAL);

		pump.cmd[0] = 0xAA;
		pump.cmd[1] = 0x07;
		pump.cmd[2] = pump.panelPhy[pos];
		pump.cmd[3] = 0x16;
		pump.cmd[4] = preset_type;

		memcpy(pump.cmd + 5, amount, 3);
		pump.cmd[8] = Check_xor(&pump.cmd[2], pump.cmd[1] - 1);
		
		pump.cmdlen = 9;
		pump.wantLen= 9;

		ret = WriteToPort( pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT );
		if( ret < 0 ) {
			RunLog("Node[%d]FP[%d]�·�Ԥ�ü�������ʧ��", node, pos + 1);
			continue;
		}

		memset( pump.cmd, 0, sizeof(pump.cmd) );
		pump.cmdlen = sizeof(pump.cmd)-1;
		ret = GetReturnMsg( GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if( ret < 0 ) {
			RunLog("Node[%d]FP[%d]�·�Ԥ�ü�������ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		}

		if (pump.cmd[0] != 0xBB || Check_xor(&pump.cmd[2], 6) != pump.cmd[8]) {
			RunLog("Node[%d]FP[%d]��������У�����", node, pos + 1);
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
	int  trynum;
	__u8 price[4] = {0};
	__u8 pr_no;
	__u8 lnz, status;
	__u8 err_code;
	__u8 err_code_tmp;


	for (pr_no = 0; pr_no < FP_PR_NB_CNT; pr_no++) {    
              //һ̨���ͻ�ֻ��һ����ƷҲֻ��һ���ͼ�
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
					
					for (trynum = 0; trynum < TRY_TIME; trynum++) {
						Ssleep(INTERVAL);
						pump.cmd[0] = 0xAA;
						pump.cmd[1] = 0x05;
						pump.cmd[2] = pump.panelPhy[i];
						pump.cmd[3] = 0x10;
						pump.cmd[4] = pump.newPrice[pr_no][2];
						pump.cmd[5] = pump.newPrice[pr_no][3];
						pump.cmd[6] = Check_xor(&pump.cmd[2],pump.cmd[1] - 1);
					
						pump.cmdlen = 7;
						pump.wantLen = 7;

						ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
								pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
						if (ret < 0) {
							RunLog("Node[%d]FP[%d]�·��������ʧ��", node, i + 1);
							continue;
						} else {  // ��۳ɹ����µ���д�����ݿ� & ���㻺��

						RunLog("Node[%d].FP[%d].PriceChange_CMD: %s",
							node, i + 1, Asc2Hex(pump.cmd, pump.cmdlen));

						memset( pump.cmd, 0, sizeof(pump.cmd) );
						pump.cmdlen = sizeof(pump.cmd)-1;
						ret = GetReturnMsg( GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen );
						if( ret < 0 ) {
							RunLog("Node[%d]FP[%d]�·��������ɹ�, ȡ������Ϣʧ��",
												node, i + 1);
							continue;
						}

						if (pump.cmd[0] != 0xBB ||
							(Check_xor(&pump.cmd[2], 4) != pump.cmd[6])) {
							RunLog("Node[%d]FP[%d]��������У�����", node, i + 1);
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
							continue;
						}

						if( pump.cmd[4] == 0x00 &&  pump.cmd[5] == 0x00 ) {
							RunLog( "Node[%d]FP[%d]ִ�б������ʧ��", node, i);
							err_code_tmp = 0x37;
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
							continue;
						}
						ret = GetPumpStatus(i, &status);
						if (ret < 0) {
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
							continue;
						}

						if (memcmp(&pump.cmd[5], &pump.newPrice[pr_no][2], 2) != 0) {
							RunLog("Node[%d]FP[%d]ִ�б������ʧ��", node, i);
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
				if ((++fail_cnt) < MAX_FAIL_CNT) {  // ���ͻ������жϣ�ͨѶʧ�ܳ���5�����ж��ͻ�����
					/*
					 * �����贮�����Իᵼ��ReadTTY���յ�"�Ƿ�����", ����ʱ�Ƴ�
					 * 2009.02.19
					if (fail_cnt >= (MAX_FAIL_CNT / 2)) {
						if (SetPort003() >= 0) {
							RunLog("�ڵ�%d���贮�����Գɹ�", node);
						}
					}
					*/
					continue;
				}

				// Step.1
				dostop_flag = 0;
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
			//ReDoSetPort003();
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

				// Step.1
				if (SetCtrlMode(0) != 0) {
					RunLog("�ڵ�[%d]��������ʧ��", node);
					continue;
				}

				// Step.2
				ret = InitPumpTotal();
				if (ret < 0) {
					RunLog("�ڵ�[%d]���±�������ʧ��", node);
					continue;
				}

				// Step.3
				for (j = 0;  j < pump.panelCnt; ++j) {
					lpDispenser_data->fp_data[j].FP_State = FP_State_Closed;
				}

				// һ��׼������, ����
//				gpIfsf_shm->node_online[IFSFpos] = node;
				ChangeNode_Status(node, NODE_ONLINE);
				RunLog("====== Node %d �ָ�����! =========================", node);
				keep_in_touch_xs(12);               // ȷ��CD�Ѿ��յ�����֮��������������
				// Step.4                 // �����ϴ���ǰFP״̬
				for (j = 0; j < pump.panelCnt; ++j) {
					ChangeFP_State(node, j + 0x21, FP_State_Closed, NULL);
				}
				continue;
			}

			if (((status & 0x04) == 0) || fail_cnt >= 2) {
				ret = 0;
				for (j = 0;  j < pump.panelCnt; ++j) {
					if (SetCtrlMode(j) < 0) {   // ����Ϊ����ģʽ
						RunLog("Node[%d]FP[%d]��������ģʽʧ��", node, j + 1);
						ret--;
					}
				}

				if (ret >= 0) {
					fail_cnt = 0;
				}
				/*
				 * ��SetCtrlMode���ؽ���ѽ� GetPumpStatus���ؽ������,
				 * ���Դ˴�������ξ�Ҫcontinue;
				 */
				continue;
			}

			fail_cnt = 0;						
		}

		if (status != pre_status[i]) {
			pre_status[i] = status;
			RunLog("�ڵ�[%d]���[%d]��ǰ״̬[%02x]", node, i + 1, status);
		}



		if (gpIfsf_shm->auth_flag - FCC_SWITCH_AUTH != 0) {  //����к�̨
			if ((__u8)FP_State_Closed == lpDispenser_data->fp_data[i].FP_State) {
				if (i == 0) {
					times += pump.panelCnt;					
				}
				if (times > (SENDSTATE *  pump.panelCnt)) {
					if (i >= pump.panelCnt -1)
						times = 0;
					UserLog("�ڵ�[%d]�����[%d]����Closed״̬,���ʼ����Open", node, i + 1);
				}

				// Closed״̬��ҲҪ�ܹ����
				ChangePrice();
				continue; 
			}
		} else {
			if ((__u8)FP_State_Closed == lpDispenser_data->fp_data[i].FP_State) {
				RunLog("DoPolling, û�к�̨�ı�FP[%02x]��״̬ΪIDLE", i + 0x21);
				ret = SetHbStatus(node, 0);
				ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
			}
		}

		// ��FP״̬ΪInoperative, Ҫ���˹���Ԥ,�����ܸı���״̬,Ҳ�����ڸ�״̬�����κβ���
		if ((__u8)FP_State_Inoperative == lpDispenser_data->fp_data[i].FP_State) {
			RunLog("FP[%d]����Inoperative״̬!", i + 1);
			// Inoperative ״̬��ҲҪ�ܹ����
			ChangePrice();

			CheckPriceErr(i);
			continue;
		}

		switch (status&0x03) {
		case PUMP_OFF:					// ��ǹ
		       dostop_flag = 0;
			if (FP_State_Authorised == lpDispenser_data->fp_data[i].FP_State) { 
		       // ����Ȩ״̬���ͻ�ͨѶ����, �����ж�Ϊ����֮ǰ�ָ�, �ָ�ʱ�Ѿ��ǿ���״̬��, ���Ƿ��н���
				DoStarted(i);
			}

			if (fp_state[i] >= 2) {
				// Note.��û�вɵ���ǹ�ź�(EOT)�Ĳ��䴦��
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

			if (((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State )) {
				ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
			}

			// clear in_progress_offline_tr_flag
			EO_OFFLINE_TR(node, i);
			if (gpIfsf_shm->fp_offline_flag[node - 1][i] != 0) {
		//		RunLog("Dopolling, call upload_offline_tr");
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
		       if (dostop_flag != 0) {
				break;
			}

			if ((__u8)FP_State_Calling > lpDispenser_data->fp_data[i].FP_State) {
				ret = DoCall(i);
				fp_state[i] = 1;

				DoStarted(i);
			} else if (FP_State_Authorised == lpDispenser_data->fp_data[i].FP_State) {
				DoStarted(i);
				ret = ChangeFP_State(node, i + 0x21, FP_State_Started, NULL);
			}

			break;
		case PUMP_BUSY:					// ����
			if (dostop_flag != 0) {    // �����ͣ�ɹ��򱣳�IDLE״̬
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

			DO_INTERVAL(DO_BUSY_INTERVAL, ret = DoBusy(i, status););
                   					 
			if ((__u8)FP_State_Fuelling > lpDispenser_data->fp_data[i].FP_State) {
				if (fp_state[i] == 3) {
					ret = ChangeFP_State(node, i + 0x21, FP_State_Fuelling, NULL);
					if (ret < 0) {
						break;
					}
				}
			}

			break;

		case PUMP_AUTH:			
			if ((__u8)FP_State_Authorised != lpDispenser_data->fp_data[i].FP_State) {                            
				ret = ChangeFP_State(node, i + 0x21, FP_State_Authorised, NULL);
			}
			break;	
		default:
			break;
		}
		
//		Ssleep(INTERVAL*10);
	}

}


/*
 * func: ��ȡ��FP����ǹ�ı���
 */
static int GetPumpTotal(int pos)
{
	int ret;
	int  trynum;

	for (trynum = 0; trynum <TRY_TIME ; trynum++) {
		Ssleep(INTERVAL);
		pump.cmd[0] = 0xAA;
		pump.cmd[1] = 0x03;
		pump.cmd[2] = pump.panelPhy[pos];
		pump.cmd[3] = 0x02;
		pump.cmd[4] = pump.cmd[2] ^  pump.cmd[3];
		pump.cmdlen = 5;
		pump.wantLen = 15;
		
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]������ʧ��", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]������ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		}

		if(pump.cmd[0] != 0xBB || pump.cmd[1] != 0x0D ) {
			RunLog("Node[%d]FP[%d]��������У�����", node, pos + 1);
			continue;
		}

		if(Check_xor(&pump.cmd[2], 12) != pump.cmd[14])	{
			RunLog("Node[%d]FP[%d]��������У�����", node, pos + 1);
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
	int i;
	int ret;
	int pr_no;
	int err_cnt;
	__u8 price[5];
	__u8 volume[5];
	__u8 amount[5];
	__u8 pre_total[7];
	
	__u8 status;
	__u8 msg[12];
	__u8 meter_total[7] = {0};
	__u8 amount_total[7] = {0};	
	
	RunLog("�ڵ�[%d]���[%d]�����½���", node, pos + 1);
	
	//1. ��õ���  Jie@10.15
	// ��Ϊ̫���ͻ������ٶȵ�����, ��������׺�������״̬, ��һ���ܻ�û����Ӧ, 
	ret = GetPumpStatus(pos,&status);	
	if( ret < 0 ) {
		RunLog( "�ڵ�[%d]���[%d]��ȡ����ʧ�� <ERR>" ,node, pos+1);
		memcpy(price, lpDispenser_data->fp_fm_data[pr_no][lpDispenser_data->fp_data[pos].Default_Fuelling_Mode - 1].Prod_Price, 4);
	} else {
		price[0] = 0x04;
		price[1] = 0x00;
		memcpy(&price[2], &pump.cmd[5], 2);
	}
	

	//2. ��ñ��룬���ۻ����ͽ��
	err_cnt = 0;
	do {
		ret =  GetPumpTotal(pos);	
		if( ret < 0 ) {
			err_cnt++;
			RunLog( "�ڵ�[%d]���[%d]��ȡ����ʧ�� <ERR>" ,node, pos+1);
		}
	} while (ret < 0 && err_cnt <=5);

	if (ret < 0) {
		return -1;
	}

	meter_total[0] = 0x0A;
	meter_total[1] = 0x00;
	memcpy(&meter_total[2], pump.cmd+4, 5 );

	amount_total[0] = 0x0A;
	amount_total[1] = 0x00;
	memcpy(&amount_total[2], pump.cmd+9, 5 );

	memcpy(pre_total, (lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Vol_Total, 7);
	
	//3. �õ���Ʒid ,Ӧ��Ϊ0	
	pr_no = (lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).PR_Id - 1;  // 0 - 7	
	if ((pr_no < 0) || (pr_no > 7)) {
		RunLog("DoEot PR_Id error, PR_Id[%d] <ERR>", pr_no + 1);
	}
	

	//4. �������Ƿ��б仯,������򱣴潻�׼�¼
	if (memcmp(pre_total, meter_total, 7) != 0) {
#if 0
		RunLog("ԭ����Ϊ %s",Asc2Hex((lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Vol_Total,7));
		RunLog("��ǰ����Ϊ %s",Asc2Hex(meter_total,7));	  
#endif

		//------�����벻һ�£���ȡ��Ӧ�Ľ��׼�¼---//
		//5. ��ü��ͼ�¼

		err_cnt = 0;
		do {
			ret = GetTransData( pos );
			if( ret < 0 ) {
				err_cnt++;
				RunLog( "�ڵ�[%d]���[%d]��ȡ���׼�¼ʧ�� <ERR>" ,node, pos+1);
			}
		} while (ret < 0 && err_cnt <= 5);

		if (ret < 0) {
			return -1;
		}
			
		memset( msg, 0, sizeof(msg) );
		memcpy( msg, pump.cmd, 11 );
		msg[11] = 0;

                
		//Current_Unit_Price bin8 + bcd8
		memcpy(lpDispenser_data->fp_data[pos].Current_Unit_Price, price, 4);

//--------------------------------------------------------------------//

		//Current_Volume  bin8 + bcd8
		volume[0] = 0x06;
		volume[1] = 0x00;
		memcpy(&volume[2],  msg+4, 3);
		memcpy(lpDispenser_data->fp_data[pos].Current_Volume, volume, 5);

		//Current_Amount  bin8 + bcd 8
		amount[0] = 0x06;
		amount[1] = 0x00;
		memcpy(&amount[2],  msg+7, 3);
		memcpy(lpDispenser_data->fp_data[pos].Current_Amount, amount, 5);

		memcpy(lpDispenser_data->fp_m_data[pr_no].Meter_Total, meter_total, 7 );		
		memcpy((lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Vol_Total, meter_total, 7);
		
#if 0
              RunLog("�ڵ�[%02d]���[%d]ǹ[1] ## Amount: %02x%02x.%02x Volume: %02x%02x.%02x Price: %02x.%02x Total: %02x%02x%02x.%02x", node, pos + 1, amount[2], amount[3], amount[4],
				volume[2], volume[3], volume[4], price[2], price[3], meter_total[3],
								meter_total[4], meter_total[5], meter_total[6]);
#endif
		
		// Update Amount_Total
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

	}else {
		ret = EndZeroTr4Fp(node, pos + 0x21);
		if (ret < 0) {
			RunLog("DoEot.EndZeroTr4Fp ʧ��, ret: %d <ERR>", ret);
		}
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
			pr_no = (lpDispenser_data->fp_ln_data[i][0]).PR_Id - 1;  // 0-7	
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
	__u8 status;
	__u8 meter_total[7] = {0};
	__u8 amount_total[7] = {0};

	for (i = 0; i < pump.panelCnt; i++) {
		ret = GetPumpStatus(i, &status);
		if (ret < 0) {
			return -1;
		}

		status &= 0x03;
		if (status != PUMP_OFF && status != PUMP_CALL && status != PUMP_AUTH) {
			RunLog("��ǰFP״̬���ܲ�ѯ����,Status:[%02x]",status);
			return -1;
		}


		Ssleep(INTERVAL);
		ret = GetPumpTotal(i);
		if (ret < 0) {
			RunLog("ȡ Node %d FP %d �ı���ʧ��", node, i);
			return -1;
		}

	       // Volume_Total
		pr_no = (lpDispenser_data->fp_ln_data[i][pump.gunLog[i] - 1]).PR_Id - 1;  // 0-7	
		
		meter_total[0] = 0x0A;
		meter_total[1] = 0x00;
		memcpy(&meter_total[2], pump.cmd + 4, 5 );
		
		memcpy(lpDispenser_data->fp_m_data[pr_no].Meter_Total, meter_total, 7);
		memcpy((lpDispenser_data->fp_ln_data[i][pump.gunLog[i] - 1]).Log_Noz_Vol_Total, meter_total, 7);
#if DEBUG
		RunLog("Init.fp_ln_data[%d][%d].Vol_Total: %s ",i,pump.gunLog[i] - 1,
			Asc2Hex(lpDispenser_data->fp_ln_data[i][pump.gunLog[i] - 1].Log_Noz_Vol_Total, 7));
		RunLog("Init.fp_m_data[%d].Meter_Total[%s] ", pr_no, Asc2Hex(meter_total, 7));
				
#endif

	       // Amount_Total
		amount_total[0] = 0x0A;
		amount_total[1] = 0x00;
		memcpy(&amount_total[2], pump.cmd + 9, 5 );
		memcpy((lpDispenser_data->fp_ln_data[i][pump.gunLog[i] - 1]).Log_Noz_Amount_Total, amount_total, 7);
		RunLog("Init.fp_ln_data[%d][%d].Amount_Total: %s ", 
			i,pump.gunLog[i] - 1,
			Asc2Hex(lpDispenser_data->fp_ln_data[i][pump.gunLog[i] - 1].Log_Noz_Amount_Total, 7));

	}

	return 0;
}


/*
 * func: ��ȡ���ν��׼�¼
 */ 
static int GetTransData(int pos)
{
	int ret;
	int trynum;
	__u8 temp[5] = {0};

	for (trynum = 0; trynum < TRY_TIME; ++trynum) { 
		pump.cmd[0] = 0xAA;
		pump.cmd[1] = 0x03;
		pump.cmd[2] = pump.panelPhy[pos];
		pump.cmd[3] = 0x03;
		pump.cmd[4] = pump.cmd[2] ^ pump.cmd[3];
		pump.cmdlen = 5;
		pump.wantLen = 11;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]�·�����������ʧ��", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]�·�����������ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		}
	
		if (pump.cmd[0] != 0xBB || (Check_xor(&pump.cmd[2], 8) != pump.cmd[10])) {
			RunLog("Node[%d]FP[%d]��������У�����", node, pos + 1);
#if 0
			//��λ���յ����׼�¼�󣬱��뷵�ؽ��
			//���׼�¼�ж��д��󣬷���AA+03+ǹ��+21H+У���
			temp[0] = 0xAA;
			temp[1] = 0x03;
			temp[2] = pump.panelPhy[pos];
			temp[3] = 0x21;
			temp[4] = temp[2] ^ temp[3];

			Ssleep(INTERVAL);
			ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, temp,
			       	5, 1, 2, TIME_OUT_WRITE_PORT);
			if (ret < 0) {
				RunLog("Node[%d]FP[%d]�·�����NAKʧ��", node, pos + 1);
				continue;  
			}
#endif
			continue;
		}
                     
#if 0
		//��λ���յ����׼�¼�󣬱��뷵�ؽ��
		//�յ����׼�¼�ɹ��󣬷���AA+03+ǹ��+20H+У���
		temp[0] = 0xAA;
		temp[1] = 0x03;
		temp[2] = pump.panelPhy[pos];
		temp[3] = 0x20;
		temp[4] = temp[2] ^ temp[3];

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, temp,
			5, 1, 2, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]�·�����ACKʧ��", node, pos + 1);
		}
#endif
		return 0;
	}

	return -1;
}



/*
 * func: ��ͣ���ͣ��ڴ���Terminate_FP��,�����ͻ�
 */
static int DoStop(int pos)
{
	int ret;
	int trynum;

	RunLog("Node %d FP %d DoStop", node, pos + 1);

	for( trynum = 0; trynum < TRY_TIME; trynum++ ) {
		pump.cmd[0] = 0xAA;
		pump.cmd[1] = 0x03;
		pump.cmd[2] = pump.panelPhy[pos];
		pump.cmd[3] = 0x12;
		pump.cmd[4] = pump.cmd[2] ^ pump.cmd[3];						
		pump.cmdlen = 5;
		pump.wantLen = 5;

		Ssleep( INTERVAL );
		ret = WriteToPort( pump.fd, pump.chnLog, pump.chnPhy, 
				pump.cmd, pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT );
		if (ret < 0) {
			RunLog( "Node[%d]FP[%d]�·�ֹͣ��������ʧ��", node, pos + 1);
			continue;
		}

		memset( pump.cmd, 0, sizeof(pump.cmd) );
		pump.cmdlen = sizeof(pump.cmd)-1;
		gpGunShm ->addInf[pump.indtyp - 1][0] = 0x03; //�䳤��tty.c
		ret = GetReturnMsg( GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		gpGunShm ->addInf[pump.indtyp - 1][0] = 0x00; 
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]�·�ֹͣ��������ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		}

		if (pump.cmd[0] != 0xBB || Check_xor(&pump.cmd[2], pump.cmdlen - 3) != pump.cmd[pump.cmdlen - 1]) {
			RunLog("Node[%d]FP[%d]��������У�����", node, pos + 1);
			continue;
		}
		if (pump.cmd[3] != 0x12) {
			RunLog("Node[%d]FP[%d]�������ݴ���", node, pos + 1);
			continue;
		}

		return 0;
	}

	return -1;
}


/*
 * func: �������ͻ�
 */
static int DoRecovery(int pos)
{
	int ret;
	int  trynum;

	RunLog( "Node %d FP %d DoRecovery", node , pos + 1);

	for( trynum = 0; trynum < TRY_TIME; trynum++ ) {
		pump.cmd[0] = 0xAA;
		pump.cmd[1] = 0x03;
		pump.cmd[2] = pump.panelPhy[pos];
		pump.cmd[3] = 0x13;
		pump.cmd[4] = pump.cmd[2] ^ pump.cmd[3];						
		pump.cmdlen = 5;
		pump.wantLen = 5;

		Ssleep(INTERVAL);
		ret = WriteToPort( pump.fd, pump.chnLog, pump.chnPhy,
				pump.cmd, pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT );
		if( ret < 0 ) {
			RunLog("Node[%d]FP[%d]�·��ָ���������ʧ��", node, pos + 1);
			continue;
		}
		memset( pump.cmd, 0, sizeof(pump.cmd) );
		pump.cmdlen = sizeof(pump.cmd)-1;
		gpGunShm ->addInf[pump.indtyp - 1][0] = 0x03; //�䳤��tty.c
		ret = GetReturnMsg( GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		gpGunShm ->addInf[pump.indtyp - 1][0] = 0x00;
		if( ret < 0 ) {
			RunLog( "Node[%d]FP[%d]�·��ָ���������ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		}

		if(pump.cmd[0] != 0xBB || (Check_xor(&pump.cmd[2], 2) != pump.cmd[4])) {
			RunLog("Node[%d]FP[%d]��������У�����", node, pos + 1);
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
	int i;
	int ret;
	__u8 status;
	int  trynum;

	RunLog("Node %d FP %d DoAuth", node, pos + 1);

	for (trynum = 0; trynum < TRY_TIME; trynum++) {
		pump.cmd[0] = 0xAA;
		pump.cmd[1] = 0x07;
		pump.cmd[2] = pump.panelPhy[pos];
		pump.cmd[3] = 0x16;
		pump.cmd[4] = 0x00;
		pump.cmd[5] = 0x00;
		pump.cmd[6] = 0x00;
		pump.cmd[7] = 0x00;
		pump.cmd[8] = Check_xor(&pump.cmd[2], pump.cmd[1] - 1);
		pump.cmdlen = 9;
		pump.wantLen= 9;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]�·���Ȩ����ʧ��", node, pos + 1);
			continue;
		}

		memset( pump.cmd, 0, sizeof(pump.cmd) );
		pump.cmdlen = sizeof(pump.cmd)-1;
		
		ret = GetReturnMsg( GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen );
		if( ret < 0 ) {
			RunLog("Node[%d]FP[%d]�·���Ȩ����ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		}
		   
		if (pump.cmd[0] != 0xBB || Check_xor(&pump.cmd[2], 6) != pump.cmd[8]) {
			RunLog("Node[%d]FP[%d]��������У�����", node, pos + 1);
			continue;
		}

		ret = GetPumpStatus(pos, &status);
		if (ret < 0) {
			continue;
		}
		
		if (status == PUMP_BUSY) {
			if (fp_state[pos] < 2) {
				DoStarted(pos);
			}
		}
		break;
	}

	if (trynum == TRY_TIME) {
		return -1;
	}

	return 0;
}



/*
 * func: �ϴ�ʵʱ��������
 */
static int DoBusy(int pos, __u8 status)
{
	int i, j;
	int ret;
	int trynum;
	static __u8 amount[5], volume[5];
	static __u8 arr_zero[3] = {0};

	for (trynum = 0; trynum < 2; trynum++) {
		if ((status & 0x03) == PUMP_CALL) {
			if ((__u8)FP_State_Fuelling == lpDispenser_data->fp_data[pos].FP_State) {
				ret = ChangeFP_Run_Trans(node, 0x21 + pos, amount,  volume);//ֻ��һ�����
			}
			return 0;
		}	
		
		volume[0] = 0x06;
		volume[1] = 0x00;
		memcpy(&volume[2],  &pump.cmd[7], 3);


		amount[0] = 0x06;
		amount[1] = 0x00;
		memcpy(&amount[2],  &pump.cmd[10], 3);

		if (fp_state[pos] != 3) {
			if (memcmp(arr_zero, &amount[2], 3) == 0) {
				return 0;
			}
			fp_state[pos] = 3;
			return 0;
	       }

              if ((__u8)FP_State_Fuelling == lpDispenser_data->fp_data[pos].FP_State) {
		    ret = ChangeFP_Run_Trans(node, 0x21 + pos, amount,  volume);//ֻ��һ�����
              }	

		return 0;
	}

	return -1;
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
		RunLog("Node[%d].FP[%d]��ȡ��ǰ��ǹǹ��ʧ��", node, pos + 1);
		return -1;
	}
       
	ret = ChangeFP_State(node, pos + 0x21, FP_State_Calling, NULL);

	//��λ������,���������Զ���Ȩ;��λ��������, ��Ȩ�������ڿ���״̬,��ʱ�Զ���Ȩ
	if ((gpIfsf_shm->auth_flag == FCC_SWITCH_AUTH) && (lock_status() == 1)) {
		ret = DoAuth(pos);
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]�Զ���Ȩʧ��", node, pos + 1);
		}

		ret = ChangeFP_State(node, pos + 0x21, FP_State_Started, NULL);
		RunLog("Node[%d]FP[%d]�Զ���Ȩ�ɹ�", node, pos + 1);
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
	*status = 0xFF;   // ����ʹ��00��Ϊ��ʼֵ, ���� PUMP_OFF ��ͻ

	for (i = 0; i < 2; ++i) {
		pump.cmd[0] = 0xAA;
		pump.cmd[1] = 0x03;
		pump.cmd[2] = pump.panelPhy[pos];
		pump.cmd[3] = 0x01;
		pump.cmd[4] = pump.cmd[2] ^ 0x01;
		pump.cmdlen = 5;
		pump.wantLen = 8;   

		Ssleep(INTERVAL); 
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]�·���״̬����ʧ��", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;

		gpGunShm ->addInf[pump.indtyp - 1][0] = 0x03; //�䳤��tty.c
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		gpGunShm ->addInf[pump.indtyp - 1][0] = 0x00;
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]�·���״̬����ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		} 

		if (pump.cmd[0] != 0xBB || Check_xor(&pump.cmd[2], pump.cmdlen - 3) != pump.cmd[pump.cmdlen - 1]) {
			RunLog("Node[%d]FP[%d]��������У�����", node, pos + 1);
			continue;
		}		

		*status = pump.cmd[4];
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
	*status = 0xFF;   // ����ʹ��00��Ϊ��ʼֵ, ���� PUMP_OFF ��ͻ

	for (i = 0; i < TRY_TIME; ++i) {
		pump.cmd[0] = 0xAA;
		pump.cmd[1] = 0x03;
		pump.cmd[2] = pump.panelPhy[pos];
		pump.cmd[3] = 0x01;
		pump.cmd[4] = pump.cmd[2]^0x01;
		pump.cmdlen = 5;
		pump.wantLen = 8;   

		Ssleep(INTERVAL); 
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]�·���״̬����ʧ��", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		gpGunShm ->addInf[pump.indtyp - 1][0] = 0x03; //�䳤��tty.c
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		gpGunShm ->addInf[pump.indtyp - 1][0] = 0x00;
		if (ret < 0) {
			RunLog("Node[%d]FP[%d]�·���״̬����ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		} 

		if (pump.cmd[0] != 0xBB || Check_xor(&pump.cmd[2], pump.cmdlen - 3) != pump.cmd[pump.cmdlen - 1]) {
			RunLog("Node[%d]FP[%d]��������У�����", node, pos + 1);
			continue;
		}	

		*status = pump.cmd[4];
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


/*
 * func: ��ȡ��ǰ̧����ǹ��ǹ��, ���д��pump.gunLog[]����ChangeLog_Noz_State
 * ��ǹ��
 * ret : 0 - �ɹ�, -1 - ʧ�� 
 */
static int GetCurrentLNZ(int pos)
{
	pump.gunLog[pos] = 1;
	RunLog("Node %d FP %d �� %d ��ņ̀��", node, pos + 1, pump.gunLog[pos]);
	lpDispenser_data->fp_data[pos].Log_Noz_State = 1 << pump.gunLog[pos] - 1;

	return 0;
}

/*
 * func: �趨�������ԡ�ͨѶ���ʡ�У��λ��
 */
static int SetPort003()
{
	int ret;

//	if( gpGunShm->chnLogSet[pump.chnLog-1] == 0 ) {
	pump.cmd[0] = 0x1C;
	pump.cmd[1] = 0x52;
	pump.cmd[2] = 0x30|(pump.chnPhy-1);
	pump.cmd[3] = 0x03;	//9600bps
	pump.cmd[4] = 0x01<< ((pump.chnPhy - 1) % 8);	//EVEN parity
	pump.cmd[5] = 0x00;
	pump.cmd[6] = 0x01<< ((pump.chnPhy - 1) % 8);
	pump.cmdlen = 7;
	pump.wantLen = 0;

	ret =  WriteToPort( pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			pump.cmdlen, 0, pump.wantLen, TIME_OUT_WRITE_PORT );
	if( ret < 0 ) {
		return -1;
	}
	gpGunShm->chnLogSet[pump.chnLog-1] = 1;
	Ssleep(INTERVAL);
//	}

	return 0;
}



/*
 * func:  ͨѶ��ͨʱ��ѯ�����и�һ��ʱ������һ�´�������
 *        ����������ĵ�ѹ���ȵ��µ�Ƭ����λ--�����������ûָ�Ԥ��9600,8,n,1
 */
static int ReDoSetPort003()
{
#define RE_DO_FREQUENCY   10         //  10����ѯʧ������һ�´�������

	int static polling_fail_cnt = 0;
	int ret;

	if ((++polling_fail_cnt) >= RE_DO_FREQUENCY) {
		ret = SetPort003();
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

	RunLog("SetSaleMode...");	

	for (i = 0; i < TRY_TIME; i++) {
		memset(pump.cmd,0,pump.cmdlen);
		pump.cmd[0] = 0xAA;
		pump.cmd[1] = 0x04;
		pump.cmd[2] = pump.panelPhy[pos];
		pump.cmd[3] = 0x11;				
		pump.cmd[4] = 0x04;
		pump.cmd[5] = pump.cmd[2] ^ pump.cmd[3] ^ pump.cmd[4];
		pump.cmdlen = 6;
		pump.wantLen = 6;

		ret = WriteToPort( pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT );
		if( ret < 0 ) {
			RunLog( "Node[%d]FP[%d]�·�������������ʧ��", node, pos + 1);
			continue;
		}
		
		ret = GetReturnMsg( GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if( ret < 0 ) {
			RunLog("Node[%d]FP[%d]�·�������������ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		} 

		if(pump.cmd[0] != 0xBB || Check_xor(&pump.cmd[2], 3) != pump.cmd[5]) {
			RunLog("Node[%d]FP[%d]��������У�����", node, pos + 1);
		}

		RunLog("Node[%d]FP[%d]��������ģʽ�ɹ�", node, pos + 1);
		return 0;
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

	ret = GetPumpStatus(pos, &status);
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
			node,pos+1,lnz+1, pump.cmd[5], pump.cmd[6],
		       	lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2], 
		       	lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3]);

		if ((pump.cmd[5] != lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2]) ||
			(pump.cmd[6] != lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3])) {
			return -2;
		}
	}
	
	ChangeFP_State(node, 0x21 + pos, FP_State_Closed, NULL);
	RunLog("Node[%d]FP[%d]�����ѱ����ȷ, �л�״̬ΪClosed", node, pos + 1);

	return 0;
}


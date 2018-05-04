/*
 * Filename: lischn021.c
 *
 * Description: ���ͻ�Э��ת��ģ�顪��������,2Line CL / RS485, 5760,E,8,1 
 *
 * History:
 *
 * 2008/02/25  - Guojie Zhu <zhugj@css-intelligent.com> 
 *     ���ڳ²�ʿ200802021830�汾�޸�
 *     ���� InitGunLog(), GetCurrentLNZ(), ChangePrice()
 *     ���� �ͻ������ж�, ����FP State Error�Ĵ���, ����Started״̬
 *     ɾ�� GetLogicGun(), ɾ���������
 *     �޸� DoPrice(), �������ΪDoPrice() �� ChangePrice(0 ������	
 *     ��������ϸ�ڵ��޸�(�ɶ���bak/lischn021.c�鿴)
 *
 * 2008/03/06  - Guojie Zhu <zhugj@css-intelligent.com> 
 *     ����δ֧�����ױ����Ƿ�ﵽ���Ƶ��ж�(ReachedUnPaidLimit)
 *     BeginTr4Fp������һ��DoBusy֮ǰ
 *     convertUsed[]��Ϊnode_online[]
 * 2008/03/30 - Guojie Zhu <zhugj@css-intelligent.com>
 *     �޸� DoCmd, �����жϻ����HandleStateErr
 *     �޸� AckFirst ����ʽ
 *     ��δ֧�����ױ����ﵽ�����жϷ���HandleStateErr
 * 2008/05/23 - Guojie Zhu <zhugj@css-intelligent.com>
 *     ������������Ϊ6λģʽ
 *
 * 010�����������ж�С��λ({ 0xff, 0xe9, 0xfe, 0xe0, 0xe1, 0xe0, 0xfb, 0xee, 0xf0 };)
 *�ظ�:// 1ņ̀�� ret: ba b0 b3  B1  b0 b1 b0 b0 b0  b0 b1 b0 B1 b1 B1  c3 c1 8d 8a
 *		// ��ņ̀��ret: ba b0 b3  B1  b0 b1 b0 b0 b0  b0 b1 b0 B0 b1 B0  c3 c3 8d 8a
 *�ϵĻ������Ǵ�0
 *ret[9] = 0
 *ret[9]Ϊ0 ��ʾ1ΪС�� ����Ҫ�жϰٷ�λС��
 *
 *�»�����1
 *ret[9] = 1				                                           1ǹ     2ǹ     3ǹ     4ǹ
 *��Ҫȡ010���� �жϰٷ�λС��λ ret[10] ret[11] ret[12] ret[13] 
 *��ȡ����ʱ��Ҫ����010���� �жϰٷ�λС�� ע���������ж�
 *
 *�»�����2
 *ret[9] = 2
 *�����ȡ������λС����û����ǰ�İ���λ
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

#define GUN_EXTEND_LEN  8

// FP State
#define PUMP_ERROR	0x00
#define PUMP_OFF	0x06
#define PUMP_CALL	0x07
#define PUMP_AUTH	0x08
#define PUMP_BUSY	0x09
#define PUMP_PEOT	0x0A
#define PUMP_FEOT	0x0B
#define PUMP_STOP	0x0C

#define TRY_TIME	5
#define INTERVAL	10000    // ����ͼ��, ��ֵ�Ӿ����ͻ�����, 10ms
#define INTERVAL_TOTAL	75000    // ����ͼ��, ��ֵ�Ӿ����ͻ�����, 10ms


//�泤���ٷ�λ����
typedef struct{
	unsigned char lnzOne;
	unsigned char lnzTwo;
	unsigned char lnzThr;
	unsigned char lnzFou;
}GILPERCENTILE; 

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
static int ChangePrice();
static int GetGunTotal(int pos, __u8 gunLog, __u8 *vol_total, __u8 *amount_total);
static int GetGunPrice(int pos, __u8 gunLog, __u8 *price);
static int GetTransData(int pos);
static int GetPumpStatus(int pos, __u8 *status);
static int GetPumpOnline(int pos, __u8 *status);
static int CancelRation(int pos);
static int InitPumpTotal();
static int InitCurrentPrice();
static int InitGunLog();
static int GetCurrentLNZ(int pos);
static int keep_in_touch_xs(int x_seconds);
static int SetPort001();
static int ReDoSetPort001();
static int DoRation_Idle(int pos);
static int CheckPriceErr(int pos);
static int GetPumpPoint();
static int GetPumpPercentile(const int pos);


static PUMP pump;
static int point_flag = 0; /*��������С����λ���
					0 :һλС����  
					1 :һλС���㣬�ٷ�λС����010������
					2 :��λС���㣬һ����������͹���
					*/



static GILPERCENTILE GilPercentile[MAX_FP_CNT];
					
static int node;
static int IFSFpos;
static DISPENSER_DATA *lpDispenser_data;
static NODE_CHANNEL *lpNode_channel;
static int fp_state[MAX_FP_CNT];   // 0 - IDLE, 1 - CALLING , 2 - DoStarted�ɹ�, 3 - FUELLING
static int dostop_flag[MAX_FP_CNT] = {0};
static __u8 ration_flag[MAX_FP_CNT] = {0};          // ��ʶ�Ƿ���Ԥ��Ҫ�·�
static __u8 ration_type[MAX_FP_CNT] = {0};          // ��ʶҪ�·�Ԥ�õ�����
static __u8 ration_value[MAX_FP_CNT][3] = {{0,0}};  // ����Ԥ����
static __u8 ration_noz_mask[MAX_FP_CNT] = {0};      // ��ʶԤ������ǹ��

int ListenChn001(int chnLog)
{
	long	indtyp;
	int	ret;
	int	fd;
	int	j;
	int	i,icount;
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
						// 1 -- Pump ID 1, ...... ,0 -- Pump ID 16
						pump.panelPhy[j] = (gpIfsf_shm->gun_config[idx].phyfp) % 16;
						break;
					}
				}
				pump.gunCnt[j] =  gpIfsf_shm->node_channel[i].cntOfFpNoz[j];
				// ������ʶ�Ƿ�̧ǹ���һ��ȡʵʱ��������
				// �����ͻ���Fuelling״̬��һ��ȡʵʱ����ʱ,Ҫ��ȵ�ʱ��ϳ�.����addinf[0]��һ����ʶ
				pump.addInf[j][0] = 0;	      // Note. ��;�Ӿ�����Ͷ���
			// ������ʶ�Ƿ��busy״ֱ̬��ת��Ϊ��call״̬.busyʱ��1,off��eotʱ�ָ�Ϊ0.���callʱΪ1,��ȡ���ͽ��
				pump.addInf[j][1] = 0;	      // Note. ��;�Ӿ�����Ͷ���
			}
				
			break;
		}
	}
	pump.indtyp = chnLog;
	bzero(&pump.newPrice, FP_PR_NB_CNT * 4);
	
#if 0
	show_dispenser(node); 
	show_pump(&pump);
#endif

	// ============= Part.2 �� & ���ô������� -- ������/У��λ�� ===================

	strcpy(pump.cmd, alltty[pump.port - 1]);
	pump.fd = OpenSerial(pump.cmd);
	if (pump.fd < 0) {
		RunLog("�ڵ�[%d], �򿪴���[%s]ʧ��", node, pump.cmd);
		return -1;
	}
//	RunLog("�ڵ�[%d]�򿪴���[%s]�ɹ�",node, pump.cmd);

	ret = SetPort001();
	if (ret < 0) {
		RunLog("�ڵ�[%d], ���ô�������ʧ��",node);
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
		UserLog("<����> �ڵ��[%d] , ������", node);
		
		sleep(1);       // �̶���ѯ���1s
	}

	//��������ͻ��ϵ������ʼ������ʧ�ܣ�����ͻ�ͨѶ����������
	for (icount = 0; icount < pump.panelCnt; icount++) {
		ret = GetPumpOnline(icount, &status);
	}	

	
	// ============= Part.4 ��ʼ������ͻ���ǰ״̬��ص����ݿ������ =======================
#if 0
	// commented by jie @ 2009.4.23 , �Ѿ�������Ҫ
	ret = InitGunLog();    // Note.InitGunLog ������ÿ��Ʒ���ͻ�������,�����ͻ�Ԥ�ü�����Ҫ
	if (ret < 0) {
		RunLog("�ڵ�[%d], ��ʼ��Ĭ�ϵ�ǰ�߼�ǹ��ʧ��",node);
		return -1;
	}
#endif

	/*��ȡ����С��λ��*/
	ret = GetPumpPoint();
	if (ret < 0) {
		RunLog("�ڵ�[%d], ��ȡ����С��λʧ��!Ĭ��һλС��",node);
		point_flag = 0;
	}

	ret = InitCurrentPrice(); //  ��ʼ��fp���ݿ�ĵ�ǰ�ͼ�
	if (ret < 0) {
		RunLog("�ڵ�[%d], ��ʼ����Ʒ���۵����ݿ�ʧ��",node);
		return -1;
	}

	ret = InitPumpTotal();    // ��ʼ�����뵽���ݿ�
	if (ret < 0) {
		RunLog("�ڵ�[%d], ��ʼ�����뵽���ݿ�ʧ��",node);
		return -1;
	}

	


	// һ�о���, ����
//	gpIfsf_shm->node_online[IFSFpos] = node;
	ChangeNode_Status(node, NODE_ONLINE);
	UserLog("�ڵ�[%d]����,״̬[%02x]!", node, status);
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
			Ssleep(500000);       // ������ѯ���Ϊ100ms + INTERVAL
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
			/*
			 * Note!!! pos ���ܸ�ֵΪ255,
			 * ��Ϊ��FP��ص������Сֻ��4Bytes, ����255��Խ��
			 * ����Ժ�ȷʵҪ֧�����ȫ��FP�Ĳ���, ��ô��Ҫ����޸���ش���;
			 * Ŀǰ��״̬ʹ��pos=255��ģʽ�϶��������
			 *  Guojie Zhu @ 2009.03.18
			 */
			pos = 255;
		} else if ((fp_id >= 0x21) && (fp_id <= 0x24)) {
			pos = fp_id - 0x21;
		} else {
			if (pump.cmd[0] != 0x18 && pump.cmd[0] != 0x19) {  // DoPrice & DoMode����Ҫ��������
				RunLog("�ڵ�[%d] , �Ƿ�������ַ",node);
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
			ret = 0;  // ̧ǹ״ִ̬��ֹͣ��ʧ��, ����ֱ�Ӳ�ִ��, ��ת��FP״̬
		} else {
			ret = DoStop(pos);
		}

		if (ret == 0) {
			ration_flag[pos] = 0;           //Ԥ�ñ�־��ΪδԤ��0
			ration_type[pos] = 0;
			memset(&ration_value[pos], 0, 3);
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
			//lpDispenser_data->fp_data[pos].Log_Noz_Mask = 0xFF;
			bzero(&lpDispenser_data->fp_data[pos].Remote_Volume_Preset, 5);
			bzero(&lpDispenser_data->fp_data[pos].Remote_Amount_Prepay, 5);
			break;
		}
		RunLog("HandleStateErr OK, go to PreSet:");


		/* Guojie Zhu @ 2009.7.19
		 * ��Ϊ����ʱ�·�Ԥ����Ϊ�ͻ���������������Ҫ�������β��ܱ���Ԥ��ʧ��,
		 * ���������ᵼ��Ԥ�ô���ʱ�����, ���Գ��Ը�Ϊ̧ǹ���·�
		 *
		 */
		/*
		 * ̧ǹ֮���·�Ԥ����ʹ�õı�Ǳ����ͻ�����:
		 * ration_flag[pos]: Ԥ�ñ��, ��ʶ�Ƿ�̧ǹ���Ƿ���Ԥ����Ҫ�·�
		 * ration_type[pos]: Ԥ�÷�ʽ����ֽ�
		 * ration_value[pos]:  Ԥ��������
		 * ration_noz_mask[pos]: Ԥ��������ǹ, 0xFF - ����ȫ��
		 */
		ration_flag[pos] = 1;            // Ԥ�ñ��, �����Ԥ����Ҫ�·�
		ration_type[pos] = pump.cmd[0];  // Ԥ�÷�ʽ
		ration_noz_mask[pos] = lpDispenser_data->fp_data[pos].Log_Noz_Mask;
		RunLog("ration_noz_mask:%02x", ration_noz_mask[pos]);

		if (pump.cmd[0] == 0x3D) {
			memcpy(&ration_value[pos], 
				&lpDispenser_data->fp_data[pos].Remote_Volume_Preset[2], 3);
			ret = 0;
		} else if (pump.cmd[0] == 0x4D) {
			memcpy(&ration_value[pos], 
				&lpDispenser_data->fp_data[pos].Remote_Amount_Prepay[1], 3); //MianDian, Amount is big, 06YYYYYY.00
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
		RunLog("�ڵ�[%d], ���ܵ�δ��������[0x%02x]",node, pump.cmd[0]);
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
	__u8 status;
	int tmp_lnz;
	int pump_err_cnt[MAX_FP_CNT] = {0};
	static __u8 pre_status[MAX_FP_CNT] = {0};
	static int times = 0;
	static int fail_cnt = 0;  // ��¼GetPumpStatus ʧ�ܴ���, �����ж�����
	int trynum;
	
	for (i = 0; i < pump.panelCnt; i++) {
		ret = GetPumpStatus(i, &status);
		if (ret < 0) {
			if (gpIfsf_shm->node_online[IFSFpos] == node) {
				if ((++fail_cnt) < MAX_FAIL_CNT) {  // ���ͻ������ж�
					/*
					 * 2008.12.29���Ӵ˲���, ������ѹ�������
					 * ��Ƭ����λ�󴮿����ø�λ���յ���ͨѶ��ͨ������
					 */
					/*
					 * �����贮�����Իᵼ��ReadTTY���յ�"�Ƿ�����", ����ʱ�Ƴ�
					 * 2009.02.19
					if (fail_cnt >= (MAX_FAIL_CNT / 2)) {
						if (SetPort001() >= 0) {
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
				RunLog("======�ڵ�[%d] ����! ===========================", node);

				// Step.3
				for (j = 0; j < pump.panelCnt; ++j) {
					lpDispenser_data->fp_data[j].FP_State = FP_State_Inoperative;
					lpDispenser_data->fp_data[j].Log_Noz_State = 0;
				}
			}

			// ���贮������
			//ReDoSetPort021(); 2009.02.19
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

				/*��ȡ����С��λ��*/
				ret = GetPumpPoint();
				if (ret < 0) {
					RunLog("�ڵ�[%d], ��ȡ����С��λʧ��!Ĭ��һλС��",node);
					point_flag = 0;
				}

				// Step.1
				ret = InitPumpTotal();    // ��ʼ�����뵽���ݿ�
				if (ret < 0) {
					RunLog("�ڵ�[%d], ���±��뵽���ݿ�ʧ��",node);
					continue;
				}

				// Step.2
				for (j = 0; j < pump.panelCnt; ++j) {
					lpDispenser_data->fp_data[j].FP_State = FP_State_Closed;
				}

				// Step.3
//				gpIfsf_shm->node_online[IFSFpos] = node;
				ChangeNode_Status(node, NODE_ONLINE);
				RunLog("====== �ڵ�[%d] �ָ�����! =======================", node);

				
				
				bzero(ration_flag,sizeof(ration_flag));     //�ظ����ߺ��������Ԥ�ñ�־��Ϊ0
				bzero(ration_type,sizeof(ration_type));
				keep_in_touch_xs(12);               // ȷ��CD�Ѿ��յ�����֮��������������
				// Step.4                 // �����ϴ���ǰFP״̬
				for (j = 0; j < pump.panelCnt; ++j) {
					ChangeFP_State(node, j + 0x21, FP_State_Closed, NULL);
				}
				continue;
			}

			fail_cnt = 0;
			if (status != PUMP_ERROR) {
				pump_err_cnt[i] = 0;
			}
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
					RunLog("�ڵ�[%d]���[%d] ����Closed״̬", node, i + 1);
				}

				// Closed ״̬��ҲҪ�ܹ����
				for (j = 0; j < pump.panelCnt; ++j) {
					if ((__u8)FP_State_Idle <  lpDispenser_data->fp_data[j].FP_State) {
						break;
					}
				}
				// �����涼���вű��
				if (j >= pump.panelCnt) {
					ret = ChangePrice();            // �鿴�Ƿ�����Ʒ��Ҫ���,����ִ��
				}
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
			RunLog("�ڵ�[%d]���[%d] ����Inoperative״̬!", node, i + 1);
			// Inoperative ״̬��ҲҪ�ܹ����
			for (j = 0; j < pump.panelCnt; ++j) {
				if ((__u8)FP_State_Idle <  lpDispenser_data->fp_data[j].FP_State) {
					break;
				}
			}
			// �����涼���вű��
			if (j >= pump.panelCnt) {
				ret = ChangePrice();            // �鿴�Ƿ�����Ʒ��Ҫ���,����ִ��
			}

			CheckPriceErr(i);
			continue;
		}

		switch (status) {
		case PUMP_ERROR:				// Do nothing
			break;
		case PUMP_OFF:					// ��ǹ������
			if (ration_flag[i] != 0) {              // ��Ԥ�ô��·�
				// �����ǹ״̬, ����̧��ǹ�����������̧ǹDoStarted�޷���ȡǹ��
				lpDispenser_data->fp_data[i].Log_Noz_State = 0;
				break; 	
			}
					 
			dostop_flag[i] = 0;
			pump.addInf[i][0] = 0;

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

			if ((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State) {
				ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
			}

			EO_OFFLINE_TR(node, i);
			if (gpIfsf_shm->fp_offline_flag[node - 1][i] != 0) {
				RunLog("Dopolling, call upload_offline_tr");
				upload_offline_tr(node, NULL);
			}

			// �е���������Ȩ״̬, ���踽���ж�״̬
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
		case PUMP_CALL:					// ̧ǹ
			RunLog("FP_State: %02x", lpDispenser_data->fp_data[i].FP_State);
			// 1. PAK Calling
			if ((__u8)FP_State_Calling > lpDispenser_data->fp_data[i].FP_State) {
				fp_state[i] = 1;

				if (DoCall(i) < 0) {
					break;
				}

				if (DoStarted(i) < 0) {
					break;
				}
			}
			//��ǹ�ս���֮ǰ��Ȩ��������Ϊ0���ͻ������ƣ��������⴦�� 
			if (ration_flag[i] != 0 && ration_value[i][0] == 0 && ration_value[i][1] == 0 && ration_value[i][2] == 0) {
				RunLog("��ǹ������Ԥ��Ȩ���ٴ�̧ǹԤ��Ȩȡ��");
				ration_flag[i] = 0;          // ��ʶ�Ƿ���Ԥ��Ҫ�·�
				ration_type[i] = 0;          // ��ʶҪ�·�Ԥ�õ�����
				ration_noz_mask[i] = 0;      // ��ʶԤ������ǹ��
				lpDispenser_data->fp_data[i].Log_Noz_Mask = 0xFF;
				break;
			}
			
			if (ration_flag[i] != 0) {           // ��Ԥ����δ�·�
				fp_state[i] = 1;

				if (DoStarted(i) < 0) {
					break;
				}
				/* Note! DoStarted Ҫ��DoRation֮ǰִ��,
				 * DoRation��Ҫ��ǰ̧ǹ��ǹ��, ��Ҫ��̧ǹ״̬�·�Ԥ����
				 */

				// �ж�̧����Ƿ�����ҪԤ�õ���ǹ, ������������
				if ((lpDispenser_data->fp_data[i].Log_Noz_State !=
								ration_noz_mask[i]) && 
								(ration_noz_mask[i] != 0xFF)) {
					RunLog("Log_Noz_State:%02x != ration_noz_mask:%02x", lpDispenser_data->fp_data[i].Log_Noz_State, ration_noz_mask[i]);
					break;
				}

				ration_flag[i] = 0;
				ret = DoRation(i);
				if (ret == 0) {
					ChangeFP_State(node, i + 0x21, FP_State_Started, NULL);	
				} else if (ret == 1) {
					ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);	
				}
			
				// Ԥ�ô������, �ظ�����ȫ����ǹ����
				lpDispenser_data->fp_data[i].Log_Noz_Mask = 0xFF;
				break;
			}

			break;
		case PUMP_AUTH:
			// ������PAK����Ԥ��, ����̧ǹ����Ȩ, ����PUMP_AUTH״̬ʱһ����FP_Started
			if ((__u8)FP_State_Started > lpDispenser_data->fp_data[i].FP_State) {
				ret = ChangeFP_State(node, i + 0x21, FP_State_Started, NULL);
			}
		case PUMP_BUSY:					// ����
			// ���䴦��
			if (fp_state[i] < 2) {
				DoStarted(i);
			}

			// ���䴦��
			if ((__u8)FP_State_Started > lpDispenser_data->fp_data[i].FP_State) {
				ret = ChangeFP_State(node, i + 0x21, FP_State_Started, NULL);
			}

			//DO_INTERVAL(DO_BUSY_INTERVAL,  ret = DoBusy(i););
			DoBusy(i);

			if ((__u8)FP_State_Fuelling > lpDispenser_data->fp_data[i].FP_State) {
				if (fp_state[i] == 3) {
					ChangeFP_State(node, i + 0x21, FP_State_Fuelling, NULL);
				}
			}

			break;
		case PUMP_STOP:					// ��ͣ���� 
			if ((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State) {
				ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
			}
			break;
		case PUMP_PEOT:
		case PUMP_FEOT:					// ���ͽ���
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
		RunLog("�ڵ�[%d] ����״̬�ı����",node);
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

	if ((lpDispenser_data->fp_data[i].Log_Noz_State == 0) ) {
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
 * return: -1 - ʧ��; 0 - �ɹ�; 1 - �Ѿ���ǹ, Ԥ��ȡ��
 */
static int DoRation(int pos)  
{
#define  PRE_VOL  0xF1
#define  PRE_AMO  0xF2
	int ret;
	int trynum;
	__u8 status;
	__u8 volume[3];
	__u8 amount[6];
	__u8 preset_type;
	__u8 lnz_allowed = 0;      // �ͻ�ǹ�Ŵ�0��ʼ

	if (lpDispenser_data->fp_data[pos].Log_Noz_Mask == 0) {
	//if (ration_noz_mask[pos] == 0) {
		RunLog("Node[%d]FP[%d]δ�����κ���ǹ����", node, pos + 1);
		return -1;
	}

	// Step.0 �������ǹ��
	for (ret = 0x01; ret <= 0x80; ret = (ret << 1), lnz_allowed++) {
		if ((lpDispenser_data->fp_data[pos].Log_Noz_Mask & ret) != 0) {
		//if ((ration_noz_mask[pos] & ret) != 0) {
			break;
		}
	}

	// ����������
	if (ration_type[pos] == 0x3D) { // MianDian volume have 3 point, amount have 0 point
		preset_type = PRE_VOL;
		
	amount[0] = 0xe0 | (ration_value[pos][2] & 0x0f);
	amount[1] = 0xe0 | (ration_value[pos][2] >> 4);
	amount[2] = 0xe0 | (ration_value[pos][1] & 0x0f);
	amount[3] = 0xe0 | (ration_value[pos][1] >> 4);
	amount[4] = 0xe0 | (ration_value[pos][0] & 0x0f);
	amount[5] = 0xe0 | (ration_value[pos][0] >> 4);
	RunLog("amount[0-5],PRE_Volume: %02x%02x%02x%02x%02x%02x",amount[0],amount[1],amount[2],amount[3],amount[4],amount[5]);
	} else {
		preset_type = PRE_AMO;
	amount[0] = 0xe0 | (ration_value[pos][2] & 0x0f);
	amount[1] = 0xe0 | (ration_value[pos][2] >> 4);
	amount[2] = 0xe0 | (ration_value[pos][1] & 0x0f);
	amount[3] = 0xe0 | (ration_value[pos][1] >> 4);
	amount[4] = 0xe0 | (ration_value[pos][0] & 0x0f);
	amount[5] = 0xe0 | (ration_value[pos][0] >> 4);
	RunLog("amount[0-5],PRE_Amount: %02x%02x%02x%02x%02x%02x",amount[0],amount[1],amount[2],amount[3],amount[4],amount[5]);
	}


	/*
	 * �·�Ԥ��֮ǰ�ж�һ�µ���ô״̬�Ƿ���̧ǹ, ����Ѿ���ǹ��ô��ȡ�����Ԥ��,
 	 * ����̧ǹ��Ԥ����δ�·����Ѿ���ǹ, ����̧��ǹ�޷�ȡ��Ԥ��. Guojie Zhu @ 2009.8.6
	 */
	ret = GetPumpStatus(pos, &status);
	if (ret < 0) {
		return -1;
	} else if (status != PUMP_CALL) {
		RunLog("Node[%d].FP[%d]��ǰ״̬����Ԥ��", node, pos + 1);
		return 1;
	}


	for (trynum = 0; trynum < TRY_TIME; trynum++) {
		// Step.1.1 ����Ԥ������
		pump.cmd[0] = 0x20 | pump.panelPhy[pos];
		pump.cmdlen = 1;
		pump.wantLen = 1;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d], дԤ�ü�������ʧ��", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd)-1;

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);

		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d], дԤ�ü�������ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			Ssleep(2 * INTERVAL);
			continue;
		} else if (pump.cmd[0] != (0xD0 | pump.panelPhy[pos])) { 
			Ssleep(2000000);            // 2x������Ҫ��Ϊ2s
			continue;
		}							
		RunLog("�ڵ�[%d]���[%d] Step.1 дԤ�ü�������ɹ�", node, pos + 1);
		
		// Step.1.2 ����Ԥ����
		pump.cmd[0] = 0xff;
		pump.cmd[1] = 0xe3;
		pump.cmd[2] = preset_type;
		pump.cmd[3] = 0xf4;      // Price Level 1

		if ((lpDispenser_data->fp_data[pos].Log_Noz_Mask != 0xFF) ||(preset_type == PRE_VOL) ||(lpNode_channel->cntOfFpNoz[pos] == 1)) { 
			// ֻ������Ԥ���ܹ�ָ����ǹ
			//(lpNode_channel->cntOfFpNoz[pos] == 1)) { 
			//���������������
			pump.cmd[4] = 0xf6;      // Grade data next
			pump.cmd[5] = 0xe0 | lnz_allowed;
			pump.cmd[6] = 0xf8;
			pump.cmd[7] = amount[0];
			pump.cmd[8] = amount[1];
			pump.cmd[9] = amount[2];
			pump.cmd[10] = amount[3];
			pump.cmd[11] = amount[4];		
			pump.cmd[12] = amount[5];		
			pump.cmd[13] = 0xfb;
			pump.cmd[14] = 0xe0 | (LRC(pump.cmd, 14) & 0x0f);
			pump.cmd[15] = 0xf0;
			pump.cmdlen = 16;
			pump.wantLen = 0;
		} else { //0xFF ������λ���·�Ԥ������ʱû��ָ����ǹ, add by liys @ 2009.02.06
			pump.cmd[4] = 0xf8;
			pump.cmd[5] = amount[0];
			pump.cmd[6] = amount[1];
			pump.cmd[7] = amount[2];
			pump.cmd[8] = amount[3];
			pump.cmd[9] = amount[4];		
			pump.cmd[10] = amount[5];		
			pump.cmd[11] = 0xfb;
			pump.cmd[12] = 0xe0 | (LRC(pump.cmd, 12) & 0x0f);
			pump.cmd[13] = 0xf0;
			pump.cmdlen = 14;
			pump.wantLen = 0;		   	
		}	


		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d], дԤ�ü�����ʧ��", node, pos + 1);
			// WriteToPort ʧ���Ѿ���ʱ2��, ���Բ�����Ҫ������ʱ��continue
			continue;
		}
              
		RunLog("�ڵ�[%d]���[%d] Step.2 дԤ�ü������ɹ�[%s]",
			node, pos + 1, Asc2Hex(pump.cmd, pump.cmdlen));
		
		// Step.2 ��Ȩ����
		//���ݳ���Ҫ��������ʱ���Ա���ͻ����ü���Ԥ������������, Added by liys @2009.02.06
		Ssleep(100000);  // 100ms
		ret = DoAuth(pos);
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d]Ԥ�ü�����Ȩʧ��!", node, pos + 1);
			continue;				
		}			
			  
		return 0; 
	}


	return -1;
}

/*
 * func: ͨ���·�Ԥ����0000.00�ķ�ʽȡ������
 */
static int CancelRation(int pos)
{
	int ret;
	int trynum;

	RunLog("Node[%d].FP[%d] Do CancelRation", node, pos + 1);

	for (trynum = 0; trynum < TRY_TIME; trynum++) {
		// Step.1.1 ����Ԥ������
		pump.cmd[0] = 0x20 | pump.panelPhy[pos];
		pump.cmdlen = 1;
		pump.wantLen = 1;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d]��ȡ��Ԥ�ü�������ʧ��", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd)-1;

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);

		if  (pump.cmd[0] != (0xD0 | pump.panelPhy[pos])) { 
			RunLog("�ڵ�[%d]���[%d]дȡ��Ԥ�ü�������ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			sleep(2);
			continue;
		}							
		RunLog("�ڵ�[%d]���[%d] Step.1.1 дȡ��Ԥ�ü�������ɹ�", node, pos + 1);
		
		// Step.1.2 ����Ԥ����
		pump.cmd[0] = 0xff;
		pump.cmd[1] = 0xe3;
		pump.cmd[2] = 0xf2;
		pump.cmd[3] = 0xf4;      // Price Level 1
		pump.cmd[4] = 0xf8;
		pump.cmd[5] = 0xe0;
		pump.cmd[6] = 0xe0;
		pump.cmd[7] = 0xe0;
		pump.cmd[8] = 0xe0;
		pump.cmd[9] = 0xe0;
		pump.cmd[10] = 0xe0;
		pump.cmd[11] = 0xfb;
		pump.cmd[12] = 0xe0 | (LRC(pump.cmd, 12) & 0x0f);
		pump.cmd[13] = 0xf0;
		pump.cmdlen = 14;
		pump.wantLen = 0;		   	

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("��ȡ��Ԥ�������ڵ�[%d]���[%d]ʧ��", node, pos + 1);
			continue;
		}
              
		RunLog("�ڵ�[%d]���[%d]дȡ��Ԥ�ü������ɹ�[%s]",
			node, pos + 1, Asc2Hex(pump.cmd, pump.cmdlen));
		
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
	__u8 times_print;
	static __u8 times = 0;
	
	pr_no  = pump.cmd[2] - 0x41; //��Ʒ��ַ����Ʒ���0,1,...7.
	if ((0x04 != pump.cmd[8] && 0x06 != pump.cmd[8]) || pr_no > 7) {
		RunLog("�����Ϣ����,�����·�, pump.cmd[8] = %d, pr_no = %d", pump.cmd[8], pr_no);
		return -1;
	}
	
	if ((pump.newPrice[pr_no][1] | pump.newPrice[pr_no][2] | pump.newPrice[pr_no][3]) == 0) {
		// �ϴ�ִ�б��(ChangePrice)���һ���յ�WriteFM�����ı������
		//pump.newPrice[pr_no][0] = 1;
		times_print = 1;
	} else {
		//pump.newPrice[pr_no][0]++;
		++times;
		times_print = times;
	}

	// �����Ƿ��������յ�ͬ����Ʒ�ı������, Ҳ���ܵ����Ƿ�һ��, ��ʹ���µ��۸����ϵ���
	pump.newPrice[pr_no][0] = pump.cmd[8]; // add by weihp, for MianDian
	pump.newPrice[pr_no][1] = pump.cmd[9];
	pump.newPrice[pr_no][2] = pump.cmd[10];
	pump.newPrice[pr_no][3] = pump.cmd[11];
#ifdef DEBUG
	RunLog("Recv New Price-PR_ID[%d]TIMES[%d] %02x%02x%02x",
		pr_no + 1, times_print, pump.newPrice[pr_no][1], pump.newPrice[pr_no][2], pump.newPrice[pr_no][3]);
#endif

	return 0;
}


/*
 * func: ִ�б��, ʧ�ܸı�FP״̬ΪINOPERATIVE, ��д�������ݿ�
 */
static int ChangePrice() 
{
	int i, j;
	int ret, fm_idx, trynum;
	__u8 price[4] = {0};
	__u8 tmp_price[4] = {0};
	__u8 pr_no;
	__u8 lnz;
	__u8 err_code;
	__u8 err_code_tmp;
	__u8 status;

/*
 * �����µ�������newPrice[FP_PR_NB_CNT], �������Ʒ��Ҫ���--����ֵ��Ϊ��,
 * ���������FP, �����Ʒƥ����·��������
 */

//	RunLog(">> exec ChangePrice Now !");
	for (pr_no = 0; pr_no < FP_PR_NB_CNT; pr_no++) {
//		RunLog(">>>1 newPrice[%d]: %02x.%02x", pr_no, pump.newPrice[pr_no][2], pump.newPrice[pr_no][3]);;
		if (pump.newPrice[pr_no][1] != 0x00 || pump.newPrice[pr_no][2] != 0x00 || pump.newPrice[pr_no][3] != 0x00) {
			for (i = 0; i < pump.panelCnt; i++) {
				//1.##ChangePrice 
				err_code = 0x99;
				for (j = 0; j < MAX_FP_LN_CNT; j++) { // ������gunCnt,1ǹ���ܲ���
#if DEBUG
					RunLog(">>> fp_ln_data[%d][%d].PR_Id: %d", i, j,
							lpDispenser_data->fp_ln_data[i][j].PR_Id);
#endif
					if (lpDispenser_data->fp_ln_data[i][j].PR_Id - 1 == pr_no) {
						//2.##ChangePrice 
						err_code_tmp = 0x99;
				CriticalLog("�·����������ڵ�[%02d]��[%d]ǹ[%d]......", node, i + 1, j + 1);

					//price is little endian mode, so 1 to 0
					if (0x04 == pump.newPrice[pr_no][0]) {
						price[0] = 0xe0 | (pump.newPrice[pr_no][2] & 0x0f);
						price[1] = 0xe0 | (pump.newPrice[pr_no][2] >> 4);
						price[2] = 0xe0 | (pump.newPrice[pr_no][1] & 0x0f);
						price[3] = 0xe0 | (pump.newPrice[pr_no][1] >> 4);
					} else if (0x06 == pump.newPrice[pr_no][0]) {
						price[0] = 0xe0 | (pump.newPrice[pr_no][3] & 0x0f);
						price[1] = 0xe0 | (pump.newPrice[pr_no][3] >> 4);
						price[2] = 0xe0 | (pump.newPrice[pr_no][2] & 0x0f);
						price[3] = 0xe0 | (pump.newPrice[pr_no][2] >> 4);
					}

					for (trynum = 0; trynum < TRY_TIME; trynum++) {
                                            // Step.1.1 ���������;
						Ssleep(INTERVAL);
						pump.cmd[0] = 0x20 | pump.panelPhy[i];
						pump.cmdlen = 1;
						pump.wantLen = 1;

						ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd, 
								pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
						if (ret < 0) {
							RunLog("�ڵ�[%d]���[%d], �·��������ʧ��",node ,i+1);
							continue;
						}

						memset(pump.cmd, 0, sizeof(pump.cmd));
						pump.cmdlen = sizeof(pump.cmd) - 1;
						ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);

						if (pump.cmd[0] != (0xD0 | pump.panelPhy[i])) {
							RunLog("�ڵ�[%d]���[%d]ȡ�ظ���Ϣʧ��",node,i+1);
							continue;
						}
						
						// Setp.1.2 ����������
						pump.cmd[0] = 0xff;
						pump.cmd[1] = 0xe5;
						pump.cmd[2] = 0xf4;
						pump.cmd[3] = 0xf6;
						pump.cmd[4] = 0xe0 | j; // j == lnz - 1;
						pump.cmd[5] = 0xf7;
						pump.cmd[6] = price[0];
						pump.cmd[7] = price[1];
						pump.cmd[8] = price[2];
						pump.cmd[9] = price[3];
						pump.cmd[10] = 0xfb;
						pump.cmd[11] = 0xe0 | (LRC(pump.cmd, 11) & 0x0f);
						pump.cmd[12] = 0xf0;
						pump.cmdlen = 13;
						pump.wantLen = 0;

						Ssleep(INTERVAL*5);
						ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
								pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
						if (ret < 0) {
							RunLog("�ڵ�[%d]���[%d]�·��������ʧ��",node,i+1);
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL * 6);
							continue;
						}


						RunLog("Node[%d].FP[%d].PriceChange_CMD: %s",
							node, i + 1, Asc2Hex(pump.cmd, pump.cmdlen));
										  
						// Step.2 �����������Ϣ�����ض��ͻ�����ȷ�ϱ���Ƿ�ɹ�
						
						// ����˰��æ���´�����ʱ, ������ʱ1s�ٶ�����
						sleep(1);

						ret = GetGunPrice(i, j + 1, tmp_price);
						if (ret < 0) {
							/* ���GetGunPriceʧ��,
							 * ��ô��ʱʱ��һ������2s, ����ֱ�� continue;
							 */
							continue;
						}
					      
						if (memcmp(tmp_price, price, 4) != 0) {
							RunLog("�ڵ�[%d]���[%d]�·��������ɹ�,ִ��ʧ��", node, i + 1);
							err_code_tmp = 0x37; // ���ִ��ʧ��
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - 1000000);
							continue;
						}
						 
						// Step.3  ���㵥�ۻ���
						
						fm_idx = lpDispenser_data->fp_data[i].Default_Fuelling_Mode - 1;

					RunLog("�ڵ�[%d]���[%d]���ͱ������ɹ�,�µ���fp_fm_data[%d][%d].Prode_Price: %s",
							node, i + 1, pr_no, fm_idx,
							Asc2Hex(lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price, 4));

					CriticalLog("�ڵ�[%02d]��[%d]ǹ[%d]��۳ɹ�,�µ���Ϊ%02x.%02x", node, i + 1, j + 1,
							lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2],
							lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3]);

						err_code_tmp = 0x99;
						break;
					}

					// Step.4 �����ж�, ��������д�������ݿ�/��FP״̬Ϊ Inoperative
					if (trynum >= TRY_TIME) {
						// Write Error Code: 0x36/0x37 - Price Change Error (Spare)
						//5.##ChangePrice 
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
					//6.##ChangePrice 
					if (err_code == 0x99) {
						// �������κ���ǹ���ʧ��, ��������FP���ʧ��.
						err_code = err_code_tmp;
					}
					
					} // end of if.PR_Id match
				//	break;  // ������ͻ��ܹ�����ָ����Ϣ�����ǹͬ��Ʒ���������������Ҫbreak.
				} // end of for.j < LN_CNT
				//7.##ChangePrice 
				RunLog("---------- Node[%d]FP[%d] ER_DATA, error code: %02x -----", node, i + 1, err_code);

				int tmp_idx;
				for (tmp_idx = 0; tmp_idx < pump.newPrice[pr_no][0]; tmp_idx++) {
					ChangeER_DATA(node, 0x21 + i, err_code, NULL); // Translater�Ѿ��ظ���ACK
				}

				if (err_code != 0x99) {
					ChangeFP_State(node, 0x21 + i, FP_State_Inoperative, NULL);
				}

				// ��֤ 2x ����ļ���ﵽPRICE_CHANGE_RETRY_INTERVAL(2s) (������ǰ 1s + ���� 1s)
				Ssleep(PRICE_CHANGE_RETRY_INTERVAL - 1000000);
			} // end of for.i < panelCnt
			bzero(&pump.newPrice[pr_no], 4);  // ����Ʒ��۽���
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
 * mdf to America Gilbarco
 */
static int DoEot(int pos)
{
	int i;
	int ret,lnz;
	int pr_no;
	int err_cnt;
	__u8 price[4] = {0};
	__u8 volume[5] = {0};
	__u8 amount[5] = {0};
	__u8 total[7] = {0};
	__u8 vol_total[GUN_EXTEND_LEN] = {0};
	__u8 amount_total[GUN_EXTEND_LEN] = {0};
	__u8 pre_total[7] = {0};

	RunLog("�ڵ�[%d]���[%d]�����µĽ���", node, pos + 1);

	pump.addInf[pos][0] = 0;


	err_cnt = 0;
	do {
		ret = GetGunTotal(pos, pump.gunLog[pos], vol_total, amount_total);	// ȡ��ǹ����
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d] ȡ���ͻ�����ʧ��", node, pos + 1);
			err_cnt++;
		}
	} while (ret < 0 && err_cnt <=5);
	
	if (ret < 0) {
		return -1;
	}
#if 0
	if(point_flag == 1){ //��Ҫȥ�ٷ�λ
		ret = GetPumpPercentile(pos);
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d]��ȡ�ٷ�λʧ��", node,i+1);
			return -1;
		}
	}
#endif
	pr_no = (lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).PR_Id - 1;  // 0 - 7	
	if ((pr_no < 0) || (pr_no > 7)) {
		RunLog("DoEot PR_Id error, PR_Id[%d] <ERR>", pr_no + 1);
	}
	
	memcpy(pre_total, (lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Vol_Total, 7);

	lnz = pump.gunLog[pos] - 1;
	/*Beijing���������1λС����2λС������, America Gilbarco no need this*/
	//if(point_flag == 0){
	if (1) {
		total[0] = 0x0A;
		total[1] = 0x00;
		total[2] = 0x00;
		total[3] = (vol_total[7] << 4) | (vol_total[6] & 0x0F); // modfy by weihp volum total point << 1
		total[4] = (vol_total[5] << 4) | (vol_total[4] & 0x0F);
		total[5] = (vol_total[3] << 4) | (vol_total[2] & 0x0F);
		total[6] = (vol_total[1] << 4) | (vol_total[0] & 0x0F);
		RunLog("DoEot volume total : %02x%02x%02x%02x%02x%02x%02x", total[0],total[1],total[2],total[3],total[4],total[5],total[6]);
	}else if(point_flag == 1){
		total[0] = 0x0A;
		total[1] = 0x00;
		total[2] = vol_total[7] & 0x0f;
		total[3] = (vol_total[6] << 4) | (vol_total[5] & 0x0F);
		total[4] = (vol_total[4] << 4) | (vol_total[3] & 0x0F);
		total[5] = (vol_total[2] << 4) | (vol_total[1] & 0x0F);
		
		switch(lnz){
			case 0:
				total[6] = (vol_total[0] << 4)|(GilPercentile[pos].lnzOne& 0x0F);
				break;
			case 1:
				total[6] = (vol_total[0] << 4)|(GilPercentile[pos].lnzTwo& 0x0F);
				break;
			case 2:
				total[6] = (vol_total[0] << 4)|(GilPercentile[pos].lnzThr& 0x0F);
				break;
			case 3:
				total[6] = (vol_total[0] << 4)|(GilPercentile[pos].lnzFou& 0x0F);
				break;
			default:
				break;
		}

	}else{
		total[0] = 0x0A;
		total[1] = 0x00;
		total[2] = 0x00;
		total[3] = (vol_total[7] << 4) | (vol_total[6] & 0x0F);
		total[4] = (vol_total[5] << 4) | (vol_total[4] & 0x0F);
		total[5] = (vol_total[3] << 4) | (vol_total[2] & 0x0F);
		total[6] = (vol_total[1] << 4) | (vol_total[0] & 0x0F);
	}


	if (memcmp(total, pre_total, sizeof(total)) != 0) {
		err_cnt = 0;
		do {
			ret = GetTransData(pos);	// ȡ���һ�ʼ��ͼ�¼
			if (ret < 0) {
				RunLog("�ڵ�[%d]���[%d]ȡ���׼�¼ʧ��",node,pos+1);
				err_cnt++;
			}
		} while (ret < 0 && err_cnt <= 5);

		if (ret < 0) {
			return -1;
		}

#if 0 // change volum[0]
		volume[0] = 0x05;
		volume[1] = 0x00;
		volume[2] = (pump.cmd[22] << 4) | (pump.cmd[21] & 0x0f);
		volume[3] = (pump.cmd[20] << 4) | (pump.cmd[19] & 0x0f);
		volume[4] = (pump.cmd[18] << 4) | (pump.cmd[17] & 0x0f);
		memcpy(lpDispenser_data->fp_data[pos].Current_Volume, volume, 5);

		amount[0] = 0x07;
		amount[1] = 0x00;
		amount[2] = (pump.cmd[29] << 4) | (pump.cmd[28] & 0x0f);
		amount[3] = (pump.cmd[27] << 4) | (pump.cmd[26] & 0x0f);
		amount[4] = (pump.cmd[25] << 4) | (pump.cmd[24] & 0x0f);
		memcpy(lpDispenser_data->fp_data[pos].Current_Amount, amount, 5);
#else //change volume 1-4
		volume[0] = 0x06;
		volume[1] = 0x00;
		volume[2] = (pump.cmd[22] & 0x0f);
		volume[3] = (pump.cmd[21] << 4) | (pump.cmd[20] & 0x0f);
		volume[4] = (pump.cmd[19] << 4) | (pump.cmd[18] & 0x0f);
		memcpy(lpDispenser_data->fp_data[pos].Current_Volume, volume, 5);
		RunLog("GetTrans Volume : %02x%02x%02x%02x%02x", volume[0],volume[1],volume[2],volume[3],volume[4]);

		amount[0] = 0x06;
		amount[1] = (pump.cmd[29] << 4) | (pump.cmd[28] & 0x0f);
		amount[2] = (pump.cmd[27] << 4) | (pump.cmd[26] & 0x0f);
		amount[3] = (pump.cmd[25] << 4) | (pump.cmd[24] & 0x0f);
		amount[4] = 0x00;
		memcpy(lpDispenser_data->fp_data[pos].Current_Amount, amount, 5);
		RunLog("GetTrans Amount : %02x%02x%02x%02x%02x", amount[0],amount[1],amount[2],amount[3],amount[4]);

#endif

		price[0] = 0x04;
		price[1] = (pump.cmd[15] << 4) | (pump.cmd[14] & 0x0f);
		price[2] = (pump.cmd[13] << 4) | (pump.cmd[12] & 0x0f);
		price[3] = 0x00;
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
		total[2] = 0x00;
		total[3] = (amount_total[7] & 0x0f); //modefy by weihp , point << 1
		total[4] = (amount_total[6] << 4) | (amount_total[5] & 0x0f);
		total[5] = (amount_total[4] << 4) | (amount_total[3] & 0x0f);
		total[6] = (amount_total[2] << 4) | (amount_total[1] & 0x0f);
		RunLog("DoEot amount total : %02x%02x%02x%02x%02x%02x%02x", total[0],total[1],total[2],total[3],total[4],total[5],total[6]);

		memcpy((lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Amount_Total, total, 7);
		
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

	} else {
		ret = EndZeroTr4Fp(node, pos + 0x21);
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d]DoEot.EndZeroTr4Fp ʧ��, ret: %d <ERR>", node,pos+1,ret);
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
	int trynum;
	__u8 lrc;

	for (trynum = 0; trynum <TRY_TIME ; trynum++) {
		Ssleep(INTERVAL);
		pump.cmd[0] = 0x50 | pump.panelPhy[pos];
		pump.cmdlen = 1;
		pump.wantLen = pump.gunCnt[pos] * 30 + 4;
		
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d]�·�ȡ��������ʧ��",node,pos +1);
			gpGunShm->addInf[pump.chnLog - 1][0] = 0x00;
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		//�䳤����.����ñ�����Ϊ0x00,��tty������ȡ���㹻�ĳ���ʱ,��Ҫ�ø��������ж��Ƿ��к�������
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x01;
		//�ж�������ĳ�����spec.c��
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		gpGunShm->addInf[pump.chnLog - 1][0] = 0x00;	//ȡ�����ݺ�, ��λ���
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d]�·�ȡ��������ɹ�, ȡ������Ϣʧ��",node,pos +1);
			continue;
		}

		if (pump.cmd[pump.cmdlen - 1] != 0xf0) {
			RunLog("�ڵ�[%d]���[%d] �������ݲ�����",node,pos +1);              //��������
			continue;
		}

		//lrcУ��ʧ��//
		lrc = LRC(pump.cmd, pump.cmdlen - 2);
		if ((lrc & 0x0f) != (pump.cmd[pump.cmdlen - 2] & 0x0f)) {
			RunLog("�ڵ�[%d]���[%d] LRCУ��ʧ��",node,pos + 1);
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
		RunLog("�ڵ�[%d]���[%d] ȡǹ�ı���ʧ��", node,pos + 1);
		return -1;
	}

	num = (pump.cmdlen - 4) / 30;	// ÿ����¼30�ֽ�,4���ֽ�Ϊ������ͷ,��β
	p = pump.cmd + 1;

	for (i = 0; i < num; i++) {
		if ((p[1] & 0x0F) == gunLog - 1) {
			memcpy(vol_total, p + 3, GUN_EXTEND_LEN);
			memcpy(amount_total, p + 12, GUN_EXTEND_LEN);
			break;
		}
		p = p + 30;
	}

	return 0;
}


/*
 * func: ȡĳ��ǹ�ı���
 */ 
static int GetGunPrice(int pos, __u8 gunLog, __u8 *price)
{
	int ret ;
	int i;
	int  num;
	__u8 *p;

	ret = GetPumpTotal(pos);
	if (ret < 0) {
		RunLog("�ڵ�[%d]���[%d] ȡǹ����Ʒ����ʧ��", node,pos + 1);
		return -1;
	}

	num = (pump.cmdlen - 4) / 30;	// ÿ����¼30�ֽ�,4���ֽ�Ϊ������ͷ,��β
	p = pump.cmd + 1;

	for (i = 0; i < num; i++) {
		if ((p[1] & 0x0F) == gunLog - 1) {
			memcpy(price, p + 21, 4);
			break;
		}
		p = p + 30;
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
	int trynum;
	__u8 lrc;

	for (trynum = 0; trynum < TRY_TIME; ++trynum) { 
		pump.cmd[0] = 0x40 | pump.panelPhy[pos];
		pump.cmdlen = 1;
		pump.wantLen = 33;

		Ssleep(INTERVAL);	/*ͣ10����,��ֹд��̫Ƶ��*/
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d]�·�����������ʧ��",node ,pos +1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d]�·�����������ɹ�, ȡ������Ϣʧ��",node ,pos +1);
			continue;
		}

		if (pump.cmd[pump.cmdlen - 1] != 0xf0) {
			RunLog("�ڵ�[%d]���[%d] ȷ����Ϣʧ��",node,pos+1);
			continue;
		}

		/*lrcУ��ʧ��*/
		lrc = LRC(pump.cmd, pump.cmdlen - 2);
		if ((lrc&0x0f) != (pump.cmd[pump.cmdlen - 2] & 0x0f)) {
			RunLog("�ڵ�[%d]���[%d],LRCУ��ʧ��", node, pos +1);
			continue;
		}


		fp_pos = pump.cmd[4] & 0x0f;	/*�����������16�����Ϊ0,����Ϊ��Ӧֵ.������16��f,1��0...*/
		if (fp_pos == 0x0f) {
			fp_pos = 0x00;
		} else {
			fp_pos += 1;
		}

		if (fp_pos == pump.panelPhy[pos]) {
			pump.gunLog[pos] = (pump.cmd[9] & 0x0F) + 1;
			return 0;
		}
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

	for (trynum = 0; trynum < TRY_TIME; ++trynum) {
		if (pos == 255) {
			pump.cmd[0] = 0xfc;
		} else {
			pump.cmd[0] = 0x30 | pump.panelPhy[pos];
		}
		pump.cmdlen = 1;
		pump.wantLen = 0;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d]�·�ֹͣ��������ʧ��", node, pos + 1);
			continue;
		}

		Ssleep(200000);  // 200ms, �ӳټ��
		ret = GetPumpStatus(pos, &status);
		if (ret < 0) {
			continue;
		}

		// PUMP_CALL ״̬��ִ��DoStop״̬����
		// PUMP_AUTH ״̬��ִ�� DoStop ���Ϊ PUMP_OFF
		// PUMP_BUSY ״̬��ִ��DoSTop���Ϊ PUMP_STOP
		if (status == PUMP_STOP || status == PUMP_OFF || status == PUMP_CALL) {
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
	int trynum;
	int ret;
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
		for (trynum = 0; trynum < TRY_TIME; trynum++) {
			pump.cmd[0] = 0x10 | pump.panelPhy[i];
			pump.cmdlen = 1;
			pump.wantLen = 0;

			Ssleep(INTERVAL);
			ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
			if (ret < 0) {
				RunLog("�ڵ�[%d]���[%d]�·���Ȩ����ʧ��",node,pos +1);
				continue;
			}

			Ssleep(60000);   // �������ѯ״̬֮��������Ҫ���68ms
			ret = GetPumpStatus(i, &status);
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

		if (trynum == TRY_TIME) {
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
	int i, j, freq;
	int ret;
	int trynum;
	int pr_no;
	__u8 tmp1[5];
	__u8 tmp2[5];
	__u8 status;
	__u8  ifsf_amount[5],ifsf_volume[5];
	time_t time_tmp;
	static time_t running_msg_time[MAX_FP_CNT] = {0};
	const __u8 arr_zero[3] = {0};
	long price, amount, volume;

	for (trynum = 0; trynum < 2; trynum++) {
		// Step.1 ��ȡʵʱ����
		pump.cmd[0] = 0x60 | pump.panelPhy[pos];
		pump.cmdlen = 1;
		pump.wantLen = 6;

		if (pump.addInf[pos][0] == 0) {  // ��һ��
			Ssleep(INTERVAL * 150);
			pump.addInf[pos][0] = 1;
		} else {
			Ssleep(INTERVAL * 2);
		}

		Ssleep(INTERVAL); 
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d]����ʵʱ��������ʧ��",node,pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d]����ʵʱ��������ɹ�, ȡ������Ϣʧ��",node,pos + 1);
			continue;
		}

		// Step.2  Exec ChangeFP_Run_Trans()
		memset(tmp1, 0, sizeof(tmp1));
		tmp1[0] = (pump.cmd[5] << 4) | (pump.cmd[4] & 0x0f);
		tmp1[1] = (pump.cmd[3] << 4) | (pump.cmd[2] & 0x0f);
		tmp1[2] = (pump.cmd[1] << 4) | (pump.cmd[0] & 0x0f);
		ifsf_amount[0] = 0x06;
		ifsf_amount[1] = tmp1[0]; // MianDian amount not point
		ifsf_amount[2] = tmp1[1];
		ifsf_amount[3] = tmp1[2];
		ifsf_amount[4] = 0x00;
		ifsf_volume[0] = 0x06;
		ifsf_volume[1] = 0x00;

		// ֻ�е���������Ϊ����ܽ�FP״̬�ı�ΪFuelling
		if (fp_state[pos] != 3) {
		       if (memcmp(tmp1, arr_zero, 3) == 0) { 
				return 0;
		       }
			fp_state[pos] = 3;
			return 0;
		}

		memset(tmp2, 0, sizeof(tmp2));
		pr_no = (lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).PR_Id - 1;  // 0-7	
		memcpy(tmp2, lpDispenser_data->fp_fm_data[pr_no][lpDispenser_data->fp_data[pos].Default_Fuelling_Mode - 1].Prod_Price + 1, 3);

		amount = 0;
		price = 0;
		for (i = 0; i < 3; i++) {
			amount = amount * 100 + (tmp1[i] >> 4) * 10 + (tmp1[i] & 0x0f);
			price = price * 100 + (tmp2[i] >> 4) * 10 + (tmp2[i] & 0x0f);
		}
		if (price == 0) {
			UserLog("�ڵ�[%d]���[%d] DoBusy.Price is 0 error, Default FM: %d", node, pos + 1, 
							lpDispenser_data->fp_data[pos].Default_Fuelling_Mode);
			return -1;
		}
		//volume = amount * 100 / price; //������λС��,������ע��100
		volume = amount *10000  / price; //��鲻�ó�100
		if (0 == volume) {
			UserLog("�ڵ�[%d]���[%d], Volume is zero",node,pos + 1);
		}
#if 0
		RunLog("�ڵ�[%d]���[%d] ## Amount: %ld, Volume: %d, Price: %ld",node,pos + 1, amount, volume, price);
#endif

		memset(tmp2, 0, sizeof(tmp2));
		for (i= 3; i >= 0; i--) {
			tmp2[i] = (volume % 10) | (((volume / 10)  % 10) << 4);
			volume /= 100;
		}
		if (volume != 0) {
			UserLog("�ڵ�[%d]���[%d], Current_Volume caculate error",node,pos +1);
		}

		memcpy(&(ifsf_volume[1]), tmp2, 4);

		if ((__u8)FP_State_Fuelling == lpDispenser_data->fp_data[pos].FP_State) {
			time_tmp = time(NULL);
			freq =	(((lpDispenser_data->fp_data[pos].Running_Transaction_Message_Frequency[1] >> 4) * 100) +
				((lpDispenser_data->fp_data[pos].Running_Transaction_Message_Frequency[1] & 0x0F) * 10) +
				(lpDispenser_data->fp_data[pos].Running_Transaction_Message_Frequency[0] >> 4));
			if ((time_tmp >= running_msg_time[pos] + freq) && freq != 0) {
				running_msg_time[pos] = time_tmp;

				ret = ChangeFP_Run_Trans(node, 0x21 + pos, ifsf_amount, ifsf_volume);
			}
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
	int trynum;
	__u8 ln_status;

	pump.addInf[pos][0] = 0; 

	ret = GetCurrentLNZ(pos);
	if (ret < 0) {
		RunLog("�ڵ�[%d]���[%d], ��ȡ��ǰ��ǹǹ��ʧ��",node,pos + 1);
		return -1;
	}

	ret = ChangeFP_State(node, pos + 0x21, FP_State_Calling, NULL);

	//��λ������,���������Զ���Ȩ;��λ��������, ��Ȩ�������ڿ���״̬,��ʱ�Զ���Ȩ
	if ((gpIfsf_shm->auth_flag == FCC_SWITCH_AUTH) && (lock_status() == 1)) {
		ret = DoAuth(pos);
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d]�Զ���Ȩʧ��",node,pos +1);
		}
		RunLog("�ڵ�[%d]���[%d]�Զ���Ȩ�ɹ�",node ,pos +1);
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

	for (i = 0; i < 2; ++i) {         // Note. GetPumpStatus ѭ����������ñ��ص�,��Ҫ��trynum
		pump.cmd[0] = 0x00 | pump.panelPhy[pos];
		pump.cmdlen = 1;
		pump.wantLen = 1;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d]����״̬����ʧ��",node,pos +1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd)-1;

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d]����״̬����ɹ�, ȡ������Ϣʧ��",node,pos +1);
			continue;
		} 

		if ((pump.cmd[0] & 0x0f) != pump.panelPhy[pos]) {
			RunLog("Node[%d]FP[%d]Message Error.PF_Id not match", node, pos + 1);
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
		pump.cmd[0] = 0x00 | pump.panelPhy[pos];
		pump.cmdlen = 1;
		pump.wantLen = 1;

		Ssleep(INTERVAL * 10);   // 100ms
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d]����״̬����ʧ��",node,pos +1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d]����״̬����ɹ�, ȡ������Ϣʧ��",node,pos +1);
			continue;
		} 

		if ((pump.cmd[0] & 0x0f) != pump.panelPhy[pos]) {
			RunLog("Message Error.PF_Id not match");
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
 * Note: ���������������ņ̀���״̬��ִ��,����PUMP_CALL/PUMP_BUSY/PUMP_STOP,��������Ӧ
 */
static int GetCurrentLNZ(int pos)
{
	int ret;
	int trynum;
	__u8 ln_status;
	__u8 sfc_cmd[] = { 0xff, 0xe9, 0xfe, 0xe0, 0xe1, 0xe0, 0xfb, 0xee, 0xf0 };

	for(trynum = 0; trynum < TRY_TIME; trynum++) {
		pump.cmd[0] = 0x20 | pump.panelPhy[pos];
		pump.cmdlen = 1;
		pump.wantLen = 1;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d] �򴮿�д����ʧ��",node,pos +1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d] ȡ������Ϣʧ��",node,pos + 1);
			continue;
		}

		if (pump.cmd[0] != (0xd0 | pump.panelPhy[pos])) {
			RunLog("�ڵ�[%d]���[%d] ��Ϣȷ��ʧ��",node,pos +1);
			Ssleep(2000000);   // ���2s
			continue;
		}

		memcpy(pump.cmd, sfc_cmd, 9);
		pump.cmdlen = 9;
		pump.wantLen = 19;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT) ;
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d]��ȡǹ������ʧ��",node,pos +1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d]��ȡǹ������ɹ�, ȡ������Ϣʧ��",node,pos +1);
			continue;
		}
		
		//                        ID                  Msgs:  In/Out  No  Cksum CR LF
		// 1ņ̀�� ret: ba b0 b3  B1  b0 b1 b0 b0 b0  b0 b1 b0 B1 b1 B1  c3 c1 8d 8a
		// ��ņ̀��ret: ba b0 b3  B1  b0 b1 b0 b0 b0  b0 b1 b0 B0 b1 B0  c3 c3 8d 8a
				
		if (pump.cmdlen != 19) {
			RunLog("�ڵ�[%d]���[%d]������Ϣ����",node,pos + 1);
			continue;
		}

		point_flag = pump.cmd[9] & 0x0f;

		if(point_flag < 0 || point_flag >2){
			RunLog("�ڵ�[%d]С����λ��ȡ����point_flag[%d]",node, point_flag);
			point_flag = 0;
		}

		switch(point_flag){
			case 0:
				RunLog("�ڵ�[%d]point_flag[%d] һλ����С��λ",node, point_flag);
				break;
			case 1:
				RunLog("�ڵ�[%d]point_flag[%d] һλ����С��λ,010����ȡ�ٷ�λ"
					,node, point_flag);
				break;
			case 2:
				RunLog("�ڵ�[%d]point_flag[%d] ��λ����С��λ",node, point_flag);
				break;
			default:
				RunLog("�ڵ�[%d]point_flag[%d] ����!",node, point_flag);
				break;
		}

		if ( ((pump.cmd[3] & 0xf0) == 0xb0 && 
			(pump.cmd[3] & 0x0f) == pump.panelPhy[pos]) ||
			((pump.cmd[3]&0xf0)==0xc0 &&
			 ((pump.cmd[3] & 0x0f) + 0x09 == pump.panelPhy[pos])) ) {

			if ((pump.cmd[14] & 0x0f) != 0x00) {
				pump.gunLog[pos] = pump.cmd[14] & 0x0f;	// ��ǰ̧�����ǹ
				ln_status = 1 << (pump.gunLog[pos] - 1);
				RunLog("�ڵ�[%d]���[%d] %d ��ņ̀��", node, pos + 1, pump.gunLog[pos]);
				lpDispenser_data->fp_data[pos].Log_Noz_State = ln_status;
				return 0;
			} else {
				RunLog("�ڵ�[%d]���[%d] ��������",node,pos + 1);
				continue;
			}
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
	int ret;

	for (i = 0; i < pump.panelCnt; ++i) {
		ret = GetTransData(i);
		if (ret < 0) {
			return -1;
		}

		pump.gunLog[i] = (pump.cmd[9] & 0x0F) + 1;   
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
	int fm_idx;
	__u8 price[4];

	for (i = 0; i < pump.panelCnt; i++) {
		for (j = 0; j < pump.gunCnt[i]; j++) {
			pr_no = (lpDispenser_data->fp_ln_data[i][j]).PR_Id - 1;  // 0-7	
			if ((pr_no < 0) || (pr_no > 7)) {
				RunLog("InitPumpPrice PR_Id error, PR_Id[%d]", pr_no + 1);
				return -1;
			}

			fm_idx = lpDispenser_data->fp_data[i].Default_Fuelling_Mode - 1;
			memcpy(lpDispenser_data->fp_data[i].Current_Unit_Price, 
				lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price, 4);
#ifdef DEBUG
			RunLog("�ڵ�[%d]���[%d] LNZ %d PR_Id %d,Prod_Price: %02x%02x%02x%02x",
				node,i + 1, j, pr_no + 1,
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
	int  num;
	int pr_no;
	__u8 *p;
	__u8 status;
	__u8 lnz;
	__u8 meter_total[7] = {0};
	__u8 extend[GUN_EXTEND_LEN] = {0};
	__u8 amount_total[7] = {0};
	__u8 price_bcd[2] = {0};
	__u8 price_hex[2] = {0};
	__u64 total;

	// ������ͨ����ʼ�������ʱ��, ����IO����ѹ��
	Ssleep(pump.chnLog * 20 * INTERVAL);  // added by jie @ 2009.4.18

	for (i = 0; i < pump.panelCnt; i++) {
		ret = GetPumpStatus(i, &status);
		if (ret < 0) {
			return -1;
		}

		if (status != PUMP_OFF && status != PUMP_CALL &&
			status != PUMP_STOP && status != PUMP_PEOT &&
						status != PUMP_FEOT) {
			RunLog("�ڵ�[%d]���[%d]��ǰFP״̬���ܲ�ѯ����,Status[%02x]", node, i + 1, status);
			if (status == PUMP_AUTH) {
				// ���������Ȩ״̬, �����ȡ����, ����ͨѶ����, �����޷�����
				DoStop(i);
			}
			return -1;
		}
		if(point_flag == 1){ //��Ҫȥ�ٷ�λ
			ret = GetPumpPercentile(i);
			if (ret < 0) {
				RunLog("�ڵ�[%d]���[%d]��ȡ�ٷ�λʧ��", node,i+1);
				return -1;
			}
		}
		
		Ssleep(INTERVAL);
		ret = GetPumpTotal(i);
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d]ȡ����ʧ��", node,i+1);
			return -1;
		}

		
		num = (pump.cmdlen - 4) / 30; //ÿ����¼30�ֽ�,4���ֽ�Ϊ������ͷ,��β
		p = pump.cmd + 1;

		for (j = 0; j < num; j++) {
			// Volume_Total
			memcpy(extend, p + 3, GUN_EXTEND_LEN);
			lnz = p[1] & 0x0F;
			pr_no = (lpDispenser_data->fp_ln_data[i][lnz]).PR_Id - 1;  // 0-7	
			RunLog("�ڵ�[%d]���[%d], p[1]: %02x,j: %d, lnz: %d, pr_no:%d",node,i+1,p[1], j, lnz, pr_no);
			if ((pr_no < 0) || (pr_no > 7)) {
				RunLog("�ڵ�[%d]���[%d], InitPumpTotal,FP %d, LNZ %d, PR_Id %d",
									node,i+1, i, lnz + 1, pr_no + 1);
				p = p + 30;
				continue;
			}

			/*���������1λС����2λС������*/
		/*	if(point_flag == 0){
				meter_total[0] = 0x0A;
				meter_total[1] = 0x00;
				meter_total[2] = extend[7] & 0x0f;
				meter_total[3] = (extend[6] << 4) | (extend[5] & 0x0F);
				meter_total[4] = (extend[4] << 4) | (extend[3] & 0x0F);
				meter_total[5] = (extend[2] << 4) | (extend[1] & 0x0F);
				meter_total[6] = (extend[0] << 4);
			}else if(point_flag == 1){
				meter_total[0] = 0x0A;
				meter_total[1] = 0x00;
				meter_total[2] = extend[7] & 0x0f;
				meter_total[3] = (extend[6] << 4) | (extend[5] & 0x0F);
				meter_total[4] = (extend[4] << 4) | (extend[3] & 0x0F);
				meter_total[5] = (extend[2] << 4) | (extend[1] & 0x0F);
				
				switch(lnz){
					case 0:
						meter_total[6] = (extend[0] << 4)|(GilPercentile[i].lnzOne& 0x0F);
						break;
					case 1:
						meter_total[6] = (extend[0] << 4)|(GilPercentile[i].lnzTwo& 0x0F);
						break;
					case 2:
						meter_total[6] = (extend[0] << 4)|(GilPercentile[i].lnzThr& 0x0F);
						break;
					case 3:
						meter_total[6] = (extend[0] << 4)|(GilPercentile[i].lnzFou& 0x0F);
						break;
					default:
						break;
				}

			}*/
			
			{
				meter_total[0] = 0x0A;
				meter_total[1] = 0x00;
				meter_total[2] = 0x00;
				meter_total[3] = (extend[7] << 4) | (extend[6] & 0x0F);
				meter_total[4] = (extend[5] << 4) | (extend[4] & 0x0F);
				meter_total[5] = (extend[3] << 4) | (extend[2] & 0x0F);
				meter_total[6] = (extend[1] << 4) | (extend[0] & 0x0F);
			}

			memcpy(lpDispenser_data->fp_m_data[pr_no].Meter_Total, meter_total, 7);
			memcpy((lpDispenser_data->fp_ln_data[i][lnz]).Log_Noz_Vol_Total, meter_total, 7);
#if DEBUG
			RunLog("�ڵ�[%d]���[%d] Init.fp_ln_data[%d][%d].Vol_Total: %s",
				node,i+1, i, lnz, Asc2Hex(lpDispenser_data->fp_ln_data[i][lnz].Log_Noz_Vol_Total, 7));
			RunLog("�ڵ�[%d]���[%d] Init.fp_m_data[%d].Meter_Total[%s]",
									node,i+1,pr_no, Asc2Hex(meter_total, 7));
#endif
			// Amount_Total
			memcpy(extend, p + 12, GUN_EXTEND_LEN);
			meter_total[0] = 0x0A;
			meter_total[1] = 0x00;
			meter_total[2] = 0x00;
			meter_total[3] = (extend[7] << 4) | (extend[6] & 0x0F);
			meter_total[4] = (extend[5] << 4) | (extend[4] & 0x0F);
			meter_total[5] = (extend[3] << 4) | (extend[2] & 0x0F);
			meter_total[6] = (extend[1] << 4) | (extend[0] & 0x0F);
			memcpy((lpDispenser_data->fp_ln_data[i][lnz]).Log_Noz_Amount_Total, meter_total, 7);
			RunLog("�ڵ�[%d]���[%d] Init.fp_ln_data[%d][%d].Amount_Total: %s ", node,i+1, i, lnz,
				       	Asc2Hex(lpDispenser_data->fp_ln_data[i][lnz].Log_Noz_Amount_Total, 7));

			p = p + 30;
		}

	}

	return 0;
}



/*
 * func: ���ͻ�����ͨѶ x ��, �˽��ڵȴ��ڼ��ͻ���״̬, ����ĳЩ�ͻ����б�������ģʽ������
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
		Ssleep(200000);  // 200ms
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
static int SetPort001()
{
	int ret;

	pump.cmd[0] = 0x1C;
	pump.cmd[1] = 0x52;
	pump.cmd[2] = 0x30 | (pump.chnPhy - 1);
	pump.cmd[3] = 0x05;	// 5760bps
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
	Ssleep(50*INTERVAL);      // ȷ����������ͨѶ֮ǰ���������Ѿ����ú�

	RunLog("ͨ�� %d ���ô������Գɹ�", pump.chnLog);

	return 0;
}


/*
 * func:  ͨѶ��ͨʱ��ѯ�����и�һ��ʱ������һ�´�������
 *        ����������ĵ�ѹ���ȵ��µ�Ƭ����λ--�����������ûָ�Ԥ��9600,8,n,1
 */
static int ReDoSetPort001()
{
#define RE_DO_FREQUENCY   10         //  10����ѯʧ������һ�´�������

	int static polling_fail_cnt = 0;
	int ret;

	if ((++polling_fail_cnt) >= RE_DO_FREQUENCY) {
		ret = SetPort001();
		if (ret < 0) {
			RunLog("�ڵ� %d ���贮������ʧ��", node);
		}
		polling_fail_cnt = 0;
	}

	return ret;
}


static int DoRation_Idle(int pos)  
{
#define  PRE_VOL  0xF1
#define  PRE_AMO  0xF2
	int ret;
	int trynum;
	__u8 amount[6];
	__u8 preset_type;
	__u8 lnz_allowed = 0;      // �ͻ�ǹ�Ŵ�0��ʼ

	if (lpDispenser_data->fp_data[pos].Log_Noz_Mask == 0) {
		RunLog("Node[%d]FP[%d]δ�����κ���ǹ����", node, pos + 1);
		return -1;
	}

	// Step.0 �������ǹ��
	for (ret = 0x01; ret <= 0x80; ret = (ret << 1), lnz_allowed++) {
		if ((lpDispenser_data->fp_data[pos].Log_Noz_Mask & ret) != 0) {
			break;
		}
	}

	// ����������
	if (ration_type[pos] == 0x3D) { 
		preset_type = PRE_VOL;
	} else {
		preset_type = PRE_AMO;
	}

	amount[0] = 0xe0 | (ration_value[pos][2] & 0x0f);
	amount[1] = 0xe0 | (ration_value[pos][2] >> 4);
	amount[2] = 0xe0 | (ration_value[pos][1] & 0x0f);
	amount[3] = 0xe0 | (ration_value[pos][1] >> 4);
	amount[4] = 0xe0 | (ration_value[pos][0] & 0x0f);
	amount[5] = 0xe0 | (ration_value[pos][0] >> 4);

	for (trynum = 0; trynum < 3; trynum++) {
		// Step.1.1 ����Ԥ������
		pump.cmd[0] = 0x20 | pump.panelPhy[pos];
		pump.cmdlen = 1;
		pump.wantLen = 1;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d], ԤдԤ�ü�������ʧ��", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd)-1;

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);

		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d], ԤдԤ�ü�������ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			Ssleep(2 * INTERVAL);
			continue;
		} else if (pump.cmd[0] != (0xD0 | pump.panelPhy[pos])) { 
			Ssleep(2000000);            // 2x������Ҫ��Ϊ2s
			continue;
		}							
		RunLog("�ڵ�[%d]���[%d] Step.1 ԤдԤ�ü�������ɹ�", node, pos + 1);
		
		// Step.1.2 ����Ԥ����
		pump.cmd[0] = 0xff;
		pump.cmd[1] = 0xe3;
		pump.cmd[2] = preset_type;
		pump.cmd[3] = 0xf4;      // Price Level 1

		if ((lpDispenser_data->fp_data[pos].Log_Noz_Mask != 0xFF) ||
			(preset_type == PRE_VOL) ||(lpNode_channel->cntOfFpNoz[pos] == 1)) { 
			// ֻ������Ԥ���ܹ�ָ����ǹ
			pump.cmd[4] = 0xf6;      // Grade data next
			pump.cmd[5] = 0xe0 | lnz_allowed;
			pump.cmd[6] = 0xf8;
			pump.cmd[7] = amount[0];
			pump.cmd[8] = amount[1];
			pump.cmd[9] = amount[2];
			pump.cmd[10] = amount[3];
			pump.cmd[11] = amount[4];		
			pump.cmd[12] = amount[5];		
			pump.cmd[13] = 0xfb;
			pump.cmd[14] = 0xe0 | (LRC(pump.cmd, 14) & 0x0f);
			pump.cmd[15] = 0xf0;
			pump.cmdlen = 16;
			pump.wantLen = 0;
		} else { //0xFF ������λ���·�Ԥ������ʱû��ָ����ǹ, add by liys @ 2009.02.06
			pump.cmd[4] = 0xf8;
			pump.cmd[5] = amount[0];
			pump.cmd[6] = amount[1];
			pump.cmd[7] = amount[2];
			pump.cmd[8] = amount[3];
			pump.cmd[9] = amount[4];		
			pump.cmd[10] = amount[5];		
			pump.cmd[11] = 0xfb;
			pump.cmd[12] = 0xe0 | (LRC(pump.cmd, 12) & 0x0f);
			pump.cmd[13] = 0xf0;
			pump.cmdlen = 14;
			pump.wantLen = 0;		   	
		}	


		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d], ԤдԤ�ü�����ʧ��", node, pos + 1);
			// WriteToPort ʧ���Ѿ���ʱ2��, ���Բ�����Ҫ������ʱ��continue
			continue;
		}
              
		RunLog("�ڵ�[%d]���[%d] Step.2 ԤдԤ�ü������ɹ�[%s]",
			node, pos + 1, Asc2Hex(pump.cmd, pump.cmdlen));
		
		return 0; 
	}


	return -1;
}


/*
 * ���FP�ϸ�ǹ����Ʒ�����Ƿ������ȷ(����ʵ��), ���ȫ����ȷ��ı�״̬ΪClosed
 */
static int CheckPriceErr(int pos)
{
	int ret;
	int lnz, pr_no;
	int fm_idx;
	__u8 status;
	__u8 msp[4] = {0};
	__u8 price[4] = {0};

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

		memset( price, 0, sizeof(price));
		memset( msp, 0, sizeof(msp));
		
		ret = GetGunPrice(pos, lnz + 1, msp);
		if(ret < 0){
			return -1;
		}

		price[2] = (msp[3] << 4) | (msp[2] & 0x0f);
		price[3] = (msp[1] << 4) | (msp[0] & 0x0f);
		fm_idx = lpDispenser_data->fp_data[pos].Default_Fuelling_Mode - 1;

		RunLog("Node %d FP %d LNZ %d Now price is %02x.%02x; DST Price: %02x.%02x",
			node, pos + 1, lnz + 1, price[2], price[3],	
			lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2], 
		       	lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3]);
		

		if ((price[2] != lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2]) ||
			(price[3] != lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3])) {
			return -2;
		}
	}
	
	ChangeFP_State(node, 0x21 + pos, FP_State_Closed, NULL);
	RunLog("Node[%d]FP[%d]�����ѱ����ȷ, �л�״̬ΪClosed", node, pos + 1);

	return 0;
}

/*
 * func: ��ȡ�����ͻ�С����λ��
 */
static int GetPumpPoint()
{
	int i;
	int ret;
	__u8 sfc_cmd[] = { 0xff, 0xe9, 0xfe, 0xe0, 0xe1, 0xe0, 0xfb, 0xee, 0xf0 };


	for (i = 0; i < 5; ++i) {         // Note. GetPumpStatus ѭ����������ñ��ص�,��Ҫ��trynum
		pump.cmd[0] = 0x20 | pump.panelPhy[0];
		pump.cmdlen = 1;
		pump.wantLen = 1;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d] �򴮿�д����ʧ��",node,1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d] ȡ������Ϣʧ��",node, 1);
			continue;
		}

		if (pump.cmd[0] != (0xd0 | pump.panelPhy[0])) {
			RunLog("�ڵ�[%d]���[%d] ��Ϣȷ��ʧ��",node,1);
			Ssleep(2000000);   // ���2s
			continue;
		}

		memcpy(pump.cmd, sfc_cmd, 9);
		pump.cmdlen = 9;
		pump.wantLen = 19;

		Ssleep(INTERVAL_TOTAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT) ;
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d]��������С����λ��ʧ��",node,1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d]��ȡ����λ���ɹ�, ȡ������Ϣʧ��",node,1);
			continue;
		}
		

		if ((pump.cmd[9] & 0xf0) != 0xb0) {
			RunLog("Node[%d]Message Error.PF_Id not match", node);
			continue;
		}

		point_flag = pump.cmd[9] & 0x0f;

		if(point_flag < 0 || point_flag >2){
			RunLog("�ڵ�[%d]С����λ��ȡ����point_flag[%d]",node, point_flag);
			point_flag = 0;
		}

		switch(point_flag){
			case 0:
				RunLog("�ڵ�[%d]point_flag[%d] һλ����С��λ",node, point_flag);
				break;
			case 1:
				RunLog("�ڵ�[%d]point_flag[%d] һλ����С��λ,010����ȡ�ٷ�λ"
					,node, point_flag);
				break;
			case 2:
				RunLog("�ڵ�[%d]point_flag[%d] ��λ����С��λ",node, point_flag);
				break;
			default:
				RunLog("�ڵ�[%d]point_flag[%d] ����!",node, point_flag);
				break;
		}
			
		return 0;
	}

	return -1;
}


/*
 * func: ��ȡ�����ͻ��ٷ�λ����
 */
static int GetPumpPercentile(const int pos)
{
	int i;
	int ret;
	__u8 sfc_cmd[] = { 0xff, 0xe9, 0xfe, 0xe0, 0xe1, 0xe0, 0xfb, 0xee, 0xf0 };

	if(pos < 0 || pos > MAX_FP_CNT){
		RunLog("GetPumpPercentile  pos[%d]����", pos);
		return -1;
	}
	
	for (i = 0; i < 5; ++i) {         // Note. GetPumpStatus ѭ����������ñ��ص�,��Ҫ��trynum
		pump.cmd[0] = 0x20 | pump.panelPhy[pos];
		pump.cmdlen = 1;
		pump.wantLen = 1;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d] �򴮿�д����ʧ��",node,1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d] ȡ������Ϣʧ��",node, 1);
			continue;
		}

		if (pump.cmd[0] != (0xd0 | pump.panelPhy[pos])) {
			RunLog("�ڵ�[%d]���[%d] ��Ϣȷ��ʧ��",node,1);
			Ssleep(2000000);   // ���2s
			continue;
		}

		memcpy(pump.cmd, sfc_cmd, 9);
		pump.cmdlen = 9;
		pump.wantLen = 19;

		Ssleep(INTERVAL_TOTAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT) ;
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d]��������С����λ��ʧ��",node,1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("�ڵ�[%d]���[%d]��ȡ����λ���ɹ�, ȡ������Ϣʧ��",node,1);
			continue;
		}
		
		//ret: ba b0 b3  B1  b0 b1 b0 b0 b0  b0 b1 b0 B1 b1 B1  c3 c1 8d 8a
		if ((pump.cmd[10] & 0xf0) != 0xb0) {
			RunLog("Node[%d]Message Error.PF_Id not match", node);
			continue;
		}

		GilPercentile[pos].lnzOne = pump.cmd[10] & 0x0f;

		if ((pump.cmd[10] & 0xf0) != 0xb0) {
			RunLog("Node[%d]Message Error.PF_Id not match", node);
			continue;
		}

		GilPercentile[pos].lnzTwo= pump.cmd[11] & 0x0f;

		if ((pump.cmd[10] & 0xf0) != 0xb0) {
			RunLog("Node[%d]Message Error.PF_Id not match", node);
			continue;
		}

		GilPercentile[pos].lnzThr= pump.cmd[12] & 0x0f;

		if ((pump.cmd[10] & 0xf0) != 0xb0) {
			RunLog("Node[%d]Message Error.PF_Id not match", node);
			continue;
		}

		GilPercentile[pos].lnzFou= pump.cmd[13] & 0x0f;

		RunLog("###Node[%d]FP[%d] LNZ Percentile 1[%02x] 2[%02x] 3[%02x] 4[%02x]",
			node, pos+1, GilPercentile[pos].lnzOne, GilPercentile[pos].lnzTwo, GilPercentile[pos].lnzThr,
			GilPercentile[pos].lnzFou);
			
		return 0;
	}

	return -1;
}


/* ----------------- End ----------------------------- */








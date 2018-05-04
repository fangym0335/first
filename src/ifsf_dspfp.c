/*******************************************************************
���͵����ݵĲ�������.

2007-07-04 by chenfm
2008-03-04 - Guojie Zhu <zhugj@css-intelligent.com>
    ����HandleStateErr����
2008-03-06 - Guojie Zhu <zhugj@css-intelligent.com>
    Nb_Tran_Buffer_Not_Paid ����Ϊ2��
    ExecFpCMD �ж�ӦFP_Release_FP����ʧ�ܵ��ж����� (FP_State != Authorised) 
    convertUsed[] ��Ϊ convert_hb[].lnao.node
2008-03-07 - Guojie Zhu <zhugj@css-intelligent.com>
    ����MakeFpTransMsg�н���������֯��������� - tmp_lg+5�ڲ����ڸ�ֵ֮ǰ
2008-03-09 - Guojie Zhu <zhugj@css-intelligent.com>
    EndTr4Fp��������Trans_State֮ǰ������λ������������ж�
2008-03-10 - Guojie Zhu <zhugj@css-intelligent.com>
    EndTr4Fp���ѻ����������pfp_tr_data.offline_flag = '\x01';
    ����WriteFP_DATA �� ReadFP_DATA �ظ����ĸ�ʽ���� ?Data_Id���� & ��������
2008-03-16 - Guojie Zhu <zhugj@css-intelligent.com>
    ReadFP_DATA&WriteFP_DATA���Ӷ�Running_Transaction_Frequcency��֧��
    Ԥ�������е�Ԥ�������ӶԶ�̬С�����֧��
2008-03-18 - Guojie Zhu <zhugj@css-intelligent.com>
    ExecFpCMD�з���lischnxxx����������ķ��ط�ʽ��Ϊ�����ڴ淽ʽ
2008-07-25 Chen Fu Ming<chenfm@css-intelligent.com>
     MakeFpMsg,MakeFpTransMsg,EndTr4Fp������Ϊ������Ŀ������д�ܵ�������
     ͬʱ�ڹ����ڴ��ж���1����־����:css_data_flag
     css_data_flagֵ0:û��������1:�������ǲ�Ҫʵ�����ݣ�2:������Ҫʵʱ���ݡ�
     ����ifsf_main��main�����м��˵���initCss(),initCss()�ڴ˴�ʵ��
 2008-08-19  Chen Fu Ming<chenfm@css-intelligent.com>
 	MakeFpTransMsg������unsigned char buffer[128]; Ϊ�˶�����붯ǹ�Ÿ�Ϊ128��ԭ����64
*******************************************************************/
#include <string.h>

#include "ifsf_pub.h"
#include "ifsf_dspfp.h"

extern IFSF_SHM *gpIfsf_shm; //�ýṹ��ȫ�ֵ�����ֻ��һ��,��ifsf_main.c�ж���.
static int inline RunningMsgDelay(FP_DATA *pfp);

#define _FIFO "/tmp/css_fifo.data" //��������ݹܵ�
int gCssFifoFd;//��������ݹܵ���ȫ��������

#if  1
int initCss()
{
	int trynum, ret;
	extern int errno;

	/*//����FIFO�ܵ�
	ret = IsFileExist(_FIFO);	
	//unlink(_FIFO);
	if (ret <= 0){
		for (trynum = 0; trynum < 5; trynum++) {
			ret = mkfifo(_FIFO, 0777);//O_RDWR | O_NONBLOCK
			if(ret < 0){
				UserLog("ȡ����FIFO��������.");
				continue;
			} else {
				UserLog("����ȡ��FIFO�����ɹ�.");
				break;
			}
		}
	} else {
		UserLog("ȡ����FIFO�ļ�����.");
	}

	//sleep(10);
	*/
	//�򿪹ܵ� 
	for (trynum = 0; trynum < 5; trynum++) {
		gCssFifoFd = open( _FIFO, O_RDWR| O_NONBLOCK);//O_WRONLY
		if (gCssFifoFd <=0 ) {
			UserLog( "�򿪶���ܵ��ļ�[%s]ʧ��errno[%d]\n", _FIFO ,errno);
			continue;
		} else{
			UserLog( "�򿪶���ܵ��ļ�[%s]�ɹ�", _FIFO );
			break;
		}
	}

	return 0;
}
#endif

//�ı�FP״̬. fp_stateΪ��״̬. ע��MakeFpMsgҪ����FP_Status_Message!!(Dada_Id=100),ע����������.
/*
 * func: �ı�node.fp_ad��״̬Ϊfp_state; is_cmd ���ڱ�ʶ�Ƿ��Ǻ�̨����µ�״̬�ı�
 *       ����Ҫ�ڷ���״̬��Ϣ֮ǰ�ȷ���ACK����,��ACK��������ͨ��msgbuffer����,������NULL
 * param: msgbuffer[2] �Ժ�Ϊ��׼Ack Message;
 *        msgbuffer[1] ΪAck Message�ĳ���;
 *        msgbuffer[0] Ϊevent������Ack Message�е�λ�� (0 - msgbuffer[1]-1)
 */
int ChangeFP_State(const __u8 node, const __u8 fp_ad, const __u8 fp_state, const __u8 *msgbuffer) {
	int i, ret, cnt;
	int tmp_lg = 0;  //recv_lg;
	int fp = fp_ad - 0x21;  //0,1,2,3�ż��͵�.
	DISPENSER_DATA *dispenser_data;
	FP_DATA *pfp_data;
	unsigned char buffer[64];
	const unsigned char *ack_msg = &msgbuffer[2];
	__u8 Tankcmd[5] = {0};
	__u8 Log_Noz;

	bzero(buffer, sizeof(buffer));
	if( (node <= 0) || (node > 127 ) ){
		UserLog("���ͻ��ڵ�ֵ����,ChangeFP_State ʧ��");
		return -1;
	}
	if ((fp < 0) || (fp > 3)){
		UserLog("���ͻ����͵��ַ����,ChangeFP_State ʧ��");
		return -1;
	}	
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,ChangeFP_State ʧ��");
		return -1;
	}

	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node) {
		      	break; //�ҵ��˼��ͻ��ڵ�
		}
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���, ChangeFP_State ʧ��");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	pfp_data = (FP_DATA *)(&(dispenser_data->fp_data[fp]));

	// CSSI.У����Ŀ���ݲɼ� -- ��¼��ʼ����״̬(8)
	if (gpGunShm->data_acquisition_4_TCC_adjust_flag != 0 &&
			(fp_state == 8 && pfp_data->FP_State < 8)) {
		write_tcc_adjust_data(NULL, ",,%d,%d,%d,0,55,1,,0,0",
				node, fp + 1,
				(pfp_data->Log_Noz_State == 0x04 ? 3 :
					(pfp_data->Log_Noz_State == 0x08 ? 4 : pfp_data->Log_Noz_State)));
		TCCADLog("TCC-AD: Node %d FP %d Noz %d ��¼��ʼ����", node, fp + 1, pfp_data->Log_Noz_State);
	}

	pfp_data->FP_State = fp_state;

	if (msgbuffer != NULL) {   // �Ȼظ�ACK
		ret = SendData2CD(ack_msg, msgbuffer[1], 5);
		if (ret < 0) {
			RunLog("Send command Acknowledge Message to CD failed!");
		}
	}

	ret = MakeFpMsg(node, fp_ad,buffer, &tmp_lg);
	if(ret < 0){
		UserLog("���ͻ����͵��Զ����ݹ������, ChangeFP_State ʧ��");
		return -1;
	}

	RunLog("Node %d FP %d Change FP_State to[%d]", node, fp_ad - 0x20, fp_state);

	if (gpGunShm->LiquidAdjust == 1){          //��ǰҺλ�Ǵ�У׼����
		if (fp_state == FP_State_Started){ //̧ǹ�ź�,��������̧ǹ��Ԥ�ü���
			Tankcmd[0] = 0x01;
			Tankcmd[1] = 0x00;
			Tankcmd[2] = node;                                   //�ڵ��
			Tankcmd[3] = fp + 1;                                 //����
			Log_Noz = dispenser_data->fp_data[fp].Log_Noz_State; //����ϵ��߼�ǹ��
			if ((Log_Noz & 0x01) == 0x01) {
				Tankcmd[4] = 1;
			} else if ((Log_Noz & 0x02) == 0x02) {
				Tankcmd[4] = 2;
			} else if ((Log_Noz & 0x04) == 0x04) {
				Tankcmd[4] = 3;
			} else if ((Log_Noz & 0x08) == 0x08) {
				Tankcmd[4] = 4;
			} else if ((Log_Noz & 0x10) == 0x10) {
				Tankcmd[4] = 5;
			} else if ((Log_Noz & 0x20) == 0x20) {
				Tankcmd[4] = 6;
			} else if ((Log_Noz & 0x40) == 0x40) {
				Tankcmd[4] = 7;
			} else if ((Log_Noz & 0x80) == 0x80) {
				Tankcmd[4] = 8;	
			}
			SendMsg(TANK_MSG_ID, Tankcmd, 5, 1, 3);
		}
	}	
 
	ret = SendData2CD(buffer, tmp_lg, 5);
	if (ret < 0) {
		return -1;
	}
	
	return 0;
}

//�ı��߼���ǹ״̬. ln_stateΪ��״̬. ע��MakeFpMsgҪ����FP_Status_Message!!(Dada_Id=100),ע����������.
int ChangeLog_Noz_State(const char node, const char fp_ad, const unsigned char ln_state){
	int i,ret, tmp_lg=0; //recv_lg;
	int fp=fp_ad-0x21; //0,1,2,3�ż��͵�.
	DISPENSER_DATA *dispenser_data;
	FP_DATA *pfp_data;
	unsigned char buffer[64];

	memset(buffer, 0, 64);
	if( (node <= 0) || (node > 127 ) ){
		UserLog("���ͻ��ڵ�ֵ����,ChangeLog_Noz_State ʧ��");
		return -1;
	}
	if( (fp < 0) || (fp > 3 ) ){
		UserLog("���ͻ����͵��ַ����,ChangeLog_Noz_State ʧ��,fp_ad[%x]",fp_ad);
		return -1;
	}	
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,ChangeFP_State ʧ��");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���, ChangeLog_Noz_State ʧ��");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	pfp_data =  (FP_DATA *)( &(dispenser_data->fp_data[fp])  );
	i = 0;
	if(pfp_data->Log_Noz_State  !=  ln_state){
		pfp_data->Log_Noz_State = ln_state;
		ret = MakeFpMsg(node, fp_ad,buffer, &tmp_lg);
		if(ret < 0){
			UserLog("���ͻ����͵��Զ����ݹ������, ChangeLog_Noz_State ʧ��");
			return -1;
		}
		ret = SendData2CD(buffer, tmp_lg, 5);
		if(ret < 0){
			UserLog("���ͻ�ͨѶ����,ChangeLog_Noz_State ʧ��");
			return -1;
		}
	} else {
		i++;
		if(i %100 == 0){
			ret = MakeFpMsg(node, fp_ad,buffer, &tmp_lg);
			if(ret < 0){
				UserLog("���ͻ����͵��Զ����ݹ������, ChangeLog_Noz_State ʧ��");
				return -1;
			}
			ret = SendData2CD(buffer, tmp_lg, 5);
			if(ret < 0){
				UserLog("���ͻ�ͨѶ����,ChangeLog_Noz_State ʧ��");
				return -1;
			}
		}
	}
	return 0;
}
int ChangeFP_Run_Trans(const char node, const char fp_ad, const char *amount, const char *volume){
	int i,ret, tmp_lg=0; //recv_lg;
	int fp = fp_ad - 0x21; //0,1,2,3�ż��͵�.
	DISPENSER_DATA *dispenser_data;
	FP_DATA *pfp_data;
	unsigned char buffer[64];
	memset(buffer, 0, 64);
	if( (node <= 0) || (node > 127 ) ){
		UserLog("���ͻ��ڵ�ֵ����,ChangeFP_Run_Trans ʧ��");
		return -1;
	}
	if( (fp < 0) || (fp > 3 ) ){
		UserLog("���ͻ����͵��ַ����,ChangeFP_Run_Trans ʧ��");
		return -1;
	}	
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,ChangeFP_Run_Trans ʧ��");
		return -1;
	}

	// �����δ��ɵ��ѻ�����, ���ϴ�ʵʱ��Ϣ
	if ((gpIfsf_shm->fp_offline_flag[node - 1][fp] & 0x0F) != 0) {
		return 0;
	}

	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���, ChangeFP_Run_Trans ʧ��");
		return -1;
	}

	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]));
	pfp_data =  (FP_DATA *)( &(dispenser_data->fp_data[fp])  );

	// CSSI.У����Ŀ���ݲɼ�����--��¼���һ��ʵʱ��������.�˴���¼ʵʱ����ʱ��
	if ((gpGunShm->data_acquisition_4_TCC_adjust_flag != 0) &&
		(memcmp(pfp_data->Current_Volume, volume, 5) != 0)) {
		getlocaltime(pfp_data->Current_Date_Time);
		memcpy(pfp_data->Current_Volume_4_TCC_Adjust, volume, 5);

		write_tcc_adjust_data(pfp_data->Current_Date_Time, ",,%d,%d,%d,%02x%02x.%02x,FF,1,,0,0",
			node, fp + 1,
			(pfp_data->Current_Log_Noz == 0x04 ? 3 :
				(pfp_data->Current_Log_Noz == 0x08 ? 4 : pfp_data->Current_Log_Noz)),
			pfp_data->Current_Volume_4_TCC_Adjust[2],
			pfp_data->Current_Volume_4_TCC_Adjust[3],
			pfp_data->Current_Volume_4_TCC_Adjust[4]);
	}
	// End of CSSI.У����Ŀ���ݲɼ�����

	memcpy( pfp_data->Current_Amount, amount , 5);
	memcpy( pfp_data->Current_Volume, volume, 5);
	ret = MakeFpTransMsg(node, fp_ad, buffer, &tmp_lg);
	if(ret < 0){
		UserLog("���ͻ����͵��Զ����ݹ������, ChangeFP_Run_Trans ʧ��");
		return -1;
	}

	ret = SendData2CD(buffer, tmp_lg, 5);
	if(ret < 0){
		UserLog("���ͻ�ͨѶ����,ChangeFP_Run_Trans ʧ��,node[%d]", node);
		return -1;
	}
	//RunningMsgDelay(pfp_data);    // delay 0.1 - 99.9 seconds

	return 0;
	
}
//ִ���յ�������.command����ֵ:61-67. (�·�����)
static int ExecFpCMD(const char node, const char fp_ad, const char command, const char *ack_msg){
	int i, j, ret = -1, tmpRet, cmd_lg; //recv_lg;
	int fp=fp_ad-0x21; //0,1,2,3�ż��͵�.
	DISPENSER_DATA *dispenser_data;
	FP_DATA *pfp_data;
	unsigned char chnlog;
	unsigned char tmp[5];
	unsigned char cmd[17];	

	memset(cmd, 0, 17);	
	if( (node <= 0) || (node > 127 ) ){
		UserLog("���ͻ��ڵ�ֵ����,ChangeLog_Noz_State ʧ��");
		return -1;
	}
	if( (fp < 0) || (fp > 3 ) ){
		UserLog("���ͻ����͵��ַ����,ChangeLog_Noz_State ʧ��");
		return -1;
	}	
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,ChangeFP_State ʧ��");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->node_channel[i].node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���, ChangeLog_Noz_State ʧ��");
		return -1;
	}
	chnlog =  gpIfsf_shm->node_channel[i].chnLog;
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���, ChangeLog_Noz_State ʧ��");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	pfp_data =  (FP_DATA *)( &(dispenser_data->fp_data[fp])  );
	
	tmpRet = 0;
	memset(tmp,0,5);
	switch( command ) {
		case FP_Open_FP:  //w(2)
			cmd[0] = '\x1e';
			cmd[1] = '\0';
			cmd[2] = fp_ad; //����ַ.
			memcpy(&cmd[3], ack_msg, 10);
			cmd[13] = 0x00;   // default MsgACK
			cmd[14] = command;
			cmd[15] = 0x00;   // default DataACK
			cmd_lg = 16;
			ret = SendMsg(ORD_MSG_TRAN, cmd , cmd_lg, chnlog, 3);
			if(ret < 0) {
				return ret;
			}
			RunLog( "�·�����[%s], ����[%d]", Asc2Hex(cmd, cmd_lg ), ret);
			break;
		case FP_Close_FP:  //w(3-9)
			cmd[0] = '\x1f';
			cmd[1] = '\0';
			cmd[2] =  fp_ad; //����ַ.
			memcpy(&cmd[3], ack_msg, 10);
			cmd[13] = 0x00;
			cmd[14] = command;
			cmd[15] = 0x00;
			cmd_lg = 16;
			ret = SendMsg(ORD_MSG_TRAN, cmd , cmd_lg, chnlog, 3);
			if(ret < 0) {
				return ret;
			}
			RunLog( "�·�����[%s], ����[%d]", Asc2Hex(cmd, cmd_lg ), ret);
			break;
		case FP_Release_FP:   //w(3-4)
			memset(tmp, 0, sizeof(tmp));
			if(memcmp(pfp_data->Remote_Amount_Prepay,tmp,5) !=0 ){
				cmd[0] = '\x4d';
				cmd[1] = '\0';
				cmd[2] = fp_ad; //����ַ.
			} else if (memcmp(pfp_data->Remote_Volume_Preset,tmp, 5) !=0 ){
				cmd[0] = '\x3d';
				cmd[1] = '\0';
				cmd[2] = fp_ad; //����ַ.
			} else {
				cmd[0] = '\x1d';
				cmd[1] = '\0';
				cmd[2] = fp_ad; //����ַ.
			}
			memcpy(&cmd[3], ack_msg, 10);
			cmd[13] = 0x00;   // default MsgACK
			cmd[14] = command;
			cmd[15] = 0x00;   // default DataACK
			cmd_lg = 16;
			ret = SendMsg(ORD_MSG_TRAN, cmd , cmd_lg, chnlog, 3);
			if(ret < 0) {
				return ret;
			}

			RunLog( "�·�����[%s], ����[%d]", Asc2Hex(cmd, cmd_lg ), ret);
			break;
		case FP_Terminate_FP:   //w(4-9)
			cmd[0] = '\x1c';
			cmd[1] = '\0';
			cmd[2] =  fp_ad; //����ַ.
			memcpy(&cmd[3], ack_msg, 10);
			cmd[13] = 0x00;   // default MsgACK
			cmd[14] = command;
			cmd[15] = 0x00;   // default DataACK
			cmd_lg = 16;
			ret = SendMsg(ORD_MSG_TRAN, cmd , cmd_lg, chnlog, 3);
			if (ret < 0) {
			       	return ret;
			}
			RunLog( "�·�����[%s], ����[%d]", Asc2Hex(cmd, cmd_lg ), ret);
			break;
		case FP_Suspend_FP:   //w(6,8)
			ret = -1;
			break;
		case FP_Resume_FP:   //w(7,9)
			ret = -1;
			break;
		case FP_Clear_Display:   //w(3-5)
			ret = -1;
			break;
		case FP_Leak_Command: //O   w(3)
			ret = -1;
			break;
		default:
			ret = -1;
			break;
	}
	
	return ret;
}
//������������FP_Status_Message��ֵ������,sendDataΪ������ַ��ͷ��Ϣ������,data_lg�����ܳ���
static int MakeFpMsg(const char node, const char fp_ad, unsigned char *sendBuffer, int *data_lg){
	int i,k,fp_state,tmp_lg=0; //recv_lg;
	int fp=fp_ad-0x21; //0,1,2,3�ż��͵�.
	//int er; //0,1,2,3,4,5,6,..63�Ŵ���.
	//unsigned char tmpUChar;
	DISPENSER_DATA *dispenser_data;
	FP_DATA *pfp_data;
	//int data_lg;
	unsigned char buffer[64];
	memset(buffer, 0, 64);

	if( (node <= 0) || (node > 127 ) ){
		UserLog("���ͻ��ڵ�ֵ����, MakeFpMsg ʧ��");
		return -1;
	}
	if( (fp < 0) || (fp > 3 ) ){
		UserLog("���ͻ����͵��ַ����,  MakeFpMsg ʧ��");
		return -1;
	}
	if( (NULL == sendBuffer) || (NULL == data_lg) ){
		UserLog("����Ϊ�մ���,  MakeFpMsg ʧ��");
		return -1;
		}
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,  MakeFpMsg ʧ��");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���, MakeFpMsg ʧ��");
		return -1;
	}

	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	pfp_data =  (FP_DATA *)( &(dispenser_data->fp_data[fp])  );
	fp_state = pfp_data->FP_State;
	if( fp_state >9 ) return -1;	
	
	buffer[0] = '\x02';
	buffer[1] = '\x01';
	buffer[2] = '\x01';
	buffer[3] = node;
	buffer[4] = '\0'; //recvBuffer[4]; IFSF_MC
	buffer[5] = '\x80';//unsolicited data..
	//recv_lg = recvBuffer[6];
	//recv_lg = recv_lg * 256 + recvBuffer[7];
	buffer[8] = '\x01'; //recvBuffer[8]; //Ad_lg
	buffer[9] = fp_ad;  //ad	
	//buffer[10] = recvBuffer[10]; //It's Data_Id. Data begin.	
	tmp_lg=9;

	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)FP_FP_Status_Message;
	tmp_lg++;
	buffer[tmp_lg] ='\0';
	
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)FP_FP_State;
	tmp_lg++;
	buffer[tmp_lg] ='\x01';
	tmp_lg++;
	buffer[tmp_lg] = pfp_data->FP_State;
				
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)FP_Log_Noz_State;
	tmp_lg++;
	buffer[tmp_lg] ='\x01';
	tmp_lg++;
	buffer[tmp_lg] = pfp_data->Log_Noz_State;

	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)FP_Assign_Contr_Id;
	tmp_lg++;
	buffer[tmp_lg] ='\x02';
	tmp_lg++;
	buffer[tmp_lg] = pfp_data->Assign_Contr_Id[0];
	tmp_lg++;
	buffer[tmp_lg] = pfp_data->Assign_Contr_Id[1];
				
	tmp_lg++;
	buffer[6] = (unsigned char )((tmp_lg-8) / 256) ;
	buffer[7] = (unsigned char )((tmp_lg-8) % 256);  //big end.
	*data_lg= tmp_lg;
	memcpy(sendBuffer, buffer, tmp_lg);	

#if 0
	//�������ݴ���
	static char Fuling_Flag[128][4]={0} ;//��ǹ��־����������0����.
	//bzero(Fuling_Flag,128*4);
	if (gpIfsf_shm->css_data_flag > 1){
		int ret=-1;
		if(gCssFifoFd<=0){
			UserLog("�ܵ���gCssFifoFd[%d]����,�˳�",gCssFifoFd);
			return 0;
		}
		if (pfp_data->FP_State == FP_State_Idle) {
			ret = 0;//����ret
			ret = ret + pfp_data->Current_Volume[1] + pfp_data->Current_Volume[2] + pfp_data->Current_Volume[3] \
				+ pfp_data->Current_Volume[4] ;
			if((ret == 0) &&(Fuling_Flag[node-1][fp_ad-0x21] == 1)){//����0����,Ϊ����A����ר��
				char hangGun_flag = 0xf0;
				int length= 4;
				//buffer[0]= pfp_data->Log_Noz_State;
				buffer[0] = node;
				buffer[1] = fp_ad;  //ad	
				buffer[2]= pfp_data->Log_Noz_State;
				ret=write(gCssFifoFd, (char *)&length, sizeof(int) );
				ret=write(gCssFifoFd, (char *)&hangGun_flag, 1 );//д���־0xf0
				ret=write(gCssFifoFd,  buffer,3);	
				UserLog("����0����,������Ҫ�������Ѿ�д�ܵ����.");
				Fuling_Flag[node-1][fp_ad-0x21] = 0;//��ǹ��
			}else{//��ͨ�Ŀ���״̬				
				ret=write(gCssFifoFd, (char *)&tmp_lg, sizeof(int) );
				ret=write(gCssFifoFd, buffer,tmp_lg);	
				UserLog("������Ҫ�Ŀ���״̬�����Ѿ�д�ܵ����.");
				Fuling_Flag[node-1][fp_ad-0x21] = 0;//��ǹ״̬
			}
		}else if((pfp_data->FP_State == FP_State_Calling) ||
				(pfp_data->FP_State == FP_State_Started) ||
				(pfp_data->FP_State == FP_State_Fuelling)||
				(pfp_data->FP_State == FP_State_Authorised)){
			ret=write(gCssFifoFd, (char *)&tmp_lg, sizeof(int) );
			ret=write(gCssFifoFd, buffer,tmp_lg);	
			UserLog("������Ҫ��״̬�����Ѿ�д�ܵ����.");
			Fuling_Flag[node-1][fp_ad-0x21] = 1;//��ǹ��
		}
		
	}


#endif
	
	return 0;
}

static int MakeFpTransMsg(const char node, const char fp_ad, unsigned char *sendBuffer, int *data_lg){
	int i,k,fp_state,tmp_lg=0; //recv_lg;
	int fp=fp_ad-0x21; //0,1,2,3�ż��͵�.
	//int er; //0,1,2,3,4,5,6,..63�Ŵ���.
	//unsigned char tmpUChar;
	DISPENSER_DATA *dispenser_data;
	FP_DATA *pfp_data;
	//int data_lg;
	unsigned char buffer[128];//Ϊ�����Ϊ128��ԭ����64
	memset(buffer, 0, 64);
	if( (node <= 0) || (node > 127 ) ){
		UserLog("���ͻ��ڵ�ֵ����, MakeFpMsg ʧ��");
		return -1;
	}
	if( (fp < 0) || (fp > 3 ) ){
		UserLog("���ͻ����͵��ַ����,  MakeFpMsg ʧ��");
		return -1;
	}
	if( (NULL == sendBuffer) || (NULL == data_lg) ){
		UserLog("����Ϊ�մ���,  MakeFpMsg ʧ��");
		return -1;
		}
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,  MakeFpMsg ʧ��");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���, MakeFpMsg ʧ��");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	pfp_data =  (FP_DATA *)( &(dispenser_data->fp_data[fp])  );
	fp_state = pfp_data->FP_State;
	if( fp_state >9 ) return -1;	
	
	buffer[0] = '\x02';
	buffer[1] = '\x01';
	buffer[2] = '\x01';
	buffer[3] = node;
	buffer[4] = '\0'; //recvBuffer[4]; IFSF_MC
	buffer[5] = '\x80';//unsolicited data..
	//recv_lg = recvBuffer[6];
	//recv_lg = recv_lg * 256 + recvBuffer[7];
	buffer[8] = '\x01'; //recvBuffer[8]; //Ad_lg
	buffer[9] = fp_ad;  //ad	
	//buffer[10] = recvBuffer[10]; //It's Data_Id. Data begin.	
	tmp_lg=9;

	
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)FP_FP_Running_Transaction_Message;
	tmp_lg++;
	buffer[tmp_lg] ='\0';
	
//	if (GetCfgMode()) {
           tmp_lg++;
           buffer[tmp_lg] = (unsigned char)FP_Current_Log_Noz;
           tmp_lg++;
           buffer[tmp_lg] ='\x01';
           tmp_lg++;
           buffer[tmp_lg] = pfp_data->Current_Log_Noz;
//      }

	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)FP_Current_Amount;
	tmp_lg++;
	buffer[tmp_lg] ='\x05';
	tmp_lg++;
	memcpy(&buffer[tmp_lg] , pfp_data->Current_Amount, 5);
	tmp_lg+=5;

	buffer[tmp_lg] = (unsigned char)FP_Current_Volume;
	tmp_lg++;
	buffer[tmp_lg] ='\x05';
	tmp_lg++;
	memcpy(&buffer[tmp_lg] , pfp_data->Current_Volume, 5);
	tmp_lg+=5;
				
	buffer[6] = (unsigned char )((tmp_lg-8) / 256) ;
	buffer[7] = (unsigned char )((tmp_lg-8) % 256);  //big end.
	*data_lg= tmp_lg;
	memcpy(sendBuffer, buffer, tmp_lg);
	
#if 0
	//�������ݴ���
	if(gpIfsf_shm->css_data_flag > 1){
		//UserLog("��ʼ���� ����MakeFpTransMsg ");
		int ret;
		//unsigned char cssBuffer[128];		
		//memcpy(cssBuffer, buffer, tmp_lg);
		//��ǹ��
		buffer[tmp_lg] = (unsigned char)FP_Log_Noz_State;
		tmp_lg++;
		buffer[tmp_lg] =1;
		tmp_lg++;
		buffer[tmp_lg] = pfp_data->Log_Noz_State;
		
		tmp_lg++;		
		buffer[6] = (unsigned char )((tmp_lg-8) / 256) ;
		buffer[7] = (unsigned char )((tmp_lg-8) % 256); 
		//if((pfp_data->FP_State==3)||(pfp_data->FP_State==4)||(pfp_data->FP_State==5)||(pfp_data->FP_State==8)||){
		//UserLog("MakeFpTransMsg��ʼд�� ���� FIFO");
		ret=write(gCssFifoFd, (char *)&tmp_lg, sizeof(int) );
		ret=write(gCssFifoFd, buffer,tmp_lg);	
		UserLog("���� ����MakeFpTransMsg ���");
		//}
	}
#endif
	
	return 0;
}


//������������FP_Status_Message��ֵ������.��������ĺ���,subnetOfCD��nodeOfCDΪCD��������ַ���߼��ڵ��.
//static int MakeFpMsgSend(const char node, const char fp_ad, const char subnetOfCD,const char nodeOfCD, unsigned char *sendBuffer, int *msg_lg);
//Ԥ������Ԥ����������,Ҫ����˽��Э����˿ڵ�ͨ��д����.
// dataΪ(Data_Id+Data_lg+Data_El)��������.Data_IdΪ27,28����51,52.
//static int WriteAmount_Prepay(const char node, const char fp_ad, const unsigned char *data);
//static int WriteVolume_Preset(const char node, const char fp_ad, const unsigned char *data);
//�߼���ǹ�ı亯��,Ҫ����ChangeLog_Noz_State��ChangeFP_State����.
//dataΪ(Data_Id+Data_lg+Data_El)��������.Data_IdΪ21,25,37,Data_lgΪ1.
//static int ChangeLog_Noz(const char node, const char fp_ad, const unsigned char *data);


//FP_DATA���ݲ�������. node���ͻ��߼��ڵ��,fp_ad���͵��ַ21h-24h,Ҫ�õ�gpIfsf_shm,ע��P,V����������.
int DefaultFP_DATA(const char node, const char fp_ad) 
{       //Ĭ�ϳ�ʼ��.
	int i,ret;
	DISPENSER_DATA *dispenser_data;
	FP_DATA *pfp_data;
	int fp = fp_ad -0x21; //0,1,2,3.
	unsigned char tr_seq_nb[2];

	if( (node <= 0) || (node > 127 ) ){
		UserLog("���ͻ��ڵ�ֵ����,���͵��ʼ��ʧ��");
		return -1;
	}
	if( (fp < 0) || (fp > 3) ){
		UserLog("���ͻ��ڵ�ļ��͵��ַ����,���͵��ʼ��ʧ��");
		return -1;
	}
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,���͵��ʼ��ʧ��");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node) {
		       	break; //�ҵ��˼��ͻ��ڵ�
		}
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,���͵��ʼ��ʧ��");
		return -1;
	}

	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	pfp_data =  (FP_DATA *)( &(dispenser_data->fp_data[fp]) );
	memset(pfp_data, 0 , sizeof(FP_DATA));

	//memset(&pfp_data->FP_Name[0], 0 , 8);
	pfp_data->Nb_Tran_Buffer_Not_Paid = '\x02';// 2 transaction buffer
	pfp_data->Nb_Of_Historic_Trans = '\x0F';   // set to 15
	pfp_data->Nb_Logical_Nozzle = '\x01';
	//ret = DefaultLN_DATA( node,  fp_ad, (char)0x11);
	pfp_data->Loudspeaker_Switch = '\0';
	pfp_data->Default_Fuelling_Mode = '\x01';
	pfp_data->Leak_Log_Noz_Mask = '\0';
	pfp_data->Drive_Off_Light_Switch = '\0';
	pfp_data->OPT_Light_Switch = '\0';
	pfp_data->FP_State = '\x01';            // Inoperative
	//ret = ChangeFP_State(node, fp_ad, (char)2);
	//ret = ChangeLog_Noz_State(node, fp_ad, 0x11, 2);
	pfp_data->Log_Noz_State = '\0';
	pfp_data->Assign_Contr_Id[0] = '\0';
	pfp_data->Assign_Contr_Id[1] = '\0';
	//  1 = the FP may authorise transactions as long as a free transaction buffer is available
	pfp_data->Release_Mode = '\x00' ;
	pfp_data->ZeroTR_Mode = '\x00'; // 0 = zero transaction must not be stored
	pfp_data->Log_Noz_Mask = '\xFF'; //LNZ 1 authorized.
	pfp_data->Config_Lock[0] ='\0';
	pfp_data->Config_Lock[1] ='\0';
	//pfp_data->Remote_Amount_Prepay[]
	//pfp_data->Remote_Volume_Preset[]
	pfp_data->Release_Token = '\0';
	pfp_data->Fuelling_Mode = pfp_data->Default_Fuelling_Mode;
//	printf(">>>>>��ʼִ��GetNextTrNb\n");
#if 0
	ret = GetNextTrNb(node,  fp_ad,  tr_seq_nb);
	if (ret <0 ) {
		UserLog("�¸�������Ų���ʧ��!!");
		return -1;
	}
//	printf(">>>>ִ��GetNextTrNb ok, next tr_seq_nb: %02x%02x\n", tr_seq_nb[0], tr_seq_nb[1]);
	pfp_data->Transaction_Sequence_Nb[0] = tr_seq_nb[0];
	pfp_data->Transaction_Sequence_Nb[1] = tr_seq_nb[1];
#endif
	pfp_data->Transaction_Sequence_Nb[0] = 0x00;
	pfp_data->Transaction_Sequence_Nb[1] = 0x01;  // by default, Tr_Seq_Nb is 0001
	//pfp_data->Current_TR_Seq_Nb[];
	//pfp_data->Release_Contr_Id[];
	//pfp_data->Suspend_Contr_Id[];
	//pfp_data->Current_Log_Noz;
	//pfp_data->Current_Prod_Nb;
	//pfp_data->Current_Amount[];
	//pfp_data->Current_Volume[];
	//pfp_data->Current_Unit_Price[];
	//pfp_data->Current_TR_Error_Code;
	//pfp_data->Current_Average_Temp[];
	//pfp_data->Current_Price_Set_Nb[];
	//pfp_data->Multi_Nozzle_Type;
	//pfp_data->Multi_Nozzle_State;
	//pfp_data->Local_Vol_Preset[];
	//pfp_data->Local_Amount_Prepay[];
	pfp_data->Running_Transaction_Message_Frequency[0] = 0x30;  // delay in tenths of a second
	pfp_data->Running_Transaction_Message_Frequency[1] = 0x00;  // Bcd4, by default set to 1s
	//pfp_data->FP_Alarm[];
	
	return 0;
}

int SetFP_DATA(const char node, const char fp_ad, const FP_DATA *fp_data){//���ó�ʼ��ֵ.����!!
	int i;
	DISPENSER_DATA *dispenser_data;
	FP_DATA *pfp_data;
	int fp = fp_ad -0x21; //0,1,2,3.
	//unsigned char tr_seq_nb[2];
	if( (node <= 0) || (node > 127 ) ){
		UserLog("���ͻ��ڵ�ֵ����,���͵�set ʧ��");
		return -1;
	}
	if( (fp < 0) || (fp > 3) ){
		UserLog("���ͻ��ڵ�ļ��͵��ַ����,���͵�set ʧ��");
		return -1;
	}
	if(NULL == fp_data){
		UserLog("����Ϊ�մ���,���͵�set ʧ��");
		return -1;
	}
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,���͵�set ʧ��");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,���͵�set ʧ��");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	pfp_data =  (FP_DATA *)( &(dispenser_data->fp_data[fp]) );
	memcpy(pfp_data, fp_data , sizeof(FP_DATA));
	return 0;
}
//��ȡFP_DATA. FP_ID��ַΪ21h-24h.�������Ϣ���ݵ�����Ϊ001�ش�
//recvBufferΪ�յ���data,sendBufferΪҪ���͵�data,msg_lgΪ���ɵ��ܳ���.
int ReadFP_DATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg){
	int i, ret, tmp_lg, recv_lg;
	DISPENSER_DATA *dispenser_data;
	FP_DATA *pfp_data;
	int fp = recvBuffer[9] -0x21; //0,1,2,3.
	unsigned char node = recvBuffer[1];
	unsigned char buffer[512];
	memset(buffer, 0, sizeof(buffer));
	
	if( (NULL == recvBuffer) || (NULL == sendBuffer) || (NULL == msg_lg) ){		
		UserLog("����Ϊ�մ���,���͵�Read ʧ��");
		return -1;
	}
	
	if( (node <= 0) || (node > 127 ) ){
		UserLog("���ͻ��ڵ�ֵ����,���͵�Read ʧ��");
		return -1;
		}
	if( (fp < 0) || (fp > 3) ){
		UserLog("���ͻ��ڵ�ļ��͵��ַ����,���͵�Read ʧ��");
		return -1;
		}
	
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,���͵�Read ʧ��");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,���͵�Request  ʧ��");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	pfp_data =  (FP_DATA *)( &(dispenser_data->fp_data[fp]) );
	
	buffer[0] = recvBuffer[2];
	buffer[1] = recvBuffer[3];
	buffer[2] = recvBuffer[0];
	buffer[3] = recvBuffer[1];
	buffer[4] = '\0';  //IFSF_MC
	buffer[5] =  ((recvBuffer[5] & 0x1f)|0x20); //answer
	recv_lg = recvBuffer[6];  //M_Lg
	recv_lg = recv_lg * 256 + recvBuffer[7];
	buffer[8] = '\x01'; //ad_lg
	buffer[9] =  recvBuffer[9]; //ad;
	//buffer[10] = recvBuffer[10]; //It's Data_Id. Data begin.
	tmp_lg = 9;

	for(i=0; i < (recv_lg - 2); i++){
		switch( recvBuffer[10+i] ){
		case FP_FP_Name:  //O ,w(1-2)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_FP_Name;
			tmp_lg++;
			buffer[tmp_lg] ='\0';
			break;
		case FP_Nb_Tran_Buffer_Not_Paid: //(1-15),w(1-2)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Nb_Tran_Buffer_Not_Paid;
			tmp_lg++;
			buffer[tmp_lg] ='\x01';
			tmp_lg++;
			buffer[tmp_lg] = pfp_data->Nb_Tran_Buffer_Not_Paid;
			break;
		case FP_Nb_Of_Historic_Trans://(1-15),w(1-2)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Nb_Of_Historic_Trans;
			tmp_lg++;
			buffer[tmp_lg] ='\x01';
			tmp_lg++;
			buffer[tmp_lg] = pfp_data->Nb_Of_Historic_Trans;
			break;
		case FP_Nb_Logical_Nozzle://(1-8),w(1-2)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Nb_Logical_Nozzle;
			tmp_lg++;
			buffer[tmp_lg] ='\x01';
			tmp_lg++;
			buffer[tmp_lg] = pfp_data->Nb_Logical_Nozzle;
			break;
		case FP_Default_Fuelling_Mode://(1-8),w(1-2)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Default_Fuelling_Mode;
			tmp_lg++;
			buffer[tmp_lg] ='\x01';
			tmp_lg++;
			buffer[tmp_lg] = pfp_data->Default_Fuelling_Mode;
			break;
		case FP_Leak_Log_Noz_Mask://bit set, ,w(3),not read.
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Leak_Log_Noz_Mask;
			tmp_lg++;
			buffer[tmp_lg] ='\x01';
			tmp_lg++;
			buffer[tmp_lg] = pfp_data->Leak_Log_Noz_Mask;
			break;
		case FP_FP_State: //(1-9),not write.
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_FP_State;
			tmp_lg++;
			buffer[tmp_lg] ='\x01';
			tmp_lg++;
			buffer[tmp_lg] = pfp_data->FP_State;
			break;
		case FP_Log_Noz_State: //(1-15),not write
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Log_Noz_State;
			tmp_lg++;
			buffer[tmp_lg] ='\x01';
			tmp_lg++;
			buffer[tmp_lg] = pfp_data->Log_Noz_State;
			break;
		case FP_Assign_Contr_Id: //bin16,w(2-4)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Assign_Contr_Id;
			tmp_lg++;
			buffer[tmp_lg] ='\x02';
			tmp_lg++;
			buffer[tmp_lg] = pfp_data->Assign_Contr_Id[0];
			tmp_lg++;
			buffer[tmp_lg] = pfp_data->Assign_Contr_Id[1];
			break;
		case FP_Release_Mode: //O, (0-1),w(2-4)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Release_Mode; //O			
			tmp_lg++;
			buffer[tmp_lg] ='\0';
			break;
		case FP_ZeroTR_Mode: //(0-1),w(2-4)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_ZeroTR_Mode;
			tmp_lg++;
			buffer[tmp_lg] ='\x01';
			tmp_lg++;
			buffer[tmp_lg] = pfp_data->ZeroTR_Mode;
			break;
		case FP_Log_Noz_Mask: //bit set ,w(2-4)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Log_Noz_Mask;
			tmp_lg++;
			buffer[tmp_lg] ='\x01';
			tmp_lg++;
			buffer[tmp_lg] = pfp_data->Log_Noz_Mask;
			break;
		case FP_Config_Lock: //bin16,w(1-2),r(1,2)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Config_Lock;
			if(  (1 == pfp_data->FP_State ) ||  (2 == pfp_data->FP_State )  ){
				tmp_lg++;
				buffer[tmp_lg] ='\x02';
				tmp_lg++;
				buffer[tmp_lg] = pfp_data->Config_Lock[0];
				tmp_lg++;
				buffer[tmp_lg] = pfp_data->Config_Lock[1];
				}
			else {
				tmp_lg++;
				buffer[tmp_lg] ='\0';
				}
			break;
		case FP_Remote_Amount_Prepay: //(1-15),w(1-2)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Remote_Amount_Prepay;
			tmp_lg++;
			buffer[tmp_lg] ='\x05';
			tmp_lg++;
			buffer[tmp_lg] = pfp_data->Remote_Amount_Prepay[0];
			tmp_lg++;
			buffer[tmp_lg] = pfp_data->Remote_Amount_Prepay[1];
			tmp_lg++;
			buffer[tmp_lg] = pfp_data->Remote_Amount_Prepay[2];
			tmp_lg++;
			buffer[tmp_lg] = pfp_data->Remote_Amount_Prepay[3];
			tmp_lg++;
			buffer[tmp_lg] = pfp_data->Remote_Amount_Prepay[4];
			break;
		case FP_Remote_Volume_Preset: // w(3-4)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Remote_Volume_Preset;
			tmp_lg++;
			buffer[tmp_lg] ='\x05';
			tmp_lg++;
			buffer[tmp_lg] = pfp_data->Remote_Volume_Preset[0];
			tmp_lg++;
			buffer[tmp_lg] = pfp_data->Remote_Volume_Preset[1];
			tmp_lg++;
			buffer[tmp_lg] = pfp_data->Remote_Volume_Preset[2];
			tmp_lg++;
			buffer[tmp_lg] = pfp_data->Remote_Volume_Preset[3];
			tmp_lg++;
			buffer[tmp_lg] = pfp_data->Remote_Volume_Preset[4];
			break;
		case FP_Release_Token: //w(3-4)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Release_Token;
			tmp_lg++;
			buffer[tmp_lg] ='\x01';
			tmp_lg++;
			buffer[tmp_lg] = pfp_data->Release_Token;
			break;
		case FP_Fuelling_Mode: //(1-8),w(3-4)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Fuelling_Mode;
			tmp_lg++;
			buffer[tmp_lg] ='\x01';
			tmp_lg++;
			buffer[tmp_lg] = pfp_data->Fuelling_Mode;
			break;
		case FP_Transaction_Sequence_Nb: //bcd4, w(1-2)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Transaction_Sequence_Nb;
			tmp_lg++;
			buffer[tmp_lg] ='\x02';
			tmp_lg++;
			buffer[tmp_lg] = pfp_data->Transaction_Sequence_Nb[0];
			tmp_lg++;
			buffer[tmp_lg] = pfp_data->Transaction_Sequence_Nb[1];
			break;
		case FP_Current_TR_Seq_Nb: //bcd4, r(6-9), not write.
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Current_TR_Seq_Nb;
			if(  (pfp_data->FP_State >=6  ) &&  ( pfp_data->FP_State <=9 )  ){
				tmp_lg++;
				buffer[tmp_lg] ='\x02';
				tmp_lg++;
				buffer[tmp_lg] = pfp_data->Transaction_Sequence_Nb[0];
				tmp_lg++;
				buffer[tmp_lg] = pfp_data->Transaction_Sequence_Nb[1];
				}
			else {
				tmp_lg++;
				buffer[tmp_lg] ='\0';
				}			
			break;
		case FP_Release_Contr_Id: //,w(3-4) r(3-9)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Release_Contr_Id;
			if(  (pfp_data->FP_State >=3  ) &&  ( pfp_data->FP_State <=9 )  ){
				tmp_lg++;
				buffer[tmp_lg] ='\x02';
				tmp_lg++;
				buffer[tmp_lg] = pfp_data->Release_Contr_Id[0];
				tmp_lg++;
				buffer[tmp_lg] = pfp_data->Release_Contr_Id[1];
				}
			else {
				tmp_lg++;
				buffer[tmp_lg] ='\0';
				}			
			break;
		case FP_Suspend_Contr_Id: //r(7,9),w(6,8)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Suspend_Contr_Id;
			if(  (pfp_data->FP_State ==7 ) ||  ( pfp_data->FP_State ==9 )  ){
				tmp_lg++;
				buffer[tmp_lg] ='\x02';
				tmp_lg++;
				buffer[tmp_lg] = pfp_data->Suspend_Contr_Id[0];
				tmp_lg++;
				buffer[tmp_lg] = pfp_data->Suspend_Contr_Id[1];
				}
			else {
				tmp_lg++;
				buffer[tmp_lg] ='\0';
				}		
			break;		
		case FP_Current_Amount:  //r(6-9),not writebale.
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Current_Amount;
			if(  (pfp_data->FP_State >=6  ) &&  ( pfp_data->FP_State <=9 )  ){
				tmp_lg++;
				buffer[tmp_lg] ='\x05';
				tmp_lg++;
				buffer[tmp_lg] = pfp_data->Current_Amount[0];
				tmp_lg++;
				buffer[tmp_lg] = pfp_data->Current_Amount[1];
				tmp_lg++;
				buffer[tmp_lg] = pfp_data->Current_Amount[2];
				tmp_lg++;
				buffer[tmp_lg] = pfp_data->Current_Amount[3];
				tmp_lg++;
				buffer[tmp_lg] = pfp_data->Current_Amount[4];
				}
			else {
				tmp_lg++;
				buffer[tmp_lg] ='\0';
				}
			break;
		case FP_Current_Volume:  //r(6-9),not writebale.
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Current_Volume;
			if(  (pfp_data->FP_State >=6  ) &&  ( pfp_data->FP_State <=9 )  ){
				tmp_lg++;
				buffer[tmp_lg] ='\x05';
				tmp_lg++;
				buffer[tmp_lg] = pfp_data->Current_Volume[0];
				tmp_lg++;
				buffer[tmp_lg] = pfp_data->Current_Volume[1];
				tmp_lg++;
				buffer[tmp_lg] = pfp_data->Current_Volume[2];
				tmp_lg++;
				buffer[tmp_lg] = pfp_data->Current_Volume[3];
				tmp_lg++;
				buffer[tmp_lg] = pfp_data->Current_Volume[4];
				}
			else {
				tmp_lg++;
				buffer[tmp_lg] ='\0';
				}
			break;
		case FP_Current_Unit_Price: //r(6-9),not writebale.
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Current_Unit_Price;
			if(  (pfp_data->FP_State >=6  ) &&  ( pfp_data->FP_State <=9 )  ){
				tmp_lg++;
				buffer[tmp_lg] ='\x04';
				tmp_lg++;
				buffer[tmp_lg] = pfp_data->Current_Unit_Price[0];
				tmp_lg++;
				buffer[tmp_lg] = pfp_data->Current_Unit_Price[1];
				tmp_lg++;
				buffer[tmp_lg] = pfp_data->Current_Unit_Price[2];
				tmp_lg++;
				buffer[tmp_lg] = pfp_data->Current_Unit_Price[3];
				//tmp_lg++;
				//buffer[tmp_lg] = pfp_data->Current_Unit_Price[4];
				}
			else {
				tmp_lg++;
				buffer[tmp_lg] ='\0';
				}
			break;
		case FP_Current_Log_Noz: //r(6-9),not writebale.
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Current_Log_Noz;
			if(  (pfp_data->FP_State >=6  ) &&  ( pfp_data->FP_State <=9 )  ){
				tmp_lg++;
				buffer[tmp_lg] ='\x01';
				tmp_lg++;
				buffer[tmp_lg] = pfp_data->Current_Log_Noz;				
				}
			else {
				tmp_lg++;
				buffer[tmp_lg] ='\0';
				}
			break;
		case FP_Current_Prod_Nb: //bcd8, r(6-9),not writebale.
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Current_Prod_Nb;
			if(  (pfp_data->FP_State >=6  ) &&  ( pfp_data->FP_State <=9 )  ){
				tmp_lg++;
				buffer[tmp_lg] ='\x05';
				tmp_lg++;
				buffer[tmp_lg] = pfp_data->Current_Prod_Nb[0];
				tmp_lg++;
				buffer[tmp_lg] = pfp_data->Current_Prod_Nb[1];
				tmp_lg++;
				buffer[tmp_lg] = pfp_data->Current_Prod_Nb[2];
				tmp_lg++;
				buffer[tmp_lg] = pfp_data->Current_Prod_Nb[3];
				}
			else {
				tmp_lg++;
				buffer[tmp_lg] ='\0';
				}
			break;
		case FP_Current_TR_Error_Code: //r(6-9),not writebale.
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Current_TR_Error_Code;
			if(  (pfp_data->FP_State >=6  ) &&  ( pfp_data->FP_State <=9 )  ){
				tmp_lg++;
				buffer[tmp_lg] ='\x01';
				tmp_lg++;
				buffer[tmp_lg] = pfp_data->Current_Log_Noz;				
				}
			else {
				tmp_lg++;
				buffer[tmp_lg] ='\0';
				}
			break;
		case FP_Running_Transaction_Message_Frequency:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Running_Transaction_Message_Frequency;
			tmp_lg++;
			buffer[tmp_lg] = '\x02';
			tmp_lg++;
			buffer[tmp_lg] = pfp_data->Running_Transaction_Message_Frequency[0];
			tmp_lg++;
			buffer[tmp_lg] = pfp_data->Running_Transaction_Message_Frequency[1];
			break;
			//tmp_lg++;
			//buffer[tmp_lg] = (unsigned char)FP_Current_Average_Temp;	
			//tmp_lg++;
			//buffer[tmp_lg] = (unsigned char)FP_Current_Price_Set_Nb;
			
			//tmp_lg++;
			//buffer[tmp_lg] = (unsigned char)FP_Multi_Nozzle_Type;
			//tmp_lg++;
			//buffer[tmp_lg] = (unsigned char)FP_Multi_Nozzle_State;
			//tmp_lg++;
			//buffer[tmp_lg] = (unsigned char)FP_Multi_Nozzle_Status_Message;
			//tmp_lg++;
			//buffer[tmp_lg] = (unsigned char)FP_Local_Vol_Preset;
			//tmp_lg++;
			//buffer[tmp_lg] = (unsigned char)FP_Local_Amount_Prepay;
			//tmp_lg++;
			//buffer[tmp_lg] = (unsigned char)FP_FP_Alarm;	
		default:
			tmp_lg++;
			buffer[tmp_lg] = recvBuffer[i + 10] ;
			tmp_lg++;
			buffer[tmp_lg] ='\0';
			break;
		}
	}

	tmp_lg++;
	buffer[6] = (unsigned char )((tmp_lg-8) / 256) ;
	buffer[7] = (unsigned char )((tmp_lg -8)%256);  //big end.
	*msg_lg= tmp_lg;
	memcpy(sendBuffer, buffer, tmp_lg);	

	return 0;
}

//дFP_PR_DATA����.recvBufferΪ�յ���data,sendBufferΪҪ�ظ���data,�������Ϣ���ݵ�����Ϊ111Ӧ��
//msg_lgΪ���ɵ��ܳ���,Ϊ0��ʾ����ҪӦ����Ϣ,sendBuffer��msg_lg����ҪӦ����Ϣ��������ΪNULL.
//ret: 0 - ִ�гɹ�,ACK���ϲ㷢��; 1 - ִ�гɹ�, ACK��lischnxxx����; -1 - ִ��ʧ��
int WriteFP_DATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg){
	int i, j, ret, tmp_lg, recv_lg, lg, data_lg;
	DISPENSER_DATA *dispenser_data;
	FP_DATA *pfp_data;
	int fp;      //0,1,2,3.
	unsigned char tmpMSAck, tmpDataAck;
	unsigned char node;
	unsigned char chnlog;
	unsigned char msgbuffer[256];
	unsigned char *buffer;
	unsigned char pre_buffer[5] = {0};
	unsigned char ack_msg[13] = {0};
	const unsigned char zero_array[5] = {0};
	unsigned char err_event = 0x00;    // ��¼�ڲ�����״̬�½��е�event, 0xFF��ʾ������ֵ

	memset(msgbuffer, 0, sizeof(msgbuffer));
	buffer = &msgbuffer[5];   // 0-4 eg. 1E 00 21 (pos of 1E in data) data_lg data(nBytes)

	if( NULL == recvBuffer ){		
		UserLog("����Ϊ�մ���,���͵�Write ʧ��");
		return -1;
	}
	fp = recvBuffer[9] - 0x21; //0,1,2,3.
	node = recvBuffer[1];
	if( (node <= 0) || (node > 127 ) ){
		UserLog("���ͻ��ڵ�ֵ����,���͵�Write ʧ��");
		return -1;
	}
	if( (fp < 0) || (fp > 3) ){
		UserLog("���ͻ��ڵ�ļ��͵��ַ����,���͵�Write ʧ��");
		return -1;
	}
	
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,���͵�Write ʧ��");
		return -1;
	}
	for(i = 0; i < MAX_CHANNEL_CNT; i++){
		if (node == gpIfsf_shm->node_channel[i].node) {
		       	break; //�ҵ��˼��ͻ��ڵ�
		}
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,���͵�Write  ʧ��");
		return -1;
	}
	chnlog = gpIfsf_shm->node_channel[i].chnLog;

	for(i = 0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node) {
		      	break; //�ҵ��˼��ͻ��ڵ�
		}
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,���͵�Write  ʧ��");
		return -1;
	}

	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	pfp_data =  (FP_DATA *)( &(dispenser_data->fp_data[fp]) );
//	RunLog("WriteFP_DATA, node_i: %d, fp: %d", i, fp);
	
	buffer[0] = recvBuffer[2];
	buffer[1] = recvBuffer[3];
	buffer[2] = recvBuffer[0];
	buffer[3] = recvBuffer[1];
	buffer[4] = '\0';                            // IFSF_MC
	buffer[5] =  ((recvBuffer[5] & 0x1f) |0xe0); // Ack
	recv_lg = recvBuffer[6];
	recv_lg = recv_lg * 256 + recvBuffer[7];
	buffer[8] = '\x01'; //ad_lg
	buffer[9] =  recvBuffer[9]; //ad;
	//buffer[10] =                      ; //It's MS_Ack.
	//buffer[11] = recvBuffer[10]; //It's Data_Id. Data begin.
	tmp_lg = 10; //has  a char, MS_Ack 
	i = 0;
	ret = 0;
	tmpMSAck = '\0'; // all is  ok.

	memcpy(ack_msg, buffer, 10);
	ack_msg[6] = 0x00;
	ack_msg[7] = 0x05;

	while(i < (recv_lg - 2)) {
		tmpDataAck = '\0'; //ok ack.
		lg =  (int )recvBuffer[11+i]; //Data_Lg.
//		UserLog("Data ID:%02x,", recvBuffer[10+i]);
		switch( recvBuffer[10+i] ) {
		case FP_FP_Name:  //O ,w(1-2)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_FP_Name;
			tmpDataAck = '\x04';  
			tmpMSAck = '\x05';
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case FP_Nb_Tran_Buffer_Not_Paid: //(1-15),w(1-2)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Nb_Tran_Buffer_Not_Paid;
			if((lg== 1) && (pfp_data->FP_State  < 3) &&
				(recvBuffer[12+i] >= 1) && (recvBuffer[12+i] <= 15) ) {
				pfp_data->Nb_Tran_Buffer_Not_Paid = recvBuffer[12+i];
			} else  if (pfp_data->FP_State  >= 3)  { 
				tmpDataAck = '\x02';
				tmpMSAck = '\x05'; 
			} else  { 
				tmpDataAck = '\x01';
				tmpMSAck = '\x05';
			}			
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case FP_Nb_Of_Historic_Trans://(1-15),w(1-2)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Nb_Of_Historic_Trans;
			if((lg== 1) && (pfp_data->FP_State  < 3) &&
				(recvBuffer[12+i] >= 1) && (recvBuffer[12+i] <= 15)) {
				pfp_data->Nb_Tran_Buffer_Not_Paid = recvBuffer[12+i];
			} else  if (pfp_data->FP_State  >= 3)  { 
				tmpDataAck = '\x02';
				tmpMSAck = '\x05'; 
			} else  { 
				tmpDataAck = '\x01';
				tmpMSAck = '\x05';
			}			
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case FP_Nb_Logical_Nozzle://(1-8),w(1-2)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Nb_Logical_Nozzle;
			/*
			if( ( lg== 1 ) && (pfp_data->FP_State  < 3) && (recvBuffer[12+i] >= 1) && (recvBuffer[12+i] <= 8) ) 
				pfp_data->Nb_Logical_Nozzle = recvBuffer[12+i];
			else  if (pfp_data->FP_State  >= 3)  { 
				tmpDataAck = '\x02';
				tmpMSAck = '\x05'; 
				}
			else  { 
				tmpDataAck = '\x01';
				tmpMSAck = '\x05';
				}
			*/
			tmpDataAck = '\x02';
			tmpMSAck = '\x05'; 
			
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case FP_Default_Fuelling_Mode://(1-8),w(1-2) //only 1 now, so not writable.
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Default_Fuelling_Mode;
			if ((lg == 1) && (pfp_data->FP_State < 3) && (recvBuffer[12 + i] <= FP_FM_CNT)) {
				pfp_data->Default_Fuelling_Mode = recvBuffer[12+i];
			} else if (pfp_data->FP_State  >= 3)  { 
				tmpDataAck = '\x02';
				tmpMSAck = '\x05'; 
			} else { 
				tmpDataAck = '\x01';
				tmpMSAck = '\x05';
			}
				
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case FP_Leak_Log_Noz_Mask://bit set, ,w(3),not read.
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Leak_Log_Noz_Mask;
			/*
			if( ( lg== 1 ) && (pfp_data->FP_State  == 3)   )  //only 1.
				pfp_data->Leak_Log_Noz_Mask = recvBuffer[12+i];
			else  if (pfp_data->FP_State  != 3)  { 
				tmpDataAck = '\x02';
				tmpMSAck = '\x05'; 
				}
			else  { 
				tmpDataAck = '\x01';
				tmpMSAck = '\x05';
				}
			*/
			tmpDataAck = '\x02';
			tmpMSAck = '\x05'; 
				
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case FP_FP_State: //(1-9),not write.
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_FP_State;
			tmpDataAck = '\x02';  
			tmpMSAck = '\x05';
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case FP_Log_Noz_State: //(1-15),not write
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Log_Noz_State;
			tmpDataAck = '\x02';  
			tmpMSAck = '\x05';
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case FP_Assign_Contr_Id: //bin16,w(2-4)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Assign_Contr_Id;
			if( ( lg== 2 ) && (pfp_data->FP_State  >=2) && (pfp_data->FP_State  <= 4)   ) { 
				pfp_data->Assign_Contr_Id[0] = recvBuffer[12+i];
				pfp_data->Assign_Contr_Id[1]= recvBuffer[13+i];
				}
			else  if ((pfp_data->FP_State  < 2) || (pfp_data->FP_State  > 4) )  { 
				tmpDataAck = '\x02';
				tmpMSAck = '\x05'; 
				}
			else  { 
				tmpDataAck = '\x01';
				tmpMSAck = '\x05';
				}			
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case FP_Release_Mode: //O, (0-1),w(2-4)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Release_Mode; //O			
			tmpDataAck = '\x04';  
			tmpMSAck = '\x05';
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case FP_ZeroTR_Mode: //(0-1),w(2-4) 
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_ZeroTR_Mode;
			/*
			if( ( lg== 2 ) && (pfp_data->FP_State  >= 2) && (pfp_data->FP_State  <= 4)   \
				&& (recvBuffer[12+i] ==0) && (recvBuffer[12+i] == 1)  )  //only 1.
				pfp_data->ZeroTR_Mode = recvBuffer[12+i];				
			else  if ((pfp_data->FP_State  < 2) || (pfp_data->FP_State  > 4) )  { 
				tmpDataAck = '\x02';
				tmpMSAck = '\x05'; 
				}
			else  { 
				tmpDataAck = '\x01';
				tmpMSAck = '\x05';
				}
			*/
			tmpDataAck = '\x02';
			tmpMSAck = '\x05'; 
				
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case FP_Log_Noz_Mask: //bit set ,w(2-4)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Log_Noz_Mask;
			if ((lg == 1) && (pfp_data->FP_State >= 2) && (pfp_data->FP_State <= 4)) {
				pfp_data->Log_Noz_Mask = recvBuffer[12 + i];				
				RunLog("Log_Noz_Mask:%02x", pfp_data->Log_Noz_Mask);
			} else  if ((pfp_data->FP_State < 2) || (pfp_data->FP_State  > 4)) { 
				tmpDataAck = '\x02';
				tmpMSAck = '\x05'; 
			} else  { 
				tmpDataAck = '\x01';
				tmpMSAck = '\x05';
			}
				
			tmp_lg++;
			buffer[tmp_lg] = tmpDataAck;
			i = i + 2 + lg;
			break;
		case FP_Config_Lock: //bin16,w(1-2)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Config_Lock;
			if( ( lg== 2 ) && (pfp_data->FP_State  < 3)  ) {
				pfp_data->Config_Lock[0] = recvBuffer[12+i];
				pfp_data->Config_Lock[1] = recvBuffer[13+i];
			} else  if (pfp_data->FP_State  >= 3)  { 
				tmpDataAck = '\x02';
				tmpMSAck = '\x05'; 
			} else  { 
				tmpDataAck = '\x01';
				tmpMSAck = '\x05';
			}			
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case FP_Remote_Amount_Prepay: //w(3-4)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Remote_Amount_Prepay;
			if ((lg == 5) && (pfp_data->FP_State >= 3) && (pfp_data->FP_State  <= 5)) {
				memcpy(pre_buffer, &recvBuffer[12 + i], 5);
				//TwoDecPoint(pre_buffer, 5); // for maindian
				RunLog("NO TwoDecPoint: %s", Asc2Hex(pre_buffer, 5));

#if 0
				for(j=0; j<lg; j++) {
					pfp_data->Remote_Amount_Prepay[5 - lg + j] = pre_buffer[i + j];
				}
#endif
				memcpy(&(pfp_data->Remote_Amount_Prepay), pre_buffer, 5);
			//	RunLog("Remote_Amount_Prepay: %s", Asc2Hex(pfp_data->Remote_Amount_Prepay, 5));
			} else if ((pfp_data->FP_State != 3) && (pfp_data->FP_State != 4)) { 
				tmpDataAck = '\x02';
				tmpMSAck = '\x05'; 
			} else  { 
				tmpDataAck = '\x01';
				tmpMSAck = '\x05';
			}			
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case FP_Remote_Volume_Preset: // w(3-4)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Remote_Volume_Preset;
			if ((lg == 5) && (pfp_data->FP_State >=3) && (pfp_data->FP_State <= 5)) {
				memcpy(pre_buffer, &recvBuffer[12 + i], 5);
				TwoDecPoint(pre_buffer, 5);
				RunLog("after TwoDecPoint: %s", Asc2Hex(pre_buffer, 5));

#if 0
				for(j=0; j<lg; j++) {
//					pfp_data->Remote_Volume_Preset[5-lg+j] = recvBuffer[12+i+j];
					pfp_data->Remote_Volume_Preset[5 - lg + j] = pre_buffer[i + j];
				}
#endif
				memcpy(&(pfp_data->Remote_Volume_Preset), pre_buffer, 5);
			//	RunLog("Remote_Volume_Preset: %s", Asc2Hex(pfp_data->Remote_Volume_Preset, 5));
			} else if ((pfp_data->FP_State != 3)  && (pfp_data->FP_State != 4)) { 
				tmpDataAck = '\x02';
				tmpMSAck = '\x05'; 
			} else  { 
				tmpDataAck = '\x01';
				tmpMSAck = '\x05';
			}			
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case FP_Release_Token: //w(3-4)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Release_Token;
			if( ( lg == 1 ) && (pfp_data->FP_State  >=3) && (pfp_data->FP_State  <=4) ) {
					pfp_data->Release_Token = recvBuffer[12+i];
			} else  if ((pfp_data->FP_State  !=3)  && (pfp_data->FP_State  != 4))  { 
				tmpDataAck = '\x02';
				tmpMSAck = '\x05'; 
			} else  { 
				tmpDataAck = '\x01';
				tmpMSAck = '\x05';
			}			
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case FP_Fuelling_Mode: //(1-8),w(3-4)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Fuelling_Mode;
			if ((lg == 1) && (pfp_data->FP_State >=3) && (pfp_data->FP_State <= 4) &&
				(recvBuffer[12 + i] >= 1) && (recvBuffer[12 + i] <= FP_FM_CNT)) {
					pfp_data->Fuelling_Mode = recvBuffer[12 + i];
			} else if ((pfp_data->FP_State != 3) && (pfp_data->FP_State != 4)) { 
				tmpDataAck = '\x02';
				tmpMSAck = '\x05'; 
			} else { 
				tmpDataAck = '\x01';
				tmpMSAck = '\x05';
			}
				
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case FP_Transaction_Sequence_Nb: //bcd4, w(1-2)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Transaction_Sequence_Nb;
			if( ( lg == 2 ) && (pfp_data->FP_State  >=1) && (pfp_data->FP_State  <=2) ) {
					pfp_data->Transaction_Sequence_Nb[0] = recvBuffer[12+i];
					pfp_data->Transaction_Sequence_Nb[1] = recvBuffer[13+i];
			} else  if ( (pfp_data->FP_State  !=1)  && (pfp_data->FP_State  != 2)  )  { 
				tmpDataAck = '\x02';
				tmpMSAck = '\x05'; 
			} else  { 
				tmpDataAck = '\x01';
				tmpMSAck = '\x05';
			}			
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case FP_Current_TR_Seq_Nb: //bcd4, r(6-9), not write.
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Current_TR_Seq_Nb;
			tmpDataAck = '\x02';  
			tmpMSAck = '\x05';
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case FP_Release_Contr_Id: //,w(3-4) r(3-9)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Release_Contr_Id;
			if( ( lg == 2 ) && (pfp_data->FP_State  >=3) && (pfp_data->FP_State  <=4 ) ) {
					pfp_data->Release_Contr_Id[0] = recvBuffer[12+i];
					pfp_data->Release_Contr_Id[1] = recvBuffer[13+i];
			} else  if ( (pfp_data->FP_State  !=3)  || (pfp_data->FP_State  != 4)  )  { 
				tmpDataAck = '\x02';
				tmpMSAck = '\x05'; 
			} else  { 
				tmpDataAck = '\x01';
				tmpMSAck = '\x05';
			}			
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case FP_Suspend_Contr_Id: //r(7,9),w(6,8)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Suspend_Contr_Id;
			if( ( lg == 2 ) &&  ( (pfp_data->FP_State  ==6) || (pfp_data->FP_State  == 8 ) )   ) {
					pfp_data->Suspend_Contr_Id[0] = recvBuffer[12+i];
					pfp_data->Suspend_Contr_Id[1] = recvBuffer[13+i];
			} else  if ( (pfp_data->FP_State  !=6)  || (pfp_data->FP_State  != 8)  )  { 
				tmpDataAck = '\x02';
				tmpMSAck = '\x05'; 
			} else  { 
				tmpDataAck = '\x01';
				tmpMSAck = '\x05';
			}			
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case FP_Running_Transaction_Message_Frequency:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Running_Transaction_Message_Frequency;
			pfp_data->Running_Transaction_Message_Frequency[0] = recvBuffer[12 + i];
			pfp_data->Running_Transaction_Message_Frequency[1] = recvBuffer[13 + i];
			tmp_lg++;
			buffer[tmp_lg] = tmpDataAck;
			i = i + 2 + lg;
			break;
		case FP_Current_Amount:  //r(6-9),not writebale.
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Current_Amount;
			tmpDataAck = '\x02';  
			tmpMSAck = '\x05';
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case FP_Current_Volume:  //r(6-9),not writebale.
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Current_Volume;
			tmpDataAck = '\x02';  
			tmpMSAck = '\x05';
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case FP_Current_Unit_Price: //r(6-9),not writebale.
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Current_Unit_Price;
			tmpDataAck = '\x02';  
			tmpMSAck = '\x05';
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case FP_Current_Log_Noz: //r(6-9),not writebale.
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Current_Log_Noz;
			tmpDataAck = '\x02';  
			tmpMSAck = '\x05';
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case FP_Current_Prod_Nb: //bcd8, r(6-9),not writebale.
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Current_Prod_Nb;
			tmpDataAck = '\x02';  
			tmpMSAck = '\x05';
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case FP_Current_TR_Error_Code: //r(6-9),not writebale.
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Current_TR_Error_Code;
			tmpDataAck = '\x02';  
			tmpMSAck = '\x05';
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
			//tmp_lg++;
			//buffer[tmp_lg] = (unsigned char)FP_Current_Average_Temp;	
			//tmp_lg++;
			//buffer[tmp_lg] = (unsigned char)FP_Current_Price_Set_Nb;
			
			//tmp_lg++;
			//buffer[tmp_lg] = (unsigned char)FP_Multi_Nozzle_Type;
			//tmp_lg++;
			//buffer[tmp_lg] = (unsigned char)FP_Multi_Nozzle_State;
			//tmp_lg++;
			//buffer[tmp_lg] = (unsigned char)FP_Multi_Nozzle_Status_Message;
			//tmp_lg++;
			//buffer[tmp_lg] = (unsigned char)FP_Local_Vol_Preset;
			//tmp_lg++;
			//buffer[tmp_lg] = (unsigned char)FP_Local_Amount_Prepay;
			//tmp_lg++;
			//buffer[tmp_lg] = (unsigned char)FP_FP_Alarm;

			//COMMAND:
		case FP_Open_FP:  //w(2)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Open_FP;			
			msgbuffer[3] = tmp_lg;  // cmd_pos
			if ((lg == 0) && (pfp_data->FP_State == 2)) {
				pfp_data->Assign_Contr_Id[0] = recvBuffer[2];
				pfp_data->Assign_Contr_Id[1] = recvBuffer[3];
				msgbuffer[0] = 0x1e;    // pcd_cmd
			} else if(pfp_data->FP_State != 2) { 
				err_event = Open_FP;
				tmpDataAck = '\x03';
				tmpMSAck = '\x05'; 
			} else { 
				err_event = Invalid_Event;        // ������ֵ
				tmpDataAck = '\x05';
				tmpMSAck = '\x05';
			} 
			tmp_lg++;
			buffer[tmp_lg] = tmpDataAck;
			i = i + 2 + lg;
			break;
		case FP_Close_FP:  //w(3-9)
			tmp_lg++;
			buffer[tmp_lg] =(unsigned char)FP_Close_FP;
			msgbuffer[3] = tmp_lg;  // cmd_pos
			if( ( lg== 0 ) && (pfp_data->FP_State  >= 3) && (pfp_data->FP_State <= 9)) {
				pfp_data->Assign_Contr_Id[0] = 0;
				pfp_data->Assign_Contr_Id[1] = 0;
				msgbuffer[0] = 0x1f;    // cmd_pos
			} else if ((pfp_data->FP_State < 3) || (pfp_data->FP_State > 9)) {
				err_event = Close_FP;
				tmpDataAck = '\x03';
				tmpMSAck = '\x05'; 
			} else { 
				err_event = Invalid_Event;
				tmpDataAck = '\x05';
				tmpMSAck = '\x05';
			} 
			tmp_lg++;
			buffer[tmp_lg] = tmpDataAck;
			i = i + 2 + lg;
			break;
		case FP_Release_FP:   //w(3-4)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FP_Release_FP;			
			msgbuffer[3] = tmp_lg;        // cmd_pos
			if((lg== 0) && (pfp_data->FP_State  >=3) && (pfp_data->FP_State <= 5))  {
				pfp_data->Release_Contr_Id[0] = recvBuffer[2];
				pfp_data->Release_Contr_Id[1] = recvBuffer[3];
				pfp_data->Release_Token = recvBuffer[3];
				if (memcmp(pfp_data->Remote_Volume_Preset, zero_array, 5) != 0) {
					msgbuffer[0] = 0x3d;  // pcd_cmd
				} else if (memcmp(pfp_data->Remote_Amount_Prepay, zero_array, 5) != 0) {
					msgbuffer[0] = 0x4d; 
				} else {
					msgbuffer[0] = 0x1d;
				}
			} else  if ((pfp_data->FP_State  < 3) || (pfp_data->FP_State  > 5)) {
				err_event = Release_FP;
				tmpDataAck = '\x03';
				tmpMSAck = '\x05'; 
			} else  { 
				err_event = Invalid_Event;
				tmpDataAck = '\x05';
				tmpMSAck = '\x05';
			}			
			tmp_lg++;
			buffer[tmp_lg] = tmpDataAck;
			i = i + 2 + lg;
			break;
		case FP_Terminate_FP:   //w(4-9)
			tmp_lg++;
			buffer[tmp_lg] =(unsigned char)FP_Terminate_FP;
			msgbuffer[3] = tmp_lg;  // cmd_pos
			if((lg== 0 ) && (pfp_data->FP_State  >=4) && (pfp_data->FP_State  <= 9)) {
				msgbuffer[0] = 0x1c;    // pcd_cmd
			} else  if ((pfp_data->FP_State < 4) || (pfp_data->FP_State  > 9)) {
				err_event = Terminate_FP;
				tmpDataAck = '\x03';
				tmpMSAck = '\x05'; 
			} else  { 
				err_event = Invalid_Event;
				tmpDataAck = '\x05';
				tmpMSAck = '\x05';
			}	
			tmp_lg++;
			buffer[tmp_lg] = tmpDataAck;
			i = i + 2 + lg;
			break;
		case FP_Suspend_FP:   //w(6,8)
			tmp_lg++;
			buffer[tmp_lg] =(unsigned char)FP_Suspend_FP;
			tmpDataAck = '\x05';  
			tmpMSAck = '\x05';
			tmp_lg++;
			buffer[tmp_lg] = tmpDataAck;
			i = i + 2 + lg;
			break;
		case FP_Resume_FP:   //w(7,9)
			tmp_lg++;
			buffer[tmp_lg] =(unsigned char)FP_Resume_FP;
			tmpDataAck = '\x03';  
			tmpMSAck = '\x05';
			tmp_lg++;
			buffer[tmp_lg] = tmpDataAck;
			i = i + 2 + lg;
			break;
		case FP_Clear_Display:   //w(3-5)
			tmp_lg++;
			buffer[tmp_lg] =(unsigned char)FP_Clear_Display;
			tmpDataAck = '\x05';  
			tmpMSAck = '\x05';
			tmp_lg++;
			buffer[tmp_lg] = tmpDataAck;
			i = i + 2 + lg;
			break;
		case FP_Leak_Command: //O   w(3)
			tmp_lg++;
			buffer[tmp_lg] =(unsigned char)FP_Leak_Command;
			tmpDataAck = '\x05';  
			tmpMSAck = '\x05';
			tmp_lg++;
			buffer[tmp_lg] = tmpDataAck;
			i = i + 2 + lg;
			break;
		default:
			tmp_lg++;
			buffer[tmp_lg] = recvBuffer[10 + i]  ;
			tmpDataAck = '\x04';    // Data does not exist in this device
			tmpMSAck = '\x05';      // Msg resused, some of the data is not acceptalbe
			tmp_lg++;
			buffer[tmp_lg] = tmpDataAck;
			i = i + 2 + lg;
			break;
		}
	}

	if(i != (recv_lg - 2) )	{
		UserLog("i  != recv_lg-2, writeFP_DATA error!! exit, recv_lg[%d],i [%d] ",recv_lg, i);
		return -2;
	}

	tmp_lg++;
	buffer[6] = (unsigned char)((tmp_lg - 8) / 256) ; //M_Lg
	buffer[7] = (unsigned char)((tmp_lg - 8) % 256);  //big end.
	buffer[10] = tmpMSAck;  

	msgbuffer[4] = tmp_lg;
	if (msgbuffer[0] != 0x00) {  // �����д����, MsgACK/DataACK Ĭ��0x00
		msgbuffer[1] = 0x00;
		msgbuffer[2] = recvBuffer[9];
		ret = SendMsg(ORD_MSG_TRAN, msgbuffer, tmp_lg + 5, chnlog, 3);
		if (ret == 0) {
//			gpIfsf_shm->msg_cnt_in_queue[chnlog - 1]++;
			return 1;    // ACK �� lischnxxx ������
		} else {
			RunLog( "forward[%s]failed, return[%d]", Asc2Hex(msgbuffer, tmp_lg + 5), ret);
			buffer[10] = 0x05;               // Msg_ACK
			buffer[msgbuffer[3] + 1] = 0x06; // Data_ACK
			err_event = 0xFF;
			ret = 0;     // ACK ��������������
		}

	} else if (err_event != 0x00) {  // �������д����, MsgACK/DataACK Ĭ�Ϸֱ�Ϊ 0x05/0x03
		ret = HandleStateErr(node, fp, err_event, pfp_data->FP_State, msgbuffer + 3);
		if (ret < 0) {
			if (ret == -2) {
				bzero(pfp_data->Remote_Amount_Prepay, 5);
				bzero(pfp_data->Remote_Volume_Preset, 5);
			}
			return 1;   // ACK.ERR_MSG.FP_STATE����Ϣ ���� HandleStateErr ����
		}
	} 

     	// �������, MsgACK/DataACK Ĭ�Ϸֱ�Ϊ 0x05/0x05
	if (err_event != 0) {
		// err_event == Invalid_event
		/*
		 * ����� OPEN_FP/CLOSE_FP�Ȳ���,
		 * ��ô�ָ�ACK/NAK֮����Ҫ�ٴ��ϴ�FP_STATE,
		 * ������������������, ���Էŵ�����
		 */
		ret = SendData2CD(buffer, tmp_lg, 5);
		ret = SendFP_State(node, 0x21 + fp);
		return 1;
	}


	if(NULL != msg_lg) {
		*msg_lg = tmp_lg;
	}
	if(NULL != sendBuffer) {
		memcpy(sendBuffer, buffer, tmp_lg);
	}

	return 0;
}

int SetSglFP_DATA(const char node, const char fp_ad, const unsigned char *data){
	int i, j, ret,  lg;//tmp_lg, recv_lg,
	DISPENSER_DATA *dispenser_data;
	FP_DATA *pfp_data;
	int fp =  fp_ad - 0x21; //0,1,2,3.
	unsigned char  tmpDataAck;//tmpMSAck,
	//unsigned char node;
	//unsigned char buffer[256];
	//memset(buffer, 0, 256);
	//unsigned char *recvBuffer; 
	//recvBuffer= &data[0];
	if( NULL == data ){		
		UserLog("����Ϊ�մ���,���͵�SetSgl ʧ��");
		return -1;
	}	
	if( (node <= 0) || (node > 127 ) ){
		UserLog("���ͻ��ڵ�ֵ����,���͵�SetSgl ʧ��");
		return -1;
		}
	if( (fp < 0) || (fp > 3) ){
		UserLog("���ͻ��ڵ�ļ��͵��ַ����,���͵�SetSgl ʧ��");
		return -1;
		}	
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,���͵�SetSgl ʧ��");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,���͵�SetSgl  ʧ��");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	pfp_data =  (FP_DATA *)( &(dispenser_data->fp_data[fp]) );
	
	
	tmpDataAck = '\0'; //ok ack.
	lg =  (int )data[1]; //Data_Lg.
	switch( data[0] )
	{
	case FP_FP_Name:  //O ,w(1-2)
		if( ( lg <= 8 ) && (pfp_data->FP_State  >=1) && (pfp_data->FP_State  <=2) ) {
			for(j=0; j<lg; j++)
				pfp_data->FP_Name[8-lg+j] = data[2+j];
			}
		else  if ((pfp_data->FP_State  !=1)  || (pfp_data->FP_State  != 2))  { 
			tmpDataAck = '\x02';
			//tmpMSAck = '\x05'; 
			}
		else  { 
			tmpDataAck = '\x01';
			//tmpMSAck = '\x05';
			}		
		tmpDataAck = '\x04'; 			
		break;
	case FP_Nb_Tran_Buffer_Not_Paid: //(1-15),w(1-2)
		if( ( lg== 1 ) && (pfp_data->FP_State  < 3) && (data[2] >= 1) && (data[2] <= 15) ) 
			pfp_data->Nb_Tran_Buffer_Not_Paid = data[2];
		else  if (pfp_data->FP_State  >= 3)  { 
			tmpDataAck = '\x02';
			}
		else  { 
			tmpDataAck = '\x01';
			}			
		break;
	case FP_Nb_Of_Historic_Trans://(1-15),w(1-2)
		if( ( lg== 1 ) && (pfp_data->FP_State  < 3) && (data[2] >= 1) && (data[2] <= 15) ) 
			pfp_data->Nb_Tran_Buffer_Not_Paid = data[2];
		else  if (pfp_data->FP_State  >= 3)  { 
			tmpDataAck = '\x02';
			}
		else  { 
			tmpDataAck = '\x01';
			}			
		break;
	case FP_Nb_Logical_Nozzle://(1-8),w(1-2)
		if( ( lg== 1 ) && (pfp_data->FP_State  < 3) && (data[2] >= 1) && (data[2] <= 8) ) 
			pfp_data->Nb_Logical_Nozzle = data[2];
		else  if (pfp_data->FP_State  >= 3)  { 
			tmpDataAck = '\x02';
			//tmpMSAck = '\x05'; 
			}
		else  { 
			tmpDataAck = '\x01';
			//tmpMSAck = '\x05';
			}			
		break;
	case FP_Default_Fuelling_Mode://(1-8),w(1-2)
		if( ( lg== 1 ) && (pfp_data->FP_State  < 3) && (data[2] == 1)  )  //only 1.
			pfp_data->Default_Fuelling_Mode = data[2];
		else  if (pfp_data->FP_State  >= 3)  { 
			tmpDataAck = '\x02';
			//tmpMSAck = '\x05'; 
			}
		else  { 
			tmpDataAck = '\x01';
			//tmpMSAck = '\x05';
			}			
		break;
	case FP_Leak_Log_Noz_Mask://bit set, ,w(3),not read.
		if( ( lg== 1 ) && (pfp_data->FP_State  == 3)   )  //only 1.
			pfp_data->Leak_Log_Noz_Mask = data[2];
		else  if (pfp_data->FP_State  != 3)  { 
			tmpDataAck = '\x02';
			//tmpMSAck = '\x05'; 
			}
		else  { 
			tmpDataAck = '\x01';
			//tmpMSAck = '\x05';
			}			
		break;
	case FP_FP_State: //(1-9),not write.
		if((lg== 1) && (pfp_data->FP_State  < 10)) { //only 1.				
			ret = ChangeFP_State(node, fp_ad, data[2], 0);
			if(ret<0) 
				return -1;
			//pfp_data->FP_State = recvBuffer[2];
			}
		else  if (pfp_data->FP_State >= 10)  { 
			tmpDataAck = '\x02';
			//tmpMSAck = '\x05'; 
			}
		else  { 
			tmpDataAck = '\x01';
			//tmpMSAck = '\x05';
			}		
		break;
	case FP_Log_Noz_State: //(1-15),not write
		if( ( lg== 1 ) && (pfp_data->FP_State  > 2)   ) { //only 1.				
			ret = ChangeLog_Noz_State(node, fp_ad, data[2]);				
			if( ret<0 ) 
				return -1;
			//pfp_data->Log_Noz_State = recvBuffer[2];
			}
		else  if (pfp_data->FP_State <= 2)  { 
			tmpDataAck = '\x02';
			//tmpMSAck = '\x05'; 
			}
		else  { 
			tmpDataAck = '\x01';
			//tmpMSAck = '\x05';
			}		
		break;
	case FP_Assign_Contr_Id: //bin16,w(2-4)
		tmpDataAck = '\x02'; 
		break;
	case FP_Release_Mode: //O, (0-1),w(2-4)
		tmpDataAck = '\x04';  
		break;
	case FP_ZeroTR_Mode: //(0-1),w(2-4)
		if( ( lg== 1 ) && (pfp_data->FP_State  < 5)  && (pfp_data->FP_State  != 1)  ) { //only 1.	
			pfp_data->ZeroTR_Mode = data[2];
			}
		else  if ( (pfp_data->FP_State >= 5) ||(pfp_data->FP_State == 1) ) { 
			tmpDataAck = '\x02';
			//tmpMSAck = '\x05'; 
			}
		else  { 
			tmpDataAck = '\x01';
			//tmpMSAck = '\x05';
			}		
		break;
	case FP_Log_Noz_Mask: //bit set ,w(2-4)
		if( ( lg== 1 ) && (pfp_data->FP_State  < 5) && (pfp_data->FP_State  > 1)  ) { //only 1.	
			pfp_data->Log_Noz_Mask = data[2];
			}
		else  if ( (pfp_data->FP_State >4) ||  (pfp_data->FP_State  == 1))  { 
			tmpDataAck = '\x02';
			//tmpMSAck = '\x05'; 
			}
		else  { 
			tmpDataAck = '\x01';
			//tmpMSAck = '\x05';
			}	
		break;
	case FP_Config_Lock: //bin16,w(1-2)
		if( ( lg== 1 ) && (pfp_data->FP_State  < 3)  ) {
			pfp_data->Config_Lock[0] = data[2];
			pfp_data->Config_Lock[1] = data[3];
			}
		else  if (pfp_data->FP_State  >= 3)  { 
			tmpDataAck = '\x02';
			//tmpMSAck = '\x05'; 
			}
		else  { 
			tmpDataAck = '\x01';
			//tmpMSAck = '\x05';
			}			
		break;
	case FP_Remote_Amount_Prepay: //w(3-4)
		if( ( lg == 5 ) && (pfp_data->FP_State  >=3) && (pfp_data->FP_State  <=4) ) {
			for(j=0; j<lg; j++)
				pfp_data->Remote_Amount_Prepay[j] = data[2+j];
		} else  if ((pfp_data->FP_State  !=3)  || (pfp_data->FP_State  != 4))  { 
			tmpDataAck = '\x02';
			//tmpMSAck = '\x05'; 
		} else  { 
			tmpDataAck = '\x01';
			//tmpMSAck = '\x05';
		}			
		break;
	case FP_Remote_Volume_Preset: // w(3-4)
		if( ( lg == 5 ) && (pfp_data->FP_State  >=3) && (pfp_data->FP_State  <=4) ) {
			for(j=0; j<lg; j++)
				pfp_data->Remote_Volume_Preset[j] = data[2+j];
		} else  if ((pfp_data->FP_State  !=3)  || (pfp_data->FP_State  != 4))  { 
			tmpDataAck = '\x02';
			//tmpMSAck = '\x05'; 
		} else  { 
			tmpDataAck = '\x01';
			//tmpMSAck = '\x05';
		}			
		break;
	case FP_Release_Token: //w(3-4) only CD!
		if( ( lg == 1 ) && (pfp_data->FP_State  >=3) && (pfp_data->FP_State  <=4) ) 
				pfp_data->Release_Token = data[2];
		else  if ((pfp_data->FP_State  !=3)  || (pfp_data->FP_State  != 4))  { 
			tmpDataAck = '\x02';
			//tmpMSAck = '\x05'; 
			}
		else  { 
			tmpDataAck = '\x01';
			//tmpMSAck = '\x05';
			}			
		break;
	case FP_Fuelling_Mode: //(1-8),w(3-4)
		if ((lg == 1) && (pfp_data->FP_State >= 3) && (pfp_data->FP_State <= 4) &&
						(data[2] >= 1) && (data[2] <= FP_FM_CNT)) {
				pfp_data->Fuelling_Mode = data[2];
		} else  if ((pfp_data->FP_State  !=3)  || (pfp_data->FP_State  != 4))  { 
			tmpDataAck = '\x02';
			//tmpMSAck = '\x05'; 
		} else  { 
			tmpDataAck = '\x01';
			//tmpMSAck = '\x05';
		}			
		break;
	case FP_Transaction_Sequence_Nb: //bcd4, w(1-2)
		if( ( lg == 2 ) && (pfp_data->FP_State  >=1) && (pfp_data->FP_State  <=2) ) {
				pfp_data->Transaction_Sequence_Nb[0] = data[2];
				pfp_data->Transaction_Sequence_Nb[1] = data[3];
			}
		else  if ( (pfp_data->FP_State  !=1)  || (pfp_data->FP_State  != 2)  )  { 
			tmpDataAck = '\x02';
			//tmpMSAck = '\x05'; 
			}
		else  { 
			tmpDataAck = '\x01';
			//tmpMSAck = '\x05';
			}			
		break;
	case FP_Current_TR_Seq_Nb: //bcd4, r(6-9), not write. Use BeginTr4Fp function to write.
		if( ( lg == 2 ) && (pfp_data->FP_State  >=3) && (pfp_data->FP_State  <=9) ) {
			for(j=0; j<lg; j++)
				pfp_data->Current_TR_Seq_Nb[j] = data[2+j];
			}
		else  if ((pfp_data->FP_State  < 3)  || (pfp_data->FP_State  > 9 ))  { 
			tmpDataAck = '\x02';
			//tmpMSAck = '\x05'; 
			}
		else  { 
			tmpDataAck = '\x01';
			//tmpMSAck = '\x05';
			}			 
		break;
	case FP_Release_Contr_Id: //,w(3-4) r(3-9)
		if( ( lg == 2 ) && (pfp_data->FP_State  >=3) && (pfp_data->FP_State  <=4 ) ) {
				pfp_data->Release_Contr_Id[0] = data[13+i];
				pfp_data->Release_Contr_Id[1] = data[14+i];
			}
		else  if ( (pfp_data->FP_State  !=3)  || (pfp_data->FP_State  != 4)  )  { 
			tmpDataAck = '\x02';
			//tmpMSAck = '\x05'; 
			}
		else  { 
			tmpDataAck = '\x01';
			//tmpMSAck = '\x05';
			}			
		break;
	case FP_Suspend_Contr_Id: //r(7,9),w(6,8)
		tmpDataAck = '\x02';  
		break;
	
	
	case FP_Current_Amount:  //r(6-9),not writebale.
		if( ( lg == 5 ) && (pfp_data->FP_State  >=3) && (pfp_data->FP_State  <=8) ) {
			for(j=0; j<lg; j++)
				pfp_data->Current_Amount[j] = data[2+j];
			}
		else  if ((pfp_data->FP_State  < 3)  || (pfp_data->FP_State  > 8))  { 
			tmpDataAck = '\x02';
			//tmpMSAck = '\x05'; 
			}
		else  { 
			tmpDataAck = '\x01';
			//tmpMSAck = '\x05';
			}			
		break;
	case FP_Current_Volume:  //r(6-9),not writebale.
		if( ( lg == 5 ) && (pfp_data->FP_State  >=3) && (pfp_data->FP_State  <=8) ) {
			for(j=0; j<lg; j++)
				pfp_data->Current_Volume[j] = data[2+j];
			}
		else  if ((pfp_data->FP_State  < 3)  || (pfp_data->FP_State  > 8))  { 
			tmpDataAck = '\x02';
			//tmpMSAck = '\x05'; 
			}
		else  { 
			tmpDataAck = '\x01';
			//tmpMSAck = '\x05';
			}			
		break;
	case FP_Current_Unit_Price: //r(6-9),not writebale.
		if( ( lg == 4 ) && (pfp_data->FP_State  >=3) && (pfp_data->FP_State  <=8) ) {
			for(j=0; j<lg; j++)
				pfp_data->Current_Unit_Price[j] = data[2+j];
			}
		else  if ((pfp_data->FP_State  < 3)  || (pfp_data->FP_State  > 8))  { 
			tmpDataAck = '\x02';
			//tmpMSAck = '\x05'; 
			}
		else  { 
			tmpDataAck = '\x01';
			//tmpMSAck = '\x05';
			}		
		break;
	case FP_Current_Log_Noz: //r(6-9),not writebale.
		if( ( lg== 1 ) && (pfp_data->FP_State  >=3) && (pfp_data->FP_State  <=8)    )  //only 1.
			pfp_data->Current_Log_Noz = data[2];
		else  if ( (pfp_data->FP_State  < 3)  || (pfp_data->FP_State  > 8) )  { 
			tmpDataAck = '\x02';
			//tmpMSAck = '\x05'; 
			}
		else  { 
			tmpDataAck = '\x01';
			//tmpMSAck = '\x05';
			}		
		break;
	case FP_Current_Prod_Nb: //bcd8, r(6-9),not writebale.
		if( ( lg == 4 ) && (pfp_data->FP_State  >=3) && (pfp_data->FP_State  <=8) ) {
			for(j=0; j<lg; j++)
				pfp_data->Current_Prod_Nb[4-lg+j] = data[2+j];
			
			}
		else  if ((pfp_data->FP_State  < 3)  || (pfp_data->FP_State  > 8))  { 
			tmpDataAck = '\x02';
			//tmpMSAck = '\x05'; 
			}
		else  { 
			tmpDataAck = '\x01';
			//tmpMSAck = '\x05';
			}		
		break;
	case FP_Current_TR_Error_Code: //r(6-9),not writebale.
		if( ( lg== 1 ) && (pfp_data->FP_State  >=3) && (pfp_data->FP_State  <=8)    )  //only 1.
			pfp_data->Current_TR_Error_Code = data[2];
		else  if ( (pfp_data->FP_State  < 3)  || (pfp_data->FP_State  > 8) )  { 
			tmpDataAck = '\x02';
			//tmpMSAck = '\x05'; 
			}
		else  { 
			tmpDataAck = '\x01';
			//tmpMSAck = '\x05';
			}		
		break;
		//tmp_lg++;
		//buffer[tmp_lg] = (unsigned char)FP_Current_Average_Temp;	
		//tmp_lg++;
		//buffer[tmp_lg] = (unsigned char)FP_Current_Price_Set_Nb;
		
		//tmp_lg++;
		//buffer[tmp_lg] = (unsigned char)FP_Multi_Nozzle_Type;
		//tmp_lg++;
		//buffer[tmp_lg] = (unsigned char)FP_Multi_Nozzle_State;
		//tmp_lg++;
		//buffer[tmp_lg] = (unsigned char)FP_Multi_Nozzle_Status_Message;
		//tmp_lg++;
		//buffer[tmp_lg] = (unsigned char)FP_Local_Vol_Preset;
		//tmp_lg++;
		//buffer[tmp_lg] = (unsigned char)FP_Local_Amount_Prepay;
		//tmp_lg++;
		//buffer[tmp_lg] = (unsigned char)FP_Running_Transaction_Message_Frequency;
		//tmp_lg++;
		//buffer[tmp_lg] = (unsigned char)FP_FP_Alarm;
		
	default:
		tmpDataAck = '\x04';  
		break;
	}
	
	return tmpDataAck;
}
int RequestFP_DATA(const char node, const char fp_ad){
	int i, ret, tmp_lg;
	DISPENSER_DATA *dispenser_data;
	FP_DATA *pfp_data;
	int fp = fp_ad -0x21; //0,1,2,3.
	unsigned char buffer[128];
	memset(buffer, 0, 128);
	
	if( (node <= 0) || (node > 127 ) ){
		UserLog("���ͻ��ڵ�ֵ����,���͵�Request ʧ��");
		return -1;
		}
	if( (fp < 0) || (fp > 3) ){
		UserLog("���ͻ��ڵ�ļ��͵��ַ����,���͵�Request ʧ��");
		return -1;
		}
	
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,���͵�Request ʧ��");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,���͵�Request  ʧ��");
		return -1;
	}
	//dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	//pfp_data =  (FP_DATA *)( &(dispenser_data->fp_data[fp]) );
	
	buffer[0] = '\x02';
	buffer[1] = '\x01';
	buffer[2] = '\x01';
	buffer[3] = (unsigned char)node;
	buffer[4] = '\0';  //IFSF_MC
	buffer[5] =  '\0'; //M_St; //read.
	
	buffer[8] = '\x01'; //ad_lg
	buffer[9] = fp_ad; //ad;
	//buffer[10] = recvBuffer[10]; //It's Data_Id. Data begin.
	tmp_lg=9;
	
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)FP_FP_Name;
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)FP_Nb_Tran_Buffer_Not_Paid;
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)FP_Nb_Of_Historic_Trans;
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)FP_Nb_Logical_Nozzle;
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)FP_Default_Fuelling_Mode;
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)FP_Leak_Log_Noz_Mask;
	
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)FP_FP_State;
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)FP_Log_Noz_State;
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)FP_Assign_Contr_Id;
	//tmp_lg++;
	//buffer[tmp_lg] = (unsigned char)FP_Release_Mode; //O
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)FP_ZeroTR_Mode;
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)FP_Log_Noz_Mask;
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)FP_Config_Lock;
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)FP_Remote_Amount_Prepay;
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)FP_Remote_Volume_Preset;
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)FP_Release_Token;
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)FP_Fuelling_Mode;
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)FP_Transaction_Sequence_Nb;
	
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)FP_Current_TR_Seq_Nb;
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)FP_Release_Contr_Id;
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)FP_Suspend_Contr_Id;
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)FP_Release_Token;
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)FP_Fuelling_Mode;
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)FP_Current_Amount;
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)FP_Current_Volume;
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)FP_Current_Unit_Price;
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)FP_Current_Log_Noz;
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)FP_Current_Prod_Nb;
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)FP_Current_TR_Error_Code;
	//tmp_lg++;
	//buffer[tmp_lg] = (unsigned char)FP_Current_Average_Temp;	
	//tmp_lg++;
	//buffer[tmp_lg] = (unsigned char)FP_Current_Price_Set_Nb;
	
	//tmp_lg++;
	//buffer[tmp_lg] = (unsigned char)FP_Multi_Nozzle_Type;
	//tmp_lg++;
	//buffer[tmp_lg] = (unsigned char)FP_Multi_Nozzle_State;
	//tmp_lg++;
	//buffer[tmp_lg] = (unsigned char)FP_Multi_Nozzle_Status_Message;
	//tmp_lg++;
	//buffer[tmp_lg] = (unsigned char)FP_Local_Vol_Preset;
	//tmp_lg++;
	//buffer[tmp_lg] = (unsigned char)FP_Local_Amount_Prepay;
	//tmp_lg++;
	//buffer[tmp_lg] = (unsigned char)FP_Running_Transaction_Message_Frequency;
	//tmp_lg++;
	//buffer[tmp_lg] = (unsigned char)FP_FP_Alarm;
	
	tmp_lg++;
	buffer[6] =(unsigned char)(tmp_lg /256) ;
	buffer[7] = (unsigned char)(tmp_lg % 256) ; 

	ret= SendData2CD(buffer, tmp_lg, 5);
	if (ret <0 ){
		UserLog("���ͻ�ͨѶ����,���͵㲻��Request ");
		return -1;
	}
	return 0;
}
//һ�ʽ������,Fp������ص�Ҫ���ĸı�,��洢�������ݵ�����������.
//FP�������ݵĸı��������ͼ�,������SetSglFP_DATA. 
int EndTr4Fp(const char node, const char fp_ad, const char * const start_total, const char * const end_total)
{
	int i, ret;
	DISPENSER_DATA *dispenser_data;
	FP_DATA *pfp_data;
	FP_TR_DATA pfp_tr_data;
	int fp = fp_ad -0x21; //0,1,2,3.
	unsigned char Log_Noz;
	unsigned char buffer[128];
	unsigned char date_time[7];
	long k;
	__u8 Tankcmd[5] = {0};

	memset(buffer, 0, 128);
	memset(&pfp_tr_data, 0, sizeof(FP_TR_DATA) );

	if( (node <= 0) || (node > 127 ) ){
		UserLog("���ͻ��ڵ�ֵ����,���͵�End Trans ʧ��");
		return -1;
	}
	if( (fp < 0) || (fp > 3) ){
		UserLog("���ͻ��ڵ�ļ��͵��ַ����,���͵�End Trans  ʧ��");
		return -1;
	}
	
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,���͵�End Trans  ʧ��");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,���͵�End Trans   ʧ��");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	pfp_data =  (FP_DATA *)( &(dispenser_data->fp_data[fp]) );
	
	//begin add transaction data...
	pfp_tr_data.node = node;
	pfp_tr_data.fp_ad = fp_ad;	
	pfp_tr_data.TR_Seq_Nb[0] = pfp_data->Current_TR_Seq_Nb[0]; 
	pfp_tr_data.TR_Seq_Nb[1]  = pfp_data->Current_TR_Seq_Nb[1]; 
	memcpy(pfp_tr_data.TR_Contr_Id,pfp_data->Release_Contr_Id, 2);
	pfp_tr_data.TR_Release_Token = pfp_data->Release_Token;
	pfp_tr_data.TR_Fuelling_Mode = pfp_data->Fuelling_Mode;
	
	memcpy(pfp_tr_data.TR_Amount, pfp_data->Current_Amount, 5);
	memcpy(pfp_tr_data.TR_Volume, pfp_data->Current_Volume, 5);
	memcpy(pfp_tr_data.TR_Unit_Price , pfp_data->Current_Unit_Price, 4);
	pfp_tr_data.TR_Log_Noz = pfp_data->Log_Noz_State;
	memcpy(pfp_tr_data.TR_Prod_Nb , pfp_data->Current_Prod_Nb, 4);
	pfp_tr_data.TR_Error_Code = '\0';
	memcpy(pfp_tr_data.TR_Start_Total, start_total, 7);   // user(CPPEI) defined
	memcpy(pfp_tr_data.TR_End_Total, end_total, 7);       // user defined

	// ��λ������, ������δ��ɵ��ѻ�����, �����ͻ�����, �Զ���Ϊ��֧������
	if ((gpIfsf_shm->auth_flag == FCC_SWITCH_AUTH) ||
		((gpIfsf_shm->fp_offline_flag[node - 1][fp] & 0x0F) != 0) ||  // incomplete
			gpIfsf_shm->node_online[i] == 0) { 
		pfp_tr_data.Trans_State = CLEARED;
		pfp_tr_data.offline_flag = OFFLINE_TR;      // �ѻ����
	} else {                                            // ��λ������
		pfp_tr_data.Trans_State = PAYABLE;
		pfp_tr_data.offline_flag = ONLINE_TR; 
	}
	//pfp_tr_data->TR_Buff_Contr_Id[0] = '\0';
//	RunLog("######### EndTr4Fp: Tr_state: %02x #######", pfp_tr_data.Trans_State);
	
	ret = getlocaltime(date_time);
	memcpy(pfp_tr_data.TR_Date, &date_time[0], 4);
	memcpy(pfp_tr_data.TR_Time, &date_time[4], 3);
	
	//pfp_tr_data->TR_Security_Chksum[] = //caculate check sum in AddTR_DATA() fuction? 
	
	// CSSI.У����Ŀ���ݲɼ� -- ��¼��������
	if (gpGunShm->data_acquisition_4_TCC_adjust_flag != 0) {
		gpGunShm->tr_fin_flag = 1;    // ����Һλ���̼�¼Һλ

#if 0           // ��Ϊ��¼����ʵʱ�������� @ 2011.06.02
		write_tcc_adjust_data(pfp_data->Current_Date_Time, ",,%d,%d,%d,%02x%02x.%02x,FF,1,,0,0",
			node, fp + 1,
			(pfp_tr_data.TR_Log_Noz == 0x04 ? 3 :
				(pfp_tr_data.TR_Log_Noz == 0x08 ? 4 : pfp_tr_data.TR_Log_Noz)),
			pfp_data->Current_Volume_4_TCC_Adjust[2],
			pfp_data->Current_Volume_4_TCC_Adjust[3],
			pfp_data->Current_Volume_4_TCC_Adjust[4]);
		TCCADLog("TCC-AD: Node %d FP %d Noz %d ��¼���һ��ʵʱ���� %02x%02x.%02x",
			node, fp + 1, pfp_tr_data.TR_Log_Noz,
			pfp_data->Current_Volume_4_TCC_Adjust[2],
			pfp_data->Current_Volume_4_TCC_Adjust[3],
			pfp_data->Current_Volume_4_TCC_Adjust[4]);
#endif

		write_tcc_adjust_data(NULL, ",,%d,%d,%d,%02x%02x.%02x,AA,1,,%02x%02x%02x%02x.%02x,%02x%02x%02x%02x.%02x",
			node, fp + 1,
			(pfp_tr_data.TR_Log_Noz == 0x04 ? 3 :
				(pfp_tr_data.TR_Log_Noz == 0x08 ? 4 : pfp_tr_data.TR_Log_Noz)),
			pfp_tr_data.TR_Volume[2], pfp_tr_data.TR_Volume[3], pfp_tr_data.TR_Volume[4],
			start_total[2], start_total[3], start_total[4], start_total[5], start_total[6],
			end_total[2], end_total[3], end_total[4], end_total[5], end_total[6]);
		TCCADLog("TCC-AD: Node %d FP %d Noz %d ��¼���� %02x%02x.%02x, tr_fin_flag:%d, tmp:%d",
			node, fp + 1, pfp_tr_data.TR_Log_Noz,
			pfp_tr_data.TR_Volume[2], pfp_tr_data.TR_Volume[3], pfp_tr_data.TR_Volume[4],
			gpGunShm->tr_fin_flag, gpGunShm->tr_fin_flag_tmp);
	}
	// End of CSSI.У����Ŀ���ݲɼ�

	RunLog("Node[%02d]FP[%d]Noz[%d] TR-Vol: %02x%02x.%02x Amt: %02x%02x%02x.%02x Price: %02x%02x.%02x STotal: %02x%02x%02x%02x.%02x ETotal: %02x%02x%02x%02x.%02x (%x)", 
			node, fp + 1, pfp_tr_data.TR_Log_Noz,
			pfp_tr_data.TR_Volume[2],
		    pfp_tr_data.TR_Volume[3],
		    pfp_tr_data.TR_Volume[4],
		       	pfp_tr_data.TR_Amount[1],
		       	pfp_tr_data.TR_Amount[2],
		       	pfp_tr_data.TR_Amount[3],
		       	pfp_tr_data.TR_Amount[4],
			pfp_tr_data.TR_Unit_Price[1],
			pfp_tr_data.TR_Unit_Price[2],
		    pfp_tr_data.TR_Unit_Price[3],
				start_total[2], start_total[3], start_total[4],
				start_total[5], start_total[6], 
			end_total[2], end_total[3], end_total[4],
			end_total[5], end_total[6],
			pfp_tr_data.offline_flag);
	/*
	int fd;
       int ret2;
       fd = open("/tmp/oil_fifo.data", O_RDWR|O_NONBLOCK);//write only
               if (fd  == -1){
                       RunLog("the file oil_fifo.data open failed ");
               }
               ret2 = write(fd, pfp_data->Current_Prod_Nb, 4);
               if (ret2 == -1)
                       RunLog("TAKE IT EASY CURRENT PROD NB WIRTE WRONG");
               else if (ret2 == 4){
                       RunLog("oil no send success %02x",   pfp_data->Current_Prod_Nb[3] );
               }
       close(fd);
	  */ 
	ret = AddTR_DATA(&pfp_tr_data);
	if(ret < 0) {
		UserLog("���ͻ�add �������ݴ���,���͵�End Trans ʧ��");
		return -1;
	}

	if (pfp_tr_data.offline_flag == OFFLINE_TR) {
		SET_FP_OFFLINE_TR(node, fp);           // ��ǻ����������ѻ�����
	}

	//��У׼Һλ�� 
	if (gpGunShm->LiquidAdjust == 1) {             
		Tankcmd[0] = 0x02;
		Tankcmd[1] = 0x00;
		Tankcmd[2] = node;                       //�ڵ��
		Tankcmd[3] = fp + 1;                     //����
		Log_Noz = dispenser_data->fp_data[fp].Current_Log_Noz;  //����ϵ��߼�ǹ��
		if ((Log_Noz & 0x01) == 0x01) {
			Tankcmd[4] = 1;
		} else if  ((Log_Noz & 0x02) == 0x02) {
			Tankcmd[4] = 2;
		} else if  ((Log_Noz & 0x04) == 0x04) {
			Tankcmd[4] = 3;
		} else if  ((Log_Noz & 0x08) == 0x08) {
			Tankcmd[4] = 4;
		} else if  ((Log_Noz & 0x10) == 0x10) {
			Tankcmd[4] = 5;
		} else if  ((Log_Noz & 0x20) == 0x20) {
			Tankcmd[4] = 6;
		} else if  ((Log_Noz & 0x40) == 0x40) {
			Tankcmd[4] = 7;
		} else if  ((Log_Noz & 0x80) == 0x80) {
			Tankcmd[4] = 8;	
		}

		memcpy(&Tankcmd[5], &pfp_tr_data.TR_End_Total[2], 5);
		memcpy(&Tankcmd[10], &pfp_data->Current_Volume[2], 3);
		SendMsg(TANK_MSG_ID, Tankcmd, 13, 1, 3);
	}

#if 0
	//�������ݴ���
	if(gpIfsf_shm->css_data_flag>0){
		int ret;
		char trans_flag = 0xff;
		int length= sizeof(pfp_tr_data)+1;
		if(gCssFifoFd<=0){
			UserLog("�ܵ���gCssFifoFd[%d]����",gCssFifoFd);
			//return 0;
		}
		//if((pfp_data->FP_State==3)||(pfp_data->FP_State==4)||(pfp_data->FP_State==5)||(pfp_data->FP_State==8)||){
		ret=write(gCssFifoFd, (char *)&length, sizeof(int) );
		ret=write(gCssFifoFd, (char *)&trans_flag, 1 );//д���־0xff
		ret=write(gCssFifoFd,  (char *)&pfp_tr_data,sizeof(pfp_tr_data));			
		//}
	}
#endif
	pfp_data->Fuelling_Mode = pfp_data->Default_Fuelling_Mode;
	pfp_data->Release_Token = 0;
	pfp_data->Current_Log_Noz = 0;
	pfp_data->Current_TR_Error_Code = 0  ;
	memset(pfp_data->Current_Amount, 0 , 5);
	memset(pfp_data->Current_Volume, 0 , 5);
	memset(pfp_data->Current_Average_Temp, 0 , 3);
	memset(pfp_data->Current_Price_Set_Nb, 0 , 2);
	memset(pfp_data->Current_Prod_Nb, 0 , 4);
	memset(pfp_data->Current_Unit_Price, 0 , 4);
	memset(pfp_data->Release_Contr_Id, 0 , 2);
	
	memset(pfp_data->Remote_Amount_Prepay, 0 , 5);
	memset(pfp_data->Remote_Volume_Preset, 0 , 5);
	
#if 0
	RunLog(">>>> EndTr4Fp begin Inc: %02x%02x\n",
		pfp_data->Transaction_Sequence_Nb[0], pfp_data->Transaction_Sequence_Nb[1]);
#endif

	ret = BBcdInc(pfp_data->Transaction_Sequence_Nb, 2);	
	if(ret <0) {
		UserLog("���ͻ����͵㽻��˳�����1 ����,���͵�Begin Transʧ��");
		return -1;
	}

#if 0
	RunLog(">>>> EndTr4Fp after Inc: %02x%02x\n",
		pfp_data->Transaction_Sequence_Nb[0], pfp_data->Transaction_Sequence_Nb[1]);
#endif

	return 0;
}
//һ�ʽ��׿�ʼ,FpҪ���ĸı�,��:Transaction_Sequence_Nb�����������1��.
int BeginTr4Fp(const char node, const char fp_ad)
{ 
	int i, j, ret;
	DISPENSER_DATA *dispenser_data;
	FP_DATA *pfp_data;
	int fp = fp_ad -0x21; //0,1,2,3.
	int k;
	unsigned char tmp = 1;// , buffer[128]
	//memset(buffer, 0, 128);
	
	if( (node <= 0) || (node > 127 ) ){
		UserLog("���ͻ��ڵ�ֵ����,���͵�Begin Trans ʧ��");
		return -1;
	}
	if( (fp < 0) || (fp > 3) ){
		UserLog("���ͻ��ڵ�ļ��͵��ַ����,���͵�Begin Trans  ʧ��");
		return -1;
	}
	
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,���͵�Begin Trans  ʧ��");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,���͵�Begin Trans   ʧ��");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	pfp_data =  (FP_DATA *)( &(dispenser_data->fp_data[fp]) );
	//begin...
	for(i = 0; i < 8; i++) {
		if (((pfp_data->Log_Noz_State) >> i) & 1){
			FP_LN_DATA *pfp_ln_data = (FP_LN_DATA *)( &( dispenser_data->fp_ln_data[fp][i]) );
			j = pfp_ln_data->PR_Id - 1 ;
			FP_FM_DATA *pfp_fm_data =
			       	(FP_FM_DATA *)(&(dispenser_data->fp_fm_data[j][pfp_data->Fuelling_Mode - 1]));
			memcpy(pfp_data->Current_Unit_Price, pfp_fm_data->Prod_Price, 4);
			FP_PR_DATA *pfp_pr_data = (FP_PR_DATA *)( &( dispenser_data->fp_pr_data[j]) );
			/*for (k = 0; k < gpIfsf_shm->gunCnt; k++) {
				if (gpIfsf_shm->gun_config[k].node == node && gpIfsf_shm->gun_config[k].logfp == fp + 1 \
					&& gpIfsf_shm->gun_config[k].logical_gun_no == i + 1 ){
								if (pfp_pr_data->Prod_Nb[0] != 0 && pfp_pr_data->Prod_Nb[1] != 0 \
				&& pfp_pr_data->Prod_Nb[2] != 0 && pfp_pr_data->Prod_Nb[3] != 0) {
				//memcpy(pfp_pr_data->Prod_Nb, gpIfsf_shm->gun_config[k].oil_no, 4);
				Update_Prod_No();
			}
				}
			}*/
			

			memcpy(pfp_data->Current_Prod_Nb, pfp_pr_data->Prod_Nb, 4);	
			break;
		}
	} 

	// < DEBUG
	RunLog("DEBUG.Node %d FP %d FM %d, Current_Price: %s",
		       	node, fp + 1, pfp_data->Fuelling_Mode, Asc2Hex(pfp_data->Current_Unit_Price, 4));
	// End of DEBUG>

	if (8 == i ) {
		UserLog("���ͻ��ļ��͵���ǹ�Ҳ���,���͵�Begin Trans   ʧ��");
		return -1;
	}

	if( (pfp_data->Transaction_Sequence_Nb[0] == 0) \
		&& (pfp_data->Transaction_Sequence_Nb[1] == 0) ){
		pfp_data->Transaction_Sequence_Nb[1] = 1;
	}


	pfp_data->Current_TR_Seq_Nb[0] = pfp_data->Transaction_Sequence_Nb[0];
	pfp_data->Current_TR_Seq_Nb[1] = pfp_data->Transaction_Sequence_Nb[1];
	pfp_data->Current_Log_Noz = pfp_data->Log_Noz_State;
	
	return 0;
}

/*
 * func: �㽻������Ľ�������
 */
int EndZeroTr4Fp(const char node, const char fp_ad)
{ 
	int i, j, ret;
	DISPENSER_DATA *dispenser_data;
	FP_DATA *pfp_data;
	int fp = fp_ad -0x21; //0,1,2,3.
	__u8 Log_Noz;
	__u8 Tankcmd[5] = {0};
	
	if( (node <= 0) || (node > 127 ) ){
		UserLog("���ͻ��ڵ�ֵ����,���͵�Begin Trans ʧ��");
		return -1;
	}
	if( (fp < 0) || (fp > 3) ){
		UserLog("���ͻ��ڵ�ļ��͵��ַ����,���͵�Begin Trans  ʧ��");
		return -1;
	}
	
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,���͵�Begin Trans  ʧ��");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,���͵�Begin Trans   ʧ��");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	pfp_data =  (FP_DATA *)( &(dispenser_data->fp_data[fp]) );

	//��У׼Һλ�� 
	if (gpGunShm->LiquidAdjust == 1) {       
			Tankcmd[0] = 0x02;
			Tankcmd[1] = 0x00;
			Tankcmd[2] = node;                         //�ڵ��
			Tankcmd[3] = fp + 1;                       //����

			Log_Noz = dispenser_data->fp_data[fp].Current_Log_Noz;
			if ((Log_Noz & 0x01) == 0x01) { //����ϵ��߼�ǹ��
				Tankcmd[4] = 1;
			} else if  ((Log_Noz & 0x02) == 0x02) {
				Tankcmd[4] = 2;
			} else if  ((Log_Noz & 0x04) == 0x04) {
				Tankcmd[4] = 3;
			} else if  ((Log_Noz & 0x08) == 0x08) {
				Tankcmd[4] = 4;
			} else if  ((Log_Noz & 0x10) == 0x10) {
				Tankcmd[4] = 5;
			} else if  ((Log_Noz & 0x20) == 0x20) {
				Tankcmd[4] = 6;
			} else if  ((Log_Noz & 0x40) == 0x40) {
				Tankcmd[4] = 7;
			} else if  ((Log_Noz & 0x80) == 0x80) {
				Tankcmd[4] = 8;
			}
			
		
			 memcpy(&Tankcmd[5], &dispenser_data->fp_ln_data[fp][Tankcmd[4] -1].Log_Noz_Vol_Total[2], 5);

			/* ��ȱ�ݵİ汾��һ���������ų���2ʱ������ᷢ0
			*memcpy(&Tankcmd[5], &dispenser_data->fp_ln_data[fp][dispenser_data->fp_data[fp].Current_Log_Noz - 1].Log_Noz_Vol_Total[2], 5);
			*/
			memset(&Tankcmd[10], 0, 3);
			SendMsg(TANK_MSG_ID, Tankcmd, 13, 1, 3);
	}


	if((pfp_data->Transaction_Sequence_Nb[0] == 0) &&
		(pfp_data->Transaction_Sequence_Nb[1] == 0) ){
		pfp_data->Transaction_Sequence_Nb[1] = 1;
	}

	pfp_data->Fuelling_Mode = pfp_data->Default_Fuelling_Mode;
	memset(pfp_data->Current_Prod_Nb, 0 , 4);
	memset(pfp_data->Current_Unit_Price, 0 , 4);
	pfp_data->Current_TR_Seq_Nb[0] = 0x00;
	pfp_data->Current_TR_Seq_Nb[1] = 0x00;
	pfp_data->Current_Log_Noz = 0x00;

	memset(pfp_data->Current_Amount, 0 , 5);
	memset(pfp_data->Current_Volume, 0 , 5);
	memset(pfp_data->Release_Contr_Id, 0 , 2);
	
	memset(pfp_data->Remote_Amount_Prepay, 0 , 5);
	memset(pfp_data->Remote_Volume_Preset, 0 , 5);
	
	return 0;
}

/*
 * func: �ж��ڵ�ǰ״̬state,�Ƿ���������event, ����������д�������ݿ�,������ACK Message
 *       UserDefined ����Ϊ�Զ���event, �Ǳ�׼IFSF event
 *       �ڲ�����ִ�иò����������, HandleStateErr���NACK-ERR_MSG-FP_STATE�������ݵķ���;
 * param: msgbuffer[2] �Ժ�Ϊ��׼Ack Message;
 *        msgbuffer[1] ΪAck Message�ĳ���;
 *        msgbuffer[0] Ϊevent������Ack Message�е�λ�� (0 - msgbuffer[1]-1)
 * ret: 0 - ����-1 - ������; -2 - �������Զ���event
 */
int HandleStateErr(const __u8 node, const __u8 fp_pos, const __u8 event, const __u8 state, __u8 *msgbuffer)
{
	int ret = 0;
	int i;
	__u8 err_type = 0x00;
	__u8 *sendbuffer;
      	
	if (msgbuffer == NULL) {
		return -1;
	}	       

	sendbuffer = msgbuffer + 2;

	switch (event) {
	case Unable:
		if (state >= FP_State_Idle) {
			err_type = 0x32;           // error 6
		}
		break;
	case Open_FP:
		if (state >= FP_State_Calling) {
			err_type = 0x2F;           // error 3
		} else if (state == FP_State_Inoperative) {
			err_type = 0x2D;           // error 1
		}
		break;
	case Close_FP:
		if (state == FP_State_Inoperative) {
			err_type = 0x2D;           // error 1
		}
		break;
	case Release_FP:
		if (state >= FP_State_Fuelling) {
			err_type = 0x31;           // error 5
		} else if (state == FP_State_Inoperative) {
			err_type = 0x2D;           // error 1
		} else if (state == FP_State_Closed) {
			err_type = 0x2E;           // error 2
		}
		break;
	case Suspend_FP:
	case Resume_FP:
		if (state == FP_State_Inoperative) {
			err_type = 0x2D;           // error 1
		} else if (state == FP_State_Closed) {
			err_type = 0x2E;           // error 2
		} else if (state <= FP_State_Authorised) {
			err_type = 0x30;           // error 4
		}
		break;
	case Terminate_FP:
		if (state == FP_State_Inoperative) {
			err_type = 0x2D;           // error 1
		} else if (state == FP_State_Closed) {
			err_type = 0x2E;           // error 2
		}
		break;

	// User defined event //
	case Do_Auth:
		if (state >= FP_State_Fuelling) {
			err_type = 0x31;           // error 5
		} else if (state == FP_State_Inoperative) {
			err_type = 0x2D;           // error 1
		} else if (state == FP_State_Closed) {
			err_type = 0x2E;           // error 2
		} else if (state == FP_State_Idle) {
			RunLog("Node[%d]FP[%d]��ǰ״̬��������Ȩ", node, fp_pos + 1);
			ret = -2;
		}

		if (ReachedUnPaidLimit(node, fp_pos + 0x21)) {
			ret = -2;
		}

		break;
	case Do_Ration:
		if (state >= FP_State_Calling) {
			RunLog("Node[%d]FP[%d]��ǰ״̬������Ԥ��", node, fp_pos + 1);
			ret = -2;
			for (i = 11; i < msgbuffer[1]; ++i) {
				if (msgbuffer[i] == 0x1b) {
					msgbuffer[i + 1] = 0x06;  // DataACK
					break;
				}
			}
		} else if (state == FP_State_Inoperative) {
			err_type = 0x2D;           // error 1
		} else if (state == FP_State_Closed) {
			err_type = 0x2E;           // error 2
		}
		
		if (ReachedUnPaidLimit(node, fp_pos + 0x21)) {
			ret = -2;
		}

		break;
	default:
		break;
	}


	if (err_type != 0x00) {
		// 1. NACK + ERR_MSG
		ChangeER_DATA(node, 0x21 + fp_pos, err_type, msgbuffer);

		ret = -1;
	} else if (ret == -2) {
		// 1. NACK
		sendbuffer[10] = 0x05;  // MsgACK
		sendbuffer[msgbuffer[0] + 1] = 0x06;  // DataACK
		ret = SendData2CD(sendbuffer, msgbuffer[1], 5);
		if (ret < 0) {
			RunLog("Send command Acknowledge Message to CD failed!");
		}

		ret = -2;
	}

	// 2. FP_STATE
	if (ret < 0) {
		SendFP_State(node, 0x21 + fp_pos);
	}

	return ret;
}

/*
 * func: ����DoCmd��ACK����
 * param: msgbuffer[2] �Ժ�Ϊ��׼Ack Message;
 *        msgbuffer[1] ΪAck Message�ĳ���;
 *        msgbuffer[0] Ϊ�ϲ�FP������event������Ack Message�е�λ�� (0 - msgbuffer[1]-1)
 * ret: 0 - ���ͳɹ�; -1 - ����ʧ��
 */
int SendCmdAck(const signed char ack_type, unsigned char *msgbuffer)
{
	int ret;
	__u8 *ack_msg;

	if (msgbuffer == NULL) {
		return -1;
	}

	ack_msg = msgbuffer + 2;
	if (ack_type != ACK) {
		ack_msg[10] = 0x05;  // MsgACK
		ack_msg[msgbuffer[0] + 1] = 0x06;  // DataACK
	} else {
		ack_msg[10] = 0x00;  // MsgACK
		ack_msg[msgbuffer[0] + 1] = 0x00;  // DataACK
	}

	ret = SendData2CD(ack_msg, msgbuffer[1], 5);
	if (ret < 0) {
		RunLog("Send command Acknowledge Message to CD failed!");
	}

	return ret;
}

/*
 * ʵʱ���������ϴ���ʱ
 * ���ǽ��̵����Լ�������ʱ��, ��ʱʱ���ȥ���ɺ���
 */
static int inline RunningMsgDelay(FP_DATA *pfp)
{
	int interval = 0;   // 0 - 99.9s
	if (pfp == NULL) {
		return -1;
	}

	interval = (pfp->Running_Transaction_Message_Frequency[1] & 0x0f) +
		(((pfp->Running_Transaction_Message_Frequency[1] & 0xf0) >> 4) * 10) +
		((pfp->Running_Transaction_Message_Frequency[0] & 0x0f) * 100);


	if (interval == 0) {
		Ssleep(500000);    // (500ms) , by default , �����ͻ��������õ�ʱ��������0.7s
	} else {
#if 0
		RunLog("**************************** interval: %d , time: %ld *************************",
		interval, 100000 * (interval -7));
#endif
		Ssleep(100000 * (interval - 7));  // user defined
	}

	return 0;
}

/*
 * func: �����͵�ǰFP��״̬, ��FP��״̬Ӧ��ֻ�����ͻ�����ģ����иı�,
 *      �������̲�����ChangeFP_State�Ĳ���, ������ܳ���״̬�Ĵ���ת��; �����Ӵ˺���
 */
int SendFP_State(const __u8 node, const __u8 fp_ad) 
{
	int i, ret;
	int tmp_lg = 0;  //recv_lg;
	int fp = fp_ad - 0x21;  //0,1,2,3�ż��͵�.
	unsigned char buffer[64] = {0};

	if ((node <= 0) || (node > 127)) {
		UserLog("���ͻ��ڵ�ֵ���� SendFP_State ʧ��");
		return -1;
	}

	if ((fp < 0) || (fp > 3)) {
		UserLog("���ͻ����͵��ַ����,SendFP_State ʧ��");
		return -1;
	}	

	for (i = 0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node) {
		      	break; //�ҵ��˼��ͻ��ڵ�
		}
	}

	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���, SendFP_State ʧ��");
		return -1;
	}

	ret = MakeFpMsg(node, fp_ad, buffer, &tmp_lg);
	if(ret < 0){
		UserLog("���ͻ����͵��Զ����ݹ������, SendFP_State ʧ��");
		return -1;
	}

	RunLog("Node %d FP %d Send Current FP_State to CD", node, fp_ad - 0x20);
	ret = SendData2CD(buffer, tmp_lg, 5);
	if (ret < 0) {
		return -1;
	}

	
	return 0;
}







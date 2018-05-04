/*******************************************************************
�߼���ǹ�Ĳ�������.
-------------------------------------------------------------------
2007-07-16 by chenfm
2008-03-06 - Guojie Zhu <zhugj@css-intelligent.com>
   convertUsed[] ��Ϊ convert_hb[].lnao.node
2008-03-12 - Guojie Zhu <zhugj@css-intelligent.com>
    ���� ReadLN_DATA �ظ����ĸ�ʽ����
*******************************************************************/

#include "ifsf_pub.h"

#include "bcd.h"
extern IFSF_SHM *gpIfsf_shm; //�ýṹ��ȫ�ֵ�����ֻ��һ��,��ifsf_main.c�ж���.

/**
*  destbuf �õ�ȫ���ͻ�����
*  010101070a000000011738
*  01    01  02   07    0a000000011738
*  node fp nzl  point  total
* 
*  destlen all len return to call user
**/
int GetAllLNTotal_Node(char *destbuf, int *destlen, int node)
{
	int i = 0, flag = 0, j = 0, fp = 0, nzl = 0, cntgun = 0;
	DISPENSER_DATA * dispenser_data = NULL;
	FP_LN_DATA * pfp_ln_data = NULL;
	int step = 11; //ÿ���εĳ���
	
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,�߼���ǹRead ʧ��");
		return -1;
	}
	
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if(node == gpIfsf_shm->node_channel[i].node)  {
			flag = 1; //��ʾ�нڵ�����
			RunLog("DDDDDDDDDDDDDDD  node %d", node);
			dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
			for (j = 0; j <MAX_GUN_CNT; j++) {
				if (gpIfsf_shm->gun_config[j].node == node && \
					(fp = gpIfsf_shm->gun_config[j].logfp) != 0 &&\
					(nzl = gpIfsf_shm->gun_config[j].logical_gun_no) != 0) {
					
					pfp_ln_data =  (FP_LN_DATA *)( &(dispenser_data->fp_ln_data[fp - 1][nzl - 1])  );
					destbuf[cntgun * step + 0] = node;
					destbuf[cntgun * step + 1] = fp;
					destbuf[cntgun * step + 2] = nzl;
					destbuf[cntgun * step + 3] = 0x07;
					memcpy (&destbuf[cntgun * step + 4] , pfp_ln_data->Log_Noz_Vol_Total, sizeof(pfp_ln_data->Log_Noz_Vol_Total));
					cntgun++;
				}
			}
			
			
		}
	}
	if(!flag ){
		UserLog("���ͻ��ڵ������Ҳ���,�߼���ǹRead  ʧ��");
		return -1;
	}
	*destlen = cntgun * step;
	return 0;
}


//FP_LN_DATA��������. node���ͻ��߼��ڵ��,fp_ad���͵��ַ21h-24h,ln_ad�߼���ǹ��ַ11h-18h,
//Ҫ�õ�gpIfsf_shm,ע��P,V����������.
int DefaultLN_DATA(const char node, const char fp_ad, const char ln_ad){
	int i, fp_state;
	int fp=fp_ad-0x21; //0,1,2,3�ż��͵�.
	int ln=ln_ad-0x11; //0,1,2,3,4,5,6,7���߼���ǹ.
	int ret;
	int lnz_idx;
	unsigned char tmpUChar;
	DISPENSER_DATA *dispenser_data;
	FP_LN_DATA *pfp_ln_data;

	if( (node <= 0) || (node > 127 ) ){
		UserLog("���ͻ��ڵ�ֵ����,�߼���ǹĬ������ ʧ��");
		return -1;
		}
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,�߼���ǹĬ������  ʧ��");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,�߼���ǹĬ������ ʧ��");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	pfp_ln_data =  (FP_LN_DATA *)( &(dispenser_data->fp_ln_data[fp][ln])  );
	fp_state = dispenser_data->fp_data[fp].FP_State;
	if( fp_state >9 ) return -1;	
	
	pfp_ln_data->PR_Id = '\x01';	
	//DefaultPR_DAT(node, 0x40+pfp_ln_data->PR_Id);
	//DefaultFM_DAT(node, 0x40+pfp_ln_data->PR_Id);
	pfp_ln_data->Physical_Noz_Id = '\x01';
	pfp_ln_data->Meter_1_Id = '\x00';
	//DefaultM_DAT(node, 0x81); // 0x80 + pfp_ln_data->Meter_1_Id	
	bzero(pfp_ln_data->Log_Noz_Vol_Total, 7);
	bzero(pfp_ln_data->Log_Noz_Amount_Total, 7);
	bzero(pfp_ln_data->No_TR_Total, 7);
//	bzero(pfp_ln_data->Log_Noz_SA_Vol_Total, 7);
//	bzero(pfp_ln_data->Log_Noz_SA_Amount_Total, 7);
//	bzero(pfp_ln_data->No_TR_SA_Total, 7);
	
	return 0;
}


//��ȡFP_LN_DATA. �������Ϣ���ݵ�����Ϊ001�ش�.
//recvBufferΪ�յ���data,sendBufferΪҪ���͵�data,msg_lgΪ���ɵ��ܳ���.
//�����ǹ����Ϊ0����ô����99
int ReadLN_DATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg){
	int i, j, k,fp_state,recv_lg, tmp_lg,ret, chnlog, cmd_lg, l;
	int fp=recvBuffer[9]-0x21; //0,1,2,3�ż��͵�.
	int ln=recvBuffer[10]-0x11; //0,1,2,3,4,5,6,7���߼���ǹ.	
	char node=recvBuffer[1];	
	unsigned char zeroTotal[6] = {0};
	//unsigned char tmpUChar;	
	DISPENSER_DATA *dispenser_data;
	FP_LN_DATA *pfp_ln_data;	
	unsigned char buffer[1024], cmd[13]={0}, zeroBuff[7] = {0}, nodeDriver;
	memset(buffer, 0 , 1024);
	if( recvBuffer[8] != (unsigned char)2 ){
		UserLog("���ͻ��߼���ǹ��ַ����,�߼���ǹRead  ʧ��");
		return -1;
	}
	if( (node <= 0) || (node > 127 ) ){
		UserLog("���ͻ��ڵ�ֵ����,�߼���ǹRead  ʧ��");
		return -1;
	}
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,�߼���ǹRead ʧ��");
		return -1;
	}
		
	//��ȡ�ͻ�Ʒ�ƣ���ɽTQC�ͻ���̨��ȡ����ʱ�����ͻ���ȡ
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
	nodeDriver = gpIfsf_shm->node_channel[i].nodeDriver;

	
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,�߼���ǹRead  ʧ��");
		return -1;
	}
	
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	pfp_ln_data =  (FP_LN_DATA *)( &(dispenser_data->fp_ln_data[fp][ln])  );
	fp_state = dispenser_data->fp_data[fp].FP_State;
	if (fp_state > 9) {
		return -1;  
	}
	buffer[0] = recvBuffer[2];
	buffer[1] = recvBuffer[3];
	buffer[2] = recvBuffer[0];
	buffer[3] = recvBuffer[1];
	buffer[4] = '\0'; //recvBuffer[4]; IFSF_MC
	buffer[5] = (recvBuffer[5] |0x20);//answer.
	recv_lg = recvBuffer[6];
	recv_lg = recv_lg * 256 + recvBuffer[7];
	buffer[8] = '\x02'; //recvBuffer[8]; //Ad_lg
	buffer[9] = recvBuffer[9];  //ad
	buffer[10] = recvBuffer[10]; //ad
	//buffer[11] = recvBuffer[11]; //It's Data_Id. Data begin.	
	tmp_lg=10;
	for(i=0;i<recv_lg - 3; i++){
		switch(recvBuffer[11+i]) {
			case LN_PR_Id :
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)LN_PR_Id;
				tmp_lg++;
				buffer[tmp_lg] = '\x01';
				tmp_lg++;
				buffer[tmp_lg] = pfp_ln_data->PR_Id;
				break;
			/*
			case LN_Hose_Expansion_Vol :
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)LN_Hose_Expansion_Vol;
				tmp_lg++;
				buffer[tmp_lg] = '\0';				
				break;
			case LN_Slow_Flow_Valve_Activ:	
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)LN_Slow_Flow_Valve_Activ;
				tmp_lg++;
				buffer[tmp_lg] = '\0';				
				break;
			*/
			case LN_Physical_Noz_Id :
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)LN_Physical_Noz_Id;
				tmp_lg++;
				buffer[tmp_lg] = '\x01';
				tmp_lg++;
				buffer[tmp_lg] = pfp_ln_data->Physical_Noz_Id;
				break;
			case LN_Meter_1_Id:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)LN_Meter_1_Id;
				tmp_lg++;
				buffer[tmp_lg] = '\x01';
				tmp_lg++;
				buffer[tmp_lg] = pfp_ln_data->Meter_1_Id;
				break;
			/*
			case LN_Meter_1_Blend_Ratio:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)LN_Meter_1_Blend_Ratio;
				tmp_lg++;
				buffer[tmp_lg] = '\0';
				break;
			case LN_Meter_2_Id:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)LN_Meter_2_Id;
				tmp_lg++;
				buffer[tmp_lg] = '\0';				
				break;
			case LN_Logical_Nozzle_Type:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)LN_Logical_Nozzle_Type;
				tmp_lg++;
				buffer[tmp_lg] = '\0';
				break;
			case LN_Preset_Valve_Activation:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)LN_Preset_Valve_Activation;
				tmp_lg++;
				buffer[tmp_lg] = '\0';
				break;
			*/
			case LN_Log_Noz_Vol_Total:
				//��ɽTQC�ͻ����׽����󲻶�ȡ���룬��̨��ȡ����ʱʵʱ���ͻ�ͬ��
				//TQC�ͻ����׽��������仯�ӳٴ�
				if((gpIfsf_shm->get_total_flag != node) && (nodeDriver == 0x14)){

					gpIfsf_shm->get_total_flag = node;
					cmd[0] = 0x6D;
					cmd[1] = recvBuffer[10] - 0x10;  //�߼���ǹ�� 11 -18H
					cmd[2] = recvBuffer[9];
					cmd_lg = 13;

					//��ȡ����
					ret = SendMsg(ORD_MSG_TRAN, cmd , cmd_lg, chnlog, 3);
					if (ret < 0) {
					       	return ret;
					}
					RunLog( "Node[%d].FP[%d]�·�����[%s], ����[%d]",node,fp+1, Asc2Hex(cmd, cmd_lg ), ret);

					bzero(cmd, sizeof(cmd));
					cmd_lg = sizeof(cmd) -1;
						
					ret = RecvMsg(TOTAL_MSG_TRAN, cmd, &cmd_lg, &chnlog, -1); 
					if( (ret < 0) || (cmd[1] == 0xee) ){
						gpIfsf_shm->get_total_flag = 0; //��ȡʧ�� ���´��ظ���ȡ�˽ڵ�
						RunLog("Node[%d]��ȡ����ʧ��!ret = [%d]",node, ret);
					}else if(cmd[1] == 0x55){
						RunLog("Node[%d]��ȡ����ɹ�!ret = [%d]",node, ret);
					}
				}

				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)LN_Log_Noz_Vol_Total;
				tmp_lg++;
				buffer[tmp_lg] = '\x07';	
				k = buffer[tmp_lg];

				for (j=0; j<k; j++)  {
					tmp_lg++;
					buffer[tmp_lg] = pfp_ln_data->Log_Noz_Vol_Total[j];
				}
				break;
			case LN_Log_Noz_Amount_Total:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)LN_Log_Noz_Amount_Total;
				tmp_lg++;
				buffer[tmp_lg] = '\x07';
				k = buffer[tmp_lg] ;
				for (j= 0; j< k; j++)  {
					tmp_lg++;
					buffer[tmp_lg] = pfp_ln_data->Log_Noz_Amount_Total[j];
				}
				break;
			case LN_No_TR_Total:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)LN_No_TR_Total;
				tmp_lg++;
				buffer[tmp_lg] = '\x07';
				k = buffer[tmp_lg] ;
				for (j=0; j<k; j++)  {
					tmp_lg++;
					buffer[tmp_lg] = pfp_ln_data->No_TR_Total[j];
				}
				break;
			case LN_Log_Noz_SA_Vol_Total :
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)LN_Log_Noz_SA_Vol_Total;
				tmp_lg++;
				buffer[tmp_lg] = '\x07';
				k = buffer[tmp_lg] ;
				for (j=0; j<k; j++)  {
					tmp_lg++;
					buffer[tmp_lg] = pfp_ln_data->Log_Noz_SA_Vol_Total[j];
				}
				break;
			case LN_Log_Noz_SA_Amount_Total:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)LN_Log_Noz_SA_Amount_Total;
				tmp_lg++;
				buffer[tmp_lg] = '\x07';
				k = buffer[tmp_lg] ;
				for (j=0; j<k; j++)  {
					tmp_lg++;
					buffer[tmp_lg] = pfp_ln_data->Log_Noz_SA_Amount_Total[j];
				}
				break;
			case LN_No_TR_SA_Total:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)LN_No_TR_SA_Total;
				tmp_lg++;
				buffer[tmp_lg] = '\x07';
				k = buffer[tmp_lg] ;
				for (j=0; j<k; j++)  {
					tmp_lg++;
					buffer[tmp_lg] = pfp_ln_data->No_TR_SA_Total[j];
				}
				break;
			case LN_All_Log_Noz_Vol_Total:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)LN_All_Log_Noz_Vol_Total;
				tmp_lg++;
				GetAllLNTotal_Node(&buffer[tmp_lg], &k, node);
				tmp_lg += k;
				break;
			default:
				tmp_lg++;
				buffer[tmp_lg] = recvBuffer[11+i];
				tmp_lg++;
				buffer[tmp_lg] = '\0';			
				break;			
		}	
	}

	tmp_lg++;
	buffer[6] = (unsigned char )((tmp_lg-8) / 256) ;
	buffer[7] = (unsigned char )((tmp_lg-8) %256);  //big end.
	*msg_lg= tmp_lg;
	memcpy(sendBuffer, buffer, tmp_lg);	

	return 0;
}

//дFP_LN_DATA����.recvBufferΪ�յ���data,sendBufferΪҪ�ظ���data,�������Ϣ���ݵ�����Ϊ111Ӧ��
//msg_lgΪ���ɵ��ܳ���,Ϊ0��ʾ����ҪӦ����Ϣ,sendBuffer��msg_lg����ҪӦ����Ϣ��������ΪNULL.
int WriteLN_DATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg){
	int i, j, k, fp_state,  recv_lg,  tmp_lg,  lg;
	int fp=recvBuffer[9]-0x21; //0,1,2,3�ż��͵�.
	int ln=recvBuffer[10]-0x11; //0,1,2,3,4,5,6,7���߼���ǹ.	
	int cmd_lg,ret;
	int lnz_idx;
	char node=recvBuffer[1];	
	unsigned char tmpMSAck, tmpDataAck;
	unsigned char buffer[MAX_TCP_LEN];
	unsigned char cmd[20];
	DISPENSER_DATA *dispenser_data;
	FP_LN_DATA *pfp_ln_data;	

	if( recvBuffer[8] != (unsigned char)2 ){
		UserLog("���ͻ��߼���ǹ��ַ����,�߼���ǹWrite  ʧ��");
		return -1;
	}
	if( (node <= 0) || (node > 127 ) ){
		UserLog("���ͻ��ڵ�ֵ����,�߼���ǹWrite  ʧ��");
		return -1;
	}
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,�߼���ǹWrite ʧ��");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,�߼���ǹWrite  ʧ��");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	pfp_ln_data =  (FP_LN_DATA *)( &(dispenser_data->fp_ln_data[fp][ln])  );
	fp_state = dispenser_data->fp_data[fp].FP_State;
	if( fp_state >9 ) return -1;
	
	memset(buffer, 0 , sizeof(buffer) );
	buffer[0] = recvBuffer[2]; 
	buffer[1] = recvBuffer[3];
	buffer[2] = recvBuffer[0];
	buffer[3] = recvBuffer[1]; 
	buffer[4] = '\0'; //recvBuffer[4]; IFSF_MC
	buffer[5] = ((recvBuffer[5] &0x1f)|0xe0); //MS_St
	recv_lg = recvBuffer[6];
	recv_lg = recv_lg * 256 + recvBuffer[7];
	buffer[8] = '\x02'; //recvBuffer[8];
	buffer[9] = recvBuffer[9];  // ad
	buffer[10] = recvBuffer[10]; // ad
	//buffer[11] = ; //MS_Ack
	//recvBuffer[11]; //It's Data_Id. Data begin.	
	tmp_lg=11;
	i=0;
	tmpMSAck = '\0'; // all is  ok.
	while( i < (recv_lg - 3) ) {
		tmpDataAck = '\0'; //ok ack.
		lg =  (int )recvBuffer[12+i]; //Data_Lg.
		switch(recvBuffer[11+i]) {
		case LN_PR_Id : //(1-8),W(1-2)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)LN_PR_Id;			
			if ((lg == 1) && (fp_state < 3) && (recvBuffer[13+i] >= 1) && (recvBuffer[13+i] <= 8)) {
				pfp_ln_data->PR_Id = recvBuffer[13+i];


				/*
				 * Added by Guojie zhu @ 2009.8.7 , ��Ʒ�������
				 */

				while (0xFF == gpIfsf_shm->cfg_changed) {
					RunLog("Waiting for SyncCfg2File.....");
					sleep(1);
				}

				for (lnz_idx = 0; lnz_idx < gpIfsf_shm->gunCnt; lnz_idx++) {
					if (gpIfsf_shm->gun_config[lnz_idx].node == node &&
						gpIfsf_shm->gun_config[lnz_idx].logfp ==
									(recvBuffer[9] - 0x20) &&
						gpIfsf_shm->gun_config[lnz_idx].logical_gun_no == 
									(recvBuffer[10] - 0x10)) {

						// �����µ�  Prod_Nb �� gun_config[]�� 
						memcpy(gpIfsf_shm->gun_config[lnz_idx].oil_no, 
							dispenser_data->fp_pr_data[(pfp_ln_data->PR_Id) - 1].Prod_Nb, 4);
						
						break;
					}
				}

				// �����иĶ�, ֪ͨ SyncCfg2File ͬ������ 
				gpIfsf_shm->cfg_changed = 0x0F;

				/*
				cmd[0] = '\x19';
				cmd[1] = '\x00';
				cmd[2] = recvBuffer[13+i] +0x40; //pr_ad
				cmd[3] = recvBuffer[9]; //fp_ad
				cmd[4] = recvBuffer[10]; //ln_ad
				cmd_lg =5;
				ret = SendMsg(ORD_MSG_TRAN, cmd , cmd_lg, node, 3);
				if(ret < 0) {
					tmpDataAck = '\x06';  
					tmpMSAck = '\x05';
					tmp_lg++;
					buffer[tmp_lg] =tmpDataAck;
					i = i + 2 + lg;
					break;
				}
				sleep(1); //test?
				if(pfp_ln_data->PR_Id != recvBuffer[13+i] ) {
					tmpDataAck = '\x06';  
					tmpMSAck = '\x05';
					}
				//RequestPR_DATA(node, 0x40 + LN_PR_Id);
				*/
				}
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';
				tmpMSAck = '\x05'; 
				//UserLog("write  C_DAT's Nb_Products error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';
				tmpMSAck = '\x05';
				//UserLog("write  C_DAT's Nb_Products error!too big or small.") 
				}			
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		/*
		case LN_Hose_Expansion_Vol :
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)LN_Hose_Expansion_Vol;
			tmp_lg++;
			buffer[tmp_lg] = '\0';				
			break;
		case LN_Slow_Flow_Valve_Activ:	
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)LN_Slow_Flow_Valve_Activ;
			tmp_lg++;
			buffer[tmp_lg] = '\0';				
			break;
		*/
		case LN_Physical_Noz_Id : //(1-8),W(1-2)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)LN_Physical_Noz_Id;
			/*
			if( ( lg == 1 ) && (fp_state < 3) && (recvBuffer[14+i] >= 1) && (recvBuffer[14+i] <= 8) ) {
				pfp_ln_data->Physical_Noz_Id = recvBuffer[13+i];
				//RequstPR_DATA(node, 0x40 + LN_PR_Id);
				}
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';
				tmpMSAck = '\x05'; 
				//UserLog("write  C_DAT's Nb_Products error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';
				tmpMSAck = '\x05';
				//UserLog("write  C_DAT's Nb_Products error!too big or small.") 
				}
			*/
			tmpDataAck = '\x02';
			tmpMSAck = '\x05'; 
				
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case LN_Meter_1_Id: //(0-16),W(1-2)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)LN_Meter_1_Id;
			/*
			if( ( lg == 1 ) && (fp_state < 3) && ( recvBuffer[14+i] <= 16) ) {
				pfp_ln_data->Meter_1_Id = recvBuffer[13+i];
				RequestM_DAT(node, 0x80+LN_Meter_1_Id);
				}
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';
				tmpMSAck = '\x05'; 
				//UserLog("write  C_DAT's Nb_Products error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';
				tmpMSAck = '\x05';
				//UserLog("write  C_DAT's Nb_Products error!too big or small.") 
				}
			*/
			tmpDataAck = '\x02';
			tmpMSAck = '\x05'; 
				
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		/*
		case LN_Meter_1_Blend_Ratio:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)LN_Meter_1_Blend_Ratio;
			tmp_lg++;
			buffer[tmp_lg] = '\0';
			break;
		case LN_Meter_2_Id:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)LN_Meter_2_Id;
			tmp_lg++;
			buffer[tmp_lg] = '\0';				
			break;
		case LN_Logical_Nozzle_Type:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)LN_Logical_Nozzle_Type;
			tmp_lg++;
			buffer[tmp_lg] = '\0';
			break;
		case LN_Preset_Valve_Activation:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)LN_Preset_Valve_Activation;
			tmp_lg++;
			buffer[tmp_lg] = '\0';
			break;
		*/
		case LN_Log_Noz_Vol_Total: //W(1-2)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)LN_Log_Noz_Vol_Total;
			/*
			if( ( lg == 7 ) && (fp_state < 3)  ) {
				for(j=lg-1; j>=0; j--) 
					pfp_ln_data->Log_Noz_Vol_Total[j] = recvBuffer[14+i+j];//big end.
				//RequstPR_DATA(node, 0x40 + LN_PR_Id);
				}
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';
				tmpMSAck = '\x05'; 
				//UserLog("write  C_DAT's Nb_Products error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';
				tmpMSAck = '\x05';
				//UserLog("write  C_DAT's Nb_Products error!too big or small.") 
				}
			*/
			tmpDataAck = '\x02';
			tmpMSAck = '\x05'; 
				
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 +lg;
			break;
		case LN_Log_Noz_Amount_Total:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)LN_Log_Noz_Amount_Total;
			/*
			if( ( lg == 7 ) && (fp_state < 3)  ) {
				for(j=lg-1; j>=0; j--)
					pfp_ln_data->Log_Noz_Amount_Total[j] = recvBuffer[14+i+j];//big end.
				//RequstPR_DATA(node, 0x40 + LN_PR_Id);
				}
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';
				tmpMSAck = '\x05'; 
				//UserLog("write  C_DAT's Nb_Products error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';
				tmpMSAck = '\x05';
				//UserLog("write  C_DAT's Nb_Products error!too big or small.") 
				}
			*/
			tmpDataAck = '\x02';
			tmpMSAck = '\x05'; 
				
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case LN_No_TR_Total:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)LN_No_TR_Total;
			/*
			if( ( lg == 7 ) && (fp_state < 3)  ) {
				for(j=lg-1; j>=0; j--)
					pfp_ln_data->No_TR_Total[j] = recvBuffer[14+i+j];//big end.
				//RequstPR_DATA(node, 0x40 + LN_PR_Id);
				}
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';
				tmpMSAck = '\x05'; 
				//UserLog("write  C_DAT's Nb_Products error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';
				tmpMSAck = '\x05';
				//UserLog("write  C_DAT's Nb_Products error!too big or small.") 
				}
			*/
			tmpDataAck = '\x02';
			tmpMSAck = '\x05'; 
				
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		/*
		case LN_Log_Noz_SA_Vol_Total :
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)LN_Log_Noz_SA_Vol_Total;			
			if( ( lg >= 4 ) && (fp_state < 3)  ) {
				for(j=lg-1; j>=0; j--)
					pfp_ln_data->Log_Noz_SA_Vol_Total [j] = recvBuffer[14+i+j];//big end.
				//RequstPR_DATA(node, 0x40 + LN_PR_Id);
				}
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';
				tmpMSAck = '\x05'; 
				//UserLog("write  C_DAT's Nb_Products error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';
				tmpMSAck = '\x05';
				//UserLog("write  C_DAT's Nb_Products error!too big or small.") 
				}			
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case LN_Log_Noz_SA_Amount_Total:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)LN_Log_Noz_SA_Amount_Total;			
			if( ( lg >= 4 ) && (fp_state < 3)  ) {
				for(j=lg-1; j>=0; j--)
					pfp_ln_data->LN_Log_Noz_SA_Amount_Total [j] = recvBuffer[14+i+j];//big end.
				//RequstPR_DATA(node, 0x40 + LN_PR_Id);
				}
			else  if (fp_state < 3)  { 
				tmpDataAck = '\x02';
				tmpMSAck = '\x05'; 
				//UserLog("write  C_DAT's Nb_Products error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';
				tmpMSAck = '\x05';
				//UserLog("write  C_DAT's Nb_Products error!too big or small.") 
				}			
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case LN_No_TR_SA_Total:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)LN_No_TR_SA_Total;			
			if( ( lg >= 4 ) && (fp_state < 3)  ) {
				for(j=lg-1; j>=0; j--)
					pfp_ln_data->LN_No_TR_SA_Total [j] = recvBuffer[14+i+j];//big end.
				//RequstPR_DATA(node, 0x40 + LN_PR_Id);
				}
			else  if (fp_state < 3)  { 
				tmpDataAck = '\x02';
				tmpMSAck = '\x05'; 
				//UserLog("write  C_DAT's Nb_Products error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';
				tmpMSAck = '\x05';
				//UserLog("write  C_DAT's Nb_Products error!too big or small.") 
				}			
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		*/
		default:
			tmp_lg++;
			buffer[tmp_lg] = recvBuffer[11+i];
			tmpDataAck = '\x04';  
			tmpMSAck = '\x05';
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;		
			break;			
		}
	}

	if ((i + 3) != recv_lg) {
		//UserLog("write LN_Dat error!! exit. ");
		UserLog("(i + 3) != recv_lg, write LN_Dat error!! exit. i[%d],recv_lg[%d]", i, recv_lg);
		//exit(0);
		return -2;
	}

	tmp_lg++;
	buffer[6] = (unsigned char )((tmp_lg-8) / 256) ; //M_Lg
	buffer[7] = (unsigned char )((tmp_lg-8) % 256);  //big end.
	buffer[11] = tmpMSAck; 	

	if(NULL != msg_lg) {
		*msg_lg= tmp_lg;
	}

	if(NULL != sendBuffer) {
	       	memcpy(sendBuffer, buffer, tmp_lg);
	}

	return 0;
}
//���õ���������,�����豸�����д��������߳�ʼ������.
//�ɹ�����0,ʧ�ܷ���-1����Data_Ack. data����Ϊ:Data_Id+Data_Lg+Data_El.
int SetSglLN_DATA(const char node, const char fp_ad, const char ln_ad, const unsigned char *data){
	int i, j, k, fp_state,   lg, ret;
	int fp=fp_ad-0x21; //0,1,2,3�ż��͵�.
	int ln=ln_ad-0x11; //0,1,2,3,4,5,6,7���߼���ǹ.	
	//char node=recvBuffer[1];	
	unsigned char  tmpDataAck;  //tmpMSAck,
	//unsigned char buffer[1024];
	DISPENSER_DATA *dispenser_data;
	FP_LN_DATA *pfp_ln_data;	
	if(  (fp <0)||(fp>3) ){
		UserLog("���ͻ����͵��ֵַ����,�߼���ǹset  ʧ��");
		return -1;
	}
	if(  (ln <0)||(ln>7) ){
		UserLog("���ͻ��߼���ǹ��ֵַ����,�߼���ǹset  ʧ��");
		return -1;
	}
	if( (node <= 0) || (node > 127 ) ){
		UserLog("���ͻ��ڵ�ֵ����,�߼���ǹset  ʧ��");
		return -1;
	}
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,�߼���ǹset ʧ��");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,�߼���ǹset  ʧ��");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	pfp_ln_data =  (FP_LN_DATA *)( &(dispenser_data->fp_ln_data[fp][ln])  );
	fp_state = dispenser_data->fp_data[fp].FP_State;
	if( fp_state >9 ) return -1;
	
	i=0;
	tmpDataAck = '\0'; //ok ack.
	lg =  (int )data[1]; //Data_Lg.
	switch(data[0]) {
		case LN_PR_Id : //(1-8),W(1-2)				
			if( ( lg == 1 ) && (fp_state < 3) && (data[2] >= 1) && (data[2] <= 8) ) {
				pfp_ln_data->PR_Id = data[2];
				//ret = DefaultPR_DAT(node, 0x40 + pfp_ln_data->PR_Id );
				//if(ret == 0)
				//	ret = RequestPR_DATA(node, 0x40 + pfp_ln_data->PR_Id );
				}
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';				
				//UserLog("write  C_DAT's Nb_Products error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';				
				//UserLog("write  C_DAT's Nb_Products error!too big or small.") 
				}			
			break;
		/*
		case LN_Hose_Expansion_Vol :
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)LN_Hose_Expansion_Vol;
			tmp_lg++;
			buffer[tmp_lg] = '\0';				
			break;
		case LN_Slow_Flow_Valve_Activ:	
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)LN_Slow_Flow_Valve_Activ;
			tmp_lg++;
			buffer[tmp_lg] = '\0';				
			break;
		*/
		case LN_Physical_Noz_Id : //(1-8),W(1-2)
			if( ( lg == 1 ) && (fp_state < 3) && (data[2] >= 1) && (data[2] <= 8) ) {
				pfp_ln_data->Physical_Noz_Id = data[2];
				//RequestPR_DATA(node, 0x40 + LN_PR_Id);
				}
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';				
				//UserLog("write  C_DAT's Nb_Products error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';				
				//UserLog("write  C_DAT's Nb_Products error!too big or small.") 
				}			
			break;
		case LN_Meter_1_Id: //(0-16),W(1-2)
			if( ( lg == 1 ) && (fp_state < 3) && (data[2] <= 16) ) {
				pfp_ln_data->Meter_1_Id = data[2];
				ret = DefaultM_DAT(node, 0x80 + pfp_ln_data->Meter_1_Id);
				if (ret == 0)
					ret = RequestM_DAT(node, 0x80 + pfp_ln_data->Meter_1_Id );
				//RequestPR_DATA(node, 0x40 + LN_PR_Id);
				}
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';				
				//UserLog("write  C_DAT's Nb_Products error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';				
				//UserLog("write  C_DAT's Nb_Products error!too big or small.") 
				}			
			break;
		/*
		case LN_Meter_1_Blend_Ratio:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)LN_Meter_1_Blend_Ratio;
			tmp_lg++;
			buffer[tmp_lg] = '\0';
			break;
		case LN_Meter_2_Id:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)LN_Meter_2_Id;
			tmp_lg++;
			buffer[tmp_lg] = '\0';				
			break;
		case LN_Logical_Nozzle_Type:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)LN_Logical_Nozzle_Type;
			tmp_lg++;
			buffer[tmp_lg] = '\0';
			break;
		case LN_Preset_Valve_Activation:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)LN_Preset_Valve_Activation;
			tmp_lg++;
			buffer[tmp_lg] = '\0';
			break;
		*/
		case LN_Log_Noz_Vol_Total: //W(1-2)
			if( ( lg == 7 ) && (fp_state < 3)  ) {
				for(j=lg-1; j>=0; j--)
					pfp_ln_data->Log_Noz_Vol_Total[j] = data[2+j];
				//RequestPR_DATA(node, 0x40 + LN_PR_Id);
				}
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';				
				//UserLog("write  C_DAT's Nb_Products error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';				
				//UserLog("write  C_DAT's Nb_Products error!too big or small.") 
				}		
			break;
		case LN_Log_Noz_Amount_Total:
			if( ( lg == 7 ) && (fp_state < 3)  ) {
				for(j=lg-1; j>=0; j--)
					pfp_ln_data->Log_Noz_Amount_Total[j] = data[2+j];
				//RequestPR_DATA(node, 0x40 + LN_PR_Id);
				}
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';				
				//UserLog("write  C_DAT's Nb_Products error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';				
				//UserLog("write  C_DAT's Nb_Products error!too big or small.") 
				}		
			break;
		case LN_No_TR_Total:
			if( ( lg == 7 ) && (fp_state < 3)  ) {
				for(j=lg-1; j>=0; j--)
					pfp_ln_data->No_TR_Total[j] = data[2+j];
				//RequestPR_DATA(node, 0x40 + LN_PR_Id);
				}
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';				
				//UserLog("write  C_DAT's Nb_Products error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';				
				//UserLog("write  C_DAT's Nb_Products error!too big or small.") 
				}		
			break;		
		case LN_Log_Noz_SA_Vol_Total :
			if( ( lg == 7 ) && (fp_state < 3)  ) {
				for(j=lg-1; j>=0; j--)
					pfp_ln_data->Log_Noz_SA_Vol_Total[j] = data[2+j];
				//RequestPR_DATA(node, 0x40 + LN_PR_Id);
				}
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';				
				//UserLog("write  C_DAT's Nb_Products error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';				
				//UserLog("write  C_DAT's Nb_Products error!too big or small.") 
				}	
			break;
		case LN_Log_Noz_SA_Amount_Total:
			if( ( lg == 7 ) && (fp_state < 3)  ) {
				for(j=lg-1; j>=0; j--)
					pfp_ln_data->Log_Noz_SA_Amount_Total[j] = data[2+j];
				//RequestPR_DATA(node, 0x40 + LN_PR_Id);
				}
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';				
				//UserLog("write  C_DAT's Nb_Products error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';				
				//UserLog("write  C_DAT's Nb_Products error!too big or small.") 
				}	
			break;
		case LN_No_TR_SA_Total:
			if( ( lg == 7 ) && (fp_state < 3)  ) {
				for(j=lg-1; j>=0; j--)
					pfp_ln_data->No_TR_SA_Total[j] = data[2+j];
				//RequestPR_DATA(node, 0x40 + LN_PR_Id);
				}
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';				
				//UserLog("write  C_DAT's Nb_Products error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';				
				//UserLog("write  C_DAT's Nb_Products error!too big or small.") 
				}	
			break;
		default:			
			tmpDataAck = '\x04'; 		
			break;			
	}
	return tmpDataAck;
}
int RequestLN_DATA(const char node, const char fp_ad, const char ln_ad){
	int i, fp_state, tmp_lg, ret;
	int fp=fp_ad-0x21; //0,1,2,3�ż��͵�.
	int ln=ln_ad-0x11; //0,1,2,3,4,5,6,7���߼���ǹ.	
	//char node=recvBuffer[1];	
	//unsigned char  tmpDataAck;  //tmpMSAck,
	unsigned char buffer[128];
	DISPENSER_DATA *dispenser_data;
	FP_LN_DATA *pfp_ln_data;	
	if(  (fp <0)||(fp>3) ){
		UserLog("���ͻ����͵��ֵַ����,�߼���ǹRequest  ʧ��");
		return -1;
	}
	if(  (ln <0)||(ln>7) ){
		UserLog("���ͻ��߼���ǹ��ֵַ����,�߼���ǹRequest  ʧ��");
		return -1;
	}
	if( (node <= 0) || (node > 127 ) ){
		UserLog("���ͻ��ڵ�ֵ����,�߼���ǹRequest  ʧ��");
		return -1;
	}	
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,�߼���ǹRequest ʧ��");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,�߼���ǹRequest  ʧ��");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	pfp_ln_data =  (FP_LN_DATA *)( &(dispenser_data->fp_ln_data[fp][ln])  );
	fp_state = dispenser_data->fp_data[fp].FP_State;
	if( fp_state >2 ) {
		UserLog("���ͻ����͵�״̬>2,�߼���ǹ����Request ");
		return -1;
	}
	
	buffer[0] = '\x02'; //CD
	buffer[1] = '\x01'; //CD 1
	buffer[2] = '\x01';
	buffer[3] = (unsigned char)node;
	buffer[4] = '\0'; //recvBuffer[4]; IFSF_MC
	buffer[5] = '\0'; //M_St
	
	buffer[8] = '\x02';  //Ad_lg
	buffer[9] = (unsigned char)fp_ad;  //ad
	buffer[10] = (unsigned char)ln_ad; //ad
	tmp_lg=10;
	tmp_lg++;
	buffer[tmp_lg] =(unsigned char)LN_PR_Id;	
	tmp_lg++;
	buffer[tmp_lg] =(unsigned char) LN_Physical_Noz_Id;
	tmp_lg++;
	buffer[tmp_lg] =(unsigned char) LN_Meter_1_Id;
	//tmp_lg++;
	//buffer[tmp_lg] =(unsigned char) LN_Meter_1_Id;
	tmp_lg++;
	buffer[tmp_lg] =(unsigned char) LN_Log_Noz_Vol_Total;
	tmp_lg++;
	buffer[tmp_lg] =(unsigned char) LN_Log_Noz_Amount_Total;
	tmp_lg++;
	buffer[tmp_lg] =(unsigned char) LN_No_TR_Total;

	tmp_lg++;
	buffer[6] =(unsigned char)((tmp_lg-8) /256) ;
	buffer[7] = (unsigned char)((tmp_lg-8) % 256) ; 
	ret= SendData2CD(buffer, tmp_lg, 5);
	if (ret <0 ){
		UserLog("���ͻ�ͨѶ����,�߼���ǹ����Request ");
		return -1;
	}
	return 0;
}

//һ�ʽ�������,LNZҪ�ı������. volum��������,amount���׽��.price���׵���
//int EndTr4Ln(const char node, const char fp_ad, const char ln_ad,  
//		const unsigned char * volume, const unsigned char * amount, const unsigned char * price)
int EndTr4Ln(const char node, const char fp_ad, const char ln_ad)
{
	int i, fp_state,ret1,ret2,ret3;
	int fp=fp_ad-0x21; //0,1,2,3�ż��͵�.
	int ln=ln_ad-0x11; //0,1,2,3,4,5,6,7���߼���ǹ.
	unsigned char tmpUChar[10];
	DISPENSER_DATA *dispenser_data;
	FP_LN_DATA *pfp_ln_data;

	if( (node <= 0) || (node > 127 ) ){
		UserLog("���ͻ��ڵ�ֵ����,�߼���ǹĬ������ ʧ��");
		return -1;
		}
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,�߼���ǹĬ������  ʧ��");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,�߼���ǹĬ������ ʧ��");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]));
	pfp_ln_data =  (FP_LN_DATA *)( &(dispenser_data->fp_ln_data[fp][ln]));
	fp_state = dispenser_data->fp_data[fp].FP_State;
	if(fp_state >9) return -1;

	memset(tmpUChar,0 , 10);
	//pfp_ln_data->PR_Id = '\x01';	
	
	//pfp_ln_data->Physical_Noz_Id = '\x01';
	//pfp_ln_data->Meter_1_Id = '\x01';
	//memcpy(&(pfp_ln_data->Log_Noz_Vol_Total), volume,6 )
	//if(amount != volume * price) {
	//	return -1;
	//	UserLog("������������ͼ۲����ڽ��!");
	//}
	//There used 6, I don't do with BinX. that means long number and Bcdx(in BinX+BcdX) have same fact.
	//BBcdAdd(unsigned char * bcd1, const unsigned char * bcd2, const int bcdlen1, const int bcdlen2)
//	pfp_ln_data->Log_Noz_Vol_Total[0] = 10;
//	pfp_ln_data->Log_Noz_Amount_Total[0] = 10;
	pfp_ln_data->No_TR_Total[0] = 0;
	ret3 = BBcdAddLong( (&pfp_ln_data->No_TR_Total[0]) , 1, 7); //7 must equel your real length.
	if (ret3 < 0) {
		memset( (&pfp_ln_data->No_TR_Total[0+ 1]) , 0, 6);
	}
#if 0
	ret1=BBcdAdd( (&pfp_ln_data->Log_Noz_Vol_Total[0+ 1]) , &volume[1], 6, 4); //6 must equel your real length.
	ret2=BBcdAdd( (&pfp_ln_data->Log_Noz_Amount_Total[0+ 1]) ,  &amount[1], 6,4); //6 must equel your real length.
	ret3=BBcdAddLong( (&pfp_ln_data->No_TR_Total[0]) , 1, 7); //7 must equel your real length.
	if ((ret1<0)||(ret2<0)||(ret3<0)){ //overflow.
		memset( (&pfp_ln_data->Log_Noz_Vol_Total[0+ 1]) , 0, 6);
		memset(  (&pfp_ln_data->Log_Noz_Amount_Total[0+ 1]) , 0, 6);
		memset( (&pfp_ln_data->No_TR_Total[0+ 1]) , 0, 6);
	}
#endif
		
	//pfp_ln_data->Log_Noz_SA_Vol_Total[] = '\0';
	//pfp_ln_data->Log_Noz_SA_Amount_Total[] = '\0';
	//pfp_ln_data->No_TR_SA_Total[] = '\0';
	
//	RunLog("##### end of EndTr4LNZ, tr_state: %02x #########", gpIfsf_shm->fp_tr_data[0].Trans_State);
	return 0;	
}

/*-----------------------------------------------------------------*/




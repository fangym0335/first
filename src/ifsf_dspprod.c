/*******************************************************************
��Ʒ��������
-------------------------------------------------------------------
2007-07-13 by chenfm
2008-03-04 - Guojie Zhu <zhugj@css-intelligent.com>
    ɾ������������ʱ��״̬�жϺ͵ȴ�һ�뷴��ִ�н���Ĳ���, ӦRetalixҪ����ճɹ��ͷ���ACK
2008-03-06 - Guojie Zhu <zhugj@css-intelligent.com>
    convertUsed[] ��Ϊ convert_hb[].lnao.node 
2008-03-12 - Guojie Zhu <zhugj@css-intelligent.com>
    ����ReadPR_DATA,ReadFM_DATA �ظ����ĵĸ�ʽ����
2008-03-16 - Guojie Zhu <zhugj@css-intelligent.com>
    ��������unit_price���ӶԶ�̬С����λ�õ�֧��(ʹ��TwoDecPoint())
*******************************************************************/
#include <string.h>

#include "ifsf_pub.h"


extern IFSF_SHM *gpIfsf_shm; //�ýṹ��ȫ�ֵ�����ֻ��һ��,��ifsf_main.c�ж���.
static int IsProdInUse(const __u8 *prod_nb);
static int AddProdToGlobalConfigList(const __u8 *prod_nb, const __u8 *prod_description);

//FP_PR_DATA��������,node���ͻ��߼��ڵ��,pr_ad��Ʒ��ַ,Ҫ�õ�gpIfsf_shm,ע��ʹ�÷��������ؼ���.
int DefaultPR_DAT(const char node, const unsigned char  pr_ad){ //����,���ݽڵ�ſ���ֱ��д.
	int i, fp_state;
	int m=(int )pr_ad - 0x41;//0,1,..7.
	DISPENSER_DATA *dispenser_data;
	FP_PR_DATA *fp_pr_data;
	if( (node <= 0) || (node > 127 ) ){
		UserLog("���ͻ��ڵ�ֵ����,��Ʒ��ʼ��ʧ��");
		return -1;
		}
	if( (m< 0) || (m > 8 ) ){
		UserLog("���ͻ���������ֵַ����,��Ʒ��ʼ��ʧ��");
		return -1;
	}
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,��Ʒ��ʼ��ʧ��");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,��Ʒ��ʼ��ʧ��");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	fp_pr_data =  (FP_PR_DATA *)( &(dispenser_data->fp_pr_data[m]) );
	for(i=0; i < MAX_FP_CNT; i++){ 
		fp_state = dispenser_data->fp_data[i].FP_State;
		if( fp_state >9 ) return -1;		
	}
	if( fp_state >3 ) return -1;	
	
//	fp_pr_data->Prod_Nb[3] = pr_ad - 0x40;  // commented by Guojie Zhu @ 2009.08.05(��ʼֵӦΪ0)
	
	return 0;
}


//��ȡFP_PR_DATA. FP_PR_DATA��ַΪ41h-48h.�������Ϣ���ݵ�����Ϊ001�ش�
//recvBufferΪ�յ���data,sendBufferΪҪ���͵�data,msg_lgΪ���ɵ��ܳ���.
int ReadPR_DATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg){
	int i,j, k, fp_state, recv_lg, tmp_lg ;
	int m=(int )recvBuffer[9] - 0x41;//0,1,..7.
	DISPENSER_DATA *dispenser_data;
	FP_PR_DATA *pfp_pr_data;
	unsigned char node = recvBuffer[1];
	unsigned char buffer[64];
	memset(buffer,0, 64);
	if( (node <= 0) || (node > 127 ) ){
		UserLog("���ͻ��ڵ�ֵ����,��ƷRead ʧ��");
		return -1;
		}
	if( (m< 0) || (m > 8 ) ){
		UserLog("���ͻ���������ֵַ����,��ƷRead ʧ��");
		return -1;
	}
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,��ƷRead ʧ��");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,��ƷRead ʧ��");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	pfp_pr_data =  (FP_PR_DATA *)( &(dispenser_data->fp_pr_data[m]) );
	for(i=0; i < MAX_FP_CNT; i++){ 
		fp_state = dispenser_data->fp_data[i].FP_State;
		if( fp_state >9 ) return -1;		
	}
	if( fp_state >3 ) return -1;	
	
	buffer[0] = recvBuffer[2];
	buffer[1] = recvBuffer[3];
	buffer[2] = recvBuffer[0];
	buffer[3] = recvBuffer[1];
	buffer[4] = '\0'; //recvBuffer[4];
	buffer[5] = (recvBuffer[5] |0x20);
	recv_lg = recvBuffer[6];
	recv_lg = recv_lg * 256 + recvBuffer[7];
	buffer[8] = '\x01'; //recvBuffer[8]; //Ad_lg
	buffer[9] = recvBuffer[9]; //Ad
	//buffer[10] = recvBuffer[10]; //It's Data_Id. Data begin.
	tmp_lg = 9;
	for(i=0; i < (recv_lg - 2); i++){
		switch( recvBuffer[10 + i] ){		
		case PR_Prod_Nb:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)PR_Prod_Nb;
			tmp_lg++;
			buffer[tmp_lg] ='\x04';
			k = buffer[tmp_lg];
			for (j = 0; j < k; j++){
				tmp_lg++;
				buffer[tmp_lg] = pfp_pr_data->Prod_Nb[j];
			}

			/*if (pfp_pr_data->Prod_Nb[0] != 0 && pfp_pr_data->Prod_Nb[1] != 0 \
				&& pfp_pr_data->Prod_Nb[2] != 0 && pfp_pr_data->Prod_Nb[3] != 0) {
				Update_Prod_No();
				memcpy(&buffer[tmp_lg - 3], pfp_pr_data->Prod_Nb, 4);
			}*/
			break;		
		case PR_Prod_Description:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)PR_Prod_Description;
			tmp_lg++;
			buffer[tmp_lg] = 16;
			k = buffer[tmp_lg];
			for (j = 0; j < k; j++){
				tmp_lg++;
				buffer[tmp_lg] = pfp_pr_data->Prod_Description[j];
			}
			break;
		default:
			tmp_lg++;
			buffer[tmp_lg] = recvBuffer[10 + i];
			tmp_lg++;
			buffer[tmp_lg] = '\0';			
			break;			
		}	
	}

	tmp_lg++;
	buffer[6] = (unsigned char )((tmp_lg-8) / 256) ;
	buffer[7] = (unsigned char )((tmp_lg-8) % 256);  //big end.
	*msg_lg= tmp_lg;
	memcpy(sendBuffer, buffer, tmp_lg);	
	
	return 0;
}
//дFP_PR_DATA����. recvBufferΪ�յ���data,sendBufferΪҪ�ظ���data,�������Ϣ���ݵ�����Ϊ111Ӧ��
//msg_lgΪ���ɵ��ܳ���,Ϊ0��ʾ����ҪӦ����Ϣ,sendBuffer��msg_lg����ҪӦ����Ϣ��������ΪNULL.
int WritePR_DATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg){
	int i,j, fp_state, recv_lg, tmp_lg, lg ;
	int m=(int )recvBuffer[9] - 0x41;//0,1,..7.
	DISPENSER_DATA *dispenser_data;
	FP_PR_DATA *pfp_pr_data;
	unsigned char node = recvBuffer[1];
	unsigned char tmpDataAck,  tmpMSAck ; 
	unsigned char buffer[64];
	unsigned char null_discription1[16];
	unsigned char null_discription2[16];
	unsigned char const zero4bytes[4] = {0, 0, 0, 0};

	memset(buffer,0, 64);
	memset(null_discription1, 0, sizeof(null_discription1));
	memset(null_discription2, ' ', sizeof(null_discription2));

	if( (node <= 0) || (node > 127 ) ){
		UserLog("���ͻ��ڵ�ֵ����,��ƷWrite ʧ��");
		return -1;
		}
	if( (m< 0) || (m > 8 ) ){
		UserLog("���ͻ���������ֵַ����,��ƷWrite ʧ��");
		return -1;
	}
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,��ƷRead ʧ��");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,��ƷWrite ʧ��");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	pfp_pr_data =  (FP_PR_DATA *)( &(dispenser_data->fp_pr_data[m]) );
	
	fp_state = 1;
	for(i=0; i < MAX_FP_CNT; i++){ 
		if(fp_state < dispenser_data->fp_data[i].FP_State)
			fp_state = dispenser_data->fp_data[i].FP_State;
		if( fp_state >9 ) 
			return -1;		
	}
	//if( fp_state >3 )return -1;	
	
	buffer[0] = recvBuffer[2]; //LNAR
	buffer[1] = recvBuffer[3]; //LNAR
	buffer[2] = recvBuffer[0]; //LNAO
	buffer[3] = recvBuffer[1]; //LNAO
	buffer[4] = '\0'; //recvBuffer[4]; //IFSF_MC
	buffer[5] = ( (recvBuffer[5] &0x1f)  |0xe0); //M_St:ack.
	recv_lg = recvBuffer[6];  //M_Lg, big end mode.
	recv_lg = recv_lg * 256 + recvBuffer[7]; 
	buffer[8] = '\x01'; //recvBuffer[8]; //Data_Ad_Lg
	buffer[9] = recvBuffer[9]; //Data_ad:M_DAt.
	//buffer[10] =  ; //MS_Ack
	//buffer[11] = ; //11:Data_Id, 12:Data_Ack
	//recvBuffer[10]:Data_Id, 11:Data_Lg, 12:Data_El.  Data begin here.
	tmp_lg = 10;
	i=0;
	tmpMSAck = '\0'; // all is  ok.
	while( i < (recv_lg -2) ) {
		tmpDataAck = '\0'; //ok ack.
		lg =  (int )recvBuffer[11+i]; //Data_Lg.
		switch( recvBuffer[10+i] ) {
		case PR_Prod_Nb://(1-9),w(1-2)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)PR_Prod_Nb;

			// 0.��Ʒ���ݿ��в���������ظ���Ʒ����, ���������ж�
			if (memcmp(zero4bytes, &recvBuffer[12 + i], 4) == 0) {
				// Prod_NbΪ��, ��ɾ������
				j = FP_PR_CNT;
			} else {
				// Prod_Nb����, ����ӻ����޸Ĳ���, ��Ҫ�ж��Ƿ��ظ�
				for (j = 0; j < FP_PR_CNT; ++j) {
					if ((memcmp(&(dispenser_data->fp_pr_data[j].Prod_Nb), \
								       	&recvBuffer[12 + i], 4) == 0) &&
									   (recvBuffer[9] - 0x41 != j)) {
						RunLog("��Ʒ(%s)�������Ѵ��������ݿ���[%02x],�������ظ�����", 
							Asc2Hex((unsigned char *)&recvBuffer[12 + i], 4), 0x41 + j);
						break;
					}
				}
			}

			if (j < FP_PR_CNT) {
				// ���ظ�����NAK
				tmpDataAck = '\x01';
				tmpMSAck = '\x05'; 
			} else if ((lg == 4) && (fp_state < 3) && 
					(memcmp(&recvBuffer[12 + i], zero4bytes, 4) == 0)) {
				// 1. ɾ����Ʒ��Ϣ

				if (memcmp(pfp_pr_data->Prod_Nb, zero4bytes, 4) != 0) {
					memcpy(pfp_pr_data->Prod_Nb, &recvBuffer[12 + i], 4);
					memset(pfp_pr_data->Prod_Description, ' ', 16);
					dispenser_data->fp_c_data.Nb_Products--;
					RunLog("ɾ����Ʒ(%s)��������Ϣ", 
						Asc2Hex((unsigned char *)&recvBuffer[12 + i], 4));
				} else {
					memset(pfp_pr_data->Prod_Description, ' ', 16);
					// ��������Pr_id���þ�Ϊ��, ����ɾ������, ���ܼ�����Ʒ����Nb_Products;
				}

				// ɾ�����ݿ��е�����,����ɾ�������ļ��е�����, ���Բ�ͬ���䶯�������ļ�
			} else if ((lg == 4) && (fp_state < 3)) {
				// 2. �޸Ļ��������Ʒ��Ϣ

				while (0xFF == gpIfsf_shm->cfg_changed) {
					RunLog("Waiting for SyncCfg2File.....");
					sleep(1);
				}

				RunLog("Node %d Pr_Id %02x Prod_Nb: %s", node, 
						recvBuffer[9], Asc2Hex(pfp_pr_data->Prod_Nb, 4));


				// 2.1 ���±���ȫ����Ʒ����
#if 0
				for (j = 0; j < gpIfsf_shm->oilCnt; ++j) {
					RunLog("oil_config[%d].oilno:%s", j, Asc2Hex(gpIfsf_shm->oil_config[j].oilno, 4));
					if (memcmp(&recvBuffer[12 + i], gpIfsf_shm->oil_config[j].oilno, 4) == 0) {
						RunLog("��Ʒ(%s)��������Ϣ�Ѵ���", 
							Asc2Hex((unsigned char *)&recvBuffer[12 + i], 4));
						break;
					}
				}


				// 2.2.2 ��Ʒ���ñ��в����ڸ���Ʒ����, �����
				if (j >= gpIfsf_shm->oilCnt) {
					memset(&(gpIfsf_shm->oil_config[gpIfsf_shm->oilCnt]), 0, sizeof(OIL_CONFIG));
					memcpy(gpIfsf_shm->oil_config[gpIfsf_shm->oilCnt].oilno, &recvBuffer[12 + i], 4);
					/*
					 * CD �п������޸�Prod_Description,
					 * �����ȿ�����ǰProd_Description ��Ϊ������¼����Ʒ����
					 */
					memcpy(gpIfsf_shm->oil_config[gpIfsf_shm->oilCnt].oilname,
								pfp_pr_data->Prod_Description, 16);
					// ������Ʒ��������
					(gpIfsf_shm->oilCnt)++;

					RunLog("������Ʒ(%s)��������Ϣ,����Ʒ��[%d]", 
						Asc2Hex((unsigned char *)&recvBuffer[12 + i], 4), gpIfsf_shm->oilCnt);
				}
#endif
				// �����Ʒ��Ϣ�������б�
				if (AddProdToGlobalConfigList(&recvBuffer[12 + i], NULL) == 0) {
					// 2.2 ���¸�Node��ǹ�����е���Ʒ��
					for (j = 0; j < gpIfsf_shm->gunCnt; ++j) {
						if (node == gpIfsf_shm->gun_config[j].node &&
							memcmp(pfp_pr_data->Prod_Nb, \
								gpIfsf_shm->gun_config[j].oil_no, 4) == 0) {
							RunLog("-- gun_config[%d].oil_no:%s", j,
									Asc2Hex(gpIfsf_shm->gun_config[j].oil_no, 4));
							memcpy(gpIfsf_shm->gun_config[j].oil_no, &recvBuffer[12 + i], 4);
						}
					}

					// 2.3 ����PR_DAT/C_DAT���ݿ�
					if (memcmp(pfp_pr_data->Prod_Nb, zero4bytes, 4) == 0) {
						// ����������Ʒ, ��������Ʒ����
						dispenser_data->fp_c_data.Nb_Products++;
					}

					memcpy(pfp_pr_data->Prod_Nb, &recvBuffer[12 + i], 4);
					gpIfsf_shm->cfg_changed = 0x0F;
					//RunLog("-----cfg_changed: %02x ", gpIfsf_shm->cfg_changed);
				} else {
					tmpDataAck = '\x01';
					tmpMSAck = '\x05'; 
				}
			} else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';
				tmpMSAck = '\x05'; 
				//UserLog("write  PR_DAT's Nb_Products error!fp state not in 1,2. ") 
			} else  { 
				tmpDataAck = '\x01';
				tmpMSAck = '\x05';
				//UserLog("write  PR_DAT's Nb_Products error!too big or small.") 
			}

			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case PR_Prod_Description:  // R(1-9),W(1-2)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)PR_Prod_Description;

			while (0xFF == gpIfsf_shm->cfg_changed) {
				RunLog("Waiting for SyncCfg2File.....");
				sleep(1);
			}

			// 0.��Ʒ���ݿ��в���������ظ���Ʒ����, ���������ж�
			if ((memcmp(null_discription1, &recvBuffer[12 + i], 16) == 0) ||
					 (memcmp(null_discription2, &recvBuffer[12 + i], 16) == 0)) {
				j = FP_PR_CNT;
			} else {
				for (j = 0; j < FP_PR_CNT; ++j) {
					if ((memcmp(&(dispenser_data->fp_pr_data[j].Prod_Description),  \
									&recvBuffer[12 + i], 16) == 0) &&
									   (recvBuffer[9] - 0x41 != j)) {
						RunLog("��Ʒ(%s)�������Ѵ��������ݿ���[%02x],�������ظ�����", 
							Asc2Hex((unsigned char *)&recvBuffer[12 + i], 16), 0x41 + j);
						break;
					}
				}
			}

			if (j < FP_PR_CNT) {
				// �����ظ���, ��ظ�NAK
				tmpDataAck = '\x03';
				tmpMSAck = '\x05'; 
			} else if ((lg == 16) && (fp_state < 3)) {
				// 1. �޸���Ʒ�����е���Ʒ����
				for (j = 0; j < gpIfsf_shm->oilCnt; ++j) {
					if (memcmp(pfp_pr_data->Prod_Nb, gpIfsf_shm->oil_config[j].oilno, 4) == 0) {
						RunLog("-- oil_config[%d].oilno:%s", j,
							       	Asc2Hex(gpIfsf_shm->oil_config[j].oilno, 4));

						memcpy(gpIfsf_shm->oil_config[j].oilname, &recvBuffer[12 + i], 16);

						break;
					}
				}

				if (j >= gpIfsf_shm->oilCnt) {
					RunLog("ԭʼ������δ�ҵ�����Ʒ(%s)��������Ϣ", 
						Asc2Hex((unsigned char *)&(gpIfsf_shm->oil_config[j].oilno), 4));
				}

				gpIfsf_shm->cfg_changed = 0x0F;

				// 2. �޸ı������ݿ��е���Ʒ����
				memcpy(pfp_pr_data->Prod_Description, &recvBuffer[12 + i], 16);
				UserLog("write Prod_Description[%s]", Asc2Hex(pfp_pr_data->Prod_Description, 16));
			} else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';
				tmpMSAck = '\x05'; 
				//UserLog("write  PR_DAT's Nb_Products error!fp state not in 1,2. ") 
			} else  { 
				tmpDataAck = '\x01';
				tmpMSAck = '\x05';
				//UserLog("write  PR_DAT's Nb_Products error!too big or small.") 
			}			

			tmp_lg++;
			buffer[tmp_lg] = tmpDataAck;
			i = i + 2 + lg;
			break;
		default:
			tmp_lg++;
			buffer[tmp_lg] =recvBuffer[10+ i];
			tmpDataAck = '\x04';  
			tmpMSAck = '\x05';
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		}
	}
	
	if( (i + 2) != recv_lg )	{
		//UserLog("write PR_Dat error!! exit. ");
		UserLog("i  != recv_lg-2, write PR_Dat error!! exit, recv_lg[%d],i [%d] ",recv_lg, i);
		//exit(0);
		return -2;
	}
	tmp_lg++;
	buffer[6] = (unsigned char )((tmp_lg-8) / 256); //M_Lg
	buffer[7] = (unsigned char )((tmp_lg-8) % 256);  //big end.
	buffer[10] = tmpMSAck;  
	if(NULL != msg_lg) 
		*msg_lg= tmp_lg;
	if(NULL != sendBuffer)
		memcpy(sendBuffer, buffer, tmp_lg);
	return 0;		
}
int SetSglPR_DATA(const char node, const unsigned char pr_ad, const unsigned char *data){
	int i,  fp_state,lg ;
	int m=(int )pr_ad- 0x41;//0,1,..7.
	DISPENSER_DATA *dispenser_data;
	FP_PR_DATA *pfp_pr_data;
	//unsigned char node = recvBuffer[2];
	unsigned char tmpDataAck; // tmpMSAck ; 
	//unsigned char buffer[64];
	//memset(buffer,0, 64);
	if( (node <= 0) || (node > 127 ) ){
		UserLog("���ͻ��ڵ�ֵ����,��ƷSetSgl ʧ��");
		return -1;
		}
	if( (m< 0) || (m > 8 ) ){
		UserLog("���ͻ���������ֵַ����,��ƷSetSgl ʧ��");
		return -1;
	}
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,��ƷSetSgl ʧ��");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,��ƷSetSgl ʧ��");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	pfp_pr_data =  (FP_PR_DATA *)( &(dispenser_data->fp_pr_data[m]) );
	for(i=0; i < MAX_FP_CNT; i++){ 
		fp_state = dispenser_data->fp_data[i].FP_State;
		if( fp_state >9 ) return -1;		
	}
	if( fp_state >3 ) return -1;		
	
	tmpDataAck = '\0'; //ok ack.
	lg =  (int )data[1]; //Data_Lg.
	switch( data[0] )
	{
		case PR_Prod_Nb://(1-8),w(1-2)			
			if( ( lg== 4 ) && (fp_state < 3)   ) {
				for(i =0;i < lg; i++)
					pfp_pr_data->Prod_Nb[i] = data[2+i];
				
			}
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';				
				//UserLog("write  PR_DAT's Nb_Products error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';
				//UserLog("write  PR_DAT's Nb_Products error!too big or small.") 
				}			
			break;		
		default:			
			tmpDataAck = '\x04';  			
			break;
	}	
	
	return tmpDataAck;		
	}
//����Prod_Nb[4]��ֵ.prod_nbΪ����ֵ.
int SetPR_DATA_Nb(const char node, const unsigned char pr_ad, const unsigned char *prod_nb){
	int i,  fp_state,lg ;
	int m=(int )pr_ad- 0x41;//0,1,..7.
	DISPENSER_DATA *dispenser_data;
	FP_PR_DATA *pfp_pr_data;
	//unsigned char node = recvBuffer[2];
	unsigned char tmpDataAck; // tmpMSAck ; 
	//unsigned char buffer[64];
	//memset(buffer,0, 64);
	if( (node <= 0) || (node > 127 ) ){
		UserLog("���ͻ��ڵ�ֵ����,��ƷSet�ͺ�ʧ��");
		return -1;
		}
	if( (m< 0) || (m > 8 ) ){
		UserLog("���ͻ���������ֵַ����,��ƷSet�ͺ� ʧ��");
		return -1;
	}
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,��ƷSet�ͺ� ʧ��");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,��ƷSet�ͺ� ʧ��");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	pfp_pr_data =  (FP_PR_DATA *)( &(dispenser_data->fp_pr_data[m]) );
	for(i=0; i < MAX_FP_CNT; i++){ 
		fp_state = dispenser_data->fp_data[i].FP_State;
		if( fp_state >9 ) return -1;		
	}
	if( fp_state >3 )
		return -1;
		//case PR_Prod_Nb://(1-8),w(1-2)		
	for(i =0;i < 4; i++)
		pfp_pr_data->Prod_Nb[i] = prod_nb[i];
	
			
	return 0;		
}
int RequestPR_DATA(const char node,const unsigned char pr_ad){
	int i,  fp_state, tmp_lg, lg ;
	int m=(int )pr_ad- 0x41;//0,1,..7.
	DISPENSER_DATA *dispenser_data;
	FP_PR_DATA *pfp_pr_data;
	//unsigned char node = recvBuffer[2];
	unsigned char tmpDataAck; // tmpMSAck ; 
	unsigned char buffer[64];
	memset(buffer,0, 64);
	if( (node <= 0) || (node > 127 ) ){
		UserLog("���ͻ��ڵ�ֵ����,��ƷRequest ʧ��");
		return -1;
		}
	if( (m< 0) || (m > 8 ) ){
		UserLog("���ͻ���������ֵַ����,��ƷRequest ʧ��");
		return -1;
	}
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,��ƷRequest ʧ��");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,��ƷRequest ʧ��");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	pfp_pr_data =  (FP_PR_DATA *)( &(dispenser_data->fp_pr_data[m]) );
	for(i=0; i < MAX_FP_CNT; i++){ 
		fp_state = dispenser_data->fp_data[i].FP_State;
		if( fp_state >9 ) return -1;		
	}
	if( fp_state >2 ) {
		UserLog("���͵�״̬����2,������������.");
		return -1;
	}
	buffer[0] = '\x02';
	buffer[1] = '\x01';
	buffer[2] = '\x01';
	buffer[3] = (unsigned char)node;
	buffer[4] = '\0';  //IFSF_MC
	buffer[5] =  '\0'; //M_St; //read.
	
	buffer[8] = '\x01'; //recvBuffer[8]; //Ad_lg
	buffer[9] = pr_ad; //recvBuffer[9]; //Ad
	//buffer[10] = recvBuffer[10]; //It's Data_Id. Data begin.
	tmp_lg=9;
	
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)PR_Prod_Nb  ;
	
	tmp_lg++;
	buffer[6] =(unsigned char)(tmp_lg /256) ;
	buffer[7] = (unsigned char)(tmp_lg % 256) ; 
	//i use to be ret.
	i= SendData2CD(buffer, tmp_lg, 5);
	if (i <0 ){
		UserLog("���ͻ�ͨѶ����,����������Request ");
		return -1;
	}
	return 0;
	
}
/*
//ÿ�ּ���ģʽ�µ���Ʒ. ��IFSF 3-01,V2.44,91-93, ��ַ:61h+00000001-99999999BCD+11h-18h.
enum FP_FM_DAT_AD{Fuelling_Mode_Name = 1,  Prod_Price = 2, Max_Vol,
	Max_Fill_Time, Max_Auth_Time, User_Max_Volume }; //gFp_fm_dat_ad;
typedef struct{
	//unsigned char Prod_Nb[4];  //Ϊ����PR_ID��Ӧ,PCD�����ӵ�.û�е�ַ��.
	//CONFIGURATION:
	 //unsigned char Fuelling_Mode_Name[8]; //O//
	unsigned char Prod_Price[5];
	unsigned char Max_Vol[5];
	unsigned char Max_Fill_Time;
	unsigned char Max_Auth_Time;
	unsigned char User_Max_Volume[5];	
	}FP_FM_DATA;
*/
//FP_FM_DATA��������. node���ͻ��߼��ڵ��,pr_nb��Ʒ���(1-99999999),pr_ad��Ʒ��ַ41h-48h,
int DefaultFM_DAT(const char node, const unsigned char pr_ad) {
	int i, fp_state;
	int m=pr_ad-0x41; //0,1,2,...7
	//int er=er_ad-1; //0,1,2,3,4,5,6,..63�Ŵ���.
	//unsigned char tmpUChar;
	DISPENSER_DATA *dispenser_data;
	FP_FM_DATA *pfp_fm_data;
	
	if( (node <= 0) || (node > 127 ) ){
		UserLog("���ͻ��ڵ�ֵ����,����ģʽ��ʼ�� ʧ��");
		return -1;
	}
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,����ģʽ��ʼ�� ʧ��");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,����ģʽ��ʼ�� ʧ��");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	for(i=0; i < MAX_FP_CNT; i++){ 
		fp_state = dispenser_data->fp_data[i].FP_State;
		if( fp_state >9 ) return -1;		
	}
	if( fp_state >2 ) {
		UserLog("���͵�״̬����2,���ܳ�ʼ������.");
		return -1;
	}

	for (i = 0; i < FP_FM_CNT; ++i) {
		pfp_fm_data = (FP_FM_DATA *)(&(dispenser_data->fp_fm_data[m][i]));

		pfp_fm_data->Prod_Price[0] = '\06'; // for miandian, no point
		pfp_fm_data->Prod_Price[1] = '\0';
		pfp_fm_data->Prod_Price[2] = '\x01';
		pfp_fm_data->Prod_Price[3] = '\0';  //default RMB 1.00
			
		memset(pfp_fm_data->Max_Vol, 0, 5);
		pfp_fm_data->Max_Fill_Time = '\0';
		pfp_fm_data->Max_Auth_Time = '\0';
		memset(pfp_fm_data->User_Max_Volume, 0 , 5);
#if 0
		RunLog("DEBUG.Node %d PR %d FM %02x: Price: %02x%02x%02x%02x", node, m + 1, i + 1, 
			pfp_fm_data->Prod_Price[0], pfp_fm_data->Prod_Price[1],
			pfp_fm_data->Prod_Price[2], pfp_fm_data->Prod_Price[3]);
#endif
	}
	
	return 0;
}
int SetFM_DAT(const char node, const unsigned char pr_ad, const FP_FM_DATA *fp_fm_data){//��ʼ��,ע����FP_PR_DATA��Ӧ!!
	int i, fp_state;
	int m=pr_ad-0x41; //0,1,2,...7	
	DISPENSER_DATA *dispenser_data;
	FP_FM_DATA *pfp_fm_data;
	
	if( (node <= 0) || (node > 127 ) ){
		UserLog("���ͻ��ڵ�ֵ����,����ģʽSet  ʧ��");
		return -1;
		}
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,����ģʽSet  ʧ��");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���, ����ģʽSet  ʧ��");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	pfp_fm_data =  (FP_FM_DATA *)( &(dispenser_data->fp_fm_data[m][0])  ); //only one fuelling mode.
	for(i=0; i < MAX_FP_CNT; i++){ 
		fp_state = dispenser_data->fp_data[i].FP_State;
		if( fp_state >9 ) return -1;		
	}
	if( fp_state >2 ) {
		UserLog("���͵�״̬����2,����ģʽSet  ʧ��");
		return -1;
	}
	
	memcpy(pfp_fm_data, fp_fm_data, sizeof(FP_FM_DATA));
	
	return 0;
}
//��ȡFP_FM_DATA. FP_FM_DATA��ַΪ11h(11h-18h).�������Ϣ���ݵ�����Ϊ001�ش�
//recvBufferΪ�յ���data,sendBufferΪҪ���͵�data,msg_lgΪ���ɵ��ܳ���.
int ReadFM_DATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg){
	int i,k, fp_state, recv_lg, tmp_lg ;
	int m=(int )recvBuffer[14] - 0x11;//0,1,..7.
	DISPENSER_DATA *dispenser_data;
	FP_FM_DATA *pfp_fm_data;
	FP_PR_DATA *pfp_pr_data;
	unsigned char node = recvBuffer[1];
	unsigned char buffer[128];
	memset(buffer,0, 128);
	if( (NULL == recvBuffer)  || (NULL == sendBuffer) ){
		UserLog(" �����մ���,��ƷģʽRead ʧ��");
		return -1;
	}	
	if( (node <= 0) || (node > 127 ) ){
		UserLog("���ͻ��ڵ�ֵ����,��ƷģʽRead ʧ��");
		return -1;
	}
	if (m < 0 || m > 7) {
		UserLog("���ͻ���Ʒģʽ��ֵַ����,��ƷģʽRead ʧ��");
		return -1;
	}
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,��ƷģʽRead ʧ��");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,��ƷģʽRead ʧ��");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );	
	fp_state = 1;
	for(i=0; i < MAX_FP_CNT; i++){ 
		if(fp_state < dispenser_data->fp_data[i].FP_State)
			fp_state = dispenser_data->fp_data[i].FP_State;
		if( fp_state >9 ) 
			return -1;		
	}
	for(i=0; i<FP_PR_CNT; i++){ //search prod_nb address in pr_data.
		pfp_pr_data =  (FP_PR_DATA *)( &(dispenser_data->fp_pr_data[i]) );
		if (memcmp(pfp_pr_data->Prod_Nb, (unsigned char *)(&recvBuffer[10]), 4) == 0) {
		    	pfp_fm_data = (FP_FM_DATA *)(&(dispenser_data->fp_fm_data[i][(recvBuffer[14] - 0x11)]));
			break;
		}
	}
	if (i == FP_PR_CNT){
		UserLog("û���ҵ���Ʒ��.����read ��Ʒģʽ.");
		return -1;
	}

	buffer[0] = recvBuffer[2];
	buffer[1] = recvBuffer[3];
	buffer[2] = recvBuffer[0];
	buffer[3] = recvBuffer[1];
	buffer[4] = '\0'; //recvBuffer[4]; //IFSF_MC
	buffer[5] = (recvBuffer[5] |0x20); //0x20:answer.
	recv_lg = recvBuffer[6];
	recv_lg = recv_lg * 256 + recvBuffer[7];
	buffer[8] = '\x06'; //recvBuffer[8]; //Ad_lg
	buffer[9] = recvBuffer[9]; //Ad ,61h
	buffer[10] = recvBuffer[10];
	buffer[11] = recvBuffer[11];
	buffer[12] = recvBuffer[12];
	buffer[13] = recvBuffer[13];
	buffer[14] = recvBuffer[14];
	//buffer[10] = recvBuffer[10]; //It's Data_Id. Data begin.
	tmp_lg = 14; //ad_lg=1,then 9.
	for(i=0; i < (recv_lg - 7); i++){
		switch( recvBuffer[15+i] ){			
		case FM_Fuelling_Mode_Name:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FM_Fuelling_Mode_Name;
			tmp_lg++;
			buffer[tmp_lg] = '\0';			
			break;		
		case FM_Prod_Price:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FM_Prod_Price;
			tmp_lg++;
			buffer[tmp_lg] ='\x04';
			k=buffer[tmp_lg];
			for(i=0;i<k;i++){
				tmp_lg++;
				buffer[tmp_lg] = pfp_fm_data->Prod_Price[i];
			}
			break;
		case FM_Max_Vol:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FM_Max_Vol;
			tmp_lg++;
			buffer[tmp_lg] ='\x05';
			k=buffer[tmp_lg];
			for(i=0;i<k;i++){
				tmp_lg++;
				buffer[tmp_lg] = pfp_fm_data->Max_Vol[i];
			}
			break;
		case FM_Max_Fill_Time:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FM_Max_Fill_Time;
			tmp_lg++;
			buffer[tmp_lg] ='\x01';			
			tmp_lg++;
			buffer[tmp_lg] = pfp_fm_data->Max_Fill_Time;			
			break;
		case FM_Max_Auth_Time:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FM_Max_Auth_Time;
			tmp_lg++;
			buffer[tmp_lg] ='\x01';			
			tmp_lg++;
			buffer[tmp_lg] = pfp_fm_data->Max_Auth_Time;			
			break;
		case FM_User_Max_Volume:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FM_User_Max_Volume;
			tmp_lg++;
			buffer[tmp_lg] ='\x05';
			k=buffer[tmp_lg];
			for(i=0;i<k;i++){
				tmp_lg++;
				buffer[tmp_lg] = pfp_fm_data->User_Max_Volume[i];
			}
			break;
		default:
				tmp_lg++;
				buffer[tmp_lg] = recvBuffer[15+i];
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
//дFP_PR_DATA����.recvBufferΪ�յ���data,sendBufferΪҪ�ظ���data,�������Ϣ���ݵ�����Ϊ111Ӧ��
//msg_lgΪ���ɵ��ܳ���,Ϊ0��ʾ����ҪӦ����Ϣ,sendBuffer��msg_lg����ҪӦ����Ϣ��������ΪNULL.
int WriteFM_DATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg){
	int i,j, fp_state, recv_lg, tmp_lg,  lg ;
	int m=(int )recvBuffer[14] - 0x11;//0,1,..7.
	int pr_idx = 0;
	int fp_idx, lnz_idx;
	DISPENSER_DATA *dispenser_data;
	FP_FM_DATA *pfp_fm_data;
	FP_PR_DATA *pfp_pr_data;
	unsigned char node = recvBuffer[1];
	unsigned char tmpMSAck, tmpDataAck;
	int cmd_lg;
	unsigned char buffer[128],cmd[17];
	memset(buffer,0, sizeof(buffer));
	unsigned char unit_price[4];
	unsigned char chnlog;
	
	if( (NULL == recvBuffer)  || (NULL == sendBuffer) ){
		UserLog(" �����մ���,��ƷģʽWrite ʧ��");
		return -1;
	}
	if( (node <= 0) || (node > 127 ) ){
		UserLog("���ͻ��ڵ�ֵ����,��ƷģʽWrite ʧ��");
		return -1;
	}
	if (m < 0 || m > 7) {
		UserLog("���ͻ���Ʒģʽ��ֵַ����,��ƷģʽWrite ʧ��");
		return -1;
	}
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,��ƷģʽWrite ʧ��");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,��ƷģʽWrite ʧ��,node:[%d]", node);
		return -1;
	}


	chnlog = gpIfsf_shm->node_channel[i].chnLog;

	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	fp_state = 1;
	for(i=0; i < MAX_FP_CNT; i++){ 
		if(fp_state < dispenser_data->fp_data[i].FP_State)
			fp_state = dispenser_data->fp_data[i].FP_State;
		if( fp_state >9 ) 
			return -1;		
	}

	RunLog("PR_Nb:recvBuffer: %s", Asc2Hex((unsigned char *)&recvBuffer[10], 4));
	for(i=0; i<FP_PR_CNT; i++){ //search prod_nb address in pr_data.
		pfp_pr_data =  (FP_PR_DATA *)( &(dispenser_data->fp_pr_data[i]));
		RunLog("PR_Nb:pr_data[%d]: %s", i, Asc2Hex((unsigned char *)&(pfp_pr_data->Prod_Nb[0]), 4));
		if (memcmp((unsigned char *)(&(pfp_pr_data->Prod_Nb[0])),
				      	(unsigned char *)(&recvBuffer[10]), 4) == 0) {			
		    	pfp_fm_data = (FP_FM_DATA *)(&(dispenser_data->fp_fm_data[i][(recvBuffer[14] - 0x11)]));
			pr_idx = i;
			break;
		}
	}	

	buffer[0] = recvBuffer[2];
	buffer[1] = recvBuffer[3];
	buffer[2] = recvBuffer[0];
	buffer[3] = recvBuffer[1];
	buffer[4] = '\0'; //recvBuffer[4]; //IFSF_MC
	buffer[5] = ( (recvBuffer[5] &0x1f)  |0xe0); //0x20:answer, 0xE0:ack.
	recv_lg = recvBuffer[6];
	recv_lg = recv_lg * 256 + recvBuffer[7];
	buffer[8] = '\x06'; //recvBuffer[8]; //Ad_lg
	buffer[9] = recvBuffer[9]; //Ad ,61h
	buffer[10] = recvBuffer[10];
	buffer[11] = recvBuffer[11];
	buffer[12] = recvBuffer[12];
	buffer[13] = recvBuffer[13];
	buffer[14] = recvBuffer[14];
	//buffer[15] = ; //MS_Ack
	//buffer[16] = recvBuffer[15]; //recvBuffer[15],It's Data_Id. Data begin.
	tmp_lg = 15; //ad_lg=1,then 9.

	// ��¼��ǰ��Ʒ�����Լ���ǹ״̬�� CRITICAL_LOG
	price_change_track();

	if (i == FP_PR_CNT){
		CriticalLog("�ڵ�[%02d]�յ��������[%s]--��Ʒ��[%02x%02x%02x%02x]ģʽ[%02x]",
				node, Asc2Hex(recvBuffer, recv_lg + 8), recvBuffer[10],
				recvBuffer[11], recvBuffer[12], recvBuffer[13], recvBuffer[14]);

		UserLog("�ڵ�[%02d]��û���ҵ��ú���Ʒ,�޷��޸ĸ���Ʒ�����Ϣ", node);
		CriticalLog("�ڵ�[%02d]��û���ҵ��ú���Ʒ,�޷��޸ĸ���Ʒ�����Ϣ", node);
		PriceChangeErrLog("%02x.**.** xx.xx 35", node);

		//tmpMSAck = 6; //MS_Ack, data address not exist.
		tmpMSAck = 0; //MS_Ack, data address not exist.
		tmp_lg++;
		buffer[6] = (unsigned char )((tmp_lg-8) / 256) ;
		buffer[7] = (unsigned char )((tmp_lg-8) % 256);  //big end.
		buffer[15] = tmpMSAck;  
		if(NULL != msg_lg) *msg_lg= tmp_lg;
		if(NULL != sendBuffer)  memcpy(sendBuffer, buffer, tmp_lg);

#if 0
		if (SendData2CD(sendBuffer, tmp_lg, 5) < 0) {
			RunLog("���ͻظ�����[%s]ʧ��", Asc2Hex(sendBuffer, tmp_lg));
		}
#endif

		// Error Code �޴���Ʒ����
		ChangeER_DATA(node, 0x21, 0x35, NULL);

		return 0;
	}

	i=0;
	tmpMSAck = '\0'; // all is  ok.	
	while( i < (recv_lg -7) ) {
		tmpDataAck = '\0'; //ok ack.
		lg =  (int )recvBuffer[16+i]; //Data_Lg.
		switch( recvBuffer[15+i] ){			
		case FM_Fuelling_Mode_Name: //O
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FM_Fuelling_Mode_Name;
			tmpDataAck = '\x04';  
			tmpMSAck = '\x05';
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;			
			break;		
		case FM_Prod_Price:			
			tmp_lg++;			
			buffer[tmp_lg] = (unsigned char)FM_Prod_Price;				
			// ��FP��ǰ״̬ >= 3, ��ô���ͻ�״̬��Ϊ < 3��ʱ�����·����������ͻ�
			if ((lg == 4) && (recvBuffer[14] >= 0x11) && (recvBuffer[14] <= 0x18) &&
							  (((recvBuffer[17+i]&0xF0) >>4) <= 9) &&
							  ((recvBuffer[17+i]&0x0F) <= 9) &&
							  (((recvBuffer[18+i]&0xF0) >>4) <= 9) &&
							  ((recvBuffer[18+i]&0x0F) <= 9) &&
							  (((recvBuffer[19+i]&0xF0) >>4) <= 9) &&
							  ((recvBuffer[19+i]&0x0F) <= 9) &&
							  (((recvBuffer[20+i]&0xF0) >>4) <= 9) &&
							  ((recvBuffer[20+i]&0x0F) <= 9)) {

				memcpy(unit_price, &recvBuffer[17], 4);
				//TwoDecPoint(unit_price, 4); // delete this for miandian

				// �������ݿ��и�ģʽ�ĵ���
				memcpy(pfp_fm_data->Prod_Price, unit_price, 4);


				unsigned char flag_used = 0;
				for (fp_idx = 0; fp_idx < dispenser_data->fp_c_data.Nb_FP; ++fp_idx) {
					for (lnz_idx = 0; lnz_idx < MAX_FP_LN_CNT; ++lnz_idx) {
						if (dispenser_data->fp_ln_data[fp_idx][lnz_idx].PR_Id == pr_idx + 1) {
							// ����Ʒ����ʹ��
							RunLog("---Node %d FP %d LNZ %d use PR %d",
									node, fp_idx + 1, lnz_idx + 1, pr_idx + 1);
							flag_used = 0xFF;
							break;
						}
					}
				}

				if (flag_used != 0xFF) {
					// ����Ʒ�ڴ˽ڵ�û��ʵ��ʹ��, ��ֱ�ӻظ��ɹ�(Error Code 0x99)
					// ������Ҫ�ѱ��ָ��͸��ͻ��������;
					for (fp_idx = 0; fp_idx < dispenser_data->fp_c_data.Nb_FP; ++fp_idx) {
						ChangeER_DATA(node, 0x21 + fp_idx, 0x99, NULL);
					}
				} else {

				// ---------------------------------------------------------------------
#if 0
				// commented by guojie Zhu @ 2009.11.04, ֮ǰ�Ѿ��жϹ�, �����ٴ��ж�
				for (j = 0; j < FP_PR_CNT; j++) {
					if(memcmp(dispenser_data->fp_pr_data[j].Prod_Nb, &recvBuffer[10], 4) == 0) {
						break;
					}
				}

				if (j == FP_PR_CNT){
					tmpDataAck = '\x01';
					tmpMSAck = '\x05'; 
					tmp_lg++;
					buffer[tmp_lg] = tmpDataAck;
					i = i + 2 + lg;
					break;
				}
#endif
					cmd[0] = '\x18';
					cmd[1] = '\0';
					cmd[2] = pr_idx + 0x41;              // pr_ad
					memcpy(&cmd[3], &recvBuffer[10], 4); // Prod_Nb
					cmd[7] = recvBuffer[14];             // FM_ID
					memcpy(&cmd[8], unit_price, 4);

			CriticalLog("�ڵ�[%02d]�յ��������[%s]--��Ʒ��[%02x%02x%02x%02x]ģʽ[%02x]Ŀ�굥��[%02x%02x]", // delete point for miandian
						node, Asc2Hex(recvBuffer, recv_lg + 8), recvBuffer[10],
						recvBuffer[11], recvBuffer[12], recvBuffer[13],
						recvBuffer[14], unit_price[2], unit_price[3]);


					cmd_lg = 8 + lg;
					j = SendMsg(ORD_MSG_TRAN, cmd, cmd_lg, chnlog, 3);
					if (j < 0) {
						RunLog("ת���������[%s]��ͨ��[%d]ʧ��!", Asc2Hex(cmd, cmd_lg), chnlog);
						return -1;
					}

					// ������Ʒ�����ļ��еĵ���, ͳһʹ�� SyncCfg2File �������ݵ������ļ���
					for (j = 0; j < gpIfsf_shm->oilCnt; ++j) {
						RunLog("oil_config[%d].oilno:%s",
							       	j, Asc2Hex(gpIfsf_shm->oil_config[j].oilno, 4));
						if (memcmp(&recvBuffer[10], gpIfsf_shm->oil_config[j].oilno, 4) == 0) {
							RunLog("-- oil_config[%d].oilno:%s", j,
									Asc2Hex(gpIfsf_shm->oil_config[j].oilno, 4));
							memcpy(gpIfsf_shm->oil_config[j].price, &unit_price[1], 3);
							break;
						}
					}

					if (j >= gpIfsf_shm->oilCnt) {
						RunLog("ԭʼ������δ�ҵ�����Ʒ(%s)��������Ϣ", 
								Asc2Hex((unsigned char *)&recvBuffer[10], 4));
					}

					gpIfsf_shm->cfg_changed = 0x0F;
	//				RunLog("-----cfg_changed: %02x ", gpIfsf_shm->cfg_changed);
				}
			} else  { 
				tmpDataAck = '\x01';
				tmpMSAck = '\x05';
				//UserLog("write  C_DAT's Nb_Products error!too big or small.") 
			}			
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case FM_Max_Vol:
			tmp_lg++;			
			buffer[tmp_lg] = (unsigned char)FM_Max_Vol;
			/*
			if( ( lg == 7 ) && (fp_state < 3)    ) 
				for(j=0; j<lg; j++)
					pfp_fm_data->Max_Vol[j] = recvBuffer[18+i+j];
			else  if (fp_state < 3)  { 
				tmpDataAck = '\x02';
				tmpMSAck = '\x05'; 
				//UserLog("write  FM_DAT error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';
				tmpMSAck = '\x05';
				//UserLog("write  FM_DAT error!too big or small.") 
				}
			*/
			tmpDataAck = '\x02';
			tmpMSAck = '\x05'; 
			
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case FM_Max_Fill_Time:
			tmp_lg++;			
			buffer[tmp_lg] = (unsigned char)FM_Max_Fill_Time;	
			/*
			if( (  lg== 1 ) && (fp_state < 3)    ) 				
					pfp_fm_data->Max_Fill_Time = recvBuffer[18+i];
			else  if (fp_state < 3)  { 
				tmpDataAck = '\x02';
				tmpMSAck = '\x05'; 
				//UserLog("write  FM_DAT error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';
				tmpMSAck = '\x05';
				//UserLog("write  FM_DAT error!too big or small.") 
				}
			*/
			tmpDataAck = '\x02';
			tmpMSAck = '\x05'; 
			
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;	
			break;
		case FM_Max_Auth_Time:
			tmp_lg++;			
			buffer[tmp_lg] = (unsigned char)FM_Max_Auth_Time;
			/*
			if( (  lg== 1 ) && (fp_state < 3)    ) 				
					pfp_fm_data->Max_Auth_Time = recvBuffer[18+i];
			else  if (fp_state < 3)  { 
				tmpDataAck = '\x02';
				tmpMSAck = '\x05'; 
				//UserLog("write  FM_DAT error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';
				tmpMSAck = '\x05';
				//UserLog("write  FM_DAT error!too big or small.") 
				}
			*/
			tmpDataAck = '\x02';
			tmpMSAck = '\x05'; 
			
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;	
			break;
		case FM_User_Max_Volume:
			tmp_lg++;			
			buffer[tmp_lg] = (unsigned char)FM_User_Max_Volume;
			/*
			if( ( lg == 7 ) && (fp_state < 3)    ) 
				for(j=0; j<lg; j++)
					pfp_fm_data->User_Max_Volume[j] = recvBuffer[18+i+j];
			else  if (fp_state < 3)  { 
				tmpDataAck = '\x02';
				tmpMSAck = '\x05'; 
				//UserLog("write  FM_DAT error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';
				tmpMSAck = '\x05';
				//UserLog("write  FM_DAT error!too big or small.") 
				}
			*/
			tmpDataAck = '\x02';
			tmpMSAck = '\x05'; 
			
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;			
			break;
		default:
			tmp_lg++;
			buffer[tmp_lg] =recvBuffer[15+ i]  ;
			tmpDataAck = '\x04';  
			tmpMSAck = '\x05';
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;	
		}	
	}
	if( (i + 7) != recv_lg )	{
		//UserLog("write FM_Dat error!! exit. ");
		UserLog("i  != recv_lg-7, write FM_Dat error!! exit, recv_lg[%d],i [%d] ",recv_lg, i);
		//exit(0);
		return -2;
	}
	tmp_lg++;
	buffer[6] = (unsigned char )((tmp_lg-8) / 256) ;
	buffer[7] = (unsigned char )((tmp_lg-8) %256);  //big end.
	buffer[15] = tmpMSAck;  
	if(NULL != msg_lg) *msg_lg= tmp_lg;
	if(NULL != sendBuffer)  memcpy(sendBuffer, buffer, tmp_lg);

	return 0;	
}

int SetSglFM_DAT(const char node, const unsigned char pr_ad, const unsigned char  *data){
	int i,j, fp_state,  lg ;
	int m=(int )pr_ad - 0x11;//0,1,..7.
	DISPENSER_DATA *dispenser_data;
	FP_FM_DATA *pfp_fm_data;
	FP_PR_DATA *pfp_pr_data;
	//unsigned char node = recvBuffer[2];
	unsigned char tmpDataAck; //tmpMSAck
	//unsigned char buffer[128];
	//memset(buffer,0, 128);
	if( NULL == data)  {
		UserLog(" �����մ���,��ƷģʽSetSgl ʧ��");
		return -1;
		}
	if( (node <= 0) || (node > 127 ) ){
		UserLog("���ͻ��ڵ�ֵ����,��ƷģʽSetSgl ʧ��");
		return -1;
	}
	if (m < 0 || m > 7) {
		UserLog("���ͻ���Ʒģʽ��ֵַ����,��ƷģʽSetSgl ʧ��");
		return -1;
	}
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,��ƷģʽSetSgl ʧ��");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,��ƷģʽSetSgl ʧ��");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );	
	for(i=0; i < MAX_FP_CNT; i++){ 
		fp_state = dispenser_data->fp_data[i].FP_State;
		if( fp_state >9 ) return -1;		
	}
	
	pfp_pr_data =  (FP_PR_DATA *)( &(dispenser_data->fp_pr_data[m]) );	
	pfp_fm_data =  (FP_FM_DATA *)( &(dispenser_data->fp_fm_data[m][0]) );
 	
	
	
	tmpDataAck = '\0'; //ok ack.
	lg =  (int )data[1]; //Data_Lg.
	switch( data[0] ){			
		case FM_Fuelling_Mode_Name:			
			tmpDataAck = '\x04'; 				
			break;		
		case FM_Prod_Price: //w(1-9)				
			if( ( lg== 4 ) && (fp_state < 10)    ) 
				for(j=0; j<lg; j++)
					pfp_fm_data->Prod_Price[j] = data[2+j];
			else  if (fp_state >= 10)  { 
				tmpDataAck = '\x02';
				//UserLog("write  C_DAT's Nb_Products error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';
				//UserLog("write  C_DAT's Nb_Products error!too big or small.") 
				}
			break;
		case FM_Max_Vol://w(1-9)
			if( ( lg == 5 ) && (fp_state < 10)    ) 
				for(j=0; j<lg; j++)
					pfp_fm_data->Max_Vol[j] = data[2+j];
			else  if (fp_state >= 10)  { 
				tmpDataAck = '\x02';				
				//UserLog("write  FM_DAT error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';
				//UserLog("write  FM_DAT error!too big or small.") 
				}			
			break;
		case FM_Max_Fill_Time://w(1-9)
			if( (  lg== 1 ) && (fp_state < 10)    ) 				
					pfp_fm_data->Max_Fill_Time = data[2];
			else  if (fp_state >= 10)  { 
				tmpDataAck = '\x02';
				//UserLog("write  FM_DAT error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';
				//UserLog("write  FM_DAT error!too big or small.") 
				}			
			break;
		case FM_Max_Auth_Time: //w(1-9)
			if( (  lg== 1 ) && (fp_state < 10)    ) 				
					pfp_fm_data->Max_Auth_Time =data[2] ;
			else  if (fp_state >= 10)  { 
				tmpDataAck = '\x02';
				//UserLog("write  FM_DAT error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';
				//UserLog("write  FM_DAT error!too big or small.") 
				}			
			break;
		case FM_User_Max_Volume: //w(1-9)
			if( ( lg == 5 ) && (fp_state < 10)    ) 
				for(j=0; j<lg; j++)
					pfp_fm_data->User_Max_Volume[j] = data[2+j];
			else  if (fp_state >= 10)  { 
				tmpDataAck = '\x02';
				//UserLog("write  FM_DAT error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';
				//UserLog("write  FM_DAT error!too big or small.") 
				}	
			break;
		default:			
			tmpDataAck = '\x04';  
			break;	
	}	

	return tmpDataAck;	
}
int SetFM_DAT_Price(const char node, const unsigned char pr_ad, const unsigned char  *price){
	int i,j, fp_state,  lg, cmd_lg;
	int m=(int )pr_ad - 0x11;//0,1,..7.
	DISPENSER_DATA *dispenser_data;
	FP_FM_DATA *pfp_fm_data;
	FP_PR_DATA *pfp_pr_data;
	//unsigned char node = recvBuffer[2];
	unsigned char tmpDataAck; //tmpMSAck
	unsigned char cmd[17];
	unsigned char chnlog;
	memset(cmd,0, 17);
	//unsigned char buffer[128];
	//memset(buffer,0, 128);
	if( NULL == price)  {
		UserLog(" ����price�մ���,��ƷģʽSet Price ʧ��");
		return -1;
		}
	if( (node <= 0) || (node > 127 ) ){
		UserLog("���ͻ��ڵ�ֵ����,��ƷģʽSet Price ʧ��");
		return -1;
	}
	if (m < 0 || m > 7) {
		UserLog("���ͻ���Ʒģʽ��ֵַ����,��ƷģʽSet Price  ʧ��");
		return -1;
	}
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,��ƷģʽSet Price  ʧ��");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,��ƷģʽSet Price  ʧ��");
		return -1;
	}

	chnlog = gpIfsf_shm->node_channel[i].chnLog;
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );	
	for(i=0; i < MAX_FP_CNT; i++){ 
		fp_state = dispenser_data->fp_data[i].FP_State;
		if( fp_state >9 ) return -1;		
	}
	
	pfp_pr_data =  (FP_PR_DATA *)( &(dispenser_data->fp_pr_data[m]) );	
	pfp_fm_data =  (FP_FM_DATA *)( &(dispenser_data->fp_fm_data[m][0])  );
 	
		
	tmpDataAck = '\0'; //ok ack.
	lg=4	;				
	if(  fp_state < 10   ) 
		for(j=0; j<lg; j++){
			pfp_fm_data->Prod_Price[j] = price[j];
			cmd[0] = '\x18';
			cmd[1] = '\0';
			cmd[2] = '\x01';
			for(j=0; j<4; j++)
				cmd[3+j] = pfp_pr_data->Prod_Nb[j];
			for(j=0; j<lg; j++)
				cmd[7+j] = price[1+j];
			cmd_lg=8+lg;
			j = SendMsg(ORD_MSG_TRAN, cmd, cmd_lg, chnlog, 3);
			if(j<0 ){
				UserLog("�·��ͼ۴���!!, ͨ����: %d", chnlog);
				exit(0);
			}
	} else  if (fp_state >= 10)  { 
		tmpDataAck = '\x02';				
		//UserLog("write  C_DAT's Nb_Products error!fp state not in 1,2. ") 
	} else  { 
		tmpDataAck = '\x01';
		//UserLog("write  C_DAT's Nb_Products error!too big or small.") 
	}			
				

	return tmpDataAck;	
}
int RequestFM_DAT(const char node, const unsigned char pr_ad){ //fm is only 1,ad is 11h.
	int i,k, fp_state, tmp_lg ;
	int m=(int )pr_ad- 0x11;//0,1,..7.
	DISPENSER_DATA *dispenser_data;
	FP_FM_DATA *pfp_fm_data;
	FP_PR_DATA *pfp_pr_data;
	//unsigned char node = recvBuffer[2];
	unsigned char buffer[64];
	memset(buffer,0, 64);
	
	if( (node <= 0) || (node > 127 ) ){
		UserLog("���ͻ��ڵ�ֵ����,��ƷģʽRequest ʧ��");
		return -1;
	}
	if (m < 0 || m > 7) {
		UserLog("���ͻ���Ʒģʽ��ֵַ����,��ƷģʽRequest ʧ��");
		return -1;
	}
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,��ƷģʽRequest ʧ��");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,��ƷģʽRequest ʧ��");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );	
	for(i=0; i < MAX_FP_CNT; i++){ 
		fp_state = dispenser_data->fp_data[i].FP_State;
		if( fp_state >9 ) return -1;		
	}
	
	pfp_pr_data =  (FP_PR_DATA *)( &(dispenser_data->fp_pr_data[m]) );		
	pfp_fm_data =  (FP_FM_DATA *)( &(dispenser_data->fp_fm_data[m][0]) );
	

	buffer[0] = '\x02';
	buffer[1] = '\x01';;
	buffer[2] = '\x01';;
	buffer[3] = (unsigned char)node;
	buffer[4] = '\0'; //recvBuffer[4]; //IFSF_MC
	buffer[5] = '\0'; //0x00:read.
	//recv_lg = recvBuffer[6];
	//recv_lg = recv_lg * 256 + recvBuffer[7];
	buffer[8] = '\x06'; //recvBuffer[8]; //Ad_lg
	buffer[9] = '\x61'; //Ad ,61h
	buffer[10] = pfp_pr_data->Prod_Nb[0];
	buffer[11] = pfp_pr_data->Prod_Nb[1];
	buffer[12] = pfp_pr_data->Prod_Nb[2];
	buffer[13] = pfp_pr_data->Prod_Nb[3];
	buffer[14] = '\x11';//fm_ad,11h,only 1 in this design.
	//buffer[15] = recvBuffer[15]; //It's Data_Id. Data begin.
	tmp_lg = 14; //ad_lg=1,then 9.
	
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)FM_Prod_Price;
	/*
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)FM_Max_Vol;			
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)FM_Max_Fill_Time;
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)FM_Max_Auth_Time;	
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)FM_User_Max_Volume;
	*/
	tmp_lg++;
	buffer[6] = (unsigned char )(tmp_lg / 256) ;
	buffer[7] = (unsigned char )(tmp_lg %256);  //big end.
	//i use to be ret.
	i= SendData2CD(buffer, tmp_lg, 5);
	if (i <0 ){
		UserLog("���ͻ�ͨѶ����,����������Request ");
		return -1;
	}
	return 0;
}

/*
 * @brief   ������Ʒ��Prod_Nb�жϸ���Ʒ��ǰ�Ƿ�����
 * @param   prod_nb   ��Ҫ������Ʒ��
 * @return  1 - ����, 0 - ������
 */
static int IsProdInUse(const __u8 *prod_nb)
{
	int i, pr_idx;
	int is_use = 0;
	FP_PR_DATA *pfp_tr_dat = NULL;

	for (i = 0; i < MAX_CHANNEL_CNT; ++i) {
		if (gpIfsf_shm->node_channel[i].node == 0) {
			continue;
		}

		for (pr_idx = 0; pr_idx < FP_PR_CNT; ++pr_idx) {
			if (memcmp(prod_nb, gpIfsf_shm->dispenser_data[i].fp_pr_data[pr_idx].Prod_Nb, 4) == 0) {
				is_use = 1;
				return is_use;
			}
		}
	}

	return is_use;
}

/*
 *@brief    �����Ʒ���õ�FCCȫ����Ʒ�����б���
 *@param    prod_nb            ��Ʒ��
 *@param    prod_description   ��Ʒ����
 *@return   0 - �ɹ�, -1 - ʧ��
 */
static int AddProdToGlobalConfigList(const __u8 *prod_nb, const __u8 *prod_description)
{
	int i;
	int empty_idx = 0;

	// 1. �Ƿ��Ѵ���
	for (i = 0; i < gpIfsf_shm->oilCnt; ++i) {
		RunLog("oil_config[%02d].oilno:%s", i, Asc2Hex(gpIfsf_shm->oil_config[i].oilno, 4));
		if (memcmp(prod_nb, gpIfsf_shm->oil_config[i].oilno, 4) == 0) {
			RunLog("��Ʒ(%s)��������Ϣ�Ѵ����������б���", Asc2Hex(prod_nb, 4));
			return 0;
		}
	}

	// 2. ��Ʒ���ñ��в����ڸ���Ʒ����, �����
	if (gpIfsf_shm->oilCnt >= MAX_OIL_CNT) {
		// ���ҿ������ÿռ�
		for (i = 0; i < gpIfsf_shm->oilCnt; ++i) {
			if (IsProdInUse(gpIfsf_shm->oil_config[i].oilno) == 0) {
				empty_idx = i;
				break;
			}
		}
		if (i >= gpIfsf_shm->oilCnt) {
			RunLog("�����Ʒ[%s]ʧ��, FCCȫ�������б�����", Asc2Hex(prod_nb, 4));
			return -1;
		}
	} else {
		empty_idx = gpIfsf_shm->oilCnt;
	}

	memset(&(gpIfsf_shm->oil_config[empty_idx]), 0, sizeof(OIL_CONFIG));
	memcpy(gpIfsf_shm->oil_config[empty_idx].oilno, prod_nb, 4);
	if (prod_description != NULL) {
		memcpy(gpIfsf_shm->oil_config[empty_idx].oilname, prod_description, 16);
	} else {
		snprintf(gpIfsf_shm->oil_config[empty_idx].oilname, 8 + 1, "%s", Asc2Hex(prod_nb, 4));
	}
	gpIfsf_shm->oil_config[empty_idx].price[0] = 0x00;
	gpIfsf_shm->oil_config[empty_idx].price[1] = 0x01;
	gpIfsf_shm->oil_config[empty_idx].price[2] = 0x00;

	// 3. ������Ʒ��������
	gpIfsf_shm->oilCnt = (gpIfsf_shm->oilCnt >= MAX_OIL_CNT ? MAX_OIL_CNT : (++gpIfsf_shm->oilCnt));

	RunLog("������Ʒ(%s)��������Ϣ,����Ʒ��[%d]", Asc2Hex(prod_nb, 4), gpIfsf_shm->oilCnt);
	RunLog("oil_config[%02d].oilno:%s", empty_idx, Asc2Hex(gpIfsf_shm->oil_config[empty_idx].oilno, 4));

	return 0;
}


/*-----------------------------------------------------------------*/




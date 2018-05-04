/*******************************************************************
ifsf_fp.c,���������м��ͻ������ݽṹ�Ͳ�������.
-------------------------------------------------------------------
2007-07-16 by chenfm
2008-03-05 - Guojie Zhu <zhugj@css-intelligent.com>
    ����SendDataBySock�滻ΪSendData2CD
    ɾ�� AddDispenser �е�gpIfsf_shm->convert_hb[i].lnao.node = node; 
2008-03-06 - Guojie Zhu <zhugj@css-intelligent.com>
    convertUsed[]��Ϊconvert_hb[].lnao.node

*******************************************************************/

#include "ifsf_pub.h"
#include "ifsf_main.h"
#include "ifsf_dspcal.c"
#include "ifsf_dspmeter.c"
#include "ifsf_dsptr.c"
#include "ifsf_dspfp.c"
#include "ifsf_dsplnz.c"
#include "ifsf_dsper.c"
#include "ifsf_dspprod.c"


extern IFSF_SHM* gpIfsf_shm; //�ýṹ��ȫ�ֵ�����ֻ��һ��,��ifsf_main.c�ж���.


/*---------------------------------------------------------------*/
//����Ϊ��.c�ļ�Ҫ�õ��ĺ���

static int TransRecvDspAdErr(const int newsock, const unsigned char *recvBuffer, int MS_ACK) {
	int i, ret, ad1, m_st, msg_lg;
	//DISPENSER_DATA *dispenser_data;		
	unsigned char node= recvBuffer[1];
	//unsigned char ad_char2[2], ad_char4[4];
	unsigned char buffer[128];
	
	memset(buffer, 0 , sizeof(buffer));	
	if ((NULL == recvBuffer) || (0 == newsock) ) {		
		UserLog("��������,Translate  ʧ��");
		return -1;
	}	
	m_st = ((int)recvBuffer[5]&0xe0) >>5;
	//ad_lg = recvBuffer[8];
	//ad1 = recvBuffer[9];
	
	if (node <= 0) {
		UserLog("���ͻ��ڵ�ֵ����,Translate   ʧ��");
		return -1;
		}
	if (NULL == gpIfsf_shm) {		
		UserLog("�����ڴ�ֵ����,Translate   ʧ��");
		return -1;
	}
	buffer[0] = recvBuffer[2];
	buffer[1] = recvBuffer[3];
	buffer[2] = recvBuffer[0];
	buffer[3] = recvBuffer[1];
	buffer[4] = '\0'; //IFSF_MC;
	buffer[5] = (recvBuffer[5] |0xE0); //ACK
	buffer[6] = '\0'; //data_lg;
	buffer[7] =2 + recvBuffer[8]; //data_lg
	//recv_lg = recvBuffer[6];
	//recv_lg = recv_lg * 256 + recvBuffer[7];
	buffer[8] = recvBuffer[8]; // Ad_lg   
	for (i=0; i< recvBuffer[8]; i++) {
		buffer[9+i] = recvBuffer[9+i];  //Ad  
	}
	i = recvBuffer[8];//i use as a temp value.
	buffer[9+ i] = MS_ACK;  
	//buffer[10] = recvBuffer[10]; //It's Data_Id. Data begin.
	msg_lg = 10 + recvBuffer[8];
	
	
	ret = SendData2CD(buffer, msg_lg, 5);
	if (ret <0) {		
		UserLog("���ͻظ����ݴ���,Translate   ʧ��");
		return -1;
	}
	
	return 0;
}

static int TransRecvSV_DATA(const int newsock, const unsigned char *recvBuffer) {
	int i, ret, ad1, m_st, msg_lg;
	//DISPENSER_DATA *dispenser_data;		
	unsigned char node= recvBuffer[1];
	//unsigned char ad_char2[2], ad_char4[4];
	unsigned char buffer[128];
	memset(buffer, 0 , 128);
	if ((NULL == recvBuffer) || (0 == newsock) ) {		
		UserLog("��������,Translate  ʧ��");
		return -1;
	}	
	m_st = ((int)recvBuffer[5]&0xe0) >>5;
	//ad_lg = recvBuffer[8];
	//ad1 = recvBuffer[9];
	
	if ((node <= 0) || (node > 127)) {
		UserLog("���ͻ��ڵ�ֵ����,Translate   ʧ��");
		return -1;
		}
	if (NULL == gpIfsf_shm) {		
		UserLog("�����ڴ�ֵ����,Translate   ʧ��");
		return -1;
	}
	
	ret = -1;
	switch(m_st) {
		case 0:
			ret = ReadSV_DATA(recvBuffer, buffer,&msg_lg);
			break;
		case 1:
			break;
		case 2:
			ret = WriteSV_DATA(recvBuffer, buffer, &msg_lg);
			break;
		case 3:
		case 4:
			break;
		case 7:
			break;
		default :
			UserLog("�յ������ݵ���Ϣ���ʹ���!!");
			return -1;
			break;
	}
	if(ret == 0) {
		ret = SendData2CD(buffer, msg_lg, 5);
		if (ret <0) {		
			UserLog("���ͻظ����ݴ���,Translate   ʧ��");
			return -1;
		}
		return 0;
	}
	return -1;
}
static int TransRecvC_DATA(const int newsock, const unsigned char *recvBuffer) {
	int i, ret, ad1, m_st, msg_lg;
	//DISPENSER_DATA *dispenser_data;		
	unsigned char node= recvBuffer[1];
	//unsigned char ad_char2[2], ad_char4[4];
	unsigned char buffer[256];
	memset(buffer, 0 , 256);
	
	if ((NULL == recvBuffer) || (0 == newsock) ) {		
		UserLog("��������,Translate  ʧ��");
		return -1;
	}	
	m_st = ((int)recvBuffer[5]&0xe0) >>5;
	//ad_lg = recvBuffer[8];
	//ad1 = recvBuffer[9];
	
	if (node == 0) {
		UserLog("���ͻ��ڵ�ֵ����,Translate   ʧ��");
		return -1;
		}
	if (NULL == gpIfsf_shm ) {		
		UserLog("�����ڴ�ֵ����,Translate   ʧ��");
		return -1;
	}
	
	ret = -1;
	switch(m_st) {
		case 0:
			ret = ReadC_DATA(recvBuffer, buffer,&msg_lg);
			break;
		case 1:
			break;
		case 2:
			ret = WriteC_DATA(recvBuffer, buffer, &msg_lg);
			break;
		case 3:
		case 4:
			break;
		case 7:
			break;
		default :
			UserLog("�յ������ݵ���Ϣ���ʹ���!!");
			return -1;
			break;
	}
	if(ret == 0) {
		ret = SendData2CD(buffer, msg_lg, 5);
		if (ret <0) {		
			UserLog("���ͻظ����ݴ���,Translate   ʧ��");
			return -1;
		}
		return 0;
	}
	return -1;
}

static int TransRecvFP_DATA(const int newsock, const unsigned char *recvBuffer) {
	int i, ret,  m_st, msg_lg;//ad1, 
	unsigned char node = recvBuffer[1];
	unsigned char fp_pos = recvBuffer[9] - 0x21;
	unsigned char buffer[256];

	memset(buffer, 0 , sizeof(buffer));
//	UserLog("TransRecvFP_DATA");
	if ((NULL == recvBuffer) || (0 == newsock) ) {		
		UserLog("��������,Translate  ʧ��");
		return -1;
	}	
	m_st = ((int)recvBuffer[5] & 0xe0) >> 5;
	//ad_lg = recvBuffer[8];
	//ad1 = recvBuffer[9];
	
	if (node <= 0 ) {
		UserLog("���ͻ��ڵ�ֵ����,Translate   ʧ��");
		return -1;
	}
	if (NULL == gpIfsf_shm) {		
		UserLog("�����ڴ�ֵ����,Translate   ʧ��");
		return -1;
	}
	
	ret = -1;
	switch(m_st) {
		case 0:
			ret = ReadFP_DATA(recvBuffer, buffer,&msg_lg);
			break;
		case 1:
			break; 
		case 2:
			ret = WriteFP_DATA(recvBuffer, buffer, &msg_lg);
			break;
		case 3:
		case 4:
			break;
		case 7:
			break;
		default :
			UserLog("�յ������ݵ���Ϣ���ʹ���!!");
			return -1;
			break;
	}
	if(ret == 0) {
		ret = SendData2CD(buffer, msg_lg, 5);
		if (ret <0) {		
			UserLog("���ͻظ����ݴ���,Translate   ʧ��");
			return -1;
		}
		return 0;
	} else if (ret == 1) {
		return 0;
	}

	return -1;
}


static int TransRecvLN_DATA(const int newsock, const unsigned char *recvBuffer) {
	int i, ret, ad1, m_st, msg_lg;
	//DISPENSER_DATA *dispenser_data;		
	unsigned char node= recvBuffer[1];
	//unsigned char ad_char2[2], ad_char4[4];
	unsigned char buffer[256];
	memset(buffer, 0 , 256);
	
	if ((NULL == recvBuffer) || (0 == newsock) ) {		
		UserLog("��������,Translate  ʧ��");
		return -1;
	}	
	m_st = ((int)recvBuffer[5]&0xe0) >>5;
	//ad_lg = recvBuffer[8];
	//ad1 = recvBuffer[9];
	 
	if ((node <= 0) || (node > 127)) {
		UserLog("���ͻ��ڵ�ֵ����,Translate   ʧ��");
		return -1;
		}
	if (NULL == gpIfsf_shm) {		
		UserLog("�����ڴ�ֵ����,Translate   ʧ��");
		return -1;
	}
	
	ret = -1;
	switch(m_st) {
		case 0:
			ret = ReadLN_DATA(recvBuffer, buffer,&msg_lg);
			break;
		case 1:
			break;
		case 2:
			ret = WriteLN_DATA(recvBuffer, buffer, &msg_lg);
			break;
		case 3:
		case 4:
			break;
		case 7:
			break;
		default :
			UserLog("�յ������ݵ���Ϣ���ʹ���!!");
			return -1;
			break;
	}
	if(ret == 99){
		int fp=recvBuffer[9]-0x21; //0,1,2,3�ż��͵�.
		int ln=recvBuffer[10]-0x11; //0,1,2,3,4,5,6,7���߼���ǹ.	
		RunLog("Node[%d]FP[%d]LN[%d]����Ϊ0�����ϴ�!",node, fp+1, ln+1);
		return 0;
	}
	if(ret == 0) {
		ret = SendData2CD(buffer, msg_lg, 5);
		if (ret <0) {		
			UserLog("���ͻظ����ݴ���,Translate   ʧ��");
			return -1;
		}
		return 0;
	}
	return -1;
}

static int TransRecvTR_DATA(const int newsock, const unsigned char *recvBuffer) {
	int i, ret, ad1, m_st, msg_lg;
	//DISPENSER_DATA *dispenser_data;		
	unsigned char node ;
	//unsigned char fp_ad;
	//unsigned char ad_char2[2], ad_char4[4];
	unsigned char buffer[256];
	memset(buffer, 0 , 256);
	
	if ((NULL == recvBuffer) || (0 == newsock) ) {		
		UserLog("��������,Translate  ʧ��");
		return -1; 
	}
	node = recvBuffer[1];
	//fp_ad = recvBuffer[9];
	m_st = ((int)recvBuffer[5]&0xe0) >>5;
	//ad_lg = recvBuffer[8];
	//ad1 = recvBuffer[9];  
	
	if (node <= 0) {
		UserLog("���ͻ��ڵ�ֵ����,Translate   ʧ��");
		return -1;
	}
	if (NULL == gpIfsf_shm ) {		
		UserLog("�����ڴ�ֵ����,Translate   ʧ��");
		return -1;
	}
	
	ret = -1;
	switch(m_st) {
		case 0:
			ret = ReadTR_DATA(recvBuffer, buffer,&msg_lg);
			break;
		case 1:
			break;
		case 2:
			ret = WriteTR_DATA(recvBuffer, buffer, &msg_lg);
			break;
		case 3:
		case 4:
			break;
		case 7:
			break;
		default :
			UserLog("�յ������ݵ���Ϣ���ʹ���!!");
			return -1;
			break;
	}
	if(ret == 0) {
		ret = SendData2CD(buffer, msg_lg, 5);
		if (ret <0) {		
			UserLog("���ͻظ����ݴ���,Translate   ʧ��");
			return -1;
		}
		return 0;
	}
	return 0;
}

/*
 * func: �����ѻ����״�������, ת����ͨ���������
 * ret: -1 - ʧ��, 1 - �ɹ�
 */ 
static int TransRecvTR_FILEDATA(const int newsock, const unsigned char *recvBuffer) 
{
	int i, ret, m_st, chnlog;
	unsigned char node;
	unsigned char msgbuffer[40] = {0};
	
	
	if ((NULL == recvBuffer) || (0 == newsock) ) {		
		UserLog("recvBuffer is NULL,Translate failed");
		return -1; 
	}
	
	node = recvBuffer[1];
		  	
	if (node <= 0) {
		UserLog("���ͻ��ڵ�ֵ����,Translate ʧ��");
		return -1;
	}
	
	if (NULL == gpIfsf_shm ) {		
		UserLog("�����ڴ�ֵ����,Translate ʧ��");
		return -1;
	}
	
	for (i = 0; i < MAX_CHANNEL_CNT; ++i) {
		if (node == gpIfsf_shm->node_channel[i].node) {
			break;
		}
	}

	if (i == MAX_CHANNEL_CNT) {
		RunLog("�Ҳ����ýڵ������, Translate ʧ��");
		return -1;
	}

	chnlog = gpIfsf_shm->node_channel[i].chnLog;

	m_st = ((int)recvBuffer[5] & 0xe0) >> 5;
	
	msgbuffer[0] = 0x2F;
	msgbuffer[1] = 0x00;
#if 0
	switch (m_st) {
	case 0:			
		// Read all offline transactions
		msgbuffer[1] = 0x00;
		break;
	case 2:
		// Delete one offline transaction, and request next one
		msgbuffer[1] = 0x01;
		break;
	default:
		RunLog("Message Status Error[%02x]", recvBuffer[5]);
		return -1;

	}
#endif

	msgbuffer[2] = (recvBuffer[6] * 256) + recvBuffer[7] + 8; // Msg len
	memcpy(&msgbuffer[3], recvBuffer, msgbuffer[2]);

	ret = SendMsg(ORD_MSG_TRAN, msgbuffer, msgbuffer[2] + 3, chnlog, 3);
	if (ret < 0) {
		ret = SendMsg(ORD_MSG_TRAN, msgbuffer, msgbuffer[2] + 3, chnlog, 3);
		if (ret < 0) {
			RunLog("Forward[%s]failed", Asc2Hex(msgbuffer, msgbuffer[2] + 3));
			return -1;
		}
	}

//	gpIfsf_shm->msg_cnt_in_queue[chnlog - 1]++;

	return 0;
}


static int TransRecvAllTR_DATA(const int newsock, const unsigned char *recvBuffer) 
{
	int i, ret, ad1, m_st, msg_lg;
	//DISPENSER_DATA *dispenser_data;		
	unsigned char node ;
	unsigned char fp_ad;
	//unsigned char ad_char2[2], ad_char4[4];
	unsigned char buffer[256];
	unsigned char tmpBuffer[64] = {0};
	long trAd = 0;
	FP_TR_DATA *pfp_tr_data;
	
	memset(buffer, 0 , sizeof(buffer));	
	if ((NULL == recvBuffer) || (0 == newsock) ) {		
		UserLog("��������,Translate  ʧ��");
		return -1; 
	}
	node = recvBuffer[1];
	fp_ad = recvBuffer[9];
	if(recvBuffer[10] !=0x20) {
		UserLog("��������,recvBuffer[10] !=0x20, TransRecvAllTR_DATA  ʧ��");
		return -1; 
	}
	m_st = ((int)recvBuffer[5]&0xe0) >> 5;
	//ad_lg = recvBuffer[8];
	//ad1 = recvBuffer[9];  
	
	if (node <= 0) {
		UserLog("���ͻ��ڵ�ֵ����,Translate   ʧ��");
		return -1;
	}
	if (NULL == gpIfsf_shm ) {		
		UserLog("�����ڴ�ֵ����,Translate   ʧ��");
		return -1;
	}

	memcpy(tmpBuffer, recvBuffer, 8 + (recvBuffer[6] * 256) + recvBuffer[7]);
	
	for (i = 0; i < MAX_CHANNEL_CNT; ++i) {
		if (node == gpIfsf_shm->convert_hb[i].lnao.node) {
			break;
		}
	}

	if (i == MAX_CHANNEL_CNT) {
		RunLog("���͵������Ҳ���, TransRecvAllTr ʧ��");
		return -1;
	}

	trAd = i * MAX_TR_NB_PER_DISPENSER;
	tmpBuffer[9] = recvBuffer[9];
	tmpBuffer[10] = 0x21;

	tmpBuffer[11] = 0x00;
	tmpBuffer[12] = 0x00;

	for (i = 0; i < MAX_TR_NB_PER_DISPENSER; ++i) {
		pfp_tr_data = (FP_TR_DATA *)(&(gpIfsf_shm->fp_tr_data[trAd + i]));
		if ((pfp_tr_data->fp_ad == recvBuffer[9]) &&
			((pfp_tr_data->Trans_State == PAYABLE) || (pfp_tr_data->Trans_State == LOCKED))) {
			memcpy(&tmpBuffer[11], pfp_tr_data->TR_Seq_Nb, 2);
	
			ret = -1;
			switch(m_st) {
				case 0:
					ret = ReadTR_DATA(tmpBuffer, buffer,&msg_lg);
					break;
				case 1:
					break;
				case 2:
//					ret = WriteTR_DATA(tmpBuffer, buffer, &msg_lg);
					break;
				case 3:
				case 4:
					break;
				case 7:
					break;
				default :
					UserLog("�յ������ݵ���Ϣ���ʹ���!!");
					return -1;
			}

			if(ret == 0) {
				ret = SendData2CD(buffer, msg_lg, 5);
				if (ret < 0) {		
					if (SendData2CD(buffer, msg_lg, 5) < 0) {
						UserLog("���ͻظ�����ʧ��,TranRecvAllTR_DATA ʧ��");
						return -1;
					}
				}
			}
		}
	}

	// none unpaid or locked transaction
	if (GetCfgMode() || (!GetCfgMode() && ((tmpBuffer[11] == 0) && (tmpBuffer[12] == 0)))){
		buffer[0] = recvBuffer[2];
		buffer[1] = recvBuffer[3];
		buffer[2] = recvBuffer[0];
		buffer[3] = recvBuffer[1];
		buffer[4] = 0x00;
		buffer[5] = (recvBuffer[5] & 0x1F) | 0xe0;
		buffer[6] = 0x00;
		buffer[7] = 0x06;
		buffer[8] = 0x04;
		buffer[9] = recvBuffer[9];     // FP_ID
		buffer[10] = 0x20;             // All TR_DAT
		buffer[11] = 0x00;             // TR_Seq_Nb
		buffer[12] = 0x00;
		buffer[13] = 0x00;             // ACK
		msg_lg = 14;

		ret = SendData2CD(buffer, msg_lg, 5);
		if (ret < 0) {		
			if (SendData2CD(buffer, msg_lg, 5) < 0) {
				UserLog("���ͻظ�����ʧ��,TranRecvAllTR_DATA ʧ��");
				return -1;
			}
		}
	}

	return 0;
}

static int TransRecvER_DATA(const int newsock, const unsigned char *recvBuffer) {
	int i, ret, ad1, m_st, msg_lg;
	//DISPENSER_DATA *dispenser_data;		
	unsigned char node;
	//unsigned char ad_char2[2], ad_char4[4];
	unsigned char buffer[256];
	memset(buffer, 0 , 256);
	
	if ((NULL == recvBuffer) || (0 == newsock) ) {		
		UserLog("��������,Translate  ʧ��");
		return -1;
	}
	 node= recvBuffer[1];
	m_st = ((int)recvBuffer[5]&0xe0) >>5;
	//ad_lg = recvBuffer[8];
	ad1 = recvBuffer[9];
	
	if (node <= 0 ) {
		UserLog("���ͻ��ڵ�ֵ����,Translate   ʧ��");
		return -1;
		}
	if (NULL == gpIfsf_shm ) {		
		UserLog("�����ڴ�ֵ����,Translate   ʧ��");
		return -1;
	}
	
	ret = -1;
	switch(m_st) {
		case 0:
			ret = ReadER_DATA(recvBuffer, buffer,&msg_lg);
			break;
		case 1:
			break;
		case 2:
//			ret = WriteER_DATA(recvBuffer, buffer, &msg_lg);
			break;
		case 3:
		case 4:
			break;
		case 7:
			break;
		default :
			UserLog("�յ������ݵ���Ϣ���ʹ���!!");
			return -1;
			break;
	}
	if(ret == 0) {
		ret = SendData2CD(buffer, msg_lg, 5);
		if (ret <0) {		
			UserLog("���ͻظ����ݴ���,Translate   ʧ��");
			return -1;
		}
		return 0;
	}
	return -1;
}
static int TransRecvPR_DATA(const int newsock, const unsigned char *recvBuffer) {
	int i, ret, ad1, m_st, msg_lg;
	//DISPENSER_DATA *dispenser_data;		
	unsigned char node;
	//unsigned char ad_char2[2], ad_char4[4];
	unsigned char buffer[64];
	memset(buffer, 0 , 64);
	
	if ((NULL == recvBuffer) || (0 == newsock) ) {		
		UserLog("��������,Translate  ʧ��");
		return -1;
	}
	node= recvBuffer[1];
	m_st = ((int)recvBuffer[5]&0xe0) >>5;
	//ad_lg = recvBuffer[8];
	ad1 = recvBuffer[9];
	
	if ((node <= 0) || (node > 127)) {
		UserLog("���ͻ��ڵ�ֵ����,Translate   ʧ��");
		return -1;
		}
	if (NULL == gpIfsf_shm) {		
		UserLog("�����ڴ�ֵ����,Translate   ʧ��");
		return -1;
	}
	
	ret = -1;
	switch(m_st) {
		case 0:
			ret = ReadPR_DATA(recvBuffer, buffer,&msg_lg);
			break;
		case 1:
			break;
		case 2:
			ret = WritePR_DATA(recvBuffer, buffer, &msg_lg);
			break;
		case 3:
		case 4:
			break;
		case 7:
			break;
		default :
			UserLog("�յ������ݵ���Ϣ���ʹ���!!");
			return -1;
			break;
	}
	if(ret == 0) {
		ret = SendData2CD(buffer, msg_lg, 5);
		if (ret <0) {		
			UserLog("���ͻظ����ݴ���,Translate   ʧ��");
			return -1;
		}
		return 0;
	}
	return -1;
}
static int TransRecvFM_DATA(const int newsock, const unsigned char *recvBuffer) {
	int i, ret, ad1, m_st, msg_lg;
	//DISPENSER_DATA *dispenser_data;		
	unsigned char node;
	//unsigned char ad_char2[2], ad_char4[4];
	unsigned char buffer[256];
	
	memset(buffer, 0 , sizeof(buffer));	
	if ((NULL == recvBuffer) || (0 == newsock) ) {		
		UserLog("��������,Translate  ʧ��");
		return -1;
	}	
	m_st = ((int)recvBuffer[5]&0xe0) >>5;
	//ad_lg = recvBuffer[8];
	ad1 = recvBuffer[9];
	node= recvBuffer[1];
	
	if (node <= 0 ) {
		UserLog("���ͻ��ڵ�ֵ����,Translate   ʧ��");
		return -1;
	}
	if (NULL == gpIfsf_shm) {		
		UserLog("�����ڴ�ֵ����,Translate   ʧ��");
		return -1;
	}
	
	ret = -1;
	switch(m_st) {
		case 0:
			ret = ReadFM_DATA(recvBuffer, buffer,&msg_lg);
			break;
		case 1:
			break;
		case 2:
			ret = WriteFM_DATA(recvBuffer, buffer, &msg_lg);
			break;
		case 3:
		case 4:
			break;
		case 7:
			break;
		default :
			UserLog("�յ������ݵ���Ϣ���ʹ���!!");
			return -1;
			break;
	}
	if((ret == 0) || (ret == -5)) {
		ret = SendData2CD(buffer, msg_lg, 5);
		if (ret <0) {		
			UserLog("���ͻظ����ݴ���,Translate   ʧ��");
			return -1;
		}
		return 0;
	}
	return -1;
}
static int TransRecvM_DATA(const int newsock, const unsigned char *recvBuffer) {
	int i, ret, ad1, m_st, msg_lg;
	//DISPENSER_DATA *dispenser_data;		
	unsigned char node;
	//unsigned char ad_char2[2], ad_char4[4];
	unsigned char buffer[256];
	memset(buffer, 0 , 256);
	
	if ((NULL == recvBuffer) || (0 == newsock) ) {		
		UserLog("��������,Translate  ʧ��");
		return -1;
	}	
	m_st = ((int)recvBuffer[5]&0xe0) >>5;
	//ad_lg = recvBuffer[8];
	ad1 = recvBuffer[9];
	node= recvBuffer[1];
	
	if (node <= 0 ) {
		UserLog("���ͻ��ڵ�ֵ����,Translate   ʧ��");
		return -1;
		}
	if (NULL == gpIfsf_shm) {		
		UserLog("�����ڴ�ֵ����,Translate   ʧ��");
		return -1;
	}
	
	ret = -1;
	switch(m_st) {
		case 0:
			ret = ReadM_DATA(recvBuffer, buffer,&msg_lg);
			break;
		case 1:
			break;
		case 2:
			ret = WriteM_DATA(recvBuffer, buffer, &msg_lg);
			break;
		case 3:
		case 4:
			break;
		case 7:
			break;
		default :
			UserLog("�յ������ݵ���Ϣ���ʹ���!!");
			return -1;
			break;
	}

	if(ret == 0) {
		ret = SendData2CD(buffer, msg_lg, 5);
		if (ret <0) {		
			UserLog("���ͻظ����ݴ���,Translate   ʧ��");
			return -1;
		}
		return 0;
	}
	return -1;
}



int DefaultDispenser(const char node) {
	int i, j, k,  ret;
	DISPENSER_DATA *dispenser_data;
	NODE_CHANNEL *pNode_channel;
	if (node == 0  ) {
		UserLog("���ͻ��ڵ�ֵ����,���ͻ���ʼ��ʧ��");
		return -1;
	}
	if (NULL == gpIfsf_shm) {		
		UserLog("�����ڴ�ֵ����,���ͻ���ʼ��ʧ��");
		return -1;
	}
	for (i=0; i < MAX_CHANNEL_CNT; i++) {
		if (gpIfsf_shm->convert_hb[i].lnao.node == node) {
		       	break; //�ҵ��˼��ͻ��ڵ�
		}
	}
	if(i == MAX_CHANNEL_CNT) {
		UserLog("���ͻ��ڵ������Ҳ���,���ͻ���ʼ��ʧ��");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)(&(gpIfsf_shm->dispenser_data[i]));
	for (i=0; i < MAX_CHANNEL_CNT; i++) {
		if (gpIfsf_shm->node_channel[i].node == node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT) {
		UserLog("���ͻ��ڵ������Ҳ���,���ͻ���ʼ��ʧ��");
		return -1;
	}
	pNode_channel= (NODE_CHANNEL *)(&(gpIfsf_shm->node_channel[i]));

	/*NODE_CHANNEL  ����            */
	/*0 �ڵ��			*/
	/*1 �߼�ͨ����			*/
	/*2 �������			*/
	/*3 ���ں�			*/
	/*4 ����ͨ����			*/
	/*5 ���ͻ������		*/
	/*6 ���ͻ���ǹ��	        */
	/*7 ���ͻ���Ʒ��         	*/
	/*8 ���ͻ����1-4 ��ǹ��        */
	ret=DefaultC_DATA(node);	
	if (ret < 0) {
		return -1;
	}
	//printf("ִ��DefaultC_DATA�ɹ�\n");

	dispenser_data->fp_c_data.Nb_FP = pNode_channel->cntOfFps;
	dispenser_data->fp_c_data.Nb_Products= pNode_channel->cntOfProd;
	//the relation of meter and product is one to one in China.
	dispenser_data->fp_c_data.Nb_Meters = pNode_channel->cntOfProd; 


	k = 0;
	//printf("��ʼִ��DefaultFP_DATA\n");
	for (i = 0; i < pNode_channel->cntOfFps; i++) {
		ret = DefaultFP_DATA(node, 0x21 + i);
		if (ret <0) {
				UserLog("DefaultFP_DATA error, node[%d], fp_ad[%x]", node, 0x21+i);
				return -1;
		}
//		printf("ִ��DefaultFP_DATA�ɹ�\n");

		//the relation of lnz and pnz is one to one in China.
		dispenser_data->fp_data[i].Nb_Logical_Nozzle = pNode_channel->cntOfFpNoz[i]; 
//		dispenser_data->fp_data[i].Log_Noz_Mask = 0;

		for (j = 0; j < pNode_channel->cntOfFpNoz[i]; j++) {
			k++;
			ret = DefaultLN_DATA(node, 0x21+i, 0x11+j); //oher nozzle attribute will be set in initDp00x .
			if (ret <0) {
				UserLog("DefaultLN_DATA error, node[%d], fp_ad[%x], ln_ad[%x]", node, 0x21+i, 0x11+j);
				return -1;
			}
//			printf("ִ��DefaultLN_DATA�ɹ�\n");
			//the relation of lnz and pnz is one to one in China.
			dispenser_data->fp_ln_data[i][j].Physical_Noz_Id = k; 

		}
	}
//	printf("ִ��DefaultFP_DATA, DefaultLN_DATA�ɹ�\n");
	for (i=0; i< pNode_channel->cntOfProd; i++) { //not use dispenser to  blend oil, blended oil only is in tank in China.
		ret = DefaultPR_DAT(node, 0x41+i);
		if (ret <0) {
				UserLog("DefaultPR_DAT error, node[%d], pr_ad[%x]", node, 0x41+i);
				return -1;
		}
		ret = DefaultFM_DAT(node, 0x41+i); //each product only has one fuelling mode in China now.
		if (ret <0) {
				UserLog("DefaultFM_DAT error, node[%d], pr_ad[%x]", node, 0x41+i);
				return -1;
		}
		ret = DefaultM_DAT(node, 0x81+i); //the relation of meter and product is one to one in China.
		if (ret <0) {
				UserLog("DefaultM_DAT error, node[%d], m_ad[%x]", node, 0x81+i);
				return -1;
		}
	}	
	//printf("ִ��DefaultPR_DAT, DefaultFM_DAT,DefaultM_DAT �ɹ�\n");
	return 0;
}
//Ҫʹ��gpIfsf_shm,gpIfsf_shm����Ӧ�ķ������ؼ�������ֵindex,ע��P,V����������.
//���Ӻ�ɾ��һ�����ͻ�.ע�����Ӻ�ɾ��������.
int AddDispenser(const char node) { //����DefaultDispenser����.
	int i,j, ret;
	DISPENSER_DATA *dispenser_data;
	if (node == 0  ) {
		UserLog("���ͻ��ڵ�ֵ����,���ͻ�Add ʧ��");
		return -1;
	}
	if (NULL == gpIfsf_shm) {		
		UserLog("�����ڴ�ֵ����,���ͻ�Add ʧ��");
		return -1;
	}
	for (i=0; i < MAX_CHANNEL_CNT; i++) {
		if (gpIfsf_shm->convert_hb[i].lnao.node == node)  {
			UserLog("���ͻ��ڵ��Ѿ�����,���ͻ�Add ʧ��");
			return -1; 
		}
	}
	for (i=0; i < MAX_CHANNEL_CNT; i++) {
		if (gpIfsf_shm->node_channel[i].node == node)  
			break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT) {
		UserLog("�����б����Ҳ����ýڵ�,���ͻ�Add ʧ��");
		return -1;
	}
#if DEBUG
	RunLog("�����ͻ��ڵ�, convert_hb[%d].lnao.node = %d", i, node);
#endif

	ret = AddSendHB(node);
	if (ret < 0) {
		UserLog("�ڵ� %d ������������", node);
		return -1;
	}
#if DEBUG
	RunLog("���������ɹ�");
#endif

	//dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	
	//printf("��ʼִ��DefaultDispenser\n");
	ret = DefaultDispenser(node);
	if (ret <0) {
		UserLog("DefaultDispenser error, node[%d] ", node);
		return -1;
	}
#if DEBUG
	RunLog("���������ڵ�Ĭ�����Գɹ�");
#endif


	return 0;	
}
int DelDispenser(const char node) {//delete include heartbeat.
	int index;
	int i,j, ret;
	DISPENSER_DATA *dispenser_data;
	//IFSF_HEARTBEAT  *ifsf_heartbeat;
	if (node == 0 ) {
		UserLog("���ͻ��ڵ�ֵ����,���ͻ�Del  ʧ��");
		return -1;
	}
	if (NULL == gpIfsf_shm  ) {		
		UserLog("�����ڴ�ֵ����,���ͻ�Del ʧ��");
		return -1;
	}
	for (i=0; i < MAX_CHANNEL_CNT; i++) {
		if (node == gpIfsf_shm->node_online[j])  break; //�ҵ��˿��м��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT) {
		UserLog("���ͻ��ڵ������Ҳ���,���ͻ�Del ʧ��");
		return -1;
	}
	for (j=0; j< MAX_CHANNEL_CNT; j++) {
		if (node == gpIfsf_shm->convert_hb[j].lnao.node)  break; //�ҵ��˿��м��ͻ��ڵ�
	}
	if(j == MAX_CHANNEL_CNT) {
		UserLog("���ͻ��ڵ����������Ҳ���,���ͻ�Del ʧ��");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)(&(gpIfsf_shm->dispenser_data[i]));
	index = IFSF_DP_SEM_INDEX ;
	Act_P(HEARTBEAT_SEM_CONVERT_INDEX);
	//Act_P(int index)
	gpIfsf_shm->node_online[i] = 0;
	memset(&(gpIfsf_shm->convert_hb[j]), 0,  sizeof(IFSF_HEARTBEAT));
	memset(&(gpIfsf_shm->node_channel[j]), 0,  sizeof(NODE_CHANNEL));
	//gpIfsf_shm->convertCnt --;
	Act_V(HEARTBEAT_SEM_CONVERT_INDEX);
	Act_P(index);
	//gpIfsf_shm->convert_hb[i].lnao.node = 0;
	memset(dispenser_data, 0,  sizeof(DISPENSER_DATA));
	Act_V(index);
	return 0;
}
int SetDispenser(const char node, const DISPENSER_DATA *dispenser_data) {
	int index;
	int i,j, ret;
	DISPENSER_DATA *pdispenser_data;
	//IFSF_HEARTBEAT  *ifsf_heartbeat;
	if (node == 0) {
		UserLog("���ͻ��ڵ�ֵ����,���ͻ�Set ʧ��");
		return -1;
	}
	if (NULL == gpIfsf_shm ) {		
		UserLog("�����ڴ�ֵ����,���ͻ���ʼ��ʧ��");
		return -1;
	}
	for (i=0; i < MAX_CHANNEL_CNT; i++) {
		if (gpIfsf_shm->convert_hb[i].lnao.node == node)  {
			UserLog("���ͻ��ڵ��Ѿ�����,���ͻ�Set  ʧ��");
			return -1; 
		}
	}
	for (i=0; i < MAX_CHANNEL_CNT; i++) {
		if (gpIfsf_shm->convert_hb[i].lnao.node  <= 0)  break; //�ҵ��˿��м��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT) {
		UserLog("���ͻ��ڵ������Ҳ���,���ͻ�Set ʧ��");
		return -1;
	}

	pdispenser_data = (DISPENSER_DATA *)(&(gpIfsf_shm->dispenser_data[i]));
	index = IFSF_DP_SEM_INDEX ;	
	Act_P(index);
	gpIfsf_shm->convert_hb[i].lnao.node = node;
	memcpy(pdispenser_data, dispenser_data,  sizeof(DISPENSER_DATA));
	Act_V(index);
	//Act_P(int index)
	ret = AddSendHB(node);
	if (ret <0)  {
		UserLog("���ͻ��ڵ���������add ����,���ͻ�Set  ʧ��");
		return -1;
	}
	//Act_V(int index)
	return 0;
}
/*  
int InitIfsfFpShm() {
	if (NULL == gpIfsf_shm ) {		
		UserLog("�����ڴ�ֵ����,���ͻ���ʼ��ʧ��");
		return -1;
	}
	memset(gpIfsf_shm, 0, sizeof(IFSF_SHM));
	return 0;
}
*/
/*--------------------------------------------------------------*/
//����Ϊ�����ļ�Ҫ�õ��ĺ���:
//--------------------------------
/*TcpRecvServ����:
  ����(��ִ��,���Ҫִ�еĻ�)�յ��ļ��ͻ���Ϣ����,�ɹ�����0,ʧ�ܷ���-1
  //����������������Ҫ��ŵ�,���ú���SetFpByRecv()�����gpIfsf_shm��;
  //����������������Ҫ��Ӧ(Acknowledge��Answer)��,���ú���MakeSendByRecv().
*/
int TranslateRecv(const int newsock, const unsigned char *recvBuffer) 
{
	int i, ret, fp_state = 0 , ad_lg, ad1;
	int recv_lg;
	DISPENSER_DATA *dispenser_data;		
	unsigned char node;
	//unsigned char ad_char2[2], ad_char4[4];
	unsigned char allBuffer[MAX_TCP_LEN];
	
	if((NULL == recvBuffer) || (0 == newsock)) {		
		UserLog("��������,Translate  ʧ��");
		return -1;
	}

	if('\x01' != recvBuffer[0]) {		
		UserLog("��������,���Ǽ��ͻ�������,Translate ʧ��");
		return -1;
	}	

	node= recvBuffer[1];
	//m_st = ((int)recvBuffer[5]&0xe0) >>5;
	ad_lg = recvBuffer[8];
	ad1 = recvBuffer[9];
//	UserLog("Translaterecv: ad_lg[%d], ad_el[%2x]",ad_lg,ad1);
	if (0 == node) {
		ret = TransRecvDspAdErr(newsock, recvBuffer, 1);
		UserLog("���ͻ��ڵ�ֵ����,Translate   ʧ��");
		return -1;
	}

	if (NULL == gpIfsf_shm ) {
		ret = TransRecvDspAdErr(newsock, recvBuffer, 2);
		UserLog("�����ڴ�ֵ����,Translate   ʧ��");
		return -1;
	}

	for (i = 0; i < MAX_CHANNEL_CNT; i++) {
		if (node == gpIfsf_shm->convert_hb[i].lnao.node) {
		      	break; //�ҵ��˼��ͻ��ڵ�
		}
	}

	if(i == MAX_CHANNEL_CNT) {
		ret = TransRecvDspAdErr(newsock, recvBuffer, 1);
		UserLog("���ͻ��ڵ������Ҳ���,Translate   ʧ��");
		return -1;
	}
	//UserLog("TransRecv"); 
	dispenser_data = (DISPENSER_DATA *)(&(gpIfsf_shm->dispenser_data[i]));
	for (i = 0; i < MAX_FP_CNT; i++) { 
		if (fp_state < (dispenser_data->fp_data[i].FP_State)) 
			fp_state = (dispenser_data->fp_data[i].FP_State) ;
		if (fp_state == 0) {
			ret = TransRecvDspAdErr(newsock, recvBuffer, 2);
			return -1; 
		}
		if (fp_state > 9) {
			ret = TransRecvDspAdErr(newsock, recvBuffer, 2);
			return -1;
		}
	}

	if (1 == ad_lg) {
		if(0 ==ad1) 
			ret =TransRecvSV_DATA(newsock,  recvBuffer);
		else if (1 == ad1) 
			ret = TransRecvC_DATA(newsock,  recvBuffer);
		else if ((ad1>=0x21) && (ad1 <= 0x24))
			ret = TransRecvFP_DATA(newsock,  recvBuffer);
		else if ((ad1>=0x41) && (ad1 <= 0x48))
			ret = TransRecvPR_DATA(newsock,  recvBuffer);
		else if ((ad1>=0x81) && (ad1 <= 0x90))
			ret = TransRecvM_DATA(newsock, recvBuffer);
		else if (ad1 == 0x20) {
			memset(allBuffer, 0 , sizeof(allBuffer));	
			recv_lg = recvBuffer[6];
			recv_lg = recv_lg * 256 + recvBuffer[7];
			memcpy(allBuffer,recvBuffer,recv_lg+8);
			for (i=0;i<dispenser_data->fp_c_data.Nb_FP; i++) {				
				allBuffer[9] = 0x21+i;
				ret = TransRecvFP_DATA(newsock,  allBuffer);
				if (ret <0) {
					//UserLog("Translate ���յ������ݴ���!!");
					break;
				}	
			}
		} else if (ad1 == 0x40) {
			memset(allBuffer, 0 , sizeof(allBuffer));	
			recv_lg = recvBuffer[6];
			recv_lg = recv_lg * 256 + recvBuffer[7];
			memcpy(allBuffer,recvBuffer,recv_lg+8);

			/*
			 * �޸�Ϊ�ϴ�ȫ����Ʒ��Ϣ, �����Ƿ�Ϊ��,
			 * ��Ϊ��Ч��Ʒ��������Ϊ 41/42/44,���������Ʒ���ϴ���Ʒ��Ϣ,
			 * ��ô��Ʒ���ò������������, �ϴ�����Ʒ��Ϣ����ȫ.
			 * Guojie Zhu @ 2009.09.08
			 */
			//for (i=0;i<dispenser_data->fp_c_data.Nb_Products; i++) {				
			for (i = 0; i < FP_FM_CNT; i++) {				
				allBuffer[9] = 0x41+i;
				ret = TransRecvPR_DATA(newsock,  allBuffer);
				if (ret <0) {
					//UserLog("Translate ���յ������ݴ���!!");
					break;
				}	
			}
		} else if (ad1 == 0x80) {
			memset(allBuffer, 0 , sizeof(allBuffer));	
			recv_lg = recvBuffer[6];
			recv_lg = recv_lg * 256 + recvBuffer[7];
			memcpy(allBuffer,recvBuffer,recv_lg+8);
			for (i=0;i<dispenser_data->fp_c_data.Nb_Meters; i++) {				
				allBuffer[9] = 0x81+i;
				ret = TransRecvM_DATA(newsock,  allBuffer);
				if (ret <0) {
					//UserLog("Translate ���յ������ݴ���!!");
					break;
				}	
			}
		} else {
			ret = TransRecvDspAdErr(newsock, recvBuffer, 6);
			UserLog("���յ������ݵĵ�ַ����!!");
			return -1;
		}		
	} else if (2 == ad_lg) {
		if ((recvBuffer[10] >=  0x11) && (recvBuffer[10] <=  0x18) &&
						(ad1>=0x21) && (ad1 <= 0x24)) {
			ret = TransRecvLN_DATA(newsock, recvBuffer);
		} else if ((recvBuffer[10] == 0x10) && (ad1>=0x21) && (ad1 <= 0x24)) {
			memset(allBuffer, 0 , sizeof(allBuffer));	
			recv_lg = recvBuffer[6];
			recv_lg = recv_lg * 256 + recvBuffer[7];
			memcpy(allBuffer,recvBuffer,recv_lg+8);
			for (i = 0;i<dispenser_data->fp_data[ad1-0x21].Nb_Logical_Nozzle; i++) {
				allBuffer[10] = 0x11 + i;
				ret = TransRecvLN_DATA(newsock,  allBuffer);
				if (ret <0) {
					//UserLog("Translate ���յ������ݴ���!!");
					break;
				}	
			}
		} else {
			ret = TransRecvDspAdErr(newsock, recvBuffer, 6);
			UserLog("���յ������ݵĵ�ַ����!!");
			return -1;
		}		
	} else if (3 == ad_lg) {
		if (recvBuffer[10] ==  0x41) {
			ret = TransRecvER_DATA(newsock, recvBuffer);
		} else {//all error read not implement.
			ret = TransRecvDspAdErr(newsock, recvBuffer, 6);
			UserLog("���յ������ݵĵ�ַ����!!");
			return -1;
		}			
	} else if (4 == ad_lg) {  
		if (recvBuffer[10] == 0x21) {//recvBuffer[9] = 21,22,23,24
			if( (recvBuffer[13] == 0xC8) ||  (recvBuffer[13] == 0xC9))//�յ���λ�����ѻ���������
			{
	                           //answer first trasaction 
	                           //memcpy(offline_tr[node] , trasaction );//��trasaction�ȴ�ɾ��
//				RunLog(">> before call TransRecvTR_FILE_DATA");
	                         ret =  TransRecvTR_FILEDATA(newsock, recvBuffer);

			} else {
				ret = TransRecvTR_DATA(newsock, recvBuffer);
			}
		 
		} else if (recvBuffer[10] == 0x20) {
			ret = TransRecvAllTR_DATA(newsock, recvBuffer);
		} else {		
			ret = TransRecvDspAdErr(newsock, recvBuffer, 6);
			UserLog("���յ������ݵĵ�ַ����!!");
			return -1;
		}	

	} else if (6 == ad_lg) {
		if ((recvBuffer[14] >=  0x11) && (recvBuffer[14] <=  0x18) &&  (ad1 ==0x61)) {
			ret =TransRecvFM_DATA(newsock, recvBuffer);
		} else if ((recvBuffer[14] == 0x10) && (ad1 ==0x61)) {
			memset(allBuffer, 0 , sizeof(allBuffer));	
			recv_lg = recvBuffer[6];
			recv_lg = recv_lg * 256 + recvBuffer[7];
			memcpy(allBuffer,recvBuffer,recv_lg+8);

			for (i = 0; i < FP_FM_CNT; i++) {
				allBuffer[14] = 0x11 + i;
				ret = TransRecvFM_DATA(newsock, allBuffer);
				if (ret < 0) {
					ret = TransRecvDspAdErr(newsock, recvBuffer, 2);
					//UserLog("Translate ���յ������ݴ���!!");
					return -1;
				}
			}
		
		} else {
			ret = TransRecvDspAdErr(newsock, recvBuffer, 6);
			UserLog("���յ������ݵĵ�ַ����!!");
			return -1;
		}
	} else {
		ret = TransRecvDspAdErr(newsock, recvBuffer, 6);
		UserLog("���յ������ݵĵ�ַ����!!");
		return -1;
	}

	if (ret <0) {
		ret = TransRecvDspAdErr(newsock, recvBuffer, 2);
		UserLog("Translate ���յ������ݴ���!!");
		return -1;
	}	

	return 0;
}

/*
 * func: ��С����Ϊת��Ϊ2λ
 * eg. 03007800 => 04000780 (Bin8 + Bcd6)
 */
void TwoDecPoint(unsigned char *buffer, int len) 
{
	unsigned char tmp[16] = {0};
	const int orig_len = len;
	int pos;

	memcpy(tmp, buffer, len);
	bzero(buffer, len);
	buffer[0] = (unsigned char)((len - 2) * 2);
	if (tmp[0] % 2 == 0) {   // Bin8 even
		pos = (tmp[0] / 2)  + 1;
		if (pos >= orig_len) {
			pos = orig_len - 1;
		}
		for (; pos > 0; --pos) {
			buffer[--len] = tmp[pos];
		}
	} else {                 // Bin8 odd
		pos = ((tmp[0] + 1) / 2) + 1;
		for (; pos > 1; --pos) {
			buffer[--len] = (pos >= orig_len ? 0x00 : (tmp[pos] >> 4)) | 
					(pos <= 1 ? 0x00 : (tmp[pos - 1] << 4));
		}
	}

}


/*--------------------------------------------------------------*/



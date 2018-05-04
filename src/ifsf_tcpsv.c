/*
 * 2008-03-12 - Guojie Zhu <zhugj@css-intelligent.com>
 *     ����ReadSV_DATA�������ݵĸ�ʽ����
 */

#include <stdio.h>
#include <string.h>

#include "ifsf_pub.h"

#include "log.h"
#include "power.h"
#include "mytime.h"
#include "setcfg.h"

extern IFSF_SHM *gpIfsf_shm; //�ýṹ��ȫ�ֵ�����ֻ��һ��,��ifsf_main.c�ж���.
//extern IFSF_HB_SHM *gpIfsf_shm;
////extern IFSF_FP_SHM *gpIfsf_shm;

int DefaultSV_DATA(){//Ĭ�ϳ�ʼ��.
	//int i ;
	FCC_COM_SV_DATA *pfp_sv_data;	
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,ͨѶ�������ݳ�ʼ��ʧ��");
		return -1;
	}	
	pfp_sv_data =  (FCC_COM_SV_DATA *)( &(gpIfsf_shm->fcc_com_sv_data) );
	
	memset(pfp_sv_data,0,sizeof(FCC_COM_SV_DATA));
	pfp_sv_data->Communication_Protocol_Ver[4] = '\x01';
	pfp_sv_data->Communication_Protocol_Ver[5] = '\x02';
	pfp_sv_data->Heartbeat_Interval = HEARTBEAT_INTERVAL;
	strcpy(pfp_sv_data->Heartbeat_Broad_IP, HEARTBEAT_BROAD_IP);
	return 0;
}
int SetSV_DATA(const FCC_COM_SV_DATA *fcc_com_sv_data){
	int i;
	FCC_COM_SV_DATA *pfp_sv_data;
	if( NULL == fcc_com_sv_data ){		
		UserLog("������ֵ����,ͨѶ�������ݳ�ʼ��ʧ��");
		return -1;
	}	
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,ͨѶ�������ݳ�ʼ��ʧ��");
		return -1;
	}	
	pfp_sv_data =  (FCC_COM_SV_DATA *)( &(gpIfsf_shm->fcc_com_sv_data));
	
	memcpy(pfp_sv_data,fcc_com_sv_data,sizeof(FCC_COM_SV_DATA));
	
	return 0;
	}
//��ȡFP_COM_SV_DATA. 
//COM_SV��ַΪ0, recvBufferΪ�յ���data,sendBufferΪҪ���͵�data,msg_lgΪ���ɵ��ܳ���.
int ReadSV_DATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg){
	int i, j, k,tmp,  recv_lg, tmp_lg;
	FCC_COM_SV_DATA *pfp_sv_data;
	unsigned char node= recvBuffer[1];
	unsigned char buffer[256];
	memset(buffer, 0 , 256);

	if((NULL == recvBuffer) || (NULL == sendBuffer) || (NULL == msg_lg) ){		
		UserLog("������ֵ����,ͨѶ��������read ʧ��");
		return -1;
	}	
	if('\x01' != recvBuffer[0] && 0x09 != recvBuffer[0]){		
		UserLog("��������,���Ǽ��ͻ�������,ͨѶ��������Read ʧ��");
		return -1;
	}
	if( (node <= 0) || (node > 127 ) ){
		UserLog("���ͻ��ڵ�ֵ����,ͨѶ��������read ʧ��");
		return -1;
		}	
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,ͨѶ��������Read  ʧ��");
		return -1;
	}	
	pfp_sv_data =  (FCC_COM_SV_DATA *)( &(gpIfsf_shm->fcc_com_sv_data));
	
	buffer[0] = recvBuffer[2];
	buffer[1] = recvBuffer[3];
	buffer[2] = recvBuffer[0];
	buffer[3] = recvBuffer[1];
	buffer[4] = '\0'; //recvBuffer[4];
	buffer[5] = (recvBuffer[5] |0x20); //answer
	recv_lg = recvBuffer[6];
	recv_lg = recv_lg * 256 + recvBuffer[7];
	buffer[8] = '\x01'; //recvBuffer[8]; Ad_lg
	buffer[9] = '\0'; //recvBuffer[9];Ad
	//buffer[10] = recvBuffer[10]; //It's Data_Id. Data begin.
	tmp_lg = 9;
//	RunLog(" before ReadSV_DATA. for ......., recv_lg: %d", recv_lg);
	for(i = 0; i < (recv_lg - 2); i++){
//		RunLog(" into ReadSV_DATA. for .......");
		switch(recvBuffer[10+i] ){
		case Communication_Protocol_Ver:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char) Communication_Protocol_Ver   ;
			tmp_lg++;
			buffer[tmp_lg] ='\x06';
			tmp_lg++;
			buffer[tmp_lg] = pfp_sv_data->Communication_Protocol_Ver[0];
			tmp_lg++;
			buffer[tmp_lg] = pfp_sv_data->Communication_Protocol_Ver[1];
			tmp_lg++;
			buffer[tmp_lg] = pfp_sv_data->Communication_Protocol_Ver[2];
			tmp_lg++;
			buffer[tmp_lg] = pfp_sv_data->Communication_Protocol_Ver[3];
			tmp_lg++;
			buffer[tmp_lg] = pfp_sv_data->Communication_Protocol_Ver[4];
			tmp_lg++;
			buffer[tmp_lg] = pfp_sv_data->Communication_Protocol_Ver[5];
			break;
		case Local_Node_Address:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char) Local_Node_Address    ;
			tmp_lg++;			
			buffer[tmp_lg] ='\x02';
			tmp_lg++;
			buffer[tmp_lg] = '\x01';
			tmp_lg++;
			buffer[tmp_lg] = node;
			break;
		case Recipient_Addr_Table:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char) Recipient_Addr_Table    ;
			tmp_lg++;
			k=tmp_lg;
			tmp = 0;
			for(j=0;j<MAX_RECV_CNT; j++){
				if((gpIfsf_shm->recv_hb[j].lnao.subnet != (unsigned char)0) &&  \
					(gpIfsf_shm->recv_hb[j].lnao.node != (unsigned char)0)){
					tmp++;
					tmp++;
					tmp_lg++;
					buffer[tmp_lg] = gpIfsf_shm->recv_hb[j].lnao.subnet;
					tmp_lg++;
					buffer[tmp_lg] = gpIfsf_shm->recv_hb[j].lnao.node ;
				}				
			}
			buffer[k] = (unsigned char)tmp;
			break;
		case Heartbeat_Interval:
//			RunLog(" into ReadSV_DATA. case HB_interval");
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char) Heartbeat_Interval ;
			tmp_lg++;
			buffer[tmp_lg] ='\x01';
			tmp_lg++;
			buffer[tmp_lg] = pfp_sv_data->Heartbeat_Interval;
			break;
		case Heartbeat_Broad_IP:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char) Heartbeat_Broad_IP   ;
			tmp_lg++;
			buffer[tmp_lg] ='\x10';
			tmp_lg++;
			buffer[tmp_lg] = pfp_sv_data->Heartbeat_Broad_IP[0];
			tmp_lg++;
			buffer[tmp_lg] = pfp_sv_data->Heartbeat_Broad_IP [1];
			tmp_lg++;
			buffer[tmp_lg] = pfp_sv_data->Heartbeat_Broad_IP [2];
			tmp_lg++;
			buffer[tmp_lg] = pfp_sv_data->Heartbeat_Broad_IP [3];
			tmp_lg++;
			buffer[tmp_lg] = pfp_sv_data->Heartbeat_Broad_IP [4];
			tmp_lg++;
			buffer[tmp_lg] = pfp_sv_data->Heartbeat_Broad_IP [5];
			tmp_lg++;
			buffer[tmp_lg] = pfp_sv_data->Heartbeat_Broad_IP [6];
			tmp_lg++;
			buffer[tmp_lg] = pfp_sv_data->Heartbeat_Broad_IP [7];
			tmp_lg++;
			buffer[tmp_lg] = pfp_sv_data->Heartbeat_Broad_IP [8];
			tmp_lg++;
			buffer[tmp_lg] = pfp_sv_data->Heartbeat_Broad_IP [9];
			tmp_lg++;
			buffer[tmp_lg] = pfp_sv_data->Heartbeat_Broad_IP [10];
			tmp_lg++;
			buffer[tmp_lg] = pfp_sv_data->Heartbeat_Broad_IP [11];	
			tmp_lg++;
			buffer[tmp_lg] = pfp_sv_data->Heartbeat_Broad_IP [12];	
			tmp_lg++;
			buffer[tmp_lg] = pfp_sv_data->Heartbeat_Broad_IP [13];	
			tmp_lg++;
			buffer[tmp_lg] = pfp_sv_data->Heartbeat_Broad_IP [14];	
			tmp_lg++;
			buffer[tmp_lg] = pfp_sv_data->Heartbeat_Broad_IP [15];	
			break;
		default:
			tmp_lg++;
			buffer[tmp_lg] =recvBuffer[i + 10] ;
			tmp_lg++;
			buffer[tmp_lg] ='\0';
			break;
		}
	}
	tmp_lg++;
	//RunLog("TLG:ReadSV_DATA.out of for , tmp_lg: %d", tmp_lg);
	buffer[6] = (unsigned char )((tmp_lg-8) / 256) ;
	buffer[7] = (unsigned char )((tmp_lg-8) % 256);  //big end.
	*msg_lg= tmp_lg;
	memcpy(sendBuffer, buffer, tmp_lg);	
	return 0;
}
//дFP_COM_SV_DATA����.recvBufferΪ�յ���data,sendBufferΪҪ�ظ���data, 
//msg_lgΪ���ɵ��ܳ���,Ϊ0��ʾû�лظ���Ϣ,sendBuffer��msg_lg���ظ���������ΪNULL,.
int WriteSV_DATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg){
	int i, j, k,ret,  recv_lg, tmp_lg, lg;
	FCC_COM_SV_DATA *pfp_sv_data;
	unsigned char tmpMSAck, tmpDataAck;
	unsigned char node=recvBuffer[1];
	unsigned char buffer[256], cmd[17];
	memset(buffer, 0 , 256);
	memset(cmd,0,17);
	if(NULL == recvBuffer) {		
		UserLog("������ֵ����,ͨѶ��������Write ʧ��");
		return -1;
	}	
	if(  '\x01' != recvBuffer[0]  ){		
		UserLog("��������,���Ǽ��ͻ�������,ͨѶ��������Write ʧ��");
		return -1;
	}
	if( (node <= 0) || (node > 127 ) ){
		UserLog("���ͻ��ڵ�ֵ����,ͨѶ��������Write ʧ��");
		return -1;
		}	
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,ͨѶ��������Write ʧ��");
		return -1;
	}	
	pfp_sv_data =  (FCC_COM_SV_DATA *)( &(gpIfsf_shm->fcc_com_sv_data));
	
	buffer[0] = recvBuffer[2];
	buffer[1] = recvBuffer[3];
	buffer[2] = recvBuffer[0];
	buffer[3] = recvBuffer[1];
	buffer[4] = '\0'; //recvBuffer[4];
	buffer[5] = ((recvBuffer[5] & 0x1f) | 0xe0); // ACK
	recv_lg = recvBuffer[6];
	recv_lg = recv_lg * 256 + recvBuffer[7];
	buffer[8] = '\x01'; //recvBuffer[8]; Ad_lg
	buffer[9] = '\0'; //recvBuffer[9];Ad
	//recvBuffer[10];Data_Id
	//buffer[10]   //MS_Ack
	tmp_lg = 10;
	i=0;
	tmpMSAck = '\0'; // all is  ok.
	while( i < (recv_lg -2) ) {
		tmpDataAck = '\0'; //ok ack.
		lg =  (int )recvBuffer[11+i]; //Data_Lg.
		switch( recvBuffer[10+i] ) {
		case Heartbeat_Interval: //w(*)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char) Heartbeat_Interval;			
			if( lg== 1 ) 
				pfp_sv_data->Heartbeat_Interval = recvBuffer[12+i];
			else  { 
				tmpDataAck = '\x01';
				tmpMSAck = '\x05';
				//UserLog("write  C_DAT's Nb_Products error!too big or small.") 
				}			
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		//COMMAND
		case Add_Recipient_Addr: // It must be 10 byte TCPIP Heart beat address.
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)Add_Recipient_Addr;
			if(  lg== 10 ){
				for(j=0;j<MAX_RECV_CNT; j++) {
					RunLog("recvBuffer  %d  %02x %02x  %02x %02x ", i,  recvBuffer[12+i+6], recvBuffer[12+i+7], gpIfsf_shm->recv_hb[j].lnao.subnet, gpIfsf_shm->recv_hb[j].lnao.node);
					if( (recvBuffer[12+i+6] == gpIfsf_shm->recv_hb[j].lnao.subnet) && \
					     (recvBuffer[12+i+7] == gpIfsf_shm->recv_hb[j].lnao.node)    )
							break;
					}
					if(j == MAX_RECV_CNT){
						ret = AddRecvHB((IFSF_HEARTBEAT * )(&recvBuffer[12+i]));
						if(ret <0) {
							tmpDataAck = '\x06'; 
							tmpMSAck = '\x05';
						}
					}
					
				}				
			else  {
				tmpDataAck = '\x01'; 
				tmpMSAck = '\x05';
				}			
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case Remove_Recipient_Addr: //
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)Remove_Recipient_Addr;
			if(  lg== 10 ){				
				ret = DelRecvHB((IFSF_HEARTBEAT * )(&recvBuffer[12+i]));
				if(ret <0) {
					tmpDataAck = '\x06'; 
					tmpMSAck = '\x05';
					}	
				}				
			else  {
				tmpDataAck = '\x01'; 
				tmpMSAck = '\x05';
				}			
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case Syc_Time: //
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)Syc_Time;
			if( lg== 7 ) {
				cmd[0] ='\0';
				cmd[1] ='\0';
				memcpy(cmd, &recvBuffer[12+i], 7);
				ret = sync_time( cmd);
				if(ret <0) {
					tmpDataAck = '\x06'; 
					tmpMSAck = '\x05';
					}	
				}
			else  {
				tmpDataAck = '\x01'; 
				tmpMSAck = '\x05';
				}			
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case Power_Failure: //
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)Power_Failure;
			if( lg== 0 ) {
				k = 3;
				ret=WriteNandFlash();
				while( (ret < 0) && (k--) ){
					ret=WriteNandFlash(); 
					}
				if(k<=0) {
					tmpDataAck = '\x06'; 
					tmpMSAck = '\x05';
				}
				}
			else  {
				tmpDataAck = '\x01'; 
				tmpMSAck = '\x05';
				}			
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
	case Node_Channel_Cfg: //
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)Node_Channel_Cfg;
			if( (lg%sizeof(NODE_CHANNEL)) == 0 ) {				
				memcpy(&(gpIfsf_shm->node_channel[0]), &recvBuffer[12+i], lg);
				ret = RecvNodeCfg(&recvBuffer[12+i], lg);
				if(ret < 0){
						UserLog("Node_Channel_Cfg save falure.");
						}
				for(j=0;j<lg/sizeof(NODE_CHANNEL); j++){
					ret = AddSendHB(recvBuffer[12+i+j*sizeof(NODE_CHANNEL)]);
					if(ret < 0){
						tmpDataAck = '\x06'; 
						tmpMSAck = '\x05';
						}
					}
				}
			else  {
				tmpDataAck = '\x01'; 
				tmpMSAck = '\x05';
				}			
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
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
		UserLog("(i + 2) != recv_lg , write COM_SV_Dat error!! exit, (i + 2)[%d], recv_lg[%d]",i+2, recv_lg);
		//exit(0);
		return -2;
	}
	tmp_lg++;
	buffer[6] = (unsigned char )((tmp_lg -8)/ 256) ; //M_Lg
	buffer[7] = (unsigned char )((tmp_lg -8) %256);  //big end.
	buffer[10] = tmpMSAck;  
	if(NULL != msg_lg) *msg_lg= tmp_lg;
	if(NULL != sendBuffer)  memcpy(sendBuffer, buffer, tmp_lg);
	return 0;
}
//дFP_COM_SV_DATA�ĵ���������ĺ���.��ʼ������PCD�豸��д������.
//�ɹ�����0,ʧ�ܷ���-1����Data_Ack.
int SetSglSV_DATA(const unsigned char *data){
	int i, j, k,ret,   lg;
	FCC_COM_SV_DATA *pfp_sv_data;
	unsigned char tmpDataAck;//tmpMSAck, 
	//unsigned char node=recvBuffer[1];
	unsigned char cmd[17];	
	memset(cmd,0,17);
	if(NULL == data) {		
		UserLog("������ֵ����,ͨѶ��������SetSgl ʧ��");
		return -1;
	}	
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,ͨѶ��������SetSgl ʧ��");
		return -1;
	}	
	pfp_sv_data =  (FCC_COM_SV_DATA *)( &(gpIfsf_shm->fcc_com_sv_data));
	
	
	tmpDataAck = '\0'; //ok ack.
	lg =  (int )data[1]; //Data_Lg.
	switch( data[0] )
	{
		case Heartbeat_Interval: //w(*)
			if( lg== 1 ) 
				pfp_sv_data->Heartbeat_Interval = data[2] ;
			else  { 
				tmpDataAck = '\x01';
				}			
			break;
		//COMMAND
		case Add_Recipient_Addr: // It must be 10 byte TCPIP Heart beat address.
			if(  lg== 10 ){
				for(j=0;j<MAX_RECV_CNT; j++) {
					if( ( data[8] == gpIfsf_shm->recv_hb[j].lnao.subnet) && \
					     (data[9]  == gpIfsf_shm->recv_hb[j].lnao.node)    )
							break;
					}
					if(j == MAX_RECV_CNT){
						ret = AddRecvHB((IFSF_HEARTBEAT * )(&data[2] ));
						if(ret <0) {
							tmpDataAck = '\x06'; 
							}
					}
					
				}				
			else  {
				tmpDataAck = '\x01'; 
				}			
			break;
		case Remove_Recipient_Addr: //
			if(  lg== 10 ){				
				ret = DelRecvHB((IFSF_HEARTBEAT * )(&data[2]));
				if(ret <0) {
					tmpDataAck = '\x06'; 
					}	
				}				
			else  {
				tmpDataAck = '\x01'; 
				}			
			break;		
		case Power_Failure: //
			if( lg== 0 ) {
				k = 3;
				ret=WriteNandFlash();
				while( (ret < 0) && (k--) ){
					ret=WriteNandFlash();
					}
				if(k<=0) {
					tmpDataAck = '\x06'; 
					}
				}
			else  {
				tmpDataAck = '\x01'; 
				}			
			break;
		case Node_Channel_Cfg: //
			if( (lg%sizeof(NODE_CHANNEL)) == 0 ) {				
				memcpy(&(gpIfsf_shm->node_channel[0]), &data[2], lg);
				ret = RecvNodeCfg(&data[2], lg);
				if(ret < 0){
						UserLog("Node_Channel_Cfg save falure.");
						}
				for(j=0;j<lg/sizeof(NODE_CHANNEL); j++){
					ret = AddSendHB(data[2+j*sizeof(NODE_CHANNEL)]);
					if(ret < 0){
						tmpDataAck = '\x06'; 
						}
					}
				}
			else  {
				tmpDataAck = '\x01'; 
				}			
			break;
		default:
			tmpDataAck = '\x04';  
			break;
	}
	
	return tmpDataAck;
}
//��������PCD����������Ϣ��������������CD,(�ڵ��,�ڵ�����)
//��������: 2+nodeCD+1+1+0x20(Msg_St)+Msg_Lg+0+222+
//Data_Lg+(node1+chnLog11+serialport1+chnPhy1)+(node2+chnLog12+serialport2+chnPhy2)+...
int RequestNode_Channel(unsigned char node){
 	int i, tmp_lg, ret;  
	unsigned char buffer[16];
	memset(buffer, 0 , 16 );	
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,��ʼ�����ò���Request Node_Channel");
		return -1;
	}
	if( (node <= 0) || (node > 127 ) ){
		UserLog("���ͻ��ڵ�ֵ����,��ʼ�����ò���Request Node_Channel");
		return -1;
		}
	//gpIfsf_shm->convertUse[node-1] =(unsigned char) node;
	
	
	buffer[0] = '\x02';
	buffer[1] = '\x01';
	buffer[2] = '\x01';
	buffer[3] = node;
	buffer[4] = '\0';  //IFSF_MC
	buffer[5] =  '\0'; //M_St; //read.
	
	buffer[8] = '\x01'; //recvBuffer[8];
	buffer[9] = '\x01'; //recvBuffer[9];
	//buffer[10] = recvBuffer[10]; //It's Data_Id. Data begin.
	tmp_lg=9;
	
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)Node_Channel_Cfg;
	
	tmp_lg++;
	buffer[6] ='\0' ;
	buffer[7] = (unsigned char)tmp_lg  ; 
	ret= SendData2CD(buffer, tmp_lg, 5);
	if (ret <0 ){
		UserLog("���ͻ�ͨѶ����,��ʼ�����ò���Request Node_Channel");
		return -1;
	}
	return 0;
}

//ͬ��ʱ��. dataʱ������Data_Id(ֵ200)+Data_Lg(ֵ7)+Data_El(ֵ��ʽYYYYMMDDHHMMSS).
//static int Syc_TimeCmd(const unsigned char *data);
int RequestSyc_Time(unsigned char node){//������ʱ��ͬ�����ݵ�CD
	int i, tmp_lg, ret;  
	unsigned char buffer[16];
	memset(buffer, 0 , 16 );	
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,��ʼ�����ò���Request Node_Channel");
		return -1;
	}
	if( (node <= 0) || (node > 127 ) ){
		UserLog("���ͻ��ڵ�ֵ����,��ʼ�����ò���Request Node_Channel");
		return -1;
		}
	//gpIfsf_shm->convertUsed[node-1] =(unsigned char) node;
	
	
	buffer[0] = '\x02';
	buffer[1] = '\x01';
	buffer[2] = '\x01';
	buffer[3] = node;
	buffer[4] = '\0';  //IFSF_MC
	buffer[5] =  '\0'; //M_St; //read.
	
	buffer[8] = '\x01'; //recvBuffer[8];
	buffer[9] = '\x01'; //recvBuffer[9];
	//buffer[10] = recvBuffer[10]; //It's Data_Id. Data begin.
	tmp_lg=9;
	
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)Syc_Time;
	
	tmp_lg++;
	buffer[6] ='\0' ;
	buffer[7] = (unsigned char)tmp_lg  ; 
	ret= SendData2CD(buffer, tmp_lg, 5);
	if (ret <0 ){
		UserLog("���ͻ�ͨѶ����,��ʼ�����ò���Request Syc_Time");
		return -1;
	}
	return 0;
}





/*******************************************************************
�����������
-------------------------------------------------------------------
2007-07-04 by chenfm
2008-03-06 - Gunjie Zhu <zhugj@css-intelligent.com>
    convertUsed[] ��Ϊ convert_hb[].lnao.node
*******************************************************************/
#include <string.h>
#include "ifsf_dsper.h"
#include "ifsf_pub.h"
//#include "pub.h
//#include "ifsf_def.h"
//#include "ifsf_fpad.h""


extern IFSF_SHM *gpIfsf_shm; //�ýṹ��ȫ�ֵ�����ֻ��һ��,��ifsf_main.c�ж���.

//FP_ER_DATA��������. node���ͻ��߼��ڵ��,fp_ad���͵��ַ21h-24h,
// Ҫ�õ�gpIfsf_shm,ע��P,V����������.
//�ı�������.  Ҫ����MakeErMsg����.er_ad�����ַ01h-40h
/*
 * param: msgbuffer[2] �Ժ�Ϊ��׼��ACK Message ����
 *        msgbuffer[1] ΪACK Message���ĵĳ���
 *        msgbuffer[0] Ϊevent������ACK Message�е�λ�� (0 - msgbuffer[1]-1)
 */
int ChangeER_DATA(const __u8 node, const __u8 fp_ad, const __u8 er_ad, const __u8 *msgbuffer)
{
	int i, fp_state, ret;
	int fp = fp_ad-0x21; //0,1,2,3�ż��͵�.
	int er = er_ad-1; //0,1,2,3,4,5,6,..63�Ŵ���.
	int data_lg;
	unsigned char sendBuffer[128];
	const unsigned char *ack_msg = msgbuffer + 2;
	DISPENSER_DATA *dispenser_data;
	FP_ER_DATA *pfp_er_data;	

/*		
1H RAM defect 2H ROM defect 3H Configuration or Parameter Error 4H Power supply out of order
5H Main Communication error 6H Display error 7H Pulser error 8H Calculation error
9H Blender error 0AH Download error 0BH Checksum error 0CH Leak Error 
0DH PCD RAM defect 0EH PCD ROM defect 0FH PCD Configuration or Parameter Error 10H PCD Power supply out of order
11H PCD Main Communication error 12H Vapour Recovery Error 13H-1FH Spare
MINOR ERROR
20H Battery error 21H Communication error 22H Customer_Stop_Pressed 23H Spare
Fuelling Errors 
24H Authorise_Time_Out 25H Fill_Time_Out 26H No_Progress 27H Limit_Reached
State Error 
2AH Vapour Recovery Timer Started 2BH Vapour Recovery Timer Reset 2CH Vapour Recovery Module Defect
28H Fuelling suspended 29H Fuelling resumed 2DH State error 1: FP is state INOPERATIVE
2EH State error 2: FP in state CLOSED
2FH State error 3: FP is already opened
30H State error 4: Transaction not in progress
31H State error 5: Transaction already started
32H State error 6: Parameter/Configuration change not possible
33H CD identifier not correct (assign, release, resume, clear)
34H Urea temperature low, heater failed. This is an optional minor error
and is only required by Urea dispensers. It should not be supported
by other dispensers e.g. Diesel, LPG, petroleum, etc.
*/
	

	char *err[160] = {
		"RAM defect", "ROM defect", "Configuration or Parameter Error", \
		"Power supply out of order", "Main Communication error", "Display error", \
	       	"Pulser error ", "Calculation error", "Blender error", "Download error",  \
	       	"Checksum error", "Leak Error ", "PCD RAM defect", "PCD ROM defect",      \
		"PCD Configuration or Parameter Error ", "PCD Power supply out of order", \
		"PCD Main Communication error", "Vapour Recovery Error", "", "", "", "",  \
		"", "", "", "", "", "", "", "", "", "Battery error", "Communication error", \
	     	"Customer_Stop_Pressed",   "",  "Authorise_Time_Out", "Fill_Time_Out",    \
	       	"No_Progress ",   "Limit_Reached",  "Fuelling suspended", \
		"Fuelling resumed ",  "Vapour Recovery Timer Started", \
	     	"Vapour Recovery Timer Reset",  "Vapour Recovery Module Defect", \
		"State error 1: FP is state INOPERATIVE", \
	       	"State error 2: FP in state CLOSED",  \
		"State error 3: FP is already opened", \
	       	"State error 4: Transaction not in progress",\
		"State error 5: Transaction already started", \
	      	"State error 6: Parameter/Configuration change not possible",  \
		"CD identifier not correct (assign, release, resume, clear)",  \
	      	"Urea temperature low, heater failed. ", \
		 "", "", "", "", "",\
		"", "", "","", "", "", "", "", "", "", "", "", "", "", "", "", \
		"", "", "","", "", "", "", "", "", "", "", "", "", "", "", "", \
		"", "", "","", "", "", "", "", "", "", "", "", "", "", "", "", \
		"", "", "","", "", "", "", "", "", "", "", "", "", "", "", "", \
		"", "", "","", "", "", "", "", "", "", "", "", "", "", "", "", \
		"", "", "","", "", "", "", "", "", "", "", "", "", "", "", \
		"Price change successePrice change successedd"
	};


	if (er < 0) {
		RunLog("Node[%d]FP[%d] error code ֵ����", node, fp + 1);
		return -1;
	}

	memset(sendBuffer, 0, 128);
	if( (node <= 0) || (node > 127 ) ){
		UserLog("���ͻ��ڵ�ֵ����,���´������� ʧ��");
		return -1;
	}
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,���´������� ʧ��");
		return -1;
	}
	for(i = 0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,���´������� ʧ��");
		return -1;
	}

	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	pfp_er_data =  (FP_ER_DATA *)( &(dispenser_data->fp_er_data[fp])  );
	fp_state = dispenser_data->fp_data[fp].FP_State;
	if( fp_state > 9 ) {
		return -1;	
	}
	pfp_er_data->FP_Error_Type = er_ad;

	if (strlen(err[er]) > sizeof(pfp_er_data->FP_Err_Description)) {
		memcpy(pfp_er_data->FP_Err_Description, err[er], sizeof(pfp_er_data->FP_Err_Description));
	}
	else {
		memcpy(pfp_er_data->FP_Err_Description, err[er], strlen(err[er]));
	}
	pfp_er_data->FP_Error_Total += 1;
	pfp_er_data->FP_Error_State = fp_state;

	//Update_Prod_No();
	
	if (msgbuffer != NULL) {
		ret = SendData2CD(ack_msg, msgbuffer[1], 5);
		if (ret < 0) {
			RunLog("Send command Acknowledge to CD fialed!");
		}
	}

	//usr i to ret
	i = MakeErMsg( node,  fp_ad, sendBuffer, &data_lg);
	if(i<0){
		UserLog("MakeErMsg error.");
		return -1;
	}

	i = SendData2CD(sendBuffer, data_lg, 5);
	if(i < 0){
		UserLog("SendData2CD error.");
		return -1;
	}

	return 0;
}

int DefaultER_DATA(const char node, const char fp_ad){
	int i, fp_state;
	int fp=fp_ad-0x21; //0,1,2,3�ż��͵�.
	//int er=er_ad-1; //0,1,2,3,4,5,6,..63�Ŵ���.
	//unsigned char tmpUChar;
	DISPENSER_DATA *dispenser_data;
	FP_ER_DATA *pfp_er_data;
	
	if( (node <= 0) || (node > 127 ) ){
		UserLog("���ͻ��ڵ�ֵ����,���´����ʼ�� ʧ��");
		return -1;
		}
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,���´����ʼ��ʧ��");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,���´����ʼ��ʧ��");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	pfp_er_data =  (FP_ER_DATA *)( &(dispenser_data->fp_er_data[fp])  );
	fp_state = dispenser_data->fp_data[fp].FP_State;
	if( fp_state >9 ) return -1;	
	pfp_er_data->FP_Error_Type = '\0';
	memset(pfp_er_data->FP_Err_Description, 0,20);
	pfp_er_data->FP_Error_Total  = '\0';
	pfp_er_data->FP_Error_State = fp_state;
	
	return 0;
}
//��ȡFP_ER_DATA. �������Ϣ���ݵ�����Ϊ001�ش�.
//recvBufferΪ�յ���data,sendBufferΪҪ���͵�data,msg_lgΪ���ɵ��ܳ���.
int ReadER_DATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg){
	int i,j,k, fp_state,tmp_lg=0, recv_lg;
	int fp = recvBuffer[9] - 0x21; //0,1,2,3�ż��͵�.
	int er=recvBuffer[11] - 1; //0,1,2,3,4,5,6,..63�Ŵ���.
	unsigned char node = recvBuffer[1];
	DISPENSER_DATA *dispenser_data;
	FP_ER_DATA *pfp_er_data;
	//int data_lg;
	unsigned char buffer[128];
	memset(buffer, 0, 128);
	if( (node <= 0) || (node > 127 ) ){
		UserLog("���ͻ��ڵ�ֵ����,���´���Read ʧ��");
		return -1;
	}		
	if( (node <= 0) || (node > 127 ) ){
		UserLog("���ͻ��ڵ�ֵ����,���´���Read ʧ��");
		return -1;
		}
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,���´���Read ʧ��");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,���´���Read ʧ��");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	pfp_er_data =  (FP_ER_DATA *)( &(dispenser_data->fp_er_data[fp])  );
	fp_state = dispenser_data->fp_data[fp].FP_State;
	if( fp_state >9 ) return -1;	
	
	buffer[0] = recvBuffer[2];
	buffer[1] = recvBuffer[3];
	buffer[2] = recvBuffer[0];
	buffer[3] = recvBuffer[1];
	buffer[4] = '\0'; //recvBuffer[4]; IFSF_MC
	buffer[5] = ( (recvBuffer[5] &0x1f) |0x20);//answer.
	recv_lg = recvBuffer[6];
	recv_lg = recv_lg * 256 + recvBuffer[7];
	buffer[8] = '\x03'; //recvBuffer[8]; //Ad_lg
	buffer[9] = recvBuffer[9];  //ad
	buffer[10] = recvBuffer[10]; //ad
	buffer[11] = pfp_er_data->FP_Error_Type; //ad
	//buffer[12] = recvBuffer[12]; //It's Data_Id. Data begin.	
	tmp_lg=11;
	for(i=0;i<recv_lg - 4; i++){
		switch(recvBuffer[12+i]) {			
			case FP_Error_Type :
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)FP_Error_Type;
				tmp_lg++;
				buffer[tmp_lg] = '\x01';
				tmp_lg++;
				buffer[tmp_lg] = pfp_er_data->FP_Error_Type;
				break;			
			case FP_Err_Description :
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)FP_Err_Description;
				tmp_lg++;
				buffer[tmp_lg] = '\x14';
				k= buffer[tmp_lg];
				for(j=0;j<k;j++){
					tmp_lg++;
					buffer[tmp_lg] = pfp_er_data->FP_Err_Description[j];
				}
				break;
			case FP_Error_Total:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)FP_Error_Total;
				tmp_lg++;
				buffer[tmp_lg] = '\x01';
				tmp_lg++;
				buffer[tmp_lg] = pfp_er_data->FP_Error_Total;
				break;			
			case FP_Error_State:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)FP_Error_State;
				tmp_lg++;
				buffer[tmp_lg] = '\x01';					
				tmp_lg++;
				buffer[tmp_lg] = pfp_er_data->FP_Error_State;				
				break;			
			default:
				tmp_lg++;
				buffer[tmp_lg] = recvBuffer[12+i];
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
//дFP_ER_DATA����.recvBufferΪ�յ���data,sendBufferΪҪ�ظ���data,�������Ϣ���ݵ�����Ϊ111Ӧ��.
//msg_lgΪ���ɵ��ܳ���,Ϊ0��ʾ����ҪӦ����Ϣ,sendBuffer��msg_lg����ҪӦ����Ϣ��������ΪNULL.
//int WriteER_DATA(const char node, const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg);
//int SetSglER_DATA(const char node, const char fp_ad, const unsigned char  *data);

//������������FP_Error_Type_Mes��ֵ������,sendbufferΪ�����͵�����,data_lg�����ܳ���
static int MakeErMsg(const char node, const char fp_ad, unsigned char *sendBuffer, int *data_lg){
	int i,k,fp_state,tmp_lg=0; //recv_lg;
	int fp=fp_ad-0x21; //0,1,2,3�ż��͵�.
	//int er; //0,1,2,3,4,5,6,..63�Ŵ���.
	//unsigned char tmpUChar;
	DISPENSER_DATA *dispenser_data;
	FP_ER_DATA *pfp_er_data;
	//int data_lg;
	unsigned char buffer[64];
	memset(buffer, 0, 64);
	if( (node <= 0) || (node > 127 ) ){
		UserLog("���ͻ��ڵ�ֵ����,���´���MakeErMsg ʧ��");
		return -1;
	}		
	if( (NULL == sendBuffer) || (NULL == data_lg) ){
		UserLog("����Ϊ�մ���,���´���MakeErMsg ʧ��");
		return -1;
		}
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,���´���MakeErMsg ʧ��");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,���´���MakeErMsg ʧ��");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	pfp_er_data =  (FP_ER_DATA *)( &(dispenser_data->fp_er_data[fp])  );
	fp_state = dispenser_data->fp_data[fp].FP_State;
	if( fp_state >9 )
		return -1;	
	
	buffer[0] = '\x02';
	buffer[1] = '\x01';
	buffer[2] = '\x01';
	buffer[3] = node;
	buffer[4] = '\0'; //recvBuffer[4]; IFSF_MC
	buffer[5] = '\x80';//unsolicited data..
	//recv_lg = recvBuffer[6];
	//recv_lg = recv_lg * 256 + recvBuffer[7];
	buffer[8] = '\x03'; //recvBuffer[8]; //Ad_lg
	buffer[9] = fp_ad;  //ad
	buffer[10] ='\x41'; //ad
	buffer[11] = pfp_er_data->FP_Error_Type; //ad
	//buffer[12] = recvBuffer[12]; //It's Data_Id. Data begin.	
	tmp_lg=11;

	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)FP_Error_Type_Mes;
	tmp_lg++;
	buffer[tmp_lg] = '\0';
	
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)FP_Error_Type;
	tmp_lg++;
	buffer[tmp_lg] = '\x01';
	tmp_lg++;
	buffer[tmp_lg] = pfp_er_data->FP_Error_Type;
	
		
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)FP_Error_State;
	tmp_lg++;
	buffer[tmp_lg] = '\x01';	
	tmp_lg++;
	buffer[tmp_lg] = pfp_er_data->FP_Error_State;
				
				
	tmp_lg++;
	buffer[6] = (unsigned char )((tmp_lg - 8) / 256) ;
	buffer[7] = (unsigned char )((tmp_lg - 8) % 256);  //big end.
	*data_lg= tmp_lg;
	memcpy(sendBuffer, buffer, tmp_lg);	

	return 0;
}
//������������TFP_Error_Type_Mes��ֵ������.��������ĺ���,subnetOfCD��nodeOfCDΪCD��������ַ���߼��ڵ��.
//static int MakeErMsgSend(const char node, const char fp_ad, const char subnetOfCD,const char nodeOfCD, unsigned char *sendBuffer, int *msg_lg);

/*--------------------------------------------------------------*/









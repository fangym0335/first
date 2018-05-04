
/*
 * ����ReadTLGER_DATA,WriteTLG_DATA�ظ����ݵĸ�ʽ����
 */

//#include  "ifsf.h"
#include <string.h>

#include  "ifsf_tlg.h"
#include  "ifsf_tlgtlg.h"
//#include "ifsf_pub.h"

//#include  "ifsf_tlgtp.c"


/*TANK LEVEL GAUGE DATABASE  DB_Ad = TLG_DAT (01H)
Data_Id ��
01H= Nb_Tanks
02H= Reference_Temp
03H= TLG_Measurement_Unit
06H= Country_Code
32H= TLG_Manufacturer_Id
33H= TLG_Model
34H= TLG_Type
35H=TLG_Serial_Nb
36H= TLG_Appl_Software_Ver
3Ah=IFSF_Protocol_Ver
3Bh=Current_Date
3CH=Current_Time
3DH=SW_Checksum
O - These are individual optional elements that are not part of an identified group,
including some that are only necessary for certain types of gauges.
O1 - This group identifies gauges that are capable of timekeeping and record
dating.
O2 - This group identifies gauges that have implemented the fuel and water height
threshold alarms.
O3 - This group identifies gauges that are able to calculate the volume of the
contents of the tank.
O4 - This group identifies gauges that have implemented the volume threshold
alarms.
O5 - This group identifies gauges that are capable of measuring fuel temperatures
and calculating temperature compensated volumes.



//TANK LEVEL GAUGE DATABASE  DB_Ad = TLG_DAT (01H)
enum TLG_DAT_AD{
	T_Nb_Tanks = 1, T_Reference_Temp = 2, T_TLG_Measurement_Unit = 3, T_Country_Code = 6, T_Maint_Password = 7,
	T_TLG_Manufacturer_Id = 0x32,	T_TLG_Model=0x33, T_TLG_Type=0x34, T_TLG_Serial_Nb=0x35, T_TLG_Appl_Software_Ver=0x36, 
	T_IFSF_Protocol_Ver=0x3a,	T_Current_Date=0x3b, T_Current_Time=0x3c, T_SW_Checksum=0x3d
};
typedef struct{	
	unsigned char Nb_Tanks;
	unsigned char Reference_Temp[4];//O5
	unsigned char TLG_Measurement_Unit; //0 �C metric
	unsigned char Country_Code; //china:9156 by ifsf.
	unsigned char Maint_Password[6]; //M. But it's not in CNPC standard 
	unsigned char TLG_Manufacturer_Id[3]; //
	unsigned char TLG_Model[3];
	unsigned char TLG_Type[3];
	unsigned char TLG_Serial_Nb[12];
	unsigned char TLG_Appl_Software_Ver[12];
	unsigned char IFSF_Protocol_Ve[12]; //
	unsigned char Current_Date[4];//O1
	unsigned char Current_Time[3];//O1
	unsigned char SW_Checksum[4];
	//CMD:70:Enter_Maint_Mode;71:Exit_Maint_Mode;	
} TLG_DAT;
//-
//��Э��ת����һ��Һλ�ǵĽṹ��.
//-
typedef struct{	
	TLG_DAT tlg_dat;
	TP_DAT tp_dat[MAX_TP_PER_TLG];
} TLG_DATA;
*/

extern IFSF_SHM *gpIfsf_shm; //�ýṹ��ȫ�ֵ�����ֻ��һ��,��ifsf_main.h�ж���.
//-
//Һλ��5���Ӷ�ʱ����ά��ʧЧ����, ������Enter_Maint_Mode��ִ�г����е�alarm(300)����.
//����:����maint_enableΪ��Ч.
//-
static void MaintAlarmDisable(int sig){
	int i, ret;
	TLG_DATA *pTlg_data;
	//TLG_DAT *pTlg_dat;
	
	if( NULL == gpIfsf_shm ){		
		printf("�����ڴ�ֵ����,Һλ��MaintAlarmʧ��");
		return ;
	}
	pTlg_data = (TLG_DATA*)( &(gpIfsf_shm->tlg_data) );
	//pTlg_dat = (TLG_DAT*)( &(pTlg_data->tlg_dat) );

	pTlg_data->maint_enable = 0; //��Ч.
	for(i=0;i<MAX_TP_PER_TLG;i++){
		if( (3 == gpIfsf_shm->tlg_data.tp_dat[i].TP_Status) ||(1 == gpIfsf_shm->tlg_data.tp_dat[i].TP_Status) ){						
			ret = ChangeTpStatAlarm( gpIfsf_shm->tlg_data.tlg_node, 0x21+i, 2, NULL);
			if(ret <0){ 
				printf("ChangeTpStatAlarm error in MaintAlarmDisable");
			}
		}
	}
	alarm(0); //�ر�alarm.
	return ;
}

//-
//Ĭ�ϳ�ʼ��Һλ��TLG_DA.	
//-
int DefaultTLG_DAT(const char node){
	//int i;
	TLG_DATA *pTlg_data;
	TLG_DAT *pTlg_dat;
	//TP_DAT  *pTp_dat;
	unsigned char date_time[7];
	
	if( NULL == gpIfsf_shm ){		
		printf("�����ڴ�ֵ����,Һλ�ǳ�ʼ��ʧ��");
		return -1;
	}
	if( (node <= 0) || (node > 127 )  ){//||(node != gpIfsf_shm->node_tlg)
		printf("Һλ�ǽڵ�ֵ����,Һλ�ǳ�ʼ�� ʧ��");
		return -1;
	}
	pTlg_data = (TLG_DATA*)( &(gpIfsf_shm->tlg_data) );
	pTlg_dat = (TLG_DAT*)( &(pTlg_data->tlg_dat) );
	//pTp_dat =  (TP_DAT *)( &(pTlg_data->tp_dat[i]) );
	bzero(pTlg_dat, sizeof(TLG_DAT));
	pTlg_dat->Nb_Tanks = 1;
	pTlg_dat->Reference_Temp[0] = 4;
	pTlg_dat->Reference_Temp[1] = 0;
	pTlg_dat->Reference_Temp[2] = 0x15;
	pTlg_dat->Reference_Temp[3] = 0;    //15.00 Celsius degrees
	pTlg_dat->TLG_Measurement_Unit = 0; //0 �C metric
	pTlg_dat->Country_Code[0] = 0x00;   // china
	pTlg_dat->Country_Code[1] = 0x86;
//	bzero(pTlg_dat->Maint_Password, sizeof(pTlg_dat->Maint_Password));
	strncpy(pTlg_dat->TLG_Manufacturer_Id, "PTR", 3);
	strncpy(pTlg_dat->TLG_Model, "PTR", 3);
	strncpy(pTlg_dat->TLG_Type, "PTR", 3);
	memset(pTlg_dat->TLG_Serial_Nb, 0x20 , 12);  // set to space
	bzero(pTlg_dat->TLG_Appl_Software_Ver, sizeof(pTlg_dat->TLG_Appl_Software_Ver));
	strncpy(pTlg_dat->TLG_Appl_Software_Ver, "PTRCSS", 6);
	pTlg_dat->TLG_Appl_Software_Ver[10] = 0x01;
	pTlg_dat->TLG_Appl_Software_Ver[11] = 0x00; //V1.00
	pTlg_dat->IFSF_Protocol_Ver[0] = 0x20;
	pTlg_dat->IFSF_Protocol_Ver[1] = 0x07;
	pTlg_dat->IFSF_Protocol_Ver[2] = 0x05;
	pTlg_dat->IFSF_Protocol_Ver[3] = 0x00;
	pTlg_dat->IFSF_Protocol_Ver[4] = 0x01;
	pTlg_dat->IFSF_Protocol_Ver[5] = 0x28; // 2007.5 V1.28
	getlocaltime(date_time);
	memcpy(pTlg_dat->Current_Date, &date_time[0], 4);
	memcpy(pTlg_dat->Current_Time, &date_time[4], 3);
	strncpy(pTlg_dat->SW_Checksum, "0000", 4);

	return 0;
}
//-
//����Һλ��.�����ͺ�����.
//-
int SetTLG_DAT(const char node, const TLG_DAT *tlg_dat){
	//int i;
	TLG_DATA *pTlg_data;
	TLG_DAT *pTlg_dat;
	
	if( NULL == gpIfsf_shm ){		
		printf("�����ڴ�ֵ����,Һλ��SetTLG_DATʧ��");
		return -1;
	}
	if( NULL == tlg_dat ){		
		printf("����ֵ����,Һλ��SetTLG_DATʧ��");
		return -1;
	}
	if( (node <= 0) || (node > 127 )  ){//||(node != gpIfsf_shm->node_tlg)
		printf("Һλ�ǽڵ�ֵ����,Һλ��SetTLG_DAT ʧ��");
		return -1;
	}
	
	pTlg_data = (TLG_DATA*)( &(gpIfsf_shm->tlg_data) );
	pTlg_dat = (TLG_DAT*)( &(pTlg_data->tlg_dat) );
	memcpy(pTlg_dat, tlg_dat, sizeof(TLG_DAT));
	return 0;
}
//-
//��дTLG_DATA����. ע��Ҫ�������Ϣ���ݵ�����,000��,001�ش�,010д,011������Ӧ��,100��������Ӧ��,111Ӧ��
//recvBufferΪ�յ���data,sendBufferΪҪ���͵�data,msg_lgΪ���ɵ��ܳ���.
//-

//-
//��ȡTLG_DATA.TLG_DAT��ַΪ1.�������Ϣ���ݵ�����Ϊ001�ش�
//-
int ReadTLG_DAT(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg){
	int i, recv_lg, tmp_lg = 0;
	int tp_stat;
	TLG_DATA *pTlg_data;
	TLG_DAT *pTlg_dat;
	//TP_DAT  *pTp_dat;
	unsigned char node;
	unsigned char buffer[256];
	
	if( NULL == gpIfsf_shm ){		
		printf("�����ڴ�ֵ����,Һλ��ReadTLG_DATʧ��");
		return -1;
	}
	if( (NULL == recvBuffer) || (NULL == sendBuffer) || (NULL == msg_lg)  ){		
		printf("��������,Һλ��read ʧ��");
		return -1;
	}
	
	node= recvBuffer[1];
	pTlg_data = (TLG_DATA*)( &(gpIfsf_shm->tlg_data) );
	pTlg_dat = (TLG_DAT*)( &(pTlg_data->tlg_dat) );
	
	
	bzero(buffer, sizeof(buffer));
	if(  '\x09' != recvBuffer[0]  ){		
		printf("��������,����Һλ�ǵ�����,Һλ��ReadTLG_DAT ʧ��");
		return -1;
	}
	
	if( node <= 0 ){//||(node != gpIfsf_shm->node_tlg
		printf("Һλ�ǽڵ�ֵ����,Һλ��read ʧ��");
		return -1;
		}
	
	
	
	buffer[0] = recvBuffer[2];
	buffer[1] = recvBuffer[3];
	buffer[2] = recvBuffer[0];
	buffer[3] = recvBuffer[1];
	buffer[4] = '\0'; //recvBuffer[4];
	buffer[5] = ( (recvBuffer[5] &0x1f)  |0x20); //answer
	recv_lg = recvBuffer[6];
	recv_lg = recv_lg * 256 + recvBuffer[7];
	buffer[8] = '\x01'; //db_ad_lg. recvBuffer[8];
	buffer[9] = '\x01'; //db_ad. recvBuffer[9];
	//buffer[10] = recvBuffer[10]; //It's Data_Id. Data begin.
	tmp_lg = 9;
	for(i=0; i < (recv_lg - 2); i++){
		switch( recvBuffer[10+i] ){
			case T_Nb_Tanks:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Nb_Tanks;
				tmp_lg++;
				buffer[tmp_lg] ='\x01';
				tmp_lg++;
				buffer[tmp_lg] = pTlg_dat->Nb_Tanks;
				break;
			case T_Reference_Temp: 
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Reference_Temp;
				tmp_lg++;
				buffer[tmp_lg] ='\x04';
				tmp_lg++;
				buffer[tmp_lg] = pTlg_dat->Reference_Temp[0];
				tmp_lg++;
				buffer[tmp_lg] = pTlg_dat->Reference_Temp[1];
				tmp_lg++;
				buffer[tmp_lg] = pTlg_dat->Reference_Temp[2];
				tmp_lg++;
				buffer[tmp_lg] = pTlg_dat->Reference_Temp[3];
				break;
			case T_TLG_Measurement_Unit:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_TLG_Measurement_Unit;
				tmp_lg++;
				buffer[tmp_lg] ='\x01';
				tmp_lg++;
				buffer[tmp_lg] = pTlg_dat->TLG_Measurement_Unit;
				break;
			case T_Country_Code:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Country_Code;
				tmp_lg++;
				buffer[tmp_lg] ='\x02';
				tmp_lg++;
				buffer[tmp_lg] = pTlg_dat->Country_Code[0];
				tmp_lg++;
				buffer[tmp_lg] = pTlg_dat->Country_Code[1];
				break;		
				
			case T_Maint_Password://never can be read 
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Maint_Password;
				tmp_lg++;
				buffer[tmp_lg] =0;
				/*
				tmp_lg++;
				buffer[tmp_lg] = pTlg_dat->Maint_Password[0];
				tmp_lg++;
				buffer[tmp_lg] = pTlg_dat->Maint_Password[1];
				tmp_lg++;
				buffer[tmp_lg] = pTlg_dat->Maint_Password[2];
				tmp_lg++;
				buffer[tmp_lg] = pTlg_dat->Maint_Password[3];
				tmp_lg++;
				buffer[tmp_lg] = pTlg_dat->Maint_Password[4];
				tmp_lg++;
				buffer[tmp_lg] = pTlg_dat->Maint_Password[5];
				*/
				break;
			case T_TLG_Manufacturer_Id:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_TLG_Manufacturer_Id;
				tmp_lg++;
				buffer[tmp_lg] =3;
				tmp_lg++;
				buffer[tmp_lg] = pTlg_dat->TLG_Manufacturer_Id[0];
				tmp_lg++;
				buffer[tmp_lg] = pTlg_dat->TLG_Manufacturer_Id[1];
				tmp_lg++;
				buffer[tmp_lg] = pTlg_dat->TLG_Manufacturer_Id[2];
				break;
			case T_TLG_Model:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_TLG_Model;
				tmp_lg++;
				buffer[tmp_lg] =3;
				tmp_lg++;
				buffer[tmp_lg] = pTlg_dat->TLG_Model[0];
				tmp_lg++;
				buffer[tmp_lg] = pTlg_dat->TLG_Model[1];
				tmp_lg++;
				buffer[tmp_lg] = pTlg_dat->TLG_Model[2];;
				break;
			case T_TLG_Type:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_TLG_Type;
				tmp_lg++;
				buffer[tmp_lg] =3;
				tmp_lg++;
				buffer[tmp_lg] = pTlg_dat->TLG_Type[0];
				tmp_lg++;
				buffer[tmp_lg] = pTlg_dat->TLG_Type[1];
				tmp_lg++;
				buffer[tmp_lg] = pTlg_dat->TLG_Type[2];
				break;
			case T_TLG_Serial_Nb:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_TLG_Serial_Nb;
				tmp_lg++;
				buffer[tmp_lg] =12;
				tmp_lg++;
				memcpy( &buffer[tmp_lg], pTlg_dat->TLG_Serial_Nb, sizeof(pTlg_dat->TLG_Serial_Nb) );
				tmp_lg += sizeof(pTlg_dat->TLG_Serial_Nb) -1;
				break;
			case T_TLG_Appl_Software_Ver:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_TLG_Appl_Software_Ver;
				tmp_lg++;
				buffer[tmp_lg] =12;
				tmp_lg++;
				memcpy( &buffer[tmp_lg], pTlg_dat->TLG_Appl_Software_Ver, sizeof(pTlg_dat->TLG_Appl_Software_Ver) );
				tmp_lg += sizeof(pTlg_dat->TLG_Appl_Software_Ver) -1;
				break;
			case T_IFSF_Protocol_Ver:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_IFSF_Protocol_Ver;
				tmp_lg++;
				buffer[tmp_lg] =12;
				tmp_lg++;
				memcpy( &buffer[tmp_lg], pTlg_dat->IFSF_Protocol_Ver, sizeof(pTlg_dat->IFSF_Protocol_Ver) );
				tmp_lg += sizeof(pTlg_dat->IFSF_Protocol_Ver) -1;
				break;
			case T_Current_Date:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Current_Date;
				tmp_lg++;
				buffer[tmp_lg] =4;
				tmp_lg++;
				buffer[tmp_lg] = pTlg_dat->Current_Date[0];
				tmp_lg++;
				buffer[tmp_lg] = pTlg_dat->Current_Date[1];
				tmp_lg++;
				buffer[tmp_lg] = pTlg_dat->Current_Date[2];
				tmp_lg++;
				buffer[tmp_lg] = pTlg_dat->Current_Date[3];
				break;
			case T_Current_Time:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Current_Time;
				tmp_lg++;
				buffer[tmp_lg] =3;
				tmp_lg++;
				buffer[tmp_lg] = pTlg_dat->Current_Time[0];
				tmp_lg++;
				buffer[tmp_lg] = pTlg_dat->Current_Time[1];
				tmp_lg++;
				buffer[tmp_lg] = pTlg_dat->Current_Time[2];
				break;
			case T_SW_Checksum:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_SW_Checksum;
				tmp_lg++;
				buffer[tmp_lg] =4;
				tmp_lg++;
				buffer[tmp_lg] = pTlg_dat->SW_Checksum[0];
				tmp_lg++;
				buffer[tmp_lg] = pTlg_dat->SW_Checksum[1];
				tmp_lg++;
				buffer[tmp_lg] = pTlg_dat->SW_Checksum[2];
				tmp_lg++;
				buffer[tmp_lg] = pTlg_dat->SW_Checksum[3];
				break;
			default:
				tmp_lg++;
				buffer[tmp_lg] = recvBuffer[i + 10] ;
				tmp_lg++;
				buffer[tmp_lg] ='\0';
				break;
		}
	}

	tmp_lg++;
	buffer[6] = (unsigned char )( (tmp_lg-8) / 256) ;
	buffer[7] = (unsigned char )( (tmp_lg-8) %256 );  //big end.
	*msg_lg= tmp_lg;
	memcpy(sendBuffer, buffer, tmp_lg);

	return 0;
}

//-
//дTLG_DATA����. �������Ϣ���ݵ�����Ϊ111Ӧ��.
//msg_lgΪ���ɵ��ܳ���,Ϊ0��ʾ����ҪӦ����Ϣ,sendBuffer��msg_lg����ҪӦ���������ΪNULL,.
//-
int WriteTLG_DAT(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg){
	int i, recv_lg, tmp_lg = 0, lg;	
	int tp_stat, ret;
	TLG_DATA *pTlg_data;
	TLG_DAT *pTlg_dat;
	//TP_DAT  *pTp_dat;
	unsigned char tmpMSAck, tmpDataAck;
	unsigned char node;
	unsigned char buffer[256];
	
	if( NULL == gpIfsf_shm ){		
		printf("�����ڴ�ֵ����,Һλ��WriteTLG_DATʧ��");
		return -1;
	}
	if( NULL == recvBuffer   ){		
		printf("��������,Һλ��WriteTLG_DAT ʧ��");
		return -1;
	}
	
	node= recvBuffer[1];
	pTlg_data = (TLG_DATA*)( &(gpIfsf_shm->tlg_data) );
	pTlg_dat = (TLG_DAT*)( &(pTlg_data->tlg_dat) );
	tp_stat = 2;
	for(i=0;i< MAX_TP_PER_TLG;i++){
		if(   (0 == pTlg_data->tp_dat[i].TP_Status ) ||(2 == pTlg_data->tp_dat[i].TP_Status )  )
			continue;
		if(  3 == pTlg_data->tp_dat[i].TP_Status  )
			tp_stat = 3;
	}
	
	bzero(buffer, sizeof(buffer));
	if(  '\x09' != recvBuffer[0]  ){
		printf("��������,����Һλ�ǵ�����,Һλ��WriteTLG_DAT ʧ��");
		return -1;
	}
	
	if( (node <= 0) || (node > 127 ) ){//||(node != gpIfsf_shm->node_tlg) 
		printf("Һλ�ǽڵ�ֵ����,Һλ��WriteTLG_DAT ʧ��");
		return -1;
	}
	
	
	
	buffer[0] = recvBuffer[2]; //LNAR
	buffer[1] = recvBuffer[3]; //LNAR
	buffer[2] = recvBuffer[0]; //LNAO
	buffer[3] = recvBuffer[1]; //LNAO
	buffer[4] = '\0'; //recvBuffer[4]; //IFSF_MC
	buffer[5] = ( (recvBuffer[5] &0x1f) |0xE0); //M_St
	recv_lg = recvBuffer[6];  //M_Lg, big end mode.
	recv_lg = recv_lg * 256 + recvBuffer[7]; 
	buffer[8] = '\x01'; //recvBuffer[8]; //Data_Ad_Lg
	buffer[9] = '\x01'; //recvBuffer[9]; //Data_ad:C_DAt.
	//buffer[10] = ; //MS_Ack
	//buffer[11] = recvBuffer[10]; 
	// recvBuffer[10]:Data_Id, 11:Data_Lg, 12:Data_El.  Data begin here.
	tmp_lg = 10;
	i=0;
	tmpMSAck = '\0'; // all is  ok.
	while( i < (recv_lg -2) ) {
		tmpDataAck = '\0'; //ok ack.
		lg =  (int )recvBuffer[11+i]; //Data_Lg.
		switch( recvBuffer[10+i] ) {		
			case T_Nb_Tanks:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Nb_Tanks;
				tmpDataAck = '\x02';  //read only
				tmpMSAck = '\x05';			
				tmp_lg++;
				buffer[tmp_lg] =tmpDataAck;
				i = i + 2 + lg;
				break;
			case T_Reference_Temp: 
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Reference_Temp;
				memcpy(pTlg_dat->Reference_Temp, &recvBuffer[12+i], lg);
				tmp_lg++;
				buffer[tmp_lg] =tmpDataAck;
				i = i + 2 + lg;
				break;
			case T_TLG_Measurement_Unit:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_TLG_Measurement_Unit;
				tmpDataAck = '\x02';  //read only
				tmpMSAck = '\x05';			
				tmp_lg++;
				buffer[tmp_lg] =tmpDataAck;
				i = i + 2 + lg;
				break;
			case T_Country_Code:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Country_Code;
				tmpDataAck = '\x02';  //read only
				tmpMSAck = '\x05';			
				tmp_lg++;
				buffer[tmp_lg] =tmpDataAck;
				i = i + 2 + lg;
				break;
			case T_Maint_Password:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Maint_Password;
				 if(gpIfsf_shm->tlg_data.maint_enable){
					memcpy(pTlg_dat->Maint_Password, &recvBuffer[12+i], lg);
					tmpDataAck = 0;
					}
				else if(memcmp(pTlg_dat->Maint_Password, &recvBuffer[12+i], lg) == 0){
					gpIfsf_shm->tlg_data.maint_enable = 1;
					signal(SIGALRM,MaintAlarmDisable);
					alarm(300);
					tmpDataAck = 0;
					}
				else{
					tmpDataAck = '\x06';  //Command not accepted.
					tmpMSAck = '\x05';
					}
				tmp_lg++;
				buffer[tmp_lg] =tmpDataAck;
				i = i + 2 + lg;
				break;
			case T_TLG_Manufacturer_Id:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_TLG_Manufacturer_Id;
				tmpDataAck = '\x02';  //read only
				tmpMSAck = '\x05';			
				tmp_lg++;
				buffer[tmp_lg] =tmpDataAck;
				i = i + 2 + lg;
				break;
			case T_TLG_Model:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_TLG_Model;
				tmpDataAck = '\x02';  //read only
				tmpMSAck = '\x05';			
				tmp_lg++;
				buffer[tmp_lg] =tmpDataAck;
				i = i + 2 + lg;
				break;
			case T_TLG_Type:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_TLG_Type;
				tmpDataAck = '\x02';  //read only
				tmpMSAck = '\x05';			
				tmp_lg++;
				buffer[tmp_lg] =tmpDataAck;
				i = i + 2 + lg;
				break;
			case T_TLG_Serial_Nb:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_TLG_Serial_Nb;
				tmpDataAck = '\x02';  //read only
				tmpMSAck = '\x05';			
				tmp_lg++;
				buffer[tmp_lg] =tmpDataAck;
				i = i + 2 + lg;
				break;
			case T_TLG_Appl_Software_Ver:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_TLG_Appl_Software_Ver;
				tmpDataAck = '\x02';  //read only
				tmpMSAck = '\x05';			
				tmp_lg++;
				buffer[tmp_lg] =tmpDataAck;
				i = i + 2 + lg;
				break;
			case T_IFSF_Protocol_Ver:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_IFSF_Protocol_Ver;
				tmpDataAck = '\x02';  //read only
				tmpMSAck = '\x05';			
				tmp_lg++;
				buffer[tmp_lg] =tmpDataAck;
				i = i + 2 + lg;
				break;
			case T_Current_Date:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Current_Date;
				memcpy(pTlg_dat->Current_Date, &recvBuffer[12+i], lg);
				tmp_lg++;
				buffer[tmp_lg] =tmpDataAck;
				i = i + 2 + lg;
				break;			
			case T_Current_Time:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Current_Time;
				memcpy(pTlg_dat->Current_Time, &recvBuffer[12+i], lg);
				tmp_lg++;
				buffer[tmp_lg] =tmpDataAck;
				i = i + 2 + lg;
				break;
			case T_SW_Checksum:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_SW_Checksum;
				tmpDataAck = '\x02';  //read only
				tmpMSAck = '\x05';			
				tmp_lg++;
				buffer[tmp_lg] =tmpDataAck;
				i = i + 2 + lg;
				break;
			case T_Enter_Maint_Mode://command
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Enter_Maint_Mode; 
				if(gpIfsf_shm->tlg_data.maint_enable){
					for(i=0;i<MAX_TP_PER_TLG;i++){
						if( (2 == gpIfsf_shm->tlg_data.tp_dat[i].TP_Status) ||
							       	(1 == gpIfsf_shm->tlg_data.tp_dat[i].TP_Status) ){						
							ret = ChangeTpStatAlarm( gpIfsf_shm->tlg_data.tlg_node, 
												0x21+i, 3, NULL);
							if(ret <0){ 
							printf("ChangeTpStatAlarm error in translate Enter_Maint_Mode command");
							}
						}
					}
					tmpDataAck = 0;
				} else{
					tmpDataAck = '\x06';  //Command not accepted.
					tmpMSAck = '\x05';
				}
				tmp_lg++;
				buffer[tmp_lg] =tmpDataAck;
				i = i + 2 + lg;
				break;
			case T_Exit_Maint_Mode://command
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Exit_Maint_Mode; 
				
				for(i=0;i<MAX_TP_PER_TLG;i++){
					if(3 == gpIfsf_shm->tlg_data.tp_dat[i].TP_Status){						
						ret = ChangeTpStatAlarm( gpIfsf_shm->tlg_data.tlg_node, 0x21+i, 2, NULL);
						if(ret <0){ 
							printf("ChangeTpStatAlarm error in translate Exit_Maint_Mode command");
						}
					}
				}
				tmpDataAck = 0;
				
				tmp_lg++;
				buffer[tmp_lg] =tmpDataAck;
				i = i + 2 + lg;
				break;			 
			default:
				tmp_lg++;
				buffer[tmp_lg] =recvBuffer[i+10] ;
				tmpDataAck = '\x04'; //not exist
				tmpMSAck = '\x05';
				tmp_lg++;
				buffer[tmp_lg] ='\0';
				break;
		}
	}

	tmp_lg++;
	buffer[6] = (unsigned char )( (tmp_lg-8) / 256) ;
	buffer[7] = (unsigned char )((tmp_lg-8) % 256);  //big end.
	buffer[10] = tmpMSAck;
	if(NULL != msg_lg) {
		*msg_lg= tmp_lg;
	}
	if(NULL != sendBuffer) {
		memcpy(sendBuffer, buffer, tmp_lg);
	}
	return 0;
}
//-
//ֻдTLG_DATA�ĵ����������,�����豸�Լ���д���߳�ʼ������.
//data(Data_Id+Data_Lg+Data_El), �ɹ�0,ʧ��-1����Data_Ackֵ.
//-
int SetSglTLG_DAT(const char node, const unsigned char *data){
	int  lg;//i, recv_lg, tmp_lg = 0,
	TLG_DATA *pTlg_data;
	TLG_DAT *pTlg_dat;
	//TP_DAT  *pTp_dat;
	unsigned char  tmpDataAck;//tmpMSAck,
	//unsigned char node;
	unsigned char buffer[256];
	
	if( NULL == gpIfsf_shm ){		
		printf("�����ڴ�ֵ����,Һλ��WriteTLG_DATʧ��");
		return -1;
	}
	if( (NULL == data) || (0 == node)  ){		
		printf("��������,Һλ��SetSglTLG_DAT ʧ��");
		return -1;
	}
	
	//node= recvBuffer[1];
	pTlg_data = (TLG_DATA*)( &(gpIfsf_shm->tlg_data) );
	pTlg_dat = (TLG_DAT*)( &(pTlg_data->tlg_dat) );
	
	
	//bzero(buffer, sizeof(buffer));
	
	
	if( (node <= 0) || (node > 127 ) ){// ||(node != gpIfsf_shm->node_tlg)
		printf("Һλ�ǽڵ�ֵ����,Һλ��SetSglTLG_DAT ʧ��");
		return -1;
	}
	
	
	
	//while( i < (recv_lg -2) )
	//{
	tmpDataAck = '\0'; //ok ack.
	lg =  (int )data[1]; //Data_Lg.
	switch( data[0] )
	{
		case T_Nb_Tanks:
			pTlg_dat->Nb_Tanks = data[2];
			break;
		case T_Reference_Temp: 
			memcpy(pTlg_dat->Reference_Temp, &data[2], sizeof(pTlg_dat->Reference_Temp));
			break;
		case T_TLG_Measurement_Unit:
			tmpDataAck = '\x02';  //read only
			break;
		case T_Country_Code:
			tmpDataAck = '\x02';  //read only
			break;
		case T_Maint_Password:
			memcpy(pTlg_dat->Maint_Password, &data[2], sizeof(pTlg_dat->Maint_Password));
			break;
		case T_TLG_Manufacturer_Id:
			memcpy(pTlg_dat->TLG_Manufacturer_Id, &data[2], sizeof(pTlg_dat->TLG_Manufacturer_Id));
			break;
		case T_TLG_Model:
			memcpy(pTlg_dat->TLG_Model, &data[2], sizeof(pTlg_dat->TLG_Model));
			break;
		case T_TLG_Type:
			memcpy(pTlg_dat->TLG_Type, &data[2], sizeof(pTlg_dat->TLG_Type));
			break;
		case T_TLG_Serial_Nb:
			memcpy(pTlg_dat->TLG_Serial_Nb, &data[2], sizeof(pTlg_dat->TLG_Serial_Nb));
			break;
		case T_TLG_Appl_Software_Ver:
			tmpDataAck = '\x02'; //not exist
			break;
		case T_IFSF_Protocol_Ver:
			tmpDataAck = '\x02';  //read only
			break;
		case T_Current_Date:
			memcpy(pTlg_dat->Current_Date, &data[2], sizeof(pTlg_dat->Current_Date));
			break;			
		case T_Current_Time:			
			memcpy(pTlg_dat->Current_Time, &data[2], sizeof(pTlg_dat->Current_Time));
			break;
		case T_SW_Checksum:
			memcpy(pTlg_dat->SW_Checksum, &data[2], sizeof(pTlg_dat->SW_Checksum));
			break;
		default:
			tmpDataAck = '\x04'; //not exist
			break;
	}
	//}
	
	return 0;
}

/*******************************************************************
�����������
-------------------------------------------------------------------
2007-11-09 by chenfm
*******************************************************************/


//extern IFSF_SHM *gpIfsf_shm; //�ýṹ��ȫ�ֵ�����ֻ��һ��,��ifsf_main.c�ж���.

//TLG_ER_DATA��������. nodeҺλ���߼��ڵ��,tp_adҺλ�ǵ�ַ21h-3fh,
// Ҫ�õ�gpIfsf_shm,ע��P,V����������.
//�ı�������.  Ҫ����MakeErMsg����.er_ad�����ַ01h-40h
int ChangeTLGER_DAT(const char node, const char er_ad){
	int i, tp_status;
	//int tp=tp_ad-0x21; 
	int er=er_ad-1; //0,1,2,3,4,5,6,..63�Ŵ���.
	TLG_DATA *pTlg_data;
	TLG_ER_DAT *pTlg_er_data;	

	char *err[64] = {
		"RAM defect", "ROM defect",	"Configuration or Parameter Error", "Power supply out of order",
		"Main Communication error", "", " ",	"",
		"", "", "",	"",
		"", "",	" ",	"",
		"", "",	"",	"",
		"",   "",   "",  "",
		"",   "",   "",  "",
		"",   "",   "",  "Battery error ",
		"Communication error",   "",   "",  "",
		"",   " ",   "",  " ",
		"  ",   "",   "",  "",
		"",   "",     "",  "",
		"",   "",     "",  " ",
		"",   "",   "",  "",
		"",   "",   "",  "",
		"",   "",   "",  "",
		};
	int data_lg;
	unsigned char sendBuffer[128];
	
	bzero(sendBuffer, sizeof(sendBuffer));
	if( (node <= 0) || (node > 127 ) ){
		printf("Һλ�ǽڵ�ֵ����,���´������� ʧ��");
		return -1;
		}
	if( NULL == gpIfsf_shm ){		
		printf("�����ڴ�ֵ����,���´������� ʧ��");
		return -1;
	}
	
	pTlg_data = (TLG_DATA *)( &(gpIfsf_shm->tlg_data) );
	pTlg_er_data =  (TLG_ER_DAT *)( &(pTlg_data->tlg_er_dat)  );
	tp_status = 0;
	for(i=0;i<MAX_TP_PER_TLG;i++){
		if(tp_status < pTlg_data->tp_dat[i].TP_Status)
			tp_status = pTlg_data->tp_dat[i].TP_Status;
	}
	if( (tp_status > 3) ||(tp_status <= 0) )
		return -1;	
	pTlg_er_data->TLG_Error_Type = er_ad;
	strncpy(pTlg_er_data->TLG_Err_Description, err[er], strlen(err[er])  );
	pTlg_er_data->TLG_Error_Total += 1;
	
	
	//usr i to ret
	i = MakeTLGErMsg( node, sendBuffer, &data_lg);
	if(i<0){
		printf("MakeTLGErMsg error.");
		return -1;
		}
	i = SendData2CD(sendBuffer, data_lg, 5);
	if(i<0){
		printf("SendData2CD error.");
		return -1;
		}
	return 0;
}

int DefaultTLGER_DAT(const char node){
	int i, tp_status;
	//int er=er_ad-1; //0,1,2,3,4,5,6,..63�Ŵ���.
	TLG_DATA *pTlg_data;
	TLG_ER_DAT *pTlg_er_dat;	
	
	if( (node <= 0) || (node > 127 ) ){
		UserLog("���ͻ��ڵ�ֵ����,���´����ʼ�� ʧ��");
		return -1;
		}
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,���´����ʼ��ʧ��");
		return -1;
	}
	pTlg_data = (TLG_DATA *)( &(gpIfsf_shm->tlg_data) );
	pTlg_er_dat =  (TLG_ER_DAT *)( &(pTlg_data->tlg_er_dat)  );
	tp_status = 0;
	for(i=0;i<MAX_TP_PER_TLG;i++){
		if(tp_status < pTlg_data->tp_dat[i].TP_Status)
			tp_status = pTlg_data->tp_dat[i].TP_Status;
	}
	if( (tp_status > 3) ||(tp_status < 0) ) {
		return -1;	
	}
	pTlg_er_dat->TLG_Error_Type = '\0';
	memset(pTlg_er_dat->TLG_Err_Description, 0,sizeof(pTlg_er_dat->TLG_Err_Description));
	pTlg_er_dat->TLG_Error_Total  =0;
	
	
	return 0;
}
//��ȡTLG_ER_DAT. �������Ϣ���ݵ�����Ϊ001�ش�.
//recvBufferΪ�յ���data,sendBufferΪҪ���͵�data,msg_lgΪ���ɵ��ܳ���.
int ReadTLGER_DAT(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg){
	int i,j,k, tp_status, tmp_lg=0, recv_lg;
	//int fp = recvBuffer[9] - 0x21; //0,1,2,3�ż��͵�.
	int er=recvBuffer[11] - 1; //0,1,2,3,4,5,6,..63�Ŵ���.
	unsigned char node = recvBuffer[1];
	TLG_DATA *pTlg_data;
	TLG_ER_DAT *pTlg_er_dat;	
	unsigned char buffer[128];
	
	bzero(buffer, sizeof(buffer));		
	if( (node <= 0) || (node > 127 ) ){
		printf("Һλ�ǽڵ�ֵ����,���´���Read ʧ��");
		return -1;
		}
	if( NULL == gpIfsf_shm ){		
		printf("�����ڴ�ֵ����,���´���Read ʧ��");
		return -1;
	}
	
	pTlg_data = (TLG_DATA *)( &(gpIfsf_shm->tlg_data) );
	pTlg_er_dat =  (TLG_ER_DAT *)( &(pTlg_data->tlg_er_dat)  );
	tp_status = 0;
	for(i=0;i<MAX_TP_PER_TLG;i++){
		if(tp_status < pTlg_data->tp_dat[i].TP_Status)
			tp_status = pTlg_data->tp_dat[i].TP_Status;
	}
	if( (tp_status > 3) ||(tp_status < 0) )
		return -1;	
	
	buffer[0] = recvBuffer[2];
	buffer[1] = recvBuffer[3];
	buffer[2] = recvBuffer[0];
	buffer[3] = recvBuffer[1];
	buffer[4] = '\0'; //recvBuffer[4]; IFSF_MC
	buffer[5] = ( (recvBuffer[5] &0x1f) |0x20);//answer.
	recv_lg = recvBuffer[6];
	recv_lg = recv_lg * 256 + recvBuffer[7];
	buffer[8] = '\x03'; //recvBuffer[8]; //Ad_lg
	buffer[9] = recvBuffer[9];  //tl_ad
	buffer[10] = recvBuffer[10]; //ad
	buffer[11] = pTlg_er_dat->TLG_Error_Type; //ad
	//buffer[12] = recvBuffer[12]; //It's Data_Id. Data begin.	
	tmp_lg=11;
	for(i=0;i<recv_lg - 4; i++){
		switch(recvBuffer[12+i]) {			
			case TLG_Error_Type :
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)TLG_Error_Type;
				tmp_lg++;
				buffer[tmp_lg] = '\x01';
				tmp_lg++;
				buffer[tmp_lg] = pTlg_er_dat->TLG_Error_Type;
				break;			
			case TLG_Err_Description :
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)TLG_Err_Description;
				tmp_lg++;
				buffer[tmp_lg] = '\x14';
				k= buffer[tmp_lg];
				for(j=0;j<k;j++){
					tmp_lg++;
					buffer[tmp_lg] = pTlg_er_dat->TLG_Err_Description[j];
					}
				break;
			case TLG_Error_Total:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)TLG_Error_Total;
				tmp_lg++;
				buffer[tmp_lg] = '\x01';
				tmp_lg++;
				buffer[tmp_lg] = pTlg_er_dat->TLG_Error_Total;
				break;
			/*	
			case TLG_Error_State:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)TLG_Error_State;
				tmp_lg++;
				buffer[tmp_lg] = '\x01';					
				tmp_lg++;
				buffer[tmp_lg] = pTlg_er_dat->TLG_Error_State;				
				break;	
			*/
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

//������������TP_Error_Type_Mes��ֵ������,sendbufferΪ�����͵�����,data_lg�����ܳ���
static int MakeTLGErMsg(const char node, unsigned char *sendBuffer, int *data_lg){
	int i,k,tp_status,tmp_lg=0; //recv_lg;
	//int tp=tp_ad-0x21; 
	//int er; //0,1,2,3,4,5,6,..63�Ŵ���.
	TLG_DATA *pTlg_data;
	TLG_ER_DAT *pTlg_er_dat;	
	unsigned char buffer[64];
	
	bzero(buffer, sizeof(buffer));
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
	
	pTlg_data = (TLG_DATA *)( &(gpIfsf_shm->tlg_data) );
	pTlg_er_dat =  (TLG_ER_DAT *)( &(pTlg_data->tlg_er_dat)  );
	tp_status = 0;
	for(i=0;i<MAX_TP_PER_TLG;i++){
		if(tp_status < pTlg_data->tp_dat[i].TP_Status)
			tp_status = pTlg_data->tp_dat[i].TP_Status;
	}
	if( (tp_status > 3) ||(tp_status < 0) )
		return -1;	
	
	buffer[0] = '\x02';
	buffer[1] = '\x01';
	buffer[2] = '\x09';
	buffer[3] = node;
	buffer[4] = '\0'; //recvBuffer[4]; IFSF_MC
	buffer[5] = '\x80';//unsolicited data..
	//recv_lg = recvBuffer[6];
	//recv_lg = recv_lg * 256 + recvBuffer[7];
	buffer[8] = '\x03'; //recvBuffer[8]; //Ad_lg
	buffer[9] = 0x01;  //tlg_ad
	buffer[10] ='\x41'; //ad
	buffer[11] = pTlg_er_dat->TLG_Error_Type; //ad
	//buffer[12] = recvBuffer[12]; //It's Data_Id. Data begin.	
	tmp_lg=11;

	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)TLG_Error_Type_Mes;
	tmp_lg++;
	buffer[tmp_lg] = '\0';
	
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)TLG_Error_Type;
	tmp_lg++;
	buffer[tmp_lg] = '\x01';
	tmp_lg++;
	buffer[tmp_lg] = pTlg_er_dat->TLG_Error_Type;
	
	/*	
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)TLG_Error_State;
	tmp_lg++;
	buffer[tmp_lg] = '\x01';	
	tmp_lg++;
	buffer[tmp_lg] = pTlg_data->TLG_Error_State;
	*/			
				
	tmp_lg++;
	buffer[6] = (unsigned char )(tmp_lg / 256) ;
	buffer[7] = (unsigned char )(tmp_lg %256);  //big end.
	*data_lg= tmp_lg;
	memcpy(sendBuffer, buffer, tmp_lg);	
	return 0;
}



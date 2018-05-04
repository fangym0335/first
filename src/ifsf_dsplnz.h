/*******************************************************************
1.�ṹ��FP_XXX_DATA��ö������FP_XXX_DAT_AD������IFSF���������ݼ����ַ.
2. ��.c�ļ�Ҫ��#incude "ifsf_fp.h",�˴���������߼��͵�ǹ���ݽṹ�Ͳ�������
-------------------------------------------------------------------
2007-07-13 by chenfm
*******************************************************************/
#ifndef __IFSF_FPLOZ_H__
#define __IFSF_FPLOZ_H__
#include "ifsf_def.h"
#include "ifsf_dspad.h"


/*--------------------------------------------------------------*/
//��Ҫ���ݽṹ--------------
//�������ݼ����ַ:(&��/��������_����)
//�߼���ǹ.��IFSF 3-01,V2.44,115-121. ��ַ11h-18h.
//��FP�е�Log_Noz_State(21λ״̬),Log_Noz_Mask(25λ��Ȩ),Current_Log_Noz(37λ��ǰ��ǹ)
enum FP_LN_DAT_AD{LN_PR_Id = 1,LN_Hose_Expansion_Vol = 3, LN_Slow_Flow_Valve_Activ,	
	LN_Physical_Noz_Id = 5,	LN_Meter_1_Id = 7, LN_Meter_1_Blend_Ratio,  
	LN_Meter_2_Id, 	LN_Logical_Nozzle_Type, 
	LN_Preset_Valve_Activation = 11,
	LN_Log_Noz_Vol_Total = 20, LN_Log_Noz_Amount_Total, LN_No_TR_Total,
	LN_Log_Noz_SA_Vol_Total = 30, LN_Log_Noz_SA_Amount_Total, LN_No_TR_SA_Total, LN_All_Log_Noz_Vol_Total = 208
	};//gFp_ln_dat_ad;
typedef struct{
	/*CONFIGURATION:*/
	unsigned char PR_Id;  /*1-8,��Ӧ��ַ41h-48h*/   unsigned char Physical_Noz_Id;/*O*/ 
	unsigned char Meter_1_Id;                       
	 //unsigned char Meter_1_Blend_Ratio;/*O*/        	unsigned char Meter_2_Id;/*O*/ 
	 //unsigned char Logical_Nozzle_Type;/*O*/         unsigned char Hose_Expansion_Vol;/*O*/ 
	 //unsigned char Slow_Flow_Valve_Activ;/*O*/     	unsigned char  Preset_Valve_Activation;/*O*/
	/*PERMANENT TOTALS:*/
  	unsigned char Log_Noz_Vol_Total[7];
	unsigned char Log_Noz_Amount_Total[7];          unsigned char No_TR_Total[7];
	/*STAND ALONE TOTALS:*/
	unsigned char Log_Noz_SA_Vol_Total[7];/*O*/ 
	unsigned char Log_Noz_SA_Amount_Total[7];/*O*/  unsigned char  No_TR_SA_Total[7];/*O*/  
	}FP_LN_DATA;
//FP_LN_DATA��������. node���ͻ��߼��ڵ��,fp_ad���͵��ַ21h-24h,ln_ad�߼���ǹ��ַ11h-18h,
//Ҫ�õ�gpIfsf_shm,ע��P,V����������.
int DefaultLN_DATA(const char node, const char fp_ad, const char ln_ad);
//��ȡFP_LN_DATA. �������Ϣ���ݵ�����Ϊ001�ش�.
//recvBufferΪ�յ���data,sendBufferΪҪ���͵�data,msg_lgΪ���ɵ��ܳ���.
int ReadLN_DATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg);
//дFP_LN_DATA����.recvBufferΪ�յ���data,sendBufferΪҪ�ظ���data,�������Ϣ���ݵ�����Ϊ111Ӧ��
//msg_lgΪ���ɵ��ܳ���,Ϊ0��ʾ����ҪӦ����Ϣ,sendBuffer��msg_lg����ҪӦ����Ϣ��������ΪNULL.
int WriteLN_DATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg);
//���õ���������,�����豸�����д��������߳�ʼ������.
//�ɹ�����0,ʧ�ܷ���-1����Data_Ack. data����Ϊ:Data_Id+Data_Lg+Data_El.
int SetSglLN_DATA(const char node, const char fp_ad, const char ln_ad, const unsigned char *data);
int RequestLN_DATA(const char node, const char fp_ad, const char ln_ad);
//һ�ʽ�������,LNZҪ�ı������. volum��������,amount���׽��.price���׵���
//int EndTr4Ln(const char node, const char fp_ad,  const char ln_ad,   const unsigned char * volume, const unsigned char *   amount,  const unsigned char *   price);
int EndTr4Ln(const char node, const char fp_ad,  const char ln_ad);

/*-----------------------------------------------------------------*/
#endif

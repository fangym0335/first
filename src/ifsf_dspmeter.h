/*******************************************************************
1.�ṹ��FP_XXX_DATA��ö������FP_XXX_DAT_AD������IFSF���������ݼ����ַ.
2. ��.c�ļ�Ҫ��#incude "ifsf_fp.h",�˴�������Ǽ����������ݽṹ�Ͳ�������
-------------------------------------------------------------------
2007-07-13 by chenfm
*******************************************************************/
#ifndef __IFSF_FPMETER_H__
#define __IFSF_FPMETER_H__
#include "ifsf_def.h"
#include "ifsf_dspad.h"


/*--------------------------------------------------------------*/
//��Ҫ���ݽṹ--------------
//�������ݼ����ַ:(&��/��������_����)
//������,��IFSF 3-01,V2.44,85-88. ��ַ81h-90h. C_DAT��Nb_Meters��־���ٸ�.
enum FP_M_DAT_AD{M_Meter_Type = 1,  M_Meter_Puls_Vol_Fact = 2 ,
	M_Meter_Calib_Fact ,  M_PR_Id,  M_Meter_Total = 20  };//gFp_m_dat_ad;
typedef struct{
	/*CONFIGURATION:*/
	unsigned char Meter_Type; /*O*/
	unsigned char Meter_Puls_Vol_Fact;
	unsigned char Meter_Calib_Fact[2];/*O*/
	unsigned char PR_Id;
	/*TOTAL:*/
	unsigned char Meter_Total[7];
	}FP_M_DATA;
//FP_M_DATA��������,node���ͻ��߼��ڵ��,M_Ad�������ַ81h-90h,Ҫ�õ�gpIfsf_shm,ע��P,V����������.
int DefaultM_DAT(const char node,  const char m_ad);//Ĭ�ϳ�ʼ��. 
int SetM_DAT(const  char node,  const char m_ad, const FP_M_DATA *fp_m_data);//����FP_M_DATA.  
int CalcMeterTotal(const  char node, const unsigned char m_ad, const unsigned long volume);//�����ܼ�����. 
//��ȡFP_M_DATA FP_M_DATA��ַΪ81h-90h.�������Ϣ���ݵ�����Ϊ001�ش�
//recvBufferΪ�յ���data,sendBufferΪҪ���͵�data,msg_lgΪ���ɵ��ܳ���.
int ReadM_DATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg);
//дFP_M_DATA����.recvBufferΪ�յ���data,sendBufferΪҪ�ظ���data,�������Ϣ���ݵ�����Ϊ111Ӧ��
//msg_lgΪ���ɵ��ܳ���,Ϊ0��ʾ����ҪӦ����Ϣ,sendBuffer��msg_lg����ҪӦ����Ϣ��������ΪNULL.
int WriteM_DATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg);
int SetSglM_DAT(const  char node,  const char m_ad, const unsigned char *data);//MeterTotal, give volume then add by self.
int RequestM_DAT(const  char node,  const char m_ad);

/*-----------------------------------------------------------------*/
#endif

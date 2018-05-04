/*******************************************************************
1.�ṹ��FP_XXX_DATA��ö������FP_XXX_DAT_AD������IFSF���������ݼ����ַ.�˴��������.
2. ��.c�ļ�Ҫ��#incude "ifsf_fp.h"
-------------------------------------------------------------------
2007-07-04 by chenfm
2008-03-09 - Guojie Zhu <zhugj@css-intelligent.com>
    FP_ER_DATA_AD.FP_Err_r_State ��ֵΪ 5 (ԭΪ4)
*******************************************************************/
#ifndef __IFSF_FPER_H__
#define __IFSF_FPER_H__
#include "ifsf_def.h"
#include "ifsf_dspad.h"


/*--------------------------------------------------------------*/
//��Ҫ���ݽṹ--------------
//�������ݼ����ַ:(&��/��������_����)
//��������,��IFSF 3-01,129-133,  ER_DAT��ַΪ41h,FP_Error_Type����ER_ID�ĵ�ַ01h-40h
enum FP_ER_DAT_AD{FP_Error_Type = 1, FP_Err_Description, FP_Error_Total = 3,
	FP_Error_State = 5, FP_Error_Type_Mes = 100 };//gFp_er_dat_ad;
typedef struct{
	/*ERROR DATA:*/
	unsigned char FP_Error_Type;    	         unsigned char FP_Err_Description[20];/*O*/  
	unsigned char FP_Error_Total;    	         unsigned char FP_Error_State;
	/*UNSOLICITED DATA:�ú���MakeErMsg��ʵ��MakeErMsgSend*/
	//unsigned char FP_Error_Type_Mes[2];/*see IFSF 3-01,130*/
	}FP_ER_DATA;
//FP_ER_DATA��������. node���ͻ��߼��ڵ��,fp_ad���͵��ַ21h-24h,Ҫ�õ�gpIfsf_shm,ע��P,V����������.
//�ı�������.  Ҫ����MakeErMsg����.
//
int DefaultER_DATA(const char node, const char fp_ad);
int ChangeER_DATA(const __u8 node, const __u8 fp_ad, const __u8 er_ad, const __u8 *msgbuffer);
//��ȡFP_ER_DATA. �������Ϣ���ݵ�����Ϊ001�ش�.
//recvBufferΪ�յ���data,sendBufferΪҪ���͵�data,msg_lgΪ���ɵ��ܳ���.
int ReadER_DATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg);
//дFP_ER_DATA����.recvBufferΪ�յ���data,sendBufferΪҪ�ظ���data,�������Ϣ���ݵ�����Ϊ111Ӧ��.
//msg_lgΪ���ɵ��ܳ���,Ϊ0��ʾ����ҪӦ����Ϣ,sendBuffer��msg_lg����ҪӦ����Ϣ��������ΪNULL.
//int WriteER_DATA(const char node, const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg);
//int SetSglER_DATA(const char node, const char fp_ad, const unsigned char  *data);

//������������FP_Error_Type_Mes��ֵ������,sendbufferΪ�����͵�����,data_lg�����ܳ���
static int MakeErMsg(const char node, const char fp_ad, unsigned char *sendBuffer, int *data_lg);
//������������TFP_Error_Type_Mes��ֵ������.��������ĺ���,subnetOfCD��nodeOfCDΪCD��������ַ���߼��ڵ��.
static int MakeErMsgSend(const char node, const char fp_ad, const char subnetOfCD, \
		const char nodeOfCD, unsigned char *sendBuffer, int *msg_lg);

/*--------------------------------------------------------------*/
#endif

/*******************************************************************
1.�ṹ��FP_XXX_DATA��ö������FP_XXX_DAT_AD������IFSF���������ݼ����ַ.
2. ��.c�ļ�Ҫ��#incude "ifsf_fp.h",�˴���������й���Ʒ�����ݽṹ�Ͳ�������
-------------------------------------------------------------------
2007-07-13 by chenfm
*******************************************************************/
#ifndef __IFSF_FPPROD_H__
#define __IFSF_FPPROD_H__

#include "ifsf_def.h"
#include "ifsf_dspad.h"

//#define PROD_PRICE  "00001000"  //10.00yuan

/*--------------------------------------------------------------*/
//��Ҫ���ݽṹ--------------
//�������ݼ����ַ:(&��/��������_����)
//��Ʒ,��IFSF 3-01,V2.44,89-90. PR_Id��ַ41h~48h.��ַ��M_DAT��PR_Id��Ӧ.
enum FP_PR_DAT_AD{PR_Prod_Nb = 2, PR_Prod_Description ,  PR_Vap_Recover_Const =10
	};//gFp_pr_dat_ad;
typedef struct{	
	/*CONFIGURATION:*/
	unsigned char Prod_Nb[4];
	unsigned char Prod_Description[16];//O
	 //unsigned char Vap_Recover_Const;//O
}FP_PR_DATA;
//FP_PR_DATA��������,node���ͻ��߼��ڵ��,pr_ad��Ʒ��ַ,Ҫ�õ�gpIfsf_shm,ע��ʹ�÷��������ؼ���.
int DefaultPR_DAT(const  char  node, const unsigned char pr_ad);//����,���ݽڵ�ſ���ֱ��д.

//��ȡFP_PR_DATA. FP_PR_DATA��ַΪ41h-48h.�������Ϣ���ݵ�����Ϊ001�ش�
//recvBufferΪ�յ���data,sendBufferΪҪ���͵�data,msg_lgΪ���ɵ��ܳ���.
int ReadPR_DATA( const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg);
//дFP_PR_DATA����. recvBufferΪ�յ���data,sendBufferΪҪ�ظ���data,�������Ϣ���ݵ�����Ϊ111Ӧ��
//msg_lgΪ���ɵ��ܳ���,Ϊ0��ʾ����ҪӦ����Ϣ,sendBuffer��msg_lg����ҪӦ����Ϣ��������ΪNULL.
int WritePR_DATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg);
int SetSglPR_DATA(const char node, const unsigned char pr_ad, const unsigned char *data);
//����Prod_Nb[4]��ֵ.prod_nbΪ����ֵ.
int SetPR_DATA_Nb(const char node, const unsigned char pr_ad, const unsigned char *prod_nb);
int RequestPR_DATA(const char node, const unsigned char pr_ad);

//ÿ�ּ���ģʽ�µ���Ʒ. ��IFSF 3-01,V2.44,91-93, ��ַ:61h+00000001-99999999BCD+11h-18h.
enum FP_FM_DAT_AD{FM_Fuelling_Mode_Name = 1,  FM_Prod_Price = 2, FM_Max_Vol,
	FM_Max_Fill_Time, FM_Max_Auth_Time, FM_User_Max_Volume }; //gFp_fm_dat_ad;
typedef struct{
	//unsigned char Prod_Nb[4];  //Ϊ����PR_ID��Ӧ,PCD�����ӵ�.û�е�ַ��.
	/*CONFIGURATION:*/
	 //unsigned char Fuelling_Mode_Name[8]; //O
	unsigned char Prod_Price[4];
	unsigned char Max_Vol[5];
	unsigned char Max_Fill_Time;
	unsigned char Max_Auth_Time;
	unsigned char User_Max_Volume[5];	
}FP_FM_DATA; //��PCDֻ��ָ��8����Ʒ,ÿ����Ʒֻ��1�ּ���ģʽ.
//FP_FM_DATA��������. node���ͻ��߼��ڵ��,pr_nb��Ʒ���(1-99999999),,pr_ad��Ʒ��ַ41h-48h,
//fm_id����ģʽ��ַ11h(11h-18h),Ҫ�õ�gpIfsf_shm,ע��ʹ�÷��������ؼ���.
int DefaultFM_DAT(const char node, const unsigned char pr_ad);//Ĭ�ϳ�ʼ��
int SetFM_DAT(const char node, const unsigned char pr_ad, const FP_FM_DATA *fp_fm_data);//��ʼ��,ע����FP_PR_DATA��Ӧ!!
//��ȡFP_FM_DATA. FP_FM_DATA��ַΪ11h(11h-18h).�������Ϣ���ݵ�����Ϊ001�ش�
//recvBufferΪ�յ���data,sendBufferΪҪ���͵�data,msg_lgΪ���ɵ��ܳ���.
int ReadFM_DATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg);
//дFP_PR_DATA����.recvBufferΪ�յ���data,sendBufferΪҪ�ظ���data,�������Ϣ���ݵ�����Ϊ111Ӧ��
//msg_lgΪ���ɵ��ܳ���,Ϊ0��ʾ����ҪӦ����Ϣ,sendBuffer��msg_lg����ҪӦ����Ϣ��������ΪNULL.
int WriteFM_DATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg);
int SetSglFM_DAT(const char node, const unsigned char pr_ad, const unsigned char  *data);
int SetFM_DAT_Price(const char node, const unsigned char pr_ad, const unsigned char  *price);
int RequestFM_DAT(const char node, const unsigned char pr_ad); //fm is only 1,ad is 11h.

/*-----------------------------------------------------------------*/
#endif

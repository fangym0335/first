/*******************************************************************
1.�ṹ��FP_XXX_DATA��ö������FP_XXX_DAT_AD������IFSF���������ݼ����ַ.
2. ��.c�ļ�Ҫ��#incude "ifsf_fp.h",�˴�������ǽ��׵����ݽṹ�Ͳ�������
3. ����������������Ϊÿ�����ͻ�120(4*30)�����ݼ�¼�����ݿ�,�����ͻ�
��gpIfsf_shm��˳������.
-------------------------------------------------------------------
2007-07-04 by chenfm
2008-03-09 - Guojie Zhu <zhugj@css-intelligent.com>
    struct FP_TR_DATA ���ӳ�Ա offline_flag,���ڱ���ѻ�����
    FP_TR_DATA.TR_Date_Time ���Ϊ TR_Date & TR_Time
    FP_TR_DATA.TR_Date_Time ���Ϊ TR_Date & TR_Time
    Ԥ���彻��״̬ CLEARED/PAYABLE/LOCKED

2008-03-14 modify by liys ���������������ֱ���

1.����״̬ʱ:��buff�е����м�¼״̬��Ϊ����
2.����ת����ʱ:��buff�е����м�¼д�������ļ�,buff ���

�Ķ�����α��: modify by liys 2008-03-14
2008-05-21 - Guojie Zhu <zhugj@css-intelligent.com>
    struct FP_TR_DATA ����TR_Start_Total �� TR_End_Total  ���ڴ洢���׵���ֹ����

*******************************************************************/
#ifndef __IFSF_FPTR_H__
#define __IFSF_FPTR_H__
#include "ifsf_def.h"
#include "ifsf_dspad.h"
#include "ifsf_dsp.h"

#define MAX_TR_NB_PER_DISPENSER  256 //4*64
#define CLEARED     1
#define PAYABLE     2
#define LOCKED      3
#define ONLINE_TR   0
#define OFFLINE_TR  1

/*--------------------------------------------------------------*/
//��Ҫ���ݽṹ--------------
//�������ݼ����ַ:(&��/��������_����)
//����,��IFSF 3-01,122-128
enum  FP_TR_DAT_AD{TR_Seq_Nb = 1, TR_Contr_Id , TR_Release_Token ,
	TR_Fuelling_Mode,	TR_Amount, TR_Volume, TR_Unit_Price,
	TR_Log_Noz, TR_Price_Set_Nb, TR_Prod_Nb=10, /*TR_Prod_Description,*/ TR_Error_Code=12, 
	TR_Average_Temp, TR_Security_Chksum = 14, 
	/* M1_Sub_Volume, M2_Sub_Volume, 	TR_Tax_Amount = 17, */ 
	TR_Buff_Contr_Id = 20, 	Trans_State = 21, 
	Clear_Transaction=30, Lock_Transaction, Unlock_Transaction,//CMD
	TR_Buff_States_Message = 100 , 
	TR_RD_OFFLINE = 200, TR_RM_OFFLINE = 201,  // CMD
	TR_Date = 202, TR_Time = 203, TR_Start_Total = 204, TR_End_Total = 205 // user defined Data_Id
}; //gFp_tr_dat_ad;

//enum  FP_TR_CMD_AD{ Clear_Transaction=30, Lock_Transaction, Unlock_Transaction};//gFp_tr_cmd_ad;

typedef struct{ //import!!
	/*Ϊ�˷����ȡ�������:*/
	unsigned char offline_flag;   // offline TR flag: 0 - normal TR, !0 - offline TR
	unsigned char node; //node is in a dispenser's LNA 
	unsigned char fp_ad; //21h,22h,23h,24h.It's also sequence number of FP.
	/*TRANSACTION DATA:*/
	unsigned char TR_Seq_Nb[2];                             unsigned char TR_Contr_Id[2];
	unsigned char TR_Release_Token;                        	unsigned char TR_Fuelling_Mode;
	unsigned char TR_Amount[5];                             unsigned char TR_Volume[5];
	unsigned char TR_Unit_Price[4];                         unsigned char TR_Log_Noz;
	//unsigned char TR_Prod_Description[16]; // O, not used
	unsigned char TR_Prod_Nb[4];                            unsigned char TR_Error_Code;
	unsigned char TR_Security_Chksum[3];
	//unsigned char M1_Sub_Volume[5];//not used
	//unsigned char M2_Sub_Volume[5]; //not used 
	//unsigned char TR_Tax_Amount[5]; //O, not used
	/*TRANSACTION BUFFER STATUS:*/
	unsigned char Trans_State;                              unsigned char TR_Buff_Contr_Id[2];
	//Clear_Transaction,Lock_Transaction,Unlock_Transaction.They are command.
	/*UNSOLICITED DATA:��MakeTrMsg���������!!*/
	//unsigned char TR_Buff_States_Message[17]; //see IFSF 3-01,128
	/*MANUFACTURER/OIL COMPANY SPECIFIC*/
	unsigned char TR_Date[4]; // user defined, Data_Id 202, offline Transaction Date
	unsigned char TR_Time[3]; // user defined, Data_Id 203, offline Transaction Time
	unsigned char TR_Start_Total[7]; // user defined, Data_Id 204, Start Log_Noz_Vol_Total
	unsigned char TR_End_Total[7];  // user defined, Data_Id 205, End Log_Noz_Vol_Total
} FP_TR_DATA; //42 byte;
//FP_TR_DATA��������. node���ͻ��߼��ڵ��,fp_ad���͵��ַ21h-24h,Ҫ�õ�gpIfsf_shm,ע��P,V����������.
//���Ӻ����һ�ʽ��׼�¼.�����¼��¼ע��Ҫ�ж�TR_State�Ƿ�Ϊ���״̬.
int AddTR_DATA(const FP_TR_DATA *fp_tr_data);
int DelTR_DATA(const char node, const char fp_ad, const unsigned char *tr_seq_nb);
//��ȡFP_TR_DATA. �������Ϣ���ݵ�����Ϊ001�ش�.
//recvBufferΪ�յ���data,sendBufferΪҪ���͵�data,msg_lgΪ���ɵ��ܳ���.
int ReadTR_DATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg);
//дFP_LN_DATA����.recvBufferΪ�յ���data,sendBufferΪҪ�ظ���data,�������Ϣ���ݵ�����Ϊ111Ӧ��.
//msg_lgΪ���ɵ��ܳ���,Ϊ0��ʾ����ҪӦ����Ϣ,sendBuffer��msg_lg����ҪӦ����Ϣ��������ΪNULL.
int WriteTR_DATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg);
//int SetSglTR_DATA(const char node, const char fp_ad, const unsigned char *data);
int GetNextTrNb(const char node, const char fp_ad, unsigned char *tr_seq_nb);
long GetTrAd(const char node, const char fp_ad, const unsigned char *tr_seq_nb);
static long GetHeadTrAd(const char node, const char fp_ad);
static long GetTailTrAd(const char node, const char fp_ad);
static int GetPayTrCnt(const char node, const char fp_ad);
int ReachedUnPaidLimit(unsigned char node, unsigned char fp_ad);

//Calculate value of  TR_Security_Chksum in transanction database. 
static int CalcTrCheksum( FP_TR_DATA *fp_tr_data);
int DelAllClearTR_DATA(const char node);
int DelAllTR_DATA(const char node, const char fp_ad);

//it's for more and more transation data, such as CD isn't online.
int DefaultNodeTR_DATA(const char node);
//write node trasanction to temp transanction file(define by TEMP_TR_FILE in ifsf_pub.h)
int WriteFileNodeTR_DATA(const unsigned char node);
//write node trasanction to temp transanction file(define by TEMP_TR_FILE in ifsf_pub.h), but only half old transaction
int WriteHalfNodeTR_DATA(const unsigned char node);
//read node trasanction from temp transanction file(define by TEMP_TR_FILE in ifsf_pub.h), but only half old transaction
//if all read out, delete the file for write balance.
int ReadHalfNodeTR_DATA( const unsigned char node);


//����״̬ʱ:��buff�е�����δ֧����¼״̬��Ϊ������֧��״̬ 
//buff��Ϊ����buff  modify by liys 2008-03-14
int OffLineSetBuff( const unsigned char node);

//���߱�Ϊ����ʱ:��buff�е����е����߼�¼д���ļ�,�����buff
//buff  ��Ϊ����buff modify by liys 2008-03-14
int WriteBuffDataToFile(const unsigned char node);


//��ѯbuffer �����߽��׵ļ�¼����
//��AddTR_DATA ��ʹ��,�������100��,����ⲿ���ѻ���¼д���ļ�
int  GetAllOfflineCnt(const unsigned char node);




//int PackTR_DATA();//��д,��ʱ����
//������������TR_Buff_States_Message��ֵ������,sendDataΪ������ַ��ͷ��Ϣ������,data_lg�����ܳ���
static int MakeTrMsg(const char node, const char fp_ad, const FP_TR_DATA *pfp_tr_data, unsigned char *sendBuffer, int *data_lg);
//�ı佻��״̬TR_State. ע�⽻�������״̬ʱҪ��鱣��Ĵ������״̬�Ľ�����,
//����fp��Nb_Of_Historic_Trans�Ƚ�,���ڵĻ���ɾ�������Ĵ������״̬�Ľ���
static int ChangeTR_State(const char node, const char fp_ad, FP_TR_DATA *pfp_tr_data, const char status);
//����ִ�н�������,dataΪ:Data_id+Data_lg(ֵΪ2)+Data_El(Ϊ�������)
int upload_offline_tr(const unsigned char node, const unsigned char *recvBuffer);
int has_offline_tr_in_buf(const unsigned char node);
int has_incomplete_offline_tr(const unsigned char node);
void clr_offline_flag(const unsigned char node);


/*-----------------------------------------------------------------*/
#endif


/*******************************************************************
1.�ṹ��FP_XXX_DATA��ö������FP_XXX_DAT_AD������IFSF���������ݼ����ַ.
2. ��.c�ļ�Ҫ��#incude "ifsf_fp.h",�˴�������Ǽ��͵����ݽṹ�Ͳ�������
-------------------------------------------------------------------
2007-07-04 by chenfm
2008-03-16 - Guojie Zhu <zhugj@css-intelligent.com>
    ����FP_DATA ��FP_Running_Transaction_Frequency�ĳ���
*******************************************************************/
#ifndef __IFSF_FPFP_H__
#define __IFSF_FPFP_H__

#include "ifsf_def.h"
#include "ifsf_dspad.h"

//add your define here .....

//All key indexes are defined in ifsf_def.h


/*--------------------------------------------------------------*/
//��Ҫ���ݽṹ--------------
//�������ݼ����ַ:(&��/��������_����)

//���͵�. ��ַ21h-24h.
enum FP_DAT_AD{
	FP_FP_Name = 1, FP_Nb_Tran_Buffer_Not_Paid , FP_Nb_Of_Historic_Trans ,  FP_Nb_Logical_Nozzle ,
	FP_Loudspeaker_Switch = 6,  FP_Default_Fuelling_Mode ,  FP_Leak_Log_Noz_Mask ,
	FP_Drive_Off_Light_Switch = 10,  FP_OPT_Light_Switch ,
	FP_FP_State = 20, FP_Log_Noz_State,  FP_Assign_Contr_Id,  FP_Release_Mode,
	FP_ZeroTR_Mode, FP_Log_Noz_Mask, FP_Config_Lock, FP_Remote_Amount_Prepay,
	FP_Remote_Volume_Preset, FP_Current_TR_Seq_Nb = 29, FP_Release_Contr_Id ,
	FP_Suspend_Contr_Id , FP_Release_Token = 32, FP_Fuelling_Mode ,
	FP_Current_Amount = 34, FP_Current_Volume , FP_Current_Unit_Price , FP_Current_Log_Noz ,
	FP_Current_Prod_Nb , FP_Current_TR_Error_Code , FP_Current_Average_Temp ,
	FP_Transaction_Sequence_Nb = 41, FP_Current_Price_Set_Nb, FP_Multi_Nozzle_Type ,
	FP_Multi_Nozzle_State ,  FP_Multi_Nozzle_Status_Message ,
	FP_Local_Vol_Preset = 51, FP_Local_Amount_Prepay , 
	FP_Running_Transaction_Message_Frequency = 59,
	//COMMAND:
	FP_Open_FP  = 60, FP_Close_FP  = 61, FP_Release_FP ,  FP_Terminate_FP ,  FP_Suspend_FP ,
	FP_Resume_FP ,FP_Clear_Display ,  FP_Leak_Command,/*O*/ 
	//UNSOLICITED DATA:
	FP_FP_Alarm  = 80, FP_FP_Status_Message =100, FP_FP_Multi_Nozzle_Status_Message,
	FP_FP_Running_Transaction_Message
};//gFp_dat_ad;

// ����������
enum EVENT {Operative = 0, Unable, Open_FP, Close_FP, Release_FP = 7, Suspend_FP, \
       	Resume_FP, Terminate_FP, Do_Auth = 30, Do_Ration, Invalid_Event = 255};
/*enum FP_CMD_AD{FP_Close_FP  = 61, FP_Release_FP ,  FP_Terminate_FP ,  FP_Suspend_FP ,
	FP_Resume_FP ,FP_Clear_Display ,  FP_Leak_Command /O/  };*///gFp_cmd_ad;
typedef struct{
	/*CONFIGURATION:*/
	unsigned char FP_Name[8];                          unsigned char Nb_Tran_Buffer_Not_Paid;
	unsigned char Nb_Of_Historic_Trans; /*1-15*/       unsigned char Nb_Logical_Nozzle;/*O*/
	unsigned char Loudspeaker_Switch;/*O*/             unsigned char Default_Fuelling_Mode;/*only 1*/
	unsigned char Leak_Log_Noz_Mask; //each bit is a mask.all is 0 in PCD.
	/*LIGHT CONTROL DATA:*/                  
	unsigned char Drive_Off_Light_Switch;/*O*/         unsigned char OPT_Light_Switch;/*O*/
	/*Contral data:*/
	unsigned char FP_State;                            unsigned char Log_Noz_State; //Bit set 1 show LNZ remove.
	unsigned char Assign_Contr_Id[2];                  unsigned char Release_Mode; /*O*/
	unsigned char ZeroTR_Mode;                         unsigned char Log_Noz_Mask;//bit authorized mask.
	unsigned char Config_Lock[2];/*import!!*/          unsigned char Remote_Amount_Prepay[5];
	unsigned char Remote_Volume_Preset[5];             unsigned char Release_Token;
	unsigned char Fuelling_Mode;                       unsigned char Transaction_Sequence_Nb[2];
	/*CURRENT TRANSACTION DATA:*/ 
	unsigned char Current_TR_Seq_Nb[2];                unsigned char Release_Contr_Id[2];
	unsigned char Suspend_Contr_Id[2];                 unsigned char Current_Amount[5];
	unsigned char Current_Volume[5];                   unsigned char Current_Unit_Price[4];
	unsigned char Current_Log_Noz;                     unsigned char Current_Prod_Nb[4]; //BCD8.
	unsigned char Current_TR_Error_Code;               unsigned char Current_Average_Temp[3];/*O*/ 
	unsigned char Current_Price_Set_Nb[2];/*O*/ 
	/*CONFIGURATION:*/
	unsigned char Multi_Nozzle_Type;/*O*/              unsigned char Multi_Nozzle_State; /*O*/     
	unsigned char Multi_Nozzle_Status_Message; 
	unsigned char Local_Vol_Preset[5]; /*O*/ 	   unsigned char Local_Amount_Prepay[5];/*O*/
	unsigned char Running_Transaction_Message_Frequency[2];/*O Bcd4*/
	unsigned char FP_Alarm[8];/*O*/	
	unsigned char Current_Date_Time[7]; /* Current_Data&Time, ʵʱ������������ʱ��,CSSIУ����Ŀ����*/
	unsigned char Current_Volume_4_TCC_Adjust[5];  /* У����Ŀ�����ߺ�һ��ʵʱ���� */
	/*UNSOLICITED DATA:�ú���MakeFpMsg��MakeFpMsgSendʵ��*/	
	 //unsigned char FP_Status_Message[4]; //See IFSF 3-01,113. O isn't used.
	 //unsigned char FP_Multi_Nozzle_Status_Message;//O, See IFSF 3-01,113. 
	 //unsigned char FP_Running_Transaction_Message[12];//O, See IFSF 3-01,114.	
}FP_DATA;
//FP_DATA��������. node���ͻ��߼��ڵ��,fp_ad���͵��ַ21h-24h,Ҫ�õ�gpIfsf_shm,ע��P,V����������.
int DefaultFP_DATA(const char node, const char fp_ad);//Ĭ�ϳ�ʼ��.
int SetFP_DATA(const char node, const char fp_ad, const FP_DATA *fp_data);//���ó�ʼ��ֵ.����!!
//��ȡFP_DATA. FP_ID��ַΪ21h-24h.�������Ϣ���ݵ�����Ϊ001�ش�
//recvBufferΪ�յ���data,sendBufferΪҪ���͵�data,msg_lgΪ���ɵ��ܳ���.
int ReadFP_DATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg);
//дFP_PR_DATA����.recvBufferΪ�յ���data,sendBufferΪҪ�ظ���data,�������Ϣ���ݵ�����Ϊ111Ӧ��
//msg_lgΪ���ɵ��ܳ���,Ϊ0��ʾ����ҪӦ����Ϣ,sendBuffer��msg_lg����ҪӦ����Ϣ��������ΪNULL.
int WriteFP_DATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg);
int SetSglFP_DATA(const char node, const char fp_ad, const unsigned char *data);
int RequestFP_DATA(const char node, const char fp_ad);
int EndTr4Fp(const char node, const char fp_ad, const char * const start_total, const char * const end_total);
int EndZeroTr4Fp(const char node, const char fp_ad);
int BeginTr4Fp(const char node, const char fp_ad);

//�ı�FP״̬. fp_stateΪ��״̬. ע��MakeFpMsgSendҪ����FP_Status_Message!!(Dada_Id=100),ע����������.
int ChangeFP_State(const __u8 node, const __u8 fp_ad, const __u8 fp_state, const __u8 *ack_msg);
//����FP״̬
int SendFP_State(const __u8 node, const __u8 fp_ad);
//�ı��߼���ǹ״̬. ln_stateΪ��״̬. ע��MakeFpMsgSendҪ����FP_Status_Message!!(Dada_Id=100),ע����������.
int ChangeLog_Noz_State(const char node, const char fp_ad,  const unsigned char ln_state);
//ִ���յ�������.command����ֵ:61-67. (�·�����)
static int ExecFpCMD(const char node, const char fp_ad, const char command, const char *ack_msg);
//������������FP_Status_Message��ֵ������,sendDataΪ������ַ��ͷ��Ϣ������,data_lg�����ܳ���
static int MakeFpMsg(const char node, const char fp_ad, unsigned char *senBuffer, int *data_lg);
static int MakeFpTransMsg(const char node, const char fp_ad, unsigned char *sendBuffer, int *data_lg);
//������������FP_Status_Message��ֵ������.��������ĺ���,subnetOfCD��nodeOfCDΪCD��������ַ���߼��ڵ��.
//static int MakeFpMsgSend(const char node, const char fp_ad, const char subnetOfCD,const char nodeOfCD, unsigned char *sendBuffer, int *msg_lg);
//Ԥ������Ԥ����������,Ҫ����˽��Э����˿ڵ�ͨ��д����.
// dataΪ(Data_Id+Data_lg+Data_El)��������.Data_IdΪ27,28����51,52.
//static int WriteAmount_Prepay(const char node, const char fp_ad, const unsigned char *data);
//static int WriteVolume_Preset(const char node, const char fp_ad, const unsigned char *data);

//�߼���ǹ�ı亯��,Ҫ����ChangeLog_Noz_State��ChangeFP_State����.
//dataΪ(Data_Id+Data_lg+Data_El)��������.Data_IdΪ21,25,37,Data_lgΪ1.
static int ChangeLog_Noz(const char node, const char fp_ad, const unsigned char *data);
int HandleStateErr(const __u8 node, const __u8 pos, const __u8 event, const __u8 state, __u8 *ack_msg);
int SendCmdAck(const signed char ack_type, __u8 *ack_msg);

/*--------------------------------------------------------------*/
#endif

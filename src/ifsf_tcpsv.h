/*******************************************************************
1.�ṹ��FP_XXX_DATA��ö������FP_XXX_DAT_AD������IFSF���������ݼ����ַ.�˴�����ͨѶ������.
2. ��.c�ļ�Ҫ��#incude "ifsf_pub.h"
-------------------------------------------------------------------
2007-07-13 by chenfm
*******************************************************************/
#ifndef __IFSF_TCPSV_H__
#define __IFSF_TCPSV_H__

#include "ifsf_def.h"
#include "ifsf_dspad.h"


#define HEARTBEAT_BROAD_IP  "10.1.2.255"  //IFSF�����㲥ip��ַ.
#define HEARTBEAT_INTERVAL 10   //IFSF������ʱ����Ϊ10��
#define HEARTBEAT_TIME  3      //����3��IFSF������ʱ����û���յ������ͱ�ʾ���豸����. 
//#define COMMUNICATION_PROTOCAL_VER 100  //����汾��


enum FCC_COM_SV_AD{ Communication_Protocol_Ver = 1, Local_Node_Address, Recipient_Addr_Table, 
	Heartbeat_Interval, Max_Block_Length, Add_Recipient_Addr = 11, Remove_Recipient_Addr,
	/*USER DEFINED DATA:*/Heartbeat_Broad_IP = 200, 
	/*PCD USED COMMAND:*/	Syc_Time = 220, Power_Failure = 221, Node_Channel_Cfg = 222, };
typedef struct{
	/*CONFIGURATION:*/
	unsigned char Communication_Protocol_Ver[6];
	//Local_Node_Address[2];//PCD��������.
	//unsigned char Recipient_Addr_Table[128];//PCD��������.
	unsigned char Heartbeat_Interval;
	//unsigned char Max_Block_Length;	//TCPIP����
	/*COMMANDS:*/ /*PCD��������*/
	//unsigned char Add_Recipient_Addr[2]; 
	//unsigned char Remove_Recipient_Addr[2]; 
	/*USER DEFINED DATA:*/	
	unsigned char Heartbeat_Broad_IP[16];/*�Զ�������!!�㲥��ַ*/
	/*USER DEFINED PCD COMMAND:*/ //�Զ�������	 
	 //Syc_Time; //Syc_Timeͬ��ʱ������,��ַ220.���ݸ�ʽYYYYMMDDHHMMSS.
	 //Power_Failure
	 //Node_Channel_Cfg; //����PCD����������Ϣ��������.	 
	}FCC_COM_SV_DATA;
//FCC_COM_SV_DATA��������,Ҫ�õ�gpIfsf_shm,�����涨��.
int DefaultSV_DATA();//Ĭ�ϳ�ʼ��.
int SetSV_DATA(const FCC_COM_SV_DATA *fcc_com_sv_data);
//��ȡFCC_COM_SV_DATA. 
//COM_SV��ַΪ0, recvBufferΪ�յ���data,sendBufferΪҪ���͵�data,msg_lgΪ���ɵ��ܳ���.
int ReadSV_DATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg);
//дFCC_COM_SV_DATA����.recvBufferΪ�յ���data,sendBufferΪҪ�ظ���data, 
//msg_lgΪ���ɵ��ܳ���,Ϊ0��ʾû�лظ���Ϣ,sendBuffer��msg_lg���ظ���������ΪNULL,.
int WriteSV_DATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg);
//дFCC_COM_SV_DATA�ĵ���������ĺ���.��ʼ������PCD�豸��д������.
//�ɹ�����0,ʧ�ܷ���-1����Data_Ack.
int SetSglSV_DATA(const unsigned char *data);
//��������PCD����������Ϣ��������������CD,(�ڵ��,�ڵ�����)
//��������: 2+nodeCD+1+1+20(Msg_St)+Msg_Lg+0+220+Data_Lg+node1+type1+...
int RequestNode_Channel(unsigned char node);

//ͬ��ʱ��. dataʱ������Data_Id(ֵ200)+Data_Lg(ֵ7)+Data_El(ֵ��ʽYYYYMMDDHHMMSS).
//static int Syc_TimeCmd(const unsigned char *data);
int RequestSyc_Time(unsigned char node);//������ʱ��ͬ�����ݵ�CD


#endif



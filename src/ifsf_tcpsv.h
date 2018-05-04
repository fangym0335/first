/*******************************************************************
1.结构体FP_XXX_DATA和枚举类型FP_XXX_DAT_AD定义了IFSF的所有数据及其地址.此处定义通讯服务器.
2. 本.c文件要加#incude "ifsf_pub.h"
-------------------------------------------------------------------
2007-07-13 by chenfm
*******************************************************************/
#ifndef __IFSF_TCPSV_H__
#define __IFSF_TCPSV_H__

#include "ifsf_def.h"
#include "ifsf_dspad.h"


#define HEARTBEAT_BROAD_IP  "10.1.2.255"  //IFSF心跳广播ip地址.
#define HEARTBEAT_INTERVAL 10   //IFSF心跳的时间间隔为10秒
#define HEARTBEAT_TIME  3      //经过3次IFSF心跳的时间间隔没有收到心跳就表示该设备离线. 
//#define COMMUNICATION_PROTOCAL_VER 100  //软件版本号


enum FCC_COM_SV_AD{ Communication_Protocol_Ver = 1, Local_Node_Address, Recipient_Addr_Table, 
	Heartbeat_Interval, Max_Block_Length, Add_Recipient_Addr = 11, Remove_Recipient_Addr,
	/*USER DEFINED DATA:*/Heartbeat_Broad_IP = 200, 
	/*PCD USED COMMAND:*/	Syc_Time = 220, Power_Failure = 221, Node_Channel_Cfg = 222, };
typedef struct{
	/*CONFIGURATION:*/
	unsigned char Communication_Protocol_Ver[6];
	//Local_Node_Address[2];//PCD单独定义.
	//unsigned char Recipient_Addr_Table[128];//PCD单独定义.
	unsigned char Heartbeat_Interval;
	//unsigned char Max_Block_Length;	//TCPIP不用
	/*COMMANDS:*/ /*PCD单独定义*/
	//unsigned char Add_Recipient_Addr[2]; 
	//unsigned char Remove_Recipient_Addr[2]; 
	/*USER DEFINED DATA:*/	
	unsigned char Heartbeat_Broad_IP[16];/*自定义数据!!广播地址*/
	/*USER DEFINED PCD COMMAND:*/ //自定义命令	 
	 //Syc_Time; //Syc_Time同步时间命令,地址220.数据格式YYYYMMDDHHMMSS.
	 //Power_Failure
	 //Node_Channel_Cfg; //请求PCD基础配置信息数据命令.	 
	}FCC_COM_SV_DATA;
//FCC_COM_SV_DATA操作函数,要用到gpIfsf_shm,见后面定义.
int DefaultSV_DATA();//默认初始化.
int SetSV_DATA(const FCC_COM_SV_DATA *fcc_com_sv_data);
//读取FCC_COM_SV_DATA. 
//COM_SV地址为0, recvBuffer为收到的data,sendBuffer为要发送的data,msg_lg为生成的总长度.
int ReadSV_DATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg);
//写FCC_COM_SV_DATA函数.recvBuffer为收到的data,sendBuffer为要回复的data, 
//msg_lg为生成的总长度,为0表示没有回复信息,sendBuffer和msg_lg不回复可以设置为NULL,.
int WriteSV_DATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg);
//写FCC_COM_SV_DATA的单个数据项的函数.初始化或者PCD设备改写数据项.
//成功返回0,失败返回-1或者Data_Ack.
int SetSglSV_DATA(const unsigned char *data);
//发送请求PCD基础配置信息到所有有心跳的CD,(节点号,节点类型)
//期望数据: 2+nodeCD+1+1+20(Msg_St)+Msg_Lg+0+220+Data_Lg+node1+type1+...
int RequestNode_Channel(unsigned char node);

//同步时间. data时间数据Data_Id(值200)+Data_Lg(值7)+Data_El(值格式YYYYMMDDHHMMSS).
//static int Syc_TimeCmd(const unsigned char *data);
int RequestSyc_Time(unsigned char node);//发请求时间同步数据到CD


#endif



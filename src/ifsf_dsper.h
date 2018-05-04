/*******************************************************************
1.结构体FP_XXX_DATA和枚举类型FP_XXX_DAT_AD定义了IFSF的所有数据及其地址.此处定义错误.
2. 本.c文件要加#incude "ifsf_fp.h"
-------------------------------------------------------------------
2007-07-04 by chenfm
2008-03-09 - Guojie Zhu <zhugj@css-intelligent.com>
    FP_ER_DATA_AD.FP_Err_r_State 赋值为 5 (原为4)
*******************************************************************/
#ifndef __IFSF_FPER_H__
#define __IFSF_FPER_H__
#include "ifsf_def.h"
#include "ifsf_dspad.h"


/*--------------------------------------------------------------*/
//重要数据结构--------------
//具体数据及其地址:(&和/符号已用_代替)
//错误数据,见IFSF 3-01,129-133,  ER_DAT地址为41h,FP_Error_Type就是ER_ID的地址01h-40h
enum FP_ER_DAT_AD{FP_Error_Type = 1, FP_Err_Description, FP_Error_Total = 3,
	FP_Error_State = 5, FP_Error_Type_Mes = 100 };//gFp_er_dat_ad;
typedef struct{
	/*ERROR DATA:*/
	unsigned char FP_Error_Type;    	         unsigned char FP_Err_Description[20];/*O*/  
	unsigned char FP_Error_Total;    	         unsigned char FP_Error_State;
	/*UNSOLICITED DATA:用函数MakeErMsg和实现MakeErMsgSend*/
	//unsigned char FP_Error_Type_Mes[2];/*see IFSF 3-01,130*/
	}FP_ER_DATA;
//FP_ER_DATA操作数据. node加油机逻辑节点号,fp_ad加油点地址21h-24h,要用到gpIfsf_shm,注意P,V操作和死锁.
//改变错误代码.  要调用MakeErMsg函数.
//
int DefaultER_DATA(const char node, const char fp_ad);
int ChangeER_DATA(const __u8 node, const __u8 fp_ad, const __u8 er_ad, const __u8 *msgbuffer);
//读取FP_ER_DATA. 构造的信息数据的类型为001回答.
//recvBuffer为收到的data,sendBuffer为要发送的data,msg_lg为生成的总长度.
int ReadER_DATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg);
//写FP_ER_DATA函数.recvBuffer为收到的data,sendBuffer为要回复的data,构造的信息数据的类型为111应答.
//msg_lg为生成的总长度,为0表示不需要应答信息,sendBuffer和msg_lg不需要应答信息可以设置为NULL.
//int WriteER_DATA(const char node, const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg);
//int SetSglER_DATA(const char node, const char fp_ad, const unsigned char  *data);

//构造主动数据FP_Error_Type_Mes的值不发送,sendbuffer为待发送的数据,data_lg数据总长度
static int MakeErMsg(const char node, const char fp_ad, unsigned char *sendBuffer, int *data_lg);
//构造主动数据TFP_Error_Type_Mes的值但发送.调用上面的函数,subnetOfCD和nodeOfCD为CD的子网地址和逻辑节点号.
static int MakeErMsgSend(const char node, const char fp_ad, const char subnetOfCD, \
		const char nodeOfCD, unsigned char *sendBuffer, int *msg_lg);

/*--------------------------------------------------------------*/
#endif

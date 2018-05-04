/*******************************************************************
1.结构体FP_XXX_DATA和枚举类型FP_XXX_DAT_AD定义了IFSF的所有数据及其地址.
2. 本.c文件要加#incude "ifsf_fp.h",此处定义的是计量器的数据结构和操作函数
-------------------------------------------------------------------
2007-07-13 by chenfm
*******************************************************************/
#ifndef __IFSF_FPMETER_H__
#define __IFSF_FPMETER_H__
#include "ifsf_def.h"
#include "ifsf_dspad.h"


/*--------------------------------------------------------------*/
//重要数据结构--------------
//具体数据及其地址:(&和/符号已用_代替)
//计量表,见IFSF 3-01,V2.44,85-88. 地址81h-90h. C_DAT的Nb_Meters标志多少个.
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
//FP_M_DATA操作函数,node加油机逻辑节点号,M_Ad计量表地址81h-90h,要用到gpIfsf_shm,注意P,V操作和死锁.
int DefaultM_DAT(const char node,  const char m_ad);//默认初始化. 
int SetM_DAT(const  char node,  const char m_ad, const FP_M_DATA *fp_m_data);//设置FP_M_DATA.  
int CalcMeterTotal(const  char node, const unsigned char m_ad, const unsigned long volume);//计算总加油量. 
//读取FP_M_DATA FP_M_DATA地址为81h-90h.构造的信息数据的类型为001回答
//recvBuffer为收到的data,sendBuffer为要发送的data,msg_lg为生成的总长度.
int ReadM_DATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg);
//写FP_M_DATA函数.recvBuffer为收到的data,sendBuffer为要回复的data,构造的信息数据的类型为111应答
//msg_lg为生成的总长度,为0表示不需要应答信息,sendBuffer和msg_lg不需要应答信息可以设置为NULL.
int WriteM_DATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg);
int SetSglM_DAT(const  char node,  const char m_ad, const unsigned char *data);//MeterTotal, give volume then add by self.
int RequestM_DAT(const  char node,  const char m_ad);

/*-----------------------------------------------------------------*/
#endif

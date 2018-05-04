/*******************************************************************
1.结构体FP_XXX_DATA和枚举类型FP_XXX_DAT_AD定义了IFSF的所有数据及其地址.
2. 本.c文件要加#incude "ifsf_fp.h",此处定义的是逻辑油的枪数据结构和操作函数
-------------------------------------------------------------------
2007-07-13 by chenfm
*******************************************************************/
#ifndef __IFSF_FPLOZ_H__
#define __IFSF_FPLOZ_H__
#include "ifsf_def.h"
#include "ifsf_dspad.h"


/*--------------------------------------------------------------*/
//重要数据结构--------------
//具体数据及其地址:(&和/符号已用_代替)
//逻辑油枪.见IFSF 3-01,V2.44,115-121. 地址11h-18h.
//见FP中的Log_Noz_State(21位状态),Log_Noz_Mask(25位授权),Current_Log_Noz(37位当前油枪)
enum FP_LN_DAT_AD{LN_PR_Id = 1,LN_Hose_Expansion_Vol = 3, LN_Slow_Flow_Valve_Activ,	
	LN_Physical_Noz_Id = 5,	LN_Meter_1_Id = 7, LN_Meter_1_Blend_Ratio,  
	LN_Meter_2_Id, 	LN_Logical_Nozzle_Type, 
	LN_Preset_Valve_Activation = 11,
	LN_Log_Noz_Vol_Total = 20, LN_Log_Noz_Amount_Total, LN_No_TR_Total,
	LN_Log_Noz_SA_Vol_Total = 30, LN_Log_Noz_SA_Amount_Total, LN_No_TR_SA_Total, LN_All_Log_Noz_Vol_Total = 208
	};//gFp_ln_dat_ad;
typedef struct{
	/*CONFIGURATION:*/
	unsigned char PR_Id;  /*1-8,对应地址41h-48h*/   unsigned char Physical_Noz_Id;/*O*/ 
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
//FP_LN_DATA操作数据. node加油机逻辑节点号,fp_ad加油点地址21h-24h,ln_ad逻辑油枪地址11h-18h,
//要用到gpIfsf_shm,注意P,V操作和死锁.
int DefaultLN_DATA(const char node, const char fp_ad, const char ln_ad);
//读取FP_LN_DATA. 构造的信息数据的类型为001回答.
//recvBuffer为收到的data,sendBuffer为要发送的data,msg_lg为生成的总长度.
int ReadLN_DATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg);
//写FP_LN_DATA函数.recvBuffer为收到的data,sendBuffer为要回复的data,构造的信息数据的类型为111应答
//msg_lg为生成的总长度,为0表示不需要应答信息,sendBuffer和msg_lg不需要应答信息可以设置为NULL.
int WriteLN_DATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg);
//设置单个数据项,用于设备本身改写数据项或者初始化配置.
//成功返回0,失败返回-1或者Data_Ack. data内容为:Data_Id+Data_Lg+Data_El.
int SetSglLN_DATA(const char node, const char fp_ad, const char ln_ad, const unsigned char *data);
int RequestLN_DATA(const char node, const char fp_ad, const char ln_ad);
//一笔交易做完,LNZ要改变的数据. volum交易油量,amount交易金额.price交易单价
//int EndTr4Ln(const char node, const char fp_ad,  const char ln_ad,   const unsigned char * volume, const unsigned char *   amount,  const unsigned char *   price);
int EndTr4Ln(const char node, const char fp_ad,  const char ln_ad);

/*-----------------------------------------------------------------*/
#endif

/*******************************************************************
1.结构体FP_XXX_DATA和枚举类型FP_XXX_DAT_AD定义了IFSF的所有数据及其地址.
2. 本.c文件要加#incude "ifsf_fp.h",此处定义的是有关油品的数据结构和操作函数
-------------------------------------------------------------------
2007-07-13 by chenfm
*******************************************************************/
#ifndef __IFSF_FPPROD_H__
#define __IFSF_FPPROD_H__

#include "ifsf_def.h"
#include "ifsf_dspad.h"

//#define PROD_PRICE  "00001000"  //10.00yuan

/*--------------------------------------------------------------*/
//重要数据结构--------------
//具体数据及其地址:(&和/符号已用_代替)
//油品,见IFSF 3-01,V2.44,89-90. PR_Id地址41h~48h.地址与M_DAT的PR_Id对应.
enum FP_PR_DAT_AD{PR_Prod_Nb = 2, PR_Prod_Description ,  PR_Vap_Recover_Const =10
	};//gFp_pr_dat_ad;
typedef struct{	
	/*CONFIGURATION:*/
	unsigned char Prod_Nb[4];
	unsigned char Prod_Description[16];//O
	 //unsigned char Vap_Recover_Const;//O
}FP_PR_DATA;
//FP_PR_DATA操作函数,node加油机逻辑节点号,pr_ad油品地址,要用到gpIfsf_shm,注意使用访问索引关键字.
int DefaultPR_DAT(const  char  node, const unsigned char pr_ad);//不用,根据节点号可以直接写.

//读取FP_PR_DATA. FP_PR_DATA地址为41h-48h.构造的信息数据的类型为001回答
//recvBuffer为收到的data,sendBuffer为要发送的data,msg_lg为生成的总长度.
int ReadPR_DATA( const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg);
//写FP_PR_DATA函数. recvBuffer为收到的data,sendBuffer为要回复的data,构造的信息数据的类型为111应答
//msg_lg为生成的总长度,为0表示不需要应答信息,sendBuffer和msg_lg不需要应答信息可以设置为NULL.
int WritePR_DATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg);
int SetSglPR_DATA(const char node, const unsigned char pr_ad, const unsigned char *data);
//设置Prod_Nb[4]的值.prod_nb为具体值.
int SetPR_DATA_Nb(const char node, const unsigned char pr_ad, const unsigned char *prod_nb);
int RequestPR_DATA(const char node, const unsigned char pr_ad);

//每种加油模式下的油品. 见IFSF 3-01,V2.44,91-93, 地址:61h+00000001-99999999BCD+11h-18h.
enum FP_FM_DAT_AD{FM_Fuelling_Mode_Name = 1,  FM_Prod_Price = 2, FM_Max_Vol,
	FM_Max_Fill_Time, FM_Max_Auth_Time, FM_User_Max_Volume }; //gFp_fm_dat_ad;
typedef struct{
	//unsigned char Prod_Nb[4];  //为了与PR_ID对应,PCD新增加的.没有地址号.
	/*CONFIGURATION:*/
	 //unsigned char Fuelling_Mode_Name[8]; //O
	unsigned char Prod_Price[4];
	unsigned char Max_Vol[5];
	unsigned char Max_Fill_Time;
	unsigned char Max_Auth_Time;
	unsigned char User_Max_Volume[5];	
}FP_FM_DATA; //本PCD只是指定8种油品,每种油品只有1种加油模式.
//FP_FM_DATA操作函数. node加油机逻辑节点号,pr_nb油品编号(1-99999999),,pr_ad油品地址41h-48h,
//fm_id加油模式地址11h(11h-18h),要用到gpIfsf_shm,注意使用访问索引关键字.
int DefaultFM_DAT(const char node, const unsigned char pr_ad);//默认初始化
int SetFM_DAT(const char node, const unsigned char pr_ad, const FP_FM_DATA *fp_fm_data);//初始化,注意与FP_PR_DATA对应!!
//读取FP_FM_DATA. FP_FM_DATA地址为11h(11h-18h).构造的信息数据的类型为001回答
//recvBuffer为收到的data,sendBuffer为要发送的data,msg_lg为生成的总长度.
int ReadFM_DATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg);
//写FP_PR_DATA函数.recvBuffer为收到的data,sendBuffer为要回复的data,构造的信息数据的类型为111应答
//msg_lg为生成的总长度,为0表示不需要应答信息,sendBuffer和msg_lg不需要应答信息可以设置为NULL.
int WriteFM_DATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg);
int SetSglFM_DAT(const char node, const unsigned char pr_ad, const unsigned char  *data);
int SetFM_DAT_Price(const char node, const unsigned char pr_ad, const unsigned char  *price);
int RequestFM_DAT(const char node, const unsigned char pr_ad); //fm is only 1,ad is 11h.

/*-----------------------------------------------------------------*/
#endif

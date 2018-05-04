/*******************************************************************
1.结构体FP_XXX_DATA和枚举类型FP_XXX_DAT_AD定义了IFSF的所有数据及其地址.
2. 本.c文件要加#incude "ifsf_fp.h",此处定义的是交易的数据结构和操作函数
3. 交易数据区被划分为每个加油机120(4*30)条数据记录的数据库,按加油机
在gpIfsf_shm的顺序排列.
-------------------------------------------------------------------
2007-07-04 by chenfm
2008-03-09 - Guojie Zhu <zhugj@css-intelligent.com>
    struct FP_TR_DATA 增加成员 offline_flag,用于标记脱机交易
    FP_TR_DATA.TR_Date_Time 拆分为 TR_Date & TR_Time
    FP_TR_DATA.TR_Date_Time 拆分为 TR_Date & TR_Time
    预定义交易状态 CLEARED/PAYABLE/LOCKED

2008-03-14 modify by liys 增加了两个函数分别负责

1.离线状态时:把buff中的所有记录状态置为离线
2.离线转在线时:把buff中的所有记录写入离线文件,buff 清空

改动代码段标记: modify by liys 2008-03-14
2008-05-21 - Guojie Zhu <zhugj@css-intelligent.com>
    struct FP_TR_DATA 增加TR_Start_Total 和 TR_End_Total  用于存储交易的起止泵码

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
//重要数据结构--------------
//具体数据及其地址:(&和/符号已用_代替)
//交易,见IFSF 3-01,122-128
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
	/*为了方便存取定义的量:*/
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
	/*UNSOLICITED DATA:由MakeTrMsg函数计算出!!*/
	//unsigned char TR_Buff_States_Message[17]; //see IFSF 3-01,128
	/*MANUFACTURER/OIL COMPANY SPECIFIC*/
	unsigned char TR_Date[4]; // user defined, Data_Id 202, offline Transaction Date
	unsigned char TR_Time[3]; // user defined, Data_Id 203, offline Transaction Time
	unsigned char TR_Start_Total[7]; // user defined, Data_Id 204, Start Log_Noz_Vol_Total
	unsigned char TR_End_Total[7];  // user defined, Data_Id 205, End Log_Noz_Vol_Total
} FP_TR_DATA; //42 byte;
//FP_TR_DATA操作函数. node加油机逻辑节点号,fp_ad加油点地址21h-24h,要用到gpIfsf_shm,注意P,V操作和死锁.
//增加和清除一笔交易记录.清除记录记录注意要判断TR_State是否为清除状态.
int AddTR_DATA(const FP_TR_DATA *fp_tr_data);
int DelTR_DATA(const char node, const char fp_ad, const unsigned char *tr_seq_nb);
//读取FP_TR_DATA. 构造的信息数据的类型为001回答.
//recvBuffer为收到的data,sendBuffer为要发送的data,msg_lg为生成的总长度.
int ReadTR_DATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg);
//写FP_LN_DATA函数.recvBuffer为收到的data,sendBuffer为要回复的data,构造的信息数据的类型为111应答.
//msg_lg为生成的总长度,为0表示不需要应答信息,sendBuffer和msg_lg不需要应答信息可以设置为NULL.
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


//离线状态时:把buff中的所有未支付记录状态置为离线已支付状态 
//buff变为离线buff  modify by liys 2008-03-14
int OffLineSetBuff( const unsigned char node);

//离线变为在线时:把buff中的所有的离线记录写入文件,并清空buff
//buff  变为在线buff modify by liys 2008-03-14
int WriteBuffDataToFile(const unsigned char node);


//查询buffer 中离线交易的记录条数
//在AddTR_DATA 中使用,如果超过100笔,则把这部分脱机记录写入文件
int  GetAllOfflineCnt(const unsigned char node);




//int PackTR_DATA();//不写,暂时不用
//构造主动数据TR_Buff_States_Message的值不发送,sendData为不含地址等头信息的数据,data_lg数据总长度
static int MakeTrMsg(const char node, const char fp_ad, const FP_TR_DATA *pfp_tr_data, unsigned char *sendBuffer, int *data_lg);
//改变交易状态TR_State. 注意交易置清除状态时要检查保存的处于清除状态的交易数,
//并和fp的Nb_Of_Historic_Trans比较,大于的话就删除最后面的处于清除状态的交易
static int ChangeTR_State(const char node, const char fp_ad, FP_TR_DATA *pfp_tr_data, const char status);
//解释执行交易命令,data为:Data_id+Data_lg(值为2)+Data_El(为交易序号)
int upload_offline_tr(const unsigned char node, const unsigned char *recvBuffer);
int has_offline_tr_in_buf(const unsigned char node);
int has_incomplete_offline_tr(const unsigned char node);
void clr_offline_flag(const unsigned char node);


/*-----------------------------------------------------------------*/
#endif


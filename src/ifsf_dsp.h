/*******************************************************************
1.ifsf_fpXXX.h文件枚举类型FP_AD,FP_XXX_AD定义了IFSF的基地址,
结构体FP_XXX_DATA和枚举类型FP_XXX_DAT_AD定义了IFSF的所有数据及其地址.
2.DISPENSER_DATA是一个加油机的除事务处理数据外的所有数据.
3.IFSF_SHM是所有加油机的共享内存表,在ifsf_pub.h中定义,包括了所有待
协议转换的加油机的数据和事务处理数据,运行时注意适当使用
P,V操作,防止死锁.
4.根据PCD配置文件(pcd.cfg,主要为使用的加油机的node和加油机类型等),
初始化待协议转换的加油机的IFSF_SHM  的convertUsed和dispenser_data值,设置各
加油点为配置状态,设置心跳表值,向CD请求其他配置数据.改变清零交易区,
读取掉电保护进程写入NAND FLASH的交易数据到交易区,然后擦除NAND FLASH
的交易数据,  每笔未清除交易发送主动信息到CD.设置各加油点为Close状态,
心跳为请求配置状态(1),等待CD设置加油模式,  设置油品,设置油品油枪
关系,下发油价,和下发开机命令.
-------------------------------------------------------------------
2007-07-13 by chenfm
*******************************************************************/
#ifndef __IFSF_FP_H__
#define __IFSF_FP_H__

#include "ifsf_def.h"
#include "ifsf_dspad.h"
#include "ifsf_dspcal.h"
#include "ifsf_dspmeter.h"
#include "ifsf_dsptr.h"
#include "ifsf_dspfp.h"
#include "ifsf_dsplnz.h"
#include "ifsf_dsper.h"
#include "ifsf_dspprod.h"

//#include "ifsf_pub.h"  //置于.c文件中.
//#include "ifsf_main.h"  //置于.c文件中.

#define MAX_FP_CNT     4     //每个加油机的加油点数的最大值.IFSF为4.
#define MAX_FP_LN_CNT  4     //每个加油点的逻辑油枪数的最大值.IFSF为8.
#define MAX_FP_ER_CNT  64   //每个加油点的出错类型数的最大值.IFSF目前为64.
#define FP_PR_CNT      8     //每个加油机正在使用的油品数.IFSF为8.
#define FP_PR_NB_CNT   8   //一个加油机所用的所有油品号数,只保存正在使用的油品数据.IFSF为99999999.
#define FP_FM_CNT      8     //加油模式数,最大为8,因IFSF为8.
#define FP_M_DAT_CNT   8     //每个加油机的计量器数.IFSF为16.
//add your define here .....

//All key indexes are defined in ifsf_def.h


//待协议转换的一个加油机的结构体,不含事务处理.
typedef struct{	
	FP_C_DATA fp_c_data;
	FP_DATA fp_data[MAX_FP_CNT];
	FP_LN_DATA fp_ln_data[MAX_FP_CNT][MAX_FP_LN_CNT];
	FP_ER_DATA fp_er_data[MAX_FP_CNT];	//只存放最后一个error. [MAX_FP_ER_CNT];	
	FP_PR_DATA fp_pr_data[FP_PR_CNT];       // 以PR_Id & 0x0F为索引
	FP_FM_DATA fp_fm_data[FP_PR_NB_CNT][FP_FM_CNT]; // 以PR_Id & 0x0F为索引
	FP_M_DATA fp_m_data[FP_M_DAT_CNT];
} DISPENSER_DATA;

int DefaultDispenser(const char node);

//要使用gpIfsf_shm和相应的访问区关键字索引值index,注意P,V操作和死锁.
//增加和删除一个加油机.注意增加和删除心跳表.
int AddDispenser(const char node); //调用DefaultDispenser函数.
int DelDispenser(const char node);
int SetDispenser(const char node, const DISPENSER_DATA *dispenser_data);
//通用数据设置函数,用于lischn00x,
//data格式:总长度(1)+地址长度(1)+地址(1-6)+数据地址(1)+数据长度(1)+数据(1-n)
int SetSglData(const char node, const char *single_data);
//int InitIfsfFpShm(); 


/*TcpRecvServ调用:
  解析(并执行,如果要执行的话)收到的加油机信息数据,成功返回0,失败返回-1
  */
int TranslateRecv(const int newsock, const unsigned char *recvBuffer);
void TwoDecPoint(unsigned char *buffer, int len);

#endif

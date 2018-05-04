/*******************************************************************
本设计中协议转换是在oil_main的基础上进行的,
具体操作通过宏定义IFSF在oil_main的相应程序中增加:
#ifdef IFSF
  IFSF代码
#endif
进行协议转换.
本头文件定义IFSF标识,共享内存或信号量的关键字或索引值,IFSF设备逻辑地址
-------------------------------------------------------------------
2007-07-06 by chenfm
*******************************************************************/
#ifndef __IFSF_DEF_H__
#define __IFSF_DEF_H__

#ifndef IFSF
#define IFSF
#endif

//一个IFSF协议转换设备可能同时要转换协议的非IFSF设备的最大数
//#define MAX_CHANNEL_CNT 64  //define in pub.h 在本项目中，为前庭控制器连接的待协议转换的加油机的最大个数

//共享内存或信号量的关键字的索引值,在InitIfsfShm函数中创建,注意唯一性,P,V操作和死锁.
#define IFSF_SHM_INDEX 1700    //产生访问整个共享内存IFSF_SHM的关键字的索引值 
#define IFSF_SEM_INDEX 1705

//以下3个是产生访问共享内存中IFSF_HB_SHM的及其两个部分的关键字的索引值,注意唯一性,和死锁. 
#define HEARTBEAT_SEM_INDEX 1710    //产生访问整个心跳数据区IFSF_HB_SHM的索引值
#define HEARTBEAT_SEM_RECV_INDEX 1711 //产生访问心跳接收数据区的关键字的索引值
#define HEARTBEAT_SEM_CONVERT_INDEX 1712 //产生访问心跳发送数据区的关键字的索引值

//产生访问共享内存中的整个待协议转换的加油机的IFSF_DSP_SHM的关键字的索引值
#define IFSF_DP_SEM_INDEX 1720
/*#define...
  每个加油机的数据区的访问的关键字的索引值为IFSF_DSP_SEM_INDEX+node,共建立MAX_CHANNEL_CNT个,
  注意atexit()时销毁. 
 */
 
#define IFSF_DP_TR_SEM_INDEX 1850 //整个待协议转换的加油机的交易数据区的访问.
//other key index define here...  and you must add remove key index in init.c.
#define MAX_TCP_LEN 1024
/*-------------------------------------------------------------------*/
//IFSF用到的的基础数据结构
//IFSF设备逻辑地址
typedef struct{
	unsigned char subnet; 
	unsigned char node;	
//} IFSF_LNA;
} __attribute__((__packed__)) IFSF_LNA;             // add by jie @ 02.20
//定义一个设备逻辑地址联合体，可以用于设备地址的两种访问方式,注意大小端模式
typedef union {
	unsigned short address;
	IFSF_LNA st_address;
}IFSF_LNA_UNION;
/*-------------------------------------------------------------------*/
#endif

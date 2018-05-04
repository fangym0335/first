/*******************************************************************
1.本设计中每个待转换协议的非IFSF设备都转换为IFSF设备，拥有自己的心跳,
心跳服务进程两个, 一个发送所有待协议转换设备的心跳,一个接收IFSF设备的心跳
2.本设计中所有待转换协议的非IFSF设备共用一个TCP服务进程,具体请参见ifsf_main文件.
3.IFSF的TCPIP通讯中最重要的数据结构是IFSF_HEARTBEAT，结构IFSF_HEARTBEAT是一个有心跳的设备的
具体描述，是心跳程序要发送的10个字节的结构。IFSF_HB_SHM定义了一个存放在共享内存中的
设备心跳表，用于管理当前所有应当有心跳的设备的设备描述，类似于组合类, 
也类似于一个数据库中的表，使用时注意，适当使用P，V操作。
4.!!!开机后,首先启动TCP服务.其次PCD根据配置文件ifsf.cfg确定要转换的加油机的节点号,
逻辑通道号,以及所在的串口和物理通道号,设置心跳,各个节点心跳状态为请求配置.
如果本PCD的基础配置文件不存在,那么法一:用lisncfg()阻塞监听配置信息;法二:设置一个节点node=1,并向所有有心跳的CD(subnet=2)
发送Node_Channel_Cfg读数据(请求配置PCD的所有节点号和逻辑通道号), 收到后初始化
gpIfsf_shm和gpIfsf_shm,初始化所有加油机,继续请求其他具体配置,直到完成.
最后把心跳状态置为正常(全0),各个加油点置为Closed状态或者Idle状态,开始受控加油.
-------------------------------------------------------------------
2007-06-21 by chenfm
2008-02-25 - Guojie Zhu <zhugj@css-intelligent.com>
    struct IFSF_HEARTBEAT 增加 __attribute__((__packed__))
2008-03-04 - Guojie Zhu <zhugj@css-intelligent.com>
    增加AddTLGSendHB
    TcpRecvCDServ更名为TcpSend2CDServ
*******************************************************************/
#ifndef __IFSF_TCPIP_H__
#define __IFSF_TCPIP_H__


#include "ifsf_tcpsv.h"

#define MAX_SEND_DELAY_NU	5

//下面定义一个IFSF设备可能连接的其他IFSF设备的最大数，
//最大数根据IFSF_FP2_1.01附录1可得
//#define MAX_RECV_CNT  64  //BY IFSF OVER LONWORK,LONWORK中的通讯数据库要求为64 

/*-------------------------------------------------------------------*/
//IFSF_HEARTBEAT是最重要的数据结构!!

//IFSF设备的IFSF_TCPIP心跳的数据结构，共10个字节!! 源于IFSF_FP2_1.01的41页
typedef struct __attribute__((__packed__)) {           // add by jie @ 02.20
//typedef struct {
	unsigned long host_ip;  //4个字节，主机地址，必需是IPv4的4个字节!请根据你的编译器选择类型
	unsigned short port;   //2个字节，主机端口
	IFSF_LNA lnao;  	//2个字节，设备的逻辑地址
	unsigned char ifsf_mc;    //1个字节，IFSF消息码,依据IFSF_FP2_1.88心跳设备的ifsf_mc值为1.
	unsigned char status;     //1个字节，设备状态	
}IFSF_HEARTBEAT; //注意大小端模式,此处应当为大端模式
/*设备状态status: (from IFSF FP2_1.88, p35)
  bit 1 = Configuration Needed
  bit 2-7 = Reserved for future use
  bit 8 = Software refresh required*/

int SetHbStatus(const unsigned char node,  const char status);


//把一个设备从心跳表总加入或者删除,recv,send代表心跳数据区的那个区,
//删除CD类设备时要查看gpIsfs_fp_shm的交易数据区是否由它控制,是就解除控制?.
int AddSendHB(const int node);
int AddTLGSendHB(const int node);
int DelSendHB(const int node);
int AddRecvHB(const IFSF_HEARTBEAT *device);
int DelRecvHB(const IFSF_HEARTBEAT *device);
//static int PackupHbShm();//整理设备表

/*-------------------------------------------------------------------*/
//!!!  public函数,以下为其他.c文件外要调用的函数:
/*
启动TCP监听服务，所有待协议转换的设备公用一个TCP服务进程,
返回0正常启动，-1失败,
建议参考tcp.c中的TcpServInit()和lisock.c中的ListenSock()
然后设置IFSF_HB_SHM共享内存的值.
两个重要参数port和backLog参考ListenSock()在程序中实现.
注意不要接收到本机的心跳和待转换的设备心跳. 
请参考和调用tcp.h和tcp.c的函数.
本函数要调用ifsf_fp.c中的TranslateRecv()函数.
*/
int TcpRecvServ(); 
int TcpSend2CDServ();
/*
 发送数据到控制设备,
 sendBuffer:待发送数据,msgLength:数据长度,timeout为发送超时值.
 控制设备逻辑地址为sendBuffer中的前两个字节
 成功返回0,失败返回-1.请参考和调用tcp.h和tcp.c的函数.
*/
int SendData2CD(const unsigned char *sendBuffer, const int msgLength, const int timeout);
/*
 发送数据到给出的连接newsock,
 sendBuffer:待发送数据,msgLength:数据长度,timeout为发送超时值.
 成功返回0,失败返回-1.请参考和调用tcp.h和tcp.c的函数.
*/
int SendDataBySock(const int newsock, const unsigned char *sendBuffer, const int msgLength, const int timeout);

//启动本设备心跳发送服务，返回0正常启动，-1失败.
int HBSendServ(); 
//停止本设备心跳发送服务，返回0正常启动，-1失败.
//int StopHBSendServ(); 
//启动本设备心跳接收服务，返回0正常启动，-1失败.
int HBRecvServ(); //注意不要接收本机发送的心跳.
//停止本设备心跳接收服务，返回0正常启动，-1失败.
//int StopHBRecvServ(); 

/*-------------------------------------------------------------------*/
//private函数,下面为本.c程序要用到的函数:
//----------------------------------
//获取本机IP,返回本机IP地址，返回-1失败，解决DHCP问题,因为IFSF建议使用DHCP
static long  GetLocalIP(); //参见tcp.h和tcp.c文件中有,可以选择一个.

/*---------------------------------------------------------------------*/

#endif


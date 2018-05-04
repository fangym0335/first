#ifndef __IFSF_TLG_H__
#define __IFSF_TLG_H__

#include  "ifsf_tlgtlg.h"
#include  "ifsf_tlgtp.h"

#define MAX_TP_PER_TLG 31 //by IFSF 3.03 V1.27
//#define MAX_TLG_CNT 1 //max count of  tlg in a fcc.  //only one. 

//-
//待协议转换的一个液位仪的结构体.
//-
typedef struct{
	int tlg_node;//节点号. 
	int tlg_drv;//驱动程序号.
	int tlg_serial_port;//液位仪所用串口号
	int maint_enable; //维修允许标志,非0为允许,初始化为0.
	TLG_DAT tlg_dat;
	TLG_ER_DAT tlg_er_dat;
	TP_DAT tp_dat[MAX_TP_PER_TLG];
	TP_ER_DAT tp_er_dat[MAX_TP_PER_TLG];
}  TLG_DATA;

//--------------------------------------------------
//初始化液位仪
//-------------------------------------------------
 int DefaultTLG(const char node);
//----------------------------------------
//TcpRecvServ调用:
//  解析(并执行,如果要执行的话)收到的液位仪信息数据,成功返回0,失败返回-1
//---------------------------------------
int TranslateTLGRecv(const int newsock, const unsigned char *recvBuffer);


#endif


#ifndef __IFSF_TLG_H__
#define __IFSF_TLG_H__

#include  "ifsf_tlgtlg.h"
#include  "ifsf_tlgtp.h"

#define MAX_TP_PER_TLG 31 //by IFSF 3.03 V1.27
//#define MAX_TLG_CNT 1 //max count of  tlg in a fcc.  //only one. 

//-
//��Э��ת����һ��Һλ�ǵĽṹ��.
//-
typedef struct{
	int tlg_node;//�ڵ��. 
	int tlg_drv;//���������.
	int tlg_serial_port;//Һλ�����ô��ں�
	int maint_enable; //ά�������־,��0Ϊ����,��ʼ��Ϊ0.
	TLG_DAT tlg_dat;
	TLG_ER_DAT tlg_er_dat;
	TP_DAT tp_dat[MAX_TP_PER_TLG];
	TP_ER_DAT tp_er_dat[MAX_TP_PER_TLG];
}  TLG_DATA;

//--------------------------------------------------
//��ʼ��Һλ��
//-------------------------------------------------
 int DefaultTLG(const char node);
//----------------------------------------
//TcpRecvServ����:
//  ����(��ִ��,���Ҫִ�еĻ�)�յ���Һλ����Ϣ����,�ɹ�����0,ʧ�ܷ���-1
//---------------------------------------
int TranslateTLGRecv(const int newsock, const unsigned char *recvBuffer);


#endif


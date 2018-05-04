/*******************************************************************
1.�������ÿ����ת��Э��ķ�IFSF�豸��ת��ΪIFSF�豸��ӵ���Լ�������,
���������������, һ���������д�Э��ת���豸������,һ������IFSF�豸������
2.����������д�ת��Э��ķ�IFSF�豸����һ��TCP�������,������μ�ifsf_main�ļ�.
3.IFSF��TCPIPͨѶ������Ҫ�����ݽṹ��IFSF_HEARTBEAT���ṹIFSF_HEARTBEAT��һ�����������豸��
��������������������Ҫ���͵�10���ֽڵĽṹ��IFSF_HB_SHM������һ������ڹ����ڴ��е�
�豸���������ڹ���ǰ����Ӧ�����������豸���豸�����������������, 
Ҳ������һ�����ݿ��еı�ʹ��ʱע�⣬�ʵ�ʹ��P��V������
4.!!!������,��������TCP����.���PCD���������ļ�ifsf.cfgȷ��Ҫת���ļ��ͻ��Ľڵ��,
�߼�ͨ����,�Լ����ڵĴ��ں�����ͨ����,��������,�����ڵ�����״̬Ϊ��������.
�����PCD�Ļ��������ļ�������,��ô��һ:��lisncfg()��������������Ϣ;����:����һ���ڵ�node=1,����������������CD(subnet=2)
����Node_Channel_Cfg������(��������PCD�����нڵ�ź��߼�ͨ����), �յ����ʼ��
gpIfsf_shm��gpIfsf_shm,��ʼ�����м��ͻ�,��������������������,ֱ�����.
��������״̬��Ϊ����(ȫ0),�������͵���ΪClosed״̬����Idle״̬,��ʼ�ܿؼ���.
-------------------------------------------------------------------
2007-06-21 by chenfm
2008-02-25 - Guojie Zhu <zhugj@css-intelligent.com>
    struct IFSF_HEARTBEAT ���� __attribute__((__packed__))
2008-03-04 - Guojie Zhu <zhugj@css-intelligent.com>
    ����AddTLGSendHB
    TcpRecvCDServ����ΪTcpSend2CDServ
*******************************************************************/
#ifndef __IFSF_TCPIP_H__
#define __IFSF_TCPIP_H__


#include "ifsf_tcpsv.h"

#define MAX_SEND_DELAY_NU	5

//���涨��һ��IFSF�豸�������ӵ�����IFSF�豸���������
//���������IFSF_FP2_1.01��¼1�ɵ�
//#define MAX_RECV_CNT  64  //BY IFSF OVER LONWORK,LONWORK�е�ͨѶ���ݿ�Ҫ��Ϊ64 

/*-------------------------------------------------------------------*/
//IFSF_HEARTBEAT������Ҫ�����ݽṹ!!

//IFSF�豸��IFSF_TCPIP���������ݽṹ����10���ֽ�!! Դ��IFSF_FP2_1.01��41ҳ
typedef struct __attribute__((__packed__)) {           // add by jie @ 02.20
//typedef struct {
	unsigned long host_ip;  //4���ֽڣ�������ַ��������IPv4��4���ֽ�!�������ı�����ѡ������
	unsigned short port;   //2���ֽڣ������˿�
	IFSF_LNA lnao;  	//2���ֽڣ��豸���߼���ַ
	unsigned char ifsf_mc;    //1���ֽڣ�IFSF��Ϣ��,����IFSF_FP2_1.88�����豸��ifsf_mcֵΪ1.
	unsigned char status;     //1���ֽڣ��豸״̬	
}IFSF_HEARTBEAT; //ע���С��ģʽ,�˴�Ӧ��Ϊ���ģʽ
/*�豸״̬status: (from IFSF FP2_1.88, p35)
  bit 1 = Configuration Needed
  bit 2-7 = Reserved for future use
  bit 8 = Software refresh required*/

int SetHbStatus(const unsigned char node,  const char status);


//��һ���豸���������ܼ������ɾ��,recv,send�����������������Ǹ���,
//ɾ��CD���豸ʱҪ�鿴gpIsfs_fp_shm�Ľ����������Ƿ���������,�Ǿͽ������?.
int AddSendHB(const int node);
int AddTLGSendHB(const int node);
int DelSendHB(const int node);
int AddRecvHB(const IFSF_HEARTBEAT *device);
int DelRecvHB(const IFSF_HEARTBEAT *device);
//static int PackupHbShm();//�����豸��

/*-------------------------------------------------------------------*/
//!!!  public����,����Ϊ����.c�ļ���Ҫ���õĺ���:
/*
����TCP�����������д�Э��ת�����豸����һ��TCP�������,
����0����������-1ʧ��,
����ο�tcp.c�е�TcpServInit()��lisock.c�е�ListenSock()
Ȼ������IFSF_HB_SHM�����ڴ��ֵ.
������Ҫ����port��backLog�ο�ListenSock()�ڳ�����ʵ��.
ע�ⲻҪ���յ������������ʹ�ת�����豸����. 
��ο��͵���tcp.h��tcp.c�ĺ���.
������Ҫ����ifsf_fp.c�е�TranslateRecv()����.
*/
int TcpRecvServ(); 
int TcpSend2CDServ();
/*
 �������ݵ������豸,
 sendBuffer:����������,msgLength:���ݳ���,timeoutΪ���ͳ�ʱֵ.
 �����豸�߼���ַΪsendBuffer�е�ǰ�����ֽ�
 �ɹ�����0,ʧ�ܷ���-1.��ο��͵���tcp.h��tcp.c�ĺ���.
*/
int SendData2CD(const unsigned char *sendBuffer, const int msgLength, const int timeout);
/*
 �������ݵ�����������newsock,
 sendBuffer:����������,msgLength:���ݳ���,timeoutΪ���ͳ�ʱֵ.
 �ɹ�����0,ʧ�ܷ���-1.��ο��͵���tcp.h��tcp.c�ĺ���.
*/
int SendDataBySock(const int newsock, const unsigned char *sendBuffer, const int msgLength, const int timeout);

//�������豸�������ͷ��񣬷���0����������-1ʧ��.
int HBSendServ(); 
//ֹͣ���豸�������ͷ��񣬷���0����������-1ʧ��.
//int StopHBSendServ(); 
//�������豸�������շ��񣬷���0����������-1ʧ��.
int HBRecvServ(); //ע�ⲻҪ���ձ������͵�����.
//ֹͣ���豸�������շ��񣬷���0����������-1ʧ��.
//int StopHBRecvServ(); 

/*-------------------------------------------------------------------*/
//private����,����Ϊ��.c����Ҫ�õ��ĺ���:
//----------------------------------
//��ȡ����IP,���ر���IP��ַ������-1ʧ�ܣ����DHCP����,��ΪIFSF����ʹ��DHCP
static long  GetLocalIP(); //�μ�tcp.h��tcp.c�ļ�����,����ѡ��һ��.

/*---------------------------------------------------------------------*/

#endif


/*******************************
	*CopyRight Cuihq
	*All Right Reserved
	*
	*�ļ�����: domsgq.h
	*ժҪ:
	*
	*����: �޺���
	*����: 2004/06/01
*******************************/
#ifndef __DOMSGQ_H__
#define __DOMSGQ_H__

#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <signal.h>
#include "mycomm.h"
#include "commipc.h"
#include "doio.h"

#ifndef MSGQ_TIME_INFINITE
#define MSGQ_TIME_INFINITE			-1
#endif

#define MSGQ_MAX_SIZE			100

typedef struct{
	long	msgtyp;
	size_t size;
	char	msgbuf[MSGQ_MAX_SIZE + 1];
}MsgBuf;

/*�ж�ָ����msgq�Ƿ����*/
mybool IsMsqExist( int index );

/*������������һ����Ϣ����*/
int CreateMsq( int index );

/*��������ɾ��һ����Ϣ����*/
int RemoveMsq( int index );

/*ȡ����Ϣ������Ϣ����Ŀ*/
int GetMsqNum( int index );

/*����buf�����ݣ����õ����͵�msgtyp����msgtypΪpidֵ*/
int SendMsg( int index, void *buf, size_t size, long msgtyp, int timout);

/*������Ϣ��sizeΪ���յ��ĳ��ȣ�msgtypΪ���յ���msgtyp*/
/*Return:	0	�ɹ�								 */
/*          -2	�ַ�������С�ڽ��յ��ĳ���			 */
/*			-1  recvʧ��							 */
int RecvMsg( int index, void *buf, size_t *size, long *msgtyp, int timout);

/*��һ����Ϣ���з�����Ϣ����ͬ�����շ���*/
int ClientRequestServer( int index, void *sndmsg, size_t sndlen, void *rcvmsg, size_t* rcvlen, int timout);


#endif

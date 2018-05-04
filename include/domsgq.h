/*******************************
	*CopyRight Cuihq
	*All Right Reserved
	*
	*文件名称: domsgq.h
	*摘要:
	*
	*作者: 崔洪清
	*日期: 2004/06/01
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

/*判断指定的msgq是否存在*/
mybool IsMsqExist( int index );

/*根据索引创建一个消息队列*/
int CreateMsq( int index );

/*根据索引删除一个消息队列*/
int RemoveMsq( int index );

/*取得消息队列消息的数目*/
int GetMsqNum( int index );

/*传送buf的内容，并得到传送的msgtyp，该msgtyp为pid值*/
int SendMsg( int index, void *buf, size_t size, long msgtyp, int timout);

/*接收消息，size为接收到的长度，msgtyp为接收到的msgtyp*/
/*Return:	0	成功								 */
/*          -2	字符串长度小于接收到的长度			 */
/*			-1  recv失败							 */
int RecvMsg( int index, void *buf, size_t *size, long *msgtyp, int timout);

/*向一个消息队列发送消息，并同步接收返回*/
int ClientRequestServer( int index, void *sndmsg, size_t sndlen, void *rcvmsg, size_t* rcvlen, int timout);


#endif

/*******************************
	*CopyRight Cuihq
	*All Right Reserved
	*
	*文件名称: domsgq.c
	*摘要:
	*
	*作者: 崔洪清
	*日期: 2004/06/01
*******************************/
#include "domsgq.h"

static int  AlarmFlag;
static void CatchSigAlarm(int sig);
static int GetMsgFileName(char *filename,pid_t pid);
static int MsgRcv(int index,MsgBuf *buf,size_t size,long msgtyp,int timout);
static int MsgSnd(int index, MsgBuf *buf, size_t size, int timout);


static void CatchSigAlarm( int sig )
{
   AlarmFlag = 1;
   signal( sig, CatchSigAlarm );
}


static int GetMsgFileName( char *filename, pid_t pid )
{
	static int			count = 1000;
	long				num;

	if( count > 9999 )
		count = 1000;
	num = pid;
	sprintf( filename, "/tmp/.tmp%ld%d.msg", num, count );
	count++;
	return 0;
}

mybool IsMsqExist( int index )
{
	int MsqId;

	MsqId = msgget( GetKey(index), 0 );
	if( MsqId < 0 )
		return FALSE;
	return TRUE;
}

int CreateMsq( int index )
{
	int ret;

	if( IsMsqExist( index ) )
		RemoveMsq( index );
	ret = msgget( GetKey(index), IPC_CREAT | PERMS );
	if( ret == -1 )
	{
		return -1;
	}
	return 0;
}

int RemoveMsq( int index )
{
	int MsqId;
	int ret;

	MsqId = msgget( GetKey(index), 0 );
	if( MsqId == -1 )
	{
		return -1;
	}
	ret = msgctl( MsqId, IPC_RMID, NULL );
	if( ret == -1 )
	{
		return -1;
	}
	return 0;
}

int GetMsqNum( int index )
{
	int MsqId;
	struct msqid_ds buf;

	MsqId = msgget( GetKey(index), 0 );
	if( MsqId == -1 )
	{
		return -1;
	}

	if( msgctl( MsqId, IPC_STAT, &buf ) == -1 )
	{
		return -1;
	}

	return buf.msg_qnum;
}

static int MsgRcv( int index, MsgBuf *buf, size_t size, long msgtyp, int timout )
{
	int r, MsqId;
	void (*OldAction)(int);

	MsqId = msgget( GetKey(index), 0 );
	if( MsqId == -1 )
	{
		return -1;
	}

	if( timout == 0 )
		r = msgrcv( MsqId, (struct msgbuf *)buf, size, msgtyp, IPC_NOWAIT );
	else if( timout == MSGQ_TIME_INFINITE )
		r = msgrcv( MsqId, buf, size, msgtyp, 0 );
	else
	{
		OldAction = signal(SIGALRM, CatchSigAlarm);
		AlarmFlag = 0;
		alarm(timout);
		r = msgrcv(MsqId, buf, size, msgtyp, 0);
		alarm(0);
		signal(SIGALRM, OldAction);
	}
	if( r == -1 )
	{
		if( AlarmFlag == 1 )	/*超时*/
			return -2;
		else
			return -1;
	}
	return r;
}

static int MsgSnd( int index, MsgBuf *buf, size_t size, int timout )
{
	int r, MsqId;
	void (*OldAction)(int);

	MsqId = msgget( GetKey(index), 0 );
	if( MsqId == -1 )
	{
		return -1;
	}

	if( timout == 0 )
		r = msgsnd( MsqId, buf, size, IPC_NOWAIT );
	else if( timout == MSGQ_TIME_INFINITE )
		r = msgsnd( MsqId, buf, size, 0 );
	else
	{
		OldAction = signal(SIGALRM, CatchSigAlarm);
		AlarmFlag = 0;
		alarm(timout);
		//printf( "MsqId = [%d], type = [%d], len = [%d], size = [%d]\n", MsqId, buf->msgtyp, buf->size, size );
		r = msgsnd( MsqId, buf, size, 0 );
		alarm(0);
		signal(SIGALRM, OldAction);
	}

	if( r == -1 )
	{
		if( AlarmFlag == 1 )	/*超时*/
			return -2;
		else
			return -1;
	}
	return 0;
}


int SendMsg( int index, void *buf, size_t size, long msgtyp, int timout )
{
	int				r,fd;
	pid_t			pid;
	MsgBuf			msgbuf;
	char			tmpStr[501];
	long			sendsize;

	msgbuf.msgtyp = msgtyp;
	msgbuf.size = size;

	if( size > MSGQ_MAX_SIZE )		 /* 对于长度较大的数据，将其写入文件 */
	{
		pid = getpid();
		memset( tmpStr, 0, sizeof(tmpStr) );
		GetMsgFileName( tmpStr, pid );
		fd = open( tmpStr, O_WRONLY|O_CREAT|O_TRUNC, 0644 );
		if( fd == -1 )
		{
			unlink( tmpStr );
			return -1;
		}
		if( write( fd, buf, size ) < 0 )
		{
			close( fd );
			unlink( tmpStr );
			return -1;
		}
		close( fd );
		strcpy( msgbuf.msgbuf, tmpStr );
		sendsize = sizeof(long) + sizeof(long) + strlen(tmpStr) + 1;
	}
	else
	{
		memcpy( msgbuf.msgbuf, buf, size );
		sendsize = sizeof(long) + sizeof(long) + size;
	}

	r = MsgSnd( index, &msgbuf, sendsize, timout );
	return r;
}

int RecvMsg( int index, void *buf, size_t *size, long *msgtyp, int timout )
{
	long		rcvsize;
	long		rcvmsgtyp;
	MsgBuf		msgbuf;
	int			r;
	int			fd;
	char		tmpStr[100];

	rcvsize = *size;
	rcvmsgtyp = *msgtyp;

	memset( buf, 0, *size );

	r = MsgRcv( index, &msgbuf, sizeof(MsgBuf), rcvmsgtyp, timout );
	if( r < 0 )
		return r;

	if( msgbuf.size > rcvsize )
	{
		return -99;
	}

	*msgtyp = msgbuf.msgtyp;
	//*size = msgbuf.size;
	memcpy(size,&msgbuf.size, sizeof(size_t));
	if( msgbuf.size > MSGQ_MAX_SIZE )
	{
		strcpy( tmpStr, msgbuf.msgbuf );
		fd = open( tmpStr, O_RDONLY );
		if( fd == -1 )
		{
			unlink( tmpStr );
			return -1;
		}
		if( read( fd, buf, msgbuf.size ) != msgbuf.size )
		{
			close( fd );
			unlink( tmpStr );
			return -1;
		}
		close( fd );
		unlink( tmpStr );
	}
	else
		memcpy( buf, msgbuf.msgbuf, msgbuf.size );

	return 0;
}


int ClientRequestServer( int index, void *sndmsg, size_t sndlen, void *rcvmsg, size_t *rcvlen, int timout)
{
	long msgtyp;
	int r;

	msgtyp = getpid();
	r = SendMsg( index, sndmsg, sndlen, msgtyp, timout );
	if( r < 0 )
		return r;

	r = RecvMsg( index, rcvmsg, rcvlen, &msgtyp, timout );
	if( r < 0 )
		return r;
	return 0;
}

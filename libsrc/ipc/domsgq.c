/*******************************
	*CopyRight Cuihq
	*All Right Reserved
	*
	*�ļ�����: domsgq.c
	*ժҪ:
	*
	*����: �޺���
	*����: 2004/06/01
*******************************/
#include "../../include/domsgq.h"
#include "../../include/mydebug.h"

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
		if( AlarmFlag == 1 )	/*��ʱ*/
			return -2;
		else
			return -1;
	}
	return r;
}
/*
void msg_stat(int msgid,struct msqid_ds msg_info)
{
        int reval;
        sleep(1);//ֻ��Ϊ�˺������??????�ķ���
        reval=msgctl(msgid,IPC_STAT,&msg_info);
        if(reval==-1)
        {
                    RunLog("get msg info error\n");
                        return;
        }
        RunLog("\n");
        RunLog("current number of bytes on queue is %d\n",msg_info.msg_cbytes);
        RunLog("number of messages in queue is %d\n",msg_info.msg_qnum);
        RunLog("max number of bytes on queue is %d\n",msg_info.msg_qbytes);
       
        RunLog("pid of last msgsnd is %d\n",msg_info.msg_lspid);
        RunLog("pid of last msgrcv is %d\n",msg_info.msg_lrpid);
        RunLog("last msgsnd time is %s", ctime(&(msg_info.msg_stime)));
        RunLog("last msgrcv time is %s", ctime(&(msg_info.msg_rtime)));
        RunLog("last change time is %s", ctime(&(msg_info.msg_ctime)));
        RunLog("msg uid is %d\n",msg_info.msg_perm.uid);
        RunLog("msg gid is %d\n",msg_info.msg_perm.gid);
}
*/
/*
 * Modify by Guojie Zhu @ 2009.5.14
 * �޸�˵��: ��ͬ����������ز�ͬ�������, ����msgsnd ִ��ʧ�ܵ� errno��ӳ��returnֵ��
 * ����ֵ: ��ֵ��ʾʧ��,int���ݵĵ͵ڶ��ֽ����ִ���λ�� �� 0 ��ɹ�
 * msgsnd's errno:
 *	#define EINTR            4      // Interrupted system call 
 *	#define EAGAIN          11      // Try again 
 * 	#define ENOMEM          12      // Out of memory 
 *	#define EACCES          13      // Permission denied 
 *	#define EFAULT          14      // Bad address 
 *	#define EINVAL          22      // Invalid argument 
 *	#define EIDRM           43      // Identifier removed 
 * msgget's errno:
 * 	#define ENOENT           2      // No such file or directory 
 * 	#define EEXIST          17      // File exists 
 *  	#define ENOSPC          28      // No space left on device 
 *
 */
static int MsgSnd( int index, MsgBuf *buf, size_t size, int timout )
{
	int r, MsqId;
	void (*OldAction)(int);

	MsqId = msgget( GetKey(index), 0 );
	if( MsqId == -1 )
	{
		return (-1 * errno);
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
		
		if( AlarmFlag == 1 ){	/*��ʱ*/
			struct msqid_ds msg_info;
			//msg_stat(MsqId, msg_info);
			r = msgsnd( MsqId, buf, size, 0 );
			if (r != -1)
				return 0;
			return (-256 + (-1 * errno));

		}
		else
			return (-512 + (-1 * errno));
	}

	return 0;
}


int SendMsg( int index, void *buf, size_t size, long msgtyp, int timout )
{
	int	r,fd;
	pid_t	pid;
	MsgBuf	msgbuf;
	char	tmpStr[501];
	long	sendsize;

	msgbuf.msgtyp = msgtyp;
	msgbuf.size = size;

	if( size > MSGQ_MAX_SIZE )		 /* ���ڳ��Ƚϴ�����ݣ�����д���ļ� */
	{
		pid = getpid();
		memset( tmpStr, 0, sizeof(tmpStr) );
		GetMsgFileName( tmpStr, pid );
		fd = open( tmpStr, O_WRONLY|O_CREAT|O_TRUNC, 0644 );
		if( fd == -1 )
		{
			unlink( tmpStr );
			return (-768 + (-1 * errno));
		}
		if( write( fd, buf, size ) < 0 )
		{
			close( fd );
			unlink( tmpStr );
			return (-1024 + (-1 * errno));
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

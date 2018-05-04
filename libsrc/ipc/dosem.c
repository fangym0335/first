/*******************************
	*CopyRight Cuihq
	*All Right Reserved
	*
	*文件名称: dosem.c
	*摘要:
	*
	*作者: 崔洪清
	*日期: 2004/06/01
*******************************/
#include "../../include/dosem.h"

mybool IsSemExist( int index )
{
	int SemId;

	SemId = semget( GetKey(index), 0, 0 );
	if( SemId == -1 )
		return FALSE;
	return TRUE;
}


int CreateSem( int index )
{
	int SemId;
	int	ret;

	union semun {
		int val;
		struct semid_ds *buf;
		ushort *array;
	};
	union semun semun;

	if( IsSemExist( index ) )
		RemoveSem( index );
	SemId = semget( GetKey(index), 1, IPC_CREAT | PERMS );
	if( SemId < 0 )
	{
		return -1;
	}
	semun.val = SEMINITVAL;
	ret = semctl( SemId, 0, SETVAL, semun );
	if( ret < 0 )
	{
		return -1;
	}
	return 0;
}

int RemoveSem( int index )
{
	int SemId;
	int ret;

	SemId = semget( GetKey(index), 0, 0 );
	if( SemId == -1 )
	{
		return -1;
	}
	ret = semctl( SemId, 0, IPC_RMID, 0 );
	if( ret < 0 )
	{
		return -1;
	}
	return 0;
}

int GetSemNum( int index )
{
	int SemId;
	int retnum;

	SemId = semget( GetKey(index), 0, 0 );
	if( SemId == -1 )
	{
		return -1;
	}

	retnum = semctl(SemId, 0, GETVAL, 0);
	if( retnum < 0 )
	{
		return -1;
	}
	return retnum;
}


int Act_P( int index )
{
	int SemId, r;
	struct sembuf buf;

	SemId = semget( GetKey(index), 0, 0 );
	if( SemId == -1 )
	{
		return -1;
	}

	buf.sem_op  = -1;
	buf.sem_flg = SEM_UNDO;
	buf.sem_num = 0;

	while(1)
	{
		r = semop( SemId, &buf, 1 );
		if( r == -1 && errno == EINTR )
			continue;
		break;
	}
	if( r == -1 )
	{
		return -1;
	}
	return 0;
}


int Act_V( int index )
{
	int SemId, r;
	struct sembuf buf;

	SemId = semget( GetKey(index), 0, 0 );

	if( SemId == -1 )
		return -1;

	buf.sem_op  = 1;
	buf.sem_flg = SEM_UNDO|IPC_NOWAIT;
	buf.sem_num = 0;

	while(1)
	{
		r = semop( SemId, &buf, 1 );
		if( r == -1 && errno == EINTR )
			continue;
		break;
	}
	if( r == -1 )
	{
		return -1;
	}
	return 0;
}





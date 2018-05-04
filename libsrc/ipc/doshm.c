/*******************************
	*CopyRight Cuihq
	*All Right Reserved
	*
	*文件名称: doshm.c
	*摘要:
	*
	*作者: 崔洪清
	*日期: 2004/06/01
*******************************/
#include "../../include/doshm.h"

mybool IsShmExist( int index )
{
	int ShmId;

	ShmId = shmget( GetKey(index), 0, 0 );
	if( ShmId < 0 )
		return FALSE;

	return TRUE;
}

char *AttachShm( int index )
{
	int ShmId;
	char *p;

	ShmId = shmget( GetKey(index), 0, 0 );
	if( ShmId == -1 )
	{
		return NULL;
	}
	p = (char *)shmat( ShmId, NULL, 0 );
	if( p == (char*) -1 )
	{
		return NULL;
	}
	else
		return p;
}

int DetachShm( char **p )
{
	int r;

	if( *p == NULL )
	{
		return -1;
	}
	r = shmdt( *p );
	if( r < 0 )
	{
		return -1;
	}
	*p = NULL;

	return 0;
}

int RemoveShm( int index )
{
	int ShmId;
	int ret;

	ShmId = shmget( GetKey( index ), 1, IPC_EXCL );
	if( ShmId < 0 )
	{
		return -1;
	}
	ret = shmctl( ShmId, IPC_RMID, 0 );
	if( ret < 0 )
	{
		return -1;
	}
	return 0;
}

int CreateShm( int index, int size )
{
	int shmId;
	char * ptrShm;

	ptrShm = AttachShm( index );
	if( ptrShm != NULL )
	{
		if( DetachShm( &ptrShm ) < 0 || RemoveShm( index ) < 0 )
			return -1;
	}
	shmId = shmget( GetKey(index), size, IPC_CREAT|IPC_EXCL|PERMS );
	if( shmId < 0 )
	{
		printf( "errno = [%d]\n", errno );
		return -1;
	}
	ptrShm = ( char* )shmat( shmId, (char*)0, 0 ) ;
	shmdt( ptrShm );

	return 0;
}

int CreateNewShm( int index, int size )
{
	int shmId;
	char * ptrShm;

	shmId = shmget( GetKey(index), size, IPC_CREAT|IPC_EXCL|PERMS );
	if( shmId < 0 )
	{
		return -1;
	}
	ptrShm = ( char* )shmat( shmId, (char*)0, 0 ) ;
	shmdt( ptrShm );

	return 0;
}

#include "memctl.h"

#ifdef MYDEBUG

static int AddInMemCtl( void*p, size_t size, const char *file, size_t line );
static void ChangeSizeInMemCtl( void *p, size_t size );
static void FreeNode( void *p );


static MemAlloc * gpMemAlloc = NULL;

/*��ʾ������ڴ�*/
void __ShowMemDetail()
{
	MemAlloc *pMemAlloc = gpMemAlloc;

	DMsg( "Show Memory Detail" );

	while( pMemAlloc != NULL )
	{
		DMsg("Addr = [%p], Size = [%ld], FILE = [%s], LINE = [%ld]", \
				pMemAlloc->pStart, pMemAlloc->size, pMemAlloc->file, pMemAlloc->line );

		pMemAlloc = pMemAlloc->pNext;
	}
}

/*������������һ���µĽڵ�*/
/*�����е����ݰ����ڴ��׵�ַ��С��������*/
/*	p 		������ڴ���׵�ַ	*/
/*	size 	������ڴ��С		*/
/*	file	�����ڴ���ļ�		*/
/*	line	�����ڴ����			*/
static int AddInMemCtl( void *p, size_t size, const char *file, size_t line )
{

	MemAlloc *pMemAlloc;
	MemAlloc *p1;
	MemAlloc *p2;

	Assert( p != NULL );
	Assert( size != 0 );

	pMemAlloc = ( MemAlloc* )malloc( sizeof(MemAlloc) );	/*����һ���ڵ�*/
	if( pMemAlloc == NULL )
		return -1;

	pMemAlloc->pStart = p;
	pMemAlloc->size = size;
	strncpy( pMemAlloc->file, file, __FILELEN );
	pMemAlloc->file[__FILELEN] = 0;
	pMemAlloc->line = line;
	pMemAlloc->pNext = NULL;

	if( gpMemAlloc == NULL )	/*������*/
		gpMemAlloc = pMemAlloc;
	else if( gpMemAlloc->pStart > pMemAlloc->pStart )	/*�·�����ڴ��׵�ַ����������С�ĵ�ַС*/
	{
		/*����һ���ڴ���׵�ַ���ϸÿ��ڴ�Ĵ�С���ܴ���������������ڴ���׵�ַ*/
		Assert( (char*)pMemAlloc->pStart + pMemAlloc->size <= (char*)gpMemAlloc->pStart );
		pMemAlloc->pNext = gpMemAlloc;
		gpMemAlloc = pMemAlloc;
	}
	else	/*Ϊ�·���Ľڵ�Ѱ��λ��*/
	{
		for( p1 = gpMemAlloc->pNext, p2 = gpMemAlloc; p1 != NULL && p1->pStart < pMemAlloc->pStart; p1 = p1->pNext )
			p2 = p1;
		Assert( (char*)p2->pStart + p2->size <= (char*)pMemAlloc->pStart );
		Assert( p1 == NULL || (char*)pMemAlloc->pStart + pMemAlloc->size <= (char*)p1->pStart );
		p2->pNext = pMemAlloc;
		pMemAlloc->pNext = p1;
	}
	return 0;

}

/*����ĳ���ڵ��Ӧ���ڴ�Ĵ�С*/
static void ChangeSizeInMemCtl( void *p, size_t size )
{
	MemAlloc *p1;

	Assert( p != NULL );
	Assert( gpMemAlloc != NULL );

	p1 = gpMemAlloc;

	while( p1 != NULL )
	{
		Assert( p1->pStart <= p );	/*��pStart��С��������*/

		if( p1->pStart == p )
		{
			p1->size = size;
			return;
		}
		p1 = p1->pNext;
	}
	Assert( 0 );	/*δ�ҵ���¼*/
	return;

}

/*�ͷ������е��׵�ַΪp��һ���ڵ�*/
static void FreeNode( void *p )
{
	MemAlloc *p1, *p2;

	Assert( p != NULL );
	Assert( gpMemAlloc != NULL );

	if( gpMemAlloc->pStart == p )
	{
		p1 = gpMemAlloc;
		gpMemAlloc = gpMemAlloc->pNext;
		memset( p1, bFreeGarbage, sizeof(MemAlloc) );
		free(p1);
		return;
	}
	else if( gpMemAlloc->pStart > p )
	{
		Assert( 0 );
		return;
	}
	else
	{
		p1 = gpMemAlloc->pNext;
		p2 = gpMemAlloc;

		while( p1 != NULL )
		{
			Assert( p1->pStart <= p );
			if( p1->pStart == p )
			{
				p2->pNext = p1->pNext;
				memset( p1, bFreeGarbage, sizeof(MemAlloc) );
				free(p1);
				return;
			}
			p2 = p1;
			p1 = p1->pNext;
		}
	}
	Assert( 0 );

	return;
}


void __Free( void *p )
{
	MemAlloc *p1, *p2;

	Assert( p != NULL );
	Assert( gpMemAlloc != NULL );
	assert( gpMemAlloc->pStart<=p );

	if( gpMemAlloc->pStart == p )
	{
		p1 = gpMemAlloc;
		gpMemAlloc = gpMemAlloc->pNext;
		memset( p, bFreeGarbage, p1->size );
		memset( p1, bFreeGarbage, sizeof(MemAlloc) );
		free(p1);
		free(p);
		return;
	}
	else
	{
		p1 = gpMemAlloc->pNext;
		p2 = gpMemAlloc;

		while( p1 != NULL )
		{
			Assert( p1->pStart <= p );
			if( p1->pStart == p )
			{
				p2->pNext = p1->pNext;
				memset( p, bFreeGarbage, p1->size );
				memset( p1, bFreeGarbage, sizeof(MemAlloc) );
				free(p1);
				free(p);
				return;
			}
			p2 = p1;
			p1 = p1->pNext;
		}
	}
	//Assert( 0 ); //test
	Assert( 1 ); //test?
	return;
}


/*����һ���ڴ�*/
void *__Malloc( size_t size, const char *file, size_t line )
{
	void * p;

	if( ( p = malloc( size ) ) == NULL )
		return NULL;

	memset( p, bNewGarbage, size );

	if( AddInMemCtl( p, size, file, line ) < 0 )
	{
		free(p);
		return NULL;
	}
	return p;
}

void *__Realloc( void *p, size_t size, const char *file, size_t line )
{
	void *pTmp;

	pTmp = p;

	if( ( p = realloc( p, size ) ) == NULL )
	{
		Free( pTmp );
		return NULL;
	}

	if( p == pTmp )
	{
 		ChangeSizeInMemCtl( p, size );
	}
	else
	{
		if( pTmp != NULL )
			FreeNode( pTmp );/*realloc�ķ��ص�ַ��ԭ�Ȳ�ͬ.��ԭ���ĵ�ַ�Ѿ���ϵͳ�ͷ�.���Դ˴�������ԭ��ַ�Ľڵ��ͷ�*/
		if( AddInMemCtl( p, size, file, line ) < 0 )
		{
			free(p);
			return NULL;
		}
	}
	return p;
}

void *__Calloc( size_t count, size_t size, const char *file, size_t line )
{
	void * p;

	if( ( p = calloc( count, size ) ) == NULL )
		return NULL;

	memset( p, bNewGarbage, size );

	if( AddInMemCtl( p, count * size, file, line ) < 0 )
	{
		free(p);
		return NULL;
	}
	return p;
}

/*�����˳�ʱ����Ƿ���δ�ͷ��ڴ�*/
void CheckMemCtl()
{
	if( gpMemAlloc != NULL )
	{
		DMsg( "������δ�ͷ��ڴ�" );
		ShowMemDetail();
	}
	return;
}

void __InitMemCtl()
{
	atexit( CheckMemCtl );
}

#endif /*MYDEBUG*/

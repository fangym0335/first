#include "memctl.h"

#ifdef MYDEBUG

static int AddInMemCtl( void*p, size_t size, const char *file, size_t line );
static void ChangeSizeInMemCtl( void *p, size_t size );
static void FreeNode( void *p );


static MemAlloc * gpMemAlloc = NULL;

/*显示分配的内存*/
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

/*在链表里增加一个新的节点*/
/*链表中的数据按照内存首地址从小到大排序*/
/*	p 		分配的内存的首地址	*/
/*	size 	分配的内存大小		*/
/*	file	分配内存的文件		*/
/*	line	分配内存的行			*/
static int AddInMemCtl( void *p, size_t size, const char *file, size_t line )
{

	MemAlloc *pMemAlloc;
	MemAlloc *p1;
	MemAlloc *p2;

	Assert( p != NULL );
	Assert( size != 0 );

	pMemAlloc = ( MemAlloc* )malloc( sizeof(MemAlloc) );	/*分配一个节点*/
	if( pMemAlloc == NULL )
		return -1;

	pMemAlloc->pStart = p;
	pMemAlloc->size = size;
	strncpy( pMemAlloc->file, file, __FILELEN );
	pMemAlloc->file[__FILELEN] = 0;
	pMemAlloc->line = line;
	pMemAlloc->pNext = NULL;

	if( gpMemAlloc == NULL )	/*空链表*/
		gpMemAlloc = pMemAlloc;
	else if( gpMemAlloc->pStart > pMemAlloc->pStart )	/*新分配的内存首地址比链表中最小的地址小*/
	{
		/*任意一块内存的首地址加上该块内存的大小不能大于排在他后面的内存的首地址*/
		Assert( (char*)pMemAlloc->pStart + pMemAlloc->size <= (char*)gpMemAlloc->pStart );
		pMemAlloc->pNext = gpMemAlloc;
		gpMemAlloc = pMemAlloc;
	}
	else	/*为新分配的节点寻找位置*/
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

/*更改某个节点对应的内存的大小*/
static void ChangeSizeInMemCtl( void *p, size_t size )
{
	MemAlloc *p1;

	Assert( p != NULL );
	Assert( gpMemAlloc != NULL );

	p1 = gpMemAlloc;

	while( p1 != NULL )
	{
		Assert( p1->pStart <= p );	/*按pStart由小到大排序*/

		if( p1->pStart == p )
		{
			p1->size = size;
			return;
		}
		p1 = p1->pNext;
	}
	Assert( 0 );	/*未找到记录*/
	return;

}

/*释放链表中的首地址为p的一个节点*/
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


/*分配一块内存*/
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
			FreeNode( pTmp );/*realloc的返回地址与原先不同.则原来的地址已经被系统释放.所以此处将包含原地址的节点释放*/
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

/*进程退出时检查是否有未释放内存*/
void CheckMemCtl()
{
	if( gpMemAlloc != NULL )
	{
		DMsg( "进程有未释放内存" );
		ShowMemDetail();
	}
	return;
}

void __InitMemCtl()
{
	atexit( CheckMemCtl );
}

#endif /*MYDEBUG*/

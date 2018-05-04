#ifndef __MEMCTL_H__
#define __MEMCTL_H__

#include <stdlib.h>
#include "mydebug.h"

#if defined( MYDEBUG )

#ifndef __FILELEN
#define __FILELEN	20			/*为节省空间,此值定义的比较小.有可能被截断*/
#endif /*__FILELEN*/

/*定义一个内存管理的链表.链表的每一节点用来记录分配的内存的起始地址,大小,分配内存所在的文件,行*/
/*当调用Malloc,Calloc新分配内存时,增加一个新的节点*/
/*当调用Free释放一块内存时,删除原先的节点*/
/*当调用Realloc更改一块内存时,如果原地址为空,则新增一个节点,否则找到原节点,更改size的大小*/

typedef struct memAlloc{
	void *	pStart;				/*起始地址*/
	size_t 	size;				/*大小*/
	char   	file[__FILELEN + 1];/*分配内存所在的文件*/
	size_t 	line;				/*分配内存所在的行*/
	struct memAlloc *pNext;
}MemAlloc;

void __ShowMemDetail();
void __Free( void *p );
void __InitMemCtl();
void CheckMemCtl();

void *__Malloc( size_t size, const char *file, size_t line );
void *__Calloc( size_t count, size_t size, const char *file, size_t line );
void *__Realloc( void *p, size_t size, const char *file, size_t line );

#define Free( p ) __Free(p), (p) = NULL
#define ShowMemDetail() __ShowMemDetail()
#define InitMemCtl()	__InitMemCtl()

#define Malloc( size ) __Malloc( (size), __FILE__, __LINE__ )
#define Calloc( count, size ) __Calloc( (count), (size), __FILE__, __LINE__ )
#define Realloc( p, size ) __Realloc( (p), (size), __FILE__, __LINE__ )

#else

#define Free( p ) free(p), (p) = NULL
#define ShowMemDetail() (void)(0)
#define InitMemCtl()	(void)(0)

#define Malloc( size ) malloc( (size) )
#define Calloc( count, size ) calloc( (count), (size) )
#define Realloc( p, size ) realloc( (p), (size) )

#endif /*MYDEBUG*/

#endif /*__MEMCTL_H__*/

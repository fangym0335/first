#ifndef __MEMCTL_H__
#define __MEMCTL_H__

#include <stdlib.h>
#include "mydebug.h"

#if defined( MYDEBUG )

#ifndef __FILELEN
#define __FILELEN	20			/*Ϊ��ʡ�ռ�,��ֵ����ıȽ�С.�п��ܱ��ض�*/
#endif /*__FILELEN*/

/*����һ���ڴ���������.�����ÿһ�ڵ�������¼������ڴ����ʼ��ַ,��С,�����ڴ����ڵ��ļ�,��*/
/*������Malloc,Calloc�·����ڴ�ʱ,����һ���µĽڵ�*/
/*������Free�ͷ�һ���ڴ�ʱ,ɾ��ԭ�ȵĽڵ�*/
/*������Realloc����һ���ڴ�ʱ,���ԭ��ַΪ��,������һ���ڵ�,�����ҵ�ԭ�ڵ�,����size�Ĵ�С*/

typedef struct memAlloc{
	void *	pStart;				/*��ʼ��ַ*/
	size_t 	size;				/*��С*/
	char   	file[__FILELEN + 1];/*�����ڴ����ڵ��ļ�*/
	size_t 	line;				/*�����ڴ����ڵ���*/
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

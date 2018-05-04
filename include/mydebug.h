/**********************************************
	*CopyRight Cuihq
	*All Right Reserved
	*
	*�ļ�����:mydebug.h
	*ժҪ:
	*
	*����: �޺���
	*����: 2004/07/01

***********************************************/
#ifndef __MYDEBUG_H__
#define __MYDEBUG_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include "commdef.h"


#undef MyAssert
#undef MyDebug

/*��������*/
void __WriteDebugMsg( const char *fmt, ... );

#define DMsg 				__WriteDebugMsg

#if !defined( MYDEBUG ) /*���δ����MYDEBUG*/

#ifndef NDEBUG
#define NDEBUG	/*���û�ж���MYDEBUG,����Ҫ����NDEBUG,����assert��*/
#endif /*NDEBUG*/

#define MyDebug(ignore)		((void)0)
#define Assert(ignore)		((void)0)
#define Trace(a)			(a)

#else

#undef NDEBUG	/*���������MYDEBUG,��ȡ����NDEBUG�Ķ���*/

/*��������*/
void SetFileAndLine( const char *filename, int line );
void __Assert( const char *exp, const char *filename, int line );
void __MyDebug( const char * msg );

#define MyDebug(a) 			SetFileAndLine( __FILE__, __LINE__ ), __MyDebug(a)
#define Assert(__EX) 		((__EX) ? ((void)0) : __Assert(#__EX, __FILE__, __LINE__ ))
#define Trace(a) 			DMsg("%s",#a), (a)

#endif	/*MYDEBUG*/

#include <assert.h>

#if defined( MYDEBUG )

#define bNewGarbage			0xA3
#define bFreeGarbage		0xA5

#else

#define bNewGarbage			0x00
#define bFreeGarbage		0x00

#endif /*MYDEBUG*/


#define GetVarMsg( pMsg, ap, fmt ) { va_start( ap, fmt ); pMsg = __GetVarMsg( fmt, ap ); va_end( ap ); }

char * __GetVarMsg( const char *fmt, va_list ap );
void PMsg( const char *fmt, ... );

#endif /*__MYDEBUG_H__*/

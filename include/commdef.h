/**********************************************
	*CopyRight Cuihq
	*All Right Reserved
	*
	*文件名称:commdef.h
	*摘要:
	*	本文件包含了自定义函数库所用到的一些基本信息
	*
	*作者: 崔洪清
	*日期: 2004/07/01
***********************************************/
#ifndef __COMMDEF_H__
#define __COMMDEF_H__
#include <sys/types.h>
#include <limits.h>

#define SOFTNAME		"CHQ"

#define __LINUX__
/*
#define __SCO__
*/
/*
#define __AIX__
*/

#define MYDEBUG	 						/*定义该宏可以显示调试及诊断信息*/

#ifndef TRUE
#define TRUE			1
#endif

#ifndef FALSE
#define FALSE			0
#endif

#ifndef VALID
#define VALID			1
#endif

#ifndef INVALID
#define INVALID			0
#endif

#ifdef NAME_MAX
#define MAX_FILE_NAME_LEN	NAME_MAX
#else
#define MAX_FILE_NAME_LEN	255		/*文件名的最大长度*/
#endif	/*NAME_MAX*/

#ifdef PATH_MAX
#define MAX_PATH_NAME_LEN	PATH_MAX + 1	/*PATH_MAX为相对长度,需要加上1 ?*/
#else
#define MAX_PATH_NAME_LEN	1024 	/*路径名的最大长度*/
#endif	/*PATH_MAX*/

#ifdef OPEN_MAX
#define MAX_FILE_OPEN		OPEN_MAX
#else
#define MAX_FILE_OPEN		110		/*定义每个进程能打开的文件描述符的数目*/
#endif	/*OPEN_MAX*/

#define MAX_VARIABLE_PARA_LEN	1024 	/*可变参数的最大长度*/

#define TBUFFSIZE		64		/*很小的字符串*/
#define SBUFFSIZE		256		/*短字符串的长度*/
#define MBUFFSIZE		1024	/*中等字符串长度*/
#define LBUFFSIZE		8096	/*大字符串长度*/
#define HBUFFSIZE		20480	/*超大的字符串*/

#define CONST_ZERO  0.0000001	/*如果一个浮点数小于等于此值,则认为此浮点数为0*/

#define TIME_INFINITE  -1			/*当设置闹钟时，如果将该值做为闹钟时间传入，则闹钟不起动*/


typedef signed short		sshort;
typedef signed int		sint;
typedef signed long		slong;

#define CONSOLE			"/dev/console"	/*控制台文件*/
#define ENVFILE			"env.cfg"

#define max( a, b ) 	( (a) > (b) )? (a) : (b)

typedef unsigned char		mybool;		/*TURE or FALSE*/
typedef unsigned char		myflag;		/*简单的标志*/
typedef void Sigfunc( int );			/*Sigfunc表示一个返回值为void,参数为int的函数*/
										/*定义该类型可以简化signal函数的书写*/

#endif	/*__COMMDEF_H__*/

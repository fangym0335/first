/**********************************************
	*CopyRight Cuihq
	*All Right Reserved
	*
	*�ļ�����:commdef.h
	*ժҪ:
	*	���ļ��������Զ��庯�������õ���һЩ������Ϣ
	*
	*����: �޺���
	*����: 2004/07/01
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

#define MYDEBUG	 						/*����ú������ʾ���Լ������Ϣ*/

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
#define MAX_FILE_NAME_LEN	255		/*�ļ�������󳤶�*/
#endif	/*NAME_MAX*/

#ifdef PATH_MAX
#define MAX_PATH_NAME_LEN	PATH_MAX + 1	/*PATH_MAXΪ��Գ���,��Ҫ����1 ?*/
#else
#define MAX_PATH_NAME_LEN	1024 	/*·��������󳤶�*/
#endif	/*PATH_MAX*/

#ifdef OPEN_MAX
#define MAX_FILE_OPEN		OPEN_MAX
#else
#define MAX_FILE_OPEN		110		/*����ÿ�������ܴ򿪵��ļ�����������Ŀ*/
#endif	/*OPEN_MAX*/

#define MAX_VARIABLE_PARA_LEN	1024 	/*�ɱ��������󳤶�*/

#define TBUFFSIZE		64		/*��С���ַ���*/
#define SBUFFSIZE		256		/*���ַ����ĳ���*/
#define MBUFFSIZE		1024	/*�е��ַ�������*/
#define LBUFFSIZE		8096	/*���ַ�������*/
#define HBUFFSIZE		20480	/*������ַ���*/

#define CONST_ZERO  0.0000001	/*���һ��������С�ڵ��ڴ�ֵ,����Ϊ�˸�����Ϊ0*/

#define TIME_INFINITE  -1			/*����������ʱ���������ֵ��Ϊ����ʱ�䴫�룬�����Ӳ���*/


typedef signed short		sshort;
typedef signed int		sint;
typedef signed long		slong;

#define CONSOLE			"/dev/console"	/*����̨�ļ�*/
#define ENVFILE			"env.cfg"

#define max( a, b ) 	( (a) > (b) )? (a) : (b)

typedef unsigned char		mybool;		/*TURE or FALSE*/
typedef unsigned char		myflag;		/*�򵥵ı�־*/
typedef void Sigfunc( int );			/*Sigfunc��ʾһ������ֵΪvoid,����Ϊint�ĺ���*/
										/*��������Ϳ��Լ�signal��������д*/

#endif	/*__COMMDEF_H__*/

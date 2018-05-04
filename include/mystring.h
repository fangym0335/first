#ifndef __MYSTRING_H__
#define __MYSTRING_H__

#include <string.h>
#include <stdarg.h>
#include "mycomm.h"
#include "dostring.h"

typedef struct{
	size_t	size;
	size_t	len;
	char *	str;
}String;

#ifndef __GPCOMMAREA__
extern String *gpCommArea;
#endif


/*����һ���µ�String.�ڽ�����ʱ����Ҫ��DelStringɾ����String*/
String* NewString( size_t size );
/*ɾ��һ��String*/
void DelString( String **pstStr );
/*����String���������*/
void ClearString( String *pstStr );
/*��һ��String��ֵ.fmtΪ�ɱ����*/
int CommStringAssign( String *pstStr, const char *fmt, ... );
/*��һ��String�������ַ���.fmtΪ�ɱ����*/
int CommStringAdd( String *pstStr, const char *fmt, ... );
/*��һ��String��ֵ*/
int StringAssign( String *pstStr, const char *str, size_t len );
/*��һ��String�������ַ���*/
int StringAdd( String *pstStr, const char *str, size_t len );
/*��ʾString����*/
void ShowString( String *pString );
/*ȡString�Ĵ�С*/
size_t GetStringSize( String *pString );
/*ȡString��ֵ*/
char* GetStringValue( String *pString );

/*��ʼ��������*/
String* InitCommArea( size_t size );
/*ָ�������Ļ�����*/
void SelectWorkingCommArea( String *pString );
/*���������.����󻺳������ٿ���*/
void DelCommArea( );
/*��ջ�����������*/
void ClearCommArea( );
/*���ݼ�ֵɾ������*/
int DelKey( const char *sKey );
/*���ü�ֵ������*/
int SetValue( const char *sKey, const char *sValue );
/*���ݼ�ֵȡ����*/
int GetValue( const char *sKey, char *sValue, size_t iValueSize );


#endif

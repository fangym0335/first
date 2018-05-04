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


/*产生一个新的String.在结束的时候需要用DelString删除该String*/
String* NewString( size_t size );
/*删除一个String*/
void DelString( String **pstStr );
/*将该String的数据清空*/
void ClearString( String *pstStr );
/*对一个String赋值.fmt为可变参数*/
int CommStringAssign( String *pstStr, const char *fmt, ... );
/*在一个String后增加字符串.fmt为可变参数*/
int CommStringAdd( String *pstStr, const char *fmt, ... );
/*对一个String赋值*/
int StringAssign( String *pstStr, const char *str, size_t len );
/*在一个String后增加字符串*/
int StringAdd( String *pstStr, const char *str, size_t len );
/*显示String内容*/
void ShowString( String *pString );
/*取String的大小*/
size_t GetStringSize( String *pString );
/*取String的值*/
char* GetStringValue( String *pString );

/*初始化缓冲区*/
String* InitCommArea( size_t size );
/*指定工作的缓冲区*/
void SelectWorkingCommArea( String *pString );
/*清除缓冲区.清除后缓冲区不再可用*/
void DelCommArea( );
/*清空缓冲区的数据*/
void ClearCommArea( );
/*根据键值删除数据*/
int DelKey( const char *sKey );
/*设置键值及数据*/
int SetValue( const char *sKey, const char *sValue );
/*根据键值取数据*/
int GetValue( const char *sKey, char *sValue, size_t iValueSize );


#endif

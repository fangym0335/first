/*******************************
	*CopyRight Cuihq
	*All Right Reserved
	*
	*文件名称: dostring.h
	*摘要:
	*
	*作者: 崔洪清
	*日期: 2004/06/01
*******************************/
#ifndef __DOSTRING_H__
#define __DOSTRING_H__

#include <string.h>
#include <stdarg.h>
#include "mycomm.h"


#define FILLBEHIND		0		/*在字符串尾部填充*/
#define FILLAHEAD		1		/*在字符串头部填充*/

/*判断字符串是否为空*/
int IsEmptyString( const char *s );

/*去除字符串两侧空格.需要字符串带结束符*/
char *AllTrim( char *sBuf );

/*去除字符串右侧空格,需要字符串带结束符*/
char *RightTrim( char *buf );

/*去除字符串左侧空格.需要字符串带结束符*/
char *LeftTrim( char *buf );

/*将字符串变为小写,需要字符串带结束符*/
char *StrToLower( char *s );

/*将字符串变为大写,需要字符串带结束符*/
char *StrToUpper( char *s );

/*将字符串变为小写,需要字符串带结束符*/
char *ToLower( const char *src, char *obj );

/*将字符串变为大写,需要字符串带结束符*/
char *ToUpper( const char *src, char *obj );

/***************************************************

	该函数用指定的字符填充字符串到指定的长度
	buff:	字符串
	length:	填充到的长度
	fillChr:填充的字符
	flag:	当值为FILLBEHIND时,在字符串的尾部填充
			当值为FILLAHEAD时,在字符串的头部填充

	备注:
		当选择在字符串前面填充时,因为分配临时串,所以有可能内存溢出.
		内存溢出时返回-1
***************************************************/

int FillBuff( char *buff, size_t length, char fillChr, myflag flag );

/***************************************************

	该函数判断字符串长度是否是某个数字的整数倍,
	如果不是,则填充该字符串

	buff:	字符串
	mod:	模.填充后的字符串为该模的整数倍.
	fillChr:填充的字符
	flag:	当值为FILLBEHIND时,在字符串的尾部填充
			当值为FILLAHEAD时,在字符串的头部填充
	备注:
	1.	当选择在字符串前面填充时,因为分配临时串,所以有可能内存溢出.
		内存溢出时返回-1
	2.	请分配足够大的buff,以容纳填充后的串
***************************************************/

int FillBuffWithMod( char *buff, uint mod, char fillChr, myflag flag );

/*处理可变参数*/
/*该函数用到了静态变量,不是线程安全.对进程来说,需要在下次调用该函数前保存上次调用结果*/

char *DoValist( const char *fmt, va_list pvar );

#endif

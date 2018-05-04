/*******************************
	*CopyRight Cuihq
	*All Right Reserved
	*
	*文件名称: dostring.c
	*摘要:
	*
	*作者: 崔洪清
	*日期: 2004/06/01
*******************************/
#include "dostring.h"

/*判断字符串是否为空*/
int IsEmptyString( const char *s )
{
	if( s == NULL || *s == 0 )
		return  TRUE;
	else
		return FALSE;
}

/*去除字符串右侧空格.需要字符串带结束符*/
char *RightTrim( char *buf )
{
	int		i;

	for( i = strlen(buf) - 1; i >= 0 && ( buf[i] == ' ' || buf[i] == '\t'); i-- )
		buf[i] = 0;
	return buf;
}

/*去除字符串左侧空格.需要字符串带结束符*/
char *LeftTrim( char * buf )
{
	int i, firstpos;

	for( firstpos = 0; firstpos < strlen( buf ); firstpos++ )
		if( buf[firstpos] != ' ' && buf[firstpos] != '\t' )
			break;
	i = firstpos;

	for( ; i < strlen( buf ); i++ )
		buf[ i - firstpos ] = buf[ i ];
	buf[ i - firstpos ] = 0;

	return buf;
}

/*去除字符串两侧空格,需要字符串带结束符*/
char *AllTrim( char *buf )
{
	int i,firstchar,endpos,firstpos;

	Assert( buf != NULL );

	endpos = firstchar = firstpos = 0;
	for( i = 0; buf[ i ] != '\0'; i++ )
	{
		if( buf[i] ==' ' || buf[i] == '\t')
		{
			if( firstchar == 0 )
				firstpos++;
		}
		else
		{
			endpos = i;
			firstchar = 1;
		}
	}

	for( i = firstpos; i <= endpos; i++ )
		buf[i - firstpos] = buf[i];
	buf[i - firstpos] = 0;
	return buf;
}

/*将字符串变为小写,需要字符串带结束符*/
char *StrToLower( char *s )
{
	int		i;

	for(i = 0; i < strlen(s); i++)
		if( s[i] >= 'A' && s[i] <= 'Z' )
			s[i] += 'a' - 'A';
	return s;
}

/*将字符串变为大写,需要字符串带结束符*/
char *StrToUpper( char *s )
{
	int		i;

	for(i = 0; i < strlen(s); i++)
		if( s[i] >= 'a' && s[i] <= 'z' )
			s[i] -= 'a' - 'A';
	return s;
}

/*将字符串变为小写,需要字符串带结束符*/
char *ToLower( const char *src, char *obj )
{
	int		i;

	for(i = 0; i < strlen(src); i++)
		if( src[i] >= 'A' && src[i] <= 'Z' )
			obj[i] += 'a' - 'A';
	obj[i] = 0;
	return obj;
}

/*将字符串变为大写,需要字符串带结束符*/
char *ToUpper( const char *src, char *obj )
{
	int		i;

	for(i = 0; i < strlen(src); i++)
		if( src[i] >= 'a' && src[i] <= 'z' )
			obj[i] -= 'a' - 'A';
	obj[i] = 0;
	return obj;
}

/***************************************************

	该函数用指定的字符填充字符串到指定的长度
	buff:	字符串
	length:	填充到的长度
	fillChr:填充的字符
	flag:	当值为FILLBEHIND时,在字符串的尾部填充
			当值为FILLAHEAD时,在字符串的头部填充

	备注:
	1.	当选择在字符串前面填充时,因为分配临时串,所以有可能内存溢出.
		内存溢出时返回-1
	2.	请分配足够大的buff,以容纳填充后的串
***************************************************/

int FillBuff( char *buff, size_t length, char fillChr, myflag flag )
{
	size_t	i;
	char	*tmpBuff;

	Assert( buff != NULL );
	Assert( length != 0 );
	Assert( flag == FILLBEHIND || flag == FILLAHEAD );

	if( strlen(buff) >= length ) /*字符串长度比目标长度大,不需要填充*/
		return 0;


	if ( flag == FILLBEHIND )
	{
		for ( i = strlen( buff ); i < length; i++ )
			buff[ i ] = fillChr;
		buff[ i ] = 0;
	}
	else if ( flag == FILLAHEAD )
	{
		if( ( tmpBuff = ( char * ) Malloc( length + 1 ) ) == NULL )
		{
			RptLibErr( errno );
			return -1;
		}
		memset( tmpBuff, fillChr, length );
		strcpy( tmpBuff + length - strlen( buff ), buff );
		strcpy( buff, tmpBuff );
		Free( tmpBuff );
	}

	return 0;
}

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

int FillBuffWithMod( char *buff, uint mod, char fillChr, myflag flag )
{
	size_t		i;
	size_t		fillLen;
	size_t		buffLen;
	char		*tmpBuff;

	Assert( buff != NULL );
	Assert( mod != 0 );
	Assert( flag == FILLBEHIND || flag == FILLAHEAD );

	buffLen = strlen( buff );

	if ( buffLen % mod == 0 )
		return 0;
	else
		 fillLen = mod -  buffLen % mod;


	if ( flag == FILLBEHIND )
	{
		for ( i = strlen( buff ); i < buffLen + fillLen ; i++ )
			buff[ i ] = fillChr;
		buff[ i ] = 0;
	}
	else if ( flag == FILLAHEAD )
	{
		if ( ( tmpBuff = ( char * ) Malloc( buffLen + fillLen + 1 ) ) == NULL )
		{
			RptLibErr( errno );
			return -1;
		}
		memset( tmpBuff, fillChr, buffLen + fillLen );
		strcpy( tmpBuff + fillLen, buff );
		strcpy( buff, tmpBuff );
		Free( tmpBuff );
	}
	return 0;
}

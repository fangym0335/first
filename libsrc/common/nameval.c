/******************************************************************

用于操纵name=value数据
数据在串中的存放格式时
&name1=value1&name2=value2&name3=value3
如果name或value中存在&或者=,则&或者=号被转义存放.&转换为%26,=转换为%3D.
	自然,用于转义的%也需要被转义.它被转为%25


*******************************************************************/

#include "nameval.h"
static void DecodeStr( const char *sSource, char *pStr, size_t iMaxSize );
static void DecodeStrByLen( const char *sSource, size_t iSourceLen, char *pStr, size_t iMaxSize );
static void EncodeStr( const char *sSource, char *pStr, size_t iMaxSize );
static void EncodeStrByLen( const char *sSource, size_t iSourceLen, char *pStr, size_t iMaxSize );


/*将字符串sSource中的内容解码后存放在字符串pStr中.iMaxLen为字符串最大容纳的的数据长度*/
static void DecodeStr( const char *sSource, char *pStr, size_t iMaxSize )
{
	Assert( sSource != NULL && *sSource != 0 );
	Assert( pStr != 0 );
	Assert( iMaxSize > 0 );

	DecodeStrByLen( sSource, strlen( sSource ), pStr, iMaxSize );
	return;
}
/*将从sSource开始,iSourceLen长度的字串内容解码存放在字串pStr中.iMaxLen为字符串最大容纳的的数据长度*/
static void DecodeStrByLen( const char *sSource, size_t iSourceLen, char *pStr, size_t iMaxSize )
{
	const char	*p;
	char 		sTmp[4];

	Assert( sSource != NULL && *sSource != 0 );
	Assert( pStr != NULL );
	Assert( iSourceLen > 0 && iMaxSize > 0 );

	*pStr = 0;
	sTmp[1] = 0;

	for( p = sSource; p < sSource + iSourceLen; )
	{
		if( '%' == *p )
		{
			if( strncmp( p, "%25", 3 ) == 0 )
				sTmp[0] = '%';
			else if( strncmp( p, "%3D", 3 ) == 0 )
				sTmp[0] = '=';
			else if( strncmp( p, "%26", 3 ) == 0 )
				sTmp[0] = '&';

			p = p + 3;
		}
		else
		{
			sTmp[0] = *p;
			p = p + 1;
		}

		Assert( iMaxSize >= strlen(pStr) + strlen(sTmp) + 1 );

		strcat( pStr, sTmp );
	}
	return;
}

static void EncodeStr( const char *sSource, char *pStr, size_t iMaxSize )
{
	Assert( sSource != NULL && *sSource != 0 );
	Assert( pStr != NULL );
	Assert( iMaxSize > 0 );

	EncodeStrByLen( sSource, strlen( sSource ), pStr, iMaxSize );
	return;
}

/*将sSource的值中的特殊字符转义,并将结果存于pStr中*/
static void EncodeStrByLen( const char *sSource, size_t iSourceLen, char *pStr, size_t iMaxSize )
{
	const char	*p;
	char		sTmp[4];

	Assert( sSource != NULL );
	Assert( pStr != NULL );
	Assert( iSourceLen > 0 && iMaxSize > 0 );

	*pStr = 0;
	for( p = sSource; p < sSource + iSourceLen; p++ )
	{
		switch( *p )
		{
			case '%':						/* %用来转义=和&,所以本身需要转义 */
				strcpy( sTmp, "%25" );
				break;
			case '=':						/* =用来连接key和value,需要转义*/
				strcpy( sTmp, "%3D" );
				break;
			case '&':						/* &用来分开多个key及value,需要转义*/
				strcpy( sTmp, "%26" );
				break;
			default:
				sTmp[0] = *p;
				sTmp[1] = 0;
				break;
		}
		Assert( iMaxSize >= strlen(pStr) + strlen(sTmp) + 1 );
		strcat( pStr, sTmp );
	}
	return;
}

int DelKey( char *sCommArea, const char *sKey )
{
	char 	sTmpKey[MAX_NAMVAL_LEN*3+1];
	char 	*p1;
	char	*p2;


	Assert( sCommArea != NULL );
	Assert( sKey != NULL && *sKey != 0 );

	if( *sCommArea == 0 )	/*肯定不会有数据*/
		return -1;

	memset( sTmpKey, 0, sizeof(sTmpKey) );

	sTmpKey[0] = '&';
	sTmpKey[1] = 0;

	EncodeStr( sKey, sTmpKey + 1, sizeof(sTmpKey) - 1 );	/*-2是减去第一位的&和结束符0*/
	strcat( sTmpKey, "=" );

	p1 = strstr( sCommArea, sTmpKey );
	if( p1 != NULL )	/*找到了该键值*/
	{
		p2 = strchr( p1 + 1, '&' );	/*查找是否有下一个键值*/
		if( p2 != NULL )	/*下面也有键值*/
		{
			memmove( p1, p2, strlen(p2)+1 );	/*将后面的内容前移.+1是将结束符顺便移过来*/
		}
		else
			*p1 = 0;	/*下面没有键值,直接赋上结束符*/
	}
	else
		return -1;

	return 0;
}

int SetValue( char *sCommArea, size_t iMaxSize, const char *sKey, const char *sValue )
{
	char	*p;
	char	tmpStr[MAX_NAMVAL_LEN*3+1];
	size_t	tmpSize;
	size_t	size;
	int		ret;

	Assert( sCommArea != NULL ); /*已经为缓冲区分配存储空间*/
	Assert( sKey != NULL && *sKey != 0 );

	if( sCommArea != NULL && *sCommArea != 0 )	/*如果缓冲区不为空,则试图删除由sKey指定的数据*/
		DelKey( sCommArea, sKey );

	memset( tmpStr, 0, sizeof(tmpStr) );

	strcat( tmpStr,"&" );						/*在头部加上&符号,用来区分一个值.*/
	p = tmpStr + strlen( tmpStr );				/*临时串目前的位置*/
	size = sizeof(tmpStr) - strlen( tmpStr );	/*临时串还可以容纳的最大长度(包括结束符)*/
	EncodeStr( sKey, p, size );					/*将key值转义存放*/ /*转义失败只有在空间不足的时候发生.这里手动分配的空间,不判断了*/

	strcat( tmpStr, "=" );

	p = tmpStr + strlen( tmpStr );
	size = sizeof(tmpStr) - strlen( tmpStr );
	EncodeStr( sValue, p, size );

	if( strlen(sCommArea)+strlen(tmpStr) < iMaxSize )
	{
		strcpy( sCommArea, tmpStr );
		ret = 0;
	}
	else
		ret = -2;

	return ret;
}

int GetValue( const char *sCommArea, const char *sKey, char *sValue, size_t iMaxSize )
{
	char	sTmpKey[MAX_NAMVAL_LEN*3+1];
	char	*p1;
	char	*p2;

	Assert( sValue != NULL );						/*已经为sValue分配存贮空间*/
	Assert( iMaxSize > 0 );
	Assert( sKey != NULL && *sKey != 0 );
	Assert( sCommArea != NULL );

	*sValue = 0;

	if( *sCommArea == 0 )	/*缓冲区为空*/
		return -1;

	sTmpKey[0] = '&';
	sTmpKey[1] = 0;

	EncodeStr( sKey, sTmpKey + 1,sizeof(sTmpKey) - 2 );
	strcat( sTmpKey, "=" );

	p1 = strstr( sCommArea, sTmpKey );
	if( p1 != NULL )	/*找到了键值*/
	{
		p1 = p1 + strlen( sTmpKey );	/*跳到值的部分*/
		p2 = strchr( p1, '&' );			/*查看后面是否还有别的记录*/
		if( p2 != NULL )	/*后面还有记录.解码之间部分*/
		{
			DecodeStrByLen( p1, p2 - p1, sValue, iMaxSize );
		}
		else					/*该串在缓冲区最后,所以没有&符号*/
		{
			DecodeStr( p1, sValue, iMaxSize );
		}
	}
	else
		return -1;
	return 0;
}

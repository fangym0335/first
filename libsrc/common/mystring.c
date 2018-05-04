#define __GPCOMMAREA__

#include "mystring.h"

String *NewString( size_t size )
{
	String *pstStr;

	pstStr = ( String* )Malloc( sizeof(String) );
	if( pstStr == NULL )
	{
		RptLibErr( errno );
		return NULL;
	}

	pstStr->size = size;
	pstStr->len = 0;

	if( size == 0 )
		pstStr->str = NULL;
	else
	{
		pstStr->str = ( char * )Malloc( size );
		if( pstStr->str == NULL )
		{
			Free( pstStr );
			RptLibErr( errno );
			return NULL;
		}
		memset( pstStr->str, 0, size );
	}
	return pstStr;
}

void ClearString( String *pstStr )
{
	Assert( pstStr != NULL );
	if( pstStr->size != 0 )
		memset( pstStr->str, 0, pstStr->size );
	else
		pstStr->str = NULL;

	pstStr->len = 0;
}

void DelString( String **pstStr )
{
	Assert( pstStr != NULL && *pstStr != NULL );

	if( (*pstStr)->str == NULL )
	{
		(*pstStr)->size = 0;
		(*pstStr)->len = 0;
	}
	else
	{
		memset( (*pstStr)->str, bFreeGarbage, (*pstStr)->size );
		(*pstStr)->size = 0;
		(*pstStr)->len = 0;
		Free( (*pstStr)->str );
	}
	Free( *pstStr );
}

int StringAssign( String *pstStr, const char* str, size_t len )
{
	char *tmpStr;

	Assert( pstStr != NULL );
	Assert( str != NULL );

	if( pstStr->size < len + 1 )
	{
		tmpStr = ( char * )Realloc( pstStr->str, len + 1 );
		if( tmpStr == NULL )
		{
			RptLibErr( errno );
			return -1;
		}
		pstStr->size = len + 1;
		pstStr->str = tmpStr;
	}

	memcpy( pstStr->str, str, len );
	pstStr->len = len;

	return 0;
}

int StringAdd( String *pstStr, const char *str, size_t len )
{
	char *tmpStr;
	size_t tmpSize;

	Assert( pstStr != NULL );
	Assert( str != NULL );

	if( pstStr->size < pstStr->len + len + 1 )
	{
		tmpSize = pstStr->len + len + 1;
		tmpStr = ( char * )Realloc( pstStr->str, tmpSize );
		if( tmpStr == NULL )
		{
			RptLibErr( errno );
			return -1;
		}
		pstStr->size = tmpSize;
		pstStr->str = tmpStr;
	}

	memcpy( pstStr->str + pstStr->len, str, len );
	pstStr->len += len;
	pstStr->str[pstStr->len] = 0;

	return 0;
}

int CommStringAssign( String *pstStr, const char *fmt, ... )
{
	va_list ap;
	char *pMsg;
	char *tmpStr;

	Assert( pstStr != NULL );

	GetVarMsg( pMsg, ap, fmt );

	if( pstStr->size < strlen( pMsg ) + 1 )
	{
		tmpStr = ( char * )Realloc( pstStr->str, strlen( pMsg ) + 1 );
		if( tmpStr == NULL )
		{
			RptLibErr( errno );
			return -1;
		}
		pstStr->size = strlen( pMsg ) + 1;
		pstStr->str = tmpStr;

	}

	strcpy( pstStr->str, pMsg );
	pstStr->len = strlen( pMsg );

	return 0;
}

int CommStringAdd( String *pstStr, const char *fmt, ... )
{
	va_list ap;
	char *pMsg;
	char *tmpStr;
	size_t tmpSize;

	Assert( pstStr != NULL );

	GetVarMsg( pMsg, ap, fmt );

	if( pstStr->size < pstStr->len + strlen( pMsg ) + 1 )
	{
		tmpSize = pstStr->len + strlen( pMsg ) + 1;
		tmpStr = ( char * )Realloc( pstStr->str, tmpSize );
		if( tmpStr == NULL )
		{
			RptLibErr( errno );
			return -1;
		}
		pstStr->size = tmpSize;
		pstStr->str = tmpStr;
	}

	memcpy( pstStr->str+pstStr->len, pMsg, strlen(pMsg) );
	pstStr->len += strlen(pMsg);

	return 0;
}

void ShowString( String *pString )
{
	Assert( pString != NULL );

	if( pString->str == NULL )
		DMsg( "NULL String" );
	else
		DMsg( "size = [%d], len = [%d], String = [%s]\n", pString->size, pString->len, pString->str );
}

size_t GetStringSize( String *pString )
{
	return pString->size;
}

size_t GetStringLen( String *pString )
{
	return pString->len;
}

char* GetStringValue( String *pString )
{
	return pString->str;
}

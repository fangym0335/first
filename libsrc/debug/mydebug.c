/**********************************************
	*CopyRight Cuihq
	*All Right Reserved
	*
	*文件名称:mydebug.c
	*摘要:
	*
	*作者: 崔洪清
	*日期: 2004/07/01
***********************************************/

#include "mydebug.h"

static char gsFileName[MAX_FILE_NAME_LEN + 1];	/*记录调试信息的文件名*/
static int giLine;								/*记录调试信息的行*/

void __WriteDebugMsg( const char *fmt, ... )
{
#ifdef MYDEBUG
	va_list ap;
	char *pMsg;

	GetVarMsg( pMsg, ap, fmt );

	printf( "%s\n", pMsg );

#endif /*MYDEBUG*/
	return;
}

void SetFileAndLine( const char *filename, int line )
{
	assert( strlen( filename ) + 1 <= sizeof( gsFileName) );
	strcpy( gsFileName, filename );
	giLine = line;
}

void GetFileAndLine( char *fileName, int *line )
{
	strcpy( fileName, gsFileName );
	*line = giLine;
}

void __MyDebug( const char *msg )
{
	__WriteDebugMsg("File = [%s], Line = [%u], Msg = [%s]", gsFileName, giLine, msg );
	return;
}

void __Assert( const char *exp, const char *filename, int line )
{
	__WriteDebugMsg( "Assertion Failed :[%s], file = [%s], line = [%u]", exp, filename, line );
	exit( -1 );
}

char * __GetVarMsg( const char *fmt, va_list ap )
{
	static char sMessage[MAX_VARIABLE_PARA_LEN + 1];

	vsprintf( sMessage, fmt, ap );

	return sMessage;
}

void PMsg( const char *fmt, ... )
{
	va_list ap;
	char *pMsg;

	GetVarMsg( pMsg, ap, fmt );

	printf( "%s\n", pMsg );

	return;
}

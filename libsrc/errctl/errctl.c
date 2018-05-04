/*******************************
	*CopyRight Cuihq
	*All Right Reserved
	*
	*文件名称: liberrctrl.c
	*摘要:
	*
	*作者: 崔洪清
	*日期: 2004/06/01
*******************************/
#include "errctl.h"
#include "errmsg.h"
#include "emsgdef.h"

#define __GERRDEF_SRC__
ERRDEF	gErrDef;

static void GetErrMsg( int liberrno, char *errMsg );


static void GetErrMsg( int liberrno, char *errMsg )
{
	size_t	i;

	for( i = 0; i < sizeof( gErrMsg ) / sizeof( ERRMSG ); i++ ) {
		if ( gErrMsg[i].libErrorNO == liberrno ) {
			strcpy( errMsg,gErrMsg[i].errMsg );
			return;
		}
	}
	strcpy( errMsg, "未定义的错误类型" );
	return;
}

void RptError( int liberrno, const char * file, int line )
{
	gErrDef.libErrorNO = liberrno;
	strcpy( gErrDef.fileName, file );
	gErrDef.line = line;
}



void ShowLibError( )
{
	GetErrMsg( gErrDef.libErrorNO, gErrDef.errMsg );

	DMsg( "ERRNO = [%d], FILE = [%s], LINE = [%d], ERRMSG = [%s]", gErrDef.libErrorNO, gErrDef.fileName, gErrDef.line, gErrDef.errMsg );

	return;
}

int GetLibErrno()
{
	return gErrDef.libErrorNO;
}

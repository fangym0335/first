/*******************************
	*CopyRight Cuihq
	*All Right Reserved
	*
	*文件名称: liberrctrl.h
	*摘要:文件定义了一个错误控制的结构及一个报告错误及显示错误的方法
	*
	*作者: 崔洪清
	*日期: 2004/06/01
*******************************/

#ifndef __LIBERRCTRL_H__
#define __LIBERRCTRL_H__


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "commdef.h"
#include "errdef.h"
#include "mydebug.h"

typedef struct {
	int		libErrorNO;
	char	fileName[ MAX_FILE_NAME_LEN + 1 ];
	int		line;
	char	errMsg[64];
}ERRDEF;

#define IS_TIMEOUT	( GetLibErrno() == ETIME )

/*报告错误.*/
void RptError( int liberrno, const char* file, int line );
/*显示错误*/
void ShowLibError();

#define RptLibErr( i ) RptError( i, __FILE__, __LINE__ );

int GetLibErrno();

#ifndef __GERRDEF_SRC__
extern ERRDEF	gErrDef;
#endif

#endif

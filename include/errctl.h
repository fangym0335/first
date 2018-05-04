/*******************************
	*CopyRight Cuihq
	*All Right Reserved
	*
	*�ļ�����: liberrctrl.h
	*ժҪ:�ļ�������һ��������ƵĽṹ��һ�����������ʾ����ķ���
	*
	*����: �޺���
	*����: 2004/06/01
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

/*�������.*/
void RptError( int liberrno, const char* file, int line );
/*��ʾ����*/
void ShowLibError();

#define RptLibErr( i ) RptError( i, __FILE__, __LINE__ );

int GetLibErrno();

#ifndef __GERRDEF_SRC__
extern ERRDEF	gErrDef;
#endif

#endif

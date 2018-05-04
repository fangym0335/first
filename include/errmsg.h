/*******************************
	*CopyRight Cuihq
	*All Right Reserved
	*
	*文件名称: liberrmsg.h
	*摘要:
	*
	*作者: 崔洪清
	*日期: 2004/06/01
*******************************/
#ifndef __LIBERRMSG_H__
#define __LIBERRMSG_H__

#define MAX_ERR_MSG 	64

typedef struct {
	int		libErrorNO;
	char	errMsg[MAX_ERR_MSG+1];
}ERRMSG;


extern ERRMSG gErrMsg[];

#endif

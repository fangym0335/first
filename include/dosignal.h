/*******************************
	*CopyRight Cuihq
	*All Right Reserved
	*
	*文件名称: dosignal.h
	*摘要:
	*
	*作者: 崔洪清
	*日期: 2004/06/01
*******************************/
#ifndef __DoSignal_H__
#define __DoSignal_H__

#include "mycomm.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#ifndef __DOSIANAL_STATIC__

extern int CatchSigAlrm;
extern int SetSigAlrm;

#endif

/*自定义一个signal函数.摘自<unix环境高级编程>( stevens )*/
Sigfunc *Signal( int signo, Sigfunc *func );

/*定义对SIGCHLD信号的处理*/
int WaitChild();

/*设置定时器.sec为定时的秒数.pfWhenAlarm为处理函数.如果pfWhenAlarm为空,则采用缺省的处理*/
Sigfunc *SetAlrm( uint sec, Sigfunc *pfWhenAlrm );

/*取消设置的定时器*/
void UnSetAlrm( );


#endif

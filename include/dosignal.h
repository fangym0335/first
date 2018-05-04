/*******************************
	*CopyRight Cuihq
	*All Right Reserved
	*
	*�ļ�����: dosignal.h
	*ժҪ:
	*
	*����: �޺���
	*����: 2004/06/01
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

/*�Զ���һ��signal����.ժ��<unix�����߼����>( stevens )*/
Sigfunc *Signal( int signo, Sigfunc *func );

/*�����SIGCHLD�źŵĴ���*/
int WaitChild();

/*���ö�ʱ��.secΪ��ʱ������.pfWhenAlarmΪ������.���pfWhenAlarmΪ��,�����ȱʡ�Ĵ���*/
Sigfunc *SetAlrm( uint sec, Sigfunc *pfWhenAlrm );

/*ȡ�����õĶ�ʱ��*/
void UnSetAlrm( );


#endif

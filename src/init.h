#ifndef __INIT_H__
#define __INIT_H__


//#ifdef IFSF
#include "ifsf_pub.h"
#include "ifsf_main.h"
#include "tank_pub.h"
#include "jansson.h"
//#include "initpcd.h"
//#endif

///*�������˳�,�����̸���ipc����IPC*/
void WhenExit();
int inline GetCfgMode();

///*ϵͳ��ʼ��*/
int InitAll( int n );



#endif

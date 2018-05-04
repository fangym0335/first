#ifndef __INIT_H__
#define __INIT_H__


//#ifdef IFSF
#include "ifsf_pub.h"
#include "ifsf_main.h"
#include "tank_pub.h"
#include "jansson.h"
//#include "initpcd.h"
//#endif

///*父进程退出,父进程负责ipc清理IPC*/
void WhenExit();
int inline GetCfgMode();

///*系统初始化*/
int InitAll( int n );



#endif

/*******************************************************************
-------------------------------------------------------------------
在本设备的设计中,每个待协议转换的加油机的每个上连线
都转换为一个相对独立的IFSF加油机,
在本设备的设计中,IFSF单独定义了一个共享内存IFSF_SHM
-------------------------------------------------------------------
2007-06-26 by chenfm
*******************************************************************/
#ifndef __IFSF_MAIN_H__
#define __IFSF_MAIN_H__

#include <stdio.h>
#include "ifsf_def.h"
#include "ifsf_pub.h"

#include "lrc.h"
#include "bcd.h"
#include "lischn.h"
#include "pidctrl.h"
#include "log.h"
#include "setcfg.h"
#include "tty.h"

// Release Version


#define PCD_VER       "v1.06.5540 - 2017.05.24"
#define PCD_VERSION   "v1.06.5540 - 2017.05.24"



/*全局变量区*/

GunShm	* gpGunShm; //oil_main中的最重要的数据结构.删除了GunMsg.

//#ifdef IFSF
IFSF_SHM * gpIfsf_shm;
//#endif
//unsigned char	cnt =0;		/*进程最大数*/
//Pid*	gPidList;
//char	isChild = 0;
/*------------------------------------------------------------------*/
//private函数,下面为本.c文件的main函数要用到的一些函数:
/*初始化共享内存gpIfsf_shm, 创建所有index关键字索引值并:
*/
int InitIfsfShm();
/*-------------------------------------------------------------------*/
#endif  




/*******************************
	*CopyRight Cuihq
	*All Right Reserved
	*
	*文件名称: dosem.h
	*摘要:
	*
	*作者: 崔洪清
	*日期: 2004/06/01
*******************************/
#ifndef __DOSEM_H__
#define __DOSEM_H__

#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <signal.h>
#include <errno.h>
#include "mycomm.h"
#include "commipc.h"

#include <string.h>
#include <stdlib.h>

#define SEMINITVAL		1

/*判断一个信号量存不存在*/
mybool IsSemExist( int index );

/*建一个信号量*/
int CreateSem(int index);

/*删除信号量*/
int RemoveSem(int index);

/*取得信号量的当前值*/
int GetSemNum(int index);

/*依赖信号量的p操作*/
int Act_P(int index);

/*依赖信号量的v操作*/
int Act_V(int index);

#endif

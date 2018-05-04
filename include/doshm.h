/*******************************
	*CopyRight Cuihq
	*All Right Reserved
	*
	*文件名称: doshm.h
	*摘要:
	*
	*作者: 崔洪清
	*日期: 2004/06/01
*******************************/
#ifndef __DOSHM_H__
#define __DOSHM_H__

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <signal.h>
#include "mycomm.h"
#include "commipc.h"

/*判断指定的共享内存是否存在*/
mybool IsShmExist( int index );

/*取得共享内存地址*/
char *AttachShm( int index );

/*创建一个指定大小的共享内存,如果指定的键值已经存在,则先删除*/
int CreateShm( int index,int size);

/*创建一个指定大小的共享内存,如果指定的键值已经存在,则返回错误*/
int CreateNewShm( int index, int size );

/*删除一块共享内存*/
int RemoveShm( int index );

/*脱离共享内存*/
int DetachShm( char **p );


#endif

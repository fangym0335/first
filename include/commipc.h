/*******************************
	*CopyRight Cuihq
	*All Right Reserved
	*
	*文件名称: commipc.h
	*摘要:
	*
	*作者: 崔洪清
	*日期: 2004/06/01
*******************************/
#ifndef __COMMIPC_H__
#define __COMMIPC_H__

#include <sys/types.h>
#include <sys/ipc.h>
#include <fcntl.h>
#include "mycomm.h"

#ifdef __LINUX__
#define PERMS 0666
#else
#define PERMS (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)
#endif

/*根据索引取得键值*/
key_t GetKey(int index);
/*根据名称取得键值*/
key_t GetKeyByName( const char *name );
/*在控制结构中删除指定名称的键值*/
int RemoveNamedKey( const char* name );

#define SHM_IPCKEY_CTRL			2000	/*该key值建立一块共享内存,用来管理,分配命名ipc的key值*/
#define MAX_IPCKEY_COUNT		1000	/*可以建立的命名IPC key值的数量.从2001开始*/
#define MAX_IPCNAME_LEN			20		/*临时IPC以名字标识,此为名字的最大长度*/
#define IPCCTRL_SIZE			sizeof(int) + MAX_IPCKEY_COUNT * ( MAX_IPCNAME_LEN + 1 )	/*建立的控制结构的大小.
																							一开始的一个整数记录已经分配的命名IPC的数目*/
#define NAMEDIPC_STARTED_INDEX	2001

#endif

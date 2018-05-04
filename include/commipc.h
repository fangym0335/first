/*******************************
	*CopyRight Cuihq
	*All Right Reserved
	*
	*�ļ�����: commipc.h
	*ժҪ:
	*
	*����: �޺���
	*����: 2004/06/01
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

/*��������ȡ�ü�ֵ*/
key_t GetKey(int index);
/*��������ȡ�ü�ֵ*/
key_t GetKeyByName( const char *name );
/*�ڿ��ƽṹ��ɾ��ָ�����Ƶļ�ֵ*/
int RemoveNamedKey( const char* name );

#define SHM_IPCKEY_CTRL			2000	/*��keyֵ����һ�鹲���ڴ�,��������,��������ipc��keyֵ*/
#define MAX_IPCKEY_COUNT		1000	/*���Խ���������IPC keyֵ������.��2001��ʼ*/
#define MAX_IPCNAME_LEN			20		/*��ʱIPC�����ֱ�ʶ,��Ϊ���ֵ���󳤶�*/
#define IPCCTRL_SIZE			sizeof(int) + MAX_IPCKEY_COUNT * ( MAX_IPCNAME_LEN + 1 )	/*�����Ŀ��ƽṹ�Ĵ�С.
																							һ��ʼ��һ��������¼�Ѿ����������IPC����Ŀ*/
#define NAMEDIPC_STARTED_INDEX	2001

#endif

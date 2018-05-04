/*******************************
	*CopyRight Cuihq
	*All Right Reserved
	*
	*�ļ�����: doshm.h
	*ժҪ:
	*
	*����: �޺���
	*����: 2004/06/01
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

/*�ж�ָ���Ĺ����ڴ��Ƿ����*/
mybool IsShmExist( int index );

/*ȡ�ù����ڴ��ַ*/
char *AttachShm( int index );

/*����һ��ָ����С�Ĺ����ڴ�,���ָ���ļ�ֵ�Ѿ�����,����ɾ��*/
int CreateShm( int index,int size);

/*����һ��ָ����С�Ĺ����ڴ�,���ָ���ļ�ֵ�Ѿ�����,�򷵻ش���*/
int CreateNewShm( int index, int size );

/*ɾ��һ�鹲���ڴ�*/
int RemoveShm( int index );

/*���빲���ڴ�*/
int DetachShm( char **p );


#endif

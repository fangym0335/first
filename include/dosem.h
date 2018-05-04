/*******************************
	*CopyRight Cuihq
	*All Right Reserved
	*
	*�ļ�����: dosem.h
	*ժҪ:
	*
	*����: �޺���
	*����: 2004/06/01
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

/*�ж�һ���ź����治����*/
mybool IsSemExist( int index );

/*��һ���ź���*/
int CreateSem(int index);

/*ɾ���ź���*/
int RemoveSem(int index);

/*ȡ���ź����ĵ�ǰֵ*/
int GetSemNum(int index);

/*�����ź�����p����*/
int Act_P(int index);

/*�����ź�����v����*/
int Act_V(int index);

#endif

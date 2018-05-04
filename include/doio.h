/*******************************
	*CopyRight Cuihq
	*All Right Reserved
	*
	*�ļ�����: dofile.h
	*ժҪ:
	*
	*����: �޺���
	*����: 2004/06/01
*******************************/

#ifndef __DOIO_H__
#define __DOIO_H__

#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <termios.h>
#include "mycomm.h"
#include "dosignal.h"


/*��fd��д��size���ַ�*/
/*-1ʧ��. >0 Ϊд��io���ֽ���(=size)*/
ssize_t WriteByLength( int fd, const void *buf, size_t size );

/*��fd�ж���size���ַ�*/
/*-1ʧ��.>0��ȡ���ַ�.�������ֵ������size,��ʾ�������������,��Ҫ���д���*/
ssize_t ReadByLength( int fd, void *buf, size_t size );

ssize_t ReadByLengthInTime( int fd, void *vptr, size_t n, int timeout );

ssize_t WriteByLengthInTime(int fd, const void *vptr, size_t n, int timeout );

/*Ϊ�򿪵ı�ʶ���������*/
int AddFdStat(int fd,int flag);

/*Ϊ�򿪵ı�ʶ��ɾ������*/
int DelFdStat(int fd,int flag);

/*�Բ����͵ķ�ʽ���ն�����һ�������ַ���*/
char *GetStringByMask( const char *sPrompt, char *sValue, size_t iMaxLength );


#endif

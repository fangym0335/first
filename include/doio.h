/*******************************
	*CopyRight Cuihq
	*All Right Reserved
	*
	*文件名称: dofile.h
	*摘要:
	*
	*作者: 崔洪清
	*日期: 2004/06/01
*******************************/

#ifndef __DOIO_H__
#define __DOIO_H__

#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <termios.h>
#include "mycomm.h"
#include "dosignal.h"


/*向fd中写入size个字符*/
/*-1失败. >0 为写进io的字节数(=size)*/
ssize_t WriteByLength( int fd, const void *buf, size_t size );

/*从fd中读出size个字符*/
/*-1失败.>0读取的字符.如果返回值不等于size,表示过早读到结束符,需要进行处理*/
ssize_t ReadByLength( int fd, void *buf, size_t size );

ssize_t ReadByLengthInTime( int fd, void *vptr, size_t n, int timeout );

ssize_t WriteByLengthInTime(int fd, const void *vptr, size_t n, int timeout );

/*为打开的标识符添加属性*/
int AddFdStat(int fd,int flag);

/*为打开的标识符删除属性*/
int DelFdStat(int fd,int flag);

/*以不回送的方式从终端输入一个输入字符串*/
char *GetStringByMask( const char *sPrompt, char *sValue, size_t iMaxLength );


#endif

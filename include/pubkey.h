/*******************************
	*CopyRight Cuihq
	*All Right Reserved
	*
	*文件名称: pubkey.h
	*摘要:
	*
	*作者: 崔洪清
	*日期: 2004/06/21
*******************************/

#ifndef __PUBKEY_H__
#define __PUBKEY_H__

/*信号量的key值范围为1-99*/
#define SEM_DEMO				1

/*共享内存的key值范围为101-999*/
#define SHM_DEMO				100

/*消息队列的key值范围为1001-1999*/
#define MSQ_DEMO				1000
#define MSQ_PROCESS_CTRL		1001


/*2000及以后的值为临时IPC的key值,请勿在这里分配!!切切*/

#endif


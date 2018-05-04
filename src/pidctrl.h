#ifndef __PIDCTRL_H__
#define __PIDCTRL_H__

#include "pub.h" 
/*
typedef struct{
	pid_t 	pid;
	char	flag[4];	//进程标志
}Pid;
*/	
/*Pid *pidList建议作为Pid数组*/
int AddPidCtl( Pid *pidList, pid_t pid, char *flag, unsigned char *count );
int GetPidCtl( Pid *pidList, pid_t pid, char *flag, unsigned char *count );
int ReplacePidCtl( Pid *pidList, pid_t pid, char *flag, unsigned char pos );
#endif

#include "pidctrl.h"


int AddPidCtl( Pid *pidList, pid_t pid, char *flag, unsigned char *count )
{
	Pid *pFree;

	pFree = pidList + (*count);

	pFree->pid = pid;
	strcpy( pFree->flag, flag );

	(*count)++;

	return 0;
}

int GetPidCtl( Pid *pidList, pid_t pid, char *flag, unsigned char *count )
{
	int i;
	Pid *pFree;
	unsigned char tmp;
	for( i = 0; i < *count; i++ )
	{
		pFree = pidList + i;
		if( pFree->pid == pid )
		{
			strcpy( flag, pFree->flag );
			//*count = i;
			tmp = i;
			memcpy(count, &tmp, sizeof(unsigned char));
			return 0;
		}
	}
	return -1;

}

int ReplacePidCtl( Pid *pidList, pid_t pid, char *flag, unsigned char pos )
{
	Pid *pFree;
	pFree = pidList + pos;

	pFree->pid = pid;
	strcpy( pFree->flag, flag );

	return 0;
}

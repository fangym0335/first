#include "pub.h"

#define IND_RETRY_TIME		5

int ClearInd()
{
	int	n;
	int i;
	int	j;
	int indtyp;
	int	retryTim;
	int ret;
	char cmd[MAX_ORDER_LEN+1];
	int cmdlen;

	sleep(10);
	while(1)
	{
		sleep(10);	/*每次休息10秒再检查是否有超时的消息*/

		if( gpGunShm->sysStat == STOP_PROC || gpGunShm->sysStat == RESTART_PROC )
		{
			UserLog( "程序退出" );
			return 0;
		}

		n = gpGunShm->errCnt; /*现在总超时的消息队列键值*/
		//UserLog( "n = [%d]", n );

		if( n > 0 )
		{
			n = gpGunShm->errCnt; /*现在总超时的消息队列键值*/
			Act_P( ERR_SEM_INDEX );	/*P操作*/
			for( i = 0; i < n; i++ )
			{
				//UserLog( "i = [%d]", i );
				indtyp = gpGunShm->errInd[i];
				retryTim = gpGunShm->retryTim[i];

//				UserLog( "i = [%d], 有超时消息[%d]", i,  indtyp );

				memset( cmd, 0, sizeof(cmd) );
				cmdlen = sizeof(cmd)-1;
				ret = RecvMsg( GUN_MSG_TRAN, cmd, &cmdlen, &indtyp, 0 );
				if( ret == 0 )	/*取得消息*/
				{
					UserLog( "接收到超时消息[%s]", Asc2Hex(cmd,cmdlen) );

					DelErrIndByPos( i ); /*将该键值删除.将后面的键值前移*/
					i--;	/*该位置的已被删除,故i减少1,n减少1*/
					n--;

				}
				else	/*未取得消息.次数增加*/
				{
					gpGunShm->retryTim[i] += 1;
					if( gpGunShm->retryTim[i] >= IND_RETRY_TIME )	/*达到最大的尝试次数.删掉*/
					{
						DelErrIndByPos( i );
						i--;
						n--;
					}
				}

			}
			Act_V( ERR_SEM_INDEX );	/*V操作*/
		}

	}
}

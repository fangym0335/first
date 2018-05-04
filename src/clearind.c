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
		sleep(10);	/*ÿ����Ϣ10���ټ���Ƿ��г�ʱ����Ϣ*/

		if( gpGunShm->sysStat == STOP_PROC || gpGunShm->sysStat == RESTART_PROC )
		{
			UserLog( "�����˳�" );
			return 0;
		}

		n = gpGunShm->errCnt; /*�����ܳ�ʱ����Ϣ���м�ֵ*/
		//UserLog( "n = [%d]", n );

		if( n > 0 )
		{
			n = gpGunShm->errCnt; /*�����ܳ�ʱ����Ϣ���м�ֵ*/
			Act_P( ERR_SEM_INDEX );	/*P����*/
			for( i = 0; i < n; i++ )
			{
				//UserLog( "i = [%d]", i );
				indtyp = gpGunShm->errInd[i];
				retryTim = gpGunShm->retryTim[i];

//				UserLog( "i = [%d], �г�ʱ��Ϣ[%d]", i,  indtyp );

				memset( cmd, 0, sizeof(cmd) );
				cmdlen = sizeof(cmd)-1;
				ret = RecvMsg( GUN_MSG_TRAN, cmd, &cmdlen, &indtyp, 0 );
				if( ret == 0 )	/*ȡ����Ϣ*/
				{
					UserLog( "���յ���ʱ��Ϣ[%s]", Asc2Hex(cmd,cmdlen) );

					DelErrIndByPos( i ); /*���ü�ֵɾ��.������ļ�ֵǰ��*/
					i--;	/*��λ�õ��ѱ�ɾ��,��i����1,n����1*/
					n--;

				}
				else	/*δȡ����Ϣ.��������*/
				{
					gpGunShm->retryTim[i] += 1;
					if( gpGunShm->retryTim[i] >= IND_RETRY_TIME )	/*�ﵽ���ĳ��Դ���.ɾ��*/
					{
						DelErrIndByPos( i );
						i--;
						n--;
					}
				}

			}
			Act_V( ERR_SEM_INDEX );	/*V����*/
		}

	}
}

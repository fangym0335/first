#include "pubsock.h"


char SERIAL_PORT_START[2] = {'3'};
char SERIAL_PORT_USED_COUNT[2];
char PCD_TCP_PORT[6];
char TLG_NODE[4];
char TLG_DRV[4];
char TLG_SERIAL_PORT[2];
char TLG_PROBER_NUM[4];
char TCC_ADJUST_DATA_FILE_SIZE[4];
char SHOW_FCC_SEND_DATA[2];   /*0 ����ӡFCC�������ݣ�1��ӡFCC��������*/

int err_count =0;

//change InitCfg to InitPubCfg by chenfm 2007.8.3
int InitPubCfg()
{
	char	*pFilePath;
	char	*pFileName;
	char	tccName[32];
	int	len;

	/*ȡ�����ļ�·��*/
	pFilePath = getenv( "WORK_DIR" );	/*WORK_DIR ����Ŀ¼�Ļ�������*/
	if( pFilePath == NULL )
	{
		UserLog( "δ���廷������WORK_DIR" );
		return -1;
	}

	len = strlen( pFilePath) + strlen( PUBFILE );

	pFileName = (char*)Malloc( len + 1 );
	if( pFileName == NULL ) {
		UserLog( "����ռ�ʧ��" );
		return -1;
	}

	sprintf( pFileName, "%s%s", pFilePath, PUBFILE );

	bzero(tccName, sizeof(tccName));
	sprintf( tccName, "%s%s", pFilePath, TCCFILE );

	
#if 0
	if( GetParaFromCfg( pFileName, "SERIAL_PORT_START", SERIAL_PORT_START, NULL, INVALID ) < 0 ) {
		UserLog( "ȡ�����ļ�[%s]��������Ϣ[SERIAL_PORT_START]ʧ��", pFileName );
		Free( pFileName );
		return -1;
	}
#endif
	if( GetParaFromCfg( pFileName, "SERIAL_PORT_USED_COUNT", SERIAL_PORT_USED_COUNT, NULL, INVALID ) < 0 ) {
		UserLog( "ȡ�����ļ�[%s]��������Ϣ[SERIAL_PORT_USED_COUNT]ʧ��", pFileName );
		Free( pFileName );
		return -1;
	}
	if( GetParaFromCfg( pFileName, "PCD_TCP_PORT", PCD_TCP_PORT, NULL, INVALID ) < 0 ) {
		UserLog( "ȡ�����ļ�[%s]��������Ϣ[PCD_TCP_PORT]ʧ��", pFileName );
		Free( pFileName );
		return -1;
	}
	if( GetParaFromCfg( pFileName, "TLG_NODE", TLG_NODE, NULL, INVALID ) < 0 ) {
		UserLog( "ȡ�����ļ�[%s]��������Ϣ[TLG_NODE]ʧ��", pFileName );
		Free( pFileName );
		return -1;
	}
	
	if( GetParaFromCfg( pFileName, "TLG_DRV", TLG_DRV, NULL, INVALID ) < 0 ) {
		UserLog( "ȡ�����ļ�[%s]��������Ϣ[TLG_DRV]ʧ��", pFileName );
		Free( pFileName );
		return -1;
	}
	if( GetParaFromCfg( pFileName, "TLG_SERIAL_PORT", TLG_SERIAL_PORT, NULL, INVALID ) < 0 ) {
		UserLog( "ȡ�����ļ�[%s]��������Ϣ[TLG_SERIAL_PORT]ʧ��", pFileName );
		Free( pFileName );
		return -1;
	}
	if( GetParaFromCfg( pFileName, "TLG_PROBER_NUM", TLG_PROBER_NUM, NULL, INVALID ) < 0 ) {
		UserLog( "ȡ�����ļ�[%s]��������Ϣ[TLG_PROBER_NUM]ʧ��", pFileName );
		Free( pFileName );
		return -1;
	}
	if( GetParaFromCfg( tccName, "TCC_AD_FILE_SIZE", TCC_ADJUST_DATA_FILE_SIZE, NULL, INVALID ) < 0 ) {
		system("echo TCC_AD_FILE_SIZE=200 >> /home/App/ifsf/etc/tcc.cfg");
		UserLog( "ȡ�����ļ�[%s]��������Ϣ[TCC_AD_FILE_SIZE]ʧ��", tccName );
		return -1;
	}

	if( GetParaFromCfg( tccName, "SHOW_FCC_SEND_DATA", SHOW_FCC_SEND_DATA, NULL, INVALID ) < 0 ) {
		system("echo SHOW_FCC_SEND_DATA=0 >> /home/App/ifsf/etc/tcc.cfg");
		UserLog( "ȡ�����ļ�[%s]��������Ϣ[SHOW_FCC_SEND_DATA]ʧ��", tccName );
		return -1;
	}
	
	Free( pFileName );

//	DMsg( "SESIAL_PORT_START=[%s]",SERIAL_PORT_START );
	DMsg( "SERIAL_PORT_USED_COUNT=[%s]",SERIAL_PORT_USED_COUNT );
	DMsg( "PCD_TCP_PORT=[%s]",PCD_TCP_PORT );
	DMsg( "TLG_NODE=[%s]",TLG_NODE );
	DMsg( "TLG_DRV=[%s]",TLG_DRV );
	DMsg( "TLG_SERIAL_PORT=[%s]",TLG_SERIAL_PORT );
	DMsg( "TLG_PROBER_NUM=[%s]",TLG_PROBER_NUM);
	DMsg( "TCC_AD_FILE_SIZE=[%s]",TCC_ADJUST_DATA_FILE_SIZE);
	DMsg( "SHOW_FCC_SEND_DATA=[%s]",SHOW_FCC_SEND_DATA);

	
	return 0;

}
void Ssleep( long usec )
{
    struct timeval timeout;
    ldiv_t d;

    d = ldiv( usec, 1000000L );
    timeout.tv_sec = d.quot;
    timeout.tv_usec = d.rem;
    select( 0, NULL, NULL, NULL, &timeout );
}

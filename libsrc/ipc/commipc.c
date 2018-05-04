/*******************************
	*CopyRight Cuihq
	*All Right Reserved
	*
	*文件名称: commipc.c
	*摘要:
	*
	*作者: 崔洪清
	*日期: 2004/06/01
*******************************/
#include "../../include/commipc.h"

char gKeyFile[MAX_PATH_NAME_LEN]="";


int InitKeyFile()
{
	char *p;

	p = getenv( "WORK_DIR" );
	if( p == NULL )
		return -1;

	sprintf( gKeyFile, "%s/etc/keyfile.cfg", p );

	if( IsFileExist( gKeyFile ) == 1 )
		return 0;

	return -1;
}

key_t GetKey( int index )
{
	key_t key;
	Assert( gKeyFile[0] != 0 );
	system("touch /home/App/ifsf/log/getkey0");
	key = ftok( gKeyFile, index );
	system("touch /home/App/ifsf/log/getkey1");
	Assert( key != (key_t)(-1) );
	return key;
}

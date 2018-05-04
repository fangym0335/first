/*******************************
	*CopyRight Cuihq
	*All Right Reserved
	*
	*文件名称: namedipc.c
	*摘要:
	*
	*作者: 崔洪清
	*日期: 2004/06/25
*******************************/
#include "mycomm.h"
#include "commipc.h"
#include "domsgq.h"
#include "dosem.h"
#include "doshm.h"

static char* CreateCtrlShm( int index, int size )
{
	int ret;

	ret = CreateNewShm( index, size );
	if( ret < 0 )
		return NULL;

	return AttachShm( index );

}

key_t GetKeyByName( const char *name )
{
	static char* pShm = NULL;
	static char preName[MAX_IPCNAME_LEN + 1] = "";
	static key_t key = 0;

	int	index;
	char *p;
	char tmpName[MAX_IPCNAME_LEN + 1];
	int pos;
	int firstFreePos;		/*第一个未被分配的键值的位置*/

	if( name == (char*)(-1) )	/*-1表示初始化各个静态变量*/
	{
		preName[0] = 0;
		key = 0;
		DetachShm( &pShm );
		return -1;
	}

	if( name == NULL || name == 0 )	/*NULL和空字符串为非法名称*/
	{
		return -1;
	}

	if( strcmp( preName, name ) == 0 )/*如果本次名字和上次名字相同,则返回上次查找到的key值*/
	{
		return key;
	}

	if( strlen(name) > MAX_IPCNAME_LEN )
	{
		return -1;
	}

	if( pShm == NULL )
	{
		pShm = AttachShm( SHM_IPCKEY_CTRL );
		if( pShm == NULL )	/*控制的信息不存在*/
		{
			pShm = CreateCtrlShm( SHM_IPCKEY_CTRL, IPCCTRL_SIZE );	/*建立控制共享内存*/
			if( pShm == NULL )
				return -1;
			/*因为以前没有控制信息,所以将第一个值代表的index赋给该命名ipc*/
			key = GetKey( NAMEDIPC_STARTED_INDEX );
			if( key == (key_t)-1 )
			{
				return -1;
			}
			p = pShm + sizeof(int);
			strcpy( p, name );	/*将name存入内存*/
			*(int*)pShm++;	/*分配的IPC数目+1*/
			strcpy( preName, name );	/*保存名字*/

			p = pShm;
			DetachShm( &p );
			return key;
		}
	}

	if( *(int*)pShm >= MAX_IPCKEY_COUNT )
	{
		return -1;
	}
	p = pShm + sizeof(int);

	for( pos = 0; pos < MAX_IPCKEY_COUNT; pos++, p = p + MAX_IPCNAME_LEN + 1 )
	{
		strcpy( tmpName, p );
		if( tmpName[0] == 0 )	/*该位置代表的Key值未分配*/
		{
			firstFreePos = pos;
		}
		else if( strcmp( tmpName, name ) == 0 )
		{
			key = GetKey( NAMEDIPC_STARTED_INDEX + pos );
			if( key == (key_t)-1 )
			{
				p = pShm;
				DetachShm( &p );
				return -1;
			}
			strcpy( preName, name );	/*保存名字*/

			p = pShm;
			DetachShm( &p );
			return key;
		}
	}

	/*无符合的名字,返回一个新的key值*/
	pos = firstFreePos;
	key = GetKey( NAMEDIPC_STARTED_INDEX + pos );
	if( key == (key_t)-1 )
	{
		p = pShm;
		DetachShm( &p );
		return -1;
	}
	strcpy( p, name );	/*将name存入内存*/
	*(int*)pShm++;	/*分配的IPC数目+1*/
	strcpy( preName, name );	/*保存名字*/

	p = pShm;
	DetachShm( &p );
	return key;
}

int RemoveNamedKey( const char* name )
{
	char *pShm;
	char *p;
	int pos;
	char tmpName[MAX_IPCNAME_LEN + 1];

	if( name == NULL || name == 0 )	/*NULL和空字符串为非法名称*/
	{
		return -1;
	}

	pShm = AttachShm( SHM_IPCKEY_CTRL );
	if( pShm == NULL )
	{
		return -1;
	}

	p = pShm + sizeof(int);

	for( pos = 0; pos < MAX_IPCKEY_COUNT; pos++, p = p + MAX_IPCNAME_LEN + 1 )
	{
		strcpy( tmpName, p );

		if( tmpName[0] == 0 || strcmp( tmpName, name ) != 0)
		{
			continue;
		}
		else
		{
			*p = 0;	/*将p指向的字符串赋空值*/
			*(int*)pShm--;/*分配的键值的数量减1*/

			GetKeyByName( (char*)-1 );		/*初始化GetKeyByName中的静态变量*/
			if( *(int*)pShm == 0 )	/*如果已分配的键值数量为0,则消除共享内存*/
			{
				p = pShm;
				DetachShm( &p );
				RemoveShm( SHM_IPCKEY_CTRL );
			}
			else
			{
				p = pShm;
				DetachShm( &p );
			}
			return 0;
		}
	}

	return -1;
}

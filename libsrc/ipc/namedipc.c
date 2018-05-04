/*******************************
	*CopyRight Cuihq
	*All Right Reserved
	*
	*�ļ�����: namedipc.c
	*ժҪ:
	*
	*����: �޺���
	*����: 2004/06/25
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
	int firstFreePos;		/*��һ��δ������ļ�ֵ��λ��*/

	if( name == (char*)(-1) )	/*-1��ʾ��ʼ��������̬����*/
	{
		preName[0] = 0;
		key = 0;
		DetachShm( &pShm );
		return -1;
	}

	if( name == NULL || name == 0 )	/*NULL�Ϳ��ַ���Ϊ�Ƿ�����*/
	{
		return -1;
	}

	if( strcmp( preName, name ) == 0 )/*����������ֺ��ϴ�������ͬ,�򷵻��ϴβ��ҵ���keyֵ*/
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
		if( pShm == NULL )	/*���Ƶ���Ϣ������*/
		{
			pShm = CreateCtrlShm( SHM_IPCKEY_CTRL, IPCCTRL_SIZE );	/*�������ƹ����ڴ�*/
			if( pShm == NULL )
				return -1;
			/*��Ϊ��ǰû�п�����Ϣ,���Խ���һ��ֵ�����index����������ipc*/
			key = GetKey( NAMEDIPC_STARTED_INDEX );
			if( key == (key_t)-1 )
			{
				return -1;
			}
			p = pShm + sizeof(int);
			strcpy( p, name );	/*��name�����ڴ�*/
			*(int*)pShm++;	/*�����IPC��Ŀ+1*/
			strcpy( preName, name );	/*��������*/

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
		if( tmpName[0] == 0 )	/*��λ�ô����Keyֵδ����*/
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
			strcpy( preName, name );	/*��������*/

			p = pShm;
			DetachShm( &p );
			return key;
		}
	}

	/*�޷��ϵ�����,����һ���µ�keyֵ*/
	pos = firstFreePos;
	key = GetKey( NAMEDIPC_STARTED_INDEX + pos );
	if( key == (key_t)-1 )
	{
		p = pShm;
		DetachShm( &p );
		return -1;
	}
	strcpy( p, name );	/*��name�����ڴ�*/
	*(int*)pShm++;	/*�����IPC��Ŀ+1*/
	strcpy( preName, name );	/*��������*/

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

	if( name == NULL || name == 0 )	/*NULL�Ϳ��ַ���Ϊ�Ƿ�����*/
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
			*p = 0;	/*��pָ����ַ�������ֵ*/
			*(int*)pShm--;/*����ļ�ֵ��������1*/

			GetKeyByName( (char*)-1 );		/*��ʼ��GetKeyByName�еľ�̬����*/
			if( *(int*)pShm == 0 )	/*����ѷ���ļ�ֵ����Ϊ0,�����������ڴ�*/
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

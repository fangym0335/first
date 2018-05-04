/******************************************************************

���ڲ���name=value����
�����ڴ��еĴ�Ÿ�ʽʱ
&name1=value1&name2=value2&name3=value3
���name��value�д���&����=,��&����=�ű�ת����.&ת��Ϊ%26,=ת��Ϊ%3D.
	��Ȼ,����ת���%Ҳ��Ҫ��ת��.����תΪ%25


*******************************************************************/

#include "nameval.h"
static void DecodeStr( const char *sSource, char *pStr, size_t iMaxSize );
static void DecodeStrByLen( const char *sSource, size_t iSourceLen, char *pStr, size_t iMaxSize );
static void EncodeStr( const char *sSource, char *pStr, size_t iMaxSize );
static void EncodeStrByLen( const char *sSource, size_t iSourceLen, char *pStr, size_t iMaxSize );


/*���ַ���sSource�е����ݽ���������ַ���pStr��.iMaxLenΪ�ַ���������ɵĵ����ݳ���*/
static void DecodeStr( const char *sSource, char *pStr, size_t iMaxSize )
{
	Assert( sSource != NULL && *sSource != 0 );
	Assert( pStr != 0 );
	Assert( iMaxSize > 0 );

	DecodeStrByLen( sSource, strlen( sSource ), pStr, iMaxSize );
	return;
}
/*����sSource��ʼ,iSourceLen���ȵ��ִ����ݽ��������ִ�pStr��.iMaxLenΪ�ַ���������ɵĵ����ݳ���*/
static void DecodeStrByLen( const char *sSource, size_t iSourceLen, char *pStr, size_t iMaxSize )
{
	const char	*p;
	char 		sTmp[4];

	Assert( sSource != NULL && *sSource != 0 );
	Assert( pStr != NULL );
	Assert( iSourceLen > 0 && iMaxSize > 0 );

	*pStr = 0;
	sTmp[1] = 0;

	for( p = sSource; p < sSource + iSourceLen; )
	{
		if( '%' == *p )
		{
			if( strncmp( p, "%25", 3 ) == 0 )
				sTmp[0] = '%';
			else if( strncmp( p, "%3D", 3 ) == 0 )
				sTmp[0] = '=';
			else if( strncmp( p, "%26", 3 ) == 0 )
				sTmp[0] = '&';

			p = p + 3;
		}
		else
		{
			sTmp[0] = *p;
			p = p + 1;
		}

		Assert( iMaxSize >= strlen(pStr) + strlen(sTmp) + 1 );

		strcat( pStr, sTmp );
	}
	return;
}

static void EncodeStr( const char *sSource, char *pStr, size_t iMaxSize )
{
	Assert( sSource != NULL && *sSource != 0 );
	Assert( pStr != NULL );
	Assert( iMaxSize > 0 );

	EncodeStrByLen( sSource, strlen( sSource ), pStr, iMaxSize );
	return;
}

/*��sSource��ֵ�е������ַ�ת��,�����������pStr��*/
static void EncodeStrByLen( const char *sSource, size_t iSourceLen, char *pStr, size_t iMaxSize )
{
	const char	*p;
	char		sTmp[4];

	Assert( sSource != NULL );
	Assert( pStr != NULL );
	Assert( iSourceLen > 0 && iMaxSize > 0 );

	*pStr = 0;
	for( p = sSource; p < sSource + iSourceLen; p++ )
	{
		switch( *p )
		{
			case '%':						/* %����ת��=��&,���Ա�����Ҫת�� */
				strcpy( sTmp, "%25" );
				break;
			case '=':						/* =��������key��value,��Ҫת��*/
				strcpy( sTmp, "%3D" );
				break;
			case '&':						/* &�����ֿ����key��value,��Ҫת��*/
				strcpy( sTmp, "%26" );
				break;
			default:
				sTmp[0] = *p;
				sTmp[1] = 0;
				break;
		}
		Assert( iMaxSize >= strlen(pStr) + strlen(sTmp) + 1 );
		strcat( pStr, sTmp );
	}
	return;
}

int DelKey( char *sCommArea, const char *sKey )
{
	char 	sTmpKey[MAX_NAMVAL_LEN*3+1];
	char 	*p1;
	char	*p2;


	Assert( sCommArea != NULL );
	Assert( sKey != NULL && *sKey != 0 );

	if( *sCommArea == 0 )	/*�϶�����������*/
		return -1;

	memset( sTmpKey, 0, sizeof(sTmpKey) );

	sTmpKey[0] = '&';
	sTmpKey[1] = 0;

	EncodeStr( sKey, sTmpKey + 1, sizeof(sTmpKey) - 1 );	/*-2�Ǽ�ȥ��һλ��&�ͽ�����0*/
	strcat( sTmpKey, "=" );

	p1 = strstr( sCommArea, sTmpKey );
	if( p1 != NULL )	/*�ҵ��˸ü�ֵ*/
	{
		p2 = strchr( p1 + 1, '&' );	/*�����Ƿ�����һ����ֵ*/
		if( p2 != NULL )	/*����Ҳ�м�ֵ*/
		{
			memmove( p1, p2, strlen(p2)+1 );	/*�����������ǰ��.+1�ǽ�������˳���ƹ���*/
		}
		else
			*p1 = 0;	/*����û�м�ֵ,ֱ�Ӹ��Ͻ�����*/
	}
	else
		return -1;

	return 0;
}

int SetValue( char *sCommArea, size_t iMaxSize, const char *sKey, const char *sValue )
{
	char	*p;
	char	tmpStr[MAX_NAMVAL_LEN*3+1];
	size_t	tmpSize;
	size_t	size;
	int		ret;

	Assert( sCommArea != NULL ); /*�Ѿ�Ϊ����������洢�ռ�*/
	Assert( sKey != NULL && *sKey != 0 );

	if( sCommArea != NULL && *sCommArea != 0 )	/*�����������Ϊ��,����ͼɾ����sKeyָ��������*/
		DelKey( sCommArea, sKey );

	memset( tmpStr, 0, sizeof(tmpStr) );

	strcat( tmpStr,"&" );						/*��ͷ������&����,��������һ��ֵ.*/
	p = tmpStr + strlen( tmpStr );				/*��ʱ��Ŀǰ��λ��*/
	size = sizeof(tmpStr) - strlen( tmpStr );	/*��ʱ�����������ɵ���󳤶�(����������)*/
	EncodeStr( sKey, p, size );					/*��keyֵת����*/ /*ת��ʧ��ֻ���ڿռ䲻���ʱ����.�����ֶ�����Ŀռ�,���ж���*/

	strcat( tmpStr, "=" );

	p = tmpStr + strlen( tmpStr );
	size = sizeof(tmpStr) - strlen( tmpStr );
	EncodeStr( sValue, p, size );

	if( strlen(sCommArea)+strlen(tmpStr) < iMaxSize )
	{
		strcpy( sCommArea, tmpStr );
		ret = 0;
	}
	else
		ret = -2;

	return ret;
}

int GetValue( const char *sCommArea, const char *sKey, char *sValue, size_t iMaxSize )
{
	char	sTmpKey[MAX_NAMVAL_LEN*3+1];
	char	*p1;
	char	*p2;

	Assert( sValue != NULL );						/*�Ѿ�ΪsValue��������ռ�*/
	Assert( iMaxSize > 0 );
	Assert( sKey != NULL && *sKey != 0 );
	Assert( sCommArea != NULL );

	*sValue = 0;

	if( *sCommArea == 0 )	/*������Ϊ��*/
		return -1;

	sTmpKey[0] = '&';
	sTmpKey[1] = 0;

	EncodeStr( sKey, sTmpKey + 1,sizeof(sTmpKey) - 2 );
	strcat( sTmpKey, "=" );

	p1 = strstr( sCommArea, sTmpKey );
	if( p1 != NULL )	/*�ҵ��˼�ֵ*/
	{
		p1 = p1 + strlen( sTmpKey );	/*����ֵ�Ĳ���*/
		p2 = strchr( p1, '&' );			/*�鿴�����Ƿ��б�ļ�¼*/
		if( p2 != NULL )	/*���滹�м�¼.����֮�䲿��*/
		{
			DecodeStrByLen( p1, p2 - p1, sValue, iMaxSize );
		}
		else					/*�ô��ڻ��������,����û��&����*/
		{
			DecodeStr( p1, sValue, iMaxSize );
		}
	}
	else
		return -1;
	return 0;
}

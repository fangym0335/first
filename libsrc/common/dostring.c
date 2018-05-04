/*******************************
	*CopyRight Cuihq
	*All Right Reserved
	*
	*�ļ�����: dostring.c
	*ժҪ:
	*
	*����: �޺���
	*����: 2004/06/01
*******************************/
#include "dostring.h"

/*�ж��ַ����Ƿ�Ϊ��*/
int IsEmptyString( const char *s )
{
	if( s == NULL || *s == 0 )
		return  TRUE;
	else
		return FALSE;
}

/*ȥ���ַ����Ҳ�ո�.��Ҫ�ַ�����������*/
char *RightTrim( char *buf )
{
	int		i;

	for( i = strlen(buf) - 1; i >= 0 && ( buf[i] == ' ' || buf[i] == '\t'); i-- )
		buf[i] = 0;
	return buf;
}

/*ȥ���ַ������ո�.��Ҫ�ַ�����������*/
char *LeftTrim( char * buf )
{
	int i, firstpos;

	for( firstpos = 0; firstpos < strlen( buf ); firstpos++ )
		if( buf[firstpos] != ' ' && buf[firstpos] != '\t' )
			break;
	i = firstpos;

	for( ; i < strlen( buf ); i++ )
		buf[ i - firstpos ] = buf[ i ];
	buf[ i - firstpos ] = 0;

	return buf;
}

/*ȥ���ַ�������ո�,��Ҫ�ַ�����������*/
char *AllTrim( char *buf )
{
	int i,firstchar,endpos,firstpos;

	Assert( buf != NULL );

	endpos = firstchar = firstpos = 0;
	for( i = 0; buf[ i ] != '\0'; i++ )
	{
		if( buf[i] ==' ' || buf[i] == '\t')
		{
			if( firstchar == 0 )
				firstpos++;
		}
		else
		{
			endpos = i;
			firstchar = 1;
		}
	}

	for( i = firstpos; i <= endpos; i++ )
		buf[i - firstpos] = buf[i];
	buf[i - firstpos] = 0;
	return buf;
}

/*���ַ�����ΪСд,��Ҫ�ַ�����������*/
char *StrToLower( char *s )
{
	int		i;

	for(i = 0; i < strlen(s); i++)
		if( s[i] >= 'A' && s[i] <= 'Z' )
			s[i] += 'a' - 'A';
	return s;
}

/*���ַ�����Ϊ��д,��Ҫ�ַ�����������*/
char *StrToUpper( char *s )
{
	int		i;

	for(i = 0; i < strlen(s); i++)
		if( s[i] >= 'a' && s[i] <= 'z' )
			s[i] -= 'a' - 'A';
	return s;
}

/*���ַ�����ΪСд,��Ҫ�ַ�����������*/
char *ToLower( const char *src, char *obj )
{
	int		i;

	for(i = 0; i < strlen(src); i++)
		if( src[i] >= 'A' && src[i] <= 'Z' )
			obj[i] += 'a' - 'A';
	obj[i] = 0;
	return obj;
}

/*���ַ�����Ϊ��д,��Ҫ�ַ�����������*/
char *ToUpper( const char *src, char *obj )
{
	int		i;

	for(i = 0; i < strlen(src); i++)
		if( src[i] >= 'a' && src[i] <= 'z' )
			obj[i] -= 'a' - 'A';
	obj[i] = 0;
	return obj;
}

/***************************************************

	�ú�����ָ�����ַ�����ַ�����ָ���ĳ���
	buff:	�ַ���
	length:	��䵽�ĳ���
	fillChr:�����ַ�
	flag:	��ֵΪFILLBEHINDʱ,���ַ�����β�����
			��ֵΪFILLAHEADʱ,���ַ�����ͷ�����

	��ע:
	1.	��ѡ�����ַ���ǰ�����ʱ,��Ϊ������ʱ��,�����п����ڴ����.
		�ڴ����ʱ����-1
	2.	������㹻���buff,����������Ĵ�
***************************************************/

int FillBuff( char *buff, size_t length, char fillChr, myflag flag )
{
	size_t	i;
	char	*tmpBuff;

	Assert( buff != NULL );
	Assert( length != 0 );
	Assert( flag == FILLBEHIND || flag == FILLAHEAD );

	if( strlen(buff) >= length ) /*�ַ������ȱ�Ŀ�곤�ȴ�,����Ҫ���*/
		return 0;


	if ( flag == FILLBEHIND )
	{
		for ( i = strlen( buff ); i < length; i++ )
			buff[ i ] = fillChr;
		buff[ i ] = 0;
	}
	else if ( flag == FILLAHEAD )
	{
		if( ( tmpBuff = ( char * ) Malloc( length + 1 ) ) == NULL )
		{
			RptLibErr( errno );
			return -1;
		}
		memset( tmpBuff, fillChr, length );
		strcpy( tmpBuff + length - strlen( buff ), buff );
		strcpy( buff, tmpBuff );
		Free( tmpBuff );
	}

	return 0;
}

/***************************************************

	�ú����ж��ַ��������Ƿ���ĳ�����ֵ�������,
	�������,�������ַ���

	buff:	�ַ���
	mod:	ģ.������ַ���Ϊ��ģ��������.
	fillChr:�����ַ�
	flag:	��ֵΪFILLBEHINDʱ,���ַ�����β�����
			��ֵΪFILLAHEADʱ,���ַ�����ͷ�����
	��ע:
	1.	��ѡ�����ַ���ǰ�����ʱ,��Ϊ������ʱ��,�����п����ڴ����.
		�ڴ����ʱ����-1
	2.	������㹻���buff,����������Ĵ�
***************************************************/

int FillBuffWithMod( char *buff, uint mod, char fillChr, myflag flag )
{
	size_t		i;
	size_t		fillLen;
	size_t		buffLen;
	char		*tmpBuff;

	Assert( buff != NULL );
	Assert( mod != 0 );
	Assert( flag == FILLBEHIND || flag == FILLAHEAD );

	buffLen = strlen( buff );

	if ( buffLen % mod == 0 )
		return 0;
	else
		 fillLen = mod -  buffLen % mod;


	if ( flag == FILLBEHIND )
	{
		for ( i = strlen( buff ); i < buffLen + fillLen ; i++ )
			buff[ i ] = fillChr;
		buff[ i ] = 0;
	}
	else if ( flag == FILLAHEAD )
	{
		if ( ( tmpBuff = ( char * ) Malloc( buffLen + fillLen + 1 ) ) == NULL )
		{
			RptLibErr( errno );
			return -1;
		}
		memset( tmpBuff, fillChr, buffLen + fillLen );
		strcpy( tmpBuff + fillLen, buff );
		strcpy( buff, tmpBuff );
		Free( tmpBuff );
	}
	return 0;
}

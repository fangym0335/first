/*******************************
	*CopyRight Cuihq
	*All Right Reserved
	*
	*�ļ�����: dofile.c
	*ժҪ:
	*
	*����: �޺���
	*����: 2004/06/01
*******************************/

#include "dofile.h"

static int ReadLine( FILE *f, char *s, int MaxLen );
static int ReadWord( const char *s, char *w, const char *separators, int MaxLen );
static int ReadLineFromFile_1( FILE *fp, String *pstStr, int flag );

static int ReadLine( FILE *f, char *s, int MaxLen )
{
	int i, c;

	for( i = 0; i < MaxLen; i++ )
	{
		c = fgetc( f);
		if( c == '\t' )
			c = ' ';
		if( c == EOF || c == '\n' )
			break;
		s[i] = c;
	}

	s[i] = 0;

	if( i == MaxLen )
	{
		for( ; ; )
		{
			c = fgetc(f);
			if( c == EOF || c == '\n' )
				break;
		}
	}
	if( c == EOF )
		return 1;
	else
		return 0;
}

static int ReadWord( const char *s, char *w, const char *separators, int MaxLen )
{
	int i, j;

	for( i = 0; s[i] == ' '; i++ );

	j = 0;

	if( strchr( separators, s[i] ) )
		w[j++] = s[i++];
	else
		for( ; j < MaxLen && !strchr( separators, s[i] ) && s[i] != ' ' && s[i] != 0; i++, j++ )
			w[j] = s[i];
	w[j] = 0;
	return i;
}

int GetParaFromCfg( const char *filename, const char *paraname, char *para, const char *defaultvalue, int mode )
{
	FILE *f;
	char s[LINESIZE + 1], w[LINESIZE + 1];
	int r;

	Assert( filename != NULL && *filename != 0 );
	Assert( paraname != NULL && *paraname != 0 );
	Assert( para != NULL );
	Assert( mode == VALID || mode == INVALID );

	f = fopen( filename, "rt" );

	if( ( f == NULL ) && ( mode == VALID ) )
	{
		if( defaultvalue == NULL )
			strcpy(para, "");
		else
			strcpy( para, defaultvalue );
		return 0;
	}

	if( ( f == NULL ) && ( mode == INVALID ) )
	{
		*para = 0;
		return -1;
	}

	for( ; ; )
	{
		r = ReadLine( f, s, LINESIZE );
		if( r != 0 )
			break;
		r = ReadWord( s, w, "#=", LINESIZE );
		if( strcmp( w, paraname ) == 0 )
		{
			r += ReadWord( s + r, w, "#=", LINESIZE );
			if( strcmp(w, "=") == 0 )
				r += ReadWord( s + r, w, "#=", LINESIZE );
			strcpy( para, w );
			fclose( f );
			return 0;
		}
	}

	fclose( f );

	if( mode == VALID )
	{
		if( defaultvalue == NULL )
			strcpy( para,"" );
		else
			strcpy( para, defaultvalue );
		return 0;
	}
	*para = 0;
	return -2;
}

int WriteFile( const char *filename, const char *fmt, ... )
{
	int		fd;
	va_list argptr;
	char sMessage[MAX_VARIABLE_PARA_LEN + 1];

	Assert( filename != NULL && *filename != 0 );

	va_start( argptr, fmt );
	vsprintf( sMessage,fmt, argptr );
	va_end( argptr );


	fd = open( filename, O_WRONLY | O_CREAT | O_APPEND, 0755 );
	if( fd == -1 )
	{
		RptLibErr( errno );
		return -1;
	}
	write( fd, sMessage, strlen( sMessage ) );
	close( fd );

	return 0;
}

/******************************************************************************
* ����:	�����ļ�������ļ��ֽ���
* ����:
*	���:
*		sFileName : ��ʶ�ļ�����·�����Ƶ��ַ�����ָ��
*	����:
*		��
* ����ֵ:
*	������	: �ļ��ֽ���
*	-1	: ��ȡ�ļ��ֽ���ʧ��
******************************************************************************/
long GetFileSizeByName( char* sFileName )
{
	struct stat statbuf;
	int iRet;

	Assert( sFileName != NULL && *sFileName != 0 );

	iRet = stat( sFileName, &statbuf );

	if( iRet == -1 )
	{
		if( errno == ENOENT )	/*�޴��ļ�*/
		{
			RptLibErr( errno );
			return -2;
		}
		else
		{
			RptLibErr( errno );
			return -1;
		}
	}

	return statbuf.st_size;
}

/******************************************************************************
* ����:	�����ļ����������ļ��ֽ���
* ����:
*	���:
*		fpStream : ���ļ����
*	����:
*		��
*����ֵ:
*	������	: �ļ��ֽ���
*	-1	: ��ȡ�ļ��ֽ���ʧ��(����ԭ�򱣴���ȫ�ֱ���_iLibErrorNo��)
******************************************************************************/
long GetFileSizeByPoint( FILE * fpStream )
{
	struct stat statbuf;
	int iRet;

	Assert( fpStream != NULL );

	iRet = fstat( fileno( fpStream ), &statbuf );

	if( iRet == -1 )
	{
		if( errno == ENOENT )
		{
			RptLibErr( errno );
			return -1;
		}
		else
		{
			RptLibErr( errno );
			return -1;
		}
	}

	return statbuf.st_size;
}

/*�ļ��Ƿ����*/
int IsFileExist( char * sFileName )
{
	int ret;
	struct stat file_stat;

	Assert( sFileName != NULL && *sFileName != 0 );

	ret = stat( sFileName, &file_stat );
	if( ret < 0 )
	{
		if( errno == ENOENT )
		{
			return 0;
		}
		else
		{
			RptLibErr( errno );
			return -1;
		}
	}
	return 1;
}

/*�ж��ļ�������*/
int IsDir( char *sFileName )
{
	int ret;
	struct stat file_stat;

	Assert( sFileName != NULL && *sFileName != 0 );

	ret = stat( sFileName, &file_stat );
	if( ret < 0 )
	{
		RptLibErr( errno );
		return -1;
	}

	if( S_ISDIR( file_stat.st_mode ) )
		return 1;
	return 0;
}

static int ReadLineFromFile_1( FILE *fp, String *pstStr, int flag )
{
	char buf[ LINESIZE ];
	static int first = TRUE;

	Assert( fp != NULL );

	if( first )
	{
		ClearString( pstStr );
		first = FALSE;
	}

	if( fgets( buf, LINESIZE, fp ) != NULL )
	{
		if( buf[ strlen( buf ) - 1 ] == '\n' )	/*һ�ж�����*/
		{
			buf[ strlen( buf ) - 1 ] = 0;	/*ȥ�����з�*/
/*			AllTrim( buf );*/	/*ȥ���ַ������ҿո�*/

			if( buf[ strlen( buf ) - 1 ] == '\\' || flag == CONTACT) /*���з�*/
			{
				buf[ strlen( buf ) - 1 ] = 0;	/*ȥ�����з�*/
/*				AllTrim( buf );*/
				StringAdd( pstStr, buf, strlen(buf) );
				ReadLineFromFile_1( fp, pstStr, NO_CONTACT );
			}
			else
				StringAdd( pstStr, buf, strlen(buf) );

		}
		else /*���б��ض�,������*/
		{
/*			AllTrim( buf );*/
			if( buf[ strlen( buf ) - 1 ] == '\\' ) /*���з�*/
			{
				buf[ strlen( buf ) - 1 ] = 0;	/*ȥ�����з�*/
/*				AllTrim( buf );*/
				StringAdd( pstStr, buf, strlen(buf) );
				ReadLineFromFile_1( fp, pstStr, CONTACT );
			}
			else
			{
				StringAdd( pstStr, buf, strlen(buf) );
				ReadLineFromFile_1( fp, pstStr, NO_CONTACT );
			}


		}
	}
	else
	{
		first = TRUE;
		return 0;
	}
	first = TRUE;
	return 1;
}

/*���ļ���ȡһ��*/
int ReadLineFromFile( FILE *fp, String *pstStr )
{
	Assert( fp != NULL );
	Assert( pstStr != NULL );

	return ReadLineFromFile_1( fp, pstStr, NO_CONTACT );
}


/*******************************
	*CopyRight Cuihq
	*All Right Reserved
	*
	*文件名称: dotime.c
	*摘要:
	*
	*作者: 崔洪清
	*日期: 2004/06/01
*******************************/
#define __GBEGINTIME_SRC__
#define __GENDTIME_SRC__

#include "dotime.h"

char gBeginTime[MICROSIZE];
char gEndTime[MICROSIZE];

char * GetSysDate( char *datebuff )
{
	time_t clock1;	/*clock是系统函数*/
	struct tm *tm1;
	static char buff[9];

	clock1 = time( 0 );
	tm1 = localtime( &clock1 );
	strftime( buff,9,"%Y%m%d",tm1 );

	if( datebuff != NULL )
		strcpy( datebuff, buff );
	return buff;
}

char * GetSysDateTime( char *timebuff )
{
	time_t clock1;	/*clock是系统函数*/
	struct tm *tm1;
	static char buff[15];

	clock1 = time( 0 );
	tm1 = localtime( &clock1 );
	strftime( buff,15,"%Y%m%d%H%M%S",tm1 );

	if( timebuff != NULL )
		strcpy( timebuff, buff );
	return buff;
}

void GetSysDateAndTime( char *curr_date, char *curr_time )
{
	time_t clock1;
	struct tm *tm1;

	clock1 = time( 0 );
	tm1 = localtime( &clock1 );

	if( curr_date != NULL) strftime( curr_date, 9, "%Y%m%d", tm1 );
	if( curr_time != NULL) strftime( curr_time, 7, "%H%M%S", tm1 );

	return;
}


void GetMicroTime( char * timebuf )
{
	struct timeval tv;

	Assert( timebuf != NULL );

	gettimeofday( &tv, 0 );

	memcpy( timebuf, ( char* )( &tv ),sizeof( struct timeval ) );

	return;
}

ulong GetDiffTime( const char * begin, const char *end )
{
	struct timeval *tv1,*tv2;

	Assert( begin != NULL );
	Assert( end != NULL );

	tv1 = ( struct timeval* )begin;
	tv2 = ( struct timeval* )end;

	return ( tv2->tv_sec - tv1->tv_sec ) * 1000000 + tv2->tv_usec - tv1->tv_usec;
}

ulong GetPassTime( const char * begin )
{
	struct timeval *tv1,tv2;

	Assert( begin != NULL );

	tv1 = ( struct timeval* )begin;

	gettimeofday( &tv2, 0 );

	return ( tv2.tv_sec - tv1->tv_sec ) * 1000000 + tv2.tv_usec - tv1->tv_usec;
}

void GetFormatTime( char *timebuf, size_t bufSize, const char * format )
{
	size_t 	len;
	char	tmp;
	char 	*p;

	time_t clock1;
	struct tm *tm1;

	Assert( timebuf != NULL );
	Assert( format != NULL && *format != 0 );

	*timebuf = 0;

	clock1 = time( 0 );
	tm1 = localtime( &clock1 );

	len = 1;

	p = ( char * )format;

	while( ( p = strchr( p,'%' ) ) != NULL)
	{
		p = p + 1;

		tmp = p[0];

		switch( tmp )
		{
			case 'Y':
				len += 4;
				break;
			case 'm':
				len += 2;
				break;
			case 'd':
				len += 2;
				break;
			case 'H':
				len += 2;
				break;
			case 'M':
				len += 2;
				break;
			case 'S':
				len += 2;
				break;
			default:
			{
				Assert(0);
				return;
			}
		}
	}
	Assert( len <= bufSize );
	strftime( timebuf, ( size_t )len, ( const char * )format, ( const struct tm * )tm1 );

	return;
}

int GetNextDay( char *nday )
{
	struct tm *when;
	time_t clock1;

	Assert( nday != NULL );

	*nday = 0;
	clock1 = time( 0 );

	when = localtime( &clock1 );

	when->tm_mday = when->tm_mday + 1;

    if ( mktime( when ) != ( time_t ) ( -1 ) )		/*将-1强制类型转化为time_t*/
		strftime( nday, 9, "%Y%m%d", when);
	else
	{
		RptLibErr( errno );
		return -1;
	}

	return 0;
}

int GetLastDay( char *nday )
{
	struct tm *when;
	time_t clock1;

	Assert( nday != NULL );

	*nday = 0;
	clock1 = time( 0 );

	when = localtime( &clock1 );

	when->tm_mday = when->tm_mday - 1;

    if ( mktime( when ) != ( time_t ) ( -1 ) )		/*将-1强制类型转化为time_t*/
		strftime( nday, 9, "%Y%m%d", when);
	else
	{
		RptLibErr( errno );
		return -1;
	}

	return 0;
}


int GetDay( char *nday, const char *day, int after )
{
	struct tm when;

	char year[5];
	char month[3];
	char mday[3];

	Assert( nday != NULL );
	Assert( ( day != NULL ) && ( strlen(day) == DATE_LEN ) );

	if( after == 0 )
	{
		strcpy( nday, day );
		return 0;
	}

	*nday = 0;
	memset( year, 0, sizeof( year ) );
	memset( month, 0, sizeof( month ) );
	memset( mday, 0, sizeof( day ) );
	memset( &when, 0, sizeof(when) );

	strncpy( year, day, 4);
	strncpy( month, day + 4, 2 );
	strncpy( mday, day + 6, 2);

	when.tm_year = atoi( year ) - 1900;
	when.tm_mon = atoi( month ) -1;
	when.tm_mday = atoi( mday ) + after;

    if( mktime( &when ) != ( time_t ) ( -1 ) )
		strftime( nday, 9, "%Y%m%d", &when );
	else
	{
		RptLibErr( errno );
		return -1;
	}
	return 0;
}

int GetFirstAndLastOfNowMonth( char *firstDay, char *lastDay )
{
	struct tm *when;
	time_t clock1;

	Assert( firstDay != NULL );
	Assert( lastDay != NULL );

	*firstDay = 0;
	*lastDay = 0;

	clock1 = time( 0 );

	when = localtime( &clock1 );

	when->tm_mday = 1;

    if( mktime( when ) != ( time_t ) ( -1 ) )
		strftime( firstDay, 9, "%Y%m%d", when );
	else
	{
		RptLibErr( errno );
		return -1;
	}

	when->tm_mon = when->tm_mon + 1;
	when->tm_mday = 0;
    if( mktime( when ) != ( time_t ) ( -1 ) )
		strftime( lastDay, 9, "%Y%m%d", when );
	else
	{
		RptLibErr( errno );
		return -1;
	}

	return 0;
}

int GetFirstAndLastOfMonth( char *firstDay, char *lastDay, const char *day )
{
	struct tm when;

	char year[5];
	char month[3];
	char mday[3];

	Assert( ( day != NULL ) );
	Assert( firstDay != NULL );
	Assert( lastDay != NULL );

	*firstDay = 0;
	*lastDay = 0;

	memset( year, 0,sizeof( year ) );
	memset( month, 0,sizeof( month ) );
	memset( mday, 0,sizeof( day ) );
	memset( &when, 0, sizeof(when) );

	strncpy( year, day, 4 );
	strncpy( month, day + 4, 2 );

	when.tm_year = atoi( year ) - 1900;
	when.tm_mon = atoi( month ) -1;

	when.tm_mday = 1;

    if( mktime( &when ) != ( time_t ) ( -1 ) )
		strftime( firstDay, 9, "%Y%m%d", &when );
	else
	{
		RptLibErr( errno );
		return -1;
	}
	when.tm_mon = when.tm_mon + 1;
	when.tm_mday = 0;
    if( mktime( &when ) != ( time_t ) ( -1 ) )
		strftime( lastDay, 9, "%Y%m%d", &when );
	else
	{
		RptLibErr( errno );
		return -1;
	}
	return 0;
}

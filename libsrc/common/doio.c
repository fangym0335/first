/*******************************
	*CopyRight Cuihq
	*All Right Reserved
	*
	*�ļ�����: doio.c
	*ժҪ:
	*
	*����: �޺���
	*����: 2004/06/01
*******************************/

#include "doio.h"

ssize_t ReadByLength( int fd, void *vptr, size_t n )
{
	size_t	nleft;
	ssize_t	nread;
	char	*ptr;

	ptr = vptr;
	nleft = n;
	while(nleft > 0)
	{
		if( ( nread = read( fd, ptr, nleft ) ) < 0 )
		{
			if( errno == EAGAIN )	/*������*//*���ж϶Է�����IO��Ч*/
				continue;
			if( errno == EINTR )
				nread = 0;
			else
			{
				RptLibErr( errno );
				return(-1);
			}
		}
		else if( nread == 0 )
		{
			RptLibErr( COMM_EOF );
			break;				/* EOF */
		}
//printf("_____________________________%x %x____________________\n",*ptr,*(ptr+1));
		nleft -= nread;
		ptr   += nread;
	}
	return (n - nleft);
}

ssize_t WriteByLength(int fd, const void *vptr, size_t n)
{
	size_t		nleft;
	ssize_t		nwritten;
	const char	*ptr;

	ptr = vptr;
	nleft = n;
	while (nleft > 0)
	{
		if( ( nwritten = write( fd, ptr, nleft ) ) <= 0 )
		{
			if( errno == EAGAIN )	/*�Է�����IO��Ч*/
				continue;
			if( errno == EINTR )
				nwritten = 0;
			else
			{
				RptLibErr( errno );
				return -1;
			}
		}

		nleft -= nwritten;
		ptr   += nwritten;
	}
	return(n-nleft);
}

/*���������ж�����									*/
/*����ֵ:							*/
/*	-1			����ʧ��			*/
/*	=n			�ɹ�				*/
/*	>=0 && <n	�ļ����������ӹرգ����߳�ʱ		*/
/*	ע: fdӦ��������IO*/

ssize_t ReadByLengthInTime( int fd, void *vptr, size_t n, int timeout )
{
	size_t	nleft;
	ssize_t	nread;
	char	*ptr;

	SetAlrm( timeout, NULL );

	ptr = vptr;
	nleft = n;
	while( nleft > 0 ) {
		if( ( nread = read( fd, ptr, nleft ) ) < 0 ) {
			if( CatchSigAlrm )	/*��ʱ*//*���ܴ�����,ֻҪ��׽����ʱ������*/
			{
				UnSetAlrm();
				RptLibErr( ETIME );
				break;
			}
			if( errno == EAGAIN ) {	/*������*/
				continue;
			}
			if( errno == EINTR )
			{
				/*�������źŴ�ϼ�����*/
				nread = 0;
			}
			else	/*����*/
			{
				UnSetAlrm();
				RptLibErr( errno );
				return(-1);
			}
		}
		else if( nread == 0 )
		{
			RptLibErr( COMM_EOF );
			break;				/* EOF */
		}

		nleft -= nread;
		ptr   += nread;
	}

	UnSetAlrm( );

	return (n - nleft);
}

/*���������ж�����			*/
/*����ֵ:	   	 		*/
/*	-1		����ʧ��	*/
/*	=n		�ɹ�		*/
/*	>=0 && <n	��ʱ		*/

ssize_t WriteByLengthInTime(int fd, const void *vptr, size_t n, int timeout )
{
	size_t		nleft;
	ssize_t		nwritten;
	const char	*ptr;

	SetAlrm( timeout, NULL );

	ptr = vptr;
	nleft = n;
	while( nleft > 0) {
		/*����write���Ǳ��źŴ�ϻ��߳���,���򲻻�д��0���ַ�*/
		if ((nwritten = write(fd, ptr, nleft)) <= 0 ) {
			if( CatchSigAlrm )	/*��ʱ*//*���ܴ�����,ֻҪ��׽����ʱ������*/
			{
				UnSetAlrm();
				RptLibErr( ETIME );
				break;
			}

			if(errno == EINTR) {
				nwritten = 0;
			} else {
				UnSetAlrm();
				RptLibErr( errno );
				return(-1);
			}
		}

		nleft -= nwritten;
		ptr   += nwritten;
	}

	UnSetAlrm();

	return (n-nleft);
}


int AddFdStat( int fd, int flag )
{
	int oldflg;
	int ret;

	oldflg = fcntl( fd,F_GETFL,0 );
	if( oldflg < 0 )
		return -1;

	oldflg |= flag;

	ret = fcntl( fd,F_SETFL,oldflg );
	if( ret < 0 )
	{
		RptLibErr( errno );
		return -1;
	}
	return 0;
}

int DelFdStat( int fd, int flag )
{
	int oldflg;
	int ret;

	oldflg = fcntl( fd,F_GETFL,0 );
	if( oldflg < 0 )
	{
		RptLibErr( errno );
		return -1;
	}

	oldflg &= ~flag;

	ret = fcntl( fd,F_SETFL,oldflg );
	if( ret < 0 )
	{
		RptLibErr( errno );
		return -1;
	}

	return 0;
}

/*�Բ����͵ķ�ʽ�õ�һ�������ַ���*/
/*iMaxLengthΪ�������󳤶�.�������Ľ�����*/
char *GetStringByMask( const char *sPrompt, char *sValue, size_t iMaxLength )
{
	char		*ptr;
	sigset_t	sig;
	sigset_t	sigsave;
	struct termios	term;
	struct termios	termsave;
	FILE		*fp;
	int			c;

	Assert( iMaxLength > 1 );	/*�������1,����ΪҪ����'\0',��������һ���ַ�Ҳ���ղ���*/
	*sValue = 0;

	if( ( fp = fopen( ctermid( NULL ), "r+" ) ) == NULL )	/*���ն�*/
	{
		RptLibErr( errno );
		return NULL;
	}
	setbuf( fp, NULL );					/*�رջ������*/

	sigemptyset( &sig );
	sigaddset( &sig, SIGINT );	/*SIGINT 	DELETE��CTRL+C*/
	sigaddset( &sig, SIGTSTP );	/*SIGTSTP	CTRL+Z*/
	sigaddset( &sig, SIGQUIT );	/*SIGQUIT	CTRL+\*/
	sigprocmask( SIG_BLOCK, &sig, &sigsave );

	tcgetattr( fileno( fp ), &termsave );
	memcpy( &term, &termsave, sizeof( struct termios ) );;
	term.c_lflag &= ~( ECHO | ECHOE | ECHOK | ECHONL );
	tcsetattr( fileno( fp ), TCSAFLUSH, &term );

	fputs( sPrompt, fp );
	putc( '\n', fp );

	ptr = sValue;
	while( ( c = getc( fp ) ) != EOF && c != '\n' )
	{
		if( ptr < &sValue[ iMaxLength - 1 ] )
		{
			*ptr++ = c;
		}
	}
	*ptr = 0;
	putc( '\n', fp );

	tcsetattr( fileno( fp ),TCSAFLUSH, &termsave );
	sigprocmask( SIG_SETMASK, &sigsave, NULL );

	fclose( fp );

	return sValue;
}

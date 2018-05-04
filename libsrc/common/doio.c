/*******************************
	*CopyRight Cuihq
	*All Right Reserved
	*
	*文件名称: doio.c
	*摘要:
	*
	*作者: 崔洪清
	*日期: 2004/06/01
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
			if( errno == EAGAIN )	/*无数据*//*该判断对非阻塞IO有效*/
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
			if( errno == EAGAIN )	/*对非阻塞IO有效*/
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

/*从描述符中读数据									*/
/*返回值:							*/
/*	-1			出错失败			*/
/*	=n			成功				*/
/*	>=0 && <n	文件结束（连接关闭）或者超时		*/
/*	注: fd应该是阻塞IO*/

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
			if( CatchSigAlrm )	/*超时*//*不管错误码,只要捕捉到超时都返回*/
			{
				UnSetAlrm();
				RptLibErr( ETIME );
				break;
			}
			if( errno == EAGAIN ) {	/*无数据*/
				continue;
			}
			if( errno == EINTR )
			{
				/*被其他信号打断继续读*/
				nread = 0;
			}
			else	/*出错*/
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

/*从描述符中读数据			*/
/*返回值:	   	 		*/
/*	-1		出错失败	*/
/*	=n		成功		*/
/*	>=0 && <n	超时		*/

ssize_t WriteByLengthInTime(int fd, const void *vptr, size_t n, int timeout )
{
	size_t		nleft;
	ssize_t		nwritten;
	const char	*ptr;

	SetAlrm( timeout, NULL );

	ptr = vptr;
	nleft = n;
	while( nleft > 0) {
		/*阻塞write除非被信号打断或者出错,否则不会写入0个字符*/
		if ((nwritten = write(fd, ptr, nleft)) <= 0 ) {
			if( CatchSigAlrm )	/*超时*//*不管错误码,只要捕捉到超时都返回*/
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

/*以不回送的方式得到一个输入字符串*/
/*iMaxLength为密码的最大长度.包含最后的结束符*/
char *GetStringByMask( const char *sPrompt, char *sValue, size_t iMaxLength )
{
	char		*ptr;
	sigset_t	sig;
	sigset_t	sigsave;
	struct termios	term;
	struct termios	termsave;
	FILE		*fp;
	int			c;

	Assert( iMaxLength > 1 );	/*如果等于1,则因为要包含'\0',所以密码一个字符也接收不了*/
	*sValue = 0;

	if( ( fp = fopen( ctermid( NULL ), "r+" ) ) == NULL )	/*打开终端*/
	{
		RptLibErr( errno );
		return NULL;
	}
	setbuf( fp, NULL );					/*关闭缓存机制*/

	sigemptyset( &sig );
	sigaddset( &sig, SIGINT );	/*SIGINT 	DELETE或CTRL+C*/
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

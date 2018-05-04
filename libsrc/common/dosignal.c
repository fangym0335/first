#include "dosignal.h"

#define __DOSIANAL_STATIC__
int CatchSigAlrm;
int SetSigAlrm;

/*用于记录老的alarm函数的指针*/
Sigfunc 			*pfOldFunc=NULL;

/*缺省的alarm处理函数*/
static void WhenSigAlrm( int sig );

/*当收到SIGCHLD时,等待结束的子进程.防止产生僵尸进程*/
static void WhenSigChld(int sig);


/*自定义一个signal函数.摘自<unix环境高级编程>( stevens )*/
/*如果系统支持，函数可以对被除SIGALRM信号中断的系统调用自动重新启动*/
Sigfunc *Signal( int signo, Sigfunc *func )
{
	struct sigaction 	act, oact;

	act.sa_handler = func;
	sigemptyset( &act.sa_mask );
	act.sa_flags = 0;
	if( signo == SIGALRM )
	{
#ifdef SA_INTERRUPT
		act.sa_flags |= SA_INTERRUPT;
#endif
	}
	else
	{
#ifdef SA_RESTART
		act.sa_flags |= SA_RESTART;
#endif
	}
	if( sigaction( signo, &act, &oact ) < 0 )
	{
		return ( Sigfunc* )( -1 );
	}
	return oact.sa_handler;
}

/*定义SIGCHLD信号的处理函数*/
static void WhenSigChld( int sig )
{
	pid_t	pid;

	if( sig != SIGCHLD )
		return;
	while( ( pid = waitpid( -1, NULL, WNOHANG ) ) > 0 )		/*因为调用sigaction时,当前的信号会被阻塞,而被阻塞的信号以后只会发送一次,所以需要此处多处waitpid*/
															/*小于0表示出错.等于0表示无可等待的子进程*/
	{
		DMsg( "Child [%d] terminated", pid );
	}
	return;
}

/*定义对SIGCHLD信号的处理*/
int WaitChild( )
{
	sigset_t			set;
	sigset_t			oset;

	sigemptyset( &set );
	sigaddset( &set, SIGCHLD );

	if( sigprocmask( SIG_UNBLOCK, &set, &oset ) < 0 )	/*如果原先屏蔽字中有SIGCHLD,则解除该屏蔽*/
	{
		return -1;
	}

	if( Signal( SIGCHLD, WhenSigChld ) == ( Sigfunc* )( -1 ) )
		return -1;

	return 0;
}

static void WhenSigAlrm( int sig )
{
	if( sig != SIGALRM )
		return;
	CatchSigAlrm = TRUE;
	Signal( sig, WhenSigAlrm);
}

Sigfunc *SetAlrm( uint sec, Sigfunc *pfWhenAlrm )
{

	CatchSigAlrm = FALSE;
	SetSigAlrm = FALSE;

	if( sec == TIME_INFINITE )
		return NULL;

	/*设置对SIGALRM信号的处理*/
	if( pfWhenAlrm == NULL )
		pfOldFunc = Signal( SIGALRM, WhenSigAlrm );
	else
		pfOldFunc = Signal( SIGALRM, pfWhenAlrm );

	if( pfOldFunc == (Sigfunc*)-1 )
	{
		return (Sigfunc*)-1;
	}

	/*设置闹钟*/
	alarm( sec );

	SetSigAlrm = TRUE;

	return pfOldFunc;
}

void UnSetAlrm( )
{
	CatchSigAlrm = FALSE;

	if( !SetSigAlrm )
		return;

	alarm( 0 );

	/*设置对SIGALRM信号的处理*/
	Signal( SIGALRM, pfOldFunc );
	pfOldFunc = NULL;

	SetSigAlrm = FALSE;

	return;
}

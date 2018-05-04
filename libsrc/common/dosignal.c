#include "dosignal.h"

#define __DOSIANAL_STATIC__
int CatchSigAlrm;
int SetSigAlrm;

/*���ڼ�¼�ϵ�alarm������ָ��*/
Sigfunc 			*pfOldFunc=NULL;

/*ȱʡ��alarm������*/
static void WhenSigAlrm( int sig );

/*���յ�SIGCHLDʱ,�ȴ��������ӽ���.��ֹ������ʬ����*/
static void WhenSigChld(int sig);


/*�Զ���һ��signal����.ժ��<unix�����߼����>( stevens )*/
/*���ϵͳ֧�֣��������ԶԱ���SIGALRM�ź��жϵ�ϵͳ�����Զ���������*/
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

/*����SIGCHLD�źŵĴ�����*/
static void WhenSigChld( int sig )
{
	pid_t	pid;

	if( sig != SIGCHLD )
		return;
	while( ( pid = waitpid( -1, NULL, WNOHANG ) ) > 0 )		/*��Ϊ����sigactionʱ,��ǰ���źŻᱻ����,�����������ź��Ժ�ֻ�ᷢ��һ��,������Ҫ�˴��ദwaitpid*/
															/*С��0��ʾ����.����0��ʾ�޿ɵȴ����ӽ���*/
	{
		DMsg( "Child [%d] terminated", pid );
	}
	return;
}

/*�����SIGCHLD�źŵĴ���*/
int WaitChild( )
{
	sigset_t			set;
	sigset_t			oset;

	sigemptyset( &set );
	sigaddset( &set, SIGCHLD );

	if( sigprocmask( SIG_UNBLOCK, &set, &oset ) < 0 )	/*���ԭ������������SIGCHLD,����������*/
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

	/*���ö�SIGALRM�źŵĴ���*/
	if( pfWhenAlrm == NULL )
		pfOldFunc = Signal( SIGALRM, WhenSigAlrm );
	else
		pfOldFunc = Signal( SIGALRM, pfWhenAlrm );

	if( pfOldFunc == (Sigfunc*)-1 )
	{
		return (Sigfunc*)-1;
	}

	/*��������*/
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

	/*���ö�SIGALRM�źŵĴ���*/
	Signal( SIGALRM, pfOldFunc );
	pfOldFunc = NULL;

	SetSigAlrm = FALSE;

	return;
}

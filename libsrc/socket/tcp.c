/* 套接口API for IPV4*/

#include "../../include/tcp.h"

in_port_t GetTcpPort( const char *sPortName )
{
	struct servent *serverEntry;
	in_port_t	 port;

	Assert( sPortName != NULL && *sPortName != 0 );

	serverEntry = getservbyname( sPortName, "tcp" );
	if( serverEntry == NULL )
	{
		if( !IsDigital( sPortName ) || ( port = htons( atoi( sPortName ) ) ) == 0 )	/*非法的数字 或者 非法的端口*/
		{
			RptLibErr( TCP_ILL_PORT );
			return INPORT_NONE;
		}
	}
	else
		port = serverEntry->s_port;

	return port;
}

in_addr_t GetTcpAddr( const char *sHostName )
{
	struct hostent *pHost;
	in_addr_t 		addr;

	Assert( sHostName != NULL && *sHostName != 0 );

	pHost = gethostbyname( sHostName );
	if( pHost == NULL )
	{
		addr = inet_addr( sHostName );
		if( addr == INADDR_NONE )	/*INADDR_NONE 0xFFFFFFFF*/
		{
			RptLibErr( TCP_ILL_IP );
			return INADDR_NONE;
		}
	}
	else
		addr = *( (in_addr_t *)pHost->h_addr );

	return addr;
}

/*****************************************************************************************
	 在一个不调用bind的客户上,当connect成功后,可以调用该函数返回内核分配的地址和端口号
	 在以端口0调用bind后,该函数可以返回内核分配的端口号
	 在选择了通配地址的tcp服务器上,一旦与客户建立了连接,就可以用已连接套接口调用该函数返回内核选择的地址

	 该函数返回的端口号为主机字节序
******************************************************************************************/
int GetSockInfo( int sockFd, char *sIP, in_port_t *port )
{
	struct sockaddr_in	sockAddr;

#if defined(__SCO__)
	int		len;
#elif defined(__AIX__)
	size_t len;
#elif defined(__LINUX__)
	socklen_t len;
#endif

	char 	*sIPAddr;

	len = sizeof( struct sockaddr_in );
	if( getsockname( sockFd, (SA*)&sockAddr, &len ) < 0 )
	{
		RptLibErr( errno );
		return -1;
	}

	if( sockAddr.sin_family != AF_INET )
	{
		RptLibErr( TCP_ILL_FAM );	/*非法协议 AF_INET为ipv4协议*/
		return -1;
	}

	if( sIP != NULL )
	{
		sIPAddr = inet_ntoa( sockAddr.sin_addr );
		if( sIPAddr == (char*)-1 )
		{
			RptLibErr( errno );
			return -1;
		}
		strcpy( sIP, sIPAddr );
	}

	if( port != NULL )
		*port = ntohs( sockAddr.sin_port );

	return 0;
}

int GetPeerInfo( int sockFd, char *sIP, in_port_t *port )
{
	struct sockaddr_in	sockAddr;
#ifdef __SCO__
	int		len;
#else
	size_t	len;
#endif
	char 	*sIPAddr;

	len = sizeof( struct sockaddr_in );
	if( getpeername( sockFd, (SA*)&sockAddr, &len ) < 0 )
	{
		RptLibErr(  errno );
		return -1;
	}

	if( sockAddr.sin_family != AF_INET )
	{
		RptLibErr( TCP_ILL_FAM );
		return -1;
	}

	if( sIP != NULL )
	{
		sIPAddr = inet_ntoa( sockAddr.sin_addr );
		if( sIPAddr == (char*)-1 )
		{
			RptLibErr( errno );
			return -1;
		}
		strcpy( sIP, sIPAddr );
	}

	if( port != NULL )
		*port = ntohs( sockAddr.sin_port );

	return 0;
}

int TcpServInit( const char *sPortName, int backLog )
{

	struct sockaddr_in serverAddr;
	in_port_t	port;
	int			sockFd;
	int 		opt;


	if( backLog < 5 )
		backLog = 5;

	port = GetTcpPort( sPortName );	/*根据端口号或者端口名取端口 取出的port为网络字节序*/
	if( port == INPORT_NONE )	/*GetTcpPort已经报告了错误,此处省略*/
		return -1;

	sockFd = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
	if( sockFd < 0 )
	{
		RptLibErr( errno );
		return -1;
	}

	opt = 1;	/*标志打开*/
	setsockopt( sockFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof( opt ) );	/*设置端口可重用*/

	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);	/*htonl 主机字节序到网络字节序.不过INADDR_ANY为0,所以可以不用*//*INADDR_ANY 由内核选择地址*/
	serverAddr.sin_port = port;

	if( bind( sockFd, (SA *)&serverAddr, sizeof( serverAddr ) ) < 0 )
	{
		RptLibErr( errno );
		return -1;
	}

	if( listen( sockFd, backLog ) < 0 )
	{
		RptLibErr( errno );
		return -1;
	}

	return sockFd;
}

int TcpAccept( int sock, SockPair *pSockPair  )
{
	int			newsock;
#ifdef __SCO__
	int		len;
#else
	size_t	len;
#endif
	char 		*sIP;
	in_port_t 	port;

	struct sockaddr_in ClientAddr;


	while(1)
	{
		len = sizeof( ClientAddr );
		memset( &ClientAddr, 0, len );

		newsock = accept( sock, ( SA* )&ClientAddr, &len );
		if( newsock < 0 && errno == EINTR ) /*被信号中断*/
			continue;
		break;
	}

	if( newsock >= 0 )
	{
		if(  pSockPair != NULL )
		{
			sIP = inet_ntoa( ClientAddr.sin_addr );
			if( sIP == (char*)(-1) )
			{
				RptLibErr( TCP_ILL_IP );
				close( newsock );
				return -1;
			}
			port = ntohs( ClientAddr.sin_port );

			if( pSockPair != NULL )
			{
				strcpy( pSockPair->sClientAddr, sIP );
				pSockPair->clientPort = port;

				DMsg( "Connection from 	[%s].port [%d]", sIP, port );


				if( GetSockInfo( newsock, pSockPair->sServAddr, &(pSockPair->servPort) ) < 0 )
				{
					close( newsock );
					return -1;
				}
				DMsg( "Local Addr [%s].port [%d]", pSockPair->sServAddr, pSockPair->servPort );
			}
		}
		return newsock;
	}

	RptLibErr( errno );
	return -1;
}

/*当nTimeOut设置为TIME_INFINITE或者设置的时间比系统缺省时间长时,
 connect在完成返回前都只等待系统缺省时间*/
int TcpConnect( const char *sIPName, const char *sPortName, int nTimeOut )
{
	struct sockaddr_in serverAddr;
	int sock;
	in_port_t port;
	in_addr_t addr;
	int ret;

	Sigfunc *OldAction;

	Assert( sIPName != NULL && *sIPName != 0 );
	Assert( sPortName != NULL && *sPortName != 0 );

	if( ( port = GetTcpPort( sPortName ) ) == INPORT_NONE )
	{
		RptLibErr( TCP_ILL_PORT );
		return -1;
	}

	if( ( addr = GetTcpAddr( sIPName ) ) == INADDR_NONE )
	{
		RptLibErr( TCP_ILL_IP );
		return -1;
	}

	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = addr;
	serverAddr.sin_port = port;

	sock = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
	if( sock < 0 )
	{
		RptLibErr( errno );
		return -1;
	}

	if( nTimeOut == TIME_INFINITE )
	{
		ret = connect( sock, (struct sockaddr *)&serverAddr, sizeof( serverAddr ) );
	}
	else
	{
		OldAction = SetAlrm( nTimeOut, NULL );  /*如果在alarm调用后,connect调用前超时,则connect等待时间为缺省时间*/
		ret = connect( sock, (struct sockaddr *)&serverAddr, sizeof( serverAddr ) );
		UnSetAlrm();
	}

	if( ret == 0 )
		return sock;

	close( sock );

	RptLibErr( errno );

	return -1;

}

void __TcpClose( int *psock )
{
	Assert( *psock >= 0 );

	close( *psock );
	*psock = -1;
}

/*
	0	:	成功
	-1	:	失败
	-2	:	超时
*/
ssize_t TcpSendDataByHead( int fd, const char *s, size_t len, size_t headlen, int timeout )
{
	ssize_t		retlen;
	char		*head;
	char		*t;
	char		buf[TCP_MAX_SIZE];
	size_t		sendlen;
	int			mflag; 	/*标识是否分配了空间 0未分配. 1已分配*/
	int			n;

	Assert( s != NULL && *s != 0 );
	Assert( len != 0 && headlen != 0 );
	Assert( timeout > 0 || timeout == TIME_INFINITE );

	mflag = 0;
	sendlen = headlen+len;

	if( sizeof(buf) >= sendlen )	/*预先分配的变量buf的空间足够容纳所有要发送的数据*/
		t = buf;
	else							/*预先分配的变量buf的空间不足容纳所有要发送的数据,故分配空间*/
	{
		t = (char*)Malloc(sendlen);
		if( t == NULL )
		{
			RptLibErr( errno );
			return -1;
		}
		mflag = 1;
	}

	sprintf( t, "%0*d", headlen, len );
	memcpy( t+headlen, s, len );

	n = WriteByLengthInTime( fd, t, sendlen, timeout );

	if( mflag == 1 )
		Free(t);

	if( n == sendlen )	/*数据全部发送*/
		return 0;
	else if( n == -1 ) /*出错*/
		return -1;
	else if( n >= 0 && n < sendlen )
	{
		if( GetLibErrno() == ETIME ) /*超时*/
			return -2;
		else
			return -1;
	}

}

/*
	s		:	保存读到的数据
	buflen	: 	指针。*buflen在输入时为s能接受的最大长度的数据（不包含尾部的0.如char s[100], *buflen = sizeof(s) -1 )
							在输出时为s实际保存的数据的长度(不包含尾部的0.)
	返回值	:
				0		成功
				1		成功.但数据长度超过能接受的最大长度.数据被截短
				-1		失败
				-2		超时或者连接过早关闭
*/

ssize_t TcpRecvDataByHead( int fd, char *s, size_t *buflen, size_t headlen, int timeout )
{
	ssize_t		recvlen;
	char		head[TCP_MAX_HEAD_LEN + 1];
	char		buf[TCP_MAX_SIZE];
	char		*t;
	size_t		len;
	int			mflag;
	int 		n;
	int			ret;

	Assert( TCP_MAX_HEAD_LEN >= headlen );
	Assert( s != NULL );

	mflag = 0;

	memset( buf, 0, sizeof(buf) );
	memset( head, 0, sizeof(head) );

	n = ReadByLengthInTime( fd, head, headlen, timeout );
	if( n == -1 )
		return -1;
	else if( n >= 0 && n < headlen )
	{
		if( GetLibErrno() == ETIME )	/*超时*/
			return -2;
		else
			return -1;
	}

	if( !IsDigital(head) )
		return -3;	/*非法的包长度*/

	len = atoi(head);

	if( sizeof(buf) >= len )
	{
		t = buf;
		memset( t, 0, *buflen );
	}
	else
	{
		t = Malloc( len );
		if( t == NULL )
			return -1;
		memset( t, 0, len );
		mflag = 1;
	}

	n = ReadByLengthInTime( fd,t, len, timeout );

	if( n == -1 )
		ret = -1;
	else if( n >= 0 && n < len )
	{
		*buflen = 0;
		ret = -2;
	}
	else
	{
		if( *buflen >= len  )
		{
			memcpy( s, t, len );
			s[len] = 0;
			*buflen = len;
			ret = 0;
		}
		else
		{
			memcpy( s, t, *buflen );
			s[*buflen] = 0;
			ret = 1;
		}
	}

	if( mflag == 1 )
		Free( t );

	return ret;
}

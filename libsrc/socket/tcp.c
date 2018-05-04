/* �׽ӿ�API for IPV4*/

#include "../../include/tcp.h"

in_port_t GetTcpPort( const char *sPortName )
{
	struct servent *serverEntry;
	in_port_t	 port;

	Assert( sPortName != NULL && *sPortName != 0 );

	serverEntry = getservbyname( sPortName, "tcp" );
	if( serverEntry == NULL )
	{
		if( !IsDigital( sPortName ) || ( port = htons( atoi( sPortName ) ) ) == 0 )	/*�Ƿ������� ���� �Ƿ��Ķ˿�*/
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
	 ��һ��������bind�Ŀͻ���,��connect�ɹ���,���Ե��øú��������ں˷���ĵ�ַ�Ͷ˿ں�
	 ���Զ˿�0����bind��,�ú������Է����ں˷���Ķ˿ں�
	 ��ѡ����ͨ���ַ��tcp��������,һ����ͻ�����������,�Ϳ������������׽ӿڵ��øú��������ں�ѡ��ĵ�ַ

	 �ú������صĶ˿ں�Ϊ�����ֽ���
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
		RptLibErr( TCP_ILL_FAM );	/*�Ƿ�Э�� AF_INETΪipv4Э��*/
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

	port = GetTcpPort( sPortName );	/*���ݶ˿ںŻ��߶˿���ȡ�˿� ȡ����portΪ�����ֽ���*/
	if( port == INPORT_NONE )	/*GetTcpPort�Ѿ������˴���,�˴�ʡ��*/
		return -1;

	sockFd = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
	if( sockFd < 0 )
	{
		RptLibErr( errno );
		return -1;
	}

	opt = 1;	/*��־��*/
	setsockopt( sockFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof( opt ) );	/*���ö˿ڿ�����*/

	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);	/*htonl �����ֽ��������ֽ���.����INADDR_ANYΪ0,���Կ��Բ���*//*INADDR_ANY ���ں�ѡ���ַ*/
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
		if( newsock < 0 && errno == EINTR ) /*���ź��ж�*/
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

/*��nTimeOut����ΪTIME_INFINITE�������õ�ʱ���ϵͳȱʡʱ�䳤ʱ,
 connect����ɷ���ǰ��ֻ�ȴ�ϵͳȱʡʱ��*/
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
		OldAction = SetAlrm( nTimeOut, NULL );  /*�����alarm���ú�,connect����ǰ��ʱ,��connect�ȴ�ʱ��Ϊȱʡʱ��*/
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
	0	:	�ɹ�
	-1	:	ʧ��
	-2	:	��ʱ
*/
ssize_t TcpSendDataByHead( int fd, const char *s, size_t len, size_t headlen, int timeout )
{
	ssize_t		retlen;
	char		*head;
	char		*t;
	char		buf[TCP_MAX_SIZE];
	size_t		sendlen;
	int			mflag; 	/*��ʶ�Ƿ�����˿ռ� 0δ����. 1�ѷ���*/
	int			n;

	Assert( s != NULL && *s != 0 );
	Assert( len != 0 && headlen != 0 );
	Assert( timeout > 0 || timeout == TIME_INFINITE );

	mflag = 0;
	sendlen = headlen+len;

	if( sizeof(buf) >= sendlen )	/*Ԥ�ȷ���ı���buf�Ŀռ��㹻��������Ҫ���͵�����*/
		t = buf;
	else							/*Ԥ�ȷ���ı���buf�Ŀռ䲻����������Ҫ���͵�����,�ʷ���ռ�*/
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

	if( n == sendlen )	/*����ȫ������*/
		return 0;
	else if( n == -1 ) /*����*/
		return -1;
	else if( n >= 0 && n < sendlen )
	{
		if( GetLibErrno() == ETIME ) /*��ʱ*/
			return -2;
		else
			return -1;
	}

}

/*
	s		:	�������������
	buflen	: 	ָ�롣*buflen������ʱΪs�ܽ��ܵ���󳤶ȵ����ݣ�������β����0.��char s[100], *buflen = sizeof(s) -1 )
							�����ʱΪsʵ�ʱ�������ݵĳ���(������β����0.)
	����ֵ	:
				0		�ɹ�
				1		�ɹ�.�����ݳ��ȳ����ܽ��ܵ���󳤶�.���ݱ��ض�
				-1		ʧ��
				-2		��ʱ�������ӹ���ر�
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
		if( GetLibErrno() == ETIME )	/*��ʱ*/
			return -2;
		else
			return -1;
	}

	if( !IsDigital(head) )
		return -3;	/*�Ƿ��İ�����*/

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

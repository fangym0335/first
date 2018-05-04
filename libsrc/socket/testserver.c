#include "mycomm.h"
#include "tcp.h"


main()
{
	int sock;
	int newsock;
	SockPair stSockPair;
	char sIP[16];
	in_port_t port;
	char 		s[100];
	int			slen;
	int			ret;
	
	sock = TcpServInit("9998",5 );
	if( sock < 0 )
	{
		DMsg( "TcpServInit Failed" );
		return -1;
	}
	
	while(1)
	{
		newsock = TcpAccept( sock, &stSockPair );

		if( GetSockInfo( newsock, sIP, &port ) < 0 )
		{
			DMsg( "GetSockInfo Failed" );
			return -1;
		}
		
		DMsg( "SERVER:sIP=[%s],port=[%d]", sIP, port );

		if( GetPeerInfo( newsock, sIP, &port ) < 0 )
		{
			DMsg( "GetPeerInfo Failed" );
			return -1;
		}
		DMsg( "Client:sIP=[%s],port=[%d]", sIP, port );

		DMsg( "servaddr = [%s], port = [%d], clientaddr = [%s], port = [%d]", stSockPair.sServAddr, stSockPair.servPort, stSockPair.sClientAddr, stSockPair.clientPort );
		
		slen = sizeof(s) -1;
		
		ret = TcpRecvDataByHead( newsock, s, &slen, 4, 10 );
		
		DMsg( "ret = [%d], s = [%s], slen = [%d]", ret, s, slen );
		
		TcpClose( newsock );
	}
	TcpClose(sock);
	
	return 0;
}

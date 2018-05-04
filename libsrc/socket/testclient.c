#include "mycomm.h"
#include "tcp.h"


main( int argc, char *argv[] )
{
	int sock;
	SockPair stSockPair;
	char sIP[16];
	in_port_t port;
	int ret;
	
	
	if( argc != 2 )
	{
		printf( "USAGE : testclient string " );
		return -1;
	}
	
	sock = TcpConnect("21.37.17.204","9998",10 );
	if( sock < 0 )
	{
		DMsg( "TcpConnect Failed" );
		return -1;
	}
	
	if( GetSockInfo( sock, sIP, &port ) < 0 )
	{
		DMsg( "GetSockInfo Failed" );
		return -1;
	}
	DMsg( "Client:sIP=[%s],port=[%d]", sIP, port );

	if( GetPeerInfo( sock, sIP, &port ) < 0 )
	{
		DMsg( "GetPeerInfo Failed" );
		return -1;
	}
	DMsg( "Server:sIP=[%s],port=[%d]", sIP, port );
	
	ret = TcpSendDataByHead( sock, argv[1], strlen(argv[1]), 4, 10 );
	DMsg( "ret = [%d]", ret );
	
	TcpClose(sock);
	
	return 0;
}

#include <stdio.h>
#include <mycomm.h>

int main(int argc, char *argv[])
{
	int sockfd;
	char sIP[16];
	char sPort[6];

	if( argc != 3 ) {
		printf( "Usage: testconn ip port\n" );
		exit(-1);
	}

	strcpy( sIP, argv[1] );
	strcpy( sPort, argv[2] );

	sockfd = TcpConnect( sIP, sPort, 10 );
	if( sockfd < 0 ) {
		printf( "Connect Ip[%s],Port[%s] Failed.\n", sIP, sPort );
		exit(-1);
	}
	printf( "Connect Ip[%s],Port[%s] Success.\n", sIP, sPort );

	return 0;
}

#ifndef __PUBSOCK_H__
#define __PUBSOCK_H__


#include "pub.h"

//change InitCfg to InitIpCfg by chenfm 2007.8.3
int InitPubCfg();
//int SendDataToCon( int sockfd, unsigned char *cmd, int len );
//int SendErrInfor(int sockfd, char channel, char tty_no);
void Ssleep(long usec);

#endif

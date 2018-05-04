#ifndef __TTY_H__
#define __TTY_H__

//#include "pub.h"
#include <termios.h>
#include <unistd.h>

extern char alltty[][11];
int ReadTTY( int portnum );
int SendDataToChannel( char *str, size_t len, char chn,char chnLog, long indtyp );
int OpenSerial(char *ttyname);


#endif

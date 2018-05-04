#include "pub.h"
#include "log.h"
#include "cgidecoder.h"

#define WRITE_TTY_HTML_FORM_FILE "writetty.txt"
char alltty[][11] =
{
	"/dev/ttyS1",
	"/dev/ttyS2",
	"/dev/ttyS3",
	"/dev/ttyS4",
	"/dev/ttyS5",
	"/dev/ttyS6"
};

struct chncmd{
	unsigned char	wantlen;
	int				indtyp;
	unsigned char	len;
	unsigned char	cmd[MAX_ORDER_LEN+1];
};

int SendDataToChannel( char *str, size_t len, char chn, char chnLog, long indtyp ){
	//RunLog( "SendDataToChannel	[%s]", Asc2Hex(str,len) );
	//RunLog( "senddatatochannel.通道号[%d]逻辑通道号[%d],数据[%s]", chn, chnLog, Asc2Hex(str,len) );
	return SendMsg( GUN_MSG_TRAN, str, len,indtyp, 3 );
}


int OpenSerial( char *ttynam )
{
	int fd;
	struct termios termios_new;

	fd = open (ttynam, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if( fd < 0 )
	{
		//UserLog( "打开设备[%s]失败", ttynam );
		return -1;
	}
	tcgetattr(fd, &termios_new);
	cfsetispeed(&termios_new,B115200);
	cfsetospeed(&termios_new,B115200);
	/*set 8 bit */
	termios_new.c_cflag &= ~CSIZE;
	termios_new.c_cflag |= CS8;
	/*parity checking */
	termios_new.c_cflag &= ~PARENB;   /* Clear parity enable */
	/*1 stop bit */
	termios_new.c_cflag &= ~CSTOPB;
	/*no start bit */
	termios_new.c_cflag |= CLOCAL;
	termios_new.c_cflag |= CREAD;           //make port can read
	termios_new.c_cflag &= ~CRTSCTS;  //NO flow control
	termios_new.c_oflag = 0;
	termios_new.c_lflag  = 0;
	termios_new.c_iflag &= ~(IXON |IXOFF |IXANY);
	termios_new.c_iflag &= ~(INPCK | ISTRIP | ICRNL);
	termios_new.c_cc[VTIME] = 1;       /* unit: 1/10 second. */
	termios_new.c_cc[VMIN] = 1; /* minimal characters for reading */

	tcflush (fd, TCIOFLUSH);
	/* 0 on success, -1 on failure */
	if( tcsetattr(fd, TCSANOW, &termios_new) == 0 )	/*success*/
		return fd;
	return -1;

}

int ClearSerial( int fd )
{
	int n;
	char	str[3];

	str[2] = 0;

	while(1)
	{
		n = read( fd, str, 2 );
		if( n == 0 )
			return 0;
		else if( n < 0 && errno == EAGAIN ) /*无数据.返回*/
			return 0;
		else if( n < 0 && n != EINTR ) /*出错并且不是被信号打断*/
			return -1;
		/*其他情况继续读*/
	}
}

ssize_t Read2( int fd, char *vptr )
{
	size_t	nleft;
	ssize_t	nread;
	char	*ptr;

	ssize_t n;
	n = 0;

	/*读第一位*/
	ptr = vptr;
	while(1)
	{
		if ( (nread = read(fd, ptr, 1)) < 0 )
		{
			if( errno == EAGAIN )	/*无数据*/
				continue;
			else if (errno == EINTR)	/*被信号打断*/
				nread = 0;
			else
				return(-1);
		}
		else if (nread == 0)
			return n;				/* EOF */

		if( ptr[0] != 0x00 )	/*0x00抛弃。其他认为是正常数据*/
		{
			n = 1;
			break;
		}
		//UserLog( "throw 0x00 away" );
	}

	/*读第二位*/
	ptr = vptr+1;
	while(1)
	{
		if ( (nread = read(fd, ptr, 1)) < 0 )
		{
			if( errno == EAGAIN )	/*无数据*/
				continue;
			if (errno == EINTR)	/*被信号打断*/
				nread = 0;
			else
				return(-1);
		}
		else if (nread == 0)
			return n;				/* EOF */

		n = 2;
		break;

	}
	return n;
}

int WriteToPort( int fd, int chnLog, int chnPhy, char *buff, size_t len, int flag, char wantLen, int timeout )
{
	char		cmd[MAX_ORDER_LEN+1];
	int			cnt;
	int			ret;

	if( flag == 1 )	/*flag=1表示需要发送时组包*/
	{
		cmd[0] = 0x1B;
		cmd[1] = 0x30+chnPhy-1;
		if( len > MAX_PORT_CMDLEN )
		{
			//RunLog( "向端口写的数据超过最大长度[%d]", MAX_PORT_CMDLEN );
			return -1;
		}
		cmd[2] = 0x00+len;
		memcpy( cmd+3, buff, len );
		cnt = len+3;
	}
	else
	{
		cnt = len;
		memcpy( cmd, buff, len );
	}

	//RunLog( "WriteToPort [%s]", Asc2Hex(cmd, cnt) );
	/*
	if( wantLen != 0 )	//期待返回值//
	{
		gpGunShm->wantLen[chnLog-1] = wantLen;
		gpGunShm->wantInd[chnLog-1] = gpGunShm->useInd[chnLog-1];
	}
	*/
	//RunLog("In tty.c***chnLog is: %d , gpgunshm wantLen is: %d\n",chnLog,gpGunShm->wantLen[chnLog-1]);
	Act_P( PORT_SEM_INDEX );	/*对端口的写不知道是不是原子操作。用PV原语保险点*/

	ret = WriteByLengthInTime(fd, cmd, cnt, timeout );

	Act_V( PORT_SEM_INDEX );

	return ret;
}


main( )
{
	int fd;
	char ttynam[30];
	int	 portnum;
	int	chnPhy;
	int	ret;
	char msg[512],tmp[2],msg2[512];
	unsigned char *result;
	int i,len,disp_all=0;
	char bodybuff[4096];
	//extern NAME_VALUE *nv;
	
	ret = CheckUserTime(5, NULL);
	if(ret < 0){
		moved("User time out", "/relogin.htm");
		exit(1);
	}	
	ret = getCgiInput();
	if(ret<0){
		cgi_error( "获取表单输入错误" );
		exit(-1);
	}
	if(0 == ret ){
		//moved("move to ", "./writp.htm");
		bzero(bodybuff,sizeof(bodybuff));
		ret = ReadTextFile(WRITE_TTY_HTML_FORM_FILE, bodybuff);
		if(ret < 0){
			cgi_error( "读取WRITE_TTY_HTML_FORM_FILE错误" );
			exit(-1);
		}
		printf(bodybuff);
		exit(0);
	}
	//(void) printf( "\
	//	<HTML><HEAD><TITLE>%s</TITLE></HEAD>\n\
	//	<BODY    BGCOLOR=#99cc99 TEXT=#000000 LINK=#2020ff VLINK=#4040cc>test</BODY></HTML>");
		 
	portnum = atoi( nv[0].value );	
	
	if(portnum <= 0){
		cgi_error( "表单输入的串口号错误" );
		exit(1);
	}
	
	strcpy( ttynam, alltty[portnum+1] );//1->3,2->4......
	
	
	
	fd = OpenSerial( ttynam);
	if( fd < 0 )
	{
		cgi_error( "打开串口失败" );
		exit(1);
	}
	//cgi_ok( "after getCgiInput and 0 to moved test!" );
	//exit(1);
	memset( msg, 0, sizeof(msg) );
	memcpy( msg, Hex2Asc(nv[1].value,strlen(nv[1].value)), strlen(nv[1].value)/2 );

	//PMsg( "WriteToPort[%s]", Asc2Hex(Hex2Asc(argv[2],strlen(argv[2])),strlen(argv[2])/2) );
	ret = WriteByLengthInTime( fd, Hex2Asc(nv[1].value, strlen(nv[1].value)),strlen(nv[1].value)/2, 3 );
	if( ret < 0 )
	{
		cgi_error( "Write Failed" );
		exit(1);
	}

	
	memset( msg, 0, sizeof(msg) );

	ret = ReadByLengthInTime( fd, msg, sizeof(msg)-1, 2 );
	if( ret < 0 )
	{
		cgi_error( "Read Failed" );
		exit(1);
	}

	result=Asc2Hex(msg,ret);
	len = strlen(result);
	bzero(bodybuff,sizeof(bodybuff));
	if (disp_all == 0){
		if (len % 2 != 0){
		cgi_error( "Read Failed" );
		exit(1);
		//return -1;
		}
			
		len=len/2;
		memset(msg,0,sizeof(msg));	
		memset(msg2,0,sizeof(msg2));	
		memcpy(msg,result,sizeof(msg));	

	
		for (i=0;i<len;i++)
		{
			if (i % 2 == 0 )
				continue;
			else
			{
				memset(tmp,0,2);
				memcpy(tmp,&(msg[i*2]),2);
				strcat(msg2,tmp);
			}
		}
		sprintf(bodybuff, "%s\n",msg2);
	}
	else
		sprintf(bodybuff, "%s\n",result);
	
	cgi_disp(bodybuff);
	exit(0);
	//return 0;
}




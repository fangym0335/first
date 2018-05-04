#include "tty.h"
#include "ifsf_pub.h"

/*
 * Guojie Zhu @ 8.20
 * 修改ReadTTY中调用AddJustLen时传入的参数ChnCmd[].len为 &ChmCmd[].len
 */

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
	int indtyp;
	int len;
	unsigned char	cmd[MAX_ORDER_LEN+1];
};

#define REOPENTTY()	{                                         \
	gpGunShm->sysStat = PAUSE_PROC;	/*让通道处理暂停*/        \
                                                                  \
	sleep(PAUSE_FOR_TTY);	/*停止一段时间*/                  \
                                                                  \
	close(fd);	/*关闭打开的tty*/                         \
	fd = OpenSerial( ttynam ); /*重新打开tty*/                \
	if (fd < 0) {                                             \
		UserLog("打开设备[%s]失败", ttynam);              \
		return -1;                                        \
	}                                                         \
	UserLog("重新打开串口[%s].fd=[%d]", ttynam, fd);          \
                                                                  \
	maxFd = 0;                                                \
	FD_ZERO(&set);                                            \
	FD_SET(fd, &set);                                         \
                                                                  \
	if (fd >= maxFd) {                                        \
		maxFd = fd + 1;                                   \
	}                                                         \
	memcpy( &setbak, &set, sizeof(fd_set) );                  \
                                                                  \
	gpGunShm->sysStat = RUN_PROC;	/*恢复通道处理*/          \
}


/*监听端口*/
int ReadTTY( int portnum )
{
	int		i;
	int		j;
	int		k;
	int		n;
	int		fd;
	char	ttynam[30];
	char	chn;
	int	len;
	long	indtyp;
	char	retlen;	         /*返回长度*/
	int		ret;
	int		gunCnt;	/*总枪数*/
	unsigned char	buf[3];
	unsigned char	chnPhy;	/*物理通道号*/
	unsigned char	chnLog;	/*逻辑通道号*/
	unsigned char port[MAX_CHANNEL_PER_PORT_CNT];
	struct chncmd ChnCmd[MAX_CHANNEL_CNT];	/*每个通道对应一个结构*/
	int		maxFd;
	fd_set	set;
	fd_set	setbak;	/*select返回时set被修改，这是在select之前保存set的值*/

	memset( ChnCmd, 0, sizeof(ChnCmd) );

	//gunCnt = gpGunShm->gunCnt;	/*取枪总数*/

	j = 0;

	strcpy( ttynam, alltty[portnum-1] );			/*取设备名*/
	fd = OpenSerial( ttynam );
	if( fd < 0 ) {
		/*错误处理*/
		UserLog( "打开设备[%s]失败", ttynam );
		return -1;
	}

	UserLog( "打开串口[%s].fd=[%d]", ttynam, fd );

	/*搜索共享内存,查找该串口下的物理通道号chnPhy和逻辑通道号chnLog*/
	/*每个串口的物理通道号chnPhy从1到8.这里用物理通道号chnPhy-1作为索引.存放该物理通道号对应的逻辑通道号chnLog*/
	
	for(i=0; i<MAX_CHANNEL_CNT; i++){
		if(gpIfsf_shm->node_channel[i].serialPort == (unsigned char )portnum){
			chnLog = gpIfsf_shm->node_channel[i].chnLog;
			chnPhy = gpIfsf_shm->node_channel[i].chnPhy;
			port[chnPhy-1] = chnLog;
			UserLog( "串口[%d]监听逻辑通道[%d]物理通道[%d]的数据", portnum, chnLog, chnPhy );
		}
	}	

	len = 0;
	chn = 0;
	chnLog = 0;

	/*将fd加入到要监听的描述符*/
	maxFd = 0;
	FD_ZERO( &set );
	FD_SET( fd, &set );

	if (fd >= maxFd) {
		maxFd = fd + 1;
	}
	memcpy( &setbak, &set, sizeof(fd_set) );

	while(1) {
		ret = select( maxFd, &set, NULL, NULL, NULL );  /*等到至少一个描述符准备好*/
		if( ret < 0 ) {
			if( errno == EINTR ) {
			    /*被信号打断*/
			    continue;
			} else {
			    UserLog( "监听串口出错" );
			    return -1;
			}
		} else if( ret == 0 ) {     /*超时？不会吧？*/
			continue;
		}
		
		while (1) {
			n = ReadByLength( fd, buf, 2 );
			buf[n] = 0;
			if( n < 0 ) {
				RunLog( "读串口数据失败" );
				REOPENTTY();

				break;
			} else if(n != 2) {
				RunLog( "接收数据有误n=[%d]",n );
				REOPENTTY();

				break;
			} else {		/*读到了数据.*/
				if (buf[0] < 0x30 || buf[0] > 0x3f) {
					RunLog( "Get Data buf[0] = [%02x], buf[1] = [%02x]", buf[0], buf[1] );
					RunLog( "非法数据" );

					REOPENTTY();

					break;
				}

 				chn = buf[0] - 0x30 + 1;

				chnLog = port[chn-1];
				retlen = gpGunShm->wantLen[chnLog-1];			/*应返回的长度*/
				indtyp = gpGunShm->wantInd[chnLog-1];			/*返回的消息队列键值*/

				if( retlen == 0 ) {
					//RunLog( "异常数据.Read[0x%02x][0x%02x].wantLen[%d].getLen[%d]", \
						buf[0], buf[1], ChnCmd[chnLog-1].wantlen, ChnCmd[chnLog-1].len );
					continue;
				}

				/*检查该通道是否已经有数据.有则将数据保存,无则从共享内存管理信息*/
				if (ChnCmd[chnLog-1].wantlen != 0 && ChnCmd[chnLog-1].indtyp != indtyp) { /*保存的消息已超时*/
					memset( &ChnCmd[chnLog-1], 0, sizeof(ChnCmd[chnLog-1]));
				}

				ChnCmd[chnLog-1].wantlen = retlen;
				ChnCmd[chnLog-1].indtyp = indtyp;

				ChnCmd[chnLog-1].cmd[ChnCmd[chnLog-1].len] = buf[1];
				ChnCmd[chnLog-1].len++;
#if 0
				RunLog("##chnLog:%d, wantlen: %d, len: %d, received: %s(%d)",
					       	chnLog, ChnCmd[chnLog-1].wantlen, ChnCmd[chnLog-1].len,
					       	Asc2Hex(ChnCmd[chnLog-1].cmd, ChnCmd[chnLog-1].len), ChnCmd[chnLog-1].len);
#endif

				if (ChnCmd[chnLog-1].wantlen == ChnCmd[chnLog-1].len) {
					/*addInf[0]=00时不需要特殊处理*/
					if ((gpGunShm->addInf[chnLog-1][0] == 0x00) ||
						((gpGunShm->addInf[chnLog-1][0] != 0x00) && 
						  (AddJustLen(chnLog, ChnCmd[chnLog-1].cmd, &(ChnCmd[chnLog-1].len),
							ChnCmd[chnLog-1].indtyp, gpGunShm->addInf[chnLog-1][0]) == 0))) {
						gpGunShm->wantLen[chnLog-1] = 0;
//						RunLog( "senddatatochannel.通道号[%d]逻辑通道号[%d]", chn, chnLog );
						ret = SendDataToChannel( ChnCmd[chnLog-1].cmd, 
							ChnCmd[chnLog-1].len, chn, chnLog, indtyp );
						if( ret < 0 ) {
							RunLog("发送数据到通道(%d)监听程序失败(%d)", chnLog, ret);
						//	continue;   
						/* This line removed by jie @ 2009.5.13
						 * 错误了也要清零ChnCmd,否则错误不能恢复
						 */
						}
						memset(&ChnCmd[chnLog-1], 0, sizeof(ChnCmd[chnLog-1]));	/*初始化之*/
					}
				}
			}
		}
		memcpy( &set, &setbak, sizeof(fd_set) );
	}
	return 0;
}

int SendDataToChannel( char *str, size_t len, char chn, char chnLog, long indtyp )
{
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
		UserLog( "打开设备[%s]失败", ttynam );
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
		UserLog( "throw 0x00 away" );
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
	char	cmd[MAX_ORDER_LEN+1];
	int	cnt;
	int	ret;

	if (flag == 1) {	/*flag=1表示需要发送时组包*/
		cmd[0] = 0x1B;
		cmd[1] = 0x30+chnPhy-1;
		if( len > MAX_PORT_CMDLEN ) {
			RunLog( "向端口写的数据超过最大长度[%d]", MAX_PORT_CMDLEN );
			return -1;
		}
		cmd[2] = 0x00+len;
		memcpy( cmd+3, buff, len );
		cnt = len+3;
	} else {
		cnt = len;
		memcpy( cmd, buff, len );
	}

	if( 1 == atoi(SHOW_FCC_SEND_DATA)){
		RunLog("CHN[%02d]-->Pump: %s(%d)", chnLog,Asc2Hex(buff, len), len);
	}

	if (wantLen != 0 ) {	/*期待返回值*/
		gpGunShm->wantLen[chnLog-1] = wantLen;
		gpGunShm->wantInd[chnLog-1] = gpGunShm->useInd[chnLog-1];
	}

	Act_P( PORT_SEM_INDEX );	/*对端口的写不知道是不是原子操作。用PV原语保险点*/
	ret = WriteByLengthInTime(fd, cmd, cnt, timeout );
	Act_V( PORT_SEM_INDEX );

	return ret;
}

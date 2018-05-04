#include "tty.h"
#include "ifsf_pub.h"

/*
 * Guojie Zhu @ 8.20
 * �޸�ReadTTY�е���AddJustLenʱ����Ĳ���ChnCmd[].lenΪ &ChmCmd[].len
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
	gpGunShm->sysStat = PAUSE_PROC;	/*��ͨ��������ͣ*/        \
                                                                  \
	sleep(PAUSE_FOR_TTY);	/*ֹͣһ��ʱ��*/                  \
                                                                  \
	close(fd);	/*�رմ򿪵�tty*/                         \
	fd = OpenSerial( ttynam ); /*���´�tty*/                \
	if (fd < 0) {                                             \
		UserLog("���豸[%s]ʧ��", ttynam);              \
		return -1;                                        \
	}                                                         \
	UserLog("���´򿪴���[%s].fd=[%d]", ttynam, fd);          \
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
	gpGunShm->sysStat = RUN_PROC;	/*�ָ�ͨ������*/          \
}


/*�����˿�*/
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
	char	retlen;	         /*���س���*/
	int		ret;
	int		gunCnt;	/*��ǹ��*/
	unsigned char	buf[3];
	unsigned char	chnPhy;	/*����ͨ����*/
	unsigned char	chnLog;	/*�߼�ͨ����*/
	unsigned char port[MAX_CHANNEL_PER_PORT_CNT];
	struct chncmd ChnCmd[MAX_CHANNEL_CNT];	/*ÿ��ͨ����Ӧһ���ṹ*/
	int		maxFd;
	fd_set	set;
	fd_set	setbak;	/*select����ʱset���޸ģ�������select֮ǰ����set��ֵ*/

	memset( ChnCmd, 0, sizeof(ChnCmd) );

	//gunCnt = gpGunShm->gunCnt;	/*ȡǹ����*/

	j = 0;

	strcpy( ttynam, alltty[portnum-1] );			/*ȡ�豸��*/
	fd = OpenSerial( ttynam );
	if( fd < 0 ) {
		/*������*/
		UserLog( "���豸[%s]ʧ��", ttynam );
		return -1;
	}

	UserLog( "�򿪴���[%s].fd=[%d]", ttynam, fd );

	/*���������ڴ�,���Ҹô����µ�����ͨ����chnPhy���߼�ͨ����chnLog*/
	/*ÿ�����ڵ�����ͨ����chnPhy��1��8.����������ͨ����chnPhy-1��Ϊ����.��Ÿ�����ͨ���Ŷ�Ӧ���߼�ͨ����chnLog*/
	
	for(i=0; i<MAX_CHANNEL_CNT; i++){
		if(gpIfsf_shm->node_channel[i].serialPort == (unsigned char )portnum){
			chnLog = gpIfsf_shm->node_channel[i].chnLog;
			chnPhy = gpIfsf_shm->node_channel[i].chnPhy;
			port[chnPhy-1] = chnLog;
			UserLog( "����[%d]�����߼�ͨ��[%d]����ͨ��[%d]������", portnum, chnLog, chnPhy );
		}
	}	

	len = 0;
	chn = 0;
	chnLog = 0;

	/*��fd���뵽Ҫ������������*/
	maxFd = 0;
	FD_ZERO( &set );
	FD_SET( fd, &set );

	if (fd >= maxFd) {
		maxFd = fd + 1;
	}
	memcpy( &setbak, &set, sizeof(fd_set) );

	while(1) {
		ret = select( maxFd, &set, NULL, NULL, NULL );  /*�ȵ�����һ��������׼����*/
		if( ret < 0 ) {
			if( errno == EINTR ) {
			    /*���źŴ��*/
			    continue;
			} else {
			    UserLog( "�������ڳ���" );
			    return -1;
			}
		} else if( ret == 0 ) {     /*��ʱ������ɣ�*/
			continue;
		}
		
		while (1) {
			n = ReadByLength( fd, buf, 2 );
			buf[n] = 0;
			if( n < 0 ) {
				RunLog( "����������ʧ��" );
				REOPENTTY();

				break;
			} else if(n != 2) {
				RunLog( "������������n=[%d]",n );
				REOPENTTY();

				break;
			} else {		/*����������.*/
				if (buf[0] < 0x30 || buf[0] > 0x3f) {
					RunLog( "Get Data buf[0] = [%02x], buf[1] = [%02x]", buf[0], buf[1] );
					RunLog( "�Ƿ�����" );

					REOPENTTY();

					break;
				}

 				chn = buf[0] - 0x30 + 1;

				chnLog = port[chn-1];
				retlen = gpGunShm->wantLen[chnLog-1];			/*Ӧ���صĳ���*/
				indtyp = gpGunShm->wantInd[chnLog-1];			/*���ص���Ϣ���м�ֵ*/

				if( retlen == 0 ) {
					//RunLog( "�쳣����.Read[0x%02x][0x%02x].wantLen[%d].getLen[%d]", \
						buf[0], buf[1], ChnCmd[chnLog-1].wantlen, ChnCmd[chnLog-1].len );
					continue;
				}

				/*����ͨ���Ƿ��Ѿ�������.�������ݱ���,����ӹ����ڴ������Ϣ*/
				if (ChnCmd[chnLog-1].wantlen != 0 && ChnCmd[chnLog-1].indtyp != indtyp) { /*�������Ϣ�ѳ�ʱ*/
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
					/*addInf[0]=00ʱ����Ҫ���⴦��*/
					if ((gpGunShm->addInf[chnLog-1][0] == 0x00) ||
						((gpGunShm->addInf[chnLog-1][0] != 0x00) && 
						  (AddJustLen(chnLog, ChnCmd[chnLog-1].cmd, &(ChnCmd[chnLog-1].len),
							ChnCmd[chnLog-1].indtyp, gpGunShm->addInf[chnLog-1][0]) == 0))) {
						gpGunShm->wantLen[chnLog-1] = 0;
//						RunLog( "senddatatochannel.ͨ����[%d]�߼�ͨ����[%d]", chn, chnLog );
						ret = SendDataToChannel( ChnCmd[chnLog-1].cmd, 
							ChnCmd[chnLog-1].len, chn, chnLog, indtyp );
						if( ret < 0 ) {
							RunLog("�������ݵ�ͨ��(%d)��������ʧ��(%d)", chnLog, ret);
						//	continue;   
						/* This line removed by jie @ 2009.5.13
						 * ������ҲҪ����ChnCmd,��������ָܻ�
						 */
						}
						memset(&ChnCmd[chnLog-1], 0, sizeof(ChnCmd[chnLog-1]));	/*��ʼ��֮*/
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
	//RunLog( "senddatatochannel.ͨ����[%d]�߼�ͨ����[%d],����[%s]", chn, chnLog, Asc2Hex(str,len) );
	return SendMsg( GUN_MSG_TRAN, str, len,indtyp, 3 );
}


int OpenSerial( char *ttynam )
{
	int fd;
	struct termios termios_new;

	fd = open (ttynam, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if( fd < 0 )
	{
		UserLog( "���豸[%s]ʧ��", ttynam );
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
		else if( n < 0 && errno == EAGAIN ) /*������.����*/
			return 0;
		else if( n < 0 && n != EINTR ) /*�����Ҳ��Ǳ��źŴ��*/
			return -1;
		/*�������������*/
	}
}

ssize_t Read2( int fd, char *vptr )
{
	size_t	nleft;
	ssize_t	nread;
	char	*ptr;

	ssize_t n;
	n = 0;

	/*����һλ*/
	ptr = vptr;
	while(1)
	{
		if ( (nread = read(fd, ptr, 1)) < 0 )
		{
			if( errno == EAGAIN )	/*������*/
				continue;
			else if (errno == EINTR)	/*���źŴ��*/
				nread = 0;
			else
				return(-1);
		}
		else if (nread == 0)
			return n;				/* EOF */

		if( ptr[0] != 0x00 )	/*0x00������������Ϊ����������*/
		{
			n = 1;
			break;
		}
		UserLog( "throw 0x00 away" );
	}

	/*���ڶ�λ*/
	ptr = vptr+1;
	while(1)
	{
		if ( (nread = read(fd, ptr, 1)) < 0 )
		{
			if( errno == EAGAIN )	/*������*/
				continue;
			if (errno == EINTR)	/*���źŴ��*/
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

	if (flag == 1) {	/*flag=1��ʾ��Ҫ����ʱ���*/
		cmd[0] = 0x1B;
		cmd[1] = 0x30+chnPhy-1;
		if( len > MAX_PORT_CMDLEN ) {
			RunLog( "��˿�д�����ݳ�����󳤶�[%d]", MAX_PORT_CMDLEN );
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

	if (wantLen != 0 ) {	/*�ڴ�����ֵ*/
		gpGunShm->wantLen[chnLog-1] = wantLen;
		gpGunShm->wantInd[chnLog-1] = gpGunShm->useInd[chnLog-1];
	}

	Act_P( PORT_SEM_INDEX );	/*�Զ˿ڵ�д��֪���ǲ���ԭ�Ӳ�������PVԭ�ﱣ�յ�*/
	ret = WriteByLengthInTime(fd, cmd, cnt, timeout );
	Act_V( PORT_SEM_INDEX );

	return ret;
}

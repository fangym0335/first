#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include "pub.h"

static int OpenSerial(unsigned char *ttynam, int baudrate);
static int C2Num(unsigned char c);
static unsigned char* Asc2Hex(const unsigned char *cmd, int cmdlen);
static unsigned char* Hex2Asc(const unsigned char *cmd, int cmdlen);

unsigned char alltty[][11] =
{
	"/dev/ttyS1",
	"/dev/ttyS2",
	"/dev/ttyS3",
	"/dev/ttyS4",
	"/dev/ttyS5",
	"/dev/ttyS6"
};

/*
 *  串口通讯测试工具
 *  测试主板与IO板的连通性以及与加油机的连通性
 */

int main(int argc, unsigned char *argv[])
{
	int fd;
	int portnum;
	int ret;
	int i,len;
	unsigned char cmd;
	unsigned char *result;
	unsigned char msg[512];
	unsigned char ttynam[16];

	
	if (argc < 3) {
		printf("Usage: \n");
		printf("      writp portnum msg\n");
		printf("  eg. writp 1 1B3003010101 \n");
		printf("      writp 1 1C51 \n\n");
		return -1;
	}

	portnum = atoi(argv[1]);
	if (portnum < 10) {
		if (strlen(argv[2]) % 2 != 0) {
			printf("命令不正确\n");
			return -1;
		}

		strcpy(ttynam, alltty[portnum + 1]);
		fd = OpenSerial(ttynam, 115200);
	} else if (portnum == 11) {
		strcpy(ttynam, "/dev/ttyS1");
		fd = OpenSerial(ttynam, 9600);
	} else if (portnum == 12) {
		strcpy(ttynam, "/dev/ttyS2");
		fd = OpenSerial(ttynam, 9600);
	}

	if (fd < 0) {
		printf("打开串口 %s 失败\n", ttynam);
		return -1;
	}

	if (portnum > 10) {
		goto PORT_TEST;
	}

	memset(msg, 0, sizeof(msg));
	memcpy(msg, Hex2Asc(argv[2], strlen(argv[2])), strlen(argv[2]) / 2);
	cmd = msg[0];

	ret = WriteByLengthInTime(fd, msg, strlen(argv[2]) / 2, 3);
	if (ret < 0) {
		printf("Send [%s] to PORT failed!\n", argv[2]);
		return -1;
	}

	printf("Send [%s] to PORT OK!\n", argv[2]);

	memset(msg, 0, sizeof(msg));
	ret = ReadByLengthInTime(fd, msg, sizeof(msg)-1, 2);
	if (ret < 0) {
		PMsg("Read Message from port Failed\n");
		return -1;
	}

	result = Asc2Hex(msg,ret);
	len = strlen(result);

//	printf("Received data: ");
	printf("%s\n", result);
	if (cmd == 0x1B) {
//		printf("original data: ");
		printf("Received data: ");
		for (i = 0; i < len / 4; ++i) {
			printf("%02x", msg[i * 2 + 1]);
		}
		printf("(%d)\n", len / 4);
	}

	printf("\n");

	return 0;
//-------------------------------------------------------------------------
	
PORT_TEST:
	ret = WriteByLengthInTime(fd, argv[2], strlen(argv[2]), 3);
	if (ret < 0) {
		printf("Send [%s] to PORT failed!\n", argv[2]);
		return -1;
	}

	printf("Send [%s] to PORT OK!\n", argv[2]);

	memset(msg, 0, sizeof(msg));
	ret = ReadByLengthInTime(fd, msg, sizeof(msg) - 1, 2);
	if (ret < 0) {
		PMsg("Read Message from port Failed\n");
		return -1;
	}

	printf("Received data: %s\n", msg);

	return 0;
}


static int OpenSerial(unsigned char *ttynam, int baudrate)
{
	int fd;
	struct termios termios_new;

	fd = open(ttynam, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (fd < 0) {
		printf("打开设备[%s]失败", ttynam);
		return -1;
	}

	tcgetattr(fd, &termios_new);
	if (baudrate == 115200) {
		cfsetispeed(&termios_new, B115200);
		cfsetospeed(&termios_new, B115200);
	} else if (baudrate == 9600) {
		cfsetispeed(&termios_new, B9600);
		cfsetospeed(&termios_new, B9600);
	} else {
		return -1;
	}
	/*set 8 bit */
	termios_new.c_cflag &= ~CSIZE;
	termios_new.c_cflag |= CS8;
	/*parity checking */
	termios_new.c_cflag &= ~PARENB;    /* Clear parity enable */
	/*1 stop bit */
	termios_new.c_cflag &= ~CSTOPB;
	/*no start bit */
	termios_new.c_cflag |= CLOCAL;
	termios_new.c_cflag |= CREAD;      //make port can read
	termios_new.c_cflag &= ~CRTSCTS;   //NO flow control
	termios_new.c_oflag = 0;
	termios_new.c_lflag  = 0;
	termios_new.c_iflag &= ~(IXON |IXOFF |IXANY);
	termios_new.c_iflag &= ~(INPCK | ISTRIP | ICRNL);
	termios_new.c_cc[VTIME] = 1;       /* unit: 1/10 second. */
	termios_new.c_cc[VMIN] = 1;        /* minimal characters for reading */

	tcflush(fd, TCIOFLUSH);

	/* 0 on success, -1 on failure */
	if (tcsetattr(fd, TCSANOW, &termios_new) == 0) {
		return fd;
	}

	return -1;
}


static int C2Num(unsigned char c)
{
	if( c >= 'A' && c <= 'F' )
		return( c-'A'+10 );
	else if( c >= 'a' && c <= 'f' )
		return( c-'a'+10 );
	else if( c >= '0' && c <= '9' )
		return( c-'0' );
	else
		return 0;
}


static unsigned char* Asc2Hex(const unsigned char *cmd, int cmdlen)
{
	int i;
	static char retcmd[MAX_ORDER_LEN*2+1];

	memset( retcmd, 0, sizeof(retcmd) );
	for (i = 0; i < cmdlen; i++) {
		sprintf(retcmd + i * 2, "%02x", cmd[i]);
	}

	return retcmd;
}

static unsigned char *Hex2Asc(const unsigned char *cmd, int cmdlen)
{
	int i;
	int j;
	static char retcmd[MAX_ORDER_LEN+1];
	unsigned char c;

	if (cmdlen%2 != 0) {
		return NULL;
	}

	for (i = 0, j = 0; j < cmdlen/2; j++) {
		c = C2Num(cmd[i])*16+C2Num(cmd[i+1]);
		retcmd[j] = c;
		i += 2;
	}

	return retcmd;

}

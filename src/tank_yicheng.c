/* 			
	tank yicheng css FCC module
	in this file we discribe how the tank is working and what 's the form of data transforming
	between FULE and FCC
*/

#include "tank_pub.h"
#include "pub.h"

#define MAX_RETURN_LEN		90
#define SINGLE_DATA_LEN	31

static int serial_fd = 0;
static int tp_num = 0;


static int open_yicheng(int p)
{	
	int fd;
	struct termios termios_new;

	fd = open(get_serial(p), O_RDWR | O_NOCTTY | O_NONBLOCK);
	if( fd < 0 ){
		__debug(1, "open serial port error! ");
		return -1;
	}
	tcgetattr(fd, &termios_new);
	cfsetispeed(&termios_new,B4800);
	cfsetospeed(&termios_new,B4800);
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
	termios_new.c_iflag &= ~(IXON |IXOFF |IXANY|INLCR|ICRNL);
	termios_new.c_iflag &= ~(INPCK | ISTRIP);
	termios_new.c_cc[VTIME] = 1;       /* unit: 1/10 second. */
	termios_new.c_cc[VMIN] = 1; /* minimal characters for reading */

	tcflush (fd, TCIOFLUSH);
	/* 0 on success, -1 on failure */
	if( tcsetattr(fd, TCSANOW, &termios_new) == 0 ){	/*success*/
		serial_fd = fd;
		return 0;
	}
	close(fd);
	return -1;

}


static unsigned char  get_crc(char *input, int size)
{
	int i;
	unsigned char crc = 0x00;
	for ( i = 0; i < size; i ++){
		crc = *(input+i)^crc;
	}
	return crc;
}

static int recv_cmd(unsigned char *retbuf , int delay)
{	
	int i, ret, size, count=0;
	struct timeval tv;
	fd_set ready;
	unsigned char recrc;

	tcflush (serial_fd, TCIOFLUSH);			//clear serial buffer
	tv.tv_sec = delay;						// timeout in seconds
	tv.tv_usec = 0;
	bzero(retbuf, sizeof(retbuf));

	//Ssleep(700000);			//this is necessary for opw and Yitong
	int len = 0;
	while(count++ < MAX_RETURN_LEN){
		FD_ZERO( &ready );
		FD_SET( serial_fd, &ready );
		if(select( serial_fd+1, &ready, NULL, NULL, &tv) > 0){
			 ret = read(serial_fd, retbuf+len, MAX_RETURN_LEN-len);
			 if(ret > 0){
			 	len += ret;
				//RunLog("Got: %s len is [%d]" ,Asc2Hex(retbuf,len), len);
				recrc = get_crc(retbuf+1, 7);
				if ((len == MAX_RETURN_LEN )&& (retbuf[0] == 0xaa) && (retbuf[9] == 0xbb) &&(recrc == retbuf[8])){
						RunLog("Got: %s" ,Asc2Hex(retbuf,len));
						return len;
				}
				else if (len >= MAX_RETURN_LEN){
					RunLog("there is something wrong with your data try again");
					RunLog("WRONG: %s" ,Asc2Hex(retbuf,len));
					return 0;
				}
				else {
					//RunLog("continue");
					continue;
				}
			}
			 else {
			 	RunLog("break");
				break;
			 }
		}
		else {
			RunLog("select");
			break;
		}
		
	}

	return -1;
}

static int yicheng_update(void) 		// just update data base
{	

	return 0;
}


static int parse_data2(unsigned char *src, unsigned char *des, size_t lenstr)
{ 	
	int i, j, k, n;
	unsigned char tmp1[64]={0};
	for(i=0;i<lenstr;i++){
		if(src[i] >= 0x30) 
			src[i] &= 0x0f;		//ascii to bcd
	}
	for(i=0, j=0;j <lenstr ;j++){
		if(src[j] != 0x2e && src[j] != 0x2d){
			tmp1[i] = src[j];
			i++;
		}
	}
	for(k=0, i=0;i < lenstr ;i=i+2){	
		des[k] = tmp1[i+1] | (tmp1[i] << 4);
		k++;
	}
	return 0;

}

static int asc_to_hex(const char *asc, char *hex, int lenasc)
{
	assert(hex!= NULL);
	int i;
	for(i = 0; i <lenasc; i++){
		if(asc[i]>= '0' && asc[i]<= '9')
			hex[i] = asc[i]-0x30;
		else if (asc[i] >= 'A' && asc[i] <='F')
			hex[i] = asc[i] -0x37;
		else if (asc[i] >= 'a' && asc[i] <='f')
			hex[i] = asc[i] -0x57;
		else{
			RunLog("THIS IS A INVALID CARACTOR");
			return -1;
		}
	}
	return i;
}
static int bcd_to_hex(const char *bcd, char *hex, int lenbcd)
{
	assert(hex!=NULL);
	int i, lenhex;
	for(i = 0, lenhex = 0; i < lenbcd; i++, lenhex+=2){
		hex[lenhex] = bcd[i]/16;
		hex[lenhex+1] = bcd[i]%16;
	}
	return lenhex;
}

static int hex_to_bcd(const char *hex, char *bcd, int lenhex)
{
	assert(bcd!=NULL);
	int i, lenbcd;
	for(i = 0, lenbcd = 0; i < lenhex; i+=2, lenbcd++){
		bcd[lenbcd] = ((hex[i]&0x0f)<<4)|(hex[i+1]&0x0f);
	}
	return lenbcd;
}

void tank_yicheng(int p)
{	
	int ret, len;
	int i;
	unsigned char command[10];
	unsigned char recvbuf[157];
	unsigned char tmp[32];
	unsigned char tank_data[32];
	time_t t;
	struct tm *tm;
	int k;
	int tpno;
	int cmdtype;
	int tankcnt;
	
	RunLog("开始运行怡诚控制台监听程序......");
	
	if(open_yicheng(p) < 0){
		RunLog("打开串口失败open_aoke(p)  failed! ");
		exit(-1);
	}
	while(1){
		Ssleep(TANK_INTERVAL);
		
		if((ret = recv_cmd(recvbuf, 4)) >= 0){

			if ( ret == 0)
				continue;
			if (gpIfsf_shm->node_online[MAX_CHANNEL_CNT] == 0) {
				RunLog(">>>>>>>> 液位仪处于联线状态<<<<<<<");
				tank_online(1);
				sleep(WAITFORHEART);
			}
			/* parse date and time */
			time(&t);
			tm = localtime(&t);
			bzero(tmp, sizeof(tmp));
			sprintf(tmp, "%04d%02d%02d", tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday);
			parse_data2(tmp, gpIfsf_shm->tlg_data.tlg_dat.Current_Date, strlen(tmp));
			//RunLog("current date is %s", Asc2Hex( gpIfsf_shm->tlg_data.tlg_dat.Current_Date, 4));
			bzero(tmp, sizeof(tmp));
			sprintf(tmp, "%02d%02d%02d", tm->tm_hour, tm->tm_min, tm->tm_sec);
			parse_data2(tmp, gpIfsf_shm->tlg_data.tlg_dat.Current_Time, strlen(tmp));
			//RunLog("current time is %s", Asc2Hex( gpIfsf_shm->tlg_data.tlg_dat.Current_Time, 3));
			/* Maybe the following codes are not good enough and I hope some one can optimize it one day
			BUT, you have to understand it completely before you plan to change it */


			/*one by one */
			tankcnt = ret/10;

			for ( i = 0 ; i < tankcnt; i ++) {
				bzero(tank_data, sizeof(tank_data));
				memcpy ( tank_data, &recvbuf[10*i], 10);
				tpno = tank_data[2] -1;
				cmdtype = tank_data[3];
				if (gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Status != 2) {
					gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Status = 2;   // operative
					if (gpIfsf_shm->node_online[MAX_CHANNEL_CNT] == 0) {
					
						break;
					}
					ChangeTpStatAlarm(gpIfsf_shm->tlg_data.tlg_node,
						0x21 + tpno, gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Status, gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Alarm);
				}	      
				switch(cmdtype){
				case 1:
					bzero(tmp, sizeof(tmp));
					
					tmp[0] = ((tank_data[4] & 0x0f) << 4) | ((tank_data[5] & 0xf0) >> 4);
					tmp[1] = ((tank_data[5] & 0x0f) << 4) | ((tank_data[6] & 0xf0) >> 4);
					tmp[2] = ((tank_data[6] & 0x0f) << 4) | ((tank_data[7] & 0xf0) >> 4);
					tmp[3] = ((tank_data[7] & 0x0f )<< 4) | 0x00;
					memcpy(gpIfsf_shm->tlg_data.tp_dat[tpno].Product_Level, tmp, 4);
					RunLog("product level is %s tpno is [%d]", Asc2Hex(gpIfsf_shm->tlg_data.tp_dat[tpno].Product_Level, 4), tpno);
					break;
				case 2:

					bzero(tmp, sizeof(tmp));
					tmp[0] = 0x09;
					tmp[3] = ((tank_data[4] & 0x0f) << 4) | ((tank_data[5] & 0xf0) >> 4);
					tmp[4] = ((tank_data[5] & 0x0f) << 4) | ((tank_data[6] & 0xf0) >> 4);
					tmp[5] = ((tank_data[6] & 0x0f) << 4) | ((tank_data[7] & 0xf0) >> 4);
					tmp[6] = ((tank_data[7] & 0x0f )<< 4) | 0x00;
					//memcpy(&tmp[3], &tank_data[4], 4);
					memcpy( gpIfsf_shm->tlg_data.tp_dat[tpno].Total_Observed_Volume, tmp, 7);
					memcpy( gpIfsf_shm->tlg_data.tp_dat[tpno].Gross_Standard_Volume, tmp, 7);
					RunLog("Total_Observed_Volume is %s tpno is [%d]", Asc2Hex(gpIfsf_shm->tlg_data.tp_dat[tpno].Total_Observed_Volume, 7), tpno);
					break;

				case 7:
					if ( (( tank_data[5] & 0xf0) >> 4) == 1 )
						gpIfsf_shm->tlg_data.tp_dat[tpno].Average_Temp[0] = 0x82;
					else if ( (( tank_data[5] & 0xf0) >> 4) == 0 )
						gpIfsf_shm->tlg_data.tp_dat[tpno].Average_Temp[0] = 0x02;
					
					memcpy( &gpIfsf_shm->tlg_data.tp_dat[tpno].Average_Temp[1], &tank_data[6], 2);
					RunLog("Average_Temp is %s tpno is [%d]", Asc2Hex(gpIfsf_shm->tlg_data.tp_dat[tpno].Average_Temp, 3), tpno);
					break;
				default:
					RunLog("there is no command !!![%d]", cmdtype);
					break;
				}
			}
			
			
		} else {
			tank_online(0);
			RunLog(">>>>>>>> 液位仪处于离线状态<<<<<<<");
			continue;
		}
		
	}
}

	

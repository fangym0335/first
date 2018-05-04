/* 				Qin Dao Ao Ke - Tank monitor system

	Support Types: PLS type fuel monitor
	This protocol can be used to read data from prober only through Aoke Power Controller
	2007.11
*/

#include "tank_pub.h"


#define MAX_RETURN_TB		32

const unsigned char table[256]={0,94,188,226,97,63,221,131,194,156,126,32,163,253,31,65,157,195,
33,127,252,162,64,30,95,1,227,189,62,96,130,220,35,125,159,193,66,28,254,160,225,191,93,3,128,
222,60,98,190,224,2,92,223,129,99,61,124,34,192,158,29,67,161,255,70,24,250,164,39,121,155,197,
132,218,56,102,229,187,89,7,219,133,103,57,186,228,6,88,25,71,165,251,120,38,196,154,101,59,
217,135,4,90,184,230,167,249,27,69,198,152,122,36,248,166,68,26,153,199,37,123,58,100,134,
216,91,5,231,185,140,210,48,110,237,179,81,15,78,16,242,172,47,113,147,205,17,79,173,243,
112,46,204,146,211,141,111,49,178,236,14,80,175,241,19,77,206,144,114,44,109,51,209,143,12,
82,176,238,50,108,142,208,83,13,239,177,240,174,76,18,145,207,45,115,202,148,118,40,171,
245,23,73,8,86,180,234,105,55,213,139,87,9,235,181,54,104,138,212,149,203,41,119,244,170,
72,22,233,183,85,11,136,214,52,106,43,117,151,201,74,20,246,168,116,42,200,150,21,75,169,247,
182,232,10,84,215,137,107,53};

unsigned char tb_addr[16];
static int serial_fd = 0;

static int open_aoke(int p)
{	int fd;
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
	termios_new.c_cc[VTIME] = 10;       /* unit: 1/10 second. */
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

static int send_cmd(unsigned char *cmd, unsigned char *retbuf, int len, int delay)
{	int i, ret, size,count=0;
	struct timeval tv;
	fd_set ready;
	unsigned char crc;

	tcflush (serial_fd, TCIOFLUSH);			//clear serial buffer
	tv.tv_sec = delay;						// timeout in seconds
	tv.tv_usec = 0;

	if(write(serial_fd,cmd,len) != len){
		__debug(2, "write command fail");
		return -1;
		}
	
	FD_ZERO(&ready);
	FD_SET(serial_fd, &ready);
	if(select(serial_fd+1, &ready, NULL, NULL, &tv) > 0){
			if((ret = read(serial_fd, retbuf, 2)) > 0){
				if(strncmp(cmd, retbuf, 2) != 0) return -1;
				memset(retbuf, 0, 2);
				len = 0;
				while(count++ < 3){
					Ssleep(TANK_INTERVAL_READ);
					if((ret = read(serial_fd, retbuf+len, MAX_RETURN_TB)) > 0){
						len +=ret;
						if(retbuf[len-2] == 0x03) break;
						}
					}
				}
			}else return -1;
	if(count == 3){
	__debug(2,"received data fial");
	 return -1;
	}
	crc = 0x00;
	for(i=0;i<len -1;i++)				//check crc
		crc = table[crc ^ retbuf[i]];
	if(crc != retbuf[len-1]) return -1;
	return 0;

}

static int detect_prober(void)
{	int i, j;
	unsigned char addr[17]={0x00,0xc0,0xc1,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,0xca,0xcb,0xcc,0xcd,0xce,0xcf};
	unsigned char buf[20];
	unsigned char cmd[2]={0xc0,0x10};
	for(i=1, j=0;i<17;i++){
		memset(buf,0,20);
		cmd[0] = addr[i];
		if(send_cmd(cmd,buf, 2, 1) < 0){
			__debug(2, "no prober on address: %X", cmd[0]);
			gpIfsf_shm->tlg_data.tp_dat[i].TP_Status = 0x02;
			continue;
			}
		tb_addr[j] = addr[i];				//save address
		gpIfsf_shm->tlg_data.tp_dat[i].TP_Status = 0x01;
		j++;
		}
	if(j < 1){
		__debug(2, "no prober was detected");
		return -1;
		}
	if(j > PROBER_NUM){
		__debug(2, "detect more prober: %d than setting, check config file",j);
		return j;
		}
	if(j < PROBER_NUM){
		__debug(2, "no all probers were detected: %d, check prober",j);
		}else{
			__debug(2, "detect prober success");
			}
	return j;

}

static int parse_data(unsigned char *src, unsigned char *des)
{ 	int i, k, lenstr;
	unsigned char tmp1[12]={0};

	lenstr = strlen(src);
	for(i=0;i<lenstr;i++){
		if(src[i] >= 0x30) 
			src[i] &= 0x0f;		//ascii to bcd
		}
	lenstr--;
	for(i=11;lenstr >= 0 && i >= 0;lenstr--){
		if(src[lenstr] != 0x2e && src[lenstr] != 0x2d){
			tmp1[i] = src[lenstr];
			i--;
			}
		}

	for(k=4, i=11;i > 0 && k >= 0;i=i-2){				// k=4 means leave des[5] = 0x00
		des[k] = tmp1[i] | (tmp1[i-1] << 4);
		k--;
		}
	return 0;

}

static int aoke_a8(unsigned char *msg, int mlen)
{
	int ret = 0;
	char tmpbuf[16];
	int lenstr;
	
	if( mlen != 7 ){
		__debug(2, "data length error");
		return -1;
		}

	memset( tmpbuf, 0, sizeof(tmpbuf) );	
	HexUnpack(msg, tmpbuf, 7);

/* set forecourt controller local system time */
	if((ret = set_systime(tmpbuf)) != 0){
		__debug(2, "set system time fail");
		return -1;
		}
	
	return 0;
}

static void set_err(unsigned char *msg, int n)
{	int i;
	i = atoi(msg+1);
	__debug(3, "error code: %d\n", i);
	switch(i){
		case 102:
		case 104: gpIfsf_shm->tlg_data.tp_dat[n].TP_Alarm[1] |= 0x01;	//prober fail
			break;
		case 202:
			break;
		case 204:
			break;
		case 205:
			break;
		case 206:
			break;
		case 302:
			break;
		default: break;
		}

}

static void set_tlg_time(void)
{	int i, j;
	unsigned char msg[16], tmp[8];
	time_t timep;
	struct tm *t;

	memset(msg, 0, 16);
	memset(tmp, 0, 12);
	
	time(&timep);
	t = localtime(&timep);
	sprintf(msg, "%4d%2d%2d%2d%2d%2d", t->tm_year+1900, t->tm_mon+1, 
		t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
	for(i=0; i<14; i++){
		if(msg[i] != 0x20) msg[i] &= 0x0f;
		else msg[i] = 0x00;
		}
	for(j=0, i=0; i<14; i=i+2){			
		tmp[j] = (msg[i] << 4) | msg[i+1];
		j++;
		}
	memcpy(gpIfsf_shm->tlg_data.tlg_dat.Current_Date, tmp, 4);
	memcpy(gpIfsf_shm->tlg_data.tlg_dat.Current_Time, tmp+4, 3);
	

}

void tank_aoke_tb(int p)
{	int i, j, k, ret, len, tb_num, count;
	unsigned char cmd[3]={0xc0,0x2b};
	unsigned char ret_buf[MAX_RETURN_TB];
	unsigned char data[6], sign[3], msg[16], tmp[12];
	unsigned char *pt;
	struct msg tb_msg;
	unsigned char tempdata[2];

	//__debug(2, "run in tank_aoke_tb");
	RunLog("开始运行奥科探棒监听程序......");

	if(open_aoke(p) < 0){
		//__debug(2, "open serial port for aoke fail");			
		RunLog("打开串口失败open_aoke(p)  failed! ");
		exit(-1);
		}
/*	if((msgid = msgget(TANK_MSG_ID, 0666)) < 0){  modify by liys 目前不做时间较准
		//__debug(2, "link message pipe fail");
		RunLog("取消息句柄失败! msgget(TANK_MSG_ID, 0666) failed! ");
		}
*/		
	
	while((tb_num = detect_prober()) < 0){
		  //__debug(2, "detect prober fail");		
		RunLog("检测探棒失败! detect_prober() failed! ");
		}

	while(1){
	//	sleep(INTERVAL);
/*		if((msglen = msgrcv(msgid, &tb_msg, 16, 0, IPC_NOWAIT)) > 1){ modify by liys 目前不做时间较准
			if(aoke_a8(msg, msglen) > 0)
				//__debug(2, "set system time success");
				RunLog("设置系统时间成功!");
			}
*/			

		for(count=0;count<tb_num;count++){
			memset(ret_buf, 0, MAX_RETURN_TB);
			memset(sign, 0, 3);
			cmd[0] = tb_addr[count];

//			RunLog(">>>>>>>>> 发送的2b 命令:  %02x%02x%02x",cmd[0],cmd[1],cmd[2]);
			if(send_cmd(cmd, ret_buf, 2, 4)<0){
				//__debug(2, "send cmd fail");				
				tank_online(0);
				RunLog(">>>>>>>> 液位仪处于离线状态<<<<<<<");
					continue;
			} else{
				if (gpIfsf_shm->node_online[MAX_CHANNEL_CNT] == 0) {						
					RunLog(">>>>>>>> 液位仪处于联线状态<<<<<<<");
					tank_online(1);
					sleep(WAITFORHEART);
					RunLog(">>>>>>>> 探棒的数量: %d",PROBER_NUM);
					for (i = 0; i < PROBER_NUM; i++) {
						ChangeTpStatAlarm(gpIfsf_shm->tlg_data.tlg_node, 0x21 + i, 2, gpIfsf_shm->tlg_data.tp_dat[i].TP_Alarm);
					   }
				}
			}
			
			
			
			//__debug(2, "get return data successful!");
			RunLog("得到液位仪回复数据%s",Asc2Hex(ret_buf,32));
			len = strlen(ret_buf);					//get sign
			for(j=0,k=0;j<len && k<3;j++){
				if(ret_buf[j] == 0x3a) k++;
				if(ret_buf[j] == 0x2d) sign[k] = 0x10;
				}
			
			pt = ret_buf + 1;
			for(j=0;j<3;j++){
				memset(tmp, 0, 12);
				for(k=0;k<6;k++){
					if(*pt == 0x3a || *pt == 0x03) break;
					tmp[k] = *pt;
					pt++;
					}
				pt++;
				if(tmp[0] == 0x45){
					set_err(tmp,i);		//it's not match IFSF
					continue;
					}
				memset(data,0,sizeof(data));
				if(parse_data(tmp, data) < 0){
					__debug(2, "parse data fail");
					continue;
					}
				//i = count+1; //modify by liys
				i = count ;
				switch(j){
					case 0:
						memcpy(gpIfsf_shm->tlg_data.tp_dat[i].Product_Level, data+2, 4);
						break;
					case 1:
						memcpy(gpIfsf_shm->tlg_data.tp_dat[i].Water_Level, data+2, 4);
						break;
					case 2:
						gpIfsf_shm->tlg_data.tp_dat[i].Average_Temp[0] = sign[j] |0x03;
						bzero(tempdata,sizeof(tempdata));
					       tempdata[0] = (data[3]) << 4  | ((data[4]) >> 4);
				              tempdata[1] = (data[4]) << 4  | ((data[5]) >> 4);
						memcpy(gpIfsf_shm->tlg_data.tp_dat[i].Average_Temp+1, tempdata, 2);						
						//memcpy(gpIfsf_shm->tlg_data.tp_dat[i].Average_Temp+1, data+3, 2);
						break;
					default: break;
					}
				}
			
			//gpIfsf_shm->tlg_data.tp_dat[i].TP_Status = 0; modify by liys
  			  gpIfsf_shm->tlg_data.tp_dat[i].TP_Status = 0x02;
			  //ChangeTpStatAlarm(gpIfsf_shm->tlg_data.tlg_node,0x21+i, 2, gpIfsf_shm->tlg_data.tp_dat[i].TP_Alarm);
                       
			}
		
		set_tlg_time();
	       print_result(); 
		}

	
}

	


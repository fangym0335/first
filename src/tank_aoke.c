/* 				Qin Dao Ao Ke - Tank monitor system

	Support Types: PD-3 protocol v1.2
	Be careful, this is the protocol that be used to communicate between pc and Aoke Controller
	2007.10
*/

#include "tank_pub.h"
#include "pub.h"

#define MAX_RETURN_LEN		(MAX_TP_NUM*31)
#define SINGLE_DATA_LEN	31

static int serial_fd = 0;
static int tp_num = 0;

static unsigned char pre_message[MAX_PAR_NUM][MAX_TANK_NUM][8];
static unsigned char pre_point_volum[MAX_TANK_NUM][160];
static pthread_mutex_t mutex =PTHREAD_MUTEX_INITIALIZER ;
static unsigned char parse_old_time[4][4];     
static int parse_old_cnt[4];

static int open_aoke(int p)
{	int fd;
	struct termios termios_new;

	fd = open(get_serial(p), O_RDWR | O_NOCTTY | O_NONBLOCK);
	if( fd < 0 ){
		__debug(1, "open serial port error! ");
		return -1;
	}
	tcgetattr(fd, &termios_new);
	cfsetispeed(&termios_new,B9600);
	cfsetospeed(&termios_new,B9600);
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


static int get_crc(char *input,int size,char *recrc)
{
	unsigned int crc; 
	unsigned int i,j;
	unsigned char CRC16Lo = 0xff;
	unsigned char  CRC16Hi = 0xff;	//CRC register
	unsigned char cl = 0x01;
	unsigned char ch = 0xA0;		//0xA001
	unsigned char saveHi, saveLo;
	for(i=0;i<size;i++)
	{
		CRC16Lo = CRC16Lo ^ (*(input+i)); //每一个数据与CRC寄存器进行异或
		for(j=0;j<8;j++)
		{
			saveHi = CRC16Hi;
			saveLo = CRC16Lo;
			CRC16Hi = CRC16Hi >> 1; //高位右移一位
			CRC16Lo = CRC16Lo >> 1; //低位右移一位
			if ((saveHi & 0x1) == 1) //如果高位字节最后一位为1
			{
				CRC16Lo = CRC16Lo | 0x80; //则低位字节右移后前面补1
			}
			if ( (saveLo & 0x1) == 1) //如果LSB为1，则与多项式码进行异或
			{
				CRC16Hi = CRC16Hi ^ ch;
				CRC16Lo = CRC16Lo ^ cl; 
			}
		}
	}
	crc=CRC16Lo+CRC16Hi*256;
	recrc[0] = CRC16Lo;
	recrc[1] = CRC16Hi;
	return crc;
}

static int send_cmd(unsigned char *cmd, unsigned char *retbuf, int len, int delay)
{	int i, ret, size, count=0;
	struct timeval tv;
	fd_set ready;
	unsigned char recrc[2];

	tcflush (serial_fd, TCIOFLUSH);			//clear serial buffer
	tv.tv_sec = delay;						// timeout in seconds
	tv.tv_usec = 0;

	//RunLog("发送的命令: %s",Asc2Hex(cmd, len));
	//for(i=0;i<len;i++) printf("%02x ",cmd[i]); printf("\n");
	if(write(serial_fd,cmd,len) != len){
		RunLog("write command fail");
		return -1;
	}
	
	Ssleep(TANK_INTERVAL_READ);			//this is necessary for opw and Yitong
	len = 0;
	while(count++ < 10){
		__debug(2, "read time: %d", count);
		memset(recrc, 0, 2);
		FD_ZERO( &ready );
		FD_SET( serial_fd, &ready );
		if(select( serial_fd+1, &ready, NULL, NULL, &tv) > 0){
			 ret = read(serial_fd, retbuf+len, MAX_RETURN_LEN);
			 if(ret > 0){
			 	len += ret;

				//modify  by liys 修改了开机器后重起的问题
				if (len - 2 < 0) {
				    RunLog("len - 2 < 0");
				    return -1;
				}
				
				get_crc(retbuf, len-2, recrc);
			 	if(strncmp(retbuf+len-2, recrc, 2) == 0){
					if (retbuf[1] != cmd[1]) {
						len = 0;
						continue;
					}
					RunLog("Got: %s" ,Asc2Hex(retbuf,len));
					//for(i=0;i<len;i++) printf("%02x ",retbuf[i]); printf("\n");
					return 0;
				}
			}
		}
	}
	for(i=0;i<len;i++) printf("%02x ",retbuf[i]); printf("\n");

	return -1;
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

	for(k=5, i=11;i > 0 && k >= 0;i=i-2){	
		des[k] = tmp1[i] | (tmp1[i-1] << 4);
		k--;
	}

	return 0;
}

static int aoke_update(void) 		// just update data base
{	int ret = 0, i, j, k;
	char tmpbuf[MAX_RETURN_LEN], crc[2], buf[9]={0x01,0x04,0x00,0x01,0x00,0x00,0xa1,0xca};
	unsigned char buf2[SINGLE_DATA_LEN], data[6], tmp[14];
	int lenstr, tpno;
	int level_changed_flag = 0;
	long level1, level2;
	float f, tov;
        static unsigned char pre_alarm[15][2] = {0};
	unsigned char alarm[15][2] = {0};
	unsigned char tempdata[2];

	//bzero(pre_alarm,sizeof(pre_alarm));
	

       /*    取时点库存
	        AA：1字节设备地址，固定为01H
		CC：1字节功能代码，04H
		SS SS：2字节命令编号，00H 01H
		XX：1字节保留，00H
		TT：1字节油罐编号，00所有油罐数据，01H~10H指定油罐数据
		CRC ：2字节CRC校验码
       */       

	memset(tmpbuf,0,sizeof(tmpbuf));   
	
	pthread_mutex_lock(&mutex);
	if(send_cmd(buf, tmpbuf, 8, 4) < 0){
		RunLog("send command msg update tank fail");
		pthread_mutex_unlock(&mutex);
		return -1;
	}
	pthread_mutex_unlock(&mutex);
	
	if (sizeof(tmpbuf) < 43 ) {
            RunLog("返回的油罐信息长度小于一个罐所需的长度，为不合法数据");
	     return -1;
	}	

	
	tp_num = tmpbuf[2];
	if(tp_num < 0 || tp_num > 31){
		RunLog("get tank prober number: %d, error",tp_num);
		return -1;
	}

	//###
//	RunLog("###get tank prober number: %d",tp_num);
	
	/* parse date and time */
	memset(buf2, 0, SINGLE_DATA_LEN);
	Hex2Bcd(tmpbuf+3, 2, buf2, 2);			//year
	Hex2Bcd(tmpbuf+5, 1, buf2+2, 1);		//month
	Hex2Bcd(tmpbuf+6, 1, buf2+3, 1);		//day
	memcpy(gpIfsf_shm->tlg_data.tlg_dat.Current_Date, buf2, 4);

	memset(buf2, 0, SINGLE_DATA_LEN);
	Hex2Bcd(tmpbuf+7, 1, buf2, 1);			//hour
	Hex2Bcd(tmpbuf+8, 1, buf2+1, 1);		//minute
	Hex2Bcd(tmpbuf+9, 1, buf2+2, 1);		//second
	memcpy(gpIfsf_shm->tlg_data.tlg_dat.Current_Time, buf2, 3);

/* Maybe the following codes are not good enough and I hope some one can optimize it one day
BUT, you have to understand it completely before you plan to change it */

	for (k = 0; k < tp_num; k++){
		/*one by one */
		tov = 0;
		memset(buf2, 0, SINGLE_DATA_LEN);
		if(tmpbuf[10] > 32){
			RunLog("get wrong prober number");
			return -1;
		}
		memcpy(buf2, tmpbuf+10 + k*31, SINGLE_DATA_LEN);	//get each prober record
		tpno = buf2[0] - 1;
//		RunLog("#### 探棒号: %d", n);
		//n = k;
		//n = k+1;	modify by liys //this is used to store record in proper location 		
		/*no need for fuel type? */
//		RunLog(">>>>>>>>>>>>buf2[2] = %02x",buf2[2]);
		
              if ((buf2[2] & 0x0f) == 0x00){//正常状态
			if (gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Status != 2) {				
				gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Status = 2;   // operative
				if (gpIfsf_shm->node_online[MAX_CHANNEL_CNT] == 0) {
					break;
				}
				ChangeTpStatAlarm(gpIfsf_shm->tlg_data.tlg_node,
					0x21 + tpno, gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Status, alarm[tpno]);
			}
              } else if (((buf2[2] & 0x0f) >= 0x01 && (buf2[2] & 0x0f) <= 0x09)) {  //设备有问题
			if (gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Status == 2) {				
				gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Status = 1;   // Inoperative
				ChangeTpStatAlarm(gpIfsf_shm->tlg_data.tlg_node,
						0x21 + tpno, gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Status, alarm[tpno]);
			}
			switch (buf2[2] & 0x0f){
				case 0x01: RunLog("探棒%d不可用: [%02x] 连接失败", tpno + 1,buf2[2] & 0x0f);  break;
				case 0x02: RunLog("探棒%d不可用: [%02x] 数据错误", tpno + 1,buf2[2] & 0x0f); break;
				case 0x03: RunLog("探棒%d不可用: [%02x] 校验错误", tpno + 1,buf2[2] & 0x0f);  break;
				case 0x04: RunLog("探棒%d不可用: [%02x] 无液面浮子", tpno + 1,buf2[2] & 0x0f);  break;
				case 0x05: RunLog("探棒%d不可用: [%02x] 油浮子错误", tpno + 1,buf2[2] & 0x0f);  break;
				case 0x06: RunLog("探棒%d不可用: [%02x] 无界面浮子", tpno + 1,buf2[2] & 0x0f);  break;
				case 0x07: RunLog("探棒%d不可用: [%02x] 水浮子错误", tpno + 1,buf2[2] & 0x0f);  break;
				case 0x08: RunLog("探棒%d不可用: [%02x] 温度故障", tpno + 1,buf2[2] & 0x0f);  break;
				case 0x09: RunLog("探棒%d不可用: [%02x] 无法计算温度", tpno + 1,buf2[2] & 0x0f);  break;
				
			}
			
		} else {		       
			gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Status = 2;   // Operative
			switch (buf2[2] & 0x0f){
				case 0x0a: alarm[tpno][1] |= 0x10; break;	//high-high
				case 0x0b: alarm[tpno][1] |= 0x20; break;	//high alarm
				case 0x0c: alarm[tpno][1] |= 0x40; break;	//low-low
				case 0x0d: alarm[tpno][1] |= 0x80; break;	//low alarm  
				case 0x0e: alarm[tpno][0] |= 0x01; break;	//high water alarm
				case 0x0f: alarm[tpno][0] |= 0x02; break;	//tank loss
				
				default: 
				RunLog("get error code: %x", buf2[2]);
			}

		}

		if (memcmp(pre_alarm[tpno], alarm[tpno], 2) != 0) {
			ChangeTpStatAlarm(gpIfsf_shm->tlg_data.tlg_node,
				0x21 + tpno, gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Status, alarm[tpno]);
			
			memcpy(pre_alarm[tpno], alarm[tpno], 2);
			bzero(alarm[tpno], 2);
		}
			


		for(i = 0; i < 7; i++){
			memset(tmp,0,14);
			f = exp2float(buf2+3 + i*4);	//should check valid or not?
			sprintf(tmp, "%.3f", f);			//save data as ascii 0x2d = "-" 0x2e="."
			lenstr = strlen(tmp);
			memset(data, 0, 6);
			switch(i){
				case 0: 
					//当前温度油净体积
//					printf("######### G.S.V: %0.3f\n", f);
					bzero(data, 6);
					gpIfsf_shm->tlg_data.tp_dat[tpno].Total_Observed_Volume[0] = 0x09;
					if(parse_data(tmp,data) < 0){
						RunLog("parse data fail: %d", i);
						break;
					}
					//RunLog("data is %s", Asc2Hex(data, 6));
					memcpy(gpIfsf_shm->tlg_data.tp_dat[tpno].Total_Observed_Volume+1, data, 6);
					
					break;
				case 1: 
					//20度时的标准体积，是经过换算的体积，不能用来算总体积
					//RunLog("######### Volume: %0.3f\n", tov);
					gpIfsf_shm->tlg_data.tp_dat[tpno].Gross_Standard_Volume[0] = 0x09;
					if(parse_data(tmp,data) < 0){
						RunLog("parse data fail: %d", i);
						break;
					}
					memcpy(gpIfsf_shm->tlg_data.tp_dat[tpno].Gross_Standard_Volume + 1, data, 6);
					//RunLog("data %s", Asc2Hex(data, 6));
					tov += f;
					//RunLog("######### Volume: %0.3f\n", tov);
					
					break;

				case 2: 
					break;		//no need empty volume
				case 3:
					if(parse_data(tmp,data) < 0){
						RunLog("parse data fail: %d", i);
						break;
					}
					memcpy(gpIfsf_shm->tlg_data.tp_dat[tpno].Product_Level, data + 2, 4);

					// CSSI.校罐项目数据采集 -- 记录液位变化数据
					if (gpGunShm->data_acquisition_4_TCC_adjust_flag != 0) {
						level1 = Bcd2Long(&gpGunShm->product_level[tpno][0], 4);
						level2 = Bcd2Long(gpIfsf_shm->tlg_data.tp_dat[tpno].Product_Level, 4);
						TCCADLog("level_changed_flag: %d, level1:%ld, level2: %ld",
									level_changed_flag, level1, level2);
						level2 -= level1;
						unsigned char tmp_level_mm = gpIfsf_shm->tlg_data.tp_dat[tpno].Product_Level[2] >> 4;
						if (tmp_level_mm == 0 || tmp_level_mm == 9) {
							if (level2 >= PRODUCT_LEVEL_INTERVAL2 ||
								level2 <= (-1 * PRODUCT_LEVEL_INTERVAL2)) {
								level_changed_flag = 1;
							}
						} else if (level2 >= PRODUCT_LEVEL_INTERVAL ||
								level2 <= (-1 * PRODUCT_LEVEL_INTERVAL)) {
							level_changed_flag = 1;
						}
						TCCADLog("level_changed_flag: %d, level2: %ld",
									level_changed_flag, level2);
					}
					break;
				case 4: 
					if(parse_data(tmp,data) < 0){
						RunLog("parse data fail: %d", i);
						break;
					}
					memcpy(gpIfsf_shm->tlg_data.tp_dat[tpno].Water_Level, data + 2, 4);
					break;
				case 5: 
					if(tmp[0] == 0x2d){
						gpIfsf_shm->tlg_data.tp_dat[tpno].Average_Temp[0] = 0x82;
					} else {
						gpIfsf_shm->tlg_data.tp_dat[tpno].Average_Temp[0] = 0x02;
					}
					
//					gpIfsf_shm->tlg_data.tp_dat[n].Average_Temp[0] = 0x03;
					if(parse_data(tmp,data) < 0){
						RunLog("parse data fail: %d", i);
						break;
					}
					bzero(tempdata,sizeof(tempdata));
					tempdata[0] = (data[3]) << 4  | ((data[4]) >> 4);
					tempdata[1] = (data[4]) << 4  | ((data[5]) >> 4);
					memcpy(gpIfsf_shm->tlg_data.tp_dat[tpno].Average_Temp+1, tempdata, 2);   
					//memcpy(gpIfsf_shm->tlg_data.tp_dat[n].Average_Temp+1, data+3, 2);
					break;
				case 6: 		// water volume , water vol + volume = TOV
#if 0
					tov += f;
					//RunLog("######### Water Vol: %0.3f\n", f);
					//RunLog("######### T.O.V: %0.3f\n", tov);
					bzero(tmp, 14);
					sprintf(tmp, "%.3f", tov);
					lenstr = strlen(tmp);
					bzero(data, 6);
					gpIfsf_shm->tlg_data.tp_dat[tpno].Total_Observed_Volume[0] = 0x09;
					if(parse_data(tmp,data) < 0){
						RunLog("parse data fail: %d", i);
						break;
					}
					//RunLog("data is %s", Asc2Hex(data, 6));
					memcpy(gpIfsf_shm->tlg_data.tp_dat[tpno].Total_Observed_Volume+1, data, 6);
					break;
#endif
				default: break;

				}
			}	

			// CSSI.校罐项目数据采集
			if (gpGunShm->data_acquisition_4_TCC_adjust_flag == 1) {
				// 采集服务刚生效或者程序重启时写入一条零液位记录
				write_tcc_adjust_data(NULL, "%d,0,,,,,,0,0,0,0", tpno + 1);
			}
			if (level_changed_flag != 0 || gpGunShm->tr_fin_flag_tmp != 0) {
				write_tcc_adjust_data(NULL, "%d,%02x%02x%01x.%01x%02x,,,,,,0,%02x.%02x,0,0",
							tpno + 1, gpIfsf_shm->tlg_data.tp_dat[tpno].Product_Level[0],
							gpIfsf_shm->tlg_data.tp_dat[tpno].Product_Level[1],
							(gpIfsf_shm->tlg_data.tp_dat[tpno].Product_Level[2] >> 4),
							(gpIfsf_shm->tlg_data.tp_dat[tpno].Product_Level[2] & 0x0F),
							gpIfsf_shm->tlg_data.tp_dat[tpno].Product_Level[3],
							gpIfsf_shm->tlg_data.tp_dat[tpno].Average_Temp[1],
							gpIfsf_shm->tlg_data.tp_dat[tpno].Average_Temp[2]);

				TCCADLog("TCC-AD: %d #罐 写液位记录, %02x%02x%01x.%01x%02x (orig:%02x%02x%02x%02x)",
							tpno + 1, gpIfsf_shm->tlg_data.tp_dat[tpno].Product_Level[0],
							gpIfsf_shm->tlg_data.tp_dat[tpno].Product_Level[1],
							(gpIfsf_shm->tlg_data.tp_dat[tpno].Product_Level[2] >> 4),
							(gpIfsf_shm->tlg_data.tp_dat[tpno].Product_Level[2] & 0x0F),
							gpIfsf_shm->tlg_data.tp_dat[tpno].Product_Level[3],
							gpIfsf_shm->tlg_data.tp_dat[tpno].Product_Level[0],
							gpIfsf_shm->tlg_data.tp_dat[tpno].Product_Level[1],
							gpIfsf_shm->tlg_data.tp_dat[tpno].Product_Level[2],
							gpIfsf_shm->tlg_data.tp_dat[tpno].Product_Level[3]);
				memcpy(&gpGunShm->product_level[tpno][0], gpIfsf_shm->tlg_data.tp_dat[tpno].Product_Level, 4);
				level_changed_flag = 0;
			}

		}

	return 0;
}

static int aoke_a0(unsigned char *msg, int mlen)
{

	return 0;

}

static int aoke_a8(unsigned char *msg, int mlen)
{
	int ret = 0;
	char tmpbuf[64], crc[2], buf[16]={0x01,0x10,0x00,0x01,0x00,0x07};
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
	}
	
/* set tank system time */
	tmpbuf[12]=0;	
	strncpy(buf+6, tmpbuf, 7);
	get_crc(buf,13,crc);
	strncpy(buf+13,crc,2);
	
	memset( tmpbuf, 0, sizeof(tmpbuf) );

	pthread_mutex_lock(&mutex);
	if(send_cmd(buf, tmpbuf, 15, 4) < 0){
		__debug(2, "send command msg A8 fail");
		pthread_mutex_unlock(&mutex);
		return -1;
	}
	pthread_mutex_unlock(&mutex);

	if(tmpbuf[5] == 0x07) return 0;		//make things simple
/*	lenstr = strlen(tmpbuf)-2;
	get_crc(tmpbuf,lenstr,crc);
	if(strncmp(tmpbuf+lenstr,crc,2) == 0){
		__debug(2, "set tank time success");
		return 0;
		}
	*/
	return -1;
}


static int parse_data2(unsigned char *src, unsigned char *des, size_t lenstr)
{ 	int i, j, k, n;
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
	for(k=0, i=0;i < 8 ;i=i+2){	
		des[k] = tmp1[i+1] | (tmp1[i] << 4);
		k++;
		}
	return 0;

}
int asc_to_hex(const char *asc, char *hex, int lenasc)
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
int bcd_to_hex(const char *bcd, char *hex, int lenbcd)
{
	assert(hex!=NULL);
	int i, lenhex;
	for(i = 0, lenhex = 0; i < lenbcd; i++, lenhex+=2){
		hex[lenhex] = bcd[i]/16;
		hex[lenhex+1] = bcd[i]%16;
	}
	return lenhex;
}

int hex_to_bcd(const char *hex, char *bcd, int lenhex)
{
	assert(bcd!=NULL);
	int i, lenbcd;
	for(i = 0, lenbcd = 0; i < lenhex; i+=2, lenbcd++){
		bcd[lenbcd] = ((hex[i]&0x0f)<<4)|(hex[i+1]&0x0f);
	}
	return lenbcd;
}

int make_data_bcd(unsigned char *tank_command, unsigned char *data)
{
	
	float f;
	int n = 0;
	int i;
	int point_stat = 0;	
	unsigned char sign;
	unsigned char tmp_buffer[64];

	bzero(tmp_buffer, sizeof(tmp_buffer));
	//bcd_to_hex(tank_command, tmp_buffer, 4);
	//asc2hex(tank_command, tmp_buffer);
	f = exp2float(tank_command);	
	
//	RunLog("f is %f\n", f);
	if ( f < 0) {
		f = f*(-1);
		sign = 0x01;
	}
	else 
		sign = 0x00;

	bzero (tmp_buffer, sizeof(tmp_buffer));
	bzero (data, sizeof(data));
	sprintf(tmp_buffer, "%f", f);
	for(i = 0; i< strlen(tmp_buffer); i++)
		if(tmp_buffer[i] == 0x2e)
			point_stat = i;
	data[0] = (unsigned char)sign << 4 |(unsigned char)point_stat;
	parse_data2(tmp_buffer, &data[1], strlen(tmp_buffer));
	
}
static int timeparse(unsigned char *time, unsigned char *t5)
{
	time_t tl;
	int t;
	struct tm tm;
	tm.tm_year = time[1] - 0x6c;
	RunLog("year is %d", tm.tm_year);
	tm.tm_mon = time[2]-1;
	RunLog("month is \n", tm.tm_mon);
	tm.tm_mday = time[3];
	RunLog("day %d", tm.tm_mday);
	tm.tm_hour = time[4];
	 RunLog("day %d", tm.tm_hour);
	tm.tm_min = time[5];
 	RunLog("day %d",tm.tm_min);
	tm.tm_sec= time[6];
	 RunLog("day %d",tm.tm_sec);
	tl =mktime(&tm);
	unsigned char ttt[11];
	memset(ttt, 0, 11);
	RunLog("tl is %d\n", tl);
	sprintf(ttt, "%d", tl);
	RunLog("ttt is %s\n", ttt);
	t5[0] =( (ttt[0]-0x30))<<4|(ttt[1]-0x30);
	t5[1] =( (ttt[2]-0x30))<<4|(ttt[3]-0x30); 
	t5[2] =( (ttt[4]-0x30))<<4|(ttt[5]-0x30); 
	t5[3] =( (ttt[6]-0x30))<<4|(ttt[7]-0x30); 
	t5[4] =( (ttt[8]-0x30))<<4|(ttt[9]-0x30); 	
	RunLog("t5 %02x%02x%02x%02x%02x\n", t5[0],t5[1],t5[2],t5[3],t5[4]);
	return tl;
}


static int parse_command(unsigned char *message, unsigned char *message2, int len, unsigned char *buffReturn, int par_flag)
{
	int data_cnt = 0;
	int m, ret, i;
	unsigned char tank_command[8];
	unsigned char data[5];
	int tank_cnt = buffReturn[2];
	
	//RunLog("tank_cnt is %d par_flag is %d \n\n", tank_cnt, par_flag);
	unsigned char tank_nu;
	unsigned char tmessage[MAX_PAR_NUM][MAX_TANK_NUM];
	bzero(tmessage, sizeof(tmessage));
	int n;
	int par_nu;
	unsigned char parse_new_time[4];
	int parse_new_cnt;

	parse_new_cnt = buffReturn[2];
	if (par_flag == 1){
		n = 3;
		memcpy(parse_new_time , &buffReturn[4], 4);
	}
	else if (par_flag == 2){
		n = 2;
		memcpy(parse_new_time , &buffReturn[11], 4);
	}
	else if (par_flag == 3){
		n = 1;
		memcpy(parse_new_time , &buffReturn[4], 4);
	}
	else{
		RunLog("par_flag is wrong ");
		return -1;
	}
	RunLog("newtime is %02x%02x%02x%02x oldtime is %02x oldcnt is %d newcnt is %d", \
		parse_new_time[0],parse_new_time[1],parse_new_time[2],parse_new_time[3],\
		parse_old_time[par_flag-1][3],\
		parse_old_cnt[par_flag-1], parse_new_cnt);
	if (! ((memcmp(parse_new_time, parse_old_time[par_flag-1], 4) == 0)&& (parse_new_cnt == parse_old_cnt[par_flag-1]))){

		if (memcmp(parse_new_time, parse_old_time[par_flag-1], 4) != 0){
			memcpy(parse_old_time[par_flag-1], parse_new_time, 4);
			parse_old_cnt[par_flag-1] = 0;
		}

		//RunLog("n is %d", n);
		for (i=0; i < n; i ++) {
		
			ret = 0;
			if (par_flag== 1){
				if(i == 0)
					par_nu = 4;
				else if (i == 1)
					par_nu = 10;
				else if (i == 2)
					par_nu = 2;
			}else if (par_flag == 2){
				if(i == 0)
					par_nu = 6;
				else if (i == 1)
					par_nu = 7;
			}else if(par_flag == 3)
				par_nu = 11;
		
			data_cnt =0;
			message[len] = inttobcd(par_nu);
			message2[len] = inttobcd(par_nu);
			RunLog("oldcnt is %d newcnt is %d", parse_old_cnt[par_flag-1], parse_new_cnt);
			for(m =parse_old_cnt[par_flag-1]; m < parse_new_cnt; m++){
				if(par_flag == 2){
					memcpy(tank_command, buffReturn + 3+ 31*m + 8+ 4*n + 7 + i*4, 4);
					memcpy(&tank_command[4], buffReturn + 3+ 31*m + 8 + 7 + i*4, 4);
					tank_nu = *(buffReturn + 3+ 31*m);
				}
				else if (par_flag == 1){
					memcpy(tank_command, buffReturn + 3+ 32*m + 8+ 4*n + i*4, 4);
					memcpy(&tank_command[4], buffReturn + 3+ 32*m + 8 + i*4, 4);
					tank_nu = *(buffReturn + 3+ 32*m);
				}else if(par_flag == 3){
					memcpy(tank_command, buffReturn + 3+ 8*m+1 , 7);
					tank_nu = *(buffReturn + 3+ 8*m);
					if (!ret ){
						ret = 1;
						par_cnt++;
					}
					if (tmessage[par_nu][tank_nu] != 0){
					//	RunLog("tmessage [%d][%d] is %d", par_nu, tank_nu,tmessage[par_nu][tank_nu]  );
						message[len+2 +data_cnt * 6] = tank_nu;
						message2[len+2 +data_cnt * 6] = tank_nu;
						timeparse(tank_command, data);
						memcpy(&message[len+3 +(tmessage[par_nu][tank_nu] -1)* 6], data, 5);
						memcpy(&message2[len+3 +(tmessage[par_nu][tank_nu] -1)* 6], data, 5);
					}
					else {	
						message[len+2 +data_cnt * 6] = tank_nu;
						message2[len+2 +data_cnt * 6] = tank_nu;
						tmessage[par_nu][tank_nu]=data_cnt +1;
						timeparse(tank_command, data);
					//	RunLog(" write tmessage [%d][%d] is %d",par_nu, tank_nu,tmessage[par_nu][tank_nu]  );
						memcpy(&message[len+3 +data_cnt * 6], data, 5);
						memcpy(&message2[len+3 +data_cnt * 6], data, 5);
						data_cnt++;	
					}
				
					continue;
				}
			
				//RunLog("tank_nu is %d", tank_nu);
				if (!ret ){
					ret = 1;
					par_cnt++;
				}
				if (tmessage[par_nu][tank_nu] != 0){
				//	RunLog("tmessage [%d][%d] is %d", par_nu, tank_nu,tmessage[par_nu][tank_nu]  );
					message[len+2 +data_cnt * 6] = tank_nu;
					message2[len+2 +data_cnt * 6] = tank_nu;
					make_data_bcd(tank_command, data);
					memcpy(&message[len+3 +(tmessage[par_nu][tank_nu] -1)* 6], data, 5);
				
					make_data_bcd(&tank_command[4], data);
					memcpy(&message2[len+3 +(tmessage[par_nu][tank_nu] -1)* 6], data, 5);
				}
				else {	
					message[len+2 +data_cnt * 6] = tank_nu;
					message2[len+2 +data_cnt * 6] = tank_nu;
					make_data_bcd(tank_command, data);
					tmessage[par_nu][tank_nu]=data_cnt +1;
				//	RunLog(" write tmessage [%d][%d] is %d",par_nu, tank_nu,tmessage[par_nu][tank_nu]  );
					memcpy(&message[len+3 +data_cnt * 6], data, 5);
				
					make_data_bcd(&tank_command[4], data);
					memcpy(&message2[len+3 +data_cnt * 6], data, 5);
					data_cnt++;	
				}
			
			
			}
			message[len +1] = inttobcd(data_cnt);
			message2[len +1] = inttobcd(data_cnt);
			if(data_cnt != 0)
				len = len+3 +data_cnt*6 -1;
		}
		parse_old_cnt[par_flag-1] = parse_new_cnt;
	}
	return len;
}

/**
*tank configure send to center 
*tank configure parameter include tilt ,expanision coefficient, temperature, diameter, 20 point
*author: duan 2012-11-20
*/
//#if 0
static int read_config()
{	
	int ret;
	ret = pthread_detach(pthread_self());
	if (ret ==0)
		RunLog("detach success");
	else{
		RunLog("something is wrong in the process detaching");
		pthread_exit(NULL);
	}
	
	unsigned char time_interval = 3;
	int sockfd = 0;
	unsigned char buffReturn[2048];
	unsigned char message[2048];
	unsigned char message2[2048];
	unsigned char tank_command[20] ;
	unsigned char data[5];
	unsigned char recrc[2];
	int len, i;
	int par_nu;
	//int tank_cnt;
	int timeout = 10;
	int send_time, data_cnt;
	int m;
	char *p;
	char *portnb;
	int cilent_port = 9004;
	time_t t;
	struct tm *tm;

	unsigned char tmp_buffer[512];

	sleep(20);
/*	bzero(pre_message, sizeof(pre_message));
	bzero(parse_old_time, sizeof(parse_old_time));
	bzero(parse_old_cnt, sizeof(parse_old_cnt));*/
	
	RunLog("开始read_config程序");
	int fd = open("/home/App/ifsf/etc/message.cfg", O_RDWR|O_CREAT, 00666);
	if (fd == -1)
		RunLog("创建或打开message.cfg失败");
	else{
		ret = read(fd, parse_old_time[0], 4);
		if (ret != 0){
			read(fd, parse_old_time[1], 4);
			read(fd, parse_old_time[2], 4);
			read(fd, parse_old_time[3], 4);
			read(fd, &parse_old_cnt[0], 4);
			read(fd, &parse_old_cnt[1], 4);
			read(fd, &parse_old_cnt[2], 4);
			read(fd, &parse_old_cnt[3], 4);		
		}
		close(fd);
	}

	for(i = 0; i < 4; i ++)
		RunLog("oldtime is %02x%02x%02x%02x oldcnt is %d", parse_old_time[i][0], parse_old_time[i][1],parse_old_time[i][2],parse_old_time[i][3], parse_old_cnt[i]);
	i =0;

	while(1){
		while(1) {
			portnb = "9002";	
			sockfd = Tank_TcpConnect(ipname, "9002", cilent_port ,timeout);//sPortName,(timeout+1)/2
			RunLog( "连接数据端口[%d],sock[%d]",atoi(portnb), sockfd);
			if (sockfd > 0 ){
				RunLog("TcpConnect连接成功,TANK准备发送数据");			
				break;
			}else{
				RunLog("TcpConnect连接失败,延时10s 继续");
				sleep(10);//exit(0);					
				continue;
			}		
		}
	
		bzero(message, sizeof(message));
		send_time = 0;
		message[0] = 0xF5;
		message[4] = 0x06;
		memcpy (&message[5], stano_bcd, 5);
		unsigned char tlg_drv = (unsigned char)atoi(TLG_DRV);
		message[10] =  inttobcd(tlg_drv);
		len = 11;
		/*tcp send*/
		ret = TcpSendDataInTime(sockfd, message, len, timeout);
		while ( ret < 0 && send_time < 3){
			send_time++;
			sleep (10);
			ret = TcpSendDataInTime(sockfd, message, len, timeout);
		}

		if (send_time ==3) {
			RunLog("三次发送失败，退出重连，采集信息丢弃");		
		}else{
			RunLog("发送数据成功，等待接收") ;
			int recvLength = 2;
			ret = TcpRecvDataInTime(sockfd, buffReturn, &recvLength, timeout);
			if (ret < 0) {
				RunLog("发送罐配置信息成功，没有收到返回");
			}else {
				if (0x00 == buffReturn[1]){
					RunLog("发送罐配置信息成功，返回不正常");
					sleep(10);
					if (sockfd > 0)
						close (sockfd);
					continue;
				}else if (buffReturn[1] ) {
					time_interval = buffReturn[1]&0x0f + (buffReturn[1]&0xf0)*10;
					RunLog("发送罐配置信息成功，返回正常时间间隔为%d ", time_interval);
					break;
				}
			}
		}
		if (sockfd > 0)
			close (sockfd);
	}

	if (sockfd > 0)
		close (sockfd);
	
	while(1)
	{
		if (sockfd > 0)
			close (sockfd);
		RunLog("node online is %d", gpIfsf_shm->node_online[MAX_CHANNEL_CNT]);
		if (gpIfsf_shm->node_online[MAX_CHANNEL_CNT] == 1){
		//static int ddd;
		sleep(2);
		bzero(buffReturn, sizeof(buffReturn));
		bzero(message, sizeof(message));
		bzero(message2, sizeof(message2));
		
		message[0] = 0xFC;
		message2[0] = 0xFC;
		

		memcpy (&message[5], stano_bcd, 5);
		memcpy (&message2[5], stano_bcd, 5);
		
		par_cnt = 0;
		len = 11;
		


		unsigned char get_time[7]= {0x01, 0x10, 0x00, 0x01, 0x00, 0x07, 0x07};
		memcpy(tank_command , get_time, 7);
		time(&t);
		tm = localtime(&t);
		tank_command[7] = tm->tm_year + 0x6c;
		tank_command[8] = tm->tm_mon +1;
		tank_command[9] = tm->tm_mday;
		tank_command[10] = tm->tm_hour;
		tank_command[11] = tm->tm_min;
		tank_command[12] = tm->tm_sec;
		get_crc(tank_command, 13, recrc);
		strncpy(tank_command+ 13, recrc, 2);
		RunLog("synchronize tank time %s", Asc2Hex(tank_command, 15));
		/*most diameter &&most volumn &&  expansion coefficient*/
		pthread_mutex_lock(&mutex);
		ret =send_cmd(tank_command, buffReturn, 15, 1);
		pthread_mutex_unlock(&mutex);
		RunLog("time return  %s", Asc2Hex(buffReturn, 8));
		
		unsigned char get_met[6]= {0x01, 0x05, 0x00, 0x05, 0x00, 0x00};
		memcpy(tank_command , get_met, 6);
		get_crc(tank_command, 6, recrc);
		strncpy(tank_command+ 6, recrc, 2);
		RunLog("tank command %s", Asc2Hex(tank_command, 8));
		/*most diameter &&most volumn &&  expansion coefficient*/
		pthread_mutex_lock(&mutex);
		ret =send_cmd(tank_command, buffReturn, 8, 1);
		pthread_mutex_unlock(&mutex);
		/*buffreturn :AACCddttYYYYMMDDHHmmssFFFFFFFF*6
	                                                      	   		FFFFFFFF*6&&*/


		if (ret < 0)
			RunLog("send diameter command failed ");
		else if(ret == 0){
			par_nu = 1;
			RunLog("most diameter && volumn && expansion coefficient success");
			if (buffReturn[2] != 0x00)
				len = parse_command(message, message2,len, buffReturn, par_nu);
		}
		//RunLog("message is %s", Asc2Hex(message, len));	
		//RunLog("message2 is %s", Asc2Hex(message2, len));	


		unsigned char get_dvc[6]= {0x01, 0x05, 0x00, 0x01, 0x00, 0x00};
		memcpy(tank_command , get_dvc, 6);
		get_crc(tank_command, 6, recrc);
		strncpy(tank_command+ 6, recrc, 2);
		RunLog("tank command %s", Asc2Hex(tank_command, 8));
		/*water level && oil level*/
		pthread_mutex_lock(&mutex);
		ret =send_cmd(tank_command, buffReturn, 8, 1);
		pthread_mutex_unlock(&mutex);
		
		/*buffreturn :AACCddttYYYYMMDDHHmmssYYYYMMDDHHmmssFFFFFFFF*4
	                                                      	   		FFFFFFFF*4&&*/

		if (ret < 0)
			RunLog("send water level and oil level failed ");
		else if (ret == 0){
			RunLog("water level && oil level success");
			par_nu = 2;
			if (buffReturn[2] !=0x00)
				len = parse_command(message,message2,  len, buffReturn, par_nu);
		}

		unsigned char get_tl[6]= {0x01, 0x05, 0x00, 0x02, 0x00, 0x00};
		memcpy(tank_command , get_tl, 6);
		get_crc(tank_command, 6,  recrc);
		strncpy(tank_command+ 6, recrc, 2);
		RunLog("tank command %s", Asc2Hex(tank_command, 8));
		/*table level && table volume*/
		
		pthread_mutex_lock(&mutex);
		ret =send_cmd(tank_command, buffReturn, 8, 1);
		pthread_mutex_unlock(&mutex);
		/*buffreturn :AACCddttYYYYMMDDHHmmssYYYYMMDDHHmmssFFFFFFFF*4
	                                                      	   		FFFFFFFF*4&&*/



		if (ret < 0)
			RunLog("send table level and table volume failed ");
		else if (ret ==0 &&(buffReturn[2] != 0x00)){
			RunLog("send table level && table volume success");
			par_nu = 12;
			int tank_new_cnt = buffReturn[2];
			unsigned char new_time[4] ;
			int tank_cnt;
			int tank_nu;
 			int first = 0;
			int ls;
			int t;
			unsigned char tank_record[MAX_TANK_NUM];
			unsigned char messag_tank[MAX_TANK_NUM][256];
			unsigned char tankdata[4];
			bzero(tank_record, sizeof(tank_record));
			bzero(messag_tank, sizeof(messag_tank));
			RunLog("tank old is %d tank new is %d", parse_old_cnt[3], tank_new_cnt);

			memcpy(new_time , &buffReturn[11], 4);
			if (! ((memcmp(new_time, parse_old_time[3], 4) == 0)&& (parse_old_cnt[3] == tank_new_cnt))){

				if (memcmp(new_time, parse_old_time[3], 4) != 0){
					memcpy(parse_old_time[3], new_time, 4);
					parse_old_cnt[3] = 0;
				}
				message[len] = inttobcd(par_nu);
				message2[len] = inttobcd(par_nu);
				data_cnt = 0;
				
				for (i = parse_old_cnt[3]; i < tank_new_cnt; i++){
					memcpy(tank_command, buffReturn + 3+ 31*i + 8+ 7 , 16);
					tank_nu = *(buffReturn + 3+ 31*i);
					//RunLog("tank_nu is %d", tank_nu);
					for (t =0; t < MAX_TANK_NUM; t ++){
						if (t == tank_nu -1){
							RunLog("tank_recode is %d t is %d", tank_record[t], t);
							tank_record[t]++;
							
							break;
						}
					}
					//RunLog("first is %d", first);
					if (!first ){
						first = 1;
						par_cnt++;
					}
					for (ls = 0; ls < 4; ls ++){
						memcpy (tankdata, &tank_command[ls*4], 4);
					//	RunLog("tankdata is %s", Asc2Hex(tankdata, 4));
						make_data_bcd(tankdata, data);
					//	RunLog("len is %d, i is %d tank_old_cnt is %d ls is %d data is %s", len, i, parse_old_cnt[3], ls, Asc2Hex(data, 5));
						memcpy(&messag_tank[tank_nu -1][(tank_record[tank_nu -1]-1)*20+ls*5+2], data, 5);
						
					}
					RunLog("message_tank is %s", Asc2Hex(messag_tank[tank_nu -1], tank_record[tank_nu -1]*20+2));
				}
				int len2 = len;
				len += 2;
				for (t =0; t < MAX_TANK_NUM; t ++){
					if (tank_record[t]){
						messag_tank[t][0] = 00;
						messag_tank[t][1] = tank_record[t];
						message[len] =inttobcd(t+1);
						message2[len] =inttobcd(t+1);
						
						memcpy(&message[len+1] ,messag_tank[t],  tank_record[t]*20+2);
						memcpy(&message2[len+1] ,messag_tank[t],  tank_record[t]*20+2);
						len = len +1+tank_record[t]*20+2;
						data_cnt++;
					}
				}
				
				message[len2 +1] = inttobcd(data_cnt);
				message2[len2 +1] = inttobcd(data_cnt);
				parse_old_cnt[3] = tank_new_cnt;
			}
			//len = parse_command(message, len, buffReturn, par_nu);
		}

		unsigned char get_point[6]= {0x01, 0x05, 0x00, 0x03, 0x00, 0x00};
		memcpy(tank_command , get_point, 6);
		get_crc(tank_command, 6,  recrc);
		strncpy(tank_command+ 6, recrc, 2);
		RunLog("tank command %s", Asc2Hex(tank_command, 8));
		/*table level && table volume*/
		//while(!can_send)
		pthread_mutex_lock(&mutex);
		ret =send_cmd(tank_command, buffReturn, 8, 1);
		pthread_mutex_unlock(&mutex);
		//ret = send_cmd(tank_command, buffReturn, 8, 1);
	      
		if (ret < 0)
			RunLog("send table level and table volume failed ");
		else if (ret ==0){
			RunLog("send table level && table volume success");
			par_nu = 3;
			if(buffReturn[2] != 0x00)
				len = parse_command(message,message2,  len, buffReturn, par_nu);
		}

		message[10] = inttobcd(par_cnt);
		message2[10] = inttobcd(par_cnt);
		
		int dd= len -5;
		char *d= (char *)&dd;
		message[1] = *(d+3);
		message[2] = *(d+2);
		message[3] = *(d+1);
		message[4] = *d;
		message2[1] = *(d+3);
		message2[2] = *(d+2);
		message2[3] = *(d+1);
		message2[4] = *d;
		/*  printf the message*/
		//RunLog("le n is %d", len);
		


		if(par_cnt != 0){

			RunLog("TANK pre CONFIG MESSAGE %s", Asc2Hex(message2, len));
			RunLog("TANK CONFIG MESSAGE %s", Asc2Hex(message, len));

			fd = open("/home/App/ifsf/etc/message.cfg", O_RDWR);
			if (fd == -1)
				RunLog("创建或打开message.cfg失败");
			else{
				write(fd, parse_old_time[0], 4);
				write(fd, parse_old_time[1], 4);
				write(fd, parse_old_time[2], 4);
				write(fd, parse_old_time[3], 4);
				write(fd, &parse_old_cnt[0], 4);
				write(fd, &parse_old_cnt[1], 4);
				write(fd, &parse_old_cnt[2], 4);
				write(fd, &parse_old_cnt[3], 4);	
				close(fd);
			}
					
					
			/*connect to server 9002 port*/

			send_time = 0;
			while(1) {
				portnb = "9002";
			
				//RunLog("ipname is %s", ipname);
				sockfd = Tank_TcpConnect(ipname, "9002", cilent_port +1, timeout);//sPortName,(timeout+1)/2
				RunLog( "连接数据端口[%d],sock[%d]",atoi(portnb), sockfd);
				if (sockfd > 0 ){
					RunLog("TcpConnect连接成功,TANK准备发送数据");			
					break;
				}else{
					RunLog("TcpConnect连接失败,延时10s继续");
					send_time++;
					if (send_time == 20)
						break;
					sleep(10);//exit(0);					
					continue;
				}		
			}
			if( send_time == 20){
				RunLog("连接20次失败，重取数据");
				continue;
			}
				
			send_time = 0;
			/*tcp send*/
			ret = TcpSendDataInTime(sockfd, message2, len, timeout);
			while ( ret < 0 && send_time < 3){
				send_time++;
				sleep (10);
				ret = TcpSendDataInTime(sockfd, message2, len, timeout);
			}

			if (send_time ==3) {
				RunLog("三次发送失败，退出重连，采集信息丢弃");		
				continue;
			}else{
				RunLog("发送数据成功，等待接收") ;
				int recvLength = 2;
				ret = TcpRecvDataInTime(sockfd, buffReturn, &recvLength, timeout);
				if (ret < 0) {
					RunLog("发送罐配置信息成功，没有收到返回");
					continue;
				}else {
					 if (0x00 == buffReturn[1]){
						RunLog("发送罐配置信息成功，返回不正常%02x%02x", buffReturn[0], buffReturn[1]);
						continue;
					}else if (buffReturn[1] ) {
						time_interval = buffReturn[1]&0x0f + (buffReturn[1]&0xf0)*10;
						RunLog("发送罐配置信息成功，返回正常时间间隔为%d ", time_interval);
					}
				}
			}
			if (sockfd > 0)
				close (sockfd);

			send_time = 0;
			/*connect to server 9002 port*/
			while(1) {
				portnb = "9002";
			
				//RunLog("ipname is %s", ipname);
				sockfd = Tank_TcpConnect(ipname, "9002", cilent_port -1, timeout);//sPortName,(timeout+1)/2
				RunLog( "连接数据端口[%d],sock[%d]",atoi(portnb), sockfd);
				if (sockfd > 0 ){
					RunLog("TcpConnect连接成功,TANK准备发送数据");			
					break;
				}else{
					RunLog("TcpConnect连接失败,延时2s 继续");
					send_time++;
					if (send_time == 20)
						break;
					sleep(2);//exit(0);					
					continue;
				}		
			}

			
			if( send_time == 20){
				RunLog("连接20次失败，重取数据");
				continue;
			}
			
			send_time = 0;
			/*tcp send*/
			ret = TcpSendDataInTime(sockfd, message, len, timeout);
			while ( ret < 0 && send_time < 3){
				send_time++;
				sleep (10);
				ret = TcpSendDataInTime(sockfd, message, len, timeout);
			}

			if (send_time ==3) {
				RunLog("三次发送失败，退出重连，采集信息丢弃");		
			}else{
				RunLog("发送数据成功，等待接收") ;
				int recvLength = 2;
				ret = TcpRecvDataInTime(sockfd, buffReturn, &recvLength, timeout);
				if (ret < 0) {
					RunLog("发送罐配置信息成功，没有收到返回");
				}else {
					 if (0x00 == buffReturn[1]){
						RunLog("发送罐配置信息成功，返回不正常");
						continue;
					}else if (buffReturn[1] ) {
						time_interval = buffReturn[1]&0x0f + (buffReturn[1]&0xf0)*10;
						RunLog("发送罐配置信息成功，返回正常时间间隔为%d ", time_interval);
					}
				}
			}
			if (sockfd > 0)
				close (sockfd);
			

		}

		/*judge whether send online message once a day*/
		sleep(time_interval*60);
	}
	sleep(5);
	}
	
}
//#endif
void tank_aoke(int p)
{	
	int ret, len;
	int i;
	//struct msg ak_msg;
	//int online_flag = 0;
	static int first_getin;
	pthread_t p_id_config;


	RunLog("开始运行奥科控制台监听程序......");
	
	if(open_aoke(p) < 0){
		//__debug(2, "open serial port fail");
		RunLog("打开串口失败open_aoke(p)  failed! ");
		exit(-1);
	}

	//1.  获取与TANK_MSG_ID 关联的消息队列标识
/*	if((msgid = msgget(TANK_MSG_ID, 0666)) < 0){  modify by liys  目前不做较时工作
		//__debug(2, "link message pipe fail");
		RunLog("取消息句柄失败! msgget(TANK_MSG_ID, 0666) failed! ");
	} 
*/
	tank_init_config(ipname, stano_bcd, &globle_config, &tank_cnt);
	RunLog("first getin is %d globle_config is %d tankcnt is %d", first_getin, globle_config, tank_cnt);
	if (!first_getin && globle_config){
              
               RunLog("first getin is %d globle_config is %d tankcnt is %d", first_getin, globle_config, tank_cnt);
               ret = pthread_create( &p_id_config, NULL, (void *) read_config, NULL );
               if(ret != 0){
                       __debug(2, "create read config thread fail");
                       exit(-1);
               }
                                               
       }

	while(1){
		if (globle_config && 0 != pthread_kill(p_id_config, 0))
		{
			
			RunLog("TcpPosServ的[%d]已退出,重启该线程", p_id_config);
			pthread_cancel(p_id_config); 
			sleep(3);			
			ret = pthread_create( &p_id_config, NULL, (void *) read_config, NULL );
           	if(ret != 0){
                   	RunLog( "create read config thread fail");
                   	exit(-1);
           	}
		}
		Ssleep(TANK_INTERVAL);
/*		if((msglen = msgrcv(msgid, &ak_msg, 16, 0, IPC_NOWAIT)) > 1){ modify by liys  目前不做较时工作
			// Waiting msg for INTERVAL seconds by select 
			switch( cmd_buf[0] ){
				case 0xa0:				// set tank parameters 
					aoke_a0(cmd_buf+2, msglen-2);
					break;
				case 0xa8:				// set system time 
					aoke_a8(cmd_buf+2, msglen-2);
					break;
				default:	
					break;
			}
}*/
		// CSSI.校罐项目数据采集处理
		if (gpGunShm->tr_fin_flag != 0) {
			gpGunShm->tr_fin_flag_tmp = gpGunShm->tr_fin_flag;
			gpGunShm->tr_fin_flag = 0;
			/*
			 * 若液位数据处理完成之后才复位tr_fin_flag, 那么如果期间有交易完成,
			 * 会导致那些交易之后没有液位数据记录,所以处理之前先缓存并复位tr_fin_flag.
			 */
		}
		// End of CSSI.校罐

		if(aoke_update() >= 0){
			if (gpGunShm->data_acquisition_4_TCC_adjust_flag == 1) {
				gpGunShm->data_acquisition_4_TCC_adjust_flag = 2;
			}
			gpGunShm->tr_fin_flag_tmp = 0;          // 确定记录数据之后再复位,避免缺失记录
			//__debug(2, "aoke update tank success");
			if (gpIfsf_shm->node_online[MAX_CHANNEL_CNT] == 0) {
				//if ((++online_flag) < 2) {  modify by liys 
				//	continue;
				//}
				//__debug(2, "TLG online");			
				RunLog(">>>>>>>> 液位仪处于联线状态<<<<<<<");
				tank_online(1);
				sleep(WAITFORHEART);
				for (i = 0; i < tp_num; ++i) {
					ChangeTpStatAlarm(gpIfsf_shm->tlg_data.tlg_node, 0x21 + i,
						gpIfsf_shm->tlg_data.tp_dat[i].TP_Status, gpIfsf_shm->tlg_data.tp_dat[i].TP_Alarm);
				}
			}
		} else {
			//__debug(2, "TLG offline"); modify by liys 
			//if (gpIfsf_shm->node_online[MAX_CHANNEL_CNT] != 0) {
				//if ((--online_flag) == 0) {
					tank_online(0);
					RunLog(">>>>>>>> 液位仪处于离线状态<<<<<<<");
					continue;
				//}
			//}
		}

	//	print_result();
		
	}
		
	
}

	

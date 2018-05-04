/* 				Bo Rui Te - Tank monitor system

	Support Types: 2007.6 version 6.01
	
	2007.11
*/

#include "tank_pub.h"


static int serial_fd = 0;

static int open_brite(int p)
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

static int check_ret(unsigned char * buf)
{	int len;
	len = strlen(buf);
	if(buf[len-1] != 0x03) return -1;

	return 0;
}

static int send_cmd(unsigned char *cmd, unsigned char *retbuf, int len, int delay)
{	int i, ret, size, count=0;
	struct timeval tv;
	fd_set ready;

	tcflush (serial_fd, TCIOFLUSH);			//clear serial buffer
	tv.tv_sec = delay;						// timeout in seconds
	tv.tv_usec = 0;
	
	if(write(serial_fd,cmd,len) != len){
		__debug(2, "write command fail");
		return -1;
		}
	
	Ssleep(TANK_INTERVAL_READ);			//this is necessary for opw and Yitong
	len = 0;
	while(count++ < 3){
		FD_ZERO(&ready);
		FD_SET(serial_fd, &ready);
		if(select(serial_fd+1, &ready, NULL, NULL, &tv) > 0){
			 ret = read(serial_fd, retbuf+len, MAX_TP_NUM*65);
			 if(ret > 0){
			 	len += ret;
			 	if(retbuf[len-1] == 0x03){
				   printf("received data: ");
					for(i=0;i<len;i++) printf("%02x ",retbuf[i]); printf("\n");
                            
					return 0;
			 		}
				}
			}
		}
	
	return -1;
}

static int parse_data(unsigned char *src, unsigned char *des)
{ 	int i, k, lenstr, tmp_len;
	unsigned char tmp1[12]={0};

	lenstr = strlen(src);
	for(i=0;i<lenstr;i++){
		if(src[i] >= 0x30) 
			src[i] &= 0x0f;		//ascii to bcd
		}
	
	lenstr--; 

	for( i = lenstr ; i >= 0 ; i-- ){
		
		if( src[i] == 0x2e ) {           // src[i] = "."
			if( i == (lenstr -1)){    //一位小数
				tmp_len = 10;           	     //默认按两位小数处理
				break;
			}else if( i == (lenstr -2)){
				tmp_len = 11;		     //两位小数
				break;

			}else{
				RunLog("小数点位置错误! 大于两位小数");
			}	
		}

		if( 0 == i ){                    //没有小数位
			tmp_len = 9;
			break;
		}
	}
	
	for( i = tmp_len ;lenstr >= 0 && i >= 0;lenstr--){
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

static int brite_a8(unsigned char *msg, int mlen)
{
	int ret = 0;
	char tmpbuf[16];
	char tmpbuf1[32] = {0x52,0x05,0x31,0x05,0x44,0x05, 0x31,0x2e,0x30,0x31,0x02};
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
	/* set tank system time */
	memcpy(tmpbuf1+11, tmpbuf,8);		// need reprogram!!
	tmpbuf1[19] = 0x05;
	memcpy(tmpbuf1+20, tmpbuf,6);
	tmpbuf1[26] = 0x05;
	tmpbuf1[27] = 0x04;
	tmpbuf1[28] = 0x03;

	memset( tmpbuf, 0, sizeof(tmpbuf) );
	ret = send_cmd(tmpbuf1, tmpbuf, 29, 3);
	return 0;
}

static void fill_struct(unsigned char * pbuf)
{	int i, j, k, k1,p, tpno=-1, len;
	int level_changed_flag = 0;
	int tpno_current = 0;
	long level1, level2;
	unsigned char data[6], tempdata[6],tempdata1[6], msg[16], tmp[MAX_MSG_LEN];
	float f, tov;
	
	len = strlen(pbuf);
	
	for(p=0;pbuf[p] != 0x02; p++);			//get start position ,to STX (0x02)
		
	while(pbuf[++p] != 0x03){                     //ETX 数据终止符
		memset(tmp, 0, MAX_MSG_LEN);
		for(i=0;pbuf[p]!=0x04 && p <len;p++,i++)    //EOT 段终止符
			tmp[i] = pbuf[p];
		tmp[i] = 0x04;

		if(p >= len) break;
			
		j = 0; //标示数据段
		
		k = 0;//指示到第几个字节
		
		while(tmp[k] != 0x04){
			memset(msg, 0, 16);
			memset(data, 0, sizeof(data));
			for(i=0;tmp[k] != 0x05;i++,k++) {
                           // printf(" %02x",tmp[k]);
				if (tmp[k] == 0x04	) break;
				msg[i] = tmp[k];
			}                
					 
			if (tmp[k] == 0x04	) 				
			    break;
			
			if(tmp[k] == 0x05){
				j++;
				k++;
				}

			
			if(j < 3){
				if(j == 1) tpno = atoi(msg) - 1;
				continue;
				}
			
			if(tpno < 0 ||tpno > 16) break;		//max is 12 in borite

			//if(j==5 && msg[0] == 0x2d) sign = 0x10;
			// CSSI.校罐项目数据采集处理
			if (gpGunShm->data_acquisition_4_TCC_adjust_flag == 1) {
				// 采集服务刚生效或者程序重启时写入一条零液位记录
				write_tcc_adjust_data(NULL, "%d,0,,,,,,0,0,0,0", tpno + 1);
			}
			if (level_changed_flag != 0 || gpGunShm->tr_fin_flag_tmp != 0) {
				// 若开始处理另一个罐的数据,则判断之前处理的那个罐的数据是否需要记录
				if (level_changed_flag != 0) {
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

				tpno_current = tpno;
			}
			// End of CSSI.校罐项目数据采集处理
			
			parse_data(msg, data);
			
			switch(j){
				case 3:				// fuel level
					//gpIfsf_shm->tlg_data.tp_dat[tpno].Product_Level[0] = 0x05;
					//三位小数，向左移动半个字节 增加一位小数
					bzero(tempdata,sizeof(tempdata));  
					tempdata[0] = (data[0]) << 4  | ((data[1]) >> 4);
					tempdata[1] = (data[1]) << 4  | ((data[2]) >> 4);
                                 	tempdata[2] = (data[2]) << 4  | ((data[3]) >> 4);
                                   	tempdata[3] = (data[3]) << 4  | ((data[4]) >> 4);
                                   	tempdata[4] = (data[4]) << 4  | ((data[5]) >> 4);
                                   	tempdata[5] = (data[5]) << 4  | 0x00;	
									
					memcpy(gpIfsf_shm->tlg_data.tp_dat[tpno].Product_Level, tempdata+2, 4);


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
#if 0
					RunLog("\n");			  
					RunLog("油高\n");
					for(k1=0;k1<4;k1++) RunLog("%02x ",gpIfsf_shm->tlg_data.tp_dat[tpno].Product_Level[k1]);
#endif					
					break;
				case 4:				//water level
					//gpIfsf_shm->tlg_data.tp_dat[tpno].Water_Level[0] = 0x05;
					//三位小数，向左移动半个字节 增加一位小数
					bzero(tempdata,sizeof(tempdata));  
					tempdata[0] = (data[0]) << 4  | ((data[1]) >> 4);
					tempdata[1] = (data[1]) << 4  | ((data[2]) >> 4);
                                 	tempdata[2] = (data[2]) << 4  | ((data[3]) >> 4);
                                   	tempdata[3] = (data[3]) << 4  | ((data[4]) >> 4);
                                   	tempdata[4] = (data[4]) << 4  | ((data[5]) >> 4);
                                   	tempdata[5] = (data[5]) << 4  | 0x00;	
									
					memcpy(gpIfsf_shm->tlg_data.tp_dat[tpno].Water_Level, tempdata+2, 4);
#if 0
					RunLog("\n");			  
					RunLog("水高\n");					
					for(k1=0;k1<4;k1++) RunLog("%02x ",gpIfsf_shm->tlg_data.tp_dat[tpno].Water_Level[k1]);
#endif					
					break;
				case 5:				//temperature
					//gpIfsf_shm->tlg_data.tp_dat[tpno].Average_Temp[0] = sign |0x03;

					if(msg[0] == 0x2d){
						gpIfsf_shm->tlg_data.tp_dat[tpno].Average_Temp[0] = 0x82;
					} else {
						gpIfsf_shm->tlg_data.tp_dat[tpno].Average_Temp[0] = 0x02;
					}
					memcpy(gpIfsf_shm->tlg_data.tp_dat[tpno].Average_Temp+1, data+4, 2);
#if 0					
					RunLog("\n");			   					
					RunLog("温度\n");									
					for(k1=0;k1<3;k1++) RunLog("%02x ",gpIfsf_shm->tlg_data.tp_dat[tpno].Average_Temp[k1]);
#endif					
					
					break;

#if 0
				case 6:
					gpIfsf_shm->tlg_data.tp_dat[tpno].Total_Observed_Volume[0] = 0x09;
                                   bzero(tempdata,sizeof(tempdata));
					tempdata[0] = (data[0]) << 4  | ((data[1]) >> 4);
					tempdata[1] = (data[1]) << 4  | ((data[2]) >> 4);
                                   tempdata[2] = (data[2]) << 4  | ((data[3]) >> 4);
                                   tempdata[3] = (data[3]) << 4  | ((data[4]) >> 4);
                                   tempdata[4] = (data[4]) << 4  | ((data[5]) >> 4);
                                   tempdata[5] = (data[5]) << 4  | 0x00;


					
					memcpy(gpIfsf_shm->tlg_data.tp_dat[tpno].Total_Observed_Volume+1, tempdata, 6);

					RunLog("\n");			
					RunLog("油体积\n");   
					for(k1=0;k1<7;k1++) RunLog("%02x ",gpIfsf_shm->tlg_data.tp_dat[tpno].Total_Observed_Volume[k1]);
					
					break;
#endif					

				case 6:                 
					// ifsf  Dataid = 42  为油净体积 
					gpIfsf_shm->tlg_data.tp_dat[tpno].Total_Observed_Volume[0] = 0x09;
                                   bzero(tempdata,sizeof(tempdata));  
					tempdata[0] = (data[0]) << 4  | ((data[1]) >> 4);
					tempdata[1] = (data[1]) << 4  | ((data[2]) >> 4);
                                   tempdata[2] = (data[2]) << 4  | ((data[3]) >> 4);
                                   tempdata[3] = (data[3]) << 4  | ((data[4]) >> 4);
                                   tempdata[4] = (data[4]) << 4  | ((data[5]) >> 4);
                                   tempdata[5] = (data[5]) << 4  | 0x00;					
						
					memcpy(gpIfsf_shm->tlg_data.tp_dat[tpno].Total_Observed_Volume+1, tempdata, 6);
#if 0
					RunLog("\n");			
					RunLog("标准体积\n");
					for(k1=0;k1<7;k1++) RunLog("%02x ",gpIfsf_shm->tlg_data.tp_dat[tpno].Gross_Standard_Volume[k1]);
#endif
                                                                      
					break;
                              case 8:  
				      //水容积,同时计算出油水总体积对应ifsf Dataid = 41
					gpIfsf_shm->tlg_data.tp_dat[tpno].Gross_Standard_Volume[0] = 0x09;
                                   bzero(tempdata,sizeof(tempdata));								   
					tempdata[0] = (data[0]) << 4  | ((data[1]) >> 4);
					tempdata[1] = (data[1]) << 4  | ((data[2]) >> 4);
                                   tempdata[2] = (data[2]) << 4  | ((data[3]) >> 4);
                                   tempdata[3] = (data[3]) << 4  | ((data[4]) >> 4);
                                   tempdata[4] = (data[4]) << 4  | ((data[5]) >> 4);
                                   tempdata[5] = (data[5]) << 4  | 0x00;

#if 0
					RunLog("\n");			
					RunLog("水体积\n");                                  
					for(k1=0;k1<6;k1++) RunLog("%02x ",tempdata[k1]);
#endif				
					BBcdMinus(gpIfsf_shm->tlg_data.tp_dat[tpno].Total_Observed_Volume+2, 5, \
						tempdata+1, 5 ,gpIfsf_shm->tlg_data.tp_dat[tpno].Gross_Standard_Volume+2);
#if 0
					RunLog("\n");			
					RunLog("油水总体积\n");
					for(k1=0;k1<7;k1++) RunLog("%02x ",gpIfsf_shm->tlg_data.tp_dat[tpno].Gross_Standard_Volume[k1]);
#endif				
                                   break;
				default: break;
				}
			}
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
	for(i=0;i<14;i++){
		if(msg[i] != 0x20) msg[i] &= 0x0f;
		else msg[i] = 0x00;
		}
	for(j=0, i=0;i < 14;i=i+2){			
		tmp[j] = (msg[i] << 4) | msg[i+1];
		j++;
		}
	memcpy(gpIfsf_shm->tlg_data.tlg_dat.Current_Date, tmp, 4);
	memcpy(gpIfsf_shm->tlg_data.tlg_dat.Current_Time, tmp+4, 3);
//	for(i=0;i<3;i++) printf("%02X ", gpIfsf_shm->tlg_data.tlg_dat.Current_Time[i]);
//	printf("\n");

}

static void set_status(unsigned char *pbuf)
{	int i,j,k,len,p,tpno,itpno;
	unsigned char data[6], sign, msg[16], tmp[MAX_MSG_LEN];
	char *alarm[]={"A201","A202","A203","A204","A205","A206","T104","A306","A307"};
	unsigned char ifsf_alarm[15][2]={0};
	static unsigned char pre_ifsf_alarm[15][2] = {0};
	len = strlen(pbuf);
	for(p=0;pbuf[p] != 0x02; p++);			//get start position

		
	while(pbuf[++p] != 0x03){
		memset(tmp, 0, MAX_MSG_LEN);
		for(i=0;pbuf[p]!=0x04 && p <len;p++,i++)
			tmp[i] = pbuf[p];
		tmp[i] = 0x04;
		if(p >= len) break;
		
		j = 0;
		k = 0;
		
		while(tmp[k] != 0x04){
			memset(msg, 0, 16);
			for(i=0;tmp[k] != 0x05;i++,k++) msg[i] = tmp[k];
			if(tmp[k] == 0x05){
				j++;
				k++;
				}
			
			if(j == 1){                                               //罐号 
				tpno = atoi(msg) -1;
				if(tpno < 0 ||tpno > 16) break;		//max is 12 in borite
				gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Status = 2;
				continue;
				}
			
                    if(j == 3){                                               //报警编码
                            for(i=0;i<9 && strcmp(alarm[i], msg) != 0;i++);
				if(i==9) return;
				switch(i){
					case 0:                                   //A201 高液位报警
					         ifsf_alarm[tpno][1] |= 0x20; break;
					case 1:                                  //A202 高高液位报警
					         ifsf_alarm[tpno][1] |= 0x10; break;
					case 2:                                  //A203 低液位报警
					         ifsf_alarm[tpno][1] |= 0x40; break;
					case 3:                                  //A204 低低液位报警
						  ifsf_alarm[tpno][1] |= 0x80; break;
					case 4:                                  //A205 高水位报警
                                            ifsf_alarm[tpno][0] |= 0x01; break;
					case 5:                                  //A206 高高水位报警
						 ifsf_alarm[tpno][0] |= 0x01; break;
					case 6:                                  //T104 探棒错误
					        RunLog("!!!!!!!!!!!! 探棒%d 中断 !!!!!!!!!!!!!!",tpno + 1);
                                          // gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Status = 1;
						 break;
					case 7:                                  //A306        泄漏报警
					        ifsf_alarm[tpno][0] |= 0x02; break;   //Tank Loss alarm
					case 8:                                  //A307         渗漏报警
					        ifsf_alarm[tpno][0] |= 0x04; break; //tank leak alarm
						
					default:  break;
				}
                      
			}

/*			if(j == 2){
				for(i=0;i<6 && strcmp(alarm[i], msg) != 0;i++);
				if(i == 6){
					//__debug(2, "no alarm / warning");
					return ;
					}
				switch(i){
					case 0: gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Alarm[1] |= 0x40;break;	//low product level
					case 1: gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Alarm[0] |= 0x01;break;	//high water
					case 2: break;
					case 3: break;
					case 4: gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Alarm[0] |= 0x04;break;	//tank leak alarm
					case 5: break;

					default: break;
					}
				}

			
			}						
*/			
	}
		

}

       for(itpno = 0; itpno < PROBER_NUM; itpno++)
       {
		if ((memcmp(pre_ifsf_alarm[itpno], ifsf_alarm[itpno], 2) != 0) && !((ifsf_alarm[itpno][0] == 0 ) &&(ifsf_alarm[itpno][1] == 0))) {
		RunLog(">>>>>>>>>>>>>>>> ifsf_alarm[itpno]  = %02x%02x",ifsf_alarm[itpno][0],ifsf_alarm[itpno][1]);
				ChangeTpStatAlarm(gpIfsf_shm->tlg_data.tlg_node,
					0x21+itpno, gpIfsf_shm->tlg_data.tp_dat[itpno].TP_Status, ifsf_alarm[itpno]);
				memcpy(pre_ifsf_alarm[itpno], ifsf_alarm[itpno], 2);
				bzero(ifsf_alarm[itpno], 2);
		}
       }

}

void tank_brite(int p)
{	int i, ret,msglen, msgid;
	unsigned char cmd[16]={0x41,0x05,0x31,0x05,0x52,0x05, 0x31,0x2e,0x30,0x31,0x02,0x30,
		0x05,0x04,0x03};
	unsigned char cmd1[16]={0x42,0x05,0x31,0x05,0x52,0x05,0x31,0x2e,0x30,0x31,0x02,0x30,
		0x05,0x04,0x03};
	unsigned char ret_buf[MAX_TANK_PARA_LEN], msg[16];
	struct msg br_msg;
	int alarm_read_cnt = 5;

	
	RunLog("开始运行Brite  协议监听程序......");

	if(open_brite(p) < 0){
		__debug(2, "open serial port for brite fail");
		exit(-1);
		}

#if 0	
	if((msgid = msgget(TANK_MSG_ID, 0666)) < 0){
		__debug(2, "link message pipe fail");
		}
#endif	

	while(1){
		if (alarm_read_cnt != 5) {
			Ssleep(TANK_INTERVAL);
		}

#if 0		
		if((msglen = msgrcv(msgid, &br_msg, 16, 0, IPC_NOWAIT)) > 1){
			if(brite_a8(msg, msglen) > 0)
				__debug(2, "set system time success");
			}
#endif		
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


		memset(ret_buf, 0, MAX_TANK_PARA_LEN);
		ret = send_cmd(cmd, ret_buf, 15, 3);	
		

		if(ret == 0){
			if (gpIfsf_shm->node_online[MAX_CHANNEL_CNT] == 0) {				
				tank_online(1);
				sleep(WAITFORHEART);
				RunLog(">>>>>>>> 液位仪处于联线状态<<<<<<<");	
				RunLog(">>>>>>>> 配置文件中探棒的数量: %d",PROBER_NUM);
				for (i = 0; i < PROBER_NUM; i++) {
					ChangeTpStatAlarm(gpIfsf_shm->tlg_data.tlg_node, 0x21 + i, 2, gpIfsf_shm->tlg_data.tp_dat[i].TP_Alarm);
				}
			}
		} else {			
			tank_online(0);
			RunLog(">>>>>>>> 液位仪处于离线状态, CommandSend 返回值= %d <<<<<<<", ret );
			continue;
		}

		if(check_ret(ret_buf) != 0){
			RunLog("得到的罐存数据校验失败!send: %s\n return: %s", cmd, ret_buf);			
			continue;
		}
		
		fill_struct(ret_buf);
		if (gpGunShm->data_acquisition_4_TCC_adjust_flag == 1) {
			gpGunShm->data_acquisition_4_TCC_adjust_flag = 2;
		}
		gpGunShm->tr_fin_flag_tmp = 0;          // 确定记录数据之后再复位,避免缺失记录

		RunLog("TLG Report: %s", ret_buf);

		set_tlg_time();
		
		while (alarm_read_cnt-- <= 0) {
			alarm_read_cnt = 5;
			Ssleep(TANK_INTERVAL_ALARM);
			memset(ret_buf, 0, MAX_TANK_PARA_LEN);
			ret = send_cmd(cmd1, ret_buf, 15, 3);	
			
			if((ret = check_ret(ret_buf)) != 0){
				continue;
			}
#if SHOW_ALARM_MSG
			RunLog("TLG Alarm Msg: %s", ret_buf);
#endif
				  
			set_status(ret_buf);
			
		}
	}

	
}


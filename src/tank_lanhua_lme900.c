/* 	
       兰州石化LME-900 - Tank monitor system

	Support Types:  LanHua  
	Version:		   LME-900
	This is IFSF protocol program
	目前只会出现01000000
	2009.03.02
*/

#include "tank_pub.h"

int udp_sock;

int VRCommInitiate();
int NozzleLift(char *NozzleID, int SecondsDelay);
int NozzleDrop(char *NozzleID, double CumulativeVolume, float TransactVolume, int SecondsDelay);
int VRCommTerminate(int ForceQuit);

int TrxStart(int FP, int SecondsDelay);
int TrxStop(int FP, int NozzleID, double CumulativeVolume, float TransactVolume, int SecondsDelay);

int CommandSend(char *strCommand,  char *buffReturn);

int CheckSum(char *Source, char *sReturn);


#define MAX_RETURN_LEN		(MAX_TP_NUM*31)
#define SINGLE_DATA_LEN			31
#define TCP_HEAD_LEN			4	
#define TIMEOUT				(5000*1000)	
#define RSP_HEAD_LEN			       15            //xx201 + 时间   									
#define RSP_HEAD_DT_OFFSET		5             //xx201		
#define RSP_HEAD_DT_LEN			10									
#define RSP_TAIL_LEN			       6             //&& + 校验码 			
#define TANK_STATUS_LEN			9             //罐号(2字符) + 字符串0000007									
#define TANK_STATUS_LEN2		(TANK_STATUS_LEN+7*8)				
#define DELIVERY_ITEM_LEN		22						
#define DELIVERY_ITEM_LEN2	(DELIVERY_ITEM_LEN+10*8)	
static int serial_fd=0;
static unsigned char flag[31];


static int open_opw(int p)
{
	int fd;
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
		__debug(1, "open serial port %s successful!, fd: %d", get_serial(p), serial_fd);
		return 0;
	}

	close(fd);
	return -1;
}
static void vr_adjust(void)
{
	int clen, ret;
	unsigned char msg[MAX_RECORD_MSG_LEN];
	int msglen,indtyp;
	unsigned char gun_status[MAX_GUN_CNT];
	//unsigned char gun_str[16];
	int panelno,gun_no;
	unsigned char tmp[5];
	double total;
	float  volume;
	struct sockaddr_in caddr;

	unsigned long long test;
	
	
	memset(msg, 0, sizeof(msg));
	while(1){
		/* if((ret = recvfrom(udp_sock, msg, sizeof(msg), 0, (struct sockaddr *)&caddr, &clen)) <= 0){
			__debug(2, "recvfrom in adjust tank fail: d%", ret);
			continue;
			}
		*/	
		indtyp = 1;
		msglen = sizeof(msg);
		ret = RecvMsg(TANK_MSG_ID, msg, &msglen, &indtyp, TIME_FOR_RECV_CMD); 
		if (ret < 0){
			continue;
		}
								
		//__debug(3, "received msg from oil_main: %s", msg);
		
		if(memcmp(msg, GUN_CALL, CMD_LEN ) == 0 || memcmp(msg, GUN_BUSY, CMD_LEN ) == 0){
			if( gun_status[msg[2]] == CALL )
				continue;
			gun_status[msg[2]] = CALL;
			//snprintf( gun_str, sizeof(gun_str), "%02d", msg[2] );

			panelno = 0;
			panelno = msg[2] ;
			RunLog("调用发送抬抢信号函数TrxStart( %d, 0)",panelno);
			
			//ret=NozzleLift( gun_str, 0 );

			ret = TrxStart(panelno,0);
			
			//__debug(3, "send data to tank: %s",gun_str);
		}

		if(memcmp( msg, GUN_OFF, CMD_LEN ) == 0){
			memset(tmp, 0, sizeof(tmp));			/*pump total*/
			memcpy(tmp, msg+4, 5);
			total = (double)Bcd2U64(tmp,5)/100;			
			
			memset( tmp, 0, sizeof(tmp) );			/*pump volume*/
			memcpy( tmp, msg+9, 3 );		
			volume = (float)Bcd2U64(tmp,3)/100;		
			
			gun_status[msg[2]] = OFF;

			panelno = 0;
			panelno = msg[2] ;

			gun_no = 0;
			gun_no = msg[3] ;
					 
			//snprintf( gun_str, sizeof(gun_str), "%02d", msg[2] );

			RunLog("调用发送挂抢信号函数TrxStop(%d, %d, %.2lf, %.2lf, 0)",panelno,gun_no,total,volume);
					 
			ret=TrxStop( panelno,gun_no, total, volume, 0 );			
			//__debug(3, "Adjust data:gun_str=[%s] dropped.total=[%.2lf],volume=[%.2lf]!ret=[%d]", gun_str, total, volume, ret );
			}
		
		}

}

static int check_ret(unsigned char *buf)
{	
      int icount,len;
      unsigned char bufcrc[2];
      unsigned short  tempchar;
      len = strlen(buf) - 4;      	 
      tempchar = 0;
      for(icount =0; icount <len-1 ; icount++){
		tempchar += buf[icount];	  				  	
      }
	  
      bzero(bufcrc,sizeof(bufcrc));
      HexPack(&buf[len-1],bufcrc,4);
      tempchar += bufcrc[0] * 256 + bufcrc[1];	  	
	  
	if (tempchar == 0)	 {
		return 0;
	}
	else{
		return -1;
	}	
}


static int send_cmd(unsigned char *cmd, unsigned char *retbuf, int len, int delay)
{
       int i, ret, size, count;
	struct timeval tv;
	fd_set ready;
	unsigned char crc;

	tcflush(serial_fd, TCIOFLUSH);			//clear serial buffer
	tv.tv_sec = delay;						// timeout in seconds
	tv.tv_usec = 0;
	
	if(write(serial_fd,cmd,len) != len){
		RunLog("write command fail");
		return -1;
	}
	
	Ssleep(TANK_INTERVAL_READ);			//this is necessary for opw and Yitong
	len = 0;
	count = 0;
	while(count++ < 3){
		FD_ZERO(&ready);
		FD_SET(serial_fd, &ready);
		if(select(serial_fd + 1, &ready, NULL, NULL, &tv) > 0){
			 ret = read(serial_fd, retbuf+len, MAX_TP_NUM*72 );
			 if(ret > 0){
			 	len += ret;
			 	if(retbuf[len-6] == '&'){
					__debug(3, "send: %s\n return: %s\nlen:%d", cmd, retbuf,len);					
					return 0;
				}
			}
		}
	}

	RunLog("返回数据失败");
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

static int opw_a0(unsigned char *msg, int mlen)
{


return 0;
}

static int opw_a8(unsigned char *msg, int mlen)
{
	int ret = 0;
	char tmpbuf[64], tmpbuf2[64];
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
	tmpbuf[12]=0;	
	memset( tmpbuf2, 0, sizeof(tmpbuf2) );
	snprintf( tmpbuf2, sizeof(tmpbuf2), "\x01s50100%s", tmpbuf+2 );		
	memset( tmpbuf, 0, sizeof(tmpbuf) );
	ret = CommandSend(tmpbuf2, tmpbuf);

	if((ret = check_ret(tmpbuf)) != 0) return -1;
	
	return ret;
}

static void fill_struct( unsigned char *tmpbuf)
{	char tmpbuf2[256];
	char *p = tmpbuf + 2 + 7;		// 罐号 +   字符串
	unsigned char result[16], data[6];
	unsigned char tempdata[2];
	int i, num, tpno,k,k1;	
	int level_changed_flag = 0;
	long level1, level2;
	float f;
	unsigned char sign=0;
	const unsigned char zero_arr[6] = {0};
	static unsigned char first_flag = 0;

       //罐号+  字符串0000007 + FFFFFFFF ....
	memset( tmpbuf2, 0, sizeof(tmpbuf2) );

	//罐号
	memcpy( tmpbuf2, tmpbuf, 2 );
	tpno = atoi( tmpbuf2 )-1;						

	if(tpno < 0 || tpno >30) return ;
	if (flag[tpno] == 0 && gpIfsf_shm->auth_flag == DEFAULT_AUTH_MODE) {
		if (first_flag == 0) {
			sleep(12);  // for test
			__debug(2, "%s:>>> after sleep 12s ......", __FILE__);
		}
		flag[tpno] = 1;
	}
	

       // P-> FFFFFFFF....
	for(i = 0; i < 7; i++){
		if(i == 2) continue;
		memset(tmpbuf2, 0, sizeof(tmpbuf2));
		memset(result, 0, sizeof(result));
		memset(data, 0, sizeof(data));
		memcpy( tmpbuf2, p+i*8, 8 );
		asc2hex(tmpbuf2, result);
		f = exp2float(result);			

              //if (( i+1 == 4) || (i+1 ==5) ) f = f *10; //兰化的特殊处理
		//only temperature can be nagetive
		if(f<0 && i == 5){			
			sign = 0x80;
			f = f*(-1);
		}

		if(f > 0){
			memset(tmpbuf2, 0, sizeof(tmpbuf2));	
			sprintf(tmpbuf2, "%.3f", f);
			parse_data(tmpbuf2, data);
		}
		
		 switch(i+1) {
			case 1:							
				gpIfsf_shm->tlg_data.tp_dat[tpno].Total_Observed_Volume[0] = 0x09;
				memcpy(gpIfsf_shm->tlg_data.tp_dat[tpno].Total_Observed_Volume+1, data, 6);				
				break;
#if 0			
                     case 2: 		
                            //observed volume // TC == G.S.V  
			
				gpIfsf_shm->tlg_data.tp_dat[tpno].Gross_Standard_Volume[0] = 0x09;
				memcpy(gpIfsf_shm->tlg_data.tp_dat[tpno].Gross_Standard_Volume+1, data, 6);

				if ((memcmp(gpIfsf_shm->tlg_data.tp_dat[tpno].Total_Observed_Volume+1, zero_arr, 6) == 0) &&
									(gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Status != 1)) {
					gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Status = 1;
					ChangeTpStatAlarm(gpIfsf_shm->tlg_data.tlg_node,
						0x21+tpno, gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Status, NULL);
				} else if (gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Status == 1) {
					gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Status = 2;
					ChangeTpStatAlarm(gpIfsf_shm->tlg_data.tlg_node,
						0x21+tpno, gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Status, NULL);
				}
			break;
#endif			
			case 4:			
				//油位
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
					} else if (level2 >= PRODUCT_LEVEL_INTERVAL || level2 <= (-1 * PRODUCT_LEVEL_INTERVAL)) {
						level_changed_flag = 1;
					}
					TCCADLog("level_changed_flag: %d, level2: %ld",
								level_changed_flag, level2);
				}
				break;
			case 5:			
				//水位
				memcpy(gpIfsf_shm->tlg_data.tp_dat[tpno].Water_Level, data + 2, 4);
				break;
			case 6:			
				//温度
				gpIfsf_shm->tlg_data.tp_dat[tpno].Average_Temp[0] = sign | 0x02;
				bzero(tempdata,sizeof(tempdata));				
				tempdata[0] = (data[3]) << 4  | ((data[4]) >> 4);
				tempdata[1] = (data[4]) << 4  | ((data[5]) >> 4);
				memcpy(gpIfsf_shm->tlg_data.tp_dat[tpno].Average_Temp + 1 , tempdata , 2);				
				break;
                     case 7:                
				// 油净体积 =  油水总体积 - 水体积     
				gpIfsf_shm->tlg_data.tp_dat[tpno].Gross_Standard_Volume[0] = 0x09;
				BBcdMinus(gpIfsf_shm->tlg_data.tp_dat[tpno].Total_Observed_Volume + 2, 5,\
					data + 1, 5 ,gpIfsf_shm->tlg_data.tp_dat[tpno].Gross_Standard_Volume+2);	
				gpIfsf_shm->tlg_data.tp_dat[tpno].Average_Temp[0] = sign | 0x02;
                            break;

			default:
				break;			
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

static void set_status(unsigned char *pbuf)
{	int i, len, tpno, num, prob, no;
	unsigned char tmp[8]={0},alarm[15][2]={0};
	static unsigned char pre_alarm[15][2] = {0};

	//pbuf : xx101＋时间＋罐号＋状态信息（8位）＋ &&＋校验码
	
	
	len = strlen(pbuf);
	//if(pbuf[0] != 0x01 || pbuf[len-1] != 0x03) return ;
	pbuf += 15;

	while(*pbuf != '&'){   
              //tt + ssssssss		
              //罐号
		memcpy(tmp, pbuf, 2);
		tpno = atoi(tmp)-1;
		if(tpno < 0 || tpno >30) return ;

		gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Status = 2;

	       //状态信息(8 位)
	       pbuf += 2;		   		
			
			memcpy(tmp,pbuf,8);
                     if (tmp[0] == 0x31){  //Tank Probe Out Alarm
			                   RunLog(">>>>>>>>>>>>>>> 探棒%d 错误,Tank Probe error",tpno + 1);
					     alarm[tpno][1] |= 0x01;					
                     }
			if (tmp[1] == 0x30){  //液位数据状态：1表示有效数据，0表示无效数据，目前翻译为探棒错误
					      alarm[tpno][1] |= 0x01;	                
                     }
			
                     if (tmp[2] == 0x31){  //Tank High Water Warning
					     alarm[tpno][0] |= 0x01; 	                
                     }

                     if (tmp[3] == 0x31){ //溢出状态：1表示液位溢出，0表示液位未溢出    上上限
			                  // alarm[tpno][1] |= 0x02; 					     
			                  alarm[tpno][1] |= 0x10; 
                     }
		 	
                     if (tmp[4] == 0x31){ //油位状态：1表示油位过低，0表示油位正常      油位下限
					    alarm[tpno][1] |= 0x40; 		
                     }

                     if (tmp[5] == 0x31){ //进油状态：1表示需要进油，0表示不需要进油    油位下下限		
					    alarm[tpno][1] |= 0x80; 		 
                     }

                    if (tmp[7] == 0x31){ //Tank High Product Alarm
					    alarm[tpno][1] |= 0x20; 		
                     } 					
               pbuf +=8;		
               if (memcmp(pre_alarm[tpno], alarm[tpno], 2) != 0) {
			ChangeTpStatAlarm(gpIfsf_shm->tlg_data.tlg_node,
				0x21+tpno, gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Status, alarm[tpno]);
			memcpy(pre_alarm[tpno], alarm[tpno], 2);
			bzero(alarm[tpno], 2);
	        }			   
	}
		


}

void tank_lanhua_lme900(int p)
{	
	int i,newsock, msgid,ret;
	int len,msglen, buf_len, lenstr, num;
	int tpno;
	unsigned char tmpbuf[MAX_CMD_LEN];
	unsigned char  msg[16], s_tank[256];
	unsigned char *buf;
	pthread_t p_id;
	struct msg vr_msg;
	unsigned char tp_num[2];
	int alarm_read_cnt = 5;

       RunLog("开始运行兰州石化 LME - 900 协议监听程序......");

	if(open_opw(p) < 0){
		RunLog("打开串口失败open_opw(p)  failed! ");
		exit(-1);
	}
	
	buf_len = MAX_TANK_NUM*TANK_STATUS_LEN2+RSP_HEAD_LEN+RSP_TAIL_LEN+32;
	if((buf = (char *)malloc(buf_len)) == NULL){		
		return ;
	}
	
	while(1){
		if (alarm_read_cnt != 5) {
			Ssleep(TANK_INTERVAL);
		}
		memset(buf, 0, buf_len);
		memset(s_tank, 0, 256);

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

		//取油罐信息
		snprintf(s_tank, sizeof(s_tank), "yr201");
		ret = send_cmd(s_tank, buf, 5, 3);		
		
		if(ret == 0){
			if (gpIfsf_shm->node_online[MAX_CHANNEL_CNT] == 0) {				
				tank_online(1);
				sleep(12);
				RunLog(">>>>>>>> 液位仪处于联线状态<<<<<<<");	
				RunLog(">>>>>>>> 探棒的数量: %d",PROBER_NUM);
				for (i = 0; i < PROBER_NUM; i++) {
					ChangeTpStatAlarm(gpIfsf_shm->tlg_data.tlg_node, 0x21 + i, 2, gpIfsf_shm->tlg_data.tp_dat[i].TP_Alarm);
				}
			}
		} else {			
			tank_online(0);
			RunLog(">>>>>>>> 液位仪处于离线状态, send_cmd 返回值= %d <<<<<<<", ret );
			continue;
		}

		if((ret = check_ret(buf)) != 0){			
			continue;
		}
		
		RunLog("TLG Report: %s", buf);


		/* 
		    xx201（发送的命令）＋时间＋ 罐号（2字符） 
		    ＋ 字符串0000007   ＋ 数据（每8个字符为一组，共7组，
		    依次为油水容积、温度修正后油水容积、剩余容积、油位、水位、温度、水容积） ＋ 
		    &&   ＋校验码
              */
		memset( tmpbuf, 0, sizeof(tmpbuf) );
		memcpy( tmpbuf, buf+RSP_HEAD_DT_OFFSET, 10 );

		//YYMMDDHHmm00
		strcat( tmpbuf, "00" );


	       //DATE = CCYYMMDD
		HexPack( tmpbuf, msg, 12 ); 
		gpIfsf_shm->tlg_data.tlg_dat.Current_Date[0] = 0x20;			//add 20xx year on it
		memcpy(gpIfsf_shm->tlg_data.tlg_dat.Current_Date+1, msg, 3);

              //TIME = HHmmss
		memcpy(gpIfsf_shm->tlg_data.tlg_dat.Current_Time, msg+3, 3);
		
		for( len = 0; strlen(buf) > RSP_HEAD_LEN + RSP_TAIL_LEN + len;){
			tank_online(1);

			//NN
			memset(s_tank, 0, sizeof(s_tank));
			HexPack(buf+RSP_HEAD_LEN+len+2+1+4, s_tank, 2);	
			
			if(strlen(buf) < RSP_HEAD_LEN+RSP_TAIL_LEN+len){
				RunLog("无效的数据包,长度为%d !",strlen(buf));
				break;
			}	
		
			/*make sigle packet mode*/
			memset(s_tank, 0, sizeof(s_tank));
			memcpy(s_tank, buf+RSP_HEAD_LEN+len, TANK_STATUS_LEN + 7 * 8);
			len = len + TANK_STATUS_LEN + 7 * 8; 

			//s_tank :
			fill_struct(s_tank);			
		}
		// CSSI.校罐数据采集
		if (gpGunShm->data_acquisition_4_TCC_adjust_flag == 1) {
			gpGunShm->data_acquisition_4_TCC_adjust_flag = 2;
		}
		gpGunShm->tr_fin_flag_tmp = 0;          // 确定记录数据之后再复位,避免缺失记录

		while (alarm_read_cnt-- <= 0) {
			alarm_read_cnt = 5;
			//取报警状态
			Ssleep(TANK_INTERVAL_ALARM);
			memset(buf, 0, buf_len);
			memset(s_tank, 0, 256);
			snprintf(s_tank, sizeof(s_tank), "yr101");
			ret = send_cmd(s_tank, buf, 5, 3);	

#if SHOW_ALARM_MSG
			RunLog("TLG Alarm Msg: %s",buf);
#endif
			
			if((ret = check_ret(buf)) != 0){
				RunLog("得到的报警状态为无效值!");
				continue;
			}

			set_status(buf);
			if (gpIfsf_shm->node_online[MAX_CHANNEL_CNT] != 0) {
				print_result();
			}
		}
	}	
}

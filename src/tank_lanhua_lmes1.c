/* 	
       兰州石化LMES1 - Tank monitor system

	Support Types:  LanHua  
	Version:		   LMES1
	This is IFSF protocol program
	2009.03.02
*/

#include "tank_pub.h"

int udp_sock;


int CommandSend(char *strCommand,  char *buffReturn);

int CheckSum(char *Source, char *sReturn);



#define MAX_RETURN_LEN		(MAX_TP_NUM*31)
#define SINGLE_DATA_LEN			31
#define TCP_HEAD_LEN			4	
#define TIMEOUT				(5000*1000)	
#define RSP_HEAD_LEN			17									
#define RSP_HEAD_DT_OFFSET		7		
#define RSP_HEAD_DT_LEN			10									
#define RSP_TAIL_LEN			7			
#define TANK_STATUS_LEN			9									
#define TANK_STATUS_LEN2		(TANK_STATUS_LEN+7*8)				
#define DELIVERY_ITEM_LEN		22						
#define DELIVERY_ITEM_LEN2	(DELIVERY_ITEM_LEN+10*8)	
static int serial_fd=0;
static unsigned char flag[31];




static int check_ret(unsigned char *buf)
{	
      int icount,len;
      unsigned char bufcrc[2];
      unsigned short  tempchar;
      len = buf[2] * 256 + buf[3] + 4;      	 
      tempchar = 0;
      for(icount =0; icount <len ; icount++){
		tempchar += buf[icount];	  				  	
      }

      bzero(bufcrc,sizeof(bufcrc));
      //AsciiToHex(&buf[len+2],bufcrc,4);
      HexPack(&buf[len+2],bufcrc,4);
      tempchar += bufcrc[0] * 256 + bufcrc[1];	  	
	  
	if (tempchar == 0)	 {
		return 0;
	}
	else{
		return -1;
	}	
}
static int open_serial(int p)
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


static int send_cmd(unsigned char *cmd, unsigned char *retbuf, int len, int delay)
{	int i, ret, size, count;
	struct timeval tv;
	fd_set ready;
	unsigned char crc;
	unsigned char recrc[2];

	tcflush(serial_fd, TCIOFLUSH);			//clear serial buffer
	tv.tv_sec = delay;						// timeout in seconds
	tv.tv_usec = 0;
	

	if(write(serial_fd,cmd,len) != len){
		__debug(2, "write command fail");
		return -1;
	}
	

	Ssleep(TANK_INTERVAL_READ);			//this is necessary for opw and Yitong
	len = 0;
	count = 0;
	while(count++ < 3){
		FD_ZERO(&ready);
		FD_SET(serial_fd, &ready);

		if(select(serial_fd + 1, &ready, NULL, NULL, &tv) > 0){
			 ret = read(serial_fd, retbuf+len, MAX_TP_NUM*72);
			 if(ret > 0){
			 	len += ret;
			 	if(retbuf[len-6] == '&'){
					       RunLog("Got: %02x%02x%02x%02x(%s)" ,retbuf[0],retbuf[1],retbuf[2],retbuf[3],&retbuf[4]);
						__debug(3, "send: %s\n return: %s\nlen:%d", cmd, retbuf,len);					
						return 0;
				}
			}
		}
	}
	
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


static void fill_struct( unsigned char *tmpbuf)
{	char tmpbuf2[256];
	char *p = tmpbuf+2+8;					//see data format
	unsigned char result[16], data[6];
	unsigned char tempdata[2];
	int i, num, tpno,k,k1;	
	int level_changed_flag = 0;
	long level1, level2;
	float f;
	unsigned char sign=0;
	const unsigned char zero_arr[6] = {0};
	static unsigned char first_flag = 0;


	memset( tmpbuf2, 0, sizeof(tmpbuf2) );

	//TT
	memcpy( tmpbuf2, tmpbuf, 2 );
	tpno = atoi( tmpbuf2 )-1;							//get tank number
	if(tpno < 0 || tpno >30) return ;
	if (flag[tpno] == 0 && gpIfsf_shm->auth_flag == DEFAULT_AUTH_MODE) {
		if (first_flag == 0) {
			sleep(12);  // for test
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
		if(f<0 && i == 5){			//only temperature can be nagetive
			sign = 0x80;
			f = f*(-1);
		}

		if(f > 0){
			memset(tmpbuf2, 0, sizeof(tmpbuf2));	
			sprintf(tmpbuf2, "%.3f", f);
			parse_data(tmpbuf2, data);
		}
		 switch(i+1) {
			case 1:			// total Observed volume				
				gpIfsf_shm->tlg_data.tp_dat[tpno].Total_Observed_Volume[0] = 0x09;
				memcpy(gpIfsf_shm->tlg_data.tp_dat[tpno].Total_Observed_Volume+1, data, 6);
#if 0
					RunLog("\n");			
					RunLog("油水总体积\n");
					for(k1=0;k1<7;k1++) RunLog("%02x ",gpIfsf_shm->tlg_data.tp_dat[tpno].Total_Observed_Volume[k1]);
#endif
				

#if 0
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
#endif
				break;

//			case 2: 		//observed volume // TC == G.S.V  
			
//				gpIfsf_shm->tlg_data.tp_dat[tpno].Gross_Standard_Volume[0] = 0x09;
//				memcpy(gpIfsf_shm->tlg_data.tp_dat[tpno].Gross_Standard_Volume+1, data, 6);
#if 0
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
#endif
//				break;
			case 4:			//fuel level
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
			case 5:			//water level
				memcpy(gpIfsf_shm->tlg_data.tp_dat[tpno].Water_Level, data + 2, 4);
				break;
			case 6:			//temperature
				gpIfsf_shm->tlg_data.tp_dat[tpno].Average_Temp[0] = sign | 0x02;
				bzero(tempdata,sizeof(tempdata));
				tempdata[0] = (data[3]) << 4  | ((data[4]) >> 4);
				tempdata[1] = (data[4]) << 4  | ((data[5]) >> 4);
				//RunLog(">>>>>>>>>TEMPDATA = %s",Asc2Hex(tempdata, sizeof(tempdata)));
				//memcpy(gpIfsf_shm->tlg_data.tp_dat[tpno].Average_Temp + 1, data + 3, 2);
				memcpy(gpIfsf_shm->tlg_data.tp_dat[tpno].Average_Temp + 1, tempdata , 2);
				break;
                     case 7:                //water volume 
				// [油净体积 =  油水总体积 - 水体积]
#if 0
					RunLog("\n");			
					RunLog("水体积\n");                                  
					for(k1=0;k1<6;k1++) RunLog("%02x ",data[k1]);
#endif	                             
				gpIfsf_shm->tlg_data.tp_dat[tpno].Gross_Standard_Volume[0] = 0x09;
				BBcdMinus(gpIfsf_shm->tlg_data.tp_dat[tpno].Total_Observed_Volume + 2, 5, \
					data + 1, 5 ,gpIfsf_shm->tlg_data.tp_dat[tpno].Gross_Standard_Volume+2);
				gpIfsf_shm->tlg_data.tp_dat[tpno].Average_Temp[0] = sign | 0x02;
#if 0
					RunLog("\n");			
					RunLog("油净体积\n");
					for(k1=0;k1<7;k1++) RunLog("%02x ",gpIfsf_shm->tlg_data.tp_dat[tpno].Gross_Standard_Volume[k1]);
#endif

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
	unsigned char tmp[33]={0},alarm[15][2]={0},tmpno[2]={0};
	static unsigned char pre_alarm[15][2] = {0};
			
	len = strlen(pbuf);
	//if(pbuf[0] != 0x01 || pbuf[len-1] != 0x03) return ;
	pbuf += 4 + 3 + 14;

	while(*pbuf != '&'){        //  退出条件需要修改
              //tt + ssssssss		
              //罐号
		memcpy(tmpno, pbuf, 2);
		tpno = atoi(tmpno)-1;
		if(tpno < 0 || tpno >30) return ;

		gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Status = 2;

	       //状态信息(8 位)
	       pbuf += 2;		   		

			memcpy(tmp,pbuf,32);			
                     if (tmp[1] == 0x30){  //0:未连接，1：连接
			                   RunLog(">>>>>>>>>>>>>>> 探棒%d 错误,Tank Probe error",tpno + 1);
					     alarm[tpno][1] |= 0x01;					
                     }
			if (tmp[3] == 0x31){  //0:正常 , 1:探棒失效
					     alarm[tpno][1] |= 0x01;					
                     }						
                     if (tmp[5] == 0x31){  //0:正常 , 1:溢出报警（报警上上限）
					     alarm[tpno][1] |= 0x10; 
                     }

                     if (tmp[7] == 0x31){ // 0:正常 , 1油位最高液面（报警上限）
			                   alarm[tpno][1] |= 0x20; 
                     }
		 	
                     if (tmp[9] == 0x31){ // 0:正常 , 1:水位过高（水位上限）
					     alarm[tpno][0] |= 0x01; 					
                     }

                     if (tmp[11] == 0x31){ // 0:正常 , 1:液位过低（报警下下限）
					     alarm[tpno][1] |= 0x80; 		
                     }

                     if (tmp[13] == 0x31){ // 0:正常 , 1:无效液位,按探棒失效处理
					     alarm[tpno][1] |= 0x01;		
                     } 					

                     if (tmp[15] == 0x31){ // 0:正常 , 1:需要进油（报警下限）
					     alarm[tpno][1] |= 0x40; 
                     } 					

                     if (tmp[17] == 0x31){ // 0:正常 , 1:温度过低
					    // alarm[tpno][1] |= 0x40; 
                     } 			
					 
                     if (tmp[19] == 0x31){ //0:正常 , 1:泄漏测试失败
					    alarm[tpno][0] |= 0x04; 		
                     } 			

                     if (tmp[21] == 0x31){//0:正常 , 1: 定期泄漏测试失败
                     		    alarm[tpno][0] |= 0x04; 		 
			}

                     if (tmp[23] == 0x31){//0:正常 , 1:年度泄漏测试失败
                     		    alarm[tpno][0] |= 0x04; 		 
			}

                     if (tmp[25] == 0x31){//0:正常 , 1:加油机接口无效
                     		    alarm[tpno][1] |= 0x01;		
			}
			
                     if (tmp[27] == 0x31){//0:正常 , 1:加油机接口通讯错误
                     		    alarm[tpno][1] |= 0x01;		
			}

                     if (tmp[29] == 0x31){//0:正常 , 1:加油机接口交易报警
                     		    alarm[tpno][1] |= 0x01;		
			}
                     					 
               pbuf +=32;		
               if (memcmp(pre_alarm[tpno], alarm[tpno], 2) != 0) {
			ChangeTpStatAlarm(gpIfsf_shm->tlg_data.tlg_node,
				0x21+tpno, gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Status, alarm[tpno]);
			memcpy(pre_alarm[tpno], alarm[tpno], 2);
			bzero(alarm[tpno], 2);
	        }			   
		//break;  // add by jie @ 4.7
	}

}

void tank_lanhua_lmes1(int p)
{	
	int i,newsock, msgid,ret,itankno;
	int len,msglen, buf_len, lenstr, num;
	int tpno;
	unsigned char tmpbuf[MAX_CMD_LEN];
	unsigned char  msg[16], R02[13] ={0xAA,0x55,0x00,0x05,0x52,0x30,0x32,0x30,0x31,0x46,0x44,0x45,0x37};
	unsigned char  R01[13] ={0xAA,0x55,0x00,0x05,0x52,0x30,0x31,0x46,0x46,0x46,0x44,0x42,0x44},s_tank[256];
	unsigned char *buf;
	pthread_t p_id;
	struct msg vr_msg;
	unsigned char tp_num[2];
	int alarm_read_cnt = 5;
	
       RunLog("开始运行兰州仪表LMES1  控制器监听程序......");

	if(open_serial(p) < 0){
		RunLog("打开串口失败open_serial(p)  failed! ");
		exit(-1);
	}

	// 暂时屏蔽   	
	buf_len = MAX_TANK_NUM*TANK_STATUS_LEN2+RSP_HEAD_LEN+RSP_TAIL_LEN+32;
	if((buf = (char *)malloc(buf_len)) == NULL){
		return ;
	}

	
	while(1){
		while (alarm_read_cnt-- <= 0) {
			alarm_read_cnt = 5;
			Ssleep(TANK_INTERVAL_ALARM);

			//获取所有罐的报警状态
			memset(buf, 0, buf_len);	
			ret = send_cmd(R01, buf, 13, 3);	

			if(ret == 0){
				if (gpIfsf_shm->node_online[MAX_CHANNEL_CNT] == 0) {				
					tank_online(1);
					sleep(2);
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
				RunLog("得到的报警状态为无效值!");
				continue;
			}
			
			set_status(buf);
		}

		if (alarm_read_cnt != 5) {
			Ssleep(TANK_INTERVAL);
		}
			  
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

		//按罐号获取油罐信息
		for (itankno = 0;itankno < PROBER_NUM;itankno++){		
				memset(buf, 0, buf_len);
				R02[8] = 0x31 + itankno;
				R02[12] = 0x37 - itankno ;
				ret = send_cmd(R02, buf, 13, 3);
				if (ret <0) {
					RunLog(">>>>>>>>>>>>>FCC与探棒%d 通讯失败!",itankno);
				}
				
				Ssleep(TANK_INTERVAL);					
				
				if((ret = check_ret(buf)) != 0){		
					       RunLog(">>>>>>>>>>>>>校验失败");
						continue;
				}
				
             //RunLog("TLG Report: %s",Asc2Hex(buf,strlen(buf)) );

/*
       (0xAA 0x55 0x00 0x53)  （共83个字节，从R开始，到&前结束）
       R02													--3
       0x20 0x09 0x03 0x02 0x0E 0x02 0x30 (2009年03月02日14时02分 30秒)	--7*2
       01 （1号灌）		       					--2
       0007										    	--8
       油水容积									--8
    	温度修正后油水容积					--8
    	剩余容积									--8
    	油位											--8
    	水位											--8
    	温度											--8
    	水容积										--8
       &&
       CRC
*/		
				memset( tmpbuf, 0, sizeof(tmpbuf) );
				memcpy( tmpbuf, buf + 4 + 3, 14 );

	       		//DATE = CCYYMMDD
				HexPack( tmpbuf, msg, 14 ); 		
				memcpy(gpIfsf_shm->tlg_data.tlg_dat.Current_Date, msg, 4);
		              //TIME = HHmmss
				memcpy(gpIfsf_shm->tlg_data.tlg_dat.Current_Time, msg+4, 3);
		
				for(len = 0; strlen(buf+4) +4> 4 + 3 + 14 + 6 +len;){
					tank_online(1);		
		
					/*make sigle packet mode*/
					memset(s_tank, 0, sizeof(s_tank));
					memcpy(s_tank, buf+4 + 3 + 14 + len, 2 + 8 + 7*8);
					len = len + 2 + 8 + 7 * 8;
					//s_tank :TTpssssNNFFFFFFFF...
					fill_struct(s_tank);			
				}

		}

		//CSSI.校罐数据采集处理
		if (gpGunShm->data_acquisition_4_TCC_adjust_flag == 1) {
			gpGunShm->data_acquisition_4_TCC_adjust_flag = 2;
		}
		gpGunShm->tr_fin_flag_tmp = 0;          // 确定记录数据之后再复位,避免缺失记录

		if (gpIfsf_shm->node_online[MAX_CHANNEL_CNT] != 0) {
			print_result();
		}
	}
	
}

/* 				  爱国者 - Tank monitor system

	Support Types: 爱国者
	Version:		  遵循vr350r  , 只是在返回的数据包后 + 00
	                       和vr  不同的一点是当某个探棒被禁用后，上送的数据为全0 
                              报警信号有四种，分别为高油位、高高油位、低油位、高水位	                       
	This is IFSF protocol program
	2009.03.03

*/

#include "tank_pub.h"

int udp_sock;

int CommandSend(char *strCommand,  char *buffReturn);

int CheckSum(char *Source, char *sReturn);
double AsciiToDouble(char *sAsciiStr, char *sResult);


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

static int check_ret(unsigned char *buf)
{
	const static char FAIL_RES[] = { 0x01,'9', '9', '9', '9', 'F', 'F', '1', 'B', 0x00 };
	const static char LABEL[] = "&&";
	char *pChecked = NULL, *pTmp;
	int len;
	char result[5];
	
	if( memcmp( buf, FAIL_RES, sizeof(FAIL_RES)-1 ) == 0 ){
		__debug(2, "received unknown data string");
		return -1;	
	}
	
/*	pChecked = strstr( buf, LABEL );			// && is a bar 
	if( NULL == pChecked ){
		__debug(2, "receiced data invalid");
		return -1;	
		}
	len = pChecked+sizeof(LABEL) + 1 - buf;	
	pTmp = (char *)malloc( len*sizeof(char) );		
	if( NULL == pTmp ){
		__debug(2, "allocate memory error at check_ret");
		return -1;	
		}
	memset( pTmp, 0, sizeof(char)*len );
	memcpy( pTmp, buf, len-1 );
	
	memset( result, 0, sizeof(result) );
	CheckSum( pTmp, result );			//in libvrcomm
	
	if( NULL != pTmp ) free( pTmp );		
	if( memcmp( buf+len-1, result, sizeof(result)-1 ) != 0 ){
		__debug(2, "check sum fail");
		return -1;
		}
*/
	return 0;

}


static int send_cmd(unsigned char *cmd, unsigned char *retbuf, int len, int delay)
{	int i, ret, size, count;
	struct timeval tv;
	fd_set ready;
	unsigned char crc;

	tcflush(serial_fd, TCIOFLUSH);			//clear serial buffer
	tv.tv_sec = delay;						// timeout in seconds
	tv.tv_usec = 0;
	
//	__debug(2, ">>> send cmd Before send i20100, cmd: %s", cmd);
	if(write(serial_fd,cmd,len) != len){
		__debug(2, "write command fail");
		return -1;
	}
	
//	__debug(2, ">>> send cmd send i20100 ok");
	Ssleep(TANK_INTERVAL_READ);			//this is necessary for opw and Yitong
	len = 0;
	count = 0;
	while(count++ < 3){
		FD_ZERO(&ready);
		FD_SET(serial_fd, &ready);
//		__debug(2, ">>> send cmd  before select...");
		if(select(serial_fd + 1, &ready, NULL, NULL, &tv) > 0){
//			__debug(2, ">>> send cmd  reading ");
			 ret = read(serial_fd, retbuf+len, MAX_TP_NUM*72 );
			 if(ret > 0){
			 	len += ret;
			 	if(retbuf[len-8] == '&'){ //与vr协议相比，数据包后面都多加了00
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
	char *p = tmpbuf+2+1+4+2;					//see data format
	unsigned char result[16], data[6];
	unsigned char tempdata[2];
	int i, num, tpno,k,k1;	
	int level_changed_flag = 0;
	long level1, level2;
	float f;
	unsigned char sign=0;
	const unsigned char zero_arr[6] = {0};
	static unsigned char first_flag = 0;

	//tmpbuf = s_tank :TTpssssNNFFFFFFFF...;   
	memset( tmpbuf2, 0, sizeof(tmpbuf2) );

	//TT
	memcpy( tmpbuf2, tmpbuf, 2 );
	tpno = atoi( tmpbuf2 )-1;							//get tank number
	__debug(2, "%s:>>> into fill strut , tpno: %d",__FILE__, tpno);
	if(tpno < 0 || tpno >30) return ;
	if (flag[tpno] == 0 && gpIfsf_shm->auth_flag == DEFAULT_AUTH_MODE) {
		if (first_flag == 0) {
			sleep(12);  // for test
			__debug(2, "%s:>>> after sleep 12s ......", __FILE__);
		}
		//ChangeTpStatAlarm(gpIfsf_shm->tlg_data.tlg_node,0x21+tpno, 2, gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Alarm);   modify by liys
		//__debug(2, "%s:>>> after do changeTpStatAlarm, Tp_Status: %d", __FILE__, gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Status);
		flag[tpno] = 1;
	}
	
/*
	memset( tmpbuf2, 0, sizeof(tmpbuf2) );
	HexPack( tmpbuf+2+1+4, tmpbuf2, 2 );	
	num = (unsigned char)tmpbuf2[0];
*/


// P-> FFFFFFFF....
	for(i = 0; i < 7; i++){
		if(i == 2) continue;
		memset(tmpbuf2, 0, sizeof(tmpbuf2));
		memset(result, 0, sizeof(result));
		memset(data, 0, sizeof(data));
		memcpy( tmpbuf2, p+i*8, 8 );
		asc2hex(tmpbuf2, result);
		f = exp2float(result);			
		//if(TANK_TYPE == TK_OPW_SS1 && (i+1 != 6)) f = f * 10;		//convert cm -> mm
		if (( i+1 == 4) || (i+1 ==5)) f = f /10;
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
				//gpIfsf_shm->tlg_data.tp_dat[tpno].Gross_Standard_Volume[0] = 0x09;
				//memcpy(gpIfsf_shm->tlg_data.tp_dat[tpno].Gross_Standard_Volume+1, data, 6);
				
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
{	int i, len, tpno, num, prob, no,ntpno;
	unsigned char tmp[3]={0},alarm[15][2]={0};
	static unsigned char pre_alarm[15][2] = {0};

	//pbuf : <SOH>i205TTYYMMDDHHmmTTnnAA...
	//                                                    TTnnAA...&CCCC<ETX>
	
	len = strlen(pbuf);
	if(pbuf[0] != 0x01 || pbuf[len-1] != 0x03) return ;
	pbuf += RSP_HEAD_LEN;

	while(*pbuf != '&'){        //  退出条件需要修改
              //TT 
		memcpy(tmp, pbuf, 2);
		tpno = atoi(tmp)-1;
		if(tpno < 0 || tpno >30) return ;

		//nn
		pbuf += 2;
		memcpy(tmp, pbuf, 2);
		num = atoi(tmp);

	       //AA....(nn)
		pbuf += 2;		   
		gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Status = 2;
		
		for(i=0 ;i < num; i++){
			memcpy(tmp,pbuf,2);
			no = atoi(tmp);

			switch(no){

				case 2: //bit 11 set = Tank leak alarm  <----> 0202 = Tank Leak Alarm
					     alarm[tpno][0] |= 0x04; 
					     break; 
				
				case 3: //bit 9 set = High water alarm  <-----> 0203 = Tank High Water Alarm
					     alarm[tpno][0] |= 0x01; 
					     break;

				case 4: //bit 2 set = Overfill status   <---->  0204 = Tank Overfill Alarm 					
				            alarm[tpno][1] |= 0x02; 
					     break; 

				case 5: //bit 7 set = Low level alarm  <----> 0205 = Tank Low Product Alarm
					     alarm[tpno][1] |= 0x40; 
					     break; 
				     

				case 6: //bit 10 set = Tank loss alarm  <----> 0206 = Tank Sudden Loss Alarm
					     alarm[tpno][0] |= 0x02; 
					     break; 

				case 7: // bit 6 set = High level alarm <----> 0207 = Tank High Product Alarm
					     alarm[tpno][1] |= 0x20; 
					     break; 
				
				case 8: //bit 8 set = Low-Low level alarm <----> 0208 = Tank Invalid Fuel Level Alarm
					     //alarm[tpno][1] |= 0x80;  目前爱国者的overfill ，和低低油位都是8 ，所以改为探棒错误
                                        RunLog(">>>>>>>>>>>>>>> 探棒%d 错误,Tank Probe error",tpno + 1);
					     alarm[tpno][1] |= 0x01;										     
					     break; 

				case 9:  //bit 1 set = Tank Probe error  <----> 0209 = Tank Probe Out Alarm
                                        printf(">>>>>>>>>>>>>>> 探棒%d 错误,Tank Probe error",tpno + 1);
					     alarm[tpno][1] |= 0x01;					
					    //gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Status = 1;								
					     break;
						 
				case 10: //bit 9 set = High water alarm <----> 0210 = Tank High Water Warning
					     alarm[tpno][0] |= 0x01; 
					     break; 

				case 11: //bit 4 set = Supply warning  <----> 0211 = Tank Delivery Needed Warning
					     alarm[tpno][1] |= 0x08; 
					     break; 

				case 12: //bit 5 set = High-High level alarm <----> 0212 = Tank Maximum Product Alarm
					      alarm[tpno][1] |= 0x10; 
					      break; 

				case 20: 

#if 0
				// 富兰克林TLG
				case 9:  alarm[1] |= 0x10; break; // High High level
				case 4:  alarm[1] |= 0x20; break; // high product level
				case 11: alarm[1] |= 0x40; break; // low product level
				case 8:  alarm[1] |= 0x80; break; // Low low product level
				case 3:  alarm[0] |= 0x01; break; // High water alarm
#endif
#if 0
				case :  alarm[1] |= 0x20; break; 
				case :  alarm[0] |= 0x01; break; //
				case :  alarm[0] |= 0x10; break; // 
				case :  alarm[0] |= 0x80; break; // High water alarm
					                         // Tank loss alarm
				case 2: alarm[0] |= 0x04; break; // Tank leak alarm 
				case 20: 
#endif

				default: break;
			}
			pbuf += 2;
		}
		
		if (memcmp(pre_alarm[tpno], alarm[tpno], 2) != 0) {
			ChangeTpStatAlarm(gpIfsf_shm->tlg_data.tlg_node,
				0x21+tpno, gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Status, alarm[tpno]);
			memcpy(pre_alarm[tpno], alarm[tpno], 2);
			bzero(alarm[tpno], 2);
		}

		//break;  // add by jie @ 4.7
	}

}

void tank_rtl750b(int p)
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

       RunLog("开始运行爱国者协议监听程序......");

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
		if (alarm_read_cnt != 0) {
			Ssleep(TANK_INTERVAL);
		}
		/*

		暂时屏蔽
		if((msglen = msgrcv(msgid, &vr_msg, 16, 0, IPC_NOWAIT)) > 1){
			if(opw_a8(msg, msglen) > 0)
				__debug(2, "set system time success");
		}

		*/
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
		snprintf(s_tank, sizeof(s_tank), "\x01i20100");
		ret = send_cmd(s_tank, buf, 7, 3);
		
		if(ret == 0){
			if (gpIfsf_shm->node_online[MAX_CHANNEL_CNT] == 0) {				
				tank_online(1);
				sleep(WAITFORHEART);
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

#if 0
		// added by jie @ 4.7
		if (strlen(buf) == 0) {
			if (offline_cnt == 5) {
				tank_online(0); // offline
			} else {
				__debug(2, "%s: TLG Node %d Offline !", __FILE__, gpIfsf_shm->tlg_data.tlg_node);
				offline_cnt++;
			}
		}

#endif
		/* set by tank time, otherwise, use set_tlg_time to set by local time*/
              //buf : <SOH>i205TTYYMMDDHHmmTTpssssNNFFFFFFFF...
	       //                                                  TTpssssNNFFFFFFFF...&&CCCC<ETX>	       
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
		
		for(tpno = 0, len = 0; strlen(buf) > RSP_HEAD_LEN+TANK_STATUS_LEN+RSP_TAIL_LEN+len; tpno++){
			tank_online(1);

			//NN
			memset(s_tank, 0, sizeof(s_tank));
			HexPack(buf+RSP_HEAD_LEN+len+2+1+4, s_tank, 2);	
			num = (unsigned char)s_tank[0];
			if(strlen(buf) < RSP_HEAD_LEN+TANK_STATUS_LEN+RSP_TAIL_LEN+len+num*8){
				//__debug(2,"invalid data");
				RunLog("无效的数据包,长度为%d !",strlen(buf));
				break;
			}	
		
			/*make sigle packet mode*/
			memset(s_tank, 0, sizeof(s_tank));
			memcpy(s_tank, buf+RSP_HEAD_LEN+len, 2+1+4+2+num*8);
			len = len + 2+1+4+2+num*8;

			// jie @ 6.17 , LB011
#if 0
			memcpy(tp_num, s_tank, 2);
			if (tpno < (atoi(tp_num) - 1)) {   // 处理禁用并且断开的情况
				for (; tpno < (atoi(tp_num) - 1); ++tpno) {
					if (gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Status != 1) {
						gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Status = 1;
						ChangeTpStatAlarm(gpIfsf_shm->tlg_data.tlg_node,
							0x21 + tpno, gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Status, NULL);
					} 
				}
//				tpno = atoi(tp_num) - 1;
			} else if (gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Status == 1) {
				gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Status = 2;
				ChangeTpStatAlarm(gpIfsf_shm->tlg_data.tlg_node,
					0x21 + tpno, gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Status, NULL);
			}
#endif							

			//s_tank :TTpssssNNFFFFFFFF...
			fill_struct(s_tank);			
		}
		// CSSI.校罐数据采集处理
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
			snprintf(s_tank, sizeof(s_tank), "\x01i20500");
			ret = send_cmd(s_tank, buf, 7, 3);	

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

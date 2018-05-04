/* 				OPW - Tank monitor system

	Support Types: TLS-350
	Compatible OPW SS1, Yi Tong YT-A3.0, FRANKLIN
	Note: this program is only for IFSF protocol
	2007.11
	
*/

#include "tank_pub.h"


#define MAX_RETURN_LEN		(MAX_TP_NUM*31)
#define SINGLE_DATA_LEN         31
#define TCP_HEAD_LEN		4	
#define TIMEOUT			(5000*1000)	
#define RSP_HEAD_LEN		17									
#define RSP_HEAD_DT_OFFSET	7		
#define RSP_HEAD_DT_LEN		10									
#define RSP_TAIL_LEN		7			
#define TANK_STATUS_LEN		9									
#define TANK_STATUS_LEN2	(TANK_STATUS_LEN+7*8)				
#define DELIVERY_ITEM_LEN	22						
#define DELIVERY_ITEM_LEN2	(DELIVERY_ITEM_LEN+10*8)	
#define INTERVAL		3

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

static int check_ret(unsigned char *buf)
{
	const static char FAIL_RES[] = { 0x01,'9', '9', '9', '9', 'F', 'F', '1', 'B', 0x00 };
	char *pTmp;
	int len;
	char result[5];
	
	if( memcmp( buf, FAIL_RES, sizeof(FAIL_RES)-1 ) == 0 ){
		//__debug(2, "received unknown data string");
		RunLog("received unknown data string");
		return -1;	
		}
	
/*

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
			 ret = read(serial_fd, retbuf+len, MAX_TP_NUM*72);
			 if(ret > 0){
			 	len += ret;
			 	if(retbuf[len-6] == '&'){
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
	lenstr = strlen(tmpbuf2);
	ret = send_cmd(tmpbuf2, tmpbuf, lenstr, 3);

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
	static unsigned char first_flag = 0;


        unsigned char a[16];
        unsigned long b;
 	 int len;

#ifdef TANK_DEBUG
	RunLog("fillstruct is running");
#endif

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
	//	if(TANK_TYPE == 3 && (i+1 != 6)) f = f * 10;		//convert cm -> mm modify by liys 08.08.19
	       if((TANK_TYPE == TK_OPW_ISITE ||TANK_TYPE == TK_OPW_SS1) && (i == 4-1 || i ==5-1) ) f = f * 10;  //convert cm -> mm
		
		if(f<0 && i == 5){			//only temperature can be nagetive
			sign = 0x80;
			f = f*(-1);
		}

		if(f > 0){
			memset(tmpbuf2, 0, sizeof(tmpbuf2));	
			sprintf(tmpbuf2, "%.3f", f);
#ifdef TANK_DEBUG
			RunLog("tmpbuf2 is %s", tmpbuf2);
#endif
			parse_data(tmpbuf2, data);
		}
		 switch(i+1) {
			case 1:			// total Observed volume				
			       if (TANK_TYPE == TK_OPW_ISITE ||TANK_TYPE == TK_YITONG || TANK_TYPE == TK_OPW_GALAXY) {     //仪通volume  油水总体积		       
					gpIfsf_shm->tlg_data.tp_dat[tpno].Total_Observed_Volume[0] = 0x09;
					memcpy(gpIfsf_shm->tlg_data.tp_dat[tpno].Total_Observed_Volume+1, data, 6);
#ifdef TANK_DEBUG
					RunLog("\n");			
					RunLog("油水总体积\n");
					for(k1=0;k1<7;k1++) RunLog("%02x ",gpIfsf_shm->tlg_data.tp_dat[tpno].Total_Observed_Volume[k1]);
#endif

			       }
				else if (TANK_TYPE == TK_OPW_SS1 ||TANK_TYPE == TK_FRANKLIN  ){  //OPW SS1 / Flanklin  volume  油净体积
					gpIfsf_shm->tlg_data.tp_dat[tpno].Gross_Standard_Volume[0] = 0x09;
					memcpy(gpIfsf_shm->tlg_data.tp_dat[tpno].Gross_Standard_Volume+1, data, 6);
#if 0
					RunLog("\n");			
					RunLog("油净体积\n");
					for(k1=0;k1<7;k1++) RunLog("%02x ",gpIfsf_shm->tlg_data.tp_dat[tpno].Gross_Standard_Volume[k1]);
#endif					
				}
				
				break;

//			case 2: 		 tc volume  is not Dataid 42 in ifsf
//				gpIfsf_shm->tlg_data.tp_dat[tpno].Gross_Standard_Volume[0] = 0x09;
//				memcpy(gpIfsf_shm->tlg_data.tp_dat[tpno].Gross_Standard_Volume+1, data, 6);
//				break;

			case 4:			//fuel level
				memcpy(gpIfsf_shm->tlg_data.tp_dat[tpno].Product_Level, data + 2, 4);

				// CSSI.校罐项目数据采集 -- 记录液位变化数据
				if (gpGunShm->data_acquisition_4_TCC_adjust_flag != 0) {
					level1 = Bcd2Long(&gpGunShm->product_level[tpno][0], 4);
					level2 = Bcd2Long(gpIfsf_shm->tlg_data.tp_dat[tpno].Product_Level, 4);
					//TCCADLog("level_changed_flag: %d, level1:%ld, level2: %ld",\
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
					//TCCADLog("level_changed_flag: %d, level2: %ld",\
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
				memcpy(gpIfsf_shm->tlg_data.tp_dat[tpno].Average_Temp + 1, tempdata , 2);
				break;
			case 7:                //water volume 
				if (TANK_TYPE == TK_OPW_ISITE ||TANK_TYPE == TK_YITONG || TANK_TYPE == TK_OPW_GALAXY) { //仪通 [油净体积 =  油水总体积 - 水体积]
#if 0
					RunLog("\n");			
					RunLog("水体积\n");                                  
					for(k1=0;k1<6;k1++) RunLog("%02x ",data[k1]);
#endif	                             
					gpIfsf_shm->tlg_data.tp_dat[tpno].Gross_Standard_Volume[0] = 0x09;
					BBcdMinus(gpIfsf_shm->tlg_data.tp_dat[tpno].Total_Observed_Volume + 2, 5, \
						data + 1, 5 ,gpIfsf_shm->tlg_data.tp_dat[tpno].Gross_Standard_Volume+2);
					
#ifdef TANK_DEBUG
					RunLog("\n");			
					RunLog("油净体积\n");
					for(k1=0;k1<7;k1++) RunLog("%02x ",gpIfsf_shm->tlg_data.tp_dat[tpno].Gross_Standard_Volume[k1]);
#endif
				}
				else if (TANK_TYPE == TK_OPW_SS1 ||TANK_TYPE == TK_FRANKLIN ) { //OPW  SS1 / Flanklin  [ 油水总体积 =油净体积  + 水体积]
#if 0
					RunLog("\n");			
					RunLog("水体积\n");                                  
					for(k1=0;k1<6;k1++) RunLog("%02x ",data[k1]);
#endif	                             
#ifdef TANK_DEBUG
					RunLog("\n");			
					RunLog("pre 油水总体积\n");
					for(k1=0;k1<7;k1++) RunLog("%02x ",gpIfsf_shm->tlg_data.tp_dat[tpno].Total_Observed_Volume[k1]);
					RunLog("Total_Observed_Volume 's address is %p pub_tk is %p %d %d", gpIfsf_shm->tlg_data.tp_dat[tpno].Total_Observed_Volume, gpIfsf_shm, tpno, sizeof(gpIfsf_shm->tlg_data.tp_dat[0]));
					
#endif 					
					BBcdAdd(data,gpIfsf_shm->tlg_data.tp_dat[tpno].Gross_Standard_Volume+1,6,6);
#ifdef TANK_DEBUG
					RunLog("data after added by gross volume is %02x%02x%02x%02x%02x%02x",\
					data[0], data[1],data[2], data[3],data[4], data[5]);
#endif
					gpIfsf_shm->tlg_data.tp_dat[tpno].Total_Observed_Volume[0] = 0x09;
					memcpy(gpIfsf_shm->tlg_data.tp_dat[tpno].Total_Observed_Volume+1, data, 6);

#ifdef TANK_DEBUG
					RunLog("\n");			
					RunLog("油水总体积\n");
					for(k1=0;k1<7;k1++) RunLog("%02x ",gpIfsf_shm->tlg_data.tp_dat[tpno].Total_Observed_Volume[k1]);
#endif
				}
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

#if 0			
			if (TANK_TYPE == 3){    //OPW 报警 信息处理
					switch(no){

						case 2: alarm[tpno][0] |= 0x04; break; //tank leak alarm
						
						case 3: alarm[tpno][0] |= 0x01; break; //* high water alarm in ifsf = highwater  in veed  

						case 4: alarm[tpno][1] |= 0x20; break; //*  high product alarm in ifsf = overfill in OPW

						case 5: alarm[tpno][1] |= 0x80; break; //* lowlow product level in ifsf  = low product level in OPW

						case 6: alarm[tpno][0] |= 0x20; break; //Tank Loss alarm

						case 7: alarm[tpno][1] |= 0x10; break; //high high level in ifsf = max product in OPW
						
						case 8: alarm[tpno][1] |= 0x80; break; // Low low product level
						case 9: alarm[tpno][1] |= 0x01; 
		                                        printf(">>>>>>>>>>>>>>> 探棒%d 中断",tpno + 1);
							     gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Status = 1;					
						            break; //prober fail
						case 10: alarm[tpno][0] |= 0x01; break; //high water warning in OPW

					       case 11: alarm[tpno][1] |= 0x40; break; //*  Low level alarm in ifsf = delivery needed in OPW

						case 12: alarm[tpno][1] |= 0x10; break; //*  high high level in ifsf = max product in veed 

						default: break;
					}
			}
			else if (TANK_TYPE == 5 || TANK_TYPE == 7)  //仪通/ Flanklin ,报警 信息处理
			{
#endif			
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
								     alarm[tpno][1] |= 0x80; 
								     break; 

							case 9:  //bit 1 set = Tank Probe error  <----> 0209 = Tank Probe Out Alarm
			                                        RunLog(">>>>>>>>>>>>>>> 探棒%d 中断,Tank Probe error",tpno + 1);
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
							case 51:// OPW - GALAXY Tank Prober Error
			                                        RunLog(">>>>>>>>>>>>>>> 探棒%d 中断, GALAXY Tank Probe error",tpno + 1);
								     alarm[tpno][1] |= 0x01;					
								     //gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Status = 1;								
								     break;

						default: break;
					}                         

//			}
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

void tank_opw(int p)
{
	int newsock, ret, i;
	int len,  buf_len, lenstr, num;
	unsigned char tmpbuf[MAX_CMD_LEN];
	unsigned char msg[16], s_tank[256];
	unsigned char *buf;
	struct msg opw_msg;
	static offline_cnt = 0;
	int alarm_read_cnt = 5;
	int read_fail_cnt = 3;
	

	//__debug(2, "run in tank_opw");
	RunLog("开始运行OPW(Vr350)  协议监听程序......");
#ifdef TANK_DEBUG
	RunLog("tank debug is opening");
#endif
	
	if(open_opw(p) < 0){
		//__debug(2, "open serial port fail");
		RunLog("打开串口失败open_opw(p)  failed! ");
		exit(-1);
	}

/*	if((msgid = msgget(TANK_MSG_ID, 0666)) < 0){     modify by liys 目前不做较时
		//__debug(2, "link message pipe fail");
		RunLog("取消息句柄失败! msgget(TANK_MSG_ID, 0666) failed! ");
	}
*/	

	buf_len = MAX_TANK_NUM*TANK_STATUS_LEN2+RSP_HEAD_LEN+RSP_TAIL_LEN+32;
	if((buf = (char *)malloc(buf_len)) == NULL){
		RunLog("allocate memory error! ");
		return ;
	}
	
	while(1){
		if (alarm_read_cnt != 5) {
			Ssleep(TANK_INTERVAL);
		}
//		if((msglen = msgrcv(msgid, &opw_msg, 16, 0, IPC_NOWAIT)) > 1){ modify by liys 目前不做较时
//			if(opw_a8(msg, msglen) > 0)
//				RunLog("设置系统时间成功!");
//		}
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
			if (read_fail_cnt > 0) {
				read_fail_cnt--;
				continue;
			}
			tank_online(0);
			RunLog(">>>>>>>> 液位仪处于离线状态<<<<<<<");					
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
//		tank_online(1);
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
		
		/*__debug(2, "%s:strlen(buf): %d, RSP_HEAD: %d, buf: %s", __FILE__, 
			strlen(buf), RSP_HEAD_LEN+TANK_STATUS_LEN+RSP_TAIL_LEN+len, Asc2Hex(buf, sizeof(buf)));*/

		for(len = 0; strlen(buf) > RSP_HEAD_LEN+TANK_STATUS_LEN+RSP_TAIL_LEN+len;){
			tank_online(1);

			//NN
			memset(s_tank, 0, sizeof(s_tank));
			HexPack(buf+RSP_HEAD_LEN+len+2+1+4, s_tank, 2);	
			num = (unsigned char)s_tank[0];
			if(strlen(buf) < RSP_HEAD_LEN+TANK_STATUS_LEN+RSP_TAIL_LEN+len+num*8){
				//__debug(2,"invalid data");
				RunLog("无效的数据包!");
				break;
			}	
		
			/*make sigle packet mode*/
			memset(s_tank, 0, sizeof(s_tank));
			memcpy(s_tank, buf+RSP_HEAD_LEN+len, 2+1+4+2+num*8);
			len = len + 2+1+4+2+num*8;

							
			//__debug(2, "%s:before  call fill_struct ...............",__FILE__);

			//s_tank :TTpssssNNFFFFFFFF...
			fill_struct(s_tank);			
		}
		// CSSI.校罐数据采集处理
		if (gpGunShm->data_acquisition_4_TCC_adjust_flag == 1) {
			gpGunShm->data_acquisition_4_TCC_adjust_flag = 2;
		}
		gpGunShm->tr_fin_flag_tmp = 0;          // 确定读到数据之后再复位,避免缺失记录


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
				//__debug(2, "get msg from tank invalid!send: %s\n return: %s", s_tank, buf);
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


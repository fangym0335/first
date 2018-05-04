/* 				Veederroot - Tank monitor system

	Support Types: Veederroot 350R (Auto-Calibration/Recalibration)
	Version:		350R
	This is IFSF protocol program, use veederroot library
	2007.12
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

static unsigned char pre_message[MAX_PAR_NUM][MAX_TANK_NUM][8];
static unsigned char pre_point_volum[MAX_TANK_NUM][160];
static pthread_mutex_t mutex =PTHREAD_MUTEX_INITIALIZER ;

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
		//	if( gun_status[msg[2]] == CALL )
		//		continue;
			gun_status[msg[2]] = CALL;
			//snprintf( gun_str, sizeof(gun_str), "%02d", msg[2] );

			panelno = 0;
			panelno = VrPanelNo[msg[2]-1][msg[3]-1];
			
			
			//ret=NozzleLift( gun_str, 0 );
			pthread_mutex_lock(&mutex);	
			RunLog("调用发送抬抢信号函数TrxStart( %d, 0)",panelno);
			Ssleep(200000L);
			ret = TrxStart(panelno,0);
			RunLog("TrxStart Success ret = %d", ret);
			pthread_mutex_unlock(&mutex);
			//__debug(3, "send data to tank: %s",gun_str);
		}

		if(memcmp( msg, GUN_OFF, CMD_LEN ) == 0){
			memset(tmp, 0, sizeof(tmp));			/*pump total*/
			memcpy(tmp, msg+5, 5);
			total = (double)Bcd2U64(tmp,5)/100;			
			
			memset( tmp, 0, sizeof(tmp) );			/*pump volume*/
			memcpy( tmp, msg+10, 3 );		
			volume = (float)Bcd2U64(tmp,3)/100;		
			
			gun_status[msg[2]] = OFF;

			panelno = 0;
			panelno = VrPanelNo[msg[2]-1][msg[3]-1] ;

			gun_no = 0;
			gun_no = msg[4]-1 ;
					 
			//snprintf( gun_str, sizeof(gun_str), "%02d", msg[2] );

			pthread_mutex_lock(&mutex);		 
			RunLog("调用发送挂抢信号函数TrxStop(%d, %d, %.2lf, %.2lf, 0)",panelno,gun_no,total,volume);
			Ssleep(200000L);
			ret=TrxStop( panelno,gun_no, total, volume, 0 );	
			RunLog("TrxStop Success ret = %d", ret);
			pthread_mutex_unlock(&mutex);
			//__debug(3, "Adjust data:gun_str=[%s] dropped.total=[%.2lf],volume=[%.2lf]!ret=[%d]", gun_str, total, volume, ret );
			}
		
		}

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
	while(count++ < 20){	
		tv.tv_sec = delay;						// timeout in seconds
		tv.tv_usec = 0;
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
	pthread_mutex_lock(&mutex);
	ret = CommandSend(tmpbuf2, tmpbuf);
	pthread_mutex_unlock(&mutex);

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
#if 0
		if (first_flag == 0) {
			sleep(12);  // for test
			__debug(2, "%s:>>> after sleep 12s ......", __FILE__);
		}
#endif
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
		//RunLog("tmpbuf2 is %s", tmpbuf2);
		asc2hex(tmpbuf2, result);
		f = exp2float(result);			
		if(TANK_TYPE == TK_OPW_SS1 && (i+1 != 6)) f = f * 10;		//convert cm -> mm
		
		if(f<0 && i == 5){			//only temperature can be nagetive
			sign = 0x80;
			f = f*(-1);
		}

		if(f > 0){
			memset(tmpbuf2, 0, sizeof(tmpbuf2));	
			sprintf(tmpbuf2, "%.3f", f);
			parse_data(tmpbuf2, data);
		}
		//RunLog("data is %02x%02x%02x%02x%02x%02x", data[0], data[1], data[2], data[3], data[4], data[5]);
		switch (i + 1) {
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
					   data + 1, 5 ,gpIfsf_shm->tlg_data.tp_dat[tpno].Gross_Standard_Volume + 2);
				
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
					     alarm[tpno][1] |= 0x80; 
					     break; 

				case 9:  //bit 1 set = Tank Probe error  <----> 0209 = Tank Probe Out Alarm
                                        printf(">>>>>>>>>>>>>>> 探棒%d 错误, Tank Probe error",tpno + 1);
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

static unsigned int MyCheckSum(char *Source, int Length, char *sReturn, int *CheckSumLen)
{
	int i;    
	unsigned int iResult = 0;
	if (Source[0] != 0x01|| Source[strlen(Source)-1] != 0x03)
		return -1;
	
   	for (i=0; i<Length; i++)
	{
       	 iResult += Source[i];
    	}
   	 iResult = ( (~iResult) + 1 ) & 0xFFFF;

    	sprintf(sReturn, "%04X", iResult);
    
    	if ( CheckSumLen != NULL )
    	{
       	 *CheckSumLen = 4;
    	}
    
   	 return 0;
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

static int  get_msglen(const unsigned char *message)
{
	int i;
	if(message == NULL)
		return -1;
	for ( i = 0; !(message[i] == '&' && message[i+1] == '&') ; i++);
	if ( message[i + 6] != 0x03){
		RunLog("message last is %02x\n", message[i+6]);
		RunLog("You should check your message, Maybe it is not what you want \n");
		return -1;
	}
	return i;
}

static int make_data(unsigned char *tank_command, unsigned char *data, int type){
	
	float f;
	int n = 0;
	int i;
	int point_stat = 0;	
	unsigned char sign;
	unsigned char tmp_buffer[64];

	bzero(tmp_buffer, sizeof(tmp_buffer));
	
	asc2hex(tank_command, tmp_buffer);
	f = exp2float(tmp_buffer);	
	
	//RunLog("f is %f\n", f);
	if ( f < 0) {
		f = f*(-1);
		sign = 0x01;
	}
	else 
		sign = 0x00;

	bzero (tmp_buffer, sizeof(tmp_buffer));
	bzero (data, sizeof(data));
	if (type == 0)
		sprintf(tmp_buffer, "%.1f", f);
	else if(type ==2)
		sprintf(tmp_buffer, "%.2f", f);
	else if(type == 6 )
		sprintf(tmp_buffer, "%.6f", f);
	else {
		RunLog("type is invalid type");
		return -1;
	}
		
	for(i = 0; i< strlen(tmp_buffer); i++)
		if(tmp_buffer[i] == 0x2e)
			point_stat = i;
	data[0] = (unsigned char)sign << 4 |(unsigned char)point_stat;
	parse_data2(tmp_buffer, &data[1], strlen(tmp_buffer));
	return 0;
}

/**
*fill the parse data into message
*
*/
static int command_parse(int par_nu, int len,const unsigned char *buffReturn, unsigned char *message)
{	
	int ret = 0;
	int data_cnt = 0;
	int i;
	unsigned char tank_command[8];
	unsigned char data[5];
	unsigned char tank_no;
	int lent;
	lent = get_msglen(buffReturn);
	//RunLog("len is [%d] lent is [%d] tank_cnt is [%d]", len, lent, tank_cnt);
	for (i=0; i < tank_cnt; i ++) {
			
		memcpy(tank_command, buffReturn + 17 + i*10 +2, 8);
		
		tank_no = *(buffReturn + 17 + i*10+ 1) -0x30; 
		/*RunLog("pre_message[%d][%d] %02x%02x%02x%02x tank_command %02x%02x%02x%02x", par_nu, i, \
			pre_message[par_nu][i][0], pre_message[par_nu][i][1],pre_message[par_nu][i][2],pre_message[par_nu][i][3],\
			tank_command[0], tank_command[1], tank_command[2], tank_command[3] );*/
		if (memcmp(&pre_message[par_nu][i][0], tank_command, 8) != 0){
			memcpy(pre_message[par_nu][i], tank_command, 8);
			RunLog("pre_message[%d][%d] %02x%02x%02x%02x",par_nu, i,pre_message[par_nu][i][0], pre_message[par_nu][i][1],pre_message[par_nu][i][2],pre_message[par_nu][i][3]);
			if (ret == 0){
				ret = 1;
				par_cnt++;
			}
			message[len] = inttobcd(par_nu);
			message[len+2 +data_cnt * 6] = tank_no;
			if (par_nu == 1 ||par_nu == 4 )
				make_data(tank_command, data, 2);
			else if (par_nu == 2)
				make_data(tank_command, data, 6);
			memcpy(&message[len+3 +data_cnt * 6], data, 5);
			data_cnt++;
			message[len +1] = inttobcd(data_cnt);
		}
			
	}
	if (data_cnt != 0)
		len = len+3 +data_cnt*6 -1;
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
	int sockfd = 0;
	unsigned char tank_command[256];
	unsigned char buffReturn[4096];
	unsigned char message[2048];
	unsigned char data[5];
	int len, i, ret;
	int par_nu;
	int timeout = 5;
	int tryN = 10;
	int send_time, data_cnt;
	int para_cnt = 5; // 5 parameter tank configure
	char *p;
	char *portnb;
	unsigned char sReturn[5];
	int get_len;
	int return_len = 4;
	unsigned char time_interval = 3;
	int cilent_port = 9004;

	bzero(pre_message, sizeof(pre_message));
	bzero(pre_point_volum, sizeof(pre_point_volum));
	sleep(30);

	while(1){
		while(1) {
			portnb = "9002";	
			sockfd = Tank_TcpConnect(ipname, "9002", cilent_port , timeout);//sPortName,(timeout+1)/2
			RunLog( "连接数据端口[%d],sock[%d]",atoi(portnb), sockfd);
			if (sockfd > 0 ){
				RunLog("TcpConnect连接成功,TANK准备发送数据");			
				break;
			}else{
				RunLog("TcpConnect连接失败,延时2s 继续");
				sleep(2);//exit(0);					
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
			RunLog("send three times failed and run again");		
		}else{
			//RunLog("发送数据成功，等待接收%s", ) ;
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
	
	
	while(1)
	{
		if (sockfd > 0)
			close (sockfd);
		if (gpIfsf_shm->node_online[MAX_CHANNEL_CNT] == 1){
		//RunLog("tank 在线");
		
		bzero(buffReturn, sizeof(buffReturn));
		bzero(tank_command, sizeof(tank_command));
		bzero(message, sizeof(message));
		
		message[0] = 0xFC;
		par_cnt = 0;
		memcpy (&message[5], stano_bcd, 5);
		
		len = 11;
		/*tank tilt*/
		//CommandSend("\x01i60800", buffReturn);
		pthread_mutex_lock(&mutex);
		ret = CommandSend("\x01i60800", buffReturn);
		pthread_mutex_unlock(&mutex);

		
	       RunLog("倾斜度为%s", buffReturn);
		bzero(sReturn, sizeof(sReturn));
		get_len =get_msglen(buffReturn)+2;
		//RunLog("ret is %d\n", get_len);
		if (get_len == 1){
			//RunLog("get_len == 1");
		}
		else{
			MyCheckSum(buffReturn, get_len, sReturn, &return_len);

			if (memcmp(buffReturn+get_len, sReturn, 4) != 0|| buffReturn[2] != '6' || buffReturn[3] != '0' || buffReturn[4] != '8'){
				RunLog("608 校验失败");
			}
			
			else if (ret == 0){
				//RunLog("send cmd success ");
		
				/*buffreturn :<SOH>i608TTYYMMDDHHmmTTFFFFFFFF
	                                                      		       TTFFFFFFFF&&CCCC<ETX>*/

				//RunLog("length is %d", len);
				par_nu = 1;
				len = command_parse(par_nu, len, buffReturn,  message);
				//RunLog("message is %s", Asc2Hex(message, len));

			}
			else 
				RunLog("send cmd failed ret is  %d", ret);
		}
		
		bzero(buffReturn, sizeof(buffReturn));
		/*thermal expansion coefficient*/
		pthread_mutex_lock(&mutex);
		ret = CommandSend("\x01i60900", buffReturn);
		pthread_mutex_unlock(&mutex);
		bzero(sReturn, sizeof(sReturn));

		
	       RunLog("膨胀系数%s", buffReturn);
		get_len =get_msglen(buffReturn)+2;
		//RunLog("ret is %d\n", get_len);
		if (get_len == 1){
			//RunLog("get_len == 1");
		
		}
		else{
			MyCheckSum(buffReturn, get_len, sReturn, &return_len);

			if (memcmp(buffReturn+get_len, sReturn, 4) != 0|| buffReturn[2] != '6' || buffReturn[3] != '0' || buffReturn[4] != '9'){
				RunLog("609 校验失败");
				
			}
		/*buffreturn :<SOH>i609TTYYMMDDHHmmTTFFFFFFFF
	                                                      		       TTFFFFFFFF&&CCCC<ETX>*/
/*		p = "\x01i609001211261011013A54562E023AA52696033AA52696043AA52696&&F3CF\x03";
	       memcpy(buffReturn, p, get_msglen(p)+7);*/
	      		else if (ret == 0){

				par_nu = 2;
				len = command_parse(par_nu, len, buffReturn,  message);
				//RunLog("message is %s", Asc2Hex(message, len));

			}
			else 
		 		RunLog("send command failed ");
		}
		bzero(buffReturn, sizeof(buffReturn));	
		
		pthread_mutex_lock(&mutex);
		CommandSend("\x01i50E00", buffReturn);	
		pthread_mutex_unlock(&mutex);
	       RunLog("temperature compensation is %s", buffReturn);
		
		bzero(sReturn, sizeof(sReturn));
		get_len =get_msglen(buffReturn)+2;
		if (get_len !=1){
			MyCheckSum(buffReturn, get_len, sReturn, &return_len);

			if (memcmp(buffReturn+get_len, sReturn, 4) != 0 || buffReturn[2] != '5' || buffReturn[3] != '0' || buffReturn[4] != 'E'){
				RunLog("50e 校验失败");
			}
			else{
				ret = 0;
				data_cnt = 0;
				par_nu = 3;
				for (i=0; i < tank_cnt; i ++) {
			
					memcpy(tank_command, buffReturn + 17 , 8);
					//RunLog("tank command is %s", tank_command);
					if (memcmp(pre_message[par_nu][i], tank_command, 8) != 0){
						memcpy(pre_message[par_nu][i], tank_command, 8);
						if (ret == 0){
							ret = 1;
							par_cnt++;
						}
						message[len] = inttobcd(par_nu);
						message[len+2 +data_cnt * 6] = inttobcd(i+1);
				
						make_data(tank_command, data, 2);
						memcpy(&message[len+3 +data_cnt * 6], data, 5);
						data_cnt++;
						message[len +1] = inttobcd(data_cnt);
					}
			
				}		
				if (data_cnt != 0)
					len = len+3 +data_cnt*6 -1;
			}
		}
		/*tank diameter*/
		bzero(buffReturn, sizeof(buffReturn));			
		//CommandSend("\x01i60700", buffReturn);
		
		pthread_mutex_lock(&mutex);
		ret = CommandSend("\x01i60700", buffReturn);
		pthread_mutex_unlock(&mutex);
		RunLog("tank diameter is %s", buffReturn);
		
		bzero(sReturn, sizeof(sReturn));
		get_len =get_msglen(buffReturn)+2;

		if (get_len != 1){
			MyCheckSum(buffReturn, get_len, sReturn, &return_len);

			if (memcmp(buffReturn+get_len, sReturn, 4) != 0|| buffReturn[2] != '6' || buffReturn[3] != '0' || buffReturn[4] != '7'){
				RunLog("607 校验失败");
			}
		
		/*buffreturn :<SOH>i50ETTYYMMDDHHmmTTFFFFFFFF
											TTFFFFFFFF&&CCCC<ETX>*/
			else if (ret == 0){
	       		//RunLog("tank diameter %s", buffReturn);
				par_nu = 4;
				len = command_parse(par_nu, len, buffReturn,  message);
				//RunLog("message is %s", Asc2Hex(message, len));
			}
			else 
				RunLog("tank diameter failed ");
		}
		/*tank 20 point*/
		/*buffreturn :<SOH>i606TTYYMMDDHHmmTTFFFFFFFF...
											TTFFFFFFFF...&&CCCC<ETX>*/		

		bzero(buffReturn, sizeof(buffReturn));	
		
		pthread_mutex_lock(&mutex);
		CommandSend("\x01i60600", buffReturn);
		pthread_mutex_unlock(&mutex);
		RunLog(" 20 point %s", buffReturn);
		
		bzero(sReturn, sizeof(sReturn));
		get_len =get_msglen(buffReturn)+2;

		if (get_len != 1){
			MyCheckSum(buffReturn, get_len, sReturn, &return_len);

			if (memcmp(buffReturn+get_len, sReturn, 4) != 0|| buffReturn[2] != '6' || buffReturn[3] != '0' || buffReturn[4] != '6'){
				RunLog("606 校验失败");
			}
			else{
	      	 		//RunLog("tank 20 point %s", buffReturn);
				int j =0;
				ret = 0;
				data_cnt = 0;
				par_nu = 5;
				//RunLog("tank cnt is %d", tank_cnt);
				for (i=0; i < tank_cnt; i ++) {
					bzero(tank_command, sizeof(tank_command));
					memcpy(tank_command, buffReturn+ 17 + i*162 +2, 160);
					if (tank_command[0] == 0x00+0x30){
						sleep(10);
						continue;
					}
					if (strlen(tank_command) < 160){
						sleep(10);
						continue;

					}
					RunLog("tank Command is %s pre point volumn is %s", tank_command, pre_point_volum[i]);
					if (memcmp(pre_point_volum[i], tank_command, 160) != 0){
						memcpy(pre_point_volum[i], tank_command, 160);
						if (ret == 0){
							ret = 1;
							par_cnt++;
						}
						message[len] = inttobcd(par_nu);
						message[len+2 +data_cnt * 101] = *(buffReturn +17 +i*162 +1) -0x30;
						for(j = 0; j < 20; j++){
							//RunLog("j = %d\n", j);
							make_data(&tank_command[j*8], data, 0);
							memcpy(&message[len+3 +data_cnt * 101+j*5], data, 5);
						}
				
						data_cnt++;
						message[len+1 ] = inttobcd(data_cnt);
					}
			
				}		
				if (data_cnt !=  0)
					len = len+3 +data_cnt*101 - 1;

			}
		}
		message[10] = inttobcd(par_cnt);
	
		/*  printf the message*/
		int dd= len -5;
		char *d= (char *)&dd;
		message[1] = *(d+3);
		message[2] = *(d+2);
		message[3] = *(d+1);
		message[4] = *d;
		RunLog("TANK CONFIG MESSAGE %s", Asc2Hex(message, len));
		
		if(par_cnt != 0){
			send_time = 0;
			while(1) {
				portnb = "9002";
			
				//RunLog("ipname is %s", ipname);
				sockfd = Tank_TcpConnect(ipname, "9002", cilent_port+1 , timeout);//sPortName,(timeout+1)/2
				RunLog( "连接数据端口[%d],sock[%d]",atoi(portnb), sockfd);
				if (sockfd > 0 ){
					//RunLog("TcpConnect连接成功,TANK准备发送数据");			
					break;
				}else{
					RunLog("TcpConnect连接失败,延时2s  继续");
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
				//RunLog("发送数据成功，等待接收") ;
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
					}
				}
			}
			if (sockfd > 0)
				close (sockfd);

		}
		sleep(time_interval *60);
		/*judge whether send online message once a day*/
		}
		sleep(5);
	}
	
}
void tank_vr350(int p)
{	
	int i,newsock, msgid,ret;
	int len,msglen, buf_len, lenstr, num;
	int tpno;
	unsigned char tmpbuf[MAX_CMD_LEN];
	unsigned char  msg[16], s_tank[256];
	unsigned char *buf;
	pthread_t p_id , p_id_config=0;
 	int first_getin = 0;
	struct msg vr_msg;
	unsigned char tp_num[2];
	int alarm_read_cnt = 5;
	int sendcnt = 0;
	//__debug(2, "run in tank_vr350");

       RunLog("开始运行VR  350R 协议监听程序......");
	gpGunShm->LiquidAdjust = 1;  //校准标志设置为1

/*	if(open_opw(p) < 0){
		//__debug(2, "open serial port fail");
		RunLog("打开串口失败open_opw(p)  failed! ");
		exit(-1);
	}
*/

	// 暂时屏蔽   
	if(VRCommInitiate() != 0){
		__debug(2, "init vr serial port fail");
		exit(-1);
	}

      /*
	if((msgid = msgget(TANK_MSG_ID, 0666)) < 0){
		__debug(2, "link message pipe fail");
	}
     */	

	ret = pthread_create( &p_id, NULL, (void *) vr_adjust, NULL );
	if(ret != 0){
		RunLog("create vr adjust thread fail");
		exit(-1);
	}
		
	
	buf_len = MAX_TANK_NUM*TANK_STATUS_LEN2+RSP_HEAD_LEN+RSP_TAIL_LEN+32;
	if((buf = (char *)malloc(buf_len)) == NULL){
		//__debug(2, "allocate memory error");
		return ;
	}

       tank_init_config(ipname, stano_bcd, &globle_config, &tank_cnt);
       RunLog("first getin is %d globle_config is %d tankcnt is %d", first_getin, globle_config, tank_cnt);
       if (!first_getin && globle_config){
          
              ret = pthread_create( &p_id_config, NULL, (void *) read_config, NULL );
              if(ret != 0){
                        RunLog( "create read config thread fail");
                        exit(-1);
              }
              first_getin = 1;
    
       }


	while(1){
		if (globle_config && 0 != pthread_kill(p_id_config, 0))
		{
			
			RunLog("TcpPosServ 线程退出，重启 %d", p_id_config);
			pthread_cancel(p_id_config); 
			sleep(3);			
			ret = pthread_create( &p_id_config, NULL, (void *) read_config, NULL );
	           	if(ret != 0){
	                   	RunLog( "create read config thread fail");
	                   	exit(-1);
	           	}
		}
		if (0 != pthread_kill(p_id, 0)) {
			RunLog("校罐 线程退出，重启 %d", p_id);
			pthread_cancel(p_id); 
			sleep(3);			
			ret = pthread_create( &p_id, NULL, (void *) vr_adjust, NULL );
			if(ret != 0){
				RunLog("create vr adjust thread fail");
				exit(-1);
			}		
		}
		
		if (alarm_read_cnt != 5) {
			Ssleep(TANK_INTERVAL * 4); //读取液位仪数据过快，影响校罐数据发送
		}
		/* 暂时屏蔽
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
		//ret = send_cmd(s_tank, buf, 7, 3);
		
		pthread_mutex_lock(&mutex);
		Ssleep(TANK_INTERVAL );
		ret = CommandSend(s_tank,buf);
		Ssleep(TANK_INTERVAL );
		pthread_mutex_unlock(&mutex);
		
		if(ret == 0){
			if (gpIfsf_shm->node_online[MAX_CHANNEL_CNT] == 0) {				
				tank_online(1);
				sendcnt = 0;
				sleep(WAITFORHEART);
				RunLog(">>>>>>>> 液位仪处于联线状态<<<<<<<");	
				RunLog(">>>>>>>> 探棒的数量: %d",PROBER_NUM);
				for (i = 0; i < PROBER_NUM; i++) {
					ChangeTpStatAlarm(gpIfsf_shm->tlg_data.tlg_node, 0x21 + i, 2, gpIfsf_shm->tlg_data.tp_dat[i].TP_Alarm);
				}
			}
		} 
		else if (sendcnt++ <= 3){
			sleep(6);
			continue;
		}
		else {
			sendcnt = 0;
			tank_online(0);
			RunLog(">>>>>>>> 液位仪处于离线状态, CommandSend 返回值= %d <<<<<<<", ret );
			continue;
		}

		if((ret = check_ret(buf)) != 0){			
			continue;
		}
		

              //RunLog("取油罐信息的命令: %s",s_tank);
		RunLog("TLG Report: %s %02x", buf, buf[0]);
		if (buf[0] != 0x01) {
			sleep(2);
			continue;
		}
		unsigned char sReturn[5];
		int get_len;
		int return_len = 4;
		bzero(sReturn, sizeof(sReturn));
		get_len =get_msglen(buf)+2;
		if (get_len == 1){
			RunLog("get_len == 1");
			continue;
		}
		else{
			MyCheckSum(buf, get_len, sReturn, &return_len);

			if (memcmp(buf+get_len, sReturn, 4) != 0 || buf[2] != '2' || buf[3] != '0' || buf[4] != '1'){
				RunLog("201 校验失败");
				continue;
			}

		}
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
			//__debug(2, "%s:before  call fill_struct ...............",__FILE__);

			//s_tank :TTpssssNNFFFFFFFF...

			fill_struct(s_tank);			
		}
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
			//ret = send_cmd(s_tank, buf, 7, 3);	
			
			RunLog("DDDDDDD");
			pthread_mutex_lock(&mutex);
			Ssleep(TANK_INTERVAL );
			ret = CommandSend(s_tank,buf);
			Ssleep(TANK_INTERVAL );
			pthread_mutex_unlock(&mutex);
			RunLog("DDDDDDD");
			//RunLog("取报警状态的命令: %s",s_tank);
#if SHOW_ALARM_MSG
			RunLog("TLG Alarm Msg: %s",buf);
#endif
	//		RunLog("*** 取到报警信息***");
			
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




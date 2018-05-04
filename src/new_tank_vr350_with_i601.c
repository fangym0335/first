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
static int CheckTPCfg(unsigned char *buf, int buf_len);

#define MAX_RETURN_LEN		(MAX_TP_NUM*31)
#define SINGLE_DATA_LEN		31
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
	sleep(1);								//this is necessary for opw and Yitong
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
	ret = CommandSend(tmpbuf2, tmpbuf);

	if((ret = check_ret(tmpbuf)) != 0) return -1;
	
	return ret;
}

static void fill_struct( unsigned char *tmpbuf)
{	char tmpbuf2[256];
	char *p = tmpbuf+2+1+4+2;					//see data format
	unsigned char result[16], data[6];
	unsigned char tempdata[2];
	int i, num, tpno,k;	
	float f;
	unsigned char sign=0;
	const unsigned char zero_arr[6] = {0};
	static unsigned char first_flag = 0;

	//tmpbuf = s_tank :TTpssssNNFFFFFFFF...;   
	memset( tmpbuf2, 0, sizeof(tmpbuf2) );

	//TT
	memcpy( tmpbuf2, tmpbuf, 2 );
	tpno = atoi( tmpbuf2 )-1;					//get tank number
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
	for(i = 0; i < 6; i++){
		if(i == 2) continue;
		memset(tmpbuf2, 0, sizeof(tmpbuf2));
		memset(result, 0, sizeof(result));
		memset(data, 0, sizeof(data));
		memcpy( tmpbuf2, p+i*8, 8 );
		asc2hex(tmpbuf2, result);
		f = exp2float(result);			
		if(TANK_TYPE == 3 && (i+1 != 6)) f = f * 10;		//convert cm -> mm
		
		if(f<0 && i == 5){			//only temperature can be nagetive
			sign = 0x10;
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

				break;
			case 2: 		//observed volume // TC == G.S.V  
			//	gpIfsf_shm->tlg_data.tp_dat[tpno].Total_Observed_Volume[0] = 0x09;
			//	memcpy(gpIfsf_shm->tlg_data.tp_dat[tpno].Total_Observed_Volume+1, data, 6);
				gpIfsf_shm->tlg_data.tp_dat[tpno].Gross_Standard_Volume[0] = 0x09;
				memcpy(gpIfsf_shm->tlg_data.tp_dat[tpno].Gross_Standard_Volume+1, data, 6);
				break;
			case 4:			//fuel level
				memcpy(gpIfsf_shm->tlg_data.tp_dat[tpno].Product_Level, data + 2, 4);
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

			default:
				break;			
		}


	}


}

static void set_status(unsigned char *pbuf)
{	int i, len, tpno, num, prob, no;
	unsigned char tmp[3]={0},alarm[15][2]={0};
	static unsigned char pre_alarm[15][2] = {0};

	//pbuf : <SOH>i205TTYYMMDDHHmmTTnnAA.....&CCCC<ETX>
	
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
//		gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Status = 2;
		// #######
//		RunLog("tpno: %d, change TP_Status to : %d", tpno, gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Status);
		
		for(i=0 ;i < num; i++){
			memcpy(tmp,pbuf,2);
			no = atoi(tmp);
			//__debug(3, "tpno=%d num=%d error no=%d",tpno,num,no);
			switch(no){
				case 2: alarm[tpno][0] |= 0x04; break; //tank leak alarm
				
				case 3: alarm[tpno][0] |= 0x01; break; //* high water alarm in ifsf = highwater  in veed  

				case 4: alarm[tpno][1] |= 0x20; break; //*  high product alarm in ifsf = overfill in veed
				//  alarm[1] |= 0x10; break; //High High level , added by jie @ 4.7

				case 5: //alarm[1] |= 0x40; break; //   mdofiy by liys @ 5.6
				     alarm[tpno][1] |= 0x80; break; //* lowlow product level in ifsf = low product level in VR

				case 6: alarm[tpno][0] |= 0x20; break; //Tank Loss alarm
				case 7: alarm[tpno][1] |= 0x20; break; //high product alarm
				
				case 8: alarm[tpno][1] |= 0x80; break; // Low low product level
				case 9: alarm[tpno][1] |= 0x01; 
                                        printf(">>>>>>>>>>>>>>> 探棒%d 中断",tpno + 1);
					gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Status = 1;					
					break; //prober fail
				case 10: alarm[tpno][0] |= 0x01; break; //high water warning

				//case 11: alarm[1] |= 0x08; break; //delivery error , original   modify by liys @ 5.6
			       case 11: alarm[tpno][1] |= 0x40; break; //*  Low level alarm in ifsf = delivery needed in veed

				case 12: alarm[tpno][1] |= 0x10; break; //*  high high level in ifsf = max product in veed 

				case 20: 

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

void tank_vr350(int p)
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
	static const unsigned char func_code_i201[4] = {'i', '2', '0', '1'};
	static const unsigned char func_code_i205[4] = {'i', '2', '0', '5'};
	static int check_cnt = 0;

	RunLog("开始运行VR  350R 协议监听程序......");

	// 暂时屏蔽   
	if(VRCommInitiate() != 0){
		__debug(2, "init vr serial port fail");
		exit(-1);
	}

	ret = pthread_create( &p_id, NULL, (void *) vr_adjust, NULL );
	if(ret != 0){
		__debug(2, "create vr adjust thread fail");
		exit(-1);
	}
		
	
	buf_len = MAX_TANK_NUM*TANK_STATUS_LEN2+RSP_HEAD_LEN+RSP_TAIL_LEN+32;
	if((buf = (char *)malloc(buf_len)) == NULL){
		RunLog("TLG allocate memory failed");
		return;
	}

#if 1
	// 初始化探棒状态 , Operative/Inoperative
	do {
		ret = CheckTPCfg(buf, buf_len);
	} while (ret < 0);  // 返回数据有误, 则重试, 若只是没有返回数据则继续
#endif
	
	while (1) {
		sleep(5);


		memset(buf, 0, buf_len);
		memset(s_tank, 0, 256);

		// ## Step.1 取油罐信息, fill_struct
		snprintf(s_tank, sizeof(s_tank), "\x01i20100");
		ret = CommandSend(s_tank, buf);
		
		if(ret == 0){
			if (gpIfsf_shm->node_online[MAX_CHANNEL_CNT] == 0) {				
				tank_online(1);

				sleep(12);
				RunLog(">>>>>>>> 液位仪处于联线状态<<<<<<<");	
				RunLog(">>>>>>>> 探棒的数量: %d", PROBER_NUM);
				for (i = 0; i < PROBER_NUM; i++) {
					ChangeTpStatAlarm(gpIfsf_shm->tlg_data.tlg_node, 0x21 + i, gpIfsf_shm->tlg_data.tp_dat[i].TP_Status, NULL);
				}
			}
		} else {			
			tank_online(0);
			for (i = 0; i < PROBER_NUM; i++) {
				gpIfsf_shm->tlg_data.tp_dat[i].TP_Status = 1;   // --> Inoperative
			}
			RunLog(">>>>>>>> 液位仪处于离线状态, CommandSend 返回值= %d <<<<<<<", ret );
			continue;
		}


		if((ret = check_ret(buf)) != 0){			
			continue;
		}
		
		RunLog("TLG Report: %s", buf);

		if (memcmp(func_code_i201, buf + 1, 4) != 0) {
			RunLog("Function Code Error!, %02x, %02x %02x, %02x", buf[1], buf[2], buf[3], buf[4]);
			continue;
		}
		/* set by tank time, otherwise, use set_tlg_time to set by local time*/

		// buf : <SOH>i205TTYYMMDDHHmmTTpssssNNFFFFFFFF.....&&CCCC<ETX>
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
		
		for (tpno = 0, len = 0; strlen(buf) > RSP_HEAD_LEN+TANK_STATUS_LEN+RSP_TAIL_LEN+len; tpno++){
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
#if 0                   // jie @ 6.18
			
			memcpy(tp_num, s_tank, 2);
			if (tpno < (atoi(tp_num) - 1)) {   // 跳过没有配置的探棒
				for (; tpno < (atoi(tp_num) - 1); ++tpno) {
					if (gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Status != 1) {
						gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Status = 1;
						ChangeTpStatAlarm(gpIfsf_shm->tlg_data.tlg_node,
							0x21 + tpno, gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Status, NULL);
					}
				}
			} else if (gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Status == 1) {
				gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Status = 2;
				ChangeTpStatAlarm(gpIfsf_shm->tlg_data.tlg_node,
					0x21 + tpno, gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Status, NULL);
			}
#endif
							

			//s_tank :TTpssssNNFFFFFFFF...
			fill_struct(s_tank);			
		}


		// ## Step.3 取报警状态 , set_status
		sleep(5);
		memset(buf, 0, buf_len);
		memset(s_tank, 0, 256);
		snprintf(s_tank, sizeof(s_tank), "\x01i20500");
		ret = CommandSend(s_tank, buf);

		RunLog("TLG Alarm Msg: %s", buf);
		
		if((ret = check_ret(buf)) != 0){
			//__debug(2, "get msg from tank invalid!send: %s\n return: %s", s_tank, buf);
			RunLog("得到的报警状态为无效值!");
			continue;
		}

		if (memcmp(func_code_i205, buf + 1, 4) != 0) {
			RunLog("Function Code Error!, %02x, %02x %02x, %02x", buf[1], buf[2], buf[3], buf[4]);
			continue;
		}

		set_status(buf);
#if DEBUG
		if (gpIfsf_shm->node_online[MAX_CHANNEL_CNT] != 0) {
			print_result();
		}
#endif

		// ## Step.3 update TP_Status per 60'
		if (++check_cnt >= 60) {
			check_cnt = 0;
			sleep(10);
			CheckTPCfg(buf, buf_len);
		}
	}
	
}

/*
 * func: 检查探棒的配置情况, 更新TP_Status
 * ret: 0 - 成功; -1 - 内存分配失败 ; -2 - 数据错误, 1 - 读数失败
 */

//static int CheckTPCfg();
static int CheckTPCfg(unsigned char * buf, int buf_len) 
{
#define TP_NUM 10    // 国内不会超过10个罐
#define BUF_LEN ((TP_NUM * 3) + RSP_HEAD_LEN + RSP_TAIL_LEN + 8)
#define TEST_VR 0

	unsigned char  s_tank[16] = {0};
	unsigned char *p = NULL;
	static const unsigned char func_code_i601[4] = {'i', '6', '0', '1'};
	int i, tpno, ret;
#if TEST_VR
	unsigned char *buf = NULL;
	int buf_len;
	buf_len = MAX_TANK_NUM*TANK_STATUS_LEN2+RSP_HEAD_LEN+RSP_TAIL_LEN+32;

	if((buf = (char *)malloc(buf_len)) == NULL){
		RunLog("allocate memory failed!");
		return -1;
	}

#endif
	bzero(buf, buf_len);
	snprintf(s_tank, sizeof(s_tank), "\x01i60100");
	ret = CommandSend(s_tank, buf);
	if (ret != 0) {
		RunLog("Inquire Tank Configuration Failed");
#if TEST_VR
		free(buf);
#endif
		return 1;
	}

	if (memcmp(func_code_i601, buf + 1, 4) != 0) {
		RunLog("Function Code Error!, %02x, %02x %02x, %02x", buf[1], buf[2], buf[3], buf[4]);
#if TEST_VR
		free(buf);
#endif
		return -1;
	}

	if (0 != check_ret(buf)) {
		RunLog("Get Msg from tank invalid, Msg: %s", buf);
#if TEST_VR
		free(buf);
#endif
		return -2;
	}

	RunLog("Got Tank Configuration: %s", buf);

	p = buf + RSP_HEAD_LEN;  // point to TT
	for (; p[0] != '&'; ) {
		// p[0][1]: Tank Number; p[2]: Tank Configuration Flag.
		tpno = ((p[0] & 0x0F) << 4) | (p[1] & 0x0F);
		if (tpno == 0) {
			if ('1' == p[2]) {  // all tank Operative 
				for (i = 0; i < PROBER_NUM; i++) {
					if (gpIfsf_shm->tlg_data.tp_dat[i].TP_Status != 2) {
						RunLog("%d# 探棒处于启用状态", i + 1);
						gpIfsf_shm->tlg_data.tp_dat[i].TP_Status = 2;
						ChangeTpStatAlarm(gpIfsf_shm->tlg_data.tlg_node, 0x21 + i,
								gpIfsf_shm->tlg_data.tp_dat[i].TP_Status, NULL);
					}
				}
			} else {            // all tank Inoperative
				for (i = 0; i < PROBER_NUM; i++) {
					if (gpIfsf_shm->tlg_data.tp_dat[i].TP_Status != 1) {
						RunLog("%d# 探棒处于禁用状态", tpno + 1);
						gpIfsf_shm->tlg_data.tp_dat[i].TP_Status = 1;
						ChangeTpStatAlarm(gpIfsf_shm->tlg_data.tlg_node, 0x21 + i,
								gpIfsf_shm->tlg_data.tp_dat[i].TP_Status, NULL);
					}
				}
			}

			p += 3;
		} 

		tpno--;   // --> array index
		if ('1' == p[2]) { 
			if (gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Status != 2) {
				RunLog("%d# 探棒处于启用状态", tpno + 1);
				gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Status = 2;   // --> Operative
				ChangeTpStatAlarm(gpIfsf_shm->tlg_data.tlg_node, 0x21 + tpno,
						gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Status, NULL);
			}
		} else {
			if (gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Status != 1) {
				RunLog("%d# 探棒处于禁用状态", tpno + 1);
				gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Status = 1;   // --> Inoperative
				ChangeTpStatAlarm(gpIfsf_shm->tlg_data.tlg_node, 0x21 + tpno,
						gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Status, NULL);
			}
		}

		p += 3;  // next TP
	}

#if TEST_VR
	free(buf);
#endif

	return 0;
}

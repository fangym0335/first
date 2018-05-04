/* 				
                   OPW - Tank monitor system

	Support Types: OPW - PV4
	Compatible: OPW Site Sentinel
	Note: this program is only for IFSF protocol
	2009.3
	OPW-PV4 只有高油位、高水位、低油位三种报警
	
*/

#include "tank_pub.h"
#include <pthread.h>


#define MAX_RETURN_LEN		(MAX_TP_NUM*31)
#define SINGLE_DATA_LEN            31
#define TCP_HEAD_LEN		      4	
#define TIMEOUT			(5000*1000)	
#define RSP_HEAD_LEN		       17									
#define RSP_HEAD_DT_OFFSET	7		
#define RSP_HEAD_DT_LEN		10									
#define RSP_TAIL_LEN		       7			
#define TANK_STATUS_LEN		9									
#define TANK_STATUS_LEN2	(TANK_STATUS_LEN+7*8)				
#define DELIVERY_ITEM_LEN	22						
#define DELIVERY_ITEM_LEN2	(DELIVERY_ITEM_LEN+10*8)	
#define INTERVAL		3

static int serial_fd=0;
static unsigned char flag[31];
static unsigned char GunMask[64];
static unsigned char token = 0x30;
static unsigned char portlock = 0 ,portlock2=0;
//pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


static unsigned char check_ret(unsigned char *buf,int len)
{	
       int i;
	unsigned char x=0;
	
	for(i=0;i<len;i++) x ^= buf[i];
	return x;
}

static int send_cmd(unsigned char *cmd, unsigned char *retbuf, int len, int delay)
{	int i, ret, size, count;
	struct timeval tv;
	fd_set ready;
	unsigned char crc;
	
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
				if  ((check_ret(retbuf,strlen(retbuf)) == 0)||(retbuf[0] ==0x06)){
					if (retbuf[0] == 0x06) RunLog(">>>>>>>>>>>>>>retbuf = %02x",retbuf[0]);
					return 0;
				}
				else
				{
				       RunLog(">>>>>>>>>>>>>>>crc check error!");
					return -1;
				}					
			}
		}
	}
	
	return -1;
}


static int open_opw_pv4(int p)
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
	termios_new.c_cflag |= CS7;	     /* PV4 ACR only */
	/*parity checking */
	termios_new.c_cflag |= PARENB;      /* parity enable */
	termios_new.c_cflag &= ~PARODD;
	/*1 stop bit */
	termios_new.c_cflag &= ~CSTOPB;
	/*no start bit */
	termios_new.c_cflag |= CLOCAL;
	termios_new.c_cflag |= CREAD;        //make port can read
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


static void opw_polling(void)
{     
       int icount,pollingcnt,ret;
       unsigned char buf[10];
	unsigned char pingcmd[256];
	pollingcnt = 0;

       while(1){
	    	
            while(pollingcnt <= 80){				
		  pollingcnt +=1;
	         Ssleep(TANK_INTERVAL);
            }	      
	      			
	      portlock2 =1;
	      pingcmd[0] = 0x02;
	      pingcmd[1] = 0x50;
	      for (icount=0;icount <gpIfsf_shm->gunCnt;icount++){
		  	pingcmd[2+icount] = GunMask[icount];
	      }
	      pingcmd[2+icount]=0x03;
	      pingcmd[2+icount+1]= check_ret(pingcmd,2+icount+1);
	      memset(buf, 0, 10);	  
	      

//	      if(pthread_mutex_lock(&mutex)!=0) {	  
                if (portlock ==1) 
		  	continue; 
		  RunLog(">>>>>>>>>>>>>向液位仪发送Polling 命令:%s",Asc2Hex(pingcmd,2+icount+1+1));		
	         ret = send_cmd(pingcmd, buf, 2+icount+1+1, 3);
//		} 
//	      pthread_mutex_unlock(&mutex);	  
	      if ((ret <0) || (buf[0] != 0x06)){
    	             RunLog(">>>>>>>>>>>>>向液位仪发送Polling 命令失败!");
	      }	
	      pollingcnt =0;
	      portlock2 =0;

	       
	}   	
}

static void opw_adjust(void)
{
	int clen, ret,icount,i;
	unsigned char msg[MAX_RECORD_MSG_LEN];
	unsigned char GunOnCmd[256],GunOffCmd[256],GunData[256];
	int msglen,indtyp;
	unsigned char gun_status[MAX_GUN_CNT];
	unsigned char buf[10],buf_gun_no[2],buf_volume[7],buf_total[9];
	int panelno,gun_no;
	unsigned char tmp[5];
	long long total,  volume;
	
	
	struct sockaddr_in caddr;
	unsigned long long test;	
	
	memset(msg, 0, sizeof(msg));
	while(1){
		indtyp = 1;
		msglen = sizeof(msg);
		ret = RecvMsg(TANK_MSG_ID, msg, &msglen, &indtyp, TIME_FOR_RECV_CMD); 
		if (ret < 0){
			continue;
		}								
		
		if(memcmp(msg, GUN_CALL, CMD_LEN ) == 0 || memcmp(msg, GUN_BUSY, CMD_LEN ) == 0){
			portlock = 1;
			gun_no = OpwGunNo[msg[2]-1][msg[3]-1][msg[4]-1];			
			if( gun_status[gun_no] == CALL )
				continue;
			gun_status[gun_no] = CALL;

			bzero(GunOnCmd,sizeof(GunOnCmd));
			GunOnCmd[0] = 0x02;
			GunOnCmd[1] = 0x50;
			for (icount=0;icount<gun_no -1;icount++){
				GunOnCmd[2 + icount] = 0x4F;  
			}
			GunOnCmd[2+icount] = 0x50;
			GunMask[icount] =0x4F;

			GunOnCmd[2+icount+1] = 0x03;
			GunOnCmd[2+icount+2] = check_ret(GunOnCmd,(2+icount+2));			
                     for(i=0; i<3 ;i++){
 				RunLog(">>>>>>>>>>>>>发送抬枪信号:%s",Asc2Hex(GunOnCmd,2+icount+3));
				bzero(buf,sizeof(buf));					 	
				send_cmd(GunOnCmd, buf, 2+icount+3, 3);			
	       	       if (buf[0] != 0x06){
    	              	    RunLog(">>>>>>>>>>>>>发送抬枪信号失败! %02x",buf[0]);
				    continue;
	              	}
	                     break;						 
			}
			 portlock = 0;	  
		}

		if(memcmp( msg, GUN_OFF, CMD_LEN ) == 0){
			portlock = 1;
			gun_no = OpwGunNo[msg[2]-1][msg[3]-1][msg[4]-1];			
			if( gun_status[gun_no] == OFF )
				continue;
			gun_status[gun_no] = OFF;
			
			memset(tmp, 0, sizeof(tmp));			/*pump total*/
			memcpy(tmp, msg+5, 5);
			total = (double)Bcd2U64(tmp,5);			
			
			memset( tmp, 0, sizeof(tmp) );			/*pump volume*/
			memcpy( tmp, msg+10, 3 );		
			volume = (float)Bcd2U64(tmp,3);		


                     //发送最终形成的加油量
                     bzero(GunData,sizeof(GunData));
			GunData[0] = 0x02;
			GunData[1] = 0x56;

                     bzero(buf_gun_no,sizeof(buf_gun_no));
			sprintf(buf_gun_no,"%02d",gun_no);
			memcpy(&GunData[2],buf_gun_no,2);

		       GunData[4] = 0x30;

			GunData[5] = 0x30;
			GunData[6] = 0x30;
			GunData[7] = 0x30;
			GunData[8] = 0x30;

			GunData[9] = 0x4c;

		       GunData[10] = 0x30;
			GunData[11] = 0x30;
			GunData[12] = 0x30;

                     bzero(buf_volume,sizeof(buf_volume));
                     sprintf(buf_volume,"%07d",volume*10);
                     memcpy(&GunData[13],buf_volume,7);					 

			memset(&GunData[20],	0x30,7+1);
			
			
                    RunLog(">>>>>>>>>>>>>>>>>>>>token = %02x",token); 
			if (token > 0x3F)  
				token = 0x30;
			GunData[28] = token;
			token += 1;
			
                     bzero(buf_total,sizeof(buf_total));
                     sprintf(buf_total,"%09d",total);
                     memcpy(&GunData[29],buf_total,9);		

                     memset(&GunData[38],	0x30,9);
                      
	              GunData[47] = 0x03;
                     GunData[48] = check_ret(GunData,48);	
                     for(i=0; i<3 ;i++){					 	
	                     RunLog(">>>>>>>>>>发送加油数据:%s",Asc2Hex(GunData,49));
       	              bzero(buf,sizeof(buf));		
	                     send_cmd(GunData, buf, 49, 3);

				if (buf[0] != 0x06){
    	       	           RunLog(">>>>>>>>>>>>>发送加油数据失败!");
				    continue;
	              	}				
				break;
                     }
					 
                     Ssleep(TANK_INTERVAL);					 

                     //发送挂枪信号
                     
			bzero(GunOffCmd,sizeof(GunOffCmd));
			GunOffCmd[0] = 0x02;
			GunOffCmd[1] = 0x50;
			for (icount=0;icount<gun_no -1;icount++){
				GunOffCmd[2 + icount] = 0x4F;  
			}
			GunOffCmd[2+icount] = 0x40;
			GunMask[icount] =0x40;

			GunOffCmd[2+icount+1] = 0x03;
			GunOffCmd[2+icount+2] = check_ret(GunOffCmd,2+icount+2);	

			for(i=0; i<3 ;i++){
				RunLog(">>>>>>>>>>>>>发送挂枪信号:%s",Asc2Hex(GunOffCmd,2+icount+3));
				bzero(buf,sizeof(buf));
				send_cmd(GunOffCmd, buf, 2+icount+3, 3);
			
		              if (buf[0] != 0x06){
    		                  RunLog(">>>>>>>>>>>>>发送挂枪信号失败!");
				    continue;
	       	       }			  
			       break;
			}
			portlock = 0;
			}		
		}
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
	char *p = tmpbuf;					//see data format
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
 
	memset( tmpbuf2, 0, sizeof(tmpbuf2) );

	//NN TANK_NO
	memcpy( tmpbuf2, tmpbuf, 2 );
	tpno = atoi( tmpbuf2 )-1;		

	//get tank number
	if(tpno < 0 || tpno >30) return ;
	
	if (flag[tpno] == 0 && gpIfsf_shm->auth_flag == DEFAULT_AUTH_MODE) {
		if (first_flag == 0) {
			sleep(12);  // for test
			__debug(2, "%s:>>> after sleep 12s ......", __FILE__);
		}
		flag[tpno] = 1;
	}
	
       //油高
       memset(tmpbuf2, 0, sizeof(tmpbuf2));
       memcpy(tmpbuf2,p+5,7);
       f = atoi(tmpbuf2) /100;
       if(f > 0){
			memset(tmpbuf2, 0, sizeof(tmpbuf2));	
			sprintf(tmpbuf2, "%.3f", f);
			parse_data(tmpbuf2, data);
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
		} else if (level2 >= PRODUCT_LEVEL_INTERVAL || level2 <= (-1 * PRODUCT_LEVEL_INTERVAL)) {
			level_changed_flag = 1;
		}
		TCCADLog("level_changed_flag: %d, level2: %ld",
					level_changed_flag, level2);
	}
	// End of CSSI.校罐项目数据采集

	//水高
       memset(tmpbuf2, 0, sizeof(tmpbuf2));
       memcpy(tmpbuf2,p+5+7,7);
       f = atoi(tmpbuf2) / 100;
       if(f > 0){
			memset(tmpbuf2, 0, sizeof(tmpbuf2));	
			sprintf(tmpbuf2, "%.3f", f);
			parse_data(tmpbuf2, data);
	}
	memcpy(gpIfsf_shm->tlg_data.tp_dat[tpno].Water_Level, data + 2, 4);

	//油水总体积
       memset(tmpbuf2, 0, sizeof(tmpbuf2));
       memcpy(tmpbuf2,p+5+7+7,5);   
       f = atoi(tmpbuf2) ;
       if(f > 0){
			memset(tmpbuf2, 0, sizeof(tmpbuf2));	
			sprintf(tmpbuf2, "%.3f", f);
			parse_data(tmpbuf2, data);
	}
	gpIfsf_shm->tlg_data.tp_dat[tpno].Total_Observed_Volume[0] = 0x09;
	memcpy(gpIfsf_shm->tlg_data.tp_dat[tpno].Total_Observed_Volume+1, data, 6);

      //温度
       memset(tmpbuf2, 0, sizeof(tmpbuf2));
       memcpy(tmpbuf2,p+5+7+7+5+4+5,4);

       f = atoi(tmpbuf2) ;
	if (f <0){
		sign = 0x80;
		f = f * (-1);
	}
	   
       if(f > 0){
			memset(tmpbuf2, 0, sizeof(tmpbuf2));	
			sprintf(tmpbuf2, "%.3f", f);
			parse_data(tmpbuf2, data);
	}
	gpIfsf_shm->tlg_data.tp_dat[tpno].Average_Temp[0] = sign | 0x02;
	bzero(tempdata,sizeof(tempdata));
	tempdata[0] = data[3];
	tempdata[1] = data[4];			
	memcpy(gpIfsf_shm->tlg_data.tp_dat[tpno].Average_Temp + 1, tempdata , 2);

	//油净体积
       memset(tmpbuf2, 0, sizeof(tmpbuf2));
       memcpy(tmpbuf2,p+5+7+7+5+4+5+4+3,5);
       f = atoi(tmpbuf2) ;
       if(f > 0){
			memset(tmpbuf2, 0, sizeof(tmpbuf2));	
			sprintf(tmpbuf2, "%.3f", f);
			parse_data(tmpbuf2, data);
	}
	gpIfsf_shm->tlg_data.tp_dat[tpno].Gross_Standard_Volume[0] = 0x09;
       memcpy(gpIfsf_shm->tlg_data.tp_dat[tpno].Gross_Standard_Volume+1, data, 6);
	   
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
{	
       int i, len, tpno, num, prob, no;
	unsigned char tmp[3]={0},alarm[15][2]={0};
	static unsigned char pre_alarm[15][2] = {0};
	
       //TT 
	memcpy(tmp, pbuf, 2);
	tpno = atoi(tmp)-1;
	if(tpno < 0 || tpno >30) return ;

	pbuf += 2;

       //High level alarm <----> Tank High Product Alarm
	if ((pbuf[0] & 0x04) == 0x04)
		alarm[tpno][1] |= 0x20;

	//High water alarm  <-----> Tank High Water Alarm	
	if ((pbuf[0] & 0x02) == 0x02)
		alarm[tpno][0] |= 0x01;

	// Low level alarm  <----> Tank Low Product Alarm
       if ((pbuf[0] & 0x01) == 0x01)
 	       alarm[tpno][1] |= 0x40; 

	if (pbuf[1] == '1' ){
		RunLog(">>>>>>>>>>>>>>> 探棒%d 中断,Tank Probe error",tpno + 1);
		alarm[tpno][1] |= 0x01;	
	}
 			
	if (memcmp(pre_alarm[tpno], alarm[tpno], 2) != 0) {
			ChangeTpStatAlarm(gpIfsf_shm->tlg_data.tlg_node,
				0x21+tpno, gpIfsf_shm->tlg_data.tp_dat[tpno].TP_Status, alarm[tpno]);
			memcpy(pre_alarm[tpno], alarm[tpno], 2);
			bzero(alarm[tpno], 2);
	}	

}

void tank_opw_pv4(int p)
{
	int newsock, ret, i;
	int len,  buf_len, lenstr, num,icount;
	unsigned char tmpbuf[MAX_CMD_LEN];
	unsigned char msg[16], s_tank[256];
       unsigned char cmd1[6] ={0x02,0x6c,0x30,0x31,0x03,0x6c};
	unsigned char *buf;
	struct msg opw_msg;
	pthread_t p_id,p_id1;
	static offline_cnt = 0;
	unsigned char pingcmd[256];
	
	RunLog("开始运行OPW(Site Sentinel PV4)  协议监听程序......");

       gpGunShm->LiquidAdjust = 1;  //校准标志设置为1
	   
       memset(GunMask,sizeof(GunMask),0x40);	   	   	   
	if(open_opw_pv4(p) < 0){
		RunLog("打开串口失败open_opw_pv4(p)  failed! ");
		exit(-1);
	}
	//pthread_mutex_init(&mutex,NULL);

	ret = pthread_create( &p_id, NULL, (void *) opw_adjust, NULL );
	if(ret != 0){
	      RunLog(">>>>>>>>>>>>create opw adjust thread fail");
	      return;
	}

	ret = pthread_create( &p_id1, NULL, (void *) opw_polling, NULL );
	if(ret != 0){
	      RunLog(">>>>>>>>>>>>create opw polling thread fail");
	      return;
	}


	buf_len = MAX_TANK_NUM*TANK_STATUS_LEN2+RSP_HEAD_LEN+RSP_TAIL_LEN+32;
	if((buf = (char *)malloc(buf_len)) == NULL){
		RunLog("allocate memory error! ");
		return;
	}
	
	while(1){
		Ssleep(TANK_INTERVAL);
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

              for (icount = 0;icount <PROBER_NUM;icount++){
		       cmd1[3] = icount + 0x31;
		       cmd1[5] = check_ret(cmd1,5);		
		       //取油罐信息
		  //     if (pthread_mutex_trylock(&mutex) !=0)  continue;
		       if (portlock == 1||portlock2==1) continue;
		       ret = send_cmd(cmd1, buf, 6, 3);	
		 //	pthread_mutex_unlock(&mutex);   
			
		 //      ret = -1;
                     //RunLog(">>>>>>发送取油罐信息的命令: %s ",Asc2Hex(cmd1,6));
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
			          RunLog(">>>>>>>> 液位仪处于离线状态<<<<<<<");					
			          continue;
		       }

		       if((ret = check_ret(buf,strlen(buf))) != 0){			
			       continue;
		       }
	      	

                     RunLog("第%d 号罐的数据: %s",icount+1, buf);
		
  		       for(len = 0; strlen(buf) > 3 + 2+len;){
			      tank_online(1);
			      memset(s_tank, 0, sizeof(s_tank));						
			      if(strlen(buf) < 3  + 5 +7 + 7 + 5 + 4 + 5 + 4 + 3 + 5 + 5 + 1 + 2){
				   RunLog("无效的数据包!");
				   break;
			      }	
		
			       /*make sigle packet mode*/
			      memset(s_tank, 0, sizeof(s_tank));
			      memcpy(s_tank, buf+3+len,5 +7 + 7 + 5 + 4 + 5 + 4 + 3 + 5 + 5 + 1);
			      fill_struct(s_tank);		

			      memset(s_tank, 0, sizeof(s_tank));
			      memcpy(s_tank,buf+3+len,5);
			      set_status(s_tank);			
			      len = len + 5 +7 + 7 + 5 + 4 + 5 + 4 + 3 + 5 + 5 + 1;
		       }
	      }
		// CSSI.校罐数据采集处理
		if (gpGunShm->data_acquisition_4_TCC_adjust_flag == 1) {
			gpGunShm->data_acquisition_4_TCC_adjust_flag = 2;
		}
		gpGunShm->tr_fin_flag_tmp = 0;          // 确定记录数据之后再复位,避免缺失记录

	}
	
}


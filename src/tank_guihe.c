/*
 * Filename: Tank_guihe.c
 *
 * Description: 液位仪通讯模块—青岛贵和
 *
 * 2008.12 by Yongsheng Li <Liys@css-intelligent.com>
 *
 */

#include "tank_pub.h"
#include "pub.h"

#define MAX_RETURN_LEN		(10*(3+42+2)) 
#define SINGLE_DATA_LEN	42

static int serial_fd = 0;
static int tp_num = 0;

static unsigned char portlock = 0; 
static int iport = 0;

static int send_cmd(unsigned char *cmd, unsigned char *retbuf, int len, int delay)
{	int i, ret, size, count=0;
	struct timeval tv;
	fd_set ready;
	unsigned short crc;
	unsigned char recrc[2];

	tcflush (serial_fd, TCIOFLUSH);			//clear serial buffer
	tv.tv_sec = delay;						// timeout in seconds
	tv.tv_usec = 0;

	if(write(serial_fd,cmd,len) != len){
		RunLog("write command fail");
		return -1;
	}
	
	Ssleep(TANK_INTERVAL_READ);			//this is necessary for opw and Yitong
	len = 0;
	while(count++ < 3){
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
				
				//get_crc(retbuf, len-2, recrc);
				
				crc = CRC_16_guihe(retbuf,len-2);
				recrc[0] =crc & 0x00FF;
				recrc[1] = crc >> 8;
				
			 	if(strncmp(retbuf+len-2, recrc, 2) == 0){
					RunLog("Got: %s" ,Asc2Hex(retbuf,len));
					return 0;
				}else
				{
				       RunLog("Crc Error: %s" ,Asc2Hex(retbuf,len));
					return -1;
				}
				
			}
		}
	}
	for(i=0;i<len;i++) printf("%02x ",retbuf[i]); printf("\n");

	return -1;
}


static int open_guihe(int p)
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

static void guihe_adjust(void)
{
	int clen, ret;
	unsigned char msg[MAX_RECORD_MSG_LEN],GunOnMsg[20],GunOffMsg[20],retbuf[30];
	int msglen,indtyp,trynum;
	unsigned char gun_status[MAX_GUN_CNT];
	int panelno,gun_no;
	unsigned short crc;
	unsigned char tmp[5],tmpvol[8],tmpvol1[4];
	double total;
	float  volume;
	struct sockaddr_in caddr;
	unsigned long long test;
	int   *pt_a =   (int   *)&volume  ;
	
	
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
			if( gun_status[msg[2]] == CALL )
				continue;
			gun_status[msg[2]] = CALL;

			panelno = 0;
			panelno = OpwGunNo[msg[2]-1][msg[3]-1][msg[4]-1];
				                     

                     bzero(GunOnMsg,sizeof(GunOnMsg));											
			GunOnMsg[0] = 0x01;
			GunOnMsg[1] = 0x10;
			GunOnMsg[2] = 0x00;
			GunOnMsg[3] = 0x04;
			GunOnMsg[4] = 0x00;
			GunOnMsg[5] = 0x02;
			GunOnMsg[6] = 0x04;
			GunOnMsg[7] = 0x00;
			GunOnMsg[8] = msg[2];
			GunOnMsg[9] = 0x00;
			GunOnMsg[10] = 0x00;
			crc = CRC_16_guihe(&GunOnMsg[0],11);				
			GunOnMsg[11] = crc & 0x00FF; 
			GunOnMsg[12] = crc >> 8;
			
			for (trynum = 0; trynum <3; trynum++){				
	                     RunLog("++++++++++++++>调用发送抬抢信号: %s",Asc2Hex(GunOnMsg, 13));					 
				bzero(retbuf,sizeof(retbuf));			
				if (send_cmd(GunOnMsg,retbuf,13,5) <0) {
					RunLog("发送抬抢信号失败");	
					open_guihe(iport);
					continue;
				}		
				break;
			}
			Ssleep(TANK_INTERVAL);
			portlock = 0;
			
		}

		if(memcmp( msg, GUN_OFF, CMD_LEN ) == 0){
			portlock = 1;
			memset(tmp, 0, sizeof(tmp));			/*pump total*/
			memcpy(tmp, msg+5, 5);
			total = (double)Bcd2U64(tmp,5)/100;			
			
			memset( tmp, 0, sizeof(tmp) );			/*pump volume*/
			memcpy( tmp, msg+10, 3 );		
			volume = (float)Bcd2U64(tmp,3)/100;			
			
			gun_status[msg[2]] = OFF;

			panelno = 0;
			panelno = msg[2] ;

			gun_no = 0;
			gun_no = msg[3] ;					 

			RunLog("调用发送挂抢信号函数TrxStop(%d, %d, %.2lf, %.2lf, 0)",panelno,gun_no,total,volume);

			// 01  10  00 06   00 03  06  00 01        EB 85 42 20            2E 2A
				
                     bzero(GunOffMsg,sizeof(GunOffMsg));											
			GunOffMsg[0] = 0x01;
			GunOffMsg[1] = 0x10;
			GunOffMsg[2] = 0x00;
			GunOffMsg[3] = 0x06;
			GunOffMsg[4] = 0x00;
			GunOffMsg[5] = 0x03;
			GunOffMsg[6] = 0x06;
			GunOffMsg[7] = 0x00;
			GunOffMsg[8] = msg[2];

#if 0
                     float a的IEEE754格式数为：0x41c40000
                     pt_a = 00 00 c4 41
#endif 

                     sprintf(tmpvol,"%x",*pt_a);
			HexPack(tmpvol,tmpvol1,4);
			GunOffMsg[9] = tmpvol1[2];
			GunOffMsg[10] = tmpvol1[3];
			GunOffMsg[11] = tmpvol1[0];
			GunOffMsg[12] = tmpvol1[1];
			crc = CRC_16_guihe(&GunOffMsg[0],13);				
			GunOffMsg[13] = crc & 0x00FF;      			   
   		       GunOffMsg[14] = crc >> 8;

				
			for (trynum = 0; trynum <3; trynum++){
				RunLog("++++++++++++++>发送挂抢信号: %s",Asc2Hex(GunOffMsg, 15));
	                     if (send_cmd(GunOffMsg,retbuf,15,5) <0) {
					RunLog("发送挂抢信号失败");		
					open_guihe(iport);
					continue;
				}				
				break;
			}					 
			//ret=TrxStop( panelno,gun_no, total, volume, 0 );	
			Ssleep(TANK_INTERVAL);
			portlock = 0;
			}		
		}
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

static int guihe_update(void) 		// just update data base
{	int ret = 0;
       int n, i, j, k,icount;
	char tmpbuf[MAX_RETURN_LEN],tmpbuf1[MAX_RETURN_LEN], buf[8],buf1[8];
	unsigned char buf2[SINGLE_DATA_LEN],buf3[11], data[6], tmp[30];
	unsigned char bufdata[4];
	unsigned char sign=0;
	int lenstr;
	int level_changed_flag = 0;
	long level1, level2;
	float f, tov;
       static unsigned char pre_alarm[15][2] = {0};
       unsigned char alarm[15][2] = {0};
 	unsigned char tempdata[2];	
       unsigned short crc;

	   
       //油罐数量
	if(PROBER_NUM < 0 || PROBER_NUM > 31){
		   RunLog("FCC 上油罐数量配置有误，当前油罐数量: %d",tp_num);
		   return -1;
	}

       /* 
          发送读取命令
       
          [设备地址] [功能码] [地址高8位] [地址低8位] [数据量高8位] [数据量低8位] [CRC校验的高8位] [CRC校验的低8位]

          例：[01]      [03]     [00][63]     [00][15]     [CRC高][CRC低] 

       */       

	buf[0] = 0x01;
	buf[1] = 0x03;
	buf[2] = 0x00;
	buf[3] = 0x63;
	buf[4] = 0x00;
	buf[5] = 0x15;
	buf[6] = 0x74;
	buf[7] = 0x1B;
	
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

       for (icount = 0; icount < PROBER_NUM; icount++) {              
	   	         //RunLog(">>>>>>>>>>>>>>>>>>>>icount = %d,prober_num = %d ",icount,PROBER_NUM);
                       Long2Hex((99 + icount * 21),&buf[2],2);	
			  crc = CRC_16_guihe(&buf[0],6);	
 		         buf[6] = crc & 0x00FF; 
		         buf[7] = crc >> 8;
                      
		         
		         memset(tmpbuf,0,sizeof(tmpbuf));   

		         //RunLog(">>>>>>>>>>>>>>>>>>>>>>发送读取罐存信息命令: %s",Asc2Hex(buf, 8));
			  /*3 +  SINGLE_DATA_LEN  + 2 */
		         if (portlock == 1) continue;
	                if(send_cmd(buf, tmpbuf, 8  , 4) < 0){
		             RunLog("发送读取罐存信息命令失败!");
		             return -1;
	                }
					
/*
      读取命令的响应

      [设备地址] [功能码] [数据量] [数据1] 。。。[数据N] [CRC校验的高8位] [CRC校验的低8位]

      例如: [01]   [03]   [04]    [00][01][00][80]    [CRC高][CRC低] 


      数据项:                    42 个字节

      油罐号                      2
      油罐状态                2
      油品类型                2
      油高                           4
      水高                           4
      温度                           4
      油水总体积          4
      油体积                     4
      水体积                     4
      可卸量                     4
      数据上传日期     8	     
      

*/


  	                if (tmpbuf[2] < SINGLE_DATA_LEN ) {
                          RunLog("返回的罐存信息长度小于一个罐所需的长度，为不合法数据");
	                   return -1;
	                }	


                        /*	    上传日期???
 	                    memset(buf2, 0, SINGLE_DATA_LEN);
 	                    Hex2Bcd(tmpbuf+3, 2, buf2, 2);			//year
	                    Hex2Bcd(tmpbuf+5, 1, buf2+2, 1);		//month
	                    Hex2Bcd(tmpbuf+6, 1, buf2+3, 1);		//day
	                    memcpy(gpIfsf_shm->tlg_data.tlg_dat.Current_Date, buf2, 4);

                            //上传时间
	                    memset(buf2, 0, SINGLE_DATA_LEN);
	                    Hex2Bcd(tmpbuf+7, 1, buf2, 1);			//hour
	                    Hex2Bcd(tmpbuf+8, 1, buf2+1, 1);		//minute
	                    Hex2Bcd(tmpbuf+9, 1, buf2+2, 1);		//second
	                    memcpy(gpIfsf_shm->tlg_data.tlg_dat.Current_Time, buf2, 3);
                       */

		         memset(buf2, 0, SINGLE_DATA_LEN);
		         memcpy(buf2, tmpbuf + 3 , SINGLE_DATA_LEN);	

		         n = buf2[1] - 1;		
                       if ((buf2[3] & 0x80) == 0x80){//设备处于启用状态
			        if (gpIfsf_shm->tlg_data.tp_dat[n].TP_Status != 2) {				
				      gpIfsf_shm->tlg_data.tp_dat[n].TP_Status = 2;   //operative
				      if (gpIfsf_shm->node_online[MAX_CHANNEL_CNT] == 0) {
				  	   continue;
				      }
 				     ChangeTpStatAlarm(gpIfsf_shm->tlg_data.tlg_node,
					  0x21 + n, gpIfsf_shm->tlg_data.tp_dat[n].TP_Status, alarm[n]);				    	  
 			       }
                       } else  {  //设备处于停用状态
			       if (gpIfsf_shm->tlg_data.tp_dat[n].TP_Status == 2) {				
				     gpIfsf_shm->tlg_data.tp_dat[n].TP_Status = 1;   //Inoperative
				     ChangeTpStatAlarm(gpIfsf_shm->tlg_data.tlg_node,
						0x21 + n, gpIfsf_shm->tlg_data.tp_dat[n].TP_Status, alarm[n]);
			       }
			       RunLog("第%d  个探棒处于停用状态, ", n + 1);  						
		         } 
	
		         for(i = 0; i < 7; i++){
				 bufdata[2] = buf2[(4 * i) + 6];
				 bufdata[3] = buf2[(4 * i) + 7];
				 bufdata[0] = buf2[(4 * i) + 8];
				 bufdata[1] = buf2[(4 * i) + 9];		
				  
			        f = exp2float(bufdata);	     //should check valid or not?


                            memset(data, 0, 6);
				if(f<0 && i == 5){			//only temperature can be nagetive
					sign = 0x80;
					f = f*(-1);
				}
                            
				if(f > 0){
					memset(tmp, 0, sizeof(tmp));	
					sprintf(tmp, "%.3f", f);
					parse_data(tmp, data);
				}
				
			        switch(i){
				   case 0:                           //油高
					      memcpy(gpIfsf_shm->tlg_data.tp_dat[n].Product_Level, data + 2, 4);

					// CSSI.校罐项目数据采集 -- 记录液位变化数据
					if (gpGunShm->data_acquisition_4_TCC_adjust_flag != 0) {
						level1 = Bcd2Long(&gpGunShm->product_level[n][0], 4);
						level2 = Bcd2Long(gpIfsf_shm->tlg_data.tp_dat[n].Product_Level, 4);
						TCCADLog("level_changed_flag: %d, level1:%ld, level2: %ld",
									level_changed_flag, level1, level2);
						level2 -= level1;
						unsigned char tmp_level_mm = gpIfsf_shm->tlg_data.tp_dat[n].Product_Level[2] >> 4;
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
					
				   case 1:                         //水高
					     memcpy(gpIfsf_shm->tlg_data.tp_dat[n].Water_Level, data + 2, 4); 
				   break;

				   case 2:                          //温度
				            gpIfsf_shm->tlg_data.tp_dat[n].Average_Temp[0] = sign | 0x02;
					     bzero(tempdata,sizeof(tempdata));
					     tempdata[0] = (data[3]) << 4  | ((data[4]) >> 4);
					     tempdata[1] = (data[4]) << 4  | ((data[5]) >> 4);
					   //  tempdata[0] = data[4];
					   //  tempdata[1] = data[5];
					     memcpy(gpIfsf_shm->tlg_data.tp_dat[n].Average_Temp+1, tempdata, 2);   					      	 
				   break;		
					
				   case 3:                        //油水总体积					
					     gpIfsf_shm->tlg_data.tp_dat[n].Total_Observed_Volume[0] = 0x09;
					     memcpy(gpIfsf_shm->tlg_data.tp_dat[n].Total_Observed_Volume+1, data, 6);
			          break;
				
				   case 4:                        //油净体积		
				 	     gpIfsf_shm->tlg_data.tp_dat[n].Gross_Standard_Volume[0] = 0x09;
					     memcpy(gpIfsf_shm->tlg_data.tp_dat[n].Gross_Standard_Volume + 1, data, 6); 
				   break;
				
  				   default: break;

				}
			}	

			// CSSI.校罐项目数据采集
			if (gpGunShm->data_acquisition_4_TCC_adjust_flag == 1) {
				// 采集服务刚生效或者程序重启时写入一条零液位记录
				write_tcc_adjust_data(NULL, "%d,0,,,,,,0,0,0,0", n + 1);
			}
			if (level_changed_flag != 0 || gpGunShm->tr_fin_flag_tmp != 0) {
				write_tcc_adjust_data(NULL, "%d,%02x%02x%01x.%01x%02x,,,,,,0,%02x.%02x,0,0",
							n + 1, gpIfsf_shm->tlg_data.tp_dat[n].Product_Level[0],
							gpIfsf_shm->tlg_data.tp_dat[n].Product_Level[1],
							(gpIfsf_shm->tlg_data.tp_dat[n].Product_Level[2] >> 4),
							(gpIfsf_shm->tlg_data.tp_dat[n].Product_Level[2] & 0x0F),
							gpIfsf_shm->tlg_data.tp_dat[n].Product_Level[3],
							gpIfsf_shm->tlg_data.tp_dat[n].Average_Temp[1],
							gpIfsf_shm->tlg_data.tp_dat[n].Average_Temp[2]);

				TCCADLog("TCC-AD: %d #罐 写液位记录, %02x%02x%01x.%01x%02x (orig:%02x%02x%02x%02x)",
							n + 1, gpIfsf_shm->tlg_data.tp_dat[n].Product_Level[0],
							gpIfsf_shm->tlg_data.tp_dat[n].Product_Level[1],
							(gpIfsf_shm->tlg_data.tp_dat[n].Product_Level[2] >> 4),
							(gpIfsf_shm->tlg_data.tp_dat[n].Product_Level[2] & 0x0F),
							gpIfsf_shm->tlg_data.tp_dat[n].Product_Level[3],
							gpIfsf_shm->tlg_data.tp_dat[n].Product_Level[0],
							gpIfsf_shm->tlg_data.tp_dat[n].Product_Level[1],
							gpIfsf_shm->tlg_data.tp_dat[n].Product_Level[2],
							gpIfsf_shm->tlg_data.tp_dat[n].Product_Level[3]);
				memcpy(&gpGunShm->product_level[n][0], gpIfsf_shm->tlg_data.tp_dat[n].Product_Level, 4);
				level_changed_flag = 0;
			}
 				        	       	 
	}

	// CSSI.校罐数据采集处理
	if (gpGunShm->data_acquisition_4_TCC_adjust_flag == 1) {
		gpGunShm->data_acquisition_4_TCC_adjust_flag = 2;
	}
	gpGunShm->tr_fin_flag_tmp = 0;          // 确定记录数据之后再复位,避免缺失记录

      		
      Ssleep(TANK_INTERVAL);	  
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

          发送读取油罐报警信息命令
       
          [设备地址] [功能码] [地址高8位] [地址低8位] [数据量高8位] [数据量低8位] [CRC校验的高8位] [CRC校验的低8位]

          例：[01]      [03]     [01][F3]     [00][06]     [CRC高][CRC低] 

*/       

	buf1[0] = 0x01;
	buf1[1] = 0x03;
	buf1[2] = 0x01;
	buf1[3] = 0xF3;
	buf1[4] = 0x00;
	buf1[5] = 0x06;
	buf1[6] = 0x34;
	buf1[7] = 0x07;

       for (icount = 0; icount < PROBER_NUM; ++icount) {                       
              Long2Hex((499 + icount * 6),&buf1[2],2);	
              crc = CRC_16_guihe(&buf1[0],6);				
		buf1[6] = crc & 0x00FF; 
		buf1[7] = crc >> 8;
		memset(tmpbuf1,0,sizeof(tmpbuf1));   
		
             // RunLog(">>>>>>>>>>>>>>>>>>>>>>发送读取油罐报警数据: %s",Asc2Hex(buf1, 8));
		//发送的数据为: 01  03  01 F3  00 06  34 07
		if (portlock == 1) continue;
	       if(send_cmd(buf1, tmpbuf1, 8  , 4) < 0){
		             RunLog("发送读取油罐报警信息失败!");
		             return -1;
	       }

	      //返回的数据为: 01  03  0C   00 01 01 40 06 C6 13 F8 77 F4 40 E3   58 D4		   
	      tp_num = tmpbuf1[4];
	      if(tp_num < 0 || tp_num > 31){
		  RunLog("油罐罐号错误，当前罐号为: %d",tp_num);
		  return -1;
	      }

	      n = tmpbuf1[4] - 1;		            		  
             memcpy( alarm[n], tmpbuf1 + 5, 2);
             if (memcmp(pre_alarm[n], alarm[n], 2) != 0) {
			ChangeTpStatAlarm(gpIfsf_shm->tlg_data.tlg_node,
				0x21 + n, gpIfsf_shm->tlg_data.tp_dat[n].TP_Status, alarm[n]);			
			memcpy(pre_alarm[n], alarm[n], 2);
			bzero(alarm[n], 2);
	      }             			 
	}
	   
       print_result();
	return 0;
}



void tank_guihe(int p)
{	
	int ret, len;
	int i;
	pthread_t p_id;

	RunLog("开始运行青岛贵和液位仪通讯程序..................");
	gpGunShm->LiquidAdjust = 1;  //校准标志设置为1
	
	if(open_guihe(p) < 0){
		RunLog("打开串口失败open_aoke(p)  failed! ");
		exit(-1);
	}
	iport = p;

	ret = pthread_create( &p_id, NULL, (void *) guihe_adjust, NULL );
	if(ret != 0){
		__debug(2, "create guihe adjust thread fail");
		exit(-1);
	}

	
	while(1){
		Ssleep(TANK_INTERVAL);
		if(guihe_update() >= 0){
			if (gpIfsf_shm->node_online[MAX_CHANNEL_CNT] == 0) {
				RunLog(">>>>>>>> 液位仪处于联线状态<<<<<<<");
				tank_online(1);
				sleep(WAITFORHEART);
				for (i = 0; i < tp_num; ++i) {
					ChangeTpStatAlarm(gpIfsf_shm->tlg_data.tlg_node, 0x21 + i,
						gpIfsf_shm->tlg_data.tp_dat[i].TP_Status, gpIfsf_shm->tlg_data.tp_dat[i].TP_Alarm);
				}
			}
		} else {
					tank_online(0);
					RunLog(">>>>>>>> 液位仪处于离线状态<<<<<<<");
					if(open_guihe(p) < 0){
		 				RunLog("打开串口失败open_aoke(p)  failed! ");
						exit(-1);
					}
					continue;
		}
		
	}			
}

	

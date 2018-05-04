/*******************************************************************
计量器操作函数
-------------------------------------------------------------------
2007-07-13 by chenfm
2008-03-06 - Guojie Zhu <zhugj@css-intelligent.com>
    convertUsed[] 改为 convert_hb[].lnao.node
2008-03-11 - Guojie Zhu <zhugj@css-intelligent.com>
    By default, fp_m_data->Meter_Puls_Vol_Fact  set to 0x00
    修正ReadM_DATA回复数据的格式问题
*******************************************************************/

#include "ifsf_pub.h"


extern IFSF_SHM *gpIfsf_shm; //该结构是全局的量，只有一个,在ifsf_main.c中定义.

//FP_M_DATA操作函数,node加油机逻辑节点号,M_Ad计量表地址81h-90h,要用到gpIfsf_shm,注意P,V操作和死锁.
int DefaultM_DAT(const char node,  const char m_ad){//默认初始化. 
	int i, fp_state;
	int m= (int )m_ad - 0x81;//0,1,..15.
	DISPENSER_DATA *dispenser_data;
	FP_M_DATA *fp_m_data;

	if((node <= 0) || (node > 127)){
		UserLog("加油机节点值错误,计量器初始化失败");
		return -1;
	}
	if((m< 0) || (m > 15)){
		UserLog("加油机计量器地址值错误,计量器初始化失败");
		return -1;
	}
	if(NULL == gpIfsf_shm){
		UserLog("共享内存值错误,计量器初始化失败");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //找到了加油机节点
	}
	if(i == MAX_CHANNEL_CNT){
		UserLog("加油机节点数据找不到,计量器初始化失败");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	fp_m_data =  (FP_M_DATA *)( &(dispenser_data->fp_m_data[m]) );
	fp_state = 1;
	for(i=0; i < MAX_FP_CNT; i++){
		if(fp_state < dispenser_data->fp_data[i].FP_State) {
			fp_state = dispenser_data->fp_data[i].FP_State;
		}
		if(fp_state > 9) {
			return -1;		
		}
	}
	if(fp_state >3) {
		return -1;	
	}

	fp_m_data->Meter_Type = '\0'; //O
	fp_m_data->Meter_Puls_Vol_Fact = '\0';
	fp_m_data->Meter_Calib_Fact[0] = '\0'; //O
	fp_m_data->Meter_Calib_Fact[1] = '\0';//O	
	fp_m_data->PR_Id = m + 1;
	memset(fp_m_data->Meter_Total , 0, 7); //zero. means first time to fuelling.

	return 0;
}

int SetM_DAT(const  char node,  const char m_ad, const FP_M_DATA *fp_m_data){//设置FP_M_DATA.
 	int i, fp_state;
	int m=(int )m_ad - 0x081;//0,1,..15.
	DISPENSER_DATA *dispenser_data;
	FP_M_DATA *pfp_m_data;

	if( (node <= 0) || (node > 127 ) ){
		UserLog("加油机节点值错误,计量器Set 失败");
		return -1;
		}
	if( (m< 0) || (m > 15 ) ){
		UserLog("加油机计量器地址值错误,计量器Set 失败");
		return -1;
	}
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,计量器Set 失败");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //找到了加油机节点
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,计量器Set 失败");
		return -1;
	}
	if(NULL == fp_m_data) {
		UserLog("给定的加油机计量器数据为空,计量器Set 失败");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	pfp_m_data =  (FP_M_DATA *)( &(dispenser_data->fp_m_data[m]) );
	for(i=0; i < MAX_FP_CNT; i++){ 
		fp_state = dispenser_data->fp_data[i].FP_State;
		if( fp_state >9 ) return -1;		
	}
	if( fp_state >3 ) return -1;	
	memcpy(pfp_m_data, fp_m_data, sizeof(FP_M_DATA));
	return 0;	
}
int CalcMeterTotal(const  char node, const unsigned char m_ad, const unsigned  long volume){//计算总加油量. 
	int i, fp_state;
	int m=(int )m_ad - 0x081;//0,1,..15.
	DISPENSER_DATA *dispenser_data;
	FP_M_DATA *pfp_m_data;
	if( (node <= 0) || (node > 127 ) ){
		UserLog("加油机节点值错误,计量器MeterTotal 失败");
		return -1;
		}
	if( (m< 0) || (m > 15 ) ){
		UserLog("加油机计量器地址值错误,计量器MeterTotal 失败");
		return -1;
	}
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,计量器MeterTotal 失败");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //找到了加油机节点
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,计量器MeterTotal 失败");
		return -1;
	}	
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	pfp_m_data =  (FP_M_DATA *)( &(dispenser_data->fp_m_data[m]) );
	for(i=0; i < MAX_FP_CNT; i++){ 
		fp_state = dispenser_data->fp_data[i].FP_State;
		if( fp_state >9 ) return -1;		
	}
	//use i as ret.	
	i =  BBcdAddLong( (&pfp_m_data->Meter_Total[0]) + 1, volume, 6);
	if (i < 0) {
		memset( (&pfp_m_data->Meter_Total[0]) + 1, 0, 6 );
		UserLog("计量器总数溢出,下次从0开始计算.");
		return -1;
		}
	return 0;
}
//读取FP_M_DATA. FP_M_DATA地址为81h-90h.构造的信息数据的类型为001回答
//recvBuffer为收到的data,sendBuffer为要发送的data,msg_lg为生成的总长度,为指针.
int ReadM_DATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg){
	int i, k,fp_state,tmp_lg, recv_lg;
	int m=recvBuffer[9] - 0x081; //0,1,..15.
	DISPENSER_DATA *dispenser_data;
	FP_M_DATA *pfp_m_data;
	unsigned char node=recvBuffer[1];
	unsigned char buffer[128];
	memset(buffer,0, 128);
	if( (node <= 0) || (node > 127 ) ){
		UserLog("加油机节点值错误,计量器Read  失败");
		return -1;
		}
	if( (m< 0) || (m > 15 ) ){
		UserLog("加油机计量器地址值错误,计量器Read 失败");
		return -1;
	}
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,计量器Read 失败");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //找到了加油机节点
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,计量器Read 失败");
		return -1;
	}	
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	pfp_m_data =  (FP_M_DATA *)( &(dispenser_data->fp_m_data[m]) );
	fp_state = 1;
	for(i=0; i < MAX_FP_CNT; i++){ 
		if(fp_state < dispenser_data->fp_data[i].FP_State)
			fp_state = dispenser_data->fp_data[i].FP_State;
		if( fp_state >9 ) 
			return -1;		
	}
	
	buffer[0] = recvBuffer[2];
	buffer[1] = recvBuffer[3];
	buffer[2] = recvBuffer[0];
	buffer[3] = recvBuffer[1];
	buffer[4] = '\0'; //recvBuffer[4];
	buffer[5] = (recvBuffer[5] |0x20);
	recv_lg = recvBuffer[6];
	recv_lg = recv_lg * 256 + recvBuffer[7];
	buffer[8] = '\x01'; //recvBuffer[8]; //Ad_lg
	buffer[9] = recvBuffer[9]; //Ad
	//buffer[10] = recvBuffer[10]; //It's Data_Id. Data begin.
	tmp_lg = 9;
	for(i=0; i < (recv_lg - 2); i++){
		switch( recvBuffer[10+i] ){			
		case M_Meter_Type://O,(0-2),w(1-2)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)M_Meter_Type;
			tmp_lg++;
			buffer[tmp_lg] ='\x01';
			tmp_lg++;
			buffer[tmp_lg] = pfp_m_data->Meter_Type;
			break;
		case M_Meter_Puls_Vol_Fact:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)M_Meter_Puls_Vol_Fact;
			tmp_lg++;
			buffer[tmp_lg] ='\x01';
			tmp_lg++;
			buffer[tmp_lg] = pfp_m_data->Meter_Puls_Vol_Fact;
			break;
		case M_Meter_Calib_Fact: //O
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)M_Meter_Calib_Fact;
			tmp_lg++;
			buffer[tmp_lg] ='\0';			
			break;
		case M_PR_Id:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)M_PR_Id;
			tmp_lg++;
			buffer[tmp_lg] ='\x01';
			tmp_lg++;
			buffer[tmp_lg] = pfp_m_data->PR_Id;
			break;
		case M_Meter_Total:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)M_Meter_Total;
			tmp_lg++;
			buffer[tmp_lg] ='\x07';
			k=buffer[tmp_lg];
			for(i=0;i<k;i++){
				tmp_lg++;
				buffer[tmp_lg] = pfp_m_data->Meter_Total[i];
			}
			break;		
		default:
				tmp_lg++;
				buffer[tmp_lg] = recvBuffer[10+i];
				tmp_lg++;
				buffer[tmp_lg] = '\0';			
				break;			
		}	
	}

	tmp_lg++;
	buffer[6] = (unsigned char )((tmp_lg-8) / 256) ;
	buffer[7] = (unsigned char )((tmp_lg-8) % 256);  //big end.
	*msg_lg= tmp_lg;
	memcpy(sendBuffer, buffer, tmp_lg);	

	return 0;
}

//写FP_M_DATA函数.recvBuffer为收到的data,sendBuffer为要回复的data,构造的信息数据的类型为111应答
//msg_lg为生成的总长度,为0表示不需要应答信息,sendBuffer和msg_lg不需要应答信息可以设置为NULL.
int WriteM_DATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg){
	int i,  j, lg;
	int  fp_state = 0 , recv_lg, tmp_lg = 0;	
	int m=(int )recvBuffer[9] - 0x081;//0,1,..15.
	DISPENSER_DATA *dispenser_data;
	FP_M_DATA *pfp_m_data;	
	unsigned char tmpMSAck, tmpDataAck;
	unsigned char node=recvBuffer[1];
	unsigned char buffer[512];
	memset(buffer, 0 , 512 );
	if( '\x01' != recvBuffer[0]  ){		
		UserLog("参数错误,不是本加油机的数据,计算器Write 失败");
		return -1;
	}
	if( NULL == recvBuffer ){		
		UserLog("参数错误,计算器Write 失败");
		return -1;
	}
	if( (node <= 0) || (node > 127 ) ){
		UserLog("加油机节点值错误,计算器Write 失败");
		return -1;
		}
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,计算器Write 失败");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //找到了加油机节点
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,计算器Write 失败");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	pfp_m_data =  (FP_M_DATA *)( &(dispenser_data->fp_m_data[m]) );
	fp_state = 1;
	for(i=0; i < MAX_FP_CNT; i++){ 
		if( fp_state < (dispenser_data->fp_data[i].FP_State) )  fp_state = (int )(dispenser_data->fp_data[i].FP_State) ;
		if( fp_state >9 ) 
			return -1;
	}
	//UserLog("Write M_DAT of %d error.", node);
	
	buffer[0] = recvBuffer[2]; //LNAR
	buffer[1] = recvBuffer[3]; //LNAR
	buffer[2] = recvBuffer[0]; //LNAO
	buffer[3] = recvBuffer[1]; //LNAO
	buffer[4] = '\0'; //recvBuffer[4]; //IFSF_MC
	buffer[5] = ( (recvBuffer[5] &0x1f)  |0xE0); //M_St:ack.
	recv_lg = recvBuffer[6];  //M_Lg, big end mode.
	recv_lg = recv_lg * 256 + recvBuffer[7]; 
	buffer[8] = '\x01'; //recvBuffer[8]; //Data_Ad_Lg
	buffer[9] = recvBuffer[9]; //Data_ad:M_DAt.
	//buffer[10] = ; //MS_Ack
	//buffer[11] = recvBuffer[10]; //recvBuffer[10]:Data_Id, 11:Data_Lg, 12:Data_El.  Data begin here.
	tmp_lg = 10;
	i=0;
	tmpMSAck = '\0'; // all is  ok.
	while( i < (recv_lg -2) )
	{
		tmpDataAck = '\0'; //ok ack.
		lg =  (int )recvBuffer[11+i]; //Data_Lg.
		switch( recvBuffer[10+i] ) {
		case M_Meter_Type: //O,(0-2),w(1-2)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)M_Meter_Type;	
			/*
			if( ( lg== 1 ) && (fp_state < 3) && (recvBuffer[13+i] <= 2)  ) 
				pfp_m_data->Meter_Type = recvBuffer[13+i];
			else  if (fp_state < 3)  { 
				tmpDataAck = '\x02';
				tmpMSAck = '\x05'; 
				//UserLog("write  C_DAT's Nb_Products error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';
				tmpMSAck = '\x05';
				//UserLog("write  C_DAT's Nb_Products error!too big or small.") 
				}
			*/
			tmpDataAck = '\x02';
			tmpMSAck = '\x05'; 
			
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;			
		case M_Meter_Puls_Vol_Fact: //(1-255),w(1-2)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)M_Meter_Puls_Vol_Fact;
			/*
			if( ( lg== 1 ) && (fp_state < 3) && (recvBuffer[13+i] != 0)  ) 
				pfp_m_data->Meter_Puls_Vol_Fact = recvBuffer[13+i];
			else  if (fp_state < 3)  { 
				tmpDataAck = '\x02';
				tmpMSAck = '\x05'; 
				//UserLog("write  C_DAT's Nb_Products error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';
				tmpMSAck = '\x05';
				//UserLog("write  C_DAT's Nb_Products error!too big or small.") 
				}
			*/
			tmpDataAck = '\x02';
			tmpMSAck = '\x05'; 
			
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case M_Meter_Calib_Fact: //O
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)M_Meter_Calib_Fact;
			tmpDataAck = '\x04';  
			tmpMSAck = '\x05';
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;		
			break;
		case M_PR_Id://(1-8),w(1-2)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)M_PR_Id;
			/*
			if( ( lg== 1 ) && (fp_state < 3) && (recvBuffer[13+i] >= 1) && (recvBuffer[13+i] <=8) ) 
				pfp_m_data->PR_Id = recvBuffer[13+i];
			else  if (fp_state < 3)  { 
				tmpDataAck = '\x02';
				tmpMSAck = '\x05'; 
				//UserLog("write  C_DAT's Nb_Products error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';
				tmpMSAck = '\x05';
				//UserLog("write  C_DAT's Nb_Products error!too big or small.") 
				}
			*/
			tmpDataAck = '\x02';
			tmpMSAck = '\x05'; 
			
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case M_Meter_Total: //r(1-9)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)M_Meter_Total;
			/*
			if( ( lg<= 6 ) && (fp_state < 3)  ) 
				for(i = 5; i <= (6-lg) ;i--){
					tmp_lg++;
					pfp_m_data->Meter_Total[i]=recvBuffer[13+lg-1 +i-5]; //must debug.
				}				
			else  if (fp_state < 3)  { 
				tmpDataAck = '\x02';
				tmpMSAck = '\x05'; 
				//UserLog("write  C_DAT's Nb_Products error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';
				tmpMSAck = '\x05';
				//UserLog("write  C_DAT's Nb_Products error!too big or small.") 
				}
			*/
			tmpDataAck = '\x02';
			tmpMSAck = '\x05'; 
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			
			break;	
		default:
			tmp_lg++;
			buffer[tmp_lg] =recvBuffer[10+ i]  ;
			tmpDataAck = '\x04';  
			tmpMSAck = '\x05';
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		}
	}
	
	if( (i + 2) != recv_lg )	{
		//UserLog("write M_Dat error!! exit. ");
		UserLog("i  != recv_lg-2, write M_Dat  error!! exit, recv_lg[%d],i [%d] ",recv_lg, i);
		//exit(0);
		return -2;
	}
	tmp_lg++;
	buffer[6] = (unsigned char )((tmp_lg-8) / 256) ; //M_Lg
	buffer[7] = (unsigned char )((tmp_lg-8) %256);  //big end.
	buffer[10] = tmpMSAck;  
	if(NULL != msg_lg) *msg_lg= tmp_lg;
	if(NULL != sendBuffer)  memcpy(sendBuffer, buffer, tmp_lg);
	return 0;		
}
int SetSglM_DAT(const  char node,  const char m_ad, const unsigned char *data){//MeterTotal, give volume then add by self.
	int i,  j, lg;
	int  fp_state = 0 ;	
	int m=(int )m_ad - 0x081;//0,1,..15.
	DISPENSER_DATA *dispenser_data;
	FP_M_DATA *pfp_m_data;
	//unsigned char buffer[1000];
	unsigned char tmpDataAck; //tmpMSAck, 
	//unsigned char node=recvBuffer[1];
	/*
	if( NULL == recvBuffer ){		
		UserLog("参数错误,计算器SetSgl 失败");
		return -1;
	}*/
	if( (node <= 0) || (node > 127 ) ){
		UserLog("加油机节点值错误,计算器SetSgl 失败");
		return -1;
		}
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,计算器SetSgl 失败");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //找到了加油机节点
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,计算器SetSgl 失败");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	pfp_m_data =  (FP_M_DATA *)( &(dispenser_data->fp_m_data[m]) );
	for(i=0; i < MAX_FP_CNT; i++){ 
		if( fp_state < (dispenser_data->fp_data[i].FP_State) )  fp_state = (int )(dispenser_data->fp_data[i].FP_State) ;
		if( fp_state >9 ) return -1;
	}
	//UserLog("Write M_DAT of %d error.", node);
	//memset(buffer, 0 , sizeof(buffer) );
	
	tmpDataAck = '\0'; //ok ack.
	lg =  (int )data[1]; //Data_Lg.
	switch( data[0] ) //Data_Id.
	{
	case M_Meter_Type: //O,(0-2),w(1-2)			
		if( ( lg== 1 ) && (fp_state < 3) && (data[2] <= 2)  ) 
			pfp_m_data->Meter_Type = data[2];
		else  if (fp_state > 3)  { 
			tmpDataAck = '\x02';
			//UserLog("write  C_DAT's Nb_Products error!fp state not in 1,2. ") 
			}
		else  { 
			tmpDataAck = '\x01';
			//UserLog("write  C_DAT's Nb_Products error!too big or small.") 
			}		
		break;			
	case M_Meter_Puls_Vol_Fact: //(1-255),w(1-2)			
		if( ( lg== 1 ) && (fp_state < 3) && (data[2] != 0)  ) 
			pfp_m_data->Meter_Puls_Vol_Fact =data[2];
		else  if (fp_state < 3)  { 
			tmpDataAck = '\x02';			
			//UserLog("write  C_DAT's Nb_Products error!fp state not in 1,2. ") 
			}
		else  { 
			tmpDataAck = '\x01';
			//UserLog("write  C_DAT's Nb_Products error!too big or small.") 
			}			
		break;
	case M_Meter_Calib_Fact: //O		
		tmpDataAck = '\x04';  			
		break;
	case M_PR_Id://(1-8),w(1-2)		
		if( ( lg== 1 ) && (fp_state < 3) && (data[2] >= 1) && (data[2] <=8) ) 
			pfp_m_data->PR_Id = data[2];
		else  if (fp_state < 3)  { 
			tmpDataAck = '\x02';			
			//UserLog("write  C_DAT's Nb_Products error!fp state not in 1,2. ") 
			}
		else  { 
			tmpDataAck = '\x01';			
			//UserLog("write  C_DAT's Nb_Products error!too big or small.") 
			}				
		break;
	case M_Meter_Total: //r(1-9)		
		if( ( lg == 7 ) && (fp_state > 2) && (fp_state <9)  ) 
			for(j= 0; j <lg ;j++){				
				pfp_m_data->Meter_Total[j]=data[2+j]; //must debug.
			}				
		else  if (fp_state < 2)  { 
			tmpDataAck = '\x02';			
			//UserLog("write  C_DAT's Nb_Products error!fp state not in 1,2. ") 
			}
		else  { 
			tmpDataAck = '\x01';			
			//UserLog("write  C_DAT's Nb_Products error!too big or small.") 
			}
		break;		
	default:		
		tmpDataAck = '\x04';  		
		break;
	}
	return tmpDataAck;		
}
int RequestM_DAT(const  char node,  const char m_ad){
	int i, fp_state,tmp_lg;
	int m=(int )m_ad - 0x081;//0,1,..15.
	DISPENSER_DATA *dispenser_data;
	FP_M_DATA *pfp_m_data;
	unsigned char buffer[512];
	memset(buffer, 0 , 512 );
	if( (node <= 0) || (node > 127 ) ){
		UserLog("加油机节点值错误,计量器RequestM_DAT 失败");
		return -1;
		}
	if( (m< 0) || (m > 15 ) ){
		UserLog("加油机计量器地址值错误,计量器RequestM_DAT 失败");
		return -1;
	}
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,计量器Set 失败");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //找到了加油机节点
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,计量器RequestM_DAT 失败");
		return -1;
	}
	
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	pfp_m_data =  (FP_M_DATA *)( &(dispenser_data->fp_m_data[m]) );
	for(i=0; i < MAX_FP_CNT; i++){ 
		fp_state = dispenser_data->fp_data[i].FP_State;
		if( fp_state >9 ) return -1;		
	}
	if( fp_state >2 ) {
		UserLog("加油点状态大于2,不能请求配置.");
		return -1;
	}
	buffer[0] = '\x02';
	buffer[1] = '\x01';
	buffer[2] = '\x01';
	buffer[3] = (unsigned char)node;
	buffer[4] = '\0';  //IFSF_MC
	buffer[5] =  '\0'; //M_St; //read.
	
	buffer[8] = '\x01'; //recvBuffer[8]; //Ad_lg
	buffer[9] = m_ad; //recvBuffer[9]; //Ad
	//buffer[10] = recvBuffer[10]; //It's Data_Id. Data begin.
	tmp_lg=9;
	
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)M_Meter_Type  ;
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)M_Meter_Puls_Vol_Fact  ;
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)M_PR_Id  ;
	//tmp_lg++;
	//buffer[tmp_lg] = (unsigned char)M_Meter_Total  ;	
	tmp_lg++;
	buffer[6] =(unsigned char)(tmp_lg /256) ;
	buffer[7] = (unsigned char)(tmp_lg % 256) ; 
	//i use to be ret.
	i= SendData2CD(buffer, tmp_lg, 5);
	if (i <0 ){
		UserLog("加油机通讯错误,计量器不能Request ");
		return -1;
	}
	return 0;
}

/*-----------------------------------------------------------------*/


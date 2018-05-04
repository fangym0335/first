
/*
 * 2008-03-05  - Guojie Zhu <zhugj@css-intelligent.com>
 *     所有SendDataBySock替换为SendData2CD
 */
//#include  "ifsf.h"
#include <string.h>

#include  "ifsf_tlg.h"
#include "ifsf_pub.h"
#include "ifsf_tcpip.h"

#include  "ifsf_tlgtp.c"
#include  "ifsf_tlgtlg.c"

static int TransRecvTLGAdErr(const int newsock, const unsigned char *recvBuffer, int MS_ACK){
	int i, ret, ad1, m_st, msg_lg;
	//DISPENSER_DATA *dispenser_data;		
	unsigned char node= recvBuffer[1];
	//unsigned char ad_char2[2], ad_char4[4];
	unsigned char buffer[128];
	
	memset(buffer, 0 , sizeof(buffer));	
	if( (NULL == recvBuffer) || (0 == newsock)  ){		
		UserLog("参数错误,Translate  失败");
		return -1;
	}	
	m_st = ( (int)recvBuffer[5]&0xe0) >>5;
	//ad_lg = recvBuffer[8];
	//ad1 = recvBuffer[9];
	
	if( node <= 0 ){
		UserLog("加油机节点值错误,Translate   失败");
		return -1;
		}
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,Translate   失败");
		return -1;
	}
	buffer[0] = recvBuffer[2];
	buffer[1] = recvBuffer[3];
	buffer[2] = recvBuffer[0];
	buffer[3] = recvBuffer[1];
	buffer[4] = '\0'; //IFSF_MC;
	buffer[5] = (recvBuffer[5] |0x20); //answer
	buffer[6] = '\0'; //data_lg;
	buffer[7] =2 + recvBuffer[8]; //data_lg
	//recv_lg = recvBuffer[6];
	//recv_lg = recv_lg * 256 + recvBuffer[7];
	buffer[8] = recvBuffer[8]; // Ad_lg   
	for(i=0; i< recvBuffer[8]; i++ ){
		buffer[9+i] = recvBuffer[9+i];  //Ad  
	}
	i = recvBuffer[8];//i use as a temp value.
	buffer[9+ i] = MS_ACK;  
	//buffer[10] = recvBuffer[10]; //It's Data_Id. Data begin.
	msg_lg = 10 + recvBuffer[8];
	
	
	ret = SendData2CD(buffer, msg_lg, 5);
	if (ret <0){		
		UserLog("发送回复数据错误,Translate   失败");
		return -1;
	}
	
	return 0;
}


static int TransRecvTLGSV_DATA(const int newsock, const unsigned char *recvBuffer ){
	int i, ret, ad1, m_st, msg_lg;
	//DISPENSER_DATA *dispenser_data;		
	unsigned char node= recvBuffer[1];
	//unsigned char ad_char2[2], ad_char4[4];
	unsigned char buffer[128];
	
//	RunLog("in to TransRecvTLGSV_DATA func ");
	memset(buffer, 0 , sizeof(buffer));	
	if( (NULL == recvBuffer) || (0 == newsock)  ){		
		UserLog("参数错误,Translate  失败");
		return -1;
	}	
	m_st = ( (int)recvBuffer[5]&0xe0) >>5;
	//ad_lg = recvBuffer[8];
	//ad1 = recvBuffer[9];
	
	if(  node == 0  ){
		UserLog("加油机节点值错误,Translate   失败");
		return -1;
	}
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,Translate   失败");
		return -1;
	}
	
	ret = -1;
//	RunLog(">>>>>> before switch , m_st: %d", m_st);
	switch(m_st){
		case 0:
//			RunLog(">>>>>> into switch , call ReadSV_DATA, m_st: %d", m_st);
			ret = ReadSV_DATA(recvBuffer, buffer,&msg_lg);
			break;
		case 1:
			break;
		case 2:
			ret = WriteSV_DATA(recvBuffer, buffer, &msg_lg);
			break;
		case 3:
		case 4:
			break;
		case 7:
			break;
		default :
			UserLog("收到的数据的消息类型错误!!");
			return -1;
			break;
	}
	if(ret == 0){
		ret = SendData2CD(buffer, msg_lg, 5);
		if (ret <0){		
			UserLog("发送回复数据错误,Translate   失败");
			return -1;
		}
		return 0;
	}

	return -1;
}


static int TransRecvTLG_DATA(const int newsock, const unsigned char *recvBuffer ){
	int i, ret,  m_st, msg_lg;//ad1, 
	//DISPENSER_DATA *dispenser_data;		
	unsigned char node= recvBuffer[1];
	//unsigned char ad_char2[2], ad_char4[4];
	unsigned char buffer[256];
	
	memset(buffer, 0 , sizeof(buffer));
	printf("TransRecvTLG_DATA");
	if( (NULL == recvBuffer) || (0 == newsock)  ){		
		printf("参数错误,Translate  失败");
		return -1;
	}	
	m_st = ( (int)recvBuffer[5]&0xe0) >>5;
	//ad_lg = recvBuffer[8];
	//ad1 = recvBuffer[9];
	
	if( node == 0 ){
		printf("液位仪节点值错误,Translate   失败");
		return -1;
		}
	if( NULL == gpIfsf_shm ){		
		printf("共享内存值错误,Translate   失败");
		return -1;
	}
	
	ret = -1;
	switch(m_st){
		case 0:
			ret = ReadTLG_DAT(recvBuffer, buffer,&msg_lg);
			break;
		case 1:
			break; 
		case 2:
			ret = WriteTLG_DAT(recvBuffer, buffer, &msg_lg);
			break;
		case 3:
		case 4:
			break;
		case 7:
			break;
		default :
			printf("收到的数据的消息类型错误!!");
			return -1;
			break;
	}
	if(ret == 0){
		ret = SendData2CD(buffer, msg_lg, 5);
		if (ret <0){		
			printf("发送回复数据错误,Translate   失败");
			return -1;
		}
		return 0;
	}
	return -1;
}

static int TransRecvTLGER_DATA(const int newsock, const unsigned char *recvBuffer ){
	int i, ret, ad1, m_st, msg_lg;
	//DISPENSER_DATA *dispenser_data;		
	unsigned char node= recvBuffer[1];
	//unsigned char ad_char2[2], ad_char4[4];
	unsigned char buffer[256];
	memset(buffer, 0 , 256);
	
	if( (NULL == recvBuffer) || (0 == newsock)  ){		
		printf("参数错误,Translate  失败");
		return -1;
	}	
	m_st = ( (int)recvBuffer[5]&0xe0) >>5;
	//ad_lg = recvBuffer[8];
	ad1 = recvBuffer[9];
	
	if( node <= 0 ){
		printf("液位仪节点值错误,Translate   失败");
		return -1;
		}
	if( NULL == gpIfsf_shm  ){		
		printf("共享内存值错误,Translate   失败");
		return -1;
	}
	
	ret = -1;
	switch(m_st){
		case 0:
			ret = ReadTLGER_DAT(recvBuffer, buffer,&msg_lg);
			break;
		case 1:
			break;
		case 2:
			//ret = WriteER_DATA(recvBuffer, buffer, &msg_lg);
			break;
		case 3:
		case 4:
			break;
		case 7:
			break;
		default :
			UserLog("收到的数据的消息类型错误!!");
			return -1;
			break;
	}
	if(ret == 0){
		ret = SendData2CD(buffer, msg_lg, 5);
		if (ret <0){		
			UserLog("发送回复数据错误,Translate   失败");
			return -1;
		}
		return 0;
	}
	return -1;
}

static int TransRecvTP_DATA(const int newsock, const unsigned char *recvBuffer ){
	int i, ret,  m_st, msg_lg;//ad1, 
	//DISPENSER_DATA *dispenser_data;		
	unsigned char node= recvBuffer[1];
	//unsigned char ad_char2[2], ad_char4[4];
	unsigned char buffer[256];
	
	memset(buffer, 0 , sizeof(buffer));
	printf("TransRecvTP_DATA");
	if( (NULL == recvBuffer) || (0 == newsock)  ){		
		UserLog("参数错误,Translate  失败");
		return -1;
	}	
	m_st = ( (int)recvBuffer[5]&0xe0) >>5;
	//ad_lg = recvBuffer[8];
	//ad1 = recvBuffer[9];
	
	if( node == 0 ){
		printf("液位仪节点值错误,Translate   失败");
		return -1;
		}
	if( NULL == gpIfsf_shm ){		
		printf("共享内存值错误,Translate   失败");
		return -1;
	}
	
	ret = -1;
	switch(m_st){
		case 0:
			ret = ReadTP_DAT(recvBuffer, buffer,&msg_lg);
			break;
		case 1:
			break; 
		case 2:
			ret = WriteTP_DAT(recvBuffer, buffer, &msg_lg);
			break;
		case 3:
		case 4:
			break;
		case 7:
			break;
		default :
			printf("收到的数据的消息类型错误!!");
			return -1;
			break;
	}
	if(ret == 0){
		ret = SendData2CD(buffer, msg_lg, 5);
		if (ret <0){		
			printf("发送回复数据错误,Translate   失败");
			return -1;
		}
		return 0;
	}
	return -1;
}


static int TransRecvTPER_DATA(const int newsock, const unsigned char *recvBuffer ){
	int i, ret, ad1, m_st, msg_lg;
	//DISPENSER_DATA *dispenser_data;		
	unsigned char node= recvBuffer[1];
	//unsigned char ad_char2[2], ad_char4[4];
	unsigned char buffer[256];
	memset(buffer, 0 , 256);
	
	if( (NULL == recvBuffer) || (0 == newsock)  ){		
		printf("参数错误,Translate  失败");
		return -1;
	}	
	m_st = ( (int)recvBuffer[5]&0xe0) >>5;
	//ad_lg = recvBuffer[8];
	ad1 = recvBuffer[9];
	
	if( node <= 0 ){
		printf("液位仪节点值错误,Translate   失败");
		return -1;
		}
	if( NULL == gpIfsf_shm  ){		
		printf("共享内存值错误,Translate   失败");
		return -1;
	}
	
	ret = -1;
	switch(m_st){
		case 0:
			ret = ReadTPER_DAT(recvBuffer, buffer,&msg_lg);
			break;
		case 1:
			break;
		case 2:
			//ret = WriteER_DATA(recvBuffer, buffer, &msg_lg);
			break;
		case 3:
		case 4:
			break;
		case 7:
			break;
		default :
			printf("收到的数据的消息类型错误!!");
			return -1;
			break;
	}
	if(ret == 0){
		ret = SendData2CD(buffer, msg_lg, 5);
		if (ret <0){		
			printf("发送回复数据错误,Translate   失败");
			return -1;
		}
		return 0;
	}
	return -1;
}



//--------------------------------------------------
//初始化液位仪
//-------------------------------------------------
 int DefaultTLG(const char node)
{
 	int i, ret;
//	TLG_DATA *pTlg_data;
//	TLG_DAT *pTlg_dat;
//	TLG_ER_DAT *pTlg_er_dat;
//	TP_DAT *pTp_dat;
//	TP_ER_DAT  *pTp_er_dat;

	if( node == 0 ){
		RunLog("加油机节点值错误,加油机初始化失败");
		return -1;
	}
	if( NULL == gpIfsf_shm ){		
		RunLog("共享内存值错误,加油机初始化失败");
		return -1;
	}
	
	//pTlg_data = (TLG_DATA *)( &(gpIfsf_shm->tlg_data) );
	//pTlg_dat = (TLG_DAT*)( &(pTlg_data->tlg_dat) );
	//pTlg_er_dat =  (TLG_ER_DAT *)( &(pTlg_data->tlg_er_dat)  )
	//tp =  tp_ad - 0x21;	
	//pTp_dat = ( TP_DAT *)( &(pTlg_data->tp_dat[0]) );
	//pTp_er_dat =  ( TP_ER_DAT *)( &(pTlg_data->tp_er_dat[0]) );
	
	ret = DefaultTLG_DAT(node);	
	if (ret <0 ) {
		return -1;
	}
	ret = DefaultTLGER_DAT(node);	
	if (ret <0 ) {
		return -1;
	}

	gpIfsf_shm->tlg_data.tlg_dat.Nb_Tanks = atoi(TLG_PROBER_NUM);
	for (i = 0; i < gpIfsf_shm->tlg_data.tlg_dat.Nb_Tanks; ++i) {
		ret = DefaultTP_DAT(node, 0x21 + i);	
		if (ret <0 ) {
			return -1;
		}
		ret = DefaultTPER_DAT(node, 0x21 + i);	
		if (ret <0 ) {
			return -1;
		}
	}
	
	
	return 0;
 }
//----------------------------------------
//TcpRecvServ调用:
//  解析(并执行,如果要执行的话)收到的液位仪信息数据,成功返回0,失败返回-1
//---------------------------------------
int TranslateTLGRecv(const int newsock, const unsigned char *recvBuffer){
	int i, ret, tp_status = 0 , ad_lg, ad1;
	unsigned char node = recvBuffer[1];
	TLG_DATA *pTlg_data;
	TLG_DAT *pTlg_dat;
	//TLG_ER_DAT *pTlg_er_dat;
	//TP_DAT *pTp_dat;
	//TP_ER_DAT  *pTp_er_dat;	
	unsigned char allBuffer[256];
	int recv_lg;
	
//	RunLog("in to TranslateTLGRecv.........");
	if('\x09' != recvBuffer[0]  ){	
		printf("参数错误,不是液位仪的数据,Translate 失败");
		return -1;
	}
	if( (NULL == recvBuffer) || (0 == newsock)  ){		
		printf("参数错误,Translate  失败");
		return -1;
	}
	//m_st = ( (int)recvBuffer[5]&0xe0) >>5;
	ad_lg = recvBuffer[8];
	ad1 = recvBuffer[9];
	printf("Translaterecv: ad_lg[%d], ad_b1[%2x]",ad_lg,ad1);
	if( 0 == node ){
		ret = TransRecvTLGAdErr(newsock, recvBuffer, 1);
		printf("液位仪节点值错误,Translate   失败");
		return -1;
	}
	if(  NULL == gpIfsf_shm  ){
		ret = TransRecvTLGAdErr(newsock, recvBuffer, 2);
		printf("共享内存值错误,Translate   失败");
		return -1;
	}
	
	//UserLog("TransRecv"); 
	pTlg_data = (TLG_DATA *)( &(gpIfsf_shm->tlg_data) );
	pTlg_dat = (TLG_DAT*)( &(pTlg_data->tlg_dat) );
	for(i=0; i < MAX_TP_PER_TLG; i++){ 
		if( tp_status < (pTlg_data->tp_dat[i].TP_Status) ) {
			tp_status = (pTlg_data->tp_dat[i].TP_Status) ;
		}
	}	
#if 1
	if( (tp_status <= 0)||(tp_status > 3) ) {
		ret = TransRecvTLGAdErr(newsock, recvBuffer, 2);
		RunLog("tp Status error , tp_status: %d!", tp_status);
		return -1; 
	}
#endif
	if(1 == ad_lg){
		if(0 ==ad1) {//Communication Servicie Data.
			ret =TransRecvTLGSV_DATA(newsock,  recvBuffer);
			if (ret < 0) {
				RunLog("TransRecvTLGSV_DATA return %d", ret);
			}
		} else if (1 == ad1) 
			ret = TransRecvTLG_DATA( newsock,  recvBuffer);
		else if(  (ad1>=0x21) && (ad1 <= 0x3f) )
			ret = TransRecvTP_DATA(newsock,  recvBuffer);	
		else if( ad1==0x20 ){//all tp data
			recv_lg = recvBuffer[6];
			recv_lg = recv_lg * 256 + recvBuffer[7];
			memcpy(allBuffer,recvBuffer,recv_lg+8);
			for(i=0;i<pTlg_dat->Nb_Tanks;i++){				
				allBuffer[9] = 0x21+i;
				ret = TransRecvTP_DATA(newsock,  allBuffer);
				if (ret <0){
					ret = TransRecvTLGAdErr(newsock, recvBuffer, 2);
					//UserLog("Translate 接收到的数据错误!!");
					break;
				}	
			}
		} else{
			ret = TransRecvTLGAdErr(newsock, recvBuffer, 6);
			printf("接收到的数据的地址错误!!");
			return -1;
		}		
	} else if(2 == ad_lg){
		ret = TransRecvTLGAdErr(newsock, recvBuffer, 6);
		printf("接收到的数据的地址错误!!");
		return -1;
	} else if(3 == ad_lg){
		if( (recvBuffer[9] == 1) && (recvBuffer[10] ==  0x41) ){
			ret =TransRecvTLGER_DATA(newsock, recvBuffer);
		} else if( (recvBuffer[9] >= 0x21) && (recvBuffer[9] <= 0x3f) &&(recvBuffer[10] ==  0x41) ){
			ret =TransRecvTPER_DATA(newsock, recvBuffer);
		} else{//all error read  not implement.
			ret = TransRecvTLGAdErr(newsock, recvBuffer, 6);
			printf("接收到的数据的地址错误!!");
			return -1;
		}	
	} else{
		ret = TransRecvTLGAdErr(newsock, recvBuffer, 6);
		printf("接收到的数据的地址错误!!");
		return -1;
	}

	if (ret <0){
		ret = TransRecvTLGAdErr(newsock, recvBuffer, 2);
		printf("Translate 接收到的数据错误!!");
		return -1;
	}	

//	RunLog("TranslateTLGRecv successful !");

	return 0;
}




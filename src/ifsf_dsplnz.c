/*******************************************************************
逻辑油枪的操作函数.
-------------------------------------------------------------------
2007-07-16 by chenfm
2008-03-06 - Guojie Zhu <zhugj@css-intelligent.com>
   convertUsed[] 改为 convert_hb[].lnao.node
2008-03-12 - Guojie Zhu <zhugj@css-intelligent.com>
    修正 ReadLN_DATA 回复报文格式问题
*******************************************************************/

#include "ifsf_pub.h"

#include "bcd.h"
extern IFSF_SHM *gpIfsf_shm; //该结构是全局的量，只有一个,在ifsf_main.c中定义.

/**
*  destbuf 得到全部油机泵码
*  010101070a000000011738
*  01    01  02   07    0a000000011738
*  node fp nzl  point  total
* 
*  destlen all len return to call user
**/
int GetAllLNTotal_Node(char *destbuf, int *destlen, int node)
{
	int i = 0, flag = 0, j = 0, fp = 0, nzl = 0, cntgun = 0;
	DISPENSER_DATA * dispenser_data = NULL;
	FP_LN_DATA * pfp_ln_data = NULL;
	int step = 11; //每个段的长度
	
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,逻辑油枪Read 失败");
		return -1;
	}
	
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if(node == gpIfsf_shm->node_channel[i].node)  {
			flag = 1; //表示有节点在线
			RunLog("DDDDDDDDDDDDDDD  node %d", node);
			dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
			for (j = 0; j <MAX_GUN_CNT; j++) {
				if (gpIfsf_shm->gun_config[j].node == node && \
					(fp = gpIfsf_shm->gun_config[j].logfp) != 0 &&\
					(nzl = gpIfsf_shm->gun_config[j].logical_gun_no) != 0) {
					
					pfp_ln_data =  (FP_LN_DATA *)( &(dispenser_data->fp_ln_data[fp - 1][nzl - 1])  );
					destbuf[cntgun * step + 0] = node;
					destbuf[cntgun * step + 1] = fp;
					destbuf[cntgun * step + 2] = nzl;
					destbuf[cntgun * step + 3] = 0x07;
					memcpy (&destbuf[cntgun * step + 4] , pfp_ln_data->Log_Noz_Vol_Total, sizeof(pfp_ln_data->Log_Noz_Vol_Total));
					cntgun++;
				}
			}
			
			
		}
	}
	if(!flag ){
		UserLog("加油机节点数据找不到,逻辑油枪Read  失败");
		return -1;
	}
	*destlen = cntgun * step;
	return 0;
}


//FP_LN_DATA操作数据. node加油机逻辑节点号,fp_ad加油点地址21h-24h,ln_ad逻辑油枪地址11h-18h,
//要用到gpIfsf_shm,注意P,V操作和死锁.
int DefaultLN_DATA(const char node, const char fp_ad, const char ln_ad){
	int i, fp_state;
	int fp=fp_ad-0x21; //0,1,2,3号加油点.
	int ln=ln_ad-0x11; //0,1,2,3,4,5,6,7号逻辑油枪.
	int ret;
	int lnz_idx;
	unsigned char tmpUChar;
	DISPENSER_DATA *dispenser_data;
	FP_LN_DATA *pfp_ln_data;

	if( (node <= 0) || (node > 127 ) ){
		UserLog("加油机节点值错误,逻辑油枪默认设置 失败");
		return -1;
		}
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,逻辑油枪默认设置  失败");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //找到了加油机节点
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,逻辑油枪默认设置 失败");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	pfp_ln_data =  (FP_LN_DATA *)( &(dispenser_data->fp_ln_data[fp][ln])  );
	fp_state = dispenser_data->fp_data[fp].FP_State;
	if( fp_state >9 ) return -1;	
	
	pfp_ln_data->PR_Id = '\x01';	
	//DefaultPR_DAT(node, 0x40+pfp_ln_data->PR_Id);
	//DefaultFM_DAT(node, 0x40+pfp_ln_data->PR_Id);
	pfp_ln_data->Physical_Noz_Id = '\x01';
	pfp_ln_data->Meter_1_Id = '\x00';
	//DefaultM_DAT(node, 0x81); // 0x80 + pfp_ln_data->Meter_1_Id	
	bzero(pfp_ln_data->Log_Noz_Vol_Total, 7);
	bzero(pfp_ln_data->Log_Noz_Amount_Total, 7);
	bzero(pfp_ln_data->No_TR_Total, 7);
//	bzero(pfp_ln_data->Log_Noz_SA_Vol_Total, 7);
//	bzero(pfp_ln_data->Log_Noz_SA_Amount_Total, 7);
//	bzero(pfp_ln_data->No_TR_SA_Total, 7);
	
	return 0;
}


//读取FP_LN_DATA. 构造的信息数据的类型为001回答.
//recvBuffer为收到的data,sendBuffer为要发送的data,msg_lg为生成的总长度.
//如果油枪泵码为0，那么返回99
int ReadLN_DATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg){
	int i, j, k,fp_state,recv_lg, tmp_lg,ret, chnlog, cmd_lg, l;
	int fp=recvBuffer[9]-0x21; //0,1,2,3号加油点.
	int ln=recvBuffer[10]-0x11; //0,1,2,3,4,5,6,7号逻辑油枪.	
	char node=recvBuffer[1];	
	unsigned char zeroTotal[6] = {0};
	//unsigned char tmpUChar;	
	DISPENSER_DATA *dispenser_data;
	FP_LN_DATA *pfp_ln_data;	
	unsigned char buffer[1024], cmd[13]={0}, zeroBuff[7] = {0}, nodeDriver;
	memset(buffer, 0 , 1024);
	if( recvBuffer[8] != (unsigned char)2 ){
		UserLog("加油机逻辑油枪地址错误,逻辑油枪Read  失败");
		return -1;
	}
	if( (node <= 0) || (node > 127 ) ){
		UserLog("加油机节点值错误,逻辑油枪Read  失败");
		return -1;
	}
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,逻辑油枪Read 失败");
		return -1;
	}
		
	//获取油机品牌，恒山TQC油机后台读取泵码时，从油机读取
	for(i = 0; i < MAX_CHANNEL_CNT; i++){
		if (node == gpIfsf_shm->node_channel[i].node) {
		       	break; //找到了加油机节点
		}
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,加油点Write  失败");
		return -1;
	}
	chnlog = gpIfsf_shm->node_channel[i].chnLog;
	nodeDriver = gpIfsf_shm->node_channel[i].nodeDriver;

	
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //找到了加油机节点
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,逻辑油枪Read  失败");
		return -1;
	}
	
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	pfp_ln_data =  (FP_LN_DATA *)( &(dispenser_data->fp_ln_data[fp][ln])  );
	fp_state = dispenser_data->fp_data[fp].FP_State;
	if (fp_state > 9) {
		return -1;  
	}
	buffer[0] = recvBuffer[2];
	buffer[1] = recvBuffer[3];
	buffer[2] = recvBuffer[0];
	buffer[3] = recvBuffer[1];
	buffer[4] = '\0'; //recvBuffer[4]; IFSF_MC
	buffer[5] = (recvBuffer[5] |0x20);//answer.
	recv_lg = recvBuffer[6];
	recv_lg = recv_lg * 256 + recvBuffer[7];
	buffer[8] = '\x02'; //recvBuffer[8]; //Ad_lg
	buffer[9] = recvBuffer[9];  //ad
	buffer[10] = recvBuffer[10]; //ad
	//buffer[11] = recvBuffer[11]; //It's Data_Id. Data begin.	
	tmp_lg=10;
	for(i=0;i<recv_lg - 3; i++){
		switch(recvBuffer[11+i]) {
			case LN_PR_Id :
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)LN_PR_Id;
				tmp_lg++;
				buffer[tmp_lg] = '\x01';
				tmp_lg++;
				buffer[tmp_lg] = pfp_ln_data->PR_Id;
				break;
			/*
			case LN_Hose_Expansion_Vol :
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)LN_Hose_Expansion_Vol;
				tmp_lg++;
				buffer[tmp_lg] = '\0';				
				break;
			case LN_Slow_Flow_Valve_Activ:	
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)LN_Slow_Flow_Valve_Activ;
				tmp_lg++;
				buffer[tmp_lg] = '\0';				
				break;
			*/
			case LN_Physical_Noz_Id :
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)LN_Physical_Noz_Id;
				tmp_lg++;
				buffer[tmp_lg] = '\x01';
				tmp_lg++;
				buffer[tmp_lg] = pfp_ln_data->Physical_Noz_Id;
				break;
			case LN_Meter_1_Id:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)LN_Meter_1_Id;
				tmp_lg++;
				buffer[tmp_lg] = '\x01';
				tmp_lg++;
				buffer[tmp_lg] = pfp_ln_data->Meter_1_Id;
				break;
			/*
			case LN_Meter_1_Blend_Ratio:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)LN_Meter_1_Blend_Ratio;
				tmp_lg++;
				buffer[tmp_lg] = '\0';
				break;
			case LN_Meter_2_Id:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)LN_Meter_2_Id;
				tmp_lg++;
				buffer[tmp_lg] = '\0';				
				break;
			case LN_Logical_Nozzle_Type:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)LN_Logical_Nozzle_Type;
				tmp_lg++;
				buffer[tmp_lg] = '\0';
				break;
			case LN_Preset_Valve_Activation:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)LN_Preset_Valve_Activation;
				tmp_lg++;
				buffer[tmp_lg] = '\0';
				break;
			*/
			case LN_Log_Noz_Vol_Total:
				//恒山TQC油机交易结束后不读取泵码，后台读取泵码时实时从油机同步
				//TQC油机交易结束后泵码变化延迟大
				if((gpIfsf_shm->get_total_flag != node) && (nodeDriver == 0x14)){

					gpIfsf_shm->get_total_flag = node;
					cmd[0] = 0x6D;
					cmd[1] = recvBuffer[10] - 0x10;  //逻辑油枪号 11 -18H
					cmd[2] = recvBuffer[9];
					cmd_lg = 13;

					//读取泵码
					ret = SendMsg(ORD_MSG_TRAN, cmd , cmd_lg, chnlog, 3);
					if (ret < 0) {
					       	return ret;
					}
					RunLog( "Node[%d].FP[%d]下发命令[%s], 返回[%d]",node,fp+1, Asc2Hex(cmd, cmd_lg ), ret);

					bzero(cmd, sizeof(cmd));
					cmd_lg = sizeof(cmd) -1;
						
					ret = RecvMsg(TOTAL_MSG_TRAN, cmd, &cmd_lg, &chnlog, -1); 
					if( (ret < 0) || (cmd[1] == 0xee) ){
						gpIfsf_shm->get_total_flag = 0; //读取失败 ，下次重复读取此节点
						RunLog("Node[%d]读取泵码失败!ret = [%d]",node, ret);
					}else if(cmd[1] == 0x55){
						RunLog("Node[%d]读取泵码成功!ret = [%d]",node, ret);
					}
				}

				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)LN_Log_Noz_Vol_Total;
				tmp_lg++;
				buffer[tmp_lg] = '\x07';	
				k = buffer[tmp_lg];

				for (j=0; j<k; j++)  {
					tmp_lg++;
					buffer[tmp_lg] = pfp_ln_data->Log_Noz_Vol_Total[j];
				}
				break;
			case LN_Log_Noz_Amount_Total:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)LN_Log_Noz_Amount_Total;
				tmp_lg++;
				buffer[tmp_lg] = '\x07';
				k = buffer[tmp_lg] ;
				for (j= 0; j< k; j++)  {
					tmp_lg++;
					buffer[tmp_lg] = pfp_ln_data->Log_Noz_Amount_Total[j];
				}
				break;
			case LN_No_TR_Total:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)LN_No_TR_Total;
				tmp_lg++;
				buffer[tmp_lg] = '\x07';
				k = buffer[tmp_lg] ;
				for (j=0; j<k; j++)  {
					tmp_lg++;
					buffer[tmp_lg] = pfp_ln_data->No_TR_Total[j];
				}
				break;
			case LN_Log_Noz_SA_Vol_Total :
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)LN_Log_Noz_SA_Vol_Total;
				tmp_lg++;
				buffer[tmp_lg] = '\x07';
				k = buffer[tmp_lg] ;
				for (j=0; j<k; j++)  {
					tmp_lg++;
					buffer[tmp_lg] = pfp_ln_data->Log_Noz_SA_Vol_Total[j];
				}
				break;
			case LN_Log_Noz_SA_Amount_Total:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)LN_Log_Noz_SA_Amount_Total;
				tmp_lg++;
				buffer[tmp_lg] = '\x07';
				k = buffer[tmp_lg] ;
				for (j=0; j<k; j++)  {
					tmp_lg++;
					buffer[tmp_lg] = pfp_ln_data->Log_Noz_SA_Amount_Total[j];
				}
				break;
			case LN_No_TR_SA_Total:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)LN_No_TR_SA_Total;
				tmp_lg++;
				buffer[tmp_lg] = '\x07';
				k = buffer[tmp_lg] ;
				for (j=0; j<k; j++)  {
					tmp_lg++;
					buffer[tmp_lg] = pfp_ln_data->No_TR_SA_Total[j];
				}
				break;
			case LN_All_Log_Noz_Vol_Total:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)LN_All_Log_Noz_Vol_Total;
				tmp_lg++;
				GetAllLNTotal_Node(&buffer[tmp_lg], &k, node);
				tmp_lg += k;
				break;
			default:
				tmp_lg++;
				buffer[tmp_lg] = recvBuffer[11+i];
				tmp_lg++;
				buffer[tmp_lg] = '\0';			
				break;			
		}	
	}

	tmp_lg++;
	buffer[6] = (unsigned char )((tmp_lg-8) / 256) ;
	buffer[7] = (unsigned char )((tmp_lg-8) %256);  //big end.
	*msg_lg= tmp_lg;
	memcpy(sendBuffer, buffer, tmp_lg);	

	return 0;
}

//写FP_LN_DATA函数.recvBuffer为收到的data,sendBuffer为要回复的data,构造的信息数据的类型为111应答
//msg_lg为生成的总长度,为0表示不需要应答信息,sendBuffer和msg_lg不需要应答信息可以设置为NULL.
int WriteLN_DATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg){
	int i, j, k, fp_state,  recv_lg,  tmp_lg,  lg;
	int fp=recvBuffer[9]-0x21; //0,1,2,3号加油点.
	int ln=recvBuffer[10]-0x11; //0,1,2,3,4,5,6,7号逻辑油枪.	
	int cmd_lg,ret;
	int lnz_idx;
	char node=recvBuffer[1];	
	unsigned char tmpMSAck, tmpDataAck;
	unsigned char buffer[MAX_TCP_LEN];
	unsigned char cmd[20];
	DISPENSER_DATA *dispenser_data;
	FP_LN_DATA *pfp_ln_data;	

	if( recvBuffer[8] != (unsigned char)2 ){
		UserLog("加油机逻辑油枪地址错误,逻辑油枪Write  失败");
		return -1;
	}
	if( (node <= 0) || (node > 127 ) ){
		UserLog("加油机节点值错误,逻辑油枪Write  失败");
		return -1;
	}
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,逻辑油枪Write 失败");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //找到了加油机节点
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,逻辑油枪Write  失败");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	pfp_ln_data =  (FP_LN_DATA *)( &(dispenser_data->fp_ln_data[fp][ln])  );
	fp_state = dispenser_data->fp_data[fp].FP_State;
	if( fp_state >9 ) return -1;
	
	memset(buffer, 0 , sizeof(buffer) );
	buffer[0] = recvBuffer[2]; 
	buffer[1] = recvBuffer[3];
	buffer[2] = recvBuffer[0];
	buffer[3] = recvBuffer[1]; 
	buffer[4] = '\0'; //recvBuffer[4]; IFSF_MC
	buffer[5] = ((recvBuffer[5] &0x1f)|0xe0); //MS_St
	recv_lg = recvBuffer[6];
	recv_lg = recv_lg * 256 + recvBuffer[7];
	buffer[8] = '\x02'; //recvBuffer[8];
	buffer[9] = recvBuffer[9];  // ad
	buffer[10] = recvBuffer[10]; // ad
	//buffer[11] = ; //MS_Ack
	//recvBuffer[11]; //It's Data_Id. Data begin.	
	tmp_lg=11;
	i=0;
	tmpMSAck = '\0'; // all is  ok.
	while( i < (recv_lg - 3) ) {
		tmpDataAck = '\0'; //ok ack.
		lg =  (int )recvBuffer[12+i]; //Data_Lg.
		switch(recvBuffer[11+i]) {
		case LN_PR_Id : //(1-8),W(1-2)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)LN_PR_Id;			
			if ((lg == 1) && (fp_state < 3) && (recvBuffer[13+i] >= 1) && (recvBuffer[13+i] <= 8)) {
				pfp_ln_data->PR_Id = recvBuffer[13+i];


				/*
				 * Added by Guojie zhu @ 2009.8.7 , 油品换号相关
				 */

				while (0xFF == gpIfsf_shm->cfg_changed) {
					RunLog("Waiting for SyncCfg2File.....");
					sleep(1);
				}

				for (lnz_idx = 0; lnz_idx < gpIfsf_shm->gunCnt; lnz_idx++) {
					if (gpIfsf_shm->gun_config[lnz_idx].node == node &&
						gpIfsf_shm->gun_config[lnz_idx].logfp ==
									(recvBuffer[9] - 0x20) &&
						gpIfsf_shm->gun_config[lnz_idx].logical_gun_no == 
									(recvBuffer[10] - 0x10)) {

						// 保存新的  Prod_Nb 到 gun_config[]中 
						memcpy(gpIfsf_shm->gun_config[lnz_idx].oil_no, 
							dispenser_data->fp_pr_data[(pfp_ln_data->PR_Id) - 1].Prod_Nb, 4);
						
						break;
					}
				}

				// 配置有改动, 通知 SyncCfg2File 同步配置 
				gpIfsf_shm->cfg_changed = 0x0F;

				/*
				cmd[0] = '\x19';
				cmd[1] = '\x00';
				cmd[2] = recvBuffer[13+i] +0x40; //pr_ad
				cmd[3] = recvBuffer[9]; //fp_ad
				cmd[4] = recvBuffer[10]; //ln_ad
				cmd_lg =5;
				ret = SendMsg(ORD_MSG_TRAN, cmd , cmd_lg, node, 3);
				if(ret < 0) {
					tmpDataAck = '\x06';  
					tmpMSAck = '\x05';
					tmp_lg++;
					buffer[tmp_lg] =tmpDataAck;
					i = i + 2 + lg;
					break;
				}
				sleep(1); //test?
				if(pfp_ln_data->PR_Id != recvBuffer[13+i] ) {
					tmpDataAck = '\x06';  
					tmpMSAck = '\x05';
					}
				//RequestPR_DATA(node, 0x40 + LN_PR_Id);
				*/
				}
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';
				tmpMSAck = '\x05'; 
				//UserLog("write  C_DAT's Nb_Products error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';
				tmpMSAck = '\x05';
				//UserLog("write  C_DAT's Nb_Products error!too big or small.") 
				}			
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		/*
		case LN_Hose_Expansion_Vol :
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)LN_Hose_Expansion_Vol;
			tmp_lg++;
			buffer[tmp_lg] = '\0';				
			break;
		case LN_Slow_Flow_Valve_Activ:	
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)LN_Slow_Flow_Valve_Activ;
			tmp_lg++;
			buffer[tmp_lg] = '\0';				
			break;
		*/
		case LN_Physical_Noz_Id : //(1-8),W(1-2)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)LN_Physical_Noz_Id;
			/*
			if( ( lg == 1 ) && (fp_state < 3) && (recvBuffer[14+i] >= 1) && (recvBuffer[14+i] <= 8) ) {
				pfp_ln_data->Physical_Noz_Id = recvBuffer[13+i];
				//RequstPR_DATA(node, 0x40 + LN_PR_Id);
				}
			else  if (fp_state >= 3)  { 
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
		case LN_Meter_1_Id: //(0-16),W(1-2)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)LN_Meter_1_Id;
			/*
			if( ( lg == 1 ) && (fp_state < 3) && ( recvBuffer[14+i] <= 16) ) {
				pfp_ln_data->Meter_1_Id = recvBuffer[13+i];
				RequestM_DAT(node, 0x80+LN_Meter_1_Id);
				}
			else  if (fp_state >= 3)  { 
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
		/*
		case LN_Meter_1_Blend_Ratio:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)LN_Meter_1_Blend_Ratio;
			tmp_lg++;
			buffer[tmp_lg] = '\0';
			break;
		case LN_Meter_2_Id:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)LN_Meter_2_Id;
			tmp_lg++;
			buffer[tmp_lg] = '\0';				
			break;
		case LN_Logical_Nozzle_Type:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)LN_Logical_Nozzle_Type;
			tmp_lg++;
			buffer[tmp_lg] = '\0';
			break;
		case LN_Preset_Valve_Activation:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)LN_Preset_Valve_Activation;
			tmp_lg++;
			buffer[tmp_lg] = '\0';
			break;
		*/
		case LN_Log_Noz_Vol_Total: //W(1-2)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)LN_Log_Noz_Vol_Total;
			/*
			if( ( lg == 7 ) && (fp_state < 3)  ) {
				for(j=lg-1; j>=0; j--) 
					pfp_ln_data->Log_Noz_Vol_Total[j] = recvBuffer[14+i+j];//big end.
				//RequstPR_DATA(node, 0x40 + LN_PR_Id);
				}
			else  if (fp_state >= 3)  { 
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
			i = i + 2 +lg;
			break;
		case LN_Log_Noz_Amount_Total:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)LN_Log_Noz_Amount_Total;
			/*
			if( ( lg == 7 ) && (fp_state < 3)  ) {
				for(j=lg-1; j>=0; j--)
					pfp_ln_data->Log_Noz_Amount_Total[j] = recvBuffer[14+i+j];//big end.
				//RequstPR_DATA(node, 0x40 + LN_PR_Id);
				}
			else  if (fp_state >= 3)  { 
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
		case LN_No_TR_Total:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)LN_No_TR_Total;
			/*
			if( ( lg == 7 ) && (fp_state < 3)  ) {
				for(j=lg-1; j>=0; j--)
					pfp_ln_data->No_TR_Total[j] = recvBuffer[14+i+j];//big end.
				//RequstPR_DATA(node, 0x40 + LN_PR_Id);
				}
			else  if (fp_state >= 3)  { 
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
		/*
		case LN_Log_Noz_SA_Vol_Total :
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)LN_Log_Noz_SA_Vol_Total;			
			if( ( lg >= 4 ) && (fp_state < 3)  ) {
				for(j=lg-1; j>=0; j--)
					pfp_ln_data->Log_Noz_SA_Vol_Total [j] = recvBuffer[14+i+j];//big end.
				//RequstPR_DATA(node, 0x40 + LN_PR_Id);
				}
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';
				tmpMSAck = '\x05'; 
				//UserLog("write  C_DAT's Nb_Products error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';
				tmpMSAck = '\x05';
				//UserLog("write  C_DAT's Nb_Products error!too big or small.") 
				}			
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case LN_Log_Noz_SA_Amount_Total:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)LN_Log_Noz_SA_Amount_Total;			
			if( ( lg >= 4 ) && (fp_state < 3)  ) {
				for(j=lg-1; j>=0; j--)
					pfp_ln_data->LN_Log_Noz_SA_Amount_Total [j] = recvBuffer[14+i+j];//big end.
				//RequstPR_DATA(node, 0x40 + LN_PR_Id);
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
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case LN_No_TR_SA_Total:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)LN_No_TR_SA_Total;			
			if( ( lg >= 4 ) && (fp_state < 3)  ) {
				for(j=lg-1; j>=0; j--)
					pfp_ln_data->LN_No_TR_SA_Total [j] = recvBuffer[14+i+j];//big end.
				//RequstPR_DATA(node, 0x40 + LN_PR_Id);
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
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		*/
		default:
			tmp_lg++;
			buffer[tmp_lg] = recvBuffer[11+i];
			tmpDataAck = '\x04';  
			tmpMSAck = '\x05';
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;		
			break;			
		}
	}

	if ((i + 3) != recv_lg) {
		//UserLog("write LN_Dat error!! exit. ");
		UserLog("(i + 3) != recv_lg, write LN_Dat error!! exit. i[%d],recv_lg[%d]", i, recv_lg);
		//exit(0);
		return -2;
	}

	tmp_lg++;
	buffer[6] = (unsigned char )((tmp_lg-8) / 256) ; //M_Lg
	buffer[7] = (unsigned char )((tmp_lg-8) % 256);  //big end.
	buffer[11] = tmpMSAck; 	

	if(NULL != msg_lg) {
		*msg_lg= tmp_lg;
	}

	if(NULL != sendBuffer) {
	       	memcpy(sendBuffer, buffer, tmp_lg);
	}

	return 0;
}
//设置单个数据项,用于设备本身改写数据项或者初始化配置.
//成功返回0,失败返回-1或者Data_Ack. data内容为:Data_Id+Data_Lg+Data_El.
int SetSglLN_DATA(const char node, const char fp_ad, const char ln_ad, const unsigned char *data){
	int i, j, k, fp_state,   lg, ret;
	int fp=fp_ad-0x21; //0,1,2,3号加油点.
	int ln=ln_ad-0x11; //0,1,2,3,4,5,6,7号逻辑油枪.	
	//char node=recvBuffer[1];	
	unsigned char  tmpDataAck;  //tmpMSAck,
	//unsigned char buffer[1024];
	DISPENSER_DATA *dispenser_data;
	FP_LN_DATA *pfp_ln_data;	
	if(  (fp <0)||(fp>3) ){
		UserLog("加油机加油点地址值错误,逻辑油枪set  失败");
		return -1;
	}
	if(  (ln <0)||(ln>7) ){
		UserLog("加油机逻辑油枪地址值错误,逻辑油枪set  失败");
		return -1;
	}
	if( (node <= 0) || (node > 127 ) ){
		UserLog("加油机节点值错误,逻辑油枪set  失败");
		return -1;
	}
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,逻辑油枪set 失败");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //找到了加油机节点
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,逻辑油枪set  失败");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	pfp_ln_data =  (FP_LN_DATA *)( &(dispenser_data->fp_ln_data[fp][ln])  );
	fp_state = dispenser_data->fp_data[fp].FP_State;
	if( fp_state >9 ) return -1;
	
	i=0;
	tmpDataAck = '\0'; //ok ack.
	lg =  (int )data[1]; //Data_Lg.
	switch(data[0]) {
		case LN_PR_Id : //(1-8),W(1-2)				
			if( ( lg == 1 ) && (fp_state < 3) && (data[2] >= 1) && (data[2] <= 8) ) {
				pfp_ln_data->PR_Id = data[2];
				//ret = DefaultPR_DAT(node, 0x40 + pfp_ln_data->PR_Id );
				//if(ret == 0)
				//	ret = RequestPR_DATA(node, 0x40 + pfp_ln_data->PR_Id );
				}
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';				
				//UserLog("write  C_DAT's Nb_Products error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';				
				//UserLog("write  C_DAT's Nb_Products error!too big or small.") 
				}			
			break;
		/*
		case LN_Hose_Expansion_Vol :
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)LN_Hose_Expansion_Vol;
			tmp_lg++;
			buffer[tmp_lg] = '\0';				
			break;
		case LN_Slow_Flow_Valve_Activ:	
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)LN_Slow_Flow_Valve_Activ;
			tmp_lg++;
			buffer[tmp_lg] = '\0';				
			break;
		*/
		case LN_Physical_Noz_Id : //(1-8),W(1-2)
			if( ( lg == 1 ) && (fp_state < 3) && (data[2] >= 1) && (data[2] <= 8) ) {
				pfp_ln_data->Physical_Noz_Id = data[2];
				//RequestPR_DATA(node, 0x40 + LN_PR_Id);
				}
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';				
				//UserLog("write  C_DAT's Nb_Products error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';				
				//UserLog("write  C_DAT's Nb_Products error!too big or small.") 
				}			
			break;
		case LN_Meter_1_Id: //(0-16),W(1-2)
			if( ( lg == 1 ) && (fp_state < 3) && (data[2] <= 16) ) {
				pfp_ln_data->Meter_1_Id = data[2];
				ret = DefaultM_DAT(node, 0x80 + pfp_ln_data->Meter_1_Id);
				if (ret == 0)
					ret = RequestM_DAT(node, 0x80 + pfp_ln_data->Meter_1_Id );
				//RequestPR_DATA(node, 0x40 + LN_PR_Id);
				}
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';				
				//UserLog("write  C_DAT's Nb_Products error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';				
				//UserLog("write  C_DAT's Nb_Products error!too big or small.") 
				}			
			break;
		/*
		case LN_Meter_1_Blend_Ratio:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)LN_Meter_1_Blend_Ratio;
			tmp_lg++;
			buffer[tmp_lg] = '\0';
			break;
		case LN_Meter_2_Id:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)LN_Meter_2_Id;
			tmp_lg++;
			buffer[tmp_lg] = '\0';				
			break;
		case LN_Logical_Nozzle_Type:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)LN_Logical_Nozzle_Type;
			tmp_lg++;
			buffer[tmp_lg] = '\0';
			break;
		case LN_Preset_Valve_Activation:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)LN_Preset_Valve_Activation;
			tmp_lg++;
			buffer[tmp_lg] = '\0';
			break;
		*/
		case LN_Log_Noz_Vol_Total: //W(1-2)
			if( ( lg == 7 ) && (fp_state < 3)  ) {
				for(j=lg-1; j>=0; j--)
					pfp_ln_data->Log_Noz_Vol_Total[j] = data[2+j];
				//RequestPR_DATA(node, 0x40 + LN_PR_Id);
				}
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';				
				//UserLog("write  C_DAT's Nb_Products error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';				
				//UserLog("write  C_DAT's Nb_Products error!too big or small.") 
				}		
			break;
		case LN_Log_Noz_Amount_Total:
			if( ( lg == 7 ) && (fp_state < 3)  ) {
				for(j=lg-1; j>=0; j--)
					pfp_ln_data->Log_Noz_Amount_Total[j] = data[2+j];
				//RequestPR_DATA(node, 0x40 + LN_PR_Id);
				}
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';				
				//UserLog("write  C_DAT's Nb_Products error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';				
				//UserLog("write  C_DAT's Nb_Products error!too big or small.") 
				}		
			break;
		case LN_No_TR_Total:
			if( ( lg == 7 ) && (fp_state < 3)  ) {
				for(j=lg-1; j>=0; j--)
					pfp_ln_data->No_TR_Total[j] = data[2+j];
				//RequestPR_DATA(node, 0x40 + LN_PR_Id);
				}
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';				
				//UserLog("write  C_DAT's Nb_Products error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';				
				//UserLog("write  C_DAT's Nb_Products error!too big or small.") 
				}		
			break;		
		case LN_Log_Noz_SA_Vol_Total :
			if( ( lg == 7 ) && (fp_state < 3)  ) {
				for(j=lg-1; j>=0; j--)
					pfp_ln_data->Log_Noz_SA_Vol_Total[j] = data[2+j];
				//RequestPR_DATA(node, 0x40 + LN_PR_Id);
				}
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';				
				//UserLog("write  C_DAT's Nb_Products error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';				
				//UserLog("write  C_DAT's Nb_Products error!too big or small.") 
				}	
			break;
		case LN_Log_Noz_SA_Amount_Total:
			if( ( lg == 7 ) && (fp_state < 3)  ) {
				for(j=lg-1; j>=0; j--)
					pfp_ln_data->Log_Noz_SA_Amount_Total[j] = data[2+j];
				//RequestPR_DATA(node, 0x40 + LN_PR_Id);
				}
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';				
				//UserLog("write  C_DAT's Nb_Products error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';				
				//UserLog("write  C_DAT's Nb_Products error!too big or small.") 
				}	
			break;
		case LN_No_TR_SA_Total:
			if( ( lg == 7 ) && (fp_state < 3)  ) {
				for(j=lg-1; j>=0; j--)
					pfp_ln_data->No_TR_SA_Total[j] = data[2+j];
				//RequestPR_DATA(node, 0x40 + LN_PR_Id);
				}
			else  if (fp_state >= 3)  { 
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
int RequestLN_DATA(const char node, const char fp_ad, const char ln_ad){
	int i, fp_state, tmp_lg, ret;
	int fp=fp_ad-0x21; //0,1,2,3号加油点.
	int ln=ln_ad-0x11; //0,1,2,3,4,5,6,7号逻辑油枪.	
	//char node=recvBuffer[1];	
	//unsigned char  tmpDataAck;  //tmpMSAck,
	unsigned char buffer[128];
	DISPENSER_DATA *dispenser_data;
	FP_LN_DATA *pfp_ln_data;	
	if(  (fp <0)||(fp>3) ){
		UserLog("加油机加油点地址值错误,逻辑油枪Request  失败");
		return -1;
	}
	if(  (ln <0)||(ln>7) ){
		UserLog("加油机逻辑油枪地址值错误,逻辑油枪Request  失败");
		return -1;
	}
	if( (node <= 0) || (node > 127 ) ){
		UserLog("加油机节点值错误,逻辑油枪Request  失败");
		return -1;
	}	
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,逻辑油枪Request 失败");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //找到了加油机节点
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,逻辑油枪Request  失败");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	pfp_ln_data =  (FP_LN_DATA *)( &(dispenser_data->fp_ln_data[fp][ln])  );
	fp_state = dispenser_data->fp_data[fp].FP_State;
	if( fp_state >2 ) {
		UserLog("加油机加油点状态>2,逻辑油枪不能Request ");
		return -1;
	}
	
	buffer[0] = '\x02'; //CD
	buffer[1] = '\x01'; //CD 1
	buffer[2] = '\x01';
	buffer[3] = (unsigned char)node;
	buffer[4] = '\0'; //recvBuffer[4]; IFSF_MC
	buffer[5] = '\0'; //M_St
	
	buffer[8] = '\x02';  //Ad_lg
	buffer[9] = (unsigned char)fp_ad;  //ad
	buffer[10] = (unsigned char)ln_ad; //ad
	tmp_lg=10;
	tmp_lg++;
	buffer[tmp_lg] =(unsigned char)LN_PR_Id;	
	tmp_lg++;
	buffer[tmp_lg] =(unsigned char) LN_Physical_Noz_Id;
	tmp_lg++;
	buffer[tmp_lg] =(unsigned char) LN_Meter_1_Id;
	//tmp_lg++;
	//buffer[tmp_lg] =(unsigned char) LN_Meter_1_Id;
	tmp_lg++;
	buffer[tmp_lg] =(unsigned char) LN_Log_Noz_Vol_Total;
	tmp_lg++;
	buffer[tmp_lg] =(unsigned char) LN_Log_Noz_Amount_Total;
	tmp_lg++;
	buffer[tmp_lg] =(unsigned char) LN_No_TR_Total;

	tmp_lg++;
	buffer[6] =(unsigned char)((tmp_lg-8) /256) ;
	buffer[7] = (unsigned char)((tmp_lg-8) % 256) ; 
	ret= SendData2CD(buffer, tmp_lg, 5);
	if (ret <0 ){
		UserLog("加油机通讯错误,逻辑油枪不能Request ");
		return -1;
	}
	return 0;
}

//一笔交易做完,LNZ要改变的数据. volum交易油量,amount交易金额.price交易单价
//int EndTr4Ln(const char node, const char fp_ad, const char ln_ad,  
//		const unsigned char * volume, const unsigned char * amount, const unsigned char * price)
int EndTr4Ln(const char node, const char fp_ad, const char ln_ad)
{
	int i, fp_state,ret1,ret2,ret3;
	int fp=fp_ad-0x21; //0,1,2,3号加油点.
	int ln=ln_ad-0x11; //0,1,2,3,4,5,6,7号逻辑油枪.
	unsigned char tmpUChar[10];
	DISPENSER_DATA *dispenser_data;
	FP_LN_DATA *pfp_ln_data;

	if( (node <= 0) || (node > 127 ) ){
		UserLog("加油机节点值错误,逻辑油枪默认设置 失败");
		return -1;
		}
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,逻辑油枪默认设置  失败");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //找到了加油机节点
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,逻辑油枪默认设置 失败");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]));
	pfp_ln_data =  (FP_LN_DATA *)( &(dispenser_data->fp_ln_data[fp][ln]));
	fp_state = dispenser_data->fp_data[fp].FP_State;
	if(fp_state >9) return -1;

	memset(tmpUChar,0 , 10);
	//pfp_ln_data->PR_Id = '\x01';	
	
	//pfp_ln_data->Physical_Noz_Id = '\x01';
	//pfp_ln_data->Meter_1_Id = '\x01';
	//memcpy(&(pfp_ln_data->Log_Noz_Vol_Total), volume,6 )
	//if(amount != volume * price) {
	//	return -1;
	//	UserLog("你的油量乘以油价不等于金额!");
	//}
	//There used 6, I don't do with BinX. that means long number and Bcdx(in BinX+BcdX) have same fact.
	//BBcdAdd(unsigned char * bcd1, const unsigned char * bcd2, const int bcdlen1, const int bcdlen2)
//	pfp_ln_data->Log_Noz_Vol_Total[0] = 10;
//	pfp_ln_data->Log_Noz_Amount_Total[0] = 10;
	pfp_ln_data->No_TR_Total[0] = 0;
	ret3 = BBcdAddLong( (&pfp_ln_data->No_TR_Total[0]) , 1, 7); //7 must equel your real length.
	if (ret3 < 0) {
		memset( (&pfp_ln_data->No_TR_Total[0+ 1]) , 0, 6);
	}
#if 0
	ret1=BBcdAdd( (&pfp_ln_data->Log_Noz_Vol_Total[0+ 1]) , &volume[1], 6, 4); //6 must equel your real length.
	ret2=BBcdAdd( (&pfp_ln_data->Log_Noz_Amount_Total[0+ 1]) ,  &amount[1], 6,4); //6 must equel your real length.
	ret3=BBcdAddLong( (&pfp_ln_data->No_TR_Total[0]) , 1, 7); //7 must equel your real length.
	if ((ret1<0)||(ret2<0)||(ret3<0)){ //overflow.
		memset( (&pfp_ln_data->Log_Noz_Vol_Total[0+ 1]) , 0, 6);
		memset(  (&pfp_ln_data->Log_Noz_Amount_Total[0+ 1]) , 0, 6);
		memset( (&pfp_ln_data->No_TR_Total[0+ 1]) , 0, 6);
	}
#endif
		
	//pfp_ln_data->Log_Noz_SA_Vol_Total[] = '\0';
	//pfp_ln_data->Log_Noz_SA_Amount_Total[] = '\0';
	//pfp_ln_data->No_TR_SA_Total[] = '\0';
	
//	RunLog("##### end of EndTr4LNZ, tr_state: %02x #########", gpIfsf_shm->fp_tr_data[0].Trans_State);
	return 0;	
}

/*-----------------------------------------------------------------*/




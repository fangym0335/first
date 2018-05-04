/*******************************************************************
油品操作函数
-------------------------------------------------------------------
2007-07-13 by chenfm
2008-03-04 - Guojie Zhu <zhugj@css-intelligent.com>
    删除处理变价命令时的状态判断和等待一秒反馈执行结果的操作, 应Retalix要求接收成功就返回ACK
2008-03-06 - Guojie Zhu <zhugj@css-intelligent.com>
    convertUsed[] 改为 convert_hb[].lnao.node 
2008-03-12 - Guojie Zhu <zhugj@css-intelligent.com>
    修正ReadPR_DATA,ReadFM_DATA 回复报文的格式问题
2008-03-16 - Guojie Zhu <zhugj@css-intelligent.com>
    变价命令的unit_price增加对动态小数点位置的支持(使用TwoDecPoint())
*******************************************************************/
#include <string.h>

#include "ifsf_pub.h"


extern IFSF_SHM *gpIfsf_shm; //该结构是全局的量，只有一个,在ifsf_main.c中定义.
static int IsProdInUse(const __u8 *prod_nb);
static int AddProdToGlobalConfigList(const __u8 *prod_nb, const __u8 *prod_description);

//FP_PR_DATA操作函数,node加油机逻辑节点号,pr_ad油品地址,要用到gpIfsf_shm,注意使用访问索引关键字.
int DefaultPR_DAT(const char node, const unsigned char  pr_ad){ //不用,根据节点号可以直接写.
	int i, fp_state;
	int m=(int )pr_ad - 0x41;//0,1,..7.
	DISPENSER_DATA *dispenser_data;
	FP_PR_DATA *fp_pr_data;
	if( (node <= 0) || (node > 127 ) ){
		UserLog("加油机节点值错误,油品初始化失败");
		return -1;
		}
	if( (m< 0) || (m > 8 ) ){
		UserLog("加油机计量器地址值错误,油品初始化失败");
		return -1;
	}
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,油品初始化失败");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //找到了加油机节点
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,油品初始化失败");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	fp_pr_data =  (FP_PR_DATA *)( &(dispenser_data->fp_pr_data[m]) );
	for(i=0; i < MAX_FP_CNT; i++){ 
		fp_state = dispenser_data->fp_data[i].FP_State;
		if( fp_state >9 ) return -1;		
	}
	if( fp_state >3 ) return -1;	
	
//	fp_pr_data->Prod_Nb[3] = pr_ad - 0x40;  // commented by Guojie Zhu @ 2009.08.05(初始值应为0)
	
	return 0;
}


//读取FP_PR_DATA. FP_PR_DATA地址为41h-48h.构造的信息数据的类型为001回答
//recvBuffer为收到的data,sendBuffer为要发送的data,msg_lg为生成的总长度.
int ReadPR_DATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg){
	int i,j, k, fp_state, recv_lg, tmp_lg ;
	int m=(int )recvBuffer[9] - 0x41;//0,1,..7.
	DISPENSER_DATA *dispenser_data;
	FP_PR_DATA *pfp_pr_data;
	unsigned char node = recvBuffer[1];
	unsigned char buffer[64];
	memset(buffer,0, 64);
	if( (node <= 0) || (node > 127 ) ){
		UserLog("加油机节点值错误,油品Read 失败");
		return -1;
		}
	if( (m< 0) || (m > 8 ) ){
		UserLog("加油机计量器地址值错误,油品Read 失败");
		return -1;
	}
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,油品Read 失败");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //找到了加油机节点
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,油品Read 失败");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	pfp_pr_data =  (FP_PR_DATA *)( &(dispenser_data->fp_pr_data[m]) );
	for(i=0; i < MAX_FP_CNT; i++){ 
		fp_state = dispenser_data->fp_data[i].FP_State;
		if( fp_state >9 ) return -1;		
	}
	if( fp_state >3 ) return -1;	
	
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
		switch( recvBuffer[10 + i] ){		
		case PR_Prod_Nb:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)PR_Prod_Nb;
			tmp_lg++;
			buffer[tmp_lg] ='\x04';
			k = buffer[tmp_lg];
			for (j = 0; j < k; j++){
				tmp_lg++;
				buffer[tmp_lg] = pfp_pr_data->Prod_Nb[j];
			}

			/*if (pfp_pr_data->Prod_Nb[0] != 0 && pfp_pr_data->Prod_Nb[1] != 0 \
				&& pfp_pr_data->Prod_Nb[2] != 0 && pfp_pr_data->Prod_Nb[3] != 0) {
				Update_Prod_No();
				memcpy(&buffer[tmp_lg - 3], pfp_pr_data->Prod_Nb, 4);
			}*/
			break;		
		case PR_Prod_Description:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)PR_Prod_Description;
			tmp_lg++;
			buffer[tmp_lg] = 16;
			k = buffer[tmp_lg];
			for (j = 0; j < k; j++){
				tmp_lg++;
				buffer[tmp_lg] = pfp_pr_data->Prod_Description[j];
			}
			break;
		default:
			tmp_lg++;
			buffer[tmp_lg] = recvBuffer[10 + i];
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
//写FP_PR_DATA函数. recvBuffer为收到的data,sendBuffer为要回复的data,构造的信息数据的类型为111应答
//msg_lg为生成的总长度,为0表示不需要应答信息,sendBuffer和msg_lg不需要应答信息可以设置为NULL.
int WritePR_DATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg){
	int i,j, fp_state, recv_lg, tmp_lg, lg ;
	int m=(int )recvBuffer[9] - 0x41;//0,1,..7.
	DISPENSER_DATA *dispenser_data;
	FP_PR_DATA *pfp_pr_data;
	unsigned char node = recvBuffer[1];
	unsigned char tmpDataAck,  tmpMSAck ; 
	unsigned char buffer[64];
	unsigned char null_discription1[16];
	unsigned char null_discription2[16];
	unsigned char const zero4bytes[4] = {0, 0, 0, 0};

	memset(buffer,0, 64);
	memset(null_discription1, 0, sizeof(null_discription1));
	memset(null_discription2, ' ', sizeof(null_discription2));

	if( (node <= 0) || (node > 127 ) ){
		UserLog("加油机节点值错误,油品Write 失败");
		return -1;
		}
	if( (m< 0) || (m > 8 ) ){
		UserLog("加油机计量器地址值错误,油品Write 失败");
		return -1;
	}
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,油品Read 失败");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //找到了加油机节点
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,油品Write 失败");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	pfp_pr_data =  (FP_PR_DATA *)( &(dispenser_data->fp_pr_data[m]) );
	
	fp_state = 1;
	for(i=0; i < MAX_FP_CNT; i++){ 
		if(fp_state < dispenser_data->fp_data[i].FP_State)
			fp_state = dispenser_data->fp_data[i].FP_State;
		if( fp_state >9 ) 
			return -1;		
	}
	//if( fp_state >3 )return -1;	
	
	buffer[0] = recvBuffer[2]; //LNAR
	buffer[1] = recvBuffer[3]; //LNAR
	buffer[2] = recvBuffer[0]; //LNAO
	buffer[3] = recvBuffer[1]; //LNAO
	buffer[4] = '\0'; //recvBuffer[4]; //IFSF_MC
	buffer[5] = ( (recvBuffer[5] &0x1f)  |0xe0); //M_St:ack.
	recv_lg = recvBuffer[6];  //M_Lg, big end mode.
	recv_lg = recv_lg * 256 + recvBuffer[7]; 
	buffer[8] = '\x01'; //recvBuffer[8]; //Data_Ad_Lg
	buffer[9] = recvBuffer[9]; //Data_ad:M_DAt.
	//buffer[10] =  ; //MS_Ack
	//buffer[11] = ; //11:Data_Id, 12:Data_Ack
	//recvBuffer[10]:Data_Id, 11:Data_Lg, 12:Data_El.  Data begin here.
	tmp_lg = 10;
	i=0;
	tmpMSAck = '\0'; // all is  ok.
	while( i < (recv_lg -2) ) {
		tmpDataAck = '\0'; //ok ack.
		lg =  (int )recvBuffer[11+i]; //Data_Lg.
		switch( recvBuffer[10+i] ) {
		case PR_Prod_Nb://(1-9),w(1-2)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)PR_Prod_Nb;

			// 0.油品数据库中不允许存在重复油品配置, 所以增加判断
			if (memcmp(zero4bytes, &recvBuffer[12 + i], 4) == 0) {
				// Prod_Nb为零, 是删除操作
				j = FP_PR_CNT;
			} else {
				// Prod_Nb非零, 是添加或者修改操作, 需要判断是否重复
				for (j = 0; j < FP_PR_CNT; ++j) {
					if ((memcmp(&(dispenser_data->fp_pr_data[j].Prod_Nb), \
								       	&recvBuffer[12 + i], 4) == 0) &&
									   (recvBuffer[9] - 0x41 != j)) {
						RunLog("油品(%s)的配置已存在于数据库中[%02x],不允许重复配置", 
							Asc2Hex((unsigned char *)&recvBuffer[12 + i], 4), 0x41 + j);
						break;
					}
				}
			}

			if (j < FP_PR_CNT) {
				// 有重复项则NAK
				tmpDataAck = '\x01';
				tmpMSAck = '\x05'; 
			} else if ((lg == 4) && (fp_state < 3) && 
					(memcmp(&recvBuffer[12 + i], zero4bytes, 4) == 0)) {
				// 1. 删除油品信息

				if (memcmp(pfp_pr_data->Prod_Nb, zero4bytes, 4) != 0) {
					memcpy(pfp_pr_data->Prod_Nb, &recvBuffer[12 + i], 4);
					memset(pfp_pr_data->Prod_Description, ' ', 16);
					dispenser_data->fp_c_data.Nb_Products--;
					RunLog("删除油品(%s)的配置信息", 
						Asc2Hex((unsigned char *)&recvBuffer[12 + i], 4));
				} else {
					memset(pfp_pr_data->Prod_Description, ' ', 16);
					// 若本来此Pr_id配置就为空, 不算删除操作, 不能减少油品计数Nb_Products;
				}

				// 删除数据库中的配置,无需删除配置文件中的配置, 所以不同步变动到配置文件
			} else if ((lg == 4) && (fp_state < 3)) {
				// 2. 修改或者添加油品信息

				while (0xFF == gpIfsf_shm->cfg_changed) {
					RunLog("Waiting for SyncCfg2File.....");
					sleep(1);
				}

				RunLog("Node %d Pr_Id %02x Prod_Nb: %s", node, 
						recvBuffer[9], Asc2Hex(pfp_pr_data->Prod_Nb, 4));


				// 2.1 更新本地全局油品配置
#if 0
				for (j = 0; j < gpIfsf_shm->oilCnt; ++j) {
					RunLog("oil_config[%d].oilno:%s", j, Asc2Hex(gpIfsf_shm->oil_config[j].oilno, 4));
					if (memcmp(&recvBuffer[12 + i], gpIfsf_shm->oil_config[j].oilno, 4) == 0) {
						RunLog("油品(%s)的配置信息已存在", 
							Asc2Hex((unsigned char *)&recvBuffer[12 + i], 4));
						break;
					}
				}


				// 2.2.2 油品配置表中不存在该油品配置, 则添加
				if (j >= gpIfsf_shm->oilCnt) {
					memset(&(gpIfsf_shm->oil_config[gpIfsf_shm->oilCnt]), 0, sizeof(OIL_CONFIG));
					memcpy(gpIfsf_shm->oil_config[gpIfsf_shm->oilCnt].oilno, &recvBuffer[12 + i], 4);
					/*
					 * CD 有可能先修改Prod_Description,
					 * 所以先拷贝当前Prod_Description 作为新增记录的油品名称
					 */
					memcpy(gpIfsf_shm->oil_config[gpIfsf_shm->oilCnt].oilname,
								pfp_pr_data->Prod_Description, 16);
					// 增加油品配置条数
					(gpIfsf_shm->oilCnt)++;

					RunLog("新增油品(%s)的配置信息,总油品数[%d]", 
						Asc2Hex((unsigned char *)&recvBuffer[12 + i], 4), gpIfsf_shm->oilCnt);
				}
#endif
				// 添加油品信息到配置列表
				if (AddProdToGlobalConfigList(&recvBuffer[12 + i], NULL) == 0) {
					// 2.2 更新该Node油枪配置中的油品号
					for (j = 0; j < gpIfsf_shm->gunCnt; ++j) {
						if (node == gpIfsf_shm->gun_config[j].node &&
							memcmp(pfp_pr_data->Prod_Nb, \
								gpIfsf_shm->gun_config[j].oil_no, 4) == 0) {
							RunLog("-- gun_config[%d].oil_no:%s", j,
									Asc2Hex(gpIfsf_shm->gun_config[j].oil_no, 4));
							memcpy(gpIfsf_shm->gun_config[j].oil_no, &recvBuffer[12 + i], 4);
						}
					}

					// 2.3 更新PR_DAT/C_DAT数据库
					if (memcmp(pfp_pr_data->Prod_Nb, zero4bytes, 4) == 0) {
						// 如果是添加油品, 则增加油品数量
						dispenser_data->fp_c_data.Nb_Products++;
					}

					memcpy(pfp_pr_data->Prod_Nb, &recvBuffer[12 + i], 4);
					gpIfsf_shm->cfg_changed = 0x0F;
					//RunLog("-----cfg_changed: %02x ", gpIfsf_shm->cfg_changed);
				} else {
					tmpDataAck = '\x01';
					tmpMSAck = '\x05'; 
				}
			} else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';
				tmpMSAck = '\x05'; 
				//UserLog("write  PR_DAT's Nb_Products error!fp state not in 1,2. ") 
			} else  { 
				tmpDataAck = '\x01';
				tmpMSAck = '\x05';
				//UserLog("write  PR_DAT's Nb_Products error!too big or small.") 
			}

			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case PR_Prod_Description:  // R(1-9),W(1-2)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)PR_Prod_Description;

			while (0xFF == gpIfsf_shm->cfg_changed) {
				RunLog("Waiting for SyncCfg2File.....");
				sleep(1);
			}

			// 0.油品数据库中不允许存在重复油品配置, 所以增加判断
			if ((memcmp(null_discription1, &recvBuffer[12 + i], 16) == 0) ||
					 (memcmp(null_discription2, &recvBuffer[12 + i], 16) == 0)) {
				j = FP_PR_CNT;
			} else {
				for (j = 0; j < FP_PR_CNT; ++j) {
					if ((memcmp(&(dispenser_data->fp_pr_data[j].Prod_Description),  \
									&recvBuffer[12 + i], 16) == 0) &&
									   (recvBuffer[9] - 0x41 != j)) {
						RunLog("油品(%s)的配置已存在于数据库中[%02x],不允许重复配置", 
							Asc2Hex((unsigned char *)&recvBuffer[12 + i], 16), 0x41 + j);
						break;
					}
				}
			}

			if (j < FP_PR_CNT) {
				// 若有重复项, 则回复NAK
				tmpDataAck = '\x03';
				tmpMSAck = '\x05'; 
			} else if ((lg == 16) && (fp_state < 3)) {
				// 1. 修改油品配置中的油品描述
				for (j = 0; j < gpIfsf_shm->oilCnt; ++j) {
					if (memcmp(pfp_pr_data->Prod_Nb, gpIfsf_shm->oil_config[j].oilno, 4) == 0) {
						RunLog("-- oil_config[%d].oilno:%s", j,
							       	Asc2Hex(gpIfsf_shm->oil_config[j].oilno, 4));

						memcpy(gpIfsf_shm->oil_config[j].oilname, &recvBuffer[12 + i], 16);

						break;
					}
				}

				if (j >= gpIfsf_shm->oilCnt) {
					RunLog("原始配置中未找到该油品(%s)的配置信息", 
						Asc2Hex((unsigned char *)&(gpIfsf_shm->oil_config[j].oilno), 4));
				}

				gpIfsf_shm->cfg_changed = 0x0F;

				// 2. 修改本地数据库中的油品描述
				memcpy(pfp_pr_data->Prod_Description, &recvBuffer[12 + i], 16);
				UserLog("write Prod_Description[%s]", Asc2Hex(pfp_pr_data->Prod_Description, 16));
			} else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';
				tmpMSAck = '\x05'; 
				//UserLog("write  PR_DAT's Nb_Products error!fp state not in 1,2. ") 
			} else  { 
				tmpDataAck = '\x01';
				tmpMSAck = '\x05';
				//UserLog("write  PR_DAT's Nb_Products error!too big or small.") 
			}			

			tmp_lg++;
			buffer[tmp_lg] = tmpDataAck;
			i = i + 2 + lg;
			break;
		default:
			tmp_lg++;
			buffer[tmp_lg] =recvBuffer[10+ i];
			tmpDataAck = '\x04';  
			tmpMSAck = '\x05';
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		}
	}
	
	if( (i + 2) != recv_lg )	{
		//UserLog("write PR_Dat error!! exit. ");
		UserLog("i  != recv_lg-2, write PR_Dat error!! exit, recv_lg[%d],i [%d] ",recv_lg, i);
		//exit(0);
		return -2;
	}
	tmp_lg++;
	buffer[6] = (unsigned char )((tmp_lg-8) / 256); //M_Lg
	buffer[7] = (unsigned char )((tmp_lg-8) % 256);  //big end.
	buffer[10] = tmpMSAck;  
	if(NULL != msg_lg) 
		*msg_lg= tmp_lg;
	if(NULL != sendBuffer)
		memcpy(sendBuffer, buffer, tmp_lg);
	return 0;		
}
int SetSglPR_DATA(const char node, const unsigned char pr_ad, const unsigned char *data){
	int i,  fp_state,lg ;
	int m=(int )pr_ad- 0x41;//0,1,..7.
	DISPENSER_DATA *dispenser_data;
	FP_PR_DATA *pfp_pr_data;
	//unsigned char node = recvBuffer[2];
	unsigned char tmpDataAck; // tmpMSAck ; 
	//unsigned char buffer[64];
	//memset(buffer,0, 64);
	if( (node <= 0) || (node > 127 ) ){
		UserLog("加油机节点值错误,油品SetSgl 失败");
		return -1;
		}
	if( (m< 0) || (m > 8 ) ){
		UserLog("加油机计量器地址值错误,油品SetSgl 失败");
		return -1;
	}
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,油品SetSgl 失败");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //找到了加油机节点
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,油品SetSgl 失败");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	pfp_pr_data =  (FP_PR_DATA *)( &(dispenser_data->fp_pr_data[m]) );
	for(i=0; i < MAX_FP_CNT; i++){ 
		fp_state = dispenser_data->fp_data[i].FP_State;
		if( fp_state >9 ) return -1;		
	}
	if( fp_state >3 ) return -1;		
	
	tmpDataAck = '\0'; //ok ack.
	lg =  (int )data[1]; //Data_Lg.
	switch( data[0] )
	{
		case PR_Prod_Nb://(1-8),w(1-2)			
			if( ( lg== 4 ) && (fp_state < 3)   ) {
				for(i =0;i < lg; i++)
					pfp_pr_data->Prod_Nb[i] = data[2+i];
				
			}
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';				
				//UserLog("write  PR_DAT's Nb_Products error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';
				//UserLog("write  PR_DAT's Nb_Products error!too big or small.") 
				}			
			break;		
		default:			
			tmpDataAck = '\x04';  			
			break;
	}	
	
	return tmpDataAck;		
	}
//设置Prod_Nb[4]的值.prod_nb为具体值.
int SetPR_DATA_Nb(const char node, const unsigned char pr_ad, const unsigned char *prod_nb){
	int i,  fp_state,lg ;
	int m=(int )pr_ad- 0x41;//0,1,..7.
	DISPENSER_DATA *dispenser_data;
	FP_PR_DATA *pfp_pr_data;
	//unsigned char node = recvBuffer[2];
	unsigned char tmpDataAck; // tmpMSAck ; 
	//unsigned char buffer[64];
	//memset(buffer,0, 64);
	if( (node <= 0) || (node > 127 ) ){
		UserLog("加油机节点值错误,油品Set油号失败");
		return -1;
		}
	if( (m< 0) || (m > 8 ) ){
		UserLog("加油机计量器地址值错误,油品Set油号 失败");
		return -1;
	}
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,油品Set油号 失败");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //找到了加油机节点
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,油品Set油号 失败");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	pfp_pr_data =  (FP_PR_DATA *)( &(dispenser_data->fp_pr_data[m]) );
	for(i=0; i < MAX_FP_CNT; i++){ 
		fp_state = dispenser_data->fp_data[i].FP_State;
		if( fp_state >9 ) return -1;		
	}
	if( fp_state >3 )
		return -1;
		//case PR_Prod_Nb://(1-8),w(1-2)		
	for(i =0;i < 4; i++)
		pfp_pr_data->Prod_Nb[i] = prod_nb[i];
	
			
	return 0;		
}
int RequestPR_DATA(const char node,const unsigned char pr_ad){
	int i,  fp_state, tmp_lg, lg ;
	int m=(int )pr_ad- 0x41;//0,1,..7.
	DISPENSER_DATA *dispenser_data;
	FP_PR_DATA *pfp_pr_data;
	//unsigned char node = recvBuffer[2];
	unsigned char tmpDataAck; // tmpMSAck ; 
	unsigned char buffer[64];
	memset(buffer,0, 64);
	if( (node <= 0) || (node > 127 ) ){
		UserLog("加油机节点值错误,油品Request 失败");
		return -1;
		}
	if( (m< 0) || (m > 8 ) ){
		UserLog("加油机计量器地址值错误,油品Request 失败");
		return -1;
	}
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,油品Request 失败");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //找到了加油机节点
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,油品Request 失败");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	pfp_pr_data =  (FP_PR_DATA *)( &(dispenser_data->fp_pr_data[m]) );
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
	buffer[9] = pr_ad; //recvBuffer[9]; //Ad
	//buffer[10] = recvBuffer[10]; //It's Data_Id. Data begin.
	tmp_lg=9;
	
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)PR_Prod_Nb  ;
	
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
/*
//每种加油模式下的油品. 见IFSF 3-01,V2.44,91-93, 地址:61h+00000001-99999999BCD+11h-18h.
enum FP_FM_DAT_AD{Fuelling_Mode_Name = 1,  Prod_Price = 2, Max_Vol,
	Max_Fill_Time, Max_Auth_Time, User_Max_Volume }; //gFp_fm_dat_ad;
typedef struct{
	//unsigned char Prod_Nb[4];  //为了与PR_ID对应,PCD新增加的.没有地址号.
	//CONFIGURATION:
	 //unsigned char Fuelling_Mode_Name[8]; //O//
	unsigned char Prod_Price[5];
	unsigned char Max_Vol[5];
	unsigned char Max_Fill_Time;
	unsigned char Max_Auth_Time;
	unsigned char User_Max_Volume[5];	
	}FP_FM_DATA;
*/
//FP_FM_DATA操作函数. node加油机逻辑节点号,pr_nb油品编号(1-99999999),pr_ad油品地址41h-48h,
int DefaultFM_DAT(const char node, const unsigned char pr_ad) {
	int i, fp_state;
	int m=pr_ad-0x41; //0,1,2,...7
	//int er=er_ad-1; //0,1,2,3,4,5,6,..63号错误.
	//unsigned char tmpUChar;
	DISPENSER_DATA *dispenser_data;
	FP_FM_DATA *pfp_fm_data;
	
	if( (node <= 0) || (node > 127 ) ){
		UserLog("加油机节点值错误,加油模式初始化 失败");
		return -1;
	}
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,加油模式初始化 失败");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //找到了加油机节点
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,加油模式初始化 失败");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	for(i=0; i < MAX_FP_CNT; i++){ 
		fp_state = dispenser_data->fp_data[i].FP_State;
		if( fp_state >9 ) return -1;		
	}
	if( fp_state >2 ) {
		UserLog("加油点状态大于2,不能初始化配置.");
		return -1;
	}

	for (i = 0; i < FP_FM_CNT; ++i) {
		pfp_fm_data = (FP_FM_DATA *)(&(dispenser_data->fp_fm_data[m][i]));

		pfp_fm_data->Prod_Price[0] = '\06'; // for miandian, no point
		pfp_fm_data->Prod_Price[1] = '\0';
		pfp_fm_data->Prod_Price[2] = '\x01';
		pfp_fm_data->Prod_Price[3] = '\0';  //default RMB 1.00
			
		memset(pfp_fm_data->Max_Vol, 0, 5);
		pfp_fm_data->Max_Fill_Time = '\0';
		pfp_fm_data->Max_Auth_Time = '\0';
		memset(pfp_fm_data->User_Max_Volume, 0 , 5);
#if 0
		RunLog("DEBUG.Node %d PR %d FM %02x: Price: %02x%02x%02x%02x", node, m + 1, i + 1, 
			pfp_fm_data->Prod_Price[0], pfp_fm_data->Prod_Price[1],
			pfp_fm_data->Prod_Price[2], pfp_fm_data->Prod_Price[3]);
#endif
	}
	
	return 0;
}
int SetFM_DAT(const char node, const unsigned char pr_ad, const FP_FM_DATA *fp_fm_data){//初始化,注意与FP_PR_DATA对应!!
	int i, fp_state;
	int m=pr_ad-0x41; //0,1,2,...7	
	DISPENSER_DATA *dispenser_data;
	FP_FM_DATA *pfp_fm_data;
	
	if( (node <= 0) || (node > 127 ) ){
		UserLog("加油机节点值错误,加油模式Set  失败");
		return -1;
		}
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,加油模式Set  失败");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //找到了加油机节点
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到, 加油模式Set  失败");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	pfp_fm_data =  (FP_FM_DATA *)( &(dispenser_data->fp_fm_data[m][0])  ); //only one fuelling mode.
	for(i=0; i < MAX_FP_CNT; i++){ 
		fp_state = dispenser_data->fp_data[i].FP_State;
		if( fp_state >9 ) return -1;		
	}
	if( fp_state >2 ) {
		UserLog("加油点状态大于2,加油模式Set  失败");
		return -1;
	}
	
	memcpy(pfp_fm_data, fp_fm_data, sizeof(FP_FM_DATA));
	
	return 0;
}
//读取FP_FM_DATA. FP_FM_DATA地址为11h(11h-18h).构造的信息数据的类型为001回答
//recvBuffer为收到的data,sendBuffer为要发送的data,msg_lg为生成的总长度.
int ReadFM_DATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg){
	int i,k, fp_state, recv_lg, tmp_lg ;
	int m=(int )recvBuffer[14] - 0x11;//0,1,..7.
	DISPENSER_DATA *dispenser_data;
	FP_FM_DATA *pfp_fm_data;
	FP_PR_DATA *pfp_pr_data;
	unsigned char node = recvBuffer[1];
	unsigned char buffer[128];
	memset(buffer,0, 128);
	if( (NULL == recvBuffer)  || (NULL == sendBuffer) ){
		UserLog(" 参数空错误,油品模式Read 失败");
		return -1;
	}	
	if( (node <= 0) || (node > 127 ) ){
		UserLog("加油机节点值错误,油品模式Read 失败");
		return -1;
	}
	if (m < 0 || m > 7) {
		UserLog("加油机油品模式地址值错误,油品模式Read 失败");
		return -1;
	}
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,油品模式Read 失败");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //找到了加油机节点
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,油品模式Read 失败");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );	
	fp_state = 1;
	for(i=0; i < MAX_FP_CNT; i++){ 
		if(fp_state < dispenser_data->fp_data[i].FP_State)
			fp_state = dispenser_data->fp_data[i].FP_State;
		if( fp_state >9 ) 
			return -1;		
	}
	for(i=0; i<FP_PR_CNT; i++){ //search prod_nb address in pr_data.
		pfp_pr_data =  (FP_PR_DATA *)( &(dispenser_data->fp_pr_data[i]) );
		if (memcmp(pfp_pr_data->Prod_Nb, (unsigned char *)(&recvBuffer[10]), 4) == 0) {
		    	pfp_fm_data = (FP_FM_DATA *)(&(dispenser_data->fp_fm_data[i][(recvBuffer[14] - 0x11)]));
			break;
		}
	}
	if (i == FP_PR_CNT){
		UserLog("没有找到油品号.不能read 油品模式.");
		return -1;
	}

	buffer[0] = recvBuffer[2];
	buffer[1] = recvBuffer[3];
	buffer[2] = recvBuffer[0];
	buffer[3] = recvBuffer[1];
	buffer[4] = '\0'; //recvBuffer[4]; //IFSF_MC
	buffer[5] = (recvBuffer[5] |0x20); //0x20:answer.
	recv_lg = recvBuffer[6];
	recv_lg = recv_lg * 256 + recvBuffer[7];
	buffer[8] = '\x06'; //recvBuffer[8]; //Ad_lg
	buffer[9] = recvBuffer[9]; //Ad ,61h
	buffer[10] = recvBuffer[10];
	buffer[11] = recvBuffer[11];
	buffer[12] = recvBuffer[12];
	buffer[13] = recvBuffer[13];
	buffer[14] = recvBuffer[14];
	//buffer[10] = recvBuffer[10]; //It's Data_Id. Data begin.
	tmp_lg = 14; //ad_lg=1,then 9.
	for(i=0; i < (recv_lg - 7); i++){
		switch( recvBuffer[15+i] ){			
		case FM_Fuelling_Mode_Name:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FM_Fuelling_Mode_Name;
			tmp_lg++;
			buffer[tmp_lg] = '\0';			
			break;		
		case FM_Prod_Price:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FM_Prod_Price;
			tmp_lg++;
			buffer[tmp_lg] ='\x04';
			k=buffer[tmp_lg];
			for(i=0;i<k;i++){
				tmp_lg++;
				buffer[tmp_lg] = pfp_fm_data->Prod_Price[i];
			}
			break;
		case FM_Max_Vol:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FM_Max_Vol;
			tmp_lg++;
			buffer[tmp_lg] ='\x05';
			k=buffer[tmp_lg];
			for(i=0;i<k;i++){
				tmp_lg++;
				buffer[tmp_lg] = pfp_fm_data->Max_Vol[i];
			}
			break;
		case FM_Max_Fill_Time:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FM_Max_Fill_Time;
			tmp_lg++;
			buffer[tmp_lg] ='\x01';			
			tmp_lg++;
			buffer[tmp_lg] = pfp_fm_data->Max_Fill_Time;			
			break;
		case FM_Max_Auth_Time:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FM_Max_Auth_Time;
			tmp_lg++;
			buffer[tmp_lg] ='\x01';			
			tmp_lg++;
			buffer[tmp_lg] = pfp_fm_data->Max_Auth_Time;			
			break;
		case FM_User_Max_Volume:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FM_User_Max_Volume;
			tmp_lg++;
			buffer[tmp_lg] ='\x05';
			k=buffer[tmp_lg];
			for(i=0;i<k;i++){
				tmp_lg++;
				buffer[tmp_lg] = pfp_fm_data->User_Max_Volume[i];
			}
			break;
		default:
				tmp_lg++;
				buffer[tmp_lg] = recvBuffer[15+i];
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
//写FP_PR_DATA函数.recvBuffer为收到的data,sendBuffer为要回复的data,构造的信息数据的类型为111应答
//msg_lg为生成的总长度,为0表示不需要应答信息,sendBuffer和msg_lg不需要应答信息可以设置为NULL.
int WriteFM_DATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg){
	int i,j, fp_state, recv_lg, tmp_lg,  lg ;
	int m=(int )recvBuffer[14] - 0x11;//0,1,..7.
	int pr_idx = 0;
	int fp_idx, lnz_idx;
	DISPENSER_DATA *dispenser_data;
	FP_FM_DATA *pfp_fm_data;
	FP_PR_DATA *pfp_pr_data;
	unsigned char node = recvBuffer[1];
	unsigned char tmpMSAck, tmpDataAck;
	int cmd_lg;
	unsigned char buffer[128],cmd[17];
	memset(buffer,0, sizeof(buffer));
	unsigned char unit_price[4];
	unsigned char chnlog;
	
	if( (NULL == recvBuffer)  || (NULL == sendBuffer) ){
		UserLog(" 参数空错误,油品模式Write 失败");
		return -1;
	}
	if( (node <= 0) || (node > 127 ) ){
		UserLog("加油机节点值错误,油品模式Write 失败");
		return -1;
	}
	if (m < 0 || m > 7) {
		UserLog("加油机油品模式地址值错误,油品模式Write 失败");
		return -1;
	}
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,油品模式Write 失败");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //找到了加油机节点
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,油品模式Write 失败,node:[%d]", node);
		return -1;
	}


	chnlog = gpIfsf_shm->node_channel[i].chnLog;

	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	fp_state = 1;
	for(i=0; i < MAX_FP_CNT; i++){ 
		if(fp_state < dispenser_data->fp_data[i].FP_State)
			fp_state = dispenser_data->fp_data[i].FP_State;
		if( fp_state >9 ) 
			return -1;		
	}

	RunLog("PR_Nb:recvBuffer: %s", Asc2Hex((unsigned char *)&recvBuffer[10], 4));
	for(i=0; i<FP_PR_CNT; i++){ //search prod_nb address in pr_data.
		pfp_pr_data =  (FP_PR_DATA *)( &(dispenser_data->fp_pr_data[i]));
		RunLog("PR_Nb:pr_data[%d]: %s", i, Asc2Hex((unsigned char *)&(pfp_pr_data->Prod_Nb[0]), 4));
		if (memcmp((unsigned char *)(&(pfp_pr_data->Prod_Nb[0])),
				      	(unsigned char *)(&recvBuffer[10]), 4) == 0) {			
		    	pfp_fm_data = (FP_FM_DATA *)(&(dispenser_data->fp_fm_data[i][(recvBuffer[14] - 0x11)]));
			pr_idx = i;
			break;
		}
	}	

	buffer[0] = recvBuffer[2];
	buffer[1] = recvBuffer[3];
	buffer[2] = recvBuffer[0];
	buffer[3] = recvBuffer[1];
	buffer[4] = '\0'; //recvBuffer[4]; //IFSF_MC
	buffer[5] = ( (recvBuffer[5] &0x1f)  |0xe0); //0x20:answer, 0xE0:ack.
	recv_lg = recvBuffer[6];
	recv_lg = recv_lg * 256 + recvBuffer[7];
	buffer[8] = '\x06'; //recvBuffer[8]; //Ad_lg
	buffer[9] = recvBuffer[9]; //Ad ,61h
	buffer[10] = recvBuffer[10];
	buffer[11] = recvBuffer[11];
	buffer[12] = recvBuffer[12];
	buffer[13] = recvBuffer[13];
	buffer[14] = recvBuffer[14];
	//buffer[15] = ; //MS_Ack
	//buffer[16] = recvBuffer[15]; //recvBuffer[15],It's Data_Id. Data begin.
	tmp_lg = 15; //ad_lg=1,then 9.

	// 记录当前油品配置以及油枪状态到 CRITICAL_LOG
	price_change_track();

	if (i == FP_PR_CNT){
		CriticalLog("节点[%02d]收到变价命令[%s]--油品号[%02x%02x%02x%02x]模式[%02x]",
				node, Asc2Hex(recvBuffer, recv_lg + 8), recvBuffer[10],
				recvBuffer[11], recvBuffer[12], recvBuffer[13], recvBuffer[14]);

		UserLog("节点[%02d]中没有找到该号油品,无法修改该油品相关信息", node);
		CriticalLog("节点[%02d]中没有找到该号油品,无法修改该油品相关信息", node);
		PriceChangeErrLog("%02x.**.** xx.xx 35", node);

		//tmpMSAck = 6; //MS_Ack, data address not exist.
		tmpMSAck = 0; //MS_Ack, data address not exist.
		tmp_lg++;
		buffer[6] = (unsigned char )((tmp_lg-8) / 256) ;
		buffer[7] = (unsigned char )((tmp_lg-8) % 256);  //big end.
		buffer[15] = tmpMSAck;  
		if(NULL != msg_lg) *msg_lg= tmp_lg;
		if(NULL != sendBuffer)  memcpy(sendBuffer, buffer, tmp_lg);

#if 0
		if (SendData2CD(sendBuffer, tmp_lg, 5) < 0) {
			RunLog("发送回复数据[%s]失败", Asc2Hex(sendBuffer, tmp_lg));
		}
#endif

		// Error Code 无此油品配置
		ChangeER_DATA(node, 0x21, 0x35, NULL);

		return 0;
	}

	i=0;
	tmpMSAck = '\0'; // all is  ok.	
	while( i < (recv_lg -7) ) {
		tmpDataAck = '\0'; //ok ack.
		lg =  (int )recvBuffer[16+i]; //Data_Lg.
		switch( recvBuffer[15+i] ){			
		case FM_Fuelling_Mode_Name: //O
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)FM_Fuelling_Mode_Name;
			tmpDataAck = '\x04';  
			tmpMSAck = '\x05';
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;			
			break;		
		case FM_Prod_Price:			
			tmp_lg++;			
			buffer[tmp_lg] = (unsigned char)FM_Prod_Price;				
			// 若FP当前状态 >= 3, 那么等油机状态变为 < 3的时候再下发变价命令给油机
			if ((lg == 4) && (recvBuffer[14] >= 0x11) && (recvBuffer[14] <= 0x18) &&
							  (((recvBuffer[17+i]&0xF0) >>4) <= 9) &&
							  ((recvBuffer[17+i]&0x0F) <= 9) &&
							  (((recvBuffer[18+i]&0xF0) >>4) <= 9) &&
							  ((recvBuffer[18+i]&0x0F) <= 9) &&
							  (((recvBuffer[19+i]&0xF0) >>4) <= 9) &&
							  ((recvBuffer[19+i]&0x0F) <= 9) &&
							  (((recvBuffer[20+i]&0xF0) >>4) <= 9) &&
							  ((recvBuffer[20+i]&0x0F) <= 9)) {

				memcpy(unit_price, &recvBuffer[17], 4);
				//TwoDecPoint(unit_price, 4); // delete this for miandian

				// 更新数据库中该模式的单价
				memcpy(pfp_fm_data->Prod_Price, unit_price, 4);


				unsigned char flag_used = 0;
				for (fp_idx = 0; fp_idx < dispenser_data->fp_c_data.Nb_FP; ++fp_idx) {
					for (lnz_idx = 0; lnz_idx < MAX_FP_LN_CNT; ++lnz_idx) {
						if (dispenser_data->fp_ln_data[fp_idx][lnz_idx].PR_Id == pr_idx + 1) {
							// 该油品有在使用
							RunLog("---Node %d FP %d LNZ %d use PR %d",
									node, fp_idx + 1, lnz_idx + 1, pr_idx + 1);
							flag_used = 0xFF;
							break;
						}
					}
				}

				if (flag_used != 0xFF) {
					// 该油品在此节点没有实际使用, 则直接回复成功(Error Code 0x99)
					// 不再需要把变价指令发送给油机处理进程;
					for (fp_idx = 0; fp_idx < dispenser_data->fp_c_data.Nb_FP; ++fp_idx) {
						ChangeER_DATA(node, 0x21 + fp_idx, 0x99, NULL);
					}
				} else {

				// ---------------------------------------------------------------------
#if 0
				// commented by guojie Zhu @ 2009.11.04, 之前已经判断过, 无需再次判断
				for (j = 0; j < FP_PR_CNT; j++) {
					if(memcmp(dispenser_data->fp_pr_data[j].Prod_Nb, &recvBuffer[10], 4) == 0) {
						break;
					}
				}

				if (j == FP_PR_CNT){
					tmpDataAck = '\x01';
					tmpMSAck = '\x05'; 
					tmp_lg++;
					buffer[tmp_lg] = tmpDataAck;
					i = i + 2 + lg;
					break;
				}
#endif
					cmd[0] = '\x18';
					cmd[1] = '\0';
					cmd[2] = pr_idx + 0x41;              // pr_ad
					memcpy(&cmd[3], &recvBuffer[10], 4); // Prod_Nb
					cmd[7] = recvBuffer[14];             // FM_ID
					memcpy(&cmd[8], unit_price, 4);

			CriticalLog("节点[%02d]收到变价命令[%s]--油品号[%02x%02x%02x%02x]模式[%02x]目标单价[%02x%02x]", // delete point for miandian
						node, Asc2Hex(recvBuffer, recv_lg + 8), recvBuffer[10],
						recvBuffer[11], recvBuffer[12], recvBuffer[13],
						recvBuffer[14], unit_price[2], unit_price[3]);


					cmd_lg = 8 + lg;
					j = SendMsg(ORD_MSG_TRAN, cmd, cmd_lg, chnlog, 3);
					if (j < 0) {
						RunLog("转发变价命令[%s]给通道[%d]失败!", Asc2Hex(cmd, cmd_lg), chnlog);
						return -1;
					}

					// 更新油品配置文件中的单价, 统一使用 SyncCfg2File 更新数据到配置文件中
					for (j = 0; j < gpIfsf_shm->oilCnt; ++j) {
						RunLog("oil_config[%d].oilno:%s",
							       	j, Asc2Hex(gpIfsf_shm->oil_config[j].oilno, 4));
						if (memcmp(&recvBuffer[10], gpIfsf_shm->oil_config[j].oilno, 4) == 0) {
							RunLog("-- oil_config[%d].oilno:%s", j,
									Asc2Hex(gpIfsf_shm->oil_config[j].oilno, 4));
							memcpy(gpIfsf_shm->oil_config[j].price, &unit_price[1], 3);
							break;
						}
					}

					if (j >= gpIfsf_shm->oilCnt) {
						RunLog("原始配置中未找到该油品(%s)的配置信息", 
								Asc2Hex((unsigned char *)&recvBuffer[10], 4));
					}

					gpIfsf_shm->cfg_changed = 0x0F;
	//				RunLog("-----cfg_changed: %02x ", gpIfsf_shm->cfg_changed);
				}
			} else  { 
				tmpDataAck = '\x01';
				tmpMSAck = '\x05';
				//UserLog("write  C_DAT's Nb_Products error!too big or small.") 
			}			
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case FM_Max_Vol:
			tmp_lg++;			
			buffer[tmp_lg] = (unsigned char)FM_Max_Vol;
			/*
			if( ( lg == 7 ) && (fp_state < 3)    ) 
				for(j=0; j<lg; j++)
					pfp_fm_data->Max_Vol[j] = recvBuffer[18+i+j];
			else  if (fp_state < 3)  { 
				tmpDataAck = '\x02';
				tmpMSAck = '\x05'; 
				//UserLog("write  FM_DAT error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';
				tmpMSAck = '\x05';
				//UserLog("write  FM_DAT error!too big or small.") 
				}
			*/
			tmpDataAck = '\x02';
			tmpMSAck = '\x05'; 
			
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case FM_Max_Fill_Time:
			tmp_lg++;			
			buffer[tmp_lg] = (unsigned char)FM_Max_Fill_Time;	
			/*
			if( (  lg== 1 ) && (fp_state < 3)    ) 				
					pfp_fm_data->Max_Fill_Time = recvBuffer[18+i];
			else  if (fp_state < 3)  { 
				tmpDataAck = '\x02';
				tmpMSAck = '\x05'; 
				//UserLog("write  FM_DAT error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';
				tmpMSAck = '\x05';
				//UserLog("write  FM_DAT error!too big or small.") 
				}
			*/
			tmpDataAck = '\x02';
			tmpMSAck = '\x05'; 
			
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;	
			break;
		case FM_Max_Auth_Time:
			tmp_lg++;			
			buffer[tmp_lg] = (unsigned char)FM_Max_Auth_Time;
			/*
			if( (  lg== 1 ) && (fp_state < 3)    ) 				
					pfp_fm_data->Max_Auth_Time = recvBuffer[18+i];
			else  if (fp_state < 3)  { 
				tmpDataAck = '\x02';
				tmpMSAck = '\x05'; 
				//UserLog("write  FM_DAT error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';
				tmpMSAck = '\x05';
				//UserLog("write  FM_DAT error!too big or small.") 
				}
			*/
			tmpDataAck = '\x02';
			tmpMSAck = '\x05'; 
			
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;	
			break;
		case FM_User_Max_Volume:
			tmp_lg++;			
			buffer[tmp_lg] = (unsigned char)FM_User_Max_Volume;
			/*
			if( ( lg == 7 ) && (fp_state < 3)    ) 
				for(j=0; j<lg; j++)
					pfp_fm_data->User_Max_Volume[j] = recvBuffer[18+i+j];
			else  if (fp_state < 3)  { 
				tmpDataAck = '\x02';
				tmpMSAck = '\x05'; 
				//UserLog("write  FM_DAT error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';
				tmpMSAck = '\x05';
				//UserLog("write  FM_DAT error!too big or small.") 
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
			buffer[tmp_lg] =recvBuffer[15+ i]  ;
			tmpDataAck = '\x04';  
			tmpMSAck = '\x05';
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;	
		}	
	}
	if( (i + 7) != recv_lg )	{
		//UserLog("write FM_Dat error!! exit. ");
		UserLog("i  != recv_lg-7, write FM_Dat error!! exit, recv_lg[%d],i [%d] ",recv_lg, i);
		//exit(0);
		return -2;
	}
	tmp_lg++;
	buffer[6] = (unsigned char )((tmp_lg-8) / 256) ;
	buffer[7] = (unsigned char )((tmp_lg-8) %256);  //big end.
	buffer[15] = tmpMSAck;  
	if(NULL != msg_lg) *msg_lg= tmp_lg;
	if(NULL != sendBuffer)  memcpy(sendBuffer, buffer, tmp_lg);

	return 0;	
}

int SetSglFM_DAT(const char node, const unsigned char pr_ad, const unsigned char  *data){
	int i,j, fp_state,  lg ;
	int m=(int )pr_ad - 0x11;//0,1,..7.
	DISPENSER_DATA *dispenser_data;
	FP_FM_DATA *pfp_fm_data;
	FP_PR_DATA *pfp_pr_data;
	//unsigned char node = recvBuffer[2];
	unsigned char tmpDataAck; //tmpMSAck
	//unsigned char buffer[128];
	//memset(buffer,0, 128);
	if( NULL == data)  {
		UserLog(" 参数空错误,油品模式SetSgl 失败");
		return -1;
		}
	if( (node <= 0) || (node > 127 ) ){
		UserLog("加油机节点值错误,油品模式SetSgl 失败");
		return -1;
	}
	if (m < 0 || m > 7) {
		UserLog("加油机油品模式地址值错误,油品模式SetSgl 失败");
		return -1;
	}
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,油品模式SetSgl 失败");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //找到了加油机节点
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,油品模式SetSgl 失败");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );	
	for(i=0; i < MAX_FP_CNT; i++){ 
		fp_state = dispenser_data->fp_data[i].FP_State;
		if( fp_state >9 ) return -1;		
	}
	
	pfp_pr_data =  (FP_PR_DATA *)( &(dispenser_data->fp_pr_data[m]) );	
	pfp_fm_data =  (FP_FM_DATA *)( &(dispenser_data->fp_fm_data[m][0]) );
 	
	
	
	tmpDataAck = '\0'; //ok ack.
	lg =  (int )data[1]; //Data_Lg.
	switch( data[0] ){			
		case FM_Fuelling_Mode_Name:			
			tmpDataAck = '\x04'; 				
			break;		
		case FM_Prod_Price: //w(1-9)				
			if( ( lg== 4 ) && (fp_state < 10)    ) 
				for(j=0; j<lg; j++)
					pfp_fm_data->Prod_Price[j] = data[2+j];
			else  if (fp_state >= 10)  { 
				tmpDataAck = '\x02';
				//UserLog("write  C_DAT's Nb_Products error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';
				//UserLog("write  C_DAT's Nb_Products error!too big or small.") 
				}
			break;
		case FM_Max_Vol://w(1-9)
			if( ( lg == 5 ) && (fp_state < 10)    ) 
				for(j=0; j<lg; j++)
					pfp_fm_data->Max_Vol[j] = data[2+j];
			else  if (fp_state >= 10)  { 
				tmpDataAck = '\x02';				
				//UserLog("write  FM_DAT error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';
				//UserLog("write  FM_DAT error!too big or small.") 
				}			
			break;
		case FM_Max_Fill_Time://w(1-9)
			if( (  lg== 1 ) && (fp_state < 10)    ) 				
					pfp_fm_data->Max_Fill_Time = data[2];
			else  if (fp_state >= 10)  { 
				tmpDataAck = '\x02';
				//UserLog("write  FM_DAT error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';
				//UserLog("write  FM_DAT error!too big or small.") 
				}			
			break;
		case FM_Max_Auth_Time: //w(1-9)
			if( (  lg== 1 ) && (fp_state < 10)    ) 				
					pfp_fm_data->Max_Auth_Time =data[2] ;
			else  if (fp_state >= 10)  { 
				tmpDataAck = '\x02';
				//UserLog("write  FM_DAT error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';
				//UserLog("write  FM_DAT error!too big or small.") 
				}			
			break;
		case FM_User_Max_Volume: //w(1-9)
			if( ( lg == 5 ) && (fp_state < 10)    ) 
				for(j=0; j<lg; j++)
					pfp_fm_data->User_Max_Volume[j] = data[2+j];
			else  if (fp_state >= 10)  { 
				tmpDataAck = '\x02';
				//UserLog("write  FM_DAT error!fp state not in 1,2. ") 
				}
			else  { 
				tmpDataAck = '\x01';
				//UserLog("write  FM_DAT error!too big or small.") 
				}	
			break;
		default:			
			tmpDataAck = '\x04';  
			break;	
	}	

	return tmpDataAck;	
}
int SetFM_DAT_Price(const char node, const unsigned char pr_ad, const unsigned char  *price){
	int i,j, fp_state,  lg, cmd_lg;
	int m=(int )pr_ad - 0x11;//0,1,..7.
	DISPENSER_DATA *dispenser_data;
	FP_FM_DATA *pfp_fm_data;
	FP_PR_DATA *pfp_pr_data;
	//unsigned char node = recvBuffer[2];
	unsigned char tmpDataAck; //tmpMSAck
	unsigned char cmd[17];
	unsigned char chnlog;
	memset(cmd,0, 17);
	//unsigned char buffer[128];
	//memset(buffer,0, 128);
	if( NULL == price)  {
		UserLog(" 参数price空错误,油品模式Set Price 失败");
		return -1;
		}
	if( (node <= 0) || (node > 127 ) ){
		UserLog("加油机节点值错误,油品模式Set Price 失败");
		return -1;
	}
	if (m < 0 || m > 7) {
		UserLog("加油机油品模式地址值错误,油品模式Set Price  失败");
		return -1;
	}
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,油品模式Set Price  失败");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //找到了加油机节点
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,油品模式Set Price  失败");
		return -1;
	}

	chnlog = gpIfsf_shm->node_channel[i].chnLog;
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );	
	for(i=0; i < MAX_FP_CNT; i++){ 
		fp_state = dispenser_data->fp_data[i].FP_State;
		if( fp_state >9 ) return -1;		
	}
	
	pfp_pr_data =  (FP_PR_DATA *)( &(dispenser_data->fp_pr_data[m]) );	
	pfp_fm_data =  (FP_FM_DATA *)( &(dispenser_data->fp_fm_data[m][0])  );
 	
		
	tmpDataAck = '\0'; //ok ack.
	lg=4	;				
	if(  fp_state < 10   ) 
		for(j=0; j<lg; j++){
			pfp_fm_data->Prod_Price[j] = price[j];
			cmd[0] = '\x18';
			cmd[1] = '\0';
			cmd[2] = '\x01';
			for(j=0; j<4; j++)
				cmd[3+j] = pfp_pr_data->Prod_Nb[j];
			for(j=0; j<lg; j++)
				cmd[7+j] = price[1+j];
			cmd_lg=8+lg;
			j = SendMsg(ORD_MSG_TRAN, cmd, cmd_lg, chnlog, 3);
			if(j<0 ){
				UserLog("下发油价错误!!, 通道号: %d", chnlog);
				exit(0);
			}
	} else  if (fp_state >= 10)  { 
		tmpDataAck = '\x02';				
		//UserLog("write  C_DAT's Nb_Products error!fp state not in 1,2. ") 
	} else  { 
		tmpDataAck = '\x01';
		//UserLog("write  C_DAT's Nb_Products error!too big or small.") 
	}			
				

	return tmpDataAck;	
}
int RequestFM_DAT(const char node, const unsigned char pr_ad){ //fm is only 1,ad is 11h.
	int i,k, fp_state, tmp_lg ;
	int m=(int )pr_ad- 0x11;//0,1,..7.
	DISPENSER_DATA *dispenser_data;
	FP_FM_DATA *pfp_fm_data;
	FP_PR_DATA *pfp_pr_data;
	//unsigned char node = recvBuffer[2];
	unsigned char buffer[64];
	memset(buffer,0, 64);
	
	if( (node <= 0) || (node > 127 ) ){
		UserLog("加油机节点值错误,油品模式Request 失败");
		return -1;
	}
	if (m < 0 || m > 7) {
		UserLog("加油机油品模式地址值错误,油品模式Request 失败");
		return -1;
	}
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,油品模式Request 失败");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //找到了加油机节点
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,油品模式Request 失败");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );	
	for(i=0; i < MAX_FP_CNT; i++){ 
		fp_state = dispenser_data->fp_data[i].FP_State;
		if( fp_state >9 ) return -1;		
	}
	
	pfp_pr_data =  (FP_PR_DATA *)( &(dispenser_data->fp_pr_data[m]) );		
	pfp_fm_data =  (FP_FM_DATA *)( &(dispenser_data->fp_fm_data[m][0]) );
	

	buffer[0] = '\x02';
	buffer[1] = '\x01';;
	buffer[2] = '\x01';;
	buffer[3] = (unsigned char)node;
	buffer[4] = '\0'; //recvBuffer[4]; //IFSF_MC
	buffer[5] = '\0'; //0x00:read.
	//recv_lg = recvBuffer[6];
	//recv_lg = recv_lg * 256 + recvBuffer[7];
	buffer[8] = '\x06'; //recvBuffer[8]; //Ad_lg
	buffer[9] = '\x61'; //Ad ,61h
	buffer[10] = pfp_pr_data->Prod_Nb[0];
	buffer[11] = pfp_pr_data->Prod_Nb[1];
	buffer[12] = pfp_pr_data->Prod_Nb[2];
	buffer[13] = pfp_pr_data->Prod_Nb[3];
	buffer[14] = '\x11';//fm_ad,11h,only 1 in this design.
	//buffer[15] = recvBuffer[15]; //It's Data_Id. Data begin.
	tmp_lg = 14; //ad_lg=1,then 9.
	
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)FM_Prod_Price;
	/*
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)FM_Max_Vol;			
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)FM_Max_Fill_Time;
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)FM_Max_Auth_Time;	
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)FM_User_Max_Volume;
	*/
	tmp_lg++;
	buffer[6] = (unsigned char )(tmp_lg / 256) ;
	buffer[7] = (unsigned char )(tmp_lg %256);  //big end.
	//i use to be ret.
	i= SendData2CD(buffer, tmp_lg, 5);
	if (i <0 ){
		UserLog("加油机通讯错误,计量器不能Request ");
		return -1;
	}
	return 0;
}

/*
 * @brief   根据油品号Prod_Nb判断该油品当前是否在用
 * @param   prod_nb   需要检查的油品号
 * @return  1 - 在用, 0 - 不在用
 */
static int IsProdInUse(const __u8 *prod_nb)
{
	int i, pr_idx;
	int is_use = 0;
	FP_PR_DATA *pfp_tr_dat = NULL;

	for (i = 0; i < MAX_CHANNEL_CNT; ++i) {
		if (gpIfsf_shm->node_channel[i].node == 0) {
			continue;
		}

		for (pr_idx = 0; pr_idx < FP_PR_CNT; ++pr_idx) {
			if (memcmp(prod_nb, gpIfsf_shm->dispenser_data[i].fp_pr_data[pr_idx].Prod_Nb, 4) == 0) {
				is_use = 1;
				return is_use;
			}
		}
	}

	return is_use;
}

/*
 *@brief    添加油品配置到FCC全局油品配置列表中
 *@param    prod_nb            油品号
 *@param    prod_description   油品描述
 *@return   0 - 成功, -1 - 失败
 */
static int AddProdToGlobalConfigList(const __u8 *prod_nb, const __u8 *prod_description)
{
	int i;
	int empty_idx = 0;

	// 1. 是否已存在
	for (i = 0; i < gpIfsf_shm->oilCnt; ++i) {
		RunLog("oil_config[%02d].oilno:%s", i, Asc2Hex(gpIfsf_shm->oil_config[i].oilno, 4));
		if (memcmp(prod_nb, gpIfsf_shm->oil_config[i].oilno, 4) == 0) {
			RunLog("油品(%s)的配置信息已存在于配置列表中", Asc2Hex(prod_nb, 4));
			return 0;
		}
	}

	// 2. 油品配置表中不存在该油品配置, 则添加
	if (gpIfsf_shm->oilCnt >= MAX_OIL_CNT) {
		// 查找可用配置空间
		for (i = 0; i < gpIfsf_shm->oilCnt; ++i) {
			if (IsProdInUse(gpIfsf_shm->oil_config[i].oilno) == 0) {
				empty_idx = i;
				break;
			}
		}
		if (i >= gpIfsf_shm->oilCnt) {
			RunLog("添加油品[%s]失败, FCC全局配置列表已满", Asc2Hex(prod_nb, 4));
			return -1;
		}
	} else {
		empty_idx = gpIfsf_shm->oilCnt;
	}

	memset(&(gpIfsf_shm->oil_config[empty_idx]), 0, sizeof(OIL_CONFIG));
	memcpy(gpIfsf_shm->oil_config[empty_idx].oilno, prod_nb, 4);
	if (prod_description != NULL) {
		memcpy(gpIfsf_shm->oil_config[empty_idx].oilname, prod_description, 16);
	} else {
		snprintf(gpIfsf_shm->oil_config[empty_idx].oilname, 8 + 1, "%s", Asc2Hex(prod_nb, 4));
	}
	gpIfsf_shm->oil_config[empty_idx].price[0] = 0x00;
	gpIfsf_shm->oil_config[empty_idx].price[1] = 0x01;
	gpIfsf_shm->oil_config[empty_idx].price[2] = 0x00;

	// 3. 增加油品配置条数
	gpIfsf_shm->oilCnt = (gpIfsf_shm->oilCnt >= MAX_OIL_CNT ? MAX_OIL_CNT : (++gpIfsf_shm->oilCnt));

	RunLog("新增油品(%s)的配置信息,总油品数[%d]", Asc2Hex(prod_nb, 4), gpIfsf_shm->oilCnt);
	RunLog("oil_config[%02d].oilno:%s", empty_idx, Asc2Hex(gpIfsf_shm->oil_config[empty_idx].oilno, 4));

	return 0;
}


/*-----------------------------------------------------------------*/




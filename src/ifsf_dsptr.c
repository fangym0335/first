/*******************************************************************
交易操作函数
-------------------------------------------------------------------
2007-07-04 by chenfm
2008-02-28 - Guojie Zhu <zhugj@css-intelligent.com>
    增加函数ReachedUnPaidLimit 用于判断未支付交易笔数是否达到限制
2008-03-06 - Guojie Zhu <zhugj@css-intelligent.com>
    convertUsed[] 改为 convert_hb[].lnao.node
2008-03-07 - Guojie Zhu <zhugj@css-intelligent.com>
    AddTR_DATA中移除对交易进行校验的操作( CalcTrCheksum(pfp_tr_data))
    WriteTR_DATA中移除recvBuffer[2/3]与TR_Buff_Contr_Id的比对
    修正Clear TR的ACK报文没有MS_ACK的问题(WriteTR_DATA中while(){}之前增加 tmp_lg = 13)
    ReadTR_DATA中的case TR_Date_Time拆分为 TR_Date & TR_Time
2008-03-10 - Guojie Zhu <zhugj@css-intelligent.com>
    WriteTR_DATA中修正MSACK的设置?buffer[15] = tmpMSAck =>  buffer[13] = tmpMSAck  
    修正ReadTR_DATA回复数据的格式问题
2008-03-20 - Guojie Zhu <zhugj@css-intelligent.com>
    WriteTR_DATA中对TR操作部分在做ChangeTR_State之前先回ACK
2008-04-07 - Guojie Zhu <zhugj@css-intelligent.com>
    修正GetTrAd计算错误
2008-05-13 - Yongsheng Li <Liys@css-intelligent.com>
    增加了一个函数GetAllOfflineCnt
    并在AddTR_DATA 函数中判断脱机笔数超过100 ,则写文件

*******************************************************************/
#include <string.h>

#include "ifsf_pub.h"
#include "pub.h"
#include "ifsf_dsptr.h"

#include <sys/mman.h>  /* for mmap and munmap */
#include <sys/types.h> /* for open */
#include <sys/stat.h>  /* for open */
#include <fcntl.h>     /* for open */
#include <unistd.h>    /* for lseek and write */


extern IFSF_SHM *gpIfsf_shm; //该结构是全局的量，只有一个,在ifsf_main.c中定义.

/*--------------------------------------------------------------*/
//获得ifsf 数据库中某个fp 所有的交易数量
static int GetAllTrCnt(const char node, const char fp_ad){
	long i, trAd;
	int k = 0;
	FP_TR_DATA *pfp_tr_data;	
	
	if( node == 0){
		UserLog("加油机节点值错误,交易数据地址GetClearTrCnt  失败");
		return -1;
	}
	
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,交易数据地址GetClearTrCnt  失败");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //找到了加油机节点
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,加油点GetClearTrCnt  失败");
		return -1;
	}
	trAd = i *MAX_TR_NB_PER_DISPENSER;
	for(i=0; i<MAX_TR_NB_PER_DISPENSER; i++){ 
		pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd+i]) );
		if( (pfp_tr_data->fp_ad == fp_ad) &&  (pfp_tr_data->node == node) \
			&& ( pfp_tr_data->Trans_State > 0)&& ( pfp_tr_data->Trans_State < 4)  )
			k++;	
	}
	return k;
}

//查询buffer 中离线交易的记录条数add by liys 2008-05-13
int  GetAllOfflineCnt(const unsigned char node){
	long i, trAd;
	int k = 0;
	FP_TR_DATA *pfp_tr_data;	
	
	if( node == 0){
		UserLog("加油机节点值错误,交易数据地址GetClearTrCnt  失败");
		return -1;
	}
	
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,交易数据地址GetClearTrCnt  失败");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //找到了加油机节点
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,加油点GetClearTrCnt  失败");
		return -1;
	}
	trAd = i *MAX_TR_NB_PER_DISPENSER;
	for(i=0; i<MAX_TR_NB_PER_DISPENSER; i++){ 
		pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd+i]) );
		if(pfp_tr_data->offline_flag != 0) //脱机交易记录
			k++;	
	}
	return k;
}


//针对某个节点的所有 非离线交易条数test
static int GetNodeAllOnLineTrCnt(const char node){
	long i, trAd;
	int k = 0;
	FP_TR_DATA *pfp_tr_data;	
	
	if( node == 0){
		UserLog("加油机节点值错误,交易数据地址GetClearTrCnt  失败");
		return -1;
	}
	
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,交易数据地址GetClearTrCnt  失败");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //找到了加油机节点
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,加油点GetClearTrCnt  失败");
		return -1;
	}
	trAd = i *MAX_TR_NB_PER_DISPENSER;
	for(i=0; i<MAX_TR_NB_PER_DISPENSER; i++){ 
		pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd+i]) );
		if((pfp_tr_data->Trans_State > 0) && (pfp_tr_data->Trans_State < 4) && (pfp_tr_data->offline_flag == ONLINE_TR))
			k++;	
	}

	return k;
}


//针对某个节点的所有的交易数量
static int GetNodeAllTrCnt(const char node){
	long i, trAd;
	int k = 0;
	FP_TR_DATA *pfp_tr_data;	
	
	if( node == 0){
		UserLog("加油机节点值错误,交易数据地址GetClearTrCnt  失败");
		return -1;
	}
	
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,交易数据地址GetClearTrCnt  失败");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //找到了加油机节点
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,加油点GetClearTrCnt  失败");
		return -1;
	}
	trAd = i *MAX_TR_NB_PER_DISPENSER;
	for(i=0; i<MAX_TR_NB_PER_DISPENSER; i++){ 
		pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd+i]) );
		if((pfp_tr_data->Trans_State > 0) && (pfp_tr_data->Trans_State < 4))
			k++;	
	}

	return k;
}

static int GetClearTrCnt(const char node, const char fp_ad){
	long i, trAd;
	int k = 0;
	FP_TR_DATA *pfp_tr_data;	
	
	if( node == 0){
		UserLog("加油机节点值错误,交易数据地址GetClearTrCnt  失败");
		return -1;
	}
	
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,交易数据地址GetClearTrCnt  失败");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //找到了加油机节点
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,加油点GetClearTrCnt  失败");
		return -1;
	}
	trAd = i *MAX_TR_NB_PER_DISPENSER;
	for(i=0; i<MAX_TR_NB_PER_DISPENSER; i++){ 
		pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd+i]) );
		if ((pfp_tr_data->fp_ad == fp_ad) &&  (pfp_tr_data->node == node) \
			&&(pfp_tr_data->Trans_State == (unsigned char)1) )
			k++;	
	}	
	return k;
}


/*
 * func: 判断是否达到未支付交易的限制笔数
 * ret: 1 - reached ; 0 - not reached
 */
int ReachedUnPaidLimit(unsigned char node, unsigned char fp_ad)
{
	long i, trAd;
	int k = 0;
	int limit_val;
	FP_TR_DATA *pfp_tr_data;	
	DISPENSER_DATA *lpDispenser_data;
	
	if( node == 0){
		UserLog("加油机节点值错误,交易数据地址GetClearTrCnt  失败");
		return -1;
	}
	
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,交易数据地址GetClearTrCnt  失败");
		return -1;
	}

	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node) {
		      break; //找到了加油机节点
		}
	}

	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,加油点RechedUnPaidLimit失败");
		return -1;
	}

	lpDispenser_data = (DISPENSER_DATA *)(&(gpIfsf_shm->dispenser_data[i]));

	trAd = i * MAX_TR_NB_PER_DISPENSER;
	for (i = 0; i < MAX_TR_NB_PER_DISPENSER; i++) {
		pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd + i]));
		if ((pfp_tr_data->fp_ad == fp_ad) && (pfp_tr_data->node == node) \
			&& (pfp_tr_data->Trans_State != (unsigned char)1) ) {
#if 0
			RunLog("Trans_State: %d", pfp_tr_data->Trans_State);
#endif
			k++;	
		}
	}	

	if (k < (lpDispenser_data->fp_data[fp_ad - 0x21]).Nb_Tran_Buffer_Not_Paid) {
	//if (k < UNPAIDLIMIT) {
		return 0;
	} else {
		UserLog("达到未支付交易笔数限定值 %d", UNPAIDLIMIT);
		return 1;
	}
}


//获得某个加油节点的已支付交易数量
static int GetNodeClearTrCnt(const char node){
	long i, trAd;
	int k = 0;
	FP_TR_DATA *pfp_tr_data;	
	
	if( node == 0){
		UserLog("加油机节点值错误,交易数据地址GetClearTrCnt  失败");
		return -1;
	}
	
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,交易数据地址GetClearTrCnt  失败");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //找到了加油机节点
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,加油点GetClearTrCnt  失败");
		return -1;
	}
	trAd = i *MAX_TR_NB_PER_DISPENSER;
	for(i=0; i<MAX_TR_NB_PER_DISPENSER; i++){ 
		pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd+i]) );
		if( pfp_tr_data->Trans_State == (unsigned char)1 )
			k++;	
	}	
	return k;
}

//获得某个fp 未支付交易的数量
int GetPayTrCnt(const char node, const char fp_ad){
	long i, trAd;
	int k = 0;
	int n = 0;
	FP_TR_DATA *pfp_tr_data;	
	
	if( node == 0 ){
		UserLog("加油机节点值错误,交易数据地址Get  失败");
		return -1;
	}
	
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,交易数据地址Get  失败");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //找到了加油机节点
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,加油点SetSgl  失败");
		return -1;
	}
	trAd = i *MAX_TR_NB_PER_DISPENSER;
	for(i=0; i<MAX_TR_NB_PER_DISPENSER; i++){ 
		pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd+i]) );
		if(   (pfp_tr_data->fp_ad == fp_ad) &&  (pfp_tr_data->node == node) \
			&& ( (pfp_tr_data->Trans_State == 2)  ||(pfp_tr_data->Trans_State == 3)  )   ){
			k++;
			n++;
		}
		else if(pfp_tr_data->Trans_State == 1){
			n++;
		}				
	}	
	
	return k;
}


//获得某个节点未支付交易的数量
static int GetNodePayTrCnt(const char node){
	long i, trAd;
	int k = 0;
	//int n = 0;
	FP_TR_DATA *pfp_tr_data;	
	
	if( node == 0 ){
		UserLog("加油机节点值错误,交易数据地址Get  失败");
		return -1;
	}
	
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,交易数据地址Get  失败");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //找到了加油机节点
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,加油点SetSgl  失败");
		return -1;
	}
	trAd = i *MAX_TR_NB_PER_DISPENSER;
	for(i=0; i<MAX_TR_NB_PER_DISPENSER; i++){ 
		pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd+i]) );
		if(  (pfp_tr_data->node == node) && ( (pfp_tr_data->Trans_State == 2)  ||(pfp_tr_data->Trans_State == 3)  )   ){
			k++;
			//n++;
		}
		//else if(pfp_tr_data->Trans_State == 1){
		//	n++;
		//}				
	}	
	
	return k;
}

//每一个fp 的交易顺序号都是 从0001 -9999,判断是否 回环,如果回环返回1,否则0
static int Is99990001(const char node, const char fp_ad){
	long i, j,  trAd;
	FP_TR_DATA *pfp_tr_data;
	unsigned char tmp_tr_nb[2];
	unsigned char zero2[2];
	unsigned char tmp01[2] = {0,1};
	//unsigned char tmp99[2] = {'\x99','\x99'};
	int flag9901 = 0;	
	if(node == 0){
		UserLog("加油机节点值错误,交易数据地址GetHeadTrAd  失败");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //找到了加油机节点
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,加油点GetHeadTrAd  失败");
		return -2;
	}
	trAd = i *MAX_TR_NB_PER_DISPENSER;//每个节点交易记录的起始地址4* 64

	bzero(zero2,sizeof(zero2));
	if(memcmp(zero2,gpIfsf_shm->dispenser_data[i].fp_data[fp_ad - 0x21].Transaction_Sequence_Nb,2) == 0){
		tmp_tr_nb[0] = 0;
		tmp_tr_nb[1] = 1;
		flag9901 = 0;
		return flag9901;
	} else {
		memcpy(tmp_tr_nb,gpIfsf_shm->dispenser_data[i].fp_data[fp_ad - 0x21].Transaction_Sequence_Nb,2);
	}
	if(memcmp(tmp01, tmp_tr_nb, 2) == 0){
		flag9901 = 0;
		return flag9901;
	}
	//查找交易号码是否存在9999-->0001	
	flag9901 = 0;
	for(i=0; i<MAX_TR_NB_PER_DISPENSER; i++){ 
		pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd+i]) );
		if(  (pfp_tr_data->fp_ad == fp_ad) &&  (pfp_tr_data->node == node) \
			&&(memcmp(pfp_tr_data->TR_Seq_Nb, tmp_tr_nb, 2) > 0)  ){ //tmp99==
			/*
			for(j=0; j<MAX_TR_NB_PER_DISPENSER; j++){
				pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd+i]) );
				if(  (pfp_tr_data->fp_ad == fp_ad) &&  (pfp_tr_data->node == node) \
					&& (memcmp(pfp_tr_data->TR_Seq_Nb,tmp01,2) == 0)  ) {
					flag9901 = 1;
					break;
				}		
			}
			*/
			flag9901 = 1;
			break;
		}
	}
	return flag9901;
}

//得到最新的一笔未支付交易 
//gpIfsf_shm->dispenser_data[i].fp_data[fp_ad - 0x21].Transaction_Sequence_Nb -1,判断是否回环,1 的情况
//同时检查该记录是否存在,不存在则返回-1
static long GetHeadTrAd(const char node, const char fp_ad){
	long i, j, k, trAd;
	int ret;
	FP_TR_DATA *pfp_tr_data;
	unsigned char tmp_tr_nb[2];
	unsigned char zero2[2];
	
	unsigned char tmp01[2] = {0,1};
	unsigned char tmp99[2] = {'\x99','\x99'};
	unsigned char tmp50[2];
	int flag9901= 0;	
	
	if(  node == 0  ){
		UserLog("加油机节点值错误,交易数据地址GetHeadTrAd  失败");
		return -1;
	}	
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,交易数据地址GetHeadTrAd  失败");
		return -1;
	}
	
	//tmp_tr_nb[0] = '\0';
	//tmp_tr_nb[1] = '\0';	
	k = 0;
	//计算起始地址
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //找到了加油机节点
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,加油点GetHeadTrAd  失败");
		return -2;
	}
	trAd = i *MAX_TR_NB_PER_DISPENSER;//起始地址
	
	bzero(zero2,sizeof(zero2));
	if(memcmp(zero2, gpIfsf_shm->dispenser_data[i].fp_data[fp_ad - 0x21].Transaction_Sequence_Nb, 2) == 0){
		tmp_tr_nb[0] = 0;
		tmp_tr_nb[1] = 1;
		//flag = 1;
		k = 0;
		return 0;
	} else{
		memcpy(tmp_tr_nb,gpIfsf_shm->dispenser_data[i].fp_data[fp_ad - 0x21].Transaction_Sequence_Nb,2);
	}

	if(memcmp(tmp01, tmp_tr_nb, 2) == 0){
		for(i=0; i<MAX_TR_NB_PER_DISPENSER; i++){
			pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd+i]) );
			//只找pfp_tr_data->TR_Seq_Nb号码比MAX_TR_NB_PER_DISPENSER小的.
			if(   (pfp_tr_data->fp_ad == fp_ad) &&  (pfp_tr_data->node == node) \
					&&( memcmp(tmp99, pfp_tr_data->TR_Seq_Nb, 2) == 0)   ) {
				k=trAd+i;
				return k;
			}			
		}	
	}
	for(i=0; i<MAX_TR_NB_PER_DISPENSER; i++){
		pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd+i]) );
		//只找pfp_tr_data->TR_Seq_Nb号码比MAX_TR_NB_PER_DISPENSER小的.
		if(   (pfp_tr_data->fp_ad == fp_ad) &&  (pfp_tr_data->node == node) \
				&&( memcmp(tmp_tr_nb, pfp_tr_data->TR_Seq_Nb, 2) == 1)  ) {//&&( memcmp(tmp_tr_nb, pfp_tr_data->TR_Seq_Nb, 2) > 0)  \
			k=trAd+i;
			return k;
		}			
	}
	/*
	//查找交易号码是否存在9999-->0001	
	flag9901 = 0;
	for(i=0; i<MAX_TR_NB_PER_DISPENSER; i++){ 
		pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd+i]) );
		if(  (pfp_tr_data->fp_ad == fp_ad) &&  (pfp_tr_data->node == node) \
			&&(memcmp(pfp_tr_data->TR_Seq_Nb,tmp99,2) == 0)  ){ 
			for(j=0; j<MAX_TR_NB_PER_DISPENSER; j++){
				pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd+i]) );
				if(  (pfp_tr_data->fp_ad == fp_ad) &&  (pfp_tr_data->node == node) \
					&&(memcmp(pfp_tr_data->TR_Seq_Nb,tmp01,2) == 0) ) {
					flag9901 = 1;
					break;
				}					
			}
			break;
		}
	}
	*/
	flag9901 = 0;
	if(Is99990001(node, fp_ad)){
		flag9901 =1;
	}
	if (!flag9901){ //no 9999-->0001
		for(i=0; i<MAX_TR_NB_PER_DISPENSER; i++){
			pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd+i]) );
			if(   (pfp_tr_data->fp_ad == fp_ad) &&  (pfp_tr_data->node == node) \
					&&(memcmp(pfp_tr_data->TR_Seq_Nb,tmp01,2) >= 0)  ) {
				memcpy(tmp01, pfp_tr_data->TR_Seq_Nb,2);
				k=trAd+i;
			}					
		}
		return k;
	}
	else {
		//MAX_TR_NB_PER_DISPENSER必需小于500,否则出错
		if(MAX_TR_NB_PER_DISPENSER >=500){
			UserLog("MAX_TR_NB_PER_DISPENSER >=500, GetHeadTrAd  失败");
			return -3;
		}
		tmp50[0] = MAX_TR_NB_PER_DISPENSER / 256;
		tmp50[1] = MAX_TR_NB_PER_DISPENSER % 256;		
		for(i=0; i<MAX_TR_NB_PER_DISPENSER; i++){
			pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd+i]) );
			//只找pfp_tr_data->TR_Seq_Nb号码比MAX_TR_NB_PER_DISPENSER小的.
			if(   (pfp_tr_data->fp_ad == fp_ad) &&  (pfp_tr_data->node == node) \
					&&(memcmp(pfp_tr_data->TR_Seq_Nb,tmp50, 2) < 0)  \
					&& (memcmp(pfp_tr_data->TR_Seq_Nb,tmp01, 2) >= 0)  ) {
				memcpy(tmp01, pfp_tr_data->TR_Seq_Nb, 2);
				k=trAd+i;
			}			
		}
		return k;
	}	
	
	return -1;
}

//gpIfsf_shm->dispenser_data[i].fp_data[fp_ad - 0x21].Transaction_Sequence_Nb逐步往回-1 ,
//直道找到以支付交易为止,注意回环
//得到最新一个以支付交易的地址
static long GetClearHeadTrAd(const char node, const char fp_ad){
	long i, j, k, trAd;
	int ret;
	FP_TR_DATA *pfp_tr_data;
	unsigned char tmp01[2] = {0,1};
	unsigned char tmp99[2] = {'\x99','\x99'};
	unsigned char tmp50[2];
	int flag9901;	
	
	if(  node == 0  ){
		UserLog("加油机节点值错误,交易数据地址GetClearHeadTrAd  失败");
		return -1;
	}	
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,交易数据地址GetClearHeadTrAd  失败");
		return -1;
	}
	k = 0;
	//tmp_tr_nb[0] = '\0';
	//tmp_tr_nb[1] = '\0';	
	//计算起始地址
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //找到了加油机节点
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,加油点GetClearHeadTrAd  失败");
		return -2;
	}
	trAd = i *MAX_TR_NB_PER_DISPENSER;//起始地址
	
	//查找交易号码是否存在9999-->0001	
	flag9901 = 0;
	if(Is99990001(node, fp_ad)){
		flag9901 =1;
	}
	/*
	for(i=0; i<MAX_TR_NB_PER_DISPENSER; i++){ 
		pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd+i]) );
		if( memcmp(pfp_tr_data->TR_Seq_Nb,tmp99,2) == 0){ 
			for(j=0; j<MAX_TR_NB_PER_DISPENSER; j++){
				pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd+i]) );
				if(  (pfp_tr_data->fp_ad == fp_ad) &&  (pfp_tr_data->node == node) \
					&&(memcmp(pfp_tr_data->TR_Seq_Nb,tmp01,2) == 0) ) {
					flag9901 = 1;
					break;
				}					
			}
			break;
		}
	}
	*/
	k = -1;
	if (!flag9901){ //no 9999-->0001
		for(i=0; i<MAX_TR_NB_PER_DISPENSER; i++){
			pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd+i]) );
			if(   (pfp_tr_data->fp_ad == fp_ad) &&  (pfp_tr_data->node == node) \
					&&(memcmp(pfp_tr_data->TR_Seq_Nb,tmp01,2) >= 0) \
					&& (pfp_tr_data->offline_flag != OFFLINE_TR)\
					//离线交易不做备份最后一笔交易操作，因为离线交易不会被删除
					//8-25 by lil  test
					&&( pfp_tr_data->Trans_State == (unsigned char)1)  ) {
				memcpy(tmp01, pfp_tr_data->TR_Seq_Nb,2);
				k=trAd+i;
			}					
		}
		if(k<0){
			UserLog("加油机交易数据找不到清除状态的,加油点GetClearHeadTrAd  失败");
		}
		return k;		
	}
	else {
		//MAX_TR_NB_PER_DISPENSER必需小于500,否则出错
		if(MAX_TR_NB_PER_DISPENSER >=500){
			UserLog("MAX_TR_NB_PER_DISPENSER >=500, GeCleartHeadTrAd  失败");
			return -3;
		}
		tmp50[0] = MAX_TR_NB_PER_DISPENSER / 256;
		tmp50[1] = MAX_TR_NB_PER_DISPENSER % 256;	
		for(i=0; i<MAX_TR_NB_PER_DISPENSER; i++){
			pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd+i]) );
			//只找pfp_tr_data->TR_Seq_Nb号码比MAX_TR_NB_PER_DISPENSER小的中的最大的.
			if(   (pfp_tr_data->fp_ad == fp_ad) &&  (pfp_tr_data->node == node) \
					&&( pfp_tr_data->Trans_State == (unsigned char)1) \
					&& (pfp_tr_data->offline_flag != OFFLINE_TR)\
					//离线交易不做备份最后一笔交易操作，因为离线交易不会被删除
					//8-25 by lil  test
					&& (memcmp(pfp_tr_data->TR_Seq_Nb,tmp50, 2) < 0) \
					&& (memcmp(pfp_tr_data->TR_Seq_Nb,tmp01, 2) >= 0)  ) {
				memcpy(tmp01, pfp_tr_data->TR_Seq_Nb, 2);
				k=trAd+i;
			}
		}
		if(k < 0){
			//没找到,就找比(1000 - MAX_TR_NB_PER_DISPENSER) 大的中的最大的.
			tmp50[0] = (1000 - MAX_TR_NB_PER_DISPENSER) / 256;
			tmp50[1] = (1000 - MAX_TR_NB_PER_DISPENSER) % 256;
			tmp01[0] = 0;
			tmp01[1] = 1;
			for(i=0; i<MAX_TR_NB_PER_DISPENSER; i++){
				pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd+i]) );
				//只找pfp_tr_data->TR_Seq_Nb号码比(1000 - MAX_TR_NB_PER_DISPENSER) 大的中的最大的.
				if(   (pfp_tr_data->fp_ad == fp_ad) &&  (pfp_tr_data->node == node) \
					&&( pfp_tr_data->Trans_State == (unsigned char)1) \
					&& (pfp_tr_data->offline_flag != OFFLINE_TR)\
					//离线交易不做备份最后一笔交易操作，因为离线交易不会被删除
					//8-25 by lil   test
					&& (memcmp(pfp_tr_data->TR_Seq_Nb,tmp50, 2) > 0) \
					&& (memcmp(pfp_tr_data->TR_Seq_Nb,tmp01, 2) >= 0)  ) {
					memcpy(tmp01, pfp_tr_data->TR_Seq_Nb, 2);
					k=trAd+i;
				}
			}
		}
		if(k<0){
			UserLog("加油机交易数据找不到清除状态的,加油点GetClearHeadTrAd  失败");
		}
		return k;
	}
	
	return -1;
}

//得到最早一笔交易记录的地址,暂时不用,不需要修改
static long GetTailTrAd(const char node, const char fp_ad){
	long i, j, k, trAd;
	int ret;
	FP_TR_DATA *pfp_tr_data;
	unsigned char tmp01[2] = {0,1};
	unsigned char tmp99[2] = {'\x99','\x99'};
	unsigned char tmp50[2];
	int flag9901 = 0;	
	
	k = 0;
	if(  node == 0  ){
		UserLog("加油机节点值错误,交易数据地址GetHeadTrAd  失败");
		return -1;
	}	
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,交易数据地址GetHeadTrAd  失败");
		return -1;
	}
		
	//计算起始地址
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //找到了加油机节点
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,加油点GetHeadTrAd  失败");
		return -2;
	}	
	trAd = i *MAX_TR_NB_PER_DISPENSER;//起始地址
	
	//查找交易号码是否存在9999-->0001	
	flag9901 = 0;
	if(Is99990001(node, fp_ad)){
		flag9901 =1;
	}
	/*
	for(i=0; i<MAX_TR_NB_PER_DISPENSER; i++){ 
		pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd+i]) );
		if(  (pfp_tr_data->fp_ad == fp_ad) &&  (pfp_tr_data->node == node) \
					&&(memcmp(pfp_tr_data->TR_Seq_Nb,tmp99,2) == 0) ){ 
			for(j=0; j<MAX_TR_NB_PER_DISPENSER; j++){
				pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd+i]) );
				if(  (pfp_tr_data->fp_ad == fp_ad) &&  (pfp_tr_data->node == node) \
					&&(memcmp(pfp_tr_data->TR_Seq_Nb,tmp01,2) == 0)  ) {
					flag9901 = 1;
					break;
				}					
			}
			break;
		}		
	}
	*/
	if (!flag9901){ //no 9999-->0001
		for(i=0; i<MAX_TR_NB_PER_DISPENSER; i++){
			pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd+i]) );
			if(  (pfp_tr_data->fp_ad == fp_ad) &&  (pfp_tr_data->node == node) \
					&&(memcmp(pfp_tr_data->TR_Seq_Nb,tmp99,2) <= 0) ) {
				memcpy(tmp99, pfp_tr_data->TR_Seq_Nb,2);
				k=trAd+i;
			}					
		}
		return k;
	}
	else {
		//MAX_TR_NB_PER_DISPENSER必需小于500,否则出错
		if(MAX_TR_NB_PER_DISPENSER >=500){
			UserLog("MAX_TR_NB_PER_DISPENSER >=500, GetHeadTrAd  失败");
			return -3;
		}
		tmp50[0] = (1000 - MAX_TR_NB_PER_DISPENSER) / 256;
		tmp50[1] = (1000 - MAX_TR_NB_PER_DISPENSER) % 256;		
		for(i=0; i<MAX_TR_NB_PER_DISPENSER; i++){
			pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd+i]) );
			//只找pfp_tr_data->TR_Seq_Nb号码比MAX_TR_NB_PER_DISPENSER大的.
			if(   (pfp_tr_data->fp_ad == fp_ad) &&  (pfp_tr_data->node == node) \
					&&(memcmp(pfp_tr_data->TR_Seq_Nb,tmp50, 2) > 0) \
					&& (memcmp(pfp_tr_data->TR_Seq_Nb,tmp99, 2) <= 0)  ) {
				memcpy(tmp99, pfp_tr_data->TR_Seq_Nb, 2);
				k=trAd+i;
			}			
		}
		return k;
	}	
	
	return -1;
}



//删除最早一笔已支付交易记录
static int DelLastTR_DATA(const char node, const char fp_ad){
	int tr_state, ret, inode,ccnt;//,noPay
	//DISPENSER_DATA *dispenser_data;
	FP_TR_DATA *pfp_tr_data;
	//unsigned char node, fp_ad;
	//int m;
	long i,j, k, trAd;//n, 
	int flag9901;//flag for 9999->0001
	int data_lg;
	unsigned char tmp[2];
	unsigned char tmp50[2];
	unsigned char tmp99[2];
	//memset(buffer, 0, 64);	

	if(node == 0){
		UserLog("加油机节点值错误,交易数据Del 失败");
		return -1;
	}
	
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,交易数据Del 失败");
		return -1;
	}
	
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node) { //找到了加油机节点
			inode = i;
			 break;
		}
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,加油点DelLastTR_DATA  失败");
		return -1;
	}
	ccnt = GetClearTrCnt(node, fp_ad);
	//noPay = gpIfsf_shm->dispenser_data[inode].fp_data[fp_ad-0x21].Nb_Tran_Buffer_Not_Paid;
	if(ccnt <= gpIfsf_shm->dispenser_data[inode].fp_data[fp_ad-0x21].Nb_Of_Historic_Trans){//
		return -2;
	}
	tmp[0] ='\x0';
	tmp[1] ='\x01';
	tmp99[0] ='\x99';
	tmp99[1] ='\x99';
	k = 0;
	
	trAd = inode *MAX_TR_NB_PER_DISPENSER;//起始地址
	//查找交易号码是否存在9999-->0001
	flag9901 = 0;
	if(Is99990001(node, fp_ad)){
		flag9901 =1;
	}
	/*
	for(i=0; i<MAX_TR_NB_PER_DISPENSER; i++){ 
		pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd+i]) );
		if(  (pfp_tr_data->fp_ad == fp_ad) &&  (pfp_tr_data->node == node) \
					&&(memcmp(pfp_tr_data->TR_Seq_Nb,tmp99,2) == 0)  ){ 
			for(j=0; j<MAX_TR_NB_PER_DISPENSER; j++){
				pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd+i]) );
				if(  (pfp_tr_data->fp_ad == fp_ad) &&  (pfp_tr_data->node == node) \
					&&(memcmp(pfp_tr_data->TR_Seq_Nb,tmp,2) == 0) ) {
					flag9901 = 1;
					break;
				}					
			}
			break;
		}
	}
	*/
	//没有9999-->0001, 删除最小值
	if (!flag9901){ //no 9999-->0001, //i== MAX_TR_NB_PER_DISPENSER
		for(i=0; i<MAX_TR_NB_PER_DISPENSER; i++){
			pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd+i]) );
			if((pfp_tr_data->fp_ad == fp_ad) &&  (pfp_tr_data->node == node) \
				&&(pfp_tr_data->Trans_State == 1) && (memcmp(pfp_tr_data->TR_Seq_Nb,tmp99,2) <= 0)) {
				memcpy(tmp99, pfp_tr_data->TR_Seq_Nb,2);
				k=trAd+i;
			}					
		}
		pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[k]) );
		tr_state = pfp_tr_data->Trans_State;
		if ( tr_state == (unsigned char)CLEARED)
			memset(pfp_tr_data, 0, sizeof(FP_TR_DATA) );
		else {
			UserLog("该交易数据不处于clear 状态,交易数据Del 失败");
			return -2;
		}
	} else {//存在9999-->0001,删除大于1000-MAX_TR_NB_PER_DISPENSER的最小值
		//MAX_TR_NB_PER_DISPENSER必需小于500,否则出错
		if(MAX_TR_NB_PER_DISPENSER >=500){
			UserLog("MAX_TR_NB_PER_DISPENSER >=500, GetHeadTrAd  失败");
			return -3;
		}
		tmp50[0] = (1000 - MAX_TR_NB_PER_DISPENSER) / 256;
		tmp50[1] = (1000 - MAX_TR_NB_PER_DISPENSER) % 256;
		k=0;
		for(i=0; i<MAX_TR_NB_PER_DISPENSER; i++){
			pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd+i]) );
			//只找pfp_tr_data->TR_Seq_Nb号码比1000-MAX_TR_NB_PER_DISPENSER大的最小值.
			if((pfp_tr_data->fp_ad == fp_ad) &&  (pfp_tr_data->node == node) \
				&&(pfp_tr_data->Trans_State == 1) && \
				(memcmp(pfp_tr_data->TR_Seq_Nb,tmp50, 2) > 0) && \
				(memcmp(pfp_tr_data->TR_Seq_Nb,tmp99, 2) <= 0)  ) {
				memcpy(tmp99, pfp_tr_data->TR_Seq_Nb, 2);
				k=trAd+i;
			}			
		}
		pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[k]) );
		tr_state = pfp_tr_data->Trans_State;
		if ( tr_state == (unsigned char)1 ) {
			memset(pfp_tr_data, 0, sizeof(FP_TR_DATA) );
		} else if( k != 0 ){
			UserLog("该交易数据不处于clear 状态,交易数据Del 失败");
			return -1;
		} else {			
			UserLog("查找交易错误, 删除失败");
			return -1;
		}	
		
	}
	//本节点所有交易数据量如果低于一定数量,读取备份的交易数据,如果有的话.
	//临时用下ccnt,统计本节点所有交易数据量
#if 0
	// comment out by jie @ 3.10 for test
	ccnt = GetNodeAllTrCnt(node);
	if(ccnt < MAX_TR_NB_PER_DISPENSER/4-2){
		ret = ReadHalfNodeTR_DATA(node);
	}
#endif
	return 0;
}

// 处于已支付状态并且如果是 离线的也已上传的可以清除标志
int DelAllClearTR_DATA(const char node){
	int tr_state, inode, ret, acnt;//,noPay,ccnt
	FP_TR_DATA *pfp_tr_data;
	//unsigned char node, fp_ad;
	long i,j, k, trAd;//n, 	
	//int data_lg;
	if(  node == 0  ){
		UserLog("加油机节点值错误,交易数据Del 失败");
		return -1;
	}	
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,交易数据Del 失败");
		return -1;
	}

	k = 0;
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node) { //找到了加油机节点
		inode = i;
		 break;
		}
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,加油点DelAllClearTR_DATA  失败");
		return -1;
	}
		
	trAd = inode *MAX_TR_NB_PER_DISPENSER;//起始地址
	for(i=0; i<MAX_TR_NB_PER_DISPENSER; i++){
		pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd+i]) );

		//modify by liys 2008-03-13 处于已支付状态可以清除标志了
		if(pfp_tr_data->Trans_State == (unsigned char)1) {
			memset(pfp_tr_data, 0, sizeof(FP_TR_DATA) );
		}					
	}

/*	 //modify by liys  2008-03-13  
//本节点所有交易数据量如果低于一定数量,读取备份的交易数据,如果有的话.
	acnt = GetNodeAllTrCnt( node); 
	if(acnt < MAX_TR_NB_PER_DISPENSER/4-2){
		ret = ReadHalfNodeTR_DATA(node);
	} 
*/
	return 0;
}

//清除某个面板所有的加油记录
int DelAllTR_DATA(const char node, const char fp_ad){
	int tr_state, inode, ret;//,noPay,ccnt
	FP_TR_DATA *pfp_tr_data;
	//unsigned char node, fp_ad;
	long i,j, k, trAd;//n, 	
	//int data_lg;
	if(  node == 0  ){
		UserLog("加油机节点值错误,交易数据Del 失败");
		return -1;
	}	
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,交易数据Del 失败");
		return -1;
	}
	
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node) { //找到了加油机节点
		inode = i;
		 break;
		}
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,加油点DelAllTR_DATA  失败");
		return -1;
	}
		
	trAd = inode *MAX_TR_NB_PER_DISPENSER;//起始地址
	for(i=0; i<MAX_TR_NB_PER_DISPENSER; i++){
		pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd+i]) );		
		memset(pfp_tr_data, 0, sizeof(FP_TR_DATA) );			
	}
		
	return 0;
}

//获得第一个空白地址,用于存放新交易
static long GetEmptyTrAd(const char node, const char fp_ad){
	long i, trAd;	
	FP_TR_DATA *pfp_tr_data;	
	
	if(  node == 0  ){
		UserLog("加油机节点值错误,交易数据地址Get  失败");
		return -1;
	}
	
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,交易数据地址Get  失败");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //找到了加油机节点
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,加油点SetSgl  失败");
		return -1;
	}
	trAd = i *MAX_TR_NB_PER_DISPENSER;
	for(i=0; i<MAX_TR_NB_PER_DISPENSER; i++){ 
		pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd]) );
		if((pfp_tr_data->node == 0) && 
			(pfp_tr_data->fp_ad == 0) && 
			(pfp_tr_data->TR_Log_Noz == 0))
			break;
		trAd++;		
	}

	if (i != MAX_TR_NB_PER_DISPENSER) {
		return trAd;
	} else {
		UserLog("交易数据区已满!!  获取交易数据地址失败");
		return -1;
	}
	
	return -1;
}

//处于已支付状态并且如果是 离线的也已上传的可以清除标志
int DelClearTR_DATA(const char node){//delete all clear transaction data, but left only the new one!!
	int tr_state, inode, ret, acnt;//,noPay,ccnt
	FP_TR_DATA *pfp_tr_data, tmp_fp_tr_data[4] = {0};
	//unsigned char node, fp_ad;
	long i,j, k, trAd;//n, 	
	//int data_lg;
	if(  node == 0  ){
		UserLog("加油机节点值错误,交易数据Del 失败");
		return -1;
	}	
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,交易数据Del 失败");
		return -1;
	}

	k = 0;
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node) { //找到了加油机节点
		inode = i;
		 break;
		}
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,加油点DelAllClearTR_DATA  失败");
		return -1;
	}		

	trAd = inode *MAX_TR_NB_PER_DISPENSER;//起始地址
	memset(tmp_fp_tr_data, 0, sizeof(tmp_fp_tr_data));  //add by liys2008-06-09
	for(i=0;i<4;i++){
		j = 0x21+i;
		k = GetClearHeadTrAd(node, j);
		if(k >= 0){
			memcpy(&tmp_fp_tr_data[i], &(gpIfsf_shm->fp_tr_data[k]), sizeof(FP_TR_DATA) );
		}		
	}
	for(i=0; i<MAX_TR_NB_PER_DISPENSER; i++){
		pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd+i]));

		//modify by liys 2008-03-13 处于已支付状态可以清除标志了
		//modify by Guojie Zhu @ 2009-05-23 增加离线交易判断, 离线交易不能被清除
		if ((pfp_tr_data->Trans_State == (unsigned char)1) &&
				pfp_tr_data->offline_flag != OFFLINE_TR) {
			memset(pfp_tr_data, 0, sizeof(FP_TR_DATA) );
		}					
	}
	for(i=0;i<4;i++){
		k= GetEmptyTrAd(node, 0x21);
		if(k >= 0){
			memcpy(&(gpIfsf_shm->fp_tr_data[k]), &tmp_fp_tr_data[i], sizeof(FP_TR_DATA));
		}		
	}
	/* modify by liys 2008-03-13 未支付交易只有两笔，没必要保留这段了
	//本节点所有交易数据量如果低于一定数量,读取备份的交易数据,如果有的话.
	acnt = GetNodeAllTrCnt( node);
	if(acnt < MAX_TR_NB_PER_DISPENSER/4-2){
		ret = ReadHalfNodeTR_DATA(node);
	}
	*/
	return 0;
}

//把一个节点的所有未支付交易记录全部写入文件
int WriteFileNodeTR_DATA(const unsigned char node){
	int tr_state, inode, ret;//,noPay,ccnt
	FP_TR_DATA *pfp_tr_data;
	long i,  trAd;//n, j,k,
	int fd;//len, 
	char fileName[128] ;
	
	if(  node == 0  ){
		UserLog("加油机节点值错误,交易数据WriteFileAllTR_DATA 失败");
		return -1;
	}

	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,交易数据WriteFileAllTR_DATA 失败");
		return -1;
	}
	bzero(fileName,sizeof(fileName));
	sprintf(fileName,"%s%02x",TEMP_TR_FILE, node);	
	fd = open( fileName, O_WRONLY|O_CREAT|O_APPEND, 00644 );
	if( fd < 0 )
	{
		UserLog( "打开交易数据临时存放文件[%s]失败.",fileName );
		return -2;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node) { //找到了加油机节点
		inode = i;
		 break;
		}
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,加油点WriteFileAllTR_DATA  失败");
		return -1;
	}
		
	trAd = inode *MAX_TR_NB_PER_DISPENSER;//起始地址
	
	for(i=0; i<MAX_TR_NB_PER_DISPENSER; i++){
		pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd+i]) );		
		if( (pfp_tr_data->Trans_State == PAYABLE) || (pfp_tr_data->Trans_State == LOCKED) ) {
			ret = write(fd, pfp_tr_data,  sizeof(FP_TR_DATA) );
			if(ret == 0){
				bzero(pfp_tr_data,   sizeof(FP_TR_DATA)  );
			}
		}
	}

	close(fd);

	return 0;
}

//写一半未支付交易到交易缓存区
int WriteHalfNodeTR_DATA(const unsigned char node){
	int tr_state, inode, ret,accnt;//,noPay,ccnt
	FP_TR_DATA *pfp_tr_data;
	long trAd;//n, j ,
	int i, k, n;
	int fd;//len, 
	unsigned char tmp50[2];
	char fileName[128] ;
	
	if(  node == 0  ){
		UserLog("加油机节点值错误,交易数据WriteFileAllTR_DATA 失败");
		return -1;
	}

	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,交易数据WriteFileAllTR_DATA 失败");
		return -1;
	}
	bzero(fileName,sizeof(fileName));
	sprintf(fileName,"%s%02x",TEMP_TR_FILE, node);	
	fd = open( fileName, O_WRONLY|O_CREAT|O_APPEND, 00644 );
	if( fd < 0 )
	{
		UserLog( "打开交易数据临时存放文件[%s]失败.",fileName );
		return -2;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node) { //找到了加油机节点
		inode = i;
		 break;
		}
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,加油点WriteFileAllTR_DATA  失败");
		return -1;
	}		
	trAd = inode *MAX_TR_NB_PER_DISPENSER;//起始地址
	
	n = 0x21;
	for(i=0; i<gpIfsf_shm->node_channel[inode].cntOfFps; i++){
		n += i;
		accnt = GetAllTrCnt(node, n);
		if(accnt>= MAX_TR_NB_PER_DISPENSER/4)
			break;
	}
	if( Is99990001(node,n) ){
		tmp50[0] = (1000 - MAX_TR_NB_PER_DISPENSER) / 256;
		tmp50[1] = (1000 - MAX_TR_NB_PER_DISPENSER) % 256;
		k = 0;
		for(i=0; i<MAX_TR_NB_PER_DISPENSER; i++){			
			pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd+i]) );		
			if(   (pfp_tr_data->fp_ad == n) &&  (pfp_tr_data->node == node) \
				&&(memcmp(pfp_tr_data->TR_Seq_Nb,tmp50, 2) > 0) && \
				( (pfp_tr_data->Trans_State == 2) || (pfp_tr_data->Trans_State == 3)  )   ) {
				ret = write(fd, pfp_tr_data,  sizeof(FP_TR_DATA) );
				if(ret == 0){
					bzero(pfp_tr_data,   sizeof(FP_TR_DATA)  );
				}
				if(++k >= MAX_TR_NB_PER_DISPENSER/2){//只写一半儿
					break;
				}
			}			
		}
		if(k < MAX_TR_NB_PER_DISPENSER/2 ){
			UserLog("交易号码9999循环至0001,因此本次只存最旧的交易数据到9999,共%d个",k);
		}
	}
	else{
		k = 0;
		for(i=0; i<MAX_TR_NB_PER_DISPENSER; i++){	
			pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd+i]) );		
			if(  (pfp_tr_data->fp_ad == n) &&  (pfp_tr_data->node == node) \
				&&(pfp_tr_data->Trans_State == 2) || (pfp_tr_data->Trans_State == 3) ) {
				ret = write(fd, pfp_tr_data,  sizeof(FP_TR_DATA) );
				if(ret == 0){
					bzero(pfp_tr_data,   sizeof(FP_TR_DATA)  );
				}
				if(++k >= MAX_TR_NB_PER_DISPENSER/2){//只写一半儿
					break;
				}
			}
		}
	}
	
	close(fd);
	return 0;
}

//从文件中读回buff 的一半未支付交易
int ReadHalfNodeTR_DATA(const unsigned char node){
	int tr_state, inode, ret, acnt;//,noPay,ccnt
	FP_TR_DATA *pfp_tr_data,fp_tr_data;
	long i, j, k,  trAd;//n, j,k,
	int fd,tempfd,okflag = -1;//len, 
	char fileName[128] ;
	char tempFileName[128] ;
	
	if(  node == 0  ){
		UserLog("加油机节点值错误,交易数据ReadHalfNodeTR_DATA 失败");
		return -1;
	}

	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,交易数据ReadHalfNodeTR_DATA 失败");
		return -1;
	}
	bzero(fileName,sizeof(fileName));
	sprintf(fileName,"%s%02x",TEMP_TR_FILE, node);	
	//fd = open( fileName, O_WRONLY|O_CREAT|O_APPEND, 00644 );
	fd = open(fileName, O_RDONLY);
	if( fd < 0 ) {
		UserLog( "打开交易数据临时存放文件[%s]失败.",fileName );
		return -2;
	}
	bzero(tempFileName,sizeof(tempFileName));
	sprintf(tempFileName,"%stemp%02x",TEMP_TR_FILE, node);	
	tempfd = open( fileName, O_WRONLY|O_CREAT|O_APPEND, 00644 );
	if( tempfd< 0 ) {
		UserLog( "打开交易数据临时存放备份文件[%s]失败.",tempFileName );
		return -2;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node) { //找到了加油机节点
		inode = i;
		 break;
		}
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,加油点ReadHalfNodeTR_DATA  失败");
		return -1;
	}
	
	trAd = inode *MAX_TR_NB_PER_DISPENSER;//起始地址
	
	acnt = GetNodeAllTrCnt(node);
	if(acnt >MAX_TR_NB_PER_DISPENSER/2){
		UserLog("共享内存交易数据太多, ReadHalfNodeTR_DATA  失败");
		return -1;
	}
	k=0;
	for(i=0; i<MAX_TR_NB_PER_DISPENSER; i++){			
		ret = read(fd, &fp_tr_data,  sizeof(FP_TR_DATA) );
		if( ret == 0 )	{	//文件结束
			okflag = 1;
			break;
		}
		else if(errno == EINTR){
			continue;
		}
		else if(ret != sizeof(FP_TR_DATA)  ){
			UserLog("读取交易数据失败, ReadHalfNodeTR_DATA  失败");
			okflag = -2;
			break;
		}
		ret = AddTR_DATA(&fp_tr_data);//离线交易是否采取这种方式
		if(ret <0){
			UserLog("AddTR_DATA失败, ReadHalfNodeTR_DATA  失败");
			okflag = -3;
			break;
		}
		if(++k >= MAX_TR_NB_PER_DISPENSER/2 -1 ){
			okflag = 0;
			break;
		}
		
	}
	if( (k == 0)&&(okflag != 1) ){
		UserLog("备份的交易数据处理错误, ReadHalfNodeTR_DATA  失败");
		close(fd);
		close(tempfd);
		return -1;
	}
	else if( (k == 0)&&(okflag == 1) ){
		UserLog("没有备份的交易数据, ReadHalfNodeTR_DATA  成功");
		close(fd);
		close(tempfd);
		ret = remove(fileName);
		ret = remove(tempFileName);
		return 0;
	}
	else if(okflag != 1){
		while(1){
			ret = read(fd, pfp_tr_data,  sizeof(FP_TR_DATA) );
			if( ret == 0 ){	//文件结束
				okflag = 2;
				break;
			}
			else if(errno == EINTR){
				continue;
			}
			else if(ret != sizeof(FP_TR_DATA)  ){
				UserLog("读取交易数据失败, ReadHalfNodeTR_DATA  失败");
				break;
		       }
			ret = write(fd, pfp_tr_data,  sizeof(FP_TR_DATA) );			
			if(ret != sizeof(FP_TR_DATA)  ){
				UserLog("交易数据整理错误, ReadHalfNodeTR_DATA  失败");
				break;
		       }		
		}	
		close(fd);
		close(tempfd);
	}
	if(okflag == 2){
		ret = remove(fileName);
		if(ret == 0){
			ret = rename(tempFileName, fileName);
			if(ret < 0){
				RunLog("严重!!交易数据整理错误, ReadFileNodeTR_DATA  失败,交易数据可能丢失!!");
				return -4;
			}
		}
		else{
			UserLog("交易数据整理错误, ReadFileNodeTR_DATA  失败");
			return -3;
		}
	}
	if(okflag == 1){
		ret = remove(fileName);
		if( ret < 0){
			RunLog("交易数据整理错误, 交易备份数据可能存在冗余,ReadFileNodeTR_DATA  失败");
			return -3;
		}
	}
	return 0;
}

int ReadAllNodeTR_DATA(const unsigned char node){
	int tr_state, inode, ret, acnt;//,noPay,ccnt
	FP_TR_DATA *pfp_tr_data,fp_tr_data;
	long i, j, k,  trAd;//n, j,k,
	int fd,tempfd,okflag = -1;//len, 
	char fileName[128] ;
	char tempFileName[128] ;
	
	if(  node == 0  ){
		UserLog("加油机节点值错误,交易数据ReadAllNodeTR_DATA 失败");
		return -1;
	}
	if(  fd <= 0  ){
		UserLog("加油机节点值错误,交易数据ReadAllNodeTR_DATA 失败");
		return -1;
	}	
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,交易数据ReadAllNodeTR_DATA 失败");
		return -1;
	}
	bzero(fileName,sizeof(fileName));
	sprintf(fileName,"%s%02x",TEMP_TR_FILE, node);	
	//fd = open( fileName, O_WRONLY|O_CREAT|O_APPEND, 00644 );
	fd = open( fileName, O_RDONLY);
	if( fd < 0 )
	{
		UserLog( "打开交易数据临时存放文件[%s]失败.",fileName );
		return -2;
	}
	bzero(tempFileName,sizeof(tempFileName));
	sprintf(tempFileName,"%stemp%02x",TEMP_TR_FILE, node);	
	tempfd = open( fileName, O_WRONLY|O_CREAT|O_APPEND, 00644 );
	if( tempfd< 0 )
	{
		UserLog( "打开交易数据临时存放备份文件[%s]失败.",tempFileName );
		return -2;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node) { //找到了加油机节点
		inode = i;
		 break;
		}
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,加油点ReadAllNodeTR_DATA  失败");
		return -1;
	}
	
	trAd = inode *MAX_TR_NB_PER_DISPENSER;//起始地址
	
	acnt = GetNodeAllTrCnt(node);
	if(acnt >MAX_TR_NB_PER_DISPENSER/3){
		UserLog("共享内存交易数据太多, ReadAllNodeTR_DATA  失败");
		return -1;
	}
	k=0;
	for(i=0; i<MAX_TR_NB_PER_DISPENSER; i++){			
		ret = read(fd, &fp_tr_data,  sizeof(FP_TR_DATA) );
		if( ret == 0 )	{	//文件结束
			okflag = 1;
			break;
		}
		else if(errno == EINTR){
			continue;
		}
		else if(ret != sizeof(FP_TR_DATA)  ){
			UserLog("读取交易数据失败, ReadAllNodeTR_DATA  失败");
			okflag = -2;
			break;
		}
		ret = AddTR_DATA(&fp_tr_data);//离线交易是否采用这种方式
		if(ret <0){
			UserLog("AddTR_DATA失败, ReadAllNodeTR_DATA  失败");
			okflag = -3;
			break;
		}
		if(++k >= MAX_TR_NB_PER_DISPENSER*7/8 -1 ){
			okflag = 0;
			break;
		}
		
	}
	if( (k == 0)&&(okflag != 1) ){
		UserLog("备份的交易数据处理错误, ReadAllNodeTR_DATA  失败");
		close(fd);
		close(tempfd);
		return -1;
	}
	else if( (k == 0)&&(okflag == 1) ){
		UserLog("没有备份的交易数据, ReadAllNodeTR_DATA  成功");
		close(fd);
		close(tempfd);
		ret = unlink(fileName);
		ret = unlink(tempFileName);
		return 0;
	}
	else if(okflag != 1){
		while(1){
			ret = read(fd, pfp_tr_data,  sizeof(FP_TR_DATA) );
			if( ret == 0 ){	//文件结束
				okflag = 2;
				break;
			}
			else if(errno == EINTR){
				continue;
			}
			else if(ret != sizeof(FP_TR_DATA)  ){
				UserLog("读取交易数据失败, ReadAllNodeTR_DATA  失败");
				break;
		       }
			ret = write(fd, pfp_tr_data,  sizeof(FP_TR_DATA) );			
			if(ret != sizeof(FP_TR_DATA)  ){
				UserLog("交易数据整理错误, ReadAllNodeTR_DATA  失败");
				break;
		       }		
		}	
		close(fd);
		close(tempfd);
	}
	if(okflag == 2){
		ret = unlink(fileName);
		if(ret == 0){
			ret = rename(tempFileName, fileName);
			if(ret < 0){
				RunLog("严重!!交易数据整理错误, ReadAllNodeTR_DATA  失败,交易数据可能丢失!!");
				return -4;
			}
		}
		else{
			UserLog("交易数据整理错误, ReadAllNodeTR_DATA  失败");
			return -3;
		}
	}
	if(okflag == 1){
		ret = unlink(fileName);
		if( ret < 0){
			RunLog("交易数据整理错误, 交易备份数据可能存在冗余,ReadAllNodeTR_DATA  失败");
			return -3;
		}
	}
	return 0;
}

//获取下一个交易事务顺序号.成功返回TR_Seq_Nb.失败返回-1.
int GetNextTrNb(const char node, const char fp_ad, unsigned char *tr_seq_nb){
	long i, j, k, trAd;
	int ret;
	unsigned char tmp_tr_nb[2];
	FP_TR_DATA *pfp_tr_data;
	unsigned char zero2[2];
	
	if( NULL == tr_seq_nb ){
		UserLog("交易数据错误,交易数据地址Get 失败");
		return -1;
	}
	if(  node == 0  ){
		UserLog("加油机节点值错误,交易数据地址Get  失败");
		return -1;
	}	
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,交易数据地址Get  失败");
		return -1;
	}
	
	
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //找到了加油机节点
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,加油点GetNextTrNb  失败");
		return -1;
	}
	bzero(zero2,sizeof(zero2));
	//最好在buff 中找
	if(memcmp(zero2,gpIfsf_shm->dispenser_data[i].fp_data[fp_ad - 0x21].Transaction_Sequence_Nb,2) == 0){
		tmp_tr_nb[0] = 0;
		tmp_tr_nb[1] = 1;
	} else {
		memcpy(tmp_tr_nb,gpIfsf_shm->dispenser_data[i].fp_data[fp_ad - 0x21].Transaction_Sequence_Nb,2);
	}
	//tmp_tr_nb[0] = pfp_tr_data->TR_Seq_Nb[0];
	//tmp_tr_nb[1] = pfp_tr_data->TR_Seq_Nb[1];
	memcpy(tr_seq_nb, tmp_tr_nb, 2);

	return 0;
}

static int MakeTrMsg(const char node, const char fp_ad, const FP_TR_DATA *pfp_tr_data, unsigned char *sendBuffer, int *data_lg){
	int i,k,tr_state,tmp_lg=0; //recv_lg;	
	//DISPENSER_DATA *dispenser_data;
	//FP_TR_DATA *pfp_tr_data;
	//int data_lg;
	unsigned char buffer[64];
	memset(buffer, 0, 64);
	if( (node <= 0) || (node > 127 ) ){
		UserLog("加油机节点值错误,交易数据MakeTrMsg 失败");
		return -1;
	}		
	if( (NULL == pfp_tr_data) || (NULL == sendBuffer) || (NULL == data_lg) ){
		UserLog("参数为空错误,交易数据MakeTrMsg 失败");
		return -1;
		}
	/*
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,交易数据MakeTrMsg 失败");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //找到了加油机节点
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,交易数据MakeTrMsg 失败");
		return -1;
	}
	//dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	//pfp_tr_data =  (FP_ER_DATA *)( &(gpIfsf_shm->fp_tr_data[fp])  );
	*/
	tr_state = pfp_tr_data->Trans_State;
	if( tr_state >3 ) return -1;	
	
	buffer[0] = '\x02';
	buffer[1] = '\x01';
	buffer[2] = '\x01';
	buffer[3] = node;
	buffer[4] = '\0'; //recvBuffer[4]; IFSF_MC
	buffer[5] = '\x80';//unsolicited data..
	//recv_lg = recvBuffer[6];
	//recv_lg = recv_lg * 256 + recvBuffer[7];
	buffer[8] = '\x04'; //recvBuffer[8]; //Ad_lg
	buffer[9] = fp_ad;  //ad
	buffer[10] ='\x21'; //ad
	buffer[11] = pfp_tr_data->TR_Seq_Nb[0]; //ad
	buffer[12] = pfp_tr_data->TR_Seq_Nb[1]; //ad
	//buffer[13] = recvBuffer[13]; //It's Data_Id. Data begin.	
	tmp_lg=12;

	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)TR_Buff_States_Message;
	tmp_lg++;
	buffer[tmp_lg] = '\0';
	
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)TR_Seq_Nb;
	tmp_lg++;
	buffer[tmp_lg] = '\x02';
	tmp_lg++;
	buffer[tmp_lg] = pfp_tr_data->TR_Seq_Nb[0];
	tmp_lg++;
	buffer[tmp_lg] = pfp_tr_data->TR_Seq_Nb[1];
		
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)Trans_State;
	tmp_lg++;
	buffer[tmp_lg] = '\x01';	
	tmp_lg++;
	buffer[tmp_lg] = pfp_tr_data->Trans_State;

	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)TR_Buff_Contr_Id;
	tmp_lg++;
	buffer[tmp_lg] = '\x02';
	tmp_lg++;
	buffer[tmp_lg] = pfp_tr_data->TR_Buff_Contr_Id[0];
	tmp_lg++;
	buffer[tmp_lg] = pfp_tr_data->TR_Buff_Contr_Id[1];

	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)TR_Amount;
	tmp_lg++;
	buffer[tmp_lg] = '\x05';
	k = buffer[tmp_lg];
	for(i = 0; i < k; i++){
		tmp_lg++;
		buffer[tmp_lg] = pfp_tr_data->TR_Amount[i];
	}

	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)TR_Volume;
	tmp_lg++;
	buffer[tmp_lg] = '\x05';
	k = buffer[tmp_lg];
	for(i = 0; i < k; i++){
		tmp_lg++;
		buffer[tmp_lg] = pfp_tr_data->TR_Volume[i];
	}
	
	/*TR_Seq_Nb = 1, TR_Contr_Id , TR_Release_Token ,
	TR_Fuelling_Mode,	TR_Amount, TR_Volume, TR_Unit_Price,
	TR_Log_Noz, TR_Price_Set_Nb, TR_Prod_Nb=10, /TR_Prod_Description,/ TR_Error_Code=12, 
	TR_Average_Temp, TR_Security_Chksum = 14, 
	/ M1_Sub_Volume, M2_Sub_Volume, 	TR_Tax_Amount = 17, / 
	TR_Buff_Contr_Id = 20, 	Trans_State = 21, */
	
	tmp_lg++;
	buffer[6] = (unsigned char )((tmp_lg-8) / 256) ;
	buffer[7] = (unsigned char )((tmp_lg-8) %256);  //big end.
	*data_lg= tmp_lg;
	memcpy(sendBuffer, buffer, tmp_lg);	
	return 0;
}
//改变交易状态TR_State. 注意交易置清除状态时要检查保存的处于清除状态的交易数,
//并和fp的Nb_Of_Historic_Trans比较,大于的话就删除最后面的处于清除状态的交易
static int ChangeTR_State(const char node, const char fp_ad, FP_TR_DATA *pfp_tr_data, const char status)
{
	int i,k,ret, tr_state,tmp_lg=0; //recv_lg;		
	unsigned char buffer[64];
	memset(buffer, 0, 64);
	if(node == 0){
		UserLog("加油机节点值错误,交易数据ChangeTR_State 失败");
		return -1;
	}		
	if(NULL == pfp_tr_data){
		UserLog("参数为空错误,交易数据ChangeTR_State 失败");
		return -1;
	}	
	pfp_tr_data->Trans_State = status;
	
	//modify by liys 2008-03-13
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //找到了加油机节点
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,加油点End Trans   失败");
		return -1;
	}

	ret = MakeTrMsg(node, fp_ad, pfp_tr_data, buffer, &tmp_lg);
	if (ret < 0) {
		UserLog("构建主动数据错误,交易数据ChangeTR_State 失败");
		return -1;
	}
	ret = SendData2CD(buffer, tmp_lg, 5);
	if (ret < 0) {
		UserLog("通讯错误,交易数据ChangeTR_State 失败");
		return -1;
	}

	if (GetClearTrCnt(node, fp_ad) > gpIfsf_shm->dispenser_data[i].fp_data[fp_ad-0x21].Nb_Of_Historic_Trans){
		ret = DelLastTR_DATA(node, fp_ad);
	}

	return 0;
}

//This tow functions is used to calculate value of  TR_Security_Chksum in transanction database. 
//crc calculate
static unsigned short CrcCalc(const unsigned short data, const unsigned short poly, unsigned short accum){
	int i;
	data<<1;
	for(i=8;i>0;i--){
		data>>1;
		if( (data^accum) & 0x0001 ){
			accum = (accum>>1)^poly;
		}
		else{
			accum>>1;
		}
	}
	return accum;
}
//calculate value of  TR_Security_Chksum in transanction database, used CrcCalc() function.
static int CalcTrCheksum( FP_TR_DATA *fp_tr_data){	
	int i;
	unsigned short ret, data,  poly,  accum = 0;
	//unsigned char node;
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,交易数据CalcTrCheksum  失败");
		return -1;
	}
	if( NULL ==  fp_tr_data ){
		UserLog("参数空指针错误,交易数据CalcTrCheksum 失败");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( fp_tr_data->node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //找到了加油机节点
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,交易数据CalcTrCheksum 失败");
		return -1;
	}
	if(  (0 == gpIfsf_shm->dispenser_data[i].fp_c_data.W_M_Polynomial[0])  \
		&&(0 == gpIfsf_shm->dispenser_data[i].fp_c_data.W_M_Polynomial[1])  ){		
		UserLog("校验和多项式值为0错误,交易数据CalcTrCheksum  失败");
		return -1;
	}
	poly = gpIfsf_shm->dispenser_data[i].fp_c_data.W_M_Polynomial[0];
	poly = poly *256  + gpIfsf_shm->dispenser_data[i].fp_c_data.W_M_Polynomial[1];
	data = fp_tr_data->fp_ad;
	ret =CrcCalc(data, poly, accum);
	
	data = fp_tr_data->TR_Seq_Nb[0];
	ret =CrcCalc(data, poly, accum);
	data = fp_tr_data->TR_Seq_Nb[1];	
	ret =CrcCalc(data, poly, accum);
	
	data = fp_tr_data->TR_Amount[0];
	ret =CrcCalc(data, poly, accum);
	data = fp_tr_data->TR_Amount[1];
	ret =CrcCalc(data, poly, accum);
	data = fp_tr_data->TR_Amount[2];
	ret =CrcCalc(data, poly, accum);
	data = fp_tr_data->TR_Amount[3];
	ret =CrcCalc(data, poly, accum);
	data = fp_tr_data->TR_Amount[4];
	
	ret =CrcCalc(data, poly, accum);
	data = fp_tr_data->TR_Volume[0];
	ret =CrcCalc(data, poly, accum);
	data = fp_tr_data->TR_Volume[1];
	ret =CrcCalc(data, poly, accum);
	data = fp_tr_data->TR_Volume[2];
	ret =CrcCalc(data, poly, accum);
	data = fp_tr_data->TR_Volume[3];
	ret =CrcCalc(data, poly, accum);
	data = fp_tr_data->TR_Volume[4];
	ret =CrcCalc(data, poly, accum);

	fp_tr_data->TR_Security_Chksum[0] = 0;
	fp_tr_data->TR_Security_Chksum[1] = accum %256;
	fp_tr_data->TR_Security_Chksum[2] = accum /256;

	return 0;
}

long GetTrAd(const char node, const char fp_ad, const unsigned char *tr_seq_nb){
	long i, trAd;	
	FP_TR_DATA *pfp_tr_data;
	
	if( NULL == tr_seq_nb ){
		UserLog("交易序列号错误");
		return -1;
	}
	if(  node == 0  ){
		UserLog("加油机节点值错误");
		return -1;
	}
	
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存指针为空");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //找到了加油机节点
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到");
		return -1;
	}
	
//	RunLog("want del tr_seq_nb: %02x%02x", tr_seq_nb[0], tr_seq_nb[1]);
	trAd = i*MAX_TR_NB_PER_DISPENSER ;	
	for(i=0; i<MAX_TR_NB_PER_DISPENSER; i++){ 
		//trAd++;
//		pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd]) );
		pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd + i]) );
		if ((pfp_tr_data->TR_Seq_Nb[0] != tr_seq_nb[0])  ||
			(pfp_tr_data->TR_Seq_Nb[1] != tr_seq_nb[1]) || 
			(pfp_tr_data->fp_ad != fp_ad) || 
			(pfp_tr_data->node != node) ||
			(pfp_tr_data->Trans_State != PAYABLE)) {
#if 0
			RunLog(">>fp_tr_data[%d].tr_seq_no: %02x%02x", i + trAd,
				pfp_tr_data->TR_Seq_Nb[0], pfp_tr_data->TR_Seq_Nb[1]);
#endif
			continue;
		}
		 
		break;
	}

	if (i !=MAX_TR_NB_PER_DISPENSER) {
		return trAd+i;
	} else {
		UserLog("交易数据区找不到该交易,交易数据地址获取失败!");
		return -1;
	}
	
	return -1;
}

int DefaultNodeTR_DATA(const char node)
{
	//long i, trAd;	
	int ret;
	//FP_TR_DATA *pfp_tr_data;	
	
	if(  node == 0  ){
		UserLog("加油机节点值错误,交易数据地址获取失败");
		return -1;
	}	
	ret = ReadAllNodeTR_DATA(node);
	
	return 0;
}

//FP_TR_DATA操作函数. node加油机逻辑节点号,fp_ad加油点地址21h-24h,要用到gpIfsf_shm,注意P,V操作和死锁.
//增加和清除一笔交易记录.清除记录记录注意要判断TR_State是否为清除状态.
// Return: -1/-2/1 执行失败, 返回1不允许重试执行AddTR_DATA, -1/-2可以重试; 0 - 成功
int AddTR_DATA(const FP_TR_DATA *fp_tr_data)
{ //Payable.
	int i, ret;
	int acnt;//,ccnt
	int fd;
	int data_lg;
	long k = 0;
	char fileName[128];
	unsigned char node, fp_ad;
	unsigned char buffer[64];
	FP_TR_DATA *pfp_tr_data;
	
	memset(buffer, 0, sizeof(buffer));
	if( NULL == fp_tr_data ){
		UserLog("交易数据错误,交易数据Add 失败");
		return -1;
	}
	node = fp_tr_data->node;
	fp_ad = fp_tr_data->fp_ad ;
	if(  node <= 0  ){
		UserLog("加油机节点值错误,交易数据Add 失败");
		return -1;
	}

	//针对某个节点的所有 非离线交易条数
	acnt = GetNodeAllOnLineTrCnt(node);
	if(acnt < 0){
		return -2;
	}

	//如果加油记录数大于等于64 + 1，则清除buff 中的已支付记录
	//或者是离线已上传的交易记录
	if(acnt >= (MAX_TR_NB_PER_DISPENSER/4+1) ){
		ret = DelClearTR_DATA(node);
		acnt = GetNodeAllTrCnt(node);
		if(acnt < 0){
			return -2;
		}
	}	

/*	//如果加油记录数大于等于225，则写一半未支付交易数据到文件中
	//由于目前只允许两笔未支付交易，所以该函数不会执行
	if(acnt >= (MAX_TR_NB_PER_DISPENSER*7/8+1) ){
		ret = WriteHalfNodeTR_DATA(node);
	}	
*/  

	//获得用于存放新交易的空白地址
	k = GetEmptyTrAd(node, fp_ad);
	if (k >= 0)
		pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[k]) );
	else {
		UserLog("查找空闲交易数据区失败,交易数据Add 失败");
		return -1;
	}
	
	memcpy(pfp_tr_data, fp_tr_data, sizeof(FP_TR_DATA) );

       //获得脱机交易的数量,如果大于200 则直接写入文件  add by liys 2008-05-13
       //由GetEmptyTrAD之前挪到这里, 真是反映离线交易情况  jie @ 2009-05-23
       //获得脱机交易的数量,如果大于100 则直接写入文件  add by lil 2008-05-13
	acnt = GetAllOfflineCnt(node);
	RunLog("目前内存中Node[%d]的脱机交易记录为%d 笔", node, acnt);
	if (acnt > 100) {
		ret = WriteBuffDataToFile(node);
       } 			
#if 0
	ret= CalcTrCheksum(pfp_tr_data);
	if(ret <0 ) {
		RunLog("AddTR_DATA函数中,计算交易数据校验和失败!");
		//return -1;
	}
#endif
	// < Test code ##01  Added by jie @ 2009.8.28
	if ((gpIfsf_shm->fp_offline_flag[node - 1][fp_ad - 0x21] & 0x0F) != 0) {
		RunLog("gpshm->offline_flag[%d][%d]: %d",
			(node - 1), (fp_ad - 0x21),
		       	(gpIfsf_shm->fp_offline_flag[node - 1][fp_ad - 0x21]));
	}
	// End of test code 01>

	//构造向上位机发送的数据包
	// 如果这是脱机交易, 则直接返回不上传交易信息.(恢复联机后依然在加的离线交易) 
	if ((gpIfsf_shm->fp_offline_flag[node - 1][fp_ad - 0x21] & 0x0F) == 0x0F) {
		return 0;
	}

	ret= MakeTrMsg(node, fp_ad, pfp_tr_data, buffer, &data_lg);
	if(ret <0 ) {
		UserLog("构造交易主动数据失败!");
		return 1;
	}

	//向上位机发送数据

	for (i = 0; i < MAX_CHANNEL_CNT; i++) {
		if (gpIfsf_shm->node_online[i] == node) {
			ret = SendData2CD(buffer, data_lg, 5);
			if(ret <0 ) {
				UserLog("发送交易主动数据失败!");
				return 1;
			}
			break;
		} // else { Dispenser 不在线则不发送,处理脱机交易 }
	}

	return 0;
}

//删除已支付交易数据
int DelTR_DATA(const char node, const char fp_ad, const unsigned char *tr_seq_nb)
{
	int i, tr_state, ret;
	FP_TR_DATA *pfp_tr_data;
	long k = 0;;
	unsigned char buffer[64];
	memset(buffer, 0, sizeof(buffer));
	if( NULL == tr_seq_nb ){
		UserLog("交易数据错误,交易数据Del 失败");
		return -1;
		}	
	if(  node == 0 ){
		UserLog("加油机节点值错误,交易数据Del 失败");
		return -1;
		}
	
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,交易数据Del 失败");
		return -1;
	}
	
	k = GetTrAd(node, fp_ad,tr_seq_nb);
	if (k >= 0)
		pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[k]) );
	else {
		UserLog("查找交易数据区失败,交易数据Del 失败");
		return -1;
	}
	tr_state = pfp_tr_data->Trans_State;
	if ( tr_state == (unsigned char)1 )
		memset(pfp_tr_data, 0, sizeof(FP_TR_DATA) );
	else {
		UserLog("该交易数据不处于clear 状态,交易数据Del 失败");
		return -1;
	}	
	
	return 0;
}



//读取FP_TR_DATA. 构造的信息数据的类型为001回答.
//recvBuffer为收到的data,sendBuffer为要发送的data,msg_lg为生成的总长度.
//根据加油机发送的seq_nb 得到具体的交易记录
int ReadTR_DATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg){
	int i,j, recv_lg, tmp_lg;
	//DISPENSER_DATA *dispenser_data;
	FP_TR_DATA *pfp_tr_data;
	unsigned char node, fp_ad, *tr_seq_nb;
	//int m;
	long k = 0;
	//int data_lg;
	unsigned char buffer[128];
	
	memset(buffer, 0, sizeof(buffer));
	if(  (NULL == recvBuffer) ||  (NULL == sendBuffer) ){
		UserLog("交易数据错误,交易数据Read 失败");
		return -1;
	}
	node = recvBuffer[1];
	fp_ad = recvBuffer[9];	
	if( node <= 0 ){
		UserLog("加油机节点值错误,交易数据Read 失败");
		return -1;
		}
	
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,交易数据Read  失败");
		return -1;
	}
	if ((unsigned char)4 !=recvBuffer[8]){
		UserLog("交易数据错误,交易数据Read 失败");
		return -1;
	}
	tr_seq_nb = (unsigned char *)(&recvBuffer[11]);
	k = GetTrAd(node, fp_ad,tr_seq_nb);
	if (k >= 0)
		pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[k]) );
	else {
		UserLog("查找交易数据区失败,交易数据Read 失败");
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
	buffer[8] = '\x04'; //recvBuffer[8]; //Ad_lg
	buffer[9] = recvBuffer[9];  //ad
	buffer[10] = recvBuffer[10]; //ad
	buffer[11] = recvBuffer[11]; //ad
	buffer[12] = recvBuffer[12]; //ad
	//buffer[13] = recvBuffer[13]; //It's Data_Id. Data begin.	
	tmp_lg = 12;
	for(i=0;i<recv_lg - 5; i++){
		switch(recvBuffer[13 + i]) {		
			case TR_Seq_Nb :
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)TR_Seq_Nb;
				tmp_lg++;
				buffer[tmp_lg] = '\x02';
				k= buffer[tmp_lg];
				for(j=0;j<k;j++){
					tmp_lg++;
					buffer[tmp_lg] = pfp_tr_data->TR_Seq_Nb[j];
				}
				break;			
			case TR_Contr_Id :
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)TR_Contr_Id;
				tmp_lg++;
				buffer[tmp_lg] = '\x02';
				k= buffer[tmp_lg];
				for(j=0;j<k;j++){
					tmp_lg++;
					buffer[tmp_lg] = pfp_tr_data->TR_Contr_Id[j];
				}
				break;
			case TR_Release_Token:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)TR_Release_Token;
				tmp_lg++;
				buffer[tmp_lg] = '\x01';
				tmp_lg++;
				buffer[tmp_lg] = pfp_tr_data->TR_Release_Token;
				break;			
			case TR_Fuelling_Mode:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)TR_Fuelling_Mode;
				tmp_lg++;
				buffer[tmp_lg] = '\x01';					
				tmp_lg++;
				buffer[tmp_lg] = pfp_tr_data->TR_Fuelling_Mode;				
				break;
			case TR_Amount :
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)TR_Amount;
				tmp_lg++;
				buffer[tmp_lg] = '\x05';
				k= buffer[tmp_lg];
				for(j=0;j<k;j++){
					tmp_lg++;
					buffer[tmp_lg] = pfp_tr_data->TR_Amount[j];
				}
				break;	
			case TR_Volume :
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)TR_Volume;
				tmp_lg++;
				buffer[tmp_lg] = '\x05';
				k= buffer[tmp_lg];
				for(j=0;j<k;j++){
					tmp_lg++;
					buffer[tmp_lg] = pfp_tr_data->TR_Volume[j];
				}
				break;
			case TR_Unit_Price :
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)TR_Unit_Price;
				tmp_lg++;
				buffer[tmp_lg] = '\x04';
				k= buffer[tmp_lg];
				for(j=0;j<k;j++){
					tmp_lg++;
					buffer[tmp_lg] = pfp_tr_data->TR_Unit_Price[j];
				}
				break;
			case TR_Log_Noz:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)TR_Log_Noz;
				tmp_lg++;
				buffer[tmp_lg] = '\x01';
				tmp_lg++;
				buffer[tmp_lg] = pfp_tr_data->TR_Log_Noz;
				break;
			case TR_Prod_Nb :
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)TR_Prod_Nb;
				tmp_lg++;
				buffer[tmp_lg] = '\x04';
				k= buffer[tmp_lg];
				for(j=0;j<k;j++){
					tmp_lg++;
					buffer[tmp_lg] = pfp_tr_data->TR_Prod_Nb[j];
				}
				break;
			case TR_Error_Code:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)TR_Error_Code;
				tmp_lg++;
				buffer[tmp_lg] = '\x01';
				tmp_lg++;
				buffer[tmp_lg] = pfp_tr_data->TR_Error_Code;
				break;
			case TR_Security_Chksum :
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)TR_Security_Chksum;
				tmp_lg++;
				buffer[tmp_lg] = '\x03';
				k= buffer[tmp_lg];
				for(j=0;j<k;j++){
					tmp_lg++;
					buffer[tmp_lg] = pfp_tr_data->TR_Security_Chksum[j];
				}
				break;
			case Trans_State:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)Trans_State;
				tmp_lg++;
				buffer[tmp_lg] = '\x01';
				tmp_lg++;
				buffer[tmp_lg] = pfp_tr_data->Trans_State;
				break;
			case TR_Buff_Contr_Id :
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)TR_Buff_Contr_Id;
				tmp_lg++;
				buffer[tmp_lg] = '\x02';
				k= buffer[tmp_lg];
				for(j=0;j<k;j++){
					tmp_lg++;
					buffer[tmp_lg] = pfp_tr_data->TR_Buff_Contr_Id[j];
				}
				break;
			case TR_Date:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)TR_Date;
				tmp_lg++;
				buffer[tmp_lg] = '\x04';
				k= buffer[tmp_lg];
				for (j = 0; j < k; j++){
					tmp_lg++;
					buffer[tmp_lg] = pfp_tr_data->TR_Date[j];
				}
				break; 
			case TR_Time:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)TR_Time;
				tmp_lg++;
				buffer[tmp_lg] = '\x03';
				k= buffer[tmp_lg];
				for (j = 0; j < k; j++) {
					tmp_lg++;
					buffer[tmp_lg] = pfp_tr_data->TR_Time[j];
				}
				break; 
			case TR_Start_Total:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)TR_Start_Total;
				tmp_lg++;
				buffer[tmp_lg] = 0x07;
				tmp_lg++;
				memcpy(&buffer[tmp_lg], pfp_tr_data->TR_Start_Total, 7);
				tmp_lg += 0x06;
				break;
			case TR_End_Total:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)TR_End_Total;
				tmp_lg++;
				buffer[tmp_lg] = 0x07;
				tmp_lg++;
				memcpy(&buffer[tmp_lg], pfp_tr_data->TR_End_Total, 7);
				tmp_lg += 0x06;
				break;
			default:
				tmp_lg++;
				buffer[tmp_lg] = recvBuffer[13 + i];
				tmp_lg++;
				buffer[tmp_lg] = '\0';			
				break;			
		}	
	}

	tmp_lg++;
	buffer[6] = (unsigned char )((tmp_lg-8) / 256) ;
	buffer[7] = (unsigned char )((tmp_lg-8) % 256);  //big end.
	*msg_lg = tmp_lg;
	memcpy(sendBuffer, buffer, tmp_lg);	

	return 0;
}

//写FP_LN_DATA函数.recvBuffer为收到的data,sendBuffer为要回复的data,构造的信息数据的类型为111应答.
//msg_lg为生成的总长度,为0表示不需要应答信息,sendBuffer和msg_lg不需要应答信息可以设置为NULL.
int WriteTR_DATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg){
	int i,j, tr_state, recv_lg, tmp_lg,lg, ret, count;
	//DISPENSER_DATA *dispenser_data;
	FP_TR_DATA *pfp_tr_data;
	unsigned char node, fp_ad, *tr_seq_nb;
	unsigned char tmpMSAck, tmpDataAck;
	unsigned char buffer[64];
	long k = 0;
	unsigned char TR_Operation = 0x00;  // 记录TR操作类型
	unsigned char err_op = 0x00;        // 记录错误操作,eg. 没有该交易

	memset(buffer, 0, sizeof(buffer));
	if(NULL == recvBuffer ){
		UserLog("交易数据错误,交易数据Write 失败");
		return -1;
	}
	node = recvBuffer[1];
	fp_ad = recvBuffer[9];	
	if( (node <= 0) || (node > 127 ) ){
		UserLog("加油机节点值错误,交易数据Write 失败");
		return -1;
	}
	
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,交易数据Write  失败");
		return -1;
	}
	// 长度错误
	if ((unsigned char)4 != recvBuffer[8]){
		UserLog("交易数据错误,交易数据Write 失败");
		return -1;
	}

	buffer[0] = recvBuffer[2];
	buffer[1] = recvBuffer[3];
	buffer[2] = recvBuffer[0];
	buffer[3] = recvBuffer[1];
	buffer[4] = '\0'; //recvBuffer[4]; IFSF_MC
	buffer[5] = (recvBuffer[5] |0xe0);//ack.
	recv_lg = recvBuffer[6];
	recv_lg = recv_lg * 256 + recvBuffer[7];
	buffer[8] = '\x04'; //recvBuffer[8]; //Ad_lg
	buffer[9] = recvBuffer[9];  //ad
	buffer[10] = recvBuffer[10]; //ad
	buffer[11] = recvBuffer[11]; //ad
	buffer[12] = recvBuffer[12]; //ad

	tr_seq_nb = (unsigned char *)(&recvBuffer[11]);
	k = GetTrAd(node, fp_ad,tr_seq_nb);
	if (k >= 0) {
	//	RunLog("WriteTR_DATA,got addr, fp_tr_data[%d]", k);
		pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[k]) );
	} else {
		UserLog("查找交易数据区失败,交易数据Write 失败");
		goto ADDR_NAK;
	}

	if(pfp_tr_data->Trans_State  > (unsigned char)3){
		UserLog("交易数据区数据错误,交易数据Write 失败");
ADDR_NAK:
		buffer[6] = 0x00;
		buffer[7] = 0x06;
		buffer[13] = 0x05; // NAK
		tmp_lg = 14;
		ret = SendData2CD(buffer, tmp_lg, 5); 
		return -1;
	}
	
	tr_state = pfp_tr_data->Trans_State;

	//buffer[13] = recvBuffer[13]; //It's Data_Id. Data begin.	
	tmp_lg = 13;   //  reserved
	i=0;
	tmpMSAck = '\0'; // all is  ok.
	while (i < (recv_lg - 5) ) {
		tmpDataAck = '\0'; //ok ack.
		lg =  (int )recvBuffer[14+i]; //Data_Lg.
		switch(recvBuffer[13 + i]) {
			case TR_Buff_Contr_Id : //w(1-3)				
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)TR_Buff_Contr_Id;
				if( ( lg== 2 ) && (recvBuffer[13+i] ==  (unsigned char)2 )  ) {     //2: CD subnet.
					pfp_tr_data->TR_Buff_Contr_Id[0] = recvBuffer[15+i];
					pfp_tr_data->TR_Buff_Contr_Id[1] = recvBuffer[16+i];
				} else  { 
					tmpDataAck = '\x01';
					tmpMSAck = '\x05';
					//UserLog("write  C_DAT's Nb_Products error!too big or small.") 
				}			
				tmp_lg++;
				buffer[tmp_lg] =tmpDataAck;
				i = i + 2 + lg;
				break;
			case Clear_Transaction : //w(2,3)
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)Clear_Transaction;
				if((lg == 0) && ((tr_state == 2) || (tr_state == 3))) {
//					&& (recvBuffer[2] ==  pfp_tr_data->TR_Buff_Contr_Id[0] )  \
//					&& (recvBuffer[3] == pfp_tr_data->TR_Buff_Contr_Id[1]  )  ) {
					//pfp_tr_data->TR_Buff_Contr_Id[0] = '\0';
					//pfp_tr_data->TR_Buff_Contr_Id[1] = '\0';
					//pfp_tr_data->Trans_State = 1;
					TR_Operation = Clear_Transaction;
				} else  if (tr_state ==  1)  { 
					tmpDataAck = '\x03';
					tmpMSAck = '\x05'; 
					UserLog("write  C_DAT's Nb_Products error!TR state not in 2,3. ");
				} else  { 
					tmpDataAck = '\x05';
					tmpMSAck = '\x05';
					UserLog("write  C_DAT's Nb_Products error!too big or small.");
				}			
				tmp_lg++;
				buffer[tmp_lg] =tmpDataAck;
				i = i + 2 + lg;
				break;
			case Lock_Transaction : //w(2)
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)Lock_Transaction;
				if ((lg== 0) && (tr_state == 2)) {
//					&& (recvBuffer[2] ==  pfp_tr_data->TR_Buff_Contr_Id[0] )  \
//					&& (recvBuffer[3] == pfp_tr_data->TR_Buff_Contr_Id[1]  ) ) {	
					//pfp_tr_data->TR_Buff_Contr_Id[0] = recvBuffer[2];
					//pfp_tr_data->TR_Buff_Contr_Id[1] = recvBuffer[3];
					//pfp_tr_data->Trans_State = 3;
					TR_Operation = Lock_Transaction;
				} else  if ((tr_state ==  1 ) || (tr_state ==  3)) { 
					tmpDataAck = '\x03';
					tmpMSAck = '\x05'; 
					//UserLog("write  C_DAT's Nb_Products error!fp state not in 1,2. ") 
				} else  { 
					tmpDataAck = '\x05';
					tmpMSAck = '\x05';
					//UserLog("write  C_DAT's Nb_Products error!too big or small.") 
				}			
				tmp_lg++;
				buffer[tmp_lg] = tmpDataAck;
				i = i + 2 + lg;
				break;
			case Unlock_Transaction:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)Unlock_Transaction;
				if ((lg== 0) && (tr_state ==  3)) {
//					&& (recvBuffer[2] ==  pfp_tr_data->TR_Buff_Contr_Id[0] )  \
//					&& (recvBuffer[3] == pfp_tr_data->TR_Buff_Contr_Id[1]  )  ) {
					pfp_tr_data->TR_Buff_Contr_Id[0] = '\0';
					pfp_tr_data->TR_Buff_Contr_Id[1] = '\0';
					//pfp_tr_data->Trans_State = 1;
					TR_Operation = Unlock_Transaction;
				} else if (tr_state == 1) { 
					tmpDataAck = '\x03';
					tmpMSAck = '\x05'; 
					//UserLog("write  C_DAT's Nb_Products error!fp state not in 1,2. ") 
				} else  { 
					tmpDataAck = '\x05';
					tmpMSAck = '\x05';
					//UserLog("write  C_DAT's Nb_Products error!too big or small.") 
				}			
				tmp_lg++;
				buffer[tmp_lg] = tmpDataAck;
				i = i + 2 + lg;
				break;
			 /* Clear_Transaction=30, Lock_Transaction, Unlock_Transaction,  */
			default:
				tmp_lg++;
				buffer[tmp_lg] = recvBuffer[13+ i]  ;
				tmpDataAck = '\x02';  
				tmpMSAck = '\x05';
				tmp_lg++;
				buffer[tmp_lg] = tmpDataAck;
				i = i + 2 + lg;
			break;	
		}	
	}
	if( (i + 5) != recv_lg )	{
		//UserLog("write TR_Dat error!! exit. ");
		UserLog("i  != recv_lg-5, write TR_Dat error!! exit, recv_lg[%d],i [%d] ",recv_lg, i);
		return -2;
	}
	tmp_lg++;
	buffer[6] = (unsigned char )((tmp_lg-8) / 256) ;
	buffer[7] = (unsigned char )((tmp_lg-8) % 256);  //big end.
	buffer[13] = tmpMSAck;  

	if (TR_Operation != 0x00) {
		switch (TR_Operation) {
		case Clear_Transaction:
			ret = SendData2CD(buffer, tmp_lg, 5);  // ACK first
			ret = ChangeTR_State( node, fp_ad, pfp_tr_data , (unsigned char)1);
			if (ret < 0) {
				UserLog("发送交易状态主动数据失败");
			}
			RunLog("Clear Tr 成功...............");
			return 1;   // 上层不再发送ACK报文
		case Lock_Transaction:
			ret = SendData2CD(buffer, tmp_lg, 5);
			ret = ChangeTR_State( node, fp_ad, pfp_tr_data , (unsigned char)3);
			return 1;
		case Unlock_Transaction:
			ret = SendData2CD(buffer, tmp_lg, 5);
			ret = ChangeTR_State( node, fp_ad, pfp_tr_data , (unsigned char)1);
			return 1;
		default:
			break;
		}
	}

	if (NULL != msg_lg) {
		*msg_lg= tmp_lg;
	}
	if(NULL != sendBuffer) {
	      	memcpy(sendBuffer, buffer, tmp_lg);
	}

	return 0;	
}


/* 
读取脱机文件中的最后一笔交易记录
modify by liys 2008-03-18
for example:

CD read all of one node:
0101 0201 00 00 000D 04 20 21 0000 C8 05 06 07 08 0A CA CB

Answer one by one
0201 0101 00 20 xxxx 04 21 21 xxxx C800 +need data(data_id + data_lg + data_el)

*/
/*
 * return: 0 - 有脱机交易; 1 - 无脱机交易; -1 - 出错
 */
int ReadTR_FILEDATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg)
{
	int i,j, recv_lg, tmp_lg;
	FP_TR_DATA *pfp_tr_data;
	unsigned char node;
	long k = 0;
	//int data_lg;
	unsigned char buffer[128];
	unsigned char trbuff[128];
	int n,fd,flength;
	unsigned char lsTrName[128];
	unsigned char *pFilePath;
	unsigned char *mapped_mem;
	
	memset(buffer, 0, sizeof(buffer));
	bzero(trbuff,sizeof(trbuff));
	
	if (NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,交易数据Read  失败");
		return -1;
	}

	if ((NULL == recvBuffer) ||  (NULL == sendBuffer) ){
		UserLog("交易数据错误,交易数据Read 失败");
		return -1;
	}
	node = recvBuffer[1];

	if (node <= 0 ){
		UserLog("加油机节点值错误,交易数据Read 失败");
		return -1;
	}
	
	if (0x04 != recvBuffer[8]){
		UserLog("交易数据错误,交易数据Read 失败");
		return -1;
	}

	sprintf(lsTrName, "%s%02x",  TEMP_TR_FILE, node);

	fd = open(lsTrName, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
	if (fd == -1) {
		fd = open(lsTrName, O_RDWR);
		if (fd < 0) {
			RunLog("Open dsptr%02x failed", node);
			return -1;
		}
	}	

	flength = lseek(fd, 0, SEEK_END);

	RunLog("脱机文件大小: %d", flength);		

	if (flength < sizeof(FP_TR_DATA)) { //无脱机记录
		buffer[0] = recvBuffer[2];  
		buffer[1] = recvBuffer[3];

		//发送方   
		buffer[2] = recvBuffer[0];
		buffer[3] = recvBuffer[1];

		//recvBuffer[4]; IFSF_MC	
		buffer[4] = '\0';
		//ACK
		buffer[5] = (recvBuffer[5] & 0x1f) | 0xe0;

		//msg_length
		buffer[6] = '\0';
		buffer[7] = '\x06';

		//ad_l ad
		buffer[8] = '\x04';
		buffer[9] = '\x20';
		
		buffer[10] = '\x21';

		//tran_no					  
		buffer[11]='\0';
		buffer[12]='\0';

		//msg_ack
		buffer[13]= 0x00;
				
		tmp_lg = 14;
		*msg_lg = tmp_lg;
		memcpy(sendBuffer, buffer, tmp_lg);
		close(fd);

		return 1;      // 上层据此判断是否需要发送此回复报文
	}
			  
	lseek(fd, 0, SEEK_SET);

	mapped_mem = mmap(NULL, flength, PROT_READ, MAP_SHARED, fd, 0);
	memcpy(trbuff,mapped_mem,sizeof(FP_TR_DATA));              //FIFO	

	close(fd);	       
	munmap(mapped_mem, flength);
	
	pfp_tr_data = (FP_TR_DATA*)(trbuff);

	//接收方
	buffer[0] = recvBuffer[2];  
	buffer[1] = recvBuffer[3];

	//发送方   
	buffer[2] = recvBuffer[0];
	buffer[3] = recvBuffer[1];

       //recvBuffer[4]; IFSF_MC
	buffer[4] = '\0'; 

       //answer
	buffer[5] = (recvBuffer[5] & 0x1f) | 0x20;

       //msg_length
	buffer[6] = '\0';
	buffer[7] = '\0';

	//ad_l ad
	buffer[8] = '\x04';
	buffer[9] = pfp_tr_data->fp_ad;
	buffer[10] = '\x21';

	//tran_no	
	buffer[11] = pfp_tr_data->TR_Seq_Nb[0];
	buffer[12] = pfp_tr_data->TR_Seq_Nb[1];

	//read msg length				
	recv_lg = recvBuffer[6];
	recv_lg = recv_lg * 256 + recvBuffer[7];

	tmp_lg = 12;
//	RunLog("recv_lg = %d", recv_lg);

	for(i = 0; i < recv_lg - 5; i++) {
	//	printf("i:%d %02x; ", i, recvBuffer[13 + i]);
		switch(recvBuffer[13 + i]) {		
			case TR_Seq_Nb :
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)TR_Seq_Nb;
				tmp_lg++;
				buffer[tmp_lg] = '\x02';
				k= buffer[tmp_lg];
				for(j=0;j<k;j++){
					tmp_lg++;
					buffer[tmp_lg] = pfp_tr_data->TR_Seq_Nb[j];
				}
				break;			
			case TR_Contr_Id :
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)TR_Contr_Id;
				tmp_lg++;
				buffer[tmp_lg] = '\x02';
				k= buffer[tmp_lg];
				for(j=0;j<k;j++){
					tmp_lg++;
					buffer[tmp_lg] = pfp_tr_data->TR_Contr_Id[j];
				}
				break;
			case TR_Release_Token:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)TR_Release_Token;
				tmp_lg++;
				buffer[tmp_lg] = '\x01';
				tmp_lg++;
				buffer[tmp_lg] = pfp_tr_data->TR_Release_Token;
				break;			

			case TR_Fuelling_Mode:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)TR_Fuelling_Mode;
				tmp_lg++;
				buffer[tmp_lg] = '\x01';					
				tmp_lg++;
				buffer[tmp_lg] = pfp_tr_data->TR_Fuelling_Mode;				
				break;
				
			case TR_Amount : //05 脱机字段
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)TR_Amount;
				tmp_lg++;
				buffer[tmp_lg] = '\x05';
				k= buffer[tmp_lg];
#if 0
				RunLog("TR_Amount: %s", Asc2Hex(pfp_tr_data->TR_Amount, 5));
#endif
				for(j=0;j<k;j++){
					tmp_lg++;
					buffer[tmp_lg] = pfp_tr_data->TR_Amount[j];
				}
				break;	
			case TR_Volume :  //06 脱机字段
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)TR_Volume;
				tmp_lg++;
				buffer[tmp_lg] = '\x05';
				k= buffer[tmp_lg];
#if 0
				RunLog("TR_Volume: %s", Asc2Hex(pfp_tr_data->TR_Volume, 5));
#endif
				for(j=0;j<k;j++){
					tmp_lg++;
					buffer[tmp_lg] = pfp_tr_data->TR_Volume[j];
				}
				break;
			case TR_Unit_Price : //07脱机字段
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)TR_Unit_Price;
				tmp_lg++;
				buffer[tmp_lg] = '\x04';
				k= buffer[tmp_lg];
#if 0
				RunLog("TR_Unit_Price: %s", Asc2Hex(pfp_tr_data->TR_Unit_Price, 4));
#endif
				for(j=0;j<k;j++){
					tmp_lg++;
					buffer[tmp_lg] = pfp_tr_data->TR_Unit_Price[j];
				}
				break;
			case TR_Log_Noz: //08 脱机字段
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)TR_Log_Noz;
				tmp_lg++;
				buffer[tmp_lg] = '\x01';
				tmp_lg++;
#if 0
				RunLog("Log_Noz: %02x", pfp_tr_data->TR_Log_Noz);
#endif
				buffer[tmp_lg] = pfp_tr_data->TR_Log_Noz;
				break;
			case TR_Prod_Nb : //0A脱机字段
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)TR_Prod_Nb;
				tmp_lg++;
				buffer[tmp_lg] = '\x04';
				k= buffer[tmp_lg];
				for(j=0;j<k;j++){
					tmp_lg++;
					buffer[tmp_lg] = pfp_tr_data->TR_Prod_Nb[j];
				}
				break;
			case TR_Error_Code:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)TR_Error_Code;
				tmp_lg++;
				buffer[tmp_lg] = '\x01';
				tmp_lg++;
				buffer[tmp_lg] = pfp_tr_data->TR_Error_Code;
				break;
			case TR_Security_Chksum :
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)TR_Security_Chksum;
				tmp_lg++;
				buffer[tmp_lg] = '\x03';
				k= buffer[tmp_lg];
				for(j=0;j<k;j++){
					tmp_lg++;
					buffer[tmp_lg] = pfp_tr_data->TR_Security_Chksum[j];
				}
				break;
			case Trans_State:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)Trans_State;
				tmp_lg++;
				buffer[tmp_lg] = '\x01';
				tmp_lg++;
				buffer[tmp_lg] = pfp_tr_data->Trans_State;
				break;
			case TR_Buff_Contr_Id :
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)TR_Buff_Contr_Id;
				tmp_lg++;
				buffer[tmp_lg] = '\x02';
				k= buffer[tmp_lg];
				for(j=0;j<k;j++){
					tmp_lg++;
					buffer[tmp_lg] = pfp_tr_data->TR_Buff_Contr_Id[j];
				}
				break;
			case TR_RD_OFFLINE: //C8  读脱机交易
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)TR_RD_OFFLINE;
				tmp_lg++;
				buffer[tmp_lg] = '\x00';
				break;
			case TR_Date:  //CA 脱机字段
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)TR_Date;
				tmp_lg++;
				buffer[tmp_lg] = '\x04';
				k= buffer[tmp_lg];
				for (j = 0; j < k; j++){
					tmp_lg++;
					buffer[tmp_lg] = pfp_tr_data->TR_Date[j];
				}
				break; 
			case TR_Time:  //CB 脱机字段
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)TR_Time;
				tmp_lg++;
				buffer[tmp_lg] = '\x03';
				k= buffer[tmp_lg];
				for (j = 0; j < k; j++) {
					tmp_lg++;
					buffer[tmp_lg] = pfp_tr_data->TR_Time[j];
				}

				RunLog("Node[%02d]FP[%d]Noz[%d] OFFLINE-Vol: %02x%02x.%02x Amt: %02x%02x.%02x Price: %02x.%02x STotal: %02x%02x%02x%02x.%02x ETotal: %02x%02x%02x%02x.%02x DTime: %02x-%02x %02x:%02x:%02x", 
					pfp_tr_data->node, pfp_tr_data->fp_ad - 0x20,
				       	pfp_tr_data->TR_Log_Noz,
					pfp_tr_data->TR_Volume[2],
					pfp_tr_data->TR_Volume[3],
					pfp_tr_data->TR_Volume[4],
					pfp_tr_data->TR_Amount[2],
					pfp_tr_data->TR_Amount[3],
					pfp_tr_data->TR_Amount[4],
					pfp_tr_data->TR_Unit_Price[2],
					pfp_tr_data->TR_Unit_Price[3],
					pfp_tr_data->TR_Start_Total[2],
					pfp_tr_data->TR_Start_Total[3],
					pfp_tr_data->TR_Start_Total[4],
					pfp_tr_data->TR_Start_Total[5],
					pfp_tr_data->TR_Start_Total[6],
					pfp_tr_data->TR_End_Total[2],
					pfp_tr_data->TR_End_Total[3],
					pfp_tr_data->TR_End_Total[4],
					pfp_tr_data->TR_End_Total[5],
					pfp_tr_data->TR_End_Total[6],
					pfp_tr_data->TR_Date[2],
					pfp_tr_data->TR_Date[3],
					pfp_tr_data->TR_Time[0],
					pfp_tr_data->TR_Time[1],
					pfp_tr_data->TR_Time[2]);
				break; 
			case TR_Start_Total:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)TR_Start_Total;
				tmp_lg++;
				buffer[tmp_lg] = 0x07;
				tmp_lg++;
				memcpy(&buffer[tmp_lg], pfp_tr_data->TR_Start_Total, 7);
				tmp_lg += 0x06;
				break;
			case TR_End_Total:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)TR_End_Total;
				tmp_lg++;
				buffer[tmp_lg] = 0x07;
				tmp_lg++;
				memcpy(&buffer[tmp_lg], pfp_tr_data->TR_End_Total, 7);
				tmp_lg += 0x06;
				break;
			default:
//				RunLog("switch.default:i=%d, recvBuffer[13+i]=%d", i, recvBuffer[13+i]); 
				tmp_lg++;
				buffer[tmp_lg] = recvBuffer[13 + i];
				tmp_lg++;
				buffer[tmp_lg] = '\0';			
				break;			
		}
	}

	tmp_lg++;
	buffer[6] = (unsigned char )((tmp_lg-8) / 256) ;
	buffer[7] = (unsigned char )((tmp_lg-8) % 256);  //big end.
	*msg_lg = tmp_lg;

//	RunLog("end of ReadTR_FILEDATA, buffer: %s", Asc2Hex(buffer, tmp_lg));

	memcpy(sendBuffer, buffer, tmp_lg);	
	close(fd);

	return 0;
}


/* 
删除脱机文件中的最后一笔交易记录
modify by liys 2008-03-18
for example:

CD clear offline Transaction:
0101 0201 00 40 xxxx 04 21 21 xxxx C900

Answer  for clear
0101 0201 00 E0 xxxx 04 21 21 xxxx 00 C9 00
*/
int DeleteTR_FILEDATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg)
{
	unsigned char node;
	unsigned char buffer[128];
	int fd,m,flength;
	unsigned char lsTrName[128];
	unsigned char *pFilePath;
	unsigned char *mapped_mem;
	FP_TR_DATA *pfp_tr_data;
	
	memset(buffer, 0, sizeof(buffer));
	
	if((NULL == recvBuffer) ||  (NULL == sendBuffer) ){
		UserLog("交易数据错误,交易数据Read 失败");
		return -1;
	}
	node = recvBuffer[1];

	if( node <= 0 ){
		UserLog("加油机节点值错误,交易数据Read 失败");
		return -1;
	}
	
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,交易数据Read  失败");
		return -1;
	}
	if ((unsigned char)4 !=recvBuffer[8]){
		UserLog("交易数据错误,交易数据Read 失败");
		return -1;
	}


	sprintf( lsTrName, "%s%02x", TEMP_TR_FILE,node);   // dsptrxx

	m = GetFileSizeByFullName(lsTrName);	

	if( m == -1 ) {
		UserLog( "取脱机交易文件 失败." );
		return -1;
	} else if(m == -2) {	//-2文件不存在
		UserLog( "脱机交易 文件不存在" );
		return -1;
	}		      
			
	fd = open(lsTrName, O_RDWR);
	if (fd < 0) {
		return -1;
	}

	flength = lseek(fd, 0, SEEK_END);
	
	lseek(fd, 0, SEEK_SET);
	mapped_mem = mmap(NULL, flength, PROT_READ | PROT_WRITE, MAP_SHARED,fd, 0);		
	pfp_tr_data = (FP_TR_DATA *)mapped_mem;
      
	buffer[0] = recvBuffer[2];
	buffer[1] = recvBuffer[3];
	buffer[2] = recvBuffer[0];  
	buffer[3] = recvBuffer[1];

	//recvBuffer[4]; IFSF_MC	
	buffer[4] = '\0';

	//ACK
	buffer[5] = (recvBuffer[5] & 0x1f) | 0xe0;

	//msg_length
	buffer[6] = '\0';
	buffer[7] = '\x08';

	//ad_l ad
	buffer[8] = '\x04';

	buffer[9] = recvBuffer[9];
	buffer[10] = '\x21';

	//tran_no					  
	buffer[11] = recvBuffer[11];
	buffer[12] = recvBuffer[12];

	if (memcmp(pfp_tr_data->TR_Seq_Nb, &recvBuffer[11], 2) == 0) {	
//		RunLog(">>>>>>>> 清除对应的脱机交易记录");
		memset(mapped_mem,0,sizeof(FP_TR_DATA));   //删除文件的第一条记录 FIFO
		memcpy(mapped_mem,mapped_mem + sizeof(FP_TR_DATA) ,flength - sizeof(FP_TR_DATA));
			 
//		RunLog(">>>>>>>> 文件同步<<<<<<<<");
		msync(mapped_mem, flength,MS_SYNC);	

		// 改变文件大小
		if (ftruncate(fd, m - sizeof(FP_TR_DATA)) == -1) { //同时截断文件
			UserLog("ftruncate file error!");
			return -1;
		}
		  	      		               
		//data_ack
		buffer[13]='\0';

		//C900
		buffer[14]='\xC9';
		buffer[15]='\0';
	} else {
		buffer[13]='\05';
		buffer[14]='\xC9';
		buffer[15]='\06';
	}
			
	*msg_lg = 16;
	memcpy(sendBuffer, buffer, *msg_lg);

	close(fd);
	munmap(mapped_mem, flength); 
	

	return 0;		
}


//int PackTR_DATA(){ }//不写,不用


//离线状态时:把buff中的所有未支付记录状态置为离线已支付状态 
//modify by liys 2008-03-14
int OffLineSetBuff( const unsigned char node)
{
	int inode,i;
	long trAd;
	FP_TR_DATA *pfp_tr_data;

	if(  node == 0  ) {
		UserLog("加油机节点值错误,交易数据OffLineStatus 失败");
		return -1;
	}

	if( NULL == gpIfsf_shm ) {		
		UserLog("共享内存值错误,交易数据OffLineStatus 失败");
		return -1;
	}

	for(i=0; i < MAX_CHANNEL_CNT; i++) {
		if( node == gpIfsf_shm->convert_hb[i].lnao.node) { //找到了加油机节点
		  inode = i;
		  break;
		}
	}
	if(i == MAX_CHANNEL_CNT ) {
		UserLog("加油机节点数据找不到,加油点OffLineStatus  失败");
		return -1;
	}

	trAd = inode *MAX_TR_NB_PER_DISPENSER;//起始地址
	
	for(i=0; i<MAX_TR_NB_PER_DISPENSER; i++) {
		pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd+i]) );				
		if( (pfp_tr_data->Trans_State == PAYABLE)  ||
			(pfp_tr_data->Trans_State == LOCKED)) { //如果buff中的记录为未支付状态,则offline设为 离线已支付
			pfp_tr_data->offline_flag = OFFLINE_TR; 
			pfp_tr_data->Trans_State = CLEARED;
			SET_FP_OFFLINE_TR(pfp_tr_data->node, (pfp_tr_data->fp_ad - 0x21));
		} 
	}	

	return 0;
}


//离线变为在线时:把buff中的所有的离线记录写入文件
//modify by liys 2008-03-14
int WriteBuffDataToFile(const unsigned char node)
{
	int tr_state, inode, ret;//,noPay,ccnt
	FP_TR_DATA *pfp_tr_data;
	FP_TR_DATA *pfp_tr_data_tmp; //ll
	long i,  trAd ,j;
	static int flag;//n, j,k,
	int fd;//len, 
	char fileName[128] ;
	int cnt = 1;
	
	if (node == 0){
		UserLog("加油机节点值错误,交易数据WriteBuffDataToFile 失败");
		return -1;
	}

	if(NULL == gpIfsf_shm){
		UserLog("共享内存值错误,交易数据WriteBuffDataToFile 失败");
		return -1;
	}

	bzero(fileName, sizeof(fileName));
	sprintf(fileName,"%s%02x",TEMP_TR_FILE, node);	
	fd = open(fileName, O_RDWR|O_CREAT|O_APPEND, 00644);
	if (fd < 0) {
		UserLog( "打开交易数据临时存放文件[%s]失败.",fileName );
		return -2;
	}

	RunLog("Open dsptr%02x successful!", node);
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node) { //找到了加油机节点
			inode = i;
			break;
		}
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,加油点WriteFileAllTR_DATA  失败");
		return -1;
	}
		
	trAd = inode * MAX_TR_NB_PER_DISPENSER;  //起始地址
	
//	写入文件前，对离线交易进行排序。索引为TR_Date  TR_Time 
//modify by lil  @2009-07-07
	do {
		flag = 0;  
		for (i = 0; i < MAX_TR_NB_PER_DISPENSER; i++){
			pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd+i]) );	
	//		if ((pfp_tr_data->node > 0)&&(pfp_tr_data->fp_ad != 0)) {
			if ((pfp_tr_data->node > 0) && (pfp_tr_data->offline_flag != 0)) {

				for (j = 0; j < MAX_TR_NB_PER_DISPENSER; j++) {
					pfp_tr_data_tmp =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd+j]) );	
					if ((pfp_tr_data_tmp->node > 0) && (pfp_tr_data_tmp->offline_flag != 0)) {
						if((memcmp(&pfp_tr_data->TR_Date[0] , &pfp_tr_data_tmp->TR_Date[0] , 7) > 0)) {
							flag = 1;  //如果先找到非最小索引值，循环结束后从头再次循环
							break;
						} else {
							continue;
						}
					}
				}

				if( j == MAX_TR_NB_PER_DISPENSER  ){
					ret = write(fd, pfp_tr_data,  sizeof(FP_TR_DATA));
					if (ret < 0) {
						i -= cnt;
						cnt = 0;
					}
					pfp_tr_data->offline_flag = 0;
				}
			}
		}
    	} while (flag);

	close(fd);

	RunLog("Write all offline TR of Node %d successful", node);

	return 0;
}


/* 
 * func: 解析具体的脱机交易处理指令并执行, 由油机模块调用
 * ret : 0 - 成功; -1 - 失败; 1 - recvBuffer 为 NULL
 */

int upload_offline_tr(const unsigned char node, const unsigned char *recvBuffer) 
{
	int i, ret, m_st, msg_lg;
	unsigned char buffer[256] = {0};
	static unsigned char ReadTRCMD[32] = {0};  // 保存脱机交易读取命令
	static int cmdlen = 0;

	// upload progress == 0 : 还没有读取命令; 或者是已经读完, 缓冲区中没有脱机交易， 也没有未完成的脱机交易
	// upload progress == 1 : 读取中...
	// upload progress == 2 : 已经下发读取命令, 缓冲区中没有脱机交易, 但是有未完成的脱机交易
	// upload progress == 3 : 所有未完成脱机交易均已完成, 等待处理
	static int upload_progress = 0;

	if (NULL != recvBuffer) {
		RunLog("Node %d FP 1, upload_progress: %d, offline_flag: %02x",
		       	node, upload_progress, gpIfsf_shm->fp_offline_flag[node - 1][0]);
	}

	if (gpIfsf_shm->auth_flag != DEFAULT_AUTH_MODE) {
		upload_progress = 0;
		return 0;
	}

	switch (upload_progress) {
		case 0:
		case 1:
			// 如果缓冲区有脱机交易, 则写入文件
			if (has_offline_tr_in_buf(node)) {
//				RunLog("%s: node %d case %d, 缓冲区有脱机交易", __FUNCTION__, node, upload_progress);
				ret = WriteBuffDataToFile(node);
				if (ret == 0) {
					clr_offline_flag(node);
				} else {
					RunLog("节点 %d 写脱机交易到文件失败", node);
					return -1;
				}
			}
			break;
		case 2:
			// 如果所有FP都已完成未完成的脱机交易, 则主动上传脱机交易, 否则直接返回
			// 所有未完成脱机交易一起上传
			if (has_incomplete_offline_tr(node)) {
				RunLog("%s: case 2, node %d 有未完成的脱机交易", __FUNCTION__, node);
				return 0;
			}

			// 如果缓冲区中有脱机交易(未完成的已完成), 则写入文件; 
			if (has_offline_tr_in_buf(node)) {
				RunLog("%s: node %d case 2, 缓冲区有脱机交易, 无未完成脱机交易",__FUNCTION__, node);
				// 如果缓冲区有脱机交易, 则写入文件
				ret = WriteBuffDataToFile(node);
				if (ret == 0) {
					clr_offline_flag(node);
					upload_progress = 3;
//	RunLog("%s: case 2, Node %d 写缓冲区交易到文件成功, upload_progress: %d",__FUNCTION__, node, upload_progress);
				} else {
					RunLog("节点 %d 写脱机交易到文件失败", node);
					return -1;
				}
			}

			break;
		default:
			break;
	}
	
	// additional handle, unsolicited
	if (upload_progress == 3) {
//		RunLog("node %d 主动上传脱机交易", node);
		ReadTRCMD[1] = node;
		ReadTRCMD[5] = 0x00;           // token is zero
		ret = ReadTR_FILEDATA(ReadTRCMD, buffer, &msg_lg);
		if(ret == 0) {
			ret = SendData2CD(buffer, msg_lg, 5);
			if (ret <0) {		
				UserLog("发送回复数据错误,Translate 失败");
				return -1;
			}
			upload_progress = 0;
		}

		return 0;
	}

	// normal handle -- DoPolling 调用时, 无需上传交易或者恢复信息
	if (NULL == recvBuffer) {
		UserLog("recvBuffer is NULL");
		return 1; 
	}
	

	ret = -1;
	m_st = ((int)recvBuffer[5] & 0xe0) >> 5;
	
	switch(m_st) {
		case 0:			
			cmdlen = (recvBuffer[6] * 256) + recvBuffer[7] + 8;
			memcpy(ReadTRCMD, recvBuffer, cmdlen);        // backup for next TR

			ret = ReadTR_FILEDATA(recvBuffer, buffer,&msg_lg);  
			if (ret == 1) {  // 无脱机交易, 是否有未完成的呢?
				if (has_incomplete_offline_tr(node)) {
					RunLog("after ReadTR_FILE, node %d 有未完成脱机交易 ", node);
					upload_progress = 2;
				} else {
					upload_progress = 0;
				}
			}

			if (ret >= 0) {  // 有无脱机交易都ACK
				// 如果加油中, 则恢复 device busy
				if (upload_progress == 2) {
					buffer[13] = 0x07;    // busy
					RunLog("---------------- device busy --------------");
				}

				ret = SendData2CD(buffer, msg_lg, 5);
				if (ret < 0) {		
					RunLog("发送回复数据错误,Translate 失败");
					return -1;
				}
//				RunLog("发送回复数据 %s 成功", Asc2Hex(buffer, msg_lg));
			}

			break;
		case 1:
			break;
		case 2:
			ret = DeleteTR_FILEDATA(recvBuffer, buffer, &msg_lg);
			if (ret == 0) {
				ret = SendData2CD(buffer, msg_lg, 5);
				if (ret < 0) {		
					UserLog("发送回复数据错误,Translate 失败");
					return -1;
				}
			}
			//缅甸修改主动上传模式,改为清脱机交易后不主动上传
			
			if (ReadTRCMD[0] == 0) {
				break;
			}

			ReadTRCMD[5] = (ReadTRCMD[5] & 0xe0) | (recvBuffer[5] & 0x1f);  // get token
			ret = ReadTR_FILEDATA(ReadTRCMD, buffer, &msg_lg);
			if (ret == 1) {
				if (has_incomplete_offline_tr(node)) {
					RunLog("after Delete_FILE& ReadTR_FILE, node %d 有未完成脱机交易 ", node);
					upload_progress = 2;
				} else {
					upload_progress = 0;
				}
			} else if(ret == 0) {  // 还有脱机交易数据则ACK, 如果脱机交易传完则不再发送ACK
				ret = SendData2CD(buffer, msg_lg, 5);
				if (ret <0) {		
					UserLog("发送回复数据错误,Translate 失败");
					return -1;
				}
			}
			
			break;
		case 3:
		case 4:
		case 7:
			break;
		default :
			UserLog("收到的数据的消息类型错误!!");
			return -1;
	}
	
	return 0;
}

/*
 * func: 判断缓冲区中是否有该节点的脱机交易
 * ret : 1 - 有脱机交易; 0 - 没有
 */
int has_offline_tr_in_buf(unsigned char node)
{
	int i;

	for (i = 0; i < MAX_FP_CNT; ++i) {
		if ((gpIfsf_shm->fp_offline_flag[node - 1][i] & 0xE0) != 0) {
			return 1;
		}
	}

	return 0;
}

/*
 * func: 清除该节点的缓冲区中有脱机交易标志
 */
void clr_offline_flag(unsigned char node)
{
	int i;

	for (i = 0; i < MAX_FP_CNT; ++i) {
		CLR_FP_OFFLINE_TR(node, i);
//		gpIfsf_shm->fp_offline_flag[node - 1][i] &= 0x0F;
//		RunLog("cls_offline_flag: fp_offline_flag[%d][%d] : %02x", node - 1, i, 
//						gpIfsf_shm->fp_offline_flag[node - 1][i]);
	}
}

/*
 * func:该节点有未完成的脱机交易
 * ret : 1 - 有未完成脱机交易; 0 - 没有
 */
int has_incomplete_offline_tr(unsigned char node)
{
	int i;
	for (i = 0; i < MAX_FP_CNT; ++i) {
		if ((gpIfsf_shm->fp_offline_flag[node - 1][i] & 0x0F) != 0) {
			return 1;
		}
	}

	return 0;
}

/*-----------------------------------------------------------------*/








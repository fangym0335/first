/*******************************************************************
���ײ�������
-------------------------------------------------------------------
2007-07-04 by chenfm
2008-02-28 - Guojie Zhu <zhugj@css-intelligent.com>
    ���Ӻ���ReachedUnPaidLimit �����ж�δ֧�����ױ����Ƿ�ﵽ����
2008-03-06 - Guojie Zhu <zhugj@css-intelligent.com>
    convertUsed[] ��Ϊ convert_hb[].lnao.node
2008-03-07 - Guojie Zhu <zhugj@css-intelligent.com>
    AddTR_DATA���Ƴ��Խ��׽���У��Ĳ���( CalcTrCheksum(pfp_tr_data))
    WriteTR_DATA���Ƴ�recvBuffer[2/3]��TR_Buff_Contr_Id�ıȶ�
    ����Clear TR��ACK����û��MS_ACK������(WriteTR_DATA��while(){}֮ǰ���� tmp_lg = 13)
    ReadTR_DATA�е�case TR_Date_Time���Ϊ TR_Date & TR_Time
2008-03-10 - Guojie Zhu <zhugj@css-intelligent.com>
    WriteTR_DATA������MSACK������?buffer[15] = tmpMSAck =>  buffer[13] = tmpMSAck  
    ����ReadTR_DATA�ظ����ݵĸ�ʽ����
2008-03-20 - Guojie Zhu <zhugj@css-intelligent.com>
    WriteTR_DATA�ж�TR������������ChangeTR_State֮ǰ�Ȼ�ACK
2008-04-07 - Guojie Zhu <zhugj@css-intelligent.com>
    ����GetTrAd�������
2008-05-13 - Yongsheng Li <Liys@css-intelligent.com>
    ������һ������GetAllOfflineCnt
    ����AddTR_DATA �������ж��ѻ���������100 ,��д�ļ�

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


extern IFSF_SHM *gpIfsf_shm; //�ýṹ��ȫ�ֵ�����ֻ��һ��,��ifsf_main.c�ж���.

/*--------------------------------------------------------------*/
//���ifsf ���ݿ���ĳ��fp ���еĽ�������
static int GetAllTrCnt(const char node, const char fp_ad){
	long i, trAd;
	int k = 0;
	FP_TR_DATA *pfp_tr_data;	
	
	if( node == 0){
		UserLog("���ͻ��ڵ�ֵ����,�������ݵ�ַGetClearTrCnt  ʧ��");
		return -1;
	}
	
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,�������ݵ�ַGetClearTrCnt  ʧ��");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,���͵�GetClearTrCnt  ʧ��");
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

//��ѯbuffer �����߽��׵ļ�¼����add by liys 2008-05-13
int  GetAllOfflineCnt(const unsigned char node){
	long i, trAd;
	int k = 0;
	FP_TR_DATA *pfp_tr_data;	
	
	if( node == 0){
		UserLog("���ͻ��ڵ�ֵ����,�������ݵ�ַGetClearTrCnt  ʧ��");
		return -1;
	}
	
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,�������ݵ�ַGetClearTrCnt  ʧ��");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,���͵�GetClearTrCnt  ʧ��");
		return -1;
	}
	trAd = i *MAX_TR_NB_PER_DISPENSER;
	for(i=0; i<MAX_TR_NB_PER_DISPENSER; i++){ 
		pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd+i]) );
		if(pfp_tr_data->offline_flag != 0) //�ѻ����׼�¼
			k++;	
	}
	return k;
}


//���ĳ���ڵ������ �����߽�������test
static int GetNodeAllOnLineTrCnt(const char node){
	long i, trAd;
	int k = 0;
	FP_TR_DATA *pfp_tr_data;	
	
	if( node == 0){
		UserLog("���ͻ��ڵ�ֵ����,�������ݵ�ַGetClearTrCnt  ʧ��");
		return -1;
	}
	
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,�������ݵ�ַGetClearTrCnt  ʧ��");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,���͵�GetClearTrCnt  ʧ��");
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


//���ĳ���ڵ�����еĽ�������
static int GetNodeAllTrCnt(const char node){
	long i, trAd;
	int k = 0;
	FP_TR_DATA *pfp_tr_data;	
	
	if( node == 0){
		UserLog("���ͻ��ڵ�ֵ����,�������ݵ�ַGetClearTrCnt  ʧ��");
		return -1;
	}
	
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,�������ݵ�ַGetClearTrCnt  ʧ��");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,���͵�GetClearTrCnt  ʧ��");
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
		UserLog("���ͻ��ڵ�ֵ����,�������ݵ�ַGetClearTrCnt  ʧ��");
		return -1;
	}
	
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,�������ݵ�ַGetClearTrCnt  ʧ��");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,���͵�GetClearTrCnt  ʧ��");
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
 * func: �ж��Ƿ�ﵽδ֧�����׵����Ʊ���
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
		UserLog("���ͻ��ڵ�ֵ����,�������ݵ�ַGetClearTrCnt  ʧ��");
		return -1;
	}
	
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,�������ݵ�ַGetClearTrCnt  ʧ��");
		return -1;
	}

	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node) {
		      break; //�ҵ��˼��ͻ��ڵ�
		}
	}

	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,���͵�RechedUnPaidLimitʧ��");
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
		UserLog("�ﵽδ֧�����ױ����޶�ֵ %d", UNPAIDLIMIT);
		return 1;
	}
}


//���ĳ�����ͽڵ����֧����������
static int GetNodeClearTrCnt(const char node){
	long i, trAd;
	int k = 0;
	FP_TR_DATA *pfp_tr_data;	
	
	if( node == 0){
		UserLog("���ͻ��ڵ�ֵ����,�������ݵ�ַGetClearTrCnt  ʧ��");
		return -1;
	}
	
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,�������ݵ�ַGetClearTrCnt  ʧ��");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,���͵�GetClearTrCnt  ʧ��");
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

//���ĳ��fp δ֧�����׵�����
int GetPayTrCnt(const char node, const char fp_ad){
	long i, trAd;
	int k = 0;
	int n = 0;
	FP_TR_DATA *pfp_tr_data;	
	
	if( node == 0 ){
		UserLog("���ͻ��ڵ�ֵ����,�������ݵ�ַGet  ʧ��");
		return -1;
	}
	
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,�������ݵ�ַGet  ʧ��");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,���͵�SetSgl  ʧ��");
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


//���ĳ���ڵ�δ֧�����׵�����
static int GetNodePayTrCnt(const char node){
	long i, trAd;
	int k = 0;
	//int n = 0;
	FP_TR_DATA *pfp_tr_data;	
	
	if( node == 0 ){
		UserLog("���ͻ��ڵ�ֵ����,�������ݵ�ַGet  ʧ��");
		return -1;
	}
	
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,�������ݵ�ַGet  ʧ��");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,���͵�SetSgl  ʧ��");
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

//ÿһ��fp �Ľ���˳��Ŷ��� ��0001 -9999,�ж��Ƿ� �ػ�,����ػ�����1,����0
static int Is99990001(const char node, const char fp_ad){
	long i, j,  trAd;
	FP_TR_DATA *pfp_tr_data;
	unsigned char tmp_tr_nb[2];
	unsigned char zero2[2];
	unsigned char tmp01[2] = {0,1};
	//unsigned char tmp99[2] = {'\x99','\x99'};
	int flag9901 = 0;	
	if(node == 0){
		UserLog("���ͻ��ڵ�ֵ����,�������ݵ�ַGetHeadTrAd  ʧ��");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,���͵�GetHeadTrAd  ʧ��");
		return -2;
	}
	trAd = i *MAX_TR_NB_PER_DISPENSER;//ÿ���ڵ㽻�׼�¼����ʼ��ַ4* 64

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
	//���ҽ��׺����Ƿ����9999-->0001	
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

//�õ����µ�һ��δ֧������ 
//gpIfsf_shm->dispenser_data[i].fp_data[fp_ad - 0x21].Transaction_Sequence_Nb -1,�ж��Ƿ�ػ�,1 �����
//ͬʱ���ü�¼�Ƿ����,�������򷵻�-1
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
		UserLog("���ͻ��ڵ�ֵ����,�������ݵ�ַGetHeadTrAd  ʧ��");
		return -1;
	}	
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,�������ݵ�ַGetHeadTrAd  ʧ��");
		return -1;
	}
	
	//tmp_tr_nb[0] = '\0';
	//tmp_tr_nb[1] = '\0';	
	k = 0;
	//������ʼ��ַ
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,���͵�GetHeadTrAd  ʧ��");
		return -2;
	}
	trAd = i *MAX_TR_NB_PER_DISPENSER;//��ʼ��ַ
	
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
			//ֻ��pfp_tr_data->TR_Seq_Nb�����MAX_TR_NB_PER_DISPENSERС��.
			if(   (pfp_tr_data->fp_ad == fp_ad) &&  (pfp_tr_data->node == node) \
					&&( memcmp(tmp99, pfp_tr_data->TR_Seq_Nb, 2) == 0)   ) {
				k=trAd+i;
				return k;
			}			
		}	
	}
	for(i=0; i<MAX_TR_NB_PER_DISPENSER; i++){
		pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd+i]) );
		//ֻ��pfp_tr_data->TR_Seq_Nb�����MAX_TR_NB_PER_DISPENSERС��.
		if(   (pfp_tr_data->fp_ad == fp_ad) &&  (pfp_tr_data->node == node) \
				&&( memcmp(tmp_tr_nb, pfp_tr_data->TR_Seq_Nb, 2) == 1)  ) {//&&( memcmp(tmp_tr_nb, pfp_tr_data->TR_Seq_Nb, 2) > 0)  \
			k=trAd+i;
			return k;
		}			
	}
	/*
	//���ҽ��׺����Ƿ����9999-->0001	
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
		//MAX_TR_NB_PER_DISPENSER����С��500,�������
		if(MAX_TR_NB_PER_DISPENSER >=500){
			UserLog("MAX_TR_NB_PER_DISPENSER >=500, GetHeadTrAd  ʧ��");
			return -3;
		}
		tmp50[0] = MAX_TR_NB_PER_DISPENSER / 256;
		tmp50[1] = MAX_TR_NB_PER_DISPENSER % 256;		
		for(i=0; i<MAX_TR_NB_PER_DISPENSER; i++){
			pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd+i]) );
			//ֻ��pfp_tr_data->TR_Seq_Nb�����MAX_TR_NB_PER_DISPENSERС��.
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

//gpIfsf_shm->dispenser_data[i].fp_data[fp_ad - 0x21].Transaction_Sequence_Nb������-1 ,
//ֱ���ҵ���֧������Ϊֹ,ע��ػ�
//�õ�����һ����֧�����׵ĵ�ַ
static long GetClearHeadTrAd(const char node, const char fp_ad){
	long i, j, k, trAd;
	int ret;
	FP_TR_DATA *pfp_tr_data;
	unsigned char tmp01[2] = {0,1};
	unsigned char tmp99[2] = {'\x99','\x99'};
	unsigned char tmp50[2];
	int flag9901;	
	
	if(  node == 0  ){
		UserLog("���ͻ��ڵ�ֵ����,�������ݵ�ַGetClearHeadTrAd  ʧ��");
		return -1;
	}	
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,�������ݵ�ַGetClearHeadTrAd  ʧ��");
		return -1;
	}
	k = 0;
	//tmp_tr_nb[0] = '\0';
	//tmp_tr_nb[1] = '\0';	
	//������ʼ��ַ
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,���͵�GetClearHeadTrAd  ʧ��");
		return -2;
	}
	trAd = i *MAX_TR_NB_PER_DISPENSER;//��ʼ��ַ
	
	//���ҽ��׺����Ƿ����9999-->0001	
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
					//���߽��ײ����������һ�ʽ��ײ�������Ϊ���߽��ײ��ᱻɾ��
					//8-25 by lil  test
					&&( pfp_tr_data->Trans_State == (unsigned char)1)  ) {
				memcpy(tmp01, pfp_tr_data->TR_Seq_Nb,2);
				k=trAd+i;
			}					
		}
		if(k<0){
			UserLog("���ͻ����������Ҳ������״̬��,���͵�GetClearHeadTrAd  ʧ��");
		}
		return k;		
	}
	else {
		//MAX_TR_NB_PER_DISPENSER����С��500,�������
		if(MAX_TR_NB_PER_DISPENSER >=500){
			UserLog("MAX_TR_NB_PER_DISPENSER >=500, GeCleartHeadTrAd  ʧ��");
			return -3;
		}
		tmp50[0] = MAX_TR_NB_PER_DISPENSER / 256;
		tmp50[1] = MAX_TR_NB_PER_DISPENSER % 256;	
		for(i=0; i<MAX_TR_NB_PER_DISPENSER; i++){
			pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd+i]) );
			//ֻ��pfp_tr_data->TR_Seq_Nb�����MAX_TR_NB_PER_DISPENSERС���е�����.
			if(   (pfp_tr_data->fp_ad == fp_ad) &&  (pfp_tr_data->node == node) \
					&&( pfp_tr_data->Trans_State == (unsigned char)1) \
					&& (pfp_tr_data->offline_flag != OFFLINE_TR)\
					//���߽��ײ����������һ�ʽ��ײ�������Ϊ���߽��ײ��ᱻɾ��
					//8-25 by lil  test
					&& (memcmp(pfp_tr_data->TR_Seq_Nb,tmp50, 2) < 0) \
					&& (memcmp(pfp_tr_data->TR_Seq_Nb,tmp01, 2) >= 0)  ) {
				memcpy(tmp01, pfp_tr_data->TR_Seq_Nb, 2);
				k=trAd+i;
			}
		}
		if(k < 0){
			//û�ҵ�,���ұ�(1000 - MAX_TR_NB_PER_DISPENSER) ����е�����.
			tmp50[0] = (1000 - MAX_TR_NB_PER_DISPENSER) / 256;
			tmp50[1] = (1000 - MAX_TR_NB_PER_DISPENSER) % 256;
			tmp01[0] = 0;
			tmp01[1] = 1;
			for(i=0; i<MAX_TR_NB_PER_DISPENSER; i++){
				pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd+i]) );
				//ֻ��pfp_tr_data->TR_Seq_Nb�����(1000 - MAX_TR_NB_PER_DISPENSER) ����е�����.
				if(   (pfp_tr_data->fp_ad == fp_ad) &&  (pfp_tr_data->node == node) \
					&&( pfp_tr_data->Trans_State == (unsigned char)1) \
					&& (pfp_tr_data->offline_flag != OFFLINE_TR)\
					//���߽��ײ����������һ�ʽ��ײ�������Ϊ���߽��ײ��ᱻɾ��
					//8-25 by lil   test
					&& (memcmp(pfp_tr_data->TR_Seq_Nb,tmp50, 2) > 0) \
					&& (memcmp(pfp_tr_data->TR_Seq_Nb,tmp01, 2) >= 0)  ) {
					memcpy(tmp01, pfp_tr_data->TR_Seq_Nb, 2);
					k=trAd+i;
				}
			}
		}
		if(k<0){
			UserLog("���ͻ����������Ҳ������״̬��,���͵�GetClearHeadTrAd  ʧ��");
		}
		return k;
	}
	
	return -1;
}

//�õ�����һ�ʽ��׼�¼�ĵ�ַ,��ʱ����,����Ҫ�޸�
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
		UserLog("���ͻ��ڵ�ֵ����,�������ݵ�ַGetHeadTrAd  ʧ��");
		return -1;
	}	
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,�������ݵ�ַGetHeadTrAd  ʧ��");
		return -1;
	}
		
	//������ʼ��ַ
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,���͵�GetHeadTrAd  ʧ��");
		return -2;
	}	
	trAd = i *MAX_TR_NB_PER_DISPENSER;//��ʼ��ַ
	
	//���ҽ��׺����Ƿ����9999-->0001	
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
		//MAX_TR_NB_PER_DISPENSER����С��500,�������
		if(MAX_TR_NB_PER_DISPENSER >=500){
			UserLog("MAX_TR_NB_PER_DISPENSER >=500, GetHeadTrAd  ʧ��");
			return -3;
		}
		tmp50[0] = (1000 - MAX_TR_NB_PER_DISPENSER) / 256;
		tmp50[1] = (1000 - MAX_TR_NB_PER_DISPENSER) % 256;		
		for(i=0; i<MAX_TR_NB_PER_DISPENSER; i++){
			pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd+i]) );
			//ֻ��pfp_tr_data->TR_Seq_Nb�����MAX_TR_NB_PER_DISPENSER���.
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



//ɾ������һ����֧�����׼�¼
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
		UserLog("���ͻ��ڵ�ֵ����,��������Del ʧ��");
		return -1;
	}
	
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,��������Del ʧ��");
		return -1;
	}
	
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node) { //�ҵ��˼��ͻ��ڵ�
			inode = i;
			 break;
		}
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,���͵�DelLastTR_DATA  ʧ��");
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
	
	trAd = inode *MAX_TR_NB_PER_DISPENSER;//��ʼ��ַ
	//���ҽ��׺����Ƿ����9999-->0001
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
	//û��9999-->0001, ɾ����Сֵ
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
			UserLog("�ý������ݲ�����clear ״̬,��������Del ʧ��");
			return -2;
		}
	} else {//����9999-->0001,ɾ������1000-MAX_TR_NB_PER_DISPENSER����Сֵ
		//MAX_TR_NB_PER_DISPENSER����С��500,�������
		if(MAX_TR_NB_PER_DISPENSER >=500){
			UserLog("MAX_TR_NB_PER_DISPENSER >=500, GetHeadTrAd  ʧ��");
			return -3;
		}
		tmp50[0] = (1000 - MAX_TR_NB_PER_DISPENSER) / 256;
		tmp50[1] = (1000 - MAX_TR_NB_PER_DISPENSER) % 256;
		k=0;
		for(i=0; i<MAX_TR_NB_PER_DISPENSER; i++){
			pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd+i]) );
			//ֻ��pfp_tr_data->TR_Seq_Nb�����1000-MAX_TR_NB_PER_DISPENSER�����Сֵ.
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
			UserLog("�ý������ݲ�����clear ״̬,��������Del ʧ��");
			return -1;
		} else {			
			UserLog("���ҽ��״���, ɾ��ʧ��");
			return -1;
		}	
		
	}
	//���ڵ����н����������������һ������,��ȡ���ݵĽ�������,����еĻ�.
	//��ʱ����ccnt,ͳ�Ʊ��ڵ����н���������
#if 0
	// comment out by jie @ 3.10 for test
	ccnt = GetNodeAllTrCnt(node);
	if(ccnt < MAX_TR_NB_PER_DISPENSER/4-2){
		ret = ReadHalfNodeTR_DATA(node);
	}
#endif
	return 0;
}

// ������֧��״̬��������� ���ߵ�Ҳ���ϴ��Ŀ��������־
int DelAllClearTR_DATA(const char node){
	int tr_state, inode, ret, acnt;//,noPay,ccnt
	FP_TR_DATA *pfp_tr_data;
	//unsigned char node, fp_ad;
	long i,j, k, trAd;//n, 	
	//int data_lg;
	if(  node == 0  ){
		UserLog("���ͻ��ڵ�ֵ����,��������Del ʧ��");
		return -1;
	}	
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,��������Del ʧ��");
		return -1;
	}

	k = 0;
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node) { //�ҵ��˼��ͻ��ڵ�
		inode = i;
		 break;
		}
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,���͵�DelAllClearTR_DATA  ʧ��");
		return -1;
	}
		
	trAd = inode *MAX_TR_NB_PER_DISPENSER;//��ʼ��ַ
	for(i=0; i<MAX_TR_NB_PER_DISPENSER; i++){
		pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd+i]) );

		//modify by liys 2008-03-13 ������֧��״̬���������־��
		if(pfp_tr_data->Trans_State == (unsigned char)1) {
			memset(pfp_tr_data, 0, sizeof(FP_TR_DATA) );
		}					
	}

/*	 //modify by liys  2008-03-13  
//���ڵ����н����������������һ������,��ȡ���ݵĽ�������,����еĻ�.
	acnt = GetNodeAllTrCnt( node); 
	if(acnt < MAX_TR_NB_PER_DISPENSER/4-2){
		ret = ReadHalfNodeTR_DATA(node);
	} 
*/
	return 0;
}

//���ĳ��������еļ��ͼ�¼
int DelAllTR_DATA(const char node, const char fp_ad){
	int tr_state, inode, ret;//,noPay,ccnt
	FP_TR_DATA *pfp_tr_data;
	//unsigned char node, fp_ad;
	long i,j, k, trAd;//n, 	
	//int data_lg;
	if(  node == 0  ){
		UserLog("���ͻ��ڵ�ֵ����,��������Del ʧ��");
		return -1;
	}	
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,��������Del ʧ��");
		return -1;
	}
	
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node) { //�ҵ��˼��ͻ��ڵ�
		inode = i;
		 break;
		}
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,���͵�DelAllTR_DATA  ʧ��");
		return -1;
	}
		
	trAd = inode *MAX_TR_NB_PER_DISPENSER;//��ʼ��ַ
	for(i=0; i<MAX_TR_NB_PER_DISPENSER; i++){
		pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd+i]) );		
		memset(pfp_tr_data, 0, sizeof(FP_TR_DATA) );			
	}
		
	return 0;
}

//��õ�һ���հ׵�ַ,���ڴ���½���
static long GetEmptyTrAd(const char node, const char fp_ad){
	long i, trAd;	
	FP_TR_DATA *pfp_tr_data;	
	
	if(  node == 0  ){
		UserLog("���ͻ��ڵ�ֵ����,�������ݵ�ַGet  ʧ��");
		return -1;
	}
	
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,�������ݵ�ַGet  ʧ��");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,���͵�SetSgl  ʧ��");
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
		UserLog("��������������!!  ��ȡ�������ݵ�ַʧ��");
		return -1;
	}
	
	return -1;
}

//������֧��״̬��������� ���ߵ�Ҳ���ϴ��Ŀ��������־
int DelClearTR_DATA(const char node){//delete all clear transaction data, but left only the new one!!
	int tr_state, inode, ret, acnt;//,noPay,ccnt
	FP_TR_DATA *pfp_tr_data, tmp_fp_tr_data[4] = {0};
	//unsigned char node, fp_ad;
	long i,j, k, trAd;//n, 	
	//int data_lg;
	if(  node == 0  ){
		UserLog("���ͻ��ڵ�ֵ����,��������Del ʧ��");
		return -1;
	}	
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,��������Del ʧ��");
		return -1;
	}

	k = 0;
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node) { //�ҵ��˼��ͻ��ڵ�
		inode = i;
		 break;
		}
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,���͵�DelAllClearTR_DATA  ʧ��");
		return -1;
	}		

	trAd = inode *MAX_TR_NB_PER_DISPENSER;//��ʼ��ַ
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

		//modify by liys 2008-03-13 ������֧��״̬���������־��
		//modify by Guojie Zhu @ 2009-05-23 �������߽����ж�, ���߽��ײ��ܱ����
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
	/* modify by liys 2008-03-13 δ֧������ֻ�����ʣ�û��Ҫ���������
	//���ڵ����н����������������һ������,��ȡ���ݵĽ�������,����еĻ�.
	acnt = GetNodeAllTrCnt( node);
	if(acnt < MAX_TR_NB_PER_DISPENSER/4-2){
		ret = ReadHalfNodeTR_DATA(node);
	}
	*/
	return 0;
}

//��һ���ڵ������δ֧�����׼�¼ȫ��д���ļ�
int WriteFileNodeTR_DATA(const unsigned char node){
	int tr_state, inode, ret;//,noPay,ccnt
	FP_TR_DATA *pfp_tr_data;
	long i,  trAd;//n, j,k,
	int fd;//len, 
	char fileName[128] ;
	
	if(  node == 0  ){
		UserLog("���ͻ��ڵ�ֵ����,��������WriteFileAllTR_DATA ʧ��");
		return -1;
	}

	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,��������WriteFileAllTR_DATA ʧ��");
		return -1;
	}
	bzero(fileName,sizeof(fileName));
	sprintf(fileName,"%s%02x",TEMP_TR_FILE, node);	
	fd = open( fileName, O_WRONLY|O_CREAT|O_APPEND, 00644 );
	if( fd < 0 )
	{
		UserLog( "�򿪽���������ʱ����ļ�[%s]ʧ��.",fileName );
		return -2;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node) { //�ҵ��˼��ͻ��ڵ�
		inode = i;
		 break;
		}
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,���͵�WriteFileAllTR_DATA  ʧ��");
		return -1;
	}
		
	trAd = inode *MAX_TR_NB_PER_DISPENSER;//��ʼ��ַ
	
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

//дһ��δ֧�����׵����׻�����
int WriteHalfNodeTR_DATA(const unsigned char node){
	int tr_state, inode, ret,accnt;//,noPay,ccnt
	FP_TR_DATA *pfp_tr_data;
	long trAd;//n, j ,
	int i, k, n;
	int fd;//len, 
	unsigned char tmp50[2];
	char fileName[128] ;
	
	if(  node == 0  ){
		UserLog("���ͻ��ڵ�ֵ����,��������WriteFileAllTR_DATA ʧ��");
		return -1;
	}

	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,��������WriteFileAllTR_DATA ʧ��");
		return -1;
	}
	bzero(fileName,sizeof(fileName));
	sprintf(fileName,"%s%02x",TEMP_TR_FILE, node);	
	fd = open( fileName, O_WRONLY|O_CREAT|O_APPEND, 00644 );
	if( fd < 0 )
	{
		UserLog( "�򿪽���������ʱ����ļ�[%s]ʧ��.",fileName );
		return -2;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node) { //�ҵ��˼��ͻ��ڵ�
		inode = i;
		 break;
		}
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,���͵�WriteFileAllTR_DATA  ʧ��");
		return -1;
	}		
	trAd = inode *MAX_TR_NB_PER_DISPENSER;//��ʼ��ַ
	
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
				if(++k >= MAX_TR_NB_PER_DISPENSER/2){//ֻдһ���
					break;
				}
			}			
		}
		if(k < MAX_TR_NB_PER_DISPENSER/2 ){
			UserLog("���׺���9999ѭ����0001,��˱���ֻ����ɵĽ������ݵ�9999,��%d��",k);
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
				if(++k >= MAX_TR_NB_PER_DISPENSER/2){//ֻдһ���
					break;
				}
			}
		}
	}
	
	close(fd);
	return 0;
}

//���ļ��ж���buff ��һ��δ֧������
int ReadHalfNodeTR_DATA(const unsigned char node){
	int tr_state, inode, ret, acnt;//,noPay,ccnt
	FP_TR_DATA *pfp_tr_data,fp_tr_data;
	long i, j, k,  trAd;//n, j,k,
	int fd,tempfd,okflag = -1;//len, 
	char fileName[128] ;
	char tempFileName[128] ;
	
	if(  node == 0  ){
		UserLog("���ͻ��ڵ�ֵ����,��������ReadHalfNodeTR_DATA ʧ��");
		return -1;
	}

	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,��������ReadHalfNodeTR_DATA ʧ��");
		return -1;
	}
	bzero(fileName,sizeof(fileName));
	sprintf(fileName,"%s%02x",TEMP_TR_FILE, node);	
	//fd = open( fileName, O_WRONLY|O_CREAT|O_APPEND, 00644 );
	fd = open(fileName, O_RDONLY);
	if( fd < 0 ) {
		UserLog( "�򿪽���������ʱ����ļ�[%s]ʧ��.",fileName );
		return -2;
	}
	bzero(tempFileName,sizeof(tempFileName));
	sprintf(tempFileName,"%stemp%02x",TEMP_TR_FILE, node);	
	tempfd = open( fileName, O_WRONLY|O_CREAT|O_APPEND, 00644 );
	if( tempfd< 0 ) {
		UserLog( "�򿪽���������ʱ��ű����ļ�[%s]ʧ��.",tempFileName );
		return -2;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node) { //�ҵ��˼��ͻ��ڵ�
		inode = i;
		 break;
		}
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,���͵�ReadHalfNodeTR_DATA  ʧ��");
		return -1;
	}
	
	trAd = inode *MAX_TR_NB_PER_DISPENSER;//��ʼ��ַ
	
	acnt = GetNodeAllTrCnt(node);
	if(acnt >MAX_TR_NB_PER_DISPENSER/2){
		UserLog("�����ڴ潻������̫��, ReadHalfNodeTR_DATA  ʧ��");
		return -1;
	}
	k=0;
	for(i=0; i<MAX_TR_NB_PER_DISPENSER; i++){			
		ret = read(fd, &fp_tr_data,  sizeof(FP_TR_DATA) );
		if( ret == 0 )	{	//�ļ�����
			okflag = 1;
			break;
		}
		else if(errno == EINTR){
			continue;
		}
		else if(ret != sizeof(FP_TR_DATA)  ){
			UserLog("��ȡ��������ʧ��, ReadHalfNodeTR_DATA  ʧ��");
			okflag = -2;
			break;
		}
		ret = AddTR_DATA(&fp_tr_data);//���߽����Ƿ��ȡ���ַ�ʽ
		if(ret <0){
			UserLog("AddTR_DATAʧ��, ReadHalfNodeTR_DATA  ʧ��");
			okflag = -3;
			break;
		}
		if(++k >= MAX_TR_NB_PER_DISPENSER/2 -1 ){
			okflag = 0;
			break;
		}
		
	}
	if( (k == 0)&&(okflag != 1) ){
		UserLog("���ݵĽ������ݴ������, ReadHalfNodeTR_DATA  ʧ��");
		close(fd);
		close(tempfd);
		return -1;
	}
	else if( (k == 0)&&(okflag == 1) ){
		UserLog("û�б��ݵĽ�������, ReadHalfNodeTR_DATA  �ɹ�");
		close(fd);
		close(tempfd);
		ret = remove(fileName);
		ret = remove(tempFileName);
		return 0;
	}
	else if(okflag != 1){
		while(1){
			ret = read(fd, pfp_tr_data,  sizeof(FP_TR_DATA) );
			if( ret == 0 ){	//�ļ�����
				okflag = 2;
				break;
			}
			else if(errno == EINTR){
				continue;
			}
			else if(ret != sizeof(FP_TR_DATA)  ){
				UserLog("��ȡ��������ʧ��, ReadHalfNodeTR_DATA  ʧ��");
				break;
		       }
			ret = write(fd, pfp_tr_data,  sizeof(FP_TR_DATA) );			
			if(ret != sizeof(FP_TR_DATA)  ){
				UserLog("���������������, ReadHalfNodeTR_DATA  ʧ��");
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
				RunLog("����!!���������������, ReadFileNodeTR_DATA  ʧ��,�������ݿ��ܶ�ʧ!!");
				return -4;
			}
		}
		else{
			UserLog("���������������, ReadFileNodeTR_DATA  ʧ��");
			return -3;
		}
	}
	if(okflag == 1){
		ret = remove(fileName);
		if( ret < 0){
			RunLog("���������������, ���ױ������ݿ��ܴ�������,ReadFileNodeTR_DATA  ʧ��");
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
		UserLog("���ͻ��ڵ�ֵ����,��������ReadAllNodeTR_DATA ʧ��");
		return -1;
	}
	if(  fd <= 0  ){
		UserLog("���ͻ��ڵ�ֵ����,��������ReadAllNodeTR_DATA ʧ��");
		return -1;
	}	
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,��������ReadAllNodeTR_DATA ʧ��");
		return -1;
	}
	bzero(fileName,sizeof(fileName));
	sprintf(fileName,"%s%02x",TEMP_TR_FILE, node);	
	//fd = open( fileName, O_WRONLY|O_CREAT|O_APPEND, 00644 );
	fd = open( fileName, O_RDONLY);
	if( fd < 0 )
	{
		UserLog( "�򿪽���������ʱ����ļ�[%s]ʧ��.",fileName );
		return -2;
	}
	bzero(tempFileName,sizeof(tempFileName));
	sprintf(tempFileName,"%stemp%02x",TEMP_TR_FILE, node);	
	tempfd = open( fileName, O_WRONLY|O_CREAT|O_APPEND, 00644 );
	if( tempfd< 0 )
	{
		UserLog( "�򿪽���������ʱ��ű����ļ�[%s]ʧ��.",tempFileName );
		return -2;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node) { //�ҵ��˼��ͻ��ڵ�
		inode = i;
		 break;
		}
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,���͵�ReadAllNodeTR_DATA  ʧ��");
		return -1;
	}
	
	trAd = inode *MAX_TR_NB_PER_DISPENSER;//��ʼ��ַ
	
	acnt = GetNodeAllTrCnt(node);
	if(acnt >MAX_TR_NB_PER_DISPENSER/3){
		UserLog("�����ڴ潻������̫��, ReadAllNodeTR_DATA  ʧ��");
		return -1;
	}
	k=0;
	for(i=0; i<MAX_TR_NB_PER_DISPENSER; i++){			
		ret = read(fd, &fp_tr_data,  sizeof(FP_TR_DATA) );
		if( ret == 0 )	{	//�ļ�����
			okflag = 1;
			break;
		}
		else if(errno == EINTR){
			continue;
		}
		else if(ret != sizeof(FP_TR_DATA)  ){
			UserLog("��ȡ��������ʧ��, ReadAllNodeTR_DATA  ʧ��");
			okflag = -2;
			break;
		}
		ret = AddTR_DATA(&fp_tr_data);//���߽����Ƿ�������ַ�ʽ
		if(ret <0){
			UserLog("AddTR_DATAʧ��, ReadAllNodeTR_DATA  ʧ��");
			okflag = -3;
			break;
		}
		if(++k >= MAX_TR_NB_PER_DISPENSER*7/8 -1 ){
			okflag = 0;
			break;
		}
		
	}
	if( (k == 0)&&(okflag != 1) ){
		UserLog("���ݵĽ������ݴ������, ReadAllNodeTR_DATA  ʧ��");
		close(fd);
		close(tempfd);
		return -1;
	}
	else if( (k == 0)&&(okflag == 1) ){
		UserLog("û�б��ݵĽ�������, ReadAllNodeTR_DATA  �ɹ�");
		close(fd);
		close(tempfd);
		ret = unlink(fileName);
		ret = unlink(tempFileName);
		return 0;
	}
	else if(okflag != 1){
		while(1){
			ret = read(fd, pfp_tr_data,  sizeof(FP_TR_DATA) );
			if( ret == 0 ){	//�ļ�����
				okflag = 2;
				break;
			}
			else if(errno == EINTR){
				continue;
			}
			else if(ret != sizeof(FP_TR_DATA)  ){
				UserLog("��ȡ��������ʧ��, ReadAllNodeTR_DATA  ʧ��");
				break;
		       }
			ret = write(fd, pfp_tr_data,  sizeof(FP_TR_DATA) );			
			if(ret != sizeof(FP_TR_DATA)  ){
				UserLog("���������������, ReadAllNodeTR_DATA  ʧ��");
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
				RunLog("����!!���������������, ReadAllNodeTR_DATA  ʧ��,�������ݿ��ܶ�ʧ!!");
				return -4;
			}
		}
		else{
			UserLog("���������������, ReadAllNodeTR_DATA  ʧ��");
			return -3;
		}
	}
	if(okflag == 1){
		ret = unlink(fileName);
		if( ret < 0){
			RunLog("���������������, ���ױ������ݿ��ܴ�������,ReadAllNodeTR_DATA  ʧ��");
			return -3;
		}
	}
	return 0;
}

//��ȡ��һ����������˳���.�ɹ�����TR_Seq_Nb.ʧ�ܷ���-1.
int GetNextTrNb(const char node, const char fp_ad, unsigned char *tr_seq_nb){
	long i, j, k, trAd;
	int ret;
	unsigned char tmp_tr_nb[2];
	FP_TR_DATA *pfp_tr_data;
	unsigned char zero2[2];
	
	if( NULL == tr_seq_nb ){
		UserLog("�������ݴ���,�������ݵ�ַGet ʧ��");
		return -1;
	}
	if(  node == 0  ){
		UserLog("���ͻ��ڵ�ֵ����,�������ݵ�ַGet  ʧ��");
		return -1;
	}	
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,�������ݵ�ַGet  ʧ��");
		return -1;
	}
	
	
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,���͵�GetNextTrNb  ʧ��");
		return -1;
	}
	bzero(zero2,sizeof(zero2));
	//�����buff ����
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
		UserLog("���ͻ��ڵ�ֵ����,��������MakeTrMsg ʧ��");
		return -1;
	}		
	if( (NULL == pfp_tr_data) || (NULL == sendBuffer) || (NULL == data_lg) ){
		UserLog("����Ϊ�մ���,��������MakeTrMsg ʧ��");
		return -1;
		}
	/*
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,��������MakeTrMsg ʧ��");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,��������MakeTrMsg ʧ��");
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
//�ı佻��״̬TR_State. ע�⽻�������״̬ʱҪ��鱣��Ĵ������״̬�Ľ�����,
//����fp��Nb_Of_Historic_Trans�Ƚ�,���ڵĻ���ɾ�������Ĵ������״̬�Ľ���
static int ChangeTR_State(const char node, const char fp_ad, FP_TR_DATA *pfp_tr_data, const char status)
{
	int i,k,ret, tr_state,tmp_lg=0; //recv_lg;		
	unsigned char buffer[64];
	memset(buffer, 0, 64);
	if(node == 0){
		UserLog("���ͻ��ڵ�ֵ����,��������ChangeTR_State ʧ��");
		return -1;
	}		
	if(NULL == pfp_tr_data){
		UserLog("����Ϊ�մ���,��������ChangeTR_State ʧ��");
		return -1;
	}	
	pfp_tr_data->Trans_State = status;
	
	//modify by liys 2008-03-13
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,���͵�End Trans   ʧ��");
		return -1;
	}

	ret = MakeTrMsg(node, fp_ad, pfp_tr_data, buffer, &tmp_lg);
	if (ret < 0) {
		UserLog("�����������ݴ���,��������ChangeTR_State ʧ��");
		return -1;
	}
	ret = SendData2CD(buffer, tmp_lg, 5);
	if (ret < 0) {
		UserLog("ͨѶ����,��������ChangeTR_State ʧ��");
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
		UserLog("�����ڴ�ֵ����,��������CalcTrCheksum  ʧ��");
		return -1;
	}
	if( NULL ==  fp_tr_data ){
		UserLog("������ָ�����,��������CalcTrCheksum ʧ��");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( fp_tr_data->node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,��������CalcTrCheksum ʧ��");
		return -1;
	}
	if(  (0 == gpIfsf_shm->dispenser_data[i].fp_c_data.W_M_Polynomial[0])  \
		&&(0 == gpIfsf_shm->dispenser_data[i].fp_c_data.W_M_Polynomial[1])  ){		
		UserLog("У��Ͷ���ʽֵΪ0����,��������CalcTrCheksum  ʧ��");
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
		UserLog("�������кŴ���");
		return -1;
	}
	if(  node == 0  ){
		UserLog("���ͻ��ڵ�ֵ����");
		return -1;
	}
	
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ָ��Ϊ��");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //�ҵ��˼��ͻ��ڵ�
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���");
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
		UserLog("�����������Ҳ����ý���,�������ݵ�ַ��ȡʧ��!");
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
		UserLog("���ͻ��ڵ�ֵ����,�������ݵ�ַ��ȡʧ��");
		return -1;
	}	
	ret = ReadAllNodeTR_DATA(node);
	
	return 0;
}

//FP_TR_DATA��������. node���ͻ��߼��ڵ��,fp_ad���͵��ַ21h-24h,Ҫ�õ�gpIfsf_shm,ע��P,V����������.
//���Ӻ����һ�ʽ��׼�¼.�����¼��¼ע��Ҫ�ж�TR_State�Ƿ�Ϊ���״̬.
// Return: -1/-2/1 ִ��ʧ��, ����1����������ִ��AddTR_DATA, -1/-2��������; 0 - �ɹ�
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
		UserLog("�������ݴ���,��������Add ʧ��");
		return -1;
	}
	node = fp_tr_data->node;
	fp_ad = fp_tr_data->fp_ad ;
	if(  node <= 0  ){
		UserLog("���ͻ��ڵ�ֵ����,��������Add ʧ��");
		return -1;
	}

	//���ĳ���ڵ������ �����߽�������
	acnt = GetNodeAllOnLineTrCnt(node);
	if(acnt < 0){
		return -2;
	}

	//������ͼ�¼�����ڵ���64 + 1�������buff �е���֧����¼
	//�������������ϴ��Ľ��׼�¼
	if(acnt >= (MAX_TR_NB_PER_DISPENSER/4+1) ){
		ret = DelClearTR_DATA(node);
		acnt = GetNodeAllTrCnt(node);
		if(acnt < 0){
			return -2;
		}
	}	

/*	//������ͼ�¼�����ڵ���225����дһ��δ֧���������ݵ��ļ���
	//����Ŀǰֻ��������δ֧�����ף����Ըú�������ִ��
	if(acnt >= (MAX_TR_NB_PER_DISPENSER*7/8+1) ){
		ret = WriteHalfNodeTR_DATA(node);
	}	
*/  

	//������ڴ���½��׵Ŀհ׵�ַ
	k = GetEmptyTrAd(node, fp_ad);
	if (k >= 0)
		pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[k]) );
	else {
		UserLog("���ҿ��н���������ʧ��,��������Add ʧ��");
		return -1;
	}
	
	memcpy(pfp_tr_data, fp_tr_data, sizeof(FP_TR_DATA) );

       //����ѻ����׵�����,�������200 ��ֱ��д���ļ�  add by liys 2008-05-13
       //��GetEmptyTrAD֮ǰŲ������, ���Ƿ�ӳ���߽������  jie @ 2009-05-23
       //����ѻ����׵�����,�������100 ��ֱ��д���ļ�  add by lil 2008-05-13
	acnt = GetAllOfflineCnt(node);
	RunLog("Ŀǰ�ڴ���Node[%d]���ѻ����׼�¼Ϊ%d ��", node, acnt);
	if (acnt > 100) {
		ret = WriteBuffDataToFile(node);
       } 			
#if 0
	ret= CalcTrCheksum(pfp_tr_data);
	if(ret <0 ) {
		RunLog("AddTR_DATA������,���㽻������У���ʧ��!");
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

	//��������λ�����͵����ݰ�
	// ��������ѻ�����, ��ֱ�ӷ��ز��ϴ�������Ϣ.(�ָ���������Ȼ�ڼӵ����߽���) 
	if ((gpIfsf_shm->fp_offline_flag[node - 1][fp_ad - 0x21] & 0x0F) == 0x0F) {
		return 0;
	}

	ret= MakeTrMsg(node, fp_ad, pfp_tr_data, buffer, &data_lg);
	if(ret <0 ) {
		UserLog("���콻����������ʧ��!");
		return 1;
	}

	//����λ����������

	for (i = 0; i < MAX_CHANNEL_CNT; i++) {
		if (gpIfsf_shm->node_online[i] == node) {
			ret = SendData2CD(buffer, data_lg, 5);
			if(ret <0 ) {
				UserLog("���ͽ�����������ʧ��!");
				return 1;
			}
			break;
		} // else { Dispenser �������򲻷���,�����ѻ����� }
	}

	return 0;
}

//ɾ����֧����������
int DelTR_DATA(const char node, const char fp_ad, const unsigned char *tr_seq_nb)
{
	int i, tr_state, ret;
	FP_TR_DATA *pfp_tr_data;
	long k = 0;;
	unsigned char buffer[64];
	memset(buffer, 0, sizeof(buffer));
	if( NULL == tr_seq_nb ){
		UserLog("�������ݴ���,��������Del ʧ��");
		return -1;
		}	
	if(  node == 0 ){
		UserLog("���ͻ��ڵ�ֵ����,��������Del ʧ��");
		return -1;
		}
	
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,��������Del ʧ��");
		return -1;
	}
	
	k = GetTrAd(node, fp_ad,tr_seq_nb);
	if (k >= 0)
		pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[k]) );
	else {
		UserLog("���ҽ���������ʧ��,��������Del ʧ��");
		return -1;
	}
	tr_state = pfp_tr_data->Trans_State;
	if ( tr_state == (unsigned char)1 )
		memset(pfp_tr_data, 0, sizeof(FP_TR_DATA) );
	else {
		UserLog("�ý������ݲ�����clear ״̬,��������Del ʧ��");
		return -1;
	}	
	
	return 0;
}



//��ȡFP_TR_DATA. �������Ϣ���ݵ�����Ϊ001�ش�.
//recvBufferΪ�յ���data,sendBufferΪҪ���͵�data,msg_lgΪ���ɵ��ܳ���.
//���ݼ��ͻ����͵�seq_nb �õ�����Ľ��׼�¼
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
		UserLog("�������ݴ���,��������Read ʧ��");
		return -1;
	}
	node = recvBuffer[1];
	fp_ad = recvBuffer[9];	
	if( node <= 0 ){
		UserLog("���ͻ��ڵ�ֵ����,��������Read ʧ��");
		return -1;
		}
	
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,��������Read  ʧ��");
		return -1;
	}
	if ((unsigned char)4 !=recvBuffer[8]){
		UserLog("�������ݴ���,��������Read ʧ��");
		return -1;
	}
	tr_seq_nb = (unsigned char *)(&recvBuffer[11]);
	k = GetTrAd(node, fp_ad,tr_seq_nb);
	if (k >= 0)
		pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[k]) );
	else {
		UserLog("���ҽ���������ʧ��,��������Read ʧ��");
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

//дFP_LN_DATA����.recvBufferΪ�յ���data,sendBufferΪҪ�ظ���data,�������Ϣ���ݵ�����Ϊ111Ӧ��.
//msg_lgΪ���ɵ��ܳ���,Ϊ0��ʾ����ҪӦ����Ϣ,sendBuffer��msg_lg����ҪӦ����Ϣ��������ΪNULL.
int WriteTR_DATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg){
	int i,j, tr_state, recv_lg, tmp_lg,lg, ret, count;
	//DISPENSER_DATA *dispenser_data;
	FP_TR_DATA *pfp_tr_data;
	unsigned char node, fp_ad, *tr_seq_nb;
	unsigned char tmpMSAck, tmpDataAck;
	unsigned char buffer[64];
	long k = 0;
	unsigned char TR_Operation = 0x00;  // ��¼TR��������
	unsigned char err_op = 0x00;        // ��¼�������,eg. û�иý���

	memset(buffer, 0, sizeof(buffer));
	if(NULL == recvBuffer ){
		UserLog("�������ݴ���,��������Write ʧ��");
		return -1;
	}
	node = recvBuffer[1];
	fp_ad = recvBuffer[9];	
	if( (node <= 0) || (node > 127 ) ){
		UserLog("���ͻ��ڵ�ֵ����,��������Write ʧ��");
		return -1;
	}
	
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,��������Write  ʧ��");
		return -1;
	}
	// ���ȴ���
	if ((unsigned char)4 != recvBuffer[8]){
		UserLog("�������ݴ���,��������Write ʧ��");
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
		UserLog("���ҽ���������ʧ��,��������Write ʧ��");
		goto ADDR_NAK;
	}

	if(pfp_tr_data->Trans_State  > (unsigned char)3){
		UserLog("�������������ݴ���,��������Write ʧ��");
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
				UserLog("���ͽ���״̬��������ʧ��");
			}
			RunLog("Clear Tr �ɹ�...............");
			return 1;   // �ϲ㲻�ٷ���ACK����
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
��ȡ�ѻ��ļ��е����һ�ʽ��׼�¼
modify by liys 2008-03-18
for example:

CD read all of one node:
0101 0201 00 00 000D 04 20 21 0000 C8 05 06 07 08 0A CA CB

Answer one by one
0201 0101 00 20 xxxx 04 21 21 xxxx C800 +need data(data_id + data_lg + data_el)

*/
/*
 * return: 0 - ���ѻ�����; 1 - ���ѻ�����; -1 - ����
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
		UserLog("�����ڴ�ֵ����,��������Read  ʧ��");
		return -1;
	}

	if ((NULL == recvBuffer) ||  (NULL == sendBuffer) ){
		UserLog("�������ݴ���,��������Read ʧ��");
		return -1;
	}
	node = recvBuffer[1];

	if (node <= 0 ){
		UserLog("���ͻ��ڵ�ֵ����,��������Read ʧ��");
		return -1;
	}
	
	if (0x04 != recvBuffer[8]){
		UserLog("�������ݴ���,��������Read ʧ��");
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

	RunLog("�ѻ��ļ���С: %d", flength);		

	if (flength < sizeof(FP_TR_DATA)) { //���ѻ���¼
		buffer[0] = recvBuffer[2];  
		buffer[1] = recvBuffer[3];

		//���ͷ�   
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

		return 1;      // �ϲ�ݴ��ж��Ƿ���Ҫ���ʹ˻ظ�����
	}
			  
	lseek(fd, 0, SEEK_SET);

	mapped_mem = mmap(NULL, flength, PROT_READ, MAP_SHARED, fd, 0);
	memcpy(trbuff,mapped_mem,sizeof(FP_TR_DATA));              //FIFO	

	close(fd);	       
	munmap(mapped_mem, flength);
	
	pfp_tr_data = (FP_TR_DATA*)(trbuff);

	//���շ�
	buffer[0] = recvBuffer[2];  
	buffer[1] = recvBuffer[3];

	//���ͷ�   
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
				
			case TR_Amount : //05 �ѻ��ֶ�
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
			case TR_Volume :  //06 �ѻ��ֶ�
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
			case TR_Unit_Price : //07�ѻ��ֶ�
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
			case TR_Log_Noz: //08 �ѻ��ֶ�
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
			case TR_Prod_Nb : //0A�ѻ��ֶ�
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
			case TR_RD_OFFLINE: //C8  ���ѻ�����
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)TR_RD_OFFLINE;
				tmp_lg++;
				buffer[tmp_lg] = '\x00';
				break;
			case TR_Date:  //CA �ѻ��ֶ�
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
			case TR_Time:  //CB �ѻ��ֶ�
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
ɾ���ѻ��ļ��е����һ�ʽ��׼�¼
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
		UserLog("�������ݴ���,��������Read ʧ��");
		return -1;
	}
	node = recvBuffer[1];

	if( node <= 0 ){
		UserLog("���ͻ��ڵ�ֵ����,��������Read ʧ��");
		return -1;
	}
	
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,��������Read  ʧ��");
		return -1;
	}
	if ((unsigned char)4 !=recvBuffer[8]){
		UserLog("�������ݴ���,��������Read ʧ��");
		return -1;
	}


	sprintf( lsTrName, "%s%02x", TEMP_TR_FILE,node);   // dsptrxx

	m = GetFileSizeByFullName(lsTrName);	

	if( m == -1 ) {
		UserLog( "ȡ�ѻ������ļ� ʧ��." );
		return -1;
	} else if(m == -2) {	//-2�ļ�������
		UserLog( "�ѻ����� �ļ�������" );
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
//		RunLog(">>>>>>>> �����Ӧ���ѻ����׼�¼");
		memset(mapped_mem,0,sizeof(FP_TR_DATA));   //ɾ���ļ��ĵ�һ����¼ FIFO
		memcpy(mapped_mem,mapped_mem + sizeof(FP_TR_DATA) ,flength - sizeof(FP_TR_DATA));
			 
//		RunLog(">>>>>>>> �ļ�ͬ��<<<<<<<<");
		msync(mapped_mem, flength,MS_SYNC);	

		// �ı��ļ���С
		if (ftruncate(fd, m - sizeof(FP_TR_DATA)) == -1) { //ͬʱ�ض��ļ�
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


//int PackTR_DATA(){ }//��д,����


//����״̬ʱ:��buff�е�����δ֧����¼״̬��Ϊ������֧��״̬ 
//modify by liys 2008-03-14
int OffLineSetBuff( const unsigned char node)
{
	int inode,i;
	long trAd;
	FP_TR_DATA *pfp_tr_data;

	if(  node == 0  ) {
		UserLog("���ͻ��ڵ�ֵ����,��������OffLineStatus ʧ��");
		return -1;
	}

	if( NULL == gpIfsf_shm ) {		
		UserLog("�����ڴ�ֵ����,��������OffLineStatus ʧ��");
		return -1;
	}

	for(i=0; i < MAX_CHANNEL_CNT; i++) {
		if( node == gpIfsf_shm->convert_hb[i].lnao.node) { //�ҵ��˼��ͻ��ڵ�
		  inode = i;
		  break;
		}
	}
	if(i == MAX_CHANNEL_CNT ) {
		UserLog("���ͻ��ڵ������Ҳ���,���͵�OffLineStatus  ʧ��");
		return -1;
	}

	trAd = inode *MAX_TR_NB_PER_DISPENSER;//��ʼ��ַ
	
	for(i=0; i<MAX_TR_NB_PER_DISPENSER; i++) {
		pfp_tr_data =  (FP_TR_DATA *)( &(gpIfsf_shm->fp_tr_data[trAd+i]) );				
		if( (pfp_tr_data->Trans_State == PAYABLE)  ||
			(pfp_tr_data->Trans_State == LOCKED)) { //���buff�еļ�¼Ϊδ֧��״̬,��offline��Ϊ ������֧��
			pfp_tr_data->offline_flag = OFFLINE_TR; 
			pfp_tr_data->Trans_State = CLEARED;
			SET_FP_OFFLINE_TR(pfp_tr_data->node, (pfp_tr_data->fp_ad - 0x21));
		} 
	}	

	return 0;
}


//���߱�Ϊ����ʱ:��buff�е����е����߼�¼д���ļ�
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
		UserLog("���ͻ��ڵ�ֵ����,��������WriteBuffDataToFile ʧ��");
		return -1;
	}

	if(NULL == gpIfsf_shm){
		UserLog("�����ڴ�ֵ����,��������WriteBuffDataToFile ʧ��");
		return -1;
	}

	bzero(fileName, sizeof(fileName));
	sprintf(fileName,"%s%02x",TEMP_TR_FILE, node);	
	fd = open(fileName, O_RDWR|O_CREAT|O_APPEND, 00644);
	if (fd < 0) {
		UserLog( "�򿪽���������ʱ����ļ�[%s]ʧ��.",fileName );
		return -2;
	}

	RunLog("Open dsptr%02x successful!", node);
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node) { //�ҵ��˼��ͻ��ڵ�
			inode = i;
			break;
		}
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ������Ҳ���,���͵�WriteFileAllTR_DATA  ʧ��");
		return -1;
	}
		
	trAd = inode * MAX_TR_NB_PER_DISPENSER;  //��ʼ��ַ
	
//	д���ļ�ǰ�������߽��׽�����������ΪTR_Date  TR_Time 
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
							flag = 1;  //������ҵ�����С����ֵ��ѭ���������ͷ�ٴ�ѭ��
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
 * func: ����������ѻ����״���ָ�ִ��, ���ͻ�ģ�����
 * ret : 0 - �ɹ�; -1 - ʧ��; 1 - recvBuffer Ϊ NULL
 */

int upload_offline_tr(const unsigned char node, const unsigned char *recvBuffer) 
{
	int i, ret, m_st, msg_lg;
	unsigned char buffer[256] = {0};
	static unsigned char ReadTRCMD[32] = {0};  // �����ѻ����׶�ȡ����
	static int cmdlen = 0;

	// upload progress == 0 : ��û�ж�ȡ����; �������Ѿ�����, ��������û���ѻ����ף� Ҳû��δ��ɵ��ѻ�����
	// upload progress == 1 : ��ȡ��...
	// upload progress == 2 : �Ѿ��·���ȡ����, ��������û���ѻ�����, ������δ��ɵ��ѻ�����
	// upload progress == 3 : ����δ����ѻ����׾������, �ȴ�����
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
			// ������������ѻ�����, ��д���ļ�
			if (has_offline_tr_in_buf(node)) {
//				RunLog("%s: node %d case %d, ���������ѻ�����", __FUNCTION__, node, upload_progress);
				ret = WriteBuffDataToFile(node);
				if (ret == 0) {
					clr_offline_flag(node);
				} else {
					RunLog("�ڵ� %d д�ѻ����׵��ļ�ʧ��", node);
					return -1;
				}
			}
			break;
		case 2:
			// �������FP�������δ��ɵ��ѻ�����, �������ϴ��ѻ�����, ����ֱ�ӷ���
			// ����δ����ѻ�����һ���ϴ�
			if (has_incomplete_offline_tr(node)) {
				RunLog("%s: case 2, node %d ��δ��ɵ��ѻ�����", __FUNCTION__, node);
				return 0;
			}

			// ��������������ѻ�����(δ��ɵ������), ��д���ļ�; 
			if (has_offline_tr_in_buf(node)) {
				RunLog("%s: node %d case 2, ���������ѻ�����, ��δ����ѻ�����",__FUNCTION__, node);
				// ������������ѻ�����, ��д���ļ�
				ret = WriteBuffDataToFile(node);
				if (ret == 0) {
					clr_offline_flag(node);
					upload_progress = 3;
//	RunLog("%s: case 2, Node %d д���������׵��ļ��ɹ�, upload_progress: %d",__FUNCTION__, node, upload_progress);
				} else {
					RunLog("�ڵ� %d д�ѻ����׵��ļ�ʧ��", node);
					return -1;
				}
			}

			break;
		default:
			break;
	}
	
	// additional handle, unsolicited
	if (upload_progress == 3) {
//		RunLog("node %d �����ϴ��ѻ�����", node);
		ReadTRCMD[1] = node;
		ReadTRCMD[5] = 0x00;           // token is zero
		ret = ReadTR_FILEDATA(ReadTRCMD, buffer, &msg_lg);
		if(ret == 0) {
			ret = SendData2CD(buffer, msg_lg, 5);
			if (ret <0) {		
				UserLog("���ͻظ����ݴ���,Translate ʧ��");
				return -1;
			}
			upload_progress = 0;
		}

		return 0;
	}

	// normal handle -- DoPolling ����ʱ, �����ϴ����׻��߻ָ���Ϣ
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
			if (ret == 1) {  // ���ѻ�����, �Ƿ���δ��ɵ���?
				if (has_incomplete_offline_tr(node)) {
					RunLog("after ReadTR_FILE, node %d ��δ����ѻ����� ", node);
					upload_progress = 2;
				} else {
					upload_progress = 0;
				}
			}

			if (ret >= 0) {  // �����ѻ����׶�ACK
				// ���������, ��ָ� device busy
				if (upload_progress == 2) {
					buffer[13] = 0x07;    // busy
					RunLog("---------------- device busy --------------");
				}

				ret = SendData2CD(buffer, msg_lg, 5);
				if (ret < 0) {		
					RunLog("���ͻظ����ݴ���,Translate ʧ��");
					return -1;
				}
//				RunLog("���ͻظ����� %s �ɹ�", Asc2Hex(buffer, msg_lg));
			}

			break;
		case 1:
			break;
		case 2:
			ret = DeleteTR_FILEDATA(recvBuffer, buffer, &msg_lg);
			if (ret == 0) {
				ret = SendData2CD(buffer, msg_lg, 5);
				if (ret < 0) {		
					UserLog("���ͻظ����ݴ���,Translate ʧ��");
					return -1;
				}
			}
			//����޸������ϴ�ģʽ,��Ϊ���ѻ����׺������ϴ�
			
			if (ReadTRCMD[0] == 0) {
				break;
			}

			ReadTRCMD[5] = (ReadTRCMD[5] & 0xe0) | (recvBuffer[5] & 0x1f);  // get token
			ret = ReadTR_FILEDATA(ReadTRCMD, buffer, &msg_lg);
			if (ret == 1) {
				if (has_incomplete_offline_tr(node)) {
					RunLog("after Delete_FILE& ReadTR_FILE, node %d ��δ����ѻ����� ", node);
					upload_progress = 2;
				} else {
					upload_progress = 0;
				}
			} else if(ret == 0) {  // �����ѻ�����������ACK, ����ѻ����״������ٷ���ACK
				ret = SendData2CD(buffer, msg_lg, 5);
				if (ret <0) {		
					UserLog("���ͻظ����ݴ���,Translate ʧ��");
					return -1;
				}
			}
			
			break;
		case 3:
		case 4:
		case 7:
			break;
		default :
			UserLog("�յ������ݵ���Ϣ���ʹ���!!");
			return -1;
	}
	
	return 0;
}

/*
 * func: �жϻ��������Ƿ��иýڵ���ѻ�����
 * ret : 1 - ���ѻ�����; 0 - û��
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
 * func: ����ýڵ�Ļ����������ѻ����ױ�־
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
 * func:�ýڵ���δ��ɵ��ѻ�����
 * ret : 1 - ��δ����ѻ�����; 0 - û��
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








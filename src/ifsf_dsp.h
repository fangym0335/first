/*******************************************************************
1.ifsf_fpXXX.h�ļ�ö������FP_AD,FP_XXX_AD������IFSF�Ļ���ַ,
�ṹ��FP_XXX_DATA��ö������FP_XXX_DAT_AD������IFSF���������ݼ����ַ.
2.DISPENSER_DATA��һ�����ͻ��ĳ����������������������.
3.IFSF_SHM�����м��ͻ��Ĺ����ڴ��,��ifsf_pub.h�ж���,���������д�
Э��ת���ļ��ͻ������ݺ�����������,����ʱע���ʵ�ʹ��
P,V����,��ֹ����.
4.����PCD�����ļ�(pcd.cfg,��ҪΪʹ�õļ��ͻ���node�ͼ��ͻ����͵�),
��ʼ����Э��ת���ļ��ͻ���IFSF_SHM  ��convertUsed��dispenser_dataֵ,���ø�
���͵�Ϊ����״̬,����������ֵ,��CD����������������.�ı����㽻����,
��ȡ���籣������д��NAND FLASH�Ľ������ݵ�������,Ȼ�����NAND FLASH
�Ľ�������,  ÿ��δ������׷���������Ϣ��CD.���ø����͵�ΪClose״̬,
����Ϊ��������״̬(1),�ȴ�CD���ü���ģʽ,  ������Ʒ,������Ʒ��ǹ
��ϵ,�·��ͼ�,���·���������.
-------------------------------------------------------------------
2007-07-13 by chenfm
*******************************************************************/
#ifndef __IFSF_FP_H__
#define __IFSF_FP_H__

#include "ifsf_def.h"
#include "ifsf_dspad.h"
#include "ifsf_dspcal.h"
#include "ifsf_dspmeter.h"
#include "ifsf_dsptr.h"
#include "ifsf_dspfp.h"
#include "ifsf_dsplnz.h"
#include "ifsf_dsper.h"
#include "ifsf_dspprod.h"

//#include "ifsf_pub.h"  //����.c�ļ���.
//#include "ifsf_main.h"  //����.c�ļ���.

#define MAX_FP_CNT     4     //ÿ�����ͻ��ļ��͵��������ֵ.IFSFΪ4.
#define MAX_FP_LN_CNT  4     //ÿ�����͵���߼���ǹ�������ֵ.IFSFΪ8.
#define MAX_FP_ER_CNT  64   //ÿ�����͵�ĳ��������������ֵ.IFSFĿǰΪ64.
#define FP_PR_CNT      8     //ÿ�����ͻ�����ʹ�õ���Ʒ��.IFSFΪ8.
#define FP_PR_NB_CNT   8   //һ�����ͻ����õ�������Ʒ����,ֻ��������ʹ�õ���Ʒ����.IFSFΪ99999999.
#define FP_FM_CNT      8     //����ģʽ��,���Ϊ8,��IFSFΪ8.
#define FP_M_DAT_CNT   8     //ÿ�����ͻ��ļ�������.IFSFΪ16.
//add your define here .....

//All key indexes are defined in ifsf_def.h


//��Э��ת����һ�����ͻ��Ľṹ��,����������.
typedef struct{	
	FP_C_DATA fp_c_data;
	FP_DATA fp_data[MAX_FP_CNT];
	FP_LN_DATA fp_ln_data[MAX_FP_CNT][MAX_FP_LN_CNT];
	FP_ER_DATA fp_er_data[MAX_FP_CNT];	//ֻ������һ��error. [MAX_FP_ER_CNT];	
	FP_PR_DATA fp_pr_data[FP_PR_CNT];       // ��PR_Id & 0x0FΪ����
	FP_FM_DATA fp_fm_data[FP_PR_NB_CNT][FP_FM_CNT]; // ��PR_Id & 0x0FΪ����
	FP_M_DATA fp_m_data[FP_M_DAT_CNT];
} DISPENSER_DATA;

int DefaultDispenser(const char node);

//Ҫʹ��gpIfsf_shm����Ӧ�ķ������ؼ�������ֵindex,ע��P,V����������.
//���Ӻ�ɾ��һ�����ͻ�.ע�����Ӻ�ɾ��������.
int AddDispenser(const char node); //����DefaultDispenser����.
int DelDispenser(const char node);
int SetDispenser(const char node, const DISPENSER_DATA *dispenser_data);
//ͨ���������ú���,����lischn00x,
//data��ʽ:�ܳ���(1)+��ַ����(1)+��ַ(1-6)+���ݵ�ַ(1)+���ݳ���(1)+����(1-n)
int SetSglData(const char node, const char *single_data);
//int InitIfsfFpShm(); 


/*TcpRecvServ����:
  ����(��ִ��,���Ҫִ�еĻ�)�յ��ļ��ͻ���Ϣ����,�ɹ�����0,ʧ�ܷ���-1
  */
int TranslateRecv(const int newsock, const unsigned char *recvBuffer);
void TwoDecPoint(unsigned char *buffer, int len);

#endif

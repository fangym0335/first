/*******************************************************************
�������Э��ת������oil_main�Ļ����Ͻ��е�,
�������ͨ���궨��IFSF��oil_main����Ӧ����������:
#ifdef IFSF
  IFSF����
#endif
����Э��ת��.
��ͷ�ļ�����IFSF��ʶ,�����ڴ���ź����Ĺؼ��ֻ�����ֵ,IFSF�豸�߼���ַ
-------------------------------------------------------------------
2007-07-06 by chenfm
*******************************************************************/
#ifndef __IFSF_DEF_H__
#define __IFSF_DEF_H__

#ifndef IFSF
#define IFSF
#endif

//һ��IFSFЭ��ת���豸����ͬʱҪת��Э��ķ�IFSF�豸�������
//#define MAX_CHANNEL_CNT 64  //define in pub.h �ڱ���Ŀ�У�Ϊǰͥ���������ӵĴ�Э��ת���ļ��ͻ���������

//�����ڴ���ź����Ĺؼ��ֵ�����ֵ,��InitIfsfShm�����д���,ע��Ψһ��,P,V����������.
#define IFSF_SHM_INDEX 1700    //�����������������ڴ�IFSF_SHM�Ĺؼ��ֵ�����ֵ 
#define IFSF_SEM_INDEX 1705

//����3���ǲ������ʹ����ڴ���IFSF_HB_SHM�ļ����������ֵĹؼ��ֵ�����ֵ,ע��Ψһ��,������. 
#define HEARTBEAT_SEM_INDEX 1710    //����������������������IFSF_HB_SHM������ֵ
#define HEARTBEAT_SEM_RECV_INDEX 1711 //�����������������������Ĺؼ��ֵ�����ֵ
#define HEARTBEAT_SEM_CONVERT_INDEX 1712 //�����������������������Ĺؼ��ֵ�����ֵ

//�������ʹ����ڴ��е�������Э��ת���ļ��ͻ���IFSF_DSP_SHM�Ĺؼ��ֵ�����ֵ
#define IFSF_DP_SEM_INDEX 1720
/*#define...
  ÿ�����ͻ����������ķ��ʵĹؼ��ֵ�����ֵΪIFSF_DSP_SEM_INDEX+node,������MAX_CHANNEL_CNT��,
  ע��atexit()ʱ����. 
 */
 
#define IFSF_DP_TR_SEM_INDEX 1850 //������Э��ת���ļ��ͻ��Ľ����������ķ���.
//other key index define here...  and you must add remove key index in init.c.
#define MAX_TCP_LEN 1024
/*-------------------------------------------------------------------*/
//IFSF�õ��ĵĻ������ݽṹ
//IFSF�豸�߼���ַ
typedef struct{
	unsigned char subnet; 
	unsigned char node;	
//} IFSF_LNA;
} __attribute__((__packed__)) IFSF_LNA;             // add by jie @ 02.20
//����һ���豸�߼���ַ�����壬���������豸��ַ�����ַ��ʷ�ʽ,ע���С��ģʽ
typedef union {
	unsigned short address;
	IFSF_LNA st_address;
}IFSF_LNA_UNION;
/*-------------------------------------------------------------------*/
#endif

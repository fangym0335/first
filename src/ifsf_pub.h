/*******************************************************************
�������Э��ת������oil_main�Ļ����Ͻ��е�,������ifsf_��ͷ���ļ���,
��������ļ�ҲҪ�޸�.
ע: oil_mainҲҪ�������ı�,������SRAM��Ϊ�����ڴ�,ͬʱ���ں������ӵ���ϵͳ����syscall0,
Ӧ�ó������ӵ���д�����ڴ浽nand flash�Ľ���.
-------------------------------------------------------------------
IFSF_SHM,Э��ת�����Ҫʹ�õĹ����ڴ�,
�������������,��Э��ת���ļ��ͻ��ļ�¼���,
�����͵�������Ϣ��Ĺ����ڴ�.
-------------------------------------------------------------------
2007-06-26 by chenfm
2008-03-06 - Guojie Zhu <zhugj@css-intelligent.com>
    NODE_CHANNEL.convertUsed[]��Ϊnode_online[]
*******************************************************************/
#ifndef __IFSF_PUB_H__
#define __IFSF_PUB_H__

#include "pub.h"
#include "ifsf_def.h"
#include "ifsf_dspad.h"
#include "ifsf_tcpip.h"
#include "ifsf_dsp.h"
#include "ifsf_tlg.h"


// ��������--���������ش���
#define SECOND_DISTRIBUTION  0


//ǰͥ������3����Ȩģʽ,0��1ֻ��ʼ�������ͻ�ʱ������,��ʼ��������λ�����Ƹ����ͻ�.
#define POS_CTRL_FCC  0 //��λ������ģʽ
#define POS_LISTEN_FCC 1 //��λ������ģʽ.��λ������ʱǰͥ�������Զ���Ȩ.
#define FCC_SWITCH_AUTH 2  //������Ȩ���ؿ���ģʽ,��λ�������ߵ�ʱ���Զ���λ.

#define DEFAULT_AUTH_MODE 0  //Ĭ����Ȩģʽ,ֻ��λ������ʱ������,��λ��������ʱ�Զ�תģʽ2,��IFSF_SHM->auth_flag

#define FP_TR_CONVERT_CNT  19456L //���д�Э��ת���ļ��ͻ�����������ֵ.IFSFÿ�����͵�9999��.ÿ�����ͻ������256��
#define TEMP_TR_FILE  "/home/Data/dsptr"    //����������ʱ����ļ���ǰ�沿��,����Ϊ������ĸ�Ľڵ��.

//���涨��һ��IFSF�豸�������ӵ�����IFSF�豸���������
//���������IFSF_FP2_1.01��¼1�ɵ�
#define MAX_RECV_CNT  64  // IFSF OVER LONWORK,LONWORK�е�ͨѶ���ݿ�Ҫ��Ϊ64 
//#define MAX_TLG_CNT 1 //max count of  tlg in a fcc.  only one. 
//#define MAX_NO_RECV_TIMES  -6 //think a device such as CD is dead when it has not  heartbeat more HEARTBEAT_TIME  than MAX_NO_RECV_TIMES times 
#define MAX_NO_RECV_TIMES  -1 //think a device such as CD is dead when it has not  heartbeat more HEARTBEAT_TIME  than MAX_NO_RECV_TIMES times 

#define ACK       1
#define NACK      -1


// Mark in progress offline transaction. Guojie Zhu@2008.7.9
	// There is an in progress offline transaction (Start Of OFFLINE_TR)
#define SO_OFFLINE_TR(node, idx_fp)  (gpIfsf_shm->fp_offline_flag[(node - 1)][(idx_fp)] |= 0x0F)
	// Then in progress offline tr finished (End Of OFFLINE_TR)
#define EO_OFFLINE_TR(node, idx_fp)  (gpIfsf_shm->fp_offline_flag[(node - 1)][(idx_fp)] &= 0xF0)
	// Mark There is more than 1 offline tr of this node in TR_Buffer
#define SET_FP_OFFLINE_TR(node, idx_fp)  (gpIfsf_shm->fp_offline_flag[(node - 1)][(idx_fp)] |= 0xE0)
	// Mark There is no offline tr of this node in TR_Buffer
#define CLR_FP_OFFLINE_TR(node, idx_fp)  (gpIfsf_shm->fp_offline_flag[(node - 1)][(idx_fp)] &= 0x0F)


/*-------------------------------------------------------------------*/
//relation of node and logical channel, include serial port and physical channel. 
//and also include  4 import attributes of a dispenser, that is count of fuelling poits, nozzles,
//products and nozzles of each fuelling point. 12 bytes.
typedef struct{
	unsigned char node;
	//logical channel number in a PCD, the number is no. of the linked tag which signed  at back of FCC.	
	unsigned char chnLog; 
	unsigned char nodeDriver; //dispenser driver type,this determine how to initial and wich lischnxxx to use.	
	unsigned char cntOfFps; //count of fuelling points
	unsigned char cntOfNoz; //all count of Nozzles  in the dispenser.
	unsigned char cntOfProd;//all count of oil products in the dispenser.
	//count of Nozzles  in each FP of the dispenser, if there is not  2nd,3rd,4th FP then which set 0.
	unsigned char cntOfFpNoz[4]; 
	//runtime information
	unsigned char serialPort; //use which serial port in main board, it's zero in file, only used it in runtime.
	unsigned char chnPhy;  //phisical channel number at a serial port , it's zero in file, only used it in runtime.
} NODE_CHANNEL;

//��ǹ���ýṹ��,��Ӧ��ǹ�����ļ�dsp.cfg  modify by liys
typedef struct {
	unsigned char node;             //�ڵ��
	unsigned char chnLog;          //�߼�ͨ����
	unsigned char nodeDriver;      //������
	unsigned char phyfp;           //��������, 1+.
	unsigned char logfp;           //�߼�����, 1+.
	unsigned char physical_gun_no; //������ǹ��
	unsigned char logical_gun_no;  //�߼���ǹ��
	unsigned char oil_no[4];       //��Ʒ��
	unsigned char isused;          //�Ƿ�����
} GUN_CONFIG;


typedef struct{
        unsigned char oilno[4];
	unsigned char oilname[40];
	unsigned char price[3];        //BCD
} OIL_CONFIG;

//--------------------------------------------------------------------//
//IFSF_SHM,Э��ת�����Ҫʹ�õĹ����ڴ�,
typedef struct{
	//��Ȩģʽ��־,2Ϊ��λ������ģʽ,�ɿ��ؿ����Ƿ�ɼ���;Ĭ�ϳ�ʼ��ֵΪ0��1,����λ������ģʽ
	unsigned char auth_flag;//0:��λ�����Ƽ���,1:��λ����������ǹ�ɼ���,2:��ǰͥ���������ؾ����Ƿ��Զ���Ȩ����

	//�������ݱ�־��0:û��������1:�������ǲ�Ҫʵ�����ݣ�2:������Ҫʵʱ���ݡ�
	unsigned char  css_data_flag; 
	
	//�ڵ�,�߼�ͨ��,������,�����,������,��Ʒ���Լ�ÿ�������ǹ���Ĺ�ϵ,�������ں�����ͨ��.
	NODE_CHANNEL   node_channel[MAX_CHANNEL_CNT];   // �� Node - 1 Ϊ����
	
	////ͨѶ������:----------------------------------------------------	
	FCC_COM_SV_DATA  fcc_com_sv_data;//���Զ�������,�㲥��ַ,Data_Id=200.
	
	//�������ݽ�����:----------------------------------------------------		
	//��־�����IFSF�豸�����ĸ���Ч��ÿ���յ�����ʱ��ֵΪHEARTBEAT_TIME��ֵ,
	//��ʱ���ղ���HEARTBEAT_INTERVAL�źż�1,��Ч0.��ֵ��-6?���ϱ�ʾ���Գ���ɾ��.
	short  recvUsed[MAX_RECV_CNT];
	//IFSF�豸���� 
	IFSF_HEARTBEAT recv_hb[MAX_RECV_CNT]; 
	
	//���ӵ�CD��fd
	int recvSockfd[MAX_RECV_CNT];
		
	//�������ݷ�����:----------------------------------------------------	
	//��־������豸�����ĸ����ߣ����߽ڵ��,����0, ��PCD�豸��������.   
	unsigned char node_online[MAX_CHANNEL_CNT + 1];//���һ��ΪҺλ�ǵ�����
	//��Э��ת�����豸����
	IFSF_HEARTBEAT convert_hb[MAX_CHANNEL_CNT + 1]; //���һ��ΪҺλ�ǵ�����

	//Һλ��--------------------------------------------------------------------------
	//Һλ��,��һ��,Nb_Tanksֵ(TP����)�����������õ�.	
	TLG_DATA tlg_data;//[MAX_TLG_CNT]
	
	//��Э��ת�������м��ͻ��Ľṹ��:-------------------------
	DISPENSER_DATA  dispenser_data[MAX_CHANNEL_CNT];  //Ԥ�Ʋ���1M.
	FP_TR_DATA fp_tr_data[FP_TR_CONVERT_CNT];         //import!! Ԥ�ƴ�2M.	

	GUN_CONFIG gun_config[MAX_GUN_CNT];      // ��ǹ������Ϣmodify by liys 2008-03-14
	OIL_CONFIG oil_config[MAX_OIL_CNT];      // ��Ʒ������Ϣmodify by liys 2008-03-14
	unsigned char gunCnt;                    // ��ǹ�� added by jie @ 2008-03-24
	unsigned char oilCnt;                    // ����Ʒ�� added by jie @ 2008-11-10
	unsigned char need_reconnect;            // Tcp.C ��Ҫ����Server
	unsigned char is_new_kernel;             // 1 - new version kernel(2.6.21-fcc-x.x), 0 - old version kernel(2.6.21)
	unsigned char log_level;                 // ��־�ȼ�
	// fp_offline_flag ��ǻ��������Ƿ����ѻ����׻��߸�fp�Ƿ���δ��ɵ��ѻ�����
	unsigned char fp_offline_flag[MAX_CHANNEL_CNT][MAX_FP_CNT];
	unsigned char msg_cnt_in_queue[MAX_CHANNEL_CNT]; // ��Ϣ����ORD_MSG_TRAN�е���Ϣ���� // added by jie @2008-10-27
	unsigned char cfg_changed;               // �����Ʒ�����Ƿ��иı�, 0x0F-�иı�, 0xFF-����ͬ��, 0-�޸ı�
	unsigned char get_total_flag;
	unsigned char tank_heartbeat;          //0:�ر�Һλ���������� 1:��
} IFSF_SHM;

int VrPanelNo[MAX_CHANNEL_CNT][MAX_PANEL_CNT];//Videer-RootҺλ��У׼����Ҫ�Ĵ���������
int OpwGunNo[MAX_CHANNEL_CNT][MAX_PANEL_CNT][8];  //opw-pv4 Һλ��У׼����Ĵ�����ǹ��

extern IFSF_SHM *gpIfsf_shm; //�ýṹ��ȫ�ֵ�����ֻ��һ��,��ifsf_main.h�ж���.
void show_all_dispenser();
void show_dispenser(unsigned char node);
void show_pump(PUMP *ppump);
int price_change_track();

#endif


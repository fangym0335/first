#ifndef __PUB_H__
#define __PUB_H__

#include <stdio.h>
#include <stdlib.h>
#include <linux/unistd.h>
#include "mycomm.h"
#include "mystring.h"
#include "dotime.h"
#include "commipc.h"
#include "dofile.h"
#include "doshm.h"
#include "dosem.h"
#include "tcp.h"
#include "log.h"
#include "ifsf_def.h"
#include "ifsf_dsp.h"
#include "tcc_adjust.h"
#include "ifsf_tlg.h"

/* add other headers here********************/

#define DEBUG  1


#ifdef __NR_tkill
#undef __NR_tkill
#endif

#define lock_status()    syscall(349)
#define power_status()    syscall(348)

// The value of flowing extern char is writen in file pub.cfg, 
// dsp pcd used only 3 number,tlg pcd used 3 number. it's in file pubsock.c
extern char	PCD_TCP_PORT[6]; 
extern char	SERIAL_PORT_START[2];
extern char	SERIAL_PORT_USED_COUNT[2];
extern char	TLG_NODE[4];
extern char 	TLG_DRV[4];
extern char 	TLG_SERIAL_PORT[2];
extern char 	TLG_PROBER_NUM[4];
extern char 	TCC_ADJUST_DATA_FILE_SIZE[4];         // CSSI.У�����ݲɼ�, �����ļ����ݴ�С
extern char 	SHOW_FCC_SEND_DATA[2]; 		//0 ����ӡFCC���͸��ͻ����ݣ�1��ӡ

#define TIME_OUT_WRITE_PORT	2
#define RET_TIME_OUT		-2
#define TIME_FOR_RECV_CMD	0	
#define TIME_FOR_TCP_CMD 	0

#define MAX_EXTEND_LEN		8		/*���ͻ��������󳤶�*/
#define GUN_SHM_INDEX		100
#define GUN_SHM_TRAN		101
#define GUN_MSG_TRAN		102
#define ORD_MSG_TRAN		103
#define TOTAL_MSG_TRAN        119
#define TCP_MSG_TRAN        	120
#define TCP_MSG_TRAN2        	121
#define PORT_SEM_INDEX		104
#define ERR_SEM_INDEX		105
#define CD_SOCK_INDEX		106
#define CFG_SHM_INDEX		107
#define TANK_MSG_ID		111 
#define WAYNE_RCAP2L_SEM_INDEX  108             // ��������rcap2lʹ��
#define TCC_ADJUST_SEM_INDEX    109             // У����Ŀ���ݲɼ��ļ�����

#define MAX_GUN_CNT     	128             /*���ǹ��*/ //modify by liys 2008-03-14
#define MAX_OIL_CNT             16              /*�����Ʒ��*///modify by liys 2008-03-14

#define MAX_CHANNEL_CNT    	64		/*���ͨ����*/  //=MAX_CHANNEL_CNT
#define MAX_CONVERT_CNT 	MAX_CHANNEL_CNT
#define MAX_PORT_CNT		6		/*��󴮿���*/
#define MAX_CHANNEL_PER_PORT_CNT	16	/*ÿ���������ͨ����*/
#define MAX_PANEL_CNT		4		/*ÿ���ͻ����������*/
#define MAX_ERR_IND		200
#define MAX_ORDER_LEN		160
#define MAX_LOG_LEN		160
#define MAX_PORT_CMDLEN		255	        /*�򴮿�д�����ݵ���󳤶�*/

#define INDTYP			10000000
#define MAXADD		 	9999999
#define TCP_HEAD_LEN		4
#define TCP_TIME_OUT		3

#define RUN_PROC		0		/*����״̬*/
#define STOP_PROC 		1		/*ϵͳֹͣ*/
#define RESTART_PROC 		2		/*ϵͳ����*/
#define PAUSE_PROC		3		/*��ͣ*/

#define PAUSE_FOR_TTY		1		/*��ͣʱtty��ͣ��ʱ��*/
#define MAX_VOLUME		3		// volume length

#define DEFAULT_WORK_DIR        "/home/App/ifsf/"   //�����Ĭ�Ϲ���Ŀ¼. 
#define DEFAULT_DATA_DIR        "/home/Data/"       // Ĭ�����ݴ��Ŀ¼
#define NAND_FLASH_FILE	        "/home/Data/pwf_tr.dat" // ����ʱ��Ž�������
#define BAK_TR_SEQ_NB_FILE      "/home/Data/pwf_tr_seq_nb.dat"   // ����ʱ���TR_SEQ_NB
#define BAK_LNZ_TOTAL_FILE      "/home/Data/pwf_lnz_total.dat"   // ����ʱ�����ǹ����

#define UNPAIDLIMIT             2               // �����δ֧�����ױ���
#define MAX_FAIL_CNT            3               // ���ͻ�ͨѶʧ�ܼ����ﵽ��ֵ�ж�Ϊ�ͻ�����
#define SENDSTATE               5               // ��CLOSED״̬��, ���ڵ��ڸ�POS����״̬��Ƶ��
#define PRICE_CHANGE_RETRY_INTERVAL 2000000     // ���ʧ�����Լ�� 2s (2000000΢��), ��ʱ����˰���Լ��ͻ�����;

#define SYSLOGFILE	"log/sys.log"	        /*ϵͳ��־�ļ�.׼��������¼�¶�,��������ʱ��,����״̬��ʱ����־*/
#define TLGLOGFILE	"log/tlg.log"		/*Һλ��������־.��¼Һλ�ǵ�������Ϣ.*/
#define DSPCFGFILE	"etc/dsp.cfg"    	/*���ͻ��ڵ��,���ͻ��ͺ�,ͨ��,���ڵ�������Ϣ�ļ�,IFSF���ͻ�ר��*/
#define PUBFILE		"etc/pub.cfg"		/*���ͻ�,Һλ�ǵȵĵ� ���������ļ�*/
#define TCCFILE		"etc/tcc.cfg"	/*У�������ļ���С���Ʋ��������ļ���λ K*/
#define IFSFFILE	"etc/dsp.cfg"		/*�ڵ��,���ͻ�,ͨ��,���ڵ�������Ϣ�ļ�,IFSFר��*/
#define OILFILE         "etc/oil.cfg"            /*��Ʒ�����ļ�,modify by liys */
#define LOGFILE		"log/oil.log"		/*���ͻ���־�ļ�.��¼���ͻ������ʹ������ͨ��־*/
#define RUNFILE		"log/run.log"		/*���ͻ�������־.��¼���ͻ���������Ϣ.*/

#define PRODUCT_LEVEL_INTERVAL  100             // CSSI.У�� Һλ���ٱ仯 (PRODUCT_LEVEL_INTERVAL / 1000)mm ��¼һ��Һλ
#define PRODUCT_LEVEL_INTERVAL2  10             // CSSI.У�� �ӽ�������Һλʱ�ļ�¼���

/*macro do segment must be interval seconds after last one*/
#define DO_BUSY_INTERVAL 1

static time_t last_time;
#define DO_INTERVAL(interval, segment)  do {\
	time_t tmp_time = time(NULL);\
	if (tmp_time > last_time + interval) {\
		{segment}\
		last_time = tmp_time;\
	}\
}while(0)

typedef struct{
	pid_t 	pid;
	char	flag[4];	//���̱�־,����������������ý���.
}Pid;

typedef struct{
	int	fd;
	int	port;
	unsigned char chn[MAX_PORT_CNT][MAX_CHANNEL_PER_PORT_CNT];
}Port;



/*���干���ڴ�Ľṹ*/
typedef struct{
	unsigned char	chnLogUsed[MAX_CHANNEL_CNT];	/*��ʾ�߼�ͨ���Ƿ�ʹ��*/
	unsigned char	portLogUsed[MAX_PORT_CNT];	/*��ʾ�����Ƿ�ʹ��*/
	unsigned char	chnLogSet[MAX_CHANNEL_CNT];	/*����ͨ���Ƿ��Ѿ���������*/
	int		chnCnt;				/*��ʹ�õ�ͨ����*/
	int		portCnt;			/*��ʹ�õĴ�����*/
	unsigned char	wantLen[MAX_CHANNEL_CNT];       /*Ϊͨ���ڽ���ʱʹ��.��ʶ���߼�ͨ��ϣ����̨���صĳ���*/
	long	wantInd[MAX_CHANNEL_CNT];       /*Ϊͨ���ڽ���ʱʹ��.��ʶ�ں�̨ȡ�ø��߼�ͨ�������ݺ�,����Ϣ����д����Ϣ��type*/
	long	minInd[MAX_CHANNEL_CNT];	/*ÿ��ͨ������С��Ϣ����TYPEֵ*/
	long	maxInd[MAX_CHANNEL_CNT];	/*ÿ��ͨ�������Ϣ����TYPEֵ*/
	long	useInd[MAX_CHANNEL_CNT];	/*Ϊͨ���ڽ���ʱʹ��.��ʶ��ͨ���ڽ�����ʹ�õ���Ϣ����type*/
						/*��ͨ�����պ�̨���ݳ�ʱʱ,useInd+1*/
	int	isCyc[MAX_CHANNEL_CNT];		/*ͨ����indtyp�Ƿ��Ѿ��������ֵ��ѭ����*/
	int	errCnt;				/*��Ϊ�������ѳ�ʱ����type����.���ڻ��ճ������*/
	long	errInd[MAX_ERR_IND];		/*��Ϊ�������ѳ�ʱ����type*/
	int	retryTim[MAX_ERR_IND];		/*���ճ�������ѳ�ʱ���еĴ���*/
	int	sysStat;			/*ϵͳ���б�־. 0 �������� 1ϵͳ��ֹ 2���������ļ�(�Ѿ�����),3��ͣ*/
	unsigned char	addInf[MAX_CHANNEL_CNT][16];	/*������Ϣ.�����������治ͬЭ���һЩ��������*/
	int ppid;                               /*2007.8.27 parent proccess id of main proccess, used for remove ipc*/
	int sockCDPos;                          /*2007.9.10  position of socked fd of current  CD */
	unsigned char CDBuffer[1024];            /*2007.9.10  use  to send data to current  CD */
	size_t CDLen;                           /*2007.9.10  data length of to send data to current  CD */
	int CDtimeout;                          /*2007.9.10  time out to send data to current  CD */
	int LiquidAdjust;                       /*2009.04.13 Adjust for Liquid ,0: ����У׼, 1:��У׼*/
	/* wayne_rcap2l_flag: ����ģ��ʹ��, ����Ƿ��Ѿ��޸��˴��ڲ����ж����� 
	 * 0 - ��, 1 - ��������, 2 - �������, 3 - IO����汾�Ͳ�֧���޸�����
	 *
	 */
	int wayne_rcap2l_flag;
	int tr_fin_flag;                          // TCC: ���׽������, ����н��׽���, Һλ�ǽ�����Ҫ��¼һ��Һλ
	int tr_fin_flag_tmp;                      // TCC:
	int data_acquisition_4_TCC_adjust_flag;   // TCC: Tank capacity chart, !0 - start(1-first time), 0 - stop
	__u8 product_level[MAX_TP_PER_TLG][4];    // TCC: У����Ŀ���ݲɼ�����,��¼��һ�βɼ���Һλ�߶�
	int if_changed_prno;
} GunShm;


/*����һ���ṹ.����ά������ʱһ���ڵ����Ϣ*/
typedef struct{
	unsigned char	port;                             // �ڵ����ڴ���
	unsigned char	chnLog;                           // ������ڵ�ͨ����(�߼�ͨ����)
	unsigned char	chnPhy;                           // ����ͨ���� 1-8 / 1-16, ���ͬ�⴮��
	unsigned char	panelCnt;                         // �����
	unsigned char	panelLog[MAX_FP_CNT];             // �߼�����1-MAX
	unsigned char	panelPhy[MAX_FP_CNT];             // FP �������ַ, ʵ�ʸ�ֵ��Ӧ�ͻ�Э���е�ǹ�š����� 
	unsigned char 	cmd[MAX_ORDER_LEN+1];
	// ����ʹ�õ��߼�ǹ��(1,2..), ��ǹ�����㣬��ͬ��FP_DB��Current_Log_Noz, ��Ϊ����Ҫ��һ
	unsigned char	gunLog[MAX_FP_CNT];               
	unsigned char	gunPhy[MAX_FP_CNT];               // ����ʹ�õ�����ǹ��
	unsigned char 	gunCnt[MAX_FP_CNT];               // ÿ������ǹ��
	unsigned char	wantLen;                          // ��Ҫ��̨���صĳ���
	unsigned char	addInf[MAX_FP_CNT][16];           // ������Ϣ������ģ����;��ͬ
	unsigned char   newPrice[FP_PR_NB_CNT][4];        // Bin8 + Bcd6
			// �����������ͼۻ����ڴˣ������ɺ�����
	size_t		cmdlen;
	int		fd;                               // serial port fd 
	int		indtyp;                           // �Ӻ�̨ȡ�������Ϣ����,typeֵ���ᷢ���仯
	time_t          time_offline;                     // ��¼�ͻ����߻ָ�����Сʱ���(��֤CD�ܹ�֪���ͻ����߹�)
	int             safe_resume;            // ���, ���ٻָ����߻���ȷ��CD֪���ͻ�������֮��������, 1-��ȫ, 0-����
} PUMP;

extern GunShm	*gpGunShm;                                // �����ڴ��е�gpGunShm

#define  NODE_OFFLINE  0
#define  NODE_ONLINE   1
int ChangeNode_Status(int node, __u8 status);
int Update_Prod_No();
#endif


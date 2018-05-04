#include <sys/utsname.h>
#include <string.h>
#include <errno.h>
#include "pub.h"
#include "ifsf_pub.h"
#include "tty.h"

int DelErrInd(long errInd)
{
	int i;
	int n;

	n = gpGunShm->errCnt;

	for(i = 0; i < n; i++) {
		if(gpGunShm->errInd[i] == errInd) {
			DelErrIndByPos(i);
			break;
		}
	}
	return 0;
}

int DelErrIndByPos(int pos)
{
	int i;
	int n;

	n = gpGunShm->errCnt;

	if(pos != n -1)	/*pos=n-1ʱ�����Ѿ�û�з���Ϣ��*/
	{
		for(i = pos; i < n-1; i++)	/*��i����Ϣ�Ѿ���ȡ��,�������Ϣǰ��*/
		{
			gpGunShm->errInd[i] = gpGunShm->errInd[i+1];
			gpGunShm->retryTim[i] = gpGunShm->retryTim[i+1];
		}
	}
	gpGunShm->errCnt--;

}

int GetReturnMsg(int index, unsigned char chnLog, char *cmd, size_t *cmdlen)
{
	int ret;
	int i;
	int errcnt;
	long indtyp;

	indtyp = gpGunShm->useInd[chnLog - 1];


	/*����Ϣ���ж�ȡ��Ϣ*/
	ret = RecvMsg(GUN_MSG_TRAN, cmd, cmdlen, &indtyp, 2);    /*����Ϣ������ȡֵ*/
	if(ret < 0) {
		if(ret == -2)	{                                  /*��ʱ.����ʱ��indtyp���빲���ڴ�.ͬʱindtyp++*/
			RunLog("Recv Msg Timeout");
#if 0
			RunLog("CHN[%d] Recv Msg Timeout: %s(%d)", chnLog, Asc2Hex(cmd, *cmdlen), *cmdlen);
#endif

			Act_P(ERR_SEM_INDEX);

			/*ȡ���еĳ�ʱindtypֵ*/
			errcnt = gpGunShm->errCnt;                 /*��һ���ֽ��������ܳ�ʱindtypֵ*/
			if(errcnt < MAX_ERR_IND) {               /*��ʱ��ֵδ�ﵽ���ֵ.��ʱ�����һ����λ�ü��ϴ˳�ʱ��ֵ*/
				gpGunShm->errInd[errcnt] = indtyp; /*��ʱ�ļ�ֵ�������*/
				gpGunShm->retryTim[errcnt] = 0;
				gpGunShm->errCnt++;
			} else {                                   /*δ����ĳ�ʱ��ֵ�ﵽ���ֵ*/
				/*����ǰ��ļ�ֵΪ��ɵļ�ֵ.�����ѳ�ʱ��ֵǰ��*/
				for(i = 0; i < errcnt - 1; i++) {
					gpGunShm->errInd[i] = gpGunShm->errInd[i + 1];
					gpGunShm->retryTim[i] = gpGunShm->retryTim[i + 1];
				}
				gpGunShm->errInd[errcnt] = indtyp;	/*��ʱ�ļ�ֵ�������*/

			}

			if(gpGunShm->useInd[chnLog - 1] == gpGunShm->maxInd[chnLog - 1])  {     /*�������ֵ*/
				gpGunShm->useInd[chnLog - 1] = gpGunShm->minInd[chnLog - 1];
				DelErrInd(gpGunShm->minInd[chnLog - 1]); /*�ڳ�ʱ�Ķ���������Ƿ��д˳�ʱ��ֵ.����ɾ��*/
			} else {
				gpGunShm->useInd[chnLog - 1] += 1;
				if(gpGunShm->isCyc[chnLog - 1] == 1)	 /*indtyp�Ѿ�ѭ����һ����*/
					DelErrInd(gpGunShm->useInd[chnLog - 1]);
			}

			Act_V(ERR_SEM_INDEX);

			return RET_TIME_OUT;
		} else {
			RunLog("Recv Msg Failed");
			return -1;
		}

	}

	RunLog("CHN[%02d]<--Pump: %s(%d)", chnLog, Asc2Hex(cmd, *cmdlen), *cmdlen);

	return 0;
}

void EndProc()
{
	gpGunShm->sysStat = STOP_PROC;
	return;
}


/*
 * func: �ж��Ƿ�Ϊ�°��ں�
 *       6.30�Ե���ʹ�õ��ں�Ϊ���ں�, ���ں˸Ľ��˵����ǵļ�⴦��, 
 *       ������������״̬�ļ��(����Ƿ������߶Ͽ�������), ��������Ķ��޷���ģ�����ʽʵ��, 
 *       ����, ֻ����ʹ�����ں�(2.6.21)��ϵͳʹ��ԭ�д���ʽ,
 *       �����ں�(2.6.21-fcc-1.0)ʹ���µĴ���ʽ
 * return: 0 - ���ں�(kernel release ver Ϊ 2.6.21), 1 - ���ں�(2.6.21-fcc-1.0 ��֮��汾)
 */

int is_new_version_kernel()
{
	int ret;
	struct utsname uts;
	unsigned char ver_o[24] = "2.6.21";

	if (uname(&uts) < 0) {
		RunLog("Get kernel release version failed, err: %s", strerror(errno));
		return -2;
	}

	RunLog("Kernel release version is: %s(%d)", uts.release, strlen(uts.release));
	ret = memcmp(uts.release, ver_o, strlen(uts.release));
	if (ret > 0) {
		gpIfsf_shm->is_new_kernel = 1;
		return 1;    // yes, it's new version
	} else {
		gpIfsf_shm->is_new_kernel = 0;
		return 0;    // no
	}
}




int Update_Prod_No()
{
	int i, j, x, y, k;
	FP_PR_DATA *pfp_pr_data;
	FP_PR_DATA *pfp_pr_data2;
	DISPENSER_DATA *dispenser_data;
	 
	
	for (i = 0; i < MAX_CHANNEL_CNT; i++) {
		dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
		for (j = 0; j < MAX_FP_CNT; j++) {
			pfp_pr_data = (FP_PR_DATA *)( &( dispenser_data->fp_pr_data[j]) );
			if (pfp_pr_data->Prod_Nb[0] != 0 && pfp_pr_data->Prod_Nb[1] != 0 \
						&& pfp_pr_data->Prod_Nb[2] != 0 && pfp_pr_data->Prod_Nb[3] != 0) {
				//memcpy()
				for (x = 0; x < gpIfsf_shm->gunCnt; x++) {
					if (gpIfsf_shm->gun_config[x].node == gpIfsf_shm->node_channel[i].node) {
						for (y = 0; y < FP_PR_CNT; y++) {
							RunLog(" i = %d j = %d x = %d y = %d k = %d", i, j, x, y, k);
							pfp_pr_data2 = (FP_PR_DATA *)( &( dispenser_data->fp_pr_data[y]) );
							RunLog("FFFFFFF %s  ", Asc2Hex(pfp_pr_data2->Prod_Nb, 4));

							if (memcmp(gpIfsf_shm->gun_config[x].oil_no, pfp_pr_data2->Prod_Nb, 4) == 0) {
								break;
							}
						}

						if (y == FP_PR_CNT) {
							memcpy(pfp_pr_data->Prod_Nb, gpIfsf_shm->gun_config[x].oil_no, 4);
							for (k = 0; k < gpIfsf_shm->oilCnt; k++) {
								if (memcmp(gpIfsf_shm->gun_config[x].oil_no, gpIfsf_shm->oil_config[k].oilno, 4) == 0) {
									memcpy(pfp_pr_data->Prod_Description, gpIfsf_shm->oil_config[k].oilname, sizeof(pfp_pr_data->Prod_Description));
								}
							}
						}
					}
				}
			}
		}
	}
}



/*
 * func: ����һЩ���� (��־����, ���ø���ͬ����)
 */

void Misc_Process()
{
	int cnt = 0;
	int cnt_tcc = 0;
	int i;
	gpIfsf_shm->cfg_changed = 0;

	while (1) {
		// 1. ÿ��10s���һ�α�� cfg_changed
		sleep(10);
		cnt++;
		cnt_tcc++;

		
		//Update_Prod_No();
		
		if (0x0F == gpIfsf_shm->cfg_changed) {
			SyncCfg2File();
		}

		// 2. ÿ�� 10 ����, ���һ����־�Ƿ���Ҫ����
		//if (cnt >= 60) {
		if (cnt >= 30) {
			cnt = 0;
			if (backup_log_file() < 0) {
				RunLog("exec backup_log_file failed");
			}

			// 2.2 У�������ļ�ת��
			dump_tcc_adjust_data_file(1);
		}

		// 3. ÿ�� 1 ���ӣ����һ�����ݲɼ���ͣ�ļ����
		if (cnt_tcc >= 6) {
			cnt_tcc = 0;
			RunLog("access if the file start/stop exsit");
			if ((access("/home/Data/log_backup/start", F_OK) == 0) &&
					    gpGunShm->data_acquisition_4_TCC_adjust_flag == 0) {
				// start�ļ�����,�����ɼ�
				gpGunShm->data_acquisition_4_TCC_adjust_flag = 1;
				TCCADLog("TCC-AD: ���ݲɼ���������, data_aq_4_tcc_adjust_flag: %d", 
								gpGunShm->data_acquisition_4_TCC_adjust_flag);
				// ��¼��ǰ���ͻ��ڵ����������, 0 - ����, 1 - ����
				for (i = 0; i <  MAX_CHANNEL_CNT; ++i) {
					if (gpIfsf_shm->node_channel[i].node == 0) {
						continue;
					}
					write_tcc_adjust_data(NULL, ",,%d,,,,,%d,,,",
							gpIfsf_shm->node_channel[i].node,
							(gpIfsf_shm->node_online[i] == 0 ? 0 : 1));
					TCCADLog("TCC-AD: Node %d %s", gpIfsf_shm->node_channel[i].node,
							(gpIfsf_shm->node_online[i] == 0 ? "����" : "����"));
				}
				// -- Һλ��
				write_tcc_adjust_data(NULL, ",,91,,,,,%d,,,",
						gpIfsf_shm->node_online[MAX_CHANNEL_CNT]);
				TCCADLog("TCC-AD: Һλ��%s", (gpIfsf_shm->node_online[MAX_CHANNEL_CNT] == 0 ? "����" : "����"));
			}
			if ((access("/home/Data/log_backup/stop", F_OK) == 0) &&
					     gpGunShm->data_acquisition_4_TCC_adjust_flag != 0) {
				// stop�ļ�����,ֹͣ�ɼ�,stop�ļ������ǲ���ֹͣ,
				// start|stop��������ʱ��֮ǰ״̬����ִ��
				gpGunShm->data_acquisition_4_TCC_adjust_flag = 0;
				TCCADLog("TCC-AD: ���ݲɼ�����ֹͣ, data_aq_4_tcc_adjust_flag: %d",
								gpGunShm->data_acquisition_4_TCC_adjust_flag);
				// ����ת��У�������ļ�
				dump_tcc_adjust_data_file(0);
			}
		}
	}
}


/*
 * func: ��ӡȫ��dispenser����Ϣ
 */
void show_all_dispenser()
{
	int i, j, k;
	FP_DATA *pfp;
	DISPENSER_DATA *pdsp;

	if (gpIfsf_shm == NULL) {
		RunLog("gpIfsf_shm Ϊ��");
		return;
	}

	for (i = 0; i < MAX_CHANNEL_CNT; i++) {
       		if (gpIfsf_shm->convert_hb[i].lnao.node > 0) {
			show_dispenser(gpIfsf_shm->convert_hb[i].lnao.node);
		}
	}
}

/*
 * ��ӡһ�� dispenser ����Ϣ
 */
void show_dispenser(unsigned char node)
{
	int i, j, k;
	FP_DATA *pfp;
	DISPENSER_DATA *pdsp;

	i = 0;
	if (gpIfsf_shm == NULL) {
		RunLog("gpIfsf_shm Ϊ��");
		return;
	}

	for (i = 0; i < MAX_CHANNEL_CNT; ++i) {
		if (gpIfsf_shm->convert_hb[i].lnao.node == node) {
			break;
		}
	}

	printf("--------------- Dispenser.Node: %d --------------------------------\n", 
							gpIfsf_shm->convert_hb[i].lnao.node);

	pdsp = (DISPENSER_DATA *)&gpIfsf_shm->dispenser_data[i];
	printf("------- FP_C_DATA --------------------------\n");
	printf(" .Nb_Products: %02x, .Nb_FP: %02x\n", pdsp->fp_c_data.Nb_Products, pdsp->fp_c_data.Nb_FP);

	printf("\n------ FP_DATA ---------------------------\n");
	for (j = 0; j < pdsp->fp_c_data.Nb_FP; j++) {
		pfp = (FP_DATA *)&pdsp->fp_data[j];
		printf("**FP_ID: %02x \n \
.FP_State: %02x                 .Nb_Logical_Nozzle: %02x \n \
.Log_Noz_State: %02x            .Fuelling_Mode: %02x     \n \
.Nb_Tran_Buffer_Not_Paid: %02x  .Nb_Of_Historic_Trans: %02x \n \
.Assign_Contr_Id: %02x%02x        .Release_Contr_Id: %02x%02x \n \
.Current_TR_Seq_Nb: %02x%02x      .Transaction_Sequence_Nb: %02x%02x \n \
.Current_Prod_Nb: %s     ",
			0x21 + j, pfp->FP_State, pfp->Nb_Logical_Nozzle,
			pfp->Log_Noz_State, pfp->Fuelling_Mode, 
			pfp->Nb_Tran_Buffer_Not_Paid, pfp->Nb_Of_Historic_Trans,
			pfp->Assign_Contr_Id[0], pfp->Assign_Contr_Id[1],
			pfp->Release_Contr_Id[0], pfp->Release_Contr_Id[1],
			pfp->Current_TR_Seq_Nb[0], pfp->Current_TR_Seq_Nb[1], 
			pfp->Transaction_Sequence_Nb[0], pfp->Transaction_Sequence_Nb[1], 
			Asc2Hex(pfp->Current_Prod_Nb, 4));
		printf(".Current_Unit_Price: %s \n", Asc2Hex(pfp->Current_Unit_Price, 4));

		printf("      -------- FP_LN_DATA ---------\n");
		for (k = 0; k < pfp->Nb_Logical_Nozzle; k++) {
			printf("     LNZ_ID: %02x", k);
			printf("     .PR_Id: %02x,                   .Physical_Noz_Id: %02x\n",
			pdsp->fp_ln_data[j][k].PR_Id, pdsp->fp_ln_data[j][k].Physical_Noz_Id);
		}
		printf("\n");
	}

	printf("\n------ FP_PR_DATA  & FP_FM_DATA -----------------\n");
	for (j = 0; j < gpIfsf_shm->node_channel[i].cntOfProd; j++) {	
		printf(" PR_Id[%d].Nb_Pb: %s\t", j, Asc2Hex(pdsp->fp_pr_data[j].Prod_Nb, 4));
		printf("Prod_Price: %s \n", Asc2Hex(pdsp->fp_fm_data[j][0].Prod_Price, 4));
	}

	printf("\n--------------------- End --------------------------------------\n");
}

/*
 *  ��ӡPUMP����Ϣ
 */

void show_pump(PUMP *ppump)
{
	int i;
	int fp_cnt;

	fp_cnt = ppump->panelCnt;

	printf("\n-------------------- Pump Message ---------------------------------\n");
	printf(".port: %02x,  .chnLog: %02x,  .chnPhy: %02x,  fpCnt: %02x\n",
			ppump->port, ppump->chnLog, ppump->chnPhy, fp_cnt);

	printf(".fp_pos:   ");
	for (i = 0; i < fp_cnt; ++i) {
		printf("%02x    ", i);
	}
	printf("\n");

	printf(".panelPhy: ");
	for (i = 0; i < fp_cnt; ++i) {
		printf("%02x    ", ppump->panelPhy[i]);
	}
	printf("\n");

	printf(".gunCnt:   ");
	for (i = 0; i < fp_cnt; ++i) {
		printf("%02x    ", ppump->gunCnt[i]);
	}
	printf("\n");

	printf(".gunLog:   ");
	for (i = 0; i < fp_cnt; ++i) {
		printf("%02x    ", ppump->gunLog[i]);
	}
	printf("\n");

	printf(".gunPhy:   ");
	for (i = 0; i < fp_cnt; ++i) {
		printf("%02x    ", ppump->gunPhy[i]);
	}
	printf("\n");

	printf("\n--------------------- End --------------------------------------\n");
}



/*
 * func: ��ӡ������ǹ��״̬����Ʒ��������Ϣ���ؼ���־�ļ�critical.log
 */
int price_change_track()
{
	int i, j;
	int node;
	int fp_cnt;
	int fp_idx;
	int lnz_idx;
	int pr_no;
	int state_idx;
	FP_DATA *pfp;
	DISPENSER_DATA *pdsp;
	unsigned char *fp_state[] = {"����  ", "������", "����  ", "����  ",
			             "̧ǹ  ", "����Ȩ", "      ", "      ", "������"};
	static time_t pre_price_change_start = 0; // ���һ�α�۲�������ʼʱ��

	RunLog("------------ call price _change_track %ld---------", pre_price_change_start);
	if (time(NULL) < (pre_price_change_start + 600)) {
		// ���յ������������һ���յ��������δ����10����, ����Ϊ��ͬһ��
		RunLog("---------- time:%ld less than pre + 600: %ld", time(NULL), pre_price_change_start);
		return 0;
	}

	// һ���µı�۲���, ��ӡ������ǹ��״̬�Լ���Ʒ�������������
	
	pre_price_change_start = time(NULL);
	CriticalLog("\n\n________________________________________________________________________________________________\n"); 

	CriticalLog("         < ��ǰ��ǹ״̬������Ʒ������������� >  \n");
	for (i = 0; i < MAX_CHANNEL_CNT; ++i) {
		node = gpIfsf_shm->convert_hb[i].lnao.node;
		if (node == 0) { 
			// ��λ��û�нڵ�����, ����
			continue;
		}

		// ÿ��Node����Ϣ�ÿ��зָ�
		if (gpIfsf_shm->node_online[i] == 0) {
			CriticalLog("�ڵ�[%02d]����[����]״̬\n", node);
			continue;
		}

		pdsp = (DISPENSER_DATA *)&(gpIfsf_shm->dispenser_data[i]);
		fp_cnt = pdsp->fp_c_data.Nb_FP;
		for (fp_idx = 0; fp_idx < fp_cnt; ++fp_idx) {
			state_idx = gpIfsf_shm->node_online[i] == 0 ? 0 : pdsp->fp_data[fp_idx].FP_State;
			for (lnz_idx = 0; lnz_idx < (pdsp->fp_data[fp_idx].Nb_Logical_Nozzle); ++lnz_idx) {
				
				pr_no = pdsp->fp_ln_data[fp_idx][lnz_idx].PR_Id - 1;

				CriticalLog("�ڵ�[%02d]��[%d]ǹ[%d]״̬[%s]��Ʒ��[%02X%02x%02x%02x]��ǰ����[%02x.%02x]", 
					node, fp_idx + 1, lnz_idx + 1, 
					fp_state[state_idx],
					pdsp->fp_pr_data[pr_no].Prod_Nb[0],
					pdsp->fp_pr_data[pr_no].Prod_Nb[1],
					pdsp->fp_pr_data[pr_no].Prod_Nb[2],
					pdsp->fp_pr_data[pr_no].Prod_Nb[3],
					pdsp->fp_fm_data[pr_no][pdsp->fp_data[fp_idx].Default_Fuelling_Mode - 1].Prod_Price[2],
					pdsp->fp_fm_data[pr_no][pdsp->fp_data[fp_idx].Default_Fuelling_Mode - 1].Prod_Price[3]);
			} // end for lnz_idx
		} // end for fp_idx
		CriticalLog("");
	} // end for i < MAX_CHANNEL_CNT
	CriticalLog("         < ��������·�ִ����� >  \n");

	return 0;
}


/*
 * ��ȡIO�������汾�Ϳ���, �汾��Ϊ�����ֽڳ���,����1�ֽ�
 */
static int ReadIOVersion(int portnum, unsigned char *msg) 
{	
	int fd;
	int ret;
	int timeout = 1;
	unsigned char io_cmd45[2] = {0x1c, 0x45};
	unsigned char io_cmd50[2] = {0x1c, 0x50};
	unsigned char ttynam[30] = {0};
	unsigned char buff[16] = {0};
	
	strcpy(ttynam, alltty[portnum - 1]);

	fd = OpenSerial(ttynam);
	if (fd < 0) {
		RunLog( "�򿪴���%sʧ��", ttyname);
		return -1;
	}

	ret = WriteByLengthInTime(fd, io_cmd45, 2, timeout);
	if (ret < 0) {
		RunLog( "��1c45������� %d ʧ��", portnum);
		return -1;
	}

	memset(msg, 0, sizeof(msg));
	ret = ReadByLengthInTime(fd, msg, 2, timeout);
	if (ret < 0 || ret == 0) {
		RunLog( "����1c45������Ϣʧ��");
		return -1;
	}

	/*
	 * 1. ��ȡIO����
	 * 2. �����ϰ汾(0201etc.)1C45����û�н�����������̵�bug,
	 *  ����������1C����û�н������²���ͨѶ.
	 */
	ret = WriteByLengthInTime(fd, io_cmd50, 2, timeout);
	if (ret < 0) {
		RunLog( "��1c50������� %d ʧ��", portnum);
		return -1;
	} else {
		ret = ReadByLengthInTime(fd, buff, 1, timeout);
		if (ret < 0 || ret == 0) {
			RunLog( "����1c50������Ϣʧ��");
			return -1;
		}
		msg[2] = buff[0];
	}

	close(fd);

	return 0;
}

/*
 * ��ȡȫ��IO��ĳ���汾, ���ݾ���������� wayne_rcap2l_flag���λ,
 * ����ģ��ݴ����ж��Ƿ��ܹ��·�1c45�����޸�IO�ж�����,
 * ��Ϊ����·�1c45����֧�ָ������IO��, ������IOͨѶ����;
 *
 * ��IO����汾������1006, �� wayne_rcap2l_flag ��Ϊ0, ������Ϊ3.
 *
 * ��, ����汾��Ϣ���ļ� /var/run/io.ver. (Note! FCC�汾��Ϣ�ռ������õ����ļ���Ϣ)
 *
 *
 */
int InitWayneRcap2lFlag()
{
	int fd;
	int ret;
	int portCnt;
	int startPort;
	int portnum;
	unsigned char msg[256];
	unsigned char buf[256];


	memset(msg, '\0', sizeof(msg));
	memset(buf, '\0', sizeof(buf));

	startPort = atoi(SERIAL_PORT_START);
	if (startPort <= 0 || startPort > MAX_PORT_CNT) {
		RunLog("value of SERIAL_PORT_START[%d] is error", startPort);
		return -1;
	}

	portCnt = atoi(SERIAL_PORT_USED_COUNT);
	if (portCnt <= 0 || portCnt > MAX_PORT_CNT) {
		RunLog("value of SERIAL_PORT_USED_COUNT[%d] is error", portCnt);
		return -1;
	}


	fd = open("/var/run/io.ver", O_CREAT | O_TRUNC | O_RDWR | O_APPEND, 00664);
	if (fd < 0) {
		RunLog("Open /var/run/io.ver failed[%s]", strerror(errno));
		return -1;
	}

	gpGunShm->wayne_rcap2l_flag = 0;

	for (portnum = startPort; portnum < (portCnt + startPort); ++portnum) {
		ret = ReadIOVersion(portnum, msg);
		/*
		 * ����ɹ�����IO�汾��, ���Ұ汾�Ų�С�� 1006,
		 * ���ʾIO����֧����������RCAP2L
		 */
		if ((ret == 0) && ((msg[0] * 256 + msg[1]) >= 0x1006)) {
			ret = snprintf(buf + strlen(buf), 16, "<PORT%1d,%02x%02x,%02d>",
							portnum, msg[0], msg[1], msg[2]);
			continue;
		} else if (ret == 0) {
			ret = snprintf(buf + strlen(buf), 16, "<PORT%1d,%02x%02x,%02d>", 
							portnum, msg[0], msg[1], msg[2]);
			// IO����֧������RCAP2L
			gpGunShm->wayne_rcap2l_flag = 3;   
			continue;
		} else {
			// IO����֧������RCAP2L
			gpGunShm->wayne_rcap2l_flag = 3;   
			ret = snprintf(buf + strlen(buf), 16, "<PORT%1d,0000,%02d>", portnum, msg[2]);
			continue;
		}
	}

	RunLog("���ô�����[%d],IO�汾[%s], wayne_flag[%d]",
		       	portCnt, buf, gpGunShm->wayne_rcap2l_flag);

	ret = write(fd, buf, strlen(buf));
	if (ret != strlen(buf)) {
		RunLog("дIO�汾��Ϣ��io.verʧ��[%s]", strerror(errno));
	}

	close(fd);

	if (3 == gpGunShm->wayne_rcap2l_flag) { 
		return 1;
	} else {
		return 0;
	}
}

/*
 * \brief  ���ýڵ�������״̬
 * \param  node    �ڵ��
 * \param  status  ����--0������-1
 * \return int     �ɹ�����0��ʧ�ܷ���-1
 */
int ChangeNode_Status(int node, __u8 status)
{
	int i;

	if (gpIfsf_shm == NULL) {
		RunLog("���ӹ����ڴ����");
		return -1;
	}

	for (i = 0; i < MAX_CHANNEL_CNT; i++) {
		if (node != gpIfsf_shm->node_channel[i].node) {
			continue;
		}

		if (status == NODE_OFFLINE) {
			gpIfsf_shm->node_online[i] = 0;
		} else {
			gpIfsf_shm->node_online[i] = node;
		}

		if (gpGunShm->data_acquisition_4_TCC_adjust_flag != 0) {
			write_tcc_adjust_data(NULL, ",,%d,,,,,%d,,,", node, status);
			TCCADLog("TCC-AD: �ڵ� %d %s", node, (status == NODE_OFFLINE ? "����" : "����"));
		}
		return 0;
	}

	return -1;
}

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

	if(pos != n -1)	/*pos=n-1时后面已经没有废消息了*/
	{
		for(i = pos; i < n-1; i++)	/*第i个消息已经被取走,后面的消息前移*/
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


	/*从消息队列读取消息*/
	ret = RecvMsg(GUN_MSG_TRAN, cmd, cmdlen, &indtyp, 2);    /*从消息队列中取值*/
	if(ret < 0) {
		if(ret == -2)	{                                  /*超时.将超时的indtyp放入共享内存.同时indtyp++*/
			RunLog("Recv Msg Timeout");
#if 0
			RunLog("CHN[%d] Recv Msg Timeout: %s(%d)", chnLog, Asc2Hex(cmd, *cmdlen), *cmdlen);
#endif

			Act_P(ERR_SEM_INDEX);

			/*取现有的超时indtyp值*/
			errcnt = gpGunShm->errCnt;                 /*第一个字节是现在总超时indtyp值*/
			if(errcnt < MAX_ERR_IND) {               /*超时键值未达到最大值.此时在最后一个的位置加上此超时键值*/
				gpGunShm->errInd[errcnt] = indtyp; /*超时的键值放在最后*/
				gpGunShm->retryTim[errcnt] = 0;
				gpGunShm->errCnt++;
			} else {                                   /*未处理的超时键值达到最大值*/
				/*在最前面的键值为最旧的键值.所有已超时键值前移*/
				for(i = 0; i < errcnt - 1; i++) {
					gpGunShm->errInd[i] = gpGunShm->errInd[i + 1];
					gpGunShm->retryTim[i] = gpGunShm->retryTim[i + 1];
				}
				gpGunShm->errInd[errcnt] = indtyp;	/*超时的键值放在最后*/

			}

			if(gpGunShm->useInd[chnLog - 1] == gpGunShm->maxInd[chnLog - 1])  {     /*到达最大值*/
				gpGunShm->useInd[chnLog - 1] = gpGunShm->minInd[chnLog - 1];
				DelErrInd(gpGunShm->minInd[chnLog - 1]); /*在超时的队列里查找是否有此超时键值.有则删掉*/
			} else {
				gpGunShm->useInd[chnLog - 1] += 1;
				if(gpGunShm->isCyc[chnLog - 1] == 1)	 /*indtyp已经循环过一次了*/
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
 * func: 判断是否为新版内核
 *       6.30试点所使用的内核为老内核, 新内核改进了掉电标记的检测处理, 
 *       并增加了网络状态的检测(检测是否有网线断开了连接), 因这两项改动无法以模块的形式实现, 
 *       所以, 只能是使用老内核(2.6.21)的系统使用原有处理方式,
 *       而新内核(2.6.21-fcc-1.0)使用新的处理方式
 * return: 0 - 老内核(kernel release ver 为 2.6.21), 1 - 新内核(2.6.21-fcc-1.0 或之后版本)
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
 * func: 处理一些琐事 (日志备份, 配置更新同步等)
 */

void Misc_Process()
{
	int cnt = 0;
	int cnt_tcc = 0;
	int i;
	gpIfsf_shm->cfg_changed = 0;

	while (1) {
		// 1. 每隔10s检查一次标记 cfg_changed
		sleep(10);
		cnt++;
		cnt_tcc++;

		
		//Update_Prod_No();
		
		if (0x0F == gpIfsf_shm->cfg_changed) {
			SyncCfg2File();
		}

		// 2. 每隔 10 分钟, 检查一次日志是否需要备份
		//if (cnt >= 60) {
		if (cnt >= 30) {
			cnt = 0;
			if (backup_log_file() < 0) {
				RunLog("exec backup_log_file failed");
			}

			// 2.2 校罐数据文件转储
			dump_tcc_adjust_data_file(1);
		}

		// 3. 每隔 1 分钟，检查一次数据采集启停文件标记
		if (cnt_tcc >= 6) {
			cnt_tcc = 0;
			RunLog("access if the file start/stop exsit");
			if ((access("/home/Data/log_backup/start", F_OK) == 0) &&
					    gpGunShm->data_acquisition_4_TCC_adjust_flag == 0) {
				// start文件存在,启动采集
				gpGunShm->data_acquisition_4_TCC_adjust_flag = 1;
				TCCADLog("TCC-AD: 数据采集服务启动, data_aq_4_tcc_adjust_flag: %d", 
								gpGunShm->data_acquisition_4_TCC_adjust_flag);
				// 记录当前各油机节点离在线情况, 0 - 离线, 1 - 在线
				for (i = 0; i <  MAX_CHANNEL_CNT; ++i) {
					if (gpIfsf_shm->node_channel[i].node == 0) {
						continue;
					}
					write_tcc_adjust_data(NULL, ",,%d,,,,,%d,,,",
							gpIfsf_shm->node_channel[i].node,
							(gpIfsf_shm->node_online[i] == 0 ? 0 : 1));
					TCCADLog("TCC-AD: Node %d %s", gpIfsf_shm->node_channel[i].node,
							(gpIfsf_shm->node_online[i] == 0 ? "离线" : "在线"));
				}
				// -- 液位仪
				write_tcc_adjust_data(NULL, ",,91,,,,,%d,,,",
						gpIfsf_shm->node_online[MAX_CHANNEL_CNT]);
				TCCADLog("TCC-AD: 液位仪%s", (gpIfsf_shm->node_online[MAX_CHANNEL_CNT] == 0 ? "离线" : "在线"));
			}
			if ((access("/home/Data/log_backup/stop", F_OK) == 0) &&
					     gpGunShm->data_acquisition_4_TCC_adjust_flag != 0) {
				// stop文件存在,停止采集,stop文件存在是才能停止,
				// start|stop都不存在时按之前状态继续执行
				gpGunShm->data_acquisition_4_TCC_adjust_flag = 0;
				TCCADLog("TCC-AD: 数据采集服务停止, data_aq_4_tcc_adjust_flag: %d",
								gpGunShm->data_acquisition_4_TCC_adjust_flag);
				// 立即转储校罐数据文件
				dump_tcc_adjust_data_file(0);
			}
		}
	}
}


/*
 * func: 打印全部dispenser的信息
 */
void show_all_dispenser()
{
	int i, j, k;
	FP_DATA *pfp;
	DISPENSER_DATA *pdsp;

	if (gpIfsf_shm == NULL) {
		RunLog("gpIfsf_shm 为空");
		return;
	}

	for (i = 0; i < MAX_CHANNEL_CNT; i++) {
       		if (gpIfsf_shm->convert_hb[i].lnao.node > 0) {
			show_dispenser(gpIfsf_shm->convert_hb[i].lnao.node);
		}
	}
}

/*
 * 打印一个 dispenser 的信息
 */
void show_dispenser(unsigned char node)
{
	int i, j, k;
	FP_DATA *pfp;
	DISPENSER_DATA *pdsp;

	i = 0;
	if (gpIfsf_shm == NULL) {
		RunLog("gpIfsf_shm 为空");
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
 *  打印PUMP的信息
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
 * func: 打印所有油枪的状态、油品、单价信息到关键日志文件critical.log
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
	unsigned char *fp_state[] = {"离线  ", "不可用", "锁定  ", "空闲  ",
			             "抬枪  ", "已授权", "      ", "      ", "加油中"};
	static time_t pre_price_change_start = 0; // 最近一次变价操作的起始时间

	RunLog("------------ call price _change_track %ld---------", pre_price_change_start);
	if (time(NULL) < (pre_price_change_start + 600)) {
		// 若收到变价命令距最近一次收到变价命令未超过10分钟, 则认为是同一次
		RunLog("---------- time:%ld less than pre + 600: %ld", time(NULL), pre_price_change_start);
		return 0;
	}

	// 一次新的变价操作, 打印所有油枪的状态以及油品、单价配置情况
	
	pre_price_change_start = time(NULL);
	CriticalLog("\n\n________________________________________________________________________________________________\n"); 

	CriticalLog("         < 当前油枪状态及其油品、单价配置情况 >  \n");
	for (i = 0; i < MAX_CHANNEL_CNT; ++i) {
		node = gpIfsf_shm->convert_hb[i].lnao.node;
		if (node == 0) { 
			// 该位置没有节点配置, 跳过
			continue;
		}

		// 每个Node的信息用空行分割
		if (gpIfsf_shm->node_online[i] == 0) {
			CriticalLog("节点[%02d]处于[离线]状态\n", node);
			continue;
		}

		pdsp = (DISPENSER_DATA *)&(gpIfsf_shm->dispenser_data[i]);
		fp_cnt = pdsp->fp_c_data.Nb_FP;
		for (fp_idx = 0; fp_idx < fp_cnt; ++fp_idx) {
			state_idx = gpIfsf_shm->node_online[i] == 0 ? 0 : pdsp->fp_data[fp_idx].FP_State;
			for (lnz_idx = 0; lnz_idx < (pdsp->fp_data[fp_idx].Nb_Logical_Nozzle); ++lnz_idx) {
				
				pr_no = pdsp->fp_ln_data[fp_idx][lnz_idx].PR_Id - 1;

				CriticalLog("节点[%02d]面[%d]枪[%d]状态[%s]油品号[%02X%02x%02x%02x]当前单价[%02x.%02x]", 
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
	CriticalLog("         < 变价命令下发执行情况 >  \n");

	return 0;
}


/*
 * 读取IO板的软件版本和口数, 版本号为两个字节长度,口数1字节
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
		RunLog( "打开串口%s失败", ttyname);
		return -1;
	}

	ret = WriteByLengthInTime(fd, io_cmd45, 2, timeout);
	if (ret < 0) {
		RunLog( "发1c45命令到串口 %d 失败", portnum);
		return -1;
	}

	memset(msg, 0, sizeof(msg));
	ret = ReadByLengthInTime(fd, msg, 2, timeout);
	if (ret < 0 || ret == 0) {
		RunLog( "接收1c45返回信息失败");
		return -1;
	}

	/*
	 * 1. 读取IO口数
	 * 2. 对于老版本(0201etc.)1C45命令没有结束命令处理流程的bug,
	 *  此命令兼避免1C命令没有结束导致不能通讯.
	 */
	ret = WriteByLengthInTime(fd, io_cmd50, 2, timeout);
	if (ret < 0) {
		RunLog( "发1c50命令到串口 %d 失败", portnum);
		return -1;
	} else {
		ret = ReadByLengthInTime(fd, buff, 1, timeout);
		if (ret < 0 || ret == 0) {
			RunLog( "接收1c50返回信息失败");
			return -1;
		}
		msg[2] = buff[0];
	}

	close(fd);

	return 0;
}

/*
 * 读取全部IO板的程序版本, 根据具体情况设置 wayne_rcap2l_flag标记位,
 * 稳牌模块据此来判断是否能够下发1c45命令修改IO中断周期,
 * 因为如果下发1c45给不支持该命令的IO板, 将导致IO通讯错误;
 *
 * 若IO程序版本不低于1006, 则 wayne_rcap2l_flag 置为0, 否则置为3.
 *
 * 另, 保存版本信息到文件 /var/run/io.ver. (Note! FCC版本信息收集工具用到该文件信息)
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
		 * 如果成功读到IO版本号, 并且版本号不小于 1006,
		 * 则表示IO程序支持命令设置RCAP2L
		 */
		if ((ret == 0) && ((msg[0] * 256 + msg[1]) >= 0x1006)) {
			ret = snprintf(buf + strlen(buf), 16, "<PORT%1d,%02x%02x,%02d>",
							portnum, msg[0], msg[1], msg[2]);
			continue;
		} else if (ret == 0) {
			ret = snprintf(buf + strlen(buf), 16, "<PORT%1d,%02x%02x,%02d>", 
							portnum, msg[0], msg[1], msg[2]);
			// IO程序不支持设置RCAP2L
			gpGunShm->wayne_rcap2l_flag = 3;   
			continue;
		} else {
			// IO程序不支持设置RCAP2L
			gpGunShm->wayne_rcap2l_flag = 3;   
			ret = snprintf(buf + strlen(buf), 16, "<PORT%1d,0000,%02d>", portnum, msg[2]);
			continue;
		}
	}

	RunLog("配置串口数[%d],IO版本[%s], wayne_flag[%d]",
		       	portCnt, buf, gpGunShm->wayne_rcap2l_flag);

	ret = write(fd, buf, strlen(buf));
	if (ret != strlen(buf)) {
		RunLog("写IO版本信息到io.ver失败[%s]", strerror(errno));
	}

	close(fd);

	if (3 == gpGunShm->wayne_rcap2l_flag) { 
		return 1;
	} else {
		return 0;
	}
}

/*
 * \brief  设置节点离在线状态
 * \param  node    节点号
 * \param  status  离线--0，在线-1
 * \return int     成功返回0，失败返回-1
 */
int ChangeNode_Status(int node, __u8 status)
{
	int i;

	if (gpIfsf_shm == NULL) {
		RunLog("连接共享内存错误");
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
			TCCADLog("TCC-AD: 节点 %d %s", node, (status == NODE_OFFLINE ? "离线" : "联线"));
		}
		return 0;
	}

	return -1;
}

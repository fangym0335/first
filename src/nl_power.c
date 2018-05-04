#include "power.h"
#include "ifsf_def.h"
#include "ifsf_pub.h"
#include "log.h"
#include "pub.h"
#include "dofile.h"

#define TR_BUF_SIZE	(sizeof(FP_TR_DATA) * FP_TR_CONVERT_CNT)

extern IFSF_SHM *gpIfsf_shm;
//extern IFSF_FP_SHM *gpIfsf_shm;
char gsFlashName[50];

static int GetFlashName( ){

	char	*pFilePath;

#if 0
	pFilePath = getenv( "DATA_DIR" );	/*WORK_DIR 工作目录的环境变量*/
	if( pFilePath == NULL ) {
		UserLog( "未定义环境变量DATA_DIR" );
		return -1;
	}

#endif
	//取配置文件名*/
	//if( strlen( pFilePath ) + strlen( NAND_FLASH_FILE ) > sizeof(gsFlashName)-1 )//
	if( strlen(NAND_FLASH_FILE ) > sizeof(gsFlashName)-1 )//
	{
		UserLog( "文件名（包含绝对路径）过长。请调整不要超过[%d]字节\n ", sizeof(gsFlashName)-1 );
		return -1;
	}
//	sprintf(gsFlashName, "%s%s", pFilePath,   NAND_FLASH_FILE);//ETCFILE
	sprintf(gsFlashName, "%s", NAND_FLASH_FILE);//ETCFILE
	
	return 0;

}

//stop all ifsf fork.
static int StopProc(){
	int i,ret;
	sigset_t set;
	sigset_t oset;
	Pid *PidList ;
	const union sigval sv = {0};
	extern Pid *gPidList;
	extern unsigned char cnt;

	PidList = gPidList;
	gpGunShm->sysStat = STOP_PROC;
	sigemptyset(&set);
	sigaddset(&set, SIGCHLD);
	sigprocmask(SIG_BLOCK, &set, &oset);
	for (i = 0; i < cnt; i++, PidList++) {
		ret = sigqueue(PidList->pid, SIGSTOP, sv);
		if (ret != 0) {
		       	sigqueue(PidList->pid, SIGSTOP, sv);
		}
		RunLog("PidList->pid = %d\n", PidList->pid);
	}

	return 0;
}

/*
 * func: backup unpaid transactions to file at flash
 */
int WriteNandFlash()
{	
	int fd;
	int i, j, k;
	int trynum = 10;
	size_t ret;
	FP_TR_DATA *pfp_tr_data;
	FP_TR_DATA *mstart;
	BAK_LNZ_TOTAL bak_lnz_total;
	BAK_TR_SEQ_NB bak_tr_seq_nb;
	
	if( NULL == gpIfsf_shm ){		
		RunLog("gpIfsf_shm is NULL, Exec WriteNandFlash failed");
		return -1;
	}

	// Open or create a empty file in nand flash
	fd = open( gsFlashName, O_WRONLY | O_CREAT | O_TRUNC, 00644 );
	if( fd < 0 ) {
		RunLog( "Open data file[%s] failed.", gsFlashName );
		return -1;
	}		

#if 0
	// map data file into share memory
	ret = lseek(fd, TR_BUF_SIZE - 1, SEEK_SET);	
	if (ret < 0) {
		close(fd);
		return -1;
	}
	ret = write(fd, "", 1);
	if (ret != 1) {
		close(fd);
		return -1;
	}
	do {
		mstart = (FP_TR_DATA *)mmap(&gpIfsf_shm->fp_tr_data, 
			TR_BUF_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	} while (mstart == MAP_FAILED && (--trynum) != 0);
	if (trynum == 0) {
		mstart = NULL
		RunLog("map data file[%s] into shm failed, errno[%s]", gsFlashName, strerror(errno));
		close(fd);
		return -1;
	}

	// sync data file with share memory
	trynum = 10;
	do {
		ret = msync(mstart, TR_BUF_SIZE, MS_SYNC);
	} while (ret != 0 && (--trynum) != 0);

	if (trynum == 0) {
		RunLog("sync data file[%s] with shm failed, errno[%s]", gsFlashName, strerror(errno));
		close(fd);
		return -1;
	}
#endif
	for (trynum = 10; trynum > 0; trynum--) {
		ret = write(fd, gpIfsf_shm->fp_tr_data, TR_BUF_SIZE);
		if (ret != TR_BUF_SIZE) {
			lseek(fd, 0, SEEK_SET);
			continue;
		}
		break;
	}
	RunLog("Backup.1 备份未支付交易.............................成功");

	close(fd);

	if (trynum == 0) {
		RunLog("备份未上传交易失败!");
		return -1;
	}
	
	RunLog("备份未上传交易成功!");
 
	ret = backupPowerOff_log_file();
	if (ret < 0){
		RunLog( "掉电时,备份日志失败!");
		return -1;	   
	}
	
	return 0;
}

int ReadDelNandFlash()
{
	int fd;
	int i;
	int n;	
	int ret;
	unsigned char buff[128];
	FP_TR_DATA *pfp_tr_data;
	/*
	ret = GetFlashName();
	if(ret < 0)
		return -1;
	*/
	/*打开文件*/
	fd = open( gsFlashName,  O_RDONLY  );
	if( fd < 0 )
	{
		//UserLog( "打开配置文件[%s]失败.", gsFlashName );
		return  1;
	}		
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,交易数据WriteNandFlash  失败");
		return -1;
	}
	
	while( 1 ) {
		//n = read( fd, buff, GUN_CFG_LEN );
		n = read( fd, buff, sizeof(FP_TR_DATA) );
		
		if( n == 0 )		/*文件结束*/
			break;
		else if( n < 0 )	/*出错*/
		{
			UserLog( "读取配置文件出错" );
			close(fd);
			return -1;
		}
		if( n != sizeof(FP_TR_DATA) )//GUN_CFG_LEN
		{
			UserLog( "配置信息有误" );
			close( fd );
			return -1;
		}
		
		pfp_tr_data = (FP_TR_DATA *)(&buff[0]);
		ret = AddTR_DATA(pfp_tr_data);
		if(ret <0 ){
			UserLog( "NAND FLASH交易数据读取有误" );
			close( fd );
			exit(0);
		}
	}	

	close(fd);
	remove(gsFlashName);
	UserLog( "处理数据成功了" );

	return 0;
	
}

/*
 *掉电保护服务,在掉电的情况下,把ifsf_fp_shm数据区的交易数据写入NAND FLASH中,
 *ifsf_fp_shm数据区的交易数据是重要的加油机数据区.
 *PowerFailureServ用阻塞读判断掉电中断.
 */

int PowerFailureServ( )
{
	int ret, trytime;	
	int num;
	static int fail_times = 0;
	static int current_status = 1;
	int dst_addr_len;
	int sock_fd;
	struct sockaddr_nl usr_addr, kernel_addr;
	MSG_PFS_TO_KERNEL msg_to_kernel;
	PWMSG pwmsg_from_kernel;

#ifdef OLD_PWF
	while (1);
#endif
	
	sleep(10); //防止提前退出
	UserLog("Power Failure Server");
	ret = GetFlashName();
	if(ret < 0){
		UserLog("GetFlashName error!!");
		return -1;
	}
		
	ret = 0;

	// ------------------------- Test 7.29 Using netlink----------------//

	sock_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_POWERDOWN);
	if (sock_fd < 0) {
		RunLog("Power failured server allocate socket failed");
		return -1;
	}
	
	// src addr init
	memset(&usr_addr, 0, sizeof(usr_addr));
	usr_addr.nl_family  = AF_NETLINK;
	usr_addr.nl_pid = getpid();             // self pid
	usr_addr.nl_groups = 0;                 // not in mcast groups

	if (bind(sock_fd, (struct sockaddr*)&usr_addr, sizeof(usr_addr)) != 0 ) {
		RunLog("Bind usr_addr to sock_fd failed");
		goto FAIL_EXIT; 
	}

	// dst addr init
	memset(&usr_addr, 0, sizeof(usr_addr));
	kernel_addr.nl_family  = AF_NETLINK;
	kernel_addr.nl_pid = 0;                  // For Linux kernel
	kernel_addr.nl_groups = 0;               // unimcast 

	// Fill Msg
	msg_to_kernel.hdr.nlmsg_len = NLMSG_LENGTH(0);   // Just a request, no additional data
	msg_to_kernel.hdr.nlmsg_flags = 0;
	msg_to_kernel.hdr.nlmsg_type = POWER_STATUS;
	msg_to_kernel.hdr.nlmsg_pid =  usr_addr.nl_pid;  // sender's pid

	// send msg to kernel
	
	ret = sendto(sock_fd, &msg_to_kernel, msg_to_kernel.hdr.nlmsg_len, 0,
				(struct sockaddr*)&kernel_addr, sizeof(kernel_addr));
	if (ret < 0) {
		RunLog("Send message to kernel failed, errno: %s", strerror(errno));
		goto FAIL_EXIT; 
	}

	
	while (1) {
		gpGunShm->sysStat = RUN_PROC;
		dst_addr_len = sizeof(struct sockaddr_nl);
		ret = recvfrom(sock_fd, &pwmsg_from_kernel, sizeof(pwmsg_from_kernel), 0,
					(struct sockaddr*)&kernel_addr, &dst_addr_len);
		if (ret < 0) {
			RunLog("Recv message from kernel error!");
			goto FAIL_EXIT; 
		}

		if (pwmsg_from_kernel.pstatus != 0) {
			continue;
		} 

		RunLog("Power failure......................");
#if 0
 	// for LB011 station
	while (1) {
		if ((ret = power_status()) == 0) {
			RunLog("Warning!, power status is : %d ", ret);
		}
	}
#endif

		trytime = 3;		
		ret = StopProc();
		ret = WriteNandFlash();
		if(ret > 0){
			RunLog("Power Failure!! Write to Nand Flash finsished!!");
		}

		while( (ret < 0) && (trytime--) ){
			ret = WriteNandFlash();
			if(ret > 0){
				RunLog("Power Failure!! Write to Nand Flash finsished!!");
			}
		}
		if(trytime <=0 ) {
			RunLog("WriteNandFlash error!! lost data!!");
			continue;
		}
		continue;
	}
	
FAIL_EXIT:
	close(sock_fd);	
	return -1;
}


/*
 * func: 恢复掉电时备份的交易数据到脱机交易文件
 * return: 1 - 没有交易需要恢复, 0 - 成功恢复, -1 -  失败
 */
int RecoveryTrAsOffline()
{
	int fd_src, fd_dst;
	int i, j;
	int trynum = 10;
	size_t ret;
	__u8 node;
	__u8 filename[128];
	FP_TR_DATA *pfp_tr_data;

	RunLog("## Do Recovery Transaction As offline.....");
	if (NULL == gpIfsf_shm) {
		UserLog("gpIfsf_shm is NULL, Transaction Recovery As Offline Failed");
		return -1;
	}
	fd_src = open(gsFlashName, O_RDONLY | O_CREAT | O_EXCL, 00644);
	if (fd_src != -1) {
		RunLog("无掉电备份数据");
		return 1;
	}

	fd_src = open(gsFlashName, O_RDONLY);
	if (fd_src < 0) {
		RunLog( "Open data file[%s] failed", gsFlashName);
		return -1;
	}

	// Step.1 恢复掉电前交易缓冲区内容到交易缓冲区
	for (trynum = 10; trynum > 0; trynum--) {
		ret = read(fd_src, gpIfsf_shm->fp_tr_data, TR_BUF_SIZE);
		if (ret == 0) {          // EOF
			RunLog("无掉电备份数据!");
			break;
		} else if (ret < 0) {    // ERROR
			continue;
		}

		if (ret != TR_BUF_SIZE) {
			RunLog("从[%s]读取的交易数据不完整!!", gsFlashName);
		}
		break;
	}

	if (trynum == 0) {
		RunLog("读取数据文件[%s]出错!", gsFlashName);
	}

	// Step.2 处理未支付交易
	for (i = 0; i < MAX_CHANNEL_CNT * MAX_TR_NB_PER_DISPENSER; ++i) {
		pfp_tr_data = &gpIfsf_shm->fp_tr_data[i];
		if (pfp_tr_data->Trans_State == 0) {
			continue;
		}
		node = pfp_tr_data->node;

		sprintf(filename, "%s%02x", TEMP_TR_FILE, node);
		fd_dst = open(filename, O_RDWR | O_CREAT | O_APPEND, 00644);
		if (fd_dst < 0) {
			RunLog("Open/Create file[%s] failed", filename);
			continue;
		}

		// 将该node所有交易写入文件
		for (j = i; j < MAX_CHANNEL_CNT * MAX_TR_NB_PER_DISPENSER; ++j) {
			if (((pfp_tr_data->Trans_State == PAYABLE ||
				pfp_tr_data->Trans_State == LOCKED ||
				pfp_tr_data->offline_flag != 0)) &&
						pfp_tr_data->node == node) {

				RunLog(">>>> tr_state: %02x, offline_flag: %02x, node: %02x", pfp_tr_data->Trans_State, 
						pfp_tr_data->offline_flag, pfp_tr_data->node);
				ret = write(fd_dst, pfp_tr_data, sizeof(FP_TR_DATA));
				if (ret != sizeof(FP_TR_DATA)) {
					ret = write(fd_dst, pfp_tr_data, sizeof(FP_TR_DATA));
					if (ret != sizeof(FP_TR_DATA)) {
						RunLog("write TR[%s] to file failed", 
							Asc2Hex((__u8 *)pfp_tr_data, sizeof(FP_TR_DATA)));
					}
				}
				pfp_tr_data->Trans_State = CLEARED;
				pfp_tr_data->offline_flag = 0;
			}
		}
	}

	close(fd_src);
	close(fd_dst);
	remove(gsFlashName);
	RunLog("掉电备份数据恢复成功!");

	return 0;
}

/*
 * func: 恢复油枪泵码到已经初始化好的共享内存
 */

int RecoveryVolTotal()
{
	int i;
	int fd;
	int ret;
	int err_cnt = 0;
	__u8 node;
	BAK_LNZ_TOTAL bak_lnz_total;

	if (NULL == gpIfsf_shm) {
		UserLog("gpIfsf_shm is NULL, Transaction Recovery As Offline Failed");
		return -1;
	}

	fd = open(BAK_LNZ_TOTAL_FILE, O_RDONLY | O_CREAT | O_EXCL, 00644);
	if (fd != -1) {
		RunLog("Recovery. Has no %s", BAK_LNZ_TOTAL_FILE);
		remove(BAK_LNZ_TOTAL_FILE);
		return 1;
	}

	// open pwf_lnz_total.dat
	fd = open(BAK_LNZ_TOTAL_FILE, O_RDONLY);
	if( fd < 0 ) {
		RunLog( "open data file[%s] failed.", BAK_LNZ_TOTAL_FILE);
		return -1;
	}		

	// recovery totals to shm
	while (1) {
		ret = read(fd, &bak_lnz_total, sizeof(BAK_LNZ_TOTAL));
		if (ret == 0) {
			break;
		} else if (ret != sizeof(BAK_LNZ_TOTAL)) {
			lseek(fd, 0, SEEK_SET);
			if (err_cnt < 5) {
				err_cnt++;
				continue;
			} else {
				RunLog("读取油枪泵码备份值失败");
				return -1;
			}
		}
		err_cnt = 0;

		node = bak_lnz_total.node;
		for (i = 0; i < MAX_CHANNEL_CNT; ++i) {
			if (node == gpIfsf_shm->convert_hb[i].lnao.node) {
				break;
			}
		}

		if (i == MAX_CHANNEL_CNT) {
			RunLog("can not foud this node in shm");
		}

		memcpy(&gpIfsf_shm->dispenser_data[i].fp_ln_data[bak_lnz_total.fp][bak_lnz_total.lnz],
							&(bak_lnz_total.Log_Noz_Vol_Total), 7);
	}

	// 关闭 删除备份文件
	close(fd);
	remove(BAK_LNZ_TOTAL_FILE);

	return 0;
}

/*
 * func: 恢复交易序列号到已经初始化好的共享内存
 * return: 1 - 没有数据需要恢复, 0 - 成功恢复, -1 -  失败
 */

int RecoveryTrSeqNb()
{
	int i;
	int fd;
	int ret;
	int err_cnt = 0;
	__u8 node;
	BAK_TR_SEQ_NB bak_tr_seq_nb;

	if (NULL == gpIfsf_shm) {
		UserLog("gpIfsf_shm is NULL, Transaction Recovery As Offline Failed");
		return -1;
	}

	fd = open(BAK_TR_SEQ_NB_FILE, O_RDONLY | O_CREAT | O_EXCL, 00644);
	if (fd != -1) {
		RunLog("Recovery. Has no %s", BAK_TR_SEQ_NB_FILE);
		remove(BAK_TR_SEQ_NB_FILE);
		return 1;
	}

	// open pwf_tr_seq_nb.dat
	fd = open(BAK_TR_SEQ_NB_FILE, O_RDONLY);
	if( fd < 0 ) {
		RunLog( "open data file[%s] failed.", BAK_TR_SEQ_NB_FILE);
		return -1;
	}		

	// recovery tr_seq_nb to shm
	while (1) {
		ret = read(fd, &bak_tr_seq_nb, sizeof(BAK_TR_SEQ_NB));
		if (ret == 0) {
			break;
		} else if (ret != sizeof(BAK_TR_SEQ_NB)) {
			lseek(fd, 0, SEEK_SET);
			if (err_cnt < 5) {
				err_cnt++;
				continue;
			} else {
				RunLog("读取交易序列号备份值失败");
				return -1;
			}
		}
		err_cnt = 0;

		node = bak_tr_seq_nb.node;

		for (i = 0; i < MAX_CHANNEL_CNT; ++i) {
			if (node == gpIfsf_shm->convert_hb[i].lnao.node) {
				break;
			}
		}

		if (i == MAX_CHANNEL_CNT) {
			RunLog("can not foud this node in shm");
		}

		memcpy(&gpIfsf_shm->dispenser_data[i].fp_data[bak_tr_seq_nb.fp].Transaction_Sequence_Nb, 
								&bak_tr_seq_nb.Transaction_Sequence_Nb, 2);
	}

	// 关闭 删除备份文件
	close(fd);
	remove(BAK_TR_SEQ_NB_FILE);

	return 0;
}



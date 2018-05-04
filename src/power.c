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
	pFilePath = getenv( "DATA_DIR" );	/*WORK_DIR ����Ŀ¼�Ļ�������*/
	if( pFilePath == NULL ) {
		UserLog( "δ���廷������DATA_DIR" );
		return -1;
	}

#endif
	//ȡ�����ļ���*/
	//if( strlen( pFilePath ) + strlen( NAND_FLASH_FILE ) > sizeof(gsFlashName)-1 )//
	if( strlen(NAND_FLASH_FILE ) > sizeof(gsFlashName)-1 )//
	{
		UserLog( "�ļ�������������·�����������������Ҫ����[%d]�ֽ�\n ", sizeof(gsFlashName)-1 );
		return -1;
	}
//	sprintf(gsFlashName, "%s%s", pFilePath,   NAND_FLASH_FILE);//ETCFILE
	sprintf(gsFlashName, "%s", NAND_FLASH_FILE);//ETCFILE
	
	return 0;

}

//Kill all ifsf fork.
static int StopProc()
{
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
		if (sigqueue(PidList->pid, SIGKILL, sv) != 0 ) {
		       	sigqueue(PidList->pid, SIGKILL, sv);
		}
//		RunLog("PidList->pid = %d\n", PidList->pid);
	}

	RunLog("Killed all child process\n");

	return 0;
}

/*
 * func: backup unpaid transactions to file at flash
 */
int WriteNandFlash()
{	
	int fd;
	int trynum;
	size_t ret;
	FP_TR_DATA *pfp_tr_data;
//	FP_TR_DATA *mstart;
//	BAK_LNZ_TOTAL bak_lnz_total;
//	BAK_TR_SEQ_NB bak_tr_seq_nb;
	
	// Open or create a empty file in nand flash
	fd = open(gsFlashName, O_WRONLY | O_CREAT | O_TRUNC, 00644 );
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
	RunLog("Backup.1 ����δ֧������.............................�ɹ�");

	close(fd);

	if (trynum == 0) {
		RunLog("����δ�ϴ�����ʧ��!");
		return -1;
	}
	
	RunLog("����δ�ϴ����׳ɹ�!");
 
	ret = backupPowerOff_log_file();
	if (ret < 0){
		RunLog( "����ʱ,������־ʧ��!");
		return 1;	   
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
	/*���ļ�*/
	fd = open( gsFlashName,  O_RDONLY  );
	if( fd < 0 )
	{
		//UserLog( "�������ļ�[%s]ʧ��.", gsFlashName );
		return  1;
	}		
	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,��������WriteNandFlash  ʧ��");
		return -1;
	}
	
	while( 1 ) {
		//n = read( fd, buff, GUN_CFG_LEN );
		n = read( fd, buff, sizeof(FP_TR_DATA) );
		
		if( n == 0 )		/*�ļ�����*/
			break;
		else if( n < 0 )	/*����*/
		{
			UserLog( "��ȡ�����ļ�����" );
			close(fd);
			return -1;
		}
		if( n != sizeof(FP_TR_DATA) )//GUN_CFG_LEN
		{
			UserLog( "������Ϣ����" );
			close( fd );
			return -1;
		}
		
		pfp_tr_data = (FP_TR_DATA *)(&buff[0]);
		ret = AddTR_DATA(pfp_tr_data);
		if(ret <0 ){
			UserLog( "NAND FLASH�������ݶ�ȡ����" );
			close( fd );
			return -1;
		}
		
	}	

	close(fd);
	remove(gsFlashName);
	UserLog( "�������ݳɹ���" );

	return 0;
	
}

/*
 * func: �����ں˰汾���ѡ���������籣������
 */
int StartPowerFailureServ()
{
	int ret;

	if (gpIfsf_shm->is_new_kernel == 1) {
		ret = PowerFailureServ_Netlink();
	} else {
		ret = PowerFailureServ_Syscall();
	}

	return ret;
}

/*
 *���籣������,�ڵ���������,��ifsf_fp_shm�������Ľ�������д��NAND FLASH��,
 *ifsf_fp_shm�������Ľ�����������Ҫ�ļ��ͻ�������.
 *PowerFailureServ���������жϵ����ж�.
 *
 * <<< �ں�ʹ��netlink��ʽ֪ͨӦ�õ��� >>
 */

int PowerFailureServ_Netlink()
{
	int ret;
	int trytime = 3;    // ��ǰ��ʼ����
	int fail_cnt = 0;
	int dst_addr_len;
	int sock_fd;
	time_t time_power_failure = 0;
	struct sockaddr_nl usr_addr, kernel_addr;
	MSG_PFS_TO_KERNEL msg_to_kernel;
	PWMSG pwmsg_from_kernel;

	
	sleep(10); //��ֹ��ǰ�˳�
	ret = GetFlashName();
	if(ret < 0){
		UserLog("GetFlashName error!!");
		return -1;
	}
		
	ret = 0;
	UserLog("Power Failure Server Started(NL)");

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

		// 1
		if (pwmsg_from_kernel.pstatus != 0) {
			continue;
		} 
		RunLog("Power failure.0......................time_pwf: %ld", time_power_failure);

#if 1
		/* ���⴦�� Guojie zhu @ 6.27
		 * ��ֹ˲��������� , �����ݲ�����
		 * Note: ��һЩվ�������ϴ��ڵ����ź�
		 */	
		while ((ret = power_status()) == 0) {
			RunLog("#### power status is : %d , time_pwf: %ld",
						ret, time_power_failure);
			/* ���������ֻ�ڿ�ʼʱ(4s֮��)���б��ݴ���,
			 * �����ڽ�Ҫ��ȫ�ϵ�ʱ����дflash����, ������ݴ�����ļ���;
			 */
			if (time_power_failure == 0) {
				time_power_failure = time(NULL);
				WriteNandFlash();
				backupPowerOff_log_file();
				RunLog("-=-=-=--------------------------------------0");
			} else if (time(NULL) <= time_power_failure + 4) {
				backupPowerOff_log_file();
				RunLog("-=-=-=--------------------------------------1");
			} 
			Ssleep(500000);
		};

		time_power_failure = 0;
		RunLog("Power resume.......................time_pwf: %ld", time_power_failure);
		continue;
#endif

		// 2
		Ssleep(400000);  // 400ms
		if (power_status() == 1) {
			RunLog("Power resume.0......................");
			continue;
		} 
		RunLog("Power failure.1......................");

		// 3
		Ssleep(400000);  // 400ms
		if (power_status() == 1) {
			RunLog("Power resume.1......................");
			continue;
		}
		RunLog("Power failure.2......................");

		StopProc();
		if (WriteNandFlash() == 0) {
			RunLog("Power Failure!! backup finsished!!");
			close(sock_fd);
			return 0;
		} else {
			while(trytime--){
				if (WriteNandFlash() == 0) {
					RunLog("Power Failure!! backup finsished!!");
					close(sock_fd);
					return 0;
				}
			}
		}
	}
	
FAIL_EXIT:
	RunLog("End of PowerFailureServ_Netlink, exit");
	close(sock_fd);	
	return -1;
}


/*
 *���籣������,�ڵ���������,��ifsf_fp_shm�������Ľ�������д��NAND FLASH��,
 *ifsf_fp_shm�������Ľ�����������Ҫ�ļ��ͻ�������.
 *�����ж���Ӧ�ó������ݽ���ʹ��syscall����power_failure(),
 *PowerFailureServ���������жϵ����ж�.
 *
 * << Ӧ��ʹ��ϵͳ���ò�����ѯ������ >>
 */
int PowerFailureServ_Syscall( )
{
	int ret;
       	int trytime = 3;
	time_t time_power_failure = 0;
	static int fail_times = 0;
	static int current_status = 1;
	
	sleep(10); //��ֹ��ǰ�˳�
	ret = GetFlashName();
	if(ret < 0){
		UserLog("GetFlashName error!!");
		return -1;
	}
		
	ret = 0;
	UserLog("Power Failure Server Started (SC)");

#if 1
	/* ���⴦�� Guojie zhu @ 6.27
	 * ��ֹ˲��������� , �����ݲ�����
 	 * Note: ��һЩվ�������ϴ��ڵ����ź�
	 */	
	while (1) {
		Ssleep(500000);
		if ((ret = power_status()) == 0) {
			RunLog("Warning!, power status is : %d , time_pwf: %ld", ret, time_power_failure);
			if (time_power_failure == 0) {
				/* ���������ֻ�ڿ�ʼʱ(4s֮��)���б��ݴ���,
				 * �����ڽ�Ҫ��ȫ�ϵ�ʱ����дflash����, ������ݴ�����ļ���;
				 */
				time_power_failure = time(NULL);
				backupPowerOff_log_file();
				WriteNandFlash();
				RunLog("-=-=-=--------------------------------------0");
			} else if (time(NULL) <= time_power_failure + 4) {
				backupPowerOff_log_file();
				RunLog("-=-=-=--------------------------------------1");
			} else if (time(NULL) >= time_power_failure + 30) {
				time_power_failure = 0;
			}
		}
	}
#endif

	while (1) {
		while (current_status == 0) {
			if (power_status() == 1) {
				gpGunShm->sysStat = RUN_PROC;
				current_status = 1;
				break;
			}
		}

		do {
			if (power_status() == 0) {
				fail_times++;
				RunLog("Power Failure, fail_times: %d", fail_times);
			} else {
				fail_times = 0;
			}
			Ssleep(400000);    // ���400ms
		} while(fail_times < 3);  //wait for power failure. 3��ȷ��Ϊ����

		current_status = 0;  //power failure
		fail_times = 0;

		StopProc();
		if (WriteNandFlash() == 0) {
			RunLog("Power Failure!! backup finsished!!");
			return 0;
		} else {
			while(trytime--) {
				if (WriteNandFlash() == 0) {
					RunLog("Power Failure!! backup finsished!!");
					return 0;
				}
			}
		}

#if 0
		if(trytime <=0 ) {
			RunLog("WriteNandFlash error!! lost data!!");
			continue;
		}
		//UserLog("Power Failure!! Write to Nand Flash!!");
		continue;
#endif
	}
	
	return 0;
}

/*
 * func: �ָ�����ʱ���ݵĽ������ݵ��ѻ������ļ�
 * return: 1 - û�н�����Ҫ�ָ�, 0 - �ɹ��ָ�, -1 -  ʧ��
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
		RunLog("�޵��籸������");
		return 1;
	}

	fd_src = open(gsFlashName, O_RDONLY);
	if (fd_src < 0) {
		RunLog( "Open data file[%s] failed", gsFlashName);
		return -1;
	}

	// Step.1 �ָ�����ǰ���׻��������ݵ����׻�����
	for (trynum = 10; trynum > 0; trynum--) {
		ret = read(fd_src, gpIfsf_shm->fp_tr_data, TR_BUF_SIZE);
		if (ret == 0) {          // EOF
			RunLog("�޵��籸������!");
			break;
		} else if (ret < 0) {    // ERROR
			lseek(fd_src, 0, SEEK_SET);  // ��λ�ļ�ָ��,��readʧ�ܺ�ָ��λ���ǲ�ȷ����, jie@8.19
			continue;
		}

		if (ret != TR_BUF_SIZE) {
			RunLog("��[%s]��ȡ�Ľ������ݲ�����!!", gsFlashName);
		}
		break;
	}

	if (trynum == 0) {
		RunLog("��ȡ�����ļ�[%s]����!", gsFlashName);
	}

	// Step.2 ����δ֧������
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

		// ����node���н���д���ļ�
		for (j = i; j < MAX_CHANNEL_CNT * MAX_TR_NB_PER_DISPENSER; ++j) {
			if (((pfp_tr_data->Trans_State == PAYABLE ||
				pfp_tr_data->Trans_State == LOCKED ||
				pfp_tr_data->offline_flag != 0)) &&
						pfp_tr_data->node == node) {

				RunLog(">>>> tr_state: %02x, offline_flag: %02x, node: %02x", pfp_tr_data->Trans_State, 
						pfp_tr_data->offline_flag, pfp_tr_data->node);
				ret = write(fd_dst, pfp_tr_data, sizeof(FP_TR_DATA));
				if (ret != sizeof(FP_TR_DATA)) {
					lseek(fd_dst, 0, SEEK_SET);
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
	RunLog("���籸�����ݻָ��ɹ�!");

	return 0;
}

/*
 * func: �ָ���ǹ���뵽�Ѿ���ʼ���õĹ����ڴ�
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
				RunLog("��ȡ��ǹ���뱸��ֵʧ��");
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

	// �ر� ɾ�������ļ�
	close(fd);
	remove(BAK_LNZ_TOTAL_FILE);

	return 0;
}

/*
 * func: �ָ��������кŵ��Ѿ���ʼ���õĹ����ڴ�
 * return: 1 - û��������Ҫ�ָ�, 0 - �ɹ��ָ�, -1 -  ʧ��
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
				RunLog("��ȡ�������кű���ֵʧ��");
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

	// �ر� ɾ�������ļ�
	close(fd);
	remove(BAK_TR_SEQ_NB_FILE);

	return 0;
}




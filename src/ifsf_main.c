#include "ifsf_main.h"
#include "tank_pub.h"
#include "power.h"
#include "power.c"


static int StartProc();
static void WhenSigChld( int sig );
static int WaitMyChild( /*int sig */);
static int save_version();
//static void Stop(  int sig  );
//static int AddChannelHB(int chnNo); //ͨ����������

/*-------- ȫ�ֱ����� --------------*/
unsigned char cnt = 0;	/*��������*/
Pid*	gPidList;
TLG_DATA *  pub_tk;
char	isChild = 0;
unsigned char cd_type = 0;     // CD ����, 1 - ��׼IFSF CD; 0 - ����IFSF CD
extern int errno;
int HB_PORT;


/*��ʼ�������ڴ�gpIfsf_shm,*/
int InitIfsfShm(){
	int i;
	if(gpIfsf_shm != NULL ) {
		memset(gpIfsf_shm,  0,  sizeof( IFSF_SHM ) );
		for(i=0; i<MAX_RECV_CNT; i++)
			gpIfsf_shm->recvUsed[i] = MAX_NO_RECV_TIMES - 1;
	} else {
		return -1;
	}

	return 0;
}

int InitUDPport(){
	
	FILE *f;
	char *UDPPORT_FILE = "/home/App/ifsf/etc/udpport.cfg";
	char my_data[128];
	int c, i, ret;
	
	f = fopen( UDPPORT_FILE, "rt" );
	if( !f  ){	
		char pbuff[256];
		bzero(pbuff,sizeof(pbuff));
		sprintf( pbuff, "file[%s] open error in readIni!! ", UDPPORT_FILE );
		return -3;
	}	
	for(i=0;i<sizeof(my_data)-1;i++){
		c = fgetc( f);
			
		if( c == EOF)
			break;
		my_data[i] = c;
	}
	my_data[i] = 0;	
	if(i==127){
		RunLog("udpport.cfg is NULL ,get a default udpport 3486");
		HB_PORT = 3486;
	}else{
		HB_PORT = atoi(my_data);
	}

	RunLog("HB_PORT is %d\n",HB_PORT );
	
	fclose( f );

	return 0;
}

int InitCfg(cfg_opt *cfgopt)
{
	init_cfg initcfg[32];
	
	ReadAllCfg( initcfg);
	if ((cfgopt->if_open_dlb = ReadCfgByName(initcfg, "IF_OPEN_DLB") ) == -1){
		RunLog("open dlb will be set as defult value 0");
		cfgopt->if_open_dlb =0;
	}
	if ((cfgopt->if_open_leak = ReadCfgByName(initcfg, "IF_OPEN_DLB") ) == -1){
		RunLog("open leak will be set as defult value 0");
		cfgopt->if_open_leak =0;
	}
	
	return 0;
}
int main(int argc, char *argv[])
{
	int ret;
	if (argc == 2) {
	       if (strcmp(argv[1], "-v") == 0) {
			printf( "Ifsf_main. FCC version %s\n", PCD_VER);
			return 0;
	       } else if (strcmp(argv[1], "--version") == 0) {
			printf( "Ifsf_main. FCC version %s\n", PCD_VERSION);
			return 0;
	       } else if (strcmp(argv[1], "--std") == 0) {
			cd_type = 1;
	       } else {
		       printf("Usage: ifsf_main [-v | --std]\n");
		       return -1;
	       }
	}
	system("/usr/local/bin/delogd&");
	//2007.07.13 change oil_main to ifsf_main, and change frame, share memory, etc...
	
	InitMemCtl();  //this function isn't  changed. 
	
	
	ret = InitLog(); //this function is'nt changed.
	if (ret < 0) {
		UserLog( "��ʼ����־�ļ�ʧ��" );
		return -1;
	}

	ret = InitKeyFile(); //this function is'nt changed.
	if (ret < 0) {
		UserLog( "��ʼ��Key Fileʧ��" );
		return -1;
	}
	//ret = SaveWorkDir();//���滷������WORK_DIR������ʱ�ļ�: /var/run/ifsf_main.wkd. move it into InitAll()
	//ret = InitCfg();	/*ȡ������Ϣ*/  //this function is  changed.
	ret = InitPubCfg();
	if (ret < 0) {
		UserLog( "��ʼ�����ò���ʧ��");
		return -1;
	}
	
	RunLog(""); // newline
	RunLog("IFSF Main ( %s ) Startup ................ ", PCD_VERSION);

	ret = InitUDPport();
	if(ret < 0){
		
	}
	if (save_version() < 0) {
		RunLog("�������а汾��Ϣ��ifsf_main.verʧ��");
		return -1;
	}

	while( 1 ) {
		ret = StartProc(); //this function is changed!!
		if( ret < 0 ) {
			UserLog( "��������ʧ��.������־" );
			return -1;
		}

		if( gpGunShm->sysStat == STOP_PROC ) {
			UserLog( "ϵͳ��ֹͣ.ллʹ��" );
			break;
		}
	}

	DetachShm( (char**)(&gpIfsf_shm) );
	DetachShm( (char**)(&gpGunShm) );
	UserLog( "�������˳�.������־" );
	
	return 0;

}

/*�����������еķ������*/
static int StartProc()
{
	int ret;
	int i;
	int n;
	pid_t	pid;
	char flag[4];

	while(1) {
		if (!GetCfgMode()) { // for station test 2017-03-22
			UserLog("Get dsp.cfg =====================");
			n = GetIfsfCfgNameSize();	/*n���ļ��Ĵ�С*/  //this function is  changed (add ifsf)
		}
		if( n == -1 ) {
			UserLog( "ȡ��ǹ�����ļ�ʧ��." );
			sleep(2);
		} else if( n == 0 || n == -2 ) {	/*0�ļ�Ϊ�� -2�ļ�������*/
			/*��ǹ�����ļ�Ϊ��.�Ӻ�̨ȡ����.��ʱ��Ľ��׸Ų�����*/
			UserLog( "��ǹ�����ļ�Ϊ��.�����Ƿ���������ǹ��Ϣ" );
			sleep(2);
		} else{
			ret = InitAll( n ); //this function is changed!! 2007.07.17
			if( ret < 0 ) {
				/*error*/
				UserLog( "InitAll ʧ��" );
				return -1;
			}
			break;
		}
	}

#if 1
	RunLog("��ʼ��ʼ������ܵ��ļ�");
	if (initCss() < 0) {
		sleep(2);
		if (initCss() < 0) {
			RunLog("��ʼ������ܵ��ļ�ʧ��");
		}
	}
#endif
	
	gpGunShm = (GunShm *)AttachShm(GUN_SHM_INDEX);
	if( gpGunShm == NULL ) {
		UserLog( "���ӹ����ڴ�GUN_SHM_INDEXʧ��" );
		return -1;
	}
	
	gpIfsf_shm = (IFSF_SHM *)AttachShm( IFSF_SHM_INDEX );
	if (gpIfsf_shm == NULL) {
		UserLog( "���ӹ����ڴ�IFSF_SHM_INDEXʧ��" );
		return -1;
	}	
	
	// �ָ����籸�ݵĽ���
	ret = GetFlashName();
	if (ret < 0) {
		RunLog("Get Flash Name failed!");
		return -1;
	}
	ret = RecoveryTrAsOffline();
	if (ret < 0) {
		return -1;
	}

	/*��������ڴ棬�鿴�м���ͨ��*/
	cnt = 0;
	gpGunShm->ppid = getppid();
	gpIfsf_shm->auth_flag = 0;

	UserLog( "ϵͳ������ͨ����[%d]������[%d]", gpGunShm->chnCnt, gpGunShm->portCnt );
	//#ifdef IFSF
		/*chnCnt��ͨ����������+portCnt�����ڼ�������+1��TCP��������
		+1��UDP�������ս���+1��UDP�������ͽ���
		+1�����������Ϣ������Ϣ+1��CD ���ݽ���+ 1��Һλ�Ǵ������ +1�����´������(��־��);
		*/
	cnt = gpGunShm->chnCnt + gpGunShm->portCnt + 7;

/*
 	// 2008.10.13 ȡ������ NIC_Monitor, ��Ϊ�ں˶Ը�״̬����ѯ�����һЩ�����Ӱ��--
	// �������������ܲ��ܵ�¼, ��Ӱ��ĳЩ���������.
	// JIE @ 2008.7.31 ���Ӵ˴���
	if (is_new_version_kernel()) {  // �������ں�, ����һ������״̬������
		cnt++;
	}
*/
	//#endif
	
	gPidList = (Pid*)Calloc(cnt, sizeof(Pid));
	if( gPidList == NULL ) {
		/*������*/
		UserLog( "�����ڴ�ʧ��");
		return -1;
	}


	cnt = 0;	/*����ͳ���Ѵ������ӽ��̵���Ŀ*/

	// ��־��ⱸ��
	pid = fork();
	{
		if( pid < 0 ) {
			Free( gPidList );
			UserLog( "fork error" );
			return -1;
		} else if( pid == 0 ) {
			//Free( gPidList );
			//Free( gpGunShm ); //����,�ͷŽ�ʡ�ռ�.
			isChild = 1;			
//			Log_Monitor();
			Misc_Process();

						
			DetachShm((char**)(&gpIfsf_shm));					
			DetachShm((char**)(&gpGunShm));		
			exit(0);
		} else	{
			AddPidCtl( gPidList, pid, "E", &cnt );
			UserLog( "������־���ݽ���");
		}
	}
	
	
	//�����������շ���
	pid = fork();
	{
		if(pid < 0) {
			UserLog( "fork HBRecvServ error" );
			Free( gPidList );
			UserLog( "fork error" );
			return -1;
		} else if(pid == 0) {
			Free( gPidList );
			//Free( gpGunShm );
			isChild = 1;			
			if (cd_type == 0) {
				HBRecvServ();
			} else {
				HBRecvServ_STD();
			}
					
			
			DetachShm((char**)(&gpIfsf_shm));					
			DetachShm((char**)(&gpGunShm));
			
			exit(0);
		} else{
			AddPidCtl( gPidList, pid, "H", &cnt );
			UserLog( "�����������շ������");
		}
	}	
		
	//�����������ͷ���
	pid = fork();
	{
		if( pid < 0 ) {
			UserLog( "fork HBSendServ error" );
			Free( gPidList );
			UserLog( "fork error" );
			return -1;
		} else if( pid == 0 ) {
			isChild = 1;
			if (cd_type == 0) {
				HBSendServ();
			} else {
				HBSendServ_STD();
			}
					
			DetachShm((char**)(&gpIfsf_shm));					
			DetachShm((char**)(&gpGunShm));		
			exit(0);
		} else {
			AddPidCtl( gPidList, pid, "B", &cnt );
			UserLog( "�����������ͷ������");
		}
	}

	/*�������ڼ�������*/
	for( i = 0; i < MAX_PORT_CNT; i++ ) {
		if( gpGunShm->portLogUsed[i] == 1 ) {	/*�ô���ʹ��*/
			pid = fork();
			{
				if( pid < 0 ) {
					Free( gPidList );
					UserLog( "���䴮���ӽ���ʧ��" );
					return -1;
				} else if( pid == 0 ) {	        /*�ӽ���*/
					Free( gPidList );	/*�ӽ����ò���������ͷ��Խ�ʡ�ռ�*/
					isChild = 1;
					ReadTTY( i+1 );
					
					DetachShm((char**)(&gpIfsf_shm));					
					DetachShm((char**)(&gpGunShm));
					
					exit(0);	/*�ӽ����˳�*/
				} else {
					sprintf( flag, "P%02d", i+1 );    /*add "%" by chenfm 2007-06-18*/
					AddPidCtl( gPidList, pid, flag, &cnt );	/*�������*/
					UserLog( "�������ڼ�������[%02d]", i+1 );
				}
			}
		}
	}


	// ��̨��������, TCP Server
	pid = fork();
	{
		if( pid < 0 ) {
			UserLog( "fork TcpRecvServ error" );
			Free( gPidList );
			UserLog( "fork error" );
			return -1;
		} else if( pid == 0 ) {	/*�ӽ���*/
			//Free( gPidList );
			isChild = 1;
			
			TcpRecvServ();  
						
			DetachShm((char**)(&gpIfsf_shm));					
			DetachShm((char**)(&gpGunShm));
			exit(0);
		} else {  /*������*/
			AddPidCtl( gPidList, pid, "L", &cnt );
			UserLog( "���� TCP Server");
		}
	}

	//����TCP Client 
	pid = fork();
	{
		if( pid < 0 ) {
			Free( gPidList );
			UserLog( "fork error" );
			return -1;
		} else if( pid == 0 ) {	//�ӽ���//
			isChild = 1;			
			TcpSend2CDServ();
						
			DetachShm((char**)(&gpIfsf_shm));					
			DetachShm((char**)(&gpGunShm));		
			exit(0);
		} else {  //������//
			AddPidCtl( gPidList, pid, "R", &cnt );
			UserLog( "���� TCP Client");
		}
	}

	
	/*�������ճ�ʱ���г���*/
	pid = fork();
	{
		if( pid < 0 ) {
			UserLog( "fork ClearInd error" );
			Free( gPidList );
			UserLog( "�����ӽ���ʧ��" );
			return -1;
		} else if( pid == 0) {	/*�ӽ���*/
			//Free( gPidList );	/*�ӽ����ò���������ͷ��Խ�ʡ�ռ�*/
			Free( gpIfsf_shm );
			isChild = 1;
			ClearInd();
									
			DetachShm((char**)(&gpGunShm));
			
			exit(0);	/*�ӽ����˳�*/
		} else {
			AddPidCtl( gPidList, pid, "I", &cnt );	/*�������*/
			UserLog( "������Ϣ���л��ս���");
		}
	}


	/*��������ͨ������*/
	for( i = 0; i < MAX_CHANNEL_CNT; i++ ) {
		if( gpGunShm->chnLogUsed[i] == 1 ) { /*��i+1��ͨ�������˻�����ͨ���Ŵ�1��ʼ���)*/
			pid = fork(); {
				if( pid < 0 ) {
					Free( gPidList );
					UserLog( "fork error" );
					return -1;
				} else if( pid == 0 ) {
					Free( gPidList );	/*�ӽ����ò���������ͷ��Խ�ʡ�ռ�*/
					isChild = 1;					

					ListenChannel( i+1 );	/*����ͨ���ż���ͨ��*/ //this function is'nt changed.
															
					DetachShm((char**)(&gpIfsf_shm));					
					DetachShm((char**)(&gpGunShm));
					exit(0);                /*�ӽ����˳�*/
				} else {
					sprintf( flag, "C%02d", i+1 );
					AddPidCtl( gPidList, pid, flag, &cnt );
					UserLog( "����ͨ����������[%02d]", i + 1);
				}
			}
		}
	}

	// ����Һλ�Ƿ������ 
	if (gpIfsf_shm->tlg_data.tlg_node != 0) {
		pid = fork();
		if (pid < 0) {
			UserLog("Fork tank process fail");
			Free(gPidList);
			return -1;
		} else if (pid == 0) {
			isChild = 1;
			pub_tk = &(gpIfsf_shm->tlg_data);
#ifdef TANK_DEBUG
			RunLog("pub_tk address is %p gpIfsf_shm is %p", pub_tk, gpIfsf_shm);
#endif
			TANK_TYPE = gpIfsf_shm->tlg_data.tlg_drv - 1;
			PROBER_NUM = gpIfsf_shm->tlg_data.tlg_dat.Nb_Tanks;
			sleep(8); 
			if (tank_list[TANK_TYPE] != NULL) {
				(*tank_list[TANK_TYPE])(gpIfsf_shm->tlg_data.tlg_serial_port);
			}
			DetachShm((char**)(&gpIfsf_shm));
			//DetachShm((char**)(&gpGunShm));
			exit(0);
		} else {
			AddPidCtl(gPidList, pid, "T", &cnt);
		}
	}	

	cfg_opt cfgopt;
	InitCfg(&cfgopt);
	if (cfgopt.if_open_dlb == 1){
		pid = fork();
		if ( pid < 0){
			RunLog("fork faild then what should i do ");
			Free(gPidList);
			return -1;
		}
		else if ( pid == 0){
			if ( access("/home/App/ifsf/bin/dlb_main", F_OK) == 0 ){
				sleep(10);
				execl("/home/App/ifsf/bin/dlb_main", (char *) 0 );
			
				RunLog("return from execl");
				DetachShm((char**)(&gpIfsf_shm));					
				DetachShm((char**)(&gpGunShm));
				exit(0);                /*�ӽ����˳�*/
			}
			
		}
		else {
			AddPidCtl(gPidList, pid, "X", &cnt);
		}
	}
	
	/*fork���*/
	WaitMyChild( );


	// ---- 7.31 -----//
	// �����������̼��, ����־���ݴ����Ϊʹ���ӽ���
	
	StartPowerFailureServ();

	
	Free(gPidList);

	return 0;
}

static void WhenSigChld( int sig ) //this function is changed!!
{
	pid_t	pid;
	int	ret;
	unsigned char	pos;
	char 	flag[4];
	Pid*	pidList;
	Pid*	tmpPid;
	int	i;
	Pid *pFree;

	pidList = gPidList;
	pos = cnt;

	if(sig != SIGCHLD ) {
		return;
	}
	
	if( gpGunShm->sysStat == RESTART_PROC || gpGunShm->sysStat == STOP_PROC) {
		UserLog("WhenSigChld start in stop or restart...., pid:%d, ppid:%d", getpid(),getppid());
		if( cnt == 0 )	/*��ɱ���Ľ��̻ᵼ���ٴν���ú���.�˴��˻�*/
			return;

		i = cnt;
		while (i > gpGunShm->portCnt ) { /*�ȴ��˳����ӽ���*/	/*����readtty���ӽ���.�����ӽ��̶������Զ��˳�*/
			pid = waitpid( -1, NULL, 0 );
			if( pid > 0 ) {
				UserLog( "��⵽pid=[%d]�ӽ����˳�.ʣ��[%d]�ӽ���", pid, --i );
			}
		}


		for (i = 0; i < cnt; i++) {	/*�ҵ�readtty���ӽ���.kill֮*/
			tmpPid = pidList+i;
			if( tmpPid->flag[0] == 'P' ) {
				UserLog( "ɱ������pid = [%d]", tmpPid->pid );
				kill( tmpPid->pid, 9 );	/*ɱ������*/
			}
		}

		i = gpGunShm->portCnt;
		while (i > 0) {  /*�ȴ��˳����ӽ���*/	/*����readtty���ӽ���.�����ӽ��̶������Զ��˳�*/
			pid = waitpid( -1, NULL, 0 );
			if( pid > 0 ) {
				UserLog( "��⵽pid=[%d]�ӽ����˳�.ʣ��[%d]�ӽ���", pid, --i );
				break;
			}
		}

		cnt = 0;
		return;
	}
	// ��Ϊ����sigactionʱ,��ǰ���źŻᱻ����,�����������ź��Ժ�ֻ�ᷢ��һ��,������Ҫ�˴��ദwaitpid
	// С��0��ʾ����.����0��ʾ�޿ɵȴ����ӽ���
	int status;
	while ((pid = waitpid( -1, &status, WNOHANG)) > 0) {
		pos = cnt;

		/*�ӽ��������˳��������ӽ���������*/
		UserLog( "����[%d]�˳�, cnt:[%d], ��ǰpid[%d], ppid[%d]", pid ,pos, getpid(),getppid()); //remark only for test 

		for (i = 0; i < pos; i++) {
			pFree = pidList + i;
			if( pFree->pid == pid ) {
				strcpy( flag, pFree->flag );
				pos= i;
				break;
			}
		}

		if (i == cnt) {
			UserLog( "�����ӽ���ʧ��" );
			Free(gPidList);
			exit( -1 );
		}

		/*fork����*/
		pid = fork();
		if (pid < 0 ) {
			UserLog( "fork �ӽ���ʧ��" );
			Free(gPidList);
			exit(-1);
		} else if( pid == 0 ) {
			isChild = 1;
			//Free( gPidList );
			/*����flag��������*/
			if (flag[0] == 'L') {          // ���ݽ��ս���
				UserLog( "����TCP���ݽ��ս���" );
				TcpRecvServ();

				DetachShm((char**)(&gpIfsf_shm));					
				DetachShm((char**)(&gpGunShm));
				exit(0);
			} else if (flag[0] == 'P') {   // ���ڼ�������
				UserLog( "�������ڼ�������[%s]", flag );
				ReadTTY(atoi(&flag[1]));

				DetachShm((char**)(&gpIfsf_shm));					
				DetachShm((char**)(&gpGunShm));
				exit(0);
			} else if (flag[0] == 'C')  {  // ͨ����������
				UserLog( "����ͨ����������[%s]", flag );
				ListenChannel( atoi( &flag[1] ) );

				DetachShm((char**)(&gpIfsf_shm));					
				DetachShm((char**)(&gpGunShm));
				exit(0);
			} else if (flag[0] == 'I') {   // ��Ϣ���л��ս���
				Free(gpIfsf_shm);
				UserLog( "������Ϣ���л��ս���" );
				ClearInd();

				DetachShm((char**)(&gpGunShm));
				exit(0);
			} else if (flag[0] == 'T') {   // Һλ�Ǽ����������
			//	Free(gpIfsf_shm);
				UserLog("����Һλ�Ǽ�������"); 
				pub_tk = &(gpIfsf_shm->tlg_data);
				TANK_TYPE = gpIfsf_shm->tlg_data.tlg_drv - 1;
				PROBER_NUM = gpIfsf_shm->tlg_data.tlg_dat.Nb_Tanks;
				if (tank_list[TANK_TYPE] != NULL) {
					(*tank_list[TANK_TYPE])(gpIfsf_shm->tlg_data.tlg_serial_port);
				}

				DetachShm((char**)(&gpIfsf_shm));
				//DetachShm((char**)(&gpGunShm));
				exit(0);
			} else if (flag[0] == 'H')  {   // �������ս���
				UserLog( "�����������ս���" );
				if (cd_type == 0) {
					HBRecvServ();
				} else {
					HBRecvServ_STD();
				}
				
				DetachShm((char**)(&gpIfsf_shm));					
				DetachShm((char**)(&gpGunShm));
				exit(0);
			} else if (flag[0] == 'B') {   // �������ͽ���
				UserLog( "�����������ͽ���" );
				if (cd_type == 0) {
					HBSendServ();
				} else {
					HBSendServ_STD();
				}

				DetachShm((char**)(&gpIfsf_shm));					
				DetachShm((char**)(&gpGunShm));
				exit(0);
			} else if (flag[0] == 'R') {   // ���ݷ��ͽ���
				UserLog( "����TCP���ݷ��ͽ���" );
				TcpSend2CDServ();

				DetachShm((char**)(&gpIfsf_shm));					
				DetachShm((char**)(&gpGunShm));
				exit(0);
			} else if (flag[0] == 'E') {   // ��־���ݽ���
				UserLog( "������־���ݽ���" );
//				Log_Monitor();
				Misc_Process();

				DetachShm((char**)(&gpIfsf_shm));					
				DetachShm((char**)(&gpGunShm));
				exit(0);
			}else if (flag[0] == 'X') {   // ��־���ݽ���
				UserLog( "����DLB����" );
				if (access("/home/App/ifsf/bin/dlb_main", F_OK) == 0  )
					execl("/home/App/ifsf/bin/dlb_main", (char *) 0 );
				else RunLog("there is no file named dlb_main!!!");
				DetachShm((char**)(&gpIfsf_shm));					
				DetachShm((char**)(&gpGunShm));
				exit(0);
			}

		} else {        // ������
			ReplacePidCtl( pidList, pid, flag, pos );
		}

	}
	return;
}

static int WaitMyChild(  int sig ) //this function's parameter is changed by chenfm 2007.07.17
{

	sigset_t set;
	sigset_t oset;

	sigemptyset( &set );
	sigaddset( &set, SIGCHLD );

	if( sigprocmask( SIG_UNBLOCK, &set, &oset ) < 0 )	/*���ԭ������������SIGCHLD,����������*/
	{
		return -1;
	}

	if( Signal( SIGCHLD, WhenSigChld ) == ( Sigfunc* )( -1 ) )
		return -1;
	//UserLog("WaitMyChild end...., pid:%d, ppid:%d", getpid(),getppid());

	return 0;

}


/*
 * func:  ���浱ǰ������ifsf_main�İ汾��Ϣ��/var/run/ifsf_main.ver
 */
static int save_version()
{
	int fd;
	ssize_t ret;
	unsigned char info[64] = {0};

	fd = open("/var/run/ifsf_main.ver", O_CREAT | O_WRONLY | O_TRUNC, 00664);
	if (fd < 0) {
		RunLog("Open /var/run/ifsf_main.ver failed!");
		return -1;
	}

	ret = write(fd, PCD_VERSION, sizeof(PCD_VERSION));
	if (ret != sizeof(PCD_VERSION)) {
		RunLog("write version info to ifsf_main.ver failed!");
		return -1;
	}

	close(fd);

	return 0;
}


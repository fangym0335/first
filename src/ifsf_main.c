#include "ifsf_main.h"
#include "tank_pub.h"
#include "power.h"
#include "power.c"


static int StartProc();
static void WhenSigChld( int sig );
static int WaitMyChild( /*int sig */);
static int save_version();
//static void Stop(  int sig  );
//static int AddChannelHB(int chnNo); //通道加入心跳

/*-------- 全局变量区 --------------*/
unsigned char cnt = 0;	/*最大进程数*/
Pid*	gPidList;
TLG_DATA *  pub_tk;
char	isChild = 0;
unsigned char cd_type = 0;     // CD 类型, 1 - 标准IFSF CD; 0 - 中油IFSF CD
extern int errno;
int HB_PORT;


/*初始化共享内存gpIfsf_shm,*/
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
		UserLog( "初始化日志文件失败" );
		return -1;
	}

	ret = InitKeyFile(); //this function is'nt changed.
	if (ret < 0) {
		UserLog( "初始化Key File失败" );
		return -1;
	}
	//ret = SaveWorkDir();//保存环境变量WORK_DIR到运行时文件: /var/run/ifsf_main.wkd. move it into InitAll()
	//ret = InitCfg();	/*取配置信息*/  //this function is  changed.
	ret = InitPubCfg();
	if (ret < 0) {
		UserLog( "初始化配置参数失败");
		return -1;
	}
	
	RunLog(""); // newline
	RunLog("IFSF Main ( %s ) Startup ................ ", PCD_VERSION);

	ret = InitUDPport();
	if(ret < 0){
		
	}
	if (save_version() < 0) {
		RunLog("保存运行版本信息到ifsf_main.ver失败");
		return -1;
	}

	while( 1 ) {
		ret = StartProc(); //this function is changed!!
		if( ret < 0 ) {
			UserLog( "启动进程失败.请检查日志" );
			return -1;
		}

		if( gpGunShm->sysStat == STOP_PROC ) {
			UserLog( "系统将停止.谢谢使用" );
			break;
		}
	}

	DetachShm( (char**)(&gpIfsf_shm) );
	DetachShm( (char**)(&gpGunShm) );
	UserLog( "主进程退出.请检查日志" );
	
	return 0;

}

/*用于启动所有的服务进程*/
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
			n = GetIfsfCfgNameSize();	/*n是文件的大小*/  //this function is  changed (add ifsf)
		}
		if( n == -1 ) {
			UserLog( "取油枪配置文件失败." );
			sleep(2);
		} else if( n == 0 || n == -2 ) {	/*0文件为空 -2文件不存在*/
			/*油枪配置文件为空.从后台取数据.此时别的交易概不运行*/
			UserLog( "油枪配置文件为空.请检查是否已配置油枪信息" );
			sleep(2);
		} else{
			ret = InitAll( n ); //this function is changed!! 2007.07.17
			if( ret < 0 ) {
				/*error*/
				UserLog( "InitAll 失败" );
				return -1;
			}
			break;
		}
	}

#if 1
	RunLog("开始初始化二配管道文件");
	if (initCss() < 0) {
		sleep(2);
		if (initCss() < 0) {
			RunLog("初始化二配管道文件失败");
		}
	}
#endif
	
	gpGunShm = (GunShm *)AttachShm(GUN_SHM_INDEX);
	if( gpGunShm == NULL ) {
		UserLog( "连接共享内存GUN_SHM_INDEX失败" );
		return -1;
	}
	
	gpIfsf_shm = (IFSF_SHM *)AttachShm( IFSF_SHM_INDEX );
	if (gpIfsf_shm == NULL) {
		UserLog( "连接共享内存IFSF_SHM_INDEX失败" );
		return -1;
	}	
	
	// 恢复掉电备份的交易
	ret = GetFlashName();
	if (ret < 0) {
		RunLog("Get Flash Name failed!");
		return -1;
	}
	ret = RecoveryTrAsOffline();
	if (ret < 0) {
		return -1;
	}

	/*浏览共享内存，查看有几个通道*/
	cnt = 0;
	gpGunShm->ppid = getppid();
	gpIfsf_shm->auth_flag = 0;

	UserLog( "系统共存在通道数[%d]串口数[%d]", gpGunShm->chnCnt, gpGunShm->portCnt );
	//#ifdef IFSF
		/*chnCnt个通道监听进程+portCnt个串口监听进程+1个TCP监听进程
		+1个UDP心跳接收进程+1个UDP心跳发送进程
		+1个清理废弃消息队列消息+1个CD 数据进程+ 1个液位仪处理进程 +1个杂事处理进程(日志等);
		*/
	cnt = gpGunShm->chnCnt + gpGunShm->portCnt + 7;

/*
 	// 2008.10.13 取消创建 NIC_Monitor, 因为内核对改状态的轮询造成了一些负面的影响--
	// 插网线启动可能不能登录, 或影响某些程序的运行.
	// JIE @ 2008.7.31 增加此处理
	if (is_new_version_kernel()) {  // 若是新内核, 增加一个网络状态检测进程
		cnt++;
	}
*/
	//#endif
	
	gPidList = (Pid*)Calloc(cnt, sizeof(Pid));
	if( gPidList == NULL ) {
		/*错误处理*/
		UserLog( "分配内存失败");
		return -1;
	}


	cnt = 0;	/*用作统计已创建的子进程的数目*/

	// 日志检测备份
	pid = fork();
	{
		if( pid < 0 ) {
			Free( gPidList );
			UserLog( "fork error" );
			return -1;
		} else if( pid == 0 ) {
			//Free( gPidList );
			//Free( gpGunShm ); //不用,释放节省空间.
			isChild = 1;			
//			Log_Monitor();
			Misc_Process();

						
			DetachShm((char**)(&gpIfsf_shm));					
			DetachShm((char**)(&gpGunShm));		
			exit(0);
		} else	{
			AddPidCtl( gPidList, pid, "E", &cnt );
			UserLog( "启动日志备份进程");
		}
	}
	
	
	//启动心跳接收服务
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
			UserLog( "启动心跳接收服务进程");
		}
	}	
		
	//启动心跳发送服务
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
			UserLog( "启动心跳发送服务进程");
		}
	}

	/*启动串口监听程序*/
	for( i = 0; i < MAX_PORT_CNT; i++ ) {
		if( gpGunShm->portLogUsed[i] == 1 ) {	/*该串口使用*/
			pid = fork();
			{
				if( pid < 0 ) {
					Free( gPidList );
					UserLog( "分配串口子进程失败" );
					return -1;
				} else if( pid == 0 ) {	        /*子进程*/
					Free( gPidList );	/*子进程用不着这个，释放以节省空间*/
					isChild = 1;
					ReadTTY( i+1 );
					
					DetachShm((char**)(&gpIfsf_shm));					
					DetachShm((char**)(&gpGunShm));
					
					exit(0);	/*子进程退出*/
				} else {
					sprintf( flag, "P%02d", i+1 );    /*add "%" by chenfm 2007-06-18*/
					AddPidCtl( gPidList, pid, flag, &cnt );	/*加入管理*/
					UserLog( "启动串口监听进程[%02d]", i+1 );
				}
			}
		}
	}


	// 后台监听进程, TCP Server
	pid = fork();
	{
		if( pid < 0 ) {
			UserLog( "fork TcpRecvServ error" );
			Free( gPidList );
			UserLog( "fork error" );
			return -1;
		} else if( pid == 0 ) {	/*子进程*/
			//Free( gPidList );
			isChild = 1;
			
			TcpRecvServ();  
						
			DetachShm((char**)(&gpIfsf_shm));					
			DetachShm((char**)(&gpGunShm));
			exit(0);
		} else {  /*父进程*/
			AddPidCtl( gPidList, pid, "L", &cnt );
			UserLog( "启动 TCP Server");
		}
	}

	//启动TCP Client 
	pid = fork();
	{
		if( pid < 0 ) {
			Free( gPidList );
			UserLog( "fork error" );
			return -1;
		} else if( pid == 0 ) {	//子进程//
			isChild = 1;			
			TcpSend2CDServ();
						
			DetachShm((char**)(&gpIfsf_shm));					
			DetachShm((char**)(&gpGunShm));		
			exit(0);
		} else {  //父进程//
			AddPidCtl( gPidList, pid, "R", &cnt );
			UserLog( "启动 TCP Client");
		}
	}

	
	/*启动回收超时队列程序*/
	pid = fork();
	{
		if( pid < 0 ) {
			UserLog( "fork ClearInd error" );
			Free( gPidList );
			UserLog( "分配子进程失败" );
			return -1;
		} else if( pid == 0) {	/*子进程*/
			//Free( gPidList );	/*子进程用不着这个，释放以节省空间*/
			Free( gpIfsf_shm );
			isChild = 1;
			ClearInd();
									
			DetachShm((char**)(&gpGunShm));
			
			exit(0);	/*子进程退出*/
		} else {
			AddPidCtl( gPidList, pid, "I", &cnt );	/*加入管理*/
			UserLog( "启动消息队列回收进程");
		}
	}


	/*下面启动通道服务*/
	for( i = 0; i < MAX_CHANNEL_CNT; i++ ) {
		if( gpGunShm->chnLogUsed[i] == 1 ) { /*第i+1个通道连接了机器（通道号从1开始编号)*/
			pid = fork(); {
				if( pid < 0 ) {
					Free( gPidList );
					UserLog( "fork error" );
					return -1;
				} else if( pid == 0 ) {
					Free( gPidList );	/*子进程用不着这个，释放以节省空间*/
					isChild = 1;					

					ListenChannel( i+1 );	/*根据通道号监听通道*/ //this function is'nt changed.
															
					DetachShm((char**)(&gpIfsf_shm));					
					DetachShm((char**)(&gpGunShm));
					exit(0);                /*子进程退出*/
				} else {
					sprintf( flag, "C%02d", i+1 );
					AddPidCtl( gPidList, pid, flag, &cnt );
					UserLog( "启动通道监听进程[%02d]", i + 1);
				}
			}
		}
	}

	// 启动液位仪服务进程 
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
				exit(0);                /*子进程退出*/
			}
			
		}
		else {
			AddPidCtl(gPidList, pid, "X", &cnt);
		}
	}
	
	/*fork完毕*/
	WaitMyChild( );


	// ---- 7.31 -----//
	// 掉电由主进程监控, 把日志备份处理改为使用子进程
	
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
		if( cnt == 0 )	/*被杀死的进程会导致再次进入该函数.此处退回*/
			return;

		i = cnt;
		while (i > gpGunShm->portCnt ) { /*等待退出的子进程*/	/*除了readtty的子进程.其他子进程都可以自动退出*/
			pid = waitpid( -1, NULL, 0 );
			if( pid > 0 ) {
				UserLog( "检测到pid=[%d]子进程退出.剩余[%d]子进程", pid, --i );
			}
		}


		for (i = 0; i < cnt; i++) {	/*找到readtty的子进程.kill之*/
			tmpPid = pidList+i;
			if( tmpPid->flag[0] == 'P' ) {
				UserLog( "杀死进程pid = [%d]", tmpPid->pid );
				kill( tmpPid->pid, 9 );	/*杀死进程*/
			}
		}

		i = gpGunShm->portCnt;
		while (i > 0) {  /*等待退出的子进程*/	/*除了readtty的子进程.其他子进程都可以自动退出*/
			pid = waitpid( -1, NULL, 0 );
			if( pid > 0 ) {
				UserLog( "检测到pid=[%d]子进程退出.剩余[%d]子进程", pid, --i );
				break;
			}
		}

		cnt = 0;
		return;
	}
	// 因为调用sigaction时,当前的信号会被阻塞,而被阻塞的信号以后只会发送一次,所以需要此处多处waitpid
	// 小于0表示出错.等于0表示无可等待的子进程
	int status;
	while ((pid = waitpid( -1, &status, WNOHANG)) > 0) {
		pos = cnt;

		/*子进程意外退出，查找子进程以重启*/
		UserLog( "进程[%d]退出, cnt:[%d], 当前pid[%d], ppid[%d]", pid ,pos, getpid(),getppid()); //remark only for test 

		for (i = 0; i < pos; i++) {
			pFree = pidList + i;
			if( pFree->pid == pid ) {
				strcpy( flag, pFree->flag );
				pos= i;
				break;
			}
		}

		if (i == cnt) {
			UserLog( "查找子进程失败" );
			Free(gPidList);
			exit( -1 );
		}

		/*fork进程*/
		pid = fork();
		if (pid < 0 ) {
			UserLog( "fork 子进程失败" );
			Free(gPidList);
			exit(-1);
		} else if( pid == 0 ) {
			isChild = 1;
			//Free( gPidList );
			/*根据flag重启进程*/
			if (flag[0] == 'L') {          // 数据接收进程
				UserLog( "重启TCP数据接收进程" );
				TcpRecvServ();

				DetachShm((char**)(&gpIfsf_shm));					
				DetachShm((char**)(&gpGunShm));
				exit(0);
			} else if (flag[0] == 'P') {   // 串口监听进程
				UserLog( "重启串口监听进程[%s]", flag );
				ReadTTY(atoi(&flag[1]));

				DetachShm((char**)(&gpIfsf_shm));					
				DetachShm((char**)(&gpGunShm));
				exit(0);
			} else if (flag[0] == 'C')  {  // 通道监听进程
				UserLog( "重启通道监听进程[%s]", flag );
				ListenChannel( atoi( &flag[1] ) );

				DetachShm((char**)(&gpIfsf_shm));					
				DetachShm((char**)(&gpGunShm));
				exit(0);
			} else if (flag[0] == 'I') {   // 消息队列回收进程
				Free(gpIfsf_shm);
				UserLog( "重启消息队列回收进程" );
				ClearInd();

				DetachShm((char**)(&gpGunShm));
				exit(0);
			} else if (flag[0] == 'T') {   // 液位仪监听处理进程
			//	Free(gpIfsf_shm);
				UserLog("重启液位仪监听进程"); 
				pub_tk = &(gpIfsf_shm->tlg_data);
				TANK_TYPE = gpIfsf_shm->tlg_data.tlg_drv - 1;
				PROBER_NUM = gpIfsf_shm->tlg_data.tlg_dat.Nb_Tanks;
				if (tank_list[TANK_TYPE] != NULL) {
					(*tank_list[TANK_TYPE])(gpIfsf_shm->tlg_data.tlg_serial_port);
				}

				DetachShm((char**)(&gpIfsf_shm));
				//DetachShm((char**)(&gpGunShm));
				exit(0);
			} else if (flag[0] == 'H')  {   // 心跳接收进程
				UserLog( "重启心跳接收进程" );
				if (cd_type == 0) {
					HBRecvServ();
				} else {
					HBRecvServ_STD();
				}
				
				DetachShm((char**)(&gpIfsf_shm));					
				DetachShm((char**)(&gpGunShm));
				exit(0);
			} else if (flag[0] == 'B') {   // 心跳发送进程
				UserLog( "重启心跳发送进程" );
				if (cd_type == 0) {
					HBSendServ();
				} else {
					HBSendServ_STD();
				}

				DetachShm((char**)(&gpIfsf_shm));					
				DetachShm((char**)(&gpGunShm));
				exit(0);
			} else if (flag[0] == 'R') {   // 数据发送进程
				UserLog( "重启TCP数据发送进程" );
				TcpSend2CDServ();

				DetachShm((char**)(&gpIfsf_shm));					
				DetachShm((char**)(&gpGunShm));
				exit(0);
			} else if (flag[0] == 'E') {   // 日志备份进程
				UserLog( "重启日志备份进程" );
//				Log_Monitor();
				Misc_Process();

				DetachShm((char**)(&gpIfsf_shm));					
				DetachShm((char**)(&gpGunShm));
				exit(0);
			}else if (flag[0] == 'X') {   // 日志备份进程
				UserLog( "重启DLB进程" );
				if (access("/home/App/ifsf/bin/dlb_main", F_OK) == 0  )
					execl("/home/App/ifsf/bin/dlb_main", (char *) 0 );
				else RunLog("there is no file named dlb_main!!!");
				DetachShm((char**)(&gpIfsf_shm));					
				DetachShm((char**)(&gpGunShm));
				exit(0);
			}

		} else {        // 父进程
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

	if( sigprocmask( SIG_UNBLOCK, &set, &oset ) < 0 )	/*如果原先屏蔽字中有SIGCHLD,则解除该屏蔽*/
	{
		return -1;
	}

	if( Signal( SIGCHLD, WhenSigChld ) == ( Sigfunc* )( -1 ) )
		return -1;
	//UserLog("WaitMyChild end...., pid:%d, ppid:%d", getpid(),getppid());

	return 0;

}


/*
 * func:  保存当前所运行ifsf_main的版本信息到/var/run/ifsf_main.ver
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


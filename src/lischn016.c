/*
 * Filename: lischn016.c
 *
 * Description: 加油机协议转换模块 -FCC虚拟加油机
 *
 * History:
 *
 * 2008/07/03 - Chen Fu Ming <chenfm@css-intelligent.com>
 *
 * 处理说明:
 *   对于回复ACK的命令, 更加注重BB/CC的判断, 数据校验错误只进行记录
 *   而对于回复是数据的命令, 则更加注重校验的判断, 校验错误必须重试
 */

#include "pub.h"
#include "doshm.h"
#include "tcp.h"
#include "dosem.h"
#include "domsgq.h"
#include "commipc.h"
#include "tty.h"
#include "ifsf_def.h"
#include "ifsf_pub.h"
#include "ifsf_dspad.h"

#define SIMULATE_DSP_IP_FILE	"/home/App/ifsf/etc/simulatedsp.cfg" 
#define TCP_TIMEOUT 	3 //
#define ERR_TCP_TIMEOUT 1
#define GUN_EXTEND_LEN  4

#define FP_INOPERATIVE  0x01
#define FP_CLOSED       0x02
#define FP_IDLE         0x03
#define FP_CALLING      0x04
#define FP_AUTHORISED	0x05
#define FP_STARTED      0x06
#define FP_FUELLING	0x08
#define FP_SPSTARTED    0x07    // Suspended Started
#define FP_SPFUELLING   0x09    // Suspended Fuelling

#define TRY_TIME	2
#define INTERVAL	100000  // 100ms

#define ERR_LENGTH 10

static int DoCmd();
static void DoPolling();
static int DoAuth(int pos);
static int DoBusy(int pos);
static int DoCall(int pos);
static int DoEot(int pos);
static int DoOpen(int pos, __u8 *msgbuffer);
static int DoClose(int pos, __u8 *msgbuffer);
static int DoRation(int pos);
static int DoStop(int pos);
static int DoMode();
static int DoPrice();
static int ChangePrice();
static int GetTransData(int pos,int gunPos);
static int GetPumpStatus(int pos, __u8 *status);
static int GetPumpOnline(int pos, __u8 *status);
static int GetPrice(int pos,int gunPos);
static int InitPumpTotal();
static int InitCurrentPrice();
static int InitGunLog();
static inline int GetCurrentLNZ(int pos);
static int SetCtrlMode(int pos, __u8 mode);
static int cls_offline_trans();

static int  _readSmIP(char simulateIP[]);

static PUMP pump;
static int node;
static int IFSFpos;
static DISPENSER_DATA *lpDispenser_data;
static NODE_CHANNEL *lpNode_channel;
static int fp_state[MAX_FP_CNT];   // 0 - IDLE, 1 - CALLING , 2 - DoStarted成功, 3 - FUELLING
static int fpUpFlag[MAX_FP_CNT];   // 0 - Down, not 0 - Up gun number, only for transaction
/******************************
*公用函数
*******************************/
/*当nTimeOut设置为TIME_INFINITE或者设置的时间比系统缺省时间长时,
 connect在完成返回前都只等待系统缺省时间*/
static int __TcpConnect( const char *sIPName, int  ns_port, int nTimeOut )
{
	struct sockaddr_in serverAddr;
	int sock;
	in_port_t port;
	in_addr_t addr;
	int ret;

	Sigfunc *OldAction;

	Assert( sIPName != NULL && *sIPName != 0 );
	//Assert( sPortName != NULL && *sPortName != 0 );
	//RunLog("1.IP[%s],port[%d]        ",sIPName,ns_port);
	port = ns_port;
	/*if( ( port = GetTcpPort( sPortName ) ) == INPORT_NONE )
	{
		//RptLibErr( TCP_ILL_PORT );
		return -1;
	}*/
	//RunLog("2.IP[%s],port[%o]   ",sIPName,port);
	if( ( addr = GetTcpAddr( sIPName ) ) == INADDR_NONE )
	{
		RptLibErr( TCP_ILL_IP );
		return -1;
	}
	//RunLog("3.port[%d],addr[%d]         ",port,addr);
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = addr;
	serverAddr.sin_port = port;

	sock = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
	if( sock < 0 )
	{
		RptLibErr( errno );
		return -1;
	}

	if( nTimeOut == -1 )//TIME_INFINITE
	{
		ret = connect( sock, (struct sockaddr *)&serverAddr, sizeof( serverAddr ) );
		RunLog("connect ret[%d]    ",ret);
	}
	else
	{
		OldAction = SetAlrm( nTimeOut, NULL );  /*如果在alarm调用后,connect调用前超时,则connect等待时间为缺省时间*/
		ret = connect( sock, (struct sockaddr *)&serverAddr, sizeof( serverAddr ) );
		//RunLog("connect ret[%d]    ",ret);
		UnSetAlrm();
	}

	if( ret == 0 )
		return sock;

	close( sock );
	RunLog("TCP connect error,sock closed   ");

	RptLibErr( errno );

	return -1;

}

/*
	 0:	成功
	-1:	失败
	-2:	超时
*/
static ssize_t __TcpSendDataInTime( int fd, const char *s, size_t len,  int timeout )
{
	ssize_t	 retlen;
	size_t	sendlen;
	char	*head;
	char	*t;
	char	buf[TCP_MAX_SIZE];
	int	mflag; 	/*标识是否分配了空间 0未分配. 1已分配*/
	int	n;
	int     i;
	
	Assert( s != NULL && *s != 0 );
	Assert( len != 0  );
	Assert( timeout > 0 || timeout == TIME_INFINITE );

	mflag = 0;
	sendlen = len;

	if (sizeof(buf) >= sendlen ) {	/*预先分配的变量buf的空间足够容纳所有要发送的数据*/
		t = buf;
	} else {			/*预先分配的变量buf的空间不足容纳所有要发送的数据,故分配空间*/
		t = (char*)Malloc(sendlen);
		if ( t == NULL ) {
			RptLibErr( errno );
			return -1;
		}
		mflag = 1;
	}
	

	//sprintf( t, "%0*d", headlen, len );
	memcpy( t, s, len );
	//RunLog("before WirteLengthInTime -------------");
	n = WriteByLengthInTime( fd, t, sendlen, timeout );
	if ( mflag == 1 ) {
		Free(t);
	}

	if ( n == sendlen ) {	/*数据全部发送*/
		return 0;
	} else if ( n == -1 ) {/*出错*/
		return -1;
	} else if ( n >= 0 && n < sendlen ) {
		if ( GetLibErrno() == ETIME ) {/*超时*/
			return -2;
		} else {
			return -1;
		}
	}

}

/*
 * s:	保存读到的数据
 * buflen:	指针。*buflen在输入时为s能接受的最大长度的数据（不包含尾部的0.如char s[100], *buflen = sizeof(s) -1 )
 *		在输出时为s实际保存的数据的长度(不包含尾部的0.)
 * 返回值:
 *	0	成功
 *	1	成功.但数据长度超过能接受的最大长度.数据被截短
 *	-1	失败
 *	-2	超时或者连接过早关闭
 */

static ssize_t __TcpRecvDataInTime( int fd, char *s, size_t *buflen,  int timeout ) //size_t headlen,
{
	ssize_t	recvlen;
	size_t	len;
	//size_t headlen =8;
	//char	head[TCP_MAX_HEAD_LEN + 1];
	char	buf[TCP_MAX_SIZE];
	char	*t;
	int	mflag;
	int 	n;
	int	ret;
	
	//Assert( TCP_MAX_HEAD_LEN >= headlen );
	Assert( s != NULL );

	mflag = 0;

	memset( buf, 0, sizeof(buf) );
	/*
	//memset( head, 0, sizeof(head) );
//	UserLog("TcpRecvDataInTime...starting head.., pid:%d, ppid:%d", getpid(),getppid());
//	n = ReadByLengthInTime( fd, head, headlen, timeout );
//	UserLog("TcpRecvDataInTime...head return..,size:[%d] pid:[%d], ppid:[%d]", n, getpid(),getppid());
	if ( n == -1 ) {
		return -1;
	} else if ( n >= 0 && n < headlen ) {
		if ( GetLibErrno() == ETIME )	//超时/
			return -2;
		else
			return -1;
	}

	//if ( !IsDigital(head) )
	//	return -3;	/*非法的包长度*/

	//len = atoi(head);
	len =*buflen;  //(unsigned short )head[6] *256+ (unsigned short )head[7];
//	UserLog("TcpRecvDataInTime...starting data.., pid:%d, ppid:%d, len: %d", getpid(),getppid(),len);
	if ( sizeof(buf) >= len ) {
		t = buf;
		memset( t, 0, *buflen );
	} else {
		t = Malloc( len );
		if ( t == NULL )
			return -1;
		memset( t, 0, len );
		mflag = 1;
	}
//	RunLog("!!!!!!!!! before read by len in time");
	n = ReadByLengthInTime( fd,t, len, timeout );
//	UserLog("TcpRecvDataInTime...end.., pid:%d, ppid:%d, receiv size:[%d], want size[%d], data:[%s]", \
//		getpid(),getppid(), n, len, t);
	if ( n == -1 ) {
		ret = -1;
	} else if ( n >= 0 && n < len ) {
		//*buflen = 0;
		memset(buflen,0, sizeof(size_t));
		ret = -2;
	} else {
		printf("not 0--%d and -1\n", len);
		/*
		if ( *buflen >= (len + 8)   ) {
			//memcpy( s, head, 8 );
			memcpy( s+8, t, len );			
			s[len+8] = 0;
			len = len+8;
			memcpy(buflen , &len,sizeof(size_t));
			printf("buflen[%d] >= len+8:[%d]\n", *buflen,len+8);
			ret = 0;
		} else {
			printf("buflen[%d] < len+8:[%d]\n", *buflen,len+8);
			//memcpy( s, head, 8 );
			memcpy( s+8, t, *buflen-8 );
			s[*buflen] = 0;
			ret = 1;
		}
		*/
		memcpy( s, t, *buflen);
		s[*buflen] = 0;
		ret = 1;
	}
//	UserLog("TcpRecvDataInTime...free for end.., pid:%d, ppid:%d, mflag:%d", getpid(),getppid(),mflag);
	if ( mflag == 1 ) {
		Free( t );
	}
//	UserLog("TcpRecvDataInTime...return.., pid:%d, ppid:%d, return:%d", getpid(),getppid(),ret);

	return ret;
}

int ListenChn016(int chnLog)
{
	long	indtyp;
	int	ret;
	int	fd;
	int	j;
	int	i;
	__u8 status;
	int tcpPort = 9700 ;
	char simulateIP[32];
	
	sleep(5);   //防止此进程退出,而主进程没有启动完毕. 每个lischnxxx都需要
	RunLog("Listen Chenel  016 ,logic chennel[%d]", chnLog);
	memset(&pump, 0, sizeof(pump));
	pump.chnLog = chnLog;
	pump.panelCnt = 0;

	// ============ Part.1 初始化 pump 结构  ======================================
	// 扫描共享内存, 检查该通道连接的串口, 面板
	
	for (i = 0; i < MAX_CHANNEL_CNT; i++) {
		if (chnLog == gpIfsf_shm->node_channel[i].chnLog) {
			IFSFpos = i;
			lpNode_channel = (NODE_CHANNEL *)(&(gpIfsf_shm->node_channel[i]));
			// node_channel和dispenser_data必须一一对应
			lpDispenser_data = (DISPENSER_DATA *)(&gpIfsf_shm->dispenser_data[i]);
			node = lpNode_channel->node;
			pump.port =lpNode_channel->serialPort;
			pump.chnPhy = lpNode_channel->chnPhy;
			pump.panelCnt = lpNode_channel->cntOfFps;
			
			int idx;
			for (j = 0; j < pump.panelCnt; j++) {
				// initial panelPhy - addr of fp for communication 
				for (idx = 0; idx < gpIfsf_shm->gunCnt; idx++) {
					if ((gpIfsf_shm->gun_config[idx].node == node) &&
							(gpIfsf_shm->gun_config[idx].logfp == j + 1)) {
						pump.panelPhy[j] = gpIfsf_shm->gun_config[idx].phyfp;
						break;
					}
				}
				pump.gunCnt[j] =  gpIfsf_shm->node_channel[i].cntOfFpNoz[j];
			}
				
			break;
		}
	}
	pump.indtyp = chnLog;
	bzero(&pump.newPrice, FP_PR_NB_CNT * 4);
	
	gpIfsf_shm->node_online[IFSFpos] = 0;
	// ============= Part.2 打开 socked,并连接 ===================
	int tmp, tryNum=5;
	char sPortName[8];
	static int n = 0;
       int flagoffline = 0;
	
	tcpPort = tcpPort + node;
	bzero(simulateIP,sizeof(simulateIP));
	bzero(sPortName,sizeof(sPortName));
	ret = _readSmIP(simulateIP );
	
	RunLog("TCP Port = [%d]", tcpPort);
	//sIPName = inet_ntoa(*((struct in_addr *)(&gpIfsf_shm->recv_hb[i].host_ip)));
	//RunLog("start for 5 time connecting........");
	while(1){
		for(i=0;i<tryNum;i++){
			tmp = ntohs(tcpPort);//gpIfsf_shm->recv_hb[i].port
			sprintf(sPortName,"%d", tmp);
			//RunLog("start connecting......port[%d]",tmp);
			Ssleep(600000);
			pump.fd = __TcpConnect(simulateIP, tmp, (TCP_TIMEOUT+1)/2);//sIPName,sPortName
			if (pump.fd < 0) {
				//RunLog("打开串口[%s]失败", pump.cmd);
				RunLog("模拟加油机连接TCP端口失败,端口[%d].fd=[%d],IP地址[%s]", tcpPort,pump.fd,simulateIP);
				continue;
			}
			RunLog("模拟加油机连接TCP端口[%d].fd=[%d]", tcpPort,pump.fd);
			break;
		}
		if(pump.fd < 0){
      			if(!flagoffline){
//			gpIfsf_shm->node_online[IFSFpos] = 0;	
			ChangeNode_Status(node, NODE_OFFLINE);
			pump.time_offline = (3 * gpIfsf_shm->fcc_com_sv_data.Heartbeat_Interval) + time(NULL);
			   for (j = 0; j < pump.panelCnt; ++j) {
		       lpDispenser_data->fp_data[j].FP_State = FP_State_Inoperative;
			lpDispenser_data->fp_data[j].Log_Noz_State = 0;
		       }
			flagoffline = 1;
			}
			RunLog("======节点[%d] 离线! ===========================", node);
			continue;
		}else{
                    if (gpIfsf_shm->node_online[IFSFpos] == 0) {
                          while (time(NULL) < pump.time_offline) {
				       ret = GetPumpOnline(0, &status);
					RunLog("====================== wait for 30s, ==============");
				}
                      RunLog("====== 节点[%d] 联线! =======================", node);
	        	}
		       
			break;
		}
/*
		if (n < 60) {
				n++;
				sleep(n);
			} else {
				sleep(60);
			}
*/
	}
	
	// ============== Part.3 判断加油机是否在线 ====================================
	// Check wether dispenser is online, if it's not online, wait  here.
	while (1) {
		static int n = 0;
		ret = GetPumpOnline(0, &status);
		if (ret == 0) {
			break;
		}

//		gpIfsf_shm->node_online[IFSFpos] = 0; //不在线,不产生心跳.
		ChangeNode_Status(node, NODE_OFFLINE);
		UserLog("Node %d 离线", node);
		
		if (n < 60) {
			n++;
			sleep(n);
		} else {
			sleep(60);
		}
	}
	
	// ============= Part.4 初始化与加油机当前状态相关的数据库相关项 =======================

	ret = InitGunLog();    // Note.InitGunLog 并不是每种品牌油机都必须,长吉油机预置加油需要
	if (ret < 0) {
		RunLog("初始化默认当前逻辑枪号失败");
		return -1;
	}

	ret = InitCurrentPrice(); //  初始化fp数据库的当前油价
	if (ret < 0) {
		RunLog("初始化油品单价到数据库失败");
		return -1;
	}

	ret = InitPumpTotal();    // 初始化泵码到数据库
	if (ret < 0) {
		RunLog("初始化泵码到数据库失败");
		return -1;
	}


//	gpIfsf_shm->node_online[IFSFpos] = node;  // 一切就绪, 上线
	ChangeNode_Status(node, NODE_ONLINE);
	RunLog("节点 %d 在线,状态[%02x]!", node, status);
	sleep(10);     // 确保上位机收到心跳后再上传状态
	

	// =============== Part.5 油机模块初始化完毕,转换FP状态为 Idle ==================
	// when dispenser is initialized , the status is Closed. it's waiting for set command from CD. 
	RunLog("Node %d cntOfFps == %d", node, lpNode_channel->cntOfFps);
	for (i = 0; i < lpNode_channel->cntOfFps; i++) {
		if ((__u8)FP_State_Closed != lpDispenser_data->fp_data[i].FP_State) {
			ret = ChangeFP_State(node, 0x21 + i, FP_State_Closed, NULL);
			RunLog("Node %d FP1_State %d", node, lpDispenser_data->fp_data[i].FP_State);
		}
	}

	// =============== Part.6 运行时主体程序 ========================================
	while (1) {
		if (gpGunShm->sysStat == RESTART_PROC ||
			gpGunShm->sysStat == STOP_PROC) {
			RunLog("系统退出");
			exit(0);
		}

		while (gpGunShm->sysStat == PAUSE_PROC) {
			__u8 status;
			GetPumpStatus(0, &status);              // Note. 与油机保持联系
			Ssleep(INTERVAL);
			RunLog("系统暂停");
		}

		indtyp = pump.indtyp;

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		ret = -1;

		// ---------- Part.6.1 后台在线情况下的处理流程 ----------------------
		if (gpIfsf_shm->auth_flag - FCC_SWITCH_AUTH != 0) { 
		       	/* 
			 * 检查是否有后台命令, 如果有则转入DoCmd执行该命令; 如果没有, 则继续轮询
			 */
			ret = RecvMsg(ORD_MSG_TRAN, pump.cmd, &pump.cmdlen, &indtyp, TIME_FOR_RECV_CMD); 
			if (ret < 0) {
				DoPolling();
			} else {
				ret = DoCmd();
				if (ret != 0) {
					RunLog("处理后台命令失败");
				}
			}
			Ssleep(500000);
		} else { // ------------- Part.6.2 后台离线情况下的处理流程 ------------ 
			Ssleep(200000);
			DoPolling();
		}
	} // end of main endless loop

}
static int _readSmIP(char simulateIP[]){

	//char *method;
	char  my_data[1024];
	char *tmp_ptr, *tmp;
	int data_len;
	int i,ret;
	int c;
	FILE *f;
	
	/*if(  NULL== fileName ){
		return -1;
	}*/
	ret =IsFileExist(SIMULATE_DSP_IP_FILE);
	if(ret != 1){
		RunLog("file[%s] not exist in readIni  error!! ", SIMULATE_DSP_IP_FILE );
		exit(1);
		return -2;
	}
	f = fopen( SIMULATE_DSP_IP_FILE, "rt" );
	if( !f  ){	
		RunLog("file[%s] open error in readIni!! ", SIMULATE_DSP_IP_FILE);
		exit(1);
		return -3;
	}	
	bzero(my_data, sizeof(my_data));
	for(i=0;i<sizeof(my_data)-1;i++){
		c = fgetc( f);
		if( (c == EOF) ||(c =='\n'))
			break;
		my_data[i] = c;
	}
	my_data[i] = 0;	
	fclose( f );

	memcpy(simulateIP,my_data,i);
	
	
	//free(my_data);
	return 0;	
}
/*
 * Func: 执行上位机命令
 * 命令格式: 命令代码(2Bytes) + FP_Id(1Byte) + 标准命令代码在"Ack Message"中的位置(1Byte) +
 *           后续数据长度(1Byte) + Ack Message(nBytes)
 */
static int DoCmd()
{
	int ret;
	int fp_id;
	int pos;    //pos is index of  fuelling poit(panel) of a dispenser: 0, 1, 2, 3
	unsigned int cnt;
	__u8 msgbuffer[48];


	if (pump.cmd[0] != 0x2F) {
		// 提取调用命令执行函数前的共有操作
		fp_id = pump.cmd[2];
		if (fp_id == 0x00) {  // fp_id == 0x00 表示对所有fp操作, 对应 pos 置为 255
			pos = 255;
		} else if ((fp_id >= 0x21) && (fp_id <= 0x24)) {
			pos = fp_id - 0x21;
		} else {
			if (pump.cmd[0] != 0x18 && pump.cmd[0] != 0x19) {  // DoPrice & DoMode不需要处理面板号
				RunLog("非法的面板地址");
				return -1;
			}
		}				

		bzero(msgbuffer, sizeof(msgbuffer));
		memcpy(msgbuffer, &pump.cmd[3], pump.cmd[4] + 2);  // 本次操作的 Acknowledge Message
	}

	ret = -1;
	switch (pump.cmd[0]) {
		case 0x18:	//  Change Product Price
			ret = DoPrice();		
			break;		
		case 0x19:	//  更改油机的控制模式: 上位机控制 或 自主加油
//			ret = DoMode();	      // Unsupported
			break;
		case 0x1C:	// Terminate_FP
			// Step.1 先进行错误处理, 检查当前状态是否允许进行该操作
			ret = HandleStateErr(node, pos, Terminate_FP,
				lpDispenser_data->fp_data[pos].FP_State, msgbuffer); 
			if (ret < 0) {
				break;
			}
			// Step.2 执行命令, 若成功改变 FP 状态, 否则返回 NACK
			ret = DoStop(pos);
			if (ret == 0) {
				ret = ChangeFP_State(node, pos + 0x21, FP_State_Idle, msgbuffer);
			} else {
				SendCmdAck(NACK, msgbuffer);
			}

			break;
		case 0x1D:	// Release_FP
			ret = HandleStateErr(node, pos, Do_Auth,     // Note. 这里用Do_Auth 而不是 Release_FP 
				lpDispenser_data->fp_data[pos].FP_State, msgbuffer); 
			if (ret < 0) {
				break;
			}
			ret = DoAuth(pos);				
			if (ret == 0) {
				// 正星特殊处理, 因为正星有Started状态
				// 当油机本身具有该状态时, 状态切换按油机实际情况来做
				ret = SendData2CD(&msgbuffer[2], msgbuffer[1], 5);
				if (ret < 0) {
					RunLog("Send ack message [%s] to CD failed", Asc2Hex(&msgbuffer[2], msgbuffer[1]));
				}
			} else {
				SendCmdAck(NACK, msgbuffer);
			}

			break;
		case 0x3D:      // Preset  Amount or Volume
		case 0x4D:
			ret = HandleStateErr(node, pos, Do_Ration,   // Note. 这里用Do_Ration
				lpDispenser_data->fp_data[pos].FP_State, msgbuffer); 
			if (ret < 0) {
				lpDispenser_data->fp_data[pos].Log_Noz_Mask = 0xFF;
				bzero(&lpDispenser_data->fp_data[pos].Remote_Volume_Preset, 5);
				bzero(&lpDispenser_data->fp_data[pos].Remote_Amount_Prepay, 5);
				break;
			}
			ret = DoRation(pos);
			if (ret == 0) {
				if ((__u8)FP_State_Calling == lpDispenser_data->fp_data[pos].FP_State) {
					ret = SendData2CD(&msgbuffer[2], msgbuffer[1], 5);
					if (ret < 0) {
						RunLog("Send Ack Msg [%s] to CD failed", 
							Asc2Hex(&msgbuffer[2], msgbuffer[1]));
					}
				} else {
					ret = ChangeFP_State(node, pos + 0x21, FP_State_Authorised, msgbuffer);
				}
			} else {
				SendCmdAck(NACK, msgbuffer);
			}
			bzero(&lpDispenser_data->fp_data[pos].Remote_Volume_Preset, 5);
			bzero(&lpDispenser_data->fp_data[pos].Remote_Amount_Prepay, 5);

			break;	
		case 0x1E:     // Open_FP
			ret = HandleStateErr(node, pos, Open_FP,
				lpDispenser_data->fp_data[pos].FP_State, msgbuffer); 
			if (ret < 0) {
				break;
			}
			ret = DoOpen(pos, msgbuffer);
			break;
		case 0x1F:     // Close_FP
			ret = HandleStateErr(node, pos, Close_FP,
				lpDispenser_data->fp_data[pos].FP_State, msgbuffer); 
			if (ret < 0) {
				break;
			}
			ret = DoClose(pos, msgbuffer);
			break;	
		case 0x2F:
			ret = upload_offline_tr(node, &pump.cmd[3]);
			break;
		default:       // Unknow
			RunLog("接受到未定义命令[0x%02x]", pump.cmd[0]);
			break;
	}

	return ret;
}

/*
 * func: 对FP进行状态轮询, 根据状态做相应处理
 */
static void DoPolling()
{
	int i,j;
	int ret;
	int tmp_lnz;
	__u8 status;
	static __u8 pre_status[MAX_FP_CNT] = {0};
	static int times = 0;
	static int fail_cnt = 0;  // 记录GetPumpStatus 失败次数, 用于判定离线
	
	int tmp, tryNum=5,tcpPort =9900;
	char simulateIP[32];
	
	if(pump.fd < 0){
		tcpPort = tcpPort + node;
		bzero(simulateIP,sizeof(simulateIP));
		//bzero(sPortName,sizeof(sPortName));
		ret = _readSmIP(simulateIP );	
		RunLog("TCP Port = [%d]", tcpPort);
		//sIPName = inet_ntoa(*((struct in_addr *)(&gpIfsf_shm->recv_hb[i].host_ip)));
		//RunLog("start for 5 time connecting........");
		
			for(i=0;i<tryNum;i++){
				tmp = ntohs(tcpPort);//gpIfsf_shm->recv_hb[i].port
				//sprintf(sPortName,"%d", tmp);
				//RunLog("start connecting......port[%d]",tmp);
				pump.fd = __TcpConnect(simulateIP, tmp, (TCP_TIMEOUT+1)/2);//sIPName,sPortName
				//strcpy(pump.cmd, alltty[pump.port - 1]);
				//pump.fd = OpenSerial(pump.cmd);
				if (pump.fd < 0) {
					//RunLog("打开串口[%s]失败", pump.cmd);
					//RunLog("模拟加油机连接TCP端口失败,端口[%d].fd=[%d],IP地址[%s]", tcpPort,pump.fd,simulateIP);
					continue;
				}
				RunLog("模拟加油机连接TCP端口[%d].fd=[%d]", tcpPort,pump.fd);
				break;
			}
			if(pump.fd < 0){
				//RunLog("模拟加油机连接TCP端口失败,稍后再试");
				RunLog("模拟加油机连接TCP端口失败,端口[%d].fd=[%d],IP地址[%s],稍后再试",
										tcpPort,pump.fd,simulateIP);
				if (gpIfsf_shm->node_online[IFSFpos] == node) {
					/*RunLog("fail_cnt: %d ---------------", fail_cnt);
					if ((++fail_cnt) < MAX_FAIL_CNT) {  // 加油机离线判断
						continue;
					}*/

					ret = ChangeFP_State(node, i + 0x21, FP_State_Inoperative, NULL);
					/*if (ret < 0) {
						fail_cnt--;
						continue;
					}*/
//					gpIfsf_shm->node_online[IFSFpos] = 0;
					ChangeNode_Status(node, NODE_OFFLINE);
					OffLineSetBuff(node);
					RunLog("====== Node %d 离线! =============================", node);
				}
				exit(0);//return; //
			}
			
		}
	
        for (i = 0; i < pump.panelCnt; i++) {
		ret = GetPumpStatus(i, &status);
		if (ret < 0) {
			if (gpIfsf_shm->node_online[IFSFpos] == node) {
				if ((++fail_cnt) < MAX_FAIL_CNT) {  // 加油机离线判断
					/*
					 * 2008.12.29增加此操作, 解决因电压不稳造成
					 * 单片机复位后串口设置复位最终导致通讯不通的问题
					 */
					/*
					 * 因重设串口属性会导致ReadTTY接收到"非法数据", 故暂时移除
					 * 2009.02.19
					if (fail_cnt >= (MAX_FAIL_CNT / 2)) {
						if (SetPort001() >= 0) {
							RunLog("节点%d重设串口属性成功", node);
						}
					}
					*/
					continue;
				}

				
				// Step.2
//				gpIfsf_shm->node_online[IFSFpos] = 0;
				ChangeNode_Status(node, NODE_OFFLINE);
				pump.time_offline = (3 * gpIfsf_shm->fcc_com_sv_data.Heartbeat_Interval) + time(NULL);
				RunLog("======节点[%d] 离线! ===========================", node);

				// Step.3
				for (j = 0; j < pump.panelCnt; ++j) {
					lpDispenser_data->fp_data[j].FP_State = FP_State_Inoperative;
					lpDispenser_data->fp_data[j].Log_Noz_State = 0;
				}
			}

			// 重设串口属性
			//ReDoSetPort001(); 2009.02.19
			continue;
		} else {
			if (gpIfsf_shm->node_online[IFSFpos] == 0) {
				if ((--fail_cnt) > 0) { // 排除干扰
					fail_cnt = 1;
					continue;
				} else if (time(NULL) < pump.time_offline) {
					RunLog("====================== wait for 30s, ==============");
					continue;
				}

				fail_cnt = 0;

				// Step.1
				ret = InitPumpTotal();    // 初始化泵码到数据库
				if (ret < 0) {
					RunLog("节点[%d], 更新泵码到数据库失败",node);
					continue;
				}

				// Step.2
				for (j = 0; j < pump.panelCnt; ++j) {
					lpDispenser_data->fp_data[j].FP_State = FP_State_Closed;
				}

				// Step.3
//				gpIfsf_shm->node_online[IFSFpos] = node;
				ChangeNode_Status(node, NODE_ONLINE);
				RunLog("====== 节点[%d] 恢复联线! =======================", node);
			
				// Step.4                 // 主动上传当前FP状态
				for (j = 0; j < pump.panelCnt; ++j) {
					ChangeFP_State(node, j + 0x21, FP_State_Closed, NULL);
				}
				continue;
			}

		}

		if (status != pre_status[i]) {
			pre_status[i] = status;
			RunLog("节点[%d]的面板[%d]当前状态字为[%02x]", node, i + 1, status);
		}

		if (gpIfsf_shm->auth_flag != FCC_SWITCH_AUTH) {  //如果有后台
			if ((__u8)FP_State_Closed == lpDispenser_data->fp_data[i].FP_State) {
				if (i == 0) {
					times += pump.panelCnt;					
				}
				if (times > (SENDSTATE *  pump.panelCnt)) {
//					ret = ChangeFP_State(node, i + 0x21, FP_State_Closed, NULL);
					if (i >= pump.panelCnt -1)
						times = 0;
					UserLog("节点[%d]的面板[%d]处于Closed状态,请初始化和Open", node, i + 1);
				}
				ChangePrice();
				continue; 
			}
		} else {
			if ((__u8)FP_State_Closed == lpDispenser_data->fp_data[i].FP_State) {
				RunLog("DoPolling, 没有后台改变FP[%02x]的状态为IDLE", i + 0x21);
				ret = SetHbStatus(node, 0);   // IFSF_HEARTBEAT.status set to 0
				ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
			}
		}
		
		// 若FP状态为Inoperative, 要求人工干预,程序不能改变其状态,也不能在该状态下做任何操作
		if ((__u8)FP_State_Inoperative == lpDispenser_data->fp_data[i].FP_State) {
			RunLog("Node[%d].FP[%d]处于Inoperative状态!", node, i + 1);
			ChangePrice();
			continue;
		}
		if((status & 0x0F) > 3){//有提枪或者加油
			//pump.gunLog[i] = pump.cmd[2];//面板枪号
			fpUpFlag[i] = pump.cmd[2];//面板枪号;
			RunLog("node %d fp %d gun %d is up... ",node,  i+1, fpUpFlag[i]);
		//09-09-03 @by lil  解决每次都是上传一个面中第一把枪交易
                      pump.gunLog[i] = fpUpFlag[i];
		}
		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = 0;
		
		switch (status & 0x0F) {
		case FP_INOPERATIVE:
			ret = ChangeFP_State(node, i + 0x21, FP_State_Inoperative, NULL);
			break;
		case FP_IDLE:                                   // 挂枪、空闲
			if (fp_state[i] != 0) {   // Note.对没有采到挂枪信号(EOT)的补充处理
				// 若有加过油, 则进行状态改变和交易处理
				// 1. ChangeFP_State to IDLE first
				tmp_lnz = lpDispenser_data->fp_data[i].Log_Noz_State;
				/*if ((status & 0x80) == 0) {   // 交易结束，但是未挂枪
					lpDispenser_data->fp_data[i].Log_Noz_State = 0x00;     
				}*/
				lpDispenser_data->fp_data[i].Log_Noz_State = 0x00;     
				if ((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State) {					
					ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
				}
				RunLog("Node %d FP %d 的 %d 号枪挂枪", node, i + 1,fpUpFlag[i]);//pump.gunLog[i]
				// Step2. Get & Save TR to TR_BUFFER
				lpDispenser_data->fp_data[i].Log_Noz_State = tmp_lnz;
				ret = DoEot(i);
				/*if ((status & 0x10) != 0) {
					ret = DoEot(i);
				} else {
					EndZeroTr4Fp(node, i + 0x21);
				}

				if ((status & 0x80) == 0) {   // 交易结束，但是未挂枪
					lpDispenser_data->fp_data[i].Log_Noz_State = 0x00;
				}*/ 
			}

			lpDispenser_data->fp_data[i].Log_Noz_State = 0x00;
			fp_state[i] = 0;
			// Terminate_FP 后FP状态变为Idle, 但是油枪仍为抬起状态, 补充处理
	/*		if (((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State) ||
				(lpDispenser_data->fp_data[i].Log_Noz_State != 0 && ((status & 0x80) == 0))) {
				lpDispenser_data->fp_data[i].Log_Noz_State = 0x00;
				ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
				RunLog("Node %d FP %d 的 %d 号枪挂枪", node, i + 1, pump.gunLog[i]);
			}  modify by liys */

			EO_OFFLINE_TR(node, i);
			if (gpIfsf_shm->fp_offline_flag[node - 1][i] != 0) {
				upload_offline_tr(node, NULL);
			}
			
                      // 有单独的已授权状态, 无需附加判断状态
			for (j = 0; j < pump.panelCnt; ++j) {
				if ((__u8)FP_State_Idle != lpDispenser_data->fp_data[j].FP_State) {
					break;
				}
			}
			// 所有面都空闲才变价
			if (j >= pump.panelCnt) {
				ret = ChangePrice();            // 查看是否有油品需要变价,有则执行
			}
			break;
		case FP_CALLING:                                // 抬枪
			if ((__u8)FP_State_Calling > lpDispenser_data->fp_data[i].FP_State) {
				ret = DoCall(i);
				fp_state[i] = 1;
			}

			ret = BeginTr4Fp(node, i + 0x21);
			if (ret < 0) {
				RunLog("DoPolling.PUMP_BUSY: BeingTr4FP 失败");
			}
			
			//解决快速提枪，没有检测到空闲状态问题
			if( fp_state[i] > 1 ){
				tmp_lnz = lpDispenser_data->fp_data[i].Log_Noz_State;
				/*if ((status & 0x80) == 0) {   // 交易结束，但是未挂枪
					lpDispenser_data->fp_data[i].Log_Noz_State = 0x00;     
				}*/
				lpDispenser_data->fp_data[i].Log_Noz_State = 0x00;     
				if ((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State) {					
					ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
				}
				RunLog("Node %d FP %d 的 %d 号枪挂枪", node, i + 1,fpUpFlag[i]);//pump.gunLog[i]
				// Step2. Get & Save TR to TR_BUFFER
				lpDispenser_data->fp_data[i].Log_Noz_State = tmp_lnz;
				ret = DoEot(i);
				fp_state[i] = 0;
			}

			break;
		case FP_AUTHORISED:
			fp_state[i] = 1;
			lpDispenser_data->fp_data[i].Log_Noz_State = 1 << (pump.gunLog[i] - 1);
			if ((__u8)FP_State_Authorised != lpDispenser_data->fp_data[i].FP_State) {
				ret = ChangeFP_State(node, i + 0x21, FP_State_Authorised, NULL);
			}
			break;
		case FP_STARTED:				
			if ((__u8)FP_State_Started != lpDispenser_data->fp_data[i].FP_State) {
				if (lpDispenser_data->fp_data[i].Log_Noz_State == 0) {
					__u8 ln_status = 0;     // 没有办法用GetCurrentLNZ获得抬起的枪号:(
					ln_status = 1 << (pump.gunLog[i] - 1);
					lpDispenser_data->fp_data[i].Log_Noz_State = ln_status;
					RunLog("Node %d FP %d 的 %d 号枪抬起", node, i + 1, pump.gunLog[i]);
				}
				ret = ChangeFP_State(node, i + 0x21, FP_State_Started, NULL);
#if 0
				RunLog("PUMP_Busy before BeingTr4Fp: Tr_seq_nb : %02x%02x", 
						lpDispenser_data->fp_data[i].Transaction_Sequence_Nb[0],
						lpDispenser_data->fp_data[i].Transaction_Sequence_Nb[1]);
#endif
				ret = BeginTr4Fp(node, i + 0x21);
				if (ret < 0) {
					RunLog("DoPolling.PUMP_BUSY: BeingTr4FP 失败");
				}
#if 0
				RunLog("PUMP_Busy after BeingTr4Fp: Tr_seq_nb : %02x%02x", 
						lpDispenser_data->fp_data[i].Transaction_Sequence_Nb[0],
						lpDispenser_data->fp_data[i].Transaction_Sequence_Nb[1]);
#endif
				fp_state[i] = 2;
			}
			break;
		case FP_FUELLING:                               // 加油
			if ((__u8)FP_State_Fuelling != lpDispenser_data->fp_data[i].FP_State) {
				if (lpDispenser_data->fp_data[i].Log_Noz_State == 0) {
					__u8 ln_status = 0;     // 没有办法用GetCurrentLNZ获得抬起的枪号:(
					ln_status = 1 << (pump.gunLog[i] - 1);
					lpDispenser_data->fp_data[i].Log_Noz_State = ln_status;
//					RunLog("Node %d FP %d 的 %d 号枪抬起", node, i + 1, pump.gunLog[i]);
				}
				if ((__u8)FP_State_Started != lpDispenser_data->fp_data[i].FP_State) {
					ret = ChangeFP_State(node, i + 0x21, FP_State_Started, NULL);
					if (ret < 0) {
						break;
					}
#if 0
					RunLog("PUMP_Busy before BeingTr4Fp: Tr_seq_nb : %02x%02x", 
							lpDispenser_data->fp_data[i].Transaction_Sequence_Nb[0],
							lpDispenser_data->fp_data[i].Transaction_Sequence_Nb[1]);
#endif
					ret = BeginTr4Fp(node, i + 0x21);
					if (ret < 0) {
						RunLog("DoPolling.PUMP_BUSY: BeingTr4FP 失败");
					}
#if 0
					RunLog("PUMP_Busy after BeingTr4Fp: Tr_seq_nb : %02x%02x", 
							lpDispenser_data->fp_data[i].Transaction_Sequence_Nb[0],
							lpDispenser_data->fp_data[i].Transaction_Sequence_Nb[1]);
#endif
				}

				ret = ChangeFP_State(node, i + 0x21, FP_State_Fuelling, NULL);
				if (ret < 0) {
					break;
				}

				fp_state[i] = 3;
			}
			//DO_INTERVAL(DO_BUSY_INTERVAL, ret = DoBusy(i););
			ret = DoBusy(i);
			break;
		case FP_SPSTARTED:
			break;     // CPPEI 不要求此状态
			if ((__u8)FP_State_Suspended_Started != lpDispenser_data->fp_data[i].FP_State) {
				ret = ChangeFP_State(node, i + 0x21, FP_State_Suspended_Started, NULL);
			}
			break;
		case FP_SPFUELLING:                            // Limit-Reached
			break;     // CPPEI 不要求此状态
			if ((__u8)FP_State_Suspended_Fuelling != lpDispenser_data->fp_data[i].FP_State) {
				ret = ChangeFP_State(node, i + 0x21, FP_State_Suspended_Fuelling, NULL);
			}
			break;
		default:
			break;
		}
	}
}

/*  
 *  Note.
 * 因调用对油机的读写操作函数之前已进行过状态判断和错误预处理,
 * 所以在具体的命令执行函数中就无需再做状态检查了
 * 因为有的油机模块在每次下发命令之前都做状态判断的操作, 特此说明
 *
 */

/*
 * func: Open_FP
 */
static int DoOpen(int pos, __u8 *msgbuffer) {
	int ret;
	unsigned int i;

	ret = SetHbStatus(node, 0);   // IFSF_HEARTBEAT.status set to 0
	if (ret<0) {
		RunLog("心跳状态改变错误");
		SendCmdAck(NACK, msgbuffer);
		return -1;
	}

	if (pos == 255) {
		SendCmdAck(ACK, msgbuffer);
		for (i = 0; i < pump.panelCnt; i++) {
			ret = ChangeFP_State(node, 0x21 + i, FP_State_Idle, NULL);
		}
	} else {
		ret = ChangeFP_State(node, pos + 0x21, FP_State_Idle, msgbuffer);
	}

	return ret;
}

/*
 * func: Close_FP
 */
static int DoClose(int pos, __u8 *msgbuffer) 
{
	int i, ret;

	if (pos == 255) {
		SendCmdAck(ACK, msgbuffer);
		for (i = 0; i < pump.panelCnt; i++) {			
			ret = ChangeFP_State(node, 0x21 + i, FP_State_Closed, NULL);
		}
	} else {
		ret = ChangeFP_State(node, pos + 0x21, FP_State_Closed, msgbuffer);
	}
	
	return ret;
}

// Only full mode? cann't be changed.
static int DoMode() {
	return -1;
}

/*
 * func: PreSet/PrePay & Release_FP
 */
static int DoRation(int pos)  
{
	int ret;
	int trycnt;
	int ration_type;
	__u8 pre_volume[3] = {0};
	static __u8 chain_no[2] = {0, 0};  // 链号

	if (pump.cmd[0] == 0x3D) {   // Preset
		ration_type = 0x02;
		memcpy(pre_volume, &lpDispenser_data->fp_data[pos].Remote_Volume_Preset[2], 3);
	} else {                     // Prepay
		ration_type = 0x01;
		memcpy(pre_volume, &lpDispenser_data->fp_data[pos].Remote_Amount_Prepay[2], 3);
	}

	BBcdInc(chain_no, 2);  // 链号增1, 0001 - 9999
	for (trycnt = 0; trycnt < TRY_TIME; trycnt++) {

		//----------------------------------
		//收发数据前先清空TCP接收通道
		pump.cmdlen = ERR_LENGTH;
		ret = __TcpRecvDataInTime(pump.fd, pump.cmd, &(pump.cmdlen), ERR_TCP_TIMEOUT);
		//-------------------------------------
		
		// Step.1 发送预置命令
		if (ration_type == 0x01) {
			pump.cmd[0] = 0x11;    // command code
		} else {
			pump.cmd[0] = 0x22;   // command code
		}
		

		
		//pump.cmd[0] = 0x55; 
		pump.cmd[1] = pos + 1; 
		pump.cmd[2] = 0; //Fueling point auth.
		pump.cmd[3] = 5;         // command length
		
		pump.wantLen = 4;
		pump.cmdlen = 9;
		if (ration_type == 0x01) {
			memcpy(&pump.cmd[4] ,&lpDispenser_data->fp_data[pos].Remote_Amount_Prepay,5);  
			
		} else {
			memcpy(&pump.cmd[4] ,&lpDispenser_data->fp_data[pos].Remote_Volume_Preset,5);  
		}

		Ssleep(INTERVAL);
		ret = __TcpSendDataInTime(pump.fd, pump.cmd, pump.cmdlen, TCP_TIMEOUT);
		if (ret < 0) {
			RunLog("写预置量到Node[%d].FP[%d]失败", node, pos + 1);
			continue;
		}
		
		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = __TcpRecvDataInTime(pump.fd, pump.cmd, &(pump.cmdlen), TCP_TIMEOUT);
		if (ret < 0) {
			RunLog("写预置量到Node[%d].FP[%d]成功, 取返回信息失败", node, pos + 1);
			continue;
		} 
		RunLog("取预置返回:[%s]", Asc2Hex(pump.cmd, 4));
		if ((pump.cmd[0] !=0x11) &&( pump.cmd[0] !=0x22)) {
			RunLog("执行预置加油命令失败");
			continue;
		}
		
		if (pump.cmd[3] != 0) {
			RunLog("执行预置加油命令失败");
			continue;
		}

		RunLog("写预置量到Node[%d].FP[%d]成功", node, pos + 1);
			
		return 0; 
	}

	return -1;
}

/*
 * func: 接收油品单价, 存入newPrice[][], 在IDLE状态执行变价
 */
static int DoPrice()
{
	__u8 pr_no;
	
	pr_no  = pump.cmd[2] - 0x41; //油品地址到油品编号0,1,...7.
	RunLog("变价 %02x %02x %d", pump.cmd[8], pump.cmd[9], pr_no);
	/*if ((0x04 != pump.cmd[8]) || (0x00 != pump.cmd[9]) || pr_no > 7) {
		RunLog("变价信息错误,不能下发");
		return -1;
	}*/
	
	if ((pump.newPrice[pr_no][2] | pump.newPrice[pr_no][3]) == 0) {
		// 上次执行变价(ChangePrice)后第一次收到WriteFM发来的变价命令
		pump.newPrice[pr_no][0] = 1;
	} else {
		pump.newPrice[pr_no][0]++;
	}

	// 不管是否是连续收到同种油品的变价命令, 也不管单价是否一致, 均使用新单价覆盖老单价
	pump.newPrice[pr_no][1] = pump.cmd[9];
	pump.newPrice[pr_no][2] = pump.cmd[10];
	pump.newPrice[pr_no][3] = pump.cmd[11];
#ifdef DEBUG
	RunLog("Recv New Price-PR_ID[%d]TIMES[%d] %02x%02x.%02x",
		pr_no + 1, pump.newPrice[pr_no][0], pump.newPrice[pr_no][1], pump.newPrice[pr_no][2], pump.newPrice[pr_no][3]);
#endif

	return 0;
}


/*
 * func: 执行变价, 失败改变FP状态为INOPERATIVE, 并写错误数据库
 */
static int ChangePrice() 
{
	int i, j;
	int ret;
	int trycnt;
	__u8 pr_no;
	__u8 lnz;
	__u8 err_code_tmp;
	__u8 err_code;

/*
 * 遍历新单价数组newPrice[FP_PR_NB_CNT], 如果有油品需要变价--数组值不为零,
 * 则遍历所有FP, 如果油品匹配就下发变价命令
 */

	for (pr_no = 0; pr_no < FP_PR_NB_CNT; pr_no++) {
		if (pump.newPrice[pr_no][2] != 0x00 || pump.newPrice[pr_no][3] != 0x00 || pump.newPrice[pr_no][1] != 0x00) {
			for (i = 0; i < pump.panelCnt; i++) {
				err_code = 0x99;
				for (j = 0; j < MAX_FP_LN_CNT; j++) { // 不能用gunCnt,1枪可能不用
#if DEBUG
					RunLog(">>>2 lnz: %d, fp_ln_data[%d][%d].PR_Id: %d", j + 1, i, j,\
							lpDispenser_data->fp_ln_data[i][j].PR_Id);
#endif
					if (lpDispenser_data->fp_ln_data[i][j].PR_Id - 1 == pr_no) {

					err_code_tmp = 0x99;
					for (trycnt = 0; trycnt < TRY_TIME; trycnt++) {
						// Step.1 发变价命令
						
						//----------------------------------
						//收发数据前先清空TCP接收通道
						pump.cmdlen = ERR_LENGTH;
						ret = __TcpRecvDataInTime(pump.fd, pump.cmd, &(pump.cmdlen), ERR_TCP_TIMEOUT);
						//-------------------------------------
						
						pump.cmd[0] = 0x77;
						pump.cmd[1] = i+1;
						pump.cmd[2] = j+1;
						pump.cmd[3] = 0x04;
						pump.cmd[4] = 0x04;
						pump.cmd[5] = pump.newPrice[pr_no][1];
						pump.cmd[6] = pump.newPrice[pr_no][2];
						pump.cmd[7] = pump.newPrice[pr_no][3];
						pump.cmdlen = 8;
						pump.wantLen = 4;

						Ssleep(INTERVAL);
						sleep(2);
						ret = __TcpSendDataInTime(pump.fd, pump.cmd, pump.cmdlen, TCP_TIMEOUT);
						if (ret < 0) {
							RunLog("发变价命令到Node[%d].FP[%d]失败", node, i + 1);
							continue;
						}

						// Step.2 根据命令返回信息或者重读油机单价确认变价是否成功
						bzero(pump.cmd, pump.wantLen);
						pump.cmdlen = pump.wantLen;
						ret = __TcpRecvDataInTime(pump.fd, pump.cmd, &(pump.cmdlen), TCP_TIMEOUT);
						if (ret < 0) {
							RunLog("发变价命令到Node[%d].FP[%d]成功, 取返回信息失败", node, i + 1);
							//trycnt = TRY_TIME;  // Note. 不能直接返回, 需清零单价	
							continue;//break;
						}
						
						if ( (pump.cmd[0] != 0x77)||(pump.cmd[3] != 0x00) ) {
							RunLog("加油点变价错误");
							trycnt = TRY_TIME;  // Note.
							RunLog("取变价返回:[%s]", Asc2Hex(pump.cmd, 42));
							err_code_tmp = 0x37;
							break;
						}
						
						// Step.3 写新单价到油品数据库 & 清零单价缓存
#if DEBUG
						RunLog("发送变价命令成功,新单价fp_fm_data[%d][0].Prode_Price: %s", pr_no,
							Asc2Hex(lpDispenser_data->fp_fm_data[pr_no][lpDispenser_data->fp_data[i].Default_Fuelling_Mode - 1].Prod_Price, 4));
#endif
						err_code_tmp = 0x99;
						break;
					}

					// Step.4 错误判断, 若出错则写错误数据库/置FP状态为 Inoperative
					if (trycnt >= TRY_TIME) {
						// Write Error Code: 0x36/0x37 - Change price Error (Spare)
						if (0x37 != err_code_tmp) {
							err_code_tmp = 0x36;
						}
					}

					if (err_code == 0x99) {
						// 该面有任何油枪变价失败, 则算整个FP变价失败.
						err_code = err_code_tmp;
					}

					} // end of if.PR_Id match
				//	break;  // 如果加油机能够根据指令信息处理多枪同油品的情况，那这里需要break.
				} // end of for.j < LN_CNT

				RunLog("---------- Node[%d]FP[%d] ER_DATA, error code: %02x -----", node, i + 1, err_code);

				int tmp_idx;
				for (tmp_idx = 0; tmp_idx < pump.newPrice[pr_no][0]; tmp_idx++) {
					ChangeER_DATA(node, 0x21 + i, err_code, NULL); // Translater已经回复了ACK
				}

				if (err_code != 0x99) {
					ChangeFP_State(node, 0x21 + i, FP_State_Inoperative, NULL);
				}

				// 保证 2x 命令的间隔达到PRICE_CHANGE_RETRY_INTERVAL(2s) (读单价前 1s + 这里 1s)
				Ssleep(PRICE_CHANGE_RETRY_INTERVAL - 1000000);
			} // end of for.i < panelCnt
			bzero(&pump.newPrice[pr_no], 4);
		} // end of if.newPrice
	} // end of for.pr_no

	return 0;
}


/*
 * func: 加油结束，进行交易记录的读取和处理
 *       零交易的判断和处理由DoEot来做, DoPoling 不关心
 * ret : 0 - 成功; -1 - 失败
 *
 */
/*
 * 修改建议&实现说明:  需要从油机读取的数据项有 当次加油量 & 加油金额 &
 *                     单价 & 油量总累 [& 金额总累] , 分别更新相应的数据库项
 *                     
 */
static int DoEot(int pos)
{
	int i,k;
	int ret;
	int pr_no;
	__u8 price[5];
	__u8 volume[5];
	__u8 amount[5];
	__u8 total[7];
	__u8 pre_total[7];
	__u8 tmp_total[GUN_EXTEND_LEN];
	__u8 buffer[3];

	RunLog(">> DoEot, FP[%d]", pos + 1);
	if(0== fpUpFlag[pos]){
		k = 0;
		RunLog("Up gun error, not up gun[%d]", fpUpFlag[pos]);
		return -1;
	}else{
		k = fpUpFlag[pos]-1;//枪位置
		RunLog("Get node %d fp %d gun %d trasaction ", node, pos+1, fpUpFlag[pos]);
              pump.gunLog[pos] = fpUpFlag[pos];
	}
	ret = GetTransData(pos,k);	// 取当次交易记录	
	if (ret != 0) {
		return -1;
	}
	
	k = pump.cmd[2];//读取的枪号
	pump.gunLog[pos] =  pump.cmd[2];//读取的枪号
	memcpy(total,&pump.cmd[19],7);
	

	pr_no = (lpDispenser_data->fp_ln_data[pos][k- 1]).PR_Id - 1;  //pump.gunLog[pos]  0 - 7	
	if ((pr_no < 0) || (pr_no > 7)) {
		RunLog("DoEot PR_Id error, PR_Id[%d]", pr_no + 1);
		return -1;
	}
	memcpy(pre_total, (lpDispenser_data->fp_ln_data[pos][k- 1]).Log_Noz_Vol_Total, 7);//pump.gunLog[pos] 

	if (memcmp(total, pre_total, 7) != 0) {
//		RunLog("泵码有变化,前[%s]:后[%s] ",Asc2Hex(pre_total, 7),Asc2Hex(total, 7));

		// Amount
		memcpy(amount,&pump.cmd[14],5);
		memcpy(lpDispenser_data->fp_data[pos].Current_Amount, amount, 5);

		// Volume
		memcpy(volume,&pump.cmd[9],5);
		memcpy(lpDispenser_data->fp_data[pos].Current_Volume, volume, 5);

		// Price
		memcpy(price,&pump.cmd[5],4);
		memcpy(lpDispenser_data->fp_data[pos].Current_Unit_Price, price, 4);
		
		memcpy(lpDispenser_data->fp_m_data[pr_no].Meter_Total, total, 7);
		memcpy((lpDispenser_data->fp_ln_data[pos][k - 1]).Log_Noz_Vol_Total, total, 7);//pump.gunLog[pos]


#if DEBUG
		RunLog("EOT.fp_m_data[%d].Meter_Total[%s] ", pr_no, Asc2Hex(total, 7));
		RunLog("EOT.fp_data[%d].Current_Unit_Price[%s]", pos, Asc2Hex(price, 4));
#endif
		// Update Amount_Total
		memcpy(total,&pump.cmd[26],7);
		memcpy((lpDispenser_data->fp_ln_data[pos][ k- 1]).Log_Noz_Amount_Total, total, 7);//pump.gunLog[pos]

		memcpy(lpDispenser_data->fp_data[pos].Current_Prod_Nb, lpDispenser_data->fp_pr_data[pr_no].Prod_Nb, 4);

//		ret = EndTr4Ln(node, pos + 0x21, pump.gunLog[pos] + 0x10, volume, amount, price);
		ret = EndTr4Ln(node, pos + 0x21, k + 0x10);//pump.gunLog[pos]
		if (ret < 0) {
			RunLog("DoEot.EndTr4Ln 失败, ret: %d", ret);
		}

		ret = EndTr4Fp(node, pos + 0x21, pre_total, 
			(lpDispenser_data->fp_ln_data[pos][k - 1]).Log_Noz_Vol_Total);//pump.gunLog[pos]
		if (ret < 0) {
			RunLog("DoEot.EndTr4Fp 失败, ret: %d", ret);
		}

	} else {
		RunLog("泵码没有变化 ,前泵码[%s],现泵码[%s]",Asc2Hex(pre_total, 7), Asc2Hex(total, 7));
		//memcpy((lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Vol_Total, total, 7);
		ret = EndZeroTr4Fp(node, pos + 0x21);
		if (ret < 0) {
			RunLog("DoEot.EndZeroTr4Fp 失败, ret: %d", ret);			
		}
	}
	RunLog("node %d fp %d gun %d transaction has been read. ", node, pos+1, fpUpFlag[pos]);
	fpUpFlag[pos] = 0;//挂枪取交易完毕
	return 0;
}


/*
 * func: 获取该FP所有枪的泵码
 */
static int GetPumpTotal(int pos,int gunPos)
{
	int ret;
	int trycnt;

	for (trycnt = 0; trycnt <TRY_TIME ; trycnt++) {
		
		//----------------------------------
		//收发数据前先清空TCP接收通道
		pump.cmdlen = ERR_LENGTH;
		ret = __TcpRecvDataInTime(pump.fd, pump.cmd, &(pump.cmdlen), ERR_TCP_TIMEOUT);
		//-------------------------------------
		
		pump.cmd[0] = 0x44;
		pump.cmd[1] = pos +1 ;
		pump.cmd[2] = gunPos+1;
		pump.cmd[3] = 0x00;
		pump.cmdlen = 4;
		pump.wantLen = 18;
		
		Ssleep(INTERVAL);
		ret = __TcpSendDataInTime(pump.fd, pump.cmd, pump.cmdlen, TCP_TIMEOUT);
		if (ret < 0) {
			RunLog("发 取泵码命令 到Node[%d].FP[%d]失败", node, pos + 1);
			continue;
		}

		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = __TcpRecvDataInTime(pump.fd, pump.cmd, &(pump.cmdlen), TCP_TIMEOUT);
		if (ret < 0) {
			RunLog("发 取泵码命令 到Node[%d].FP[%d]成功, 取返回信息失败", node, pos + 1);
			continue;
		}
		RunLog("取泵码返回:[%s]", Asc2Hex(pump.cmd, 18));
		if ( (pump.cmd[0] != 0x44)||(pump.cmd[3] != 0x0E) ) {
			RunLog("加油点取泵码错误");
			continue;
		}

		return 0;
	}

	return -1;
}

/*
 * func: 读取当次交易记录
 * return: 0 - 成功, 1 - 成功但无交易, -1 - 失败
 */ 
static int GetTransData(int pos, int gunPos)
{
	int ret;
	int trycnt;
	int ack_cmdlen;
	int ack_wantLen;
	__u8 status;
	__u8 ack_cmd[6];

	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {
		
		//----------------------------------
		//收发数据前先清空TCP接收通道
		pump.cmdlen = ERR_LENGTH;
		ret = __TcpRecvDataInTime(pump.fd, pump.cmd, &(pump.cmdlen), ERR_TCP_TIMEOUT);
		//-------------------------------------
		
		// Step.1 Read Transaction Data
		pump.cmd[0] = 0xAA;
		pump.cmd[1] = pos+1;
		pump.cmd[2] = gunPos +1;
		pump.cmd[3] = 0x00;
		pump.cmdlen = 4;
		pump.wantLen = 33;

		Ssleep(INTERVAL);
		ret = __TcpSendDataInTime(pump.fd, pump.cmd, pump.cmdlen, TCP_TIMEOUT);
		if (ret < 0) {
			RunLog("发读交易数据命令到Node[%d].FP[%d]失败", node, pos + 1);
			continue;
		}

		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = __TcpRecvDataInTime(pump.fd, pump.cmd, &(pump.cmdlen), TCP_TIMEOUT);
		if (ret < 0) {
			RunLog("取交易数据返回:[%s]", Asc2Hex(pump.cmd, 31));
			RunLog("发读交易数据命令到Node[%d].FP[%d]成功, 取返回信息失败", node, pos + 1);
			continue;
		}
		RunLog("取交易数据返回:[%s]", Asc2Hex(pump.cmd, 33));
		if ( (pump.cmd[0] != 0xAA)||(pump.cmd[3] != 0x1D) ) {
			RunLog("加油点取交易数据错误");
			continue;
		}

		return 0;
	}

	return -1;
}


static int DoStop( int pos )
{
	int ret;
	unsigned char pump_id;

	RunLog( "DoStop" );
	return 0;	//2006.02 to kill the 3x command

}
/*不用
 * func: 暂停加油，在此做Terminate_FP用
 */
static int _DoStop(int pos)
{
	int i;
	int ret;
	int trycnt;
	int maxCnt;
	__u8 status;

	RunLog("DoStop");

	if (pos == 255)	{
		i = 0;
		maxCnt = pump.panelCnt;
	} else {
		i = pos;
		maxCnt = pos + 1;
	}

	for (; i < maxCnt; i++) {
		for (trycnt = 0; trycnt < TRY_TIME; trycnt++) {
			pump.cmd[0] = 0xFC;
			pump.cmd[1] = 0x05;
			pump.cmd[2] = pump.panelPhy[pos];
			pump.cmd[3] = pump.cmd[1] ^ pump.cmd[2];
			pump.cmdlen = 4;
			pump.wantLen = 5;

			Ssleep(INTERVAL);
			ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
					pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
			if (ret < 0) {
				RunLog("发 终止加油命令 到Node[%d].FP[%d]失败", node, pos + 1);
				continue;
			}

			bzero(pump.cmd, pump.wantLen);
			pump.cmdlen = pump.wantLen;
			ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
			if (ret < 0) {
				RunLog("发 终止加油命令 到Node[%d].FP[%d]成功, 取返回信息失败", node, pos + 1);
				continue;
			}

			if (pump.cmd[2] != pump.panelPhy[pos]) {
				RunLog("加油点号错误");
				continue;
			}

			if (pump.cmd[pump.wantLen - 1] != Check_xor(&pump.cmd[1], pump.wantLen - 2)) {
				RunLog("Return Message XOR ERROR");
			}

			if ((pump.cmd[3] & 0x0F) == FP_IDLE) {
				break;
			}
		}

		if (trycnt == TRY_TIME) {
			return -1;
		}
	}
	return 0;
}

/*
 * func: Release_FP
 */
static int DoAuth(int pos)
{
	int i;
	int ret;
	int trycnt;
	int maxCnt;
	__u8 status;

	RunLog("DoAuth");

	if (pos == 255)	{
		i = 0;
		maxCnt = pump.panelCnt;
	} else {
		i = pos;
		maxCnt = pos + 1;
	}

	for (; i < maxCnt; i++) {
		for (trycnt = 0; trycnt < TRY_TIME; trycnt++) {
				
			//----------------------------------
			//收发数据前先清空TCP接收通道
			pump.cmdlen = ERR_LENGTH;
			RunLog("Send Authorised Cmd %s", Asc2Hex(pump.cmd, pump.cmdlen));
			ret = __TcpRecvDataInTime(pump.fd, pump.cmd, &(pump.cmdlen), ERR_TCP_TIMEOUT);
			//-------------------------------------
			
			pump.cmd[0] = 0xFF;
			pump.cmd[1] = pos + 1;
			pump.cmd[2] = 0x00;//面板授权
			pump.cmd[3] = 0x00;
			pump.cmdlen = 4;
			pump.wantLen = 4;

			Ssleep(INTERVAL);
			ret = __TcpSendDataInTime(pump.fd, pump.cmd, pump.cmdlen, TCP_TIMEOUT);
			if (ret < 0) {
				RunLog("发授权命令到Node[%d].FP[%d]失败", node, i + 1);
				continue;
			}

			bzero(pump.cmd, pump.wantLen);
			pump.cmdlen = pump.wantLen;
			ret = __TcpRecvDataInTime(pump.fd, pump.cmd, &(pump.cmdlen), TCP_TIMEOUT);
			if (ret < 0) {
				RunLog("发授权命令到Node[%d].FP[%d]成功, 取返回信息失败", node, i + 1);
				continue;
			}
			RunLog("取授权返回:[%s]", Asc2Hex(pump.cmd, 4));
			if ( (pump.cmd[0] != 0xFF)||(pump.cmd[3] != 0) ) {
				RunLog("加油点取授权命令错误");
				continue;
			}else{
				break;
			}
			
		}

		if (trycnt == TRY_TIME) {
			return -1;
		}
	}

	return 0;
}

/*
 * func: 读取并上传实时加油数据
 */
static int DoBusy(int pos)
{
	int ret, freq;
	int k;
	int trycnt;
	__u8 amount[5] = {0};
	__u8 volume[5] = {0};
	time_t time_tmp;
	static time_t running_msg_time[MAX_FP_CNT] = {0};

	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {
		
		//----------------------------------
		//收发数据前先清空TCP接收通道
		pump.cmdlen = ERR_LENGTH;
		ret = __TcpRecvDataInTime(pump.fd, pump.cmd, &(pump.cmdlen), ERR_TCP_TIMEOUT);
		//-------------------------------------
		
		// Step.1 读取实时数据
		ret = GetCurrentLNZ(pos);
		if (ret < 0) {
			RunLog("获取当前油枪枪号失败");
			return -1;
		}
		k = pump.gunLog[pos];
		pump.cmd[0] = 0x33;
		pump.cmd[1] = pos +1;
		pump.cmd[2] = k;
		pump.cmd[3] = 0x00;
		pump.cmdlen = 4;
		pump.wantLen = 18;

		Ssleep(INTERVAL); 
		ret = __TcpSendDataInTime(pump.fd, pump.cmd, pump.cmdlen, TCP_TIMEOUT);
		if (ret < 0) {
			RunLog("发 取实时加油数据 命令到Node[%d].FP[%d]失败", node, pos + 1);
			continue;
		}

		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = __TcpRecvDataInTime(pump.fd, pump.cmd, &(pump.cmdlen), TCP_TIMEOUT);
		if (ret < 0) {
			RunLog("发 取实时加油数据 命令到Node[%d].FP[%d]成功, 取返回信息失败", node, pos + 1);
			continue;
		}
		RunLog("取实时加油数据返回:[%s]", Asc2Hex(pump.cmd, 18));
		if ( (pump.cmd[0] != 0x33)||(pump.cmd[3] != 0x0E) ) {
				RunLog("加油点取实时加油数据错误");
				continue;
		}

		// Step.2  Exec ChangeFP_Run_Trans()
		memcpy(volume, &pump.cmd[8], 5);
		memcpy(amount, &pump.cmd[13], 5);

		time_tmp = time(NULL);
		freq =	(((lpDispenser_data->fp_data[pos].Running_Transaction_Message_Frequency[1] >> 4) * 100) +
				((lpDispenser_data->fp_data[pos].Running_Transaction_Message_Frequency[1] & 0x0F) * 10) +
				(lpDispenser_data->fp_data[pos].Running_Transaction_Message_Frequency[0] >> 4));
		if ((time_tmp >= running_msg_time[pos] + freq) && freq != 0) {
			running_msg_time[pos] = time_tmp;

			RunLog("test -----------------------------------------------");
			ret = ChangeFP_Run_Trans(node, 0x21 + pos, amount, volume);
		}
			RunLog("--- freq = [%d] ------------------------------------", freq);
			RunLog("time_tmp = [%d] ;  running_msg_time = [%d]", time_tmp, running_msg_time[pos]);

		return 0;
	}

	return -1;
}

/*
 * func: 抬枪处理 
 * 获取当前抬起的逻辑枪号,设置fp_data[].Log_Noz_State & 改变FP状态为Calling
 */
static int DoCall(int pos)
{
	int ret;

	ret = GetCurrentLNZ(pos);
	if (ret < 0) {
		RunLog("获取当前油枪枪号失败");
		return -1;
	}

	ret = ChangeFP_State(node, pos + 0x21, FP_State_Calling, NULL);

	//上位机在线,但是允许自动授权;上位机不在线, 授权开关置于开的状态,此时自动授权
	if ((gpIfsf_shm->auth_flag == FCC_SWITCH_AUTH) && (lock_status() == 1)) {
		ret = DoAuth(pos);
		if (ret < 0) {
			RunLog("自动授权失败");
		}
		ret = ChangeFP_State(node, pos + 0x21, FP_State_Started, NULL);
		RunLog("自动授权成功");
		SO_OFFLINE_TR(node, pos);
	}

	return ret;
}

/*
 * func: 读取油机单价 密度 时间
 * 	prince:pump.cmd[4]pump.cmd[5] (BCD)
 */

static int GetPrice(int pos,int gunPos)
{
	int ret;
	int trycnt;

	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {
		
		//----------------------------------
		//收发数据前先清空TCP接收通道
		pump.cmdlen = ERR_LENGTH;
		ret = __TcpRecvDataInTime(pump.fd, pump.cmd, &(pump.cmdlen), ERR_TCP_TIMEOUT);
		//-------------------------------------
		
		pump.cmd[0] = 0x66;
		pump.cmd[2] = pos+1;
		pump.cmd[3] = gunPos +1;
		pump.cmd[4] = 0x0;
		pump.cmdlen = 4;
		pump.wantLen = 8;
		
		Ssleep(INTERVAL);
		ret = __TcpSendDataInTime(pump.fd, pump.cmd, pump.cmdlen, TCP_TIMEOUT);
		if (ret < 0) {
			RunLog("发读取油机单价命令到Node[%d].FP[%d]失败", node, pos + 1);
			continue;
		}

		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = __TcpRecvDataInTime(pump.fd, pump.cmd, &(pump.cmdlen), TCP_TIMEOUT);
		if (ret < 0) {
			RunLog("取返回信息失败");
			continue;
		} 
		RunLog("取油机单价返回:[%s]", Asc2Hex(pump.cmd, 8));
		if ( (pump.cmd[0] != 0x66)||(pump.cmd[3] != 0x04) ) {
				RunLog("加油点取单价错误");
				continue;
		}

		return 0;
	}

	return -1;
}

/*
 * func: 读取实时状态
 */

static int GetPumpStatus(int pos, __u8 *status)
{
	int trycnt;
	int ret;

	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {
		
		//----------------------------------
		//收发数据前先清空TCP接收通道
		pump.cmdlen = ERR_LENGTH;
		//ret = __TcpRecvDataInTime(pump.fd, pump.cmd, &(pump.cmdlen), ERR_TCP_TIMEOUT);
		//-------------------------------------
		pump.cmd[0] = 0x55;
		pump.cmd[1] = pos +1;
		pump.cmd[2] = 0x0;
		pump.cmd[3] = 0x0;
		pump.cmdlen = 4;
		pump.wantLen = 5;
		Ssleep(INTERVAL);
		ret = __TcpSendDataInTime(pump.fd, pump.cmd, pump.cmdlen, TCP_TIMEOUT);
		if (ret < 0) {
			RunLog("发取状态命令到Node[%d].FP[%d]失败", node, pos + 1);
			continue;
		}
		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = __TcpRecvDataInTime(pump.fd, pump.cmd, &(pump.cmdlen), TCP_TIMEOUT);
		if (ret < 0) {
			RunLog("发取状态命令到Node[%d].FP[%d]成功, 取返回信息失败", node, pos + 1);
			continue;
		} 
		RunLog("Node [%d], 取状态返回:[%s]",node, Asc2Hex(pump.cmd, 5));
			
		if ( (pump.cmd[0] != 0x55)||(pump.cmd[3] != 0x01) ) {
				RunLog("加油点取状态错误");
				continue;
		}

		*status = pump.cmd[4];

		return 0;
	}

	return -1;
}

//test wether the dispenser is online, we only use fp 1 to test.
/*
 * func: 判断Dispenser是否在线
 */

static int GetPumpOnline(int pos, __u8 *status)
{
	int ret;
	int trycnt;

	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {         // Note. GetPumpStatus 循环变量最好用本地的,不要用trycnt
		
		//----------------------------------
		//收发数据前先清空TCP接收通道
		pump.cmdlen = ERR_LENGTH;
		ret = __TcpRecvDataInTime(pump.fd, pump.cmd, &(pump.cmdlen), ERR_TCP_TIMEOUT);
		//-------------------------------------
		
		pump.cmd[0] = 0x55;
		pump.cmd[1] = pos +1;
		pump.cmd[2] = 0x0;
		pump.cmd[3] = 0x0;
		pump.cmdlen = 4;
		pump.wantLen = 5;

		Ssleep(INTERVAL);
		ret = __TcpSendDataInTime(pump.fd, pump.cmd, pump.cmdlen, TCP_TIMEOUT);
		if (ret < 0) {
			RunLog("发取状态命令到Node[%d].FP[%d]失败", node, pos + 1);
			continue;
		}

		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = __TcpRecvDataInTime(pump.fd, pump.cmd, &(pump.cmdlen), TCP_TIMEOUT);
		if (ret < 0) {
			RunLog("发取状态命令到Node[%d].FP[%d]成功, 取返回信息失败", node, pos + 1);
			continue;
		} 
		RunLog("取在线状态返回:[%s]", Asc2Hex(pump.cmd, 5));
		if ( (pump.cmd[0] != 0x55)||(pump.cmd[3] != 0x01) ) {
				RunLog("加油点取状态错误");
				continue;
		}

		*status = pump.cmd[4];

		return 0;
	}

	return -1;
}

/*
 * func: 获取当前抬起油枪的枪号, 结果写入pump.gunLog[] & fp_data[].Log_Noz_State
 * ret : 0 - 成功, -1 - 失败 
 */
static inline int GetCurrentLNZ(int pos)
{
	RunLog("Node %d FP %d 的 %d 号枪抬起", node, pos + 1, pump.gunLog[pos]);
	lpDispenser_data->fp_data[pos].Log_Noz_State = 1 << (pump.gunLog[pos] - 1);

	return 0;
}

/*
 * func: 初始的pump.gunLog, 预置加油指令需要; 不修改 Log_Noz_State
 */
static int InitGunLog()
{
	int i;

	for (i = 0; i < pump.panelCnt; ++i) {
		pump.gunLog[i] = 1;//当前动枪
	}

	return 0;
}

/*
 * 初始化FP的当前油价. 从配置文件中读取单价数据,
 * 写入 fp_data[].Current_Unit_Price
 * 假设 PR_Id 与 PR_Nb 是一一对应的
 */
static int InitCurrentPrice() {
	int i, j;
	int ret;
	int pr_no;
	__u8 price[4];

	for (i = 0; i < pump.panelCnt; i++) {
		for (j = 0; j < pump.gunCnt[i]; j++) {
			pr_no = (lpDispenser_data->fp_ln_data[i][0]).PR_Id - 1;  // 0-7	
			if ((pr_no < 0) || (pr_no > 7)) {
				RunLog("InitPumpPrice PR_Id error, PR_Id[%d]", pr_no + 1);
				return -1;
			}

			memcpy(lpDispenser_data->fp_data[i].Current_Unit_Price, 
					lpDispenser_data->fp_fm_data[pr_no][lpDispenser_data->fp_data[i].Default_Fuelling_Mode - 1].Prod_Price, 4);
#ifdef DEBUG
		RunLog("Init.FP %d LNZ %d PR_Id %d,Prod_Pricd: %02x%02x%02x%02x", i + 1, pump.gunLog[i], pr_no + 1,
				lpDispenser_data->fp_fm_data[pr_no][lpDispenser_data->fp_data[i].Default_Fuelling_Mode - 1].Prod_Price[0],
				lpDispenser_data->fp_fm_data[pr_no][lpDispenser_data->fp_data[i].Default_Fuelling_Mode - 1].Prod_Price[1],
				lpDispenser_data->fp_fm_data[pr_no][lpDispenser_data->fp_data[i].Default_Fuelling_Mode - 1].Prod_Price[2],
				lpDispenser_data->fp_fm_data[pr_no][lpDispenser_data->fp_data[i].Default_Fuelling_Mode - 1].Prod_Price[3]);
#endif
		}
	}


	return 0;
}

/*
 * func: 初始化油枪泵码
 */
static int InitPumpTotal()
{
	int ret;
	int i,j;
	int pr_no;
	__u8 lnz = 0;     // fix value
	__u8 total[7] = {0};

	for (i = 0; i < pump.panelCnt; i++) {
		for (j = 0; j < pump.gunCnt[i]; j++) {
			ret = GetPumpTotal(i,j);
			if (ret < 0) {
				RunLog("取 FP%d 的泵码失败", i);
				return -1;
			}

			// Volume_Total
			
			pr_no = (lpDispenser_data->fp_ln_data[i][j]).PR_Id - 1;  // 0-7	lnz
			if ((pr_no < 0) || (pr_no > 7)) {
				RunLog("InitPumpTotal,FP %d, LNZ %d, PR_Id %d", i, lnz + 1, pr_no + 1);
			}
			memcpy(total, &pump.cmd[4], 7);
			memcpy(lpDispenser_data->fp_m_data[pr_no].Meter_Total, total, 7);
			memcpy((lpDispenser_data->fp_ln_data[i][j]).Log_Noz_Vol_Total, total, 7);//pump.gunLog[i] - 1
#if DEBUG
			RunLog("Init.fp_ln_data[%d][%d].Vol_Total: %s ", i, j, //pump.gunLog[i] - 1,
					Asc2Hex(lpDispenser_data->fp_ln_data[i][j].Log_Noz_Vol_Total, 7));//00
			RunLog("Init.fp_m_data[%d].Meter_Total[%s] ", pr_no, Asc2Hex(total, 7));
			RunLog("逻辑枪号[%d]的泵码为[%s]", 
				 j+ 0x01, Asc2Hex(&pump.cmd[4],7 ));//lnz GUN_EXTEND_LEN
#endif
			// Amount_Total
			memcpy(total, &pump.cmd[11], 7);
			memcpy((lpDispenser_data->fp_ln_data[i][j]).Log_Noz_Amount_Total, total, 7);//pump.gunLog[i] - 1
			RunLog("Init.fp_ln_data[%d][%d].Amount_Total: %s ", i, j, //pump.gunLog[i] - 1,
					Asc2Hex(lpDispenser_data->fp_ln_data[i][j].Log_Noz_Amount_Total, 7));//0 0
			sleep(1);
		}
	}

	return 0;
}

/*无，不用
 * func: 设置控制模式
 * incomplete
 *
 */

static int SetCtrlMode(int pos, __u8 mode)
{
	int trycnt;
	int ret;

	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {         // Note. GetPumpStatus 循环变量最好用本地的,不要用trycnt
		pump.cmd[0] = 0xFC;
		pump.cmd[1] = 0x08;
		pump.cmd[2] = pump.panelPhy[pos];
		pump.cmd[3] = mode;
		pump.cmd[4] = pump.cmd[1] ^ pump.cmd[2] ^ pump.cmd[3];
		pump.cmdlen = 5;
		pump.wantLen = 5;

		Ssleep(INTERVAL);
		ret = __TcpSendDataInTime(pump.fd, pump.cmd, pump.cmdlen, TCP_TIMEOUT);
		if (ret < 0) {
			RunLog("发设置控制模式命令到Node[%d].FP[%d]失败", node, pos + 1);
			continue;
		}

		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("发设置控制模式命令到Node[%d].FP[%d]成功, 取返回信息失败", node, pos + 1);
			continue;
		} 

		if (pump.cmd[2] != pump.panelPhy[pos]) {
			RunLog("加油点号错误");
			continue;
		}

		if (pump.cmd[pump.wantLen - 1] != Check_xor(&pump.cmd[1], pump.wantLen - 2)) {
			RunLog("Return Message XOR ERROR");
                       continue;
               }

               return 0;
       }

       RunLog("改变模式 node %d pos %d", node, pos);
       return -1;
}


/* ----------------- End of lischn016.c ------------------- */



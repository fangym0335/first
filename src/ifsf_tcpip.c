/*******************************************************************
1.本设计中每个待转换协议的非IFSF设备都转换为IFSF设备，拥有自己的心跳,
心跳服务进程两个, 一个发送所有待协议转换设备的心跳,一个接收IFSF设备的心跳
2.本设计中所有待转换协议的非IFSF设备共用一个TCP服务进程,具体请参见ifsf_main文件.
3.IFSF的TCPIP通讯中最重要的数据结构是IFSF_HEARTBEAT，结构IFSF_HEARTBEAT是一个有心跳的设备的
具体描述，是心跳程序要发送的10个字节的结构。IFSF_HB_SHM定义了一个存放在共享内存中的
设备心跳表，用于管理当前所有应当有心跳的设备的设备描述，类似于组合类, 
也类似于一个数据库中的表，使用时注意，适当使用P，V操作。
4.!!!开机后IFSF FCC PCD查找配置文件pcd.cfg,如果没有该文件或者文件错误,
那么系统启动一个tcp进程阻塞等待配置,然后根据pcd.cfg文件中的配置情况
初始化gpIfsf_shm和gpIfsf_shm,初始化所有加油机,各个加油点置为Closed状态,
继续请求CD 设置控制模式, 下发油价,设置油品,设置油品油枪关系,
最后打开加油机(Open_FP),该命令会把心跳状态置为正常(全0),使得加油机处于
Idle状态,开始加油.
-------------------------------------------------------------------
2007-06-21 by chenfm
2008-02-27 - Guojie Zhu <zhugj@css-intelligent.com>
    TCP Server 和 TCP Client均改为单向, 连接均改为常连接
    所有对SendDataBySock的调用均改为SendData2CD
2008-03-04 - Guojie Zhu <zhugj@css-intelligent.com>
    TcpRecvCDServ 更名为 TcpSend2CDServ
    增加AddTLGSendHB
2008-03-06 - Guojie Zhu <zhugj@css-intelligent.com>
    convertUsed[] 改为 convert_hb[].lnao.node
    HBSend/DelSendHB.convertUsed[] 改为 node_online[]
2008-03-26 - Guojie Zhu <zhugj@css-intelligent.com>
    当POS离线, 关闭POS.C -> FCC.S 的连接
2008-05-04 - Guojie Zhu <zhugj@css-intelligent.com>
    修改TCP Server 的处理程序, 设置server为非阻塞模式, 同时最大连接允许设置为 1
2008-05-05 - Guojie Zhu <zhugj@css-intelligent.com>
    改正重连上传的状态主动消息中消息长度错误的bug(0xE0 -> 0x0E)
2008-05-10 - Guojie Zhu <zhugj@css-intelligent.com>
    修正DoReonline中设备不在线也发送状态信息的错误
2008-05-13 - Guojie Zhu <zhugj@css-intelligent.com>
    发送心跳前增加判断,如果POS不在线则不发送心跳
    

*******************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <linux/sockios.h>
#include "ifsf_def.h"
//#include "ifsf_fp.h"
//#include "ifsf_tcpip.h"
#include "ifsf_pub.h"
#include "ifsf_dsp.h"
#include "ifsf_tlg.h"

#include "ifsf_tcpip.h"
#include "pubsock.h"
#include "tcp.h"
#include "pub.h"

#include "ifsf_tcpsv.c"

extern IFSF_SHM  *gpIfsf_shm; 
extern int HB_PORT;
extern int gCssFifoFd;
static long  GetLocalIP();
static unsigned long  GetIPMask();
static unsigned long  GetBroadCast();
static int DoReOnline();
static ssize_t TcpSendDataInTime( int fd, const char *s, size_t len,  int timeout );
static ssize_t TcpRecvDataInTime( int fd, char *s, size_t *buflen,  int timeout );
static int flag_pause_send_hb;


static int IfsfTcpConnect( const char *sIPName, const char *sPortName, int nTimeOut )
{
	struct sockaddr_in serverAddr;
	int sock;
	in_port_t port;
	in_addr_t addr;
	int ret;

	Sigfunc *OldAction;

	Assert( sIPName != NULL && *sIPName != 0 );
	Assert( sPortName != NULL && *sPortName != 0 );

	if( ( port = GetTcpPort( sPortName ) ) == INPORT_NONE )
	{
		RptLibErr( TCP_ILL_PORT );
		return -1;
	}

	if( ( addr = GetTcpAddr( sIPName ) ) == INADDR_NONE )
	{
		RptLibErr( TCP_ILL_IP );
		return -1;
	}

	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = addr;
	serverAddr.sin_port = port;

	sock = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
	if( sock < 0 )
	{
		RunLog("sock is %d errno is %s", sock, strerror(errno));
		RptLibErr( errno );
		return -1;
	}

	if( nTimeOut == TIME_INFINITE )
	{
		ret = connect( sock, (struct sockaddr *)&serverAddr, sizeof( serverAddr ) );
		if (ret != 0) RunLog("errno is %s", strerror(errno));
	}
	else
	{
		OldAction = SetAlrm( nTimeOut, NULL );  /*如果在alarm调用后,connect调用前超时,则connect等待时间为缺省时间*/
		ret = connect( sock, (struct sockaddr *)&serverAddr, sizeof( serverAddr ) );
		if (ret != 0) RunLog("errno is %s", strerror(errno));
		UnSetAlrm();
	}

	if( ret == 0 )
		return sock;

	close( sock );

	RptLibErr( errno );

	return -1;

}
int SetHbStatus(const unsigned char node,  const char status){
	int i;
	if ( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,Del 心跳失败");
		return -1;
	}
	for(i=0; i< MAX_CHANNEL_CNT; i++){
		if (gpIfsf_shm->convert_hb[i].lnao.node == node  ) {
			break; //找到了空闲加油机节点
		}
	}
	if (i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点心跳空闲数据区找不到,SetHbStatus 失败");
		return -1;
	}
	gpIfsf_shm->convert_hb[i].status = status;
		
	return 0;	
}

//把一个设备从心跳表总加入或者删除,recvsend为心跳数据区的那个区,
//删除CD类设备时要查看gpIsfs_fp_shm的交易数据区是否由它控制,是就解除控制?.
int AddSendHB(const int node){//first read IO, second add dispenser data,finally add heartbeat.
	int i;
	//int p;
	IFSF_HEARTBEAT ifsf_heartbeat;
	extern char PCD_TCP_PORT[];
	if ( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,Add 心跳失败");
		return -1;
	}
	for(i=0; i< MAX_CHANNEL_CNT; i++){
		if (  gpIfsf_shm->node_channel[i].node == node )  
			break; //找到了加油机节点
	}
	if (i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点找不到,加油机心跳Add  失败");
		return -1;
	}
	
	//UserLog("error before init ifsf_heartbeat");
	ifsf_heartbeat.host_ip =GetLocalIP();
	//UserLog("error after  1  fuction,PCD_TCP_PORT:%s",PCD_TCP_PORT);
	//Assert( PCD_TCP_PORT != NULL && atoi(PCD_TCP_PORT) != 0 );
	//ifsf_heartbeat.port = htons(atoi(PCD_TCP_PORT));
	ifsf_heartbeat.port = GetTcpPort(PCD_TCP_PORT);
	//UserLog("error after  2 fuction");
	ifsf_heartbeat.lnao.subnet = '\x01';
	ifsf_heartbeat.lnao.node = node;
	ifsf_heartbeat.ifsf_mc = '\x01';
	ifsf_heartbeat.status = '\x01'; //default is request config.
	//UserLog("error before memcpy");
	memcpy( &(gpIfsf_shm->convert_hb[i]), &ifsf_heartbeat, sizeof(IFSF_HEARTBEAT) );
	//gpIfsf_shm->convertCnt++;
		
	return 0;
}

int AddTLGSendHB(const int node) {
	IFSF_HEARTBEAT ifsf_heartbeat;
	extern char PCD_TCP_PORT[];
	if ( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,Add 心跳失败");
		return -1;
	}
	
	//UserLog("error before init ifsf_heartbeat");
	ifsf_heartbeat.host_ip =GetLocalIP();
	//UserLog("error after  1  fuction,PCD_TCP_PORT:%s",PCD_TCP_PORT);
	//Assert( PCD_TCP_PORT != NULL && atoi(PCD_TCP_PORT) != 0 );
	//ifsf_heartbeat.port = htons(atoi(PCD_TCP_PORT));
	ifsf_heartbeat.port = GetTcpPort(PCD_TCP_PORT);
	//UserLog("error after  2 fuction");
	ifsf_heartbeat.lnao.subnet = '\x09';
	ifsf_heartbeat.lnao.node = node;
	ifsf_heartbeat.ifsf_mc = '\x01';
	ifsf_heartbeat.status = '\x00'; //!default is request config.
	//UserLog("error before memcpy");
	memcpy( &(gpIfsf_shm->convert_hb[MAX_CHANNEL_CNT]), &ifsf_heartbeat, sizeof(IFSF_HEARTBEAT) );
	//gpIfsf_shm->convertCnt++;
		
	return 0;
}

int DelSendHB(const int node){//first delete dispenser data, then delete heartbeat.
	int i;
	if ( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,Del 心跳失败");
		return -1;
	}
	for(i=0; i< MAX_CHANNEL_CNT; i++){
		if (gpIfsf_shm->convert_hb[i].lnao.node == node) {
		      break; //找到了空闲加油机节点
		}
	}
	if (i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点心跳空闲数据区找不到,加油机Del  失败");
		return -1;
	}
	gpIfsf_shm->node_online[i] = 0;
	
	
	memset( &(gpIfsf_shm->convert_hb[i]), 0, sizeof(IFSF_HEARTBEAT) );
	//gpIfsf_shm->convertCnt--;
		
	return 0;
}
int AddRecvHB(const IFSF_HEARTBEAT *device){
	int i;
	
	if ( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,Add 心跳失败");
		return -1;
	}
	if ( NULL == device ){		
		UserLog("参数空地址值错误,Add 心跳失败");
		return -1;
	}
	for(i=0; i< MAX_RECV_CNT; i++){
		if (  gpIfsf_shm->recvUsed[i] <= MAX_NO_RECV_TIMES  ) 
			break; //找到了空闲设备节点
	}
	if (i == MAX_RECV_CNT ){
		UserLog("加油机节点心跳空闲数据区找不到,加油机Add  失败");
		return -1;
	}
	gpIfsf_shm->recvUsed[i] = HEARTBEAT_TIME;	
	memcpy( &(gpIfsf_shm->recv_hb[i]), device, sizeof(IFSF_HEARTBEAT) );	
	return 0;
}
int DelRecvHB(const IFSF_HEARTBEAT *device){
	int i, node;
	if ( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,Del 心跳失败");
		return -1;
	}
	if ( NULL == device ){		
		UserLog("参数空地址值错误,Del 心跳失败");
		return -1;
	}
	node = device->lnao.node;
	for(i=0; i< MAX_RECV_CNT; i++){
		if (  gpIfsf_shm->recvUsed[i] == node  )  break; //找到了加油机节点
	}
	if (i == MAX_RECV_CNT ){
		UserLog("设备节点心跳数据找不到,设备Del  失败");
		return -1;
	}
	gpIfsf_shm->recvUsed [i]= MAX_NO_RECV_TIMES;
	memset( &(gpIfsf_shm->recv_hb[i]), 0, sizeof(IFSF_HEARTBEAT) );	
	//gpIfsf_shm->resvCnt --;
			
	return 0;
}
//static int PackupDevShm(const int index);//整理设备表

//!!!  public函数,以下为其他.c文件外要调用的函数:
/*
启动TCP监听服务，所有待协议转换的设备公用一个TCP服务进程,
返回0正常启动，-1失败,
然后设置IFSF_SHM共享内存的值.  两个重要参数port和backLog参考ListenSock()在程序中实现.
注意不要接收到本机的心跳和待转换的设备心跳. 
请参考和调用tcp.h和tcp.c的函数.
本函数要调用ifsf_fp.c中的TranslateRecv()函数.
*/
/*
 * POS.Client -> POS.Server
 * 所有下行数据的通道
 */
int TcpRecvServ()
{
	char port[6];
	int sock, newsock, tmp_sock;
	int ret, i,len;
	int msg_lg = 1024;
	int timeout = 5;       // 5s
	int errcnt = 0;
	unsigned char buffer[1024];
	extern int errno;
		
	sleep(5);
	strcpy( port, PCD_TCP_PORT );	
	sock = TcpServInit(port, 1);
	if ( sock < 0 ) {
		UserLog( "初始化socket失败.port=[%s]\n", port );
		return -1;
	}
	fcntl(sock, F_SETFL, O_NONBLOCK);   // 非阻塞模式
	
	newsock  = -1;
	tmp_sock = -1;
	while (1) {	
		if (((errcnt > 32) || (gpIfsf_shm->auth_flag == FCC_SWITCH_AUTH)) && newsock > 0) {
			RunLog("Out of ERR_CNT/CD offline, Close the connection <--X---------------");
			close(newsock);
			newsock = -1;
			errcnt = 0;
		}


		// 一直监听, 如果有新链接, 则关闭前一个连接
		tmp_sock = TcpAccept(sock, NULL);
		if (tmp_sock > 0) {
			if (newsock > 0) {
				RunLog("Accept a new connection, close privous one   <--X---------------");
				close(newsock);
			}
			newsock = tmp_sock;
			tmp_sock = -1;
			RunLog("auth_flag: %d, Accept a connection(%d) from CD <-----------------",
								gpIfsf_shm->auth_flag, newsock);
#if 1
			// 一旦CD重连FCC, 那么FCC也重连CD?
			// 不能即时发现网络出了问题, 暂时只好如此 
			// (老版本:2.6.21没有加网络状态检测的版本才需要这么做)
			if (gpIfsf_shm->recvSockfd[0] >= 0 && gpIfsf_shm->is_new_kernel == 0) {
				gpIfsf_shm->need_reconnect = 1;
				RunLog("Client need to reconnect to CD");
			}
#endif
		}

		if (newsock <= 0) {
			continue;
		}

		msg_lg = sizeof(buffer);
		memset( buffer, 0, sizeof(buffer) );
//		UserLog("TcpRecvServ...before call TcpRecvDataIntime.., timeout: %d", timeout);
		ret = TcpRecvDataInTime(newsock, buffer, &msg_lg, timeout );//TIME_INFINITE
//		UserLog("TcpRecvServ...after call TcpRecvDataIntime.., ret: %d, errno: %d", ret, errno);
		if (ret < 0) {
			
			if (errno == 4) {  // time out
//				printf("超时,接收到后台命令 buffer[%s]\n",  Asc2Hex(buffer, 32));
//				RunLog("errcnt is %d errno is %d", errcnt, errno);
				continue;
			} else if ((ret == -2) || errcnt > 16 && (gpIfsf_shm->need_reconnect == 0)) { 
				 // maybe CD quit or restart
				if (gpIfsf_shm->recvSockfd[0] >= 0) {
					gpIfsf_shm->need_reconnect = 1;
					RunLog("Client need to reconnect to CD");
				}
			}	
			RunLog( "接收数据失败,errno:[%d][%s]", errno, strerror(errno) );
			RunLog("buffer: %s", Asc2Hex(buffer, 32));
			errcnt++;
			continue;
		} else {
			RunLog( "<<<收到命令: [%s(%d)]", Asc2Hex(buffer, msg_lg ), msg_lg);
			if (sizeof(buffer) == msg_lg){ //received data must be error!!
				UserLog( "received data error" );
				errcnt++;
				continue;
			}

			if (0 == msg_lg){ //receivd data must be error!!
				UserLog( "received size is  0 error" );	
				errcnt++;
				continue;
			}	
			//处理后台命令.
			if (1 == buffer[0]){     // Dispenser
			//	RunLog("call TranslateRecv ........");
				ret = TranslateRecv(newsock, buffer);
				if (ret <0 ){
					errcnt++;
					continue;
				}
			} else if (9 == buffer[0]) {   // TLG
			//	RunLog("call TranslateTLGRecv ........");
				ret = TranslateTLGRecv(newsock, buffer);
				if (ret < 0 ){
					errcnt++;
					continue;
				}
			} else{
				UserLog( "解析数据失败,Subnet不是1或9." );
			}

			errcnt = 0;
			//sleep(10);			
		}

		if ( errcnt > 40 ) {
			/*错误处理*/
			UserLog( "初始化socket 失败.port=[%s]\n", port );
			break; 
		}
	}

	RunLog("Tcp Server Stop ......");
	close(sock);

	return -1;
} 

int GetSemInfo(int key) 
{
	int SemId;
	int ret;
	union semum {
               int              val;    /* Value for SETVAL */
               struct semid_ds *buf;    /* Buffer for IPC_STAT, IPC_SET */
               unsigned short  *array;  /* Array for GETALL, SETALL */
               struct seminfo  *__buf;  /* Buffer for IPC_INFO
                                           (Linux-specific) */
	}seminfo;
	
	SemId = semget( GetKey(key), 0, 0 );
	if( SemId == -1 )
	{
		RunLog("Get Sem Key Failed");
		return -1;
	}
	RunLog("DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD  SemId is %ld", SemId);
	ret = semctl(SemId, 0, GETVAL, seminfo);
	if (ret < 0) {
		RunLog("Get Sem Info Failed %s", strerror(errno));
		return -1;
	}
	RunLog("DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD  Sem Value is %d", ret);
	return 0;
}

//停止TCP服务，返回0正常停止,-1失败
//int StopTcpRecvServ();

/*
 发送数据到控制设备,
 sendBuffer:待发送数据,msgLength:数据长度,timeout为发送超时值.
 控制设备逻辑地址为sendBuffer中的前两个字节
 成功返回0,失败返回-1.请参考和调用tcp.h和tcp.c的函数.
*/
/*
 * 写待发送书上到Buffer
 */
int SendData2CD(const unsigned char *_sendBuffer, const int _msgLength, const int timeout)
{
	int i, ret, tmp, sockfd, sendcnt;

	// -------- 矫正格式 -------------
	unsigned char buffer[32] = {0};
	unsigned char *sendBuffer = NULL;
	int msgLength;

	// 如果是Acknowledge, 并且是MS_ACK非05, 则不发送Data_Ack
	if (((_sendBuffer[5] & 0xE0) == 0xE0) && (_sendBuffer[_sendBuffer[8] + 9] != 5)) {
		sendBuffer = (unsigned char *)buffer;
		msgLength = _sendBuffer[8] + 10;
		memcpy(sendBuffer, _sendBuffer, msgLength);
		sendBuffer[6] = 0x00;
		sendBuffer[7] = _sendBuffer[8] + 2;
	} else {
		sendBuffer = (unsigned char *)_sendBuffer;    // !! __u8 * = const __u8 *
		msgLength = _msgLength;
	}

	// -------- End  -------------
	
	//unsigned char cdNode;
	//int sockflag =0;
	//unsigned char sPortName[5],*sIPName;	
	//size_t msg_lg;
	//unsigned char buffer[512];
	//memset(sPortName, 0, sizeof(sPortName));	
	//memset(buffer, 0, sizeof(buffer));	
	if ( ( NULL == gpIfsf_shm ) && (NULL == gpGunShm) ){		
		UserLog("共享内存值错误,SendData2CD 失败");
		return -1;
	}
	if ( (NULL == sendBuffer) || (0 == msgLength) ){		
		UserLog("参数值错误,SendData2CD 失败");
		return -1;
	}
	if (msgLength > 1024){
		UserLog("参数值错误,SendData2CD 失败");
		return -1;
	}
	

	
	if (gpIfsf_shm->auth_flag != FCC_SWITCH_AUTH){
		/*壳牌填坑，信号量机制有问题导致进程锁死，改用消息队列*/
		sendcnt = 3;
		while (--sendcnt) {    
			ret = SendMsg(TCP_MSG_TRAN, sendBuffer, msgLength, 1, 3);
			if (ret < 0) {
				RunLog("Send TCP_MSG_TRAN Failed ret = [%d]", ret);
			}
			else break;
		}
		if (sendcnt == 0) {
			RunLog("Send TCP_MSG_TRAN2 ");
			ret = SendMsg(TCP_MSG_TRAN2, sendBuffer, msgLength, 1, 3);
			if (ret < 0) {
				RunLog("Send TCP_MSG_TRAN2 Failed ret = [%d]", ret);
			}
		}
	}

	return 0;
}
/*
 * 完成实际的数据发送
 */
static int SendData2CD2(const unsigned char *sendBuffer, const int msgLength, const int timeout)
{
	int i, ret, tmp, tryn;
	int sockfd = -1;
	unsigned char cdNode;
	int sockflag =0;
	unsigned char sPortName[5],*sIPName;	
	size_t msg_lg;
	unsigned char tempchar[MAX_TCP_LEN] = {0};
	unsigned char IFSFchar[MAX_TCP_LEN] = {0};
	int len = msgLength;
	memset(sPortName, 0, sizeof(sPortName));	

	if ( ( NULL == gpIfsf_shm ) && (NULL == gpGunShm) ){		
		UserLog("共享内存值L错误,SendData2CD 失败");
		return -1;
	}
	if ( (NULL == sendBuffer) || (0 == msgLength) ){		
		UserLog("参数值错误,SendData2CD 失败");
		return -1;
	}
	//only for log.
	{
		memcpy(tempchar, sendBuffer, len);
//		UserLog(">>>FCC上传 : %s(%d)", Asc2Hex(tempchar, len), msgLength);
	}

	cdNode = sendBuffer[1];
	for(i=0; i< MAX_RECV_CNT; i++){
		if (gpIfsf_shm->recv_hb[i].lnao.node== cdNode )
			break; //找到了CD 节点
	}
	if (i == MAX_RECV_CNT ){
		for(i=0; i< MAX_RECV_CNT; i++){
			if (gpIfsf_shm->recv_hb[i].lnao.subnet == 2  ) 
				break; //找到了空闲CD 节点
		}
		if (i == MAX_RECV_CNT ){
			gpIfsf_shm->auth_flag = FCC_SWITCH_AUTH;
			UserLog("CD  心跳数据找不到,SendData2CD 失败");
			return -1;
		}
	}

	tryn = 5 ;
	while(1){
#if 0
		if (gpIfsf_shm->auth_flag == FCC_SWITCH_AUTH){
			gpIfsf_shm->need_reconnect = 0;      // 如果离线了就不要关闭连接了
			return 0;
		}		

#endif
		if (gpIfsf_shm->need_reconnect == 1) {
			if (gpIfsf_shm->recvSockfd[i] >= 0) {
				RunLog("Close privous connection(%d) --X-------------->", 
								gpIfsf_shm->recvSockfd[i]);
				close(gpIfsf_shm->recvSockfd[i]);
			}
			gpIfsf_shm->recvSockfd[i] = -1;
			gpIfsf_shm->need_reconnect = 0;
		}

		if (gpIfsf_shm->recvSockfd[i] >= 0){
			sockfd = gpIfsf_shm->recvSockfd[i];
			sockflag = 1;
		} else {
			sIPName = inet_ntoa(*((struct in_addr *)(&gpIfsf_shm->recv_hb[i].host_ip)));
			tmp = ntohs(gpIfsf_shm->recv_hb[i].port);
			sprintf(sPortName,"%d", tmp);
			RunLog("CD IP:%s, CD port: %s",sIPName, sPortName);
			sockfd = IfsfTcpConnect(sIPName, sPortName, (timeout+1)/2);
			
			gpIfsf_shm->recvSockfd[i] = sockfd;
			gpGunShm->sockCDPos = i;
			if (sockfd >= 0) {
				RunLog("Creat a new connection(%d) to CD -------------->", sockfd);
			}
		}

		//UserLog("SendData2CD2 TcpConnect socket ok..... sockfd %d", sockfd);
		if (sockfd >= 0){
			//Act_P(CD_SOCK_INDEX);
			ret = TcpSendDataInTime(sockfd, sendBuffer, msgLength, timeout);
			//Act_V(CD_SOCK_INDEX);
			if (ret < 0 ){

				if ((sockflag != 0) && (tryn-- > 0) || ret == -2) {
					RunLog("发送数据[%s(%d)]给上位机失败[%d], 重试", 
						Asc2Hex(tempchar, len), msgLength, errno);
					continue;
				} else {
					close(sockfd);
					gpIfsf_shm->recvSockfd[i] = -1;
					sockflag = 0;
					UserLog("发送数据[%s(%d)]给上位机失败.", Asc2Hex(tempchar, len), msgLength);
					return -1;
					
				}
			}

			RunLog(">>>FCC上传 : %s(%d)", Asc2Hex(tempchar, len), msgLength);
			if (gpIfsf_shm->css_data_flag > 0 && gCssFifoFd >= 0) {
				bzero(IFSFchar, sizeof(IFSFchar));
				memcpy(IFSFchar, &msgLength, 4);
				memcpy(&IFSFchar[4], tempchar, msgLength);
				ret = write(gCssFifoFd, IFSFchar, msgLength + 4);
				RunLog("写二配数据 %s(%d)", Asc2Hex(IFSFchar, len), ret);
			}
		} else {
			UserLog("建立TCP连接FCC->Fuel Server不成功, 发送数据给上位机失败");
			return -1;
		}
		break;
	}
	
	return 0;
}
/*
 发送数据到给出的连接newsock,
 sendBuffer:待发送数据,msgLength:数据长度,timeout为发送超时值.
 成功返回0,失败返回-1.请参考和调用tcp.h和tcp.c的函数.
*/
int SendDataBySock(const int newsock, const unsigned char *sendBuffer, const int msgLength, const int timeout){
	int i, ret, tmp;
	if ( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,SendDataBySock 失败");
		return -1;
	}
	if ( (NULL == sendBuffer) || (0 == msgLength) ){		
		UserLog("参数值错误,SendDataBySock 失败");
		return -1;
	}
#if 0
	printf("SendData2CD,size[%d]",msgLength);
	for(i=0; i<msgLength;i++){
		printf("%02x",sendBuffer[i]);
	}
	printf("]\n");
#endif
	ret =TcpSendDataInTime(newsock, sendBuffer, msgLength,  timeout);
	if (ret < 0 ){
		UserLog("Tcp Send Data不成功,SendDataBySock 失败");
		return -1;
		}	
	return 0;
}

static int TranslateRecvErr(const int newsock, const unsigned char *recvBuffer, int MS_ACK){
	int i, ret, ad1, m_st, msg_lg;
	//DISPENSER_DATA *dispenser_data;		
	unsigned char node= recvBuffer[1];
	//unsigned char ad_char2[2], ad_char4[4];
	unsigned char buffer[128];
	
	memset(buffer, 0 , sizeof(buffer));	
	if ( (NULL == recvBuffer) || (0 == newsock)  ){		
		UserLog("参数错误,Translate  失败");
		return -1;
	}	
	m_st = ( (int)recvBuffer[5]&0xe0) >>5;
	//ad_lg = recvBuffer[8];
	//ad1 = recvBuffer[9];
	
	if ( node <= 0 ){
		UserLog("加油机节点值错误,Translate   失败");
		return -1;
		}
	if ( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,Translate   失败");
		return -1;
	}
	buffer[0] = recvBuffer[2];
	buffer[1] = recvBuffer[3];
	buffer[2] = recvBuffer[0];
	buffer[3] = recvBuffer[1];
	buffer[4] = '\0'; //IFSF_MC;
	buffer[5] = (recvBuffer[5] |0x20); //answer
	buffer[6] = '\0'; //data_lg;
	buffer[7] =2 + recvBuffer[8]; //data_lg
	//recv_lg = recvBuffer[6];
	//recv_lg = recv_lg * 256 + recvBuffer[7];
	buffer[8] = recvBuffer[8]; // Ad_lg   
	for(i=0; i< recvBuffer[8]; i++ ){
		buffer[9+i] = recvBuffer[9+i];  //Ad  
	}
	i = recvBuffer[8];//i use as a temp value.
	buffer[9+ i] = MS_ACK;  
	//buffer[10] = recvBuffer[10]; //It's Data_Id. Data begin.
	msg_lg = 10 + recvBuffer[8];
	
	ret = SendData2CD(buffer, msg_lg, 5);
	if (ret <0){		
		UserLog("发送回复数据错误,Translate   失败");
		return -1;
	}
	
	return 0;
}

/*
 * TcpSend2CDServ
 * FCC.Client -> POS.Server
 * 所有上行数据的通道
 */

int TcpSend2CDServ()
{
	size_t msg_lg;
	int i, j, ret, msgtyp = 1;
	const int SEND_CD_TRY_TIMES = 3;
	unsigned char buffer[MAX_TCP_LEN];
	int timeout = 1;
	int sockfd =-1;
	int sockflag = 0;
	unsigned char sPortName[5],*sIPName;	
	//sleep(10);
//	UserLog("Tcp Send to CD Server <---> ");
	memset(sPortName, 0, sizeof(sPortName));	
	if ( ( NULL == gpIfsf_shm ) && (NULL == gpGunShm) ){		
		UserLog("共享内存值错误,RecvCDServ 失败");
		//return -1;
		exit(-1);
	}

	Signal(SIGPIPE, SIG_IGN);

	while(1){
		//UserLog("TcpSendCDServ----0----, i: %d , pid:%d, ppid:%d", i, getpid(),getppid());
		for(i=0; i< MAX_RECV_CNT; i++){
			if (  gpIfsf_shm->recv_hb[i].lnao.subnet == 2  ) 
				break; //找到了空闲CD 节点
		}

		if (i == MAX_RECV_CNT ){		
			//UserLog("CD  心跳数据找不到,SendData2CD 失败");
			//return -1;
			//UserLog("Tcp Receive from CD Server not find CD");
			sleep(1);
			continue;
		} else{
			break;
		}
		//UserLog("TcpSendCDServ---00---, i: %d , pid:%d, ppid:%d", i, getpid(),getppid());
	}


	if (gpIfsf_shm->recvSockfd[i] >= 0) {
		close(gpIfsf_shm->recvSockfd[i]);
		gpIfsf_shm->recvSockfd[i] = -1;
	}
	
	gpGunShm->CDLen = sizeof(gpGunShm->CDBuffer);
	while(1){
		if (( ret = RecvMsg(TCP_MSG_TRAN, gpGunShm->CDBuffer, &gpGunShm->CDLen, &msgtyp, 2)) == 0 ||
			RecvMsg(TCP_MSG_TRAN2, gpGunShm->CDBuffer, &gpGunShm->CDLen, &msgtyp, TIME_FOR_TCP_CMD) == 0){			
			UserLog(">>>>Tcp will Receive from CD Server by socket of sendData2CD");
			break;
		}
		Ssleep(300000L);
	}

	j = 0;
	while (1) {
		if (gpIfsf_shm->auth_flag == FCC_SWITCH_AUTH){
			gpIfsf_shm->need_reconnect = 0;      // 如果离线了就复位标记
			if (sockfd >= 0) {
				RunLog("CD offline, close connection --X-------------->");
				close(sockfd);
				sockfd = -1;
				gpIfsf_shm->recvSockfd[gpGunShm->sockCDPos] = -1;
			}
			sleep(1);
			continue;
		}

		if (gpIfsf_shm->need_reconnect == 1) {
			if (gpIfsf_shm->recvSockfd[i] >= 0) {
				RunLog("Close privous connection(%d) --X-------------->", 
								gpIfsf_shm->recvSockfd[i]);
				close(gpIfsf_shm->recvSockfd[i]);
			}
			gpIfsf_shm->recvSockfd[i] = -1;
			gpIfsf_shm->need_reconnect = 0;
		}

		if ( (gpGunShm->CDLen > 0)  && (gpGunShm->CDBuffer[0] > 0) ){			
			ret = SendData2CD2(gpGunShm->CDBuffer, gpGunShm->CDLen, 3);
			if (ret < 0 ){
				RunLog("发送失败");
			} 
			memset(gpGunShm->CDBuffer, 0, gpGunShm->CDLen);	
			gpGunShm->CDLen = 0;
			
		}
		else {
			gpGunShm->CDLen = sizeof(gpGunShm->CDBuffer);
			if (RecvMsg(TCP_MSG_TRAN, gpGunShm->CDBuffer, &gpGunShm->CDLen, &msgtyp, 2) == 0 ||
			RecvMsg(TCP_MSG_TRAN2, gpGunShm->CDBuffer, &gpGunShm->CDLen, &msgtyp, TIME_FOR_TCP_CMD) == 0){	
				ret = SendData2CD2(gpGunShm->CDBuffer, gpGunShm->CDLen, 3);
				if (ret < 0 ){
					RunLog("发送失败2");
				} 
				memset(gpGunShm->CDBuffer, 0, gpGunShm->CDLen);	
				gpGunShm->CDLen = 0;
			}
		
			memset(gpGunShm->CDBuffer, 0, gpGunShm->CDLen);	
			gpGunShm->CDtimeout = 0;
			gpGunShm->CDLen = 0;
		}


//		UserLog("TcpSendCDServ--------while0-----------, pid:%d, ppid:%d", getpid(),getppid());
		if ( (gpGunShm->sockCDPos >= 0) && (sockfd >= 0) && \
			(gpIfsf_shm->recvSockfd[gpGunShm->sockCDPos] >= 0) ){
			if (sockfd != gpIfsf_shm->recvSockfd[gpGunShm->sockCDPos]) {
				close(sockfd);
				sockfd = -1;
			}
			i = gpGunShm->sockCDPos;  //this means sendData2CD is using  a CD.
//			UserLog("Tcp will Receive from CD Server by socket of sendData2CD");
		}
		if (gpIfsf_shm->recvSockfd[i] >= 0) {
			sockfd =gpIfsf_shm->recvSockfd[i];
			sockflag = 1;
//			UserLog("Tcp Receive from CD Server by share memory socket fd, sockfd[%d]",sockfd);
		}


		if ( sockfd < 0){
			//return -1;
			sockflag = 0;
			//sleep(3);
			continue;
		}
		
		//Ssleep(100000L);
	}

	return -1;
}

//only for HBSendServ.
static int  HBSend(const int time, const int sockfd, struct sockaddr *remoteSrv)
{
	int i,k,step;
	int startn, endn;
	int ret;	
	static int runcnt = 0;	
	IFSF_HEARTBEAT *ifsf_heartbeat;	
	extern int errno;
	char hearttmp[16] = {0};
	int len;
	
	//心跳接收区数据次数减1.
	runcnt++;
	if (runcnt >= time){ 
		runcnt = 0;
		for(i=0; i< MAX_RECV_CNT; i++){		
			gpIfsf_shm->recvUsed[i]--; 
			if ( (gpIfsf_shm->recvUsed[i] < -30) || (gpIfsf_shm->recvUsed[i] > HEARTBEAT_TIME+1)){
				//memset(&gpIfsf_shm->recv_hb[i],0, sizeof(IFSF_HEARTBEAT));
				gpIfsf_shm->recvUsed[i] = MAX_NO_RECV_TIMES;
			}
		}
	}
	
	
	if (time <=0){
		UserLog("time error, time:[%d]",time);
		return -1;
	}

	startn = (MAX_CHANNEL_CNT + 1) / time * runcnt ;       // MAX_CHANNEL_CNT + 1:  all dispenser + tlg
	endn = (MAX_CHANNEL_CNT + 1) * (runcnt + 1) / time;	
	k = 0;
	//UserLog("HBSend  starting....runcnt:%d, startn:%d, endn:%d", runcnt, startn,endn);
	for (i = startn; i < endn; i++) {
		step = k * time + runcnt;
		if (step >= (MAX_CHANNEL_CNT + 1)){
			step = 0;
			return 1;
		}

		if ((gpIfsf_shm->node_online[step] != 0)              // 节点在线
			&& (gpIfsf_shm->convert_hb[step].port != 0)) {   //找到了加油机节点
			ifsf_heartbeat = (IFSF_HEARTBEAT *)(&(gpIfsf_shm->convert_hb[step]));
			//RunLog("HBSend  starting....[%d] is sending, ppid:[%d]", step,getppid() );
			//RunLog("HBSend  starting....[%d], pid:%d, ppid:%d, time:%d, send size:%d, ip:%s:%d",  \
			//step,  getpid(), getppid(),  time, sizeof(IFSF_HEARTBEAT), inet_ntoa(((struct sockaddr_in *)remoteSrv)->sin_addr), ntohs(((struct sockaddr_in *)remoteSrv)->sin_port));


			if ((ifsf_heartbeat->lnao.subnet == 0x09) && (gpIfsf_shm->tank_heartbeat == 0) ){
				RunLog("======为解决正星卡机处理心跳混乱，关闭液位仪心跳======");
			}else{
				ret = sendto(sockfd, ifsf_heartbeat, sizeof(IFSF_HEARTBEAT),
						0, remoteSrv, sizeof(struct sockaddr));
			}

			if (ret < 0) {				
				UserLog("心跳数据发送失败,error:[%s]", strerror(errno));	
			}
			if (gpIfsf_shm->css_data_flag > 0 && gCssFifoFd >= 0) {
				len = 11;
				memcpy(hearttmp, &len, 4);
				hearttmp[4] = 0xca;
				memcpy(&hearttmp[5], (char *)ifsf_heartbeat, sizeof(IFSF_HEARTBEAT));
				ret = write(gCssFifoFd, hearttmp, 15);
				if (ret == 15) {
					//RunLog("IFSF心跳数据发送二配成功%s", Asc2Hex(hearttmp, 15));
				}
				else {
					RunLog("二配心跳发送失败ret = %d", ret);
				}
			}
		}

		k++;			
	}	

	return 0;
}

//启动本设备心跳发送服务，返回0正常启动，-1失败.
int HBSendServ()
{
	int time,ret;
	int sockfd;
	int broadcast = 1;
	//struct itimerval value;
	char	ip[16];
	struct sockaddr_in remoteSrv;	
	struct in_addr  iplong;
	struct in_addr  ipmask;
	int 	port;
	extern int errno;

	sleep(10);
	UserLog("Heartbeat Send Server-->");

	
	gCssFifoFd = open( "/tmp/css_fifo.data", O_RDWR| O_NONBLOCK);//O_WRONLY
	if (gCssFifoFd < 0 ) {
		UserLog( "打开二配管道文件[%s]失败errno[%d]\n", "/tmp/css_fifo.data" ,errno);
	} else{
		UserLog( "打开二配管道文件[%s]成功", "/tmp/css_fifo.data" );
	}
	
	if ( (sockfd = socket(AF_INET,SOCK_DGRAM,0)) == -1 )
        {
                UserLog("socket function error!\n");
                return -1;
        }

        if ( setsockopt(sockfd,SOL_SOCKET,SO_BROADCAST,&broadcast,sizeof(broadcast)) == -1)
        {
                UserLog("setsockopt function error!\n");
                return -1;
        }
	

	//iplong.s_addr = GetLocalIP() |htonl(0x0ff); //xxx.xxx.xxx.255;
	iplong.s_addr = GetBroadCast();
	
	strcpy( ip, inet_ntoa(iplong));
	port = HB_PORT;	
	memset( &remoteSrv, 0, sizeof(remoteSrv) );
	remoteSrv.sin_family = AF_INET;	
	remoteSrv.sin_addr.s_addr = inet_addr(ip);//(GetLocalIP() | 0xff);
	remoteSrv.sin_port = htons(port);
	//UserLog("ip:%x \n", remoteSrv.sin_addr.s_addr );	
	while(1){		
		sleep(1);
		//UserLog("HBSendServ start...., pid:%d, ppid:%d", getpid(),getppid());
		time = gpIfsf_shm->fcc_com_sv_data.Heartbeat_Interval;
		HBSend(time, sockfd, (struct sockaddr *)(&remoteSrv) );
	}

	close(sockfd);

	return 0;
}


/*
 * 心跳发送服务, 标准版--全网广播
 */
int HBSendServ_STD()
{
	int time;
	int sockfd;
	int broadcast = 1;
	struct sockaddr_in remoteSrv;	

	sleep(10);
	UserLog("Heartbeat Send Server (Standard) -->");

	if ((sockfd = socket(PF_INET, SOCK_DGRAM, 0)) == -1) {
                UserLog("socket function error!\n");
                return -1;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, 
			&broadcast, sizeof(broadcast)) == -1) {
                UserLog("setsockopt function error!\n");
                return -1;
        }
	

	memset(&remoteSrv, 0, sizeof(remoteSrv));
	remoteSrv.sin_family = PF_INET;
	remoteSrv.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	remoteSrv.sin_port = htons(HB_PORT);
	//UserLog("ip:%x \n", remoteSrv.sin_addr.s_addr);
	
	while (1) {		
		sleep(1);
		time = gpIfsf_shm->fcc_com_sv_data.Heartbeat_Interval;
		HBSend(time, sockfd, (struct sockaddr *)(&remoteSrv));
	}

	close(sockfd);

	return 0;
}


/*
 * Macro: 离线联线检测以及相关处理
 *        脱机重联后,等所有交易全都写入文件再改变授权标记为上位机授权
 */
#define offline_detect_and_handle()   do {                              \
	auth_flag = gpIfsf_shm->auth_flag;                              \
	for (i = 0; i< MAX_RECV_CNT; i++) {                             \
		if ((gpIfsf_shm->recvUsed[i] > MAX_NO_RECV_TIMES) &&    \
			(gpIfsf_shm->recv_hb[i].lnao.subnet == 2)) {  /* 如果有CD的心跳 */            \
	/*		gpIfsf_shm->auth_flag = DEFAULT_AUTH_MODE; */   /* 设置为上位机默认模式.*/    \
			auth_flag = DEFAULT_AUTH_MODE;                  \
			break;                                          \
		}                                                       \
	}		                                                \
	if (i == MAX_RECV_CNT) {                                        \
		RunLog("=========  上位机离线，FCC接管控制模式!! =========");                         \
		auth_flag = FCC_SWITCH_AUTH;                            \
	}                                                               \
	if (old_offline_flg != auth_flag) {                             \
		for (i = 0; i < MAX_CHANNEL_CNT; i++) {		        \
			if (gpIfsf_shm->convert_hb[i].lnao.node != 0) { /* 节点可用 */                \
				if (auth_flag != FCC_SWITCH_AUTH) {/*离线转在线*/                     \
					RunLog("=========  上位机恢复联线!! ======================"); \
				} else {     /* 在线转离线 */                                         \
					OffLineSetBuff(gpIfsf_shm->convert_hb[i].lnao.node);          \
				}	                                \
			}                                               \
		}                                                       \
		gpIfsf_shm->auth_flag = auth_flag;                      \
		old_offline_flg = gpIfsf_shm->auth_flag;                \
		if (auth_flag != FCC_SWITCH_AUTH) {                     \
			DoReOnline();                                   \
		}                                                       \
	}                                                               \
} while (0);                                                    
		// jjjie @7.11
		//RunLog("before call write buffer data to file,node: %d",    \
		//		gpIfsf_shm->convert_hb[i].lnao.node);       \
		//WriteBuffDataToFile(gpIfsf_shm->convert_hb[i].lnao.node);   \

//启动本设备心跳接收服务，返回0正常启动，-1失败.
int HBRecvServ(){ //注意不要接收本机发送的心跳.
	int i;
	struct sockaddr_in remoteSrv;
	char ip[16];
	struct in_addr  iplong;
	int port;
	int ret;
	int broadcast = 1;
	int sockfd;
	int fromlen;
	IFSF_HEARTBEAT *pIfsf_heartbeat;
	unsigned char buffer[32];
	extern int errno;
	static __u8 old_offline_flg; //正常modify by liys  2008-03-14
	static int auth_flag = 0;
       
	if ( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,心跳服务 失败");
		return -1;
	}
	sleep(2);	

	UserLog("Heartbeat Receive Server<---");

	iplong.s_addr = GetBroadCast();
	
	strcpy( ip, inet_ntoa(iplong));
	port = HB_PORT;
	if ( (sockfd = socket(AF_INET,SOCK_DGRAM,0)) == -1 ) {
                UserLog("socket function!error!\n");
                return -1;
        }	
        if ( setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR, &broadcast,sizeof(broadcast)) == -1) {
                UserLog("setsockopt function error!\n");
                return -1;
        }	
	memset( &remoteSrv, 0, sizeof(remoteSrv) );
	remoteSrv.sin_family = AF_INET;
	remoteSrv.sin_addr.s_addr = inet_addr( ip );
	remoteSrv.sin_port = htons( port );
	fromlen =  sizeof(remoteSrv);
	ret = bind(sockfd,(struct sockaddr *)&remoteSrv, sizeof(remoteSrv)  );
	if (ret<0){
		UserLog("heartbeat send socket bind error!\n");
                return -1;
	}

	//UserLog("HBRecvServ.....2, pid:%d, ppid:%d", getpid(),getppid());
	while(1){
		memset(buffer, 0, sizeof(buffer));
		// recvfrom 改成定时的 !!!
		ret = recvfrom( sockfd,  buffer, sizeof(IFSF_HEARTBEAT), 0,\
			(struct sockaddr *)&remoteSrv, &fromlen);   //sizeof(IFSF_HEARTBEAT)
		if ( (ret == 0) || (84 == errno ) ){
			pIfsf_heartbeat =(IFSF_HEARTBEAT *)buffer;
			RunLog("HBRecvServ.....while2.., pid:%d, ppid:%d, node:%d , subnet: %d", \
				getpid(),getppid(), pIfsf_heartbeat->lnao.node,pIfsf_heartbeat->lnao.subnet);
			if ((1 == (int)pIfsf_heartbeat->lnao.subnet) ||
				(9 == (int)pIfsf_heartbeat->lnao.subnet)||
				(((int)pIfsf_heartbeat->lnao.node > 1)&&(2==((int)pIfsf_heartbeat->lnao.subnet)))) {  // 不接收 Dispenser & TLG 的心跳
				offline_detect_and_handle();               
				continue;
			}

			for(i=0; i< MAX_RECV_CNT; i++){
				if (( gpIfsf_shm->recv_hb[i].lnao.node == pIfsf_heartbeat->lnao.node ) && 
					(gpIfsf_shm->recv_hb[i].lnao.subnet == pIfsf_heartbeat->lnao.subnet)) {
					//找到了加油机节点
					break;
				}
			}

			if (i == MAX_RECV_CNT){ 
				for(i = 0; i< MAX_RECV_CNT; i++){
					if (gpIfsf_shm->recvUsed[i] < MAX_NO_RECV_TIMES) {//找到了
						RunLog("Find empty space to save new received heartbeat");
						break;
					}
				}
			}
			if (i != MAX_RECV_CNT){
				memcpy( &gpIfsf_shm->recv_hb[i], pIfsf_heartbeat,sizeof(IFSF_HEARTBEAT) );
				gpIfsf_shm->recvUsed[i] = HEARTBEAT_TIME;
			} else {
				UserLog("接收心跳数据错误, 没有存储,i:[%d]",i);
			}

		} else {
		 	RunLog("HBRecvServ recvfrom error..., pid:%d, ppid:%d,size receive:%d, size hb:%d,  error:%s",  \
				getpid(),getppid() , fromlen,sizeof(IFSF_HEARTBEAT), strerror(errno) );
		}
		
		offline_detect_and_handle();

		Ssleep(100000L);
	}
	//UserLog("HBRecvServ exit ....., pid:%d, ppid:%d", getpid(),getppid());
	close(sockfd);
}


/*
 * 标准IFSF心跳接收服务 (全网广播)
 */
int HBRecvServ_STD()
{
	int i;
	int ret;
	int broadcast = 1;
	int sockfd;
	int fromlen;
	IFSF_HEARTBEAT *pIfsf_heartbeat;
	struct sockaddr_in remoteSrv;
	unsigned char buffer[32];
	static __u8 old_offline_flg;
	static int auth_flag = 0;
	extern int errno;
       
	if (NULL == gpIfsf_shm) {
		UserLog("共享内存值错误,心跳服务启动失败");
		return -1;
	}

	sleep(2);	
	UserLog("Heartbeat Receive Server (Standard) <---");

	if ((sockfd = socket(AF_INET,SOCK_DGRAM,0)) == -1) {
                UserLog("socket function!error!\n");
                return -1;
        }	

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &broadcast, sizeof(broadcast)) == -1) {
                UserLog("setsockopt function error!\n");
                return -1;
        }	

	memset( &remoteSrv, 0, sizeof(remoteSrv));
	remoteSrv.sin_family = PF_INET;
	remoteSrv.sin_addr.s_addr = htonl(INADDR_ANY);
	remoteSrv.sin_port = htons(HB_PORT);
	fromlen =  sizeof(remoteSrv);

	ret = bind(sockfd,(struct sockaddr *)&remoteSrv, sizeof(remoteSrv));
	if (ret<0){
		UserLog("heartbeat send socket bind error!\n");
                return -1;
	}

	//UserLog("HBRecvServ.....2, pid:%d, ppid:%d", getpid(),getppid());
	while(1){
		memset(buffer, 0, sizeof(buffer));
		// recvfrom 改成定时的 !!!
		ret = recvfrom( sockfd,  buffer, sizeof(IFSF_HEARTBEAT), 0,\
			(struct sockaddr *)&remoteSrv, &fromlen);   //sizeof(IFSF_HEARTBEAT)
		if ( (ret == 0) || (84 == errno ) ){
			pIfsf_heartbeat =(IFSF_HEARTBEAT *)buffer;
			//UserLog("HBRecvServ.....while2.., pid:%d, ppid:%d, node:%d , subnet: %d", \
			//	getpid(),getppid(), pIfsf_heartbeat->lnao.node,pIfsf_heartbeat->lnao.subnet);
			if (1 == (int)pIfsf_heartbeat->lnao.subnet ||
				(9 == (int)pIfsf_heartbeat->lnao.subnet)) {  // 不接收 Dispenser & TLG 的心跳
				offline_detect_and_handle();               
				continue;
			}
			for(i=0; i< MAX_RECV_CNT; i++){
				if (( gpIfsf_shm->recv_hb[i].lnao.node == pIfsf_heartbeat->lnao.node ) && 
					(gpIfsf_shm->recv_hb[i].lnao.subnet == pIfsf_heartbeat->lnao.subnet)) {
					//找到了加油机节点
					break;
				}
			}
			if (i == MAX_RECV_CNT){ 
				for(i = 0; i< MAX_RECV_CNT; i++){
					if (gpIfsf_shm->recvUsed[i] < MAX_NO_RECV_TIMES) {//找到了
						RunLog("Find empty space to save new received heartbeat");
						break;
					}
				}
			}
			if (i != MAX_RECV_CNT){
				memcpy( &gpIfsf_shm->recv_hb[i], pIfsf_heartbeat,sizeof(IFSF_HEARTBEAT) );
				gpIfsf_shm->recvUsed[i] = HEARTBEAT_TIME;
			} else {
				UserLog("接收心跳数据错误, 没有存储, i:%d",i);
			}

		} else {
//		 	printf("HBRecvServ recvfrom error..., pid:%d, ppid:%d,size receive:%d, size hb:%d,  error:%s",  \
				getpid(),getppid() , fromlen,sizeof(IFSF_HEARTBEAT), strerror(errno) );
		}
		
		offline_detect_and_handle();

		Ssleep(100000L);
	}
	//UserLog("HBRecvServ exit ....., pid:%d, ppid:%d", getpid(),getppid());
	close(sockfd);
}


//停止本设备心跳接收服务，返回0正常启动，-1失败.
//int StopHBRecvServ(); 

/*-------------------------------------------------------------------*/
//private函数,下面为本.c程序要用到的函数:
//----------------------------------
//获取本机IP,返回本机IP地址，返回-1失败，解决DHCP问题,因为IFSF建议使用DHCP
static long  GetLocalIP(){//参见tcp.h和tcp.c文件中有,可以选择一个.
	int  MAXINTERFACES=16;
	long ip;
	int fd, intrface;
 	struct ifreq buf[MAXINTERFACES]; ///if.h
	struct ifconf ifc; ///if.h
	ip = -1;
	if ((fd = socket (AF_INET, SOCK_DGRAM, 0)) >= 0) //socket.h
	{
 		ifc.ifc_len = sizeof(buf);
 		ifc.ifc_buf = (caddr_t) buf;
 		if (!ioctl (fd, SIOCGIFCONF, (char *) &ifc)) //ioctl.h
		{
 			intrface = ifc.ifc_len / sizeof (struct ifreq); 
			while (intrface-- > 0) {
 				if (!(ioctl (fd, SIOCGIFADDR, (char *) &buf[intrface]))) {
 					ip = inet_addr(inet_ntoa(((struct sockaddr_in*)(&buf[intrface].ifr_addr))->sin_addr));//types
 					break;  //only find firrst eth.
				}
           		}
			
		}
 		close (fd);
 	}
	return ip;	
}


/*
 * func: 获取广播地址
 */
static unsigned long  GetBroadCast()
{	
	int  MAXINTERFACES = 16;
	int i, fd, intrface;
 	struct ifreq buf[MAXINTERFACES]; ///if.h
	struct ifconf ifc; ///if.h
	char *lipaddr;
	char *lnetmask;
	long ip;

	ip = -1;

	if ((fd = socket (AF_INET, SOCK_DGRAM, 0)) >= 0) //socket.h
	{		
		ifc.ifc_len = sizeof(buf);
 		ifc.ifc_buf = (caddr_t) buf;
 		if (!ioctl (fd, SIOCGIFCONF, (char *) &ifc)) //ioctl.h
		{			
 			intrface = ifc.ifc_len / sizeof (struct ifreq); 
			//i = 0 ;
			while (intrface-- >0 )//intrface-- > 0
 			{ 	
 				i = intrface;
 				if (!(ioctl (fd, SIOCGIFBRDADDR, (char *) &buf[intrface])) )
 				{			
 				       close (fd); 					
					ip= inet_addr( inet_ntoa( ((struct sockaddr_in*)(&buf[intrface].ifr_addr))->sin_addr));
					return ip;
				}
           		}

		}
 		close (fd);
 	}
	return 0;		
}
/**
*  sometimes socket send data to peer, but in the same time the peer close
*  the receive socket, in this case ,datas had sent to the sendbuffer but 
*  don't send to the peer in real. 
*  this function do only one thing that is verify the send buffer data have 
*  been sent by tcp/ip .if sent return 0, else return -1
**/
int VerifySendBufEmpt(int fd, int timeout)
{
	Assert(timeout > 0 || timeout == TIME_INFINITE);

	size_t value;
	int ret;
	

	SetAlrm( timeout, NULL );

	value = 1;
	while (1) {
		if( CatchSigAlrm )	/*超时*//*不管错误码,只要捕捉到超时都返回*/
		{
			UnSetAlrm();
			RptLibErr( ETIME );
			return -1;
		}
		//RunLog("value is %d", value);
		if ((ret = ioctl(fd, SIOCOUTQ, &value)) == -1 ) {
			UnSetAlrm();
			return -1;
		}
		else if (value == 0) {
			break;
		}
	}

	UnSetAlrm();
	return 0;
}
/*
	 0:	成功
	-1:	失败
	-2:	超时
*/
static ssize_t TcpSendDataInTime( int fd, const char *s, size_t len,  int timeout )
{
	ssize_t	 retlen;
	size_t	sendlen;
	size_t  value;
	char	*head;
	char	*t;
	char	buf[TCP_MAX_SIZE];
	int	mflag; 	/*标识是否分配了空间 0未分配. 1已分配*/
	int	n;
	int     i;
	
	//Assert(s != NULL && *s != 0);
	Assert(len != 0);
	Assert(timeout > 0 || timeout == TIME_INFINITE);

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
	memcpy(t, s, len );
//	RunLog("DDDDDDDDDDDDDD");
	n = WriteByLengthInTime(fd, t, sendlen, timeout );

	if ( mflag == 1 ) {
		Free(t);
	}

///	RunLog("DDDDDDDDDDDDDD n = %d", n);
	if ( n == sendlen ) {	/*数据全部发送*/
		if (s[2] == 0x09 && VerifySendBufEmpt(fd, 2) != 0) {
			return -1;
		}
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

static ssize_t TcpRecvDataInTime( int fd, char *s, size_t *buflen,  int timeout ) //size_t headlen,
{
	ssize_t	recvlen;
	size_t	len;
	size_t headlen =8;
	char	head[TCP_MAX_HEAD_LEN + 1];
	char	buf[TCP_MAX_SIZE];
	char	*t;
	int	mflag;
	int 	n;
	int	ret;
	
	//Assert( TCP_MAX_HEAD_LEN >= headlen );
	Assert( s != NULL );

	mflag = 0;

	memset( buf, 0, sizeof(buf) );
	memset( head, 0, sizeof(head) );
//	UserLog("TcpRecvDataInTime...starting head.., pid:%d, ppid:%d", getpid(),getppid());
	n = ReadByLengthInTime( fd, head, headlen, timeout );
//	UserLog("TcpRecvDataInTime...head return..,size:[%d] pid:[%d], ppid:[%d]", n, getpid(),getppid());
	if ( n == -1 ) {
		return -1;
	} else if ( n >= 0 && n < headlen ) {
		if ( GetLibErrno() == COMM_EOF )	/*对端关闭*/
			return -2;
		else
			return -1;
	}

	//if ( !IsDigital(head) )
	//	return -3;	/*非法的包长度*/

	//len = atoi(head);
	len =(unsigned short )head[6] *256+ (unsigned short )head[7];
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
		getpid(),getppid(), n, len, t);
	if ( n == -1 ) {
		ret = -1;
	} else if ( n >= 0 && n < len ) {
		//*buflen = 0;
		memset(buflen,0, sizeof(size_t));
		ret = -2;
	} else {
		if ( *buflen >= (len + 8)   ) {
			memcpy( s, head, 8 );
			memcpy( s+8, t, len );			
			s[len+8] = 0;
			len = len+8;
			memcpy(buflen , &len,sizeof(size_t));
			ret = 0;
		} else {
			memcpy( s, head, 8 );
			memcpy( s+8, t, *buflen-8 );
			s[*buflen] = 0;
			ret = 1;
		}
	}
//	UserLog("TcpRecvDataInTime...free for end.., pid:%d, ppid:%d, mflag:%d", getpid(),getppid(),mflag);
	if ( mflag == 1 ) {
		Free( t );
	}
//	UserLog("TcpRecvDataInTime...return.., pid:%d, ppid:%d, return:%d", getpid(),getppid(),ret);

	return ret;
}


static int DoReOnline()
{
	int i, fp, ret;
	unsigned char buffer[22] = {0x02, 0x01, 0x01, 0x01, 0x00, 0x80, 0x00, \
				    0x0E, 0x01, 0x21, 0x64, 0x00, 0x14, 0x01, \
				    0x02, 0x15, 0x01, 0x00, 0x16, 0x02, 0x00, 0x00};


	Ssleep(15000000);         // 延迟15s,确保CD已经收到 dispensers & TLG 的心跳
	for(i=0; i< MAX_RECV_CNT; i++){
		if (gpIfsf_shm->recv_hb[i].lnao.subnet == 2) {
			break;    //找到了空闲CD 节点
		}
	}
	buffer[1] = gpIfsf_shm->recv_hb[i].lnao.node;  // CD node

	for (i = 0; i < MAX_CHANNEL_CNT; ++i) {
		if (gpIfsf_shm->node_online[i] != 0x00) {
			buffer[3] = i + 1;  // node 
			for (fp = 0; fp < gpIfsf_shm->node_channel[i].cntOfFps; fp++) {
				buffer[9] = 0x21 + fp;   // FP_ID
				buffer[14] = gpIfsf_shm->dispenser_data[i].fp_data[fp].FP_State;
				buffer[17] = gpIfsf_shm->dispenser_data[i].fp_data[fp].Log_Noz_State;
				buffer[20] = gpIfsf_shm->dispenser_data[i].fp_data[fp].Assign_Contr_Id[0];
				buffer[21] = gpIfsf_shm->dispenser_data[i].fp_data[fp].Assign_Contr_Id[1];
				SendData2CD(buffer, 22, 5);
			}
		}
	}

	if (gpIfsf_shm->node_online[MAX_CHANNEL_CNT] != 0x00) {
		buffer[2] = 0x09;  // subnet
		buffer[3] = gpIfsf_shm->tlg_data.tlg_node;
		buffer[7] = 0x0B;  // data_lg
		buffer[12] = 0x20; // TP_Status
		buffer[15] = 0x21; // TP_Alarm
		buffer[16] = 0x02;
		for (i = 0; i < gpIfsf_shm->tlg_data.tlg_dat.Nb_Tanks; ++i) 		   {
			buffer[9] = 0x21 + i;  // TP_ID
			buffer[14] = gpIfsf_shm->tlg_data.tp_dat[i].TP_Status;
			buffer[17] = gpIfsf_shm->tlg_data.tp_dat[i].TP_Alarm[0];
			buffer[18] = gpIfsf_shm->tlg_data.tp_dat[i].TP_Alarm[1];
                               SendData2CD(buffer, 19, 5);
               }

       }

       return 0;
}
 


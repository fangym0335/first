/*******************************************************************
1.�������ÿ����ת��Э��ķ�IFSF�豸��ת��ΪIFSF�豸��ӵ���Լ�������,
���������������, һ���������д�Э��ת���豸������,һ������IFSF�豸������
2.����������д�ת��Э��ķ�IFSF�豸����һ��TCP�������,������μ�ifsf_main�ļ�.
3.IFSF��TCPIPͨѶ������Ҫ�����ݽṹ��IFSF_HEARTBEAT���ṹIFSF_HEARTBEAT��һ�����������豸��
��������������������Ҫ���͵�10���ֽڵĽṹ��IFSF_HB_SHM������һ������ڹ����ڴ��е�
�豸���������ڹ���ǰ����Ӧ�����������豸���豸�����������������, 
Ҳ������һ�����ݿ��еı�ʹ��ʱע�⣬�ʵ�ʹ��P��V������
4.!!!������IFSF FCC PCD���������ļ�pcd.cfg,���û�и��ļ������ļ�����,
��ôϵͳ����һ��tcp���������ȴ�����,Ȼ�����pcd.cfg�ļ��е��������
��ʼ��gpIfsf_shm��gpIfsf_shm,��ʼ�����м��ͻ�,�������͵���ΪClosed״̬,
��������CD ���ÿ���ģʽ, �·��ͼ�,������Ʒ,������Ʒ��ǹ��ϵ,
���򿪼��ͻ�(Open_FP),������������״̬��Ϊ����(ȫ0),ʹ�ü��ͻ�����
Idle״̬,��ʼ����.
-------------------------------------------------------------------
2007-06-21 by chenfm
2008-02-27 - Guojie Zhu <zhugj@css-intelligent.com>
    TCP Server �� TCP Client����Ϊ����, ���Ӿ���Ϊ������
    ���ж�SendDataBySock�ĵ��þ���ΪSendData2CD
2008-03-04 - Guojie Zhu <zhugj@css-intelligent.com>
    TcpRecvCDServ ����Ϊ TcpSend2CDServ
    ����AddTLGSendHB
2008-03-06 - Guojie Zhu <zhugj@css-intelligent.com>
    convertUsed[] ��Ϊ convert_hb[].lnao.node
    HBSend/DelSendHB.convertUsed[] ��Ϊ node_online[]
2008-03-26 - Guojie Zhu <zhugj@css-intelligent.com>
    ��POS����, �ر�POS.C -> FCC.S ������
2008-05-04 - Guojie Zhu <zhugj@css-intelligent.com>
    �޸�TCP Server �Ĵ������, ����serverΪ������ģʽ, ͬʱ���������������Ϊ 1
2008-05-05 - Guojie Zhu <zhugj@css-intelligent.com>
    ���������ϴ���״̬������Ϣ����Ϣ���ȴ����bug(0xE0 -> 0x0E)
2008-05-10 - Guojie Zhu <zhugj@css-intelligent.com>
    ����DoReonline���豸������Ҳ����״̬��Ϣ�Ĵ���
2008-05-13 - Guojie Zhu <zhugj@css-intelligent.com>
    ��������ǰ�����ж�,���POS�������򲻷�������
    

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
		OldAction = SetAlrm( nTimeOut, NULL );  /*�����alarm���ú�,connect����ǰ��ʱ,��connect�ȴ�ʱ��Ϊȱʡʱ��*/
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
		UserLog("�����ڴ�ֵ����,Del ����ʧ��");
		return -1;
	}
	for(i=0; i< MAX_CHANNEL_CNT; i++){
		if (gpIfsf_shm->convert_hb[i].lnao.node == node  ) {
			break; //�ҵ��˿��м��ͻ��ڵ�
		}
	}
	if (i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ����������������Ҳ���,SetHbStatus ʧ��");
		return -1;
	}
	gpIfsf_shm->convert_hb[i].status = status;
		
	return 0;	
}

//��һ���豸���������ܼ������ɾ��,recvsendΪ�������������Ǹ���,
//ɾ��CD���豸ʱҪ�鿴gpIsfs_fp_shm�Ľ����������Ƿ���������,�Ǿͽ������?.
int AddSendHB(const int node){//first read IO, second add dispenser data,finally add heartbeat.
	int i;
	//int p;
	IFSF_HEARTBEAT ifsf_heartbeat;
	extern char PCD_TCP_PORT[];
	if ( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,Add ����ʧ��");
		return -1;
	}
	for(i=0; i< MAX_CHANNEL_CNT; i++){
		if (  gpIfsf_shm->node_channel[i].node == node )  
			break; //�ҵ��˼��ͻ��ڵ�
	}
	if (i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ��Ҳ���,���ͻ�����Add  ʧ��");
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
		UserLog("�����ڴ�ֵ����,Add ����ʧ��");
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
		UserLog("�����ڴ�ֵ����,Del ����ʧ��");
		return -1;
	}
	for(i=0; i< MAX_CHANNEL_CNT; i++){
		if (gpIfsf_shm->convert_hb[i].lnao.node == node) {
		      break; //�ҵ��˿��м��ͻ��ڵ�
		}
	}
	if (i == MAX_CHANNEL_CNT ){
		UserLog("���ͻ��ڵ����������������Ҳ���,���ͻ�Del  ʧ��");
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
		UserLog("�����ڴ�ֵ����,Add ����ʧ��");
		return -1;
	}
	if ( NULL == device ){		
		UserLog("�����յ�ֵַ����,Add ����ʧ��");
		return -1;
	}
	for(i=0; i< MAX_RECV_CNT; i++){
		if (  gpIfsf_shm->recvUsed[i] <= MAX_NO_RECV_TIMES  ) 
			break; //�ҵ��˿����豸�ڵ�
	}
	if (i == MAX_RECV_CNT ){
		UserLog("���ͻ��ڵ����������������Ҳ���,���ͻ�Add  ʧ��");
		return -1;
	}
	gpIfsf_shm->recvUsed[i] = HEARTBEAT_TIME;	
	memcpy( &(gpIfsf_shm->recv_hb[i]), device, sizeof(IFSF_HEARTBEAT) );	
	return 0;
}
int DelRecvHB(const IFSF_HEARTBEAT *device){
	int i, node;
	if ( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,Del ����ʧ��");
		return -1;
	}
	if ( NULL == device ){		
		UserLog("�����յ�ֵַ����,Del ����ʧ��");
		return -1;
	}
	node = device->lnao.node;
	for(i=0; i< MAX_RECV_CNT; i++){
		if (  gpIfsf_shm->recvUsed[i] == node  )  break; //�ҵ��˼��ͻ��ڵ�
	}
	if (i == MAX_RECV_CNT ){
		UserLog("�豸�ڵ����������Ҳ���,�豸Del  ʧ��");
		return -1;
	}
	gpIfsf_shm->recvUsed [i]= MAX_NO_RECV_TIMES;
	memset( &(gpIfsf_shm->recv_hb[i]), 0, sizeof(IFSF_HEARTBEAT) );	
	//gpIfsf_shm->resvCnt --;
			
	return 0;
}
//static int PackupDevShm(const int index);//�����豸��

//!!!  public����,����Ϊ����.c�ļ���Ҫ���õĺ���:
/*
����TCP�����������д�Э��ת�����豸����һ��TCP�������,
����0����������-1ʧ��,
Ȼ������IFSF_SHM�����ڴ��ֵ.  ������Ҫ����port��backLog�ο�ListenSock()�ڳ�����ʵ��.
ע�ⲻҪ���յ������������ʹ�ת�����豸����. 
��ο��͵���tcp.h��tcp.c�ĺ���.
������Ҫ����ifsf_fp.c�е�TranslateRecv()����.
*/
/*
 * POS.Client -> POS.Server
 * �����������ݵ�ͨ��
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
		UserLog( "��ʼ��socketʧ��.port=[%s]\n", port );
		return -1;
	}
	fcntl(sock, F_SETFL, O_NONBLOCK);   // ������ģʽ
	
	newsock  = -1;
	tmp_sock = -1;
	while (1) {	
		if (((errcnt > 32) || (gpIfsf_shm->auth_flag == FCC_SWITCH_AUTH)) && newsock > 0) {
			RunLog("Out of ERR_CNT/CD offline, Close the connection <--X---------------");
			close(newsock);
			newsock = -1;
			errcnt = 0;
		}


		// һֱ����, �����������, ��ر�ǰһ������
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
			// һ��CD����FCC, ��ôFCCҲ����CD?
			// ���ܼ�ʱ���������������, ��ʱֻ����� 
			// (�ϰ汾:2.6.21û�м�����״̬���İ汾����Ҫ��ô��)
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
//				printf("��ʱ,���յ���̨���� buffer[%s]\n",  Asc2Hex(buffer, 32));
//				RunLog("errcnt is %d errno is %d", errcnt, errno);
				continue;
			} else if ((ret == -2) || errcnt > 16 && (gpIfsf_shm->need_reconnect == 0)) { 
				 // maybe CD quit or restart
				if (gpIfsf_shm->recvSockfd[0] >= 0) {
					gpIfsf_shm->need_reconnect = 1;
					RunLog("Client need to reconnect to CD");
				}
			}	
			RunLog( "��������ʧ��,errno:[%d][%s]", errno, strerror(errno) );
			RunLog("buffer: %s", Asc2Hex(buffer, 32));
			errcnt++;
			continue;
		} else {
			RunLog( "<<<�յ�����: [%s(%d)]", Asc2Hex(buffer, msg_lg ), msg_lg);
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
			//�����̨����.
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
				UserLog( "��������ʧ��,Subnet����1��9." );
			}

			errcnt = 0;
			//sleep(10);			
		}

		if ( errcnt > 40 ) {
			/*������*/
			UserLog( "��ʼ��socket ʧ��.port=[%s]\n", port );
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

//ֹͣTCP���񣬷���0����ֹͣ,-1ʧ��
//int StopTcpRecvServ();

/*
 �������ݵ������豸,
 sendBuffer:����������,msgLength:���ݳ���,timeoutΪ���ͳ�ʱֵ.
 �����豸�߼���ַΪsendBuffer�е�ǰ�����ֽ�
 �ɹ�����0,ʧ�ܷ���-1.��ο��͵���tcp.h��tcp.c�ĺ���.
*/
/*
 * д���������ϵ�Buffer
 */
int SendData2CD(const unsigned char *_sendBuffer, const int _msgLength, const int timeout)
{
	int i, ret, tmp, sockfd, sendcnt;

	// -------- ������ʽ -------------
	unsigned char buffer[32] = {0};
	unsigned char *sendBuffer = NULL;
	int msgLength;

	// �����Acknowledge, ������MS_ACK��05, �򲻷���Data_Ack
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
		UserLog("�����ڴ�ֵ����,SendData2CD ʧ��");
		return -1;
	}
	if ( (NULL == sendBuffer) || (0 == msgLength) ){		
		UserLog("����ֵ����,SendData2CD ʧ��");
		return -1;
	}
	if (msgLength > 1024){
		UserLog("����ֵ����,SendData2CD ʧ��");
		return -1;
	}
	

	
	if (gpIfsf_shm->auth_flag != FCC_SWITCH_AUTH){
		/*������ӣ��ź������������⵼�½���������������Ϣ����*/
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
 * ���ʵ�ʵ����ݷ���
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
		UserLog("�����ڴ�ֵL����,SendData2CD ʧ��");
		return -1;
	}
	if ( (NULL == sendBuffer) || (0 == msgLength) ){		
		UserLog("����ֵ����,SendData2CD ʧ��");
		return -1;
	}
	//only for log.
	{
		memcpy(tempchar, sendBuffer, len);
//		UserLog(">>>FCC�ϴ� : %s(%d)", Asc2Hex(tempchar, len), msgLength);
	}

	cdNode = sendBuffer[1];
	for(i=0; i< MAX_RECV_CNT; i++){
		if (gpIfsf_shm->recv_hb[i].lnao.node== cdNode )
			break; //�ҵ���CD �ڵ�
	}
	if (i == MAX_RECV_CNT ){
		for(i=0; i< MAX_RECV_CNT; i++){
			if (gpIfsf_shm->recv_hb[i].lnao.subnet == 2  ) 
				break; //�ҵ��˿���CD �ڵ�
		}
		if (i == MAX_RECV_CNT ){
			gpIfsf_shm->auth_flag = FCC_SWITCH_AUTH;
			UserLog("CD  ���������Ҳ���,SendData2CD ʧ��");
			return -1;
		}
	}

	tryn = 5 ;
	while(1){
#if 0
		if (gpIfsf_shm->auth_flag == FCC_SWITCH_AUTH){
			gpIfsf_shm->need_reconnect = 0;      // ��������˾Ͳ�Ҫ�ر�������
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
					RunLog("��������[%s(%d)]����λ��ʧ��[%d], ����", 
						Asc2Hex(tempchar, len), msgLength, errno);
					continue;
				} else {
					close(sockfd);
					gpIfsf_shm->recvSockfd[i] = -1;
					sockflag = 0;
					UserLog("��������[%s(%d)]����λ��ʧ��.", Asc2Hex(tempchar, len), msgLength);
					return -1;
					
				}
			}

			RunLog(">>>FCC�ϴ� : %s(%d)", Asc2Hex(tempchar, len), msgLength);
			if (gpIfsf_shm->css_data_flag > 0 && gCssFifoFd >= 0) {
				bzero(IFSFchar, sizeof(IFSFchar));
				memcpy(IFSFchar, &msgLength, 4);
				memcpy(&IFSFchar[4], tempchar, msgLength);
				ret = write(gCssFifoFd, IFSFchar, msgLength + 4);
				RunLog("д�������� %s(%d)", Asc2Hex(IFSFchar, len), ret);
			}
		} else {
			UserLog("����TCP����FCC->Fuel Server���ɹ�, �������ݸ���λ��ʧ��");
			return -1;
		}
		break;
	}
	
	return 0;
}
/*
 �������ݵ�����������newsock,
 sendBuffer:����������,msgLength:���ݳ���,timeoutΪ���ͳ�ʱֵ.
 �ɹ�����0,ʧ�ܷ���-1.��ο��͵���tcp.h��tcp.c�ĺ���.
*/
int SendDataBySock(const int newsock, const unsigned char *sendBuffer, const int msgLength, const int timeout){
	int i, ret, tmp;
	if ( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,SendDataBySock ʧ��");
		return -1;
	}
	if ( (NULL == sendBuffer) || (0 == msgLength) ){		
		UserLog("����ֵ����,SendDataBySock ʧ��");
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
		UserLog("Tcp Send Data���ɹ�,SendDataBySock ʧ��");
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
		UserLog("��������,Translate  ʧ��");
		return -1;
	}	
	m_st = ( (int)recvBuffer[5]&0xe0) >>5;
	//ad_lg = recvBuffer[8];
	//ad1 = recvBuffer[9];
	
	if ( node <= 0 ){
		UserLog("���ͻ��ڵ�ֵ����,Translate   ʧ��");
		return -1;
		}
	if ( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,Translate   ʧ��");
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
		UserLog("���ͻظ����ݴ���,Translate   ʧ��");
		return -1;
	}
	
	return 0;
}

/*
 * TcpSend2CDServ
 * FCC.Client -> POS.Server
 * �����������ݵ�ͨ��
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
		UserLog("�����ڴ�ֵ����,RecvCDServ ʧ��");
		//return -1;
		exit(-1);
	}

	Signal(SIGPIPE, SIG_IGN);

	while(1){
		//UserLog("TcpSendCDServ----0----, i: %d , pid:%d, ppid:%d", i, getpid(),getppid());
		for(i=0; i< MAX_RECV_CNT; i++){
			if (  gpIfsf_shm->recv_hb[i].lnao.subnet == 2  ) 
				break; //�ҵ��˿���CD �ڵ�
		}

		if (i == MAX_RECV_CNT ){		
			//UserLog("CD  ���������Ҳ���,SendData2CD ʧ��");
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
			gpIfsf_shm->need_reconnect = 0;      // ��������˾͸�λ���
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
				RunLog("����ʧ��");
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
					RunLog("����ʧ��2");
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
	
	//�������������ݴ�����1.
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

		if ((gpIfsf_shm->node_online[step] != 0)              // �ڵ�����
			&& (gpIfsf_shm->convert_hb[step].port != 0)) {   //�ҵ��˼��ͻ��ڵ�
			ifsf_heartbeat = (IFSF_HEARTBEAT *)(&(gpIfsf_shm->convert_hb[step]));
			//RunLog("HBSend  starting....[%d] is sending, ppid:[%d]", step,getppid() );
			//RunLog("HBSend  starting....[%d], pid:%d, ppid:%d, time:%d, send size:%d, ip:%s:%d",  \
			//step,  getpid(), getppid(),  time, sizeof(IFSF_HEARTBEAT), inet_ntoa(((struct sockaddr_in *)remoteSrv)->sin_addr), ntohs(((struct sockaddr_in *)remoteSrv)->sin_port));


			if ((ifsf_heartbeat->lnao.subnet == 0x09) && (gpIfsf_shm->tank_heartbeat == 0) ){
				RunLog("======Ϊ������ǿ��������������ң��ر�Һλ������======");
			}else{
				ret = sendto(sockfd, ifsf_heartbeat, sizeof(IFSF_HEARTBEAT),
						0, remoteSrv, sizeof(struct sockaddr));
			}

			if (ret < 0) {				
				UserLog("�������ݷ���ʧ��,error:[%s]", strerror(errno));	
			}
			if (gpIfsf_shm->css_data_flag > 0 && gCssFifoFd >= 0) {
				len = 11;
				memcpy(hearttmp, &len, 4);
				hearttmp[4] = 0xca;
				memcpy(&hearttmp[5], (char *)ifsf_heartbeat, sizeof(IFSF_HEARTBEAT));
				ret = write(gCssFifoFd, hearttmp, 15);
				if (ret == 15) {
					//RunLog("IFSF�������ݷ��Ͷ���ɹ�%s", Asc2Hex(hearttmp, 15));
				}
				else {
					RunLog("������������ʧ��ret = %d", ret);
				}
			}
		}

		k++;			
	}	

	return 0;
}

//�������豸�������ͷ��񣬷���0����������-1ʧ��.
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
		UserLog( "�򿪶���ܵ��ļ�[%s]ʧ��errno[%d]\n", "/tmp/css_fifo.data" ,errno);
	} else{
		UserLog( "�򿪶���ܵ��ļ�[%s]�ɹ�", "/tmp/css_fifo.data" );
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
 * �������ͷ���, ��׼��--ȫ���㲥
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
 * Macro: �������߼���Լ���ش���
 *        �ѻ�������,�����н���ȫ��д���ļ��ٸı���Ȩ���Ϊ��λ����Ȩ
 */
#define offline_detect_and_handle()   do {                              \
	auth_flag = gpIfsf_shm->auth_flag;                              \
	for (i = 0; i< MAX_RECV_CNT; i++) {                             \
		if ((gpIfsf_shm->recvUsed[i] > MAX_NO_RECV_TIMES) &&    \
			(gpIfsf_shm->recv_hb[i].lnao.subnet == 2)) {  /* �����CD������ */            \
	/*		gpIfsf_shm->auth_flag = DEFAULT_AUTH_MODE; */   /* ����Ϊ��λ��Ĭ��ģʽ.*/    \
			auth_flag = DEFAULT_AUTH_MODE;                  \
			break;                                          \
		}                                                       \
	}		                                                \
	if (i == MAX_RECV_CNT) {                                        \
		RunLog("=========  ��λ�����ߣ�FCC�ӹܿ���ģʽ!! =========");                         \
		auth_flag = FCC_SWITCH_AUTH;                            \
	}                                                               \
	if (old_offline_flg != auth_flag) {                             \
		for (i = 0; i < MAX_CHANNEL_CNT; i++) {		        \
			if (gpIfsf_shm->convert_hb[i].lnao.node != 0) { /* �ڵ���� */                \
				if (auth_flag != FCC_SWITCH_AUTH) {/*����ת����*/                     \
					RunLog("=========  ��λ���ָ�����!! ======================"); \
				} else {     /* ����ת���� */                                         \
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

//�������豸�������շ��񣬷���0����������-1ʧ��.
int HBRecvServ(){ //ע�ⲻҪ���ձ������͵�����.
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
	static __u8 old_offline_flg; //����modify by liys  2008-03-14
	static int auth_flag = 0;
       
	if ( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,�������� ʧ��");
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
		// recvfrom �ĳɶ�ʱ�� !!!
		ret = recvfrom( sockfd,  buffer, sizeof(IFSF_HEARTBEAT), 0,\
			(struct sockaddr *)&remoteSrv, &fromlen);   //sizeof(IFSF_HEARTBEAT)
		if ( (ret == 0) || (84 == errno ) ){
			pIfsf_heartbeat =(IFSF_HEARTBEAT *)buffer;
			RunLog("HBRecvServ.....while2.., pid:%d, ppid:%d, node:%d , subnet: %d", \
				getpid(),getppid(), pIfsf_heartbeat->lnao.node,pIfsf_heartbeat->lnao.subnet);
			if ((1 == (int)pIfsf_heartbeat->lnao.subnet) ||
				(9 == (int)pIfsf_heartbeat->lnao.subnet)||
				(((int)pIfsf_heartbeat->lnao.node > 1)&&(2==((int)pIfsf_heartbeat->lnao.subnet)))) {  // ������ Dispenser & TLG ������
				offline_detect_and_handle();               
				continue;
			}

			for(i=0; i< MAX_RECV_CNT; i++){
				if (( gpIfsf_shm->recv_hb[i].lnao.node == pIfsf_heartbeat->lnao.node ) && 
					(gpIfsf_shm->recv_hb[i].lnao.subnet == pIfsf_heartbeat->lnao.subnet)) {
					//�ҵ��˼��ͻ��ڵ�
					break;
				}
			}

			if (i == MAX_RECV_CNT){ 
				for(i = 0; i< MAX_RECV_CNT; i++){
					if (gpIfsf_shm->recvUsed[i] < MAX_NO_RECV_TIMES) {//�ҵ���
						RunLog("Find empty space to save new received heartbeat");
						break;
					}
				}
			}
			if (i != MAX_RECV_CNT){
				memcpy( &gpIfsf_shm->recv_hb[i], pIfsf_heartbeat,sizeof(IFSF_HEARTBEAT) );
				gpIfsf_shm->recvUsed[i] = HEARTBEAT_TIME;
			} else {
				UserLog("�����������ݴ���, û�д洢,i:[%d]",i);
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
 * ��׼IFSF�������շ��� (ȫ���㲥)
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
		UserLog("�����ڴ�ֵ����,������������ʧ��");
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
		// recvfrom �ĳɶ�ʱ�� !!!
		ret = recvfrom( sockfd,  buffer, sizeof(IFSF_HEARTBEAT), 0,\
			(struct sockaddr *)&remoteSrv, &fromlen);   //sizeof(IFSF_HEARTBEAT)
		if ( (ret == 0) || (84 == errno ) ){
			pIfsf_heartbeat =(IFSF_HEARTBEAT *)buffer;
			//UserLog("HBRecvServ.....while2.., pid:%d, ppid:%d, node:%d , subnet: %d", \
			//	getpid(),getppid(), pIfsf_heartbeat->lnao.node,pIfsf_heartbeat->lnao.subnet);
			if (1 == (int)pIfsf_heartbeat->lnao.subnet ||
				(9 == (int)pIfsf_heartbeat->lnao.subnet)) {  // ������ Dispenser & TLG ������
				offline_detect_and_handle();               
				continue;
			}
			for(i=0; i< MAX_RECV_CNT; i++){
				if (( gpIfsf_shm->recv_hb[i].lnao.node == pIfsf_heartbeat->lnao.node ) && 
					(gpIfsf_shm->recv_hb[i].lnao.subnet == pIfsf_heartbeat->lnao.subnet)) {
					//�ҵ��˼��ͻ��ڵ�
					break;
				}
			}
			if (i == MAX_RECV_CNT){ 
				for(i = 0; i< MAX_RECV_CNT; i++){
					if (gpIfsf_shm->recvUsed[i] < MAX_NO_RECV_TIMES) {//�ҵ���
						RunLog("Find empty space to save new received heartbeat");
						break;
					}
				}
			}
			if (i != MAX_RECV_CNT){
				memcpy( &gpIfsf_shm->recv_hb[i], pIfsf_heartbeat,sizeof(IFSF_HEARTBEAT) );
				gpIfsf_shm->recvUsed[i] = HEARTBEAT_TIME;
			} else {
				UserLog("�����������ݴ���, û�д洢, i:%d",i);
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


//ֹͣ���豸�������շ��񣬷���0����������-1ʧ��.
//int StopHBRecvServ(); 

/*-------------------------------------------------------------------*/
//private����,����Ϊ��.c����Ҫ�õ��ĺ���:
//----------------------------------
//��ȡ����IP,���ر���IP��ַ������-1ʧ�ܣ����DHCP����,��ΪIFSF����ʹ��DHCP
static long  GetLocalIP(){//�μ�tcp.h��tcp.c�ļ�����,����ѡ��һ��.
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
 * func: ��ȡ�㲥��ַ
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
		if( CatchSigAlrm )	/*��ʱ*//*���ܴ�����,ֻҪ��׽����ʱ������*/
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
	 0:	�ɹ�
	-1:	ʧ��
	-2:	��ʱ
*/
static ssize_t TcpSendDataInTime( int fd, const char *s, size_t len,  int timeout )
{
	ssize_t	 retlen;
	size_t	sendlen;
	size_t  value;
	char	*head;
	char	*t;
	char	buf[TCP_MAX_SIZE];
	int	mflag; 	/*��ʶ�Ƿ�����˿ռ� 0δ����. 1�ѷ���*/
	int	n;
	int     i;
	
	//Assert(s != NULL && *s != 0);
	Assert(len != 0);
	Assert(timeout > 0 || timeout == TIME_INFINITE);

	mflag = 0;
	sendlen = len;

	if (sizeof(buf) >= sendlen ) {	/*Ԥ�ȷ���ı���buf�Ŀռ��㹻��������Ҫ���͵�����*/
		t = buf;
	} else {			/*Ԥ�ȷ���ı���buf�Ŀռ䲻����������Ҫ���͵�����,�ʷ���ռ�*/
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
	if ( n == sendlen ) {	/*����ȫ������*/
		if (s[2] == 0x09 && VerifySendBufEmpt(fd, 2) != 0) {
			return -1;
		}
		return 0;
	} else if ( n == -1 ) {/*����*/
		return -1;
	} else if ( n >= 0 && n < sendlen ) {
		if ( GetLibErrno() == ETIME ) {/*��ʱ*/
			return -2;
		} else {
			return -1;
		}
	}

}

/*
 * s:	�������������
 * buflen:	ָ�롣*buflen������ʱΪs�ܽ��ܵ���󳤶ȵ����ݣ�������β����0.��char s[100], *buflen = sizeof(s) -1 )
 *		�����ʱΪsʵ�ʱ�������ݵĳ���(������β����0.)
 * ����ֵ:
 *	0	�ɹ�
 *	1	�ɹ�.�����ݳ��ȳ����ܽ��ܵ���󳤶�.���ݱ��ض�
 *	-1	ʧ��
 *	-2	��ʱ�������ӹ���ر�
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
		if ( GetLibErrno() == COMM_EOF )	/*�Զ˹ر�*/
			return -2;
		else
			return -1;
	}

	//if ( !IsDigital(head) )
	//	return -3;	/*�Ƿ��İ�����*/

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


	Ssleep(15000000);         // �ӳ�15s,ȷ��CD�Ѿ��յ� dispensers & TLG ������
	for(i=0; i< MAX_RECV_CNT; i++){
		if (gpIfsf_shm->recv_hb[i].lnao.subnet == 2) {
			break;    //�ҵ��˿���CD �ڵ�
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
 


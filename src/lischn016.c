/*
 * Filename: lischn016.c
 *
 * Description: ���ͻ�Э��ת��ģ�� -FCC������ͻ�
 *
 * History:
 *
 * 2008/07/03 - Chen Fu Ming <chenfm@css-intelligent.com>
 *
 * ����˵��:
 *   ���ڻظ�ACK������, ����ע��BB/CC���ж�, ����У�����ֻ���м�¼
 *   �����ڻظ������ݵ�����, �����ע��У����ж�, У������������
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
static int fp_state[MAX_FP_CNT];   // 0 - IDLE, 1 - CALLING , 2 - DoStarted�ɹ�, 3 - FUELLING
static int fpUpFlag[MAX_FP_CNT];   // 0 - Down, not 0 - Up gun number, only for transaction
/******************************
*���ú���
*******************************/
/*��nTimeOut����ΪTIME_INFINITE�������õ�ʱ���ϵͳȱʡʱ�䳤ʱ,
 connect����ɷ���ǰ��ֻ�ȴ�ϵͳȱʡʱ��*/
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
		OldAction = SetAlrm( nTimeOut, NULL );  /*�����alarm���ú�,connect����ǰ��ʱ,��connect�ȴ�ʱ��Ϊȱʡʱ��*/
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
	 0:	�ɹ�
	-1:	ʧ��
	-2:	��ʱ
*/
static ssize_t __TcpSendDataInTime( int fd, const char *s, size_t len,  int timeout )
{
	ssize_t	 retlen;
	size_t	sendlen;
	char	*head;
	char	*t;
	char	buf[TCP_MAX_SIZE];
	int	mflag; 	/*��ʶ�Ƿ�����˿ռ� 0δ����. 1�ѷ���*/
	int	n;
	int     i;
	
	Assert( s != NULL && *s != 0 );
	Assert( len != 0  );
	Assert( timeout > 0 || timeout == TIME_INFINITE );

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
	memcpy( t, s, len );
	//RunLog("before WirteLengthInTime -------------");
	n = WriteByLengthInTime( fd, t, sendlen, timeout );
	if ( mflag == 1 ) {
		Free(t);
	}

	if ( n == sendlen ) {	/*����ȫ������*/
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
		if ( GetLibErrno() == ETIME )	//��ʱ/
			return -2;
		else
			return -1;
	}

	//if ( !IsDigital(head) )
	//	return -3;	/*�Ƿ��İ�����*/

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
	
	sleep(5);   //��ֹ�˽����˳�,��������û���������. ÿ��lischnxxx����Ҫ
	RunLog("Listen Chenel  016 ,logic chennel[%d]", chnLog);
	memset(&pump, 0, sizeof(pump));
	pump.chnLog = chnLog;
	pump.panelCnt = 0;

	// ============ Part.1 ��ʼ�� pump �ṹ  ======================================
	// ɨ�蹲���ڴ�, ����ͨ�����ӵĴ���, ���
	
	for (i = 0; i < MAX_CHANNEL_CNT; i++) {
		if (chnLog == gpIfsf_shm->node_channel[i].chnLog) {
			IFSFpos = i;
			lpNode_channel = (NODE_CHANNEL *)(&(gpIfsf_shm->node_channel[i]));
			// node_channel��dispenser_data����һһ��Ӧ
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
	// ============= Part.2 �� socked,������ ===================
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
				//RunLog("�򿪴���[%s]ʧ��", pump.cmd);
				RunLog("ģ����ͻ�����TCP�˿�ʧ��,�˿�[%d].fd=[%d],IP��ַ[%s]", tcpPort,pump.fd,simulateIP);
				continue;
			}
			RunLog("ģ����ͻ�����TCP�˿�[%d].fd=[%d]", tcpPort,pump.fd);
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
			RunLog("======�ڵ�[%d] ����! ===========================", node);
			continue;
		}else{
                    if (gpIfsf_shm->node_online[IFSFpos] == 0) {
                          while (time(NULL) < pump.time_offline) {
				       ret = GetPumpOnline(0, &status);
					RunLog("====================== wait for 30s, ==============");
				}
                      RunLog("====== �ڵ�[%d] ����! =======================", node);
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
	
	// ============== Part.3 �жϼ��ͻ��Ƿ����� ====================================
	// Check wether dispenser is online, if it's not online, wait  here.
	while (1) {
		static int n = 0;
		ret = GetPumpOnline(0, &status);
		if (ret == 0) {
			break;
		}

//		gpIfsf_shm->node_online[IFSFpos] = 0; //������,����������.
		ChangeNode_Status(node, NODE_OFFLINE);
		UserLog("Node %d ����", node);
		
		if (n < 60) {
			n++;
			sleep(n);
		} else {
			sleep(60);
		}
	}
	
	// ============= Part.4 ��ʼ������ͻ���ǰ״̬��ص����ݿ������ =======================

	ret = InitGunLog();    // Note.InitGunLog ������ÿ��Ʒ���ͻ�������,�����ͻ�Ԥ�ü�����Ҫ
	if (ret < 0) {
		RunLog("��ʼ��Ĭ�ϵ�ǰ�߼�ǹ��ʧ��");
		return -1;
	}

	ret = InitCurrentPrice(); //  ��ʼ��fp���ݿ�ĵ�ǰ�ͼ�
	if (ret < 0) {
		RunLog("��ʼ����Ʒ���۵����ݿ�ʧ��");
		return -1;
	}

	ret = InitPumpTotal();    // ��ʼ�����뵽���ݿ�
	if (ret < 0) {
		RunLog("��ʼ�����뵽���ݿ�ʧ��");
		return -1;
	}


//	gpIfsf_shm->node_online[IFSFpos] = node;  // һ�о���, ����
	ChangeNode_Status(node, NODE_ONLINE);
	RunLog("�ڵ� %d ����,״̬[%02x]!", node, status);
	sleep(10);     // ȷ����λ���յ����������ϴ�״̬
	

	// =============== Part.5 �ͻ�ģ���ʼ�����,ת��FP״̬Ϊ Idle ==================
	// when dispenser is initialized , the status is Closed. it's waiting for set command from CD. 
	RunLog("Node %d cntOfFps == %d", node, lpNode_channel->cntOfFps);
	for (i = 0; i < lpNode_channel->cntOfFps; i++) {
		if ((__u8)FP_State_Closed != lpDispenser_data->fp_data[i].FP_State) {
			ret = ChangeFP_State(node, 0x21 + i, FP_State_Closed, NULL);
			RunLog("Node %d FP1_State %d", node, lpDispenser_data->fp_data[i].FP_State);
		}
	}

	// =============== Part.6 ����ʱ������� ========================================
	while (1) {
		if (gpGunShm->sysStat == RESTART_PROC ||
			gpGunShm->sysStat == STOP_PROC) {
			RunLog("ϵͳ�˳�");
			exit(0);
		}

		while (gpGunShm->sysStat == PAUSE_PROC) {
			__u8 status;
			GetPumpStatus(0, &status);              // Note. ���ͻ�������ϵ
			Ssleep(INTERVAL);
			RunLog("ϵͳ��ͣ");
		}

		indtyp = pump.indtyp;

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		ret = -1;

		// ---------- Part.6.1 ��̨��������µĴ������� ----------------------
		if (gpIfsf_shm->auth_flag - FCC_SWITCH_AUTH != 0) { 
		       	/* 
			 * ����Ƿ��к�̨����, �������ת��DoCmdִ�и�����; ���û��, �������ѯ
			 */
			ret = RecvMsg(ORD_MSG_TRAN, pump.cmd, &pump.cmdlen, &indtyp, TIME_FOR_RECV_CMD); 
			if (ret < 0) {
				DoPolling();
			} else {
				ret = DoCmd();
				if (ret != 0) {
					RunLog("�����̨����ʧ��");
				}
			}
			Ssleep(500000);
		} else { // ------------- Part.6.2 ��̨��������µĴ������� ------------ 
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
 * Func: ִ����λ������
 * �����ʽ: �������(2Bytes) + FP_Id(1Byte) + ��׼���������"Ack Message"�е�λ��(1Byte) +
 *           �������ݳ���(1Byte) + Ack Message(nBytes)
 */
static int DoCmd()
{
	int ret;
	int fp_id;
	int pos;    //pos is index of  fuelling poit(panel) of a dispenser: 0, 1, 2, 3
	unsigned int cnt;
	__u8 msgbuffer[48];


	if (pump.cmd[0] != 0x2F) {
		// ��ȡ��������ִ�к���ǰ�Ĺ��в���
		fp_id = pump.cmd[2];
		if (fp_id == 0x00) {  // fp_id == 0x00 ��ʾ������fp����, ��Ӧ pos ��Ϊ 255
			pos = 255;
		} else if ((fp_id >= 0x21) && (fp_id <= 0x24)) {
			pos = fp_id - 0x21;
		} else {
			if (pump.cmd[0] != 0x18 && pump.cmd[0] != 0x19) {  // DoPrice & DoMode����Ҫ��������
				RunLog("�Ƿ�������ַ");
				return -1;
			}
		}				

		bzero(msgbuffer, sizeof(msgbuffer));
		memcpy(msgbuffer, &pump.cmd[3], pump.cmd[4] + 2);  // ���β����� Acknowledge Message
	}

	ret = -1;
	switch (pump.cmd[0]) {
		case 0x18:	//  Change Product Price
			ret = DoPrice();		
			break;		
		case 0x19:	//  �����ͻ��Ŀ���ģʽ: ��λ������ �� ��������
//			ret = DoMode();	      // Unsupported
			break;
		case 0x1C:	// Terminate_FP
			// Step.1 �Ƚ��д�����, ��鵱ǰ״̬�Ƿ�������иò���
			ret = HandleStateErr(node, pos, Terminate_FP,
				lpDispenser_data->fp_data[pos].FP_State, msgbuffer); 
			if (ret < 0) {
				break;
			}
			// Step.2 ִ������, ���ɹ��ı� FP ״̬, ���򷵻� NACK
			ret = DoStop(pos);
			if (ret == 0) {
				ret = ChangeFP_State(node, pos + 0x21, FP_State_Idle, msgbuffer);
			} else {
				SendCmdAck(NACK, msgbuffer);
			}

			break;
		case 0x1D:	// Release_FP
			ret = HandleStateErr(node, pos, Do_Auth,     // Note. ������Do_Auth ������ Release_FP 
				lpDispenser_data->fp_data[pos].FP_State, msgbuffer); 
			if (ret < 0) {
				break;
			}
			ret = DoAuth(pos);				
			if (ret == 0) {
				// �������⴦��, ��Ϊ������Started״̬
				// ���ͻ�������и�״̬ʱ, ״̬�л����ͻ�ʵ���������
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
			ret = HandleStateErr(node, pos, Do_Ration,   // Note. ������Do_Ration
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
			RunLog("���ܵ�δ��������[0x%02x]", pump.cmd[0]);
			break;
	}

	return ret;
}

/*
 * func: ��FP����״̬��ѯ, ����״̬����Ӧ����
 */
static void DoPolling()
{
	int i,j;
	int ret;
	int tmp_lnz;
	__u8 status;
	static __u8 pre_status[MAX_FP_CNT] = {0};
	static int times = 0;
	static int fail_cnt = 0;  // ��¼GetPumpStatus ʧ�ܴ���, �����ж�����
	
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
					//RunLog("�򿪴���[%s]ʧ��", pump.cmd);
					//RunLog("ģ����ͻ�����TCP�˿�ʧ��,�˿�[%d].fd=[%d],IP��ַ[%s]", tcpPort,pump.fd,simulateIP);
					continue;
				}
				RunLog("ģ����ͻ�����TCP�˿�[%d].fd=[%d]", tcpPort,pump.fd);
				break;
			}
			if(pump.fd < 0){
				//RunLog("ģ����ͻ�����TCP�˿�ʧ��,�Ժ�����");
				RunLog("ģ����ͻ�����TCP�˿�ʧ��,�˿�[%d].fd=[%d],IP��ַ[%s],�Ժ�����",
										tcpPort,pump.fd,simulateIP);
				if (gpIfsf_shm->node_online[IFSFpos] == node) {
					/*RunLog("fail_cnt: %d ---------------", fail_cnt);
					if ((++fail_cnt) < MAX_FAIL_CNT) {  // ���ͻ������ж�
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
					RunLog("====== Node %d ����! =============================", node);
				}
				exit(0);//return; //
			}
			
		}
	
        for (i = 0; i < pump.panelCnt; i++) {
		ret = GetPumpStatus(i, &status);
		if (ret < 0) {
			if (gpIfsf_shm->node_online[IFSFpos] == node) {
				if ((++fail_cnt) < MAX_FAIL_CNT) {  // ���ͻ������ж�
					/*
					 * 2008.12.29���Ӵ˲���, ������ѹ�������
					 * ��Ƭ����λ�󴮿����ø�λ���յ���ͨѶ��ͨ������
					 */
					/*
					 * �����贮�����Իᵼ��ReadTTY���յ�"�Ƿ�����", ����ʱ�Ƴ�
					 * 2009.02.19
					if (fail_cnt >= (MAX_FAIL_CNT / 2)) {
						if (SetPort001() >= 0) {
							RunLog("�ڵ�%d���贮�����Գɹ�", node);
						}
					}
					*/
					continue;
				}

				
				// Step.2
//				gpIfsf_shm->node_online[IFSFpos] = 0;
				ChangeNode_Status(node, NODE_OFFLINE);
				pump.time_offline = (3 * gpIfsf_shm->fcc_com_sv_data.Heartbeat_Interval) + time(NULL);
				RunLog("======�ڵ�[%d] ����! ===========================", node);

				// Step.3
				for (j = 0; j < pump.panelCnt; ++j) {
					lpDispenser_data->fp_data[j].FP_State = FP_State_Inoperative;
					lpDispenser_data->fp_data[j].Log_Noz_State = 0;
				}
			}

			// ���贮������
			//ReDoSetPort001(); 2009.02.19
			continue;
		} else {
			if (gpIfsf_shm->node_online[IFSFpos] == 0) {
				if ((--fail_cnt) > 0) { // �ų�����
					fail_cnt = 1;
					continue;
				} else if (time(NULL) < pump.time_offline) {
					RunLog("====================== wait for 30s, ==============");
					continue;
				}

				fail_cnt = 0;

				// Step.1
				ret = InitPumpTotal();    // ��ʼ�����뵽���ݿ�
				if (ret < 0) {
					RunLog("�ڵ�[%d], ���±��뵽���ݿ�ʧ��",node);
					continue;
				}

				// Step.2
				for (j = 0; j < pump.panelCnt; ++j) {
					lpDispenser_data->fp_data[j].FP_State = FP_State_Closed;
				}

				// Step.3
//				gpIfsf_shm->node_online[IFSFpos] = node;
				ChangeNode_Status(node, NODE_ONLINE);
				RunLog("====== �ڵ�[%d] �ָ�����! =======================", node);
			
				// Step.4                 // �����ϴ���ǰFP״̬
				for (j = 0; j < pump.panelCnt; ++j) {
					ChangeFP_State(node, j + 0x21, FP_State_Closed, NULL);
				}
				continue;
			}

		}

		if (status != pre_status[i]) {
			pre_status[i] = status;
			RunLog("�ڵ�[%d]�����[%d]��ǰ״̬��Ϊ[%02x]", node, i + 1, status);
		}

		if (gpIfsf_shm->auth_flag != FCC_SWITCH_AUTH) {  //����к�̨
			if ((__u8)FP_State_Closed == lpDispenser_data->fp_data[i].FP_State) {
				if (i == 0) {
					times += pump.panelCnt;					
				}
				if (times > (SENDSTATE *  pump.panelCnt)) {
//					ret = ChangeFP_State(node, i + 0x21, FP_State_Closed, NULL);
					if (i >= pump.panelCnt -1)
						times = 0;
					UserLog("�ڵ�[%d]�����[%d]����Closed״̬,���ʼ����Open", node, i + 1);
				}
				ChangePrice();
				continue; 
			}
		} else {
			if ((__u8)FP_State_Closed == lpDispenser_data->fp_data[i].FP_State) {
				RunLog("DoPolling, û�к�̨�ı�FP[%02x]��״̬ΪIDLE", i + 0x21);
				ret = SetHbStatus(node, 0);   // IFSF_HEARTBEAT.status set to 0
				ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
			}
		}
		
		// ��FP״̬ΪInoperative, Ҫ���˹���Ԥ,�����ܸı���״̬,Ҳ�����ڸ�״̬�����κβ���
		if ((__u8)FP_State_Inoperative == lpDispenser_data->fp_data[i].FP_State) {
			RunLog("Node[%d].FP[%d]����Inoperative״̬!", node, i + 1);
			ChangePrice();
			continue;
		}
		if((status & 0x0F) > 3){//����ǹ���߼���
			//pump.gunLog[i] = pump.cmd[2];//���ǹ��
			fpUpFlag[i] = pump.cmd[2];//���ǹ��;
			RunLog("node %d fp %d gun %d is up... ",node,  i+1, fpUpFlag[i]);
		//09-09-03 @by lil  ���ÿ�ζ����ϴ�һ�����е�һ��ǹ����
                      pump.gunLog[i] = fpUpFlag[i];
		}
		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = 0;
		
		switch (status & 0x0F) {
		case FP_INOPERATIVE:
			ret = ChangeFP_State(node, i + 0x21, FP_State_Inoperative, NULL);
			break;
		case FP_IDLE:                                   // ��ǹ������
			if (fp_state[i] != 0) {   // Note.��û�вɵ���ǹ�ź�(EOT)�Ĳ��䴦��
				// ���мӹ���, �����״̬�ı�ͽ��״���
				// 1. ChangeFP_State to IDLE first
				tmp_lnz = lpDispenser_data->fp_data[i].Log_Noz_State;
				/*if ((status & 0x80) == 0) {   // ���׽���������δ��ǹ
					lpDispenser_data->fp_data[i].Log_Noz_State = 0x00;     
				}*/
				lpDispenser_data->fp_data[i].Log_Noz_State = 0x00;     
				if ((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State) {					
					ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
				}
				RunLog("Node %d FP %d �� %d ��ǹ��ǹ", node, i + 1,fpUpFlag[i]);//pump.gunLog[i]
				// Step2. Get & Save TR to TR_BUFFER
				lpDispenser_data->fp_data[i].Log_Noz_State = tmp_lnz;
				ret = DoEot(i);
				/*if ((status & 0x10) != 0) {
					ret = DoEot(i);
				} else {
					EndZeroTr4Fp(node, i + 0x21);
				}

				if ((status & 0x80) == 0) {   // ���׽���������δ��ǹ
					lpDispenser_data->fp_data[i].Log_Noz_State = 0x00;
				}*/ 
			}

			lpDispenser_data->fp_data[i].Log_Noz_State = 0x00;
			fp_state[i] = 0;
			// Terminate_FP ��FP״̬��ΪIdle, ������ǹ��Ϊ̧��״̬, ���䴦��
	/*		if (((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State) ||
				(lpDispenser_data->fp_data[i].Log_Noz_State != 0 && ((status & 0x80) == 0))) {
				lpDispenser_data->fp_data[i].Log_Noz_State = 0x00;
				ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
				RunLog("Node %d FP %d �� %d ��ǹ��ǹ", node, i + 1, pump.gunLog[i]);
			}  modify by liys */

			EO_OFFLINE_TR(node, i);
			if (gpIfsf_shm->fp_offline_flag[node - 1][i] != 0) {
				upload_offline_tr(node, NULL);
			}
			
                      // �е���������Ȩ״̬, ���踽���ж�״̬
			for (j = 0; j < pump.panelCnt; ++j) {
				if ((__u8)FP_State_Idle != lpDispenser_data->fp_data[j].FP_State) {
					break;
				}
			}
			// �����涼���вű��
			if (j >= pump.panelCnt) {
				ret = ChangePrice();            // �鿴�Ƿ�����Ʒ��Ҫ���,����ִ��
			}
			break;
		case FP_CALLING:                                // ̧ǹ
			if ((__u8)FP_State_Calling > lpDispenser_data->fp_data[i].FP_State) {
				ret = DoCall(i);
				fp_state[i] = 1;
			}

			ret = BeginTr4Fp(node, i + 0x21);
			if (ret < 0) {
				RunLog("DoPolling.PUMP_BUSY: BeingTr4FP ʧ��");
			}
			
			//���������ǹ��û�м�⵽����״̬����
			if( fp_state[i] > 1 ){
				tmp_lnz = lpDispenser_data->fp_data[i].Log_Noz_State;
				/*if ((status & 0x80) == 0) {   // ���׽���������δ��ǹ
					lpDispenser_data->fp_data[i].Log_Noz_State = 0x00;     
				}*/
				lpDispenser_data->fp_data[i].Log_Noz_State = 0x00;     
				if ((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State) {					
					ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
				}
				RunLog("Node %d FP %d �� %d ��ǹ��ǹ", node, i + 1,fpUpFlag[i]);//pump.gunLog[i]
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
					__u8 ln_status = 0;     // û�а취��GetCurrentLNZ���̧���ǹ��:(
					ln_status = 1 << (pump.gunLog[i] - 1);
					lpDispenser_data->fp_data[i].Log_Noz_State = ln_status;
					RunLog("Node %d FP %d �� %d ��ņ̀��", node, i + 1, pump.gunLog[i]);
				}
				ret = ChangeFP_State(node, i + 0x21, FP_State_Started, NULL);
#if 0
				RunLog("PUMP_Busy before BeingTr4Fp: Tr_seq_nb : %02x%02x", 
						lpDispenser_data->fp_data[i].Transaction_Sequence_Nb[0],
						lpDispenser_data->fp_data[i].Transaction_Sequence_Nb[1]);
#endif
				ret = BeginTr4Fp(node, i + 0x21);
				if (ret < 0) {
					RunLog("DoPolling.PUMP_BUSY: BeingTr4FP ʧ��");
				}
#if 0
				RunLog("PUMP_Busy after BeingTr4Fp: Tr_seq_nb : %02x%02x", 
						lpDispenser_data->fp_data[i].Transaction_Sequence_Nb[0],
						lpDispenser_data->fp_data[i].Transaction_Sequence_Nb[1]);
#endif
				fp_state[i] = 2;
			}
			break;
		case FP_FUELLING:                               // ����
			if ((__u8)FP_State_Fuelling != lpDispenser_data->fp_data[i].FP_State) {
				if (lpDispenser_data->fp_data[i].Log_Noz_State == 0) {
					__u8 ln_status = 0;     // û�а취��GetCurrentLNZ���̧���ǹ��:(
					ln_status = 1 << (pump.gunLog[i] - 1);
					lpDispenser_data->fp_data[i].Log_Noz_State = ln_status;
//					RunLog("Node %d FP %d �� %d ��ņ̀��", node, i + 1, pump.gunLog[i]);
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
						RunLog("DoPolling.PUMP_BUSY: BeingTr4FP ʧ��");
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
			break;     // CPPEI ��Ҫ���״̬
			if ((__u8)FP_State_Suspended_Started != lpDispenser_data->fp_data[i].FP_State) {
				ret = ChangeFP_State(node, i + 0x21, FP_State_Suspended_Started, NULL);
			}
			break;
		case FP_SPFUELLING:                            // Limit-Reached
			break;     // CPPEI ��Ҫ���״̬
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
 * ����ö��ͻ��Ķ�д��������֮ǰ�ѽ��й�״̬�жϺʹ���Ԥ����,
 * �����ھ��������ִ�к����о���������״̬�����
 * ��Ϊ�е��ͻ�ģ����ÿ���·�����֮ǰ����״̬�жϵĲ���, �ش�˵��
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
		RunLog("����״̬�ı����");
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
	static __u8 chain_no[2] = {0, 0};  // ����

	if (pump.cmd[0] == 0x3D) {   // Preset
		ration_type = 0x02;
		memcpy(pre_volume, &lpDispenser_data->fp_data[pos].Remote_Volume_Preset[2], 3);
	} else {                     // Prepay
		ration_type = 0x01;
		memcpy(pre_volume, &lpDispenser_data->fp_data[pos].Remote_Amount_Prepay[2], 3);
	}

	BBcdInc(chain_no, 2);  // ������1, 0001 - 9999
	for (trycnt = 0; trycnt < TRY_TIME; trycnt++) {

		//----------------------------------
		//�շ�����ǰ�����TCP����ͨ��
		pump.cmdlen = ERR_LENGTH;
		ret = __TcpRecvDataInTime(pump.fd, pump.cmd, &(pump.cmdlen), ERR_TCP_TIMEOUT);
		//-------------------------------------
		
		// Step.1 ����Ԥ������
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
			RunLog("дԤ������Node[%d].FP[%d]ʧ��", node, pos + 1);
			continue;
		}
		
		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = __TcpRecvDataInTime(pump.fd, pump.cmd, &(pump.cmdlen), TCP_TIMEOUT);
		if (ret < 0) {
			RunLog("дԤ������Node[%d].FP[%d]�ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		} 
		RunLog("ȡԤ�÷���:[%s]", Asc2Hex(pump.cmd, 4));
		if ((pump.cmd[0] !=0x11) &&( pump.cmd[0] !=0x22)) {
			RunLog("ִ��Ԥ�ü�������ʧ��");
			continue;
		}
		
		if (pump.cmd[3] != 0) {
			RunLog("ִ��Ԥ�ü�������ʧ��");
			continue;
		}

		RunLog("дԤ������Node[%d].FP[%d]�ɹ�", node, pos + 1);
			
		return 0; 
	}

	return -1;
}

/*
 * func: ������Ʒ����, ����newPrice[][], ��IDLE״ִ̬�б��
 */
static int DoPrice()
{
	__u8 pr_no;
	
	pr_no  = pump.cmd[2] - 0x41; //��Ʒ��ַ����Ʒ���0,1,...7.
	RunLog("��� %02x %02x %d", pump.cmd[8], pump.cmd[9], pr_no);
	/*if ((0x04 != pump.cmd[8]) || (0x00 != pump.cmd[9]) || pr_no > 7) {
		RunLog("�����Ϣ����,�����·�");
		return -1;
	}*/
	
	if ((pump.newPrice[pr_no][2] | pump.newPrice[pr_no][3]) == 0) {
		// �ϴ�ִ�б��(ChangePrice)���һ���յ�WriteFM�����ı������
		pump.newPrice[pr_no][0] = 1;
	} else {
		pump.newPrice[pr_no][0]++;
	}

	// �����Ƿ��������յ�ͬ����Ʒ�ı������, Ҳ���ܵ����Ƿ�һ��, ��ʹ���µ��۸����ϵ���
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
 * func: ִ�б��, ʧ�ܸı�FP״̬ΪINOPERATIVE, ��д�������ݿ�
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
 * �����µ�������newPrice[FP_PR_NB_CNT], �������Ʒ��Ҫ���--����ֵ��Ϊ��,
 * ���������FP, �����Ʒƥ����·��������
 */

	for (pr_no = 0; pr_no < FP_PR_NB_CNT; pr_no++) {
		if (pump.newPrice[pr_no][2] != 0x00 || pump.newPrice[pr_no][3] != 0x00 || pump.newPrice[pr_no][1] != 0x00) {
			for (i = 0; i < pump.panelCnt; i++) {
				err_code = 0x99;
				for (j = 0; j < MAX_FP_LN_CNT; j++) { // ������gunCnt,1ǹ���ܲ���
#if DEBUG
					RunLog(">>>2 lnz: %d, fp_ln_data[%d][%d].PR_Id: %d", j + 1, i, j,\
							lpDispenser_data->fp_ln_data[i][j].PR_Id);
#endif
					if (lpDispenser_data->fp_ln_data[i][j].PR_Id - 1 == pr_no) {

					err_code_tmp = 0x99;
					for (trycnt = 0; trycnt < TRY_TIME; trycnt++) {
						// Step.1 ���������
						
						//----------------------------------
						//�շ�����ǰ�����TCP����ͨ��
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
							RunLog("��������Node[%d].FP[%d]ʧ��", node, i + 1);
							continue;
						}

						// Step.2 �����������Ϣ�����ض��ͻ�����ȷ�ϱ���Ƿ�ɹ�
						bzero(pump.cmd, pump.wantLen);
						pump.cmdlen = pump.wantLen;
						ret = __TcpRecvDataInTime(pump.fd, pump.cmd, &(pump.cmdlen), TCP_TIMEOUT);
						if (ret < 0) {
							RunLog("��������Node[%d].FP[%d]�ɹ�, ȡ������Ϣʧ��", node, i + 1);
							//trycnt = TRY_TIME;  // Note. ����ֱ�ӷ���, �����㵥��	
							continue;//break;
						}
						
						if ( (pump.cmd[0] != 0x77)||(pump.cmd[3] != 0x00) ) {
							RunLog("���͵��۴���");
							trycnt = TRY_TIME;  // Note.
							RunLog("ȡ��۷���:[%s]", Asc2Hex(pump.cmd, 42));
							err_code_tmp = 0x37;
							break;
						}
						
						// Step.3 д�µ��۵���Ʒ���ݿ� & ���㵥�ۻ���
#if DEBUG
						RunLog("���ͱ������ɹ�,�µ���fp_fm_data[%d][0].Prode_Price: %s", pr_no,
							Asc2Hex(lpDispenser_data->fp_fm_data[pr_no][lpDispenser_data->fp_data[i].Default_Fuelling_Mode - 1].Prod_Price, 4));
#endif
						err_code_tmp = 0x99;
						break;
					}

					// Step.4 �����ж�, ��������д�������ݿ�/��FP״̬Ϊ Inoperative
					if (trycnt >= TRY_TIME) {
						// Write Error Code: 0x36/0x37 - Change price Error (Spare)
						if (0x37 != err_code_tmp) {
							err_code_tmp = 0x36;
						}
					}

					if (err_code == 0x99) {
						// �������κ���ǹ���ʧ��, ��������FP���ʧ��.
						err_code = err_code_tmp;
					}

					} // end of if.PR_Id match
				//	break;  // ������ͻ��ܹ�����ָ����Ϣ�����ǹͬ��Ʒ���������������Ҫbreak.
				} // end of for.j < LN_CNT

				RunLog("---------- Node[%d]FP[%d] ER_DATA, error code: %02x -----", node, i + 1, err_code);

				int tmp_idx;
				for (tmp_idx = 0; tmp_idx < pump.newPrice[pr_no][0]; tmp_idx++) {
					ChangeER_DATA(node, 0x21 + i, err_code, NULL); // Translater�Ѿ��ظ���ACK
				}

				if (err_code != 0x99) {
					ChangeFP_State(node, 0x21 + i, FP_State_Inoperative, NULL);
				}

				// ��֤ 2x ����ļ���ﵽPRICE_CHANGE_RETRY_INTERVAL(2s) (������ǰ 1s + ���� 1s)
				Ssleep(PRICE_CHANGE_RETRY_INTERVAL - 1000000);
			} // end of for.i < panelCnt
			bzero(&pump.newPrice[pr_no], 4);
		} // end of if.newPrice
	} // end of for.pr_no

	return 0;
}


/*
 * func: ���ͽ��������н��׼�¼�Ķ�ȡ�ʹ���
 *       �㽻�׵��жϺʹ�����DoEot����, DoPoling ������
 * ret : 0 - �ɹ�; -1 - ʧ��
 *
 */
/*
 * �޸Ľ���&ʵ��˵��:  ��Ҫ���ͻ���ȡ���������� ���μ����� & ���ͽ�� &
 *                     ���� & �������� [& �������] , �ֱ������Ӧ�����ݿ���
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
		k = fpUpFlag[pos]-1;//ǹλ��
		RunLog("Get node %d fp %d gun %d trasaction ", node, pos+1, fpUpFlag[pos]);
              pump.gunLog[pos] = fpUpFlag[pos];
	}
	ret = GetTransData(pos,k);	// ȡ���ν��׼�¼	
	if (ret != 0) {
		return -1;
	}
	
	k = pump.cmd[2];//��ȡ��ǹ��
	pump.gunLog[pos] =  pump.cmd[2];//��ȡ��ǹ��
	memcpy(total,&pump.cmd[19],7);
	

	pr_no = (lpDispenser_data->fp_ln_data[pos][k- 1]).PR_Id - 1;  //pump.gunLog[pos]  0 - 7	
	if ((pr_no < 0) || (pr_no > 7)) {
		RunLog("DoEot PR_Id error, PR_Id[%d]", pr_no + 1);
		return -1;
	}
	memcpy(pre_total, (lpDispenser_data->fp_ln_data[pos][k- 1]).Log_Noz_Vol_Total, 7);//pump.gunLog[pos] 

	if (memcmp(total, pre_total, 7) != 0) {
//		RunLog("�����б仯,ǰ[%s]:��[%s] ",Asc2Hex(pre_total, 7),Asc2Hex(total, 7));

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
			RunLog("DoEot.EndTr4Ln ʧ��, ret: %d", ret);
		}

		ret = EndTr4Fp(node, pos + 0x21, pre_total, 
			(lpDispenser_data->fp_ln_data[pos][k - 1]).Log_Noz_Vol_Total);//pump.gunLog[pos]
		if (ret < 0) {
			RunLog("DoEot.EndTr4Fp ʧ��, ret: %d", ret);
		}

	} else {
		RunLog("����û�б仯 ,ǰ����[%s],�ֱ���[%s]",Asc2Hex(pre_total, 7), Asc2Hex(total, 7));
		//memcpy((lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Vol_Total, total, 7);
		ret = EndZeroTr4Fp(node, pos + 0x21);
		if (ret < 0) {
			RunLog("DoEot.EndZeroTr4Fp ʧ��, ret: %d", ret);			
		}
	}
	RunLog("node %d fp %d gun %d transaction has been read. ", node, pos+1, fpUpFlag[pos]);
	fpUpFlag[pos] = 0;//��ǹȡ�������
	return 0;
}


/*
 * func: ��ȡ��FP����ǹ�ı���
 */
static int GetPumpTotal(int pos,int gunPos)
{
	int ret;
	int trycnt;

	for (trycnt = 0; trycnt <TRY_TIME ; trycnt++) {
		
		//----------------------------------
		//�շ�����ǰ�����TCP����ͨ��
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
			RunLog("�� ȡ�������� ��Node[%d].FP[%d]ʧ��", node, pos + 1);
			continue;
		}

		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = __TcpRecvDataInTime(pump.fd, pump.cmd, &(pump.cmdlen), TCP_TIMEOUT);
		if (ret < 0) {
			RunLog("�� ȡ�������� ��Node[%d].FP[%d]�ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		}
		RunLog("ȡ���뷵��:[%s]", Asc2Hex(pump.cmd, 18));
		if ( (pump.cmd[0] != 0x44)||(pump.cmd[3] != 0x0E) ) {
			RunLog("���͵�ȡ�������");
			continue;
		}

		return 0;
	}

	return -1;
}

/*
 * func: ��ȡ���ν��׼�¼
 * return: 0 - �ɹ�, 1 - �ɹ����޽���, -1 - ʧ��
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
		//�շ�����ǰ�����TCP����ͨ��
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
			RunLog("���������������Node[%d].FP[%d]ʧ��", node, pos + 1);
			continue;
		}

		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = __TcpRecvDataInTime(pump.fd, pump.cmd, &(pump.cmdlen), TCP_TIMEOUT);
		if (ret < 0) {
			RunLog("ȡ�������ݷ���:[%s]", Asc2Hex(pump.cmd, 31));
			RunLog("���������������Node[%d].FP[%d]�ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		}
		RunLog("ȡ�������ݷ���:[%s]", Asc2Hex(pump.cmd, 33));
		if ( (pump.cmd[0] != 0xAA)||(pump.cmd[3] != 0x1D) ) {
			RunLog("���͵�ȡ�������ݴ���");
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
/*����
 * func: ��ͣ���ͣ��ڴ���Terminate_FP��
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
				RunLog("�� ��ֹ�������� ��Node[%d].FP[%d]ʧ��", node, pos + 1);
				continue;
			}

			bzero(pump.cmd, pump.wantLen);
			pump.cmdlen = pump.wantLen;
			ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
			if (ret < 0) {
				RunLog("�� ��ֹ�������� ��Node[%d].FP[%d]�ɹ�, ȡ������Ϣʧ��", node, pos + 1);
				continue;
			}

			if (pump.cmd[2] != pump.panelPhy[pos]) {
				RunLog("���͵�Ŵ���");
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
			//�շ�����ǰ�����TCP����ͨ��
			pump.cmdlen = ERR_LENGTH;
			RunLog("Send Authorised Cmd %s", Asc2Hex(pump.cmd, pump.cmdlen));
			ret = __TcpRecvDataInTime(pump.fd, pump.cmd, &(pump.cmdlen), ERR_TCP_TIMEOUT);
			//-------------------------------------
			
			pump.cmd[0] = 0xFF;
			pump.cmd[1] = pos + 1;
			pump.cmd[2] = 0x00;//�����Ȩ
			pump.cmd[3] = 0x00;
			pump.cmdlen = 4;
			pump.wantLen = 4;

			Ssleep(INTERVAL);
			ret = __TcpSendDataInTime(pump.fd, pump.cmd, pump.cmdlen, TCP_TIMEOUT);
			if (ret < 0) {
				RunLog("����Ȩ���Node[%d].FP[%d]ʧ��", node, i + 1);
				continue;
			}

			bzero(pump.cmd, pump.wantLen);
			pump.cmdlen = pump.wantLen;
			ret = __TcpRecvDataInTime(pump.fd, pump.cmd, &(pump.cmdlen), TCP_TIMEOUT);
			if (ret < 0) {
				RunLog("����Ȩ���Node[%d].FP[%d]�ɹ�, ȡ������Ϣʧ��", node, i + 1);
				continue;
			}
			RunLog("ȡ��Ȩ����:[%s]", Asc2Hex(pump.cmd, 4));
			if ( (pump.cmd[0] != 0xFF)||(pump.cmd[3] != 0) ) {
				RunLog("���͵�ȡ��Ȩ�������");
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
 * func: ��ȡ���ϴ�ʵʱ��������
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
		//�շ�����ǰ�����TCP����ͨ��
		pump.cmdlen = ERR_LENGTH;
		ret = __TcpRecvDataInTime(pump.fd, pump.cmd, &(pump.cmdlen), ERR_TCP_TIMEOUT);
		//-------------------------------------
		
		// Step.1 ��ȡʵʱ����
		ret = GetCurrentLNZ(pos);
		if (ret < 0) {
			RunLog("��ȡ��ǰ��ǹǹ��ʧ��");
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
			RunLog("�� ȡʵʱ�������� ���Node[%d].FP[%d]ʧ��", node, pos + 1);
			continue;
		}

		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = __TcpRecvDataInTime(pump.fd, pump.cmd, &(pump.cmdlen), TCP_TIMEOUT);
		if (ret < 0) {
			RunLog("�� ȡʵʱ�������� ���Node[%d].FP[%d]�ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		}
		RunLog("ȡʵʱ�������ݷ���:[%s]", Asc2Hex(pump.cmd, 18));
		if ( (pump.cmd[0] != 0x33)||(pump.cmd[3] != 0x0E) ) {
				RunLog("���͵�ȡʵʱ�������ݴ���");
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
 * func: ̧ǹ���� 
 * ��ȡ��ǰ̧����߼�ǹ��,����fp_data[].Log_Noz_State & �ı�FP״̬ΪCalling
 */
static int DoCall(int pos)
{
	int ret;

	ret = GetCurrentLNZ(pos);
	if (ret < 0) {
		RunLog("��ȡ��ǰ��ǹǹ��ʧ��");
		return -1;
	}

	ret = ChangeFP_State(node, pos + 0x21, FP_State_Calling, NULL);

	//��λ������,���������Զ���Ȩ;��λ��������, ��Ȩ�������ڿ���״̬,��ʱ�Զ���Ȩ
	if ((gpIfsf_shm->auth_flag == FCC_SWITCH_AUTH) && (lock_status() == 1)) {
		ret = DoAuth(pos);
		if (ret < 0) {
			RunLog("�Զ���Ȩʧ��");
		}
		ret = ChangeFP_State(node, pos + 0x21, FP_State_Started, NULL);
		RunLog("�Զ���Ȩ�ɹ�");
		SO_OFFLINE_TR(node, pos);
	}

	return ret;
}

/*
 * func: ��ȡ�ͻ����� �ܶ� ʱ��
 * 	prince:pump.cmd[4]pump.cmd[5] (BCD)
 */

static int GetPrice(int pos,int gunPos)
{
	int ret;
	int trycnt;

	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {
		
		//----------------------------------
		//�շ�����ǰ�����TCP����ͨ��
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
			RunLog("����ȡ�ͻ��������Node[%d].FP[%d]ʧ��", node, pos + 1);
			continue;
		}

		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = __TcpRecvDataInTime(pump.fd, pump.cmd, &(pump.cmdlen), TCP_TIMEOUT);
		if (ret < 0) {
			RunLog("ȡ������Ϣʧ��");
			continue;
		} 
		RunLog("ȡ�ͻ����۷���:[%s]", Asc2Hex(pump.cmd, 8));
		if ( (pump.cmd[0] != 0x66)||(pump.cmd[3] != 0x04) ) {
				RunLog("���͵�ȡ���۴���");
				continue;
		}

		return 0;
	}

	return -1;
}

/*
 * func: ��ȡʵʱ״̬
 */

static int GetPumpStatus(int pos, __u8 *status)
{
	int trycnt;
	int ret;

	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {
		
		//----------------------------------
		//�շ�����ǰ�����TCP����ͨ��
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
			RunLog("��ȡ״̬���Node[%d].FP[%d]ʧ��", node, pos + 1);
			continue;
		}
		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = __TcpRecvDataInTime(pump.fd, pump.cmd, &(pump.cmdlen), TCP_TIMEOUT);
		if (ret < 0) {
			RunLog("��ȡ״̬���Node[%d].FP[%d]�ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		} 
		RunLog("Node [%d], ȡ״̬����:[%s]",node, Asc2Hex(pump.cmd, 5));
			
		if ( (pump.cmd[0] != 0x55)||(pump.cmd[3] != 0x01) ) {
				RunLog("���͵�ȡ״̬����");
				continue;
		}

		*status = pump.cmd[4];

		return 0;
	}

	return -1;
}

//test wether the dispenser is online, we only use fp 1 to test.
/*
 * func: �ж�Dispenser�Ƿ�����
 */

static int GetPumpOnline(int pos, __u8 *status)
{
	int ret;
	int trycnt;

	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {         // Note. GetPumpStatus ѭ����������ñ��ص�,��Ҫ��trycnt
		
		//----------------------------------
		//�շ�����ǰ�����TCP����ͨ��
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
			RunLog("��ȡ״̬���Node[%d].FP[%d]ʧ��", node, pos + 1);
			continue;
		}

		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = __TcpRecvDataInTime(pump.fd, pump.cmd, &(pump.cmdlen), TCP_TIMEOUT);
		if (ret < 0) {
			RunLog("��ȡ״̬���Node[%d].FP[%d]�ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		} 
		RunLog("ȡ����״̬����:[%s]", Asc2Hex(pump.cmd, 5));
		if ( (pump.cmd[0] != 0x55)||(pump.cmd[3] != 0x01) ) {
				RunLog("���͵�ȡ״̬����");
				continue;
		}

		*status = pump.cmd[4];

		return 0;
	}

	return -1;
}

/*
 * func: ��ȡ��ǰ̧����ǹ��ǹ��, ���д��pump.gunLog[] & fp_data[].Log_Noz_State
 * ret : 0 - �ɹ�, -1 - ʧ�� 
 */
static inline int GetCurrentLNZ(int pos)
{
	RunLog("Node %d FP %d �� %d ��ņ̀��", node, pos + 1, pump.gunLog[pos]);
	lpDispenser_data->fp_data[pos].Log_Noz_State = 1 << (pump.gunLog[pos] - 1);

	return 0;
}

/*
 * func: ��ʼ��pump.gunLog, Ԥ�ü���ָ����Ҫ; ���޸� Log_Noz_State
 */
static int InitGunLog()
{
	int i;

	for (i = 0; i < pump.panelCnt; ++i) {
		pump.gunLog[i] = 1;//��ǰ��ǹ
	}

	return 0;
}

/*
 * ��ʼ��FP�ĵ�ǰ�ͼ�. �������ļ��ж�ȡ��������,
 * д�� fp_data[].Current_Unit_Price
 * ���� PR_Id �� PR_Nb ��һһ��Ӧ��
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
 * func: ��ʼ����ǹ����
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
				RunLog("ȡ FP%d �ı���ʧ��", i);
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
			RunLog("�߼�ǹ��[%d]�ı���Ϊ[%s]", 
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

/*�ޣ�����
 * func: ���ÿ���ģʽ
 * incomplete
 *
 */

static int SetCtrlMode(int pos, __u8 mode)
{
	int trycnt;
	int ret;

	for (trycnt = 0; trycnt < TRY_TIME; ++trycnt) {         // Note. GetPumpStatus ѭ����������ñ��ص�,��Ҫ��trycnt
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
			RunLog("�����ÿ���ģʽ���Node[%d].FP[%d]ʧ��", node, pos + 1);
			continue;
		}

		bzero(pump.cmd, pump.wantLen);
		pump.cmdlen = pump.wantLen;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("�����ÿ���ģʽ���Node[%d].FP[%d]�ɹ�, ȡ������Ϣʧ��", node, pos + 1);
			continue;
		} 

		if (pump.cmd[2] != pump.panelPhy[pos]) {
			RunLog("���͵�Ŵ���");
			continue;
		}

		if (pump.cmd[pump.wantLen - 1] != Check_xor(&pump.cmd[1], pump.wantLen - 2)) {
			RunLog("Return Message XOR ERROR");
                       continue;
               }

               return 0;
       }

       RunLog("�ı�ģʽ node %d pos %d", node, pos);
       return -1;
}


/* ----------------- End of lischn016.c ------------------- */



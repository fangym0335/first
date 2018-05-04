/********************************************************************************
 *		Name:		update_main.c			                                                   			*
 *		Purpose:		Maintenance the FCC		                                                     		*
 *		Author:    	Lei Li							                                                    	*			
 *		Date written:   09-11-16					                                                          *
 ********************************************************************************/

 /* history:
 // 09/11/17  by lil  @v1.02.02
 // sysinfo模块增加返回ifsf_main运行中版本号. 增加返回update_main版本号
 // 所有system命令添加绝对路径。
 // 3分钟没收到信息断开通讯，改为5分钟没有收到信息断开通讯
 //
 //// 09/11/24  by lil  @v1.02.03
 //sysinfo模块返回的版本号是用函数截取并计算得出数据长度
 
 // ---------------------The end --------------------------- */





#include <stdio.h>
#include <stdlib.h>
#include <sys/timeb.h>
#include <time.h>
#include <mydebug.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include "tcp.h"
#include "md5lib.h"
#include <string.h>
#include <errno.h>

#define UP_VERSION "v1.02.02 - 2009.11.24"
#define UP_MAX_VARIABLE_PARA_LEN  1024
#define UP_MAX_FILE_NAME_LEN  40
#define Up_RunLog SetFileAndLine( __FILE__, __LINE__ ), __Up_RunLog

static ssize_t TcpRecvDataInTime( int fd, char *s, size_t *buflen,  int timeout ); //size_t headlen,
static ssize_t TcpSendDataInTime( int fd, const char *s, size_t len,  int timeout );
static unsigned char* Asc2Hex(const unsigned char *cmd, int cmdlen );
static void __Up_RunLog( const char *fmt, ... );
static int Up_TcpRecvServ();
static int readfile(char *fileName,char *buff);
static char *TrimAll( char *buf );
static int update_auto(const int newsock, const unsigned char *recvBuffer);
static int IsTheFileExist(const char * sFileName );
static int cppycfg(const int newsock, const unsigned char *recvBuffer);
static int killall(const int newsock, const unsigned char *recvBuffer);
static int sysinfo(const int newsock, const unsigned char *recvBuffer);
static int update_main_ver(const int newsock, const unsigned char *recvBuffer);
static char *split_part2(char *s);


static unsigned char Up_run_log[48] = "/home/Data/log_backup/update.log";
static unsigned char update_tar_name[50]="/home/Data/log_backup/update_tar_ver.log";
static unsigned char update_tar_way[50] = "/home/ftp/";
static unsigned char update_tar_ver[70] = {0};
static unsigned char sysinfo_way[20]="/home/ftp/sysinfo";

int main( int argc, char *argv[] )
{
	int ret;
	if( argc == 2 && strcmp( argv[1], "-v" ) == 0 ) {
		printf( "update_main.  version %s\n",UP_VERSION);
		return 0;
	}
	Up_RunLog("update_main (%s ) Startup ................ ",UP_VERSION);

	ret=Up_TcpRecvServ(); 
	if( ret < 0 ) {
		Up_RunLog( "启动进程失败.请检查日志" );
	}
	return 0;

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
	size_t headlen =2;
	char	head[3];
	char	buf[TCP_MAX_SIZE];
	char lg[6];
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
	if ( n == -1 ) {
		return -1;
	} else if ( n >= 0 && n < headlen ) {
		if ( GetLibErrno() == ETIME )	/*超时*/
			return -2;
		else
			return -1;
	}

	//if ( !IsDigital(head) )
	//	return -3;	/*非法的包长度*/

	//len = atoi(head);
	memset( lg, 0, sizeof(lg) );
	memcpy( lg, head, 2);
	len =atoi(lg);
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
		if ( *buflen >= (len + 2)   ) {
			memcpy( s, head, 2 );
			memcpy( s+2, t, len );			
			s[len+2] = 0;
			len = len+2;
			memcpy(buflen , &len,sizeof(size_t));
			ret = 0;
		} else {
			memcpy( s, head, 2 );
			memcpy( s+2, t, *buflen-2 );
			s[*buflen] = 0;
			ret = 1;
		}
	}
	if ( mflag == 1 ) {
		Free( t );
	}
	return ret;
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

static int Up_TcpRecvServ()
{
       char port[6]="16800";
	int sock, newsock, tmp_sock;
	int ret, i,len;
	int msg_lg =256;
	int timeout = 5;       // 5s
	int errcnt = 0;
	int timeout_cnt = 0;
	unsigned char buffer[265];
	extern int errno;

	Up_RunLog("Tcp Receive Server <--->");
	sock = TcpServInit(port, 1);
	if ( sock < 0 ) {
		Up_RunLog( "初始化socket失败.port=[%s]\n", port );
		close(sock);
		return -1;
	}

//	fcntl(sock, F_SETFL, O_NONBLOCK);   // 非阻塞模式
	
	newsock  = -1;
	tmp_sock = -1;


	while (1) {	

		if (newsock <= 0) {             // 若连接不存在, 则监听
			Up_RunLog("Listening .....................");
			newsock = TcpAccept(sock, NULL);
			if (newsock == -1) {
				continue;
			}
			Up_RunLog("created a new connection .....................");
			timeout_cnt = 0;
		} else if(timeout_cnt >= (300 / timeout)) {  // 若5分钟没有收到数据则关闭连接
			Up_RunLog("Timeout, close the connection .....................");
			close(newsock);
			newsock = -1;
			continue;
		}


		msg_lg = sizeof(buffer);
		memset( buffer, 0, sizeof(buffer) );
		ret = TcpRecvDataInTime(newsock,buffer,&msg_lg,timeout);
		if (ret < 0) {
				if (errno == 4) {  // time out
		//		Up_RunLog("超时,接收到后台命令 buffer[%s]\n",  Asc2Hex(buffer, 32));
				timeout_cnt++;
				continue;
			} 
			errcnt++;
			close(newsock);
			newsock  = -1;
			continue;
		} else{
			timeout_cnt = 0;
			Up_RunLog( "<<<收到命令: [%s(%d)]", Asc2Hex(buffer, msg_lg ), msg_lg);
			if (sizeof(buffer) == msg_lg){ //received data must be error!!
				Up_RunLog( "received data error" );
				errcnt++;
				continue;
			}

			if (0 == msg_lg){ //receivd data must be error!!
				Up_RunLog( "received size is  0 error" );	
				errcnt++;
				continue;
			}	
			//处理客户端命令命令.
			if (0x31 == buffer[3]){     // update
				Up_RunLog("start update ........");
				ret=update_auto(newsock,buffer);
				if (ret <0 ){
					errcnt++;
					continue;
				}
			} else if (0x32 == buffer[3]){     // backup all cfg
				Up_RunLog("start copy cfg ........");
				ret=cppycfg(newsock,buffer);
				if (ret <0 ){
					errcnt++;
					continue;
				}
			}else if (0x33 == buffer[3]){     // killall ifsf_main
				Up_RunLog("start killall ifsf_main ........");
				ret=killall(newsock,buffer);
				if (ret <0 ){
					errcnt++;
					continue;
				}
			}else if (0x34 == buffer[3]){     // sysinfo
				Up_RunLog("send sysinfo.......");
				ret=sysinfo(newsock,buffer);
				if (ret <0 ){
					errcnt++;
					continue;
				}
			}else if (0x35 == buffer[3]){     // update_main version
				Up_RunLog("send update_main version.......");
				ret=update_main_ver(newsock,buffer);
				if (ret <0 ){
					errcnt++;
					continue;
				}
			}else{
				Up_RunLog( "解析数据失败,不是1或2." );
			}

			errcnt = 0;
		}

	}

	Up_RunLog("Tcp Server Stop ......");
	close(sock);

	return -1;
} 

static unsigned char* Asc2Hex(const unsigned char *cmd, int cmdlen )
{
	static char retcmd[100];
	int	i;

	memset( retcmd, 0, sizeof(retcmd) );
	for( i = 0; i < cmdlen; i++ ) {
		sprintf(retcmd + i * 2, "%02x", cmd[i]);
	}

	return retcmd;
}

/*
*   写升级日志到/home/Data/log_backup/update.log
*/
static void __Up_RunLog( const char *fmt, ... )
{
	va_list argptr;
	static char sMessage[UP_MAX_VARIABLE_PARA_LEN + 1];
	char fileName[UP_MAX_FILE_NAME_LEN + 1];	/*记录调试信息的文件名*/
	int line;				/*记录调试信息的行*/
	int fd;
	unsigned char date_time[7] = {0};


	// New Time Stamp, modify by jie @ 2008-11-20
	struct timeb tp;
	struct tm *tm;

	ftime(&tp);
	tm = localtime(&(tp.time));

	sprintf(sMessage, "%02d-%02d %02d:%02d:%02d.%03d ", (tm->tm_mon + 1), (tm->tm_mday),
			(tm->tm_hour), (tm->tm_min), (tm->tm_sec), (tp.millitm));


	GetFileAndLine(fileName, &line );
	sprintf(sMessage + strlen(sMessage), "%s:%d ", fileName, line);


	sMessage[UP_MAX_FILE_NAME_LEN] = '\0';
	memset(sMessage + strlen(sMessage), ' ', UP_MAX_FILE_NAME_LEN - strlen(sMessage));

	sprintf(sMessage + strlen(sMessage), "[%-4d]  ", getpid());


	va_start(argptr, fmt);
	vsprintf(sMessage + strlen(sMessage), fmt, argptr);
	va_end(argptr);

	printf(" %s\n", sMessage);
	if ((fd = open(Up_run_log, O_RDWR | O_CREAT | O_APPEND, 00644)) < 0) {
		printf("Open log file run.log failed\n");
		return;
	}

	sprintf(sMessage + strlen(sMessage), "\n");
	write(fd, sMessage, strlen(sMessage));

	close(fd);

	return;
}

/*
	  0	:	成功
	-1	:	失败
	-2	:	超时
*/
static ssize_t TcpSendDataInTime( int fd, const char *s, size_t len,  int timeout )
{
	ssize_t		retlen;
	char		*head;
	char		*t;
	char		buf[TCP_MAX_SIZE];
	size_t		sendlen;
	int			mflag; 	/*标识是否分配了空间 0未分配. 1已分配*/
	int			n;
	int		 i;
	Assert( s != NULL && *s != 0 );
	Assert( len != 0  );
	Assert( timeout > 0 || timeout == TIME_INFINITE );

	mflag = 0;
	sendlen = len;

	if ( sizeof(buf) >= sendlen )	/*预先分配的变量buf的空间足够容纳所有要发送的数据*/
		t = buf;
	else							/*预先分配的变量buf的空间不足容纳所有要发送的数据,故分配空间*/
	{
		t = (char*)Malloc(sendlen);
		if ( t == NULL )
		{
			RptLibErr( errno );
			return -1;
		}
		mflag = 1;
	}

	//sprintf( t, "%0*d", headlen, len );
	memcpy( t, s, len );
	
	n = WriteByLengthInTime( fd, t, sendlen, timeout );

	if ( mflag == 1 )
		Free(t);

	if ( n == sendlen )	/*数据全部发送*/
		return 0;
	else if ( n == -1 ) /*出错*/
		return -1;
	else if ( n >= 0 && n < sendlen )
	{
		if ( GetLibErrno() == ETIME ) /*超时*/
			return -2;
		else
			return -1;
	}

}

/* 
*   截取第二项数据
*
*/
static char *split_part2(char *s){
	char *data;
	char *tmp;
	int i, len, j, tag =1,g = 0;

	len = strlen(s);
	tmp = s;
	data = (char *)malloc( sizeof(char) * (len + 1));
	if(NULL == data ){
		return NULL;
	}
	//去掉字符串最前面的空格
	for(i =0; i < len; i ++){
		if (s[i] != ' '){
			
		}
		else {
			i ++;
			break;
		}
	}

	for(j = i; j < len; j++) 
		s[j - i] = tmp[j];
	s[len -i] = '\0';

	//截取字符串中第二项数据
	for(i =0; i < len; i ++){
		if ((s[i] == ' ' ||s[i] == '\t') && tag == 1){
			continue;
		}else if ((s[i] != ' ') && (s[i] != '\t')  && (s[i] != '\n')){
			tag = 0;
			data[g++] = s[i] ;
		}
		else if( s[i] == ' ' ||s[i] == '\t' || s[i] == '\n' ){
		//	g++;
			break;
		}
	}
	data[g] = '\0';
	
	return data;
}

//-
//读取升级包的名字，用与md5校验。
//-
static int readfile(char *fileName,char *buff){

	//char *method;
	char  my_data[1024];
	char *tmp_ptr, *tmp;
	int data_len;
	int i,ret;
	int c;
	FILE *f;
	
	if(  NULL== fileName ){
		return -1;
	}
	ret = IsTheFileExist(fileName);
	if(ret != 1){
		char pbuff[256];
		bzero(pbuff,sizeof(pbuff));
		sprintf( pbuff, "file[%s] not exist in readIni  error!! ", fileName );
		Up_RunLog(pbuff);
		exit(1);
		return -2;
	}
	f = fopen( fileName, "rt" );
	if( !f  ){	
		char pbuff[256];
		bzero(pbuff,sizeof(pbuff));
		sprintf( pbuff, "file[%s] open error in readIni!! ", fileName );
		Up_RunLog(pbuff);
		exit(1);
		return -3;
	}	
	bzero(my_data, sizeof(my_data));
	for(i=0;i<sizeof(my_data)-1;i++){
		c = fgetc( f);
		if( c == EOF)
			break;
		my_data[i] = c;
	}
	my_data[i] = 0;	
	fclose( f );
	
	memcpy( buff, my_data ,i);
	return 0;	
}


static int IsTheFileExist(const char * sFileName ){
	int ret;
	extern int errno;
	if( sFileName == NULL || *sFileName == 0 )
		return -1;
	ret = access( sFileName, F_OK );
	//cgi_ok("in  IsTheFileExist after call stat");
	//exit( 0 );
	if( ret < 0 )
	{
		if( errno == ENOENT )
		{
			return 0;//not exist
		}
		else
		{			
			return -1;
		}
		
	}
	return 1;
}


//-
//去除字符串两侧空格及回车换行符,需要字符串带结束符
//-
static char *TrimAll( char *buf )
{
	int i,firstchar,endpos,firstpos;

	if(  NULL  ==buf){
		return NULL;
		};

	endpos = firstchar = firstpos = 0;
	for( i = 0; buf[ i ] != '\0'; i++ )
	{
		if( buf[i] ==' ' || buf[i] == '\t' || buf[i] == '\n' || buf[i] == '\r')
		{
			if( firstchar == 0 )
				firstpos++;
		}
		else
		{
			endpos = i;
			firstchar = 1;
		}
	}

	for( i = firstpos; i <= endpos; i++ )
		buf[i - firstpos] = buf[i];
	buf[i - firstpos] = 0;
	return buf;
}

/*
*	return :
*	  0:	升级成功
*	-1:	收到参数错误
*	-2:	/home/ftp/ 没有升级包
*	-3:	打开/home/Data/log_backup/update_tar_ver.log 失败
*	-4:	升级成功     返回ACK失败
*	-5:	升级失败     
*	-6:	MD5校验失败
*/

static int update_auto(const int newsock, const unsigned char *recvBuffer)
{
	int ret;
	int msg_ll = 10;
	int timeout =5;
	 char *md5nu;
	unsigned char ack_msg[10]="06010200";//成功
	unsigned char ack_fail[10]="06010210";//执行升级包失败
	unsigned char ack_md5[10]="06010211";//md5校验失败
	unsigned char ack_null[10]="06010212";// /home/ftp/没有升级包
	
	if((NULL == recvBuffer) || (0 == newsock)) {		
		Up_RunLog("参数错误,update_auto  失败");
		return -1;//参数错误
	}
	ret=system("/bin/ls /home/ftp/*.tar.gz>/home/Data/log_backup/update_tar_ver.log");
	if(ret){
			Up_RunLog("目录[/home/ftp/]  里无升级包!");
			ret=TcpSendDataInTime(newsock,ack_null,msg_ll,timeout);
			if(ret==0){
				Up_RunLog(">>>发送ACK [%s]",ack_null);
				return -2; ///home/ftp/ 没有升级包
			}
	}
	sleep(1);
	ret=readfile(update_tar_name,update_tar_ver);
	if(ret<0){
	 	ret=TcpSendDataInTime(newsock,ack_fail,msg_ll,timeout);
		Up_RunLog("file[%s] open error",update_tar_name);
		return -3;   //打开/home/Data/log_backup/update_tar_ver.log 失败
	}
	md5nu = MDFile(TrimAll(update_tar_ver));
	Up_RunLog("md5nu [%s] ",md5nu);
	Up_RunLog("<<<recvBuffer [%s] ",recvBuffer);
	//比较 MD5码
	if(memcmp( &md5nu[0], &recvBuffer[12],32)==0){
		Up_RunLog("MD5  校验成功!");
		ret=system("/bin/sh  /home/App/ifsf/data/cgi-bin/localupdate1");
              if(ret==0){
			Up_RunLog("升级成功!");
			ret=TcpSendDataInTime(newsock,ack_msg,msg_ll,timeout);
			if(ret==0){
				Up_RunLog("send Update ACK [%s]",ack_msg);
				return 0; //升级成功，返回ACK成功
			}else{
			       ret=TcpSendDataInTime(newsock,ack_fail,msg_ll,timeout);
				Up_RunLog("send Update ACK failure!");
				return -4;//升级成功，返回ACK失败
			}
		}else{
			Up_RunLog("升级包执行失败!");
			ret=TcpSendDataInTime(newsock,ack_fail,msg_ll,timeout);
			if(ret==0){
				Up_RunLog(">>>发送ACK [%s]",ack_fail);
		         	return -5;//升级失败
			}	

		}
	}
	else{
			Up_RunLog("MD5  校验失败!");
			ret=TcpSendDataInTime(newsock,ack_md5,msg_ll,timeout);
			if(ret==0){
				Up_RunLog(">>>发送ACK [%s]",ack_md5);
				return -6;//MD5校验失败
			}	
	}
	
}


/*
*	return :
*	  0:	升级成功
*	-1:	收到参数错误
*	-2:	拷贝配置文件成功，返回ACK失败
*	-3:	拷贝配置文件失败
*/
static int cppycfg(const int newsock, const unsigned char *recvBuffer){

	int ret;
	int msg_ll = 10;
	int timeout =5;
	unsigned char ack_msg[10]="06020200";//成功
	unsigned char ack_fail[10]="06020210";//失败
	
	if((NULL == recvBuffer) || (0 == newsock)) {		
		Up_RunLog("参数错误,backup_cfg  失败");
		return -1;//参数错误
	}

	ret=system("/bin/cp -a /home/App/ifsf/etc/dsp.cfg  /home/App/ifsf/etc/oil.cfg \
		 /home/App/ifsf/etc/pub.cfg  /home/ftp/");
	if(ret==0){
			Up_RunLog("拷贝配置文件到/home/ftp/成功!");
			ret=TcpSendDataInTime(newsock,ack_msg,msg_ll,timeout);
			if(ret==0){
				Up_RunLog(">>>发送 ACK [%s]",ack_msg);
				return 0; //拷贝配置文件成功，返回ACK成功
			}else{
				ret=TcpSendDataInTime(newsock,ack_fail,msg_ll,timeout);
				Up_RunLog("send copycfg ACK failure!");
				return -2;//拷贝配置文件失败，返回ACK失败
			}
	}else{
			Up_RunLog("拷贝配置文件到/home/ftp/失败!");
			ret=TcpSendDataInTime(newsock,ack_fail,msg_ll,timeout);
			if(ret){
				Up_RunLog(">>>发送ACK [%s]",ack_fail);
				return -3;//拷贝配置文件失败
			}
	}

}

/*
*	return :
*	  0:	重启主程序成功
*	-1:	收到参数错误
*	-2:	重启主程序成功，返回ACK失败
*	-3:	重启主程序失败
*/
static int killall(const int newsock, const unsigned char *recvBuffer){

	int ret;
	int msg_ll = 10;
	int timeout =5;
	unsigned char ack_msg[10]="06030200";//成功
	unsigned char ack_fail[10]="06030210";//失败
	
	if((NULL == recvBuffer) || (0 == newsock)) {		
		Up_RunLog("参数错误,killall 失败");
		return -1;//参数错误
	}

	ret=system("/usr/bin/killall ifsf_main");
	if(ret==0){
			Up_RunLog("重启主程序成功!");
			ret=TcpSendDataInTime(newsock,ack_msg,msg_ll,timeout);
			if(ret==0){
				Up_RunLog(">>>发送 ACK [%s]",ack_msg);
				return 0; //重启主程序成功，返回ACK成功
			}else{
				ret=TcpSendDataInTime(newsock,ack_fail,msg_ll,timeout);
				Up_RunLog("send killall ACK failure!");
				return -2;//重启主程序成功，返回ACK失败
			}
	}else{
			Up_RunLog("重启主程序失败");
			ret=TcpSendDataInTime(newsock,ack_fail,msg_ll,timeout);
			if(ret){
				Up_RunLog(">>>发送ACK [%s]",ack_fail);
				return -3;//拷贝配置文件失败
			}
	}

}


//发送sysinfo脚本打印的信息
static int sysinfo(const int newsock, const unsigned char *recvBuffer){

	 unsigned char info[512] ={0};
	 unsigned char info1[512] ={0};
//	 unsigned char ptr[512] ={0};
	 int ret =0;
	 int timeout =5;
	 unsigned char ack_fail[10]="06040210";//失败
	 char *ptr =0;
	 int data_len =0, tmp_len = 0 ,tmp_lg = 0, int_h, int_l;
	 unsigned char ack_msg[256];//成功

	
	if((NULL == recvBuffer) || (0 == newsock)) {		
		Up_RunLog("参数错误,sysinfo 失败");
		ret=TcpSendDataInTime(newsock,ack_fail,sizeof(ack_fail),timeout);
		return -1;//参数错误
	}
	ret = system("/bin/rm /home/ftp/sysinfo");
	sleep(1);
	ret=system("/usr/local/bin/sysinfo >/home/ftp/sysinfo");
	if(ret){
		Up_RunLog("写系统信息到失败[%s]",sysinfo_way);
		ret=TcpSendDataInTime(newsock,ack_fail,sizeof(ack_fail),timeout);
		return -2;
	}

	
	ret=readfile(sysinfo_way,info);
	if(ret<0){
		Up_RunLog("file[%s] open error",sysinfo_way);
		ret=TcpSendDataInTime(newsock,ack_fail,sizeof(ack_fail),timeout);
		return -3;   //打开/home/ftp/sysinfo失败
	}
	


	bzero(ack_msg,sizeof(ack_msg));
	//构造系统信息返回值
	ack_msg[0] = 0x30;
	ack_msg[1] = 0x30;
	ack_msg[2] = 0x30;
	ack_msg[3] = 0x34;
	ack_msg[4] = 0x30;
	ack_msg[5] = 0x32;
	ack_msg[6] = 0x30;
	ack_msg[7] = 0x30;
	ack_msg[8] = 'I';
	ack_msg[9] = 'V';

	//ifsf_main version
	strcpy(info1,"IFSF_MAIN_RUN:");
	ptr = strstr( info, info1);
       ptr = split_part2(ptr);
	tmp_len = strlen(ptr);
	memcpy(&ack_msg[12],ptr,tmp_len);
	ack_msg[10] =0x30 |(tmp_len / 10);
	ack_msg[11] =0x30 |(tmp_len % 10);

	//ifsf_main run version
	tmp_lg = tmp_len + 12;
	ack_msg[tmp_lg] = 'I';
	tmp_lg++;
	ack_msg[tmp_lg] = 'R';
	strcpy(info1,"IFSF_MAIN_VER:");
	ptr = strstr( info, info1);
       ptr = split_part2(ptr);
	tmp_len = strlen(ptr);	
	tmp_lg++;
	ack_msg[tmp_lg] =0x30 |(tmp_len / 10);
	tmp_lg++;
	ack_msg[tmp_lg] =0x30 |(tmp_len % 10);
	tmp_lg++;
	memcpy(&ack_msg[tmp_lg],ptr,tmp_len);

	//kernel version
	tmp_lg = tmp_lg +tmp_len;
	ack_msg[tmp_lg] = 'K';
	tmp_lg++;
	ack_msg[tmp_lg] = 'V';
	strcpy(info1,"KERNEL_VER:");
	ptr = strstr( info, info1);
       ptr = split_part2(ptr);
	tmp_len = strlen(ptr);	
	tmp_lg++;
	ack_msg[tmp_lg] =0x30 |(tmp_len / 10);
	tmp_lg++;
	ack_msg[tmp_lg] =0x30 |(tmp_len % 10);
	tmp_lg++;
	memcpy(&ack_msg[tmp_lg],ptr,tmp_len);

	//rootfs version
	tmp_lg = tmp_lg +tmp_len;
	ack_msg[tmp_lg] = 'R';
	tmp_lg++;
	ack_msg[tmp_lg] = 'V';
	strcpy(info1,"ROOTFS_VER:");
	ptr = strstr( info, info1);
       ptr = split_part2(ptr);
	tmp_len = strlen(ptr);	
	tmp_lg++;
	ack_msg[tmp_lg] =0x30 |(tmp_len / 10);
	tmp_lg++;
	ack_msg[tmp_lg] =0x30 |(tmp_len % 10);
	tmp_lg++;
	memcpy(&ack_msg[tmp_lg],ptr,tmp_len);

	//busybox version
	tmp_lg = tmp_lg +tmp_len;
	ack_msg[tmp_lg] = 'B';
	tmp_lg++;
	ack_msg[tmp_lg] = 'V';
	strcpy(info1,"BUSYBOX_VER:");
	ptr = strstr( info, info1);
       ptr = split_part2(ptr);
	tmp_len = strlen(ptr);	
	tmp_lg++;
	ack_msg[tmp_lg] =0x30 |(tmp_len / 10);
	tmp_lg++;
	ack_msg[tmp_lg] =0x30 |(tmp_len % 10);
	tmp_lg++;
	memcpy(&ack_msg[tmp_lg],ptr,tmp_len);

	///dev/root 
	tmp_lg = tmp_lg +tmp_len;
	ack_msg[tmp_lg] = 'D';
	tmp_lg++;
	ack_msg[tmp_lg] = 'R';
	strcpy(info1,"/dev/root");
	ptr = strstr( info, info1);
       ptr = split_part2(ptr);
	
	tmp_len = strlen(ptr);	
	tmp_lg++;
	ack_msg[tmp_lg] =0x30 |(tmp_len / 10);
	tmp_lg++;
	ack_msg[tmp_lg] =0x30 |(tmp_len % 10);
	tmp_lg++;
	memcpy(&ack_msg[tmp_lg],ptr,tmp_len);

	///dev/mtdblock1
	tmp_lg = tmp_lg +tmp_len;
	ack_msg[tmp_lg] = 'D';
	tmp_lg++;
	ack_msg[tmp_lg] = 'M';
	strcpy(info1,"/dev/mtdblock1");
	ptr = strstr( info, info1);
       ptr = split_part2(ptr);
	tmp_len = strlen(ptr);	
	tmp_lg++;
	ack_msg[tmp_lg] =0x30 |(tmp_len / 10);
	tmp_lg++;
	ack_msg[tmp_lg] =0x30 |(tmp_len % 10);
	tmp_lg++;
	memcpy(&ack_msg[tmp_lg],ptr,tmp_len);

	//update_main's version
	tmp_lg = tmp_lg +tmp_len;
	ack_msg[tmp_lg] = 'U';
	tmp_lg++;
	ack_msg[tmp_lg] = 'P';
	tmp_len = 8;	
	tmp_lg++;
	ack_msg[tmp_lg] =0x30 |(tmp_len / 10);
	tmp_lg++;
	ack_msg[tmp_lg] =0x30 |(tmp_len % 10);
	tmp_lg++;
	memcpy(&ack_msg[tmp_lg],UP_VERSION,tmp_len);

	//把数据转化为十六进制格式的ASCII码 
	//如:数据长度为125字节,125 = 7DHex  最终ASCII长度表示 = 0x37 0x44
	tmp_lg = strlen(ack_msg) -2; //减去数据头的数据长度2字节
	if(tmp_lg > 255){
		printf("error : message(%d) is greater than 255!", tmp_lg);
		return -1;
	}
	int_h = tmp_lg /16;
	int_l  = tmp_lg % 16;
	if(int_h < 10){
		ack_msg[0] = 0x30 | (int_h & 0x0f);
	}else {
		int_h = int_h - 9;
		ack_msg[0] = 0x40 | (int_h & 0x0f);
	}

	if(int_l < 10){
		ack_msg[1] = 0x30 | (int_l & 0x0f);
	}else {
		int_l = int_l - 9;
		ack_msg[1] = 0x40 | (int_l & 0x0f);
	}
	
	
	ret=TcpSendDataInTime(newsock,ack_msg,tmp_lg + 2,timeout);
	if(ret==0){
				Up_RunLog(">>>发送 ACK [%s]",ack_msg);
				return 0; //读取系统信息成功，返回ACK成功
			}else{
				Up_RunLog("send copycfg ACK failure!");
				return -4;//读取系统信息成功，返回ACK失败
			}

	return 0;
}


//读取正在运行的update_main_version
static int update_main_ver(const int newsock, const unsigned char *recvBuffer){
	int ret;
	unsigned char ack_msg[15];//成功
	int msg_ll = 14;
	int timeout =5;
	unsigned char ack_fail[10]="06050210";//失败

	if((NULL == recvBuffer) || (0 == newsock)) {		
		Up_RunLog("参数错误,sysinfo 失败");
		ret=TcpSendDataInTime(newsock,ack_fail,msg_ll,timeout);
		return -1;//参数错误
	}

	bzero(ack_msg,sizeof(ack_msg));
	ack_msg[0] = 0x31;
	ack_msg[1] = 0x32;
	ack_msg[2] = 0x30;
	ack_msg[3] = 0x35;
	ack_msg[4] = 0x30;
	ack_msg[5] = 0x38;
	memcpy(&ack_msg[6],UP_VERSION,8);//update_main's version
	
	ret=TcpSendDataInTime(newsock,ack_msg,msg_ll,timeout);
	if(ret==0){
				Up_RunLog(">>>发送 ACK [%s]",ack_msg);
				return 0; //读取update_main版本号成功，返回ACK成功
			}else{
                             ret=TcpSendDataInTime(newsock,ack_fail,sizeof(ack_fail),timeout);
				Up_RunLog("send copycfg ACK failure!");
				return -1; //读取update_main版本号成功，返回ACK失败
			}
	return 0;
}





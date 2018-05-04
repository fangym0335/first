#include "setcfg.h"
#include "ifsf_def.h"
#include "ifsf_pub.h"
#include "tty.h"
#include "cgidecoder.h"

//#define GUN_CFG_LEN 10
//#define DEV_CFG_LEN 16


char gsFullName[50];
 
static int ReadPortNum(int portnum) 
{	
	unsigned char IO_cmd[2] = {0x1c,0x50};
	int fd;
	char ttynam[30];
	int	ret;
	char msg[16]; //,tmp[2],msg2[512];
	int timeout = 1;
	//int len;
	
	strcpy( ttynam, alltty[portnum-1] );


	fd = OpenSerial( ttynam);
	if( fd < 0 ) {
		//RunLog( "打开串口失败" );
		return -1;
	}
	ret = WriteByLengthInTime( fd, IO_cmd, 2,timeout );
	if( ret < 0 ) {
		RunLog( "Write Failed" );
		return -1;
	}

	memset( msg, 0, sizeof(msg) );
	ret = ReadByLengthInTime( fd, msg, sizeof(msg)-1, timeout );
	if( ret < 0 ) {
		RunLog( "Read Failed" );
		return -1;
	}
	if( ret == 0 ) {
		RunLog( "Read data error" );
		return -1;
	}
	RunLog( "Port[%d]: [%d] channels" , portnum, msg[0]);
	//msg[ret] = 0;	
	return msg[0];	
}

int GetPortChannel(char *portChnl){
	int i, ret;	
	int startPort;
	int portUsedCnt;

	memset(portChnl, 8, MAX_PORT_CNT);
	startPort =atoi(SERIAL_PORT_START);

	if( (startPort <=0 ) || (startPort >MAX_PORT_CNT) ){
		UserLog("value of SERIAL_PORT_START[%d] is error",startPort );
		return -1;
	}
	portUsedCnt =atoi(SERIAL_PORT_USED_COUNT);
	if( (portUsedCnt <=0 ) || (portUsedCnt >MAX_PORT_CNT) ){
		UserLog("value of SERIAL_PORT_USED_COUNT[%d] is error",portUsedCnt );
		return -1;
	}
	for(i=0; i<portUsedCnt; i++){
		ret = ReadPortNum(startPort+i);
		if(16 == ret){
			portChnl[i] = 16;
		} else if (8 != ret){
			UserLog("read port[%d] channel number is error, return number is [%d]",startPort + i, ret );
		}
	}
	return 0;
}
int GetSerialPort(const int chnLog, const char *portChnl){
	int i, ret;	
	int startPort;
	int tmp;
	int portUsedCnt;

	startPort =atoi(SERIAL_PORT_START);
	if( (startPort <=0 ) || (startPort >MAX_PORT_CNT) ){
		UserLog("value of SERIAL_PORT_START[%d] is error",startPort );
		return -1;
	}
	portUsedCnt =atoi(SERIAL_PORT_USED_COUNT);
	if( (portUsedCnt <=0 ) || (portUsedCnt >MAX_PORT_CNT) ){
		UserLog("value of SERIAL_PORT_USED_COUNT[%d] is error",portUsedCnt );
		return -1;
	}
	tmp = chnLog;
	for(i = 0; i < portUsedCnt; i++){
		tmp = tmp - portChnl[i];
		if(tmp <= 0){
			return startPort + i;
		}		
	}
	UserLog("GetSerialPort  error, chnLog[%d],portChnl[%s]",
			chnLog,Asc2Hex((unsigned char *)portChnl,MAX_PORT_CNT));

	return -1;
}

/*
 * func: 将背板连接号(逻辑通道号)转换为物理通道号
 */
int GetChnPhy(const int chnLog, const char *portChnl)
{
	int i;
	int startPort;
	int tmp;
	int portUsedCnt;

	startPort =atoi(SERIAL_PORT_START);
	if( (startPort <=0 ) || (startPort >MAX_PORT_CNT) ){
		UserLog("value of SERIAL_PORT_START[%d] is error",startPort );
		return -1;
	}

	portUsedCnt =atoi(SERIAL_PORT_USED_COUNT);
	if( (portUsedCnt <=0 ) || (portUsedCnt >MAX_PORT_CNT) ){
		UserLog("value of SERIAL_PORT_USED_COUNT[%d] is error",portUsedCnt );
		return -1;
	}

	tmp = chnLog;	
	for (i = 0; i < portUsedCnt; i++) {		
		if ((tmp - portChnl[i] - 1) < 0) {
			return tmp;
		} 
		tmp = tmp - portChnl[i] ;
	}

	UserLog("GetSerialPort  error, chnLog[%d],portChnl[%s]",chnLog,Asc2Hex((unsigned char *)portChnl,MAX_PORT_CNT));

	return -1;
}

//static int RecvGunCfg( char *cmd, int cmdlen )
 int RecvNodeCfg( const unsigned char *cmd, int cmdlen )
{
	int fd;
	int pos;
	int	i;
	//unsigned char buff[GUN_CFG_LEN+1];
	unsigned char buff[16];
	//if( cmdlen == 0 || cmdlen%DEV_CFG_LEN != 0 )
	if( cmdlen == 0 || cmdlen%sizeof(NODE_CHANNEL) != 0 ) {
		/*下传枪信息有误*/
		UserLog( "下传信息有误." );
		return -1;
	}

	/*打开文件*/
	fd = open(gsFullName, O_WRONLY|O_CREAT|O_TRUNC, 00644 );
	if( fd < 0 ) {
		UserLog( "打开配置文件[%s]失败.", gsFullName );
		return -1;
	}

	/*处理每条配置*/
	//for( i = 0; i < cmdlen/DEV_CFG_LEN; i++ )
	for( i = 0; i < cmdlen/sizeof(NODE_CHANNEL); i++ )
	{
		//buff[0] = (cmd+i*DEV_CFG_LEN)[0];		/*逻辑枪号*/
		//buff[1] = (cmd+i*DEV_CFG_LEN)[1];		/*油品号*/
		//buff[2] = (cmd+i*DEV_CFG_LEN)[5];		/*逻辑面板号*/
		//buff[3] = (cmd+i*DEV_CFG_LEN)[2];		/*通道号*/
		//buff[4] = (cmd+i*DEV_CFG_LEN)[4];		/*串口号*/
		//buff[5] = (cmd+i*DEV_CFG_LEN)[11];		/*品牌号*/
		//buff[6] = (cmd+i*DEV_CFG_LEN)[7];		/*物理枪号*/
		//buff[7] = (cmd+i*DEV_CFG_LEN)[6];		/*物理面板号*/
		//buff[8] = (cmd+i*DEV_CFG_LEN)[3];		/*物理通道号*/
		//buff[9] = (cmd+i*DEV_CFG_LEN)[12];		/*启用标志*/
		//buff[10] = 0;

		buff[0] = (cmd+i*sizeof(NODE_CHANNEL))[0];		/*逻辑节点号*/
		buff[1] = (cmd+i*sizeof(NODE_CHANNEL))[1];		/*逻辑面板号*/
		buff[2] = (cmd+i*sizeof(NODE_CHANNEL))[2];		/*加油机驱动号*/		
		buff[3] = (cmd+i*sizeof(NODE_CHANNEL))[3];		/*加油机面板数*/		
		buff[4] = (cmd+i*sizeof(NODE_CHANNEL))[4];		/*加油机油枪数*/		
		buff[5] = (cmd+i*sizeof(NODE_CHANNEL))[5];		/*加油机油品数*/	
		buff[6] = (cmd+i*sizeof(NODE_CHANNEL))[6];		/*加油机油枪数1*/	
		buff[7] = (cmd+i*sizeof(NODE_CHANNEL))[7];		/*加油机油枪数2*/	
		buff[8] = (cmd+i*sizeof(NODE_CHANNEL))[8];		/*加油机油枪数3*/	
		buff[9] = (cmd+i*sizeof(NODE_CHANNEL))[8];		/*加油机油枪数4*/	
		buff[10] = 0;//(cmd+i*sizeof(NODE_CHANNEL))[10];		/*串口号*/		
		buff[11] = 0; //(cmd+i*sizeof(NODE_CHANNEL))[11];		/*物理面板号*/	
		// this 2 data will caculate.
		buff[12] = 0;

		//write( fd, buff, GUN_CFG_LEN );
		write( fd, buff, sizeof(NODE_CHANNEL) );
	}
	close(fd);

	UserLog( "处理配置信息成功" );

	return 0;
}
 int SaveWorkDir(  )
{
	int fd;	
	char	*pFilePath;
	char temp[256];
	
	
	/*取配置文件路径*/
	pFilePath = getenv( "WORK_DIR" );	/*WORK_DIR 工作目录的环境变量*/
	if( pFilePath == NULL )
	{
		UserLog( "未定义环境变量WORK_DIR" );
		return -1;
	}

	/*打开文件*/
	fd = open( "/var/run/ifsf_main.wkd", O_WRONLY|O_CREAT|O_TRUNC, 00644 );
	if( fd < 0 ) {
		UserLog( "打开当前运行工作路径信息文件[%s]失败.", "/var/run/ifsf_main.wkd" );
		return -1;
	}
	bzero(temp,sizeof(temp));
	memcpy(temp,pFilePath,strlen(pFilePath));
	write( fd, temp, strlen(pFilePath)+2 );
	
	close(fd);

#if DEBUG
//	UserLog( "保存当前运行工作路径信息到/var/run/ifsf_main.wkd成功" );
#endif

	return 0;
}


int GetCfgNameSize( ){

	char	*pFilePath;
	int ret;

	ret = SaveWorkDir();
	if( ret < 0 ){		
		return -1;
	}

	/*取配置文件路径*/
	pFilePath = getenv( "WORK_DIR" );	/*WORK_DIR 工作目录的环境变量*/
	if( pFilePath == NULL )
	{
		UserLog( "未定义环境变量WORK_DIR" );
		return -1;
	}

	/*取配置文件名*/
	if( strlen( pFilePath ) + strlen( DSPCFGFILE ) > sizeof(gsFullName)-1 )//
	{
		UserLog( "文件名（包含绝对路径）过长。请调整不要超过[%d]字节\n ", sizeof(gsFullName)-1 );
		return -1;
	}
	sprintf( gsFullName, "%s/%s", pFilePath,  DSPCFGFILE);//ETCFILE


	/*取文件信息*/
	return GetFileSizeByName( gsFullName );

}

int GetIfsfCfgNameSize( )
{

	char	*pFilePath;
	int ret;

	ret = SaveWorkDir();
	if( ret < 0 ){		
		return -1;
	}
	/*取配置文件路径*/
	pFilePath = getenv( "WORK_DIR" );	/*WORK_DIR 工作目录的环境变量*/
	if( pFilePath == NULL )
	{
		UserLog( "未定义环境变量WORK_DIR" );
		return -1;
	}

	/*取配置文件名*/
	if( strlen( pFilePath ) + strlen( IFSFFILE) > sizeof(gsFullName)-1 )//ETCFILE
	{
		UserLog( "文件名（包含绝对路径）过长。请调整不要超过[%d]字节\n ", sizeof(gsFullName)-1 );
		return -1;
	}
	sprintf( gsFullName, "%s%s", pFilePath,  IFSFFILE);//ETCFILE
	//UserLog("file name:[%s]",gsFullName);

	/*取文件信息*/
	return GetFileSizeByName( gsFullName );

}

//得到oil.cfg的字节数modify by liys 
int GetOILCfgNameSize( )
{

	char	*pFilePath;
	int ret;

	ret = SaveWorkDir();
	if( ret < 0 ){		
		return -1;
	}
	/*取配置文件路径*/
	pFilePath = getenv( "WORK_DIR" );	/*WORK_DIR 工作目录的环境变量*/
	if( pFilePath == NULL ) {
		UserLog( "未定义环境变量WORK_DIR" );
		return -1;
	}

	/*取配置文件名*/
	if( strlen( pFilePath ) + strlen( OILFILE) > sizeof(gsFullName)-1 )//ETCFILE
	{
		UserLog( "文件名（包含绝对路径）过长。请调整不要超过[%d]字节\n ", sizeof(gsFullName)-1 );
		return -1;
	}
	sprintf( gsFullName, "%s%s", pFilePath,  OILFILE);//ETCFILE
	//UserLog("file name:[%s]",gsFullName);

	/*取文件信息*/
	return GetFileSizeByName( gsFullName );

}



//得到文件的字节数
//modify by liys 2008-03-17
int  GetFileSizeByFullName(const char *sFileName) {	
	FILE *fp;
	int len;
	int ret;
	int c;
	extern int errno;
	if(NULL == sFileName ){
		RunLog("GetLogFileSize argument is null");
		exit(1);
	}

	
	//file is exist or not?
	ret = access( sFileName, F_OK );
	if(ret < 0){
		len = 0;
	} 
	
	//get file size.

	fp = fopen(sFileName,"rb");
	if(!fp){
		char pbuff[256];
		bzero(pbuff,sizeof(pbuff));
		sprintf( pbuff,"打开文件[%s]失败.\n", sFileName );
		RunLog(pbuff);
		//return -1;
		exit(1);
	}

	fseek(fp, 0, SEEK_END);
	len = ftell(fp);
	fclose(fp);

	return len;	
}

int ListenCfg()
{
	char		port[6];
	int			sock;
	int			newsock;
	ssize_t		ret;
	char		*cmd;
	int			cmdlen;
	char		retcmd[2];

	strcpy( port, PCD_TCP_PORT );


	cmd = (char*)Malloc( MAX_ORDER_LEN + 1 );
	if( cmd == NULL ) {
		/*error*/
		UserLog( "分配空间失败" );
		return -1;
	}

	sock = TcpServInit( port, 5 );
	if( sock < 0 ) {
		/*错误处理*/
		UserLog( "初始化socket失败.port=[%s]\n", port );
		Free( cmd );
		return -1;
	}

	while( 1 ) {
		newsock = TcpAccept( sock, NULL );
		if( newsock < 0 )
		{
			UserLog( "TcpAccetp Failed" );
			close( sock );
			Free( cmd );
			return -1;
		}
		cmdlen = MAX_ORDER_LEN;
		memset( cmd, 0, sizeof(cmd) );

		ret = TcpRecvDataByHead( newsock, cmd, &cmdlen, TCP_HEAD_LEN, TIME_INFINITE );
		if( ret < 0 )
		{
			UserLog( "接收数据失败" );
			close( sock );
			close( newsock );
			Free( cmd );
			return -1;
		} else {

			UserLog( "接收到后台命令[%s]", Asc2Hex(cmd, cmdlen) );

			/*接收成功,向后台返回*/
			retcmd[0] = cmd[0];
			retcmd[1] = 0x10;
			ret = TcpSendDataByHead( newsock, retcmd, 2, TCP_HEAD_LEN, TCP_TIME_OUT );
			if( ret < 0 ) {
				UserLog( "向后台发送数据失败" );	/*错误忽略*/
			}

			/*根据cmd取枪号*/
			switch( cmd[0] ) {
				case 0x2D:							/*油枪配置*/
					ret = RecvNodeCfg( cmd+2, cmdlen-2 ); /*该命令不下传,自己处理*/
					break;
				default:			/*其他命令*/
					continue;		/*无效命令*/

			}
			if( ret == 0 ) {
				break;
			} else {
				UserLog( "处理命令失败.\n" );
				close(newsock);
				continue;
			}

		}

	}
	Free(cmd);
	close(sock);
	close(newsock);

	return ret;

}


int OpenCfg()
{
	/*打开配置文件*/
	return open( gsFullName, O_RDONLY );
}

/*
 * func: [变价后]更新配置文件中的油品单价
 */
int UpdateOilCfg(unsigned char const *oilno, unsigned char const *price)
{
	int ret;
	FILE *fp = NULL;
	fpos_t *fpos = NULL;
	unsigned char *pFilePath;
	unsigned char cfgbuf[sizeof(OIL_CONFIG) + 1];
	OIL_CONFIG *pcfg = NULL;
	long pos = 0;

	if (oilno == NULL || price == NULL) {
		return -1;
	}
	/*取配置文件路径*/
	pFilePath = getenv("WORK_DIR");	/*WORK_DIR 工作目录的环境变量*/
	if (pFilePath == NULL) {
		UserLog( "未定义环境变量WORK_DIR" );
		return -1;
	}

	/*取配置文件名*/
	if (strlen(pFilePath) + strlen(OILFILE) > sizeof(gsFullName) - 1) {
		UserLog( "文件名（包含绝对路径）过长。请调整不要超过[%d]字节\n ", sizeof(gsFullName) - 1);
		return -1;
	}
	sprintf(gsFullName, "%s%s", pFilePath, OILFILE);

	fp = fopen(gsFullName, "r+");
	if (fp == NULL) {
		RunLog("打开配置文件 oil.cfg 失败");
		return -1;
	}

	while (1) {
		pos = ftell(fp);
		if (ret < 0) {
			RunLog("获取当前位置指针失败");
			fclose(fp);
			return -1;
		}
#if 0
		// 用fgetpos 会出错, 为什么??
		ret = fgetpos(fp, fpos);
		if (ret < 0) {
			RunLog("获取当前位置指针失败");
			fclose(fp);
			return -1;
		}
#endif

		ret = fread(cfgbuf, sizeof(OIL_CONFIG), 1, fp);
		if (ret < 0) {
			RunLog("读取配置文件 oil.cfg 出错");
			break;
		} else if (ret == 0) {
			break;
		} else if (ret != 1) {
			RunLog("读取的油品配置信息有错");
			fclose(fp);
			return -1;
		}

		pcfg = (OIL_CONFIG *)cfgbuf;
//		RunLog("读到油品配置: %s", Asc2Hex(pcfg, sizeof(OIL_CONFIG)));
		if (memcmp(oilno, cfgbuf, 4) == 0) {
			// 找到该油品号配置项
			memcpy(pcfg->price, price, 3);
			
			ret = fseek(fp, pos, SEEK_SET);
			if (ret < 0) {
				RunLog("设置当前位置指针失败");
				fclose(fp);
				return -1;
			}

			ret = fwrite(cfgbuf, sizeof(OIL_CONFIG), 1, fp);
			if (ret != 1) {
				ret = fsetpos(fp, fpos);
				ret = fwrite(cfgbuf, sizeof(OIL_CONFIG), 1, fp);
				if (ret != 1) {
					RunLog("更新油品 %s 的新单价到配置文件失败,", 
							Asc2Hex(pcfg->oilno, 4));
					RunLog("errno: %s", strerror(errno));
					fclose(fp);
					return -1;
				}
			}

			RunLog("更新油品 %s 的新单价到配置文件成功", Asc2Hex(pcfg->oilno, 4));
			break;
		} else {
			continue;
		}

	}

	fclose(fp);

	return 0;
}


/*
 * func: 同步共享内存(数据库)中的配置到配置文件
 */
int SyncCfg2File()
{
	int ret;
	FILE *fp = NULL;
	unsigned char *pFilePath;
	unsigned char cfgbuf[sizeof(OIL_CONFIG) + 1];
	unsigned char buffer[sizeof(gsFullName) + 30];

	gpIfsf_shm->cfg_changed = 0xFF;       // locked

	RunLog("============= sizeof(buffer) : %d ===============", sizeof(buffer));
	/*取配置文件路径*/
	pFilePath = getenv("WORK_DIR");
	if (pFilePath == NULL) {
		UserLog("未定义环境变量WORK_DIR");
		gpIfsf_shm->cfg_changed = 0x0F;
		return -1;
	}

	/*
	 * Step.1 同步油品配置到文件
	 */
	if (strlen(pFilePath) + strlen(OILFILE) > sizeof(gsFullName) - 1) {
		UserLog("文件名(包含绝对路径)过长. 请调整不要超过[%d]字节\n ", sizeof(gsFullName) - 1);
		gpIfsf_shm->cfg_changed = 0x0F;
		return -1;
	}

	sprintf(gsFullName, "%s%s", pFilePath, OILFILE);

	// --Backup config file - oil.cfg
	sprintf(buffer, "cp -f %s %s.bak", gsFullName, gsFullName);
	RunLog("oil.cfg: %s", buffer);
	system(buffer);

	fp = fopen(gsFullName, "w+");
	if (fp == NULL) {
		RunLog("打开配置文件 oil.cfg 失败");
		gpIfsf_shm->cfg_changed = 0x0F;
		return -1;
	}

	ret = fwrite(gpIfsf_shm->oil_config, sizeof(OIL_CONFIG), gpIfsf_shm->oilCnt, fp);
	if (ret == gpIfsf_shm->oilCnt) {
		RunLog("Sync oil_config to oil.cfg successful!");
		fclose(fp);
	} else {
		RunLog("Sync oil_config to oil.cfg failed!");
		fclose(fp);
		gpIfsf_shm->cfg_changed = 0x0F;
		return -1;
	}


	/*
	 * Step.2 同步油枪配置到文件
	 */
	if (strlen(pFilePath) + strlen(DSPCFGFILE) > sizeof(gsFullName) - 1) {
		UserLog("文件名(包含绝对路径)过长. 请调整不要超过[%d]字节\n ", sizeof(gsFullName) - 1);
		gpIfsf_shm->cfg_changed = 0x0F;
		return -1;
	}

	sprintf(gsFullName, "%s%s", pFilePath, DSPCFGFILE);

	// --Backup config file - oil.cfg
	sprintf(buffer, "cp -f %s %s.bak", gsFullName, gsFullName);
	RunLog("oil.cfg: %s", buffer);
	system(buffer);

	fp = fopen(gsFullName, "w+");
	if (fp == NULL) {
		RunLog("打开配置文件 dsp.cfg 失败");
		gpIfsf_shm->cfg_changed = 0x0F;
		return -1;
	}

	ret = fwrite(gpIfsf_shm->gun_config, sizeof(GUN_CONFIG), gpIfsf_shm->gunCnt, fp);
	if (ret == gpIfsf_shm->gunCnt) {
		RunLog("Sync gun_config to gun.cfg successful!");
		fclose(fp);
	} else {
		RunLog("Sync gun_config to gun.cfg failed!");
		fclose(fp);
		gpIfsf_shm->cfg_changed = 0x0F;
		return -1;
	}

	gpIfsf_shm->cfg_changed = 0;       // release
	gpGunShm->if_changed_prno = 1;
	RunLog("同步油品油枪配置到配置文件成功");
	return 0;
}



int ReadAllCfg(init_cfg *initcfg)
{
	FILE *fp;
	char buff[128];
	char *pbuff;
	int term_no = 0;
	int i = 0;
	RunLog("Read All Cfg");
	if (( fp= fopen(CFGFILE, "a+"))== NULL){
		RunLog("!!!fopen [%s] failed ", CFGFILE);
		return -1;
	}
	while(fgets(buff, 128, fp)){
		pbuff = TrimAll(buff);

		for (i = 0;*pbuff != '='; pbuff++, i++){
			initcfg[term_no].name[i] = *pbuff;
		}
		initcfg[term_no].name[i]= '\0';
		for (i = 0,  ++pbuff; *pbuff != '\0'; pbuff++, i++){
			initcfg[term_no].value[i] = *pbuff;
		}	
		initcfg[term_no].value[i]= '\0';
		RunLog("name is %s value is %s",initcfg[term_no].name, initcfg[term_no].value );
		term_no++;
	}
	bzero((initcfg+term_no), sizeof(init_cfg));
	fclose(fp);
	RunLog("Read All Cfg");	
	return term_no;
}

int ReadCfgByName(init_cfg *initcfg, const char *name)
{
	while(initcfg->name[0]!= 0){
		if (strcmp(initcfg->name, name) == 0){
			return atoi(initcfg->value);
		}
		initcfg++;
	}
	RunLog("there is no config term named [%s]", name);
	return -1;
}



/* 			public functions
*/

#include <stdarg.h>
#include "tank_pub.h"


unsigned char *S_PORT[3]={"/dev/ttyS0","/dev/ttyS1", "/dev/ttyS2"};

void __debug(int preority, const char *fmt,...)
{
	va_list arg;
	unsigned char msg[MAX_TANK_PARA_LEN];							
	
	if(preority >= DEBUG_P){
		va_start(arg, fmt );
		vsprintf(msg,fmt, arg );
		va_end(arg);

		printf("%s\n", msg);
	}
	return;
}

unsigned char *get_serial(int p)
{
	return S_PORT[p];
}

float exp2float(unsigned char *data)
{
	unsigned char sign, m[3];
	float ret = 0, base = 8388608;
	double exp, e = 0;
	long tmp;

	sign = data[0] >> 7;

	e = (unsigned char)((data[0] << 1) | (data[1] >> 7)) - 127;
	exp = pow(2, e);

	memcpy(m, data + 1, 3);
	m[0] = m[0] & 0x7F;
	tmp = Hex2Long(m, 3);
	ret = (1.0 + tmp / base) * exp;

	if (sign == 1) {
	       	ret = ret * (-1);
	}

	return ret;
}

int asc2hex(unsigned char *src, unsigned char *des)
{	int i, j,ret;
	for(i=0;i<8;i++){
		if(src[i] >= 0x30 && src[i] <= 0x39) src[i] -= 0x30;
		if(src[i] > 0x39 && src[i] <=0x46) src[i] -= 0x37;
		}
	for(i=0, j=0;i<8;i+=2){
		des[j] = src[i] * 16 +src[i+1];
		j++;
		}

	return 0;
}

int set_systime( char *datetime )
{
	struct tm bkTime;
	struct timeval timeSet;
	char tmp[8];
	int offset = 0;
	int ret;
	
	memset( &bkTime, 0, sizeof(bkTime) );
	
	if( strlen(datetime) != 14 )
		return -1;
		
	/*year*/
	memset( tmp, 0, sizeof(tmp) );
	memcpy( tmp, datetime, 4 ); tmp[4] = 0; offset += 4;
	bkTime.tm_year = atoi(tmp)-1900;

	/*month*/
	memcpy( tmp, datetime+offset, 2 ); tmp[2] = 0; offset += 2;
	bkTime.tm_mon = atoi(tmp)-1;
	if( bkTime.tm_mon < 0 || bkTime.tm_mon > 11 )
		return -1;
			
	/*day*/
	memcpy( tmp, datetime+offset, 2 ); tmp[2] = 0; offset += 2;
	bkTime.tm_mday = atoi(tmp);
	if( bkTime.tm_mday < 1 || bkTime.tm_mday > 31 )
		return -1;
			
	/*hour*/
	memcpy( tmp, datetime+offset, 2 ); tmp[2] = 0; offset += 2;
	bkTime.tm_hour = atoi(tmp);
	if( bkTime.tm_hour < 0 || bkTime.tm_hour > 23 )
		return -1;

	/*minute*/
	memcpy( tmp, datetime+offset, 2 ); tmp[2] = 0; offset += 2;
	bkTime.tm_min = atoi(tmp);
	if( bkTime.tm_min < 0 || bkTime.tm_min > 59 )
		return -1;

	/*second*/
	memcpy( tmp, datetime+offset, 2 ); tmp[2] = 0; offset += 2;
	bkTime.tm_sec = atoi(tmp);
	if( bkTime.tm_sec < 0 || bkTime.tm_sec > 59 )
		return -1;

	memset( &timeSet, 0, sizeof(timeSet) );
	timeSet.tv_sec = mktime( &bkTime );
	
	ret = settimeofday( &timeSet, NULL );
	
	return ret;
}

void print_result(void)
{ 	int i,k;

	printf("Get data time: ");
	for(i=0;i<4;i++) printf("%02x ", gpIfsf_shm->tlg_data.tlg_dat.Current_Date[i]);
	for(i=0;i<3;i++) printf("%02x ", gpIfsf_shm->tlg_data.tlg_dat.Current_Time[i]);
	printf("\n");
	
	for(i=0;i<PROBER_NUM;i++){
		printf("probe number: \t%d\n",i);
		printf("Gross_Standard_Volume: \t");
		for(k=0;k<7;k++) printf("%02x ",gpIfsf_shm->tlg_data.tp_dat[i].Gross_Standard_Volume[k]);
		printf("\n");
		printf("Total_Observed_Volume:");
		for(k=0;k<7;k++) printf("%02x ",gpIfsf_shm->tlg_data.tp_dat[i].Total_Observed_Volume[k]);
		printf("\n");
		printf("product level: \t");
		for(k=0;k<4;k++) printf("%02x ",gpIfsf_shm->tlg_data.tp_dat[i].Product_Level[k]);
		printf("\n");
		printf("water level: \t");
		for(k=0;k<4;k++) printf("%02x ",gpIfsf_shm->tlg_data.tp_dat[i].Water_Level[k]);
		printf("\n");
		printf("temperature: \t");
		for(k=0;k<4;k++) printf("%02x ",gpIfsf_shm->tlg_data.tp_dat[i].Average_Temp[k]);
		printf("\n");
		printf("Alarm code: \t");
		for(k=0;k<2;k++) printf("%02x ",gpIfsf_shm->tlg_data.tp_dat[i].TP_Alarm[k]);
		printf("\n\n");
		}
}

int tank_online(__u8 status)
{
	static __u8 pre_status = 0xFF;

	if (gpIfsf_shm == NULL) {
		RunLog("连接共享内存失败");
		return -1;
	}

	if (pre_status == status) {
		return 0;
	} else {
		gpIfsf_shm->node_online[MAX_CHANNEL_CNT] = status;
		pre_status = status;
	}

	if (gpGunShm->data_acquisition_4_TCC_adjust_flag != 0) {
		write_tcc_adjust_data(NULL, ",,91,,,,,%d,,,", status);
		TCCADLog("TCC-AD: 液位仪 %s", (status == NODE_OFFLINE ? "离线" : "联线"));
	}

	return 0;
}

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
static char *split(char *s, char stop){
	char *data;
	char *tmp;
	int i, len, j;

	len = strlen(s);
	tmp = s;
	data = (char *)malloc( sizeof(char) * (len + 1));
	if(NULL == data ){
		return NULL;
	}

	for(i =0; i < len; i ++){
		if (s[i] != stop){
			data[i] = s[i] ;
		}
		else {
			i ++;
			break;
		}
	}
	data[i] = '\0';
	for(j = i; j < len; j++) 
		s[j - i] = tmp[j];
	s[len -i] = '\0';
	return data;
}
/**
*fuc: get configure of server ip and station num
*
*/
static int tank_readIni(char *fileName){

	char  my_data[1024];
	char *tmp_ptr, *tmp;
	int data_len;
	int i,ret;
	int c;
	FILE *f;
	
	if(  NULL== fileName ){
		return -1;
	}
	ret = IsFileExist(fileName);
	if(ret != 1){
		char pbuff[256];
		bzero(pbuff,sizeof(pbuff));
		sprintf( pbuff, "file[%s] not exist in readIni  error!! ", fileName );
		RunLog(pbuff);
		return -2;
	}
	
	f = fopen( fileName, "rt" );
	if( !f  ){	
		char pbuff[256];
		bzero(pbuff,sizeof(pbuff));
		sprintf( pbuff, "file[%s] open error in readIni!! ", fileName );
		RunLog(pbuff);
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
	
	bzero(tank_ini, sizeof(tank_ini));	
	i = 0;
	while (my_data[0] !='\0'){
		tmp = split(my_data, '=');// split data to get field name.
		bzero(&(tank_ini[i].name), sizeof(tank_ini[i].name));
		strcpy(tank_ini[i].name, TrimAll(tmp) );
		if (tmp != NULL) {
			free(tmp);
			tmp = NULL;
		}
		tmp = split(my_data, '\n');// split data to get value.
		bzero(&(tank_ini[i].value), sizeof(tank_ini[i].value));
		strcpy(tank_ini[i].value, TrimAll(tmp));
		if (tmp != NULL) {
			free(tmp);
			tmp = NULL;
		}
		i ++;
	}
	
	return i--;	
}


int tank_init_config(unsigned char *ip, unsigned char *stano, int * globle_config, int *tank_cnt)
{
	int i,ret,iniFlag;
	char configFile[256];

	int fd;
	char obtain[2];
	//solve the tankserver heart point send after tank status message
	sleep(11);
	bzero(obtain, 2);
	fd = open("/home/App/ifsf/etc/obtain.cfg", O_RDWR|O_CREAT, 00666);
	if (fd == -1)
		RunLog("open obtain.cfg failed");
	else{
		ret = read(fd, obtain, 1);
		if (ret == 0){
			write(fd, "0", 1);
			memcpy(obtain, "0", 2);
		}
		close(fd);
	}
	*globle_config = atoi(obtain);
	*tank_cnt = atoi(TLG_PROBER_NUM);
	RunLog("read tank configure flag is [%d] tank cnt is [%d]", *globle_config, *tank_cnt);

	if (*globle_config == 0)
		return 1;
	ret = tank_readIni(TANK_CONFIG_FILE);
	
	//RunLog("33333");
	if(ret < 0){
		RunLog("readIni error in _InitServerIP");
		return -1;
	}
	iniFlag = 0;
	RunLog("开始查找配置参数");
	for(i=0;i<ret;i++){
		if( memcmp(tank_ini[i].name,"SERVER_IP",9) == 0){
			memcpy(ip, tank_ini[i].value, sizeof(tank_ini[i].value) );
			RunLog("SERVER_IP[%s]",ip);
			iniFlag += 1;
		}else if( memcmp(tank_ini[i].name,"STATION_NO",10) == 0 ){
			//memcpy(gpDLB_Shm->css._STATION_NO, DLB_ini[i].value, sizeof(DLB_ini[i].value) );
			stano[0] = ( (tank_ini[i].value[0]-'0')&0x0f ) << 4 | ((tank_ini[i].value[1]-'0')&0x0f);
			stano[1] = ( (tank_ini[i].value[2]-'0')&0x0f ) << 4 | ((tank_ini[i].value[3]-'0')&0x0f);
			stano[2] = ( (tank_ini[i].value[4]-'0')&0x0f ) << 4 | ((tank_ini[i].value[5]-'0')&0x0f);
			stano[3] = ( (tank_ini[i].value[6]-'0')&0x0f ) << 4 | ((tank_ini[i].value[7]-'0')&0x0f);
			stano[4] = ( (tank_ini[i].value[8]-'0')&0x0f ) << 4 | ((tank_ini[i].value[9]-'0')&0x0f);
			RunLog("STATION_NO[%s]", Asc2Hex(stano, 5) );
			iniFlag += 1;
		}
	}
	//DLB_LevelLog("查找配置参数结束");
	if(iniFlag != 2 ) 
		return -1;
	else 
		return 0;	
 }


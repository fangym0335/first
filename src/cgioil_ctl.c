#include "ifsf_pub.h"
#include "cgidecoder.h"

#define MAX_PATH_NAME_LENGTH 1024
#define DF_WRK_DIR	"/home/App/ifsf/" 
#define OIL_CONTROL_HTML_FILE "oilctrl.txt" 
char allStatus[][16] = {
	"正常系统状态",
	"停止系统状态",
	"重启系统状态",
	"暂停系统状态"
};

static char gKeyFile[MAX_PATH_NAME_LENGTH]="";


static int InitTheKeyFile() 
{
	char	*pFilePath, FilePath[]=DEFAULT_WORK_DIR;
	char temp[128];
	int ret;
	
	//取keyfile文件路径//	
	pFilePath = getenv( "WORK_DIR" );	//WORK_DIR 工作目录的环境变量
	if( NULL ==  pFilePath){
		bzero(temp, sizeof(temp));
		ret = GetWorkDir(temp);
		if(ret >= 0 ){
			pFilePath = temp;
		}
		else{
			pFilePath = FilePath;
		}
		//cgi_error( "未定义环境变量WORK_DIR" );
		//exit(1);
		//return -1;
	}

	sprintf( gKeyFile, "%s/etc/keyfile.cfg", pFilePath );

	if( IsTheFileExist( gKeyFile ) == 1 )
		return 0;

	return -1;
}

int main( )
{
	GunShm 	*pGunShm;
	int ret,n,k;
	char htmlbuff[4096];

	ret = InitTheKeyFile();
	if( ret < 0 )
	{
		cgi_error( "初始化Key File失败\n" );
		exit(1);
	}

	pGunShm = (GunShm*)AttachShm( GUN_SHM_INDEX );
	if( pGunShm == NULL )
	{
		cgi_error( "连接共享内存GUN_SHM_INDEX失败\n" );
		exit(1);
	}

	//pGunShm->sysStat = STOP_PROC;
	
	n = getCgiInput();
	if(n < 0){
		cgi_error( "获取CGI参数错误.\n" );
		exit(-1);
	}
	if(0 == n ){
		bzero( htmlbuff,sizeof(htmlbuff) );
		ret = ReadTextFile(OIL_CONTROL_HTML_FILE, htmlbuff);		
		printf(htmlbuff, allStatus[pGunShm->sysStat]);
		DetachShm((char**)(&pGunShm));
		exit(1);
	}
	else if(memcpy(nv[0].name,"status",6) == 0){
		k= atoi( TrimAll(nv[0].value) );
		if( (k>=0) &&(k <= 3) ){
			pGunShm->sysStat = k;
			DetachShm( (char**)(&pGunShm) );
			bzero(htmlbuff,sizeof(htmlbuff));
			sprintf( htmlbuff, "前庭控制器状态[%s]设置成功成功\n", allStatus[k]);
			cgi_ok(htmlbuff);
			exit(1);
		}
	}
	
	DetachShm((char**)(&pGunShm));
	cgi_ok( "暂停前庭控制器成功\n" );
	exit(1);
	return 0;
}

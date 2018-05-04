#include "ifsf_pub.h"
#include "cgidecoder.h"

#define MAX_PATH_NAME_LENGTH 1024
#define DF_WRK_DIR	"/home/App/ifsf/" 
#define OIL_CONTROL_HTML_FILE "oilctrl.txt" 
char allStatus[][16] = {
	"����ϵͳ״̬",
	"ֹͣϵͳ״̬",
	"����ϵͳ״̬",
	"��ͣϵͳ״̬"
};

static char gKeyFile[MAX_PATH_NAME_LENGTH]="";


static int InitTheKeyFile() 
{
	char	*pFilePath, FilePath[]=DEFAULT_WORK_DIR;
	char temp[128];
	int ret;
	
	//ȡkeyfile�ļ�·��//	
	pFilePath = getenv( "WORK_DIR" );	//WORK_DIR ����Ŀ¼�Ļ�������
	if( NULL ==  pFilePath){
		bzero(temp, sizeof(temp));
		ret = GetWorkDir(temp);
		if(ret >= 0 ){
			pFilePath = temp;
		}
		else{
			pFilePath = FilePath;
		}
		//cgi_error( "δ���廷������WORK_DIR" );
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
		cgi_error( "��ʼ��Key Fileʧ��\n" );
		exit(1);
	}

	pGunShm = (GunShm*)AttachShm( GUN_SHM_INDEX );
	if( pGunShm == NULL )
	{
		cgi_error( "���ӹ����ڴ�GUN_SHM_INDEXʧ��\n" );
		exit(1);
	}

	//pGunShm->sysStat = STOP_PROC;
	
	n = getCgiInput();
	if(n < 0){
		cgi_error( "��ȡCGI��������.\n" );
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
			sprintf( htmlbuff, "ǰͥ������״̬[%s]���óɹ��ɹ�\n", allStatus[k]);
			cgi_ok(htmlbuff);
			exit(1);
		}
	}
	
	DetachShm((char**)(&pGunShm));
	cgi_ok( "��ͣǰͥ�������ɹ�\n" );
	exit(1);
	return 0;
}

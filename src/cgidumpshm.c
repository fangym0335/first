/*
 * 2008-03-10 - Guojie Zhu <zhugj@css-intelligent.com>
 *     TR_Date_Time[0] - TR_Date_Time[3] => TR_Date[0] - TR_Date[3]
 *     TR_Date_Time[4] - TR_Date_Time[6] => TR_Time[0] - TR_Time[2]
 */

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <signal.h>
#include <sys/ipc.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
//#include <ifsf_main.h>


#include "ifsf_pub.h"
#include "cgidecoder.h"
#include "jansson.h"

#define MAX_PATH_NAME_LENGTH 1024
#define OIL_STAT_LENGTH 10
#define STAT_CPY_LENGTH 8
#define TEMPLATE_HTML_FILE    "cgitemplate.txt"
//#define  TRANS_PER_PAGE 30
typedef struct{	
	unsigned char node;
	unsigned char chnLog;  //logical channel number in a PCD.	
	unsigned char nodeDriver; //dispenser driver type,this determine how to initial and wich lischnxxx to use.
	unsigned char serialPort; //use which serial Port at main board.
	unsigned char fpPhy;   // phyzical fuelling point in this node.	
	unsigned char cntOfFpNoz; //count of Nozzles  in this FP of the dispenser.
	unsigned char movedNoz; //which nozzle is moved.
	unsigned char fpStat; // this fuelling poit status.
	unsigned char volume[5]; //current vollume
	unsigned char amount[5]; //current amount
	unsigned char prodId; //product ID of this fuelling point. 
	unsigned char meter[7]; //current pump total
}  NODE_FP_STAT;


static char gKeyFile[MAX_PATH_NAME_LENGTH]="";

static void  refreshshm( char * title, char* htmltable,char *returl)
    {
    (void) printf( "\
Content-type: text/html\n\
\n\
<HTML><HEAD><TITLE>%s</TITLE></HEAD>\n\
<meta http-equiv=refresh content=3>\n\
<BODY><H2 align=center>%s </H2>\n\
 %s \n\
 <table align=center><tr><td><a  href=\"%s \" >返回</a></td></tr></table>\n\
</BODY></HTML>\n    ",  title,  title, htmltable, returl );
    }
static int InitTheKeyFile() 
{
	char	*pFilePath, FilePath[]=DEFAULT_WORK_DIR_4CGI;
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
	if((NULL ==  pFilePath) ||(pFilePath[0] !=  '/') ){
		pFilePath = FilePath;
		}
	sprintf( gKeyFile, "%s/etc/keyfile.cfg", pFilePath );
	//cgi_ok(gKeyFile);
	//exit(0);
	if( IsTheFileExist( gKeyFile ) == 1 )
		return 0;

	return -1;
}

static key_t GetTheKey( int index )
{
	key_t key;
	if( gKeyFile[0] == 0 ){
		cgi_error( "gKeyFile[0] == 0" );
		exit(1);
	}
	key = ftok( gKeyFile, index );
	if ( key == (key_t)(-1) ){
		cgi_error( " key == (key_t)(-1)" );
		exit(1);
	}
	return key;
}


static char *AttachTheShm( int index )
{
	int ShmId;
	char *p;

	ShmId = shmget( GetTheKey(index), 0, 0 );
	if( ShmId == -1 )
	{
		return NULL;
	}
	p = (char *)shmat( ShmId, NULL, 0 );
	if( p == (char*) -1 )
	{
		return NULL;
	}
	else
		return p;
}

static int DetachTheShm( char **p )
{
	int r;

	if( *p == NULL )
	{
		return -1;
	}
	r = shmdt( *p );
	if( r < 0 )
	{
		return -1;
	}
	*p = NULL;

	return 0;
}
/*
static int  ListFlashTrans(char *sFilename, const int  node){
	long i, k;
	int j;
	FP_TR_DATA *transdata;
	unsigned char filebuffer[sizeof(FP_TR_DATA)*32];
	char strtmp[16384];
	
	if(  (NULL == sFilename) || ( node <=0) ||( node >255)){
		return -1;
	}
	
	bzero( strtmp,sizeof(strtmp) );
	sprintf(strtmp, "<table> <tr width=1024 align=center><td>加油机节点号:%d</td></tr> </table>  \
		<table><tr  width=1024  align=center style='color:#4A3C8C;background-color:#F9F9F9;'>  \
		<td>node</td><td>fp_ad</td><td>Seq_Nb:2</td><td>Contr_Id:2</td>    \
		<td>Release_Token</td><td>Fuelling_Mode</td><td>Amount:5</td><td>Volume:5</td><td>Unit_Price:4</td>  \
		<td>Log_Noz</td><td>Prod_Nb:4</td><td>Error_Code</td><td>Security_Chksum:3</td>  \
		<td>Trans_State</td><td>Buff_Contr_Id:2</td> <td>Date_Time:7</td></tr>     ", node); 
	for(i=0,j=0;i<FP_TR_CONVERT_CNT; i++){
		transdata = (FP_TR_DATA *)&(ifsftrans[0]);
		if(transdata->node == node ){
			k = strlen(strtmp);	
			j++;
			k = strlen(strtmp);	
			if( j %2 == 0)
			sprintf(&strtmp[k],  "<tr width=1024  align=center style='color:#4A3C8C;background-color:#F7F7F7;'>   \
		<td>%d</td><td>%02x</td><td>%02x%02x</td><td>%02x%02x</td><td>%d</td>   \
		<td>%d</td><td>%02x%02x%02x%02x%02x</td><td>%02x%02x%02x%02x%02x</td><td>%02x%02x%02x%02x</td>  \
		<td>%d</td><td>%02x%02x%02x%02x</td><td>%d</td><td>%02x%02x%02x</td>  \
		<td>%d</td><td>%02x%02x</td><td>%02x%02x%02x%02x%02x%02x%02x</td></tr>     " ,   transdata->node, transdata->fp_ad, \
		transdata->TR_Seq_Nb[0],   transdata->TR_Seq_Nb[1], transdata->TR_Contr_Id[0], transdata->TR_Contr_Id[1], transdata->TR_Release_Token,    \
		transdata->TR_Fuelling_Mode,  transdata->TR_Amount[0], transdata->TR_Amount[1], transdata->TR_Amount[2], transdata->TR_Amount[3],   \
		transdata->TR_Amount[4], transdata->TR_Volume[0], transdata->TR_Volume[1],  transdata->TR_Volume[2],   transdata->TR_Volume[3],  \
		transdata->TR_Volume[4],  transdata->TR_Unit_Price[0],  transdata->TR_Unit_Price[1],  transdata->TR_Unit_Price[2],   transdata->TR_Unit_Price[3], \
		transdata->TR_Log_Noz,  transdata->TR_Prod_Nb[0], transdata->TR_Prod_Nb[1], transdata->TR_Prod_Nb[2], transdata->TR_Prod_Nb[3], \
		transdata->TR_Error_Code, transdata->TR_Security_Chksum[0], transdata->TR_Security_Chksum[1], transdata->TR_Security_Chksum[2], \
		transdata->Trans_State, transdata->TR_Buff_Contr_Id[0], transdata->TR_Buff_Contr_Id[1],  transdata->TR_Date_Time[0], transdata->TR_Date_Time[1],  \
		transdata->TR_Date_Time[2],  transdata->TR_Date_Time[3], transdata->TR_Date_Time[4],  transdata->TR_Date_Time[5],  transdata->TR_Date_Time[6] );
		else
			sprintf(&strtmp[k],  "<tr width=1024 align=center style='color:#4A3C8C;background-color:#E7E7FF;'>  \
		<td>%d</td><td>%02x</td><td>%02x%02x</td><td>%02x%02x</td>	<td>%d</td>   \
		<td>%d</td><td>%02x%02x%02x%02x%02x</td><td>%02x%02x%02x%02x%02x</td><td>%02x%02x%02x%02x</td>  \
		<td>%d</td><td>%02x%02x%02x%02x</td><td>%d</td><td>%02x%02x%02x</td>  \
		<td>%d</td><td>%02x%02x</td><td>%02x%02x%02x%02x%02x%02x%02x</td></tr>     " ,   transdata->node, transdata->fp_ad, \
		transdata->TR_Seq_Nb[0],   transdata->TR_Seq_Nb[1], transdata->TR_Contr_Id[0], transdata->TR_Contr_Id[1], transdata->TR_Release_Token,    \
		transdata->TR_Fuelling_Mode,  transdata->TR_Amount[0], transdata->TR_Amount[1], transdata->TR_Amount[2], transdata->TR_Amount[3],   \
		transdata->TR_Amount[4], transdata->TR_Volume[0], transdata->TR_Volume[1],  transdata->TR_Volume[2],   transdata->TR_Volume[3],  \
		transdata->TR_Volume[4],  transdata->TR_Unit_Price[0],  transdata->TR_Unit_Price[1],  transdata->TR_Unit_Price[2],   transdata->TR_Unit_Price[3], \
		transdata->TR_Log_Noz,  transdata->TR_Prod_Nb[0], transdata->TR_Prod_Nb[1], transdata->TR_Prod_Nb[2],  transdata->TR_Prod_Nb[3], \
		transdata->TR_Error_Code, transdata->TR_Security_Chksum[0], transdata->TR_Security_Chksum[1], transdata->TR_Security_Chksum[2],  \
		transdata->Trans_State, transdata->TR_Buff_Contr_Id[0], transdata->TR_Buff_Contr_Id[1],  transdata->TR_Date_Time[0], transdata->TR_Date_Time[1],  \
		transdata->TR_Date_Time[2],   transdata->TR_Date_Time[3], transdata->TR_Date_Time[4],  transdata->TR_Date_Time[5],  transdata->TR_Date_Time[6] );
		}
	}
	k = strlen(strtmp);	
	sprintf(&strtmp[k], " </table>    ");
	//返回
	memcpy(ifsftrans,strtmp,strlen(strtmp)+1 );
	return 0;
}
*/
#define SERVER_CONFIG_FILE "/home/App/ifsf/etc/config.cfg"

int connect_cfgsrv(void)
{
	int sockfd;
	struct sockaddr_in seraddr,
					   cltaddr;
	const int const_int_1 = 1;
	json_t *root = NULL;
	json_t *child = NULL;
	json_t *value = NULL;
	json_error_t error;
	const char *key = NULL;
	char cgi_error_buff[200] = "";

	root = json_load_file(SERVER_CONFIG_FILE, 0, &error);
	if (NULL == root) {
		sprintf(cgi_error_buff,"json load file config.cfg error:%s", error.text);
		cgi_error(cgi_error_buff);
		return -1;
	}

	child = json_object_get(root, "general");
	if (NULL == child) {
		cgi_error("try to get general failed");
		goto EXIT_READ_JSON;
	}

	json_object_foreach(child, key, value) {
		if (value == NULL) {
			sprintf(cgi_error_buff, "parsing general error, key: %s", key);
			cgi_error(cgi_error_buff);
			goto EXIT_READ_JSON;
		}

		if (strcmp("config_server_ip", key) == 0) {
			seraddr.sin_addr.s_addr = inet_addr(json_string_value(value));
		}
		else if (strcmp("config_server_port", key) == 0) {
			seraddr.sin_port = htons(json_integer_value(value));
		}
	}
	seraddr.sin_family = AF_INET;

	if (-1 == (sockfd = socket(AF_INET, SOCK_STREAM, 0))) {
		cgi_error("open socket error");
		goto EXIT_READ_JSON;
	}
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &const_int_1, sizeof(const_int_1));
	if (-1 == (connect(sockfd, (struct sockaddr *)&seraddr, sizeof(seraddr)) ) ) {
		//cgi_error("connect to cd config server failed");
		close(sockfd);
		//cgi_error("connect to CD config server error ............");
		goto EXIT_READ_JSON;
	}

	//cgi_ok("connect to CD CONFIG SERVER OK !!");

	json_decref(root);
	return sockfd;

EXIT_READ_JSON:
	//cgi_error("int EXIT_READ_JSON");
	json_decref(root);
	return -1;
}

static int  ListMemTrans(const IFSF_SHM *pIfsf_Shm, const int  node, char *ifsftrans){
	long i, k;
	int j;
	FP_TR_DATA *transdata;
	char strtmp[16384];
	
	if(  (NULL == pIfsf_Shm) || (NULL == ifsftrans) ||(0 == node) ){
		return -1;
		}	
	
	bzero( strtmp,sizeof(strtmp) );
	sprintf(strtmp, "<table> <tr width=1024 align=center><td>加油机节点号:%d</td></tr> </table>  \
		<table><tr  width=1024  align=center style='color:#4A3C8C;background-color:#F9F9F9;'>  \
		<td>node</td><td>fp_ad</td><td>Seq_Nb:2</td><td>Contr_Id:2</td>    \
		<td>Release_Token</td><td>Fuelling_Mode</td><td>Amount:5</td><td>Volume:5</td><td>Unit_Price:4</td>  \
		<td>Log_Noz</td><td>Prod_Nb:4</td><td>Error_Code</td><td>Security_Chksum:3</td>  \
		<td>Trans_State</td><td>Buff_Contr_Id:2</td> <td>Date_Time:7</td></tr>     ", node); 
	for(i=0,j=0;i<FP_TR_CONVERT_CNT; i++){
		transdata = (FP_TR_DATA *)&(pIfsf_Shm->fp_tr_data[i]);
		if(transdata->node == node ){
			k = strlen(strtmp);	
			j++;
			k = strlen(strtmp);	
			if( j %2 == 0)
			sprintf(&strtmp[k],  "<tr width=1024  align=center style='color:#4A3C8C;background-color:#F7F7F7;'>   \
		<td>%d</td><td>%02x</td><td>%02x%02x</td><td>%02x%02x</td><td>%d</td>   \
		<td>%d</td><td>%02x%02x%02x%02x%02x</td><td>%02x%02x%02x%02x%02x</td><td>%02x%02x%02x%02x</td>  \
		<td>%d</td><td>%02x%02x%02x%02x</td><td>%d</td><td>%02x%02x%02x</td>  \
		<td>%d</td><td>%02x%02x</td><td>%02x%02x%02x%02x%02x%02x%02x</td></tr>     " ,   transdata->node, transdata->fp_ad, \
		transdata->TR_Seq_Nb[0],   transdata->TR_Seq_Nb[1], transdata->TR_Contr_Id[0], transdata->TR_Contr_Id[1], transdata->TR_Release_Token,    \
		transdata->TR_Fuelling_Mode,  transdata->TR_Amount[0], transdata->TR_Amount[1], transdata->TR_Amount[2], transdata->TR_Amount[3],   \
		transdata->TR_Amount[4], transdata->TR_Volume[0], transdata->TR_Volume[1],  transdata->TR_Volume[2],   transdata->TR_Volume[3],  \
		transdata->TR_Volume[4],  transdata->TR_Unit_Price[0],  transdata->TR_Unit_Price[1],  transdata->TR_Unit_Price[2],   transdata->TR_Unit_Price[3], \
		transdata->TR_Log_Noz,  transdata->TR_Prod_Nb[0], transdata->TR_Prod_Nb[1], transdata->TR_Prod_Nb[2], transdata->TR_Prod_Nb[3], \
		transdata->TR_Error_Code, transdata->TR_Security_Chksum[0], transdata->TR_Security_Chksum[1], transdata->TR_Security_Chksum[2], \
		transdata->Trans_State, transdata->TR_Buff_Contr_Id[0], transdata->TR_Buff_Contr_Id[1],  transdata->TR_Date[0], transdata->TR_Date[1],  \
		transdata->TR_Date[2],  transdata->TR_Date[3], transdata->TR_Time[0],  transdata->TR_Time[1],  transdata->TR_Time[2]);
		else
			sprintf(&strtmp[k],  "<tr width=1024 align=center style='color:#4A3C8C;background-color:#E7E7FF;'>  \
		<td>%d</td><td>%02x</td><td>%02x%02x</td><td>%02x%02x</td>	<td>%d</td>   \
		<td>%d</td><td>%02x%02x%02x%02x%02x</td><td>%02x%02x%02x%02x%02x</td><td>%02x%02x%02x%02x</td>  \
		<td>%d</td><td>%02x%02x%02x%02x</td><td>%d</td><td>%02x%02x%02x</td>  \
		<td>%d</td><td>%02x%02x</td><td>%02x%02x%02x%02x%02x%02x%02x</td></tr>     " ,   transdata->node, transdata->fp_ad, \
		transdata->TR_Seq_Nb[0],   transdata->TR_Seq_Nb[1], transdata->TR_Contr_Id[0], transdata->TR_Contr_Id[1], transdata->TR_Release_Token,    \
		transdata->TR_Fuelling_Mode,  transdata->TR_Amount[0], transdata->TR_Amount[1], transdata->TR_Amount[2], transdata->TR_Amount[3],   \
		transdata->TR_Amount[4], transdata->TR_Volume[0], transdata->TR_Volume[1],  transdata->TR_Volume[2],   transdata->TR_Volume[3],  \
		transdata->TR_Volume[4],  transdata->TR_Unit_Price[0],  transdata->TR_Unit_Price[1],  transdata->TR_Unit_Price[2],   transdata->TR_Unit_Price[3], \
		transdata->TR_Log_Noz,  transdata->TR_Prod_Nb[0], transdata->TR_Prod_Nb[1], transdata->TR_Prod_Nb[2],  transdata->TR_Prod_Nb[3], \
		transdata->TR_Error_Code, transdata->TR_Security_Chksum[0], transdata->TR_Security_Chksum[1], transdata->TR_Security_Chksum[2],  \
		transdata->Trans_State, transdata->TR_Buff_Contr_Id[0], transdata->TR_Buff_Contr_Id[1],  transdata->TR_Date[0], transdata->TR_Date[1],  \
		transdata->TR_Date[2],  transdata->TR_Date[3], transdata->TR_Time[0],  transdata->TR_Time[1],  transdata->TR_Time[2]);
		}
	}
	k = strlen(strtmp);	
	sprintf(&strtmp[k], " </table>    ");
	//返回
	memcpy(ifsftrans,strtmp,strlen(strtmp)+1 );
	return 0;
}
//-----------------------------------------------------------------------------------------------
//list all data of a dispenser, but not transaction data.
//pIfsf_Shm:share memory poiter, m: which dispenser(1-64), success, return 0 and html string in dspserstat 
//-----------------------------------------------------------------------------------------------
static int  ListDispenser(const IFSF_SHM *pIfsf_Shm ,const  int node, char *dspserstat){
	char strtmp[16384];
	int i,j,k,n;
	unsigned char *tmp;
	char tmphexasc[2048];
	
	if(  (NULL == pIfsf_Shm) ||(node< 0) || (node > MAX_CHANNEL_CNT ) || (NULL == dspserstat) ){
		return -1;
	}
	
	n = node -1;
	
	//fp_c_data
	bzero( strtmp,sizeof(strtmp) );
	sprintf(strtmp, " <table> <tr width=800 align=center><td>加油机节点号:%d</td></tr> </table> \
		<table> <tr width=800 align=center style='color:#4A3C8C;background-color:#E7E7FF;'><td>  \
		fp_c_data:<br> Nb_Products; Nb_Fuelling_Modes;Nb_Meters; Nb_FP;Country_Code[2]; Blend_Tolerance;Drive_Off_Lights_Mode; \
		OPT_Light_Mode; Clear_Display_Mode;Auth_State_Mode; Stand_Alone_Auth;Max_Auth_Time; Max_Time_W_O_Prog;Min_Fuelling_Vol; Min_Display_Vol; \
		Min_Guard_Time; Pulser_Err_Tolerance;Time_Display_Product_Name; -- Digits_Vol_Layout;Digits_Amount_Layout; Digits_Unit_Price;Unit_Price_Mult_Fact; \
		Amount_Rounding_Type[2];Preset_Rounding_Amount; Price_Set_Nb[2];-- Manufacturer_Id[3]; Dispenser_Model[3];Calculator_Type[3]; \
		Calculator_Serial_No[12];Appl_Software_Ver[12]; W_M_Software_Ver[6];W_M_Software_Date[4]; W_M_Security_Type;Protocol_Ver[6]; SW_Change_Date[4]; \
		SW_Change_Personal_Nb[7]; SW_Checksum[4];-- Calc_Illumination; LCD_Backlight_Switch;Display_Intensity;-- W_M_Polynomial[2];W_M_Seed[2]  \
		</td><td width=300>    " ,node);
	tmp = (unsigned char *) &(pIfsf_Shm->dispenser_data[n].fp_c_data.Nb_Products);//
	k  = strlen(strtmp)-1;
	for(i=0; i< sizeof(FP_C_DATA); i++){
		sprintf(&strtmp[k+3*i], "%02x    ", tmp[i]);
		}
	k  = strlen(strtmp)-1;
	sprintf(&strtmp[k], " </td></tr>   " );
	
	//fp_data
	for(i=0; i<MAX_FP_CNT;i++){
		if(0 == pIfsf_Shm->dispenser_data[n].fp_data[i].FP_State ) 
			continue;		
		bzero( tmphexasc,sizeof(tmphexasc) );
		
		sprintf(tmphexasc, " <tr width=800  align=center style='color:#4A3C8C;background-color:#F7F7F7;'><td> \
			fp_data[%d]:<br> FP_Name[8]; Nb_Tran_Buffer_Not_Paid;Nb_Of_Historic_Trans; Nb_Logical_Nozzle; \
			Loudspeaker_Switch Default_Fuelling_Mode;Leak_Log_Noz_Mask; -- Drive_Off_Light_Switch; OPT_Light_Switch;-- FP_State; Log_Noz_State; \
			Assign_Contr_Id[2]; Release_Mode;ZeroTR_Mode; Log_Noz_Mask;Config_Lock[2]; Remote_Amount_Prepay[5];Remote_Volume_Preset[5];  \
			Release_Token;Fuelling_Mode; Transaction_Sequence_Nb[2];-- Current_TR_Seq_Nb[2]; Release_Contr_Id[2];Suspend_Contr_Id[2];  \
			Current_Amount[5];Current_Volume[5]; Current_Unit_Price[4];Current_Log_Noz; Current_Prod_Nb[4];Current_TR_Error_Code; Current_Average_Temp[3]; \
			Current_Price_Set_Nb[2]; -- Multi_Nozzle_Type; Multi_Nozzle_State;Multi_Nozzle_Status_Message; Local_Vol_Preset[5]; Local_Amount_Prepay[5]; \
			Running_Transaction_Message_Frequency; FP_Alarm[8]</td><td  width=300>  ",i+1 );		
		tmp = (unsigned char *)  &(pIfsf_Shm->dispenser_data[n].fp_data[i].FP_Name[0]);		
		k  = strlen(tmphexasc)-1;		
		//cgi_ok(tmphexasc);
		//exit(0);
		for(i=0; i<sizeof(FP_DATA) ; i++){//sizeof(FP_DATA)-1
			sprintf(&tmphexasc[k+3*i], "%02x    ", tmp[i]);
		}
		//cgi_ok(tmphexasc);
		//exit(0);
		k  = strlen(tmphexasc)-1;
		sprintf(&tmphexasc[k], " </td></tr>   " );
		//cgi_ok(tmphexasc);
		//exit(0);
		k  = strlen(strtmp)-1;
		memcpy(&strtmp[k], tmphexasc, strlen(tmphexasc) );
	}
	
	
	//fp_ln_data
	for(i=0; i<MAX_FP_CNT;i++){
		if(0 == pIfsf_Shm->dispenser_data[n].fp_ln_data[i][0].PR_Id) 
			continue;
		
		bzero( tmphexasc,sizeof(tmphexasc) );
		sprintf(tmphexasc, "<tr width=800 align=center style='color:#4A3C8C;background-color:#E7E7FF;'><td>  \
			fp_ln_data[%d][1]:<br>  PR_Id;Physical_Noz_Id;Meter_1_Id;--Log_Noz_Vol_Total[7];Log_Noz_Amount_Total[7]; \
			No_TR_Total[7];--Log_Noz_SA_Vol_Total[7];Log_Noz_SA_Amount_Total[7];No_TR_SA_Total[7] </td><td  width=300>  ", i+1 );
		tmp = (unsigned char *)  &(pIfsf_Shm->dispenser_data[n].fp_ln_data[i][0].PR_Id);
		k  = strlen(tmphexasc);
		for(i=0; i< sizeof(FP_LN_DATA); i++){
			sprintf(&tmphexasc[k+3*i], "%02x  ", tmp[i]);
			}
		k  = strlen(tmphexasc);
		sprintf(&tmphexasc[k], "</td></tr>  " );
		k  = strlen(strtmp);
		memcpy(&strtmp[k], tmphexasc, strlen(tmphexasc) );
	}
	//fp_pr_data
	for(i=0; i<FP_PR_CNT;i++){
		if(0 == pIfsf_Shm->dispenser_data[n].fp_pr_data[i].Prod_Nb[3]) 
			continue;		
		bzero( tmphexasc,sizeof(tmphexasc) );
		sprintf(tmphexasc, "<tr width=800 align=center style='color:#4A3C8C;background-color:#F7F7F7;'><td> \
			fp_pr_data[%d]: Prod_Nb[4]</td><td  width=300>  ", i+1 );
		tmp = (unsigned char *) &(pIfsf_Shm->dispenser_data[n].fp_pr_data[i].Prod_Nb[0]);
		k  = strlen(tmphexasc);
		for(i=0; i< sizeof(FP_PR_DATA); i++){
			sprintf(&tmphexasc[k+3*i], "%02x  ", tmp[i]);
		}
		k  = strlen(tmphexasc);
		sprintf(&tmphexasc[k], "</td></tr>  " );
		k  = strlen(strtmp);
		memcpy(&strtmp[k], tmphexasc, strlen(tmphexasc) );
	}
	//fp_fm_data
	for(i=0; i<FP_PR_NB_CNT;i++){
		if(0 == pIfsf_Shm->dispenser_data[n].fp_fm_data[i][0].Prod_Price[0]) 
			continue;	
		bzero( tmphexasc,sizeof(tmphexasc) );
		sprintf(tmphexasc, "<tr width=800  align=center style='color:#4A3C8C;background-color:#E7E7FF;'><td>  \
			fp_fm_data[%d]: Prod_Price[4]; Max_Vol[5]; Max_Fill_Time; Max_Auth_Time; User_Max_Volume[5]</td><td  width=300>  ", i+1 );
		tmp = (unsigned char *) &(pIfsf_Shm->dispenser_data[n].fp_fm_data[i][0].Prod_Price[0]);
		k  = strlen(tmphexasc);
		for(i=0; i< sizeof(FP_FM_DATA); i++){
			sprintf(&tmphexasc[k+3*i], "%02x  ", tmp[i]);
			}
		k  = strlen(tmphexasc);
		sprintf(&tmphexasc[k], "</td></tr>  " );
		k  = strlen(strtmp);
		memcpy(&strtmp[k], tmphexasc, strlen(tmphexasc) );
	}
	
	//fp_m_data
	for(i=0; i<FP_PR_NB_CNT;i++){
		if(0 == pIfsf_Shm->dispenser_data[n].fp_m_data[i].PR_Id) 
			continue;	
		bzero( tmphexasc,sizeof(tmphexasc) );
		sprintf(tmphexasc, "<tr width=800 align=center style='color:#4A3C8C;background-color:#F7F7F7;'><td>   \
			fp_m_data[%d]: Meter_Type; Meter_Puls_Vol_Fact; Meter_Calib_Fact[2];PR_Id;--Meter_Total[7]</td><td   width=300>  ", i+1 );
		tmp = (unsigned char *)  &(pIfsf_Shm->dispenser_data[n].fp_m_data[i].Meter_Type);
		k  = strlen(tmphexasc);
		for(i=0; i< sizeof(FP_M_DATA); i++){
			sprintf(&tmphexasc[k+3*i], "%02x  ", tmp[i]);
			}
		k  = strlen(tmphexasc);
		sprintf(&tmphexasc[k], "</td></tr>  " );
		k  = strlen(strtmp);
		memcpy(&strtmp[k], tmphexasc, strlen(tmphexasc) );
	}
	k  = strlen(strtmp);
	memcpy(&strtmp[k], "</table>  ", strlen( "</table>  ") );
	
	//返回值
	memcpy(dspserstat, strtmp, strlen(strtmp) );	
	 return 0;
	}

//---------------------------------------------------------------------------------------------------
//获取内存中所有加油机的状态信息,多面板的每个面板当做一个加油机给出.
//返回值: 成功为面板的总个数,失败为负值.
//---------------------------------------------------------------------------------------------------
static int  ListFpStat(const IFSF_SHM *pIfsf_Shm, char *ifsfstat){
	int i,j,k,n,z;
	unsigned int tmp;
	//unsigned long lresult;
	NODE_FP_STAT oilstat;
	char strtmp[12288],tmp22[100];
	unsigned char iszero[8];
	static char stat[STAT_CPY_LENGTH] ;

	DISPENSER_DATA *dispenser_data;
	FP_DATA *pfp_data;

	char State_Inoperative[STAT_CPY_LENGTH] = "不可用";
	char State_Closed[STAT_CPY_LENGTH] = "锁定";
	char State_Idle[STAT_CPY_LENGTH] = "空闲";
	char State_Calling[STAT_CPY_LENGTH] = "抬枪";
	char State_Authorised[STAT_CPY_LENGTH] = "已授权";
	char State_Started[STAT_CPY_LENGTH] = "启动加油";
	char State_Suspended_Started[STAT_CPY_LENGTH] = "停止加油";
	char State_Fuelling[STAT_CPY_LENGTH] = "加油中";
	char State_Suspended_Fuelling[STAT_CPY_LENGTH] = "停止加油";
	char FuelOnline[STAT_CPY_LENGTH] = "连接";
	char FuelOffline[STAT_CPY_LENGTH] = "断开";
	char TankOnline[STAT_CPY_LENGTH] ="连接";
	char TankOffline[STAT_CPY_LENGTH] ="断开";
	char CDCfgserverOnline[STAT_CPY_LENGTH] = "连接";
	char CDCfgserverOffline[STAT_CPY_LENGTH] = "断开";
	
	if(  (NULL == pIfsf_Shm) || (NULL == ifsfstat) ){
		return -1;
	}
	
	k = 0;
	bzero( strtmp,sizeof(strtmp) );
	
	if(pIfsf_Shm->auth_flag == 0){
		memcpy( stat, FuelOnline,STAT_CPY_LENGTH );
	}else if(pIfsf_Shm->auth_flag == 2){
		memcpy( stat, FuelOffline,STAT_CPY_LENGTH );
	}
	sprintf(strtmp, "<table><tr    width=1024   align=left style='color:red;background-color:#E7E7FF;font-size: 20px'>  \
		<td>* FCC与上位机连接状态:%s</td></tr></table>     ", stat);
	n  = strlen(strtmp);

	bzero( stat,sizeof(stat) );
	if(pIfsf_Shm->node_online[ MAX_CHANNEL_CNT ]== 0){
		memcpy( stat, TankOffline,STAT_CPY_LENGTH );
	}else if(pIfsf_Shm->node_online[ MAX_CHANNEL_CNT ]== 1){
		memcpy( stat, TankOnline,STAT_CPY_LENGTH );
	}
	sprintf(&strtmp[n], "<table><tr    width=1024   align=left style='color:red;background-color:#E7E7FF;font-size: 20px'>  \
		<td>* FCC与液位仪连接状态:%s</td></tr></table>     ", stat);
	n  = strlen(strtmp);
	
	//add by weihp, check if connect to CD CFG SERVER 
	bzero( stat,sizeof(stat) );
	int cdsockfd;
	//char linshi_cgibuf[200] = "";
	cdsockfd = connect_cfgsrv();
	//sprintf(linshi_cgibuf, "stat = %s , cdsockfd = %d", stat, cdsockfd);
	//cgi_ok(linshi_cgibuf);
	if (cdsockfd >= 0) {
		//cgi_ok("connect cfgsrv return > 0, before memcpy CDCfgserverOffline to stat");
		memcpy( stat, CDCfgserverOnline,STAT_CPY_LENGTH );
		close(cdsockfd);
	}else if(-1 == cdsockfd){
		//cgi_ok("connect cfgsrv return == -1, before memcpy CDCfgserverOffline to stat");
		memcpy( stat, CDCfgserverOffline,STAT_CPY_LENGTH );
	}
	//sprintf(linshi_cgibuf, "stat = %s , cdsockfd = %d", stat, cdsockfd);
	//cgi_ok(linshi_cgibuf);
	sprintf(&strtmp[n], "<table><tr    width=1024   align=left style='color:red;background-color:#E7E7FF;font-size: 20px'>  \
		<td>* FCC与配置服务器连接状态:%s</td></tr></table>     ", stat);
	n  = strlen(strtmp);

	sprintf(&strtmp[n], "<table><tr  width=1024  align=center style='color:#4A3C8C;background-color:#E7E7FF;'>  \
		<td  width=70> 节点号 </td><td width=110>背板连接号</td><td width=110>油机品牌号</td><td width=110>逻辑面板号</td>  \
		<td  width=200>面板抬起枪号(逻辑枪号)</td><td  width=140>面板状态</td></tr>"); 

	
	//cgi_ok( "in ListFpStat begin " );//<td>所在串口</td>
	//exit(1);
	for(i=0;i<MAX_CHANNEL_CNT;i++){
		if(pIfsf_Shm->node_channel[i].node != 0||pIfsf_Shm->convert_hb[i].lnao.node != 0){
			//cgi_ok( "in ListFpStat for then if " );
			//exit(1);
			for(j=0;j<MAX_FP_CNT;j++){
				//cgi_ok( "in ListFpStat for if and for" );
				//exit(1);
				if(pIfsf_Shm->node_channel[i].cntOfFps > j){
					//cgi_ok( "in ListFpStat inner if" );
					//exit(1);
					oilstat.serialPort = pIfsf_Shm->node_channel[i].serialPort;//串口号
					oilstat.node   = pIfsf_Shm->node_channel[i].node;   //加油机节点号
					oilstat.chnLog = pIfsf_Shm->node_channel[i].chnLog; //加油机后背板连接号
					oilstat.nodeDriver= pIfsf_Shm->node_channel[i].nodeDriver;//驱动程序号
					oilstat.fpPhy = j+1; //当前加油点的面板号
					oilstat.cntOfFpNoz = pIfsf_Shm->node_channel[i].cntOfFpNoz[j];//当前面板油枪数
					
					tmp = pIfsf_Shm->dispenser_data[i].fp_data[j].Log_Noz_State; //当前正在使用的油枪
					oilstat.movedNoz =0;
					if(  tmp ){
						while(tmp){
							oilstat.movedNoz++;
							tmp >>= 1;
						}
					}   		
			/*
					for(z = 0; z < MAX_CHANNEL_CNT; z++){
					       if(oilstat.node == pIfsf_Shm->convert_hb[z].lnao.node)  break; //找到了加油机节点
					}
					
					dispenser_data = (DISPENSER_DATA *)( &(pIfsf_Shm->dispenser_data[i]) );
	                             pfp_data = (FP_DATA *)(&(dispenser_data->fp_data[j]));

	                             oilstat.fpStat = pfp_data->FP_State;
                      */
			         	oilstat.fpStat= pIfsf_Shm->dispenser_data[i].fp_data[j].FP_State;//当前面板状态
					//cgi_ok( "in ListFpStat inner if before call IfsfBcd2Long" );
					//exit(1);
					bzero( stat, sizeof(stat));
					switch(oilstat.fpStat){
						case FP_State_Inoperative:
							memcpy( stat, State_Inoperative,STAT_CPY_LENGTH);
							break;
						case FP_State_Closed:
							memcpy( stat, State_Closed,STAT_CPY_LENGTH);
							break;
						case FP_State_Idle:
							memcpy( stat, State_Idle, STAT_CPY_LENGTH);
							oilstat.movedNoz = 0;
							break;
					        case FP_State_Calling:
							memcpy( stat, State_Calling, STAT_CPY_LENGTH);
							break;
						case FP_State_Authorised:
							memcpy( stat, State_Authorised, STAT_CPY_LENGTH);
							break;
						case FP_State_Started:
							memcpy( stat, State_Started, STAT_CPY_LENGTH);
							break;
						case FP_State_Suspended_Started:
							memcpy( stat, State_Suspended_Started, STAT_CPY_LENGTH);
							break;
						case FP_State_Fuelling:
							memcpy( stat, State_Fuelling, STAT_CPY_LENGTH);
							break;
						case FP_State_Suspended_Fuelling:
							memcpy( stat, State_Suspended_Fuelling, STAT_CPY_LENGTH);
							break;	
					       default:
		                                	break;
					}
					//当前面板加油量
					//memcpy( oilstat.volume, pIfsf_Shm->dispenser_data[i].fp_data[j].Current_Volume, sizeof(oilstat.volume) );
					//当前面板加油金额
					//memcpy(oilstat.amount, pIfsf_Shm->dispenser_data[i].fp_data[j].Current_Amount, sizeof(oilstat.amount) );
			/*		
					bzero(iszero, sizeof(iszero));
					if( memcmp(pIfsf_Shm->dispenser_data[i].fp_data[j].Current_Prod_Nb, iszero, 4)  != 0 ) {//当前有正在使用的油枪
						for(n=0; n<FP_PR_NB_CNT; n++){
							if(  memcmp(pIfsf_Shm->dispenser_data[i].fp_data[j].Current_Prod_Nb, \
								pIfsf_Shm->dispenser_data[i].fp_pr_data[n].Prod_Nb, 4)  == 0)
								break;
						}
						if(  n < FP_PR_NB_CNT ){
							oilstat.prodId = n+1;//油品序号
							}
						else {
							oilstat.prodId = 127;
							}
											
						if(  (0 == oilstat.prodId )  ||(oilstat.prodId  > FP_PR_NB_CNT )  ){//加油机泵码:XXXXXXXXXXX.X
							 bzero(oilstat.meter, sizeof(oilstat.meter));
							  }
						else{
							for(n=0; n < FP_M_DAT_CNT; n++){
								if(  oilstat.prodId == pIfsf_Shm->dispenser_data[i].fp_m_data[n].PR_Id)
									break;
							}
							if(FP_M_DAT_CNT  > n ){
								memcpy(oilstat.meter,pIfsf_Shm->dispenser_data[i].fp_m_data[n].Meter_Total, sizeof(oilstat.meter) );
								}
							else {
								 bzero(oilstat.meter, sizeof(oilstat.meter));
								}
							}
					}//end if memcmp
					else{ //本面板的第一条油枪
						oilstat.prodId = pIfsf_Shm->dispenser_data[i].fp_ln_data[j][0].PR_Id;	//油品序号						
						if(  (0 == oilstat.prodId )  ||(oilstat.prodId  > FP_PR_NB_CNT )   ){//加油机泵码:XXXXXXXXXXX.X
							  bzero(oilstat.meter, sizeof(oilstat.meter) );
							  }
						else{
							for(n=0; n < FP_M_DAT_CNT; n++){
								if(  oilstat.prodId == pIfsf_Shm->dispenser_data[i].fp_m_data[n].PR_Id)
									break;
							}
							if( n  < FP_M_DAT_CNT ){
								memcpy(oilstat.meter,pIfsf_Shm->dispenser_data[i].fp_m_data[n].Meter_Total, sizeof(oilstat.meter) );
								}
							else {
								  bzero(oilstat.meter, sizeof(oilstat.meter) );
								}
							}
					} //end else
			*/		
					n  = strlen(strtmp);
				      sprintf(&strtmp[n], "<tr   width=1536   align=center style='color:#4A3C8C;background-color:#F7F7F7;'>  \
						<td>%d</td><td>%d</td><td>%d</td><td>%d</td>  \
						<td>%d</td><td>%s</td>	</tr> ", \
					oilstat.node, oilstat.chnLog, oilstat.nodeDriver, oilstat.fpPhy, oilstat.movedNoz, \
					stat);
					k++; 
					//cgi_ok( "in ListFpStat inner if end" );//<td>%d</td> oilstat.serialPort,
					//exit(1);
					#if 0
					if(oilstat.node == 5){
						bzero(tmp22,sizeof(tmp22));
						sprintf(tmp22,"node5.fpstat = %d",pfp_data->FP_State);
						 cgi_error(tmp22);
					         exit(1);
					}
					#endif 
					}
				}
			}
		}
	n  = strlen(strtmp);
	sprintf(&strtmp[n], "</table><table><tr    width=1024   align=left >  \
		<td>注: 面板抬起枪号为0,表示没有油枪抬起。</td></tr></table>     ");
	n  = strlen(strtmp);
	memcpy(ifsfstat,strtmp,n);
	return k;
	}

static  unsigned char *bcd_to_asc(const unsigned char *cmd, int cmdlen, unsigned char *dest)
{
	int	i;
	memset( dest, 0, sizeof(dest) );
	for( i = 0; i < cmdlen; i++ ) {
		sprintf(dest + i * 2, "%02x", cmd[i]);
	}
	return dest;
}
float ifsf_to_float(int point, unsigned char *data)
{
	unsigned char tmp[128];
	int len;
	bzero(tmp, sizeof(tmp));
	int i , j;

	len = strlen(data);
	for(i = 0, j = 0; i < len; i ++, j++){
		if (point == i){
			tmp[j] = '.';
			j += 1;
		}
		tmp[j] = data[i];
	}

	return atof(tmp);
}
static int tlg_data(const IFSF_SHM *pIfsf_Shm, char *tlgdata)
{
	char show_data[4096];
	char *link = "连接";
	char *brk = "断开";
	int len; 
	int i;
	unsigned char Product_Level[9], Water_Level[9] ,Average_Temp[7], Total_Observed_Volume[15], Gross_Standard_Volume[15];
	unsigned char dest[128];
	if(  (NULL == pIfsf_Shm) || (NULL == tlgdata) ){
		return -1;
	}
	
	char *tmp ;
	
	tmp = "<table><tr  width=1024  align=center style='color:#4A3C8C;background-color:#E7E7FF;'>  \
		<td  width=70> 油罐号</td><td width=110>油水总高</td><td width=110>水高</td><td width=110>油温</td>  \
		<td  width=200>油水总体积</td><td  width=140>油净体积</td></tr>";
	
	sprintf(show_data, tmp); 

	tmp = "<tr   width=1536   align=center style='color:#4A3C8C;background-color:#F7F7F7;'>  \
						<td>%d</td><td>%.3f</td><td>%.3f</td><td>%.2f</td>  \
						<td>%.3f</td><td>%.3f</td>	</tr> ";

	for ( i = 0; i < pIfsf_Shm->tlg_data.tlg_dat.Nb_Tanks; i ++){
		len = strlen(show_data);

		bzero(Product_Level, sizeof(Product_Level));
		bzero(Water_Level, sizeof(Water_Level));
		bzero(Average_Temp, sizeof(Average_Temp));
		bzero(Total_Observed_Volume, sizeof(Total_Observed_Volume));
		bzero(Gross_Standard_Volume, sizeof(Gross_Standard_Volume));

		bcd_to_asc(pIfsf_Shm->tlg_data.tp_dat[i].Product_Level, 4, Product_Level);
		bcd_to_asc(pIfsf_Shm->tlg_data.tp_dat[i].Water_Level, 4, Water_Level);
		bcd_to_asc(pIfsf_Shm->tlg_data.tp_dat[i].Average_Temp, 3, Average_Temp);
		bcd_to_asc(pIfsf_Shm->tlg_data.tp_dat[i].Total_Observed_Volume, 7, Total_Observed_Volume);
		bcd_to_asc(pIfsf_Shm->tlg_data.tp_dat[i].Gross_Standard_Volume, 7, Gross_Standard_Volume);
		sprintf(&show_data[len], tmp, i + 1, \
			ifsf_to_float(5, Product_Level),\
			ifsf_to_float(5, Water_Level),\
			ifsf_to_float(pIfsf_Shm->tlg_data.tp_dat[i].Average_Temp[0], &Average_Temp[2]),\
			ifsf_to_float(pIfsf_Shm->tlg_data.tp_dat[i].Total_Observed_Volume[0], &Total_Observed_Volume[2]),\
			ifsf_to_float(pIfsf_Shm->tlg_data.tp_dat[i].Gross_Standard_Volume[0], &Gross_Standard_Volume[2]));
		//cgi_error("%d , len is %d", pIfsf_Shm->tlg_data.tp_dat[i].Gross_Standard_Volume[0], strlen(&Gross_Standard_Volume[2]));
	}

	len = strlen(show_data);
	sprintf(&show_data[len], "</table>");
	len = strlen(show_data);
	memcpy(tlgdata, show_data, len);
	
	return 0;
}
int main( int argc, char* argv[] )
{

	GunShm *pGunShm;
	IFSF_SHM *pIfsf_Shm;
	char outbuffer[24576];
	char htmlbuffer[24576];
	char sTemplateFile[]= TEMPLATE_HTML_FILE;//"cgitemplate.tpl"
	long  i, j,k,len;
	int n, ret;
	char title[64] ;
	char strtmp[12288];
	char strtmpend[12288] ;

	/*
	ret = CheckUserTime(5, NULL);
	if(ret < 0){
		moved("User time out", "/relogin.htm");
		exit(1);
	}
	ret = 1;//CheckUserTime(5, NULL);
	if(ret < 0){
		moved("User time out", "/relogin.htm");
		exit(1);
	}
	*/
	ret = InitTheKeyFile();
	if( ret < 0 )
	{
		cgi_error( "初始化Key File失败\n" );
		exit(1);
	}

	pGunShm = (GunShm*)AttachTheShm( GUN_SHM_INDEX );
	pIfsf_Shm = (IFSF_SHM*)AttachTheShm( IFSF_SHM_INDEX );
	if( (pGunShm == NULL)||(pIfsf_Shm == NULL) ){
		cgi_error( "连接共享内存失败\n" );
		exit(1);
	}
	
	//尾部的转到连接
/*
	bzero(strtmpend,sizeof(strtmpend));
	sprintf(strtmpend,"<table><tr width=1024  align=left><td>&nbsp;</td></tr><tr  width=1024  align=left>  \
		<td>附加功能:</td></tr></table><table   align=left ><tr  width=1024  align=left >  \
		<td>油机数据</td>     ");
	for(i=0; i<MAX_CHANNEL_CNT; i++){		
		if(pIfsf_Shm->node_channel[i].node != 0){
			j = strlen(strtmpend)-1;
			sprintf(&strtmpend[j], "<td><a  href=\"/cgi-bin/cgidumpshm?node=%d\" >%d</a></td>   ",   \
				pIfsf_Shm->node_channel[i].node,pIfsf_Shm->node_channel[i].node );}
	}
	j = strlen(strtmpend)-1;
	sprintf(&strtmpend[j],"</tr> <tr  width=1024  align=left >  \
		<td>交易数据</td>     ");
	for(i=0; i<MAX_CHANNEL_CNT; i++){		
		if(pIfsf_Shm->node_channel[i].node != 0){
			j = strlen(strtmpend)-1;
			sprintf(&strtmpend[j], "<td><a  href=\"/cgi-bin/cgidumpshm?trnode=%d\" >%d</a></td>   ",   \
				pIfsf_Shm->node_channel[i].node,pIfsf_Shm->node_channel[i].node );}
	}
	j = strlen(strtmpend)-1;
	sprintf(&strtmpend[j],"</tr> </table>      ");
*/
	//显示内容
	bzero(title,sizeof(title));
	bzero(strtmp,sizeof(strtmp));
	k = getCgiInput();
	if(k < 0 ){
		cgi_error("cgi argument no. is negtive, error!!");
		//return -2;
		exit(1);
		}
	else if( (k>= 1)&&( memcmp(nv[0].name, "trnode", 6) ==0 )   ){
		i = atoi(nv[0].value);
		if( (i < 0) ||(i >255))
			i =1;	
		ret = ListMemTrans(pIfsf_Shm, i, strtmp);
		if(ret <0)
			cgi_error("ListMemTrans error!!");
		j = strlen(strtmp);
		memcpy(&strtmp[j], strtmpend, strlen(strtmpend) );	
		char returl[]="/cgi-bin/cgidumpshm";
		sprintf(title, "状态浏览---加油机交易数据     ");
		refreshshm( title, strtmp,  returl);		
	}
	else if( (k>= 1)&&( memcmp(nv[0].name, "node", 4) ==0 )   ){
		i = atoi(nv[0].value);
		if( (i < 0) ||(i >255) )
			i =1;	
		ret = ListDispenser(pIfsf_Shm, i, strtmp);
		if(ret <0)
			cgi_error(" ListDispenser error!!");
		j = strlen(strtmp);
		memcpy(&strtmp[j], strtmpend, strlen(strtmpend) );	
		char returl[]="/cgi-bin/cgidumpshm";
		sprintf(title, "状态浏览---加油机数据   ");
		refreshshm( title, strtmp,  returl);
		
	}	
	else if(   (0 == k) || !( ( memcmp(nv[0].name, "node", 4) ==0 ) ||( memcmp(nv[1].name, "trnode", 6) ==0 ) )   ){
		ret = ListFpStat(pIfsf_Shm, strtmp);
		j = strlen(strtmp);
		tlg_data(pIfsf_Shm, &strtmp[j]);
		j = strlen(strtmp);
		memcpy(&strtmp[j], strtmpend, strlen(strtmpend) );
		
		sprintf(title, "状态浏览---加油机面板状态   ");
		bzero(htmlbuffer,sizeof(htmlbuffer));
		ret =ReadTextFile( sTemplateFile, htmlbuffer);
		if(ret <0){
			cgi_error("ReadTextFile error when ListFpStat");
			exit(1);
		}
		//cgi_ok(strtmp);
		//exit(0);
		bzero(outbuffer,sizeof(outbuffer));
		i=0;
		j=0;
		k=0;
		while(   j < (sizeof(outbuffer)-1)    ){
			outbuffer[j] = htmlbuffer[i];			
			if( (htmlbuffer[i] == '%' ) &&(htmlbuffer[i+1] == 's')){
				k++;
				if(1 ==k){
					sprintf(&outbuffer[j],title);
					j += strlen(title)-1;
					i++;
				}
				else if(2 == k){
					memcpy(&outbuffer[j],strtmp,strlen(strtmp));
					j += strlen(strtmp)-1;
					i++;
				}
				
			}
			if( (htmlbuffer[i] == '<' ) &&(htmlbuffer[i+1] == '/')&&(htmlbuffer[i+2] == 't')&&(htmlbuffer[i+3] == 'i')  \
				&&(htmlbuffer[i+4] == 't')&&(htmlbuffer[i+5] == 'l')&&(htmlbuffer[i+6] == 'e')){
					
					sprintf(&outbuffer[j],"</title><meta  http-equiv=refresh content=3>    ");
					j = strlen(outbuffer)-1;
					i +=8;								
			}
			i++;
			j++;		
		}	
		(void) printf( outbuffer);
	}
	
	else {
		cgi_error("cgi argument is error!!");
	}
	
	
	
	//ret = ListFpStat(pIfsf_Shm, strtmp);//(unsigned char *)&(oilstat[0].serialPort)
	//ret = ListDispenser(pIfsf_Shm,1, strtmp);
	//ret = ListMemTrans(pIfsf_Shm, 1, strtmp);
	
	//cgi_disp(strtmp);
	//cgi_refresh(strtmp);
	//refreshshm("加油机状态", strtmp, "/cgi-bin/cgidumpshm");
	DetachTheShm((char**)(&pGunShm));
	DetachTheShm((char**)(&pIfsf_Shm));
	exit(0);	
	return 0;
}

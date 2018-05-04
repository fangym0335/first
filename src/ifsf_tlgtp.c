
#include  "ifsf_tlg.h"
#include  "ifsf_tlgtlg.h"
#include  "ifsf_tlgtp.h"
#include "ifsf_pub.h"
//#include  "ifsf.h"

extern IFSF_SHM *gpIfsf_shm; //该结构是全局的量，只有一个,在ifsf_main.h中定义.

/*:TANK PROBE DATABASE  DB_Ad = TP_ID (21H-3FH)
Data_Id:
01H=TP_Manufacturer_Id
02H=TP_Type
03H=TP_Serial_Nb
04H=TP_Model
05H=TP_Appl_Software_Ver
06H=Prod_Nb
07H=Prod_Description
08H=Prod_Group_Code
09H=Ref_Density
0aH=Tank_Diameter
0bH=Shell_Capacity
0cH=Max_Safe_Fill_Capacity
0dH=Low_Capacity
0eH=Min_Operating_Capacity
0fH=HiHi_Level_Setpoint

10H=Hi_Level_Setpoint
11H=Lo_Level_Setpoint
12H=LoLo_Level_Setpoint
13H=Hi_Water_Setpoint
14H=Water_Detection_Thresh
15H=Tank_Tilt_Offset
16H=Tank_Manifold_Partners
17H=TP_Measurement_Units
20H=TP_Status
21H=TP_Alarm

TANK READING:
Data_Id ：
40H=Product_Level
41H=Total_Observed_Volume
42H=Gross_Standard_Volume
43H=Average_Temp
44H=Water_Level
O - These are individual optional elements that are not part of an identified group,
including some that are only necessary for certain types of gauges.
O1 - This group identifies gauges that are capable of timekeeping and record
dating.
O2 - This group identifies gauges that have implemented the fuel and water height
threshold alarms.
O3 - This group identifies gauges that are able to calculate the volume of the
contents of the tank.
O4 - This group identifies gauges that have implemented the volume threshold
alarms.
O5 - This group identifies gauges that are capable of measuring fuel temperatures
and calculating temperature compensated volumes.

enum TP_DAT_AD{
	T_TP_Manufacturer_Id=1, T_TP_Type, T_TP_Serial_Nb, T_TP_Model, T_TP_Appl_Software_Ver, T_Prod_Nb, T_Prod_Description,
	T_Prod_Group_Code, T_Ref_Density, T_Tank_Diameter, T_Shell_Capacity, T_Max_Safe_Fill_Capacity, T_Low_Capacity,
	T_Min_Operating_Capacity, T_HiHi_Level_Setpoint, T_Hi_Level_Setpoint = 0x10, T_Lo_Level_Setpoint, T_LoLo_Level_Setpoint,
	T_Hi_Water_Setpoint, T_Water_Detection_Thresh, T_Tank_Tilt_Offset, T_Tank_Manifold_Partners, T_TP_Measurement_Units,
	T_TP_Status=0x20, T_TP_Alarm = 0x21,
	T_Product_Level=0x40, T_Total_Observed_Volume, T_Gross_Standard_Volume, T_Average_Temp, T_Water_Level,
	T_Observed_Density =0x45, T_Last_Reading_Date = 0x46, T_Last_Reading_Time = 0x47,
	T_TP_Status_Message=100  //UNSOLICITED DATA
};
typedef struct{	
	unsigned char TP_Manufacturer_Id[3];
	unsigned char TP_Type[3];
	unsigned char TP_Serial_Nb[12];
	unsigned char TP_Model[3];
	unsigned char TP_Appl_Software_Ver[12];
	unsigned char Prod_Nb[4];//O
	unsigned char Prod_Description[16];
	unsigned char Prod_Group_Code;//O5
	unsigned char Ref_Density[2];//O. in kilograms per cubicmeter at reference temperature
	unsigned char Tank_Diameter[4];//O3. BCD8
	unsigned char Shell_Capacity[7];//O3. BIN8+BCD12. 
	unsigned char Max_Safe_Fill_Capacity[7];//O4. BIN8+BCD12:0a00...
	unsigned char Low_Capacity[7];//O4. BIN8+BCD12:0a00... 
	unsigned char Min_Operating_Capacity[7];//O4. BIN8+BCD12:0a00... 
	unsigned char HiHi_Level_Setpoint[4];//O2. Metric level would be reported in 0.001 mm (one thousandthof a mm).
	unsigned char Hi_Level_Setpoint[4];//O2. Metric level would be reported in 0.001 mm (one thousandthof a mm).
	unsigned char Lo_Level_Setpoint[4];//O2. Metric level would be reported in 0.001 mm (one thousandthof a mm).
	unsigned char LoLo_Level_Setpoint[4];//O2. Metric level would be reported in 0.001 mm (one thousandthof a mm).
	unsigned char Hi_Water_Setpoint[4];//O2. Metric level would be reported in 0.001 mm (one thousandthof a mm).
	unsigned char Water_Detection_Thresh[4];//O2. Metric level would be reported in 0.001 mm (one thousandthof a mm).
	unsigned char Tank_Tilt_Offset[4];//O. Metric level would be reported in 0.001 mm (one thousandthof a mm).
	unsigned char Tank_Manifold_Partners[4]//O. BCD8. siphon manifolds.
	unsigned char TP_Measurement_Units;//O.0 – metric
	unsigned char TP_Status;//(1-3)
	unsigned char TP_Alarm[2];//(0-65535)
	unsigned char Product_Level[4];//M. Metric level would be reported in 0.001 mm (one thousandthof a mm).
	unsigned char Total_Observed_Volume[7];//O3. BIN8+BCD12:0a00...  
	unsigned char Gross_Standard_Volume[7];//O5. BIN8+BCD12:0a00... 
	unsigned char Average_Temp[4];//O5. BIN8+BCD6.
	unsigned char Water_Level[4];//M. Metric level would be reported in 0.001 mm (one thousandthof a mm).	

	unsigned char Observed_Density[2];//O. in kilograms per cubicmeter at reference temperature. BIN16(0-65535)
	unsigned char Last_Reading_Date[4];
	unsigned char Last_Reading_Time[3];
} TP_DAT;
*/
//-
//默认初始化液位仪TP_DAT.	
//-
int DefaultTP_DAT(const char node, const char tp_ad){
	int tp;//i, 
	TLG_DATA *pTlg_data;
	TLG_DAT *pTlg_dat;
	TP_DAT  *pTp_dat;
	
	if( NULL == gpIfsf_shm ){		
		printf("共享内存值错误,液位仪TP初始化失败");
		return -1;
	}
	if( (node <= 0) || (node > 127 )  ){//||(node != gpIfsf_shm->node_tlg)
		printf("液位仪节点值错误,液位仪TP初始化 失败");
		return -1;
	}
	if( (tp_ad <0x21) || (tp_ad > 0x3f ) ){
		printf("液位仪节点TP值错误,液位仪TP初始化 失败");
		return -1;
	}
	tp = tp_ad - 0x21;
	
	pTlg_data = (TLG_DATA*)( &(gpIfsf_shm->tlg_data) );
	pTlg_dat = (TLG_DAT*)( &(pTlg_data->tlg_dat) );
	pTp_dat =  (TP_DAT *)( &(pTlg_data->tp_dat[tp]) );
	bzero(pTp_dat, sizeof(pTp_dat));
	strncpy(pTp_dat->TP_Manufacturer_Id, "PTR", 3);
	strncpy(pTp_dat->TP_Type, "PTR", 3);
	memset(pTp_dat->TP_Serial_Nb, 0x20, 12);  // set to space
	strncpy(pTp_dat->TP_Model, "PTR", 3);
	bzero(pTp_dat->TP_Appl_Software_Ver, sizeof(pTp_dat->TP_Appl_Software_Ver));
	strncpy(pTp_dat->TP_Appl_Software_Ver, "TLGCSS", 6);
	pTp_dat->TP_Appl_Software_Ver[10] = 0x01;
	pTp_dat->TP_Appl_Software_Ver[11] = 0x00; //V1.00
	//pTp_dat->Prod_Nb
	//pTp_dat->Prod_Description
	//pTp_dat->Prod_Group_Code
	//pTp_dat->Ref_Density
	//pTp_dat->Tank_Diameter
	//pTp_dat->Shell_Capacity
	//pTp_dat->Max_Safe_Fill_Capacity
	//pTp_dat->Low_Capacity
	//pTp_dat->Min_Operating_Capacity
	//pTp_dat->HiHi_Level_Setpoint
	//pTp_dat->Hi_Level_Setpoint
	//pTp_dat->Lo_Level_Setpoint
	//pTp_dat->LoLo_Level_Setpoint
	//pTp_dat->Hi_Water_Setpoint
	//pTp_dat->Water_Detection_Thresh
	//pTp_dat->Tank_Tilt_Offset
	bzero(pTp_dat->Tank_Manifold_Partners,sizeof(pTp_dat->Tank_Manifold_Partners) ); //no siphon Manifolders in china, read only.
	pTp_dat->TP_Measurement_Units = 0; //O.    0 – metric
	pTp_dat->TP_Status = 0x01;
	//pTp_dat->TP_Alarm
	//pTp_dat->Product_Level
	//pTp_dat->Total_Observed_Volume
	//pTp_dat->Gross_Standard_Volume
	//pTp_dat->Average_Temp
	//pTp_dat->Water_Level
	//T_Observed_Density
	//T_Last_Reading_Date
	//T_Last_Reading_Time

	return 0;
}
//-
//设置液位仪.根据型号设置.
//-
int SetTP_DAT(const char node, const char tp_ad, const TP_DAT *tp_dat){
	int tp;//i, 
	TLG_DATA *pTlg_data;
	TLG_DAT *pTlg_dat;
	TP_DAT  *pTp_dat;
	
	if( NULL == gpIfsf_shm ){		
		printf("共享内存值错误,液位仪SetTLG_DAT失败");
		return -1;
	}
	if( NULL == tp_dat ){		
		printf("参数值错误,液位仪SetTLG_DAT失败");
		return -1;
	}
	if( (node <= 0) || (node > 127 ) ){ //||(node != gpIfsf_shm->node_tlg)
		printf("液位仪节点值错误,液位仪SetTLG_DAT 失败");
		return -1;
	}
	if( (tp_ad <0x21) || (tp_ad > 0x3f ) ){
		printf("液位仪节点TP值错误,液位仪TP初始化 失败");
		return -1;
	}
	tp = tp_ad - 0x21;

	pTlg_data = (TLG_DATA*)( &(gpIfsf_shm->tlg_data) );
	pTlg_dat = (TLG_DAT*)( &(pTlg_data->tlg_dat) );
	pTp_dat =  (TP_DAT *)( &(pTlg_data->tp_dat[tp]) );
	bzero(pTp_dat, sizeof(pTp_dat));
	memcpy(pTp_dat, tp_dat, sizeof(TP_DAT));
	return 0;
}
//-
//读写TP_DATA函数. 注意要构造的信息数据的类型,000读,001回答,010写,011主动需应答,100主动无需应答,111应答
//recvBuffer为收到的data,sendBuffer为要发送的data,msg_lg为生成的总长度.
//-
//-
//读取TP_DAT.TLG_DAT地址为1.构造的信息数据的类型为001回答
//-
int ReadTP_DAT(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg){
	int i, recv_lg, tmp_lg = 0;
	int tp;
	int tp_stat;
	TLG_DATA *pTlg_data;
	TLG_DAT *pTlg_dat;
	TP_DAT  *pTp_dat;
	unsigned char node;
	unsigned char buffer[256];
	
	if( NULL == gpIfsf_shm ){		
		printf("共享内存值错误,液位仪ReadTP_DAT失败");
		return -1;
	}
	if( (NULL == recvBuffer) || (NULL == sendBuffer) || (NULL == msg_lg)  ){		
		printf("参数错误,液位仪ReadTP_DAT 失败");
		return -1;
	}
	
	node= recvBuffer[1];
	tp = recvBuffer[9] - 0x21;
	pTlg_data = (TLG_DATA*)( &(gpIfsf_shm->tlg_data) );
	pTlg_dat = (TLG_DAT*)( &(pTlg_data->tlg_dat) );
	pTp_dat =  (TP_DAT *)( &(pTlg_data->tp_dat[tp]) );
	tp_stat = pTp_dat->TP_Status;
	
	bzero(buffer, sizeof(buffer));
	if(  '\x09' != recvBuffer[0]  ){
		printf("参数错误,不是液位仪的数据,液位仪ReadTP_DAT 失败");
		return -1;
	}
	
	if( (node <= 0) || (node > 127 )  ){//||(node != gpIfsf_shm->node_tlg)
		printf("液位仪节点值错误,液位仪ReadTP_DAT 失败");
		return -1;
	}
	
	
	buffer[0] = recvBuffer[2];
	buffer[1] = recvBuffer[3];
	buffer[2] = recvBuffer[0];
	buffer[3] = recvBuffer[1];
	buffer[4] = '\0'; //recvBuffer[4];
	buffer[5] = ( (recvBuffer[5] &0x1f)  |0x20); //answer
	recv_lg = recvBuffer[6];
	recv_lg = recv_lg * 256 + recvBuffer[7];
	buffer[8] = '\x01'; //db_ad_lg. recvBuffer[8];
	buffer[9] = recvBuffer[9];//  db_ad.//'\x01';
	//buffer[10] = recvBuffer[10]; //It's Data_Id. Data begin.
	tmp_lg = 9;
	for(i=0; i < (recv_lg - 2); i++){
		switch( recvBuffer[10+i] ){
			case T_TP_Manufacturer_Id:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_TP_Manufacturer_Id;
				tmp_lg++;
				buffer[tmp_lg] =3;
				tmp_lg++;
				memcpy( &buffer[tmp_lg] , pTp_dat->TP_Manufacturer_Id, sizeof(pTp_dat->TP_Manufacturer_Id) );
				tmp_lg += sizeof(pTp_dat->TP_Manufacturer_Id) -1;
				break;
			case T_TP_Type: 
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_TP_Type;
				tmp_lg++;
				buffer[tmp_lg] =3;
				tmp_lg++;
				buffer[tmp_lg] = pTp_dat->TP_Type[0];
				tmp_lg++;
				buffer[tmp_lg] = pTp_dat->TP_Type[1];
				tmp_lg++;
				buffer[tmp_lg] = pTp_dat->TP_Type[2];
				break;
			case T_TP_Serial_Nb:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_TP_Serial_Nb;
				tmp_lg++;
				buffer[tmp_lg] ='\x12';
				tmp_lg++;
				memcpy( &buffer[tmp_lg] , pTp_dat->TP_Serial_Nb, sizeof(pTp_dat->TP_Serial_Nb) );
				tmp_lg += sizeof(pTp_dat->TP_Serial_Nb) -1;
				break;
			case T_TP_Model:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_TP_Model;
				tmp_lg++;
				buffer[tmp_lg] =3;
				tmp_lg++;
				buffer[tmp_lg] = pTp_dat->TP_Model[0];
				tmp_lg++;
				buffer[tmp_lg] = pTp_dat->TP_Model[1];
				tmp_lg++;
				buffer[tmp_lg] = pTp_dat->TP_Model[2];
				break;			
			case T_TP_Appl_Software_Ver:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_TP_Appl_Software_Ver;
				tmp_lg++;
				buffer[tmp_lg] ='\x12';
				tmp_lg++;
				memcpy( &buffer[tmp_lg] , pTp_dat->TP_Appl_Software_Ver, sizeof(pTp_dat->TP_Appl_Software_Ver) );
				tmp_lg += sizeof(pTp_dat->TP_Appl_Software_Ver) -1;
				break;
			case T_Prod_Nb:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Prod_Nb;
				tmp_lg++;
				buffer[tmp_lg] =4;
				tmp_lg++;
				buffer[tmp_lg] = pTp_dat->Prod_Nb[0];
				tmp_lg++;
				buffer[tmp_lg] = pTp_dat->Prod_Nb[1];
				tmp_lg++;
				buffer[tmp_lg] = pTp_dat->Prod_Nb[2];
				tmp_lg++;
				buffer[tmp_lg] = pTp_dat->Prod_Nb[3];
				break;
			case T_Prod_Description:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Prod_Description;
				tmp_lg++;
				buffer[tmp_lg] ='\x16';
				tmp_lg++;
				memcpy( &buffer[tmp_lg] , pTp_dat->Prod_Description, sizeof(pTp_dat->Prod_Description) );
				tmp_lg += sizeof(pTp_dat->Prod_Description) -1;
				break;
			case T_Prod_Group_Code:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Prod_Group_Code;
				tmp_lg++;
				buffer[tmp_lg] =1;
				tmp_lg++;
				buffer[tmp_lg] = pTp_dat->Prod_Group_Code;
				break;
			case T_Ref_Density:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Ref_Density;
				tmp_lg++;
				buffer[tmp_lg] =2;
				tmp_lg++;
				buffer[tmp_lg] = pTp_dat->Ref_Density[0];//bin16
				tmp_lg++;
				buffer[tmp_lg] = pTp_dat->Ref_Density[1];
				break;
			case T_Tank_Diameter:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Tank_Diameter;
				tmp_lg++;
				buffer[tmp_lg] =4;
				tmp_lg++;
				memcpy( &buffer[tmp_lg], pTp_dat->Tank_Diameter, sizeof(pTp_dat->Tank_Diameter) );
				tmp_lg += sizeof(pTp_dat->Tank_Diameter) -1;
				break;
			case T_Shell_Capacity:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Shell_Capacity;
				tmp_lg++;
				buffer[tmp_lg] =7;
				tmp_lg++;
				memcpy( &buffer[tmp_lg], pTp_dat->Shell_Capacity, sizeof(pTp_dat->Shell_Capacity) );
				tmp_lg += sizeof(pTp_dat->Shell_Capacity) -1;
				break;
			case T_Max_Safe_Fill_Capacity:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Max_Safe_Fill_Capacity;
				tmp_lg++;
				buffer[tmp_lg] =7;
				tmp_lg++;
				memcpy( &buffer[tmp_lg], pTp_dat->Max_Safe_Fill_Capacity, sizeof(pTp_dat->Max_Safe_Fill_Capacity) );
				tmp_lg += sizeof(pTp_dat->Max_Safe_Fill_Capacity) -1;
				break;
			case T_Low_Capacity:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Low_Capacity;
				tmp_lg++;
				buffer[tmp_lg] =7;
				tmp_lg++;
				memcpy( &buffer[tmp_lg], pTp_dat->Low_Capacity, sizeof(pTp_dat->Low_Capacity) );
				tmp_lg += sizeof(pTp_dat->Low_Capacity) -1;
				break;
			case T_Min_Operating_Capacity:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Min_Operating_Capacity;
				tmp_lg++;
				buffer[tmp_lg] =7;
				tmp_lg++;
				memcpy( &buffer[tmp_lg], pTp_dat->Min_Operating_Capacity, sizeof(pTp_dat->Min_Operating_Capacity) );
				tmp_lg += sizeof(pTp_dat->Min_Operating_Capacity) -1;
				break;
			case T_HiHi_Level_Setpoint:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_HiHi_Level_Setpoint;
				tmp_lg++;
				buffer[tmp_lg] =4;
				tmp_lg++;
				memcpy( &buffer[tmp_lg], pTp_dat->HiHi_Level_Setpoint, sizeof(pTp_dat->HiHi_Level_Setpoint) );
				tmp_lg += sizeof(pTp_dat->HiHi_Level_Setpoint) -1;
				break;
			case T_Hi_Level_Setpoint:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Hi_Level_Setpoint;
				tmp_lg++;
				buffer[tmp_lg] =4;
				tmp_lg++;
				memcpy( &buffer[tmp_lg], pTp_dat->Hi_Level_Setpoint, sizeof(pTp_dat->Hi_Level_Setpoint) );
				tmp_lg += sizeof(pTp_dat->Hi_Level_Setpoint) -1;
				break;
			case T_Lo_Level_Setpoint:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Lo_Level_Setpoint;
				tmp_lg++;
				buffer[tmp_lg] =4;
				tmp_lg++;
				memcpy( &buffer[tmp_lg], pTp_dat->Lo_Level_Setpoint, sizeof(pTp_dat->Lo_Level_Setpoint) );
				tmp_lg += sizeof(pTp_dat->Lo_Level_Setpoint) -1;
				break;
			case T_LoLo_Level_Setpoint:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_LoLo_Level_Setpoint;
				tmp_lg++;
				buffer[tmp_lg] =4;
				tmp_lg++;
				memcpy( &buffer[tmp_lg], pTp_dat->LoLo_Level_Setpoint, sizeof(pTp_dat->LoLo_Level_Setpoint) );
				tmp_lg += sizeof(pTp_dat->LoLo_Level_Setpoint) -1;
				break;
			case T_Hi_Water_Setpoint:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Hi_Water_Setpoint;
				tmp_lg++;
				buffer[tmp_lg] =4;
				tmp_lg++;
				memcpy( &buffer[tmp_lg], pTp_dat->Hi_Water_Setpoint, sizeof(pTp_dat->Hi_Water_Setpoint) );
				tmp_lg += sizeof(pTp_dat->Hi_Water_Setpoint) -1;
				break;
			case T_Water_Detection_Thresh:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Water_Detection_Thresh;
				tmp_lg++;
				buffer[tmp_lg] =4;
				tmp_lg++;
				memcpy( &buffer[tmp_lg], pTp_dat->Water_Detection_Thresh, sizeof(pTp_dat->Water_Detection_Thresh) );
				tmp_lg += sizeof(pTp_dat->Water_Detection_Thresh) -1;
				break;
			case T_Tank_Tilt_Offset:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Tank_Tilt_Offset;
				tmp_lg++;
				buffer[tmp_lg] =4;
				tmp_lg++;
				memcpy( &buffer[tmp_lg], pTp_dat->Tank_Tilt_Offset, sizeof(pTp_dat->Tank_Tilt_Offset) );
				tmp_lg += sizeof(pTp_dat->Tank_Tilt_Offset) -1;
				break;
			case T_Tank_Manifold_Partners:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Tank_Manifold_Partners;
				tmp_lg++;
				buffer[tmp_lg] =4;
				tmp_lg++;
				memcpy( &buffer[tmp_lg], pTp_dat->Tank_Manifold_Partners, sizeof(pTp_dat->Tank_Manifold_Partners) );
				tmp_lg += sizeof(pTp_dat->Tank_Manifold_Partners) -1;
				break;
			case T_TP_Measurement_Units:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_TP_Measurement_Units;
				tmp_lg++;
				buffer[tmp_lg] =1;
				tmp_lg++;
				buffer[tmp_lg] = pTp_dat->TP_Measurement_Units;
				break;
			case T_TP_Status:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_TP_Status;
				tmp_lg++;
				buffer[tmp_lg] =1;
				tmp_lg++;
				buffer[tmp_lg] = pTp_dat->TP_Status;
				break;
			case T_TP_Alarm:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_TP_Alarm;
				tmp_lg++;
				buffer[tmp_lg] =1;
				tmp_lg++;
				buffer[tmp_lg] = pTp_dat->TP_Alarm[0];
				tmp_lg++;
				buffer[tmp_lg] = pTp_dat->TP_Alarm[1];
				break;
			//follow r(2)
			case T_Product_Level:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Product_Level;
//				RunLog("ReadTP_DATA.case PR_Level, tp_stat: %d", tp_stat);
				if(tp_stat !=  2){
					tmp_lg++;
					buffer[tmp_lg] ='\0';
					break;
				}
				tmp_lg++;
				buffer[tmp_lg] =4;
				tmp_lg++;
				memcpy( &buffer[tmp_lg], pTp_dat->Product_Level, sizeof(pTp_dat->Product_Level) );
//				RunLog("PR_Level: %s", Asc2Hex(&buffer[tmp_lg], sizeof(pTp_dat->Product_Level)));
				tmp_lg += sizeof(pTp_dat->Product_Level) - 1;
				break;
			case T_Total_Observed_Volume:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Total_Observed_Volume;
				if(tp_stat  !=  2){
					tmp_lg++;
					buffer[tmp_lg] ='\0';
					break;
				}
				tmp_lg++;
				buffer[tmp_lg] =7;
				tmp_lg++;
#ifdef TANK_DEBUG
				RunLog("Total Observed Volume is %02x%02x%02x%02x", pTp_dat->Total_Observed_Volume[3],\
				pTp_dat->Total_Observed_Volume[4],\
				pTp_dat->Total_Observed_Volume[5],\
				pTp_dat->Total_Observed_Volume[6]);
				RunLog("Total_Observed_Volume ' address is %p", pTp_dat->Total_Observed_Volume);
#endif
				memcpy( &buffer[tmp_lg], pTp_dat->Total_Observed_Volume,
							sizeof(pTp_dat->Total_Observed_Volume) );

#ifdef TANK_DEBUG
				RunLog("Total Observed Volume is %02x%02x%02x%02x", pTp_dat->Total_Observed_Volume[3],\
				pTp_dat->Total_Observed_Volume[4],\
				pTp_dat->Total_Observed_Volume[5],\
				pTp_dat->Total_Observed_Volume[6]);
				RunLog("Total_Observed_Volume ' address is %p %p %p", pTp_dat->Total_Observed_Volume, pTlg_data, gpIfsf_shm);
#endif
				tmp_lg += sizeof(pTp_dat->Total_Observed_Volume) -1;
				break;
			case T_Gross_Standard_Volume:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Gross_Standard_Volume;
				if(tp_stat  !=  2){
					tmp_lg++;
					buffer[tmp_lg] ='\0';
					break;
				}
				tmp_lg++;
				buffer[tmp_lg] =7;
				tmp_lg++;
				memcpy( &buffer[tmp_lg], pTp_dat->Gross_Standard_Volume, sizeof(pTp_dat->Gross_Standard_Volume) );
				tmp_lg += sizeof(pTp_dat->Gross_Standard_Volume) -1;
				break;
			case T_Average_Temp:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Average_Temp;
				if(tp_stat  !=  2){
					tmp_lg++;
					buffer[tmp_lg] ='\0';
					break;
				}
				tmp_lg++;
				buffer[tmp_lg] =3;
				tmp_lg++;
				memcpy( &buffer[tmp_lg], pTp_dat->Average_Temp, sizeof(pTp_dat->Average_Temp) );
				tmp_lg += sizeof(pTp_dat->Average_Temp) -1;
				break;
			case T_Water_Level:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Water_Level;
				if(tp_stat  !=  2){
					tmp_lg++;
					buffer[tmp_lg] ='\0';
					break;
				}
				tmp_lg++;
				buffer[tmp_lg] =4;
				tmp_lg++;
				memcpy( &buffer[tmp_lg], pTp_dat->Water_Level, sizeof(pTp_dat->Water_Level) );
				tmp_lg += sizeof(pTp_dat->Water_Level) -1;
				break;
			case T_Observed_Density:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Observed_Density;
				if(tp_stat  !=  2){
					tmp_lg++;
					buffer[tmp_lg] ='\0';
					break;
				}
				tmp_lg++;
				buffer[tmp_lg] =2;
				tmp_lg++;
				buffer[tmp_lg] = pTp_dat->Observed_Density[0];//bin16
				tmp_lg++;
				buffer[tmp_lg] = pTp_dat->Observed_Density[1];
				break;
			case T_Last_Reading_Date:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Last_Reading_Date;
				if(tp_stat  !=  2){
					tmp_lg++;
					buffer[tmp_lg] ='\0';
					break;
				}
				tmp_lg++;
				buffer[tmp_lg] =4;
				tmp_lg++;
				memcpy( &buffer[tmp_lg], pTp_dat->Last_Reading_Date, sizeof(pTp_dat->Last_Reading_Date) );
				tmp_lg += sizeof(pTp_dat->Last_Reading_Date) -1;
				break;
			case T_Last_Reading_Time:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Last_Reading_Time;
				if(tp_stat  !=  2){
					tmp_lg++;
					buffer[tmp_lg] ='\0';
					break;
				}
				tmp_lg++;
				buffer[tmp_lg] =3;
				tmp_lg++;
				memcpy( &buffer[tmp_lg], pTp_dat->Last_Reading_Time, sizeof(pTp_dat->Last_Reading_Time) );
				tmp_lg += sizeof(pTp_dat->Last_Reading_Time) -1;
				break;
			default:
				tmp_lg++;
				buffer[tmp_lg] =recvBuffer[i+10] ;
				tmp_lg++;
				buffer[tmp_lg] ='\0';
				break;
		}
	}
	tmp_lg++;
	buffer[6] = (unsigned char )( (tmp_lg-8) / 256) ;
	buffer[7] = (unsigned char )((tmp_lg-8) %256);  //big end.
	*msg_lg= tmp_lg;
	memcpy(sendBuffer, buffer, tmp_lg);

	return 0;
}
//-
//写TP_DAT函数. 构造的信息数据的类型为111应答.
//msg_lg为生成的总长度,为0表示不需要应答信息,sendBuffer和msg_lg不需要应答可以设置为NULL,.
//-
int WriteTP_DAT(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg){
	int i, recv_lg, tmp_lg = 0, lg;
	int tp;
	int tp_stat;
	unsigned char tmpMSAck, tmpDataAck;
	TLG_DATA *pTlg_data;
	TLG_DAT *pTlg_dat;
	TP_DAT  *pTp_dat;
	unsigned char node;
	unsigned char buffer[256];
	
	if( NULL == gpIfsf_shm ){		
		printf("共享内存值错误,液位仪WriteTP_DAT失败");
		return -1;
	}
	if( (NULL == recvBuffer) || (NULL == sendBuffer) || (NULL == msg_lg)  ){		
		printf("参数错误,液位仪WriteTP_DAT 失败");
		return -1;
	}
	
	node = recvBuffer[1];
	tp = recvBuffer[9] - 0x21;
	pTlg_data = (TLG_DATA*)( &(gpIfsf_shm->tlg_data) );
	pTlg_dat = (TLG_DAT*)( &(pTlg_data->tlg_dat) );
	pTp_dat =  (TP_DAT *)( &(pTlg_data->tp_dat[tp]) );
	tp_stat = pTp_dat->TP_Status;
	
	bzero(buffer, sizeof(buffer));
	if(  '\x09' != recvBuffer[0]  ){		
		printf("参数错误,不是液位仪的数据,液位仪WriteTP_DAT 失败");
		return -1;
	}
	
	if( (node <= 0) || (node > 127 ) ){//||(node != gpIfsf_shm->node_tlg) 
		printf("液位仪节点值错误,液位仪WriteTP_DAT 失败");
		return -1;
	}
	
	
	
	buffer[0] = recvBuffer[2]; //LNAR
	buffer[1] = recvBuffer[3]; //LNAR
	buffer[2] = recvBuffer[0]; //LNAO
	buffer[3] = recvBuffer[1]; //LNAO
	buffer[4] = '\0'; //recvBuffer[4]; //IFSF_MC
	buffer[5] = ( (recvBuffer[5] &0x1f) |0xE0); //M_St
	recv_lg = recvBuffer[6];  //M_Lg, big end mode.
	recv_lg = recv_lg * 256 + recvBuffer[7]; 
	buffer[8] = '\x01'; //recvBuffer[8]; //Data_Ad_Lg
	buffer[9] =recvBuffer[9]; //Data_ad:TP_DAt.//tp + 0x21; 
	//buffer[10] = ; //MS_Ack
	//buffer[11] = recvBuffer[10]; 
	// recvBuffer[10]:Data_Id, 11:Data_Lg, 12:Data_El.  Data begin here.
	tmp_lg = 10;
	i=0;
	tmpMSAck = '\0'; // all is  ok.
	while( i < (recv_lg -2) ) {
		tmpDataAck = '\0'; //ok ack.
		lg =  (int )recvBuffer[11+i]; //Data_Lg.
		switch( recvBuffer[10+i] ) {
			case T_TP_Manufacturer_Id:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_TP_Manufacturer_Id;
				tmpDataAck = '\x02';  //read only
				tmpMSAck = '\x05';
				tmp_lg++;
				buffer[tmp_lg] =tmpDataAck;
				i = i + 2 + lg;
				break;
			case T_TP_Type: 
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_TP_Type;
				tmpDataAck = '\x02';  //read only
				tmpMSAck = '\x05';			
				tmp_lg++;
				buffer[tmp_lg] =tmpDataAck;
				i = i + 2 + lg;
				break;
			case T_TP_Serial_Nb:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_TP_Serial_Nb;
				tmpDataAck = '\x02';  //read only
				tmpMSAck = '\x05';			
				tmp_lg++;
				buffer[tmp_lg] =tmpDataAck;
				i = i + 2 + lg;
				break;
			case T_TP_Model:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_TP_Model;
				tmpDataAck = '\x02';  //read only
				tmpMSAck = '\x05';			
				tmp_lg++;
				buffer[tmp_lg] =tmpDataAck;
				i = i + 2 + lg;
				break;			
			case T_TP_Appl_Software_Ver:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_TP_Appl_Software_Ver;
				tmpDataAck = '\x02';  //read only
				tmpMSAck = '\x05';			
				tmp_lg++;
				buffer[tmp_lg] =tmpDataAck;
				i = i + 2 + lg;
				break;
			case T_Prod_Nb:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Prod_Nb;
				/*
				 * Note: 临时性处理, 屏蔽状态判断,
				 *       因为当前Fuel server下发修改此项命令前没有切换液位仪模式为维护模式, 
				 *       但是目前不能修正, 如果FCC按标准回复NAK, 又会导致换号不成功,
				 *       所以项目组要求FCC屏蔽此判断, 临时适应RTX系统.
				 *
				 *       Guojie Zhu @ 2009.8.21
				 *
				 */
				if(0){
//				if(3 != tp_stat){
					tmpDataAck = '\x02';  //read only
					tmpMSAck = '\x05';			
					tmp_lg++;
					buffer[tmp_lg] =tmpDataAck;
					i = i + 2 + lg;
					break;
				}
				memcpy(pTp_dat->Prod_Nb, &recvBuffer[12+i], lg);
				tmp_lg++;
				buffer[tmp_lg] =0;//tmpDataAck;
				i = i + 2 + lg;
				break;
			case T_Prod_Description:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Prod_Description;
				/*
				 * Note: 临时性处理, 屏蔽状态判断,
				 *       因为当前Fuel server下发修改此项命令前没有切换液位仪模式为维护模式, 
				 *       但是目前不能修正, 如果FCC按标准回复NAK, 又会导致换号不成功,
				 *       所以项目组要求FCC屏蔽此判断, 临时适应RTX系统.
				 *
				 *       Guojie Zhu @ 2009.8.21
				 *
				 */
				if(0){
//				if(3 != tp_stat){
					tmpDataAck = '\x02';  //read only
					tmpMSAck = '\x05';			
					tmp_lg++;
					buffer[tmp_lg] =tmpDataAck;
					i = i + 2 + lg;
					break;
				}
				memcpy(pTp_dat->Prod_Description, &recvBuffer[12+i], lg);
				tmp_lg++;
				buffer[tmp_lg] =0;//tmpDataAck;
				i = i + 2 + lg;
				break;
			case T_Prod_Group_Code:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Prod_Group_Code;
				if(3 != tp_stat){
					tmpDataAck = '\x02';  //read only
					tmpMSAck = '\x05';			
					tmp_lg++;
					buffer[tmp_lg] =tmpDataAck;
					i = i + 2 + lg;
					break;
				}
				pTp_dat->Prod_Group_Code = recvBuffer[12+i];
				tmp_lg++;
				buffer[tmp_lg] =0;//tmpDataAck;
				i = i + 2 + lg;
				break;
			case T_Ref_Density:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Ref_Density;
				if(3 != tp_stat){
					tmpDataAck = '\x02';  //read only
					tmpMSAck = '\x05';			
					tmp_lg++;
					buffer[tmp_lg] =tmpDataAck;
					i = i + 2 + lg;
					break;
				}
				pTp_dat->Ref_Density[0] = recvBuffer[12+i];
				pTp_dat->Ref_Density[1] = recvBuffer[13+i];
				tmp_lg++;
				buffer[tmp_lg] =0;//tmpDataAck;
				i = i + 2 + lg;
				break;
			case T_Tank_Diameter:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Tank_Diameter;
				if(3 != tp_stat){
					tmpDataAck = '\x02';  //read only
					tmpMSAck = '\x05';			
					tmp_lg++;
					buffer[tmp_lg] =tmpDataAck;
					i = i + 2 + lg;
					break;
				}
				memcpy(pTp_dat->Tank_Diameter, &recvBuffer[12+i], lg);
				tmp_lg++;
				buffer[tmp_lg] =0;//tmpDataAck;
				i = i + 2 + lg;
				break;
			case T_Shell_Capacity:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Shell_Capacity;
				if(3 != tp_stat){
					tmpDataAck = '\x02';  //read only
					tmpMSAck = '\x05';			
					tmp_lg++;
					buffer[tmp_lg] =tmpDataAck;
					i = i + 2 + lg;
					break;
				}
				memcpy(pTp_dat->Shell_Capacity, &recvBuffer[12+i], lg);
				tmp_lg++;
				buffer[tmp_lg] =0;//tmpDataAck;
				i = i + 2 + lg;
				break;
			case T_Max_Safe_Fill_Capacity:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Max_Safe_Fill_Capacity;
				memcpy(pTp_dat->Max_Safe_Fill_Capacity, &recvBuffer[12+i], lg);
				tmp_lg++;
				buffer[tmp_lg] =0;//tmpDataAck;
				i = i + 2 + lg;
				break;
			case T_Low_Capacity:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Low_Capacity;
				if(3 != tp_stat){
					tmpDataAck = '\x02';  //read only
					tmpMSAck = '\x05';			
					tmp_lg++;
					buffer[tmp_lg] =tmpDataAck;
					i = i + 2 + lg;
					break;
				}
				memcpy(pTp_dat->Low_Capacity, &recvBuffer[12+i], lg);
				tmp_lg++;
				buffer[tmp_lg] =0;//tmpDataAck;
				i = i + 2 + lg;
				break;
			case T_Min_Operating_Capacity:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Min_Operating_Capacity;
				if(3 != tp_stat){
					tmpDataAck = '\x02';  //read only
					tmpMSAck = '\x05';			
					tmp_lg++;
					buffer[tmp_lg] =tmpDataAck;
					i = i + 2 + lg;
					break;
				}
				memcpy(pTp_dat->Min_Operating_Capacity, &recvBuffer[12+i], lg);
				tmp_lg++;
				buffer[tmp_lg] =0;//tmpDataAck;
				i = i + 2 + lg;
				break;
			case T_HiHi_Level_Setpoint:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_HiHi_Level_Setpoint;
				if(3 != tp_stat){
					tmpDataAck = '\x02';  //read only
					tmpMSAck = '\x05';			
					tmp_lg++;
					buffer[tmp_lg] =tmpDataAck;
					i = i + 2 + lg;
					break;
				}
				memcpy(pTp_dat->HiHi_Level_Setpoint, &recvBuffer[12+i], lg);
				tmp_lg++;
				buffer[tmp_lg] =0;//tmpDataAck;
				i = i + 2 + lg;
				break;
			case T_Hi_Level_Setpoint:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Hi_Level_Setpoint;
				if(3 != tp_stat){
					tmpDataAck = '\x02';  //read only
					tmpMSAck = '\x05';			
					tmp_lg++;
					buffer[tmp_lg] =tmpDataAck;
					i = i + 2 + lg;
					break;
				}
				memcpy(pTp_dat->Hi_Level_Setpoint, &recvBuffer[12+i], lg);
				tmp_lg++;
				buffer[tmp_lg] =0;//tmpDataAck;
				i = i + 2 + lg;
				break;
			case T_Lo_Level_Setpoint:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Lo_Level_Setpoint;
				if(3 != tp_stat){
					tmpDataAck = '\x02';  //read only
					tmpMSAck = '\x05';			
					tmp_lg++;
					buffer[tmp_lg] =tmpDataAck;
					i = i + 2 + lg;
					break;
				}
				memcpy(pTp_dat->Lo_Level_Setpoint, &recvBuffer[12+i], lg);
				tmp_lg++;
				buffer[tmp_lg] =0;//tmpDataAck;
				i = i + 2 + lg;
				break;
			case T_LoLo_Level_Setpoint:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_LoLo_Level_Setpoint;
				if(3 != tp_stat){
					tmpDataAck = '\x02';  //read only
					tmpMSAck = '\x05';			
					tmp_lg++;
					buffer[tmp_lg] =tmpDataAck;
					i = i + 2 + lg;
					break;
				}
				memcpy(pTp_dat->LoLo_Level_Setpoint, &recvBuffer[12+i], lg);
				tmp_lg++;
				buffer[tmp_lg] =0;//tmpDataAck;
				i = i + 2 + lg;
				break;
			case T_Hi_Water_Setpoint:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Hi_Water_Setpoint;
				if(3 != tp_stat){
					tmpDataAck = '\x02';  //read only
					tmpMSAck = '\x05';			
					tmp_lg++;
					buffer[tmp_lg] =tmpDataAck;
					i = i + 2 + lg;
					break;
				}
				memcpy(pTp_dat->Hi_Water_Setpoint, &recvBuffer[12+i], lg);
				tmp_lg++;
				buffer[tmp_lg] =0;//tmpDataAck;
				i = i + 2 + lg;
				break;
			case T_Water_Detection_Thresh:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Water_Detection_Thresh;
				tmpDataAck = '\x02';  //read only
				tmpMSAck = '\x05';			
				tmp_lg++;
				buffer[tmp_lg] =tmpDataAck;
				i = i + 2 + lg;
				break;
			case T_Tank_Tilt_Offset:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Tank_Tilt_Offset;
				if(3 != tp_stat){
					tmpDataAck = '\x02';  //read only
					tmpMSAck = '\x05';			
					tmp_lg++;
					buffer[tmp_lg] =tmpDataAck;
					i = i + 2 + lg;
					break;
				}
				memcpy(pTp_dat->Tank_Tilt_Offset, &recvBuffer[12+i], lg);
				tmp_lg++;
				buffer[tmp_lg] =0;//tmpDataAck;
				i = i + 2 + lg;
				break;
			case T_Tank_Manifold_Partners:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Tank_Manifold_Partners;
				if(3 != tp_stat){
					tmpDataAck = '\x02';  //read only. all is zero in China.
					tmpMSAck = '\x05';			
					tmp_lg++;
					buffer[tmp_lg] =tmpDataAck;
					i = i + 2 + lg;
					break;
				}
				memcpy(pTp_dat->Tank_Manifold_Partners, &recvBuffer[12+i], lg);
				tmp_lg++;
				buffer[tmp_lg] =0;//tmpDataAck;
				i = i + 2 + lg;
				break;
			case T_TP_Measurement_Units:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_TP_Measurement_Units;
				tmpDataAck = '\x02';  //read only
				tmpMSAck = '\x05';			
				tmp_lg++;
				buffer[tmp_lg] =tmpDataAck;
				i = i + 2 + lg;
				break;
			case T_TP_Status:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_TP_Status;
				tmpDataAck = '\x02';  //read only
				tmpMSAck = '\x05';			
				tmp_lg++;
				buffer[tmp_lg] =tmpDataAck;
				i = i + 2 + lg;
				break;
			case T_TP_Alarm:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_TP_Alarm;
				tmpDataAck = '\x02';  //read only
				tmpMSAck = '\x05';			
				tmp_lg++;
				buffer[tmp_lg] =tmpDataAck;
				i = i + 2 + lg;
				break;
			case T_Product_Level:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Product_Level;
				tmpDataAck = '\x02';  //read only
				tmpMSAck = '\x05';			
				tmp_lg++;
				buffer[tmp_lg] =tmpDataAck;
				i = i + 2 + lg;
				break;
			case T_Total_Observed_Volume:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Total_Observed_Volume;
				tmpDataAck = '\x02';  //read only
				tmpMSAck = '\x05';			
				tmp_lg++;
				buffer[tmp_lg] =tmpDataAck;
				i = i + 2 + lg;
				break;
			case T_Gross_Standard_Volume:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Gross_Standard_Volume;
				tmpDataAck = '\x02';  //read only
				tmpMSAck = '\x05';			
				tmp_lg++;
				buffer[tmp_lg] =tmpDataAck;
				i = i + 2 + lg;
				break;
			case T_Average_Temp:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Average_Temp;
				tmpDataAck = '\x02';  //read only
				tmpMSAck = '\x05';			
				tmp_lg++;
				buffer[tmp_lg] =tmpDataAck;
				i = i + 2 + lg;
				break;
			case T_Water_Level:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Water_Level;
				tmpDataAck = '\x02';  //read only
				tmpMSAck = '\x05';			
				tmp_lg++;
				buffer[tmp_lg] =tmpDataAck;
				i = i + 2 + lg;
				break;
			case T_Observed_Density:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Water_Level;
				tmpDataAck = '\x02';  //read only
				tmpMSAck = '\x05';			
				tmp_lg++;
				buffer[tmp_lg] =tmpDataAck;
				i = i + 2 + lg;
				break;
			case T_Last_Reading_Date:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Water_Level;
				tmpDataAck = '\x02';  //read only
				tmpMSAck = '\x05';			
				tmp_lg++;
				buffer[tmp_lg] =tmpDataAck;
				i = i + 2 + lg;
				break;
			case T_Last_Reading_Time:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)T_Water_Level;
				tmpDataAck = '\x02';  //read only
				tmpMSAck = '\x05';			
				tmp_lg++;
				buffer[tmp_lg] =tmpDataAck;
				i = i + 2 + lg;
				break;
			default:
				tmp_lg++;
				buffer[tmp_lg] = recvBuffer[i+10] ;
				tmpDataAck = '\x04'; //not exist
				tmpMSAck = '\x05';
				tmp_lg++;
				buffer[tmp_lg] ='\0';
				break;
		}
	}
	tmp_lg++;
	buffer[6] = (unsigned char )( (tmp_lg-8) / 256) ;
	buffer[7] = (unsigned char )((tmp_lg-8) %256);  //big end.
	buffer[10] = tmpMSAck;
	if(NULL != msg_lg) {
		memcpy(msg_lg , &tmp_lg, sizeof(int) );
	}
	if(NULL != sendBuffer) {
		memcpy(sendBuffer, buffer, tmp_lg);
	}

	return 0;
}
//-
//只写TP_DAT的单个数据项函数,用于设备自己改写或者初始化配置.
//data(Data_Id+Data_Lg+Data_El), 成功0,失败-1或者Data_Ack值.
//-
int SetSglTP_DAT(const char node,  const char tp_ad, const unsigned char *data){
	//int i, recv_lg, tmp_lg = 0, lg;
	int tp;
	unsigned char tmpDataAck;//tmpMSAck, 
	TLG_DATA *pTlg_data;
	TLG_DAT *pTlg_dat;
	TP_DAT  *pTp_dat;
	//unsigned char node;
	//unsigned char buffer[256];
	
	if( NULL == gpIfsf_shm ){		
		printf("共享内存值错误,液位仪WriteTP_DAT失败");
		return -1;
	}
	if( (NULL == data) || ( tp_ad < 0x21) || ( tp_ad > 0x3f) || ( node <=0) || ( node >=127) ){//||(node != gpIfsf_shm->node_tlg) 		
		printf("参数错误,液位仪WriteTP_DAT 失败");
		return -1;
	}
	
	//node = recvBuffer[1];
	tp = tp_ad - 0x21;
	pTlg_data = (TLG_DATA*)( &(gpIfsf_shm->tlg_data) );
	pTlg_dat = (TLG_DAT*)( &(pTlg_data->tlg_dat) );
	pTp_dat =  (TP_DAT *)( &(pTlg_data->tp_dat[tp]) );
	
	
		
	
	
	tmpDataAck = '\0'; //ok ack.
	//lg =  (int )data[1]; //Data_Lg.
	switch( data[0] )
	{
		case T_TP_Manufacturer_Id:
			memcpy(pTp_dat->TP_Manufacturer_Id, &data[2], sizeof(pTp_dat->TP_Manufacturer_Id));
			break;
		case T_TP_Type: 
			memcpy(pTp_dat->TP_Type, &data[2], sizeof(pTp_dat->TP_Type));
			break;
		case T_TP_Serial_Nb:
			memcpy(pTp_dat->TP_Serial_Nb, &data[2], sizeof(pTp_dat->TP_Serial_Nb));
			break;
		case T_TP_Model:
			memcpy(pTp_dat->TP_Model, &data[2], sizeof(pTp_dat->TP_Model));
			break;			
		case T_TP_Appl_Software_Ver:
			tmpDataAck = '\x02';  //read only
			break;
		case T_Prod_Nb:
			memcpy(pTp_dat->Prod_Nb, &data[2], sizeof(pTp_dat->Prod_Nb));
			break;
		case T_Prod_Description:
			memcpy(pTp_dat->Prod_Description, &data[2], sizeof(pTp_dat->Prod_Description));
			break;
		case T_Prod_Group_Code:
			pTp_dat->Prod_Group_Code = data[2];
			break;
		case T_Ref_Density:
			memcpy(pTp_dat->Ref_Density, &data[2], sizeof(pTp_dat->Ref_Density));
			break;
		case T_Tank_Diameter:
			memcpy(pTp_dat->Tank_Diameter, &data[2], sizeof(pTp_dat->Tank_Diameter));
			break;
		case T_Shell_Capacity:
			memcpy(pTp_dat->Shell_Capacity, &data[2], sizeof(pTp_dat->Shell_Capacity));
			break;
		case T_Max_Safe_Fill_Capacity:
			memcpy(pTp_dat->Max_Safe_Fill_Capacity, &data[2], sizeof(pTp_dat->Max_Safe_Fill_Capacity));
			break;
		case T_Low_Capacity:
			memcpy(pTp_dat->Low_Capacity, &data[2], sizeof(pTp_dat->Low_Capacity));
			break;
		case T_Min_Operating_Capacity:
			memcpy(pTp_dat->Min_Operating_Capacity, &data[2], sizeof(pTp_dat->Min_Operating_Capacity));
			break;
		case T_HiHi_Level_Setpoint:
			memcpy(pTp_dat->HiHi_Level_Setpoint, &data[2], sizeof(pTp_dat->HiHi_Level_Setpoint));
			break;
		case T_Hi_Level_Setpoint:
			memcpy(pTp_dat->Hi_Level_Setpoint, &data[2], sizeof(pTp_dat->Hi_Level_Setpoint));
			break;
		case T_Lo_Level_Setpoint:
			memcpy(pTp_dat->Lo_Level_Setpoint, &data[2], sizeof(pTp_dat->Lo_Level_Setpoint));
			break;
		case T_LoLo_Level_Setpoint:
			memcpy(pTp_dat->LoLo_Level_Setpoint, &data[2], sizeof(pTp_dat->LoLo_Level_Setpoint));
			break;
		case T_Hi_Water_Setpoint:
			memcpy(pTp_dat->Hi_Water_Setpoint, &data[2], sizeof(pTp_dat->Hi_Water_Setpoint));
			break;
		case T_Water_Detection_Thresh:
			memcpy(pTp_dat->Water_Detection_Thresh, &data[2], sizeof(pTp_dat->Water_Detection_Thresh) );
			break;
		case T_Tank_Tilt_Offset:
			memcpy(pTp_dat->Tank_Tilt_Offset, &data[2], sizeof(pTp_dat->Tank_Tilt_Offset));
			break;
		case T_Tank_Manifold_Partners:
			memcpy(pTp_dat->Tank_Manifold_Partners, &data[2], sizeof(pTp_dat->Tank_Manifold_Partners));
			break;
		case T_TP_Measurement_Units:
			pTp_dat->TP_Measurement_Units = data[2];
			break;
		case T_TP_Status:
			pTp_dat->TP_Status = data[2];
			break;
		case T_TP_Alarm:
			memcpy(pTp_dat->TP_Alarm, &data[2], sizeof(pTp_dat->TP_Alarm));
			break;
		case T_Product_Level:
			memcpy(pTp_dat->Product_Level, &data[2], sizeof(pTp_dat->Product_Level));
			break;
		case T_Total_Observed_Volume:
			memcpy(pTp_dat->Total_Observed_Volume, &data[2], sizeof(pTp_dat->Total_Observed_Volume));
#ifdef TANK_DEBUG
				RunLog("Total Observed Volume is %02x%02x%02x%02x", pTp_dat->Total_Observed_Volume[3],\
				pTp_dat->Total_Observed_Volume[4],\
				pTp_dat->Total_Observed_Volume[5],\
				pTp_dat->Total_Observed_Volume[6]);
#endif
			break;
		case T_Gross_Standard_Volume:
			memcpy(pTp_dat->Gross_Standard_Volume, &data[2], sizeof(pTp_dat->Gross_Standard_Volume));
			break;
		case T_Average_Temp:
			memcpy(pTp_dat->Average_Temp, &data[2], sizeof(pTp_dat->Average_Temp));
			break;
		case T_Water_Level:
			memcpy(pTp_dat->Water_Level, &data[2], sizeof(pTp_dat->Water_Level));
			break;
		case T_Observed_Density:
			memcpy(pTp_dat->Observed_Density, &data[2], sizeof(pTp_dat->Observed_Density));
			break;
		case T_Last_Reading_Date:
			memcpy(pTp_dat->Last_Reading_Date, &data[2], sizeof(pTp_dat->Last_Reading_Date));
			break;
		case T_Last_Reading_Time:
			memcpy(pTp_dat->Last_Reading_Time, &data[2], sizeof(pTp_dat->Last_Reading_Time));
			break;
		default:
			tmpDataAck = '\x04'; //not exist
			break;
	}
	return tmpDataAck;
}
//-
//构造主动数据TP_Status_Message的值不发送,senBuffer为不含地址等头信息的数据,data_lg数据总长度
//-
int MakeTpMsg(const char node, const char tp_ad, unsigned char *senBuffer, int *data_lg){
	int tmp_lg = 0;//i, recv_lg, 
	int tp;
	TLG_DATA *pTlg_data;
	TLG_DAT *pTlg_dat;
	TP_DAT  *pTp_dat;
	//unsigned char node;
	unsigned char buffer[256];
	
	memset(buffer, 0, sizeof(buffer));	
	if( NULL == gpIfsf_shm ){		
		printf("共享内存值错误,液位仪MakeTpMsg失败");
		return -1;
	}
	if( (NULL == senBuffer) ||(NULL == data_lg) || ( tp_ad < 0x21) || ( tp_ad > 0x3f) || ( node <=0) || ( node >=127)  ){//||(node != gpIfsf_shm->node_tlg)		
		printf("参数错误,液位仪MakeTpMsg 失败");
		return -1;
	}
	
	tp = tp_ad - 0x21;
	pTlg_data = (TLG_DATA*)( &(gpIfsf_shm->tlg_data) );
	pTlg_dat = (TLG_DAT*)( &(pTlg_data->tlg_dat) );
	pTp_dat =  (TP_DAT *)( &(pTlg_data->tp_dat[tp]) );
	
	buffer[0] = '\x02';
	buffer[1] = '\x01';
	buffer[2] = '\x09';//液位仪9
	buffer[3] = node;
	buffer[4] = '\0'; //recvBuffer[4]; IFSF_MC
	buffer[5] = '\x80';//unsolicited data.
	//recv_lg = recvBuffer[6];
	//recv_lg = recv_lg * 256 + recvBuffer[7];
	buffer[8] = '\x01'; //recvBuffer[8]; //Ad_lg
	buffer[9] = tp_ad;  //ad	
	//buffer[10] = recvBuffer[10]; //It's Data_Id. Data begin.	
	tmp_lg=9;
	
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)T_TP_Status_Message;
	tmp_lg++;
	buffer[tmp_lg] = 0x00;

	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)T_TP_Status;
	tmp_lg++;
	buffer[tmp_lg] ='\x01';
	tmp_lg++;
	buffer[tmp_lg] = pTp_dat->TP_Status;
	
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)T_TP_Alarm;
	tmp_lg++;
	buffer[tmp_lg] ='\x02';
	tmp_lg++;
	buffer[tmp_lg] = pTp_dat->TP_Alarm[0];
	tmp_lg++;
	buffer[tmp_lg] = pTp_dat->TP_Alarm[1];
				
	tmp_lg++;
	buffer[6] = (unsigned char )((tmp_lg-8) / 256) ;
	buffer[7] = (unsigned char )((tmp_lg-8) %256);  //big end.
	*data_lg= tmp_lg;
	memcpy(senBuffer, buffer, tmp_lg);	

	return 0;
}

//-
//改变TP的状态或报警的数据.
//status为TP状态,alarm为两个字节的报警数据,如果不改变状态status值设为0, 不改变报警alarm设置为NULL
//要调用MakeTpMsg函数和SendData2CD函数.
//-
int ChangeTpStatAlarm(const char node, const char tp_ad, const unsigned char status, const unsigned char *alarm){
	int i, ret,  tmp_lg = 0;
	int tp;
	TLG_DATA *pTlg_data;
	TLG_DAT *pTlg_dat;
	TP_DAT  *pTp_dat;
	//unsigned char node;
	unsigned char buffer[256];
	
	memset(buffer, 0, sizeof(buffer));	
	if( NULL == gpIfsf_shm ){	
		printf("共享内存值错误,液位仪ChangeTpStatAlarm失败");
		return -1;
	}
	if( ( tp_ad < 0x21) || ( tp_ad > 0x3f) || ( node <=0) || ( node >=127)  ){//	||(node != gpIfsf_shm->node_tlg)	
		printf("参数错误,液位仪ChangeTpStatAlarm 失败");
		return -1;
	}
	
	tp = tp_ad - 0x21;
	pTlg_data = ( TLG_DATA*)( &(gpIfsf_shm->tlg_data) );
	pTlg_dat = ( TLG_DAT*)( &(pTlg_data->tlg_dat) );
	pTp_dat =  ( TP_DAT *)( &(pTlg_data->tp_dat[tp]) );
	if((status > 0) &&(status <= 4)){
		pTp_dat->TP_Status = status;
	}
	if(NULL != alarm ){
		memcpy(pTp_dat->TP_Alarm,alarm, sizeof(pTp_dat->TP_Alarm) );
	}
	ret = MakeTpMsg(node, tp_ad,buffer, &tmp_lg);
	if(ret < 0){
		printf("FCC液位仪主动数据构造出错, ChangeTpStatAlarm 失败");
		return -1;
	}
	//UserLog("ChangeFP_State2 fp_state[%d]",fp_state);
	if (gpIfsf_shm->node_online[MAX_CHANNEL_CNT] == 0) {
		return 0;
	}
	ret = SendData2CD(buffer, tmp_lg, 5);
	if(ret < 0){
		printf("FCC通讯错误,ChangeTpStatAlarm 失败,node[%d]", node);
		return -1;
	}
#if 0
	RunLog("ChangeTpStatAlarm tp_status[%d],tp_alarm[%02x%02x]",status,alarm[0],alarm[1]);
#endif
	return 0;
}


//----------------------------------------------------------------------------------------

//TP_ER_DATA操作数据. node液位仪逻辑节点号,tp_ad液位仪TP地址21h-3fh,
// 要用到gpIfsf_shm,注意P,V操作和死锁.
//改变错误代码.  要调用MakeTPErMsg函数.er_ad错误地址01h-40h
int ChangeTPER_DAT(const char node, const char tp_ad, const char er_ad){
	int i, tp_status;
	int tp; //0,1,2,3...号TP.
	int er=er_ad-1; //0,1,2,3,4,5,6,..63号错误.	
	TLG_DATA *pTlg_data;
	TP_DAT *pTp_dat;
	TP_ER_DAT  *pTp_er_dat;
	char *err[64] = {
		"RAM defect", "ROM defect",	"Configuration or Parameter Error", "Power supply out of order",
		"Main Communication error", "", " ",	"",
		"", "", "",	"",
		"", "",	" ",	"",
		"", "",	"",	"",
		"",   "",   "",  "",
		"",   "",   "",  "",
		"",   "",   "",  "Battery error ",
		"Communication error",   "",   "",  "",
		"",   " ",   "",  " ",
		"  ",   "",   "",  "",
		"",   "",     "",  "",
		"",   "",     "",  " ",
		"",   "",   "",  "",
		"",   "",   "",  "",
		"",   "",   "",  "",
		};
	int data_lg;
	unsigned char sendBuffer[128];
	memset(sendBuffer, 0, 128);
	if( (node <= 0) || (node > 127 ) ){
		printf("加油机节点值错误,最新错误设置 失败");
		return -1;
		}
	if( NULL == gpIfsf_shm ){		
		printf("共享内存值错误,最新错误设置 失败");
		return -1;
	}
	tp = tp_ad - 0x21;
	pTlg_data = ( TLG_DATA *)( &(gpIfsf_shm->tlg_data) );
	pTp_dat = ( TP_DAT *)( &(pTlg_data->tp_dat[tp]) );
	pTp_er_dat =  ( TP_ER_DAT *)( &(pTlg_data->tp_er_dat[tp]) );
	tp_status = pTp_dat->TP_Status;
	if((tp_status <= 0) &&(tp_status > 4)){
		return -1;
	}
	pTp_er_dat->TP_Error_Type = er_ad;
	strncpy(pTp_er_dat->TP_Err_Description, err[er], strlen(err[er])  );
	pTp_er_dat->TP_Error_Total += 1;
	pTp_er_dat->TP_Error_Status = tp_status;
	
	//usr i to ret
	i = MakeTPErMsg( node,  tp_ad, sendBuffer, &data_lg);
	if(i<0){
		printf("MakeTPErMsg error.");
		return -1;
		}
	i = SendData2CD(sendBuffer, data_lg, 5);
	if(i<0){
		printf("SendData2CD2 error.");
		return -1;
		}
	return 0;
}

int DefaultTPER_DAT(const char node, const char tp_ad){
	int i, tp_status;
	int tp; //0,1,2,3...号TP.
	//int er=er_ad-1; //0,1,2,3,4,5,6,..63号错误.	
	TLG_DATA *pTlg_data;
	TP_DAT *pTp_dat;
	TP_ER_DAT  *pTp_er_dat;
	
	if( (node <= 0) || (node > 127 ) ){
		printf("液位仪节点值错误,最新错误初始化 失败");
		return -1;
		}
	if( NULL == gpIfsf_shm ){		
		printf("共享内存值错误,最新错误初始化失败");
		return -1;
	}
	tp = tp_ad - 0x21;
	pTlg_data = ( TLG_DATA*)( &(gpIfsf_shm->tlg_data) );
	pTp_dat = ( TP_DAT *)( &(pTlg_data->tp_dat[tp]) );
	pTp_er_dat =  ( TP_ER_DAT *)( &(pTlg_data->tp_er_dat[tp]) );
	
	pTp_er_dat->TP_Error_Type = '\0';
	bzero(pTp_er_dat->TP_Err_Description, sizeof(pTp_er_dat->TP_Err_Description));
	pTp_er_dat->TP_Error_Total  = '\0';
	pTp_er_dat->TP_Error_Status= 2;
	
	return 0;
}
//读取TP_ER_DATA. 构造的信息数据的类型为001回答.
//recvBuffer为收到的data,sendBuffer为要发送的data,msg_lg为生成的总长度.
int ReadTPER_DAT(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg){
	int i,j,k, fp_state,tmp_lg=0, recv_lg;
	int tp ; //0,1,2,3...号TP.
	int er;//recvBuffer[11] - 1; //0,1,2,3,4,5,6,..63号错误.
	int tp_status;
	unsigned char node = recvBuffer[1];
	TLG_DATA *pTlg_data;
	TP_DAT *pTp_dat;
	TP_ER_DAT  *pTp_er_dat;
	
	unsigned char buffer[128];
	bzero(buffer, sizeof(buffer));
	if( (node <= 0) || (node > 127 ) ){
		printf("加油机节点值错误,最新错误Read 失败");
		return -1;
	}		
	if( (node <= 0) || (node > 127 ) ){
		printf("加油机节点值错误,最新错误Read 失败");
		return -1;
		}
	tp = recvBuffer[9] - 0x21;
	er=recvBuffer[11] - 1;
	pTlg_data = ( TLG_DATA*)( &(gpIfsf_shm->tlg_data) );
	pTp_dat = ( TP_DAT *)( &(pTlg_data->tp_dat[tp]) );
	pTp_er_dat =  ( TP_ER_DAT *)( &(pTlg_data->tp_er_dat[tp]) );
	tp_status = pTp_dat->TP_Status;
	if((tp_status <= 0) &&(tp_status > 4)){
		return -1;
	}
	
	buffer[0] = recvBuffer[2];
	buffer[1] = recvBuffer[3];
	buffer[2] = recvBuffer[0];
	buffer[3] = recvBuffer[1];
	buffer[4] = '\0'; //recvBuffer[4]; IFSF_MC
	buffer[5] = ( (recvBuffer[5] &0x1f) |0x20);//answer.
	recv_lg = recvBuffer[6];
	recv_lg = recv_lg * 256 + recvBuffer[7];
	buffer[8] = '\x03'; //recvBuffer[8]; //Ad_lg
	buffer[9] = recvBuffer[9];  //ad
	buffer[10] = recvBuffer[10]; //ad
	buffer[11] = pTp_er_dat->TP_Error_Type; //ad
	//buffer[12] = recvBuffer[12]; //It's Data_Id. Data begin.	
	tmp_lg=11;
	for(i = 0; i < recv_lg - 4; i++){
		switch(recvBuffer[12+i]) {			
			case TP_Error_Type :
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)TP_Error_Type;
				tmp_lg++;
				buffer[tmp_lg] = '\x01';
				tmp_lg++;
				buffer[tmp_lg] = pTp_er_dat->TP_Error_Type;
				break;			
			case TP_Err_Description :
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)TP_Err_Description;
				tmp_lg++;
				buffer[tmp_lg] = '\x14';
				k= buffer[tmp_lg];
				for(j=0;j<k;j++){
					tmp_lg++;
					buffer[tmp_lg] = pTp_er_dat->TP_Err_Description[j];
					}
				break;
			case TP_Error_Total:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)TP_Error_Total;
				tmp_lg++;
				buffer[tmp_lg] = '\x01';
				tmp_lg++;
				buffer[tmp_lg] = pTp_er_dat->TP_Error_Total;
				break;			
			case TP_Error_Status:
				tmp_lg++;
				buffer[tmp_lg] = (unsigned char)TP_Error_Status;
				tmp_lg++;
				buffer[tmp_lg] = '\x01';					
				tmp_lg++;
				buffer[tmp_lg] = pTp_er_dat->TP_Error_Status;				
				break;			
			default:
				tmp_lg++;
				buffer[tmp_lg] = recvBuffer[12+i];
				tmp_lg++;
				buffer[tmp_lg] = '\0';			
				break;			
		}	
	}
	tmp_lg++;
	buffer[6] = (unsigned char )((tmp_lg-8) / 256) ;
	buffer[7] = (unsigned char )((tmp_lg-8) %256);  //big end.
	*msg_lg= tmp_lg;
	memcpy(sendBuffer, buffer, tmp_lg);	
	return 0;
	
}

//构造主动数据TP_Error_Type_Mes的值不发送,sendbuffer为待发送的数据,data_lg数据总长度
static int MakeTPErMsg(const char node, const char tp_ad, unsigned char *sendBuffer, int *data_lg){
	int i,j,k,tmp_lg=0;
	int tp_status;
	int tp; //0,1,2,3...号TP.
	int er; //0,1,2,3,4,5,6,..63号错误.	
	TLG_DATA *pTlg_data;
	TP_DAT *pTp_dat;
	TP_ER_DAT  *pTp_er_dat;
	unsigned char buffer[64];
	
	memset(buffer, 0, 64);
	if( (node <= 0) || (node > 127 ) ){
		printf("液位仪节点值错误,最新错误MakeTPErMsg 失败");
		return -1;
	}
	if( (NULL == sendBuffer) || (NULL == data_lg) ){
		printf("参数为空错误,最新错误MakeTPErMsg 失败");
		return -1;
		}
	if( NULL == gpIfsf_shm ){		
		printf("共享内存值错误,最新错误MakeTPErMsg 失败");
		return -1;
	}
	tp =  tp_ad - 0x21;
	pTlg_data = ( TLG_DATA*)( &(gpIfsf_shm->tlg_data) );
	pTp_dat = ( TP_DAT *)( &(pTlg_data->tp_dat[tp]) );
	pTp_er_dat =  ( TP_ER_DAT *)( &(pTlg_data->tp_er_dat[tp]) );
	
	er=pTp_er_dat->TP_Error_Type - 1;
	tp_status = pTp_dat->TP_Status;
	if((tp_status <= 0) &&(tp_status > 4)){
		return -1;
	}
	
	buffer[0] = '\x02';
	buffer[1] = '\x01';
	buffer[2] = '\x09';
	buffer[3] = node;
	buffer[4] = '\0'; //recvBuffer[4]; IFSF_MC
	buffer[5] = '\x80';//unsolicited data..
	//recv_lg = recvBuffer[6];
	//recv_lg = recv_lg * 256 + recvBuffer[7];
	buffer[8] = '\x03'; //recvBuffer[8]; //Ad_lg
	buffer[9] = tp_ad;  //ad
	buffer[10] ='\x41'; //ad
	buffer[11] = pTp_er_dat->TP_Error_Type; //ad
	//buffer[12] = recvBuffer[12]; //It's Data_Id. Data begin.	
	tmp_lg=11;

	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)TP_Error_Type_Mes;
	tmp_lg++;
	buffer[tmp_lg] = '\0';
	
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)TP_Error_Type;
	tmp_lg++;
	buffer[tmp_lg] = '\x01';
	tmp_lg++;
	buffer[tmp_lg] = pTp_er_dat->TP_Error_Type;
	
	/*
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)TP_Error_Status;
	tmp_lg++;
	buffer[tmp_lg] = '\x01';	
	tmp_lg++;
	buffer[tmp_lg] = pTp_er_dat->TP_Error_Status;
	*/
				
	tmp_lg++;
	buffer[6] = (unsigned char )(tmp_lg / 256) ;
	buffer[7] = (unsigned char )(tmp_lg %256);  //big end.
	*data_lg= tmp_lg;
	memcpy(sendBuffer, buffer, tmp_lg);	
	return 0;
}




#ifndef __IFSF_TLGTP_H__
#define __IFSF_TLGTP_H__


//TANK PROBE DATABASE  DB_Ad = TP_ID (21H-3FH), The TP_ID=20H is used to ask for all tank probes.
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
	//O. in kilograms per cubicmeter at reference temperature. BIN16, short int, big end mode.
	unsigned char Ref_Density[2];
	unsigned char Tank_Diameter[4];//O3. BCD8
	unsigned char Shell_Capacity[7];//O3. BIN8+BCD12. 
	unsigned char Max_Safe_Fill_Capacity[7];//O4. BIN8+BCD12:0a00...
	unsigned char Low_Capacity[7];//O4. BIN8+BCD12:0a00... 
	unsigned char Min_Operating_Capacity[7];//O4. BIN8+BCD12:0a00... 
	//O2. Metric level would be reported in 0.001 mm (one thousandthof a mm).BCD8
	unsigned char HiHi_Level_Setpoint[4];
	//O2. Metric level would be reported in 0.001 mm (one thousandthof a mm).BCD8
	unsigned char Hi_Level_Setpoint[4];
	//O2. Metric level would be reported in 0.001 mm (one thousandthof a mm).BCD8
	unsigned char Lo_Level_Setpoint[4];
	//O2. Metric level would be reported in 0.001 mm (one thousandthof a mm).BCD8
	unsigned char LoLo_Level_Setpoint[4];
	//O2. Metric level would be reported in 0.001 mm (one thousandthof a mm).BCD8
	unsigned char Hi_Water_Setpoint[4];
	//O2. Metric level would be reported in 0.001 mm (one thousandthof a mm).BCD8
	unsigned char Water_Detection_Thresh[4];
	//O. Metric level would be reported in 0.001 mm (one thousandthof a mm).BIN8+BCD8:0600... 
	unsigned char Tank_Tilt_Offset[4];
	unsigned char Tank_Manifold_Partners[4];//O. BCD8. siphon manifolds.
	unsigned char TP_Measurement_Units;//O. =0. 0 – metric
	unsigned char TP_Status;//(1-3)
	unsigned char TP_Alarm[2];//(0-65535)
	unsigned char Product_Level[4];//Metric level would be reported in 0.001 mm (one thousandthof a mm).BCD8
	unsigned char Total_Observed_Volume[7];//O3. BIN8+BCD12:0a00...  
	unsigned char Gross_Standard_Volume[7];//O5. BIN8+BCD12:0a00... 

	//unsigned char Average_Temp[4];//O5. BIN8+BCD6.
	unsigned char Average_Temp[3];//O5. BIN8+BCD4. modify by liys 2008-06-03

	// Metric level would be reported in 0.001 mm (one thousandthof a mm).BIN8+BCD8:0500... 
	unsigned char Water_Level[4];
	
	unsigned char Observed_Density[2];//O. in kilograms per cubicmeter at reference temperature. BIN16(0-65535)
	unsigned char Last_Reading_Date[4];
	unsigned char Last_Reading_Time[3];
} TP_DAT;
//-
//默认初始化液位仪.	
//-
int DefaultTP_DAT(const char node, const char tp_ad);
//-
//设置液位仪.根据型号设置.
//-
int SetTP_DAT(const char node, const char tp_ad, const TP_DAT *tp_dat);
//-
//读写TP_DATA函数. 注意要构造的信息数据的类型,000读,001回答,010写,011主动需应答,100主动无需应答,111应答
//recvBuffer为收到的data,sendBuffer为要发送的data,msg_lg为生成的总长度.
//-
//-
//读取TP_DAT.TLG_DAT地址为1.构造的信息数据的类型为001回答
//-
int ReadTP_DAT(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg);
//-
//写TP_DAT函数. 构造的信息数据的类型为111应答.
//msg_lg为生成的总长度,为0表示不需要应答信息,sendBuffer和msg_lg不需要应答可以设置为NULL,.
//-
int WriteTP_DAT(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg);
//-
//只写TP_DAT的单个数据项函数,用于设备自己改写或者初始化配置.
//data(Data_Id+Data_Lg+Data_El), 成功0,失败-1或者Data_Ack值.
//-
int SetSglTP_DAT(const char node,  const char tp_ad, const unsigned char *data);
//-
//构造主动数据TP_Status_Message的值不发送,senBuffer为不含地址等头信息的数据,data_lg数据总长度
//-
int MakeTpMsg(const char node, const char tp_ad, unsigned char *senBuffer, int *data_lg);
//-
//改变TP的状态或报警的数据.
//status为TP状态,alarm为两个字节的报警数据,如果不改变状态status值设为0, 不改变报警alarm设置为NULL
//要调用MakeTpMsg函数和SendData2CD2函数.
//-
int ChangeTpStatAlarm(const char node, const char tp_ad, const unsigned char status, const unsigned char *alarm);

enum TP_ER_DAT_AD{TP_Error_Type=1, TP_Err_Description, TP_Error_Total = 3,
	TP_Error_Total_Erase_Date, TP_Error_Status = 5, TP_Error_Type_Mes = 100 };//gFp_er_dat_ad;
typedef struct{
	/*ERROR DATA:*/
	unsigned char TP_Error_Type;    	         unsigned char TP_Err_Description[20];/*O*/  
	unsigned char TP_Error_Total;    	         //unsigned char  TP_Error_Total_Erase_Date[4];//O
	unsigned char TP_Error_Status;
	/*UNSOLICITED DATA:用函数MakeTPErMsg和实现ChangeTPER_DAT*/
	//unsigned char FP_Error_Type_Mes[2];/*see IFSF 3-01,130*/
	}TP_ER_DAT;
//FP_ER_DATA操作数据. node加油机逻辑节点号,fp_ad加油点地址21h-24h,要用到gpIfsf_shm,注意P,V操作和死锁.
//改变错误代码.  要调用MakeErMsg函数.
int DefaultTPER_DAT(const char node, const char tp_ad);
int ChangeTPER_DAT(const char node, const char tp_ad, const char er_ad);
//读取FP_ER_DATA. 构造的信息数据的类型为001回答.
//recvBuffer为收到的data,sendBuffer为要发送的data,msg_lg为生成的总长度.
int ReadTPER_DAT(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg);
//构造主动数据FP_Error_Type_Mes的值不发送,sendbuffer为待发送的数据,data_lg数据总长度
static int MakeTPErMsg(const char node, const char tp_ad, unsigned char *sendBuffer, int *data_lg);

#endif

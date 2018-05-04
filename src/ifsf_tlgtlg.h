#ifndef __IFSF_TLGTLG_H__
#define __IFSF_TLGTLG_H__



//TANK LEVEL GAUGE DATABASE  DB_Ad = TLG_DAT (01H)
enum TLG_DAT_AD{
	T_Nb_Tanks = 1, T_Reference_Temp = 2, T_TLG_Measurement_Unit = 3,
       	T_Country_Code = 6, T_Maint_Password = 7, T_TLG_Manufacturer_Id = 0x32,
	T_TLG_Model=0x33, T_TLG_Type=0x34, T_TLG_Serial_Nb=0x35, 
	T_TLG_Appl_Software_Ver=0x36, T_IFSF_Protocol_Ver=0x3a,
	T_Current_Date=0x3b, T_Current_Time=0x3c, T_SW_Checksum=0x3d,
	T_Enter_Maint_Mode=0x46, T_Exit_Maint_Mode = 0x47 //command
};
typedef struct{	
	unsigned char Nb_Tanks;
	unsigned char Reference_Temp[4];//O5
	unsigned char TLG_Measurement_Unit; //0 – metric
	unsigned char Country_Code[2]; //china:9156 by ifsf bulletin.
	unsigned char Maint_Password[6]; //M. But it's not in CNPC standard 
	unsigned char TLG_Manufacturer_Id[3]; //
	unsigned char TLG_Model[3];
	unsigned char TLG_Type[3];
	unsigned char TLG_Serial_Nb[12];
	unsigned char TLG_Appl_Software_Ver[12];
	unsigned char IFSF_Protocol_Ver[12]; //
	unsigned char Current_Date[4];//O1
	unsigned char Current_Time[3];//O1
	unsigned char SW_Checksum[4];
	//CMD:70:Enter_Maint_Mode;71:Exit_Maint_Mode;	
} TLG_DAT;
//-
//默认初始化液位仪.	
//-
int DefaultTLG_DAT(const char node);
//-
//设置液位仪.根据型号设置.
//-
int SetTLG_DAT(const char node, const TLG_DAT *tlg_dat);
//-
//读写TLG_DATA函数. 注意要构造的信息数据的类型,000读,001回答,010写,011主动需应答,100主动无需应答,111应答
//recvBuffer为收到的data,sendBuffer为要发送的data,msg_lg为生成的总长度.
//-
//-
//读取TLG_DATA.TLG_DAT地址为1.构造的信息数据的类型为001回答
//-
int ReadTLG_DAT(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg);
//-
//写TLG_DATA函数. 构造的信息数据的类型为111应答.
//msg_lg为生成的总长度,为0表示不需要应答信息,sendBuffer和msg_lg不需要应答可以设置为NULL,.
//-
int WriteTLG_DAT(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg);
//-
//只写读取TLG_DATA的单个数据项函数,用于设备自己改写或者初始化配置.
//data(Data_Id+Data_Lg+Data_El), 成功0,失败-1或者Data_Ack值.
//-
int SetSglTLG_DAT(const char node, const unsigned char *data);

//---------------------------------------------------
enum TLG_ER_DAT_AD{TLG_Error_Type=1, TLG_Err_Description, TLG_Error_Total = 3,
	TLG_Error_Total_Erase_Date, TLG_Error_Type_Mes = 100 
};

typedef struct{
	/*ERROR DATA:*/
	unsigned char TLG_Error_Type; 	         unsigned char TLG_Err_Description[20];/*O*/  
	unsigned char TLG_Error_Total; 	         unsigned char TLG_Error_Total_Erase_Date; //O
	/*UNSOLICITED DATA:用函数MakeErMsg和实现MakeErMsgSend*/
	unsigned char FP_Error_Type_Mes[2];/*see IFSF 3-01,130*/
} TLG_ER_DAT;
//TLG_ER_DAT操作数据. node逻辑节点号,tp_ad点地址21h-24h,要用到gpIfsf_shm,注意P,V操作和死锁.
//改变错误代码.  要调用MakeErMsg函数.
int DefaultTLGER_DAT(const char node);
int ChangeTLGER_DAT(const char node, const char er_ad);
//读取TP_ER_DATA. 构造的信息数据的类型为001回答.
//recvBuffer为收到的data,sendBuffer为要发送的data,msg_lg为生成的总长度.
int ReadTLGER_DAT(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg);
//构造主动数据FP_Error_Type_Mes的值不发送,sendbuffer为待发送的数据,data_lg数据总长度
static int MakeTLGErMsg(const char node,  unsigned char *sendBuffer, int *data_lg);

//----------------------------------------



#endif


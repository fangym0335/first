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
	unsigned char TP_Measurement_Units;//O. =0. 0 �C metric
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
//Ĭ�ϳ�ʼ��Һλ��.	
//-
int DefaultTP_DAT(const char node, const char tp_ad);
//-
//����Һλ��.�����ͺ�����.
//-
int SetTP_DAT(const char node, const char tp_ad, const TP_DAT *tp_dat);
//-
//��дTP_DATA����. ע��Ҫ�������Ϣ���ݵ�����,000��,001�ش�,010д,011������Ӧ��,100��������Ӧ��,111Ӧ��
//recvBufferΪ�յ���data,sendBufferΪҪ���͵�data,msg_lgΪ���ɵ��ܳ���.
//-
//-
//��ȡTP_DAT.TLG_DAT��ַΪ1.�������Ϣ���ݵ�����Ϊ001�ش�
//-
int ReadTP_DAT(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg);
//-
//дTP_DAT����. �������Ϣ���ݵ�����Ϊ111Ӧ��.
//msg_lgΪ���ɵ��ܳ���,Ϊ0��ʾ����ҪӦ����Ϣ,sendBuffer��msg_lg����ҪӦ���������ΪNULL,.
//-
int WriteTP_DAT(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg);
//-
//ֻдTP_DAT�ĵ����������,�����豸�Լ���д���߳�ʼ������.
//data(Data_Id+Data_Lg+Data_El), �ɹ�0,ʧ��-1����Data_Ackֵ.
//-
int SetSglTP_DAT(const char node,  const char tp_ad, const unsigned char *data);
//-
//������������TP_Status_Message��ֵ������,senBufferΪ������ַ��ͷ��Ϣ������,data_lg�����ܳ���
//-
int MakeTpMsg(const char node, const char tp_ad, unsigned char *senBuffer, int *data_lg);
//-
//�ı�TP��״̬�򱨾�������.
//statusΪTP״̬,alarmΪ�����ֽڵı�������,������ı�״̬statusֵ��Ϊ0, ���ı䱨��alarm����ΪNULL
//Ҫ����MakeTpMsg������SendData2CD2����.
//-
int ChangeTpStatAlarm(const char node, const char tp_ad, const unsigned char status, const unsigned char *alarm);

enum TP_ER_DAT_AD{TP_Error_Type=1, TP_Err_Description, TP_Error_Total = 3,
	TP_Error_Total_Erase_Date, TP_Error_Status = 5, TP_Error_Type_Mes = 100 };//gFp_er_dat_ad;
typedef struct{
	/*ERROR DATA:*/
	unsigned char TP_Error_Type;    	         unsigned char TP_Err_Description[20];/*O*/  
	unsigned char TP_Error_Total;    	         //unsigned char  TP_Error_Total_Erase_Date[4];//O
	unsigned char TP_Error_Status;
	/*UNSOLICITED DATA:�ú���MakeTPErMsg��ʵ��ChangeTPER_DAT*/
	//unsigned char FP_Error_Type_Mes[2];/*see IFSF 3-01,130*/
	}TP_ER_DAT;
//FP_ER_DATA��������. node���ͻ��߼��ڵ��,fp_ad���͵��ַ21h-24h,Ҫ�õ�gpIfsf_shm,ע��P,V����������.
//�ı�������.  Ҫ����MakeErMsg����.
int DefaultTPER_DAT(const char node, const char tp_ad);
int ChangeTPER_DAT(const char node, const char tp_ad, const char er_ad);
//��ȡFP_ER_DATA. �������Ϣ���ݵ�����Ϊ001�ش�.
//recvBufferΪ�յ���data,sendBufferΪҪ���͵�data,msg_lgΪ���ɵ��ܳ���.
int ReadTPER_DAT(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg);
//������������FP_Error_Type_Mes��ֵ������,sendbufferΪ�����͵�����,data_lg�����ܳ���
static int MakeTPErMsg(const char node, const char tp_ad, unsigned char *sendBuffer, int *data_lg);

#endif

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
	unsigned char TLG_Measurement_Unit; //0 �C metric
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
//Ĭ�ϳ�ʼ��Һλ��.	
//-
int DefaultTLG_DAT(const char node);
//-
//����Һλ��.�����ͺ�����.
//-
int SetTLG_DAT(const char node, const TLG_DAT *tlg_dat);
//-
//��дTLG_DATA����. ע��Ҫ�������Ϣ���ݵ�����,000��,001�ش�,010д,011������Ӧ��,100��������Ӧ��,111Ӧ��
//recvBufferΪ�յ���data,sendBufferΪҪ���͵�data,msg_lgΪ���ɵ��ܳ���.
//-
//-
//��ȡTLG_DATA.TLG_DAT��ַΪ1.�������Ϣ���ݵ�����Ϊ001�ش�
//-
int ReadTLG_DAT(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg);
//-
//дTLG_DATA����. �������Ϣ���ݵ�����Ϊ111Ӧ��.
//msg_lgΪ���ɵ��ܳ���,Ϊ0��ʾ����ҪӦ����Ϣ,sendBuffer��msg_lg����ҪӦ���������ΪNULL,.
//-
int WriteTLG_DAT(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg);
//-
//ֻд��ȡTLG_DATA�ĵ����������,�����豸�Լ���д���߳�ʼ������.
//data(Data_Id+Data_Lg+Data_El), �ɹ�0,ʧ��-1����Data_Ackֵ.
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
	/*UNSOLICITED DATA:�ú���MakeErMsg��ʵ��MakeErMsgSend*/
	unsigned char FP_Error_Type_Mes[2];/*see IFSF 3-01,130*/
} TLG_ER_DAT;
//TLG_ER_DAT��������. node�߼��ڵ��,tp_ad���ַ21h-24h,Ҫ�õ�gpIfsf_shm,ע��P,V����������.
//�ı�������.  Ҫ����MakeErMsg����.
int DefaultTLGER_DAT(const char node);
int ChangeTLGER_DAT(const char node, const char er_ad);
//��ȡTP_ER_DATA. �������Ϣ���ݵ�����Ϊ001�ش�.
//recvBufferΪ�յ���data,sendBufferΪҪ���͵�data,msg_lgΪ���ɵ��ܳ���.
int ReadTLGER_DAT(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg);
//������������FP_Error_Type_Mes��ֵ������,sendbufferΪ�����͵�����,data_lg�����ܳ���
static int MakeTLGErMsg(const char node,  unsigned char *sendBuffer, int *data_lg);

//----------------------------------------



#endif


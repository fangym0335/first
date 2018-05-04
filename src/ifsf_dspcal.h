/*******************************************************************
1.�ṹ��FP_XXX_DATA��ö������FP_XXX_DAT_AD������IFSF���������ݼ����ַ.�˴����������.
2. ��.c�ļ�Ҫ��#incude "ifsf_fp.h"
-------------------------------------------------------------------
2007-07-13 by chenfm
*******************************************************************/
#ifndef __IFSF_FPCAL_H__
#define __IFSF_FPCAL_H__

#include "ifsf_def.h"
#include "ifsf_dspad.h"

/*--------------------------------------------------------------*/
//�������ݼ����ַ:(&��/��������_����)
//������,��IFSF 3-01,V2.44,61-84. ��ַ01h.
enum FP_C_DAT_AD{
	C_Nb_Products = 2, C_Nb_Fuelling_Modes , C_Nb_Meters, C_Nb_FP, C_Country_Code,
	C_Blend_Tolerance=7,C_Drive_Off_Lights_Mode, C_OPT_Light_Mode,
	C_Clear_Display_Mode, C_Auth_State_Mode, C_Stand_Alone_Auth, C_Max_Auth_Time,/*!!*/
	C_Max_Time_W_O_Prog = 21, C_Min_Fuelling_Vol, C_Min_Display_Vol, C_Min_Guard_Time,
	C_Pulser_Err_Tolerance = 26, C_Time_Display_Product_Name = 28, 
	C_Digits_Vol_Layout = 40,	C_Digits_Amount_Layout, C_Digits_Unit_Price, C_Unit_Price_Mult_Fact,
	C_Amount_Rounding_Type, C_Preset_Rounding_Amount, C_Price_Set_Nb,
	C_Manufacturer_Id = 50, C_Dispenser_Model, C_Calculator_Type,
	C_Calculator_Serial_No, C_Appl_Software_Ver, C_W_M_Software_Ver,
	C_W_M_Software_Date, C_W_M_Security_Type, C_Protocol_Ver,  C_SW_Change_Date ,
	C_SW_Change_Personal_Nb ,  C_SW_Checksum , C_Calc_Illumination = 70,
	C_LCD_Backlight_Switch , C_Display_Intensity , C_W_M_Polynomial =80, C_W_M_Seed, 
	};//gFp_c_dat_ad;
typedef struct{
	/*CONFIGURATION DATA:*/
	unsigned char Nb_Products;/*1-8*/           unsigned char Nb_Fuelling_Modes; /*1-8,���豸1*/
	unsigned char Nb_Meters; /*1-16*/           unsigned char Nb_FP; /*1-4, ���豸1-2*/
	unsigned char Country_Code[2];              unsigned char Blend_Tolerance; /*0-99*/
	unsigned char Drive_Off_Lights_Mode;/*O*/
	unsigned char OPT_Light_Mode;/*O*/          unsigned char	Clear_Display_Mode;
	unsigned char Auth_State_Mode;              unsigned char Stand_Alone_Auth;
	unsigned char Max_Auth_Time;                unsigned char Max_Time_W_O_Prog;
	unsigned char Min_Fuelling_Vol;             unsigned char Min_Display_Vol;
	unsigned char Min_Guard_Time;               unsigned char Pulser_Err_Tolerance;
	unsigned char Time_Display_Product_Name;
	/*DISPLAY AND ROUNDING CONFIGURATION:*/
	unsigned char Digits_Vol_Layout;	
	unsigned char Digits_Amount_Layout;         unsigned char Digits_Unit_Price;
	unsigned char Unit_Price_Mult_Fact;         unsigned char Amount_Rounding_Type[2];
	unsigned char Preset_Rounding_Amount;       unsigned char Price_Set_Nb[2];/*O*/
	/*IDENTIFICATION DATA:*/
	unsigned char Manufacturer_Id[3];           unsigned char Dispenser_Model[3];
	unsigned char Calculator_Type[3];           unsigned char Calculator_Serial_No[12];
	unsigned char Appl_Software_Ver[12];        unsigned char W_M_Software_Ver[6];
	unsigned char W_M_Software_Date[4];         unsigned char W_M_Security_Type;
	unsigned char Protocol_Ver[6];              unsigned char SW_Change_Date[4];
	unsigned char SW_Change_Personal_Nb[7];     unsigned char SW_Checksum[4];
	/*ILLUMINATION CONTROL DATA:*/
	unsigned char Calc_Illumination;            unsigned char LCD_Backlight_Switch; /*O*/
	unsigned char Display_Intensity;/*O*/
	/*W&M TRANSACTION SECURITY:*/
	unsigned char W_M_Polynomial[2];/*0-65535*/ /*W(1-9)*/
	unsigned char W_M_Seed[2];/*0-65535*/ /*W(1-9)*/	
}FP_C_DATA;
//FP_C_DATA��������. nodeΪ���ͻ��߼��ڵ��,Ҫ�õ�gpIfsf_shm,���涨��.
//Ĭ�ϳ�ʼ��������.	����ģʽ,���͵�,��Ʒ,�������ͼ���ģʽ��һ��.
int DefaultC_DATA(const char node);
int SetC_DATA(const char node, const FP_C_DATA *fp_c_dat);//���ü�����.�����ͺ�����.
//��дFP_C_DATA����. ע��Ҫ�������Ϣ���ݵ�����,000��,001�ش�,010д,011������Ӧ��,100��������Ӧ��,111Ӧ��
//recvBufferΪ�յ���data,sendBufferΪҪ���͵�data,msg_lgΪ���ɵ��ܳ���.
//��ȡFP_C_DATA. FP_C_DATA��ַΪ1.�������Ϣ���ݵ�����Ϊ001�ش�
int ReadC_DATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg);
//дFP_C_DATA����. �������Ϣ���ݵ�����Ϊ111Ӧ��.
//msg_lgΪ���ɵ��ܳ���,Ϊ0��ʾ����ҪӦ����Ϣ,sendBuffer��msg_lg����ҪӦ���������ΪNULL,.
int WriteC_DATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg);
//ֻдFP_C_DATA�ĵ����������,�����豸�Լ���д���߳�ʼ������.
//data(Data_Id+Data_Lg+Data_El), �ɹ�0,ʧ��-1����Data_Ackֵ.
int SetSglC_DATA(const char node, const unsigned char *data);
int RequestC_DATA(const char node); //Find CD by SendData2CD in gpIfsf_shm, there use default 2:1.
//int TransRecvC_DATA(const int newsock, const unsigned char * recvBuffer);
/*--------------------------------------------------------------*/
#endif

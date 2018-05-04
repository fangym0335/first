/*******************************
	*CopyRight Cuihq
	*All Right Reserved
	*
	*�ļ�����:
	*ժҪ:
	*
	*����: �޺���
	*����: 2004/06/01
*******************************/

#ifndef __DODIGITAL__H__
#define __DODIGITAL__H__

#include "mycomm.h"
#include "dostring.h"


/*�ж�һ���ַ����Ƿ���һ�������������ִ�*/
int IsDigital( const char *dec );
/*�ж�һ���ַ����Ƿ���һ��16���Ƶ����ִ�.������0x,0X,x,X��ʼ*/
int IsHex( const char * hex );
/*�ж�һ���ַ����Ƿ���һ�������Ƶ����ִ�.*/
int	IsBin( const char * bin );

/*��һ��ʮ������ת��Ϊ16�����ַ���*/
void DecToHex( long dec, char *hex );
/*��һ��ʮ������ת��Ϊ�������ַ���*/
void DecToBin( long dec, char *bin );
/*��һ��ʮ�����Ƶ��ַ���ת��Ϊ�������ַ���.ʮ�����ƴ�������0x,0X,x,X��ʼ*/
void HexToBin( const char *hex, char *bin );
/*��һ��ʮ�����Ƶ��ַ���ת��Ϊʮ�����ַ���.ʮ�����ƴ�������0x,0X,x,X��ʼ*/
void HexToDec( const char *hex, long *dec );
/*��һ�������Ƶ��ַ���ת��Ϊʮ�������ַ���.*/
void BinToHex( const char *bin, char *hex );
/*��һ�������Ƶ��ַ���ת��Ϊʮ������*/
void BinToDec( const char *bin, long *dec );

/*�Ƚ�����double�����Ĵ�С 1 a>b, 0 a=b, -1 a<b*/
int FloatComp( double a, double b );
/*ȡ����˫���ȸ�������������*/
double MaxFloat( double a, double b );
/*ȡ����˫���ȸ���������С����*/
double MinFloat( double a, double b );

#endif

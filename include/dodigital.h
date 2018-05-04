/*******************************
	*CopyRight Cuihq
	*All Right Reserved
	*
	*文件名称:
	*摘要:
	*
	*作者: 崔洪清
	*日期: 2004/06/01
*******************************/

#ifndef __DODIGITAL__H__
#define __DODIGITAL__H__

#include "mycomm.h"
#include "dostring.h"


/*判断一个字符串是否是一个正整数的数字串*/
int IsDigital( const char *dec );
/*判断一个字符串是否是一个16进制的数字串.可以以0x,0X,x,X开始*/
int IsHex( const char * hex );
/*判断一个字符串是否是一个二进制的数字串.*/
int	IsBin( const char * bin );

/*将一个十进制数转换为16进制字符串*/
void DecToHex( long dec, char *hex );
/*将一个十进制数转换为二进制字符串*/
void DecToBin( long dec, char *bin );
/*将一个十六进制的字符串转换为二进制字符串.十六进制串可以以0x,0X,x,X开始*/
void HexToBin( const char *hex, char *bin );
/*将一个十六进制的字符串转换为十进制字符串.十六进制串可以以0x,0X,x,X开始*/
void HexToDec( const char *hex, long *dec );
/*将一个二进制的字符串转换为十六进制字符串.*/
void BinToHex( const char *bin, char *hex );
/*将一个二进制的字符串转换为十进制数*/
void BinToDec( const char *bin, long *dec );

/*比较两个double型数的大小 1 a>b, 0 a=b, -1 a<b*/
int FloatComp( double a, double b );
/*取两个双精度浮点数中最大的数*/
double MaxFloat( double a, double b );
/*取两个双精度浮点数中最小的数*/
double MinFloat( double a, double b );

#endif

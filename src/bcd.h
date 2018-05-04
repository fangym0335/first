#ifndef __BCD_H__
#define __BCD_H__

#include <stdio.h>
/*
 *hexתascii
 */
static int ToHex( int x );

/*
 *pbcdתascii
 */
void HexUnpack(unsigned char *in, unsigned char *out, int size );

/*
 *asciiתhex
 */
static int ToBin( int c );
/*
 *pbcdתascii
 */
void HexPack(unsigned  char *in, unsigned char *out, int size );

int Long2Hex( long Num, unsigned char *buf, int len );

int Bcd2Hex( unsigned char *bcd, int lenb, unsigned char *hex, int lenh );

int Bcd2Hex_B( unsigned char *bcd, int lenb, unsigned char *hex, int lenh, int base );

long Hex2Long( unsigned char *buf, int len );

unsigned long long Hex2U64( unsigned char *buf, int len );

int Hex2Bcd( unsigned char *hex, int lenh, unsigned char *bcd, int lenb );

int Hex2ULong(unsigned char *hex, int lenh, unsigned long result);

// Hex2 __u64
unsigned long long Hex2dbLong(const unsigned char *buf, unsigned int len);

//Big end mode  BCD number add an unsigned long number, the result is in bcd and  is also a  big end mode BCD.
//bcdlen is byte size of the bcd, bcdlen>=5, if bcdlen=5, you must be sure not overflow. 
int BBcdAddLong(unsigned char *bcd, const unsigned long num, const int bcdlen);
//Two big end mode BCD number add. result is in bcd1,bcdlen1 is byte size of bcd1,bcdlen2 is byte size of bcd2.
int BBcdAdd(unsigned char *bcd1, const unsigned char *bcd2, const int bcdlen1, const int bcdlen2);
//Two big end mode BCD number compare .result is return, eqeul return 0, big negtive, small postive, bcdlen is byte size of two bcd.
int BBcdCmp(const unsigned char *bcd1, const unsigned char *bcd2, const int bcdlen);
//A big end mode BCD  add 1,bcdlen is byte size of bcd. If overflow, bcd = 1 and return -1. 
int BBcdInc(unsigned char *bcd,  const int bcdlen);
int BBcdDec(unsigned char *bcd,  const int bcdlen);
int BBcdIsZero(const unsigned char *bcd,  const int bcdlen);

//IFSF  BCD number add an unsigned long number, the result is in IFSF bcd and  is also a IFSF BCD.
//bcdlen is byte size of the bcd, bcdlen>=5, if bcdlen=5, you must be sure not overflow. 
int IFSFBcdAddLong(unsigned char *bcd, const unsigned long num, const int bcdlen);
//Two IFSF BCD number add. result is in bcd1,bcdlen1 is byte size of bcd1,bcdlen2 is byte size of bcd2.
int IFSFBcdAdd(unsigned char *bcd1, const unsigned char *bcd2, const int bcdlen1, const int bcdlen2);
//Two IFSF BCD number compare .result is return, eqeul return 0, big negtive, small postive, bcdlen is byte size of two bcd.
int IFSFBcdCmp(const unsigned char *bcd1, const unsigned char *bcd2, const int bcdlen);
//A IFSF BCD  add 1,bcdlen is byte size of bcd. If overflow, bcd = 0001 and return -1. 
int IFSFBcdInc(unsigned char *bcd,  const int bcdlen);
int IFSFBcdIsZero(const unsigned char *bcd,  const int bcdlen);
void Long2BcdL(unsigned long inval, unsigned char *buf, int *bcd_len);
int Long2BcdR(unsigned long inval, unsigned char *buf, int bcd_len);
unsigned long Bcd2Long(const unsigned char *bcd, int bcd_len);

#endif

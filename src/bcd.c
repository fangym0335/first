#include "bcd.h"

const unsigned long bcdops[11] = {1, 1, 10, 100, 1000, 10000, 100000,
					1000000, 10000000, 100000000, 1000000000};

/*
 *hex转ascii
 */
static int ToHex( int x )
{
	if( x >= 0 && x <=9 )
		return( x+'0' );
	else if( x >= 10 && x <= 15 )
		return( x+'A'-10);
	else
		return 0;
}

/*
 *pbcd转ascii
 */
void HexUnpack(unsigned char *in, unsigned char *out, int size )
{
	while( size > 0 )
	{
		*out = (unsigned char)ToHex( (unsigned char)*in/16 );
		out++;
		*out = (unsigned char)ToHex( (unsigned char)*in%16 );
		out++;
		in++;
		size--;
	}
}

/*
 *ascii转hex
 */
static int ToBin( int c )
{
	if( c >= 'A' && c <= 'F' )
		return( c-'A'+10 );
	else if( c >= 'a' && c <= 'f' )
		return( c-'a'+10 );
	else if( c >= '0' && c <= '9' )
		return( c-'0' );
	else
		return 0;
}
/*
 *pbcd转ascii
 */
void HexPack( unsigned char *in, unsigned char *out, int size )
{
	int x[2];
	int l = 2;

	while (size > 0) {
		size--;
		l--;
		x[l] = ToBin(in[size]);

		if (l == 0) {
			out[(size + 1) / 2] = (unsigned char)(x[0] * 16 + x[1]);
			l = 2;
		} else if (size == 0) {
			out[0] = (unsigned char)x[1];
		}
	}
}

int Long2Hex( long Num, unsigned char *buf, int len )
{
	int i;
	long base = 256;

	if (len < 1) {
		return -1;
	}

	for (i = 0; i < len; i++) {
		buf[len - i - 1] = (unsigned char)(Num % base);  //big mode
		Num = Num / base;
	}

	return 0;
}

int Bcd2Hex( unsigned char *bcd, int lenb, unsigned char *hex, int lenh )
{
	unsigned char a[16];
	const unsigned char max_bcd[] = { 0x42, 0x94, 0x96, 0x72, 0x95, 0x00 };
	unsigned long b;
	int ret;

	/*超出unsigned long 的界限*/
	if( lenb > 5 )
	{
		return -1;
	}
	if( lenb == 5 && memcmp( bcd, max_bcd, lenb ) > 0 )
	{
		return -1;
	}

	memset( a, 0, sizeof(a) );
	HexUnpack( bcd, a, lenb );
	b = atol( a );

	ret = Long2Hex( b, hex, lenh );

	return ret;
}

int Bcd2Hex_B( unsigned char *bcd, int lenb, unsigned char *hex, int lenh, int base )
{
	unsigned char a[16];
	const unsigned char max_bcd[] = { 0x42, 0x94, 0x96, 0x72, 0x95, 0x00 };
	unsigned long b;
	unsigned long c = 1L;
	int oper;
	int i;
	int ret;

	/*超出unsigned long 的界限*/
	if( lenb > 5 )
	{
		return -1;
	}
	if( lenb == 5 && memcmp( bcd, max_bcd, lenb ) > 0 )
	{
		return -1;
	}

	if( base > 0 )
		oper = 1;
	else
	{
		base = -base;
		oper = -1;
	}

	for( i = 0; i < base; i++ )
		c = c*10;

	memset( a, 0, sizeof(a) );
	HexUnpack( bcd, a, lenb );
	b = atol( a );
	if( oper > 0 )
		b = b*c;
	else
		b = b/c;

	ret = Long2Hex( b, hex, lenh );

	return ret;
}

long Hex2Long( unsigned char *buf, int len )
{
	int  i;
	long num = 0;
	long base = 1;

	if( len > sizeof( long ) )
		return -1;

	for( i = 0; i < len; i++ ) {
		num = buf[len-1-i]*base+num;
		base = base*256;
	}
	return num;
}

double Hex2Double( unsigned char *buf, int len )
{
	int  i;
	double num = 0;
	double base = 1;

	if( len > sizeof( double ) )
		return -1;

	for( i = 0; i < len; i++ ) {
		num = buf[len-1-i]*base+num;
		base = base*256;
	}
	return num;
}

/*
 * func: 将HEX数转换为无符号长整型
 */

unsigned long long Hex2U64( unsigned char *buf, int len )
{
	int i;
	long base = 1;
	unsigned long long num = 0;
    
	if (len > sizeof(long long)) {
		return -1; 
	}   

	for( i = 0; i < len; i++ ) {
		num = buf[len-1-i]*base+num;
		base = base*256;
	}

	return num;
}




int Hex2Bcd( unsigned char *hex, int lenh, unsigned char *bcd, int lenb )
{
	unsigned char a[33];
	double b;

	if( 2*lenb >= 33 )
		return -1;

	b = Hex2Double( hex, lenh );
//	printf("########Hex2Long: b= %lf", b);
	if (b < 0) {
		return -1;
	}

	snprintf(a, sizeof(a), "%0*.lf", 2*lenb, b);
//	printf("#######Hex2Bcd: %s", a);
	HexPack( a, bcd, 2*lenb );

	return 0;
}
//Big end mode  BCD add a unsigned long number, the result is in bcd and  is also a  big end mode BCD.
//bcdlen is byte size of the bcd, bcdlen>=5, if bcdlen<5, you must be sure not overflow. 
int BBcdAddLong(unsigned char *bcd, const unsigned long number, const int bcdlen){//add by chenfm 2007.7.19
	int i,lg=bcdlen;
	unsigned char longbcd[5],  one, two, tmp;
	unsigned int sum = 0;
	unsigned long hex = number;
	memset(longbcd, 0, 5);	
	//if (bcdlen<5) return -1;
	if((NULL == bcd)  || (number == 0) || (bcdlen <= 0)) 
		return -1;
	//long to big end mode BCD.
	for(i=0;i<4; i++){
		longbcd[4-i]=(hex%10)|( ((hex/10)%10 )<<4);
		hex/=100;
		if (hex == 0) break;
	}
	longbcd[0] = hex;

	//add 
	i = 5;
	while (lg != 0) {
		lg--;
		i--;
		one = bcd[lg];
		if( i >= 0 ){
			two = longbcd[i];
			sum = one + two + sum;
			tmp = (one & 0xf0) + (two & 0xf0 );
			if(   ( tmp !=(sum & 0xf0) )  || ( (sum & 0x0f) >9 )   )
				sum+=6;		
		}
		else {
			sum = one + sum;
			if(   (sum & 0x0f) >9    )
				sum += 6;	
		} //end if.
		if( (sum & 0xf0) >= 0xa0 )
			sum += 0x60;
		bcd[lg] = sum;
		sum >>= 8;
	}
	if (sum > 0) 
		return -2;
	return 0;
}

//Two big end mode BCD number add. result is in bcd1,bcdlen1 is byte size of bcd1,bcdlen2 is byte size of bcd2.
int BBcdAdd(unsigned char *bcd1, const unsigned char *bcd2, const int bcdlen1, const int bcdlen2){
	int i= bcdlen2,lg=bcdlen1;
	unsigned char  one, two, tmp;
	unsigned int sum = 0;
	if((NULL == bcd1) || (NULL == bcd2) || (bcdlen1 <= 0) || (bcdlen2 <= 0)) 
		return -1;
	//add 	
	while (lg != 0) {
		lg--;
		i--;
		one = bcd1[lg];
		if( i >= 0 ){
			two = bcd2[i];
			sum = one + two + sum;
			tmp = (one & 0xf0) + (two & 0xf0 );
			if(   ( tmp !=(sum & 0xf0) )  || ( (sum & 0x0f) >9 )   )
				sum+=6;		
		}
		else {
			sum = one + sum;
			if(   (sum & 0x0f) >9    )
				sum += 6;	
		} //end if.
		if( (sum & 0xf0) >= 0xa0 )
			sum += 0x60;
		bcd1[lg] = sum;
		sum >>= 8;
	}
	if (sum > 0)  //over flow.
		return -1;
	return 0;
}



//Two big end mode BCD number compare .result is return, eqeul return 0, big negtive, small postive, bcdlen is byte size of two bcd.
/*
 * 此函数不完善, 勿用
 */
int BBcdCmp(const unsigned char *bcd1, const unsigned char *bcd2, const int bcdlen)
{
	int i, ret = -1;

	if((NULL == bcd1) || (NULL == bcd2) || (bcdlen <= 0)) 
		return -1;
	for(i=0; i<bcdlen; i++){
		if( (ret = (int)( bcd1[i]&0xf0)  - (int)(bcd2[i]&0xf0) ) != 0)
			break;
		else if ( (ret = (int)(bcd1[i]&0x0f)  - (int)(bcd2[i]&0x0f)) != 0)
			break;
	}
	
	return ret;
}

//A big end mode BCD  add 1,bcdlen is byte size of bcd. If overflow, bcd = 1 and return 1. 
int BBcdInc(unsigned char *bcd,  const int bcdlen)
{
	int i;
	int lg = bcdlen;
	unsigned char one;
	unsigned int sum = 0;

	if((NULL == bcd) || (bcdlen <= 0)) {
		return -1;
	}

	//add
	lg--;
	one = bcd[lg];
	sum = one + sum+1;

	if ((sum & 0x0f) > 9) {
		sum += 6;
	}

	if ((sum & 0xf0) >= 0xa0) {
		sum += 0x60;
	}

	bcd[lg] = sum;
	sum >>= 8;
	while (lg != 0 && sum != 0) {
		lg--;
		one = bcd[lg];
		sum = one + sum;

		if ((sum & 0x0f) > 9) {
			sum += 6;
		}
		//} //end if.
		if ((sum & 0xf0) >= 0xa0) {
			sum += 0x60;
		}
		bcd[lg] = sum;
		sum >>= 8;
	}

	if (sum > 0) { //over flow.
		for(i=0; i<bcdlen-1; i++){
			bcd[i] = '\0';
		}
		bcd[bcdlen-1] = '\x01';
		return 1;
	}

	return 0;
}

int BBcdIsZero(const unsigned char *bcd,  const int bcdlen)
{
	int i;

	if(  (NULL == bcd) ||  (bcdlen <= 0) ) 
		return -1;
	for(i=0; i < bcdlen; i++) {
		if (   ( ( bcd[i] &0xf0 ) != (unsigned char)0 ) || ( ( bcd[i]&0x0f) != (unsigned char)0 )  ) 
			break;
	}
	if(i == bcdlen) {
		return -1;
	}

	return 0;
}

//IFSF BCD add a unsigned long number, the result is in bcd and  is also a  big end mode BCD.
//bcdlen is byte size of the bcd, bcdlen>=5, if bcdlen<5, you must be sure not overflow. 
int IFSFBcdAddLong(unsigned char *bcd, const unsigned long number, const int bcdlen){//add by chenfm 2007.7.19
	int i,lg=bcdlen - 1;
	unsigned char longbcd[5],  one, two, tmp;
	unsigned char *bcdok ;
	unsigned int sum = 0;
	unsigned long hex = number;
	memset(longbcd, 0, 5);	
	//if (bcdlen<5) return -1;
	if((NULL == bcd)  || (number == 0) || (bcdlen <= 0)) 
		return -1;
	bcdok = bcd + 1;
	//long to big end mode BCD.
	for(i=0;i<4; i++){
		longbcd[4-i]=(hex%10)|( ((hex/10)%10 )<<4);
		hex/=100;
		if (hex == 0) break;
	}
	longbcd[0] = hex;

	//add 
	i = 5;
	while (lg != 0) {
		lg--;
		i--;
		one = bcdok[lg];
		if( i >= 0 ){
			two = longbcd[i];
			sum = one + two + sum;
			tmp = (one & 0xf0) + (two & 0xf0 );
			if(   ( tmp !=(sum & 0xf0) )  || ( (sum & 0x0f) >9 )   )
				sum+=6;		
		}
		else {
			sum = one + sum;
			if(   (sum & 0x0f) >9    )
				sum += 6;	
		} //end if.
		if( (sum & 0xf0) >= 0xa0 )
			sum += 0x60;
		bcdok[lg] = sum;
		sum >>= 8;
	}
	if (sum > 0) 
		return -2;
	return 0;
}

//Two IFSF BCD number add. result is in bcd1,bcdlen1 is byte size of bcd1,bcdlen2 is byte size of bcd2.
int IFSFBcdAdd(unsigned char *bcd1, const unsigned char *bcd2, const int bcdlen1, const int bcdlen2){
	int i= bcdlen2-1,lg=bcdlen1-1;
	unsigned char  one, two, tmp;
	unsigned int sum = 0;
	unsigned char *bcd1ok; 
	const unsigned char *bcd2ok;
	
	if((NULL == bcd1) || (NULL == bcd2) || (bcdlen1 <= 0) || (bcdlen2 <= 0)) 
		return -1;
	bcd1ok = &bcd1[1] ;

	
	bcd2ok = &bcd2[1]; 
	//add 	
	while (lg != 0) {
		lg--;
		i--;
		one = bcd1ok[lg];
		if( i >= 0 ){
			two = bcd2ok[i];
			sum = one + two + sum;
			tmp = (one & 0xf0) + (two & 0xf0 );
			if(   ( tmp !=(sum & 0xf0) )  || ( (sum & 0x0f) >9 )   )
				sum+=6;		
		}
		else {
			sum = one + sum;
			if(   (sum & 0x0f) >9    )
				sum += 6;	
		} //end if.
		if( (sum & 0xf0) >= 0xa0 )
			sum += 0x60;
		bcd1ok[lg] = sum;
		sum >>= 8;
	}
	if (sum > 0)  //over flow.
		return -1;
	return 0;
}

//Two IFSF BCD number compare .result is return, eqeul return 0, big negtive, small postive, bcdlen is byte size of two bcd.
int IFSFBcdCmp(const unsigned char *bcd1, const unsigned char *bcd2, const int bcdlen){
	
	int ret;

	if((NULL == bcd1) || (NULL == bcd2) || (bcdlen <= 0)) {
		return -1;
	}

	ret = memcmp( (void *)bcd1,(void *)bcd2,bcdlen);
	
	return ret;
}

//A IFSF BCD  add 1,bcdlen is byte size of bcd. If overflow, bcd = 1 and return -1. 
int IFSFBcdInc(unsigned char *bcd,  const int bcdlen){
	int lg= bcdlen;
	int i;
	unsigned int sum = 0;
	unsigned char  one;

	if((NULL == bcd) || (bcdlen <= 0)) 
		return -1;
	//add
	lg--;
	one = bcd[lg];
	sum = one + sum+1;
	if(   (sum & 0x0f) >9    )
		sum += 6;	
	if( (sum & 0xf0) >= 0xa0 )
		sum += 0x60;
	bcd[lg] = sum;
	sum >>= 8;
	while (lg != 0 && sum != 0){		
		lg--;
		one = bcd[lg];
		sum = one + sum;
		if(   (sum & 0x0f) >9    )
			sum += 6;	
		//} //end if.
		if( (sum & 0xf0) >= 0xa0 )
			sum += 0x60;
		bcd[lg] = sum;
		sum >>= 8;
	}
	if (sum > 0) { //over flow.
		for(i=0; i<bcdlen-1; i++){
			bcd[i] = '\0';
			}
		bcd[bcdlen-1] = '\x01' ;
		return -1;
		}
	return 0;
}

int IFSFBcdIsZero(const unsigned char *bcd,  const int bcdlen)
{
	int i;

	if ((NULL == bcd) ||  (bcdlen <= 0)) {
		return 0;
	}

	for (i = 1; i < bcdlen; i++) {//bcd[0] is flag, not used it.
		if (bcd[i] != (unsigned char)0) {
			return 0;
		}
	}

	if (i == bcdlen) {
		return 1;
	}

	return 0;
}

/*
 * 有问题
 */

#if 1
int Hex2ULong(unsigned char *hex, int lenh, unsigned long result)
{	
	int i;
	int base = 256;

	if (lenh > sizeof(unsigned long)) {
		return -1;
	}

	result = 0;
	for (i = 0; i < lenh; i++) {
		result = result * base + hex[i];
	}

	return 0;
}
#endif


/*
 * A big end mode BCD  dec 1,bcdlen is byte size of bcd. 
 */
int BBcdDec(unsigned char *bcd, const int bcdlen) 
{
	int i;
	unsigned char lsb = 0, msb = 0;

	for (i = bcdlen - 1; i >= 0; i--) {
		lsb = bcd[i] & 0x0F;
		msb = bcd[i] & 0xF0;

		if (lsb != 0) {
			bcd[i] = msb | (lsb - 1);
			break;
		} else if (msb != 0) {
			bcd[i] = (((msb >> 4) - 1) << 4) | 0x09;
			break;
		}

	}

	if ((i >= 0 && (i < bcdlen - 2)) || i < 0) {
		memset(&bcd[i + 1], 0x99, bcdlen - i - 1);
	}

	return 0;
}

/*
 * func: 将BCD数转换为无符号长整型
 */

unsigned long long Bcd2U64(const unsigned char *bcd, int len)
{
	int i;
	unsigned long long power = 1;
	unsigned long long ret = 0;
    
	if (len > 8) {
		return -1; 
	}   

	for (i = len - 1; i > -1; --i) {
		ret += (((bcd[i] & 0x0F) + ((bcd[i] >> 4) * 10)) * power);
		power *= 100;
	}   

        return ret; 
}


/*
 * func: 16进制转为 __u64
 */
unsigned long long Hex2dbLong(const unsigned char *buf, unsigned int len)
{
	int i;
	unsigned long long base = 1;
	unsigned long long ret = 0;

	if (len > sizeof(long long)) {
		return -1;
	}

	for (i = len - 1; i >= 0; --i) {
		ret += buf[i] * base;
		base *= 256;
	}

	return ret;
}

/*
 * func: 长整型转换为压缩BCD, 在bcd缓冲中以左对齐方式存放, bcd_len参数返回bcd数据字节数
 */
void Long2BcdL(unsigned long inval, unsigned char *bcd, int *bcd_len)
{
	int i, j;
	int len;
	unsigned char tmp = 0;

	for (i = 10; i >= 0; --i) {
		tmp = inval / bcdops[i];
		if (tmp == 0) {
			continue;
		}
		break;
	}

	len = i;          // digit bits
	*bcd_len = (i + 1) / 2; 

	memset(bcd, 0, *bcd_len);

	for (i = len, j = 0; i >= 0; --i) {
		tmp = inval / bcdops[i];
		inval = inval % bcdops[i];

		if ((i % 2) == 0) {
			bcd[j] = (tmp << 4);
//			printf("--- i: %d, j: %d, tmp: %d, bcd[%d]: %02x\n", i, j, tmp, j, bcd[j]);
		} else {
			bcd[j] |= tmp;
//			printf("--- i: %d, j: %d, tmp: %d, bcd[%d]: %02x\n", i, j, tmp, j, bcd[j]);
			++j;
		}
	}

}

/*
 * func: 长整型转换为压缩BCD, 在bcd缓冲中以右对齐方式存放, bcd_len 传入存放结果的缓冲区buf的长度
 * return: -1 -- 缓冲区太小, 0 -- 成功
 */
int Long2BcdR(unsigned long inval, unsigned char *buf, int bcd_len)
{
	unsigned char tmp = 0;
	unsigned char bcd[16] = {0};
	int i, j;
	int digit_len, result_len;

	for (i = 10; i >= 0; --i) {
		tmp = inval / bcdops[i];
		if (tmp == 0) {
			continue;
		}
		break;
	}

	digit_len = i;              // digit bits
	result_len = (digit_len + 1) / 2;
	if (result_len > bcd_len) {
		return -1;          // size of buf too small
	}

	for (i = digit_len, j = 0; i >= 0; --i) {
		tmp = inval / bcdops[i];
		inval = inval % bcdops[i];

		if ((i % 2) == 0) {
			bcd[j] = (tmp << 4);
//			printf("--- i: %d, j: %d, tmp: %d, bcd[%d]: %02x\n", i, j, tmp, j, bcd[j]);
		} else {
			bcd[j] |= tmp;
//			printf("--- i: %d, j: %d, tmp: %d, bcd[%d]: %02x\n", i, j, tmp, j, bcd[j]);
			++j;
		}
	}

	memset(buf, 0, bcd_len);
	memcpy(&buf[bcd_len - result_len], bcd, result_len);

	return 0;
}


//BCD  减法bcd3 = bcd1 - bcd2
int BBcdMinus(unsigned char *bcd1, int lenb1, unsigned char *bcd2, int lenb2, unsigned char *bcd3)
{
	unsigned char a[16];
	unsigned long b;
	int ret,len;

	if (lenb1 > 5 || lenb2 > 5) {
		return -1;
	}
	
	b = 0;
	memset(a, 0, sizeof(a));
	HexUnpack(bcd1, a, lenb1);
	b = b + atol(a);

	memset(a, 0, sizeof(a));
	HexUnpack(bcd2, a, lenb2);
	b = b - atol(a);

	len = 5;
	
	ret = Long2BcdR(b, bcd3, len);

	return ret;
}


/*
 * func: 将BCD数据转换为无符号长整型
 */
unsigned long Bcd2Long(const unsigned char *bcd, int bcd_len)
{
	int i;
	unsigned long ret = 0;

	for (i = 0; i < bcd_len; ++i) {
		ret = (ret * 100) + ((bcd[i] >> 4) * 10) + (bcd[i] & 0x0F);
	}

	return ret;
}


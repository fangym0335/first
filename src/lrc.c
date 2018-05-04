#include <assert.h>
#include "lrc.h"


/*
 * 校验或产生CRC
 * FLAG = EN_CRC 使用buf中len长度的数据生成CRC校验位,存放在len之后的两个字节
 * FLAG = UN_CRC 使用buf中len长度的数据生成CRC校验位与,存放在len之后的两个字节对比
 * 0 成功, -1失败
 */
int  DoCRC( unsigned char *buf, long len, int flag )
{
	int i;
	unsigned short crc16 = 0xffff;
	unsigned char  *p = NULL;

	for( i = 0; i < len; i++ )
		crc16 = UPDC16(buf[i], crc16);

	p = (char *)&crc16;

	if( flag == UN_CRC ) {
		if( buf[len] != p[0] || buf[len+1] != p[1] ) {
			return -1;
		}
	} else {
		buf[len] = p[0];
		buf[len+1] = p[1];
		return 0;
	}
	return 0;
}

/*
 * 进行CRC计算，crc为需要继续参与计算的值
 */
int  DoCRC2( unsigned char *buf, long len, unsigned short *crc )
{
	int i;
	unsigned char *p = NULL;

	for( i = 0; i < len; i++ )
	{
		if( buf[i] == 0x0a
			|| buf[i] == 0x0d )
			continue;

		*crc = UPDC16(buf[i], *crc);
	}

	return 0;
}

/*
 * 计算LRC
 * LRC方法是将消息中的8Bit的字节连续累加，丢弃了进位
 */
unsigned char LRC( unsigned char *buf, long len )
{
	int i;
	unsigned char lrc = 0;	/*lrc字节初始化*/

	for( i = 0; i < len; i++ )
	{
		lrc = lrc + buf[i];
	}

	return ((unsigned char)(-((char)lrc)));
}

/* 
 * 异或校验, 返回校验值
 */

unsigned char Check_xor(const unsigned char *p, int count)
{
	int i;
	unsigned char ret = 0x00;

	for (i = 0; i < count; ++i) {
		ret ^= p[i];
	}

	return ret;
}


/*
*  func: 宝德尔/老长空异或校验
*/
unsigned char Check_xor_bdr(const unsigned char *p, int count)
{	
	int i = 0;
	unsigned char xdata = 0x00;

	for (i = 0; i < count; i++){
		xdata = p[i] ^ xdata;
	}

	return xdata & 0x7f;		//disable the MSB
}

/*长空LRC校验*/
unsigned char LRC_CK(const unsigned char *buf, long len )
{
	int i;
	unsigned char lrc = 0;	/*lrc字节初始化*/

	for( i = 0; i < len; i++ ) {
		lrc = lrc + buf[i];
	}

	lrc = ~lrc + 1;
	
	return ((unsigned char)(lrc & 0x0f));
}


/*
 * func: 查表法计算crc16校验值 (poly: 0xA001)
 */
unsigned short CRC_16(const unsigned char *buf, int len)
{
	int i;
	unsigned short crc = 0;

	assert(buf != NULL);

	for (i = 0; i < len; ++i) {
		crc = (unsigned short)(crc_16_tab[(buf[i] ^ crc) & 0xFF] ^ (crc >> 8));
	}

	return crc;
}

unsigned short CRC_16_guihe(const unsigned char *buf, int len)
{
	int i;
	unsigned short crc = 0xffff;

	assert(buf != NULL);

	for (i = 0; i < len; ++i) {
		crc = (unsigned short)(crc_16_tab[(buf[i] ^ crc) & 0xFF] ^ (crc >> 8));
	}

	return crc;
}


/*
 * func: 榕兴CRC16校验计算
 */
int CRC_16_RX(const unsigned char *buf, size_t len, unsigned char *CrcHi, unsigned char *CrcLo)
{
	int i;
	unsigned short crc;

	crc = 1;

	for( i = 0; i < len; i++ ) {
		crc = (unsigned short)(crc_16_tab[(buf[i] ^ crc) & 0xFF] ^ (crc >> 8));
	}

	*CrcLo = crc & 0x7f;           // 取低 15 bits
	*CrcHi = (crc >> 8) & 0x7f;    // 取低 15 bits

	return 0;
}

/*
 * func: 查表法计算crc16校验值 (poly: )
 *       for 恒山自助加油机
 */
unsigned short CRC_16_HS(const unsigned char *buf, int len)
{
	int i;
	unsigned short crc = 0;

	assert(buf != NULL);

	for (i = 0; i < len; ++i) {
		crc = (unsigned short)(crc_16_tab_ccnt[(buf[i] ^ crc) & 0xFF] ^ (crc >> 8));
	}

	return crc;
}

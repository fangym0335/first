#include <assert.h>
#include "lrc.h"


/*
 * У������CRC
 * FLAG = EN_CRC ʹ��buf��len���ȵ���������CRCУ��λ,�����len֮��������ֽ�
 * FLAG = UN_CRC ʹ��buf��len���ȵ���������CRCУ��λ��,�����len֮��������ֽڶԱ�
 * 0 �ɹ�, -1ʧ��
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
 * ����CRC���㣬crcΪ��Ҫ������������ֵ
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
 * ����LRC
 * LRC�����ǽ���Ϣ�е�8Bit���ֽ������ۼӣ������˽�λ
 */
unsigned char LRC( unsigned char *buf, long len )
{
	int i;
	unsigned char lrc = 0;	/*lrc�ֽڳ�ʼ��*/

	for( i = 0; i < len; i++ )
	{
		lrc = lrc + buf[i];
	}

	return ((unsigned char)(-((char)lrc)));
}

/* 
 * ���У��, ����У��ֵ
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
*  func: ���¶�/�ϳ������У��
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

/*����LRCУ��*/
unsigned char LRC_CK(const unsigned char *buf, long len )
{
	int i;
	unsigned char lrc = 0;	/*lrc�ֽڳ�ʼ��*/

	for( i = 0; i < len; i++ ) {
		lrc = lrc + buf[i];
	}

	lrc = ~lrc + 1;
	
	return ((unsigned char)(lrc & 0x0f));
}


/*
 * func: �������crc16У��ֵ (poly: 0xA001)
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
 * func: ����CRC16У�����
 */
int CRC_16_RX(const unsigned char *buf, size_t len, unsigned char *CrcHi, unsigned char *CrcLo)
{
	int i;
	unsigned short crc;

	crc = 1;

	for( i = 0; i < len; i++ ) {
		crc = (unsigned short)(crc_16_tab[(buf[i] ^ crc) & 0xFF] ^ (crc >> 8));
	}

	*CrcLo = crc & 0x7f;           // ȡ�� 15 bits
	*CrcHi = (crc >> 8) & 0x7f;    // ȡ�� 15 bits

	return 0;
}

/*
 * func: �������crc16У��ֵ (poly: )
 *       for ��ɽ�������ͻ�
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

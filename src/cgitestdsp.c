
#include "pub.h"
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "cgidecoder.h"

#define DRV_FIELD_LEN 63
#define DRV_MAX_CNT 256
typedef struct test_drv_cfg{
	char driverNum[DRV_FIELD_LEN + 1];//�����������
	char driverName[DRV_FIELD_LEN + 1];//�豸�ͺ�����,��:����A.
	char testWrite[DRV_FIELD_LEN + 1];//����д������
	char testReturn[DRV_FIELD_LEN + 1];//����������������
}DRV_CFG;

/*1B�����ʽ(16����)��
//1B + ����ͨ����3x��30-3F ��ӦchnPhy 1-16�� + ���ݳ��� + ���� 
//eg. ����ͨ��2���ӵ��Ǻ�ɽ���ͻ�����ѯ��������
//	1B 31 05 FF 02 A0 A0
//
//1.����A
//��ѯ��1B3x01 01,���أ�0601
//2.��ɽ
//��ѯ��1B3x05 FF02A0A0,  ���أ�FF12A0+ ��
//3.̫��
//1B3x0C AA0A + ǹ�� +01 + 7BCDʱ�䣨eg.20071126180000��+ У��ͣ�ǹ��+01+7BCDʱ������ֵ����
//BB06 + ǹ�� +01 + �� 
//4.����
//1B3x07 AA5502 + ǹ�� + 00 + 2�ֽ�CRCУ���, AA55 + ���ݳ��ȣ��������� + ǹ�� + ���� ��
//5.����
// 1B3x06 FCFCFC34 + ǹ��+ У��ͣ�ǹ�ź�34�����ֵ��, FCFCFC34��  �� FCFCFC35 ��
//6.����1
//1B3x04 F5A2D5 + 82��ǰ3���ֽڵ����ֵ�� , F5A3 + ״̬ + D5 + (ǰ4���ֽڵ����ֵ) 
//7.����B 
//1B3x01 01, 0601
//8.����
//1b3x06 01bb031000cf,  01bb030000BF
//9.����	
//1b3x10 + ��ѯ����->��ѯ�����ʽ��fa00000000830 + 7�ֽ�ʱ��(����ʱ����) + 2�ֽ�CRC16У����,
//fa00000000025a �� ���� fa00000000075a ��
//10. ����2 
//����2��ͬ����
//11. ����3 
//��ѯ״̬��1b3x + Ap->����A P:P	--	0 �� 15	ͨѶ���ϼ��ͻ���ͨѶ��ַ(���̱�̵ļ��ͻ���ַͨѶ��ַ)
//��ӦS P:S->0 ����״̬ ����ʱ״̬��,1-6����״̬
//12. ����
//1B3x07 FCFCFC80 + ǹ�� + 2�ֽ�У��� ��80��ǹ�ŵ�CRCУ��ֵ��,  FCFCFC80 + ǹ�� + ��
//13. ����
//1B3x06  EACD + ǹ�� + 010102,EACD + ǹ�� + 1201 + ��, EACD + ǹ�� + 1201 + ��
//14. ����
// 1B3x AA + ǹ�� + C510 + У��ͣ�ǹ��+C510�����ֵ��,BB + ǹ�� + CE10 + ��,BB + ǹ�� + CE10 + ��
//15. ������
//1B3x06 + CC + ǹ�� + 03301121,  CC + ǹ��+ 0330 + 11  ( 11��ʾͨѶ�ɹ�������ʧ��)+ 21
//16. ̫��(��չ)
//̫��(��չ), ̫ͬ�գ�3��
*/
DRV_CFG  drv_cfg[DRV_MAX_CNT] ={
		//������
		{"0",		"����",		"1B30050102030405",					"31013102310331043105"		},		
		{"1",		"����A",		"1B3X0101",							"0601"		},
		{"2",		"��ɽ",		"1B3X05FF02A0A0",					"FF12A0"		},
		{"3",		"̫��",		"1B3X0CAA0AGG01TTTTTTTTTTTTTTRR",	"BB06"		},//XX01
		{"4",		"����",		"1B3X07AA5502RR00",					"AA55"		},
		{"5",		"����",		"1B3X06FCFCFC34GGRR",				"FCFCFC"		},
		{"6",		"����1",		"1B3X04F5A2D5RR",					"F5A3"		},//F5A3SSD5 
		{"7",		"����B",		"1B3X0101",							"0601"		},
		{"8",		"����",		"1B3X0601BB031000CF",				"01BB030000BF"		},	
		{"9",		"����",		"1B3X10FA00000000830TTTTTTTTTTTTTTRRRR",		"FA00000000"		},
		{"10",		"����2",		"1B3X0101",							"0601"		},
		{"11",		"����3",		"1B3XPP",							"PP"		},//AP,SP
		{"12",		"����",		"1B3X07FCFCFC80GGXXXX",				"FCFCFC80"		},
		{"13",		"����",		"1B3X06EACDGG010102",				"EACD"		},
		{"14",		"����",		"1B3XAAGGC510XX",					"BB"		},
		{"15",		"������",		"1B3X07AA5502RR00",					"CCGG033011"		},
		{"16",	"̫��(��չ)",	"1B3XPP",							"PP"		}//AP,SP
};
//#define DRV_CFG__FILE "etc/dspdrv.cfg"  
//#define DF_WRK_DIR	"/tmp/"  

char alltty[][11] =
{
	"/dev/ttyS1",
	"/dev/ttyS2",
	"/dev/ttyS3",
	"/dev/ttyS4",
	"/dev/ttyS5",
	"/dev/ttyS6"
};

struct chncmd{
	unsigned char	wantlen;
	int				indtyp;
	unsigned char	len;
	unsigned char	cmd[MAX_ORDER_LEN+1];
};
/*��ʱע��,�п��ټ�����
static int MakeDrvTest1(unsigned char  chnPhy, unsigned char gunPhy){
	#define DRIVER_NUM 1
	int i;
	char temp[DRV_FIELD_LEN + 1];
	
	if(chnPhy >16){
		char pbuff[256];
		bzero( pbuff,sizeof(pbuff) );
		sprintf( pbuff, "����ͨ���Ŵ���,����0-16,ֵΪ[%d]", chnPhy );
		cgi_error(pbuff);
		exit(1);
		//return -1;
	}
	//ȷ��3Xֵ,XΪ����ͨ����
	bzero(temp,sizeof(temp));
	sprintf(temp, "%02x",chnPhy |0x30);
	drv_cfg[DRIVER_NUM].testWrite[2] = temp[0];
	drv_cfg[DRIVER_NUM].testWrite[3] = temp[1];
	return 0;
}
static int MakeDrvTest2(unsigned char  chnPhy, unsigned char gunPhy){
	int i;
	char temp[DRV_FIELD_LEN + 1];
	
	if(chnPhy >16){
		char pbuff[256];
		bzero( pbuff,sizeof(pbuff) );
		sprintf( pbuff, "����ͨ���Ŵ���,����0-16,ֵΪ[%d]", chnPhy );
		cgi_error(pbuff);
		exit(1);
		//return -1;
	}
	bzero(temp,sizeof(temp));
	sprintf(temp, "%02x",chnPhy |0x30);
	drv_cfg[2].testWrite[2] = temp[0];
	drv_cfg[2].testWrite[3] = temp[1];
	return 0;
}
static unsigned char Check_xor43(unsigned char *p, int count1)
{	int count = count1-1;
	unsigned char buf[count1];
	int k=0;

//	printf( "count1 = [%d], sizeof(buf) = [%d]\n", count1, sizeof(buf) );
	memset(buf,0,count1);
	memcpy(buf,p,count);
	for(k=0;k<count;k++){
//		printf("befor xor: %x, %x\n",buf[count],buf[k]);
		buf[count] ^= buf[k];
//		printf("after xor: %x\n",buf[count],buf[k]);
		}
	return buf[count];
}
static int MakeDrvTest3(unsigned char  chnPhy, unsigned char gunPhy){
	int i,ret;
	char temp[DRV_FIELD_LEN + 1];
	char mybcdtime[16];
	
	if(chnPhy > 16){
		char pbuff[256];
		bzero( pbuff,sizeof(pbuff) );
		sprintf( pbuff, "����ͨ���Ŵ���,����0-16,ֵΪ[%d]", chnPhy );
		cgi_error(pbuff);
		exit(1);
		//return -1;
	}
	//ȷ��3Xֵ,XΪ����ͨ����
	bzero(temp,sizeof(temp));
	sprintf(temp, "%02x",chnPhy |0x30);
	drv_cfg[3].testWrite[2] = temp[0];
	drv_cfg[3].testWrite[3] = temp[1];
	//ȷ��GGֵ,��:����ǹ��.
	bzero(temp,sizeof(temp) );
	sprintf(temp, "%02x", gunPhy);
	drv_cfg[3].testWrite[1] = temp[0];
	//ȷ��TTTTTTTTTTTTTTֵ,��:��ǰʱ��.
	bzero(mybcdtime,sizeof(mybcdtime));
	ret = getbcdtime(mybcdtime);
	if(ret < 0){
		char pbuff[256];
		bzero( pbuff,sizeof(pbuff) );
		sprintf( pbuff, "��ȡ��ǰʱ��ֵ����,����ʱ��ֵΪ[%s],����.", temp );
		cgi_error(pbuff);
		exit(1);
		//return -1;
	}
	bzero(temp,sizeof(temp));
	sprintf(temp, "%02x%02x%02x%02x%02x%02x%02x",  mybcdtime[0],  mybcdtime[1],  mybcdtime[2],  mybcdtime[3],  mybcdtime[4],  mybcdtime[5],  mybcdtime[6]);
	memcpy( &(drv_cfg[3].testWrite[14]), temp, 14 );
	//ȷ��RRֵ,��:У���.
	bzero(temp,sizeof(temp));
	for(i=0; i<9; i++){
		temp[i] = hexchr2hex(drv_cfg[3].testWrite[10+2*i] ) * 16 +  hexchr2hex(drv_cfg[3].testWrite[11+2*i] );
	}
	ret = Check_xor43(temp, 9);
	sprintf(temp, "%02x", ret);
	memcpy( &(drv_cfg[3].testWrite[28]) , temp, 2);
	//У��������ַ����Ƿ�Ϸ���0-9,a-f.
	for(i=0;i<30, i++){
		if( (drv_cfg[3].testWrite[i] >='0')  && ((drv_cfg[3].testWrite[i] <='9') ){
			continue;
		}
		else if( (drv_cfg[3].testWrite[i] >='A')  && ((drv_cfg[3].testWrite[i] <='Z') ){
			continue;
		}
		else if( (drv_cfg[3].testWrite[i] >='a')  && ((drv_cfg[3].testWrite[i] <='z') ){
			continue;
		}
		else {
			break;
		}
	}
	drv_cfg[3].testWrite[30] = 0;
	if(i < 30) {
		char pbuff[256];
		bzero( pbuff,sizeof(pbuff) );
		sprintf( pbuff, "д�봮�ڵ�ֵ����,ֵΪ[%s],����.", drv_cfg[3].testWrite );
		cgi_error(pbuff);
		exit(1);
		//return -1;
	}
	
	return 0; 
}


static unsigned int CrcData44[] = {
	0x0000, 0xc0c1, 0xc181, 0x0140, 0xc301, 0x03c0, 0x0280, 0xc241, 0xc601, 0x06c0, 0x0780, 0xc741, 0x0500, 0xc5c1, 0xc481, 0x0440,
	0xcc01, 0x0cc0, 0x0d80, 0xcd41, 0x0f00, 0xcfc1, 0xce81, 0x0e40, 0x0a00, 0xcac1, 0xcb81, 0x0b40, 0xc901, 0x09c0, 0x0880, 0xc841,
	0xd801, 0x18c0, 0x1980, 0xd941, 0x1b00, 0xdbc1, 0xda81, 0x1a40, 0x1e00, 0xdec1, 0xdf81, 0x1f40, 0xdd01, 0x1dc0, 0x1c80, 0xdc41,
	0x1400, 0xd4c1, 0xd581, 0x1540, 0xd701, 0x17c0, 0x1680, 0xd641, 0xd201, 0x12c0, 0x1380, 0xd341, 0x1100, 0xd1c1, 0xd081, 0x1040,
	0xf001, 0x30c0, 0x3180, 0xf141, 0x3300, 0xf3c1, 0xf281, 0x3240, 0x3600, 0xf6c1, 0xf781, 0x3740, 0xf501, 0x35c0, 0x3480, 0xf441,
	0x3c00, 0xfcc1, 0xfd81, 0x3d40, 0xff01, 0x3fc0, 0x3e80, 0xfe41, 0xfa01, 0x3ac0, 0x3b80, 0xfb41, 0x3900, 0xf9c1, 0xf881, 0x3840,
	0x2800, 0xe8c1, 0xe981, 0x2940, 0xeb01, 0x2bc0, 0x2a80, 0xea41, 0xee01, 0x2ec0, 0x2f80, 0xef41, 0x2d00, 0xedc1, 0xec81, 0x2c40,
	0xe401, 0x24c0, 0x2580, 0xe541, 0x2700, 0xe7c1, 0xe681, 0x2640, 0x2200, 0xe2c1, 0xe381, 0x2340, 0xe101, 0x21c0, 0x2080, 0xe041,
	0xa001, 0x60c0, 0x6180, 0xa141, 0x6300, 0xa3c1, 0xa281, 0x6240, 0x6600, 0xa6c1, 0xa781, 0x6740, 0xa501, 0x65c0, 0x6480, 0xa441,
	0x6c00, 0xacc1, 0xad81, 0x6d40, 0xaf01, 0x6fc0, 0x6e80, 0xae41, 0xaa01, 0x6ac0, 0x6b80, 0xab41, 0x6900, 0xa9c1, 0xa881, 0x6840,
	0x7800, 0xb8c1, 0xb981, 0x7940, 0xbb01, 0x7bc0, 0x7a80, 0xba41, 0xbe01, 0x7ec0, 0x7f80, 0xbf41, 0x7d00, 0xbdc1, 0xbc81, 0x7c40,
	0xb401, 0x74c0, 0x7580, 0xb541, 0x7700, 0xb7c1, 0xb681, 0x7640, 0x7200, 0xb2c1, 0xb381, 0x7340, 0xb101, 0x71c0, 0x7080, 0xb041,
	0x5000, 0x90c1, 0x9181, 0x5140, 0x9301, 0x53c0, 0x5280, 0x9241, 0x9601, 0x56c0, 0x5780, 0x9741, 0x5500, 0x95c1, 0x9481, 0x5440,
	0x9c01, 0x5cc0, 0x5d80, 0x9d41, 0x5f00, 0x9fc1, 0x9e81, 0x5e40, 0x5a00, 0x9ac1, 0x9b81, 0x5b40, 0x9901, 0x59c0, 0x5880, 0x9841,
	0x8801, 0x48c0, 0x4980, 0x8941, 0x4b00, 0x8bc1, 0x8a81, 0x4a40, 0x4e00, 0x8ec1, 0x8f81, 0x4f40, 0x8d01, 0x4dc0, 0x4c80, 0x8c41,
	0x4400, 0x84c1, 0x8581, 0x4540, 0x8701, 0x47c0, 0x4680, 0x8641, 0x8201, 0x42c0, 0x4380, 0x8341, 0x4100, 0x81c1, 0x8081, 0x4040
};
static int GetCrcData44( const unsigned char *cmd, size_t len, unsigned char *CrcHi, unsigned char *CrcLo ) {
	int i;
	int tmpNum;

	tmpNum = 1;

	//RunLog( "����Crc.Data=[%s],len=[%d]", Asc2Hex((unsigned char*)cmd,len), len );
	for( i = 0; i < len; i++ )
	{
//		RunLog( "tmpNum=[%d],Pos=[0x%04x]", tmpNum, ((tmpNum^cmd[i])&0xff) );
 		tmpNum = (CrcData44[ ((tmpNum^cmd[i])&0xff) ]^(tmpNum/256))&0xffff;
	}

//	RunLog( "tmpNum=[%d]", tmpNum );
	*CrcHi = (tmpNum / 256) & 0x7f;
	*CrcLo = tmpNum & 0x7f;

	return 0;
}

static int MakeDrvTest4(unsigned char  chnPhy, unsigned char gunPhy){
	int i,ret;
	unsigned char CrcHi;
	unsigned char CrcLo;
	char temp[DRV_FIELD_LEN + 1];
	
	if(chnPhy > 16){
		char pbuff[256];
		bzero( pbuff,sizeof(pbuff) );
		sprintf( pbuff, "����ͨ���Ŵ���,����0-16,ֵΪ[%d]", chnPhy );
		cgi_error(pbuff);
		exit(1);
		//return -1;
	}
	//ȷ��3Xֵ,XΪ����ͨ����
	bzero(temp,sizeof(temp));
	sprintf(temp, "%02x",chnPhy |0x30);
	drv_cfg[4].testWrite[2] = temp[0];
	drv_cfg[4].testWrite[3] = temp[1];
	//ȷ��GGֵ,��:����ǹ��.
	bzero(temp,sizeof(temp) );
	sprintf(temp, "%02x", gunPhy);
	drv_cfg[4].testWrite[12] = temp[0];
	drv_cfg[4].testWrite[13] = temp[1];
	//ȷ��RRRRֵ,��:У���.
	bzero(temp,sizeof(temp));
	for(i=0; i<5; i++){
		temp[i] = hexchr2hex(drv_cfg[4].testWrite[6+2*i] ) * 16 +  hexchr2hex(drv_cfg[4].testWrite[7+2*i] );
	}
	ret = GetCrcData44(temp, 5, &CrcLo , &CrcHi);
	sprintf(temp, "%02x%02x", CrcHi, CrcLo);
	memcpy( &(drv_cfg[4].testWrite[16]) , temp, 4);
	//У��������ַ����Ƿ�Ϸ���0-9,a-f.
	for(i=0;i<20, i++){
		if( (drv_cfg[4].testWrite[i] >='0')  && ((drv_cfg[4].testWrite[i] <='9') ){
			continue;
		}
		else if( (drv_cfg[4].testWrite[i] >='A')  && ((drv_cfg[4].testWrite[i] <='Z') ){
			continue;
		}
		else if( (drv_cfg[4].testWrite[i] >='a')  && ((drv_cfg[4].testWrite[i] <='z') ){
			continue;
		}
		else {
			break;
		}
	}
	drv_cfg[4].testWrite[20] = 0;
	if(i < 20) {
		char pbuff[256];
		bzero( pbuff,sizeof(pbuff) );
		sprintf( pbuff, "д�봮�ڵ�ֵ����,ֵΪ[%s],����.", drv_cfg[4].testWrite );
		cgi_error(pbuff);
		exit(1);
		//return -1;
	}
	
	return 0; 
}

static int MakeDrvTest5(unsigned char  chnPhy, unsigned char gunPhy){
	#define WRITE_SIZE 18
	#define DRIVER_NUM 5
	int i,ret;
	char temp[DRV_FIELD_LEN + 1];
	//char mybcdtime[16];
	
	if(chnPhy > 16){
		char pbuff[256];
		bzero( pbuff,sizeof(pbuff) );
		sprintf( pbuff, "����ͨ���Ŵ���,����0-16,ֵΪ[%d]", chnPhy );
		cgi_error(pbuff);
		exit(1);
		//return -1;
	}
	//ȷ��3Xֵ,XΪ����ͨ����
	bzero(temp,sizeof(temp));
	sprintf(temp, "%02x",chnPhy |0x30);
	drv_cfg[DRIVER_NUM].testWrite[2] = temp[0];
	drv_cfg[DRIVER_NUM].testWrite[3] = temp[1];
	//ȷ��GGֵ,��:����ǹ��.
	bzero(temp,sizeof(temp) );
	sprintf(temp, "%02x", gunPhy);
	drv_cfg[DRIVER_NUM].testWrite[14] = temp[0];
	drv_cfg[DRIVER_NUM].testWrite[15] = temp[1];
	
	//ȷ��RRֵ,��:У���.
	bzero(temp,sizeof(temp));	
	sprintf(temp, "%02x", gunPhy^0x34);
	memcpy( &(drv_cfg[DRIVER_NUM].testWrite[16]) , temp, 2);
	//У��������ַ����Ƿ�Ϸ���0-9,a-f.
	for(i=0;i<WRITE_SIZE, i++){
		if( (drv_cfg[DRIVER_NUM].testWrite[i] >='0')  && ((drv_cfg[DRIVER_NUM].testWrite[i] <='9') ){
			continue;
		}
		else if( (drv_cfg[DRIVER_NUM].testWrite[i] >='A')  && ((drv_cfg[DRIVER_NUM].testWrite[i] <='Z') ){
			continue;
		}
		else if( (drv_cfg[DRIVER_NUM].testWrite[i] >='a')  && ((drv_cfg[DRIVER_NUM].testWrite[i] <='z') ){
			continue;
		}
		else {
			break;
		}
	}
	drv_cfg[DRIVER_NUM].testWrite[WRITE_SIZE] = 0;
	if(i < WRITE_SIZE) {
		char pbuff[256];
		bzero( pbuff,sizeof(pbuff) );
		sprintf( pbuff, "д�봮�ڵ�ֵ����,ֵΪ[%s],����.", drv_cfg[DRIVER_NUM].testWrite );
		cgi_error(pbuff);
		exit(1);
		//return -1;
	}
	
	return 0; 
}

static int MakeDrvTest6(unsigned char  chnPhy, unsigned char gunPhy){
	#define DRIVER_NUM 6
	#define WRITE_SIZE 16
	int i,ret;
	char temp[DRV_FIELD_LEN + 1];
	//char mybcdtime[16];
	
	if(chnPhy > 16){
		char pbuff[256];
		bzero( pbuff,sizeof(pbuff) );
		sprintf( pbuff, "����ͨ���Ŵ���,����0-16,ֵΪ[%d]", chnPhy );
		cgi_error(pbuff);
		exit(1);
		//return -1;
	}
	//ȷ��3Xֵ,XΪ����ͨ����
	bzero(temp,sizeof(temp));
	sprintf(temp, "%02x",chnPhy |0x30);
	drv_cfg[DRIVER_NUM].testWrite[2] = temp[0];
	drv_cfg[DRIVER_NUM].testWrite[3] = temp[1];
	
	
	//ȷ��RRֵ,��:У���.
	bzero(temp,sizeof(temp));
	for(i=0; i<3; i++){
		temp[i] = hexchr2hex(drv_cfg[DRIVER_NUM].testWrite[0+2*i] ) * 16 +  hexchr2hex(drv_cfg[DRIVER_NUM].testWrite[1+2*i] );
	}
	ret = Check_xor43(temp, 3);
	sprintf(temp, "%02x", ret );
	memcpy( &(drv_cfg[DRIVER_NUM].testWrite[14]) , temp, 2);
	//У��������ַ����Ƿ�Ϸ���0-9,a-f.
	for(i=0;i<WRITE_SIZE, i++){
		if( (drv_cfg[DRIVER_NUM].testWrite[i] >='0')  && ((drv_cfg[DRIVER_NUM].testWrite[i] <='9') ){
			continue;
		}
		else if( (drv_cfg[DRIVER_NUM].testWrite[i] >='A')  && ((drv_cfg[DRIVER_NUM].testWrite[i] <='Z') ){
			continue;
		}
		else if( (drv_cfg[DRIVER_NUM].testWrite[i] >='a')  && ((drv_cfg[DRIVER_NUM].testWrite[i] <='z') ){
			continue;
		}
		else {
			break;
		}
	}
	drv_cfg[DRIVER_NUM].testWrite[WRITE_SIZE] = 0;
	if(i < WRITE_SIZE) {
		char pbuff[256];
		bzero( pbuff,sizeof(pbuff) );
		sprintf( pbuff, "д�봮�ڵ�ֵ����,ֵΪ[%s],����.", drv_cfg[DRIVER_NUM].testWrite );
		cgi_error(pbuff);
		exit(1);
		//return -1;
	}
	
	return 0; 
}

static int MakeDrvTest7(unsigned char  chnPhy, unsigned char gunPhy){
	#define DRIVER_NUM 7
	int i;
	char temp[DRV_FIELD_LEN + 1];
	
	if(chnPhy >16){
		char pbuff[256];
		bzero( pbuff,sizeof(pbuff) );
		sprintf( pbuff, "����ͨ���Ŵ���,����0-16,ֵΪ[%d]", chnPhy );
		cgi_error(pbuff);
		exit(1);
		//return -1;
	}
	//ȷ��3Xֵ,XΪ����ͨ����
	bzero(temp,sizeof(temp));
	sprintf(temp, "%02x",chnPhy |0x30);
	drv_cfg[DRIVER_NUM].testWrite[2] = temp[0];
	drv_cfg[DRIVER_NUM].testWrite[3] = temp[1];
	return 0;
}

static int MakeDrvTest8(unsigned char  chnPhy, unsigned char gunPhy){
	#define DRIVER_NUM 8
	int i;
	char temp[DRV_FIELD_LEN + 1];
	
	if(chnPhy >16){
		char pbuff[256];
		bzero( pbuff,sizeof(pbuff) );
		sprintf( pbuff, "����ͨ���Ŵ���,����0-16,ֵΪ[%d]", chnPhy );
		cgi_error(pbuff);
		exit(1);
		//return -1;
	}
	//ȷ��3Xֵ,XΪ����ͨ����
	bzero(temp,sizeof(temp));
	sprintf(temp, "%02x",chnPhy |0x30);
	drv_cfg[DRIVER_NUM].testWrite[2] = temp[0];
	drv_cfg[DRIVER_NUM].testWrite[3] = temp[1];
	return 0;
}

//�������ʼ��ͼ�¼У�鷽ʽ//
typedef unsigned short uint;
uint CheckCRC_JYFR( uchar *buffer,uint length)
{
	register uint i,j;
	uchar tem[2];	
	bit lsb0,lsb1;	
	tem[0]=0;
	tem[1]=0;
	for(i=0;i<length;i++)
	{
		tem[1]^=buffer[i];
		for(j=0;j<8;j++)
		{
			lsb0=tem[0]&1;
			tem[0]=tem[0]>>1;			
			lsb1=tem[1]&1;			
			tem[1]=tem[1]>>1;			
			if(lsb0)				
				tem[1]|=0x80;			
			if(lsb1)			
			{				
				tem[0]^=0xa0;				
				tem[1]^=0x01;			
			}		
		}	
	}	
	return(*(uint *)tem);
}
static int MakeDrvTest9(unsigned char  chnPhy, unsigned char gunPhy){
	#define DRIVER_NUM 9
	#define WRITE_SIZE 36
	int i,ret;
	unsigned short crc;
	char temp[DRV_FIELD_LEN + 1];
	char mybcdtime[16];
	
	if(chnPhy > 16){
		char pbuff[256];
		bzero( pbuff,sizeof(pbuff) );
		sprintf( pbuff, "����ͨ���Ŵ���,����0-16,ֵΪ[%d]", chnPhy );
		cgi_error(pbuff);
		exit(1);
		//return -1;
	}
	//ȷ��3Xֵ,XΪ����ͨ����
	bzero(temp,sizeof(temp));
	sprintf(temp, "%02x",chnPhy |0x30);
	drv_cfg[DRIVER_NUM].testWrite[2] = temp[0];
	drv_cfg[DRIVER_NUM].testWrite[3] = temp[1];
	//ȷ��TTTTTTTTTTTTTTֵ,��:��ǰʱ��.
	bzero(mybcdtime,sizeof(mybcdtime));
	ret = getbcdtime(mybcdtime);
	if(ret < 0){
		char pbuff[256];
		bzero( pbuff,sizeof(pbuff) );
		sprintf( pbuff, "��ȡ��ǰʱ��ֵ����,����ʱ��ֵΪ[%s],����.", temp );
		cgi_error(pbuff);
		exit(1);
		//return -1;
	}
	bzero(temp,sizeof(temp));
	sprintf(temp, "%02x%02x%02x%02x%02x%02x%02x",  mybcdtime[0],  mybcdtime[1],  mybcdtime[2],  mybcdtime[3],  mybcdtime[4],  mybcdtime[5],  mybcdtime[6]);
	memcpy( &(drv_cfg[DRIVER_NUM].testWrite[19]), temp, 14 );
	//ȷ��RRRRֵ,��:У���.
	bzero(temp,sizeof(temp));
	for(i=0; i<13; i++){
		temp[i] = hexchr2hex(drv_cfg[DRIVER_NUM].testWrite[6+2*i] ) * 16 +  hexchr2hex(drv_cfg[DRIVER_NUM].testWrite[7+2*i] );
	}
	crc = CheckCRC_JYFR(temp, 13);
	sprintf(temp, "%04x", crc);
	memcpy( &(drv_cfg[DRIVER_NUM].testWrite[32]) , &temp[2], 2);//��λ��ǰ
	memcpy( &(drv_cfg[DRIVER_NUM].testWrite[34]) , temp, 2);
	//У��������ַ����Ƿ�Ϸ���0-9,a-f.
	for(i=0;i<WRITE_SIZE, i++){
		if( (drv_cfg[DRIVER_NUM].testWrite[i] >='0')  && ((drv_cfg[DRIVER_NUM].testWrite[i] <='9') ){
			continue;
		}
		else if( (drv_cfg[DRIVER_NUM].testWrite[i] >='A')  && ((drv_cfg[DRIVER_NUM].testWrite[i] <='Z') ){
			continue;
		}
		else if( (drv_cfg[DRIVER_NUM].testWrite[i] >='a')  && ((drv_cfg[DRIVER_NUM].testWrite[i] <='z') ){
			continue;
		}
		else {
			break;
		}
	}
	drv_cfg[DRIVER_NUM].testWrite[WRITE_SIZE] = 0;
	if(i < 30) {
		char pbuff[256];
		bzero( pbuff,sizeof(pbuff) );
		sprintf( pbuff, "д�봮�ڵ�ֵ����,ֵΪ[%s],����.", drv_cfg[DRIVER_NUM].testWrite );
		cgi_error(pbuff);
		exit(1);
		//return -1;
	}
	
	return 0; 
}

static int MakeDrvTest10(unsigned char  chnPhy, unsigned char gunPhy){
	#define DRIVER_NUM 10
	int i;
	char temp[DRV_FIELD_LEN + 1];
	
	if(chnPhy >16){
		char pbuff[256];
		bzero( pbuff,sizeof(pbuff) );
		sprintf( pbuff, "����ͨ���Ŵ���,����0-16,ֵΪ[%d]", chnPhy );
		cgi_error(pbuff);
		exit(1);
		//return -1;
	}
	//ȷ��3Xֵ,XΪ����ͨ����
	bzero(temp,sizeof(temp));
	sprintf(temp, "%02x",chnPhy |0x30);
	drv_cfg[DRIVER_NUM].testWrite[2] = temp[0];
	drv_cfg[DRIVER_NUM].testWrite[3] = temp[1];
	return 0;
}


static int MakeDrvTest11(unsigned char  chnPhy, unsigned char gunPhy){
	#define DRIVER_NUM 11 
	#define P 1  //1-15,0����16,ͨѶ���ϼ��ͻ���ͨѶ��ַ
	#define S 1 //
	int i;
	char temp[DRV_FIELD_LEN + 1];
	
	if(chnPhy >16){
		char pbuff[256];
		bzero( pbuff,sizeof(pbuff) );
		sprintf( pbuff, "����ͨ���Ŵ���,����0-16,ֵΪ[%d]", chnPhy );
		cgi_error(pbuff);
		exit(1);
		//return -1;
	}
	//ȷ��3Xֵ,XΪ����ͨ����
	bzero(temp,sizeof(temp));
	sprintf(temp, "%02x",chnPhy |0x30);
	drv_cfg[DRIVER_NUM].testWrite[2] = temp[0];
	drv_cfg[DRIVER_NUM].testWrite[3] = temp[1];
	//ȷ��APֵ
	bzero(temp,sizeof(temp));
	sprintf(temp, "%02x",P |0xA0);
	drv_cfg[DRIVER_NUM].testWrite[2] = temp[0];
	drv_cfg[DRIVER_NUM].testWrite[3] = temp[1];
	//ȷ��SPֵ
	bzero(temp,sizeof(temp));
	sprintf(temp, "%02x",S<<8 |P);
	drv_cfg[DRIVER_NUM].testReturn[1] = temp[0];
	drv_cfg[DRIVER_NUM].testReturn[2] = temp[1];
	return 0;
}

*/
static int ReadLine( FILE *f, char *s, int MaxLen )
{
	int i, c;

	for( i = 0; i < MaxLen; i++ )
	{
		c = fgetc( f);
		if( c == '\t' )
			break;//tab is also the end.
		if( c == EOF || c == '\n' )
			break;
		s[i] = c;
	}

	s[i] = 0;

	if( i == MaxLen )
	{
		for( ; ; )
		{
			c = fgetc(f);
			if( c == EOF || c == '\n' )
				break;
		}
	}
	if( c == EOF )
		return 1;
	else
		return 0;
}
static int SendDataToChannel( char *str, size_t len, char chn, char chnLog, long indtyp )
{
	//RunLog( "SendDataToChannel	[%s]", Asc2Hex(str,len) );
	//RunLog( "senddatatochannel.ͨ����[%d]�߼�ͨ����[%d],����[%s]", chn, chnLog, Asc2Hex(str,len) );
	return SendMsg( GUN_MSG_TRAN, str, len,indtyp, 3 );
}


static int OpenSerial( char *ttynam ){
	int fd;
	struct termios termios_new;

	fd = open (ttynam, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if( fd < 0 )
	{
		//UserLog( "���豸[%s]ʧ��", ttynam );
		return -1;
	}
	tcgetattr(fd, &termios_new);
	cfsetispeed(&termios_new,B115200);
	cfsetospeed(&termios_new,B115200);
	/*set 8 bit */
	termios_new.c_cflag &= ~CSIZE;
	termios_new.c_cflag |= CS8;
	/*parity checking */
	termios_new.c_cflag &= ~PARENB;   /* Clear parity enable */
	/*1 stop bit */
	termios_new.c_cflag &= ~CSTOPB;
	/*no start bit */
	termios_new.c_cflag |= CLOCAL;
	termios_new.c_cflag |= CREAD;           //make port can read
	termios_new.c_cflag &= ~CRTSCTS;  //NO flow control
	termios_new.c_oflag = 0;
	termios_new.c_lflag  = 0;
	termios_new.c_iflag &= ~(IXON |IXOFF |IXANY);
	termios_new.c_iflag &= ~(INPCK | ISTRIP | ICRNL);
	termios_new.c_cc[VTIME] = 1;       /* unit: 1/10 second. */
	termios_new.c_cc[VMIN] = 1; /* minimal characters for reading */

	tcflush (fd, TCIOFLUSH);
	/* 0 on success, -1 on failure */
	if( tcsetattr(fd, TCSANOW, &termios_new) == 0 )	/*success*/
		return fd;
	return -1;

}

static int ClearSerial( int fd )
{
	int n;
	char	str[3];

	str[2] = 0;

	while(1)
	{
		n = read( fd, str, 2 );
		if( n == 0 )
			return 0;
		else if( n < 0 && errno == EAGAIN ) /*������.����*/
			return 0;
		else if( n < 0 && n != EINTR ) /*�����Ҳ��Ǳ��źŴ��*/
			return -1;
		/*�������������*/
	}
}

static ssize_t Read2( int fd, char *vptr ){
	size_t	nleft;
	ssize_t	nread;
	char	*ptr;

	ssize_t n;
	n = 0;

	/*����һλ*/
	ptr = vptr;
	while(1)
	{
		if ( (nread = read(fd, ptr, 1)) < 0 )
		{
			if( errno == EAGAIN )	/*������*/
				continue;
			else if (errno == EINTR)	/*���źŴ��*/
				nread = 0;
			else
				return(-1);
		}
		else if (nread == 0)
			return n;				/* EOF */

		if( ptr[0] != 0x00 )	/*0x00������������Ϊ����������*/
		{
			n = 1;
			break;
		}
		//UserLog( "throw 0x00 away" );
	}

	/*���ڶ�λ*/
	ptr = vptr+1;
	while(1)
	{
		if ( (nread = read(fd, ptr, 1)) < 0 )
		{
			if( errno == EAGAIN )	/*������*/
				continue;
			if (errno == EINTR)	/*���źŴ��*/
				nread = 0;
			else
				return(-1);
		}
		else if (nread == 0)
			return n;				/* EOF */

		n = 2;
		break;

	}
	return n;
}
/*
static int WriteToPort( int fd, int chnLog, int chnPhy, char *buff, size_t len, int flag, char wantLen, int timeout )
{
	char		cmd[MAX_ORDER_LEN+1];
	int			cnt;
	int			ret;

	if( flag == 1 )	//flag=1��ʾ��Ҫ����ʱ���
	{
		cmd[0] = 0x1B;
		cmd[1] = 0x30+chnPhy-1;
		if( len > MAX_PORT_CMDLEN )
		{
			//RunLog( "��˿�д�����ݳ�����󳤶�[%d]", MAX_PORT_CMDLEN );
			return -1;
		}
		cmd[2] = 0x00+len;
		memcpy( cmd+3, buff, len );
		cnt = len+3;
	}
	else
	{
		cnt = len;
		memcpy( cmd, buff, len );
	}

	//RunLog( "WriteToPort [%s]", Asc2Hex(cmd, cnt) );
	
	//RunLog("In tty.c***chnLog is: %d , gpgunshm wantLen is: %d\n",chnLog,gpGunShm->wantLen[chnLog-1]);
	Act_P( PORT_SEM_INDEX );	//�Զ˿ڵ�д��֪���ǲ���ԭ�Ӳ�������PVԭ�ﱣ�յ�

	ret = WriteByLengthInTime(fd, cmd, cnt, timeout );

	Act_V( PORT_SEM_INDEX );

	return ret;
}
*/
//��ȡ�������ӵļ��ͻ��ڵ����
static int ReadPortNum(int portnum) 
{	
	unsigned char IO_cmd[2] = {0x1c,0x50};
	int fd;
	char ttynam[30];
	int	ret;
	char msg[16]; //,tmp[2],msg2[512];
	int timeout = 1;
	//int len;
	
	strcpy( ttynam, alltty[portnum-1] );


	fd = OpenSerial( ttynam);
	if( fd < 0 )
	{
		//RunLog( "�򿪴���ʧ��" );
		return -1;
	}
	ret = WriteByLengthInTime( fd, IO_cmd, 2,timeout );
	if( ret < 0 ) {
		//RunLog( "Write Failed" );
		return -1;
	}

	memset( msg, 0, sizeof(msg) );
	ret = ReadByLengthInTime( fd, msg, sizeof(msg)-1, timeout );
	if( ret < 0 ) {
		//RunLog( "Read Failed" );
		return -1;
	}
	if( ret == 0 )	{
		//RunLog( "Read data error" );
		return -1;
	}
	//RunLog( "Read data [%s]" ,Asc2Hex((unsigned char *)msg, ret));
	//msg[ret] = 0;	
	return msg[0];	
}
/*��ʱע��,�п��ټ�����
static int MakeDrvTestCase(const int driverno,const unsigned char  chnPhy, const unsigned char gunPhy){
	int ret = -1;
	switch(driverno){
		case 0: 
			break;
		case 1: 
			ret = MakeDrvTest1(chnPhy, gunPhy);
			if( ret < 0 ){
				cgi_error( "MakeDrvTest1����ִ�д���" );
				exit(1);
			}
			break;
		case 2: 
			ret = MakeDrvTest2(chnPhy, gunPhy);
			if( ret < 0 ){
				cgi_error( "MakeDrvTest2����ִ�д���" );
				exit(1);
			}
			break;
		case 3: 
			ret = MakeDrvTest3(chnPhy, gunPhy);
			if( ret < 0 ){
				cgi_error( "MakeDrvTest3����ִ�д���" );
				exit(1);
			}
			break;
		case 4: 
			ret = MakeDrvTest4(chnPhy, gunPhy);
			if( ret < 0 ){
				cgi_error( "MakeDrvTest4����ִ�д���" );
				exit(1);
			}
			break;
		case 5: 
			ret = MakeDrvTest5(chnPhy, gunPhy);
			if( ret < 0 ){
				cgi_error( "MakeDrvTest5����ִ�д���" );
				exit(1);
			}
			break;
		case 6: 
			ret = MakeDrvTest6(chnPhy, gunPhy);
			if( ret < 0 ){
				cgi_error( "MakeDrvTest6����ִ�д���" );
				exit(1);
			}
			break;
		case 7: 
			ret = MakeDrvTest7(chnPhy, gunPhy);
			if( ret < 0 ){
				cgi_error( "MakeDrvTest7����ִ�д���" );
				exit(1);
			}
			break;
		case 8: 
			//ret = MakeDrvTest8(chnPhy, gunPhy);
			if( ret < 0 ){
				cgi_error( "MakeDrvTest8����ִ�д���" );
				exit(1);
			}
			break;
		case 9: 
			//ret = MakeDrvTest9(chnPhy, gunPhy);
			if( ret < 0 ){
				cgi_error( "MakeDrvTest9����ִ�д���" );
				exit(1);
			}
			break;
		case 10: 
			//ret = MakeDrvTest10(chnPhy, gunPhy);
			if( ret < 0 ){
				cgi_error( "MakeDrvTest10����ִ�д���" );
				exit(1);
			}
			break;
		case 11: 
			//ret = MakeDrvTest11(chnPhy, gunPhy);
			if( ret < 0 ){
				cgi_error( "MakeDrvTest11����ִ�д���" );
				exit(1);
			}
			break;
		case 12: 
			//ret = MakeDrvTest12(chnPhy, gunPhy);
			if( ret < 0 ){
				cgi_error( "MakeDrvTest12����ִ�д���" );
				exit(1);
			}
			break;
		case 13: 
			//ret = MakeDrvTest13(chnPhy, gunPhy);
			if( ret < 0 ){
				cgi_error( "MakeDrvTest13����ִ�д���" );
				exit(1);
			}
			break;
		case 14: 
			//ret = MakeDrvTest1(chnPhy, gunPhy);
			if( ret < 0 ){
				cgi_error( "MakeDrvTest14����ִ�д���" );
				exit(1);
			}
			break;
		case 15: 
			//ret = MakeDrvTest1(chnPhy, gunPhy);
			if( ret < 0 ){
				cgi_error( "MakeDrvTest15����ִ�д���" );
				exit(1);
			}
			break;
		case 16: 
			//ret = MakeDrvTest1(chnPhy, gunPhy);
			if( ret < 0 ){
				cgi_error( "MakeDrvTest16����ִ�д���" );
				exit(1);
			}
			break;
		case 17: 
			//ret = MakeDrvTest17(chnPhy, gunPhy);
			if( ret < 0 ){
				cgi_error( "MakeDrvTest17����ִ�д���" );
				exit(1);
			}
			break;
		default :
			cgi_error( "����,û���������Ĳ��Ժ���" );
			exit(1);
			break;
		}
}
*/
//---------------------------------
//���ݼ��ͻ����ͺͱ������Ӻ�,���Ӳ��Լ��ͻ���CGI����
//�ٶ����ڴ�COM3 ��ʼ,�������ʹ��4������,�����ͬ,���޸Ĵ˳���!
//--------------------------------------
int main( int argc, char* argv[] )
{
	int fd;	
	char ttynam[30];
	char FileName[128];
	char	*pFilePath, FilePath[] = DEFAULT_WORK_DIR;
	char temp[128];
	int	chnlog, driverno, portchn[4],  portnum;
	int	chnPhy;
	int	ret;
	char msg[512],tmp[2],msg2[512];
	unsigned char *result;
	int i, k, cnt,disp_all=0;// len,
	char bodybuff[1024];
	char *listhtml ="<html><head><title>���ͻ�������Ϣ��ʾ</title></head><body > \n\
%s</body></html>";
	char *tableinit = "<table align=center bordercolor=#cc99ff><tr><td> \n\
	�������԰�ť�����ݱ䱳�����Ӻźͼ��ͻ��ͺŲ����Ƿ���ã���\n\
	</td></tr></table>";
	char *tableok ="<table align=center border=0 ><tr><td style='color:#ff0000'> \n\
	�������Ӻ�:%s, ���ͻ���������:%s, ����������ȷ����</td>";
	char *tablebad ="<table align=center border=0 ><tr><td style='color:#00ff00'> \n\
	�������Ӻ�:%s, ���ͻ���������:%s, �������ô��󣡣�</td>";
	extern NAME_VALUE nv[];
	//extern char **alltty;
	
	ret = getCgiInput();
	if( ret < 0 ){
		cgi_error( "��ȡ���������" );
		exit(1);
	}
	if(0 == ret ){
		//moved("move to ", "./test.htm");
		sprintf(bodybuff,listhtml,tableinit);
		printf(bodybuff);
		exit(0);
	}
		 
	chnlog = atoi( TrimAll(nv[0].value) );	
	if(chnlog <= 0){
		cgi_error( "������ı������ӺŴ���" );
		exit(1);
	}
	driverno =atoi( TrimAll(nv[1].value)  );	
	if(driverno <= 0){
		cgi_error( "������ļ��ͻ������Ŵ���" );
		exit(1);
	}
	
	//���㴮�ں�
	//�ٶ����ڴ�COM3 ��ʼ,�������ʹ��4������,���㴮�ں�,�����ͬ,���޸��������!
	cnt = 0;
	portnum =0 ;
	for(i=0;i<4;i++){
	  	portchn[i] = ReadPortNum( i+3);//��COM3 ��ʼ
	  	cnt += portchn[i] ;
	  	if(chnlog - cnt <= 0){
			portnum = i + 3 ;
			break;
		}
	  }
	if(portnum == 0){
		cgi_error( "���ںż������!!" );
		exit(1);
	}
	
	
	/*
	//ȡ�ļ�·��//	
	pFilePath = getenv( "WORK_DIR" );	//WORK_DIR ����Ŀ¼�Ļ�������
	if( NULL ==  pFilePath){
		bzero(temp, sizeof(temp));
		ret = GetWorkDir(temp);
		if(ret >= 0 ){
			pFilePath = temp;
		}
		else{
			pFilePath = FilePath;
		}
	}
	if((NULL ==  pFilePath) ||(pFilePath[0] !=  '/') ){
		pFilePath = FilePath;
		}
	sprintf( FileName, "%s%s", pFilePath, DRV_CFG__FILE);

	if( IsTheFileExist( FileName ) != 1 ){
		cgi_error( FileName );
		exit(1);
	}
	
	//ȡ���������ļ���drv_cfg[].
	ret = readDrvCfg(FileName);
	if(ret<0){
		cgi_error( "��ȡ���������ļ�dspdrv.cfg����" );
		exit(1);
	}
	*/
	//�򿪴���
	strcpy( ttynam, alltty[portnum-1] );		
	fd = OpenSerial( ttynam);
	if( fd < 0 ) {
		cgi_error( "�򿪴���ʧ��" );
		exit(1);
	}
	/*
	//PMsg( "WriteToPort[%s]", Asc2Hex(Hex2Asc(argv[2],strlen(argv[2])),strlen(argv[2])/2) );
	//���Ҽ��ͻ���������
	k==0;
	for(i=0;i<DRV_CFG_MAX_CNT;i++){
		if(  memcmp(TrimAll(drv_cfg[i].driverNum),  nv[1].value, strlen(nv[1].value)  )== 0  ) {
			k=i;
			break;
		}
	}
	if( (i == DRV_CFG_MAX_CNT) &&(k == 0) ){
		cgi_error( "��������ʧ��" );
		exit(1);
	}
	*/
	ret = WriteByLengthInTime( fd, Hex2Asc(drv_cfg[driverno].testWrite, strlen(drv_cfg[driverno].testWrite)),strlen(drv_cfg[driverno].testWrite)/2, 3 );
	if( ret < 0 ){
		cgi_error( "Write Failed" );
		exit(1);
	}

	
	memset( msg, 0, sizeof(msg) );
	ret = ReadByLengthInTime( fd, msg, sizeof(msg)-1, 2 );
	if( ret < 0 )
	{
		cgi_error( "Read Failed" );
		exit(1);
	}
	if( memcmp( TrimAll(msg), TrimAll(drv_cfg[k].testReturn),  strlen( TrimAll(drv_cfg[k].testReturn) )  ) ==0) {
		disp_all = 1;//����ͨ��
	}
	//result=Asc2Hex(msg,ret);
	//len = strlen(result);
	bzero(bodybuff,sizeof(bodybuff));
	if (disp_all == 0){
		
		sprintf(msg,  tablebad,  nv[0].value,  nv[1].value );
		sprintf(bodybuff, listhtml, msg);
	}
	else{
		sprintf(msg,  tablebad,  nv[0].value,  nv[1].value );
		sprintf(bodybuff, listhtml, msg);
	}
	printf(bodybuff);
	exit(0);
}

/*
//first read driver config  file by readDrvCfg fuction , then change or add driver config  data, last write ini data to ini file by writeDrvCfgfuction.
int readDrvCfg(char *fileName){

	//char *method;
	char  my_data[sizeof(DRV_CFG)*DRV_CFG_MAX_CNT+1];
	char *my_data_tmp, *tmp, *tmpvar;
	int data_len;
	int i, ret;
	int c;
	FILE *f;
	
	if(  NULL== fileName ){
		return -1;
	}
	ret = IsTheFileExist(fileName);
	if(ret != 1){
		char pbuff[256];
		bzero( pbuff,sizeof(pbuff) );
		sprintf( pbuff, "file[%s] not exist in readDrvCfg  error!! ", fileName );
		cgi_error(pbuff);
		exit(1);
		return -2;
	}
	f = fopen( fileName, "rt" );
	if( !f  ){	
		char pbuff[256];
		bzero(pbuff,sizeof(pbuff));
		sprintf( pbuff, "file[%s] open error in readDrvCfg!! ", fileName );
		cgi_error(pbuff);
		exit(1);
		return -3;
	}	
	bzero(my_data, sizeof(my_data));
	for(i=0;i<sizeof(my_data)-1;i++){
		c = fgetc( f);
		if( c == EOF)
			break;
		my_data[i] = c;
	}
	my_data[i] = 0;	
	fclose( f );
	
	my_data_tmp = my_data;
	
	bzero(&(drv_cfg[0].driverName[0]), sizeof(DRV_CFG)*DRV_CFG_MAX_CNT);	
	i = 0;
	while (my_data_tmp[0] !='\0'){
		tmp = split(my_data_tmp, '\n');// split data to get field name.		
		tmpvar = split(tmp, ',');
		bzero(&(drv_cfg[i].driverNum), sizeof(drv_cfg[i].driverNum));
		strcpy(drv_cfg[i].driverNum, TrimAll(tmpvar) );
		tmpvar = split(tmp, ',');// split data to get value.
		bzero(&(drv_cfg[i].driverName), sizeof(drv_cfg[i].driverName));
		strcpy(drv_cfg[i].driverName, TrimAll(tmpvar));
		tmpvar = split(tmp, ',');// split data to get value.
		bzero(&(drv_cfg[i].testWrite), sizeof(drv_cfg[i].testWrite));
		strcpy(drv_cfg[i].testWrite, TrimAll(tmpvar));
		bzero(&(drv_cfg[i].testReturn), sizeof(drv_cfg[i].testReturn));
		strcpy(drv_cfg[i].testReturn, TrimAll(tmp));
		i ++;
	}
	
	//free(my_data);
	return i--;	
}

static int writeDrvCfg(const char *fileName){
	int fd;
	int i, ret;
	char temp[128];
	
	if(  NULL== fileName ){
		return -1;
	}	
	//���ļ�
	fd = open( fileName, O_WRONLY|O_CREAT|O_TRUNC, 00644 );
	if( fd < 0 )
	{
		//UserLog( "�򿪵�ǰ���й���·����Ϣ�ļ�[%s]ʧ��.",fileName );
		cgi_error("file open error in writeDrvCfg!!");
		exit(1);
		return -1;
	}
	for(i=0;i<DRV_CFG_MAX_CNT;i++){
		if( (drv_cfg[i].driverNum !=0) && (drv_cfg[i].driverName[0] !=0)  ){
			bzero(temp,sizeof(temp));
			sprintf(temp,"%s\t , %s\t , %s\t , %s\t   \n", drv_cfg[i].driverNum, drv_cfg[i].driverNum,drv_cfg[i].testWrite,drv_cfg[i].testReturn);
			write( fd, temp, strlen(temp) );
		}
	}	
	close(fd);
	return 0;
}
static int addDrvCfg(const DRV_CFG *pDrvCfg){
	int i,n;
	
	if(   NULL== pDrvCfg   ){
		return -1;
	}	
	n = strlen(pDrvCfg->driverNum);
	for(i=0;i<DRV_CFG_MAX_CNT;i++){
		if( memcmp(drv_cfg[i].driverNum,pDrvCfg->driverNum,n)  == 0 ){
			return -1;
			//break;
		}
		else if(drv_cfg[i].driverNum[0] == 0){
			memcpy(drv_cfg[i].driverNum, pDrvCfg->driverNum, strlen(pDrvCfg->driverNum) );
			memcpy(drv_cfg[i].driverNum, pDrvCfg->driverNum, strlen(pDrvCfg->driverNum) );
			memcpy(drv_cfg[i].driverNum, pDrvCfg->driverNum, strlen(pDrvCfg->driverNum) );
			memcpy(drv_cfg[i].driverNum, pDrvCfg->driverNum, strlen(pDrvCfg->driverNum) );
			return 0;
		}
	}
		
		return -2;
}
static int changeDrvCfg(const DRV_CFG *pDrvCfg){
	int i,n;
	int ret;
	
	if(    NULL== pDrvCfg    ){
		return -1;
	}	
	n = strlen(pDrvCfg->driverNum);
	for(i=0; i<DRV_CFG_MAX_CNT; i++){
		if(  memcmp(drv_cfg[i].driverNum,pDrvCfg->driverNum,n)  == 0 ){
			memcpy(drv_cfg[i].driverNum, pDrvCfg->driverNum, strlen(pDrvCfg->driverNum) );
			memcpy(drv_cfg[i].driverNum, pDrvCfg->driverNum, strlen(pDrvCfg->driverNum) );
			memcpy(drv_cfg[i].driverNum, pDrvCfg->driverNum, strlen(pDrvCfg->driverNum) );
			memcpy(drv_cfg[i].driverNum, pDrvCfg->driverNum, strlen(pDrvCfg->driverNum) );
			break;
		}
	}
	if(i == DRV_CFG_MAX_CNT){
		ret = addDrvCfg(pDrvCfg);
		if(ret < 0){
			return -2;
		}
	}	
	else
		return 0;
}


char *upper(char *str){
	int i, n;
	if(NULL == str){
		return NULL;
	}
	n = strlen(str);
	for(i=0;i<n;i++){
		if( (str[i]>='a') && (str[i]<='z') ){
			str[i] +='a' -'A';
		}
	}
	return str;
}

*/





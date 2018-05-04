#include "lischn.h"
#include "ifsf_pub.h"

/*
 * func: 选择启动通道监听处理程序
 */
int ListenChannel( int chnid )
{
	int i;
	unsigned char mtype;	/*该通道连接的机器类型*/

	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,Listen Channel失败");
		return -1;
	}	
	
	
	/*需要查看该通道连接的机器类型*/
	for(i = 0; i < MAX_CHANNEL_CNT; i++) {
		// 根据逻辑通道号查找机器类型
		if(gpIfsf_shm->node_channel[i].chnLog== chnid) {
			mtype = gpIfsf_shm->node_channel[i].nodeDriver;
			break;
		}
	}

	if(i == MAX_CHANNEL_CNT) {
		/*ERROR*/
		UserLog( "未找到逻辑通道号[%d]对应的枪信息", chnid );
		return -1;
	}

	switch(mtype) {
		case 0x01:	/*美国长吉*/
			ListenChn001(chnid);
			break;
		case 0x02:	/*恒山普通税控(单枪)*/
			ListenChn002(chnid);
			break;
		case 0x03:	/*太空*/
			ListenChn003(chnid);
			break;
		case 0x04:	/*榕兴 查询返回35字节*/
			ListenChn004(chnid);
			break;
		case 0x05:	/*正星5.0*/
			ListenChn005(chnid);
			break;
		case 0x06:	/*宝德尔*/
			ListenChn006(chnid);
			break;
		case 0x07:	/*稳牌*/
			ListenChn007(chnid);
			break;
		case 0x08:	/*恒山自助*/
			ListenChn008(chnid);
			break;
		case 0x09:	/*富仁豪升*/
			ListenChn009(chnid);
			break;
		case 0x0A:	/*老长空*/
			ListenChn010(chnid);
			break;
		case 0x0B:	/*新长空*/
			ListenChn011(chnid);
			break;
		case 0x0C:	/*三盈*/
			ListenChn012(chnid);
			break;
		case 0x0D:	/*鸿洋*/
			ListenChn013(chnid);
			break;
		case 0x0E:	/*中意*/
			ListenChn014(chnid);
			break;
		case 0x0F:	/*佳力佳*/
			ListenChn015(chnid);
			break;
		case 0x10:	/*模拟加油机*/
			ListenChn016(chnid);
			break;
		case 0x11:	/*蓝峰*/
			ListenChn017(chnid);
			break;
		case 0x12:	/*长空卡机联动*/
			ListenChn018(chnid);
			break;
		case 0x13:	/*恒山TMC(UDC协议)*/
			ListenChn019(chnid);
			break;
		case 0x14:	/*恒山TQC(UDC协议)*/
			ListenChn020(chnid);
			break;
		case 0x16:	/*北京长吉*/ /* 跳过lischn021因为罐车OPT使用了该配置 */
			ListenChn022(chnid);
			break;
		default:
			/*error*/
			exit(-1);
	}
	return 0;
}


#include "pub.h"

/*附加长度判断.判断已取得的长度是否足够返回给前端*/
/*在所有返回不固定长度数据的交易中,需要在此处增加处理*/
/*系统设计时认为所有交易均返回固定长度的数据.后来发现未必如此,不得不加此补丁*/
/*造化如斯,如之奈何............*/

/*
 * Guojie zhu @ 8.20
 * 将AddJustLen的参数cmdlen由int类型改为int *; cmd 改为非const类型.
 */

/*
 * 返回码:
 *      0   取到了所有数据.可以返回.
 *      1   未取到数据.此时会更改需要的长度字段
 */

//int AddJustLen(int chnLog, const unsigned char *cmd, int cmdlen, int wantInd, int mark  )
int AddJustLen(int chnLog, unsigned char *cmd, int *cmdlen, int wantInd, int mark)
{
	int ret;
	int wantlen = 0;   // for case 0x07
	static int idx = 0;   // for case 0x08
	static int left_fa = 0;  // for case 0x08, 如果 left_fa == 1 表示数据中出现了一个FA(非转义FA)
	static int real_wantlen = 0;  // 真正应该返回的数据长度, 不包括转义FA

	switch( mark ) {
		case 0x01:	/*长吉加油机取泵码的交易.返回的长度是油机(或面板)总枪数*30+4.*/
			if (cmd[(*cmdlen) - 1] != 0xf0 ) {
				if (wantInd == gpGunShm->wantInd[chnLog-1] ) {
					/*如果此时前端已经超时并且已经更改成了下一个交易的返回值......*/
					gpGunShm->wantLen[chnLog-1] += 30;
					return 1;
				}
			}
			break;
		case 0x03:	/*太空协议查询油机状态用*//*第二位06表示非加油状态,返回8位.第二位0C为加油状态,返回14位*/
			if (wantInd == gpGunShm->wantInd[chnLog-1]) {	/*消息没超时*/
				if (gpGunShm->wantLen[chnLog - 1] < cmd[1] + 2) {
					gpGunShm->wantLen[chnLog - 1] = cmd[1] + 2;
					gpGunShm->addInf[chnLog - 1][0] = 0x00;
					return 1;
				} 
			}
			break;
		case 0x04: /*榕兴协议。取油品累计时用。第三位是数据长度.后面＋2是crc*/
			if (wantInd == gpGunShm->wantInd[chnLog-1]) {	/*消息没超时*/
				gpGunShm->wantLen[chnLog-1] = 3 + cmd[2] + 2;
				gpGunShm->addInf[chnLog - 1][0] = 0x00;
				return 1;
			}
			break;
		case 0x06:		//2006.02.17 for 35/39 return characters
			if( cmd[2] == 0x1e ) {
				if(wantInd == gpGunShm->wantInd[chnLog-1] ) {	/*消息没超时*/
					if( gpGunShm->wantLen[chnLog-1] == 35 ) {
						return 0;
					} else {
						gpGunShm->wantLen[chnLog-1] = 35;
						return 1;
					}
				}
			} else {
		      // 	if( cmd[2] == 0x22 ) {  // 移除这个判断,避免错误数据影响接收,GuojieZhu@2009.12.14
				if(wantInd == gpGunShm->wantInd[chnLog-1] )	/*消息没超时*/
				{
					if( gpGunShm->wantLen[chnLog-1] == 39 ) {
						return 0;
					} else {
						gpGunShm->wantLen[chnLog-1] = 39;
						return 1;
					}
				}
			}
			break;
		case 0x07:             // 稳牌
			switch (cmd[1] & 0xF0) {  // 判断 CTRL
			case 0x50:     // NAK
			case 0x70:     // EOT
			case 0xc0:     // ACK
//				if (cmd[2] == 0xFA) {        // SF
					gpGunShm->addInf[chnLog - 1][0] = 0x00;
					return 0;
//				}
				break;
			case 0x30:
				if (gpGunShm->addInf[chnLog - 1][1] == 0x00) {
//					RunLog("-1---------- %s ", Asc2Hex(cmd, gpGunShm->wantLen[chnLog - 1]));
					(gpGunShm->wantLen[chnLog - 1])++;      // 取第一块DATA的长度 
					gpGunShm->addInf[chnLog - 1][1] = 0x0F;
					return 1;
				} else if (gpGunShm->addInf[chnLog - 1][1] == 0x0F) {
//					RunLog("-2---------- %s ", Asc2Hex(cmd, gpGunShm->wantLen[chnLog - 1]));
					gpGunShm->wantLen[chnLog - 1] += cmd[3] + 4;
					gpGunShm->addInf[chnLog - 1][1] = 0xF0; // 取第一块DATA
					return 1;    // 继续接收
				} else {
//					RunLog("-3---------- %s, ", Asc2Hex(cmd, gpGunShm->wantLen[chnLog - 1]));
					wantlen = gpGunShm->wantLen[chnLog - 1];
					// 这里要判断数据中是否有0xFA, 如果有要把前面的数据连接符去掉
					// 因稳牌上传数据都是BCD格式, 所以0xFA只可能出现在CRC校验中
					if (cmd[wantlen - 3] == 0xFA &&  cmd[wantlen - 4] == 0x10) {
						// 跳过数据连接码0x10(SF 覆盖 DLE 储存缓冲区)
						cmd[wantlen - 4] = cmd[wantlen - 3];
						cmd[wantlen - 3] = cmd[wantlen - 2];
						cmd[wantlen - 2] = cmd[wantlen - 1];
						(*cmdlen)--;   // 加收一个字节
//					RunLog("-------------cmdlen: %d -------", *cmdlen);
//					RunLog("-4---------- %s, ", Asc2Hex(cmd, gpGunShm->wantLen[chnLog - 1]));
						return 1;
					} else if (cmd[wantlen - 2] == 0xFA && cmd[wantlen - 3] == 0x10) {
						// 跳过数据连接码0x10(SF 覆盖 DLE 储存缓冲区)
						cmd[wantlen - 3] = cmd[wantlen - 2];
						cmd[wantlen - 2] = cmd[wantlen - 1];
						(*cmdlen)--;   // 加收一个字节
//					RunLog("-------------cmdlen: %d -------", *cmdlen);
//					RunLog("-4---------- %s, ", Asc2Hex(cmd, gpGunShm->wantLen[chnLog - 1]));
						return 1;
					} else if (cmd[wantlen - 1] == 0xFA && cmd[wantlen - 2] == 0x03) {
						gpGunShm->addInf[chnLog - 1][0] = 0x00;
						gpGunShm->addInf[chnLog - 1][1] = 0x00;
						return 0;    // 接收完毕
					} else {
						gpGunShm->wantLen[chnLog - 1] += cmd[wantlen - 3] + 2;
						return 1;    // 继续接收
					}
				}
				break; 
			default:
				RunLog("控制字符错误 [%02x]", cmd[1]);
				gpGunShm->addInf[chnLog - 1][0] = 0x00;
				return 0;
			}
			break;
		case 0x08:             // 恒山自助油机
			if (wantInd == gpGunShm->wantInd[chnLog - 1]) {
//					RunLog("-1111111111 left_fa: %d, cmdlen: %d, %s, ", 
//						left_fa, *cmdlen, Asc2Hex(cmd, gpGunShm->wantLen[chnLog - 1]));
				if (gpGunShm->wantLen[chnLog - 1] == 6) {
					real_wantlen = cmd[4] + 7;
					gpGunShm->wantLen[chnLog - 1]++;
//					RunLog("-----------real_wantLen: %d --------", real_wantlen);
//					RunLog("-1111111111 %s, ", Asc2Hex(cmd, gpGunShm->wantLen[chnLog - 1]));
					return 1;
				} else if ((*cmdlen) == real_wantlen && 
					((cmd[*cmdlen - 1] != 0xFA) || 
					 (cmd[*cmdlen - 1] == 0xFA && left_fa == 1))) {
//					RunLog("-=-=-=-=-=-=-==cmdlen: %d, idx: %d, wantLen: %d -=-=--==-=-=-=",
//						       	*cmdlen, idx, gpGunShm->wantLen[chnLog - 1]);
					gpGunShm->addInf[chnLog - 1][0] = 0x00;
					left_fa = 0;
					real_wantlen = 0;
					return 0;
				} else {
//					RunLog("----------idx: %d,  %02x----------------, ", idx, cmd[idx]);
//					RunLog("----------idx: %d,  %02x----------------, ", *cmdlen, cmd[*cmdlen - 1]);
//					RunLog("-1111111111 %s, ", Asc2Hex(cmd, gpGunShm->wantLen[chnLog - 1]));
					if (cmd[*cmdlen - 1] == 0xFA) {
//					RunLog("-1-left_fa: %d--------- %s, ", left_fa,
//						       	Asc2Hex(cmd, gpGunShm->wantLen[chnLog - 1]));
						if (left_fa == 0) {
							left_fa = 1;
							(*cmdlen)--;   // ignore this byte
							/*
					RunLog("-2-left_fa: %d, cmdlen: %d, wantLen: %d -=-=--==-=-=-=",
							left_fa, *cmdlen, gpGunShm->wantLen[chnLog - 1]);
							*/
							return 1;      // 直接返回, wantLen不能增加
						} else {
							left_fa = 0;
							/*
					RunLog("-3-left_fa: %d, cmdlen: %d, wantLen: %d -=-=--==-=-=-=",
							left_fa, *cmdlen, gpGunShm->wantLen[chnLog - 1]);
							*/
						}
					}

					gpGunShm->wantLen[chnLog - 1]++;
					return 1;
				}
			}
			left_fa = 0;
			break;
		case 0x09:             // 豪升变长
			if (wantInd == gpGunShm->wantInd[chnLog - 1]) {
//				gpGunShm->wantLen[chnLog - 1] = (cmd[4] * 256) + cmd[5] + 8;
				gpGunShm->wantLen[chnLog - 1] = cmd[5] + 8; // 长度不会大于255, 所以只用cmd[5]
				gpGunShm->addInf[chnLog - 1][0] = 0x00;
				return 1;
			}
			break;
		case 0x0F:             // 佳力佳变长
			if (wantInd == gpGunShm->wantInd[chnLog - 1]) {
				gpGunShm->wantLen[chnLog - 1] = cmd[2] + 3;
				gpGunShm->addInf[chnLog - 1][0] = 0x00;
				return 1;
			}
			break;
		case 0x10:             // 老长空卡机联动
			if (wantInd == gpGunShm->wantInd[chnLog - 1]) {
				if (gpGunShm->wantLen[chnLog - 1] == 2) {
					real_wantlen = cmd[1] + 2;
					gpGunShm->wantLen[chnLog - 1]++;
					return 1;
				} else if ((*cmdlen) == real_wantlen && 
					((cmd[*cmdlen - 1] != 0xFA) || 
					 (cmd[*cmdlen - 1] == 0xFA && left_fa == 1))) {
					gpGunShm->addInf[chnLog - 1][0] = 0x00;
					left_fa = 0;
					real_wantlen = 0;
					return 0;
				} else {
					if (cmd[*cmdlen - 1] == 0xFA) {
						if (left_fa == 0) {
							left_fa = 1;
							(*cmdlen)--;   // 忽略这个字节
							return 1;      // 直接返回, wantLen不能增加
						} else {
							left_fa = 0;
						}
					}

					gpGunShm->wantLen[chnLog - 1]++;
					return 1;
				}
			}
			left_fa = 0;
			break;
		default:
			break;
	}
	return 0;
}

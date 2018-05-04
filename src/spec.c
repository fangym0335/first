#include "pub.h"

/*���ӳ����ж�.�ж���ȡ�õĳ����Ƿ��㹻���ظ�ǰ��*/
/*�����з��ز��̶��������ݵĽ�����,��Ҫ�ڴ˴����Ӵ���*/
/*ϵͳ���ʱ��Ϊ���н��׾����ع̶����ȵ�����.��������δ�����,���ò��Ӵ˲���*/
/*�컯��˹,��֮�κ�............*/

/*
 * Guojie zhu @ 8.20
 * ��AddJustLen�Ĳ���cmdlen��int���͸�Ϊint *; cmd ��Ϊ��const����.
 */

/*
 * ������:
 *      0   ȡ������������.���Է���.
 *      1   δȡ������.��ʱ�������Ҫ�ĳ����ֶ�
 */

//int AddJustLen(int chnLog, const unsigned char *cmd, int cmdlen, int wantInd, int mark  )
int AddJustLen(int chnLog, unsigned char *cmd, int *cmdlen, int wantInd, int mark)
{
	int ret;
	int wantlen = 0;   // for case 0x07
	static int idx = 0;   // for case 0x08
	static int left_fa = 0;  // for case 0x08, ��� left_fa == 1 ��ʾ�����г�����һ��FA(��ת��FA)
	static int real_wantlen = 0;  // ����Ӧ�÷��ص����ݳ���, ������ת��FA

	switch( mark ) {
		case 0x01:	/*�������ͻ�ȡ����Ľ���.���صĳ������ͻ�(�����)��ǹ��*30+4.*/
			if (cmd[(*cmdlen) - 1] != 0xf0 ) {
				if (wantInd == gpGunShm->wantInd[chnLog-1] ) {
					/*�����ʱǰ���Ѿ���ʱ�����Ѿ����ĳ�����һ�����׵ķ���ֵ......*/
					gpGunShm->wantLen[chnLog-1] += 30;
					return 1;
				}
			}
			break;
		case 0x03:	/*̫��Э���ѯ�ͻ�״̬��*//*�ڶ�λ06��ʾ�Ǽ���״̬,����8λ.�ڶ�λ0CΪ����״̬,����14λ*/
			if (wantInd == gpGunShm->wantInd[chnLog-1]) {	/*��Ϣû��ʱ*/
				if (gpGunShm->wantLen[chnLog - 1] < cmd[1] + 2) {
					gpGunShm->wantLen[chnLog - 1] = cmd[1] + 2;
					gpGunShm->addInf[chnLog - 1][0] = 0x00;
					return 1;
				} 
			}
			break;
		case 0x04: /*����Э�顣ȡ��Ʒ�ۼ�ʱ�á�����λ�����ݳ���.���棫2��crc*/
			if (wantInd == gpGunShm->wantInd[chnLog-1]) {	/*��Ϣû��ʱ*/
				gpGunShm->wantLen[chnLog-1] = 3 + cmd[2] + 2;
				gpGunShm->addInf[chnLog - 1][0] = 0x00;
				return 1;
			}
			break;
		case 0x06:		//2006.02.17 for 35/39 return characters
			if( cmd[2] == 0x1e ) {
				if(wantInd == gpGunShm->wantInd[chnLog-1] ) {	/*��Ϣû��ʱ*/
					if( gpGunShm->wantLen[chnLog-1] == 35 ) {
						return 0;
					} else {
						gpGunShm->wantLen[chnLog-1] = 35;
						return 1;
					}
				}
			} else {
		      // 	if( cmd[2] == 0x22 ) {  // �Ƴ�����ж�,�����������Ӱ�����,GuojieZhu@2009.12.14
				if(wantInd == gpGunShm->wantInd[chnLog-1] )	/*��Ϣû��ʱ*/
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
		case 0x07:             // ����
			switch (cmd[1] & 0xF0) {  // �ж� CTRL
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
					(gpGunShm->wantLen[chnLog - 1])++;      // ȡ��һ��DATA�ĳ��� 
					gpGunShm->addInf[chnLog - 1][1] = 0x0F;
					return 1;
				} else if (gpGunShm->addInf[chnLog - 1][1] == 0x0F) {
//					RunLog("-2---------- %s ", Asc2Hex(cmd, gpGunShm->wantLen[chnLog - 1]));
					gpGunShm->wantLen[chnLog - 1] += cmd[3] + 4;
					gpGunShm->addInf[chnLog - 1][1] = 0xF0; // ȡ��һ��DATA
					return 1;    // ��������
				} else {
//					RunLog("-3---------- %s, ", Asc2Hex(cmd, gpGunShm->wantLen[chnLog - 1]));
					wantlen = gpGunShm->wantLen[chnLog - 1];
					// ����Ҫ�ж��������Ƿ���0xFA, �����Ҫ��ǰ����������ӷ�ȥ��
					// �������ϴ����ݶ���BCD��ʽ, ����0xFAֻ���ܳ�����CRCУ����
					if (cmd[wantlen - 3] == 0xFA &&  cmd[wantlen - 4] == 0x10) {
						// ��������������0x10(SF ���� DLE ���滺����)
						cmd[wantlen - 4] = cmd[wantlen - 3];
						cmd[wantlen - 3] = cmd[wantlen - 2];
						cmd[wantlen - 2] = cmd[wantlen - 1];
						(*cmdlen)--;   // ����һ���ֽ�
//					RunLog("-------------cmdlen: %d -------", *cmdlen);
//					RunLog("-4---------- %s, ", Asc2Hex(cmd, gpGunShm->wantLen[chnLog - 1]));
						return 1;
					} else if (cmd[wantlen - 2] == 0xFA && cmd[wantlen - 3] == 0x10) {
						// ��������������0x10(SF ���� DLE ���滺����)
						cmd[wantlen - 3] = cmd[wantlen - 2];
						cmd[wantlen - 2] = cmd[wantlen - 1];
						(*cmdlen)--;   // ����һ���ֽ�
//					RunLog("-------------cmdlen: %d -------", *cmdlen);
//					RunLog("-4---------- %s, ", Asc2Hex(cmd, gpGunShm->wantLen[chnLog - 1]));
						return 1;
					} else if (cmd[wantlen - 1] == 0xFA && cmd[wantlen - 2] == 0x03) {
						gpGunShm->addInf[chnLog - 1][0] = 0x00;
						gpGunShm->addInf[chnLog - 1][1] = 0x00;
						return 0;    // �������
					} else {
						gpGunShm->wantLen[chnLog - 1] += cmd[wantlen - 3] + 2;
						return 1;    // ��������
					}
				}
				break; 
			default:
				RunLog("�����ַ����� [%02x]", cmd[1]);
				gpGunShm->addInf[chnLog - 1][0] = 0x00;
				return 0;
			}
			break;
		case 0x08:             // ��ɽ�����ͻ�
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
							return 1;      // ֱ�ӷ���, wantLen��������
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
		case 0x09:             // �����䳤
			if (wantInd == gpGunShm->wantInd[chnLog - 1]) {
//				gpGunShm->wantLen[chnLog - 1] = (cmd[4] * 256) + cmd[5] + 8;
				gpGunShm->wantLen[chnLog - 1] = cmd[5] + 8; // ���Ȳ������255, ����ֻ��cmd[5]
				gpGunShm->addInf[chnLog - 1][0] = 0x00;
				return 1;
			}
			break;
		case 0x0F:             // �����ѱ䳤
			if (wantInd == gpGunShm->wantInd[chnLog - 1]) {
				gpGunShm->wantLen[chnLog - 1] = cmd[2] + 3;
				gpGunShm->addInf[chnLog - 1][0] = 0x00;
				return 1;
			}
			break;
		case 0x10:             // �ϳ��տ�������
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
							(*cmdlen)--;   // ��������ֽ�
							return 1;      // ֱ�ӷ���, wantLen��������
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

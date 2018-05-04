#include "lischn.h"
#include "ifsf_pub.h"

/*
 * func: ѡ������ͨ�������������
 */
int ListenChannel( int chnid )
{
	int i;
	unsigned char mtype;	/*��ͨ�����ӵĻ�������*/

	if( NULL == gpIfsf_shm ){		
		UserLog("�����ڴ�ֵ����,Listen Channelʧ��");
		return -1;
	}	
	
	
	/*��Ҫ�鿴��ͨ�����ӵĻ�������*/
	for(i = 0; i < MAX_CHANNEL_CNT; i++) {
		// �����߼�ͨ���Ų��һ�������
		if(gpIfsf_shm->node_channel[i].chnLog== chnid) {
			mtype = gpIfsf_shm->node_channel[i].nodeDriver;
			break;
		}
	}

	if(i == MAX_CHANNEL_CNT) {
		/*ERROR*/
		UserLog( "δ�ҵ��߼�ͨ����[%d]��Ӧ��ǹ��Ϣ", chnid );
		return -1;
	}

	switch(mtype) {
		case 0x01:	/*��������*/
			ListenChn001(chnid);
			break;
		case 0x02:	/*��ɽ��ͨ˰��(��ǹ)*/
			ListenChn002(chnid);
			break;
		case 0x03:	/*̫��*/
			ListenChn003(chnid);
			break;
		case 0x04:	/*���� ��ѯ����35�ֽ�*/
			ListenChn004(chnid);
			break;
		case 0x05:	/*����5.0*/
			ListenChn005(chnid);
			break;
		case 0x06:	/*���¶�*/
			ListenChn006(chnid);
			break;
		case 0x07:	/*����*/
			ListenChn007(chnid);
			break;
		case 0x08:	/*��ɽ����*/
			ListenChn008(chnid);
			break;
		case 0x09:	/*���ʺ���*/
			ListenChn009(chnid);
			break;
		case 0x0A:	/*�ϳ���*/
			ListenChn010(chnid);
			break;
		case 0x0B:	/*�³���*/
			ListenChn011(chnid);
			break;
		case 0x0C:	/*��ӯ*/
			ListenChn012(chnid);
			break;
		case 0x0D:	/*����*/
			ListenChn013(chnid);
			break;
		case 0x0E:	/*����*/
			ListenChn014(chnid);
			break;
		case 0x0F:	/*������*/
			ListenChn015(chnid);
			break;
		case 0x10:	/*ģ����ͻ�*/
			ListenChn016(chnid);
			break;
		case 0x11:	/*����*/
			ListenChn017(chnid);
			break;
		case 0x12:	/*���տ�������*/
			ListenChn018(chnid);
			break;
		case 0x13:	/*��ɽTMC(UDCЭ��)*/
			ListenChn019(chnid);
			break;
		case 0x14:	/*��ɽTQC(UDCЭ��)*/
			ListenChn020(chnid);
			break;
		case 0x16:	/*��������*/ /* ����lischn021��Ϊ�޳�OPTʹ���˸����� */
			ListenChn022(chnid);
			break;
		default:
			/*error*/
			exit(-1);
	}
	return 0;
}


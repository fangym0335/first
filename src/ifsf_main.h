/*******************************************************************
-------------------------------------------------------------------
�ڱ��豸�������,ÿ����Э��ת���ļ��ͻ���ÿ��������
��ת��Ϊһ����Զ�����IFSF���ͻ�,
�ڱ��豸�������,IFSF����������һ�������ڴ�IFSF_SHM
-------------------------------------------------------------------
2007-06-26 by chenfm
*******************************************************************/
#ifndef __IFSF_MAIN_H__
#define __IFSF_MAIN_H__

#include <stdio.h>
#include "ifsf_def.h"
#include "ifsf_pub.h"

#include "lrc.h"
#include "bcd.h"
#include "lischn.h"
#include "pidctrl.h"
#include "log.h"
#include "setcfg.h"
#include "tty.h"

// Release Version


#define PCD_VER       "v1.06.5540 - 2017.05.24"
#define PCD_VERSION   "v1.06.5540 - 2017.05.24"



/*ȫ�ֱ�����*/

GunShm	* gpGunShm; //oil_main�е�����Ҫ�����ݽṹ.ɾ����GunMsg.

//#ifdef IFSF
IFSF_SHM * gpIfsf_shm;
//#endif
//unsigned char	cnt =0;		/*���������*/
//Pid*	gPidList;
//char	isChild = 0;
/*------------------------------------------------------------------*/
//private����,����Ϊ��.c�ļ���main����Ҫ�õ���һЩ����:
/*��ʼ�������ڴ�gpIfsf_shm, ��������index�ؼ�������ֵ��:
*/
int InitIfsfShm();
/*-------------------------------------------------------------------*/
#endif  




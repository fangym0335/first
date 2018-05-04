#ifndef __POWER_H__
#define __POWER_H__

#include <sys/socket.h>
#include <linux/netlink.h>
#include "pub.h"

#define  NETLINK_POWERDOWN   20
#define  POWER_STATUS        21

// For send request
typedef struct {
	struct nlmsghdr hdr;
} MSG_PFS_TO_KERNEL;

// Power status message
typedef struct {
	struct nlmsghdr hdr;
	int pstatus;             // 0 - power failure, 1 - power good
} PWMSG;

typedef struct {
	unsigned char node;
	unsigned char fp;
	unsigned char lnz;
	unsigned char Log_Noz_Vol_Total[7];
} BAK_LNZ_TOTAL;

typedef struct{
	unsigned char node;
	unsigned char fp;
	unsigned char Transaction_Sequence_Nb[2];
} BAK_TR_SEQ_NB;


/*
 *掉电保护服务,在掉电的情况下,把ifsf_fp_shm数据区的交易数据写入NAND FLASH中,
 *ifsf_fp_shm数据区的交易数据是重要的加油机数据区.
 *掉电中断与应用程序数据交换使用syscall函数power_status(),
 *PowerFailureServ用阻塞读判断掉电中断.
*/

int WriteNandFlash();
int PowerFailureServ();
int ReadDelNandFlash();
int RecoveryTrAsOffline();
int RecoveryVolTotal();
int RecoveryTrSeqNb();

#endif



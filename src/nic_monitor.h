#ifndef __NIC_MONITOR_H__
#define __NIC_MONITOR_H__

#include <sys/socket.h>
#include <linux/netlink.h>
#include "ifsf_pub.h"

#define  NETLINK_MACB        21
#define  NIC_STATUS          21


// For send request
typedef struct {
	struct nlmsghdr hdr;
} MSG_NM_TO_KERNEL;

// Interface info
typedef struct {
	unsigned char port;      // 0 - 3
	unsigned char status;    // 0 - disconnect, 1 - connected
} IFINFO;

// NIC status message
typedef struct {
	struct nlmsghdr hdr;
	IFINFO ifinfo;
} NICMSG;


// functions

int NIC_Monotor();

#endif


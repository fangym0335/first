/*
 * =====================================================================================
 *
 *       Filename:  pw_time.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  03/05/09 11:07:34
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Guojie Zhu (mn), <zhugj@css-intelligent.com>
 *        Company:  CSSI
 *
 * =====================================================================================
 */

#include <sys/timeb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include "power.h"

#include <time.h>
#define power_status() syscall(348)

int main()
{
	static int flag = 0;
	static long count = 0;
	struct timeb tp;
	struct tm *tm;
	int status;
	int ret;
	int trytime = 3;    // 提前初始化好
	int fail_cnt = 0;
	int dst_addr_len;
	int sock_fd;
	struct sockaddr_nl usr_addr, kernel_addr;
	MSG_PFS_TO_KERNEL msg_to_kernel;
	PWMSG pwmsg_from_kernel;


	ftime(&tp);
	tm = localtime(&(tp.time));

	printf("%02d-%02d %02d:%02d:%02d.%03d pw_time(netlink) startup.......... \n",
		(tm->tm_mon + 1), (tm->tm_mday), (tm->tm_hour),
		(tm->tm_min), (tm->tm_sec), (tp.millitm));

	// ------------------------- Test 7.29 Using netlink----------------//

	sock_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_POWERDOWN);
	if (sock_fd < 0) {
		printf("Power failured server allocate socket failed\n");
		return -1;
	}
	
	// src addr init
	memset(&usr_addr, 0, sizeof(usr_addr));
	usr_addr.nl_family  = AF_NETLINK;
	usr_addr.nl_pid = getpid();             // self pid
	usr_addr.nl_groups = 0;                 // not in mcast groups

	if (bind(sock_fd, (struct sockaddr*)&usr_addr, sizeof(usr_addr)) != 0 ) {
		printf("Bind usr_addr to sock_fd failed\n");
		goto FAIL_EXIT; 
	}

	// dst addr init
	memset(&usr_addr, 0, sizeof(usr_addr));
	kernel_addr.nl_family  = AF_NETLINK;
	kernel_addr.nl_pid = 0;                  // For Linux kernel
	kernel_addr.nl_groups = 0;               // unimcast 

	// Fill Msg
	msg_to_kernel.hdr.nlmsg_len = NLMSG_LENGTH(0);   // Just a request, no additional data
	msg_to_kernel.hdr.nlmsg_flags = 0;
	msg_to_kernel.hdr.nlmsg_type = POWER_STATUS;
	msg_to_kernel.hdr.nlmsg_pid =  usr_addr.nl_pid;  // sender's pid

	// send msg to kernel
	
	ret = sendto(sock_fd, &msg_to_kernel, msg_to_kernel.hdr.nlmsg_len, 0,
				(struct sockaddr*)&kernel_addr, sizeof(kernel_addr));
	if (ret < 0) {
		printf("Send message to kernel failed, errno: %s\n", strerror(errno));
		goto FAIL_EXIT; 
	}

	
	while (1) {
		dst_addr_len = sizeof(struct sockaddr_nl);
		ret = recvfrom(sock_fd, &pwmsg_from_kernel, sizeof(pwmsg_from_kernel), 0,
					(struct sockaddr*)&kernel_addr, &dst_addr_len);
		if (ret < 0) {
			printf("Recv message from kernel error!\n");
			goto FAIL_EXIT; 
		}

		// 1
		if (pwmsg_from_kernel.pstatus != 0) {
			continue;
		} 

		count++;
		ftime(&tp);
		tm = localtime(&(tp.time));
		printf("\n%02d-%02d %02d:%02d:%02d.%03d begin of pw down(%d)\n",
			(tm->tm_mon + 1), (tm->tm_mday), (tm->tm_hour),
			(tm->tm_min), (tm->tm_sec), (tp.millitm), count);

		// 特殊处理 , JIE @ 2008/11/14
		while ((ret = power_status()) == 0) {
			printf("#### power status is : %d \n", ret);
			usleep(2000);
		};

		ftime(&tp);
		tm = localtime(&(tp.time));
		printf("\n%02d-%02d %02d:%02d:%02d.%03d end of pw down\n",
			(tm->tm_mon + 1), (tm->tm_mday), (tm->tm_hour),
			(tm->tm_min), (tm->tm_sec), (tp.millitm));

	}

#if 0
	while (1) {
		status = power_status();
		usleep(2000);
		printf(" %d", status);

		if (status == 1) {
			if (flag == 0) {
				count++;
				flag = 1;

				ftime(&tp);
				tm = localtime(&(tp.time));

				printf("\n%02d-%02d %02d:%02d:%02d.%03d begin of pw down(%d)\n",
					(tm->tm_mon + 1), (tm->tm_mday), (tm->tm_hour),
					(tm->tm_min), (tm->tm_sec), (tp.millitm), count);
		       }
		} else if (flag == 1) {
			flag = 0;
			ftime(&tp);
			tm = localtime(&(tp.time));

			printf("\n%02d-%02d %02d:%02d:%02d.%03d end of pw down\n",
				(tm->tm_mon + 1), (tm->tm_mday), (tm->tm_hour),
				(tm->tm_min), (tm->tm_sec), (tp.millitm));
		}
	}

#endif
FAIL_EXIT:
	close(sock_fd);

	return 0;
}


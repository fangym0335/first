/*
 * =====================================================================================
 *
 *       Filename:  nic_monitor.c
 *
 *    Description:  监视网口连接状态, 一但有断开就置位gpIfsf_shm->need_reconnect
 *
 *        Version:  1.0
 *        Created:  07/29/08 18:47:14
 *       Revision:  none
 *       Compiler:  arm-linux-gcc
 *
 *         Author:  Guojie Zhu (mn), <zhugj@css-intelligent.com>
 *        Company:  CSSI
 *
 * =====================================================================================
 */


#include "nic_monitor.h"

int  NIC_Monitor()
{
	int sock_fd;
	int dst_addr_len;
	int ret;
	struct sockaddr_nl usr_addr, kernel_addr;
	MSG_NM_TO_KERNEL msg_to_kernel;
	NICMSG nic_msg;

	sleep(2);
	RunLog("NIC Status Monitor ...");

	sock_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_MACB);
	if (sock_fd < 0) {
		RunLog("NIC Monitor allocate socket failed");
		return -1;
	}

	// usr_addr init
	memset(&usr_addr, 0, sizeof(usr_addr));
	usr_addr.nl_family = AF_NETLINK;
	usr_addr.nl_pid = getpid();            // self pid
	usr_addr.nl_groups = 0;                // not in mcast groups

	// bind
	if (bind(sock_fd, (struct sockaddr*)&usr_addr, sizeof(usr_addr)) != 0) {
		RunLog("NIC Monitor Bind usr_addr to sock_fd failed");
		goto FAIL_EXIT;
	}

	// kernel_addr init
	memset(&kernel_addr, 0, sizeof(kernel_addr));
	kernel_addr.nl_family = AF_NETLINK;
	kernel_addr.nl_pid = 0;                        // for linux kernel
	kernel_addr.nl_groups = 0;                     // unicast

	memset(&msg_to_kernel, 0, sizeof(msg_to_kernel));
	msg_to_kernel.hdr.nlmsg_len = NLMSG_LENGTH(0); // Just a request , no additional data
	msg_to_kernel.hdr.nlmsg_flags = 0;
	msg_to_kernel.hdr.nlmsg_type = NIC_STATUS;
	msg_to_kernel.hdr.nlmsg_pid = usr_addr.nl_pid;    // sender's pid


	ret = sendto(sock_fd, &msg_to_kernel, msg_to_kernel.hdr.nlmsg_len, 0,
		       	(struct sockaddr*)&kernel_addr, sizeof(kernel_addr));
	if (ret < 0) {
		RunLog("NIC Monitor Send message to kernel failed");
		goto FAIL_EXIT;
	}

	while (1) {
		dst_addr_len = sizeof(struct sockaddr_nl);
		ret = recvfrom(sock_fd, &nic_msg, sizeof(nic_msg), 0,
			       	(struct sockaddr*)&kernel_addr, &dst_addr_len);
		if (ret < 0) {
			RunLog("NIC Monitor recvfrom kernel error");
			goto FAIL_EXIT;
		}

		RunLog("Port %d status is %d", nic_msg.ifinfo.port, nic_msg.ifinfo.status);
		if (nic_msg.ifinfo.status == 0) {
			gpIfsf_shm->need_reconnect = 1;
		}
	}

FAIL_EXIT:
	close(sock_fd);
	return -1;
}


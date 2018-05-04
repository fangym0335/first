/*
 * =====================================================================================
 *
 *       Filename:  ifreset.c
 *
 *    Description:  检测到授权钥匙转动6次则复位IP为出厂设置10.1.2.107/24
 *
 *        Version:  1.1       (修改说明见底部)
 *        Created:  01/07/09 14:17:07
 *       Revision:  none
 *       Compiler:  arm-linux-gcc
 *
 *         Author:  Guojie Zhu (mn), <zhugj@css-intelligent.com>
 *        Company:  CSSI
 *
 * =====================================================================================
 */


#include <stdio.h>
#include <time.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/timeb.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <errno.h>

#define  get_lock_status()  syscall(349)
#define  plk_notice(n, t)  syscall(350, n, t)

#define  NETLINK_LOCK_STATUS  21

#define  VERSION "v1.2 (2009.08.09)"

typedef struct {
	struct nlmsghdr hdr;
} MSG_USR_TO_KERNEL;

typedef struct {
	struct nlmsghdr hdr;
	int status;
} LOCK_STATUS_MSG;

void RunLog( const char *fmt, ... );

static int log_flag = 0;                  // 标记日志是写入指定文件还是stdout


int main(int argc, char **argv)
{
	time_t t_start;
	struct timeb tp;
	struct tm *tm;
	int poll_mode = 0;      // 0 - normal, 1 - fast
	int ret;
	int pre_lock_status, lock_status;
	static unsigned int turn_cnt = 0;
	int dst_addr_len;
	int sock_fd;
	struct sockaddr_nl usr_addr, kernel_addr;
	MSG_USR_TO_KERNEL msg_to_kernel;
	LOCK_STATUS_MSG status_msg_from_kernel;

	if (argc == 2) {
	       	if (strcmp(argv[1], "-v") == 0) {
			printf("resetipd %s\n", VERSION);
			return 0;
		} else if (strcmp(argv[1], "-w") == 0) {
			log_flag = 256;
		}
	}

	RunLog("resetipd %s Startup ..... ", VERSION);

	sock_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_LOCK_STATUS);
	if (sock_fd < 0) {
		RunLog("resetipd server allocate socket failed");
		return -1;
	}
	
	// src addr init
	memset(&usr_addr, 0, sizeof(usr_addr));
	usr_addr.nl_family = PF_NETLINK;
	usr_addr.nl_pid = getpid();             // self pid
	usr_addr.nl_groups = 0;                 // not in mcast groups

	if (bind(sock_fd, (struct sockaddr*)&usr_addr, sizeof(usr_addr)) != 0 ) {
		RunLog("Bind usr_addr to sock_fd failed");
		goto FAIL_EXIT; 
	}

	// dst addr init
	memset(&kernel_addr, 0, sizeof(kernel_addr));
	kernel_addr.nl_family = PF_NETLINK;
	kernel_addr.nl_pid = 0;                  // For Linux kernel
	kernel_addr.nl_groups = 0;               // unimcast 

	// Fill Msg
	memset(&msg_to_kernel, 0, sizeof(msg_to_kernel));
	msg_to_kernel.hdr.nlmsg_len = NLMSG_LENGTH(0);   // Just a request, no additional data
	msg_to_kernel.hdr.nlmsg_flags = 0;
	msg_to_kernel.hdr.nlmsg_type = 21;              // 21
	msg_to_kernel.hdr.nlmsg_pid = usr_addr.nl_pid;  // sender's pid

	// send msg to kernel
	ret = sendto(sock_fd, &msg_to_kernel, msg_to_kernel.hdr.nlmsg_len, 0,
				(struct sockaddr*)&kernel_addr, sizeof(kernel_addr));
	if (ret < 0) {
		RunLog("Send message to kernel failed, errno: %s", strerror(errno));
		goto FAIL_EXIT; 
	}

	memset(&status_msg_from_kernel, 0, sizeof(status_msg_from_kernel));

	// 1. Initial lock_status
	lock_status = get_lock_status();
	pre_lock_status = lock_status;

	// 2. Polling
	while (1) {
		dst_addr_len = sizeof(struct sockaddr_nl);
		ret = recvfrom(sock_fd, &status_msg_from_kernel, sizeof(status_msg_from_kernel), 0,
					(struct sockaddr*)&kernel_addr, &dst_addr_len);
		if (ret < 0) {
			RunLog("Recv lock status message from kernel error!");
			goto FAIL_EXIT; 
		}

		if (status_msg_from_kernel.status != pre_lock_status) {
			pre_lock_status = status_msg_from_kernel.status;
			turn_cnt++;
		} else {
			continue;
		}

//		printf("resetipd recved msg from kernel, %d bytes, lock_status: %d, turn_cnt: %d\n",
//						ret, status_msg_from_kernel.status, turn_cnt);

		if (turn_cnt == 1) {
			t_start = time(NULL);
		} else {
			if ((time(NULL) - t_start) >= 30) {
				// 超过1分钟则重新计数
				RunLog("超时! 重新计数...........");
				turn_cnt = 1;
				t_start = time(NULL);
			} else if (turn_cnt >= 6) {   // 达到次数, 复位IP
				turn_cnt = 0;

				// reset ip
				if (system("/usr/local/bin/changeip 10.1.2.107") != 0) {
					// O 6声短鸣报错
				       	plk_notice(1, 30); usleep(8000);
				       	plk_notice(1, 30); usleep(8000);
				       	plk_notice(1, 30); usleep(8000);
				       	plk_notice(1, 30); usleep(8000);
				       	plk_notice(1, 30); usleep(8000);
				       	plk_notice(1, 30);
					continue;
				} 

				// O 复位成功长鸣
				plk_notice(1, 800);
				RunLog("复位 IP 为 10.1.2.107 成功!");
				continue;
			} // end of if (turn_cnt >= 6)
		} // end of if (turn_cnt == 1)

		// O 正常情况下短鸣 , 提示拧转有效, 计数一次
		plk_notice(1, 50); 
		RunLog("钥匙翻转...");

	};  // end of while (1)

FAIL_EXIT:
	close(sock_fd);
	return -1;
}

void RunLog( const char *fmt, ... )
{
	va_list argptr;
	unsigned char date_time[7] = {0};
	static char sMessage[64];
	struct timeb tp;
	struct tm *tm;
	int fd;

	memset(sMessage, 0, sizeof(sMessage));

	ftime(&tp);
	tm = localtime(&(tp.time));


	sprintf(sMessage, "%02d-%02d %02d:%02d:%02d.%03d ", (tm->tm_mon + 1), (tm->tm_mday),
			(tm->tm_hour), (tm->tm_min), (tm->tm_sec), (tp.millitm));
	
	va_start(argptr, fmt);
	vsprintf(sMessage + strlen(sMessage), fmt, argptr);
	va_end(argptr);

	if (log_flag != 0) {
		if ((fd = open("/home/Data/log_backup/resetipd.log",
				O_RDWR | O_CREAT | O_APPEND, 00644)) < 0) {
			printf("Open log_backup/resetipd.log failed[%s]\n", strerror(errno));
		}
		sprintf(sMessage + strlen(sMessage), "\n");
		write(fd, sMessage, strlen(sMessage));
		close(fd);
	} else {
		printf(" %s\n", sMessage);
	}

	return;
}

/*
 * 版本修订说明
 * ----------------------------------------
 * 1.0	(2009.02.08)
 * 初始版本
 *
 * 1.1  (2009.08.04)
 * 增加了日志, 日志中增加时间戳
 *
 * 1.2  (2009.08.09)
 * 增加写日志到文件的处理, -w 选项
 *
 */
	
// End of file ifreset.c

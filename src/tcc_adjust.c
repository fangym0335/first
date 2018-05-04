/*
 * =====================================================================================
 *
 *       Filename:  tcc.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  05/30/2011 05:33:49 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Guojie Zhu (mn), <zhugj@css-intelligent.com>
 *        Company:  CSSI
 *
 * =====================================================================================
 */
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/timeb.h>
#include <fcntl.h>
#include <stdarg.h>
#include "tcc_adjust.h"
#include "pubsock.h"
#include "log.h"

#define _50K    51200
#define _200K   204800
#define _500K   512800
#define _2M     2112000   // 16Block (16x (128K + 4K))
#define DATA_FILE_BACKUP_SIZE  _2M



/*
 * @brief 当 TCC_ADJUST_DATA_FILE 达到 TCC_ADJUST_DATA_FILE_SIZE * 1024 后转存到 /home/Data/log_backup/ 下
 * @param int size_limit_flag   0 - 不判断大小执行转储, 非零 - 按预设大小转储
 * @return 成功返回0, 失败返回 -1
 */
int dump_tcc_adjust_data_file(int size_limit_flag)
{
	int fd;
	int _fd;
	int ret;
	static int data_file_size = 0;
	struct stat f_stat;
	unsigned char cmd[(2 * sizeof(TCC_ADJUST_DATA_FILE)) + 16];
	unsigned char date_time[7] = {0};

	if (data_file_size == 0) {
		data_file_size = atoi(TCC_ADJUST_DATA_FILE_SIZE);
		if (data_file_size > 6000 || data_file_size < 10) {
			TCCADLog("TCC-AD: TCC_AD_FILE_SIZE 配置(%d)超出允许范围,设置默认值200K", data_file_size);
			data_file_size = _200K;
		} else {
			data_file_size *= 1024;
			TCCADLog("TCC-AD: TCC_AD_FILE_SIZE x 1024 = %d", data_file_size);
		}
	}

	if((fd = open(TCC_ADJUST_DATA_FILE, O_RDONLY, 00644)) < 0) {
		return -1;
	}

	if (fstat(fd, &f_stat) != 0) {
		return -1;
	}
	
	TCCADLog("TCC-AD: tcc_adjust_data_file st_size: %d ", f_stat.st_size);
	if ((long)f_stat.st_size > data_file_size || size_limit_flag == 0) {
		getlocaltime(date_time);

		system("[ ! -d /home/Data/log_backup ] && mkdir  /home/Data/log_backup");

		sprintf(cmd, "mv %s /home/Data/log_backup/tank_data[%02x%02x%02x%02x%02x%02x%02x].bak",
					TCC_ADJUST_DATA_FILE, date_time[0], date_time[1], date_time[2],
					date_time[3], date_time[4], date_time[5], date_time[6]);
		Act_P(TCC_ADJUST_SEM_INDEX);
		system(cmd);
		Act_V(TCC_ADJUST_SEM_INDEX);

		RunLog("TCC-AD: 转储校罐数据文件至 FTP 目录");
	} else {
		close(fd);
		return 0;
	}
	
	close(fd);

	if((fd = open(TCC_ADJUST_DATA_FILE, O_CREAT , 00644)) < 0) {
		return -1;
	}

	close(fd);

	return 0;
}

/*
 * @brief  写入采集数据到 TCC_ADJUST_DATA_FILE
 * @param  date_time   指定日期时间,记录最后一笔实时加油数据需要, NULL-不特别指定
 * @note   采集数据格式:
 *         "油罐号,液位高度,节点号,面号,逻辑枪号,加油量,状态1,加油标记,温度,前泵码,后泵码"
 *         状态1: (55:开始加油) | (FF|加油中) | (AA:交易) 
 *         加油标记: 0 - 液位数据; 1 - 加油数据
 */

int write_tcc_adjust_data(const unsigned char *date_time, const char *fmt, ...)
{
	int fd;
	va_list argptr;
	struct timeb tp;
	struct tm *tm;
	static char data[100];

	ftime(&tp);
	tm = localtime(&(tp.time));

	Act_P(TCC_ADJUST_SEM_INDEX);
	memset(data, 0, sizeof(data));

	if (date_time == NULL) {
		sprintf(data, "%04d-%02d-%02d %02d:%02d:%02d,", (tm->tm_year + 1900),
			(tm->tm_mon + 1), (tm->tm_mday), (tm->tm_hour), (tm->tm_min), (tm->tm_sec));
	} else {
		sprintf(data, "%02x%02x-%02x-%02x %02x:%02x:%02x,",
			date_time[0], date_time[1], date_time[2],
			date_time[3], date_time[4], date_time[5], date_time[6]);
	}

	va_start(argptr, fmt);
	vsprintf(data + strlen(data), fmt, argptr);
	va_end(argptr);

	sprintf(data + strlen(data), "\n");

	if ((fd = open(TCC_ADJUST_DATA_FILE, O_RDWR | O_CREAT | O_APPEND, 00644)) < 0) {
		RunLog("TCC-AD: Open %s failed\n", TCC_ADJUST_DATA_FILE);
		return;
	}

	write(fd, data, strlen(data));
	Act_V(TCC_ADJUST_SEM_INDEX);

	close(fd);

	return;
}

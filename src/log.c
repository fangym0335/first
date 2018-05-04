/*!
 * \file  log.c
 * \brief �Զ�����־����
 *
 *        ʹ��log4c��־��,�Զ��������ʽ cssi_dated, opt
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/timeb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <log4c.h>
#include <malloc.h>
#include <assert.h>
//#include "def.h"
#include "log.h"

#define  LOG_HEAD_LEN    37           // ��־ͷ������:Date Time + LOG_LEVEL + FILE:LINE
#define MAX_ORDER_LEN	160

#define MAX_TIME_FILE_LINE_LEN   40
#define _528K  528000      // 4Block(4 x (128K + 4K))
#define _500K  500000 
#define SINOPEC_LOG "/home/App/ifsf/log/sinopec.log"
static unsigned char run_log[48];

/*!
 * ������־��ʽ����cssi_dated,ʹ��CSTʱ��
 */
static const char* cssi_dated_format(const log4c_layout_t* a_layout,
				     const log4c_logging_event_t* a_event)
{
    static char buffer[1024];

    struct tm   tm;
    localtime_r(&a_event->evt_timestamp.tv_sec, &tm);
    snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d.%03ld %-8s \n",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec,
             a_event->evt_timestamp.tv_usec / 1000
            // log4c_priority_to_string(a_event->evt_priority),
            // a_event->evt_msg
			);

    return buffer;
}


/*!
 * ������־��ʽ����opt
 */
static const char* opt_format(const log4c_layout_t*  a_layout,
			      const log4c_logging_event_t* a_event)
{
	static char buffer[1024];
	struct tm   tm;

	localtime_r(&a_event->evt_timestamp.tv_sec, &tm);
	snprintf(buffer, sizeof(buffer),
		"%02d-%02d %02d:%02d:%02d.%03ld %-5s [%-4d] \n",
		tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
		a_event->evt_timestamp.tv_usec / 1000
		// log4c_priority_to_string(a_event->evt_priority),
		// getpid(), a_event->evt_msg
		);

	return buffer;
}


// -------------------------------------------------------------------------------
const log4c_layout_type_t log4c_layout_type_cssi_dated = {
	"cssi_dated",
	cssi_dated_format,
};


const log4c_layout_type_t log4c_layout_type_opt  = {
	"opt",
	opt_format,
};

// -------------------------------------------------------------------------------
static const log4c_layout_type_t * const layout_types[] = {
	&log4c_layout_type_cssi_dated,
	&log4c_layout_type_opt    
};

// -------------------------------------------------------------------------------
static int nlayout_types = (int)(sizeof(layout_types) / sizeof(layout_types[0]));

/*!
 * ��ʼ�����Զ�����־��ʽ
 */
int init_opt_formatters()
{
	int i = 0;

	for (i = 0; i < nlayout_types; i++) {
	//	log4c_layout_type_set(layout_types[i]);
	}

	return 0;
}

// -------------------------------------------------------------------------------

static int app_log_level = LOG_LEVEL_DEBUG;   //!< Ӧ�õĵ�ǰ��־����ȼ�, ͨ��log_set_level/log_get_level����
static char app_log_file[256];       //!< Ӧ�õĵ�ǰ��־����ļ�·��, ͨ��log_set_logfile/log_get_logfile����

static int flag_log_file_valid = 0;           // �����־�ļ��Ƿ���ȷ����


// -------------------------------------------------------------------------------

/*!
 * \brief  ��ʼ����־���
 * \prarm[in]  log_file ��־�ļ�·��
 * \return int �ɹ�����0��ʧ�ܷ���-1
 */
int l_log_init(char *log_file)
{
	assert(log_file != NULL);

	if (strlen(log_file) < sizeof(app_log_file)) {
		strcpy(app_log_file, log_file);
		flag_log_file_valid = 1;
		return 0;
	}
	return 0;
}

/*!
 * \brief  ������־����ȼ�
 * \prarm  level ��־�ȼ�
 * \return void
 */
void log_set_level(int level)
{
	if (level >= LOG_LEVEL_ERROR && level <= LOG_LEVEL_DEBUG) {
		app_log_level = level;
	}

	return;
}

/*!
 * \brief  ������־����ȼ�
 * \return int ���ص�ǰ��־����ȼ�
 */
int inline log_get_level()
{
	return app_log_level;
}

/*!
 * \brief  ������־�ļ�·��
 * \prarm[in]  log_file ��־�ļ�·��
 * \return     �ɹ�����0��ʧ�ܷ���-1
 */
int log_set_logfile(char *log_file)
{
	if (strlen(log_file) < sizeof(app_log_file)) {
		strcpy(app_log_file, log_file);
		flag_log_file_valid = 1;
		return 0;
	}

	return -1;
}

/*!
 * \brief  ��־�������
 * \param  level  ��־�ȼ�, \sa LOG_LEVEL
 * \param  file   ������������ļ�
 * \param  line   ������������к�
 * \return void
 */
void log_print(int level, char *file, int line, const char * fmt, ...)
{
	va_list args;
	char buff[LOG_BUFF_SIZE];
	struct timeb tp;
	struct tm *tm;
	int fd, len, head_len = 0;
	char *pmsg = buff;

	if (flag_log_file_valid == 0) {
		return;
	}

	if ((fd = open((char *)app_log_file, O_RDWR | O_CREAT | O_APPEND, 00644)) < 0) {
		fprintf(stderr, "open log file %s failed, err:%s\n",
					app_log_file, strerror(errno));
		printf("open log file %s failed, err:%s\n",
					app_log_file, strerror(errno));
		return;
	}

	// -- ����ʱ���
	ftime(&tp);
	tm = localtime(&(tp.time));

	sprintf(pmsg, "%02d-%02d %02d:%02d:%02d.%03d ",
			(tm->tm_mon + 1), (tm->tm_mday), (tm->tm_hour),
			(tm->tm_min), (tm->tm_sec), (tp.millitm));

	// -- LOG_LEVEL��Ϣ�����ڹ�����־
	switch (level) {
	case LOG_LEVEL_ERROR:
		sprintf(pmsg + strlen(pmsg), "%3s ", "(E)");
		break;
	case LOG_LEVEL_WARN:
		sprintf(pmsg + strlen(pmsg), "%3s ", "(W)");
		break;
	case LOG_LEVEL_INFO:
		sprintf(pmsg + strlen(pmsg), "%3s ", "(I)");
		break;
	case LOG_LEVEL_DEBUG:
		sprintf(pmsg + strlen(pmsg), "%3s ", "(D)");
		break;
	default:
		break;
	}

	// -- �ļ���:�к�
	len = strlen(file);
	/*
	 * �̶� __FILE__:__LINE__ ���ֳ���;
	 * __FILE__ ���������8���ַ�
	 */
	sprintf(pmsg + strlen(pmsg), "%s:%d", (file + (len > 8 ? (len - 8) : 0)), line);
	pmsg[LOG_HEAD_LEN] = '\0';
	head_len = strlen(pmsg);
	memset(pmsg + head_len, ' ', LOG_HEAD_LEN - head_len);
//	sprintf(pmsg + strlen(pmsg), "%-5d ", line);

	// -- ���̺�
	sprintf(pmsg + strlen(pmsg), "[%-5d] ", getpid());

	// -- ��Ϣ����
	head_len = strlen(pmsg);
	va_start(args, fmt);
	len = vsnprintf(pmsg + head_len, sizeof(buff) - head_len - 1, fmt, args);
	va_end(args);

	sprintf(pmsg + strlen(pmsg), "\n");

	write(fd, pmsg, strlen(pmsg));
	close(fd);
//	printf("%s", pmsg);

	return;
}

unsigned char* Asc2Hex(const unsigned char *cmd, int cmdlen)
{
	int i;
	static char retcmd[MAX_ORDER_LEN*2+1];

	memset( retcmd, 0, sizeof(retcmd) );
	for (i = 0; i < cmdlen; i++) {
		sprintf(retcmd + i * 2, "%02x", cmd[i]);
	}

	return retcmd;
}

static int static_getlocaltime(char *my_bcd)
{
	char tmp[4];
	time_t result;
	struct tm *my_time;

	time(&result);
//	my_time = gmtime(&result);
	my_time = localtime(&result);
	my_time->tm_year += 1900;
	my_time->tm_mon += 1;

	my_bcd[0] = my_time->tm_year /1000;		//year
	my_time->tm_year -= my_bcd[0]*1000;
	my_bcd[1] = my_time->tm_year /100;
	my_time->tm_year -= my_bcd[1] *100;
	my_bcd[2] = my_time->tm_year /10;
	my_time->tm_year -= my_bcd[2] *10;
	my_bcd[3] = my_time->tm_year %10;
	my_bcd[0] = my_bcd[0] << 4;
	my_bcd[0] = my_bcd[0] | my_bcd[1];
	my_bcd[2] = my_bcd[2] << 4;
	my_bcd[1] = my_bcd[2] | my_bcd[3];

	tmp[0] = my_time->tm_mon / 10;				//month
	tmp[1] = my_time->tm_mon % 10;
	tmp[0] = tmp[0] <<4;
	tmp[3] = tmp[0] | tmp[1];
	my_bcd[2] = tmp[3];

	tmp[0] = my_time->tm_mday / 10;				//day
	tmp[1] = my_time->tm_mday % 10;
	tmp[0] = tmp[0] <<4;
	tmp[3] = tmp[0] | tmp[1];
	my_bcd[3] = tmp[3];

	tmp[0] = my_time->tm_hour / 10;				//hour
	tmp[1] = my_time->tm_hour % 10;
	tmp[0] = tmp[0] <<4;
	tmp[3] = tmp[0] | tmp[1];
	my_bcd[4] = tmp[3];

	tmp[0] = my_time->tm_min / 10;				//minute
	tmp[1] = my_time->tm_min % 10;
	tmp[0] = tmp[0] <<4;
	tmp[3] = tmp[0] | tmp[1];
	my_bcd[5] = tmp[3];

	tmp[0] = my_time->tm_sec / 10;				//second
	tmp[1] = my_time->tm_sec % 10;
	tmp[0] = tmp[0] <<4;
	tmp[3] = tmp[0] | tmp[1];
	my_bcd[6] = tmp[3];
//printf("my year is : %x %x %x %x %x %x %x\n",my_bcd[0],my_bcd[1],my_bcd[2],my_bcd[3],my_bcd[4],my_bcd[5],my_bcd[6]);

	return 0;

}
/*
 *  �滻��־�ļ�-��ԭ�ļ�������/home/Data/log_bak/ ��
 *  �ɹ�����0, ʧ�ܷ��� -1
 */
int backup_log_file()
{
	int fd;
	int _fd;
	int ret;
	struct stat f_stat;
	unsigned char cmd[256];
	unsigned char date_time[7] = {0};


	if((fd = open(SINOPEC_LOG, O_RDONLY, 00644)) < 0) {
		return -1;
	}

	if (fstat(fd, &f_stat) != 0) {
		return -1;
	}
	
	/*
	 * st_size ��С����̫��, Ҫ���ǵ������ݵĹ���ʱ��, �Լ��������ݴ��������ʱ��
	 */
	RunLog("-----------------------st_size: %d -------------", f_stat.st_size);
	if ((long)f_stat.st_size > _528K) {       
		static_getlocaltime(date_time);

		RunLog("run.log ��С���� 528KB ת����־");

		system("[ ! -d /home/Data/log_backup ] && mkdir  /home/Data/log_backup");

		sprintf(cmd, "mv %s /home/Data/log_backup/sin_log[%02x%02x%02x%02x%02x%02x%02x].bak",
					SINOPEC_LOG, date_time[0], date_time[1], date_time[2],
					date_time[3], date_time[4], date_time[5], date_time[6]);
		system(cmd);

#if 0
		sprintf(cmd, "/home/Data/log_backup/run_log[%02x%02x%02x%02x%02x%02x%02x].bak",
					date_time[0], date_time[1], date_time[2],
					date_time[3], date_time[4], date_time[5], date_time[6]);
		ret = rename(run_log, cmd);
		if (ret < 0) {
			sleep(5);
			ret = rename(run_log, cmd);
			if (ret < 0) {
				RunLog("dump run.log failed, errno[%s]", strerror(errno));
			}
		}
#endif

#if 0
		// ԭʼ����ʽ
		sprintf(cmd, "cp %s /home/Data/log_backup/run_log[%02x%02x%02x%02x%02x%02x%02x].bak",
					run_log, date_time[0], date_time[1], date_time[2],
					date_time[3], date_time[4], date_time[5], date_time[6]);
		system(cmd);

		truncate(run_log, 0); // �˲������ܻᵼ�²�����־��ʧ
#endif
	} else {
		close(fd);
		return 0;
	}
	
	close(fd);

/*	if((fd = open(SINOPEC_LOG, O_CREAT , 00644)) < 0) {
		return -1;
	}

	close(fd);*/

	return 0;
}


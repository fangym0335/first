/*!
 * \file   log.h
 * \brief  日志处理相关定义
 */

#ifndef LOG_H
#define LOG_H   

#include <string.h>
#include "log4c.h"

#define WITHOUT_LOG4C                1                    //!< 不使用log4c

#define OPTLOG_CATEGORY_NAME         "opt"                //!< for log4c
#define FILE_LINE_LEN                22                   //!< __FILE__:__LINE__最大允许长度
#define DEBUG_LOG                    0                    //!< 调试日志格式标记

#define OPT_LOG(priority, args...)   opt_msg(__FILE__, __LINE__, OPTLOG_CATEGORY_NAME, priority, ##args)

#define  LOG_BUFF_SIZE               2048                 //!< 默认日志信息缓冲区


#ifndef WITHOUT_LOG4C
#define OPTLOG_PRIORITY_ERROR        LOG4C_PRIORITY_ERROR
#define OPTLOG_PRIORITY_WARN         LOG4C_PRIORITY_WARN
#define OPTLOG_PRIORITY_INFO         LOG4C_PRIORITY_INFO
#define OPTLOG_PRIORITY_DEBUG        LOG4C_PRIORITY_DEBUG
#define OPTLOG_PRIORITY_TRACE        LOG4C_PRIORITY_TRACE
#else
#define OPTLOG_PRIORITY_ERROR        1
#define OPTLOG_PRIORITY_WARN         2
#define OPTLOG_PRIORITY_INFO         3
#define OPTLOG_PRIORITY_DEBUG        4
#define OPTLOG_PRIORITY_TRACE        5

/*!
 * \brief 日志输出等级定义
 */
enum LOG_LEVEL {
	LOG_LEVEL_ERROR = 0,
	LOG_LEVEL_WARN,
	LOG_LEVEL_INFO,
	LOG_LEVEL_DEBUG
};
#endif

int  l_log_init();
void log_set_level(int level);
int log_set_logfile(char *log_file);
int inline log_get_level();
void log_print(int level, char *file, int line, const char * fmt, ...);
unsigned char* Asc2Hex(const unsigned char *cmd, int cmdlen);


#ifndef WITHOUT_LOG4C

#define LOG_ERROR(fmt, args...)     OPT_LOG(OPTLOG_PRIORITY_ERROR, fmt, ##args)
#define LOG_WARN(fmt, args...)      OPT_LOG(OPTLOG_PRIORITY_WARN,  fmt, ##args)
#define LOG_INFO(fmt, args...)      OPT_LOG(OPTLOG_PRIORITY_INFO,  fmt, ##args)
#define LOG_DEBUG(fmt, args...)     OPT_LOG(OPTLOG_PRIORITY_ERROR, fmt, ##args)

#define RunLog(fmt, args...)     OPT_LOG(OPTLOG_PRIORITY_ERROR, fmt, ##args)
#define UserLog(fmt, args...)     OPT_LOG(OPTLOG_PRIORITY_ERROR, fmt, ##args)

#else

#define RunLog(fmt, args...)     log_print(LOG_LEVEL_ERROR, __FILE__, __LINE__, fmt, ##args)
#define UserLog(fmt, args...)     log_print(LOG_LEVEL_ERROR, __FILE__, __LINE__, fmt, ##args)

#define LOG_ERROR(fmt, args...)                                                \
	do {                                                                   \
		if (LOG_LEVEL_ERROR <= log_get_level()) {                      \
			log_print(LOG_LEVEL_ERROR, __FILE__, __LINE__, fmt, ##args); \
		}                                                              \
	} while (0);

#define LOG_WARN(fmt, args...)                                                \
	do {                                                                  \
		if (LOG_LEVEL_WARN <= log_get_level()) {                      \
			log_print(LOG_LEVEL_WARN, __FILE__, __LINE__, fmt, ##args); \
		}                                                             \
	} while (0);

#define LOG_INFO(fmt, args...)\
	do {                                                                  \
		if (LOG_LEVEL_INFO <= log_get_level()) {                      \
			log_print(LOG_LEVEL_INFO, __FILE__, __LINE__, fmt, ##args); \
		}                                                             \
	} while (0);

#define LOG_DEBUG(fmt, args...)                                               \
	do {                                                                  \
		if (LOG_LEVEL_DEBUG <= log_get_level()) {                     \
			log_print(LOG_LEVEL_DEBUG, __FILE__, __LINE__, fmt, ##args);\
		}                                                             \
	} while (0);

#endif




int init_opt_formatters();


/*!
 * \brief opt日志处理-构造函数(初始化),通过WITHOUT_LOG4C切换使用LOG4C或普通日志输出
 */
static int log_init(char *log_file)
{
#ifndef WITHOUT_LOG4C
	if (init_opt_formatters() != 0) {
		return -2;
	}

	return (log4c_init());
#else
	return l_log_init(log_file);
#endif
}

/*!
 * \brief opt日志处理-析构函数(释放资源),通过WITHOUT_LOG4C切换使用LOG4C或普通日志输出
 */
static int log_fini()
{
#ifndef WITHOUT_LOG4C
	return (log4c_fini());
#else
	return 0;
#endif
}

/*!
 * \brief opt日志处理-输出函数,通过WITHOUT_LOG4C切换使用LOG4C或普通日志输出
 */
static void opt_msg(char *file, int line, char *catName,
				    int a_priority, const char * a_format, ...)
{
	static char new_format[256];
	const char *pfmt = NULL;
	int fd;

	if (access(file, F_OK) < 0) {
		if((fd = open(file, O_CREAT , 00644)) < 0) {
			return;
		}

		close(fd);
	}


#ifdef DEBUG_LOG
	int len = strlen(file);
	sprintf(new_format, "%6s:%d", file + (len > 8 ? (len - 8) : 0), line);
#else
	sprintf(new_format, "%6s:%d", file, line);
#endif

	if (strlen(a_format) + FILE_LINE_LEN < sizeof(new_format)) {
#ifdef DEBUG_LOG
		sprintf(new_format, "%-14s %s", new_format, a_format);
#else
		sprintf(new_format, "%-21s %s", new_format, a_format);
#endif
		pfmt = new_format;
	} else {
		pfmt = a_format;
	}

#ifndef WITHOUT_LOG4C
	const log4c_category_t * a_category = log4c_category_get(catName);
	if (a_category == NULL) {
		fprintf(stdout, "category is  NULL\n");
	}
	if (log4c_category_is_priority_enabled(a_category, a_priority)) {
		va_list va;
		va_start(va, a_format);
		log4c_category_vlog(a_category, a_priority, pfmt, va);
		va_end(va);
	}
#else
	va_list va;
	va_start(va, a_format);
	vprintf(pfmt, va);
	va_end(va);

#endif
}


#endif /* LOG_H */






/*
as


df



asd



asdff

*/

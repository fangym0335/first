/*******************************
	*CopyRight Cuihq
	*All Right Reserved
	*
	*文件名称: dotime.h
	*摘要:
	*
	*作者: 崔洪清
	*日期: 2004/06/01
*******************************/
#ifndef __DOTIME_H__
#define __DOTIME_H__

#include <time.h>
#include <sys/time.h>
#include "mycomm.h"

#define DATE_LEN		8
#define TIME_LEN		6
#define DATETIME_LEN	14
#define MICROSIZE	sizeof(struct timeval)

#ifndef __GBEGINTIME_SRC__
extern char gBeginTime[MICROSIZE];
#endif /*__GBEGINTIME_SRC__*/

#ifndef __GENDTIME_SRC__
extern char gEndTime[MICROSIZE];
#endif /*__GENDTIME_SRC__*/

/*取系统时间,格式为YYYYMMDD.buff长度至少为9,程序不做检查.*/
/*如果timebuff为NULL，可以通过返回值取得日期*/
char * GetSysDate( char *datebuff );

/*取系统时间,格式为YYYYMMDDHH24MISS.buff长度至少为15,程序不做检查.*/
/*如果timebuff为NULL，可以通过返回值取得时间*/
char* GetSysDateTime(char *timebuff);

/*分别取系统日期和时间,日期格式为YYYYMMDD,时间格式为HHMISS.*/
/*当某个参数为NULL时，该值不返回.curr_date长度需要至少为9,curr_time长度至少为7,程序不做检查*/
void GetSysDateAndTime(char *curr_date,char *curr_time);

/*取到微秒级的系统时间,timebuf 长度需大于36*/
void GetMicroTime( char * timebuf );

/*显示从begin到end经历的时间，begin,end 必须由GetMicroTime得到*/
ulong GetDiffTime( const char * begin, const char *end );

/*显示从begin到现在经历的时间，begin必须由GetMicroTime得到*/
ulong GetPassTime( const char *begin );

/*按指定字符串方式取系统时间，format格式中%Y 表示年 %m 表示月 %d表示日 %H 表示小时 %M表示分 %S表示秒*/
void GetFormatTime( char *timebuf, size_t bufSize, const char * format );

/*取明天的日期.nday长度至少为9*/
int GetNextDay(char *nday);

/*取指定日期之后affter天的日期，nday为目标日期,长度至少为9.day为指定日期,格式为YYYYMMDD.after为负数时表示取之前的日期*/
int GetDay(char *nday, const char *day, int after );

/*取本月第一天和最后一天.两个参数长度都至少为9*/
int GetFirstAndLastOfNowMonth( char *firstDay, char *lastDay );

/*取指定日期的当月的第一天和最后一天*/
int GetFirstAndLastOfMonth( char *firstDay, char *lastDay, const char *day );


/*以下定义一些宏,可以用来显示程序运行的微妙级的时间*/

/*宏BEGIN_TIME 取微妙级系统时间到全局变量gBeginTime中*/
#define BeginTime()	{ memset(gBeginTime,0,sizeof(gBeginTime)); GetMicroTime(gBeginTime); }

/*宏BEGIN_TIME 取微妙级系统时间到全局变量gEndTime中*/
#define EndTime() { memset(gEndTime,0,sizeof(gEndTime)); GetMicroTime(gEndTime); }

/*宏PASS_TIME 显示自上次调用BEGIN_TIME经历的微妙级时间*/
#define ShowPassTime() { DMsg( "time = [%ld us]", GetPassTime( gBeginTime ) ); }

/*宏DIFF_TIME显示最近一次调用BEGIN_TIME和最近一次调用END_TIME经历的时间*/
#define ShowDiffTime() { DMsg( "time = [%ld us]", GetDiffTime( gBeginTime, gEndTime ) ); }

/*宏PASS_TIME显示从调用BEGIN_TIME以后经历的时间*/
#define PassTime() GetPassTime( gBeginTime )

/*宏DIFF_TIME显示调用BEGIN_TIME和END_TIME之间经过的时间*/
#define DiffTime() GetDiffTime( gBeginTime, gEndTime )


#endif

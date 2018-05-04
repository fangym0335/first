/*******************************
	*CopyRight Cuihq
	*All Right Reserved
	*
	*�ļ�����: dotime.h
	*ժҪ:
	*
	*����: �޺���
	*����: 2004/06/01
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

/*ȡϵͳʱ��,��ʽΪYYYYMMDD.buff��������Ϊ9,���������.*/
/*���timebuffΪNULL������ͨ������ֵȡ������*/
char * GetSysDate( char *datebuff );

/*ȡϵͳʱ��,��ʽΪYYYYMMDDHH24MISS.buff��������Ϊ15,���������.*/
/*���timebuffΪNULL������ͨ������ֵȡ��ʱ��*/
char* GetSysDateTime(char *timebuff);

/*�ֱ�ȡϵͳ���ں�ʱ��,���ڸ�ʽΪYYYYMMDD,ʱ���ʽΪHHMISS.*/
/*��ĳ������ΪNULLʱ����ֵ������.curr_date������Ҫ����Ϊ9,curr_time��������Ϊ7,���������*/
void GetSysDateAndTime(char *curr_date,char *curr_time);

/*ȡ��΢�뼶��ϵͳʱ��,timebuf ���������36*/
void GetMicroTime( char * timebuf );

/*��ʾ��begin��end������ʱ�䣬begin,end ������GetMicroTime�õ�*/
ulong GetDiffTime( const char * begin, const char *end );

/*��ʾ��begin�����ھ�����ʱ�䣬begin������GetMicroTime�õ�*/
ulong GetPassTime( const char *begin );

/*��ָ���ַ�����ʽȡϵͳʱ�䣬format��ʽ��%Y ��ʾ�� %m ��ʾ�� %d��ʾ�� %H ��ʾСʱ %M��ʾ�� %S��ʾ��*/
void GetFormatTime( char *timebuf, size_t bufSize, const char * format );

/*ȡ���������.nday��������Ϊ9*/
int GetNextDay(char *nday);

/*ȡָ������֮��affter������ڣ�ndayΪĿ������,��������Ϊ9.dayΪָ������,��ʽΪYYYYMMDD.afterΪ����ʱ��ʾȡ֮ǰ������*/
int GetDay(char *nday, const char *day, int after );

/*ȡ���µ�һ������һ��.�����������ȶ�����Ϊ9*/
int GetFirstAndLastOfNowMonth( char *firstDay, char *lastDay );

/*ȡָ�����ڵĵ��µĵ�һ������һ��*/
int GetFirstAndLastOfMonth( char *firstDay, char *lastDay, const char *day );


/*���¶���һЩ��,����������ʾ�������е�΢���ʱ��*/

/*��BEGIN_TIME ȡ΢�ϵͳʱ�䵽ȫ�ֱ���gBeginTime��*/
#define BeginTime()	{ memset(gBeginTime,0,sizeof(gBeginTime)); GetMicroTime(gBeginTime); }

/*��BEGIN_TIME ȡ΢�ϵͳʱ�䵽ȫ�ֱ���gEndTime��*/
#define EndTime() { memset(gEndTime,0,sizeof(gEndTime)); GetMicroTime(gEndTime); }

/*��PASS_TIME ��ʾ���ϴε���BEGIN_TIME������΢�ʱ��*/
#define ShowPassTime() { DMsg( "time = [%ld us]", GetPassTime( gBeginTime ) ); }

/*��DIFF_TIME��ʾ���һ�ε���BEGIN_TIME�����һ�ε���END_TIME������ʱ��*/
#define ShowDiffTime() { DMsg( "time = [%ld us]", GetDiffTime( gBeginTime, gEndTime ) ); }

/*��PASS_TIME��ʾ�ӵ���BEGIN_TIME�Ժ�����ʱ��*/
#define PassTime() GetPassTime( gBeginTime )

/*��DIFF_TIME��ʾ����BEGIN_TIME��END_TIME֮�侭����ʱ��*/
#define DiffTime() GetDiffTime( gBeginTime, gEndTime )


#endif

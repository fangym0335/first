/**************************************************************************
*		mytime.c   //change by chenfm 2007.07.27: add mytime.h,change file name time.c to mytime.c
*		The functions include getlocaltime sync_time 
*		2006.04 build by Jiangbo Yin <yinjiangbo@hotmail.com>
*												2006 CS&S Intelligent Corp.
**************************************************************************/
#include "pub.h"
#include <time.h>
#include "mytime.h"

time_t result;
struct tm *my_time;

int getlocaltime(char *my_bcd)
{
	char tmp[4];

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

int sync_time(char *cmd)
{	int ret;
	char tmp[4];
	
//	time(&result);
//	my_time = gmtime(&result);
//	my_time->tm_year += 1900;
//	my_time->tm_mon += 1;

	tmp[0] = cmd[2] >> 4;				// get the year
	tmp[1] = cmd[2] & 0x0f;
	tmp[2] = cmd[3] >> 4;
	tmp[3] = cmd[3] &0x0f;
	my_time->tm_year = (tmp[0] * 1000 + tmp[1] * 100 + tmp[2] * 10 + tmp[3]) - 1900;

	tmp[0] = cmd[4] >>4;				//get month
	tmp[1] = cmd[4] & 0x0f;
	my_time->tm_mon = (tmp[0] * 10 + tmp[1]) - 1;

	tmp[0] = cmd[5] >> 4;				//day
	tmp[1] = cmd[5] & 0x0f;
	my_time->tm_mday =  tmp[0] * 10 + tmp[1];

	tmp[0] = cmd[6] >> 4;				//hour
	tmp[1] = cmd[6] & 0x0f;
	my_time->tm_hour =  tmp[0] * 10 + tmp[1];

	tmp[0] = cmd[7] >> 4;				//minute
	tmp[1] = cmd[7] & 0x0f;
	my_time->tm_min =  tmp[0] * 10 + tmp[1];

	tmp[0] = cmd[8] >> 4;				//second
	tmp[1] = cmd[8] & 0x0f;
	my_time->tm_sec =  tmp[0] * 10 + tmp[1];
	
	result = mktime(my_time);		//conver tm to time_t
	if(stime(&result) < 0) RunLog("sync time error");	//set system time
	else{
		system("/sbin/hwclock --systohc");		//write time to RTC
	}

	return 0;
	
}


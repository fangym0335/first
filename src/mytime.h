/**************************************************************************
*		mytime.h
*		The functions include getlocaltime sync_time 
*		2006.04 build by Jiangbo Yin <yinjiangbo@hotmail.com>
*												2006 CS&S Intelligent Corp.
**************************************************************************/
#ifndef __MYTIME_H__
#define __MYTIME_H__

int getlocaltime(char *my_bcd);

int sync_time(char *cmd);

#endif



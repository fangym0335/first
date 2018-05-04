#ifndef __NAMVAL_H__
#define __NAMVAL_H__

#include <stdio.h>
#include <string.h>

#define MAX_NAMVAL_LEN		128					/*name或者value字段的最大长度*/


int GetValue( const char *sCommArea, const char *sKey, char *sValue, size_t iMaxSize );
int SetValue( char *sCommArea, size_t iMaxSize, const char *sKey, const char *sValue );
int DelKey( char *sCommArea, const char *sKey );

#endif

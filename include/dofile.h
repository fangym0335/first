/*******************************
	*CopyRight Cuihq
	*All Right Reserved
	*
	*文件名称: dofile.h
	*摘要:
	*
	*作者: 崔洪清
	*日期: 2004/06/01
*******************************/

#ifndef __DOFILE_H__
#define __DOFILE_H__

#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h> /*For Sco*/
#include <sys/stat.h> /*For Sco*/
#include "mycomm.h"
#include "dostring.h"
#include "mystring.h"

#define LINESIZE		80

/*以下两个宏定义用来区别调用ReadLineFromFile_1时是否前面的递归读到续行符.*/
#define NO_CONTACT		0	/*未读到*/
#define CONTACT			1	/*读到*/


/*从文件中取参数值 文件格式为
	NAME = VALUE
paraname		参数名
para 	 		保存参数值的变量
defaultvalue 	缺省值
mode		 	是否可以缺省 VALID 缺省  INVALID 不缺省
return			-1 文件不存在
				-2 文件行大于LINESIZE定义的长度
*/
int GetParaFromCfg( const char *filename, const char *paraname, char *para, const char *defaultvalue, int mode );

/*向文件filename中写一条记录.
  本函数不会在最后加上新行符*/
int WriteFile( const char *filename, const char *fmt,... );
/*根据文件名取文件大小 -1失败*/
long GetFileSizeByName( char* sFileName );
/*根据打开的文件指针取文件大小 -1失败*/
long GetFileSizeByPoint( FILE * fpStream );
/*判断文件是否存在,-1 失败. 0不存在, 1存在*/
int IsFileExist( char * sFileName );
int IsDir( char *sFileName );

/*根据打开的文件指针从文件中读一行 -1失败*/
int ReadLineFromFile( FILE *fp, String *pstStr );


#endif

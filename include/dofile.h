/*******************************
	*CopyRight Cuihq
	*All Right Reserved
	*
	*�ļ�����: dofile.h
	*ժҪ:
	*
	*����: �޺���
	*����: 2004/06/01
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

/*���������궨�������������ReadLineFromFile_1ʱ�Ƿ�ǰ��ĵݹ�������з�.*/
#define NO_CONTACT		0	/*δ����*/
#define CONTACT			1	/*����*/


/*���ļ���ȡ����ֵ �ļ���ʽΪ
	NAME = VALUE
paraname		������
para 	 		�������ֵ�ı���
defaultvalue 	ȱʡֵ
mode		 	�Ƿ����ȱʡ VALID ȱʡ  INVALID ��ȱʡ
return			-1 �ļ�������
				-2 �ļ��д���LINESIZE����ĳ���
*/
int GetParaFromCfg( const char *filename, const char *paraname, char *para, const char *defaultvalue, int mode );

/*���ļ�filename��дһ����¼.
  ���������������������з�*/
int WriteFile( const char *filename, const char *fmt,... );
/*�����ļ���ȡ�ļ���С -1ʧ��*/
long GetFileSizeByName( char* sFileName );
/*���ݴ򿪵��ļ�ָ��ȡ�ļ���С -1ʧ��*/
long GetFileSizeByPoint( FILE * fpStream );
/*�ж��ļ��Ƿ����,-1 ʧ��. 0������, 1����*/
int IsFileExist( char * sFileName );
int IsDir( char *sFileName );

/*���ݴ򿪵��ļ�ָ����ļ��ж�һ�� -1ʧ��*/
int ReadLineFromFile( FILE *fp, String *pstStr );


#endif

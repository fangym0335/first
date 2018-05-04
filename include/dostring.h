/*******************************
	*CopyRight Cuihq
	*All Right Reserved
	*
	*�ļ�����: dostring.h
	*ժҪ:
	*
	*����: �޺���
	*����: 2004/06/01
*******************************/
#ifndef __DOSTRING_H__
#define __DOSTRING_H__

#include <string.h>
#include <stdarg.h>
#include "mycomm.h"


#define FILLBEHIND		0		/*���ַ���β�����*/
#define FILLAHEAD		1		/*���ַ���ͷ�����*/

/*�ж��ַ����Ƿ�Ϊ��*/
int IsEmptyString( const char *s );

/*ȥ���ַ�������ո�.��Ҫ�ַ�����������*/
char *AllTrim( char *sBuf );

/*ȥ���ַ����Ҳ�ո�,��Ҫ�ַ�����������*/
char *RightTrim( char *buf );

/*ȥ���ַ������ո�.��Ҫ�ַ�����������*/
char *LeftTrim( char *buf );

/*���ַ�����ΪСд,��Ҫ�ַ�����������*/
char *StrToLower( char *s );

/*���ַ�����Ϊ��д,��Ҫ�ַ�����������*/
char *StrToUpper( char *s );

/*���ַ�����ΪСд,��Ҫ�ַ�����������*/
char *ToLower( const char *src, char *obj );

/*���ַ�����Ϊ��д,��Ҫ�ַ�����������*/
char *ToUpper( const char *src, char *obj );

/***************************************************

	�ú�����ָ�����ַ�����ַ�����ָ���ĳ���
	buff:	�ַ���
	length:	��䵽�ĳ���
	fillChr:�����ַ�
	flag:	��ֵΪFILLBEHINDʱ,���ַ�����β�����
			��ֵΪFILLAHEADʱ,���ַ�����ͷ�����

	��ע:
		��ѡ�����ַ���ǰ�����ʱ,��Ϊ������ʱ��,�����п����ڴ����.
		�ڴ����ʱ����-1
***************************************************/

int FillBuff( char *buff, size_t length, char fillChr, myflag flag );

/***************************************************

	�ú����ж��ַ��������Ƿ���ĳ�����ֵ�������,
	�������,�������ַ���

	buff:	�ַ���
	mod:	ģ.������ַ���Ϊ��ģ��������.
	fillChr:�����ַ�
	flag:	��ֵΪFILLBEHINDʱ,���ַ�����β�����
			��ֵΪFILLAHEADʱ,���ַ�����ͷ�����
	��ע:
	1.	��ѡ�����ַ���ǰ�����ʱ,��Ϊ������ʱ��,�����п����ڴ����.
		�ڴ����ʱ����-1
	2.	������㹻���buff,����������Ĵ�
***************************************************/

int FillBuffWithMod( char *buff, uint mod, char fillChr, myflag flag );

/*����ɱ����*/
/*�ú����õ��˾�̬����,�����̰߳�ȫ.�Խ�����˵,��Ҫ���´ε��øú���ǰ�����ϴε��ý��*/

char *DoValist( const char *fmt, va_list pvar );

#endif

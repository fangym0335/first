#include "memctl.h"

main()
{
	char *p1,*p2,*p3,*p4,*p5,*p6,*p7;

	InitMemCtl();

	p1 = (char*)Malloc(100);
	printf("Addr = [%p], size = [%ld]\n", p1, 100L );

	p2 = (char*)Malloc(101);
	printf("Addr = [%p], size = [%ld]\n", p2, 101L );

	p3 = (char*)Malloc(102);
	printf("Addr = [%p], size = [%ld]\n", p3, 102L );

	p4 = (char*)malloc(1000);	/*��malloc����,�����ڴ���Ƴ������*/

	p5 = (char*)Malloc(104);
	printf("Addr = [%p], size = [%ld]\n", p5, 104L );

	free(p4);	/*��free�ͷ�,�����ڴ���Ƴ������*/

	p6 = (char*)Malloc(105);
	printf("Addr = [%p], size = [%ld]\n", p6, 105L );

	p7 = (char*)Malloc(106);
	printf("Addr = [%p], size = [%ld]\n", p7, 106L );

	ShowMemDetail();

	Trace(p5 = (char*)Realloc(p5,88));
	ShowMemDetail();

	Trace(p5 = (char*)Realloc(p5,1000));
	ShowMemDetail();

	Trace(Free(p5));
	ShowMemDetail();

	Trace( p5 = (char*)Calloc(10,8));
	ShowMemDetail();

	Trace(Free(p1));
	ShowMemDetail();

	Trace(Free(p2));
	ShowMemDetail();

	Trace(Free(p3));
	ShowMemDetail();


	Trace(Free(p6));
	ShowMemDetail();

#if 0
	Trace(Free(p4));	/*p4û�ڿ����б���,�˴������ʧ��*/
	ShowMemDetail();

#endif
	/*p7��p5����δ�ͷ�,�˳�ʱ�ᱻ��⵽*/

	return 0;
}


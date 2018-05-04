#include "mycomm.h"
#include "tochin.h"

main()
{
	double f;
	char a[100];

	printf("输入一个数字\n");
	scanf("%lf",&f);
	to_chin(f,a);
	printf("f=[%lf],a=[%s]\n",f,a);
	return 0;
}



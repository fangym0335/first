#include <stdio.h>
#include "mycomm.h"

main()
{
	char bin[100];
	char hex[100];
	long dce;

	printf("input hex string:\n");
	scanf("%s",hex);

	printf("Hex is [%s]\n", hex );

	Trace(HexToBin(hex,bin));
	printf("Bin is [%s]. len = [%d]\n", bin, strlen(bin) );

	Trace(BinToHex(bin,hex));
	printf("Hex is [%s]\n", hex );

	Trace(BinToDec(bin,&dce));
	printf("Dce is [%ld]\n", dce );

	Trace(DecToHex(dce,hex));
	printf("Hex is [%s]\n", hex );

	Trace(DecToBin(dce,bin));
	printf("Bin is [%s]. len = [%d]\n", bin, strlen(bin) );

	Trace(HexToDec(hex,&dce));
	printf("Dce is [%ld]\n", dce );





 	return 0;
}


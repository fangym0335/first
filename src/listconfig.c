/*
 * =====================================================================================
 *
 *       Filename:  listconfig.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  03/10/10 18:10:46
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Guojie Zhu (mn), <zhugj@css-intelligent.com>
 *        Company:  CSSI
 *
 * =====================================================================================
 */

#include "pub.h"
#include "ifsf_pub.h"
#include "init.h"

int main(int argc, char **argv)
{
	int option = 0;
	int ret, cnt, idx;
	FILE *fp = NULL;
	struct stat fstat;
	off_t eocfg = 0;

	unsigned char oilcfg[sizeof(OIL_CONFIG)];
	unsigned char guncfg[sizeof(GUN_CONFIG)];
	OIL_CONFIG *poil = (OIL_CONFIG *)&oilcfg;
	GUN_CONFIG *pgun = (GUN_CONFIG *)&guncfg;
	char * pump_verdors[25] = {
		"长吉", "恒山", "太空", "榕兴", "正星",       \
		"宝德尔", "稳牌", "恒山自助", "豪升",         \
		"老长空", "新长空", "三盈", "鸿洋", "中意",   \
		"佳力佳", "模拟油机", "蓝峰", "长空卡机联动", \
		"恒山TMC(TH30)", "预留1", "预留2","预留3"
	};
	



	if (argc == 2) {
	       	if (strcmp(argv[1], "-g") == 0) {
			option = 1;
		} else if (strcmp(argv[1], "-p") == 0) {
			option = 2;
		} else if (strcmp(argv[1], "-a") == 0) {
			option = 3;
		}
	}

	if (option == 0) {
		if (argc != 2)  {
			printf("listconfig v1.0\n\n");
			printf("Usage: listconfig [-gpa]    \n\n");
			printf("Display grade or pump configurations by ascii format\n");
			printf("Options: \n");
			printf("        -g	list grade configs\n");
			printf("        -p	list pump configs\n");
			printf("        -a	list all configs (grade & pump)\n\n");
			return 0;
		}
	}

	switch (option) {
	case 1:
	case 3:
		fp = fopen("/home/App/ifsf/etc/oil.cfg", "r");
		if (fp == NULL) {
			printf("open config file /home/App/ifsf/etc/oil.cfg failed\n");
			return -1;
		}

		ret = lstat("/home/App/ifsf/etc/oil.cfg", &fstat);
		if (ret < 0) {
			printf("lstat file oil.cfg failed\n");
			goto FAIL_EXIT;
		}

		cnt = fstat.st_size / sizeof(OIL_CONFIG);
		idx = 0;
		//printf("st_size: %ld, cnt: %d\n", fstat.st_size, cnt);
		while (cnt != 0) {
			cnt--;

			ret = fread(&oilcfg, sizeof(OIL_CONFIG), 1, fp);
			if (ret != 1) {
				printf("oil.cfg error!\n");
				goto FAIL_EXIT;
			}

			if (idx == 0) {
				printf(" ID  Prod_Nb    Prod_Name\n");
			}
			printf(" %02d  %02x%02x%02x%02x   %s\n", ++idx, poil->oilno[0], 
				poil->oilno[1], poil->oilno[2], poil->oilno[3], poil->oilname);
		}

		fclose(fp);
		if (option == 1) {
			break;
		}
		printf("\n");
	case 2:
		fp = fopen("/home/App/ifsf/etc/dsp.cfg", "r");
		if (fp == NULL) {
			printf("open config file /home/App/ifsf/etc/dsp.cfg failed\n");
			return -1;
		}

		ret = lstat("/home/App/ifsf/etc/dsp.cfg", &fstat);
		if (ret < 0) {
			printf("lstat file oil.cfg failed\n");
			goto FAIL_EXIT;
		}

		cnt = fstat.st_size / sizeof(GUN_CONFIG);
		idx = 0;
		//printf("st_size: %ld, cnt: %d\n", fstat.st_size, cnt);
		while (cnt != 0) {
			cnt--;

			ret = fread(&guncfg, sizeof(GUN_CONFIG), 1, fp);
			if (ret != 1) {
				printf("dsp.cfg error!\n");
				goto FAIL_EXIT;
			}

			if (idx == 0) {
				printf(" ID  Node CHN      Vendor          PFP  LFP  LNZ  PNZ Prod_Nb  Enable\n");
			}
			printf(" %02d   %02d   %02d    %-15s   %02d   %02d   %02d   %02d  %02x%02x%02x%02x   %s\n",
					++idx, pgun->node, pgun->chnLog, pump_verdors[pgun->nodeDriver - 1],
				        pgun->phyfp, pgun->logfp, pgun->logical_gun_no, pgun->physical_gun_no, 
					pgun->oil_no[0], pgun->oil_no[1], pgun->oil_no[2], pgun->oil_no[3],
					(pgun->isused == 1 ? "YES" : "NO"));
		}

		fclose(fp);
	default:
		break;
	}


	return 0;

FAIL_EXIT:
	fclose(fp);
	return -1;
}


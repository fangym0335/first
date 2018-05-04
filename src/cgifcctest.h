#ifndef CGI_FCC_TEST_H__
#define CGI_FCC_TEST_H__

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <signal.h>
#include <sys/ipc.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <sys/sem.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>

//#include "log.h"
//#include "ifsf_pub.h"
//#include "ifsf_tlg.h"
#include "cgidecoder.h"

#define MAIN_RESULT_SIZE 8
#define TEST_RESULT_SIZE 14

typedef struct {
//	char Test_Result[13];
//	char Message[512];
	char BIG_BUFF[16384];
	char Main_Result[MAIN_RESULT_SIZE];
	char Test_Result[TEST_RESULT_SIZE];
}CGI_TEST_RESULT;

CGI_TEST_RESULT *TR;

NAME_VALUE cgilist[NV_PAIRS];

#define KEY_FILE "/home/App/ifsf/data/cgi-bin/cgifccmaintest"
//#define MAX_PATH_NAME_LENGTH 1024
#define KEY_INDEX 100

#define MAIN_WEB_PAGE_1 "MainWebPart1.txt"
#define MAIN_WEB_PAGE_2 "MainWebPart2.txt"
#define MAIN_WEB_PAGE_3 "MainWebPart3.txt"
#define MAIN_RESULT_FILE "/home/ftp/MainTestResult"


#define SELF_WEB_PAGE_1 "SelfWebPart1.txt"
#define SELF_WEB_PAGE_2 "SelfWebPart2.txt"
#define SELF_WEB_PAGE_3 "SelfWebPart3.txt"
#define SELF_RESULT_FILE "/home/ftp/SelfTestResult"

#define BOX_CHECKED "checked='checked'"

static int SHM_ID;

static key_t GetTheKey(int index);
static char* Cgi_ShmCreat(int index);
static char* Cgi_ShmAttach(int index);
static int DetachTheShm(char **p);
int getNameValue(FILE *file);
int WriteMainTResultFile(FILE *file);




static key_t GetTheKey(int index)
{
	key_t key;
	
	key = ftok(KEY_FILE, index);
	if (key == (key_t)(-1)){
		cgi_error(" key == (key_t)(-1)");
		exit(1);
	}
	return key;
}

static char* Cgi_ShmCreat(int index)
{
	//int shmid;
	char *p;

	SHM_ID = shmget( GetTheKey(index), sizeof(CGI_TEST_RESULT), IPC_CREAT  | 0666);
	if (SHM_ID < 0) {
		printf("cgi共享内存创建失败\n");
		exit(-1);
	}

	p = (char*)shmat(SHM_ID, NULL, 0);
	if (p == (char*)-1){
		return NULL;
	}
	else
		return p;
}

static char* Cgi_ShmAttach(int index)
{
	int shmid;
	char *p;

	shmid = shmget( GetTheKey(index), sizeof(CGI_TEST_RESULT), 0600);
	if ( shmid == -1){
		return NULL;
	}
	p = (char*) shmat(shmid, NULL, 0);
	if (p == (char*)-1){
		return NULL;
	}
	else
		return p;
}

static int DetachTheShm(char **p)
{
	int r;

	if (*p == NULL){
		return -1;
	}
	r = shmdt( *p );
	if (r < 0){
		return -1;
	}
	*p = NULL;

	return 0;
}

int getNameValue(FILE *file)
{
	int i, n, v, l = 0; // l == line ; v == value ; n == name ;
	char buf[200];
	
	if (file == NULL) {
		cgi_error("getNameValue wrong , parameter file is NULL !!!");
		return -1;
	}

	bzero(cgilist, sizeof(cgilist));
	bzero(buf, sizeof(buf));

	//l=linenum; n=name[n]; v=value[v]; i=buf[i];
	while ( fgets(buf, sizeof(buf), file) != NULL) {
		if (buf[0] == '\n')
			break;
		for (i=0, n=0 ; buf[i] != '='; i++) {
			cgilist[l].name[n++] = buf[i];
		}
		i++;
		for (v=0 ;buf[i] != '\n'; i++) {
			cgilist[l].value[v++] = buf[i];
		}
		if ('1' == cgilist[l].value[0] ){
			memcpy(cgilist[l].value, BOX_CHECKED, sizeof(BOX_CHECKED));
		} else {
			bzero(cgilist[l].value, sizeof(cgilist[l].value));
		}
		++l;
		bzero(buf, sizeof(buf));
	}

	return l;
}

int WriteMainTResultFile(FILE *file)
{
	int i, n, ret;
	char buf[4096];

	if (NULL == file) {
		cgi_error("WriteMainTResultFile wrong, parameter file is NULL !!!");
		return -1;
	}

	bzero(buf, sizeof(buf));

	n = 0;
	i = strlen(buf);
	while (nv[n].name[0] != '\0') {
		if ( memcmp(nv[n].name, "save_print", strlen("save_print")) == 0) {
			break;
		}
		sprintf(&buf[i], "%s=%s\n", nv[n].name, nv[n].value);
		i = strlen(buf);
		++n;
	}

	i = strlen(buf);
	while ( fgets(&buf[i], sizeof(buf), file) != NULL ) {
		i = strlen(buf);
	}

	rewind(file);
	ret = fwrite(buf, 1, strlen(buf), file);
	if (ret != strlen(buf)) {
		cgi_error("Main test - fwrite buf to file wrong !!");
		return -1;
	}

	return 0;
}


#endif /* cgifcctest.h */

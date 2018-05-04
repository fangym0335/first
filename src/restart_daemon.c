/*
 * File_name: restart_daemon.c
 * Discription: restart the ifsf_main if all fps is ready
 *
 * TODO:
 *    1.增加程序版本判断,不同版本共享内存可能不同
 *
 */
#include <time.h>
#include <sys/timeb.h>
#include "ifsf_pub.h"
#include "ifsf_main.h"

#define DAEMON_VERSION          "1.0  2009.12.17"
#define IDLE_TIME               10
#define MAX_NODE_CNT            48          // 目前所有FCC最大48口; 2009.12.16
#define CHECK_INTERVAL          10          // 隔 10s 再检查
#define MAX_TIME_FILE_LINE_LEN  20

DISPENSER_DATA *lpDsp = NULL;
IFSF_SHM *gpIfsf_shm  = NULL;
GunShm *gpGun_shm     = NULL;

int AllFPIsReady();
void lprintf(const char *fmt, ...);
void Ssleep(long usec);


/*
 * 按设置的时间长度内检查各个FP的状态,
 * 若均处于空闲时间达10则重启ifsf_main
 *
 */
int main(int argc, char *argv[])
{
	struct timeb tp;
	struct tm *tm;
	time_t start_time, exit_time;
	int ret;
	
	if (argc != 2) {
		printf("Usage:\n");
	       	printf("       %s  run_time(minutes)\n", argv[0]);
		printf("   eg: %s  60  // run_time should be less than 24[hours]\n", argv[0]);
		return -1;
	}

	exit_time = atoi(argv[1]);
	if (exit_time > 1440) {
		printf("The paramter run_time should be less than 24[hours]\n");
		return -1;
	}
	start_time = time(NULL);
	exit_time = (exit_time * 60) + start_time;


	lprintf("1.Restart_daemon [%s/PCD:%s]startup .....", DAEMON_VERSION, PCD_VERSION);

	ret = setenv("WORK_DIR", "/home/App/ifsf/", 1);
	if (ret < 0) {
		lprintf("set env WORK_DIR failed");
		return -1;
	}

	InitKeyFile();

	gpIfsf_shm = (IFSF_SHM *)AttachShm(IFSF_SHM_INDEX);
	if (gpIfsf_shm == NULL) {
		lprintf("attach to ifsf shm failed");
		return -1;
	}

	gpGun_shm = (GunShm *)AttachShm(GUN_SHM_INDEX);
	if (gpGun_shm == NULL) {
		lprintf("attach to gun shm failed");
		return -1;
	}


	lprintf("2.Start check fp state .....");
	do {
		if (AllFPIsReady() == 0) {
			if (system("/usr/local/bin/run_ctrl -r") == -1) {
				lprintf("Exec restart ifsf_main failed, retry...");
			} else {
				lprintf("9.Exec restart ifsf_main successed!");
				return 0;
			}
		} else {
			sleep(CHECK_INTERVAL);
		}
	} while (exit_time > time(NULL));

	lprintf("My work done.");
	return 0;
}

/*
 * Func: 如果所有在线节点的FP均IDLE达 IDLE_TIME 那么就认为Ready.
 *
 * Return: 0 - Ready; (-1) - not yet
 */


int AllFPIsReady()
{
	int i;
	time_t idle_time_wait = 0;

	lprintf("come into Func AllFPIsReady");
	do {
		for (i = MAX_NODE_CNT - 1; i >= 0; --i) {
		//	lprintf("node %d online? %d, idle_time_wait: %ld",
		//		i + 1, gpIfsf_shm->node_online[i], idle_time_wait);
			if (gpIfsf_shm->node_online[i] == 0) {
				continue;
			}

			lpDsp = (DISPENSER_DATA *)&(gpIfsf_shm->dispenser_data[i]);
			lprintf("node %2d fp 1 state: %d", i + 1, lpDsp->fp_data[0].FP_State);
			lprintf("node %2d fp 2 state: %d", i + 1, lpDsp->fp_data[1].FP_State);
			lprintf("node %2d fp 3 state: %d", i + 1, lpDsp->fp_data[2].FP_State);
			lprintf("node %2d fp 4 state: %d", i + 1, lpDsp->fp_data[3].FP_State);
			// 按两个FP处理, 目前实际上不会多余两个FP
			if ((lpDsp->fp_data[0].FP_State >= FP_State_Calling) ||\
				(lpDsp->fp_data[1].FP_State >= FP_State_Calling) ||\
				(lpDsp->fp_data[2].FP_State >= FP_State_Calling) ||\
				(lpDsp->fp_data[3].FP_State >= FP_State_Calling)) {
				return -1;
			}
		}

		if (idle_time_wait == 0) {
			idle_time_wait = time(NULL) + IDLE_TIME;
		}

		lprintf("all fp now is idle, waiting for 500ms to check again");
		Ssleep(500000);      // 500ms 后再检查
	} while (time(NULL) < idle_time_wait);

	return 0;
}

void lprintf(const char *fmt, ...)
{
	va_list argptr;
	int line;
	int fd;
	static char sMessage[1024];
	unsigned char date_time[7] = {0};
	unsigned char fileName[33];

	struct timeb tp;
	struct tm *tm;


	ftime(&tp);
	tm = localtime(&(tp.time));

	sprintf(sMessage, "%02d-%02d %02d:%02d:%02d.%03d ", (tm->tm_mon + 1), (tm->tm_mday),
			(tm->tm_hour), (tm->tm_min), (tm->tm_sec), (tp.millitm));
	
//	GetFileAndLine(fileName, &line);
//	sprintf(sMessage + strlen(sMessage), "%s:%d ", fileName, line);

	sMessage[MAX_TIME_FILE_LINE_LEN] = '\0';
	memset(sMessage + strlen(sMessage), ' ', MAX_TIME_FILE_LINE_LEN - strlen(sMessage));

	sprintf(sMessage + strlen(sMessage), "[%-4d]  ", getpid());

	va_start(argptr, fmt);
	vsprintf(sMessage + strlen(sMessage), fmt, argptr);
	va_end(argptr);

	printf(" %s\n", sMessage);

#if 0
	if ((fd = open("/home/Data/log_backup/restart_daemon.log", O_RDWR | O_CREAT | O_APPEND, 00644)) < 0) {
		printf("Open log file restart_daemon.log failed\n");
		return;
	}

	sprintf(sMessage + strlen(sMessage), "\n");
	write(fd, sMessage, strlen(sMessage));

	close(fd);
#endif

	return;
}

/*
 * Func: sleep msec microseconds
 *
 */

void Ssleep(long usec)
{
    struct timeval timeout;
    ldiv_t d;

    d = ldiv(usec, 1000000L);
    timeout.tv_sec = d.quot;
    timeout.tv_usec = d.rem;
    select( 0, NULL, NULL, NULL, &timeout );
}

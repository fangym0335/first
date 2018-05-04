#ifndef __SETCFG_H__
#define __SETCFG_H__

#include "pub.h"

#define CFGFILE "/home/App/ifsf/etc/tlg.cfg"
typedef struct INITCFG{
	unsigned char name[64];
	unsigned char value[64];
}init_cfg;

typedef struct CFGOPT{
	int if_open_dlb;
	int if_open_leak;
}cfg_opt;

//First run GetPortChannel, then you can use GetSerialPort or GetChnPhy.
int GetPortChannel(char * portChnl);
int GetSerialPort(const int chnLog, const char *portChnl);
int GetChnPhy(const int chnLog, const char *portChnl);
int GetIfsfCfgNameSize( );
int GetCfgNameSize( );
int RecvNodeCfg( const unsigned char *cmd, int cmdlen );
int SaveWorkDir( );
int ListenCfg();
int OpenCfg();
int SyncCfg2File();
int ReadAllCfg(init_cfg *initcfg);
int ReadCfgByName(init_cfg *initcfg, const char *name);

#endif


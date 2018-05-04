#2008.3.5 new Makefile for CSSI FCC IFSF program by Yin Jiangbo 
#Copyright (c) css-intelligent Ltd.

.SUFFIXES: .c .o .e .so

WORKDIR=../

ALLSRC=$(WORKDIR)/src/
LIBSRC=$(WORKDIR)/libsrc/
LIBINC=$(WORKDIR)/include/
LIBPATH=$(WORKDIR)/lib/
DLLPATH=$(WORKDIR)/lib/
BINPATH=$(WORKDIR)/bin/

PUBLIB=-lmysock -lmyipc -lcomm -lerrctl -ldebug -lvrcomm
SYS_LIB=-lm -lpthread -lcrypt -ljansson

CROSS_COMPILER=arm-linux-
CC=$(CROSS_COMPILER)gcc
AR=$(CROSS_COMPILER)ar
STRIP=$(CROSS_COMPILER)strip

DEFS =	 -DHAVE__PROGNAME=1 -DHAVE_FCNTL_H=1 -DHAVE_GRP_H=1 -DHAVE_MEMORY_H=1 -DHAVE_PATHS_H=1 -DHAVE_POLL_H=1 -DHAVE_SYS_POLL_H=1 -DTIME_WITH_SYS_TIME=1 -DHAVE_DIRENT_H=1 -DHAVE_LIBCRYPT=1 -DHAVE_STRERROR=1 -DHAVE_WAITPID=1 -DHAVE_VSNPRINTF=1 -DHAVE_DAEMON=1 -DHAVE_SETSID=1 -DHAVE_GETADDRINFO=1 -DHAVE_GETNAMEINFO=1 -DHAVE_GAI_STRERROR=1 -DHAVE_SIGSET=1 -DHAVE_ATOLL=1 -DHAVE_UNISTD_H=1 -DHAVE_GETPAGESIZE=1 -DHAVE_MMAP=1 -DHAVE_SELECT=1 -DHAVE_POLL=1 -DHAVE_TM_GMTOFF=1 -DHAVE_INT64T=1 -DHAVE_SOCKLENT=1  

LINT=lint
COMPFLAG=-g
COMPFLAG=-O2
EFLAGS=-E -I$(LIBINC)

ver = realse
ifeq ($(ver),debug)
CFLAGS+=$(COMPFLAG) -DTANK_DEBUG -I$(LIBINC) $(SFLAGS) 
else
CFLAGS+=$(COMPFLAG) -I$(LIBINC) $(SFLAGS)
endif
#--static 
LFLAGS=-L$(LIBPATH) $(PUBLIB)

#PUB_OBJ=mytime.o bcd.o init.o  lrc.o pidctrl.o setcfg.o tty.o log.o pubsock.o clearind.o pub.o spec.o nic_monitor.o
PUB_OBJ= log.o
#IFSF_OBJ=ifsf_dsp.o ifsf_tcpip.o ifsf_tlg.o
PUMP_FILES:= $(wildcard )
PUMP_OBJ:=$(patsubst %.c,%.o,$(PUMP_FILES))
TANK_FILES:= #$(wildcard tank*.c)
TANK_OBJ:=#$(patsubst %.c,%.o,$(TANK_FILES))
MISC_OBJ= #tcc_adjust.o
#ALLOBJ=$(PUB_OBJ) $(PUMP_OBJ) $(TANK_OBJ) $(MISC_OBJ)
ALLOBJ=$(PUB_OBJ) $(PUMP_OBJ) $(MISC_OBJ)
WRITPOBJ= log.o  
DL_OBJ=
#log.o mytime.o bcd.o

all:$(ALLOBJ) cgidumpshm sinopec_main
tools:$(ALLOBJ) writp cgidumpshm update_main testconn pw_time  resetipd restart_daemon
#dla_main 


mytime.o: mytime.c mytime.h
	$(CC) $(CFLAGS) $(MYINC)  -c $*.c
$(TANK_OBJ):$(TANK_FILES)
	$(CC) $(CFLAGS) $(MYINC) -c $^
ifsf_dsp.o:ifsf_dspcal.c ifsf_dsper.c ifsf_dspfp.c ifsf_dsplnz.c ifsf_dspmeter.c  ifsf_dspprod.c ifsf_dsptr.c ifsf_dsp.c ifsf_dspcal.h ifsf_dsper.h ifsf_dspfp.h ifsf_dsplnz.h ifsf_dspmeter.h  ifsf_dspprod.h ifsf_dsptr.h ifsf_dsp.h ifsf_def.h ifsf_pub.h
	$(CC) $(CFLAGS) $(MYINC) -c $*.c
ifsf_tcpip.o:ifsf_tcpsv.c ifsf_tcpip.c ifsf_tcpsv.h ifsf_tcpip.h ifsf_def.h ifsf_pub.h
	$(CC) $(CFLAGS) $(MYINC)  -c $*.c
ifsf_tlg.o:ifsf_tlgtlg.c ifsf_tlgtp.c ifsf_tlg.c ifsf_tlgtlg.h ifsf_tlgtp.h ifsf_tlg.h 
	$(CC) $(CFLAGS) $(MYINC)  -c $*.c
ifsf_main:ifsf_main.o ifsf_main.h pub.h ifsf_def.h ifsf_pub.h 
	$(CC) $(CFLAGS) $(MYINC) -o $@ $@.o $(ALLOBJ) $(LFLAGS) $(MYLIB) $(SYS_LIB)
	$(STRIP) $@
	mv $@ $(BINPATH)
sinopec_main:sinopec_main.o 
	$(CC) $(CFLAGS) $(MYINC) -o $@ $@.o $(ALLOBJ) $(LFLAGS) $(MYLIB) $(SYS_LIB)
	$(STRIP) $@
	mv $@ $(BINPATH)
  
cgidumpshm:cgidumpshm.o ifsf_pub.h  cgidecoder.h
		$(CC) $(CFLAGS) --static $(CCOPT) $(DEFS) $(MYINC) -o $@ $@.o $(LFLAGS) $(MYLIB) $(SYS_LIB)
		$(STRIP) $@
		mv $@ $(BINPATH)
testconn:testconn.o
		$(CC) $(CFLAGS) 	$(CCOPT) $(DEFS) $(MYINC) -o $@ $@.o $(LFLAGS) $(MYLIB) $(SYS_LIB)
		$(STRIP) $@
		mv $@ $(BINPATH)
writp:writp.o 
		$(CC) $(CFLAGS) 	$(CCOPT) $(DEFS) $(MYINC) -o $@ $^ $(LFLAGS) $(MYLIB) $(SYS_LIB)
		$(STRIP) $@
		mv $@ $(BINPATH)
fcc_selftest:fcc_selftest.o
		$(CC) $(CFLAGS) 	$(CCOPT) $(DEFS) $(MYINC) -o $@ $@.o $(LFLAGS) $(MYLIB) $(SYS_LIB)
		$(STRIP) $@
		mv $@ $(BINPATH)
fcc_maintest:fcc_maintest.o
		$(CC) $(CFLAGS) 	$(CCOPT) $(DEFS) $(MYINC) -o $@ $@.o $(LFLAGS) $(MYLIB) $(SYS_LIB)
		$(STRIP) $@
		mv $@ $(BINPATH)
update_main:update_main.o
		$(CC) $(CFLAGS) 	$(CCOPT) $(DEFS) $(MYINC) -o $@ $@.o $(LFLAGS) $(MYLIB) $(SYS_LIB)
		$(STRIP) $@
		mv $@ $(BINPATH)
## For second distribution(dalian)
#dla_main:dla_main.o log.o mytime.o
#		$(CC) $(CFLAGS)	$(CCOPT) $(DEFS) $(MYINC) -o $@ $@.o $(DL_OBJ) $(LFLAGS) $(MYLIB) $(SYS_LIB)
#		mv $@ $(BINPATH)
#dlb_main:dlb_main.o log.o mytime.o bcd.o
#		$(CC) $(CFLAGS)	$(CCOPT) $(DEFS) $(MYINC) -o $@ $@.o $(DL_OBJ) $(LFLAGS) $(MYLIB) $(SYS_LIB)  
#		mv $@ $(BINPATH)
pw_time:pw_time.o
		$(CC) $(CFLAGS)	$(CCOPT) $(DEFS) $(MYINC) -o $@ $@.o $(LFLAGS) $(MYLIB) $(SYS_LIB) 
		mv $@ $(BINPATH)
restart_daemon:restart_daemon.o
		$(CC) $(CFLAGS)	$(CCOPT) $(DEFS) $(MYINC) -o $@ $@.o $(LFLAGS) $(MYLIB) $(SYS_LIB) 
		mv $@ $(BINPATH)
resetipd:
		$(CC) -o $@ resetipd.c
		mv $@ $(BINPATH)
listconfig:listconfig.o
		$(CC) $(CFLAGS)	$(CCOPT) $(DEFS) $(MYINC) -o $@ $@.o $(LFLAGS) $(MYLIB) $(SYS_LIB) 
		mv $@ $(BINPATH)

#cgioil_ctl:cgioil_ctl.o ifsf_pub.h  cgidecoder.h
#		$(CC) $(CFLAGS)	$(CCOPT) $(DEFS)  $(MYINC) -o $@ $@.o $(LFLAGS) $(MYLIB) $(SYS_LIB)
#		mv cgioil_ctl $(BINPATH)
#cgitestdsp:cgitestdsp.o ifsf_pub.h  cgidecoder.h
#		$(CC) $(CFLAGS) 	$(CCOPT) $(DEFS) $(MYINC) -o $@ $@.o $(LFLAGS) $(MYLIB) $(SYS_LIB)
#		mv cgitestdsp $(BINPATH)
#cgitty:cgitty.o log.o ifsf_pub.h  cgidecoder.h
#		$(CC) $(CFLAGS) 	$(CCOPT) $(DEFS) $(MYINC) -o $@ $@.o $(LFLAGS) $(MYLIB) $(SYS_LIB)
#		mv cgitty $(BINPATH)
					
#cgitestdsp:cgitestdsp.o log.o ifsf_pub.h  cgidecoder.h
#		$(CC) $(CFLAGS) 	$(CCOPT) $(DEFS)  $(MYINC) -o $@ $@.o $(WRITPOBJ) $(LFLAGS) $(MYLIB) $(SYS_LIB)
#		mv $@ $(BINPATH)
#	showsram:showsram.o
#		$(CC) $(CFLAGS) $(MYINC) -o $@ $@.o $(ALLOBJ) $(LFLAGS) $(MYLIB) $(SYS_LIB)
#		mv $@ $(BINPATH)
#	getcrc:getcrc.c
#		$(CC) $(CFLAGS) $(MYINC) -o $@ $@.c
#		mv $@ $(BINPATH)

				
clean:
	rm -f $(ALLSRC)*.o
	rm -f $(BINPATH)/*
.c.o:
	$(CC) $(CFLAGS) $(MYINC) -c $*.c
.c.so:
	$(CC) $(CFLAGS) $(MYINC) -c $*.c
	$(CC) $(CFLAGS) $(MYINC) -o $@ $@.o
.c.e:
	$(CC) $(EFLAGS) $(MYINC) $*.c > $*.e
.o:
	$(CC) $(CFLAGS) $(MYINC) -o $@ $@.o $(ALLOBJ) $(LFLAGS) $(MYLIB) $(SYS_LIB)
	rm $@.o

.PHONY: all
all: sinopec_main

all:liberrctl.a
check:

PRECOMP=/mnt/hgfs/myshare/FCC_SRC20170824/fcc/precomp
include $(PRECOMP)/precomp_dependence_c.mk

MYINC=
MYLIB=
ALLOBJ=
SYS_LIB=

COMM_OBJ= errctl.o

liberrctl.a:$(COMM_OBJ)
	$(AR) $(ARFLAGS) $@ $(COMM_OBJ)
	rm *.o
	mv $@ $(LIBPATH)

clean:
	if [ -s *.ln ];	then rm *.ln; fi
	if [ -s *.e ];	then rm *.e; fi
	if [ -s *.o ];	then rm *.o; fi

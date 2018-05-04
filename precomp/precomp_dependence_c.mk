# dependence_c.mk
# 普通C文件的缺省编译程序
# .c TO .o 将普通C文件编译成.o文件
#
#
#
#
#

.SUFFIXES: .c .o .e .ln

include $(PRECOMP)/precomp_env.mk

.c.o:
	$(CC) $(CFLAGS) $(MYINC) -c $*.c

.c.e:
	$(CC) $(EFLAGS) $(MYINC) $*.c > $*.e

.c.ln:
	$(LINT) $(LNFLAGS) $(MYINC) -c $*.c | sed '/null effect/d'
	rm $*.ln
.o:
	$(CC) $(CFLAGS) $(MYINC) -o $@ $@.o $(ALLOBJ) $(LFLAGS) $(MYLIB) $(SYS_LIB)
	rm $@.o
.c:
	$(CC) $(CFLAGS) $(MYINC) -o $@ $@.c $(ALLOBJ) $(LFLAGS) $(MYLIB) $(SYS_LIB)
	if [ -f $@.o ]; then rm $@.o; fi

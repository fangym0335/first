# dependence_c.mk
# ��ͨC�ļ���ȱʡ�������
# .c TO .o ����ͨC�ļ������.o�ļ�
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

# Makefile
# LOGGING_FDB_CAHNGE is work on wide camp 1203 only

CC = gcc -Wall -g -DLOGGING_FDB_CHANGE

MODULES = error.o fdb.o hash.o iftap.o net.o vxlan.o control.o
PROGNAME = vxland vxlanctl

.PHONY : all
all : modules vxland vxlanctl

.c.o:
	$(CC) -c $< -o $@

modules : $(MODULES)

vxland : main.c common.h $(MODULES)
	$(CC) -lpthread main.c $(MODULES) -o vxland

vxlanctl : vxlanctl.c common.h
	$(CC) vxlanctl.c -o vxlanctl

clean :
	rm $(MODULES) $(PROGNAME)

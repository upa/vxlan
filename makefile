# Makefile

CC = gcc -Wall -g 

MODULES = error.o fdb.o hash.o iftap.o net.o vxlan.o
PROGNAME = vxlan

.PHONY : all
all : modules vxlan

.c.o:
	$(CC) -c $< -o $@

modules : $(MODULES)

vxlan : main.c common.h $(MODULES)
	$(CC) -lpthread main.c $(MODULES) -o vxlan

clean :
	rm $(MODULES) vxlan

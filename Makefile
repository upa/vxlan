# Makefile

CC = gcc -Wall -g
PROGNAME = vxlan
MODULES = iftap.o hash.o fdb.o net.o

.PHONY: all
all: $(PROGNAME)

.c.o:
	$(CC) -c $< -o $@

vxlan: main.c $(MODULES)
	$(CC) main.c $(MODULES) -lpthread -o vxlan

modules: $(MODULES)

clean:
	rm $(MODULES) $(PROGNAME)

fdb.o: fdb.h fdb.c common.h
net.o: net.h net.c common.h
iftap.o: iftap.h iftap.h common.h


hash.o: hash.h hash.c
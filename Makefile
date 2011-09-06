# Makefile

CC = gcc -Wall -g
PROGNAME = 
MODULES = iftap.o hash.o fdb.o net.o

.PHONY: all
all: $(PROGNAME)

.c.o:
	$(CC) -c $< -o $@

modules: $(MODULES)

clean:
	rm $(MODULES) $(PROGNAME)


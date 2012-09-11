# Makefile
# LOGGING_FDB_CAHNGE is work on wide camp 1203 only

CC = gcc -Wall -g 

MODULES = error.o fdb.o iftap.o net.o vxlan.o control.o
PROGNAME = vxland vxlanctl
INITSCRIPT = vxlan

INSTALLDST = /usr/local/sbin/

RCDST = /etc/init.d/
UPDATERCD = /usr/sbin/update-rc.d


.PHONY : all
all : vxland vxlanctl

.c.o:
	$(CC) -c $< -o $@

modules : $(MODULES)

vxland : main.c common.h $(MODULES)
	$(CC) -pthread main.c $(MODULES) -o vxland

vxlanctl : vxlanctl.c common.h
	$(CC) vxlanctl.c -o vxlanctl

clean :
	rm $(MODULES) $(PROGNAME)

install	: $(PROGNAME)
	install vxland $(INSTALLDST)
	install vxlanctl $(INSTALLDST)
	install $(INITSCRIPT) $(RCDST)
	$(UPDATERCD) $(INITSCRIPT) defaults
	
	
uninstall : $(PROGNAME)
	rm $(INSTALLDST)/vxland
	rm $(INSTALLDST)/vxlanctl
	$(UPDATERCD) -f $(INITSCRIPT) remove
	rm $(RCDST)/$(INITSCRIPT)


control.c	: control.h
error.c		: error.h
fdb.c		: fdb.h
iftap.c		: iftap.h
net.h		: common.h
net.c		: net.h fdb.h error.h sockaddrmacro.h
vxlan.h		: common.h
vxlan.c		: net.h fdb.h error.h vxlan.h iftap.h sockaddrmacro.h


# Example makefile for CPE 464
#

CC = gcc
CFLAGS = -g -Wall -Werror -Wextra -std=gnu2x
OS = $(shell uname -s)
PROC = $(shell uname -p)
EXEC_SUFFIX=$(OS)-$(PROC)

ifeq ("$(OS)", "SunOS")
	OSLIB=-L/opt/csw/lib -R/opt/csw/lib -lsocket -lnsl
	OSINC=-I/opt/csw/include
	OSDEF=-DSOLARIS
else
ifeq ("$(OS)", "Darwin")
	OSLIB=
	OSINC=
	OSDEF=-DDARWIN
else
	OSLIB=
	OSINC=
	OSDEF=-DLINUX
endif
endif

FISHLIB = -L. -lfish-$(EXEC_SUFFIX)

all: fishnode-$(EXEC_SUFFIX)

fishnode-$(EXEC_SUFFIX): fishnode.c smartalloc.c fishnode.h fish.h smartalloc.h
	$(CC) $(CFLAGS) $(OSINC) $(OSDEF) -o $@ fishnode.c smartalloc.c $(FISHLIB) $(OSLIB)

handin: README.md
	~bellardo/bin/rcvhandin bellardo p1 README.md smartalloc.c smartalloc.h fishnode.c Makefile

clean:
	rm -rf fishnode-* fishnode-*.dSYM

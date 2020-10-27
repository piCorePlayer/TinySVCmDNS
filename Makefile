#
# Makefile for pcpmdnsd
#

PREFIX = /usr/local
BINDIR = $(PREFIX)/bin

CFLAGS += -Wall -pedantic -std=gnu99
#CFLAGS += -g
CFLAGS += -Os -DNDEBUG
LDLIBS = -lpthread
INSTALL_PROGRAM="install"
ifneq ($(CROSS_COMPILE),)
  CC = gcc
  CC := $(CROSS_COMPILE)$(CC)
  AR := $(CROSS_COMPILE)$(AR)
endif

BIN=pcpmdnsd

LIBTINYSVCMDNS_OBJS = mdns.o mdnsd.o

.PHONY: all clean

all: $(BIN) libtinysvcmdns.a

clean:
	-$(RM) *.o
	-$(RM) *.bin
	-$(RM) mdns
	-$(RM) $(BIN)
	-$(RM) libtinysvcmdns.a

mdns: mdns.o

mdnsd: mdns.o mdnsd.o

#testmdnsd: testmdnsd.o libtinysvcmdns.a
pcpmdnsd: pcpmdnsd.o libtinysvcmdns.a

libtinysvcmdns.a: $(patsubst %, libtinysvcmdns.a(%), $(LIBTINYSVCMDNS_OBJS))

install: pcpmdnsd
	install -D $(BIN) $(BINDIR)/$(BIN)

install-strip: pcpmdnsd
	install -D -s $(BIN) $(BINDIR)/$(BIN)


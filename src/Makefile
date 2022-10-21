include ../Makefile.cfg
CC=gcc
BUILD=$(shell svnversion . | sed -e 's/^[0-9].*://g' -e 's/[a-zA-Z]//g')
CFLAGS=-g -Wall -Wextra -Wcast-align -Wshadow -Wstrict-prototypes -O2 -Wno-makunused-but-set-parameter -Wno-unused-variable ${OPTS} -DHAVE_$(CRYPTO_TYPE) -I../include -I./include
LDFLAGS=-lz -lpthread -lutil -lc

#objects := $(patsubst %.c,%.o,$(wildcard *.c))

ifeq ($(CRYPTO_TYPE),OPENSSL)
LDFLAGS+=-lcrypto
else
LDFLAGS+=-lgcrypt -lgpg-error
endif
ifeq ($(TUNTAP),yes)
objects := $(patsubst %.c,%.o,$(wildcard *.c))
CFLAGS+=-DHAVE_TUNTAP
else
vobjects := $(patsubst vpn_%.c,,$(wildcard *.c))
tobjects := $(patsubst tun.c,,$(vobjects))
objects := $(patsubst %.c,%.o,$(tobjects))
endif

all: config ctunnel

config:
	@echo "--------------------------------------------------------------------------------"
	@echo "                          ctunnel build using $(CRYPTO_TYPE)"
	@echo "--------------------------------------------------------------------------------"

ctunnel: $(objects)
	$(CC) $(CFLAGS) -o ctunnel $(objects) $(LDFLAGS)
install:
	install ctunnel $(PREFIX)/bin
uninstall:
	rm -f $(PREFIX)/bin/ctunnel
clean:
	rm -f ctunnel *.o

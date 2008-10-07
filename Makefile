ifneq ($(DESTDIR),)
  prefix ?= $(DESTDIR)/usr
endif
prefix ?= /usr/local
exec_prefix ?= $(prefix)
datarootdir ?= $(prefix)/share
bindir ?= $(exec_prefix)/bin
libexecdir ?= $(exec_prefix)/lib
man1dir ?= $(datarootdir)/man/man1
CFLAGS = -Wall


all: mmda

install: mmda sendmail
	install -D --mode=a=rx,u+w sendmail $(bindir)/sendmail
	install -D --mode=a=rx,u+ws --strip mmda $(libexecdir)/mmta/mmda

mmda: mmda.c
	gcc $(CFLAGS) -o mmda mmda.c -llockfile

clean:
	-rm -f mmda

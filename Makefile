ifneq ($(DESTDIR),)
  prefix ?= $(DESTDIR)/usr
endif
prefix ?= /usr/local
exec_prefix ?= $(prefix)
datarootdir ?= $(prefix)/share
bindir ?= $(exec_prefix)/bin
sbindir ?= $(exec_prefix)/sbin
libexecdir ?= $(exec_prefix)/lib
man1dir ?= $(datarootdir)/man/man1

CFLAGS = -Wall
MMDABIN ?= $(libexecdir)/mmta/mmda
USERCONFDIR ?= ".config/mmta"
SYSCONFDIR ?= "/etc/mmta"


all: mmda sendmail

install: mmda sendmail
	install -D --mode=a=rx,u+w sendmail $(sbindir)/sendmail
	install -D --mode=a=rx,u+ws --strip mmda $(MMDABIN)
	install -d $(datarootdir)/mmta
	install -D --mode=a=rx,u+w scripts/* $(datarootdir)/mmta

sendmail: sendmail.in
	cat sendmail.in | sed -e "s,@@MMDABIN@@,$(MMDABIN),g" \
	-e "s,@@USERCONFDIR@@,$(USERCONFDIR),g" \
	-e "s,@@SYSCONFDIR@@,$(SYSCONFDIR),g" > sendmail

mmda: mmda.c
	gcc $(CFLAGS) -o mmda mmda.c -llockfile

clean:
	-rm -f mmda sendmail

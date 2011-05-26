prefix ?= /usr/local
exec_prefix ?= $(prefix)
datarootdir ?= $(prefix)/share
bindir ?= $(exec_prefix)/bin
sbindir ?= $(exec_prefix)/sbin
libexecdir ?= $(exec_prefix)/lib
man1dir ?= $(datarootdir)/man/man1

MMDABIN ?= $(libexecdir)/mmta/mmda
USERCONFDIR ?= ".config/mmta"
SYSCONFDIR ?= "/etc/mmta"
CFLAGS = -Wall -DUSERCONFDIR='$(USERCONFDIR)' -DSYSCONFDIR='$(SYSCONFDIR)'

all: mmda sendmail sendmail.1 mmda.1

install: mmda sendmail
	install -D --mode=a=rx,u+w sendmail $(DESTDIR)$(sbindir)/sendmail
	install -D --mode=a=rx,u+ws --strip mmda $(DESTDIR)$(MMDABIN)
	install -d $(DESTDIR)$(datarootdir)/mmta
	install -D --mode=a=rx,u+w scripts/* $(DESTDIR)$(datarootdir)/mmta
	install -D --mode=a=rx,u+w mmta-send-allusers $(DESTDIR)$(datarootdir)/mmta

sendmail: sendmail.in
	cat sendmail.in | sed -e 's,@@MMDABIN@@,$(MMDABIN),g' \
	-e 's,@@USERCONFDIR@@,$(USERCONFDIR),g' \
	-e 's,@@SYSCONFDIR@@,$(SYSCONFDIR),g' > sendmail

mmda: mmda.c
	gcc $(CFLAGS) -o mmda mmda.c -llockfile

%.1:%.1.txt
	a2x -f manpage $<

clean:
	-rm -f mmda sendmail sendmail.1 sendmail.1.xml mmda.1 mmda.1.xml

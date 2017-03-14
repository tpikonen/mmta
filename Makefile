prefix ?= /usr
exec_prefix ?= $(prefix)
datarootdir ?= $(prefix)/share
bindir ?= $(exec_prefix)/bin
sbindir ?= $(exec_prefix)/sbin
libexecdir ?= $(exec_prefix)/lib
man1dir ?= $(datarootdir)/man/man1

MMDABIN ?= $(libexecdir)/mmta/mmda
MMTASQBIN ?= $(libexecdir)/mmta/mmta-send-queue
USERCONFDIR ?= ".config/mmta"
SYSCONFDIR ?= "/etc/mmta"
CFLAGS += -Wall -DUSERCONFDIR='$(USERCONFDIR)' -DSYSCONFDIR='$(SYSCONFDIR)'
CFLAGS += -DDEBUG=0

manpages := $(patsubst %.txt,%,$(wildcard *.[1-8].txt))

all: mmda mmta-send-queue sendmail $(manpages)

install: mmda mmta-send-queue sendmail
	install -D --mode=a=rx,u+w sendmail $(DESTDIR)$(sbindir)/sendmail
	install -D --mode=a=rx,u+ws --strip mmda $(DESTDIR)$(MMDABIN)
	install -D --mode=a=rx,u+ws --strip mmta-send-queue \
		$(DESTDIR)$(MMTASQBIN)
	install -d $(DESTDIR)$(datarootdir)/mmta
	install -D --mode=a=rx,u+w scripts/* $(DESTDIR)$(datarootdir)/mmta
	install -D --mode=a=rx,u+w mmta-send-allusers \
		$(DESTDIR)$(datarootdir)/mmta

sendmail: sendmail.in
	cat sendmail.in | sed \
	-e 's,@@MMDABIN@@,$(MMDABIN),g' \
	-e 's,@@MMTASQBIN@@,$(MMTASQBIN),g' \
	-e 's,@@USERCONFDIR@@,$(USERCONFDIR),g' \
	-e 's,@@SYSCONFDIR@@,$(SYSCONFDIR),g' > sendmail
	chmod a+x sendmail

mmta-common.o: mmta-common.c
	gcc -c mmta-common.c $(CPPFLAGS) $(CFLAGS) $(LDFLAGS)

mmda: mmda.c mmta-common.o
	gcc -o mmda mmda.c mmta-common.o -llockfile $(CPPFLAGS) $(CFLAGS) $(LDFLAGS)

mmta-send-queue: mmta-send-queue.c mmta-common.o
	gcc -o mmta-send-queue mmta-send-queue.c mmta-common.o $(CPPFLAGS) $(CFLAGS) $(LDFLAGS)

%.5 : %.5.txt
%.8 : %.8.txt
$(manpages) :
	[ -x /usr/bin/a2x ] && a2x -f manpage $@.txt \
		|| printf "***\n*** Asciidoc /usr/bin/a2x not found\n***\n" ;\

clean:
	-rm -f mmda mmta-send-queue mmta-common.o sendmail

realclean: clean
	-rm -r $(manpages)

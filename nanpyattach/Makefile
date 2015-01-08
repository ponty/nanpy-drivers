CFLAGS		?= -g -O2 -Wall

PROGRAMS	= nanpyattach

PREFIX          ?= /usr/local

compile: $(PROGRAMS)

distclean: clean
clean:
	$(RM) *.o *.swp $(PROGRAMS) *.orig *.rej map *~

install: compile
	install -d $(DESTDIR)$(PREFIX)/bin
	install $(PROGRAMS) $(DESTDIR)$(PREFIX)/bin

.PHONY: compile clean distclean install

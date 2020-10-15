ifeq ($(DEBUG),1)
YFLAGS=-t -v
CPPFLAGS=-DYYDEBUG=1
endif

OBJS = lexer.o y.tab.o sblist.o sblist_pop.o hsearch.o tokens.o

-include config.mak

HOSTCC ?= $(CC)

all: re2r re2r_test

lexer.c: y.tab.h
main.c: y.tab.h template.h
tokens.c: y.tab.h

template.h: file2hdr ragel.tmpl
	./file2hdr ragel.tmpl > template.h

file2hdr: file2hdr.c
	$(HOSTCC) $(HOSTCFLAGS) $< -o $@

y.tab.h: y.tab.c

y.tab.c: ere.y
	$(YACC) -d $(YFLAGS) $<

re2r: main.o $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

re2r_test: re2r_test.o $(OBJS)
	$(CC) -o $@ $^ -ldl $(LDFLAGS)

clean:
	rm -rf re2r re2r_test file2hdr *.o y.tab.h y.tab.c template.h

.PHONY: all clean

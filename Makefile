ifeq ($(DEBUG),1)
YFLAGS=-t -v
CPPFLAGS=-DYYDEBUG=1
endif

-include config.mak

all: re2r

lexer.c: y.tab.h
main.c: y.tab.h

y.tab.h: y.tab.c

y.tab.c: ere.y
	$(YACC) -d $(YFLAGS) $<

re2r: main.o lexer.o y.tab.o
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -rf re2r *.o y.tab.h y.tab.c

.PHONY: all clean

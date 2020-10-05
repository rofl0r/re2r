#include <stdio.h>
#include <string.h>

#include "y.tab.h"
#include "yydefs.h"

extern int yyerror(const char*);

FILE *yyin;

extern int yyparse();
extern void lex_init();
extern int yydebug;

int main() {
#ifdef YYDEBUG
	yydebug = 1;
#endif
	char buf[4096];
	yyin = stdin;
	while(fgets(buf, sizeof buf, yyin)) {
		const char* p = buf, *pe = strrchr(buf, '\n');
		if(!pe) pe = buf + strlen(p);
		lex_init(p, pe);
		yyparse();
	}
}

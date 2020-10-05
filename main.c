#include <stdio.h>
#include <string.h>

#include "y.tab.h"
#include "yydefs.h"
#include "lexer.h"

extern int yyerror(const char*);

FILE *yyin;

extern int yyparse();
extern int yydebug;

int main() {
#ifdef YYDEBUG
	yydebug = 1;
#endif
	char buf[4096];
	size_t lineno = 0;
	yyin = stdin;
	while(fgets(buf, sizeof buf, yyin)) {
		++lineno;
		const char* p = buf, *pe = strrchr(buf, '\n');
		if(!pe) pe = buf + strlen(p);
		lex_init(p, pe, LEXFLAG_SILENT);
		if(yyparse() == 0) {
			/* syntax check OK */
		} else {
			size_t errpos = lex_errpos();
			fprintf(stderr, "parse error @%zu:%zu\n", lineno, errpos);
			fprintf(stderr, "%.*s\n", (int)(pe-p), p);
			fprintf(stderr, "%*s^\n", (int)errpos, "");
		}
	}
}

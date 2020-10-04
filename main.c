#include <stdio.h>
#include <string.h>

#include "y.tab.h"
#include "yydefs.h"

extern int yyerror(const char*);

FILE *yyin;

extern int yyparse();
extern void lex_init(void);
extern int yydebug;

int main() {
#ifdef YYDEBUG
	yydebug = 1;
#endif
	lex_init();
	yyin = stdin;
	yyparse();
}

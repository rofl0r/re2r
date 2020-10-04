#include <stdio.h>

#ifndef YYSTYPE
#define YYSTYPE int
#endif

extern int yyerror(const char*);
extern YYSTYPE yylval;
extern FILE* yyin;


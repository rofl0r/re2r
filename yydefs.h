#ifndef YYDEFS_H
#define YYDEFS_H

#include <stdio.h>

#ifndef YYSTYPE
#define YYSTYPE int
#endif

extern int yyerror(const char*);
extern int yylex();
extern YYSTYPE yylval;
extern FILE* yyin;

#endif

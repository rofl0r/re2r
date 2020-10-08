#ifndef LEXER_H
#define LEXER_H

#include <unistd.h> /* size_t */

#define LEXFLAG_SILENT (1<<0)

enum lex_context {
	CTX_NONE=0,
	CTX_DUP,
	CTX_BRACKET,
};

enum lex_context lex_getcontext(void);
void lex_init(const char *p, const char *pe, int flags);
size_t lex_errpos(void);
size_t lex_getpos(void);
int yylex(void);

#endif


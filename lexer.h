#ifndef LEXER_H
#define LEXER_H

#include <unistd.h> /* size_t */

#define LEXFLAG_SILENT (1<<0)

void lex_init(const char *p, const char *pe, int flags);
size_t lex_errpos(void);


#endif


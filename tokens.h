#ifndef TOKENS_H
#define TOKENS_H

#include <unistd.h>
#include "sblist.h"
#include "lexer.h"

struct list_item {
	enum lex_context type;
	size_t so, eo;
};

sblist *lex_and_transform(const char *re, const char *re_end);

#endif


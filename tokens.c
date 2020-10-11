#include "y.tab.h"
#include "tokens.h"
#include "lexer.h"
#include "sblist.h"
#include <stdio.h>
#include <assert.h>

static sblist *lex_to_list() {
	int c;
	size_t pos;
	struct list_item li;
	sblist *ret = sblist_new(sizeof li, 32);
	while((c = yylex()) != EOF) {
		enum lex_context ctx = lex_getcontext();
		pos = lex_getpos()-1;

		switch(ctx) {
		case CTX_DUP:
			do { c = yylex(); } while (lex_getcontext() == CTX_DUP);
			assert(c == '}');
			li.type = CTX_DUP;
			li.so = pos;
			li.eo = lex_getpos();
			sblist_add(ret, &li);
			break;
		case CTX_BRACKET:
			do { c = yylex(); } while (lex_getcontext() == CTX_BRACKET);
			assert(c == ']');
			li.type = CTX_BRACKET;
			li.so = pos;
			li.eo = lex_getpos();
			sblist_add(ret, &li);
			break;
		default:
			li.type = ctx;
			if (c == QUOTED_CHAR) {
				li.so = pos-1;
				li.eo = pos+1;
			} else  {
				li.so = pos;
				li.eo = pos+1;
			}
			sblist_add(ret, &li);
			break;
		}
	}
	return ret;
}

static void list_transform_dupchars(sblist* tokens, const char* org_regex) {
	size_t i;
	for(i=0; i<sblist_getsize(tokens); i++) {
		struct list_item *li= sblist_get(tokens, i);
		if(li->type == CTX_NONE) switch(org_regex[li->so]) {
			case '?': case '*': case '+':
				li->type = CTX_DUP;
				break;
		}
	}
}

static sblist* list_join_literals(sblist* tokens, const char* org_regex) {
	sblist *new = sblist_new(sizeof(struct list_item), sblist_getsize(tokens));
	size_t i,j;
	for(i=0; i<sblist_getsize(tokens); i++) {
		size_t pcnt = 0;
		for(j=i; j<sblist_getsize(tokens); ++j) {
			struct list_item *li= sblist_get(tokens, j);
			if(li->type != CTX_NONE) break;
			switch(org_regex[li->so]) {
			case '"':
			case '^':
			case '.':
			case '[':
			case '$':
			case '(':
			case ')':
			case '|':
			case '{':
				goto break_loop;
			default:
				pcnt += li->eo-li->so;
			}
			continue;
			break_loop:; break;
		}
		struct list_item ins = *((struct list_item *)sblist_get(tokens, i));
		if(j > i) {
			ins.type = 0xff;
			ins.eo = ins.so+pcnt;
			i = j-1;
		}
		sblist_add(new, &ins);
	}
	sblist_free(tokens);
	return new;
}

sblist *lex_and_transform(const char *re, const char *re_end) {
	lex_init(re, re_end, LEXFLAG_SILENT);
	sblist *tokens = lex_to_list();
	list_transform_dupchars(tokens, re);
	tokens = list_join_literals(tokens, re);
	return tokens;
}

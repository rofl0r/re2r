#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "y.tab.h"
#include "yydefs.h"
#include "lexer.h"
#include "sblist.h"
#include "hsearch.h"

extern int yyerror(const char*);

FILE *yyin;

extern int yyparse();
extern int yydebug;

static char* replace(const char*s, const char* needle, const char* repl) {
	char cp[4096+128];
	strcpy(cp, s);
	static char repl_buf[4096+128];
	char *p;
	while((p = strstr(cp, needle))) {
		snprintf(repl_buf, sizeof repl_buf, "%.*s%s%s",
			(int)(p-cp), cp, repl, p+strlen(needle));
		strcpy(cp, repl_buf);
	}
	return repl_buf;
}

struct list_item {
	enum lex_context type;
	size_t so, eo;
};

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

static void print_token(struct list_item *li, const char *org_regex) {
	if(li->type == 0xff) {
		printf(" \"%.*s\" ", (int) (li->eo-li->so), org_regex+li->so);
		return;
	} else if(li->type == CTX_BRACKET) {
		/* ragel doesn't like leading/trailing dash in bracket expression */
		if(org_regex[li->so+1] == '-') {
			printf("('-'|[%.*s)", (int) (li->eo-li->so-2), org_regex+li->so+2);
			return;
		} else if(org_regex[li->eo-2] == '-') {
			printf("('-'|%.*s])", (int) (li->eo-li->so-2), org_regex+li->so);
			return;
		}
	} else if(li->type == CTX_NONE && org_regex[li->so] == '"') {
		printf("'\"'");
		return;
	}
	printf("%.*s", (int) (li->eo-li->so), org_regex+li->so);
}

static int count_groups(sblist *tokens, const char* org_regex) {
	size_t i;
	int count = 0;
	for(i=0; i<sblist_getsize(tokens); ++i) {
		struct list_item *li = sblist_get(tokens, i);
		if(li->type == CTX_NONE && org_regex[li->so] == '(') ++count;
	}
	return count;
}

static void expand_groups(char *buf, int groups) {
	int i;
	char intbuf[16];
	for(i=0; i<groups; ++i) {
		snprintf(intbuf, sizeof intbuf, "%d", i);
		printf("%s", replace(buf, "%GROUPNR%", intbuf));
	}
}

static inline void* sblist_pop(sblist *l) {
	size_t len = sblist_getsize(l);
	if(len > 0) {
		void *x = sblist_get(l, len-1);
		sblist_delete(l, len-1);
		return x;
	}
	return 0;
}

static void dump_ragel_parser(const char *machinename, const char* org_regex, int *maxgroups) {
	FILE *f = fopen("ragel.tmpl", "r");
	char buf[4096];
	int groups, cgroup = 0;
	sblist *tokens = lex_to_list();
	list_transform_dupchars(tokens, org_regex);
	tokens = list_join_literals(tokens, org_regex);
	groups = count_groups(tokens, org_regex);
	if(groups > *maxgroups) *maxgroups = groups;
	sblist *group_order = sblist_new(sizeof (int), groups) ;

	while(fgets(buf, sizeof buf, f)) {
		char *p;
		if((p = strstr(buf, "%MACHINENAME%"))) {
			printf("%s", replace(buf, "%MACHINENAME%", machinename));
		} else if((p = strstr(buf, "%GROUPNR%"))) {
			expand_groups(buf, groups);
		} else if ((p = strstr(buf, "%MACHINEDEF%"))) {
			printf("%.*s", (int)(p-buf), buf);
			size_t i;
			/* insert group match actions */
			for(i=0; i<sblist_getsize(tokens); i++) {
				struct list_item *li= sblist_get(tokens, i);
				if(li->type == CTX_NONE && org_regex[li->so] == '(') {
					sblist_add(group_order, &cgroup);
					++cgroup;
					print_token(li, org_regex);
				} else if(li->type == CTX_NONE && org_regex[li->so] == ')') {
					struct list_item *next;
					int groupno = *((int*)sblist_pop(group_order));
					if(i+1 < sblist_getsize(tokens) && (next = sblist_get(tokens, i+1)) && next->type == CTX_DUP) {
						print_token(li, org_regex);
						print_token(next, org_regex);
						printf(" >A%d %%E%d ", groupno, groupno);
						++i;
					} else {
						print_token(li, org_regex);
						printf(" >A%d %%E%d ", groupno, groupno);
					}
				} else {
					print_token(li, org_regex);
				}
			}
			printf("%s", p+sizeof("%MACHINEDEF%")-1);
		} else {
			printf("%s", buf);
		}
	}
	fclose(f);
	sblist_free(group_order);
	sblist_free(tokens);
}

int main() {
#ifdef YYDEBUG
	yydebug = 1;
#endif
	char buf[4096];
	size_t lineno = 0;
	yyin = stdin;
	int maxgroups = 0;
	struct htab *remap = htab_create(32);
	while(fgets(buf, sizeof buf, yyin)) {
		++lineno;
		char* q = buf;
		while(!isspace(*q)) q++;
		if(*q == '\n') continue;
		*q = 0;
		const char *p = ++q, *pe = strrchr(p, '\n');
		if(!pe) pe = p + strlen(p);
		htab_value *v = htab_find(remap, (void*)p);
		if(v) {
			/* identical regex, already syntax-checked */
			printf("EXPORT int rematch_%s(const char *p, size_t nmatch, regmatch_t matches[])\n"
			       "{\n\treturn rematch_%s(p, nmatch, matches);\n}\n\n",
			       buf, (char*) v->p);
			continue;
		}
		lex_init(p, pe, LEXFLAG_SILENT);
		if(yyparse() == 0) {
			htab_insert(remap, strdup(p), HTV_P(strdup(buf)));
			/* syntax check OK */
			lex_init(p, pe, LEXFLAG_SILENT);
			dump_ragel_parser(buf, p, &maxgroups);
		} else {
			size_t errpos = lex_errpos();
			fprintf(stderr, "parse error @%zu:%zu\n", lineno, errpos);
			fprintf(stderr, "%.*s\n", (int)(pe-p), p);
			fprintf(stderr, "%*s^\n", (int)errpos, "");
		}
	}
}

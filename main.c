#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "y.tab.h"
#include "yydefs.h"
#include "lexer.h"
#include "sblist.h"
#include "hsearch.h"
#include "tokens.h"
#include "template.h"

extern int yyerror(const char*);

FILE *yyin, *yyout;

extern int yyparse();

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

static void print_token(struct list_item *li, const char *org_regex) {
	if(li->type == 0xff) {
		fprintf(yyout, " \"%.*s\" ", (int) (li->eo-li->so), org_regex+li->so);
		return;
	} else if(li->type == CTX_BRACKET) {
		/* ragel doesn't like leading/trailing dash in bracket expression */
		if(org_regex[li->so+1] == '-') {
			fprintf(yyout, "('-'|[%.*s)", (int) (li->eo-li->so-2), org_regex+li->so+2);
			return;
		} else if(org_regex[li->eo-2] == '-') {
			fprintf(yyout, "('-'|%.*s])", (int) (li->eo-li->so-2), org_regex+li->so);
			return;
		}
	} else if(li->type == CTX_NONE && org_regex[li->so] == '"') {
		fprintf(yyout, "'\"'");
		return;
	} else if(li->type == CTX_NONE && org_regex[li->so] == '.') {
		fprintf(yyout, " any ");
		return;
	}
	fprintf(yyout, "%.*s", (int) (li->eo-li->so), org_regex+li->so);
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
		snprintf(intbuf, sizeof intbuf, "%d", i+1);
		fprintf(yyout, "%s", replace(buf, "%GROUPNR%", intbuf));
	}
}

static void dump_parents(char *buf, sblist *tokens, const char* org_regex) {
	size_t i;
	int cgroup = 0;
	char parbuf[1024];
	strcpy(parbuf, "{[0]=0,");
	sblist *group_order = sblist_new(sizeof (int), 32) ;
	for(i=0; i<sblist_getsize(tokens); i++) {
		struct list_item *li= sblist_get(tokens, i);
		if(li->type == CTX_NONE && org_regex[li->so] == '(') {
			char gbuf[32];
			if(sblist_getsize(group_order))
				snprintf(gbuf, sizeof gbuf, "[%d]=%d,", cgroup+1, 1+*((int*)sblist_get(group_order, sblist_getsize(group_order)-1)));
			else
				snprintf(gbuf, sizeof gbuf, "[%d]=%d,", cgroup+1, 0);
			strcat(parbuf, gbuf);
			sblist_add(group_order, &cgroup);
			++cgroup;
		} else if(li->type == CTX_NONE && org_regex[li->so] == ')') {
			(void) sblist_pop(group_order);
		}
	}
	sblist_free(group_order);
	strcat(parbuf, "}");
	fprintf(yyout, "%s", replace(buf, "%PARENTARRAY%", parbuf));
}

static void dump_ragel_parser(FILE *f, const char *machinename, const char* org_regex, const char* org_regex_end, int *maxgroups) {
	char buf[4096];
	int groups, cgroup = 0;
	sblist *tokens = lex_and_transform(org_regex, org_regex_end);
	groups = count_groups(tokens, org_regex);
	if(groups > *maxgroups) *maxgroups = groups;
	sblist *group_order = sblist_new(sizeof (int), groups) ;

	while(fgets(buf, sizeof buf, f)) {
		char *p;
		if((p = strstr(buf, "%MACHINENAME%"))) {
			fprintf(yyout, "%s", replace(buf, "%MACHINENAME%", machinename));
		} else if((p = strstr(buf, "%GROUPNR%"))) {
			expand_groups(buf, groups);
		} else if((p = strstr(buf, "%PARENTARRAY%"))) {
			dump_parents(buf, tokens, org_regex);
		} else if ((p = strstr(buf, "%MACHINEDEF%"))) {
			fprintf(yyout, "%.*s", (int)(p-buf), buf);
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
						fprintf(yyout, " >A%d %%E%d ", groupno+1, groupno+1);
						++i;
					} else {
						print_token(li, org_regex);
						fprintf(yyout, " >A%d %%E%d ", groupno+1, groupno+1);
					}
				} else {
					print_token(li, org_regex);
				}
			}
			fprintf(yyout, "%s", p+sizeof("%MACHINEDEF%")-1);
		} else {
			fprintf(yyout, "%s", buf);
		}
	}
	sblist_free(group_order);
	sblist_free(tokens);
}

static int usage() {
	fprintf(stderr,
	        "NAME\n"
	        "\tre2r - POSIX ERE to ragel converter (C) rofl0r\n\n"
	        "SYNOPSIS\n"
	        "\tre2r [OPTIONS]\n\n"
	        "DESCRIPTION\n"
	        "\treads input from stdin, transforms ERE into a ragel machine,\n"
	        "\temitting a function with a regexec() like signature.\n"
	        "\teach input line shall consist of a machine name followed by a regex.\n\n"
	        "EXAMPLES\n"
	        "\tipv4 [0-9]+[.][0-9]+[.][0-9]+[.][0-9]+\n\n"
	        "OPTIONS\n"
	        "\t-t template.txt: use template.txt instead of builtin (ragel.tmpl)\n"
	        "\t-o outfile: write output to outfile instead of stdout\n"
		"\n"
	        "BUGS\n"
	        "\tPOSIX collation, equivalence, and character classes support is not implemented\n"
	        "\tyou can replace character classes like [[:digit:]] with [0-9] using some sort\n"
	        "\tof preprocessor. see ere.y for more details.\n"
	);
	return 1;
}

int main(int argc, char**argv) {
#if defined(YYDEBUG) && YYDEBUG
	extern int yydebug;
	yydebug = 1;
#endif
	char buf[4096], *template = 0;
	size_t lineno = 0;
	yyin = stdin;
	yyout = stdout;
	int maxgroups = 0, err = 0, c;
	struct htab *remap = htab_create(32);
	while((c = getopt(argc, argv, "t:o:")) != EOF) switch(c) {
	case 'o':
		yyout = fopen(optarg, "w");
		if(!yyout) {
			perror("open");
			return 1;
		}
		break;
	case 't':
		template = optarg;
		break;
	default:
		return usage();
	}
	fprintf(yyout, "/* automatically generated with re2r by rofl0r */\n");
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
			fprintf(yyout,
			        "RE2R_EXPORT int re2r_match_%s(const char *p, const char* pe, size_t nmatch, regmatch_t matches[])\n"
			        "{\n\treturn re2r_match_%s(p, pe, nmatch, matches);\n}\n\n",
			        buf, (char*) v->p);
			continue;
		}
		lex_init(p, pe, LEXFLAG_SILENT);
		if(yyparse() == 0) {
			/* syntax check OK */
			htab_insert(remap, strdup(p), HTV_P(strdup(buf)));
			FILE *f;
			if(template) f = fopen(template, "r");
			else f = fmemopen((void*)ragel_tmpl.data, ragel_tmpl.ulen, "r");
			dump_ragel_parser(f, buf, p, pe, &maxgroups);
			fclose(f);
		} else {
			++err;
			size_t errpos = lex_errpos();
			fprintf(stderr, "parse error @%zu:%zu\n", lineno, errpos);
			fprintf(stderr, "%.*s\n", (int)(pe-p), p);
			fprintf(stderr, "%*s^\n", (int)errpos, "");
		}
	}
	fprintf(stderr, "diagnostics: maximum number of match groups: %d\n", maxgroups+1);
	return !!err;
}

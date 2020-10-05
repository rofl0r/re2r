#if 0

https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap09.html#tag_09

COLL_ELEM_SINGLE
    Any single-character collating element, unless it is a META_CHAR.

COLL_ELEM_MULTI
    Any multi-character collating element.

DUP_COUNT
    Represents a numeric constant. It shall be an integer in the range
    0 <= DUP_COUNT <= {RE_DUP_MAX}.
    This token is only recognized when the context of the grammar requires it.
    At all other times, digits not preceded by a <backslash> character are
    treated as ORD_CHAR.

META_CHAR
    One of the characters:

    ^
        When found first in a bracket expression
    -
        When found anywhere but first (after an initial '^', if any)
        or last in a bracket expression, or as the ending range point
        in a range expression
    ]
        When found anywhere but first (after an initial '^', if any)
        in a bracket expression

SPEC_CHAR
For extended regular expressions, shall be one of the following special
characters found anywhere outside bracket expressions:

^    .    [    $    (    )    |
*    +    ?    {    \

(The close-parenthesis shall be considered special in this context only if
 matched with a preceding open-parenthesis.)

QUOTED_CHAR
In an ERE, one of the character sequences:

\^    \.    \[    \$    \(    \)    \|
\*    \+    \?    \{    \\

ORD_CHAR
    A character, other than one of the special characters in SPEC_CHAR.


\ -> used to quote one of quoted_char? QUOTED_CHAR :
     outside bracket exp ? SPEC_CHAR :
     ORD_CHAR

#endif

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "y.tab.h"

#ifndef YYSTYPE
#define YYSTYPE int
#endif
#ifndef YYDEBUG
#define YYDEBUG 0
#endif

extern int yyerror(const char*);
extern YYSTYPE yylval;
extern FILE* yyin;

enum context {
	CTX_NONE=0,
	CTX_DUP,
	CTX_BRACKET,
};

struct lex_state {
	enum context ctx;
	size_t line_pos;
	size_t brack_pos;
	int brack_neg;
	unsigned open_parens;
	const char *p, *ps, *pe;
};

static struct lex_state lex_state;

int yyerror(const char* s) {
	fprintf(stderr, "%s @%zu\n", s, lex_state.line_pos);
	return 1;
}

void lex_init(const char *p, const char *pe) {
	memset(&lex_state, 0, sizeof(lex_state));
	lex_state.p = lex_state.ps = p;
	lex_state.pe = pe;
}

static int ctxup(enum context newctx) {
	if(newctx != CTX_NONE && lex_state.ctx != CTX_NONE) {
		yyerror("invalid context switch");
		return 0;
	}
	lex_state.ctx = newctx;
	if(newctx == CTX_BRACKET) {
		lex_state.brack_pos = 0;
		lex_state.brack_neg = 0;
	}
	return 1;
}

static int mygetc(void) {
	if(lex_state.p == lex_state.pe) return EOF;
	lex_state.line_pos++;
	return *(lex_state.p++);
}

static int myungetc(int c) {
	(void) c;
	if(lex_state.p == lex_state.ps) return EOF;
	lex_state.line_pos--;
	return *(lex_state.p--);
}

#define GETC(...) mygetc()
#define UNGETC(...) myungetc(0)

#define CTX lex_state.ctx

int yylex() {
	int ch = GETC(yyin), ch2;
	if(YYDEBUG) fprintf(stderr, "yylex: %c\n", ch);
	yylval = ch;
	if(CTX == CTX_BRACKET) ++lex_state.brack_pos;
	switch(ch) {
	case EOF:
		return EOF;
	case '\\':
		ch2 = GETC(yyin);
		switch(ch2) {
		case '\\':
		case '^':
		case '.':
		case '[':
		case '$':
		case '(':
		case ')':
		case '|':
		case '*':
		case '+':
		case '?':
		case '{':
			yylval = ch2;
			return QUOTED_CHAR;
		default:
			UNGETC(ch2, yyin);
			if(CTX != CTX_BRACKET) return '\\'; /*SPEC_CHAR;*/
			return ORD_CHAR;
		}
	case ')':
		if(!lex_state.open_parens) return ORD_CHAR;
		else --lex_state.open_parens;
		/* fall-through */
	case '(':
		if(ch == '(') ++lex_state.open_parens;
		/* fall-through */
	case '^':
	case '.':
	case '[':
	case '$':
	case '|':
	case '*':
	case '+':
	case '?':
	case '{':
		if(CTX != CTX_BRACKET) {
			switch(ch) {
			case '{': ctxup(CTX_DUP); break;
			case '[': ctxup(CTX_BRACKET); break;
			}
			return ch; /*SPEC_CHAR*/;
		} else {
			if(ch == '^' && lex_state.brack_pos == 1) {
				lex_state.brack_neg = 1;
				return ch; /* META_CHAR */
			}
		}
		return ORD_CHAR;
	case ']':
		if(CTX == CTX_BRACKET && lex_state.brack_pos-lex_state.brack_neg!=1) {
			ctxup(CTX_NONE);
			return ch; /* META_CHAR */
		}
		return ORD_CHAR;
	case '-':
		/* we don't support '-' as the ending point in a range expression like '#--' */
		if(CTX == CTX_BRACKET) {
			if(lex_state.brack_pos-lex_state.brack_neg==1)
				return ORD_CHAR;
			ch2 = GETC(yyin);
			UNGETC(ch2, yyin);
			if(ch2 == ']') return ORD_CHAR;
			return ch; /* META_CHAR */
		}
		return ORD_CHAR;
	case '}':
		if(CTX == CTX_DUP) {
			ctxup(CTX_NONE);
			/* even though not described in the spec, we need
			   to return this like a SPEC_CHAR so the parser can
			   detect the end of a DUP expression. */
			return ch; /* SPEC_CHAR */
		}
		return ORD_CHAR;
	case ',':
		if(CTX == CTX_DUP)
			return ch; /* likewise, SPEC_CHAR*/
		return ORD_CHAR;
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		if(CTX == CTX_DUP) {
			unsigned n = ch - '0';
			while(isdigit((ch = GETC(yyin))))
				n = n*10 + (ch - '0');
			UNGETC(ch, yyin);
			yylval = n;
			return DUP_COUNT;
		}
		/* fall-through */
	default:
		return ORD_CHAR;
	}
}

/* (C) 2019 rofl0r */
/* released into the public domain, or at your choice 0BSD or WTFPL */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

static int usage() {
	printf( "file2hdr - converts a file into a C string\n"
		"the output is written to stdout.\n"
		"usage: file2hdr filename\n"
		);
	return 1;
}

static int isprintable(int ch) {
#define PRINTABLE " ;,:.^-+=*/%|&[](){}<>#_"
	return ch > 0 && (isdigit(ch) || isalpha(ch) || strchr(PRINTABLE, ch));
}

static int escape_char(int ch, char buf[5], int *dirty) {
	int len = 0;
	*dirty = 0;
	buf[len++] = '\\';
	switch(ch) {
		case '\a': /* 0x07 */
			buf[len++] = 'a'; break;
		case '\b': /* 0x08 */
			buf[len++] = 'b'; break;
		case '\t': /* 0x09 */
			buf[len++] = 't'; break;
		case '\n': /* 0x0a */
			buf[len++] = 'n'; break;
		case '\v': /* 0x0b */
			buf[len++] = 'v'; break;
		case '\f': /* 0x0c */
			buf[len++] = 'f'; break;
		case '\r': /* 0x0d */
			buf[len++] = 'r'; break;
		case '\"': /* 0x22 */
			buf[len++] = '\"'; break;
		case '\'': /* 0x27 */
			buf[len++] = '\''; break;
		case '\?': /* 0x3f */
			buf[len++] = '\?'; break;
		case '\\': /* 0x5c */
			buf[len++] = '\\'; break;
		default:
			if(isprintable(ch)) buf[0] = ch;
			else {
				if (ch <= 077) *dirty = 1;
				len += sprintf(buf+len, "%o", ch);
			}
	}
	buf[len] = 0;
	return len;
}

int main(int argc, char** argv) {
	int f_arg = 1;
	unsigned cpl = 77;
	if(argc-1 < f_arg) return usage();
	FILE *f = fopen(argv[f_arg], "r");
	if(!f) { perror("fopen"); return 1; }
	{
		off_t end, start = ftello(f);
		size_t insize, outsize;
		fseeko(f, 0, SEEK_END);
		end = ftello(f);
		fseeko(f, start, SEEK_SET);
		insize = end - start;
		outsize = insize;

		char *p = strrchr(argv[f_arg], '/');
		if(!p) p = argv[f_arg];
		else p++;
		char *fn = strdup(p);
		p = fn;
		while(*p) { if(*p == '.') *p = '_'; ++p; }
		printf( "const struct { unsigned clen, ulen;\n"
			"const unsigned char data[]; } %s = { %zu, %zu,\n",
			fn, outsize, insize);
		free(fn);
	}

	int ch, dirty;
	unsigned cnt = 0;
	while((ch = fgetc(f)) != EOF) {
		if(cnt == 0) { printf("\""); dirty = 0; }
		char buf[5];
		int cdirty;
		cnt += escape_char(ch, buf, &cdirty);
		// dirty and cdirty: ok
		// dirty and not cdirty: ok if buf[0] not in '0'..'7'
		if(dirty && !cdirty && buf[0] >= '0' && buf[0] <= '7') {
			printf("\"\"");
			cnt += 2;
		}
		dirty = cdirty;
		printf("%s", buf);
		if(cnt >= cpl) {
			printf("\"\n");
			cnt = 0;
		}
	}
	if(cnt) printf("\"\n");
	printf("};\n");
	fclose(f);
	return 0;
}


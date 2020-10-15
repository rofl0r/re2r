#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#include <assert.h>
#include "sblist.h"
#include "tokens.h"


char* display_match_re(const char* s, regmatch_t matches[], int mno) {
	static char buf[1024];
	snprintf(buf, sizeof buf, "%.*s", (int)(matches[mno].rm_eo - matches[mno].rm_so), s+matches[mno].rm_so);
	return buf;
}

void visualize_regex(const char* regex, int nmatch, regmatch_t matches[], int onlymatches) {
	sblist *tokens = lex_and_transform(regex, regex+strlen(regex));
	sblist *group_order = sblist_new(sizeof (int), nmatch) ;
	int cgroup = 0;
	size_t i;
	int spacediff = 0;
	for(i=0; i<sblist_getsize(tokens); i++) {
		struct list_item *li= sblist_get(tokens, i);
		if(li->type == CTX_NONE && regex[li->so] == '(') {
			sblist_add(group_order, &cgroup);
			if(!onlymatches || matches[cgroup+1].rm_so != -1) printf("%d", cgroup+1);
			else printf(" ");
			int t = cgroup+1;
			while(t >= 10) { spacediff++; t/=10; }
			++cgroup;
		} else if(li->type == CTX_NONE && regex[li->so] == ')') {
			int groupno = *((int*)sblist_pop(group_order));
			if(!onlymatches || matches[groupno+1].rm_so != -1) printf("%d", groupno+1);
			else printf(" ");
			int t = groupno+1;
			while(t >= 10) { spacediff++; t/=10; }
		} else {
			int n = li->eo - li->so - spacediff;
			if(n < 0) spacediff = n;
			else {
				printf("%*s", n, "");
				spacediff = 0;
			}
		}
	}
	puts("");
}

static int usage() {
	fprintf(stderr,
		"usage: re2r_test [OPTIONS] -r regex\n\n"
		"compiles regex with re2r and ragel, then waits for input on stdin\n"
		"and matches the input with both the compiled re2r regex and posix regexec()\n"
		"and shows some diagnostics\n"
		"in order to do its job re2r_test needs to be dynamically linked and a\n"
		"C compiler as well as ragel need to be installed.\n\n"
		"OPTIONS:\n"
		"-k : keep temporary files\n"
		"\n"
		"supported environment vars:\n"
		"CC - C compiler\n"
		"RAGEL - ragel binary\n"
		"RE2R - re2r binary\n"
	);
	return 1;
}

#include <sys/wait.h>
static int run_command(const char* cmd, const char* inp) {
	FILE *fp;
	int status;

	fp = popen(cmd, "w");
	if(fp == 0) {
		perror("popen");
		return 1;
	}

	if(inp && inp[0]) fprintf(fp, "%s", inp);

	status = pclose(fp);
	if(status == -1) {
		perror("pclose");
		return 1;
	}
	if(!WIFEXITED(status)) {
		fprintf(stderr, "opened process returned error\n");
		return 1;
	}
	return 0;
}

#include <dlfcn.h>

typedef int (*rematch_func)(const char *, const char*, size_t, regmatch_t[]);

int main(int argc, char** argv) {
	const char *cc = getenv("CC");
	if(!cc) cc = "cc";
	const char* ragel = getenv("RAGEL");
	if(!ragel) ragel = "ragel";
	const char* re2r = getenv("RE2R");
	if(!re2r) re2r = "./re2r";
	const char *regex = 0;

	int c, kflag=0;
	while((c = getopt(argc, argv, "r:k")) != EOF) switch(c) {
	case 'r':
		regex = optarg;
		break;
	case 'k':
		kflag = 1;
		break;
	default:
		return usage();
	}

	if(!regex) return usage();
	char re2r_cmd[1024];
	char ragel_cmd[1024];
	char cc_cmd[1024];
	const char *tmp = tmpnam(0);
	snprintf(re2r_cmd, sizeof re2r_cmd, "%s -o %s.rl", re2r, tmp);
	snprintf(ragel_cmd, sizeof ragel_cmd, "%s -o %s.c %s.rl", ragel, tmp, tmp);
	snprintf(cc_cmd, sizeof re2r_cmd,
		"%s -include regex.h -DRE2R_EXPORT=extern -fPIC -shared -o %s.so %s.c", cc, tmp, tmp);

	char buf[4096];
	snprintf(buf, sizeof buf, "regex %s\n", regex);
	if(run_command(re2r_cmd, buf)) return 1;
	if(run_command(ragel_cmd, "")) return 1;
	if(run_command(cc_cmd, "")) return 1;

	snprintf(buf, sizeof buf, "%s.so", tmp);
	void *so = dlopen(buf, RTLD_NOW);
	if(!so) {
		fprintf(stderr, "failed to open .so\n");
		return 1;
	}

	if(!kflag) {
		snprintf(buf, sizeof buf, "%s.rl", tmp);
		unlink(buf);
		snprintf(buf, sizeof buf, "%s.c", tmp);
		unlink(buf);
		snprintf(buf, sizeof buf, "%s.so", tmp);
		unlink(buf);
	} else {
		printf("keeping temporary files %s.*\n", tmp);
	}

	rematch_func matchfn = dlsym(so, "rematch_regex");
	if(!matchfn) {
		fprintf(stderr, "match func not found\n");
		return 1;
	}

	int ret = 0;
	while(fgets(buf, sizeof buf, stdin)) {
		char *expr = buf, *p = strchr(expr, '\n');
		if(p) *p = 0;
		else p = expr+strlen(expr);
		regmatch_t match[256];
		int r = matchfn(expr, p, 256, match);
		if(r != 0) {
			fprintf(stderr, "re2r match func failed\n");
			ret++;
		} else {
			printf("---------- RE2R  ----------\n");
			for(int i=0; i< 256; ++i)
				if(match[i].rm_so != -1) printf("%d: %s\n", i, display_match_re(expr, match, i));
			printf("%s\n", regex);
			visualize_regex(regex, 256, match, 0);
			visualize_regex(regex, 256, match, 1);
		}

		regex_t creb, *cre = &creb;
		r = regcomp (cre, regex, REG_EXTENDED | REG_NEWLINE);
		if(r != 0) {
			ret++;
			fprintf(stderr, "regcomp() failed\n");
			continue;
		}
		r = regexec (cre, buf, 256, match, 0);
		regfree(cre);

		if(r != 0) {
			ret++;
			fprintf(stderr, "regexec() failed\n");
			continue;
		}

		printf("---------- POSIX ----------\n");

		for(int i=0; i< 256; ++i)
			if(match[i].rm_so != -1) printf("%d: %s\n", i, display_match_re(expr, match, i));

		printf("%s\n", regex);
		visualize_regex(regex, 256, match, 0);
		visualize_regex(regex, 256, match, 1);
	}
	return !!ret;
}

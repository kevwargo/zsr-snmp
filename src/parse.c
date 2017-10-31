#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pcre.h>
#include "parse.h"
#include "dllist.h"
#include "utils.h"


void regexp_scan(const char *subject, struct dllist *token_specs, token_handler_t handler, void *data)
{
    int token_count = token_specs->size;
    struct token *tokens = (struct token *)malloc(sizeof(struct token) * token_count);
    int i = 0;
    struct token_spec *ts;
    dllist_foreach(ts, token_specs) {
        const char *error;
        int erroffset;
        tokens[i].spec = ts;
        tokens[i].re = pcre_compile(ts->re, PCRE_ANCHORED, &error, &erroffset, NULL);
        if (! tokens[i].re) {
            fprintf(stderr, "pcre_compile error: %s\n", error);
            exit(1);
        }
        int rc = pcre_fullinfo(tokens[i].re, NULL, PCRE_INFO_CAPTURECOUNT, &tokens[i].ovecsize);
        if (rc < 0) {
            fprintf(stderr, "pcre_fullinfo error code: %d\n", rc);
            exit(1);
        }
        tokens[i].ovecsize++;
        tokens[i].ovecsize *= 3;
        tokens[i].ovector = (int *)malloc(sizeof(int) * tokens[i].ovecsize);
        i++;
    }

    int len = strlen(subject);
    int pos = 0;
    int finished = 0;
    while (! finished) {
        int matched = 0;
        for (int i = 0; i < token_count && !matched; i++) {
            int rc = pcre_exec(tokens[i].re, NULL, subject + pos, len, 0, 0, tokens[i].ovector, tokens[i].ovecsize);
            if (rc == PCRE_ERROR_NOMATCH) {
                continue;
            }
            if (rc < 0) {
                fprintf(stderr, "pcre_exec error code: %d\n", rc);
                exit(1);
            }
            matched = 1;
            tokens[i].subject = subject + pos;
            pcre_get_substring(subject + pos, tokens[i].ovector, rc, 0, &tokens[i].match);
            if (handler(&tokens[i], data)) {
                finished = 1;
            }
            pcre_free_substring(tokens[i].match);
            pos += tokens[i].ovector[1];
            len -= tokens[i].ovector[1];
        }
        if (! matched) {
            fprintf(stderr, "None of the provided tokens were found\n");
            exit(1);
        }
    }
}

int __attribute__((__unused__)) re_test(int argc, const char **argv)
{
    const char *error;
    int erroffset;
    pcre *re = pcre_compile(argv[1], PCRE_ANCHORED, &error, &erroffset, NULL);
    const char *match;
    int ovector[6];
    printf("%d\n", pcre_exec(re, NULL, argv[2], strlen(argv[2]), atoi(argv[3]), 0, ovector, 6));
    pcre_get_substring(argv[2], ovector, 2, 0, &match);
    printf("match: %s\n", match);
    return 0;
}



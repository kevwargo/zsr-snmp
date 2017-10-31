#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pcre.h>
#include "mibtree.h"
#include "parse.h"

static int fill_mib(struct oid *target, const char *name, const char *all, const char *last)
{
    printf("name: %s, all: %s, last: %s\n", name, all, last);
    return 0;
}

int parse_oid(const char *content, char *name, struct oid *target)
{
    const char *error;
    int erroffset;
    char *oid_re = NULL;
    int *ovector;
    int ovecsize;

    int rc = asprintf(&oid_re, "^[ \\t]*(%s)%s+%s%s*::=%s*{%s*(%s%s)%s*}",
            name,
            "[ \\t\\n\\r]",
            "OBJECT[ \\t]+IDENTIFIER",
            "[ \\t\\n\\r]",
            "[ \\t\\n\\r]",
            "[ \\t\\n\\r]",
            "[a-zA-Z_-][a-zA-Z0-9_-]*",
            "([ \\t\\n\\r]+([a-zA-Z_-][a-zA-Z0-9_-]*|[0-9]+))+",
            "[ \\t\\n\\r]"
        );
    if (rc < 0) {
        return rc;
    }
    pcre *re = pcre_compile((const char *)oid_re, PCRE_MULTILINE, &error, &erroffset, NULL);
    if (! re) {
        fputs(oid_re, stderr);
        for (int i = 0; i < erroffset; i++) {
            putc(' ', stderr);
        }
        fputs("^\n", stderr);
        fprintf(stderr, "pcre_compile error `%s' at %d\n", error, erroffset);
        return -1;
    }

    rc = pcre_fullinfo(re, NULL, PCRE_INFO_CAPTURECOUNT, &ovecsize);
    if (rc < 0) {
        return rc;
    }
    ovecsize++;
    ovecsize *= 3;
    ovector = (int *)malloc(sizeof(int) * ovecsize);
    printf("ovecsize: %d\n", ovecsize);

    int len = strlen(content);
    int offset = 0;
    int matched = 0;

    while (offset < len) {
        int capture = pcre_exec(re, NULL, content, len, offset, 0, ovector, ovecsize);
        if (capture == PCRE_ERROR_NOMATCH) {
            break;
        }
        if (capture < 0) {
            printf("pcre_exec: %s\n", pcre_strerror(rc));
            return rc;
        }

        matched = 1;
        offset = ovector[1];

        const char *objname;
        const char *all;
        const char *last;
        rc = pcre_get_substring(content, ovector, capture, 1, &objname);
        if (rc < 0) {
            fprintf(stderr, "pcre_get_substring %d error: %s\n", 1, pcre_strerror(rc));
            return rc;
        }
        rc = pcre_get_substring(content, ovector, capture, 2, &all);
        if (rc < 0) {
            pcre_free_substring(objname);
            fprintf(stderr, "pcre_get_substring %d error: %s\n", 2, pcre_strerror(rc));
            return rc;
        }
        rc = pcre_get_substring(content, ovector, capture, 4, &last);
        if (rc < 0) {
            pcre_free_substring(objname);
            pcre_free_substring(all);
            fprintf(stderr, "pcre_get_substring %d error: %s\n", 4, pcre_strerror(rc));
            return rc;
        }

        rc = fill_mib(target, objname, all, last);

        pcre_free_substring(objname);
        pcre_free_substring(all);
        pcre_free_substring(last);
        if (rc < 0) {
            return rc;
        }
    }

    if (! matched) {
        fprintf(stderr, "Object %s not found\n", name);
    }

    return 0;
}

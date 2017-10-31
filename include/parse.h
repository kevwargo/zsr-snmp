#ifndef __PARSE_H_INCLUDED
#define __PARSE_H_INCLUDED

#include <pcre.h>
#include "dllist.h"

struct token_spec {
    int type;
    char *re;
};

struct token {
    struct token_spec *spec;
    pcre *re;
    const char *subject;
    const char *match;
    int *ovector;
    int ovecsize;
};


typedef int (*token_handler_t)(struct token *tok, void *data);

extern char *pcre_strerror(int code);
extern void regexp_scan(const char *subject, struct dllist *token_specs, token_handler_t handler, void *data);

#endif

#ifndef __REGEX_H_INCLUDED
#define __REGEX_H_INCLUDED

#include <pcre.h>
#include "dllist.h"

#define REGEX_PARSE_ERROR -200
#define REGEX_PARSE_UNEXPECTED_TOKEN_ERROR -201
#define REGEX_PARSE_UNEXPECTED_TOKEN_GET_ERROR -202
#define REGEX_PARSE_START_TOKEN_NOT_FOUND_ERROR -203


struct regex {
    pcre *re;
    pcre_extra *extra;
    int *ovector;
    int ovecsize;
};

struct token {
    char *pattern;
    char *subject;
    struct regex *regex;
};

typedef int (*token_handler_t)(struct token *token, int token_num, int *stateptr, void *data, char **errorptr);

struct parser {
    char *start_pattern;
    token_handler_t start_token_handler;
    int state;
    int token_count;
    int state_count;
    token_handler_t **handlers;
};

extern char *pcre_strerror(int code);

extern struct regex *regex_prepare(char *pattern, char **errorptr);
extern void regex_free(struct regex *regex);

extern int regex_match(struct regex *regex, char *subject, int length, int startoffset, int options, char **errorptr);
extern char *regex_get_match(struct regex *regex, char *subject, int group, char **errorptr);

extern token_handler_t **init_handlers(int token_count, int state_count);
extern int regex_parse(struct parser *parser, char *subject, char **tokens_re, void *data, char **errorptr);

extern int ignore_token_handler(struct token *token, int token_num, int *stateptr, void *data, char **errorptr);

extern char *remove_comments(char *subject);

#endif

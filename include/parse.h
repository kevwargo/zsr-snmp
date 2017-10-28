#ifndef __PARSE_H_INCLUDED
#define __PARSE_H_INCLUDED


enum token_type {
    IDENTIFIER,
    COMMA,
    KW_FROM,
    SEMICOLON,
    WHITESPACE
};

struct token_spec {
    enum token_type type;
    char *re;
};

struct token {
    struct token_spec *spec;
    pcre *re;
    char *subject;
    char *match;
    int *ovector;
    int ovecsize;
};

enum imports_state {
    BEFORE_DEFINITION,
    AFTER_DEFINITION,
    BEFORE_FILENAME,
    AFTER_FILENAME
};


typedef int (*token_handler_t)(struct token *tok, void *data);


#endif

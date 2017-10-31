#ifndef __IMPORTS_H_INCLUDED_
#define __IMPORTS_H_INCLUDED_

#include "dllist.h"

enum imports_token_type {
    IDENTIFIER,
    COMMA,
    KW_FROM,
    SEMICOLON,
    WHITESPACE
};

enum imports_state {
    BEFORE_DEFINITION,
    AFTER_DEFINITION,
    BEFORE_FILENAME,
    AFTER_FILENAME
};

struct imports_file_entry {
    char *name;
    struct dllist *definitions;
};

struct imports {
    enum imports_state state;
    struct dllist *files;
    struct imports_file_entry *current_file;
};

extern struct imports *parse_imports(char *filename);

#endif

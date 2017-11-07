#ifndef __IMPORTS_H_INCLUDED_
#define __IMPORTS_H_INCLUDED_

#include "dllist.h"

enum imports_token_type {
    IDENTIFIER = 0,
    IDENTIFIER_WITH_FILENAME,
    SEMICOLON
};

enum imports_state {
    STATE_IMPORTS_MAIN = 0
};

struct imports_file_entry {
    char *name;
    struct dllist *definitions;
};

struct imports {
    struct dllist *files;
    struct imports_file_entry *current_file;
};

extern struct imports *parse_imports(char *content, char **errorptr);
extern void free_imports(struct imports *imports);

#endif

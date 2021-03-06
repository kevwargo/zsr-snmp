#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pcre.h>
#include "utils.h"
#include "regex.h"
#include "mibtree.h"
#include "imports.h"
#include "dllist.h"

static int handle_symbol(struct token *token, int *stateptr, void *data, char **errorptr)
{
    struct imports *imports = (struct imports *)data;
    if (! imports->current_file->definitions) {
        imports->current_file->definitions = dllist_create();
    }
    char *definition = regex_get_match(token->regex, token->subject, 1, errorptr);
    if (! definition) {
        return -1;
    }
    /* printf("definition '%s'\n", definition); */
    dllist_append(imports->current_file->definitions, &definition);
    return 0;
}

static int handle_filename(struct token *token, int *stateptr, void *data, char **errorptr)
{
    struct imports *imports = (struct imports *)data;
    if (! imports->current_file->definitions) {
        imports->current_file->definitions = dllist_create();
    }
    char *definition = regex_get_match(token->regex, token->subject, 1, errorptr);
    if (! definition) {
        return -1;
    }
    char *filename = regex_get_match(token->regex, token->subject, 2, errorptr);
    if (! filename) {
        pcre_free_substring(definition);
        return -1;
    }
    char *endptr = regex_get_match(token->regex, token->subject, 3, errorptr);
    if (! endptr) {
        pcre_free_substring(definition);
        pcre_free_substring(filename);
        return -1;
    }
    /* printf("last def '%s', file '%s'\n", definition, filename); */
    dllist_append(imports->current_file->definitions, &definition);
    imports->current_file->name = filename;
    dllist_append(imports->files, &imports->current_file);
    imports->current_file = (struct imports_file_entry *)xcalloc(1, sizeof(struct imports_file_entry));
    char end = *endptr;
    pcre_free_substring(endptr);
    if (end == ';') {
        return 1;
    }
    return 0;
}

static int handle_semicolon(struct token *token, int *stateptr, void *data, char **errorptr)
{
    return 1;
}

struct imports *parse_imports(char *content, char **errorptr)
{
    int state_count = 1;

    struct parser parser;
    parser.start_pattern = "^\\s*IMPORTS\\s";
    parser.start_token_handler = NULL;
    parser.state = STATE_IMPORTS_MAIN;
    parser.state_count = state_count;
    parser.states = init_states(state_count);

    struct token token;
    token.pattern = "[ \\t\\n\\r]*([a-zA-Z_][a-zA-Z0-9_-]*)[ \\t\\n\\r]*,";
    token.handler = handle_symbol;
    dllist_append(parser.states[STATE_IMPORTS_MAIN], &token);
    token.pattern = "[ \\t\\n\\r]*([a-zA-Z_][a-zA-Z0-9_-]*)[ \\t\\n\\r]*FROM[ \\t\\n\\r]*([a-zA-Z_][a-zA-Z0-9_-]*)([ \\t\\n\\r;])";
    token.handler = handle_filename;
    dllist_append(parser.states[STATE_IMPORTS_MAIN], &token);
    token.pattern = "[ \\t\\n\\r]*;";
    token.handler = handle_semicolon;
    dllist_append(parser.states[STATE_IMPORTS_MAIN], &token);

    struct imports *imports = (struct imports *)xmalloc(sizeof(struct imports));
    imports->files = dllist_create();
    imports->current_file = (struct imports_file_entry *)xcalloc(1, sizeof(struct imports_file_entry));

    int rc = regex_parse(&parser, content, imports, errorptr);
    if (rc < 0 && rc != REGEX_PARSE_START_TOKEN_NOT_FOUND_ERROR) {
        free_imports(imports);
        return NULL;
    }
    return imports;
}

void free_imports(struct imports *imports)
{
    if (imports->files) {
        struct imports_file_entry **fileptr;
        dllist_foreach(fileptr, imports->files) {
            pcre_free_substring((*fileptr)->name);
            if ((*fileptr)->definitions) {
                char **defptr;
                dllist_foreach(defptr, (*fileptr)->definitions) {
                    pcre_free_substring(*defptr);
                }
                dllist_destroy((*fileptr)->definitions);
            }
        }
        dllist_destroy(imports->files);
    }
    if (imports->current_file) {
        if (imports->current_file->name) {
            pcre_free_substring(imports->current_file->name);
        }
        if (imports->current_file->definitions) {
            char **defptr;
            dllist_foreach(defptr, imports->current_file->definitions) {
                pcre_free_substring(*defptr);
            }
            dllist_destroy(imports->current_file->definitions);
        }
    }
    free(imports);
}


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pcre.h>
#include "utils.h"
#include "regex.h"
#include "mibtree.h"
#include "imports.h"
#include "dllist.h"

static int handle_symbol(struct token *token, int token_num, int *stateptr, void *data, char **errorptr)
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

static int handle_filename(struct token *token, int token_num, int *stateptr, void *data, char **errorptr)
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
    imports->current_file = (struct imports_file_entry *)calloc(1, sizeof(struct imports_file_entry));
    char end = *endptr;
    pcre_free_substring(endptr);
    if (end == ';') {
        return 1;
    }
    return 0;
}

static int handle_semicolon(struct token *token, int token_num, int *stateptr, void *data, char **errorptr)
{
    return 1;
}

struct imports *parse_imports(char *content, char **errorptr)
{
    int token_count = 3;
    int state_count = 1;

    char **tokens = (char **)malloc(token_count * sizeof(char *));
    tokens[IDENTIFIER] = "[ \\t\\n\\r]*([a-zA-Z_][a-zA-Z0-9_-]*)[ \\t\\n\\r]*,";
    tokens[IDENTIFIER_WITH_FILENAME] = "[ \\t\\n\\r]*([a-zA-Z_][a-zA-Z0-9_-]*)[ \\t\\n\\r]*FROM[ \\t\\n\\r]*([a-zA-Z_][a-zA-Z0-9_-]*)([ \\t\\n\\r;])";
    tokens[SEMICOLON] = "[ \\t\\n\\r]*;";

    token_handler_t **handlers = init_handlers(token_count, state_count);
    handlers[IDENTIFIER][STATE_IMPORTS_MAIN] = handle_symbol;
    handlers[IDENTIFIER_WITH_FILENAME][STATE_IMPORTS_MAIN] = handle_filename;
    handlers[SEMICOLON][STATE_IMPORTS_MAIN] = handle_semicolon;

    struct parser parser;
    parser.start_pattern = "^\\s*IMPORTS\\s";
    parser.start_token_handler = NULL;
    parser.state = STATE_IMPORTS_MAIN;
    parser.token_count = token_count;
    parser.state_count = state_count;
    parser.handlers = handlers;

    struct imports *imports = (struct imports *)malloc(sizeof(struct imports));
    imports->files = dllist_create();
    imports->current_file = (struct imports_file_entry *)calloc(1, sizeof(struct imports_file_entry));
    
    if (regex_parse(&parser, content, tokens, imports, errorptr) < 0) {
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

static int import_from_string(char *content, struct oid *mib, char *object)
{
    struct imports *imports = NULL;
    if (imports) {
        struct imports_file_entry *file;
        dllist_foreach(file, imports->files) {
            char *filename;
            if (asprintf(&filename, "%s.txt", file->name) < 0) {
                perror("asprintf");
                return -1;
            }
            printf("IMPORT INTERNAL %s\n", filename);
            char *file_content = read_file(filename);
            char **defptr;
            dllist_foreach(defptr, file->definitions) {
                if (import_from_string(file_content, mib, *defptr) < 0) {
                    return -1;
                }
            }
        }
    }
    /* if (parse_oid(content, object, mib) < 0) { */
    /*     return -1; */
    /* } */
    return 0;
}

int import_file(char *filename, struct oid *mib)
{
    printf("IMPORT %s\n", filename);
    return import_from_string(read_file(filename), mib, "[a-zA-Z_-][a-zA-Z0-9_-]*");
}

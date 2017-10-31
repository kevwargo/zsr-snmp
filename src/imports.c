#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pcre.h>
#include "utils.h"
#include "parse.h"
#include "imports.h"
#include "dllist.h"


static char *find_imports(char *content)
{
    const char *error;
    int erroffset;
    pcre *re = pcre_compile("^[ \t]*IMPORTS[ \t\n\r]+", PCRE_MULTILINE, &error, &erroffset, NULL);
    if (! re) {
        fprintf(stderr, "pcre_compile error: %s\n", error);
        exit(1);
    }

    int ovector[3];
    int rc = pcre_exec(re, NULL, content, strlen(content), 0, 0, ovector, 3);
    if (rc < 0) {
        if (rc == PCRE_ERROR_NOMATCH) {
            fprintf(stderr, "No IMPORTS found\n");
        } else {
            fprintf(stderr, "IMPORTS match failed with code %d\n", rc);
        }
        exit(1);
    }
    return content + ovector[1];
}

static int handle_imports_token(token, imports)
    struct token *token;
    struct imports *imports;
{
    char *definition;
    switch (token->spec->type) {
        case IDENTIFIER:
            switch (imports->state) {
                case BEFORE_DEFINITION: case AFTER_FILENAME:
                    definition = strdup(token->match);
                    dllist_append(imports->current_file->definitions, &definition);
                    imports->state = AFTER_DEFINITION;
                    break;
                case BEFORE_FILENAME:
                    imports->current_file->name = strdup(token->match);
                    dllist_append(imports->files, imports->current_file);
                    imports->current_file->name = NULL;
                    imports->current_file->definitions = dllist_create();
                    imports->state = AFTER_FILENAME;
                    break;
                default:
                    fprintf(stderr, "Unexpected identifier %s\n", token->match);
                    exit(1);
            }
            break;
        case COMMA:
            if (imports->state == AFTER_DEFINITION) {
                imports->state = BEFORE_DEFINITION;
            } else {
                fprintf(stderr, "Unexpected token COMMA\n");
                exit(1);
            }
            break;
        case KW_FROM:
            if (imports->state == AFTER_DEFINITION) {
                imports->state = BEFORE_FILENAME;
            } else {
                fprintf(stderr, "Unexpected token KW_FROM\n");
                exit(1);
            }
            break;
        case SEMICOLON:
            if (imports->state == AFTER_FILENAME) {
                return 1;
            } else {
                fprintf(stderr, "Unexpected token SEMICOLON\n");
                exit(1);
            }
            break;
        case WHITESPACE:
            break;
        default:
            fprintf(stderr, "Unexpected token type %d\n", token->spec->type);
            exit(1);
    }
    return 0;
}

struct imports *parse_imports(char *filename)
{
    struct dllist *token_specs = dllist_create();
    struct token_spec ts;

    ts.type = KW_FROM;
    ts.re = "FROM";
    dllist_append(token_specs, &ts);

    ts.type = IDENTIFIER;
    ts.re = "[a-zA-Z0-9_-]+";
    dllist_append(token_specs, &ts);

    ts.type = COMMA;
    ts.re = ",";
    dllist_append(token_specs, &ts);

    ts.type = SEMICOLON;
    ts.re = ";";
    dllist_append(token_specs, &ts);

    ts.type = WHITESPACE;
    ts.re = "[ \t\n\r]+";
    dllist_append(token_specs, &ts);

    char *content = find_imports(read_file(filename));

    struct imports *imports = (struct imports *)malloc(sizeof(struct imports));
    imports->state = BEFORE_DEFINITION;
    imports->files = dllist_create();
    imports->current_file = (struct imports_file_entry *)malloc(sizeof(struct imports_file_entry));
    imports->current_file->name = NULL;
    imports->current_file->definitions = dllist_create();
    regexp_scan(content, token_specs, handle_imports_token, imports);

    return imports;
}

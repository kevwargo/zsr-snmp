#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pcre.h>
#include "mibtree.h"
#include "regex.h"

#define SPACE "[ \\t\\n\\r]"


static int handle_symbol_def(struct token *token, int token_num, int *stateptr, void *data, char **errorptr)
{
    struct dllist *list = dllist_create();
    char *name;
    name = strdup("name");
    dllist_append(list, &name);
    name = strdup("objId");
    dllist_append(list, &name);
    name = strdup("objType");
    dllist_append(list, &name);
    name = strdup("octStrType");
    dllist_append(list, &name);
    name = strdup("objIdType");
    dllist_append(list, &name);
    name = strdup("otherType");
    dllist_append(list, &name);
    name = strdup("szLo");
    dllist_append(list, &name);
    name = strdup("szHi");
    dllist_append(list, &name);
    name = strdup("rngLo");
    dllist_append(list, &name);
    name = strdup("rngHi");
    dllist_append(list, &name);
    name = strdup("syntaxJunk");
    dllist_append(list, &name);
    name = strdup("accessType");
    dllist_append(list, &name);
    name = strdup("access");
    dllist_append(list, &name);
    name = strdup("status");
    dllist_append(list, &name);
    name = strdup("descr");
    dllist_append(list, &name);

    char **nameptr;
    dllist_foreach(nameptr, list) {
        char *match;
        int rc = pcre_get_named_substring(token->regex->re, token->subject, token->regex->ovector, token->regex->ovecsize / 3, *nameptr, &match);
        if (rc < 0) {
            fprintf(stderr, "Error %s: %s\n", *nameptr, pcre_strerror(rc));
        } else {
            printf("name %s: '%s'\n", *nameptr, match);
        }
    }
    
    return 1;
}

int parse_symbol(char *name, char *content)
{
    struct parser parser;
    int rc = asprintf(&parser.start_pattern,
            "^\\s*(?<name>%s)"  // name
            SPACE "+"
            "(((?<objId>%s)|(?<objType>%s))" SPACE "+)?"  // object identifier|object-type
            "::=" SPACE "*",

            name,
            "OBJECT" SPACE "+IDENTIFIER",
            "OBJECT-TYPE" SPACE "+.*?"
            "SYNTAX" SPACE "+"
              "("
                 "("
                  "(?<octStrType>OCTET" SPACE "+STRING)|"
                  "(?<objIdType>OBJECT" SPACE "+IDENTIFIER)|"
                  "(?<otherType>[a-zA-Z_][a-zA-Z0-9_-]*)"
                 ")"
                 "("
                  SPACE "*"
                  "("
                   "(\\(" SPACE "*SIZE" SPACE "*"
                       "\\(" SPACE "*(?<szLo>[0-9]+)" SPACE "*\\.\\." SPACE "*(?<szHi>[0-9]+)" SPACE "*\\)"
                     SPACE "*\\))|"
                   "("
                       "\\(" SPACE "*(?<rngLo>[0-9]+)" SPACE "*\\.\\." SPACE "*(?<rngHi>[0-9]+)" SPACE "*\\)"
                   ")"
                  ")"
                 ")?"
              ")?"
              "(?<syntaxJunk>.*?)" SPACE "+"
            "((?<accessType>MAX|MIN)-)?ACCESS" SPACE "+"
              "(?<access>read-only|read-write|write-only|not-accessible)" SPACE "+"
              ".*?"
            "STATUS" SPACE "+"
              "(?<status>mandatory|optional|obsolete)" SPACE "+"
              ".*?"
            "DESCRIPTION" SPACE "+"
              "(\"(?<descr>[^\"]*)\")"
              ".*?"
        );
    if (rc < 0) {
        perror("asprintf");
        return -1;
    }
    parser.start_token_handler = handle_symbol_def;

    char *error;
    if (regex_parse(&parser, content, NULL, NULL, &error) < 0) {
        fprintf(stderr, "Parse object %s failed: %s\n", name, error);
        return -1;
    }
    
    return 0;
}

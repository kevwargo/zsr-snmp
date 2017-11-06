#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pcre.h>
#include "mibtree.h"
#include "regex.h"
#include "utils.h"


#define DEFINE_NAMED_GROUP(GROUP_NAME) \
    char *(GROUP_NAME) = regex_get_named_match(token->regex, token->subject, #GROUP_NAME, errorptr); if (! (GROUP_NAME)) { return -1; }
#define DEFINE_GROUP(VAR_NAME, GROUP) \
    char *(VAR_NAME) = regex_get_match(token->regex, token->subject, GROUP, errorptr); if (! (VAR_NAME)) { return -1; }

#define TYPE_REGEX \
    "OCTET" SPACE "+STRING|" \
    "OBJECT" SPACE "+IDENTIFIER|" \
    "[a-zA-Z_][a-zA-Z0-9_-]*"


static char *normalize_type(char *type);


static void __attribute__((__unused__)) print_groups(struct token *token)
{
    for (int i = 0; i < token->regex->named_count; i++) {
        char *error;
        char *groupname = token->regex->named[i].name;
        char *group = regex_get_named_match(token->regex, token->subject, groupname, &error);
        if (! group) {
            fputs(error, stderr);
        } else {
            printf("%s: '%s'\n", groupname, group);
        }
    }
}

static int handle_object_identifier(struct token *token, int *stateptr, struct mibtree *mibtree, char **errorptr)
{
    DEFINE_NAMED_GROUP(name);
    printf("objId %s\n", name);
    return 0;
}

static int handle_object_type(struct token *token, int *stateptr, struct mibtree *mibtree, char **errorptr)
{
    DEFINE_NAMED_GROUP(name);
    printf("objType %s\n", name);
    return 0;
}

static int handle_type(struct token *token, int *stateptr, struct mibtree *mibtree, char **errorptr)
{
    DEFINE_NAMED_GROUP(name);
    DEFINE_NAMED_GROUP(parType);
    if (strcmp(parType, "CHOICE") == 0 || strcmp(parType, "SEQUENCE") == 0) {
        printf("'%s': %s aggregate type\n", name, parType);
        *stateptr = STATE_MIB_TYPE_INIT;
        return 0;
    }
    DEFINE_NAMED_GROUP(typeDescr);
    printf("'%s' parent type: '%s'\n", name, normalize_type(parType));
    return 1;
}

static int handle_symbol_def(struct token *token, int *stateptr, void *data, char **errorptr)
{
    /* print_groups(token); */
    /* return 1; */
    
    DEFINE_NAMED_GROUP(objId);
    DEFINE_NAMED_GROUP(objType);
    struct mibtree *mibtree = (struct mibtree *)data;
    if (*objId) {
        return handle_object_identifier(token, stateptr, mibtree, errorptr);
    } else if (*objType) {
        return handle_object_type(token, stateptr, mibtree, errorptr);
    } else {
        return handle_type(token, stateptr, mibtree, errorptr);
    }
}

static char *normalize_type(char *type)
{
    char *result = xstrdup(type);
    int len = strlen(result);
    int ws = -1;
    int nonws = -1;
    for (int i = 0; i < len; i++) {
        if (result[i] == ' ' || result[i] == '\t' || result[i] == '\n' || result[i] == '\r') {
            ws = i;
            nonws = ws;
            while (result[nonws] == ' ' || result[nonws] == '\t' || result[nonws] == '\n' || result[nonws] == '\r') {
                nonws++;
            }
            break;
        }
    }
    if (ws >= 0 && nonws > ws) {
        result[ws] = ' ';
        for (int i = nonws; i < len + 1 /*include null byte*/; i++) {
            result[ws + (i - nonws) + 1] = result[i];
        }
    }
    return result;
}

static char *build_type_regex(int subtype)
{
    return xasprintf(
            "%s%s"
            "("
              "(?<%s>SEQUENCE" SPACE "+" "OF" SPACE "+" ")?"
              "(?<%s>" TYPE_REGEX ")"
              "("
               SPACE "*"
               "("
                "(\\(" SPACE "*SIZE" SPACE "*"
                    "\\(" SPACE "*(?<%s>[0-9]+)" SPACE "*(\\.\\." SPACE "*(?<%s>[0-9]+)" SPACE "*)?\\)"
                  SPACE "*\\))|"
                "("
                    "\\(" SPACE "*(?<%s>[0-9]+)" SPACE "*(\\.\\." SPACE "*(?<%s>[0-9]+)" SPACE "*)?\\)"
                ")"
               ")"
              ")?"
            ")?",

            subtype ? 
                "("
                  "\\["
                    "(?<visibility>UNIVERSAL|APPLICATION|CONTEXT-SPECIFIC|PRIVATE)" SPACE "+"
                    "(?<typeId>[0-9]+)" SPACE "*"
                  "\\]" SPACE "+"
                ")?" : "",
            subtype ? "((?<implexpl>(IM|EX)PLICIT)" SPACE "+" ")?" : "",
            subtype ? "parTypeSeqOf" : "typeSeqOf",
            subtype ? "parType" : "type",
            subtype ? "typeSizeLo" : "sizeLo",
            subtype ? "typeSizeHi" : "sizeHi",
            subtype ? "typeRangeLo" : "rangeLo",
            subtype ? "typeRangeHi" : "rangeHi"
        );
}

static int handle_type_init(struct token *token, int *stateptr, void *data, char **errorptr)
{
    *stateptr = STATE_MIB_TYPE_PART;
    return 0;
}

static int handle_type_part(struct token *token, int *stateptr, void *data, char **errorptr)
{
    char *name = regex_get_match(token->regex, token->subject, 1, errorptr);
    char *type_src = regex_get_match(token->regex, token->subject, 2, errorptr);
    char *type = normalize_type(type_src);
    pcre_free_substring(type_src);
    if (strcmp(token->name, "TYPE_PART") == 0) {
        printf("mib type part: '%s' '%s'\n", name, type);
    } else {
        printf("mib type last part: '%s' '%s'\n", name, type);
        *stateptr = STATE_MIB_TYPE_END;
    }
    return 0;
}

static int handle_type_end(struct token *token, int *stateptr, void *data, char **errorptr)
{
    return 1;
}

static int handle_oids_init(struct token *token, int *stateptr, void *data, char **errorptr)
{
    *stateptr = STATE_MIB_OIDS_NEXT;
    return 0;
}

static int handle_oids_next(struct token *token, int *stateptr, void *data, char **errorptr)
{
    DEFINE_NAMED_GROUP(oid);
    DEFINE_NAMED_GROUP(oidval);
    DEFINE_NAMED_GROUP(val);
    if (*val) {
        *stateptr = STATE_MIB_OIDS_END;
        printf("oid value: '%s'\n", val);
    } else if (*oidval) {
        printf("oid '%s(%s)'\n", oid, oidval);
    } else {
        printf("oid '%s'\n", oid);
    }
    return 0;
}

static int handle_oids_end(struct token *token, int *stateptr, void *data, char **errorptr)
{
    return 1;
}

int parse_symbol(char *name, char *content)
{
    struct parser parser;
    char *type_regex = build_type_regex(1);
    char *type_inplace_regex = build_type_regex(0);
    parser.start_pattern = xasprintf(
            "^\\s*(?<name>%s)"
            SPACE "+"
            "(((?<objId>"
              "OBJECT" SPACE "+IDENTIFIER"  
            ")|(?<objType>"
              "OBJECT-TYPE" SPACE "+.*?"
                "SYNTAX" SPACE "+"
                  "%s"
                  "(?<syntaxJunk>.*?)" SPACE "+"
                "((?<accessType>MAX|MIN)-)?ACCESS" SPACE "+"
                  "(?<access>read-only|read-write|write-only|not-accessible|(?<accessJunk>.*?))" SPACE "+"
                  ".*?"
                "STATUS" SPACE "+"
                  "(?<status>mandatory|optional|obsolete|(?<statusJunk>.*?))" SPACE "+"
                  ".*?"
                "DESCRIPTION" SPACE "+"
                  "(\"(?<descr>[^\"]*)\")"
                  ".*?"
            "))" SPACE "+)?"
            "::=" SPACE "*" "(?<typeDescr>%s)?",

            name,
            type_inplace_regex,
            type_regex
        );
    free(type_regex);
    free(type_inplace_regex);
    parser.start_token_handler = handle_symbol_def;

    parser.state = STATE_MIB_OIDS_INIT;
    parser.state_count = MIB_STATE_COUNT;
    parser.states = init_states(MIB_STATE_COUNT);

    struct token token;
    token.pattern = SPACE "*" "{";
    token.handler = handle_type_init;
    token.name = "BRACE_OPEN";
    dllist_append(parser.states[STATE_MIB_TYPE_INIT], &token);

    token.pattern = SPACE "*" "([a-zA-Z_][a-zA-Z0-9_-]*)" SPACE "+" "(" TYPE_REGEX ")" SPACE "*" ",";
    token.handler = handle_type_part;
    token.name = "TYPE_PART";
    dllist_append(parser.states[STATE_MIB_TYPE_PART], &token);

    token.pattern = SPACE "*" "([a-zA-Z_][a-zA-Z0-9_-]*)" SPACE "+" "(" TYPE_REGEX ")";
    token.handler = handle_type_part;
    token.name = "TYPE_PART_LAST";
    dllist_append(parser.states[STATE_MIB_TYPE_PART], &token);

    token.pattern = SPACE "*" "}";
    token.handler = handle_type_end;
    token.name = "BRACE_CLOSE";
    dllist_append(parser.states[STATE_MIB_TYPE_END], &token);

    token.pattern = SPACE "*" "{";
    token.handler = handle_oids_init;
    token.name = "BRACE_OPEN";
    dllist_append(parser.states[STATE_MIB_OIDS_INIT], &token);

    token.pattern = SPACE "*" "((?<oid>[a-zA-Z_][a-zA-Z0-9_-]*)(" SPACE "*" "\\(" SPACE "*" "(?<oidval>[0-9]+)" SPACE "*" "\\))?|(?<val>[0-9]+))";
    token.handler = handle_oids_next;
    token.name = "OID_NEXT";
    dllist_append(parser.states[STATE_MIB_OIDS_NEXT], &token);

    token.pattern = SPACE "*" "}";
    token.handler = handle_oids_end;
    token.name = "BRACE_CLOSE";
    dllist_append(parser.states[STATE_MIB_OIDS_END], &token);

    char *error;
    if (regex_parse(&parser, content, NULL, &error) < 0) {
        fprintf(stderr, "Parse object %s failed: %s\n", name, error);
        return -1;
    }
    
    return 0;
}

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pcre.h>
#include <libgen.h>
#include "mibtree.h"
#include "regex.h"
#include "utils.h"
#include "imports.h"


#define DEFINE_NAMED_GROUP(GROUP_NAME) \
    char *GROUP_NAME = regex_get_named_match(token->regex, token->subject, #GROUP_NAME, errorptr); if (! (GROUP_NAME)) { return REGEX_PARSE_ERROR; }
#define DEFINE_GROUP(VAR_NAME, GROUP) \
    char *VAR_NAME = regex_get_match(token->regex, token->subject, GROUP, errorptr); if (! (VAR_NAME)) { return REGEX_PARSE_ERROR; }


struct mib_parser_data {
    struct mibtree *mib;
    int start;
    int end;
    char *missing_symbol;
    struct oid *current_oid;
    char *current_symbol;
    int target_oid_defined;
    struct object_type *current_type;
    struct object_type_syntax *current_container_type;
};

#define MIB_ERROR_BUFSIZE 1024
static char mib_error_buf[MIB_ERROR_BUFSIZE];

static char *normalize_type(char *type);
static struct object_type_syntax *find_type(char *name, struct dllist *types);

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

static int handle_object_identifier(struct token *token, int *stateptr, struct mib_parser_data *mpd, char **errorptr)
{
    /* DEFINE_NAMED_GROUP(name); */
    /* printf("objId %s\n", name); */
    return 0;
}

static int process_type(
        struct object_type_syntax **typeptr,
        char *name,
        char *parType,
        char *parTypeSeqOf,
        char *parTypeSizeLow,
        char *parTypeSizeHigh,
        char *parTypeRangeLow,
        char *parTypeRangeHigh,
        struct mib_parser_data *mpd,
        char **errorptr
    )
{
    struct object_type_syntax new_type;
    char *parent_type = normalize_type(parType);

    if (name) {
        (*typeptr) = find_type(name, mpd->mib->types);
    }
    if (! (*typeptr)) {
        if (name) {
            (*typeptr) = dllist_append(mpd->mib->types, &new_type);
        } else {
            (*typeptr) = (struct object_type_syntax *)xmalloc(sizeof(struct object_type_syntax));
        }
    }
    memset((*typeptr), 0, sizeof(struct object_type_syntax));
    (*typeptr)->name = name;
    (*typeptr)->parent = find_type(parent_type, mpd->mib->types);
    if (! (*typeptr)->parent) {
        mpd->missing_symbol = parent_type;
        return REGEX_PARSE_SYMBOL_NOT_FOUND_ERROR;
    }
    (*typeptr)->base_type = (*typeptr)->parent->base_type;
    
    if (*parTypeRangeLow) {
        if ((*typeptr)->parent->base_type != MIB_TYPE_INTEGER) {
            snprintf(mib_error_buf, MIB_ERROR_BUFSIZE, "Error parsing type `%s': only types derived from INTEGER can have range restrictions", name ? name : "");
            *errorptr = mib_error_buf;
            return REGEX_PARSE_ERROR;
        }
        (*typeptr)->u.range = (struct range *)xmalloc(sizeof(struct range));
        (*typeptr)->u.range->low = parTypeRangeLow;
        (*typeptr)->u.range->high = *parTypeRangeHigh ? parTypeRangeHigh : NULL;
    }
    if (*parTypeSizeLow) {
        if ((*typeptr)->parent->base_type != MIB_TYPE_OCTET_STRING) {
            snprintf(mib_error_buf, MIB_ERROR_BUFSIZE, "Error parsing type `%s': only types derived from OCTET STRING can have size restrictions", name ? name : "");
            *errorptr = mib_error_buf;
            return REGEX_PARSE_ERROR;
        }
        (*typeptr)->u.range = (struct range *)xmalloc(sizeof(struct range));
        (*typeptr)->u.range->low = parTypeSizeLow;
        (*typeptr)->u.range->high = *parTypeSizeHigh ? parTypeSizeHigh : NULL;
    }
    if (*parTypeSeqOf) {
        struct object_type_syntax *seq_of_type = dllist_append(mpd->mib->types, &new_type);
        memset(seq_of_type, 0, sizeof(struct object_type_syntax));
        seq_of_type->name = name;
        seq_of_type->parent = find_type("SEQUENCE OF", mpd->mib->types);
        seq_of_type->base_type = MIB_TYPE_SEQUENCE_OF;
        seq_of_type->u.seq_type = (*typeptr);
        (*typeptr)->name = NULL;
    }
    return 0;
}

static int handle_object_type(struct token *token, int *stateptr, struct mib_parser_data *mpd, char **errorptr)
{
    /* ("typeSeqOf", "type", "typeSizeLow", "typeSizeHigh", "typeRangeLow", "typeRangeHigh") */
    DEFINE_NAMED_GROUP(name);
    DEFINE_NAMED_GROUP(typeSeqOf);
    DEFINE_NAMED_GROUP(type);
    DEFINE_NAMED_GROUP(typeSizeLow);
    DEFINE_NAMED_GROUP(typeSizeHigh);
    DEFINE_NAMED_GROUP(typeRangeLow);
    DEFINE_NAMED_GROUP(typeRangeHigh);

    /* struct object_type_syntax *syntax = NULL; */
    /* ( */
    /*     struct object_type_syntax **typeptr, */
    /*     char *name, */
    /*     char *parType, */
    /*     char *parTypeSeqOf, */
    /*     char *parTypeSizeLow, */
    /*     char *parTypeSizeHigh, */
    /*     char *parTypeRangeLow, */
    /*     char *parTypeRangeHigh, */
    /*     struct mib_parser_data *mpd, */
    /*     char **errorptr */
    /* ) */
    /* int rc = process_type(&syntax, NULL, type) */
    
    DEFINE_NAMED_GROUP(access);
    DEFINE_NAMED_GROUP(status);
    DEFINE_NAMED_GROUP(descr);
    mpd->current_type = (struct object_type *)xmalloc(sizeof(struct object_type));

    /* if (strcmp(access, "read-only") == 0) { */
    /*     mpd->current_type->access = ACCESS_READ_ONLY; */
    /* } */
    /* if (strcmp(access, "read-write") == 0) { */
    /*     mpd->current_type->access = ACCESS_READ_WRITE; */
    /* } */
    /* if (strcmp(access, "write-only") == 0) { */
    /*     mpd->current_type->access = ACCESS_WRITE_ONLY; */
    /* } */
    /* if (strcmp(access, "not-accessible") == 0) { */
    /*     mpd->current_type->access = ACCESS_NOT_ACCESSIBLE; */
    /* } */
    
    /* if (strcmp(status, "mandatory") == 0) { */
    /*     mpd->current_type->status = STATUS_MANDATORY; */
    /* } */
    /* if (strcmp(status, "optional") == 0) { */
    /*     mpd->current_type->status = STATUS_OPTIONAL; */
    /* } */
    /* if (strcmp(status, "obsolete") == 0) { */
    /*     mpd->current_type->status = STATUS_OBSOLETE; */
    /* } */

    mpd->current_type->access = access;
    mpd->current_type->status = status;
    mpd->current_type->description = descr;
    for (char *cp = descr; *cp; cp++) {
        if (*cp == '\n' || *cp == '\r') {
            *cp = ' ';
        }
    }
     
    return 0;
}

static struct object_type_syntax *find_type(char *name, struct dllist *types)
{
    struct object_type_syntax *type;
    dllist_foreach(type, types) {
        if (strcmp(type->name, name) == 0) {
            return type;
        }
    }
    return NULL;
}


static int handle_type(struct token *token, int *stateptr, struct mib_parser_data *mpd, char **errorptr)
{
    DEFINE_NAMED_GROUP(name);
    DEFINE_NAMED_GROUP(parType);

    DEFINE_NAMED_GROUP(parTypeSeqOf);

    DEFINE_NAMED_GROUP(parTypeSizeLow);
    DEFINE_NAMED_GROUP(parTypeSizeHigh);
    DEFINE_NAMED_GROUP(parTypeRangeLow);
    DEFINE_NAMED_GROUP(parTypeRangeHigh);

    DEFINE_NAMED_GROUP(visibility);
    DEFINE_NAMED_GROUP(typeId);
    DEFINE_NAMED_GROUP(implexpl);

    DEFINE_NAMED_GROUP(typeDescr);

    struct object_type_syntax *type;
    int rc = process_type(&type, name, parType, parTypeSeqOf, parTypeSizeLow, parTypeSizeHigh, parTypeRangeLow, parTypeRangeHigh, mpd, errorptr);
    if (rc < 0) {
        return rc;
    }

    type->visibility = visibility;
    type->implexpl = implexpl;
    type->type_id = atoi(typeId);
    pcre_free_substring(typeId);

    if (strcmp(parType, "CHOICE") == 0 || strcmp(parType, "SEQUENCE") == 0) {
        mpd->current_container_type = type;
        type->u.components = dllist_create();
        *stateptr = STATE_MIB_TYPE_INIT;
        return 0;
    }
    mpd->end = token->regex->ovector[1];
    return 1;
}

static int handle_symbol_def(struct token *token, int *stateptr, void *data, char **errorptr)
{
    DEFINE_NAMED_GROUP(objId);
    DEFINE_NAMED_GROUP(objType);
    DEFINE_NAMED_GROUP(name);
    struct mib_parser_data *mpd = (struct mib_parser_data *)data;
    mpd->start = token->regex->ovector[0];
    mpd->current_symbol = name;
    if (*objId) {
        return handle_object_identifier(token, stateptr, mpd, errorptr);
    } else if (*objType) {
        return handle_object_type(token, stateptr, mpd, errorptr);
    } else {
        return handle_type(token, stateptr, mpd, errorptr);
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

static int handle_type_init(struct token *token, int *stateptr, void *data, char **errorptr)
{
    *stateptr = STATE_MIB_TYPE_PART;
    return 0;
}

static int handle_type_part(struct token *token, int *stateptr, void *data, char **errorptr)
{
    /* ( */
    /*     struct object_type_syntax **typeptr, */
    /*     char *name, */
    /*     char *parType, */
    /*     char *parTypeSeqOf, */
    /*     char *parTypeSizeLow, */
    /*     char *parTypeSizeHigh, */
    /*     char *parTypeRangeLow, */
    /*     char *parTypeRangeHigh, */
    /*     struct mib_parser_data *mpd, */
    /*     char **errorptr */
    /* ) */
    DEFINE_NAMED_GROUP(name);
    DEFINE_NAMED_GROUP(type);

    DEFINE_NAMED_GROUP(comma);
    
    DEFINE_NAMED_GROUP(seqOf);
    DEFINE_NAMED_GROUP(sizeLow);
    DEFINE_NAMED_GROUP(sizeHigh);
    DEFINE_NAMED_GROUP(rangeLow);
    DEFINE_NAMED_GROUP(rangeHigh);

    struct mib_parser_data *mpd = (struct mib_parser_data *)data;
    struct container_type_item item;
    item.name = name;
    item.type = NULL;
    struct object_type_syntax *container = mpd->current_container_type;
    int rc = process_type(&item.type, NULL, type, seqOf, sizeLow, sizeHigh, rangeLow, rangeHigh, mpd, errorptr);
    if (rc < 0) {
        return rc;
    }
    mpd->current_container_type = container;
    dllist_append(container->u.components, &item);
    if (! (*comma)) {
        *stateptr = STATE_MIB_TYPE_END;
    }
    return 0;
}

static int handle_type_end(struct token *token, int *stateptr, void *data, char **errorptr)
{
    ((struct mib_parser_data *)data)->end = token->regex->ovector[1];
    return 1;
}


static int handle_oids_init(struct token *token, int *stateptr, void *data, char **errorptr)
{
    *stateptr = STATE_MIB_OIDS_NEXT;
    return 0;
}

static struct oid *find_oid(char *oid_name, struct oid *root)
{
    if (strcmp(root->name, oid_name) == 0) {
        return root;
    }
    struct oid **oidptr;
    dllist_foreach(oidptr, root->children) {
        struct oid *oid = find_oid(oid_name, *oidptr);
        if (oid) {
            return oid;
        }
    }
    return NULL;
}

static int process_oid(char *oid, struct mib_parser_data *mpd, char **errorptr)
{
    if (mpd->current_oid) {
        snprintf(mib_error_buf, MIB_ERROR_BUFSIZE, "Invalid token `%s': symbolic OIDs are allowed only at the beginning", oid);
        *errorptr = mib_error_buf;
        return REGEX_PARSE_ERROR;
    }
    mpd->current_oid = find_oid(oid, mpd->mib->root_oid);
    if (! mpd->current_oid) {
        mpd->missing_symbol = oid;
        return REGEX_PARSE_SYMBOL_NOT_FOUND_ERROR;
    }
    /* printf("Parent OID %s found\n", oid); */
    return 0;
}

static void set_oid(char *name, char *value, struct oid **parentptr, struct oid *root)
{
    struct oid *oid = find_oid(name, root);
    if (oid) {
        printf("WARNING: OID `%s' already exists\n", name);
    }
    int numval = atoi(value);
    struct oid **oidptr;
    oid = NULL;
    dllist_foreach(oidptr, (*parentptr)->children) {
        if ((*oidptr)->value == numval) {
            printf("WARNING: an OID with the same value as `%s' already exists\n", name);
            oid = *oidptr;
            break;
        }
    }
    if (! oid) {
        oid = (struct oid *)xcalloc(1, sizeof(struct oid));
        oid->children = dllist_create();
        dllist_append((*parentptr)->children, &oid);
    }
    oid->name = name;
    oid->value = numval;
    *parentptr = oid;
}

static int process_oid_value(char *value, struct mib_parser_data *mpd, char **errorptr)
{
    if (! mpd->current_oid) {
        snprintf(mib_error_buf, MIB_ERROR_BUFSIZE, "Invalid token `%s': OID values cannot be the first in OID", value);
        *errorptr = mib_error_buf;
        return REGEX_PARSE_ERROR;
    }
    set_oid(mpd->current_symbol, value, &mpd->current_oid, mpd->mib->root_oid);
    mpd->current_oid->type = mpd->current_type;
    mpd->target_oid_defined = 1;
    return 0;
}

static int process_oid_with_value(char *oid, char *value, struct mib_parser_data *mpd, char **errorptr)
{
    if (! mpd->current_oid) {
        snprintf(mib_error_buf, MIB_ERROR_BUFSIZE, "Invalid token `%s(%s)': OID with values cannot be the first in OID", oid, value);
        *errorptr = mib_error_buf;
        return REGEX_PARSE_ERROR;
    }
    set_oid(oid, value, &mpd->current_oid, mpd->mib->root_oid);
    return 0;
}

static int handle_oids_next(struct token *token, int *stateptr, void *data, char **errorptr)
{
    DEFINE_NAMED_GROUP(oid);
    DEFINE_NAMED_GROUP(oidval);
    DEFINE_NAMED_GROUP(val);
    struct mib_parser_data *mpd = (struct mib_parser_data *)data;
    if (*val) {
        *stateptr = STATE_MIB_OIDS_END;
        return process_oid_value(val, mpd, errorptr);
    } else if (*oidval) {
        return process_oid_with_value(oid, oidval, mpd, errorptr);
    } else {
        return process_oid(oid, mpd, errorptr);
    }
}

static int handle_oids_end(struct token *token, int *stateptr, void *data, char **errorptr)
{
    struct mib_parser_data *mpd = (struct mib_parser_data *)data;
    if (! mpd->target_oid_defined) {
        snprintf(mib_error_buf, MIB_ERROR_BUFSIZE, "Invalid token `}': the last OID item must be a value");
        *errorptr = mib_error_buf;
        return REGEX_PARSE_ERROR;
    }
    mpd->end = token->regex->ovector[1];
    return 1;
}

static void print_oid(struct oid *oid, int level)
{
    for (int i = 0; i < level; i++) {
        putchar(' ');
    }
    printf("%s(%d)", oid->name, oid->value);
    if (oid->type) {
        printf(" (ACCESS: %s, STATUS: %s)\n", oid->type->access, oid->type->status);
    } else {
        putchar('\n');
    }
    struct oid **oidptr;
    dllist_foreach(oidptr, oid->children) {
        print_oid(*oidptr, level + 1);
    }
}

void print_oidtree(struct oid *tree)
{
    print_oid(tree, 0);
}

static void print_type(struct object_type_syntax *type)
{
    
    char *base_type;
    switch (type->base_type) {
        case MIB_TYPE_NULL:
            base_type = "MIB_TYPE_NULL";
            break;
        case MIB_TYPE_INTEGER:
            base_type = "MIB_TYPE_INTEGER";
            break;
        case MIB_TYPE_OBJECT_IDENTIFIER:
            base_type = "MIB_TYPE_OBJECT_IDENTIFIER";
            break;
        case MIB_TYPE_OCTET_STRING:
            base_type = "MIB_TYPE_OCTET_STRING";
            break;
        case MIB_TYPE_SEQUENCE:
            base_type = "MIB_TYPE_SEQUENCE";
            break;
        case MIB_TYPE_CHOICE:
            base_type = "MIB_TYPE_CHOICE";
            break;
        case MIB_TYPE_SEQUENCE_OF:
            base_type = "MIB_TYPE_SEQUENCE_OF";
            break;
        default:
            base_type = "UNKNOWN_MIB_BASE_TYPE";
    }
    printf("Type %s, base_type %s, parent: %s", type->name, base_type, type->parent ? type->parent->name : "");
    if (type->base_type == MIB_TYPE_INTEGER && type->u.range) {
        printf(", range restrictions: %s%s%s\n", type->u.range->low, type->u.range->high ? ".." : "", type->u.range->high ? type->u.range->high : "");
    } else if (type->base_type == MIB_TYPE_OCTET_STRING && type->u.range) {
        printf(", size restrictions: %s%s%s\n", type->u.range->low, type->u.range->high ? ".." : "", type->u.range->high ? type->u.range->high : "");
    } else {
        putchar('\n');
    }
    if (type->base_type == MIB_TYPE_CHOICE || type->base_type == MIB_TYPE_SEQUENCE) {
        printf("Elements of %s:\n", base_type);
        struct container_type_item *item;
        if (! type->u.components) {
            printf("type->u.components is NULL for %s\n", type->name);
        } else {
            dllist_foreach(item, type->u.components) {
                printf("\t%s: ", item->name);
                print_type(item->type);
            }
        }
    }
}

void print_types(struct dllist *types)
{
    struct object_type_syntax *type;
    dllist_foreach(type, types) {
        print_type(type);
    }
}

int parse_symbol(char *name, char *content, struct mibtree *mib, char **errorptr)
{
    struct parser parser;
    parser.start_pattern = xasprintf(
            "^\\s*(?<name>%s)"
            SPACE "+"
            "(((?<objId>"
              "OBJECT" SPACE "+IDENTIFIER"  
            ")|(?<objType>"
              "OBJECT-TYPE" SPACE "+.*?"
                "SYNTAX" SPACE "+"
                    BUILD_TYPE_INPLACE_REGEX("typeSeqOf", "type", "typeSizeLow", "typeSizeHigh", "typeRangeLow", "typeRangeHigh")
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
            "::=" SPACE "*"
              "(?<typeDescr>"
                BUILD_TYPE_REGEX("visibility", "typeId", "implexpl", "parTypeSeqOf", "parType", "parTypeSizeLow", "parTypeSizeHigh", "parTypeRangeLow", "parTypeRangeHigh")
              ")?",

            name
        );
    parser.start_token_handler = handle_symbol_def;

    parser.state = STATE_MIB_OIDS_INIT;
    parser.state_count = MIB_STATE_COUNT;
    parser.states = init_states(MIB_STATE_COUNT);

    struct token token;
    token.pattern = SPACE "*" "{";
    token.handler = handle_type_init;
    token.name = "BRACE_OPEN";
    dllist_append(parser.states[STATE_MIB_TYPE_INIT], &token);

    token.pattern = SPACE "*" "(?<name>[a-zA-Z_][a-zA-Z0-9_-]*)" SPACE "+"
        BUILD_TYPE_INPLACE_REGEX("seqOf", "type", "sizeLow", "sizeHigh", "rangeLow", "rangeHigh")
        "(?<comma>" SPACE "*" ",)?";
    token.handler = handle_type_part;
    token.name = "TYPE_PART";
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

    struct mib_parser_data mpd;
    memset(&mpd, 0, sizeof (struct mib_parser_data));
    mpd.mib = mib;
    int rc = regex_parse(&parser, content, &mpd, errorptr);
    if (rc == REGEX_PARSE_SYMBOL_NOT_FOUND_ERROR) {
        printf("symbol %s needs %s, entering recursive parse\n", name, mpd.missing_symbol);
        rc = parse_symbol(mpd.missing_symbol, content, mib, errorptr);
        if (rc == 0) {
            printf("needed symbol %s found\n", mpd.missing_symbol);
            parser.state = STATE_MIB_OIDS_INIT;
            rc = regex_parse(&parser, content, &mpd, errorptr);
            if (rc < 0) {
                printf("symbol %s second parse failed: %s (%s)\n", name, pcre_strerror(rc), *errorptr);
            }
        }
    }
    if (rc < 0) {
        return rc;
    }

    for (int i = mpd.start; i < mpd.end; i++) {
        content[i] = ' ';
    }
    
    return 0;
}


static int import_from_string(char *content, char *name, struct mibtree *mib, char *dir, char **errorptr, int *imports_parsed)
{
    if (! *imports_parsed) {
        struct imports *imports = parse_imports(content, errorptr);
        if (! imports) {
            return REGEX_PARSE_ERROR;
        }
        *imports_parsed = 1;
        struct imports_file_entry **fileptr;
        dllist_foreach(fileptr, imports->files) {
            char *file_content;
            char *file_content_orig;
            int basenamelen = strlen(dir) + strlen((*fileptr)->name) + 1;
            char *filename = xmalloc(basenamelen + 5);
            sprintf(filename, "%s/%s", dir, (*fileptr)->name);
            file_content_orig = read_file(filename);
            file_content = remove_comments(file_content_orig);
            if (! file_content) {
                sprintf(filename + basenamelen, ".mib");
                file_content_orig = read_file(filename);
                file_content = remove_comments(file_content_orig);
                if (! file_content) {
                    sprintf(filename + basenamelen, ".txt");
                    file_content_orig = read_file(filename);
                    file_content = remove_comments(file_content_orig);
                    if (! file_content) {
                        fprintf(stderr, "Skipping %s import\n", (*fileptr)->name);
                    }
                }
            }
            if (file_content) {
                char **nameptr;
                int file_imports_parsed = 0;
                printf("Importing from file %s\n", filename);
                dllist_foreach(nameptr, (*fileptr)->definitions) {
                    import_from_string(file_content, *nameptr, mib, dir, errorptr, &file_imports_parsed);
                }
            }
            free(filename);
        }
    }
    int rc = parse_symbol(name, content, mib, errorptr);
    if (rc < 0) {
        return rc;
    }
    while (rc >= 0) {
        rc = parse_symbol(name, content, mib, errorptr);
    }
    return rc == REGEX_PARSE_START_TOKEN_NOT_FOUND_ERROR ? 0 : rc;
}

static struct oid *create_base_oid()
{
    struct oid *oid = (struct oid *)xmalloc(sizeof(struct oid));
    oid->value = 1;
    oid->name = strdup("iso");
    oid->children = dllist_create();
    oid->type = NULL;
    return oid;
}

static struct dllist *create_base_types()
{
    struct dllist *types = dllist_create();

    struct object_type_syntax type;
    memset(&type, 0, sizeof(struct object_type_syntax));

    type.name = "NULL";
    type.base_type = MIB_TYPE_NULL;
    dllist_append(types, &type);

    type.name = "INTEGER";
    type.base_type = MIB_TYPE_INTEGER;
    dllist_append(types, &type);

    type.name = "OBJECT IDENTIFIER";
    type.base_type = MIB_TYPE_OBJECT_IDENTIFIER;
    dllist_append(types, &type);

    type.name = "OCTET STRING";
    type.base_type = MIB_TYPE_OCTET_STRING;
    dllist_append(types, &type);

    type.name = "SEQUENCE";
    type.base_type = MIB_TYPE_SEQUENCE;
    dllist_append(types, &type);

    type.name = "CHOICE";
    type.base_type = MIB_TYPE_CHOICE;
    dllist_append(types, &type);

    type.name = "SEQUENCE OF";
    type.base_type = MIB_TYPE_SEQUENCE_OF;
    dllist_append(types, &type);

    return types;
}

struct mibtree *import_file(char *filename)
{
    char *content = read_file(filename);
    if (! content) {
        perror("read_file");
        return NULL;
    }
    char *content_stripped = remove_comments(content);
    free(content);

    struct mibtree *mib = (struct mibtree *)xmalloc(sizeof(struct mibtree));
    mib->root_oid = create_base_oid();
    mib->types = create_base_types();

    char *fname = xstrdup(filename);
    char *dir = xstrdup(dirname(fname));
    free(fname);
    
    char *error;
    int imports_parsed = 0;
    printf("Importing main file %s\n", filename);
    int rc = import_from_string(content_stripped, "[a-zA-Z_][a-zA-Z0-9_-]*", mib, dir, &error, &imports_parsed);
    if (rc < 0) {
        free(mib);
        fprintf(stderr, "File `%s' import error: %s\n", filename, error);
        return NULL;
    }
    return mib;
}

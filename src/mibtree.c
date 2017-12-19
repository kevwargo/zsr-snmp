#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pcre.h>
#include <libgen.h>
#include <errno.h>
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

struct type_syntax_spec {
    char *name;
    char *par_type;
    char *par_type_seq_of;
    char *par_type_size_low;
    char *par_type_size_high;
    char *par_type_range_low;
    char *par_type_range_high;
    char *visibility;
    char *implexpl;
    char *tag;
};


#define MIB_ERROR_BUFSIZE 1024
static char mib_error_buf[MIB_ERROR_BUFSIZE];

static char *normalize_type(char *type);
static struct object_type_syntax **find_type(char *name, struct dllist *types);


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

static int parse_num(char *source, unsigned long long *numptr, char **errorptr)
{
    if (! *source) {
        snprintf(mib_error_buf, MIB_ERROR_BUFSIZE, "Number string cannot be empty");
        return -1;
    }
    char *end;
    errno = 0;
    unsigned long long result = strtoull(source, &end, 10);
    if (*end) {
        snprintf(mib_error_buf, MIB_ERROR_BUFSIZE, "Invalid character '%c' in number string at pos %d", *end, (int)(end - source));
        *errorptr = mib_error_buf;
        return -1;
    }
    if (errno != 0) {
        snprintf(mib_error_buf, MIB_ERROR_BUFSIZE, "Number parse error: %s", strerror(errno));
        *errorptr = mib_error_buf;
        return -1;
    }
    *numptr = result;
    return 0;
}

static int handle_object_identifier(struct token *token, int *stateptr, struct mib_parser_data *mpd, char **errorptr)
{
    /* DEFINE_NAMED_GROUP(name); */
    /* printf("objId %s\n", name); */
    return 0;
}

static int parse_range(struct object_type_syntax *type, char *low, char *high, char **errorptr)
{
    type->u.range = (struct range *)xmalloc(sizeof(struct range));
    memset(type->u.range, 0, sizeof(struct range));

    unsigned long long number;
    int rc = parse_num(low, &number, errorptr);
    if (rc < 0) {
        return rc;
    }
    type->u.range->low = (struct number *)xmalloc(sizeof(struct number));
    type->u.range->low->value.u = number;
    type->u.range->low->is_signed = (*low == '-');

    if (high && *high) {
        rc = parse_num(high, &number, errorptr);
        if (rc < 0) {
            return rc;
        }
        type->u.range->high = (struct number *)xmalloc(sizeof(struct number));
        type->u.range->high->value.u = number;
        type->u.range->high->is_signed = (*high == '-');
    }

    return 0;
}

static int process_type(struct object_type_syntax **typeptr, struct type_syntax_spec *spec, struct mib_parser_data *mpd, char **errorptr)
{
    char *parent_type = normalize_type(spec->par_type);
    (*typeptr) = (struct object_type_syntax *)xmalloc(sizeof(struct object_type_syntax));
    memset((*typeptr), 0, sizeof(struct object_type_syntax));
    (*typeptr)->name = spec->name;
    struct object_type_syntax **parptr = find_type(parent_type, mpd->mib->types);
    if (! parptr) {
        mpd->missing_symbol = parent_type;
        return REGEX_PARSE_SYMBOL_NOT_FOUND_ERROR;
    }
    (*typeptr)->parent = *parptr;
    (*typeptr)->base_type = (*typeptr)->parent->base_type;
    if ((*typeptr)->parent->base_type == MIB_TYPE_INTEGER
            || (*typeptr)->parent->base_type == MIB_TYPE_OCTET_STRING) {
        (*typeptr)->u.range = (*parptr)->u.range;
    }

    if (*spec->par_type_range_low) {
        if ((*typeptr)->parent->base_type != MIB_TYPE_INTEGER) {
            snprintf(mib_error_buf, MIB_ERROR_BUFSIZE, "Error parsing type `%s': only types derived from INTEGER can have range restrictions", spec->name ? spec->name : "");
            *errorptr = mib_error_buf;
            return REGEX_PARSE_ERROR;
        }
        int rc = parse_range(*typeptr, spec->par_type_range_low, spec->par_type_range_high, errorptr);
        if (rc < 0) {
            return rc;
        }
    }
    if (*spec->par_type_size_low) {
        if ((*typeptr)->parent->base_type != MIB_TYPE_OCTET_STRING) {
            snprintf(mib_error_buf, MIB_ERROR_BUFSIZE, "Error parsing type `%s': only types derived from OCTET STRING can have size restrictions", spec->name ? spec->name : "");
            *errorptr = mib_error_buf;
            return REGEX_PARSE_ERROR;
        }
        int rc = parse_range(*typeptr, spec->par_type_size_low, spec->par_type_size_high, errorptr);
        if (rc < 0) {
            return rc;
        }
    }
    if (*spec->par_type_seq_of) {
        struct object_type_syntax *seq_of_type = (struct object_type_syntax *)xmalloc(sizeof(struct object_type_syntax));
        memset(seq_of_type, 0, sizeof(struct object_type_syntax));
        seq_of_type->name = spec->name;
        seq_of_type->parent = *find_type("SEQUENCE OF", mpd->mib->types);
        seq_of_type->base_type = MIB_TYPE_SEQUENCE_OF;
        seq_of_type->u.seq_type = (*typeptr);
        (*typeptr)->name = NULL;
        (*typeptr) = seq_of_type;
    }
    (*typeptr)->is_explicit = (strcmp(spec->implexpl, "EXPLICIT") == 0);
    if (strcmp(spec->visibility, "UNIVERSAL") == 0) {
        (*typeptr)->visibility = 0;
    } else if (strcmp(spec->visibility, "APPLICATION") == 0) {
        (*typeptr)->visibility = 1;
    } else if (strcmp(spec->visibility, "CONTEXT-SPECIFIC") == 0) {
        (*typeptr)->visibility = 2;
    } else if (strcmp(spec->visibility, "PRIVATE") == 0) {
        (*typeptr)->visibility = 3;
    } else if (strcmp(spec->implexpl, "IMPLICIT") == 0) {
        (*typeptr)->visibility = 2;
    } else {
        (*typeptr)->visibility = 0;
    }
    if (*spec->tag) {
        int rc = parse_num(spec->tag, &(*typeptr)->tag, errorptr);
        if (rc < 0) {
            return rc;
        }
    } else {
        (*typeptr)->tag = (*typeptr)->parent->tag;
    }
    return 0;
}

static int handle_object_type(struct token *token, int *stateptr, struct mib_parser_data *mpd, char **errorptr)
{
    DEFINE_NAMED_GROUP(name);
    DEFINE_NAMED_GROUP(typeSeqOf);
    DEFINE_NAMED_GROUP(type);
    DEFINE_NAMED_GROUP(typeSizeLow);
    DEFINE_NAMED_GROUP(typeSizeHigh);
    DEFINE_NAMED_GROUP(typeRangeLow);
    DEFINE_NAMED_GROUP(typeRangeHigh);
    DEFINE_NAMED_GROUP(syntaxVisibility);
    DEFINE_NAMED_GROUP(syntaxImplexpl);
    DEFINE_NAMED_GROUP(syntaxTag);

    mpd->current_type = (struct object_type *)xmalloc(sizeof(struct object_type));

    struct type_syntax_spec spec;
    spec.name = name;
    spec.par_type = type;
    spec.par_type_seq_of = typeSeqOf;
    spec.par_type_size_low = typeSizeLow;
    spec.par_type_size_high = typeSizeHigh;
    spec.par_type_range_low = typeRangeLow;
    spec.par_type_range_high = typeRangeHigh;
    spec.visibility = syntaxVisibility;
    spec.implexpl = syntaxImplexpl;
    spec.tag = syntaxTag;

    int rc = process_type(&mpd->current_type->syntax, &spec, mpd, errorptr);
    if (rc < 0) {
        return rc;
    }

    DEFINE_NAMED_GROUP(access);
    DEFINE_NAMED_GROUP(status);
    DEFINE_NAMED_GROUP(descr);
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

static struct object_type_syntax **find_type(char *name, struct dllist *types)
{
    struct object_type_syntax **typeptr;
    dllist_foreach(typeptr, types) {
        if (strcmp((*typeptr)->name, name) == 0) {
            return typeptr;
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
    DEFINE_NAMED_GROUP(tag);
    DEFINE_NAMED_GROUP(implexpl);

    DEFINE_NAMED_GROUP(typeDescr);

    struct object_type_syntax *type;
    struct type_syntax_spec spec;
    spec.name = name;
    spec.par_type = parType;
    spec.par_type_seq_of = parTypeSeqOf;
    spec.par_type_size_low = parTypeSizeLow;
    spec.par_type_size_high = parTypeSizeHigh;
    spec.par_type_range_low = parTypeRangeLow;
    spec.par_type_range_high = parTypeRangeHigh;
    spec.visibility = visibility;
    spec.implexpl = implexpl;
    spec.tag = tag;
    int rc = process_type(&type, &spec, mpd, errorptr);
    if (rc < 0) {
        return rc;
    }

    struct object_type_syntax **existing_type = find_type(name, mpd->mib->types);
    if (existing_type) {
        *existing_type = type;
    } else {
        dllist_append(mpd->mib->types, &type);
    }

    if (type->base_type == MIB_TYPE_SEQUENCE_OF) {
        type = type->u.seq_type;
    }

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
    DEFINE_NAMED_GROUP(name);
    DEFINE_NAMED_GROUP(type);
    DEFINE_NAMED_GROUP(seqOf);
    DEFINE_NAMED_GROUP(sizeLow);
    DEFINE_NAMED_GROUP(sizeHigh);
    DEFINE_NAMED_GROUP(rangeLow);
    DEFINE_NAMED_GROUP(rangeHigh);
    DEFINE_NAMED_GROUP(visibility);
    DEFINE_NAMED_GROUP(implexpl);
    DEFINE_NAMED_GROUP(tag);
    DEFINE_NAMED_GROUP(comma);

    struct mib_parser_data *mpd = (struct mib_parser_data *)data;
    struct object_type_syntax *container = mpd->current_container_type;
    struct object_type_syntax *item;
    struct type_syntax_spec spec;
    spec.name = name;
    spec.par_type = type;
    spec.par_type_seq_of = seqOf;
    spec.par_type_size_low = sizeLow;
    spec.par_type_size_high = sizeHigh;
    spec.par_type_range_low = rangeLow;
    spec.par_type_range_high = rangeHigh;
    spec.visibility = visibility;
    spec.implexpl = implexpl;
    spec.tag = tag;
    int rc = process_type(&item, &spec, mpd, errorptr);
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

static int process_oid(char *oid, struct mib_parser_data *mpd, char **errorptr)
{
    if (mpd->current_oid) {
        snprintf(mib_error_buf, MIB_ERROR_BUFSIZE, "Invalid token `%s': symbolic OIDs are allowed only at the beginning", oid);
        *errorptr = mib_error_buf;
        return REGEX_PARSE_ERROR;
    }
    mpd->current_oid = find_oid_by_name(oid, mpd->mib->root_oid);
    if (! mpd->current_oid) {
        mpd->missing_symbol = oid;
        return REGEX_PARSE_SYMBOL_NOT_FOUND_ERROR;
    }
    /* printf("Parent OID %s found\n", oid); */
    return 0;
}

char *oid_to_string(struct oid *oid)
{
    struct oid *parent = oid;
    int size = 0;
    struct dllist *values = dllist_create();
    do {
        int val = parent->value;
        int digits = 1;
        while (val /= 10) {
            digits++;
        }
        size += digits + 1;
        dllist_prepend(values, &parent->value);
        parent = parent->parent;
    } while (parent);
    char *result = (char *)xmalloc(size);
    int *valptr;
    int pos = 0;
    dllist_foreach(valptr, values) {
        pos += sprintf(result + pos, "%d%s", *valptr, _dle->next ? "." : "");
    }
    return result;
}

static int set_oid(char *name, char *value, struct oid **parentptr, struct oid *root, char **errorptr)
{
    struct oid *oid = find_oid_by_name(name, root);
    if (oid) {
        char *s = oid_to_string(oid);
        printf("WARNING: OID `%s' (%s) already exists\n", name, s);
        free(s);
    }
    oid = NULL;
    unsigned long long numval;
    int rc = parse_num(value, &numval, errorptr);
    if (rc < 0) {
        return rc;
    }
    struct oid **oidptr;
    dllist_foreach(oidptr, (*parentptr)->children) {
        if ((*oidptr)->value == (int)numval) {
            char *s = oid_to_string(*oidptr);
            printf("WARNING: Duplicate values for OIDS `%s' and `%s': %s\n", (*oidptr)->name, name, s);
            free(s);
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
    oid->value = (int)numval;
    oid->parent = *parentptr;
    *parentptr = oid;
    return 0;
}

static int process_oid_value(char *value, struct mib_parser_data *mpd, char **errorptr)
{
    if (! mpd->current_oid) {
        snprintf(mib_error_buf, MIB_ERROR_BUFSIZE, "Invalid token `%s': OID values cannot be the first in OID", value);
        *errorptr = mib_error_buf;
        return REGEX_PARSE_ERROR;
    }
    int rc = set_oid(mpd->current_symbol, value, &mpd->current_oid, mpd->mib->root_oid, errorptr);
    if (rc < 0) {
        return rc;
    }
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
    int rc = set_oid(oid, value, &mpd->current_oid, mpd->mib->root_oid, errorptr);
    if (rc < 0) {
        return rc;
    }
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
    char *s = oid_to_string(oid);
    printf("%s(%d) (%s)", oid->name, oid->value, s);
    free(s);
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

static char *strnumber(struct number *number) {
    char *result = (char *)xmalloc(24);
    if (number->is_signed) {
        snprintf(result, 24, "%lld", number->value.s);
    } else {
        snprintf(result, 24, "%llu", number->value.u);
    }
    return result;
}

char *strrange(struct range *range)
{
    static char result[64];
    char *num = strnumber(range->low);
    snprintf(result, 64, num);
    free(num);
    if (range->high) {
        strcat(result, "..");
        num = strnumber(range->high);
        strcat(result, num);
        free(num);
    }
    return result;
}

static void print_type_internal(struct object_type_syntax *type, int level)
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
    for (int i = 0; i < level; i++) {
        putchar('\t');
    }
    printf("%s %s, base_type %s, vis: %d, expl: %s, tag: %llu, parent: %s", level == 0 ? "Type" : "Subtype", type->name, base_type, type->visibility, type->is_explicit ? "yes" : "no", type->tag, type->parent ? type->parent->name : "");
    if (type->base_type == MIB_TYPE_INTEGER && type->u.range) {
        printf(", range restrictions: %s\n", strrange(type->u.range));
    } else if (type->base_type == MIB_TYPE_OCTET_STRING && type->u.range) {
        printf(", size restrictions: %s\n", strrange(type->u.range));
    } else if (type->base_type == MIB_TYPE_SEQUENCE_OF && type->u.seq_type) {
        printf(", seq type: ");
        print_type_internal(type->u.seq_type, 0);
        putchar('\n');
    } else {
        putchar('\n');
    }
    if (type->base_type == MIB_TYPE_CHOICE || type->base_type == MIB_TYPE_SEQUENCE) {
        struct object_type_syntax **itemptr;
        if (type->u.components) {
            dllist_foreach(itemptr, type->u.components) {
                print_type_internal(*itemptr, level + 1);
            }
        }
    }
}

void print_type(struct object_type_syntax *type)
{
    print_type_internal(type, 0);
}

void print_types(struct dllist *types)
{
    struct object_type_syntax **typeptr;
    dllist_foreach(typeptr, types) {
        print_type_internal(*typeptr, 0);
    }
}

int parse_symbol(char *name, char *content, struct mibtree *mib, char **errorptr)
{
    struct parser parser;
    parser.start_pattern = xasprintf(
            "^\\s*(?<name>%s)"
            SPACE "+"
            "("
              "("
                "(?<objId>OBJECT" SPACE "+IDENTIFIER)"
              "|"
                "(?<objType>"
                  "OBJECT-TYPE" SPACE "+.*?"
                    "SYNTAX" SPACE "+"
                        BUILD_TYPE_REGEX("syntaxVisibility", "syntaxTag", "syntaxImplexpl", "typeSeqOf", "type", "typeSizeLow", "typeSizeHigh", "typeRangeLow", "typeRangeHigh")
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
                ")"
              ")" SPACE "+"
            ")?"
            "::=" SPACE "*"
              "(?<typeDescr>"
                BUILD_TYPE_REGEX("visibility", "tag", "implexpl", "parTypeSeqOf", "parType", "parTypeSizeLow", "parTypeSizeHigh", "parTypeRangeLow", "parTypeRangeHigh")
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
        BUILD_TYPE_REGEX("visibility", "tag", "implexpl", "seqOf", "type", "sizeLow", "sizeHigh", "rangeLow", "rangeHigh")
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
        /* printf("symbol %s needs %s, entering recursive parse\n", name, mpd.missing_symbol); */
        rc = parse_symbol(mpd.missing_symbol, content, mib, errorptr);
        if (rc == 0) {
            /* printf("needed symbol %s found\n", mpd.missing_symbol); */
            parser.state = STATE_MIB_OIDS_INIT;
            rc = regex_parse(&parser, content, &mpd, errorptr);
            if (rc < 0) {
                /* printf("symbol %s second parse failed: %s (%s)\n", name, pcre_strerror(rc), *errorptr); */
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
                        /* fprintf(stderr, "Skipping %s import\n", (*fileptr)->name); */
                    }
                }
            }
            if (file_content) {
                char **nameptr;
                int file_imports_parsed = 0;
                /* printf("Importing from file %s\n", filename); */
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
    oid->parent = NULL;
    return oid;
}

static struct dllist *create_base_types()
{
    struct dllist *types = dllist_create();

    struct object_type_syntax *type;

    type = (struct object_type_syntax *)xmalloc(sizeof(struct object_type_syntax));
    memset(type, 0, sizeof(struct object_type_syntax));
    type->name = "NULL";
    type->base_type = MIB_TYPE_NULL;
    type->is_explicit = 1;
    type->tag = 5;
    dllist_append(types, &type);

    type = (struct object_type_syntax *)xmalloc(sizeof(struct object_type_syntax));
    memset(type, 0, sizeof(struct object_type_syntax));
    type->name = "INTEGER";
    type->base_type = MIB_TYPE_INTEGER;
    type->is_explicit = 1;
    type->tag = 2;
    dllist_append(types, &type);

    type = (struct object_type_syntax *)xmalloc(sizeof(struct object_type_syntax));
    memset(type, 0, sizeof(struct object_type_syntax));
    type->name = "OBJECT IDENTIFIER";
    type->base_type = MIB_TYPE_OBJECT_IDENTIFIER;
    type->is_explicit = 1;
    type->tag = 6;
    dllist_append(types, &type);

    type = (struct object_type_syntax *)xmalloc(sizeof(struct object_type_syntax));
    memset(type, 0, sizeof(struct object_type_syntax));
    type->name = "OCTET STRING";
    type->base_type = MIB_TYPE_OCTET_STRING;
    type->is_explicit = 1;
    type->tag = 4;
    dllist_append(types, &type);

    type = (struct object_type_syntax *)xmalloc(sizeof(struct object_type_syntax));
    memset(type, 0, sizeof(struct object_type_syntax));
    type->name = "SEQUENCE";
    type->base_type = MIB_TYPE_SEQUENCE;
    type->is_explicit = 1;
    type->tag = 16;
    dllist_append(types, &type);

    type = (struct object_type_syntax *)xmalloc(sizeof(struct object_type_syntax));
    memset(type, 0, sizeof(struct object_type_syntax));
    type->name = "CHOICE";
    type->base_type = MIB_TYPE_CHOICE;
    type->is_explicit = 1;
    type->tag = 0;
    dllist_append(types, &type);

    type = (struct object_type_syntax *)xmalloc(sizeof(struct object_type_syntax));
    memset(type, 0, sizeof(struct object_type_syntax));
    type->name = "SEQUENCE OF";
    type->base_type = MIB_TYPE_SEQUENCE_OF;
    type->is_explicit = 1;
    type->tag = 16;
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
    /* printf("Importing main file %s\n", filename); */
    int rc = import_from_string(content_stripped, "[a-zA-Z_][a-zA-Z0-9_-]*", mib, dir, &error, &imports_parsed);
    if (rc < 0) {
        free(mib);
        fprintf(stderr, "File `%s' import error: %s\n", filename, error);
        return NULL;
    }
    return mib;
}

static struct oid *find_oid_by_value_destructively(char *string, struct oid *root, char **errorptr)
{
    char *dot = strchr(string, '.');
    unsigned long long number;
    if (parse_num(string, &number, errorptr) < 0) {
        return NULL;
    }
    if (! dot) {
        if (root->value == (int)number) {
            return root;
        }
    } else {
        *dot = '\0';
        if (root->value != (int)number) {
            return NULL;
        }
        struct oid *oid;
        struct oid **oidptr;
        dllist_foreach(oidptr, root->children) {
            oid = find_oid_by_value(dot + 1, *oidptr, errorptr);
            if (oid) {
                return oid;
            }
        }
    }
    return NULL;
}

struct oid *find_oid_by_value(char *string, struct oid *root, char **errorptr)
{
    char *copy = xstrdup(string);
    struct oid *result = find_oid_by_value_destructively(copy, root, errorptr);
    free(copy);
    return result;
}

struct oid *find_oid_by_name(char *oid_name, struct oid *root)
{
    if (strcmp(root->name, oid_name) == 0) {
        return root;
    }
    struct oid **oidptr;
    dllist_foreach(oidptr, root->children) {
        struct oid *oid = find_oid_by_name(oid_name, *oidptr);
        if (oid) {
            return oid;
        }
    }
    return NULL;
}

struct oid *find_oid(char *oid, struct oid *root, char **errorptr)
{
    struct regex *re = regex_prepare("^[0-9]+(\\.[0-9]+)*$", errorptr);
    if (! re) {
        return NULL;
    }
    struct oid *result;
    if (regex_match(re, oid, strlen(oid), 0, 0, errorptr) < 0) {
        result = find_oid_by_name(oid, root);
        if (! result) {
            snprintf(mib_error_buf, MIB_ERROR_BUFSIZE, "OID with name %s was not found", oid);
            *errorptr = mib_error_buf;
        }
    } else {
        result = find_oid_by_value(oid, root, errorptr);
    }
    regex_free(re);
    return result;
}

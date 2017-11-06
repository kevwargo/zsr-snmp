#ifndef __MIBTREE_H_INCLUDED_
#define __MIBTREE_H_INCLUDED_

#define CODING_IMPLICIT 1

struct range {
    char *low;
    char *high;
    struct range *next;
};

struct coding {
    int type;
    int value;
};

struct object_syntax {
    char *name;
    int is_sequence;
    struct range range;
    struct coding coding;
};

struct object_type {
    struct object_syntax syntax;
    int access;
    int status;
    char *description;
};

struct oid {
    int value;
    char *name;
    struct dllist *children;
    struct object_type *type;
};

struct mibtree {
    struct oid *root_oid;
    struct dllist *types;
};

enum {
    STATE_MIB_OIDS_INIT = 0,
    STATE_MIB_OIDS_NEXT,
    STATE_MIB_OIDS_END,
    STATE_MIB_TYPE_INIT,
    STATE_MIB_TYPE_PART,
    STATE_MIB_TYPE_END,

    MIB_STATE_COUNT
};

enum {
    TOKEN_MIB_BRACE_OPEN = 0,
    TOKEN_MIB_BRACE_CLOSE,
    TOKEN_MIB_TYPE_PART,
    TOKEN_MIB_TYPE_PART_LAST,
    TOKEN_MIB_OID,

    MIB_TOKEN_COUNT
};

extern int parse_symbol(char *name, char *content);

#endif

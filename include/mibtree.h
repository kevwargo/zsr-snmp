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

extern int parse_oid(const char *content, char *name, struct oid *target);

#endif

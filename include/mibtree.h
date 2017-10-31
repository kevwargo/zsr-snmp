#ifndef __MIBTREE_H_INCLUDED_
#define __MIBTREE_H_INCLUDED_

struct oid {
    int value;
    char *name;
    int is_object_type;
    union {
        struct dllist *children;
        struct object_type *type;
    } u;
};

#endif

#ifndef __MIBTREE_H_INCLUDED_
#define __MIBTREE_H_INCLUDED_


#define TYPE_REGEX \
    "OCTET" SPACE "+STRING|" \
    "OBJECT" SPACE "+IDENTIFIER|" \
    "[a-zA-Z_][a-zA-Z0-9_-]*"
    
#define BUILD_TYPE_INPLACE_REGEX(GROUP_seq_of, GROUP_type, GROUP_size_low, GROUP_size_high, GROUP_range_low, GROUP_range_high) \
    "(" \
      "(?<" GROUP_seq_of ">SEQUENCE" SPACE "+" "OF" SPACE "+" ")?" \
      "(?<" GROUP_type ">" TYPE_REGEX ")" \
      "(" \
       SPACE "*" \
       "(" \
        "(\\(" SPACE "*" "SIZE" SPACE "*" \
            "\\(" SPACE "*(?<" GROUP_size_low ">[0-9]+)" SPACE "*" \
              "(\\.\\." SPACE "*" "(?<" GROUP_size_high ">[0-9]+)" SPACE "*)?\\)" \
          SPACE "*\\))|" \
        "(" \
            "\\(" SPACE "*(?<" GROUP_range_low ">[0-9]+)" SPACE "*" \
              "(\\.\\." SPACE "*(?<" GROUP_range_high ">[0-9]+)" SPACE "*)?\\)" \
        ")" \
       ")" \
      ")?" \
    ")"

#define BUILD_TYPE_REGEX(GROUP_visibility, GROUP_type_id, GROUP_implexpl, GROUP_seq_of, GROUP_type, GROUP_size_low, GROUP_size_high, GROUP_range_low, GROUP_range_high) \
    "(" \
      "\\[" \
        "(?<" GROUP_visibility ">UNIVERSAL|APPLICATION|CONTEXT-SPECIFIC|PRIVATE)" SPACE "+" \
        "(?<" GROUP_type_id ">[0-9]+)" SPACE "*" \
      "\\]" SPACE "+" \
      "(?<" GROUP_implexpl ">(IM|EX)PLICIT)" SPACE "+" \
    ")?" \
    BUILD_TYPE_INPLACE_REGEX(GROUP_seq_of, GROUP_type, GROUP_size_low, GROUP_size_high, GROUP_range_low, GROUP_range_high)


enum mib_base_type {
    MIB_TYPE_NULL,
    MIB_TYPE_INTEGER,
    MIB_TYPE_OBJECT_IDENTIFIER,
    MIB_TYPE_OCTET_STRING,
    MIB_TYPE_SEQUENCE,
    MIB_TYPE_CHOICE,
    MIB_TYPE_SEQUENCE_OF
};

/* enum access { */
/*     ACCESS_NOT_ACCESSIBLE, */
/*     ACCESS_READ_ONLY, */
/*     ACCESS_WRITE_ONLY, */
/*     ACCESS_READ_WRITE */
/* }; */

struct range {
    char *low;
    char *high;
};

struct container_type_item {
    char *name;
    struct object_type_syntax *type;
};

struct object_type_syntax {
    char *name;
    struct object_type_syntax *parent;
    char *visibility;
    int type_id;
    char *implexpl;
    enum mib_base_type base_type;
    union {
        struct range *range;
        struct dllist *components;
        struct object_type_syntax *seq_type;
    } u;
};

struct object_type {
    struct object_type_syntax *syntax;
    /* int access; */
    /* int status; */
    char *access;
    char *status;
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

extern int parse_symbol(char *name, char *content, struct mibtree *mib, char **errorptr);
extern struct mibtree *import_file(char *filename);
extern void print_oidtree(struct oid *tree);
extern void print_types(struct dllist *types);

#endif

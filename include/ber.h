#ifndef __BER_H_INCLUDED_
#define __BER_H_INCLUDED_

#include "dllist.h"
#include "mibtree.h"


struct oid_type_data {
    unsigned long long length;
    unsigned char *buffer;
    struct dllist *components;
};


extern unsigned char *ber_encode(struct oid *oid, char *value, char **errorptr);

#endif

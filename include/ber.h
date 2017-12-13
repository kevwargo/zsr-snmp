#ifndef __BER_H_INCLUDED_
#define __BER_H_INCLUDED_

#include "dllist.h"
#include "mibtree.h"


struct oid_type_data {
    struct object_type_syntax *syntax;
    unsigned char *tagbuf;
    unsigned int taglen;
    unsigned char *lenbuf;
    unsigned int lenlen;
    unsigned char *databuf;
    unsigned long long datalen;
};


extern unsigned char *ber_encode(struct oid *oid, char *value, char **errorptr);

#endif

#ifndef __BER_H_INCLUDED_
#define __BER_H_INCLUDED_

#include "dllist.h"
#include "mibtree.h"


extern ssize_t ber_encode(struct oid *oid, char *value, unsigned char **bufptr, char **errorptr);

#endif

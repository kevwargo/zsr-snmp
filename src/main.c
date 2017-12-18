#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "dllist.h"
#include "regex.h"
#include "imports.h"
#include "mibtree.h"
#include "utils.h"
#include "ber.h"


static void __attribute__((__unused__)) test_encode(char *filename, char *oidstr, char *data)
{
    struct mibtree *mib = import_file(filename);
    if (! mib) {
        fprintf(stderr, "File %s import error\n", filename);
        exit(1);
    }

    char *error;
    struct oid *oid = find_oid(oidstr, mib->root_oid, &error);

    if (! oid) {
        fprintf(stderr, "find_oid error: %s\n", error);
        exit(1);
    }

    unsigned char *buffer;
    ssize_t size = ber_encode(oid, data, &buffer, &error);
    if (size < 0) {
        fprintf(stderr, "ber_encode error: %s\n", error);
        exit(1);
    }
    exit(write(1, buffer, size));
}

int main(int argc, char **argv)
{
    test_encode(argv[1], argv[2], argv[3]);
    return 0;
}

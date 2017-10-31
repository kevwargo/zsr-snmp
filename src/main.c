#include <stdio.h>
#include "dllist.h"
#include "parse.h"
#include "imports.h"

int main(int argc,  char * const *argv)
{
    struct imports *imports = parse_imports(argv[1]);
    struct imports_file_entry *file;
    dllist_foreach(file, imports->files) {
        printf("file %s:\n", file->name);
        char **defptr;
        dllist_foreach(defptr, file->definitions) {
            printf("\t%s\n", *defptr);
        }
    }
}

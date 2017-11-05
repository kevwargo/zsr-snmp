#include <stdio.h>
#include <string.h>
#include "dllist.h"
#include "parse.h"
#include "imports.h"
#include "mibtree.h"
#include "utils.h"

static void __attribute__((__unused__)) print_imports(char *filename)
{
    char *error;
    struct imports *imports = parse_imports(read_file(filename), &error);
    if (! imports) {
        fprintf(stderr, "Import parse error: %s\n", error);
        exit(1);
    }
    struct imports_file_entry **fileptr;
    dllist_foreach(fileptr, imports->files) {
        printf("file %s:\n", (*fileptr)->name);
        char **defptr;
        dllist_foreach(defptr, (*fileptr)->definitions) {
            printf("\t%s\n", *defptr);
        }
    }
    free_imports(imports);
}

static void __attribute__((__unused__)) test_oid(char *filename, char *name)
{
    struct oid oid;
    oid.value = 1;
    oid.name = "iso";
    oid.children = dllist_create();
    oid.type = NULL;
    if (parse_oid((const char *)read_file(filename), name, &oid) < 0) {
        exit(1);
    }
    
}

static int __attribute__((__unused__)) capture_count(char *regex)
{
    const char *error;
    int erroffset;
    int groups;
    pcre *re = pcre_compile(regex, 0, &error, &erroffset, NULL);
    if (! re) {
        fprintf(stderr, "pcre_compile error `%s' at %d\n", error, erroffset);
        return -1;
    }
    int rc = pcre_fullinfo(re, NULL, PCRE_INFO_CAPTURECOUNT, &groups);
    if (rc < 0) {
        fprintf(stderr, "pcre_fullinfo error: %s\n", pcre_strerror(rc));
        pcre_free(re);
        return rc;
    }
    pcre_free(re);
    return groups;
}

static void __attribute__((__unused__)) test_re(char *regex, char *subject, int offset)
{
    const char *error;
    int erroffset;
    pcre *re = pcre_compile(regex, 0, &error, &erroffset, NULL);
    if (! re) {
        fprintf(stderr, "pcre_compile error `%s' at %d\n", error, erroffset);
        exit(-1);
    }
    int ovecsize;
    int rc = pcre_fullinfo(re, NULL, PCRE_INFO_CAPTURECOUNT, &ovecsize);
    if (rc < 0) {
        fprintf(stderr, "pcre_fullinfo error: %s\n", pcre_strerror(rc));
        pcre_free(re);
        exit(rc);
    }
    ovecsize++;
    ovecsize *= 3;
    int *ovector = (int *)malloc(sizeof(int) * ovecsize);
    rc = pcre_exec(re, NULL, subject, strlen(subject), offset, PCRE_ANCHORED, ovector, ovecsize);
    if (rc < 0) {
        fprintf(stderr, "pcre_exec: %s\n", pcre_strerror(rc));
        exit(1);
    }
    for (int i = 0; i < rc; i++) {
        printf("[%d %d] ", ovector[i*2], ovector[i*2 + 1]);
    }
    putchar('\n');
}

static void __attribute__((__unused__)) test_import(char *filename)
{
    struct oid mib;
    mib.value = 1;
    mib.name = "iso";
    mib.children = dllist_create();
    mib.type = NULL;
    if (import_file(filename, &mib) < 0) {
        fprintf(stderr, "FILE IMPORT ERROR: %s\n", filename);
        exit(1);
    }
}

int main(int argc, char **argv)
{
    /* test_oid(argv[1], argv[2]); */
    /* printf("%d\n", capture_count(argv[1])); */
    /* test_re(argv[1], argv[2], atoi(argv[3])); */
    /* test_import(argv[1]); */
    print_imports(argv[1]);
    return 0;
}

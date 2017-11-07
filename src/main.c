#include <stdio.h>
#include <string.h>
#include "dllist.h"
#include "regex.h"
#include "imports.h"
#include "mibtree.h"
#include "utils.h"

static void __attribute__((__unused__)) print_imports(char *filename)
{
    char *error;
    char *content = read_file(filename);
    char *subject = remove_comments(content);
    struct imports *imports = parse_imports(subject, &error);
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
    free(subject);
    free(content);
}

static void __attribute__((__unused__)) test_oid(char *filename, char *name)
{
    /* struct oid oid; */
    /* oid.value = 1; */
    /* oid.name = "iso"; */
    /* oid.children = dllist_create(); */
    /* oid.type = NULL; */
    /* if (parse_oid(read_file(filename), name, &oid) < 0) { */
    /*     exit(1); */
    /* } */
    
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
    int *ovector = (int *)xmalloc(sizeof(int) * ovecsize);
    rc = pcre_exec(re, NULL, subject, strlen(subject), offset, 0, ovector, ovecsize);
    if (rc < 0) {
        fprintf(stderr, "pcre_exec: %s\n", pcre_strerror(rc));
        exit(1);
    }
    printf("rc = %d\n", rc);
    for (int i = 0; i < ovecsize / 3; i++) {
        char *s;
        pcre_get_substring(subject, ovector, ovecsize / 3, i, (const char **)&s);
        printf("%d) [%d %d]: '%s'\n", i, ovector[i*2], ovector[i*2 + 1], s);
        pcre_free_substring(s);
    }
    putchar('\n');
}

static void __attribute__((__unused__)) test_import(char *filename)
{
    struct mibtree *mib = import_file(filename);
    if (! mib) {
        fprintf(stderr, "File %s import error\n", filename);
        exit(1);
    }
    print_oidtree(mib->root_oid);
    print_types(mib->types);
}

static void __attribute__((__unused__)) test_mibtree(char *filename, char *name)
{
    struct mibtree *mib = NULL;
    char *error;
    if (parse_symbol(name, remove_comments(read_file(filename)), mib, &error) < 0) {
        fprintf(stderr, "parse_symbol failed: %s\n", error);
        exit(1);
    }
}

static void __attribute__((__unused__)) test_regex(char *pattern)
{
    char *error;
    struct regex *regex = regex_prepare(pattern, &error);
    if (! regex) {
        fprintf(stderr, "regex_prepare failed: %s\n", error);
        exit(1);
    }
    for (int i = 0; i < regex->named_count; i++) {
        printf("%d: %s\n", regex->named[i].num, regex->named[i].name);
    }
}


int main(int argc, char **argv)
{
    /* test_oid(argv[1], argv[2]); */
    /* printf("%d\n", capture_count(argv[1])); */
    /* test_re(argv[1], argv[2], atoi(argv[3])); */
    /* test_regex(argv[1]); */
    test_import(argv[1]);
    /* print_imports(argv[1]); */
    /* puts(remove_comments(read_file(argv[1]))); */
    /* test_mibtree(argv[1], argv[2]); */
    return 0;
}

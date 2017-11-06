#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

void *xmalloc(size_t size);


char *read_file(char *filename)
{
    struct stat st;
    int fd;
    char *buf;
    if (stat(filename, &st) < 0) {
        return NULL;
    }
    fd = open(filename, O_RDONLY);
    if (fd < 0) {
        return NULL;
    }
    buf = (char *)xmalloc(st.st_size + 1);
    if (! buf) {
        close(fd);
        return NULL;
    }
    size_t size = read(fd, buf, st.st_size);
    close(fd);
    if (size < 0) {
        free(buf);
        close(fd);
        return NULL;
    }
    buf[size] = '\0';
    return buf;
}

char *xasprintf(char *fmt, ...)
{
    char *result;
    int rc;
    va_list args;
    va_start(args, fmt);
    rc = vasprintf(&result, fmt, args);
    va_end(args);
    if (rc < 0) {
        perror("vasprintf");
        exit(1);
    }
    return result;
}

void *xmalloc(size_t size)
{
    void *result = malloc(size);
    if (! result) {
        perror("malloc");
        exit(1);
    }
    return result;
}

void *xcalloc(size_t nmemb, size_t size)
{
    void *result = calloc(nmemb, size);
    if (! result) {
        perror("calloc");
        exit(1);
    }
    return result;
}

char *xstrdup(char *string)
{
    char *result = strdup(string);
    if (! result) {
        perror("strdup");
        exit(1);
    }
    return result;
}

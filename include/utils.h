#ifndef __UTILS_H_INCLUDED_
#define __UTILS_H_INCLUDED_


extern char *read_file(char *filename);
extern void *xmalloc(size_t size);
extern void *xcalloc(size_t nmemb, size_t size);
extern char *xasprintf(char *fmt, ...);
extern char *xstrdup(char *string);

#endif

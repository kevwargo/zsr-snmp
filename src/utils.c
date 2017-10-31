#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>


char *read_file(char *filename)
{
    struct stat st;
    int fd;
    char *buf;
    if (stat(filename, &st) < 0) {
        perror("stat");
        exit(1);
    }
    fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("open");
        exit(1);
    }
    buf = (char *)malloc(st.st_size + 1);
    if (! buf) {
        perror("malloc");
        exit(1);
    }
    size_t size = read(fd, buf, st.st_size);
    close(fd);
    if (size < 0) {
        perror("read");
        exit(1);
    }
    buf[size] = '\0';
    return buf;
}

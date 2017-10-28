#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pcre.h>
#include "parse.h"
#include "dllist.h"


struct imports_file_entry {
    char *name;
    struct dllist *definitions;
};

struct imports {
    enum imports_state state;
    struct dllist *files;
    struct imports_file_entry *current_file;
};


static char *read_file(char *filename)
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

void regexp_scan(const char *subject, struct dllist *token_specs, token_handler_t handler, void *data)
{
    int token_count = token_specs->size;
    struct token *tokens = (struct token *)malloc(sizeof(struct token) * token_count);
    int i = 0;
    struct token_spec *ts;
    dllist_foreach(ts, token_specs) {
        const char *error;
        int erroffset;
        tokens[i].spec = ts;
        tokens[i].re = pcre_compile(ts->re, PCRE_ANCHORED, &error, &erroffset, NULL);
        if (! tokens[i].re) {
            fprintf(stderr, "pcre_compile error: %s\n", error);
            exit(1);
        }
        int rc = pcre_fullinfo(tokens[i].re, NULL, PCRE_INFO_CAPTURECOUNT, &tokens[i].ovecsize);
        if (rc < 0) {
            fprintf(stderr, "pcre_fullinfo error code: %d\n", rc);
            exit(1);
        }
        tokens[i].ovecsize++;
        tokens[i].ovecsize *= 3;
        tokens[i].ovector = (int *)malloc(sizeof(int) * tokens[i].ovecsize);
        i++;
    }

    int len = strlen(subject);
    int pos = 0;
    int finished = 0;
    while (! finished) {
        int matched = 0;
        for (int i = 0; i < token_count && !matched; i++) {
            int rc = pcre_exec(tokens[i].re, NULL, subject + pos, len, 0, 0, tokens[i].ovector, tokens[i].ovecsize);
            if (rc == PCRE_ERROR_NOMATCH) {
                continue;
            }
            if (rc < 0) {
                fprintf(stderr, "pcre_exec error code: %d\n", rc);
                exit(1);
            }
            matched = 1;
            tokens[i].subject = subject + pos;
            pcre_get_substring(subject + pos, tokens[i].ovector, rc, 0, &tokens[i].match);
            if (handler(&tokens[i], data)) {
                finished = 1;
            }
            pcre_free_substring(tokens[i].match);
            pos += tokens[i].ovector[1];
            len -= tokens[i].ovector[1];
        }
        if (! matched) {
            fprintf(stderr, "None of the provided tokens were found\n");
            exit(1);
        }
    }
}

static char *find_imports(char *content)
{
    const char *error;
    int erroffset;
    pcre *re = pcre_compile("^[ \t]*IMPORTS[ \t\n\r]+", PCRE_MULTILINE, &error, &erroffset, NULL);
    if (! re) {
        fprintf(stderr, "pcre_compile error: %s\n", error);
        exit(1);
    }

    int ovector[3];
    int rc = pcre_exec(re, NULL, content, strlen(content), 0, 0, ovector, 3);
    if (rc < 0) {
        if (rc == PCRE_ERROR_NOMATCH) {
            fprintf(stderr, "No IMPORTS found\n");
        } else {
            fprintf(stderr, "IMPORTS match failed with code %d\n", rc);
        }
        exit(1);
    }
    return content + ovector[1];
}

static int handle_imports_token(token, imports)
    struct token *token;
    struct imports *imports;
{
    char *definition;
    switch (token->spec->type) {
        case IDENTIFIER:
            switch (imports->state) {
                case BEFORE_DEFINITION: case AFTER_FILENAME:
                    definition = strdup(token->match);
                    dllist_append(imports->current_file->definitions, &definition);
                    imports->state = AFTER_DEFINITION;
                    break;
                case BEFORE_FILENAME:
                    imports->current_file->name = strdup(token->match);
                    dllist_append(imports->files, imports->current_file);
                    imports->current_file->name = NULL;
                    imports->current_file->definitions = dllist_create();
                    imports->state = AFTER_FILENAME;
                    break;
                default:
                    fprintf(stderr, "Unexpected identifier %s\n", token->match);
                    exit(1);
            }
            break;
        case COMMA:
            if (imports->state == AFTER_DEFINITION) {
                imports->state = BEFORE_DEFINITION;
            } else {
                fprintf(stderr, "Unexpected token COMMA\n");
                exit(1);
            }
            break;
        case KW_FROM:
            if (imports->state == AFTER_DEFINITION) {
                imports->state = BEFORE_FILENAME;
            } else {
                fprintf(stderr, "Unexpected token KW_FROM\n");
                exit(1);
            }
            break;
        case SEMICOLON:
            if (imports->state == AFTER_FILENAME) {
                return 1;
            } else {
                fprintf(stderr, "Unexpected token SEMICOLON\n");
                exit(1);
            }
            break;
        case WHITESPACE:
            break;
        default:
            fprintf(stderr, "Unexpected token type %d\n", token->spec->type);
            exit(1);
    }
    return 0;
}

static struct imports *parse_imports(char *filename)
{
    struct dllist *tokens = dllist_create();
    struct token_spec ts;

    ts.type = KW_FROM;
    ts.re = "FROM";
    dllist_append(tokens, &ts);

    ts.type = IDENTIFIER;
    ts.re = "[a-zA-Z0-9_-]+";
    dllist_append(tokens, &ts);

    ts.type = COMMA;
    ts.re = ",";
    dllist_append(tokens, &ts);

    ts.type = SEMICOLON;
    ts.re = ";";
    dllist_append(tokens, &ts);

    ts.type = WHITESPACE;
    ts.re = "[ \t\n\r]+";
    dllist_append(tokens, &ts);

    char *content = find_imports(read_file(filename));

    struct imports *imports = (struct imports *)malloc(sizeof(struct imports));
    imports->state = BEFORE_DEFINITION;
    imports->files = dllist_create();
    imports->current_file = (struct imports_file_entry *)malloc(sizeof(struct imports_file_entry));
    imports->current_file->name = NULL;
    imports->current_file->definitions = dllist_create();
    regexp_scan(content, tokens, handle_imports_token, imports);

    return imports;
}

int main(int argc, char **argv)
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

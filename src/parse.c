#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pcre.h>
#include "parse.h"
#include "dllist.h"
#include "utils.h"


char *pcre_strerror(int code)
{
    switch (code) {
        case 0:
            return "SUCCESS";
        case PCRE_ERROR_NOMATCH:
            return "PCRE_ERROR_NOMATCH";
        case PCRE_ERROR_NULL:
            return "PCRE_ERROR_NULL";
        case PCRE_ERROR_BADOPTION:
            return "PCRE_ERROR_BADOPTION";
        case PCRE_ERROR_BADMAGIC:
            return "PCRE_ERROR_BADMAGIC";
        case PCRE_ERROR_UNKNOWN_OPCODE:
            return "PCRE_ERROR_UNKNOWN_OPCODE";
#if PCRE_ERROR_UNKNOWN_NODE != PCRE_ERROR_UNKNOWN_OPCODE
        case PCRE_ERROR_UNKNOWN_NODE:
            return "PCRE_ERROR_UNKNOWN_NODE";
#endif
        case PCRE_ERROR_NOMEMORY:
            return "PCRE_ERROR_NOMEMORY";
        case PCRE_ERROR_NOSUBSTRING:
            return "PCRE_ERROR_NOSUBSTRING";
        case PCRE_ERROR_MATCHLIMIT:
            return "PCRE_ERROR_MATCHLIMIT";
        case PCRE_ERROR_CALLOUT:
            return "PCRE_ERROR_CALLOUT";
        case PCRE_ERROR_BADUTF8:
            return "PCRE_ERROR_BADUTF8";
#if PCRE_ERROR_BADUTF16 != PCRE_ERROR_BADUTF8
        case PCRE_ERROR_BADUTF16:
            return "PCRE_ERROR_BADUTF16";
#endif
#if PCRE_ERROR_BADUTF32 != PCRE_ERROR_BADUTF16 && PCRE_ERROR_BADUTF32 != PCRE_ERROR_BADUTF8
        case PCRE_ERROR_BADUTF32:
            return "PCRE_ERROR_BADUTF32";
#endif
        case PCRE_ERROR_BADUTF8_OFFSET:
            return "PCRE_ERROR_BADUTF8_OFFSET";
#if PCRE_ERROR_BADUTF16_OFFSET != PCRE_ERROR_BADUTF8_OFFSET
        case PCRE_ERROR_BADUTF16_OFFSET:
            return "PCRE_ERROR_BADUTF16_OFFSET";
#endif
        case PCRE_ERROR_PARTIAL:
            return "PCRE_ERROR_PARTIAL";
        case PCRE_ERROR_BADPARTIAL:
            return "PCRE_ERROR_BADPARTIAL";
        case PCRE_ERROR_INTERNAL:
            return "PCRE_ERROR_INTERNAL";
        case PCRE_ERROR_BADCOUNT:
            return "PCRE_ERROR_BADCOUNT";
        case PCRE_ERROR_DFA_UITEM:
            return "PCRE_ERROR_DFA_UITEM";
        case PCRE_ERROR_DFA_UCOND:
            return "PCRE_ERROR_DFA_UCOND";
        case PCRE_ERROR_DFA_UMLIMIT:
            return "PCRE_ERROR_DFA_UMLIMIT";
        case PCRE_ERROR_DFA_WSSIZE:
            return "PCRE_ERROR_DFA_WSSIZE";
        case PCRE_ERROR_DFA_RECURSE:
            return "PCRE_ERROR_DFA_RECURSE";
        case PCRE_ERROR_RECURSIONLIMIT:
            return "PCRE_ERROR_RECURSIONLIMIT";
        case PCRE_ERROR_NULLWSLIMIT:
            return "PCRE_ERROR_NULLWSLIMIT";
        case PCRE_ERROR_BADNEWLINE:
            return "PCRE_ERROR_BADNEWLINE";
        case PCRE_ERROR_BADOFFSET:
            return "PCRE_ERROR_BADOFFSET";
        case PCRE_ERROR_SHORTUTF8:
            return "PCRE_ERROR_SHORTUTF8";
#if PCRE_ERROR_SHORTUTF16 != PCRE_ERROR_SHORTUTF8
        case PCRE_ERROR_SHORTUTF16:
            return "PCRE_ERROR_SHORTUTF16";
#endif
        case PCRE_ERROR_RECURSELOOP:
            return "PCRE_ERROR_RECURSELOOP";
        case PCRE_ERROR_JIT_STACKLIMIT:
            return "PCRE_ERROR_JIT_STACKLIMIT";
        case PCRE_ERROR_BADMODE:
            return "PCRE_ERROR_BADMODE";
        case PCRE_ERROR_BADENDIANNESS:
            return "PCRE_ERROR_BADENDIANNESS";
        case PCRE_ERROR_DFA_BADRESTART:
            return "PCRE_ERROR_DFA_BADRESTART";
        case PCRE_ERROR_JIT_BADOPTION:
            return "PCRE_ERROR_JIT_BADOPTION";
        case PCRE_ERROR_BADLENGTH:
            return "PCRE_ERROR_BADLENGTH";
        case PCRE_ERROR_UNSET:
            return "PCRE_ERROR_UNSET";
        default:
            return "UNKNOWN_PCRE_ERROR";
    }
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


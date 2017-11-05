#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pcre.h>

#include "regex.h"
#include "dllist.h"
#include "utils.h"

#define ERRBUF_SIZE 1024

static char regex_errbuf[ERRBUF_SIZE];

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


struct regex *regex_prepare(char *pattern, char **errorptr)
{
    struct regex *regex = (struct regex *)malloc(sizeof(struct regex));
    char *error;
    int erroffset;
    pcre *re = pcre_compile(pattern, PCRE_MULTILINE, (const char **)&error, &erroffset, NULL);
    if (! re) {
        snprintf(regex_errbuf, ERRBUF_SIZE, "pcre_compile error: `%s' at %d", error, erroffset);
        *errorptr = regex_errbuf;
        return NULL;
    }
    pcre_extra *extra = pcre_study(re, PCRE_STUDY_EXTRA_NEEDED, (const char **)&error);
    if (! extra) {
        pcre_free(re);
        snprintf(regex_errbuf, ERRBUF_SIZE, "pcre_study error: `%s'", error);
        *errorptr = regex_errbuf;
        return NULL;
    }
    int capture_count;
    int rc = pcre_fullinfo(re, extra, PCRE_INFO_CAPTURECOUNT, &capture_count);
    if (rc < 0) {
        pcre_free_study(extra);
        pcre_free(re);
        *errorptr = pcre_strerror(rc);
        return NULL;
    }
    regex->re = re;
    regex->extra = extra;
    regex->ovecsize = (capture_count + 1) * 3;
    regex->ovector = (int *)malloc(sizeof(int) * regex->ovecsize);
    return regex;
}

void regex_free(struct regex *regex)
{
    pcre_free_study(regex->extra);
    pcre_free(regex->re);
    free(regex->ovector);
    free(regex);
}

int regex_match(struct regex *regex, char *subject, int length, int startoffset, int options, char **errorptr)
{
    int rc = pcre_exec(regex->re, regex->extra, subject, length, startoffset, options, regex->ovector, regex->ovecsize);
    if (rc < 0) {
        *errorptr = pcre_strerror(rc);
    }
    return rc;
}

char *regex_get_match(struct regex *regex, char *subject, int group, char **errorptr)
{
    char *result;
    int rc = pcre_get_substring(subject, regex->ovector, regex->ovecsize / 3, group, (const char **)&result);
    if (rc < 0) {
        *errorptr = pcre_strerror(rc);
        return NULL;
    }
    return result;
}

static struct token *prepare_tokens(struct parser *parser, char *subject, char **tokens_re, char **errorptr)
{
    struct token *tokens = (struct token *)calloc(parser->token_count, sizeof(struct token));
    for (int i = 0; i < parser->token_count; i++) {
        tokens[i].pattern = tokens_re[i];
        tokens[i].subject = subject;
        tokens[i].regex = regex_prepare(tokens_re[i], errorptr);
        if (! tokens[i].regex) {
            for (int j = 0; j < i; j++) {
                regex_free(tokens[j].regex);
            }
            free(tokens);
            return NULL;
        }
    }
    return tokens;
}

static void free_tokens(struct token *tokens, int token_count)
{
    for (int i = 0; i < token_count; i++) {
        regex_free(tokens[i].regex);
    }
    free(tokens);
}

static int match_start_token(struct parser *parser, char *subject, int len, int *posptr, void *data, char **errorptr)
{
    struct token start_token;
    start_token.pattern = parser->start_pattern;
    start_token.subject = subject;
    start_token.regex = regex_prepare(parser->start_pattern, errorptr);
    if (! start_token.regex) {
        return REGEX_PARSE_ERROR;
    }
    int rc = regex_match(start_token.regex, subject, len, *posptr, 0, errorptr);
    if (rc < 0) {
        regex_free(start_token.regex);
        return REGEX_PARSE_START_TOKEN_NOT_FOUND_ERROR;
    }
    if (parser->start_token_handler) {
        rc = parser->start_token_handler(&start_token, -1, &parser->state, data, errorptr);
        if (rc < 0) {
            regex_free(start_token.regex);
            return REGEX_PARSE_ERROR;
        } else if (rc > 0) {
            regex_free(start_token.regex);
            return 1;
        }
    }
    *posptr = start_token.regex->ovector[1];
    regex_free(start_token.regex);
    return 0;
}

int regex_parse(struct parser *parser, char *subject, char **tokens_re, void *data, char **errorptr)
{
    int pos = 0;
    int len = strlen(subject);
    /* printf("pos = %d\n", pos); */
    int rc = match_start_token(parser, subject, len, &pos, data, errorptr);
    /* printf("pos after = %d\n", pos); */
    if (rc < 0) {
        return rc;
    }
    if (rc > 0) {
        return 0;
    }
    struct token *tokens = prepare_tokens(parser, subject, tokens_re, errorptr);
    if (! tokens) {
        return REGEX_PARSE_ERROR;
    }
    int finished = 0;
    while (! finished) {
        char *error;
        int matched = 0;
        for (int i = 0; i < parser->token_count && !matched; i++) {
            /* int limit = 20; */
            /* printf("matching %s at %*.*s\n", tokens[i].pattern, limit, limit, subject + pos); */
            rc = regex_match(tokens[i].regex, subject, len, pos, PCRE_ANCHORED, &error);
            if (rc == PCRE_ERROR_NOMATCH) {
                /* printf("no match\n"); */
                continue;
            }
            if (rc < 0) {
                *errorptr = error;
                return rc;
            }
            matched = 1;
            /* printf("matched\n"); */
            token_handler_t handler = parser->handlers[i][parser->state];
            rc = handler(&tokens[i], i, &parser->state, data, errorptr);
            if (rc < 0) {
                free_tokens(tokens, parser->token_count);
                return rc;
            } else if (rc > 0) {
                finished = 1;
            }
            pos = tokens[i].regex->ovector[1];
        }
        if (! matched) {
            free_tokens(tokens, parser->token_count);
            *errorptr = "Unexpected end of file during parsing";
            return REGEX_PARSE_ERROR;
        }
    }
    free_tokens(tokens, parser->token_count);
    return 0;
}

static int unexpected_token_handler(struct token *token, int token_num, int *stateptr, void *data, char **errorptr)
{
    char *match = regex_get_match(token->regex, token->subject, 0, errorptr);
    if (! match) {
        return REGEX_PARSE_UNEXPECTED_TOKEN_GET_ERROR;
    }
    snprintf(regex_errbuf, ERRBUF_SIZE, "Unexpected token `%s' for state %d", match, *stateptr);
    pcre_free_substring(match);
    *errorptr = regex_errbuf;
    return REGEX_PARSE_UNEXPECTED_TOKEN_ERROR;
}

int ignore_token_handler(struct token *token, int token_num, int *stateptr, void *data, char **errorptr)
{
    return 0;
}

token_handler_t **init_handlers(int token_count, int state_count)
{
    token_handler_t **handlers = (token_handler_t **)malloc(token_count * sizeof(token_handler_t *));
    for (int i = 0; i < token_count; i++) {
        handlers[i] = (token_handler_t *)malloc(state_count * sizeof(token_handler_t));
        for (int j = 0; j < state_count; j++) {
            handlers[i][j] = unexpected_token_handler;
        }
    }
    return handlers;
}

char *remove_comments(char *subject)
{
    char *error;
    struct regex *comment = regex_prepare("(^|[^\"a-zA-Z0-9_-])(--.*?(--|$))", &error);
    if (! comment) {
        fprintf(stderr, "Comment regex prepare: %s\n", error);
        return NULL;
    }
    struct regex *string = regex_prepare("\"[^\"]*\"", &error);
    if (! string) {
        fprintf(stderr, "String regex prepare: %s\n", error);
        return NULL;
    }
    int len = strlen(subject);
    int pos = 0;
    int finished = 0;
    char *result = strdup(subject);
    while (! finished) {
        int rc = regex_match(comment, result + pos, len - pos, 0, 0, &error);
        if (rc == PCRE_ERROR_NOMATCH) {
            finished = 1;
        } else if (rc < 0) {
            fprintf(stderr, "Comment regex match: %s\n", error);
            free(result);
            return NULL;
        } else {
            rc = regex_match(string, result, len, pos, 0, &error);
            if (rc < 0 && rc != PCRE_ERROR_NOMATCH) {
                fprintf(stderr, "String regex match: %s\n", error);
                free(result);
                return NULL;
            }
            if (rc > 0) {
                comment->ovector[0] += pos;
                comment->ovector[1] += pos;
                comment->ovector[4] += pos;
                comment->ovector[5] += pos;
                if (string->ovector[0] < comment->ovector[0]) {
                    pos = string->ovector[1];
                    continue;
                }
            }
            for (int i = comment->ovector[4]; i < comment->ovector[5]; i++) {
                result[i] = ' ';
            }
            pos = comment->ovector[1];
        }        
    }
    return result;
}

#include "lexer.h"
#include "dynstring.h"
#include <stddef.h>
#include<stdlib.h>

static int consume_single_quoted(const char *line, size_t *i, DynString *out) {
    (*i)++;  // skip opening
    while (line[*i] != '\'') {
        if (line[*i] == '\0')return -1; // Unterminated quote case
        ds_append_char(out, line[*i]);
        (*i)++;
    }
    (*i)++;
    return 0;
}

static int consume_double_quoted(const char *line, size_t *i, DynString *out) {
    (*i)++;
    while (line[*i] != '"') {
        if (line[*i] == '\0')return -1;
        if (line[*i] == '\\') {
            char next = line[*i + 1];
            if (next == '\0')return -1;  // trailing backslash, nothing to escape
            if (next == '"' || next == '\\') {
                ds_append_char(out, next);  /* / \" -> ", \\ -> \ */
                *i += 2;
            } else {
                ds_append_char(out, '\\');  // not special: keep both chars
                ds_append_char(out, next);
                *i += 2;
            }
        } else {
            ds_append_char(out, line[*i]);
            (*i)++;
        }
    }
    (*i)++;
    return 0;
}

static int consume_escape(const char *line, size_t *i, DynString *out) {
    (*i)++;
    if (line[*i] == '\0' || line[*i] == '\n' || line[*i] == '\r') {
        return -1; 
    }
    ds_append_char(out, line[*i]);
    (*i)++;
    return 0;
}

static char *consume_word(const char *line, size_t *i) {
    DynString ds;
    ds_init(&ds);

    while (line[*i] != '\0' &&
           line[*i] != ' ' && line[*i] != '\t' &&
           line[*i] != '\n' && line[*i] != '\r' &&
           line[*i] != '|' && line[*i] != '&' &&
           line[*i] != ';' && line[*i] != '<' && line[*i] != '>') {

        int ok;
        if (line[*i] == '\'') {
            ok = consume_single_quoted(line, i, &ds);
        } else if (line[*i] == '"') {
            ok = consume_double_quoted(line, i, &ds);
        } else if (line[*i] == '\\') {
            ok = consume_escape(line, i, &ds);
        } else {
            ds_append_char(&ds, line[*i]);
            (*i)++;
            ok = 0;
        }

        if (ok != 0) {
            ds_free(&ds);
            return NULL;
        }
    }

    return ds.data;
}

static TokenType consume_operator(const char *line, size_t *i) {
    char c = line[*i];

    if (c == '>' && line[*i + 1] == '>') {
        *i += 2;
        return TOK_GTGT;
    }

    (*i)++;
    switch (c) {
        case '|': return TOK_PIPE;
        case '&': return TOK_AMP;
        case ';': return TOK_SEMI;
        case '<': return TOK_LT;
        case '>': return TOK_GT;
    }
    return TOK_WORD;
}
Token *lexer_tokenize(const char *line, size_t *count) {
    size_t capacity = 8;
    size_t n = 0;
    Token *tokens = malloc(capacity * sizeof(Token));

    size_t i = 0;
    while (line[i] != '\0') {
        if (line[i] == ' ' || line[i] == '\t' ||
            line[i] == '\n' || line[i] == '\r') {
            i++;
            continue;
        }

        Token tok;

        if (line[i] == '|' || line[i] == '&' || line[i] == ';' ||
            line[i] == '<' || line[i] == '>') {
            tok.type = consume_operator(line, &i);
            tok.value = NULL;
        } else {
            char *word = consume_word(line, &i);
            if (word == NULL) {
                for (size_t k = 0; k < n; k++) free(tokens[k].value);
                free(tokens);
                return NULL;
            }
            tok.type = TOK_WORD;
            tok.value = word;
        }

        if (n + 1 > capacity) {
            capacity *= 2;
            tokens = realloc(tokens, capacity * sizeof(Token));
        }
        tokens[n] = tok;
        n++;
    }

    *count = n;
    return tokens;
}
#ifndef LEXER_H
#define LEXER_H

#include <stddef.h>

typedef enum {
    TOK_WORD,      // a command, argument, or filename
    TOK_PIPE,      //  |
    TOK_AMP,       //  &
    TOK_SEMI,      //  ;
    TOK_LT,        //  <
    TOK_GT,        //  >
    TOK_GTGT       //  >>
} TokenType;

typedef struct Token{
    TokenType type;
    char *value;
} Token;

Token *lexer_tokenize(const char *line, size_t *count);

#endif /* LEXER_H */
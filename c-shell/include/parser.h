#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include <stddef.h>

int parser_validate(const Token *tokens, size_t count); // Return 1 if line valid. else return 0

#endif /* PARSER_H */
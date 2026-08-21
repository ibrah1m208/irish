#ifndef DYNSTRING_H
#define DYNSTRING_H

#include <stddef.h>  // includes size_t. no need for entire stdlib


/*
A dynamic Null terminatable string buffer
used by Lexer [A3] to tokenize
*/

typedef struct {
    char *data;
    size_t len; // Doesnt include `\0`, is 1-indexed
    size_t capacity;
} DynString;

void ds_init(DynString *ds);
void ds_append_char(DynString *ds, char c);
void ds_free(DynString *ds);

#endif // DYNSTRING_H
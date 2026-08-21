#include "dynstring.h"
#include "shell.h"
#include <stdlib.h>

void ds_init(DynString *ds) {
    ds->capacity = DYNSTRING_DEFAULT_CAPACITY;
    ds->data = malloc(ds->capacity);
    ds->len = 0;
    ds->data[0] = '\0';
}

void ds_append_char(DynString *ds, char c) {
    // +2: one byte for the new char, one for the null terminator
    if (ds->len + 2 > ds->capacity) {
        ds->capacity *= 2;
        ds->data = realloc(ds->data, ds->capacity);
    }
    ds->data[ds->len] = c;
    ds->len++;
    ds->data[ds->len] = '\0';
}

void ds_free(DynString *ds) {
    free(ds->data);
    ds->data = NULL;
    ds->len = 0;
    ds->capacity = 0;
}
#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "lexer.h"
#include <stddef.h>

char *executor_resolve_path(const char *name);
int executor_run(const Token *tokens, size_t count);
int executor_execute_group(const Token *tokens, size_t count, int is_bg);

#endif /* EXECUTOR_H */
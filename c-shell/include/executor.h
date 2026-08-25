#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "lexer.h"
#include <stddef.h>
/* Resolve a command name to its executable path by
checking the literal path, cwd and $PATH
*/
char *executor_resolve_path(const char *name);

int executor_run(const Token *tokens, size_t count);

#endif /* EXECUTOR_H */
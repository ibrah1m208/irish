#ifndef BG_EXEC_H
#define BG_EXEC_H

#include "lexer.h"
#include <stddef.h>

void bg_init(void);
int bg_run(const Token *tokens, size_t count);
void bg_block_sigchld(void);
void bg_unblock_sigchld(void);

#endif /* BG_EXEC_H */

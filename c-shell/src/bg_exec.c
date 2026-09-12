#include "bg_exec.h"
#include "executor.h"
#include "lexer.h"
#include "term_control.h"

#include <stdio.h>
#include <stdlib.h>

void bg_init(void) {
    term_control_init();
}

void bg_block_sigchld(void) {
    term_block_sigchld();
}

void bg_unblock_sigchld(void) {
    term_unblock_sigchld();
}

int bg_run(const Token *tokens, size_t count) {
    if (tokens == NULL || count == 0) {
        return 0;
    }

    size_t start = 0;

    while (start < count) {
        size_t end = start;
        while (end < count && tokens[end].type != TOK_AMP) {
            end++;
        }

        size_t group_len = end - start;

        if (group_len > 0) {
            if (end < count && tokens[end].type == TOK_AMP) {
                executor_execute_group(tokens + start, group_len, 1);
                end++;
            } else {
                executor_execute_group(tokens + start, group_len, 0);
            }
        } else if (end < count && tokens[end].type == TOK_AMP) {
            end++;
        }

        start = end;
    }

    return 0;
}

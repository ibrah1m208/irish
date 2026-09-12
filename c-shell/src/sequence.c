#include "sequence.h"
#include "executor.h"
#include <stddef.h>

int sequence_run(const Token *tokens, size_t count) {
    if (tokens == NULL || count == 0) {
        return 0;
    }

    size_t start = 0;

    while (start < count) {
        /* Find the next ';' */
        size_t end = start;

        while (end < count && tokens[end].type != TOK_SEMI) {
            end++;
        }

        /* Execute non-empty command group */
        if (end > start) {
            int ret = executor_run(tokens + start, end - start);

            /* Stop if the command/group failed */
            if (ret != 0) {
                return ret;
            }
        }

        /* Skip ';' */
        if (end < count && tokens[end].type == TOK_SEMI) {
            end++;
        }

        start = end;
    }

    return 0;
}

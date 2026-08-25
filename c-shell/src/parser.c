#include "parser.h"

typedef enum {
    STATE_LINE,      // start state
    STATE_ARG,
    STATE_NEED_WORD,  // reached after <, >, >>, |, ;
    STATE_BG          // reached after &
} ParserState;

int parser_validate(const Token *tokens, size_t count) {
    ParserState state = STATE_LINE;

    for (size_t i = 0; i < count; i++) {
        TokenType t = tokens[i].type;

        switch (state) {
            case STATE_LINE:
                if (t == TOK_WORD) {
                    state = STATE_ARG;
                } else {
                    return 0;
                }
                break;

            case STATE_ARG:
                switch (t) {
                    case TOK_WORD:
                        state = STATE_ARG;
                        break;
                    case TOK_LT:
                    case TOK_GT:
                    case TOK_GTGT:
                    case TOK_PIPE:
                    case TOK_SEMI:
                        state = STATE_NEED_WORD;
                        break;
                    case TOK_AMP:
                        state = STATE_BG;
                        break;
                }
                break;

            case STATE_NEED_WORD:
                if (t == TOK_WORD) {
                    state = STATE_ARG;
                } else {
                    return 0;
                }
                break;

            case STATE_BG:
                if (t == TOK_WORD) {
                    state = STATE_ARG;
                } else {
                    return 0;
                }
                break;
        }
    }
    return state == STATE_LINE || state == STATE_ARG || state == STATE_BG;
}
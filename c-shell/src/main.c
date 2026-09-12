#include "shell.h"
#include "prompt.h"
#include "lexer.h"
#include "parser.h"
#include "executor.h"
#include "hop.h"
#include "bg_exec.h"
#include "term_control.h"

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    printf("initiate C shell\n\n\n"); // Change name to Irish after assignment

    char *session_home_dir = getcwd(NULL, 0);
    hop_init(session_home_dir);
    term_control_init();
    int runtime = 1;
    char *line = NULL;
    size_t n = 0;

    while (runtime) {
        print_prompt(session_home_dir);
        if (getline(&line, &n, stdin) == -1) {
            if (term_handle_ctrl_d()) {
                job_kill_all_sighup();
                runtime = 0;
                break;
            } else {
                clearerr(stdin);
                continue;
            }
        }

        term_reset_ctrl_d();

        size_t token_count;
        Token *tokens = lexer_tokenize(line, &token_count);
        
        if (tokens == NULL) {
            printf("cshell: invalid syntax\n");
            continue;
        }

        if (!parser_validate(tokens, token_count)) {
            printf("cshell: invalid syntax\n");
            for (size_t i = 0; i < token_count; i++) free(tokens[i].value);
            free(tokens);
            continue;
        }

        if (token_count == 0) {
            free(tokens);
            continue;
        }

        if (token_count == 1 && strcmp(tokens[0].value, "exit") == 0) {
            job_kill_all_sighup();
            for (size_t i = 0; i < token_count; i++) free(tokens[i].value);
            free(tokens);
            runtime = 0;
            break;
        }

        executor_run(tokens, token_count);

        for (size_t i = 0; i < token_count; i++) free(tokens[i].value);
        free(tokens);
    }

    job_kill_all_sighup();
    free(line);
    free(session_home_dir);
    return 0;
}
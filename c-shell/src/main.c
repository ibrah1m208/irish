#include "shell.h"
#include "prompt.h"
#include "lexer.h"
#include "parser.h"
#include "executor.h"

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    printf("initiate C shell\n\n\n"); // Change name to Irish after assignment

    char *session_home_dir = getcwd(NULL, 0);
    int runtime = 1;
    char *line = NULL;
    size_t n = 0;

    while (runtime) {
        print_prompt(session_home_dir);
        if (getline(&line, &n, stdin) == -1){
            runtime = 0;
            break;
        }

        size_t token_count;
        Token *tokens = lexer_tokenize(line, &token_count);
        
        if (tokens == NULL){
            printf("cshell: invalid syntax\n");
            continue;
        }

        if (!parser_validate(tokens, token_count)){
            printf("cshell: invalid syntax\n");
            for (size_t i = 0; i < token_count; i++) free(tokens[i].value);
            free(tokens);
            continue;
        }

        if (token_count == 0) {
            free(tokens);
            continue;
        }

        if (token_count == 1 && strcmp(tokens[0].value, "exit") == 0){
            for (size_t i = 0; i < token_count; i++) free(tokens[i].value);
            free(tokens);
            runtime = 0;
            break;
        }

        executor_run(tokens, token_count);

        for (size_t i = 0; i < token_count; i++) free(tokens[i].value);
        free(tokens);
    }

    free(line);
    free(session_home_dir);
    return 0;
}
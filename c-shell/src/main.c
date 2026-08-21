#include "shell.h"
#include "prompt.h"

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    printf("initiate C shell\n\n\n"); // Change name to Irish after assignment
    char *session_home_dir = getcwd(NULL, 0);
    char *line = NULL;
    size_t n = 0;

    int runtime = 1;
    while (runtime) {
        print_prompt(session_home_dir);
        if (getline(&line, &n, stdin) == -1)runtime = 0;

        if (strncmp("exit", line, 4) == 0) runtime = 0;
    }

    free(line);
    free(session_home_dir);
    return 0;
}
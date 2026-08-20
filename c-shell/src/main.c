#include<stdio.h>
#include<stdlib.h>
#include<pwd.h>
#include<unistd.h>
#include<limits.h>
#include<stdlib.h>
#include<string.h>

char *format_prompt_path(const char *cwd, const char *home) {
    size_t home_len = strlen(home);
    if (strncmp(cwd, home, home_len) == 0) {
        char next_char = cwd[home_len];

        if (next_char == '\0') { // Check if cwd is $HOME
            return strdup("~");
        }
        if (next_char == '/') { // Check if $HOME is ancestor to cwd
            char *result = malloc(1 + strlen(cwd + home_len) + 1);
            result[0] = '~';
            strcpy(result + 1, cwd + home_len);
            return result;
        }
    }
    // Else
    return strdup(cwd);
}

void print_prompt(const char *home_dir) {
    struct passwd *pw = getpwuid(getuid()); //i208
    char hostname[256];
    char cwd[PATH_MAX];

    gethostname(hostname, sizeof(hostname)); //ASUSVivobook
    getcwd(cwd, sizeof(cwd));

    char *display_path = format_prompt_path(cwd, home_dir);

    printf("<%s@%s:%s> ", pw ? pw->pw_name : "unknown", hostname, display_path);
    fflush(stdout); //flush any unsolicited "\n"

    free(display_path);
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    printf("initiate C shell\n\n\n");
    char *session_home_dir = getcwd(NULL, 0);
    int runtime = 1;
    char *line = NULL;
    size_t n = 0;
    while(runtime) {
        print_prompt(session_home_dir);
        if(getline(&line, &n, stdin) == -1) {
            runtime = 0;
            break;
        }
        if(strncmp("exit", line, 4) == 0) runtime = 0;
    }


    free(line);
    free(session_home_dir);
    return 0;
}
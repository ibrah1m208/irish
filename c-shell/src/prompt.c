#include "prompt.h"
#include "shell.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>

char *format_prompt_path(const char *cwd, const char *home) {
    size_t home_len = strlen(home);
    if (strncmp(cwd, home, home_len) == 0) {
        char next_char = cwd[home_len];
        if (next_char == '\0') return strdup("~"); // Exact match
        if (next_char == '/'){ // Ancestor
            char *result = malloc(1 + strlen(cwd + home_len) + 1);
            result[0] = '~';
            strcpy(result + 1, cwd + home_len);
            return result;
        }
    }
    return strdup(cwd);
}

void print_prompt(const char *home_dir) {
    struct passwd *pw = getpwuid(getuid());
    char hostname[HOSTNAME_MAX_LEN];
    char cwd[PATH_MAX];

    gethostname(hostname, sizeof(hostname));
    getcwd(cwd, sizeof(cwd));
    char *display_path = format_prompt_path(cwd, home_dir);
    
    printf("<%s@%s:%s> ", pw ? pw->pw_name : "unknown", hostname, display_path);
    fflush(stdout); // flush any unsolicited /n

    free(display_path);
}
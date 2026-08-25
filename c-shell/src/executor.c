#include "executor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

static int is_executable(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0)return 0;
    if (S_ISDIR(st.st_mode))return 0;
    if (access(path, X_OK) != 0)return 0;
    
    return 1;
}

static char *join_path(const char *dir, const char *name) {
    size_t dlen = strlen(dir);
    size_t nlen = strlen(name);
    int need_slash = (dlen > 0 && dir[dlen - 1] == '/') ? 0 : 1;
    size_t len = dlen + nlen + (need_slash ? 1 : 0) + 1;
    char *result = malloc(len);
    if (!result) {
        return NULL;
    }
    if (need_slash) {
        snprintf(result, len, "%s/%s", dir, name);
    } else {
        snprintf(result, len, "%s%s", dir, name);
    }
    return result;
}

char *executor_resolve_path(const char *name) {
    int force_path_search = 0;
    if (name[0] == '%') {
        force_path_search = 1;
        name = name + 1;  // ignore %
    }

    // if contains '/' treat as literal path 
    if (!force_path_search && strchr(name, '/') != NULL) { // unless forced_path_search = 1
        if (is_executable(name)) {
            return strdup(name);
        }
        return NULL;
    }

    // check CWD
    if (!force_path_search) {
        char cwd[4096];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            char *candidate = join_path(cwd, name);
            if (candidate != NULL) {
                if (is_executable(candidate)) {
                    return candidate;
                }
                free(candidate);
            }
        }
    }

    // search $PATH
    const char *path_env = getenv("PATH"); 
    if (path_env == NULL) {
        return NULL;
    }

    char *path_copy = strdup(path_env);
    if (path_copy == NULL) {
        return NULL;
    }

    char *saveptr = NULL;
    char *dir = strtok_r(path_copy, ":", &saveptr);

    while (dir != NULL) {
        char *candidate = join_path(dir, name);
        if (candidate != NULL) {
            if (is_executable(candidate)) {
                free(path_copy);
                return candidate;
            }
            free(candidate);
        }
        dir = strtok_r(NULL, ":", &saveptr);
    }

    free(path_copy);
    return NULL;
}

int executor_run(const Token *tokens, size_t count) {
    if (tokens == NULL || count == 0) {
        return 0;
    }

    size_t group_len = 0;
    while (group_len < count && tokens[group_len].type != TOK_SEMI && tokens[group_len].type != TOK_AMP)group_len++;
    if (group_len == 0)return 0;

    //  make argv for the command
    char **argv = malloc((group_len + 1) * sizeof(char *));
    if (!argv) return -1;

    for (size_t i = 0; i < group_len; i++) {
        argv[i] = tokens[i].value;
    }
    argv[group_len] = NULL;

    if (argv[0] == NULL) {
        free(argv);
        return 0;
    }

    const char *raw_name = argv[0];
    const char *cmd_name = (raw_name[0] == '%') ? raw_name + 1 : raw_name;

    char *resolved_path = executor_resolve_path(raw_name);
    if (resolved_path == NULL) {
        printf("cshell: command not found (%s)\n", cmd_name);
        free(argv);
        return -1;
    }

    // Pass command name without '%' as argv[0]
    argv[0] = (char *)cmd_name;

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        free(resolved_path);
        free(argv);
        return -1;
    } else if (pid == 0) { // Child process
        execv(resolved_path, argv);
        perror("execv");
        _exit(1);
    } else { // Parent process
        int status = 0;
        waitpid(pid, &status, 0);
        free(resolved_path);
        free(argv);
        return 0;
    }
}
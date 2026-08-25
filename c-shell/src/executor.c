#include "executor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

static int is_executable(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return 0;
    }
    if (S_ISDIR(st.st_mode)) {
        return 0;
    }
    if (access(path, X_OK) != 0) {
        return 0;
    }
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
        name = name + 1;  /* advance past the '%' */
    }

    /* Rule 1: if name contains '/', treat as literal path (unless '%' forced PATH search) */
    if (!force_path_search && strchr(name, '/') != NULL) {
        if (is_executable(name)) {
            return strdup(name);
        }
        return NULL;
    }

    /* Rule 2: check CWD first, unless '%' forced us to skip it */
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

    /* Rule 3: search $PATH, in order */
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
    return NULL;  /* Rule 4: not found anywhere */
}

int executor_run(const Token *tokens, size_t count) {
    if (tokens == NULL || count == 0) {
        return 0;
    }

    /* For Part C: when processing commands with sequential (;) or background (&)
     * operators, execute only the first command group and ignore the rest. */
    size_t group_len = 0;
    while (group_len < count &&
           tokens[group_len].type != TOK_SEMI &&
           tokens[group_len].type != TOK_AMP) {
        group_len++;
    }

    if (group_len == 0) {
        return 0;
    }

    /* Parse command arguments and input redirection files */
    char **argv = malloc((group_len + 1) * sizeof(char *));
    char **input_files = malloc((group_len + 1) * sizeof(char *));
    if (!argv || !input_files) {
        free(argv);
        free(input_files);
        return -1;
    }

    size_t argc = 0;
    size_t num_inputs = 0;

    for (size_t i = 0; i < group_len; i++) {
        if (tokens[i].type == TOK_LT) {
            if (i + 1 < group_len && tokens[i + 1].type == TOK_WORD) {
                input_files[num_inputs++] = tokens[i + 1].value;
                i++;  /* skip the filename token */
            }
        } else if (tokens[i].type == TOK_WORD) {
            argv[argc++] = tokens[i].value;
        }
    }
    argv[argc] = NULL;

    if (argc == 0) {
        free(argv);
        free(input_files);
        return 0;
    }

    /* Open all input redirection files to verify they exist and are readable */
    int *input_fds = NULL;
    if (num_inputs > 0) {
        input_fds = malloc(num_inputs * sizeof(int));
        if (!input_fds) {
            free(argv);
            free(input_files);
            return -1;
        }
        for (size_t i = 0; i < num_inputs; i++) {
            input_fds[i] = open(input_files[i], O_RDONLY);
            if (input_fds[i] < 0) {
                printf("cshell: no such file or directory\n");
                for (size_t j = 0; j < i; j++) {
                    close(input_fds[j]);
                }
                free(input_fds);
                free(argv);
                free(input_files);
                return -1;
            }
        }
    }

    const char *raw_name = argv[0];
    const char *cmd_name = (raw_name[0] == '%') ? raw_name + 1 : raw_name;

    char *resolved_path = executor_resolve_path(raw_name);
    if (resolved_path == NULL) {
        printf("cshell: command not found (%s)\n", cmd_name);
        if (input_fds) {
            for (size_t i = 0; i < num_inputs; i++) {
                close(input_fds[i]);
            }
            free(input_fds);
        }
        free(argv);
        free(input_files);
        return -1;
    }

    /* Pass command name without '%' as argv[0] */
    argv[0] = (char *)cmd_name;

    if (num_inputs == 0) {
        /* No input redirection: normal execution */
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            free(resolved_path);
            free(argv);
            free(input_files);
            return -1;
        } else if (pid == 0) {
            execv(resolved_path, argv);
            perror("execv");
            _exit(1);
        } else {
            int status = 0;
            waitpid(pid, &status, 0);
        }
    } else if (num_inputs == 1) {
        /* Single input redirection: redirect STDIN_FILENO to input_fds[0] */
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            close(input_fds[0]);
            free(input_fds);
            free(resolved_path);
            free(argv);
            free(input_files);
            return -1;
        } else if (pid == 0) {
            if (dup2(input_fds[0], STDIN_FILENO) < 0) {
                perror("dup2");
                close(input_fds[0]);
                _exit(1);
            }
            close(input_fds[0]);
            execv(resolved_path, argv);
            perror("execv");
            _exit(1);
        } else {
            close(input_fds[0]);
            free(input_fds);
            input_fds = NULL;
            int status = 0;
            waitpid(pid, &status, 0);
        }
    } else {
        /* Multiple input redirections: create pipe and feeder child process */
        int pipefd[2];
        if (pipe(pipefd) < 0) {
            perror("pipe");
            for (size_t i = 0; i < num_inputs; i++) close(input_fds[i]);
            free(input_fds);
            free(resolved_path);
            free(argv);
            free(input_files);
            return -1;
        }

        pid_t feeder_pid = fork();
        if (feeder_pid < 0) {
            perror("fork");
            close(pipefd[0]);
            close(pipefd[1]);
            for (size_t i = 0; i < num_inputs; i++) close(input_fds[i]);
            free(input_fds);
            free(resolved_path);
            free(argv);
            free(input_files);
            return -1;
        } else if (feeder_pid == 0) {
            /* Feeder child: reads each file in order and writes to pipe */
            close(pipefd[0]);
            signal(SIGPIPE, SIG_IGN);
            char buf[4096];
            for (size_t i = 0; i < num_inputs; i++) {
                ssize_t bytes_read;
                while ((bytes_read = read(input_fds[i], buf, sizeof(buf))) > 0) {
                    ssize_t written = 0;
                    while (written < bytes_read) {
                        ssize_t w = write(pipefd[1], buf + written, bytes_read - written);
                        if (w <= 0) {
                            goto feeder_cleanup;
                        }
                        written += w;
                    }
                }
                close(input_fds[i]);
                input_fds[i] = -1;
            }
        feeder_cleanup:
            for (size_t i = 0; i < num_inputs; i++) {
                if (input_fds[i] >= 0) close(input_fds[i]);
            }
            close(pipefd[1]);
            _exit(0);
        }

        /* In parent: close input_fds since feeder child owns them now */
        for (size_t i = 0; i < num_inputs; i++) {
            close(input_fds[i]);
        }
        free(input_fds);
        input_fds = NULL;

        /* Fork command process */
        pid_t cmd_pid = fork();
        if (cmd_pid < 0) {
            perror("fork");
            close(pipefd[0]);
            close(pipefd[1]);
            waitpid(feeder_pid, NULL, 0);
            free(resolved_path);
            free(argv);
            free(input_files);
            return -1;
        } else if (cmd_pid == 0) {
            close(pipefd[1]);
            if (dup2(pipefd[0], STDIN_FILENO) < 0) {
                perror("dup2");
                close(pipefd[0]);
                _exit(1);
            }
            close(pipefd[0]);
            execv(resolved_path, argv);
            perror("execv");
            _exit(1);
        }

        /* In parent: close pipe ends and wait for both processes */
        close(pipefd[0]);
        close(pipefd[1]);

        int status = 0;
        waitpid(cmd_pid, &status, 0);
        waitpid(feeder_pid, NULL, 0);
    }

    if (input_fds) {
        free(input_fds);
    }
    free(resolved_path);
    free(argv);
    free(input_files);
    return 0;
}
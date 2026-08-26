#include "executor.h"
#include "pipes.h"
#include "redirect.h"
#include "hop.h"
#include "reveal.h"
#include "peek.h"
#include "locate.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
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

    /* Count number of pipeline stages */
    size_t num_cmds = 1;
    for (size_t i = 0; i < group_len; i++) {
        if (tokens[i].type == TOK_PIPE) {
            num_cmds++;
        }
    }

    /* Allocate commands array */
    SingleCommand *cmds = calloc(num_cmds, sizeof(SingleCommand));
    if (!cmds) {
        return -1;
    }

    for (size_t c = 0; c < num_cmds; c++) {
        cmds[c].argv = malloc((group_len + 1) * sizeof(char *));
        cmds[c].input_files = malloc((group_len + 1) * sizeof(char *));
        cmds[c].output_files = malloc((group_len + 1) * sizeof(OutputRedir));
        if (!cmds[c].argv || !cmds[c].input_files || !cmds[c].output_files) {
            for (size_t k = 0; k <= c; k++) {
                free(cmds[k].argv);
                free(cmds[k].input_files);
                free(cmds[k].output_files);
            }
            free(cmds);
            return -1;
        }
        cmds[c].argc = 0;
        cmds[c].num_inputs = 0;
        cmds[c].num_outputs = 0;
    }

    /* Parse tokens into commands */
    size_t cmd_idx = 0;
    for (size_t i = 0; i < group_len; i++) {
        if (tokens[i].type == TOK_PIPE) {
            cmds[cmd_idx].argv[cmds[cmd_idx].argc] = NULL;
            cmd_idx++;
        } else if (tokens[i].type == TOK_LT) {
            if (i + 1 < group_len && tokens[i + 1].type == TOK_WORD) {
                cmds[cmd_idx].input_files[cmds[cmd_idx].num_inputs++] = tokens[i + 1].value;
                i++;
            }
        } else if (tokens[i].type == TOK_GT) {
            if (i + 1 < group_len && tokens[i + 1].type == TOK_WORD) {
                cmds[cmd_idx].output_files[cmds[cmd_idx].num_outputs].filename = tokens[i + 1].value;
                cmds[cmd_idx].output_files[cmds[cmd_idx].num_outputs].is_append = 0;
                cmds[cmd_idx].num_outputs++;
                i++;
            }
        } else if (tokens[i].type == TOK_GTGT) {
            if (i + 1 < group_len && tokens[i + 1].type == TOK_WORD) {
                cmds[cmd_idx].output_files[cmds[cmd_idx].num_outputs].filename = tokens[i + 1].value;
                cmds[cmd_idx].output_files[cmds[cmd_idx].num_outputs].is_append = 1;
                cmds[cmd_idx].num_outputs++;
                i++;
            }
        } else if (tokens[i].type == TOK_WORD) {
            cmds[cmd_idx].argv[cmds[cmd_idx].argc++] = tokens[i].value;
        }
    }
    cmds[cmd_idx].argv[cmds[cmd_idx].argc] = NULL;

    if (num_cmds == 1 && cmds[0].argc > 0 &&
        (strcmp(cmds[0].argv[0], "hop") == 0 ||
         strcmp(cmds[0].argv[0], "reveal") == 0 ||
         strcmp(cmds[0].argv[0], "peek") == 0 ||
         strcmp(cmds[0].argv[0], "locate") == 0)) {
        int ret = 0;
        int saved_stdout = -1;
        int saved_stdin = -1;
        int in_fd = -1;
        int out_fd = -1;
        pid_t feeder_pid = -1;
        pid_t dist_pid = -1;

        if (cmds[0].num_inputs > 0) {
            if (redirect_setup_input(cmds[0].input_files, cmds[0].num_inputs, &in_fd, &feeder_pid) < 0) {
                ret = -1;
                goto builtin_cleanup;
            }
            if (in_fd >= 0) {
                saved_stdin = dup(STDIN_FILENO);
                dup2(in_fd, STDIN_FILENO);
                close(in_fd);
            }
        }

        if (cmds[0].num_outputs > 0) {
            if (redirect_setup_output(cmds[0].output_files, cmds[0].num_outputs, &out_fd, &dist_pid) < 0) {
                if (saved_stdin >= 0) {
                    dup2(saved_stdin, STDIN_FILENO);
                    close(saved_stdin);
                }
                ret = -1;
                goto builtin_cleanup;
            }
            if (out_fd >= 0) {
                saved_stdout = dup(STDOUT_FILENO);
                dup2(out_fd, STDOUT_FILENO);
                close(out_fd);
            }
        }

        if (strcmp(cmds[0].argv[0], "hop") == 0) {
            ret = hop_builtin(cmds[0].argc, cmds[0].argv);
        } else if (strcmp(cmds[0].argv[0], "reveal") == 0) {
            ret = reveal_builtin(cmds[0].argc, cmds[0].argv);
        } else if (strcmp(cmds[0].argv[0], "peek") == 0) {
            ret = peek_builtin(cmds[0].argc, cmds[0].argv);
        } else if (strcmp(cmds[0].argv[0], "locate") == 0) {
            ret = locate_builtin(cmds[0].argc, cmds[0].argv);
        }

        if (saved_stdin >= 0) {
            dup2(saved_stdin, STDIN_FILENO);
            close(saved_stdin);
        }
        if (saved_stdout >= 0) {
            dup2(saved_stdout, STDOUT_FILENO);
            close(saved_stdout);
        }
        if (feeder_pid > 0) {
            waitpid(feeder_pid, NULL, 0);
        }
        if (dist_pid > 0) {
            waitpid(dist_pid, NULL, 0);
        }

builtin_cleanup:
        for (size_t c = 0; c < num_cmds; c++) {
            free(cmds[c].argv);
            free(cmds[c].input_files);
            free(cmds[c].output_files);
        }
        free(cmds);
        return ret;
    }

    //  Delegate pipeline execution to pipes module
    int ret = pipes_execute_pipeline(cmds, num_cmds);

    // free all memory
    for (size_t c = 0; c < num_cmds; c++) {
        free(cmds[c].argv);
        free(cmds[c].input_files);
        free(cmds[c].output_files);
    }
    free(cmds);
    return ret;
}

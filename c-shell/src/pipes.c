#include "pipes.h"
#include "executor.h"
#include "redirect.h"
#include "hop.h"
#include "reveal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int pipes_execute_pipeline(SingleCommand *cmds, size_t num_cmds) {
    if (cmds == NULL || num_cmds == 0) {
        return 0;
    }

    int (*pipe_fds)[2] = NULL;
    if (num_cmds > 1) {
        pipe_fds = malloc((num_cmds - 1) * sizeof(int[2]));
        if (!pipe_fds) {
            return -1;
        }
        for (size_t i = 0; i < num_cmds - 1; i++) {
            if (pipe(pipe_fds[i]) < 0) {
                perror("pipe");
                for (size_t j = 0; j < i; j++) {
                    close(pipe_fds[j][0]);
                    close(pipe_fds[j][1]);
                }
                free(pipe_fds);
                return -1;
            }
        }
    }

    pid_t *child_pids = malloc((num_cmds * 3 + 1) * sizeof(pid_t));
    size_t num_pids = 0;

    for (size_t i = 0; i < num_cmds; i++) {
        if (cmds[i].argc == 0) {
            continue;
        }

        // Setup Input Redirection
        int in_fd = -1;
        pid_t feeder_pid = -1;
        if (redirect_setup_input(cmds[i].input_files, cmds[i].num_inputs, &in_fd, &feeder_pid) < 0) {
            continue;
        }
        if (feeder_pid > 0 && child_pids) {
            child_pids[num_pids++] = feeder_pid;
        }

        //Output Redirection
        int out_fd = -1;
        pid_t dist_pid = -1;
        if (redirect_setup_output(cmds[i].output_files, cmds[i].num_outputs, &out_fd, &dist_pid) < 0) {
            if (in_fd >= 0) close(in_fd);
            continue;
        }
        if (dist_pid > 0 && child_pids) {
            child_pids[num_pids++] = dist_pid;
        }

        // Resolve command path
        const char *raw_name = cmds[i].argv[0];
        const char *cmd_name = (raw_name[0] == '%') ? raw_name + 1 : raw_name;
        int is_hop = (strcmp(cmd_name, "hop") == 0);
        int is_reveal = (strcmp(cmd_name, "reveal") == 0);
        int is_builtin = is_hop || is_reveal;
        char *resolved_path = NULL;

        if (!is_builtin) {
            resolved_path = executor_resolve_path(raw_name);
            if (resolved_path == NULL) {
                printf("cshell: command not found (%s)\n", cmd_name);
                if (in_fd >= 0) close(in_fd);
                if (out_fd >= 0) close(out_fd);
                continue;
            }
        }

        cmds[i].argv[0] = (char *)cmd_name;

        // Fork command process
        pid_t cmd_pid = fork();
        if (cmd_pid < 0) {
            perror("fork");
            if (resolved_path) free(resolved_path);
            if (in_fd >= 0) close(in_fd);
            if (out_fd >= 0) close(out_fd);
            continue;
        } else if (cmd_pid == 0) {
            if (in_fd >= 0) {
                if (dup2(in_fd, STDIN_FILENO) < 0) {
                    perror("dup2");
                    _exit(1);
                }
                close(in_fd);
            } else if (i > 0) {
                if (dup2(pipe_fds[i - 1][0], STDIN_FILENO) < 0) {
                    perror("dup2");
                    _exit(1);
                }
            }

            if (out_fd >= 0) {
                if (dup2(out_fd, STDOUT_FILENO) < 0) {
                    perror("dup2");
                    _exit(1);
                }
                close(out_fd);
            } else if (i < num_cmds - 1) {
                if (dup2(pipe_fds[i][1], STDOUT_FILENO) < 0) {
                    perror("dup2");
                    _exit(1);
                }
            }

            if (pipe_fds) {
                for (size_t k = 0; k < num_cmds - 1; k++) {
                    close(pipe_fds[k][0]); // close all pipelines
                    close(pipe_fds[k][1]);
                }
            }

            if (is_hop) {
                int status = hop_builtin(cmds[i].argc, cmds[i].argv);
                _exit(status == 0 ? 0 : 1);
            }

            if (is_reveal) {
                int status = reveal_builtin(cmds[i].argc, cmds[i].argv);
                _exit(status == 0 ? 0 : 1);
            }

            execv(resolved_path, cmds[i].argv);
            perror("execv");
            _exit(1);
        } else {
            if (child_pids) child_pids[num_pids++] = cmd_pid;
            if (resolved_path) free(resolved_path);
            if (in_fd >= 0) close(in_fd);
            if (out_fd >= 0) close(out_fd);
        }
    }

    if (pipe_fds) {
        for (size_t k = 0; k < num_cmds - 1; k++) {
            close(pipe_fds[k][0]); // close all parent pipelines
            close(pipe_fds[k][1]);
        }
        free(pipe_fds);
    }

    if (child_pids) {
        for (size_t k = 0; k < num_pids; k++) {
            int status = 0;
            waitpid(child_pids[k], &status, 0);
        }
        free(child_pids);
    }
    return 0;
}

#include "redirect.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

static void run_feeder_child(int write_fd, int *in_fds, size_t num_inputs) {
    signal(SIGPIPE, SIG_IGN);
    char buf[4096];
    for (size_t i = 0; i < num_inputs; i++) {
        ssize_t bytes_read;
        while ((bytes_read = read(in_fds[i], buf, sizeof(buf))) > 0) {
            ssize_t written = 0;
            while (written < bytes_read) {
                ssize_t w = write(write_fd, buf + written, (size_t)(bytes_read - written));
                if (w <= 0) {
                    goto feeder_cleanup;
                }
                written += w;
            }
        }
        close(in_fds[i]);
        in_fds[i] = -1;
    }
feeder_cleanup:
    for (size_t i = 0; i < num_inputs; i++) {
        if (in_fds[i] >= 0) {
            close(in_fds[i]);
        }
    }
    close(write_fd);
    _exit(0);
}

static void run_distributor_child(int read_fd, int *out_fds, size_t num_outputs) {
    signal(SIGPIPE, SIG_IGN);
    char buf[4096];
    ssize_t bytes_read;
    while ((bytes_read = read(read_fd, buf, sizeof(buf))) > 0) {
        for (size_t j = 0; j < num_outputs; j++) {
            ssize_t written = 0;
            while (written < bytes_read) {
                ssize_t w = write(out_fds[j], buf + written, (size_t)(bytes_read - written));
                if (w < 0) {
                    break;
                }
                written += w;
            }
        }
    }
    for (size_t j = 0; j < num_outputs; j++) {
        if (out_fds[j] >= 0) {
            close(out_fds[j]);
        }
    }
    close(read_fd);
    _exit(0);
}

int redirect_setup_input(char **input_files, size_t num_inputs, int *in_fd, pid_t *feeder_pid) {
    *in_fd = -1;
    *feeder_pid = -1;

    if (num_inputs == 0) {
        return 0;
    }

    int *in_fds = malloc(num_inputs * sizeof(int));
    if (!in_fds) {
        return -1;
    }

    for (size_t j = 0; j < num_inputs; j++) {
        in_fds[j] = open(input_files[j], O_RDONLY);
        if (in_fds[j] < 0) {
            printf("cshell: no such file or directory\n");
            for (size_t k = 0; k < j; k++) {
                close(in_fds[k]);
            }
            free(in_fds);
            return -1;
        }
    }

    if (num_inputs == 1) {
        *in_fd = in_fds[0];
        free(in_fds);
        return 0;
    }

    int in_pipe[2];
    if (pipe(in_pipe) < 0) {
        perror("pipe");
        for (size_t j = 0; j < num_inputs; j++) {
            close(in_fds[j]);
        }
        free(in_fds);
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        close(in_pipe[0]);
        close(in_pipe[1]);
        for (size_t j = 0; j < num_inputs; j++) {
            close(in_fds[j]);
        }
        free(in_fds);
        return -1;
    } else if (pid == 0) {
        close(in_pipe[0]);
        run_feeder_child(in_pipe[1], in_fds, num_inputs);
    }

    for (size_t j = 0; j < num_inputs; j++) {
        close(in_fds[j]);
    }
    free(in_fds);
    close(in_pipe[1]);

    *in_fd = in_pipe[0];
    *feeder_pid = pid;
    return 0;
}

int redirect_setup_output(const OutputRedir *output_files, size_t num_outputs, int *out_fd, pid_t *dist_pid) {
    *out_fd = -1;
    *dist_pid = -1;

    if (num_outputs == 0) {
        return 0;
    }

    int *out_fds = malloc(num_outputs * sizeof(int));
    if (!out_fds) {
        return -1;
    }

    for (size_t j = 0; j < num_outputs; j++) {
        int flags = O_WRONLY | O_CREAT | (output_files[j].is_append ? O_APPEND : O_TRUNC);
        out_fds[j] = open(output_files[j].filename, flags, 0644);
        if (out_fds[j] < 0) {
            printf("cshell: unable to create file for writing\n");
            for (size_t k = 0; k < j; k++) {
                close(out_fds[k]);
            }
            free(out_fds);
            return -1;
        }
    }

    if (num_outputs == 1) {
        *out_fd = out_fds[0];
        free(out_fds);
        return 0;
    }

    int out_pipe[2];
    if (pipe(out_pipe) < 0) {
        perror("pipe");
        for (size_t j = 0; j < num_outputs; j++) {
            close(out_fds[j]);
        }
        free(out_fds);
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        close(out_pipe[0]);
        close(out_pipe[1]);
        for (size_t j = 0; j < num_outputs; j++) {
            close(out_fds[j]);
        }
        free(out_fds);
        return -1;
    } else if (pid == 0) {
        close(out_pipe[1]);
        run_distributor_child(out_pipe[0], out_fds, num_outputs);
    }

    for (size_t j = 0; j < num_outputs; j++) {
        close(out_fds[j]);
    }
    free(out_fds);
    close(out_pipe[0]);

    *out_fd = out_pipe[1];
    *dist_pid = pid;
    return 0;
}

#ifndef REDIRECT_H
#define REDIRECT_H

#include <stddef.h>
#include <sys/types.h>

typedef struct {
    char *filename;
    int is_append;  /* 0 for '>', 1 for '>>' */
} OutputRedir;

/* Sets up input redirection for a command.
 * If num_inputs == 0: sets *in_fd = -1, *feeder_pid = -1, returns 0.
 * If num_inputs == 1: opens file, sets *in_fd, *feeder_pid = -1, returns 0.
 * If num_inputs > 1: creates pipe and feeder process to concatenate files,
 *                    sets *in_fd = pipe read end, *feeder_pid = child pid, returns 0.
 * Returns -1 on error (e.g. file cannot be opened). */
int redirect_setup_input(char **input_files, size_t num_inputs, int *in_fd, pid_t *feeder_pid);

/* Sets up output redirection for a command.
 * If num_outputs == 0: sets *out_fd = -1, *dist_pid = -1, returns 0.
 * If num_outputs == 1: opens file, sets *out_fd, *dist_pid = -1, returns 0.
 * If num_outputs > 1: creates pipe and distributor process to broadcast to all files,
 *                     sets *out_fd = pipe write end, *dist_pid = child pid, returns 0.
 * Returns -1 on error (e.g. file cannot be created/opened). */
int redirect_setup_output(const OutputRedir *output_files, size_t num_outputs, int *out_fd, pid_t *dist_pid);

#endif /* REDIRECT_H */

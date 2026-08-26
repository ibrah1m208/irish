#ifndef PIPES_H
#define PIPES_H

#include "redirect.h"
#include <stddef.h>

typedef struct {
    char **argv;
    size_t argc;
    char **input_files;
    size_t num_inputs;
    OutputRedir *output_files;
    size_t num_outputs;
} SingleCommand;

/* Executes a pipeline of commands with input/output redirections.
 * Handles pipe creation, process forking, fd wiring, and waiting for all children. */
int pipes_execute_pipeline(SingleCommand *cmds, size_t num_cmds);

#endif /* PIPES_H */

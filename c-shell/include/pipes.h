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

int pipes_execute(SingleCommand *cmds, size_t num_cmds, int is_bg, const char *cmd_line);
int pipes_execute_pipeline(SingleCommand *cmds, size_t num_cmds);

#endif /* PIPES_H */

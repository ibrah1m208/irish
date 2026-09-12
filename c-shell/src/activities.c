#include "activities.h"
#include "term_control.h"

#include <stdio.h>

int activities_builtin(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    job_sweep();

    for (int i = 0; i < MAX_JOBS; i++) {
        Job *job = job_get_by_number(i + 1);
        if (job == NULL || !job->active) {
            continue;
        }

        printf("[%d] pgid %d\n", job->job_number, (int)job->pgid);
        const char *state_str = (job->state == JOB_STOPPED) ? "Stopped" : "Running";

        for (size_t p = 0; p < job->num_procs; p++) {
            if (!job->procs[p].exited) {
                printf("  %d %s  %s\n", (int)job->procs[p].pid, job->procs[p].cmd_name, state_str);
            }
        }
    }
    fflush(stdout);

    return 0;
}

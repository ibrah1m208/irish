#include "ping.h"
#include "term_control.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>

static int is_valid_signal_number(const char *str, long long *val_out) {
    if (str == NULL || *str == '\0') {
        return 0;
    }
    if (str[0] == '-') {
        return 0;
    }
    size_t i = 0;
    if (str[0] == '+') {
        i = 1;
        if (str[1] == '\0') {
            return 0;
        }
    }
    for (; str[i] != '\0'; i++) {
        if (str[i] < '0' || str[i] > '9') {
            return 0;
        }
    }

    char *endptr = NULL;
    errno = 0;
    long long val = strtoll(str, &endptr, 10);
    if (errno == ERANGE || *endptr != '\0' || val < 0) {
        return 0;
    }

    if (val_out != NULL) {
        *val_out = val;
    }
    return 1;
}

int ping_builtin(int argc, char *argv[]) {
    if (argc != 3) {
        printf("ping: invalid syntax\n");
        return -1;
    }

    long long sig_num = 0;
    if (!is_valid_signal_number(argv[2], &sig_num)) {
        printf("ping: invalid syntax\n");
        return -1;
    }

    job_sweep();

    int sig = (int)(sig_num % 64);

    if (argv[1][0] == '%') {
        if (argv[1][1] == '\0' || argv[1][1] == '-') {
            printf("ping: no such process found\n");
            return -1;
        }

        char *endptr = NULL;
        long job_num = strtol(argv[1] + 1, &endptr, 10);
        if (*endptr != '\0' || job_num <= 0) {
            printf("ping: no such process found\n");
            return -1;
        }

        Job *job = job_get_by_number((int)job_num);
        if (job == NULL || !job->active) {
            printf("ping: no such process found\n");
            return -1;
        }

        int killed = 0;
        if (job->pgid > 0) {
            if (kill(-job->pgid, sig) == 0) {
                killed = 1;
            }
        }
        if (!killed) {
            for (size_t p = 0; p < job->num_procs; p++) {
                if (!job->procs[p].exited) {
                    if (kill(job->procs[p].pid, sig) == 0) {
                        killed = 1;
                    }
                }
            }
        }

        if (sig == SIGCONT) {
            job->state = JOB_RUNNING;
        } else if (sig == SIGSTOP || sig == SIGTSTP) {
            job->state = JOB_STOPPED;
        }

        printf("Sent signal %s to %s\n", argv[2], argv[1]);
        fflush(stdout);
        return 0;
    } else {
        char *endptr = NULL;
        long pid_val = strtol(argv[1], &endptr, 10);
        if (*endptr != '\0' || pid_val <= 0) {
            printf("ping: no such process found\n");
            return -1;
        }

        size_t proc_idx = 0;
        Job *job = job_get_by_pid((pid_t)pid_val, &proc_idx);
        if (job == NULL || !job->active || job->procs[proc_idx].exited) {
            printf("ping: no such process found\n");
            return -1;
        }

        if (kill((pid_t)pid_val, sig) < 0 && errno == ESRCH) {
            printf("ping: no such process found\n");
            return -1;
        }

        if (sig == SIGCONT) {
            job->state = JOB_RUNNING;
        } else if (sig == SIGSTOP || sig == SIGTSTP) {
            job->state = JOB_STOPPED;
        }

        printf("Sent signal %s to %s\n", argv[2], argv[1]);
        fflush(stdout);
        return 0;
    }
}

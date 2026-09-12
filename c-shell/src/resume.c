#include "resume.h"
#include "term_control.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>

static volatile sig_atomic_t g_alarm_fired = 0;

static void alarm_handler(int sig) {
    (void)sig;
    g_alarm_fired = 1;
}

int resume_builtin(int argc, char *argv[]) {
    if (argc < 3) {
        printf("resume: invalid syntax\n");
        return -1;
    }

    if (argv[1][0] != '%') {
        printf("resume: invalid syntax\n");
        return -1;
    }

    char *endptr = NULL;
    long job_num = strtol(argv[1] + 1, &endptr, 10);
    if (*endptr != '\0' || job_num <= 0) {
        printf("resume: invalid syntax\n");
        return -1;
    }

    const char *mode = argv[2];
    int is_fg = (strcmp(mode, "fg") == 0);
    int is_bg = (strcmp(mode, "bg") == 0);
    if (!is_fg && !is_bg) {
        printf("resume: invalid syntax\n");
        return -1;
    }

    long timeout_sec = 0;
    if (is_bg) {
        if (argc != 3) {
            printf("resume: invalid syntax\n");
            return -1;
        }
    } else {
        if (argc == 3) {
            timeout_sec = 0;
        } else if (argc == 5 && strcmp(argv[3], "--timeout") == 0) {
            char *tend = NULL;
            timeout_sec = strtol(argv[4], &tend, 10);
            if (*tend != '\0' || timeout_sec <= 0) {
                printf("resume: invalid syntax\n");
                return -1;
            }
        } else {
            printf("resume: invalid syntax\n");
            return -1;
        }
    }

    job_sweep();
    Job *job = job_get_by_number((int)job_num);
    if (job == NULL || !job->active) {
        printf("resume: no such job\n");
        return -1;
    }

    if (is_bg) {
        kill(-job->pgid, SIGCONT);
        job->state = JOB_RUNNING;
        printf("[%d] + Running    %s\n", job->job_number, job->cmd_line);
        fflush(stdout);
        return 0;
    }

    printf("%s\n", job->cmd_line);
    fflush(stdout);

    term_give_to_pgid(job->pgid);
    job->state = JOB_RUNNING;
    kill(-job->pgid, SIGCONT);

    struct sigaction sa, old_sa;
    int timer_installed = 0;
    g_alarm_fired = 0;

    if (timeout_sec > 0) {
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = alarm_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(SIGALRM, &sa, &old_sa);
        timer_installed = 1;
        alarm((unsigned int)timeout_sec);
    }

    term_block_sigchld();

    int stopped = 0;
    while (1) {
        if (g_alarm_fired) {
            break;
        }

        int all_done = 1;
        for (size_t p = 0; p < job->num_procs; p++) {
            if (!job->procs[p].exited) {
                all_done = 0;
                break;
            }
        }
        if (all_done) {
            break;
        }

        int status = 0;
        pid_t pid = waitpid(-job->pgid, &status, WUNTRACED);
        if (pid < 0) {
            if (errno == EINTR) {
                if (g_alarm_fired) {
                    break;
                }
                continue;
            }
            break;
        }

        if (WIFSTOPPED(status)) {
            stopped = 1;
            break;
        }

        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            for (size_t p = 0; p < job->num_procs; p++) {
                if (job->procs[p].pid == pid) {
                    job->procs[p].exited = 1;
                    break;
                }
            }
        }
    }

    if (timer_installed) {
        alarm(0);
        sigaction(SIGALRM, &old_sa, NULL);
    }

    if (g_alarm_fired) {
        kill(-job->pgid, SIGTERM);
        printf("resume: job timed out\n");
        fflush(stdout);
        job->active = 0;
    } else if (stopped) {
        job->state = JOB_STOPPED;
        printf("[%d] + Stopped    %s\n", job->job_number, job->cmd_line);
        fflush(stdout);
    } else {
        int all_done = 1;
        for (size_t p = 0; p < job->num_procs; p++) {
            if (!job->procs[p].exited) {
                all_done = 0;
                break;
            }
        }
        if (all_done) {
            job->active = 0;
        }
    }

    term_reclaim();
    term_unblock_sigchld();

    return 0;
}

#include "term_control.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/types.h>

static Job g_jobs[MAX_JOBS];
static int g_next_job_num = 1;
static pid_t g_shell_pgid = 0;
static int g_ctrl_d_warned = 0;

static void sigchld_handler(int sig) {
    (void)sig;
    int saved_errno = errno;
    pid_t pid;
    int status;

    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        size_t proc_idx = 0;
        Job *job = job_get_by_pid(pid, &proc_idx);
        if (job != NULL) {
            if (WIFSTOPPED(status)) {
                job->state = JOB_STOPPED;
            } else if (WIFCONTINUED(status)) {
                job->state = JOB_RUNNING;
            } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
                job->procs[proc_idx].exited = 1;
                const char *name = job->procs[proc_idx].cmd_name;
                if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                    dprintf(STDOUT_FILENO, "%s with pid %d exited normally\n", name, (int)pid);
                } else {
                    dprintf(STDOUT_FILENO, "%s with pid %d exited abnormally\n", name, (int)pid);
                }

                int all_exited = 1;
                for (size_t p = 0; p < job->num_procs; p++) {
                    if (!job->procs[p].exited) {
                        all_exited = 0;
                        break;
                    }
                }
                if (all_exited) {
                    job->active = 0;
                }
            }
        }
    }

    errno = saved_errno;
}

void term_control_init(void) {
    memset(g_jobs, 0, sizeof(g_jobs));
    g_shell_pgid = getpgrp();

    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);
}

void term_give_to_pgid(pid_t pgid) {
    if (isatty(STDIN_FILENO) && pgid > 0) {
        tcsetpgrp(STDIN_FILENO, pgid);
    }
}

void term_reclaim(void) {
    if (isatty(STDIN_FILENO) && g_shell_pgid > 0) {
        tcsetpgrp(STDIN_FILENO, g_shell_pgid);
    }
}

pid_t term_get_shell_pgid(void) {
    return g_shell_pgid;
}

void term_block_sigchld(void) {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, NULL);
}

void term_unblock_sigchld(void) {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_UNBLOCK, &mask, NULL);
}

Job *job_add(pid_t pgid, const char *cmd_line, JobState state) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (!g_jobs[i].active) {
            g_jobs[i].job_number = g_next_job_num++;
            g_jobs[i].pgid = pgid;
            g_jobs[i].state = state;
            g_jobs[i].active = 1;
            g_jobs[i].num_procs = 0;
            if (cmd_line != NULL) {
                strncpy(g_jobs[i].cmd_line, cmd_line, sizeof(g_jobs[i].cmd_line) - 1);
                g_jobs[i].cmd_line[sizeof(g_jobs[i].cmd_line) - 1] = '\0';
            } else {
                g_jobs[i].cmd_line[0] = '\0';
            }
            return &g_jobs[i];
        }
    }
    return NULL;
}

void job_add_proc(Job *job, pid_t pid, const char *cmd_name) {
    if (job == NULL || job->num_procs >= MAX_PROCS_PER_JOB) {
        return;
    }
    if (cmd_name != NULL && cmd_name[0] == '%') {
        cmd_name++;
    }
    job->procs[job->num_procs].pid = pid;
    job->procs[job->num_procs].exited = 0;
    strncpy(job->procs[job->num_procs].cmd_name, cmd_name ? cmd_name : "?",
            sizeof(job->procs[job->num_procs].cmd_name) - 1);
    job->procs[job->num_procs].cmd_name[sizeof(job->procs[job->num_procs].cmd_name) - 1] = '\0';
    job->num_procs++;
}

Job *job_get_by_number(int job_number) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (g_jobs[i].active && g_jobs[i].job_number == job_number) {
            return &g_jobs[i];
        }
    }
    return NULL;
}

Job *job_get_by_pgid(pid_t pgid) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (g_jobs[i].active && g_jobs[i].pgid == pgid) {
            return &g_jobs[i];
        }
    }
    return NULL;
}

Job *job_get_by_pid(pid_t pid, size_t *proc_idx) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (g_jobs[i].active) {
            for (size_t p = 0; p < g_jobs[i].num_procs; p++) {
                if (g_jobs[i].procs[p].pid == pid) {
                    if (proc_idx != NULL) {
                        *proc_idx = p;
                    }
                    return &g_jobs[i];
                }
            }
        }
    }
    return NULL;
}

void job_remove(int job_number) {
    Job *job = job_get_by_number(job_number);
    if (job != NULL) {
        job->active = 0;
    }
}

void job_sweep(void) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (!g_jobs[i].active) {
            continue;
        }

        int all_exited = 1;
        for (size_t p = 0; p < g_jobs[i].num_procs; p++) {
            if (g_jobs[i].procs[p].exited) {
                continue;
            }

            int status = 0;
            pid_t res = waitpid(g_jobs[i].procs[p].pid, &status, WNOHANG | WUNTRACED | WCONTINUED);
            if (res > 0) {
                if (WIFSTOPPED(status)) {
                    g_jobs[i].state = JOB_STOPPED;
                    all_exited = 0;
                } else if (WIFCONTINUED(status)) {
                    g_jobs[i].state = JOB_RUNNING;
                    all_exited = 0;
                } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
                    g_jobs[i].procs[p].exited = 1;
                } else {
                    all_exited = 0;
                }
            } else if (res == 0) {
                all_exited = 0;
            } else {
                g_jobs[i].procs[p].exited = 1;
            }
        }

        if (all_exited) {
            g_jobs[i].active = 0;
        }
    }
}

int job_has_stopped(void) {
    job_sweep();
    for (int i = 0; i < MAX_JOBS; i++) {
        if (g_jobs[i].active && g_jobs[i].state == JOB_STOPPED) {
            return 1;
        }
    }
    return 0;
}

void job_kill_all_sighup(void) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (g_jobs[i].active && g_jobs[i].pgid > 0) {
            kill(-g_jobs[i].pgid, SIGHUP);
        }
    }
}

int job_get_next_number(void) {
    return g_next_job_num;
}

int term_handle_ctrl_d(void) {
    if (job_has_stopped()) {
        if (g_ctrl_d_warned) {
            return 1;
        } else {
            printf("cshell: there are stopped jobs\n");
            fflush(stdout);
            g_ctrl_d_warned = 1;
            return 0;
        }
    }
    return 1;
}

void term_reset_ctrl_d(void) {
    g_ctrl_d_warned = 0;
}

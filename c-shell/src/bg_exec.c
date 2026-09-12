#include "bg_exec.h"
#include "executor.h"
#include "lexer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/types.h>

#define MAX_BG_JOBS 256

typedef struct {
    pid_t pid;
    char name[256];
    int active;
} BgJob;

static BgJob g_jobs[MAX_BG_JOBS];
static int g_next_job_num = 1;

static int job_register(pid_t pid, const char *cmd_name) {
    for (int i = 0; i < MAX_BG_JOBS; i++) {
        if (!g_jobs[i].active) {
            g_jobs[i].pid = pid;
            g_jobs[i].active = 1;
            if (cmd_name && cmd_name[0] == '%') {
                cmd_name++;
            }
            strncpy(g_jobs[i].name, cmd_name ? cmd_name : "?", sizeof(g_jobs[i].name) - 1);
            g_jobs[i].name[sizeof(g_jobs[i].name) - 1] = '\0';
            return i;
        }
    }
    return -1;
}

static int job_find(pid_t pid) {
    for (int i = 0; i < MAX_BG_JOBS; i++) {
        if (g_jobs[i].active && g_jobs[i].pid == pid) {
            return i;
        }
    }
    return -1;
}

static void sigchld_handler(int sig) {
    (void)sig;
    int saved_errno = errno;
    pid_t pid;
    int status;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        int slot = job_find(pid);
        if (slot >= 0) {
            const char *name = g_jobs[slot].name;
            if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                dprintf(STDOUT_FILENO, "%s with pid %d exited normally\n", name, (int)pid);
            } else {
                dprintf(STDOUT_FILENO, "%s with pid %d exited abnormally\n", name, (int)pid);
            }
            g_jobs[slot].active = 0;
        }
    }

    errno = saved_errno;
}

void bg_init(void) {
    memset(g_jobs, 0, sizeof(g_jobs));

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);
}

void bg_block_sigchld(void) {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, NULL);
}

void bg_unblock_sigchld(void) {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_UNBLOCK, &mask, NULL);
}

static pid_t launch_bg_group(const Token *tokens, size_t len, const char *cmd_name) {
    bg_block_sigchld();

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        bg_unblock_sigchld();
        return -1;
    }

    if (pid == 0) {
        int has_in_redir = 0;
        for (size_t i = 0; i < len; i++) {
            if (tokens[i].type == TOK_LT) {
                has_in_redir = 1;
                break;
            }
        }
        if (!has_in_redir) {
            int devnull = open("/dev/null", O_RDONLY);
            if (devnull >= 0) {
                dup2(devnull, STDIN_FILENO);
                close(devnull);
            }
        }

        signal(SIGCHLD, SIG_DFL);
        bg_unblock_sigchld();

        int ret = executor_run(tokens, len);
        _exit(ret == 0 ? 0 : 1);
    }

    job_register(pid, cmd_name);
    bg_unblock_sigchld();

    return pid;
}

int bg_run(const Token *tokens, size_t count) {
    if (tokens == NULL || count == 0) {
        return 0;
    }

    size_t start = 0;

    while (start < count) {
        size_t end = start;
        while (end < count && tokens[end].type != TOK_AMP) {
            end++;
        }

        size_t group_len = end - start;

        if (group_len > 0) {
            const char *cmd_name = NULL;
            for (size_t i = start; i < end; i++) {
                if (tokens[i].type == TOK_WORD) {
                    cmd_name = tokens[i].value;
                    break;
                }
            }

            if (end < count && tokens[end].type == TOK_AMP) {
                pid_t pid = launch_bg_group(tokens + start, group_len, cmd_name);
                if (pid > 0) {
                    printf("[%d] %d\n", g_next_job_num++, (int)pid);
                    fflush(stdout);
                }
                end++;
            } else {
                bg_block_sigchld();
                executor_run(tokens + start, group_len);
                bg_unblock_sigchld();
            }
        } else if (end < count && tokens[end].type == TOK_AMP) {
            end++;
        }

        start = end;
    }

    return 0;
}

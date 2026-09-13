#include "snoop.h"
#include "executor.h"
#include "term_control.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>

typedef struct {
    long long nr;
    const char *name;
} SyscallEntry;

static const SyscallEntry g_syscall_table[] = {
    {0, "read"},
    {1, "write"},
    {35, "nanosleep"},
    {230, "clock_nanosleep"},
    {231, "exit_group"}
};
#define SYSCALL_TABLE_SIZE (sizeof(g_syscall_table) / sizeof(g_syscall_table[0]))

typedef struct {
    long long nr;
    char name[64];
    unsigned long long calls;
    double total_time;
    size_t first_seen_order;
} SyscallStat;

static void get_syscall_name(long long nr, char *buf, size_t buf_size) {
    for (size_t i = 0; i < SYSCALL_TABLE_SIZE; i++) {
        if (g_syscall_table[i].nr == nr) {
            snprintf(buf, buf_size, "%s", g_syscall_table[i].name);
            return;
        }
    }
    snprintf(buf, buf_size, "syscall_%lld", nr);
}

static int compare_stats(const void *a, const void *b) {
    const SyscallStat *sa = (const SyscallStat *)a;
    const SyscallStat *sb = (const SyscallStat *)b;
    if (sa->calls != sb->calls) {
        return (sa->calls < sb->calls) ? 1 : -1;
    }
    return (sa->first_seen_order > sb->first_seen_order) ? 1 : -1;
}

static void record_syscall(SyscallStat *stats, size_t *num_stats, long long nr, double elapsed) {
    for (size_t i = 0; i < *num_stats; i++) {
        if (stats[i].nr == nr) {
            stats[i].calls++;
            stats[i].total_time += elapsed;
            return;
        }
    }
    if (*num_stats < 1024) {
        size_t idx = (*num_stats)++;
        stats[idx].nr = nr;
        get_syscall_name(nr, stats[idx].name, sizeof(stats[idx].name));
        stats[idx].calls = 1;
        stats[idx].total_time = elapsed;
        stats[idx].first_seen_order = idx;
    }
}

int snoop_builtin(int argc, char *argv[]) {
    if (argc < 2) {
        printf("snoop: invalid syntax\n");
        return -1;
    }

    int attach_mode = 0;
    pid_t target_pid = -1;
    char *resolved_path = NULL;

    if (strcmp(argv[1], "-p") == 0) {
        if (argc != 3) {
            printf("snoop: invalid syntax\n");
            return -1;
        }
        char *endptr = NULL;
        long val = strtol(argv[2], &endptr, 10);
        if (*endptr != '\0' || val <= 0 || argv[2][0] == '-') {
            printf("snoop: no such process\n");
            return -1;
        }
        target_pid = (pid_t)val;

        char proc_dir[128];
        snprintf(proc_dir, sizeof(proc_dir), "/proc/%ld", (long)target_pid);
        if (access(proc_dir, F_OK) != 0) {
            printf("snoop: no such process\n");
            return -1;
        }

        attach_mode = 1;
    } else {
        resolved_path = executor_resolve_path(argv[1]);
        if (resolved_path == NULL) {
            printf("snoop: command not found\n");
            return -1;
        }
    }

    term_block_sigchld();

    if (attach_mode) {
        if (ptrace(PTRACE_ATTACH, target_pid, NULL, NULL) < 0) {
            printf("snoop: no such process\n");
            term_unblock_sigchld();
            return -1;
        }
        int status = 0;
        if (waitpid(target_pid, &status, 0) < 0) {
            ptrace(PTRACE_DETACH, target_pid, NULL, NULL);
            term_unblock_sigchld();
            return -1;
        }
        ptrace(PTRACE_SETOPTIONS, target_pid, 0, PTRACE_O_TRACESYSGOOD);
        ptrace(PTRACE_SYSCALL, target_pid, 0, 0);
    } else {
        pid_t child = fork();
        if (child < 0) {
            perror("fork");
            free(resolved_path);
            term_unblock_sigchld();
            return -1;
        } else if (child == 0) {
            term_unblock_sigchld();
            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            signal(SIGTTOU, SIG_DFL);
            signal(SIGTTIN, SIG_DFL);
            signal(SIGCHLD, SIG_DFL);

            if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) < 0) {
                perror("ptrace");
                _exit(1);
            }

            raise(SIGSTOP);

            execv(resolved_path, argv + 1);
            perror("execv");
            _exit(1);
        }

        target_pid = child;
        free(resolved_path);

        int status = 0;
        waitpid(target_pid, &status, 0);
        ptrace(PTRACE_SETOPTIONS, target_pid, 0, PTRACE_O_TRACESYSGOOD);
        ptrace(PTRACE_SYSCALL, target_pid, 0, 0);
    }

    SyscallStat stats[1024];
    size_t num_stats = 0;
    int in_syscall = 0;
    long long cur_syscall_nr = -1;
    struct timespec entry_time;

    while (1) {
        int status = 0;
        pid_t w = waitpid(target_pid, &status, 0);
        if (w < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }

        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            if (in_syscall) {
                struct timespec exit_time;
                clock_gettime(CLOCK_MONOTONIC, &exit_time);
                double elapsed = (exit_time.tv_sec - entry_time.tv_sec) +
                                 (exit_time.tv_nsec - entry_time.tv_nsec) / 1e9;
                if (elapsed < 0.0) elapsed = 0.0;
                record_syscall(stats, &num_stats, cur_syscall_nr, elapsed);
                in_syscall = 0;
            }
            break;
        }

        if (WIFSTOPPED(status)) {
            int sig = WSTOPSIG(status);
            if (sig == (SIGTRAP | 0x80)) {
                struct user_regs_struct regs;
                if (ptrace(PTRACE_GETREGS, target_pid, NULL, &regs) == 0) {
                    if (!in_syscall) {
                        in_syscall = 1;
                        cur_syscall_nr = (long long)regs.orig_rax;
                        clock_gettime(CLOCK_MONOTONIC, &entry_time);
                    } else {
                        struct timespec exit_time;
                        clock_gettime(CLOCK_MONOTONIC, &exit_time);
                        double elapsed = (exit_time.tv_sec - entry_time.tv_sec) +
                                         (exit_time.tv_nsec - entry_time.tv_nsec) / 1e9;
                        if (elapsed < 0.0) elapsed = 0.0;
                        record_syscall(stats, &num_stats, cur_syscall_nr, elapsed);
                        in_syscall = 0;
                    }
                }
                ptrace(PTRACE_SYSCALL, target_pid, 0, 0);
            } else if (sig == SIGTRAP) {
                ptrace(PTRACE_SYSCALL, target_pid, 0, 0);
            } else {
                ptrace(PTRACE_SYSCALL, target_pid, 0, sig);
            }
        }
    }

    term_unblock_sigchld();

    qsort(stats, num_stats, sizeof(SyscallStat), compare_stats);

    printf("syscall       calls   time\n");
    for (size_t i = 0; i < num_stats; i++) {
        if (strlen(stats[i].name) >= 14) {
            printf("%s %-7llu%.3fs\n", stats[i].name, stats[i].calls, stats[i].total_time);
        } else {
            printf("%-14s%-8llu%.3fs\n", stats[i].name, stats[i].calls, stats[i].total_time);
        }
    }
    fflush(stdout);

    return 0;
}

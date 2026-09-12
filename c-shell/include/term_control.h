#ifndef TERM_CONTROL_H
#define TERM_CONTROL_H

#include <sys/types.h>
#include <stddef.h>

#define MAX_JOBS 256
#define MAX_PROCS_PER_JOB 32

typedef enum {
    JOB_RUNNING,
    JOB_STOPPED
} JobState;

typedef struct {
    pid_t pid;
    char cmd_name[128];
    int exited;
} JobProc;

typedef struct {
    int job_number;
    pid_t pgid;
    char cmd_line[512];
    JobState state;
    int active;
    size_t num_procs;
    JobProc procs[MAX_PROCS_PER_JOB];
} Job;

void term_control_init(void);
void term_give_to_pgid(pid_t pgid);
void term_reclaim(void);
pid_t term_get_shell_pgid(void);

Job *job_add(pid_t pgid, const char *cmd_line, JobState state);
void job_add_proc(Job *job, pid_t pid, const char *cmd_name);
Job *job_get_by_number(int job_number);
Job *job_get_by_pgid(pid_t pgid);
Job *job_get_by_pid(pid_t pid, size_t *proc_idx);
void job_remove(int job_number);
int job_has_stopped(void);
void job_kill_all_sighup(void);
void job_sweep(void);
int job_get_next_number(void);

int term_handle_ctrl_d(void);
void term_reset_ctrl_d(void);

void term_block_sigchld(void);
void term_unblock_sigchld(void);

#endif /* TERM_CONTROL_H */

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define sleep(n) pause(n)

// Number of test processes to launch
#define NPROCS 5

// Busy-spin using uptime to consume a precise number of CPU ticks
void
spin_ticks(int target_ticks)
{
  int start = uptime();
  volatile int x = 0;
  while (uptime() - start < target_ticks) {
    x++;
  }
}

int
main(void)
{
  int pids[NPROCS];
  char *types[NPROCS] = {
    "CPU-bound 1",
    "CPU-bound 2",
    "I/O-bound 1",
    "I/O-bound 2",
    "Mixed (I/O+CPU)"
  };

  printf("=== Starting schedulertest (NPROCS=%d) ===\n", NPROCS);

  for (int i = 0; i < NPROCS; i++) {
    int pid = fork();
    if (pid < 0) {
      printf("fork failed\n");
      exit(1);
    }
    if (pid == 0) {
      // Child workload
      if (i == 0 || i == 1) {
        // CPU-bound: runs heavy computation for 45 continuous ticks
        // Exercises Q0 (1), Q1 (4), Q2 (8), Q3 (16), and spans across 48-tick priority boost
        spin_ticks(45);
      } else if (i == 2 || i == 3) {
        // I/O-bound: short burst (<1 tick) then voluntary sleep
        // Stays in Queue 0
        for (int b = 0; b < 20; b++) {
          for (volatile int k = 0; k < 50000; k++);
          sleep(1);
        }
      } else {
        // Mixed: runs 2 ticks (demotes to Q1) then sleeps before exhausting Q1 (slice=4)
        for (int b = 0; b < 8; b++) {
          spin_ticks(2);
          sleep(2);
        }
      }
      exit(0);
    }
    pids[i] = pid;
  }

  int wtime, rtime, stime, status;
  int total_turnaround = 0;
  int total_waiting = 0;
  int total_response = 0;

  printf("\nPID\tType\t\tTurnaround\tWaiting\tResponse\n");
  printf("----------------------------------------------------------------\n");

  for (int i = 0; i < NPROCS; i++) {
    int pid = waitx(&status, &wtime, &rtime, &stime);
    if (pid < 0) {
      printf("waitx error\n");
      exit(1);
    }
    int turnaround = rtime + wtime;
    total_turnaround += turnaround;
    total_waiting += wtime;
    total_response += stime;

    // Match PID to type
    char *type = "Unknown";
    for (int j = 0; j < NPROCS; j++) {
      if (pids[j] == pid) {
        type = types[j];
        break;
      }
    }

    printf("%d\t%s\t%d\t\t%d\t%d\n", pid, type, turnaround, wtime, stime);
  }

  printf("----------------------------------------------------------------\n");
  printf("Average Turnaround Time: %d.%d ticks\n",
         total_turnaround / NPROCS, ((total_turnaround % NPROCS) * 10) / NPROCS);
  printf("Average Waiting Time:    %d.%d ticks\n",
         total_waiting / NPROCS, ((total_waiting % NPROCS) * 10) / NPROCS);
  printf("Average Response Time:   %d.%d ticks\n\n",
         total_response / NPROCS, ((total_response % NPROCS) * 10) / NPROCS);

  exit(0);
}

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define sleep(n) pause(n)

// Number of test processes to launch
#define NPROCS 5

// Volatile spin loop to consume CPU cycles
void
spin(int n)
{
  volatile int x = 0;
  for (int i = 0; i < n; i++) {
    for (int j = 0; j < 100000; j++) {
      x += (i * j + 1);
    }
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
        // CPU-bound: sustained computation without yielding
        for (int b = 0; b < 6; b++) {
          spin(25);
        }
      } else if (i == 2 || i == 3) {
        // I/O-bound: short CPU burst then voluntary sleep
        for (int b = 0; b < 10; b++) {
          spin(2);
          sleep(1);
        }
      } else {
        // Mixed: medium CPU burst then voluntary sleep
        for (int b = 0; b < 5; b++) {
          spin(10);
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

  printf("\nPID\tType\t\t\tTurnaround\tWaiting\tResponse\n");
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

    printf("PID %d (%s): turnaround=%d waiting=%d response=%d\n",
           pid, type, turnaround, wtime, stime);
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

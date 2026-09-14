# Mini Project 1: C-Shell & xv6 MLFQ Scheduler

Operating Systems Mini Project 1 (COMPLETED🥳🥳🥳).  
This project comprises two major components:
1. **C-Shell**: A fully POSIX-compliant Unix shell supporting lexical analysis, command pipelines, I/O redirection, background job management, job control, signals, and advanced system diagnostics (`spy`, `snoop`).
2. **MLFQ Scheduler**: An implementation of a n-level (n = 4) Multi-Level Feedback Queue scheduling policy inside the xv6-riscv kernel, including strict priority preemption, anti-starvation boosting, cross-scheduler benchmarking, and empirical analysis against Round-Robin and FIFO.

---

## 1. Folder Structure

```
mini-project1/
├── c-shell/
│   ├── include/              # Header files (.h)
│   │   ├── activities.h
│   │   ├── bg_exec.h
│   │   ├── common.h
│   │   :
│   ├── src/                  # Implementation source files (.c)
│   │   ├── activities.c
│   │   ├── bg_exec.c
│   │   ├── dynstring.c
│   │   :
│   └── Makefile              # C-Shell build configuration
│
├── xv6/
│   ├── kernel/               # Operating system kernel source files
│   │   ├── proc.c
│   │   ├── proc.h
│   │   ├── trap.c
│   │   ├── defs.h
│   │   :
│   ├── user/                 # User-space utilities
│   │   ├── schedulertest.c
│   │   ├── usys.pl
│   │   ├── user.h
│   │   :
│   ├── mkfs/                 # File system disk image builder
│   ├── Makefile              # Build configuration with SCHEDULER flag
│   ├── benchmark.py          
│   ├── mlfq_timeline.png     
│   └── report.md             # Detailed scheduling analysis report
│
├── AI-usage.pdf              # AI Usage declaration & documentation
└── README.md                 # Project documentation
```

---

## 2. Run instructions

### A. C-Shell

1. **Navigate to the directory:**
   ```bash
   cd c-shell
   ```
2. **Build the shell:**
   ```bash
   make clean && make all
   ```
3. **Interractive shell:**
   ```bash
   ./shell.out
   ```
   
---

### B. xv6 Schedulers

Navigate into `xv6/`:
```bash
cd xv6
```

1. **Run with MLFQ:**
   ```bash
   make clean && make qemu SCHEDULER=MLFQ
   ```
2. **Run with default Round-Robin:**
   ```bash
   make clean && make qemu
   ```
3. **Run with FCFS/FIFO :**
   ```bash
   make clean && make qemu SCHEDULER=FIFO
   ```
4. **Inside xv6:**
   - **Run the scheduler benchmark:**
     ```bash
     $ schedulertest
     ```
     Prints Turnaround, Waiting, and Response Times for CPU-bound, I/O-bound, and mixed workloads.
   - **Trigger `procdump`:** Press **`Ctrl + P`** on your keyboard to dump process states, queues, consumed ticks, and time until the next boost.
   - **Exit QEMU:** Press **`Ctrl + A`**, release, then press **`x`**.

5. **Benchmark against different policies and Plot Generation:**
   From the host terminal inside `xv6/`:
   ```bash
   python3 benchmark.py
   ```
   Runs RR, FIFO, and MLFQ back-to-back, computes average metrics, and outputs `mlfq_timeline.png`.

---

## 3. C-Shell: Architecture & Design Choices

### Modular Design
To prevent monolithic code, the shell is separated into multiple modules:
- **`lexer` & `parser`**: Robust state-machine tokenizer handling single/double quotes, escape sequences, concatenation of adjacent tokens, and operator separation (`;`, `&`, `|`, `<`, `>`, `>>`).
- **`executor`**: Dispatches built-in commands or delegates external programs via `fork()` and `execvp()`.
- **`pipes` & `redirect`**: Implements arbitrary-depth pipelines (`cmd1 | cmd2 | cmd3`) with input/output/append redirection (`<`, `>`, `>>`). Multiple input redirects concatenate sources sequentially; output redirects write to all specified sinks.
- **`term_control` & `bg_exec`**: Enforces strict job control:
  - Each job runs in its own process group (`setpgid`).
  - Foreground jobs claim terminal access via `tcsetpgrp`.
  - The shell catches and manages signals (`SIGINT`, `SIGTSTP`, `SIGCHLD`), ensuring `Ctrl+C` and `Ctrl+Z` affect only foreground tasks and not exit ./shell.out .
  - Stopped processes transition cleanly to the background jobs table.

### Built-In Commands Implemented
- **Part A**: Dynamic prompt (`<username@hostname:cwd>`), `exit`.
- **Part B**:
  - `hop`: Directory navigation supporting `~`, `.`, `..`, `-`, and relative/absolute paths.
  - `reveal -ta`: File listing, sorted lexicographically.
  - `peek -nr`: Line-numbered and reversed file display.
  - `locate <file>`: Recursive filename lookup within the directory tree.
- **Part D**:
  - `sequence`: Sequential command execution that halts immediately on error.
  - Background process spawning with `&` logging job number and PID.
- **Part E**:
  - `activities`: Enumerates all active/stopped process groups with child PIDs and states.
  - `ping <job_num> <sig>`: Transmits signals to specific background jobs.
  - `resume %<job> [fg|bg]`: Resumes stopped processes in foreground or background.
- **Part F**:
  - `spy <pid>`: Inspects process information and memory maps via `/proc`.
  - `snoop <pid>`: Traces system calls in real time using `ptrace` (monitoring `read`, `write`, `nanosleep`, `clock_nanosleep`, `exit_group`).

---

## 4. xv6 MLFQ Scheduler: Key Changes

### 1. Build System (`Makefile`)
- Injected compile-time macro `-DSCHEDULER_$(SCHEDULER)` via `ifdef SCHEDULER`.
- Set `CPUS := 1` as default when `SCHEDULER=MLFQ` or `SCHEDULER=FIFO` to guarantee single-core queue determinism while keeping `CPUS := 3` for default Round-Robin.
- Added `schedulertest` to `UPROGS`.
- Added a user-friendly `make help` target.

### 2. Process Structure (`kernel/proc.h`)
Extended `struct proc` with:
- `int mlfq_queue`: Current priority level (0 = highest, 3 = lowest).
- `int mlfq_ticks_used`: Timer ticks consumed in the current time quantum.
- `int mlfq_total_ticks`: Total lifetime CPU ticks.
- `uint64 mlfq_enter_seq`: Monotonically increasing queue-entry sequence number to ensure strict FIFO order within queues 0–2 and true Round-Robin in queue 3.
- `uint ctime`, `int stime`, `uint etime`, `uint rtime`: High-resolution timing metrics for benchmarking.

### 3. Queue Management & Scheduling (`kernel/proc.c`)
- **Queue Insertion**: New processes enter Queue 0 on creation (`allocproc`).
- **Strict Priority Selection**: The scheduler scans queues 0 through 3 in order. In the highest non-empty queue, the process with the smallest `mlfq_enter_seq` (the head of that queue) is chosen to run.
- **Preemption**: At every timer interrupt, `mlfq_has_higher_priority()` checks if a process in a higher queue is ready. If so, the running process yields without demotion.
- **Time Slices & Demotion**:
  - Queue 0: 1 tick
  - Queue 1: 4 ticks
  - Queue 2: 8 ticks
  - Queue 3: 16 ticks
  When `mlfq_ticks_used` reaches the queue slice, `mlfq_ticks_used` is reset, the queue level increments (capped at 3), a new `mlfq_enter_seq` is assigned (pushing it to the tail), and the CPU yields.
- **Voluntary Yielding (I/O)**: In `sleep()`, `mlfq_ticks_used` is reset to 0 while maintaining the queue ID. In `wakeup()`, a new `mlfq_enter_seq` inserts the process at the tail of the same queue.
- **Starvation Prevention (Priority Boost)**: In `clockintr()` (`kernel/trap.c`), every 48 global ticks, `mlfq_boost()` resets all active processes to Queue 0, resets their slice ticks, and assigns fresh sequence numbers.

### 4. Process Accounting & Syscalls (`kernel/sysproc.c`, `user/usys.pl`)
- Implemented `waitx(int *status, int *wtime, int *rtime, int *stime)` syscall to allow `schedulertest` to extract Turnaround, Waiting, and Response times.
- Extended `procdump()` (`Ctrl+P`) to display `q=N ticks_used=N total=N ticks_since_boost=N`.

---

## 5. Assumptions

1. **Operating Environment**: Designed and tested on Linux (x86_64 host compiling for RISC-V 64-bit target via `riscv64-linux-gnu-gcc` and `qemu-system-riscv64`).
2. **C-Shell Assumptions**:
   - Maximum command length conforms to standard POSIX line input (`getline`).
   - Process states for `activities`, `spy`, and `snoop` read directly from the host `/proc` virtual file system.
   - For `snoop`, system call tracing is restricted to the top essential calls (`read`, `write`, `nanosleep`, `clock_nanosleep`, `exit_group`) as per project instructions.
3. **xv6 MLFQ Assumptions**:
   - Simulated under `CPUS=1` to enforce strict single-CPU timeline semantics and avoid multi-core interleaving across queues.
   - Timer interrupts occur approximately every 1,000,000 CPU cycles (1 tick ~ 100 ms).
   - Priority boost occurs strictly every 48 timer ticks.

---

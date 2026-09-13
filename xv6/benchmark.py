#!/usr/bin/env python3
import subprocess
import time
import os
import re
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

def run_test(sched_name, make_arg=None):
    print(f"\n==========================================")
    print(f"  BUILDING & RUNNING: {sched_name}")
    print(f"==========================================")
    
    subprocess.run(["make", "clean"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    
    build_cmd = ["make"]
    if make_arg:
        build_cmd.append(make_arg)
    res = subprocess.run(build_cmd, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    if res.returncode != 0:
        print("Build error:", res.stderr.decode())
        return None, ""
        
    qemu_cmd = ["make", "qemu"]
    if make_arg:
        qemu_cmd.append(make_arg)
        
    p = subprocess.Popen(qemu_cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    os.set_blocking(p.stdout.fileno(), False)
    
    time.sleep(3)
    p.stdin.write(b"schedulertest\n")
    p.stdin.flush()
    
    full_output = b""
    start_time = time.time()
    while time.time() - start_time < 90:
        try:
            chunk = os.read(p.stdout.fileno(), 4096)
            if chunk:
                full_output += chunk
                if b"Average Response Time:" in chunk:
                    break
        except BlockingIOError:
            pass
        time.sleep(0.4)
        
    p.stdin.write(b"\x01x")
    p.stdin.flush()
    time.sleep(0.5)
    p.kill()
    
    out_text = full_output.decode("utf-8", "replace")
    
    # Parse metrics
    avg_turnaround = None
    avg_waiting = None
    avg_response = None
    
    for line in out_text.splitlines():
        if "Average Turnaround Time:" in line:
            avg_turnaround = float(line.split(":")[1].replace("ticks", "").strip())
        elif "Average Waiting Time:" in line:
            avg_waiting = float(line.split(":")[1].replace("ticks", "").strip())
        elif "Average Response Time:" in line:
            avg_response = float(line.split(":")[1].replace("ticks", "").strip())
        elif "PID " in line:
            print("  " + line.strip())
            
    print(f"  Summary -> Turnaround: {avg_turnaround} | Waiting: {avg_waiting} | Response: {avg_response}")
    return {
        "turnaround": avg_turnaround,
        "waiting": avg_waiting,
        "response": avg_response
    }, out_text

def plot_mlfq_timeline(mlfq_output):
    traces = []
    # Pattern: [MLFQ_TRACE] <tick> <pid> <queue>
    for line in mlfq_output.splitlines():
        if "[MLFQ_TRACE]" in line:
            parts = line.replace("[MLFQ_TRACE]", "").strip().split()
            if len(parts) == 3:
                try:
                    t = int(parts[0])
                    pid = int(parts[1])
                    q = int(parts[2])
                    traces.append((t, pid, q))
                except ValueError:
                    pass
                    
    if not traces:
        print("No MLFQ_TRACE lines found in output!")
        return
        
    min_t = min(t for t, p, q in traces)
    norm_traces = [(t - min_t, pid, q) for t, pid, q in traces]
    
    pids = sorted(list(set(p for t, p, q in norm_traces)))
    colors = ['#e41a1c', '#377eb8', '#4daf4a', '#984ea3', '#ff7f00', '#ffff33']
    pid_colors = {pid: colors[i % len(colors)] for i, pid in enumerate(pids)}
    pid_labels = {
        pids[0]: f"PID {pids[0]} (CPU-bound 1)",
        pids[1]: f"PID {pids[1]} (CPU-bound 2)",
        pids[2]: f"PID {pids[2]} (I/O-bound 1)",
        pids[3]: f"PID {pids[3]} (I/O-bound 2)",
        pids[4]: f"PID {pids[4]} (Mixed I/O+CPU)"
    } if len(pids) >= 5 else {pid: f"PID {pid}" for pid in pids}
    
    plt.figure(figsize=(12, 6))
    
    for pid in pids:
        xs = [t for t, p, q in norm_traces if p == pid]
        ys = [q for t, p, q in norm_traces if p == pid]
        plt.scatter(xs, ys, label=pid_labels.get(pid, f"PID {pid}"), color=pid_colors[pid], s=35, alpha=0.85)
        # connect consecutive points
        plt.plot(xs, ys, color=pid_colors[pid], alpha=0.3, linewidth=1)
        
    # Mark the 48-tick priority boost boundaries
    max_t = max(t for t, p, q in norm_traces)
    for b in range(48, max_t + 1, 48):
        plt.axvline(x=b, color='gray', linestyle='--', alpha=0.7, label='Priority Boost (48 ticks)' if b == 48 else None)
        
    plt.yticks([0, 1, 2, 3], ["Queue 0 (slice=1)", "Queue 1 (slice=4)", "Queue 2 (slice=8)", "Queue 3 (slice=16)"])
    plt.gca().invert_yaxis()  # Highest priority (Queue 0) at the top
    plt.xlabel("Time elapsed (ticks since start)", fontsize=12)
    plt.ylabel("MLFQ Priority Queue", fontsize=12)
    plt.title("MLFQ Scheduling Timeline: Queue Progression & Periodic Priority Boost", fontsize=14, fontweight='bold')
    plt.grid(True, linestyle=':', alpha=0.6)
    plt.legend(loc='upper right', framealpha=0.9)
    plt.tight_layout()
    
    plot_file = "mlfq_timeline.png"
    plt.savefig(plot_file, dpi=200)
    print(f"\n[OK] Saved timeline plot to {plot_file}")

def main():
    results = {}
    
    # 1. Round Robin
    res_rr, _ = run_test("Round Robin (Default)", None)
    results["Round Robin (RR)"] = res_rr
    
    # 2. FIFO
    res_fifo, _ = run_test("FIFO / FCFS", "SCHEDULER=FIFO")
    results["FIFO (FCFS)"] = res_fifo
    
    # 3. MLFQ
    res_mlfq, mlfq_output = run_test("Multi-Level Feedback Queue (MLFQ)", "SCHEDULER=MLFQ")
    results["MLFQ"] = res_mlfq
    
    # Plot MLFQ Timeline
    plot_mlfq_timeline(mlfq_output)
    
    # Print comparison table
    print("\n" + "="*70)
    print("        CROSS-SCHEDULER COMPARISON RESULTS (Section 2.2)")
    print("="*70)
    print(f"{'Scheduler':<20} | {'Avg Turnaround (ticks)':<22} | {'Avg Waiting (ticks)':<20} | {'Avg Response (ticks)':<20}")
    print("-" * 70)
    for sched, metrics in results.items():
        if metrics:
            print(f"{sched:<20} | {metrics['turnaround']:<22.2f} | {metrics['waiting']:<20.2f} | {metrics['response']:<20.2f}")
        else:
            print(f"{sched:<20} | {'N/A':<22} | {'N/A':<20} | {'N/A':<20}")
    print("="*70)

if __name__ == "__main__":
    main()

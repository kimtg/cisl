import os
import sys
import time
import subprocess
import statistics
import json

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BENCH_DIR = "benchmarks"

def run_command(cmd, cwd=ROOT_DIR):
    res = subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if res.returncode != 0:
        raise RuntimeError(f"Command failed ({res.returncode}): {' '.join(cmd)}\nStderr: {res.stderr}\nStdout: {res.stdout}")
    return res.stdout.strip()

def compile_targets():
    print("=== Compiling benchmark targets ===")
    
    # 1. CISL defun
    cisl_lsp = os.path.join("benchmarks", "fib_cisl.lsp")
    cisl_exe = os.path.join("benchmarks", "fib_cisl.exe")
    print(f"Compiling CISL defun -> {cisl_exe} ...")
    run_command([".\\cisl.exe", "-o", cisl_exe, cisl_lsp])
    
    # 2. CISL labels
    cisl_labels_lsp = os.path.join("benchmarks", "fib_cisl_labels.lsp")
    cisl_labels_exe = os.path.join("benchmarks", "fib_cisl_labels.exe")
    print(f"Compiling CISL labels -> {cisl_labels_exe} ...")
    run_command([".\\cisl.exe", "-o", cisl_labels_exe, cisl_labels_lsp])
    
    # 3. Native C Reference
    c_src = os.path.join("benchmarks", "fib_ref_c.c")
    c_exe = os.path.join("benchmarks", "fib_ref_c.exe")
    print(f"Compiling Reference C -> {c_exe} ...")
    run_command(["gcc", "-O3", c_src, "-o", c_exe])
    
    print("Compilation complete.\n")

def parse_output(stdout):
    result = None
    exec_time = None
    for line in stdout.splitlines():
        line = line.strip()
        if line.startswith("RESULT="):
            result = int(line.split("=")[1])
        elif line.startswith("TIME="):
            val = float(line.split("=")[1])
            exec_time = val
    return result, exec_time

def main():
    compile_targets()
    
    targets = [
        {
            "id": "c_gcc",
            "name": "C (GCC 14.2 -O2 Reference)",
            "cmd": [os.path.join(BENCH_DIR, "fib_ref_c.exe")],
            "time_scale": 1.0,
        },
        {
            "id": "sbcl_opt",
            "name": "Common Lisp: SBCL (speed 3, fixnum)",
            "cmd": ["sbcl", "--noinform", "--load", os.path.join(BENCH_DIR, "fib_sbcl_opt.lisp"), "--quit"],
            "time_scale": 1.0,
        },
        {
            "id": "sbcl_std",
            "name": "Common Lisp: SBCL (Standard / untyped)",
            "cmd": ["sbcl", "--noinform", "--load", os.path.join(BENCH_DIR, "fib_sbcl.lisp"), "--quit"],
            "time_scale": 1.0,
        },
        {
            "id": "cisl_labels",
            "name": "CISL 1.0 (labels / direct call)",
            "cmd": [os.path.join(BENCH_DIR, "fib_cisl_labels.exe")],
            "time_scale": 0.001,
        },
        {
            "id": "cisl_defun",
            "name": "CISL 1.0 (Standard defun)",
            "cmd": [os.path.join(BENCH_DIR, "fib_cisl.exe")],
            "time_scale": 0.001,
        },
        {
            "id": "python",
            "name": "Python 3.14.4 (CPython)",
            "cmd": ["python", os.path.join(BENCH_DIR, "fib.py"), "40"],
            "time_scale": 1.0,
        },
    ]

    NUM_RUNS = 3
    EXPECTED_RES = 102334155
    all_results = {}

    print(f"=== Running fib(40) Benchmarks ({NUM_RUNS} runs per target) ===")
    for target in targets:
        t_id = target["id"]
        t_name = target["name"]
        print(f"\nTarget: {t_name}")
        internal_times = []
        process_times = []

        for r in range(1, NUM_RUNS + 1):
            t_start = time.perf_counter()
            p = subprocess.run(target["cmd"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            t_end = time.perf_counter()
            proc_time = t_end - t_start

            if p.returncode != 0:
                print(f"  Run {r}: FAILED with code {p.returncode}")
                print(p.stderr)
                continue

            res_val, int_time = parse_output(p.stdout)
            if res_val != EXPECTED_RES:
                print(f"  Run {r}: ERROR - expected {EXPECTED_RES}, got {res_val}")
                continue

            scaled_int_time = int_time * target["time_scale"]
            internal_times.append(scaled_int_time)
            process_times.append(proc_time)
            print(f"  Run {r}: internal = {scaled_int_time:.4f} s, process = {proc_time:.4f} s (res = {res_val})")

        all_results[t_id] = {
            "name": t_name,
            "internal_mean": statistics.mean(internal_times),
            "internal_min": min(internal_times),
            "internal_std": statistics.stdev(internal_times) if len(internal_times) > 1 else 0.0,
            "process_mean": statistics.mean(process_times),
            "process_min": min(process_times),
            "process_std": statistics.stdev(process_times) if len(process_times) > 1 else 0.0,
        }

    # Summary table
    print("\n" + "="*80)
    print("BENCHMARK SUMMARY RESULTS: fib(40)")
    print("="*80)
    header = f"{'Implementation':<38} | {'Internal Time (s)':<17} | {'Total Process (s)':<17} | {'vs Python':<10}"
    print(header)
    print("-" * len(header))

    py_mean = all_results["python"]["internal_mean"]
    for t_id, data in all_results.items():
        speedup_vs_py = py_mean / data["internal_mean"]
        print(f"{data['name']:<38} | {data['internal_mean']:>8.4f} +/- {data['internal_std']:<5.4f} | {data['process_mean']:>8.4f} +/- {data['process_std']:<5.4f} | {speedup_vs_py:>8.2f}x")

    with open(os.path.join(BENCH_DIR, "benchmark_results.json"), "w") as f:
        json.dump(all_results, f, indent=2)

if __name__ == "__main__":
    main()

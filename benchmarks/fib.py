import time
import sys

def fib(n):
    if n < 2:
        return n
    return fib(n - 1) + fib(n - 2)

if __name__ == "__main__":
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 40
    t0 = time.perf_counter()
    res = fib(n)
    t1 = time.perf_counter()
    elapsed = t1 - t0
    print(f"RESULT={res}")
    print(f"TIME={elapsed:.6f}")


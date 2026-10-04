#include <stdio.h>
#include <stdint.h>
#include <windows.h>

static int64_t fib(int64_t n) {
    if (n < 2) return n;
    return fib(n - 1) + fib(n - 2);
}

int main(void) {
    LARGE_INTEGER freq, t0, t1;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t0);
    int64_t res = fib(40);
    QueryPerformanceCounter(&t1);
    double elapsed_sec = (double)(t1.QuadPart - t0.QuadPart) / (double)freq.QuadPart;
    printf("RESULT=%lld\n", (long long)res);
    printf("TIME=%.6f\n", elapsed_sec);
    return 0;
}

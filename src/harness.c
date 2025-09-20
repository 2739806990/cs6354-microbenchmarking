#include "harness.h"
#include <time.h>
#include <stdint.h>
#include <stdlib.h>
#include <mach/mach_time.h> 

#if defined(__APPLE__)
  #include <mach/mach_time.h>
#endif

static int cmp_u64(const void* a, const void* b){
    uint64_t x = *(const uint64_t*)a, y = *(const uint64_t*)b;
    return (x>y) - (x<y);
}

uint64_t now_ns(void) {
#if defined(__APPLE__)
    static mach_timebase_info_data_t info = {0,0};
    if (info.denom == 0) mach_timebase_info(&info);
    uint64_t t = mach_absolute_time();
    // 关键：用 128 位中间值，避免 64 位乘法溢出
    __uint128_t wide = ( (__uint128_t)t * (__uint128_t)info.numer );
    wide /= (__uint128_t)info.denom;
    return (uint64_t)wide;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (uint64_t)ts.tv_sec*1000000000ull + (uint64_t)ts.tv_nsec;
#endif
}

double median_ns(const uint64_t *a, size_t n){
    uint64_t* tmp = (uint64_t*)malloc(n*sizeof(uint64_t));
    for(size_t i=0;i<n;++i) tmp[i]=a[i];
    qsort(tmp, n, sizeof(uint64_t), cmp_u64);
    double med = (n%2)? tmp[n/2] : 0.5*(tmp[n/2-1]+tmp[n/2]);
    free(tmp);
    return med;
}

void warmup_busy_loop(size_t iters){
    volatile uint64_t x=0;
    for (size_t i=0;i<iters;++i) x += i;
}

uint64_t timer_overhead_ns(void){
    const size_t REPEAT = 2000;
    const int K = 16; // 每轮读 K 次时间，放大粒度
    uint64_t *samples = (uint64_t*)malloc(REPEAT * sizeof(uint64_t));
    for (size_t r=0; r<REPEAT; ++r){
        uint64_t t0 = now_ns();
        
        uint64_t t1 = now_ns();
        uint64_t total = t1 - t0;
        samples[r] = total / (uint64_t)K; // 每次读表的平均 ns
    }
    double med = median_ns(samples, REPEAT);
    free(samples);
    // 至少返回 1ns，避免 0 影响差分
    uint64_t ret = (uint64_t)(med < 1.0 ? 1.0 : med);
    return ret;
}

// 可单独运行测试
#ifdef HARNESS_STANDALONE
#include <stdio.h>
int main(void){
    warmup_busy_loop(1000000);
    uint64_t oh = timer_overhead_ns();
    printf("harness ok. timer_overhead_ns ~ %llu ns\n",(unsigned long long)oh);
    return 0;
}
#endif
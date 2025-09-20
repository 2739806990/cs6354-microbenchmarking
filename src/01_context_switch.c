// 01_context_switch.c
// 两部分：A) 系统调用往返开销  B) 线程 ping-pong 切换开销
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <math.h>
#include "harness.h"
#include <sys/syscall.h>   // 新增：使用 syscall(SYS_getpid) 强制走内核
#include <dispatch/dispatch.h>   // ← 新增：Apple GCD 信号量

// -------- 通用工具：double 的中位数 --------
static double dmedian(double* a, size_t n){
    // 插入排序 + 中位数（n 不大，简单稳定）
    for(size_t i=1;i<n;++i){
        double key=a[i]; size_t j=i;
        while(j>0 && a[j-1]>key){ a[j]=a[j-1]; --j; }
        a[j]=key;
    }
    return (n%2)? a[n/2] : 0.5*(a[n/2-1]+a[n/2]);
}

// -------- A) 系统调用往返：强制进内核 + 放大K次 --------
static double syscall_roundtrip_ns(size_t iters){
    const size_t REPEAT = 21;
    const int K = 32;  // 每次循环做 K 次真正的系统调用，放大到可测范围
    double *samples = (double*)malloc(REPEAT*sizeof(double));
    const uint64_t tovh = timer_overhead_ns();

    for(size_t r=0;r<REPEAT;++r){
        // 基线：空循环（与真实循环结构一致）
        uint64_t t0 = now_ns();
        for(size_t i=0;i<iters;++i){
            for (int k=0;k<K;++k){
            }
        }
        uint64_t t1 = now_ns();
        uint64_t base_ns = t1 - t0;

        // 真实：每轮做 K 次 syscall(SYS_getpid)
        t0 = now_ns();
        for(size_t i=0;i<iters;++i){
            for (int k=0;k<K;++k){
                (void)syscall(SYS_getpid);
            }
        }
        t1 = now_ns();
        uint64_t with_sys_ns = t1 - t0;

        int64_t diff = (int64_t)with_sys_ns - (int64_t)base_ns - (int64_t)(2*tovh);
        if (diff < 0) diff = 0;

        // 除以总次数 iters*K 得到单次系统调用 ns
        samples[r] = (double)diff / (double)((iters?iters:1) * K);
    }

    double med = dmedian(samples, REPEAT);
    free(samples);
    return floor(med*10.0 + 0.5)/10.0; // 保留 1 位小数
}

// -------- B) 线程 ping-pong（GCD 信号量 + 超时保护） --------
typedef struct {
    dispatch_semaphore_t sem_main;    // worker -> main
    dispatch_semaphore_t sem_worker;  // main   -> worker
    size_t rounds;                    // 往返次数
    volatile int start;
    int warmup;
} pingpong_ctx;

static void* worker_thread(void* arg){
    pingpong_ctx* p = (pingpong_ctx*)arg;
    while(!p->start) { /* spin */ }

    size_t total = (size_t)p->warmup + p->rounds;  // 关键：吃掉预热 + 正式轮次
    for (size_t r=0; r<total; ++r) {
        if (dispatch_semaphore_wait(p->sem_worker,
            dispatch_time(DISPATCH_TIME_NOW, 1LL*1000*1000*1000)) != 0) {
            fprintf(stderr, "[worker] timeout waiting sem_worker at r=%zu\n", r);
            return NULL;
        }
        dispatch_semaphore_signal(p->sem_main);
    }
    return NULL;
}



static double thread_switch_ns(size_t rounds){
    const size_t REPEAT = 7;  // 次数不需要太多
    double *samples = (double*)malloc(REPEAT*sizeof(double));
    const uint64_t tovh = timer_overhead_ns();

    for (size_t r=0; r<REPEAT; ++r) {
        pingpong_ctx ctx;
        ctx.rounds = rounds;
        ctx.start  = 0;

        ctx.sem_main   = dispatch_semaphore_create(0);
        ctx.sem_worker = dispatch_semaphore_create(0);

        pthread_t th;
        int rc = pthread_create(&th, NULL, worker_thread, &ctx);
        if (rc != 0){ fprintf(stderr,"pthread_create failed\n"); exit(1); }

        int warmup = (int)((rounds < 200) ? rounds : 200); // 预热次数 ≤ rounds
        ctx.warmup = warmup;

        ctx.start = 1;

        // 预热（不计时）：先发 200 个球并接回
        for (int i=0; i<200; ++i) {
            dispatch_semaphore_signal(ctx.sem_worker); // main -> worker
            if (dispatch_semaphore_wait(ctx.sem_main, dispatch_time(DISPATCH_TIME_NOW, 1LL*1000*1000*1000)) != 0) {
                fprintf(stderr, "[main] warmup timeout at i=%d\n", i);
                pthread_join(th, NULL);
                samples[r] = NAN;
                goto NEXT_ROUND;
            }
        }

        // 正式计时
        uint64_t t0 = now_ns();
        for (size_t i=0; i<rounds; ++i) {
            dispatch_semaphore_signal(ctx.sem_worker); // main -> worker
            if (dispatch_semaphore_wait(ctx.sem_main, dispatch_time(DISPATCH_TIME_NOW, 1LL*1000*1000*1000)) != 0) {
                fprintf(stderr, "[main] timed run timeout at i=%zu\n", i);
                pthread_join(th, NULL);
                samples[r] = NAN;
                goto NEXT_ROUND;
            }
        }
        uint64_t t1 = now_ns();

        pthread_join(th, NULL);

        {
            int64_t net = (int64_t)(t1 - t0) - (int64_t)(2*tovh);
            if (net < 0) net = 0;
            samples[r] = (double)net / (double)(rounds*2 ? rounds*2 : 1);
        }

    NEXT_ROUND:
        ; // label 后空语句
    }

    // 过滤 NAN（如有超时）
    size_t m=0;
    for(size_t i=0;i<REPEAT;++i) if (!isnan(samples[i])) samples[m++]=samples[i];
    double med = (m? dmedian(samples, m) : NAN);
    free(samples);
    if (isnan(med)) {
        fprintf(stderr, "[thread_switch_ns] all runs failed/timeout\n");
        return -1.0;
    }
    return floor(med*10.0 + 0.5)/10.0; // 保留 1 位小数
}

int main(void){
    // A) 系统调用往返
    size_t N_sys = 50000ull; // 200 万次，Apple Silicon 可轻松完成；慢的话降到 1e6
    
    double syscall_ns = syscall_roundtrip_ns(N_sys);
    printf("[Syscall round-trip] getpid() : %.1f ns/call\n", syscall_ns);

    // B) 线程 ping-pong（往返轮次）
    size_t N_rounds = 500; // 1 万次往返；视机器负载可调
    double cs_ns = thread_switch_ns(N_rounds);
    printf("[Thread ping-pong]  context switch : %.1f ns/switch\n", cs_ns);

    return 0;
}
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "harness.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include "harness.h"

#if defined(__GNUC__)
#define NOINLINE __attribute__((noinline))
#else
#define NOINLINE
#endif

typedef struct {
    int x, y, z, w;
} MyStruct;

NOINLINE void f8iiiiiiii(int a1,int a2,int a3,int a4,
                         int a5,int a6,int a7,int a8) {
    (void)a1;(void)a2;(void)a3;(void)a4;
    (void)a5;(void)a6;(void)a7;(void)a8;
}

NOINLINE void f1s(MyStruct s) {
    (void)s.x; (void)s.y; (void)s.z; (void)s.w;
}

NOINLINE MyStruct f_ret_s(void) {
    MyStruct s = {1,2,3,4};
    return s;
}

// 防优化：让编译器不消掉我们的操作
volatile uint64_t sink_u64;

// 一组“空函数”，不同参数形态
NOINLINE void f0(void) { }
NOINLINE void f1i(int a){ (void)a; }
NOINLINE void f2ii(int a,int b){ (void)a; (void)b; }
NOINLINE void f1d(double x){ (void)x; }

static double measure_call_cost_ns(void (*call_body)(void), size_t iters){
    const size_t REPEAT = 21;            // 多次重复，奇数便于取中位数
    double *samples = (double*)malloc(REPEAT*sizeof(double));
    const uint64_t tovh = timer_overhead_ns();

    for(size_t r=0; r<REPEAT; ++r){
        warmup_busy_loop(100000);

        uint64_t t0 = now_ns();
        for(size_t i=0;i<iters;++i){
            sink_u64 += i;
        }
        uint64_t t1 = now_ns();
        uint64_t base_ns = t1 - t0;

        t0 = now_ns();
        for(size_t i=0;i<iters;++i){
            (void)call_body();
        }
        t1 = now_ns();
        uint64_t with_call_ns = t1 - t0;

        // 带符号差分 + 截断
        int64_t diff = (int64_t)with_call_ns - (int64_t)base_ns - (int64_t)(2*tovh);
        if (diff < 0) diff = 0;
        samples[r] = (double)diff / (double)(iters ? iters : 1);
    }

    // 对 double 求中位数
    // 简单做法：复制到 uint64_t 排序不可取，这里直接选择排序 double
    // 轻量排序（插入排序即可，REPEAT很小）
    for (size_t i=1;i<REPEAT;++i){
        double key = samples[i];
        size_t j = i;
        while (j>0 && samples[j-1] > key){
            samples[j] = samples[j-1];
            --j;
        }
        samples[j] = key;
    }
    double med = samples[REPEAT/2];
    free(samples);
    // 保留 1 位小数
    return floor(med*10.0 + 0.5)/10.0;
}

// 为不同签名包一层适配
static void cb_f0(void){ f0(); }
static void cb_f1i(void){ f1i(42); }
static void cb_f2ii(void){ f2ii(1,2); }
static void cb_f1d(void){ f1d(3.14); }
static void cb_f8iiiiiiii(void){ f8iiiiiiii(1,2,3,4,5,6,7,8); }
static void cb_f1s(void){ MyStruct s = {10,20,30,40}; f1s(s); }
static void cb_f_ret_s(void){ MyStruct s = f_ret_s(); sink_u64 += s.x; }

int main(void){
    const size_t N = 10*1000*1000ull; // 5千万次迭代；若机器慢可降到 10,000,000
    printf("Measuring function call cost (ns per call), N=%zu\n", N);

    double c0   = measure_call_cost_ns(cb_f0,   N);
    double c1i  = measure_call_cost_ns(cb_f1i,  N);
    double c2ii = measure_call_cost_ns(cb_f2ii, N);
    double c1d  = measure_call_cost_ns(cb_f1d,  N);
    double c8i   = measure_call_cost_ns(cb_f8iiiiiiii, N);
    double c1s   = measure_call_cost_ns(cb_f1s, N);
    double cRets = measure_call_cost_ns(cb_f_ret_s, N);


    printf("  f()            : %f ns/call\n", c0);
    printf("  f(int)         : %f ns/call\n", c1i);
    printf("  f(int,int)     : %f ns/call\n", c2ii);
    printf("  f(double)      : %f ns/call\n", c1d);
    printf("  f(int,…×8)     : %f ns/call\n", c8i);
    printf("  f(struct)      : %f ns/call\n", c1s);
    printf("  f()->struct    : %f ns/call\n", cRets);
    return 0;
}

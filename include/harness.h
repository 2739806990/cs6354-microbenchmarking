#ifndef HARNESS_H
#define HARNESS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// 取当前时间（纳秒）
uint64_t now_ns(void);

// 统计工具
double median_ns(const uint64_t *a, size_t n);

// 计时函数自身调用开销（纳秒），用于扣除测量噪声
uint64_t timer_overhead_ns(void);

// 简单打乱/预热，减少冷启动影响
void warmup_busy_loop(size_t iters);

#ifdef __cplusplus
}
#endif
#endif

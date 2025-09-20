# CS6354 Microbenchmarking Project -- CP1

**Group Members:** nfz5mv, szz5ft

---

## Environment
- macOS (Apple Silicon)
- Clang 15.0.0
- C11 standard
- No external dependencies

## Directory Structure
- `include/` — common header file (`harness.h`)
- `src/` — source code:
  - `harness.c` — timing utilities
  - `00_function_call.c` — benchmark for function call overhead
  - `01_context_switch.c` — benchmark for syscall and thread context switches
- `scripts/` — helper scripts (`run_all.sh`)
- `report/` — report files (if required for CP1)
- `bin/` — compiled binaries (created by `make`)

## Build Instructions

make

## Run Instructions

- Run each benchmark manually:
./bin/00_function_call
./bin/01_context_switch
- run everything at once:
./scripts/run_all.sh

## Sample Output

Measuring function call cost (ns per call), N=10000000
  f()            : 0.600 ns/call
  f(int)         : 0.600 ns/call
  f(int,int)     : 0.500 ns/call
  f(double)      : 0.500 ns/call
[Syscall round-trip] getpid() : 112.4 ns/call
[Thread ping-pong]  context switch : 1193.3 ns/switch

## Notes
- On macOS, syscall(SYS_getpid) shows a deprecation warning; this is expected and does not affect correctness.
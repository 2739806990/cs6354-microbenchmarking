#!/usr/bin/env bash
set -e   # 遇到错误就退出

# 编译
make clean
make

# 运行所有 benchmark
./bin/00_function_call
./bin/01_context_switch
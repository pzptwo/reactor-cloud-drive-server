#!/bin/bash
# 云盘服务端压测：编译并运行 stress_test
# 用法: ./test.sh [连接数] [每连接请求数] [host] [port] [超时秒]
set -e
cd "$(dirname "$0")"

N=${1:-100}        # 默认 100 连接
R=${2:-1}          # 默认每连接 1 个请求
H=${3:-127.0.0.1}
P=${4:-8888}

echo "==> 编译压测程序"
make -B stress_test

echo "==> 开始压测 ${N} 连接 x ${R} 请求 -> ${H}:${P}"
./stress_test "$N" "$R" "$H" "$P" "${5:-10}"

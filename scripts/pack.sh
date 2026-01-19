#!/bin/sh

for i in $(seq 1 10); do
    echo "===== run $i ====="
    sudo ./build/kperf --tui -- ./test/cpu/test1
done
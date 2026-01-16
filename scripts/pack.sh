#!/bin/bash

set -e

cp build/bpf/* kperf/ebpf/
cp build/lib/* kperf/ebpf

cp libbpf/src/libbpf.so kperf/ebpf

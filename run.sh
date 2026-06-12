#!/usr/bin/env bash
set -e

# Build (neu can) va chay timeout_strategy tren Linux/macOS.
# Vi du:
#   ./run.sh data/three_process_deadlock.csv 3 kill
#   ./run.sh data/three_process_deadlock.csv 3 --compare

make
./timeout_strategy "$@"

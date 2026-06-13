#!/usr/bin/env bash
set -euo pipefail

EXECUTABLE="./build/timeout_strategy"
if [[ -f "${EXECUTABLE}.exe" ]]; then
    EXECUTABLE="${EXECUTABLE}.exe"
elif [[ ! -f "$EXECUTABLE" ]]; then
    echo "Error: executable not found at ./build/timeout_strategy"
    exit 1
fi

DATA_DIR="data"
OUTPUT_FILE="benchmark/benchmark_results.csv"
STRATEGIES=("kill" "retry" "rollback")

extract_metric() {
    local output="$1"
    local pattern="$2"
    local value

    value=$(echo "$output" | grep -i -E "$pattern" | grep -o -E '[0-9]+(\.[0-9]+)?' | tail -1)
    echo "${value:-0}"
}

echo "testcase,timeout,strategy,killed,resolved,throughput,fp_rate" > "$OUTPUT_FILE"

shopt -s nullglob
files=("$DATA_DIR"/tc_*.csv)

if [[ ${#files[@]} -eq 0 ]]; then
    echo "Error: no testcase files matching '$DATA_DIR/tc_*.csv'"
    exit 1
fi

echo "Running benchmark and writing results to $OUTPUT_FILE..."

for testcase in "${files[@]}"; do
    tc_name=$(basename "$testcase")
    echo "  Running: $tc_name"

    for timeout in $(seq 1 15); do
        for strategy in "${STRATEGIES[@]}"; do
            output=$("$EXECUTABLE" "$testcase" "$timeout" "$strategy" 2>&1) || true

            killed=$(extract_metric "$output" "killed")
            resolved=$(extract_metric "$output" "resolved")
            throughput=$(extract_metric "$output" "throughput")
            fp_rate=$(extract_metric "$output" "fp_rate|false.positive.rate")

            echo "$tc_name,$timeout,$strategy,$killed,$resolved,$throughput,$fp_rate" >> "$OUTPUT_FILE"
        done
    done
done

echo "Done. Results saved to $OUTPUT_FILE"

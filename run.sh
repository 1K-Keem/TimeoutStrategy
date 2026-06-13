#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

usage() {
	cat <<'EOF'
Usage:
	./run.sh <dataset.csv> [timeout] [kill|retry|rollback] [retry_delay] [max_retries|max_rollbacks] [-v|--verbose] [-c|--compare]
	./run.sh benchmark
EOF
}

resolve_executable() {
	local system
	system="$(uname -s)"

	local candidates=()
	case "$system" in
		MINGW*|MSYS*|CYGWIN*)
			candidates=("./timeout_strategy.exe" "./timeout_strategy")
			;;
		*)
			candidates=("./timeout_strategy" "./timeout_strategy.exe")
			;;
	esac

	for executable in "${candidates[@]}"; do
		if [[ -f "$executable" ]]; then
			printf '%s\n' "$executable"
			return
		fi
	done

	echo "Error: executable not found at ./timeout_strategy" >&2
	exit 1
}

build_executable() {
	make -B
}

extract_metric() {
	local output="$1"
	local pattern="$2"
	local value

	value=$(echo "$output" | grep -i -E "$pattern" | grep -o -E '[0-9]+(\.[0-9]+)?' | tail -1)
	echo "${value:-0}"
}

run_benchmark() {
	build_executable

	local executable
	executable=$(resolve_executable)
	local data_dir="data"
	local output_file="benchmark/benchmark_results.csv"
	local strategies=("kill" "retry" "rollback")

	echo "testcase,timeout,strategy,killed,resolved,throughput,fp_rate" > "$output_file"

	shopt -s nullglob
	local files=("$data_dir"/tc_*.csv)
	if [[ ${#files[@]} -eq 0 ]]; then
		echo "Error: no testcase files matching '$data_dir/tc_*.csv'" >&2
		exit 1
	fi

	echo "Running benchmark and writing results to $output_file..."

	for testcase in "${files[@]}"; do
		local tc_name
		tc_name=$(basename "$testcase")
		echo "  Running: $tc_name"

		for timeout in $(seq 1 15); do
			for strategy in "${strategies[@]}"; do
				local output
				output=$("$executable" "$testcase" "$timeout" "$strategy" 2>&1) || true

				local killed resolved throughput fp_rate
				killed=$(extract_metric "$output" "killed")
				resolved=$(extract_metric "$output" "resolved")
				throughput=$(extract_metric "$output" "throughput")
				fp_rate=$(extract_metric "$output" "fp_rate|false.positive.rate")

				echo "$tc_name,$timeout,$strategy,$killed,$resolved,$throughput,$fp_rate" >> "$output_file"
			done
		done
	done

	python benchmark/plot_benchmark.py

	echo "Done. Results saved to $output_file"
}

if [[ $# -eq 0 ]]; then
	usage
	exit 1
fi

case "$1" in
	benchmark|bench|-b|--benchmark)
		shift
		if [[ $# -ne 0 ]]; then
			echo "Error: benchmark mode does not accept extra arguments." >&2
			usage
			exit 1
		fi
		run_benchmark
		;;
	-h|--help)
		usage
		;;
	*)
		build_executable
		executable=$(resolve_executable)
		"$executable" "$@"
		;;
esac

#!/usr/bin/env bash
set -euo pipefail

# Run all benchmark steps: export results and generate plots

echo "=========================================="
echo "TimeoutStrategy Benchmark Suite"
echo "=========================================="
echo ""

# Step 1: Export benchmark results
echo "[1/2] Exporting benchmark results..."
bash benchmark/export_benchmark.sh
echo ""

# Step 2: Plot results
echo "[2/2] Generating benchmark plots..."
python benchmark/plot_benchmark.py
echo ""

echo "=========================================="
echo "Benchmark complete!"
echo "Results: benchmark/benchmark_results.csv"
echo "Plots: benchmark/*.png"
echo "=========================================="

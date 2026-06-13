#!/usr/bin/env python3
"""Plot benchmark results from benchmark_results.csv."""

from __future__ import annotations

import sys
from pathlib import Path

import matplotlib.pyplot as plt
from matplotlib.lines import Line2D
from matplotlib.patches import Patch
import pandas as pd
import seaborn as sns

CSV_FILE = Path("benchmark/benchmark_results.csv")
OUTPUT_DIR = Path("benchmark")

STRATEGIES = ("kill", "retry", "rollback")
TIMEOUT_MIN = 1
TIMEOUT_MAX = 15


def load_benchmark_data(csv_path: Path) -> pd.DataFrame:
    if not csv_path.is_file():
        raise FileNotFoundError(f"Benchmark file not found: {csv_path}")

    df = pd.read_csv(csv_path)

    required_columns = {
        "testcase",
        "timeout",
        "strategy",
        "killed",
        "resolved",
        "throughput",
        "fp_rate",
    }
    missing = required_columns - set(df.columns)
    if missing:
        raise ValueError(f"Missing required columns: {sorted(missing)}")

    df["timeout"] = df["timeout"].astype(int)
    df["strategy"] = df["strategy"].astype(str)

    for column in ("killed", "resolved", "throughput", "fp_rate"):
        df[column] = pd.to_numeric(df[column], errors="coerce").fillna(0)

    return df


def testcase_slug(testcase_name: str) -> str:
    return Path(testcase_name).stem


def plot_testcase(df: pd.DataFrame, testcase_name: str, palette: dict[str, tuple]) -> Path:
    tc_data = df[df["testcase"] == testcase_name].copy()
    if tc_data.empty:
        raise ValueError(f"No rows found for testcase: {testcase_name}")

    slug = testcase_slug(testcase_name)
    output_path = OUTPUT_DIR / f"{slug}_benchmark.png"

    sns.set_theme(style="whitegrid", context="notebook")

    # ── FIGURE: taller, wider, high DPI for report ──────────────────────────
    fig, (ax_ratio, ax_count) = plt.subplots(
        2,
        1,
        figsize=(18, 14),          # much larger canvas
        sharex=False,
        gridspec_kw={"height_ratios": [1, 1], "hspace": 0.40},
    )

    line_styles = {
        "throughput": "-",
        "fp_rate": "--",
    }
    strategy_markers = {"kill": "o", "retry": "s", "rollback": "^"}
    strategy_offsets = {"kill": -0.25, "retry": 0.0, "rollback": 0.25}
    timeouts = list(range(TIMEOUT_MIN, TIMEOUT_MAX + 1))

    # ── TOP PANEL: throughput (solid) and fp_rate (dashed) lines ────────────
    for strategy in STRATEGIES:
        strategy_data = (
            tc_data[tc_data["strategy"] == strategy]
            .sort_values("timeout")
            .set_index("timeout")
            .reindex(timeouts, fill_value=0)
            .reset_index()
        )
        color = palette[strategy]
        x_offset = strategy_offsets[strategy]

        ax_ratio.plot(
            strategy_data["timeout"] + x_offset,
            strategy_data["throughput"],
            linestyle=line_styles["throughput"],
            color=color,
            linewidth=2.5,
            marker=strategy_markers[strategy],
            markersize=7,
            label=f"{strategy} – throughput",
        )
        ax_ratio.plot(
            strategy_data["timeout"] + x_offset,
            strategy_data["fp_rate"],
            linestyle=line_styles["fp_rate"],
            color=color,
            linewidth=2.5,
            marker=strategy_markers[strategy],
            markersize=7,
            label=f"{strategy} – fp_rate",
        )

    # ── BOTTOM PANEL: grouped bars (killed solid, resolved hatched) ──────────
    bar_width = 0.10
    strategy_gap = 0.04
    metric_gap = 0.28
    x_positions = list(range(len(timeouts)))

    for metric_idx, metric in enumerate(("killed", "resolved")):
        metric_center = (metric_idx - 0.5) * (len(STRATEGIES) * bar_width + strategy_gap + metric_gap)
        for strategy_idx, strategy in enumerate(STRATEGIES):
            strategy_data = (
                tc_data[tc_data["strategy"] == strategy]
                .sort_values("timeout")
                .set_index("timeout")
                .reindex(timeouts, fill_value=0)
                .reset_index()
            )
            offset = metric_center + (strategy_idx - (len(STRATEGIES) - 1) / 2) * bar_width
            hatch = "" if metric == "killed" else "//"
            ax_count.bar(
                [x + offset for x in x_positions],
                strategy_data[metric],
                width=bar_width * 0.9,
                color=palette[strategy],
                hatch=hatch,
                edgecolor="white",
                linewidth=0.5,
                alpha=0.9,
            )

    # ── TOP PANEL FORMATTING ─────────────────────────────────────────────────
    ax_ratio.set_title("Performance and Accuracy", pad=14, fontsize=15, fontweight="bold")
    ax_ratio.set_ylabel("Ratio", fontsize=13)
    ax_ratio.set_xlim(0.5, TIMEOUT_MAX + 0.5)
    ax_ratio.set_ylim(0, 1.25)
    ax_ratio.set_xticks(timeouts)
    ax_ratio.tick_params(axis="both", labelsize=12)
    ax_ratio.set_xlabel("Timeout", fontsize=13)
    ax_ratio.grid(True, alpha=0.3)

    # ── BOTTOM PANEL FORMATTING ──────────────────────────────────────────────
    killed_max = max(tc_data["killed"].max(), tc_data["resolved"].max(), 1)
    ax_count.set_title("Cost and Resolution", pad=14, fontsize=15, fontweight="bold")
    ax_count.set_ylabel("Count", fontsize=13)
    ax_count.set_ylim(0, killed_max * 1.15)
    ax_count.set_xlim(-0.6, len(timeouts) - 0.4)
    ax_count.set_xticks(x_positions)
    ax_count.set_xticklabels(timeouts, fontsize=12)
    ax_count.set_xlabel("Timeout", fontsize=13, labelpad=10)
    ax_count.tick_params(axis="y", labelsize=12)
    ax_count.grid(True, axis="y", alpha=0.3)

    # ── LEGEND: two clearly separated groups ─────────────────────────────────
    # Group 1 – strategy colours
    strategy_handles = [
        Line2D([0], [0], color=palette[s], linewidth=3,
               marker=strategy_markers[s], markersize=8, label=s.capitalize())
        for s in STRATEGIES
    ]

    # Group 2 – line / bar styles
    style_handles = [
        Line2D([0], [0], color="gray", linewidth=3,
               linestyle="-",  label="throughput"),
        Line2D([0], [0], color="gray", linewidth=3,
               linestyle="--", label="false positive rate"),
        Patch(facecolor="gray", edgecolor="white", hatch="",
              label="killed processes"),
        Patch(facecolor="gray", edgecolor="white", hatch="//",
              label="deadlock resolved"),
    ]

    # Separator: blank handle
    separator = Line2D([0], [0], color="none", label=" ")

    all_handles = strategy_handles + [separator] + style_handles

    legend = fig.legend(
        handles=all_handles,
        loc="lower center",
        bbox_to_anchor=(0.5, 0.01),  # sits inside bottom margin
        ncol=4,
        frameon=True,
        fontsize=11,
        columnspacing=1.8,
        handletextpad=0.8,
        borderpad=0.8,
        title="Strategy                                           Style / Metric",
        title_fontsize=11,
    )

    # ── OVERALL TITLE & LAYOUT ────────────────────────────────────────────────
    fig.suptitle(f"Timeout Strategy Benchmark: {slug}", fontsize=16, fontweight="bold", y=0.98)
    fig.subplots_adjust(left=0.07, right=0.97, top=0.93, bottom=0.16, hspace=0.40)

    fig.savefig(output_path, dpi=180, bbox_inches="tight")
    plt.close(fig)

    return output_path


def main() -> int:
    try:
        df = load_benchmark_data(CSV_FILE)
    except FileNotFoundError as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1
    except ValueError as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1

    palette = dict(zip(STRATEGIES, sns.color_palette("deep", n_colors=len(STRATEGIES))))
    testcases = sorted(df["testcase"].unique(), key=testcase_slug)

    generated = []
    for testcase in testcases:
        try:
            output_path = plot_testcase(df, testcase, palette)
            generated.append(output_path)
            print(f"Saved: {output_path}")
        except ValueError as exc:
            print(f"Warning: {exc}", file=sys.stderr)

    if not generated:
        print("Error: no plots were generated.", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
#!/usr/bin/env python3
"""Chart spsc_head_to_head.csv into per-metric comparison PNGs."""
import argparse
import csv
import pathlib

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

SCRIPT_DIR = pathlib.Path(__file__).resolve().parent
DEFAULT_CSV = SCRIPT_DIR / "spsc_head_to_head.csv"

# Fixed categorical order + validated palette (dataviz skill, references/palette.md,
# slots 1-5). Color always follows target identity, never plot position.
TARGET_COLORS = {
    "mine": "#2a78d6",
    "rigtorp": "#eb6834",
    "moodycamel": "#1baf7a",
    "folly": "#eda100",
    "boost": "#e87ba4",
}
TARGET_ORDER = list(TARGET_COLORS)

INK_PRIMARY = "#0b0b0b"
INK_SECONDARY = "#52514e"
INK_MUTED = "#898781"
GRID = "#e1e0d9"
SURFACE = "#fcfcfb"

PERCENTILE_METRICS = ["p25_ns", "p50_ns", "p75_ns", "p99_ns"]
PERCENTILE_LABELS = ["P25", "P50", "P75", "P99"]
TC_TITLES = {1: "TC1 — throughput", 2: "TC2 — req/resp", 3: "TC3 — burst drain"}
METRIC_LABELS = {
    "cycles": "CPU cycles",
    "instructions": "Instructions retired",
    "llc_load_misses": "LLC load misses",
    "elapsed_sec": "Elapsed time (s)",
    "ipc": "Instructions per cycle (IPC)",
    "llc_misses_per_kinstr": "LLC misses / 1K instructions",
}
# Filename-safe tags used to build one-file-per-chart names, e.g. TC1_Cycles.png.
METRIC_FILE_TAGS = {
    "cycles": "Cycles",
    "instructions": "Instructions",
    "llc_load_misses": "LLC_Misses",
    "elapsed_sec": "Elapsed",
    "ipc": "IPC",
    "llc_misses_per_kinstr": "LLC_Misses_per_Kinstr",
}


def load_rows(csv_path):
    with open(csv_path, newline="") as f:
        rows = list(csv.DictReader(f))
    for r in rows:
        r["queue_size"] = int(r["queue_size"])
        r["test_case"] = int(r["test_case"])
        r["value"] = float(r["value"])
    return rows


def style_axes(ax):
    ax.set_facecolor(SURFACE)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.spines["left"].set_color(GRID)
    ax.spines["bottom"].set_color(GRID)
    ax.tick_params(colors=INK_MUTED, labelsize=9)
    ax.yaxis.grid(True, color=GRID, linewidth=0.8)
    ax.set_axisbelow(True)
    ax.title.set_color(INK_PRIMARY)
    ax.xaxis.label.set_color(INK_SECONDARY)
    ax.yaxis.label.set_color(INK_SECONDARY)


def bar_group(ax, categories, values_by_target, targets):
    """Grouped bars, one group per category, one bar per target. Direct value
    labels above each bar satisfy the palette's relief requirement for the
    lower-contrast slots (aqua/yellow/magenta) instead of relying on hue alone."""
    n = len(targets)
    width = 0.8 / n
    xs = range(len(categories))
    for i, t in enumerate(targets):
        offsets = [x + (i - (n - 1) / 2) * width for x in xs]
        vals = [values_by_target[t].get(c) for c in categories]
        bars = ax.bar(offsets, [v if v is not None else 0 for v in vals],
                       width=width * 0.92, color=TARGET_COLORS[t], label=t)
        for b, v in zip(bars, vals):
            if v is None:
                continue
            if v >= 100:
                label = f"{v:,.0f}"
            elif v >= 1:
                label = f"{v:.2f}"
            else:
                label = f"{v:.4f}"
            ax.annotate(label, (b.get_x() + b.get_width() / 2, b.get_height()),
                        textcoords="offset points", xytext=(0, 3),
                        ha="center", va="bottom", fontsize=6, color=INK_SECONDARY,
                        rotation=90 if n > 3 else 0)
    ax.set_xticks(list(xs))
    ax.set_xticklabels([str(c) for c in categories])


def targets_present(rows):
    seen = {r["target"] for r in rows}
    return [t for t in TARGET_ORDER if t in seen]


def add_legend(fig, axes, targets, y):
    handles, labels = axes[0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="upper center", bbox_to_anchor=(0.5, y),
               ncol=len(targets), frameon=False, fontsize=9, labelcolor=INK_SECONDARY)


def plot_perf_metric(rows, metric, out_dir):
    """One PNG per (test_case, metric) — e.g. TC1_Cycles.png, TC2_IPC.png."""
    metric_rows = [r for r in rows if r["metric"] == metric]
    if not metric_rows:
        return []

    written = []
    for tc in sorted({r["test_case"] for r in metric_rows}):
        tc_rows = [r for r in metric_rows if r["test_case"] == tc]
        targets = targets_present(tc_rows)
        sizes = sorted({r["queue_size"] for r in tc_rows})
        values_by_target = {t: {} for t in targets}
        for r in tc_rows:
            values_by_target[r["target"]][r["queue_size"]] = r["value"]

        fig, ax = plt.subplots(figsize=(6.5, 4.5), facecolor=SURFACE)
        bar_group(ax, sizes, values_by_target, targets)
        style_axes(ax)
        ax.set_xlabel("Queue size (N)")
        ax.set_ylabel(METRIC_LABELS.get(metric, metric))

        add_legend(fig, [ax], targets, 1.06)
        fig.suptitle(f"{TC_TITLES.get(tc, f'TC{tc}')} — {METRIC_LABELS.get(metric, metric)}",
                     color=INK_PRIMARY, fontsize=13, y=1.16)
        fig.tight_layout()
        tag = METRIC_FILE_TAGS.get(metric, metric)
        out_path = out_dir / f"TC{tc}_{tag}.png"
        fig.savefig(out_path, dpi=150, bbox_inches="tight", facecolor=SURFACE)
        plt.close(fig)
        written.append(out_path)
    return written


def plot_percentiles(rows, out_dir):
    """One PNG per queue size — e.g. TC3_N128_Percentiles.png."""
    tc3_rows = [r for r in rows if r["metric"] in PERCENTILE_METRICS]
    if not tc3_rows:
        return []

    written = []
    for qsize in sorted({r["queue_size"] for r in tc3_rows}):
        size_rows = [r for r in tc3_rows if r["queue_size"] == qsize]
        targets = targets_present(size_rows)
        values_by_target = {t: {} for t in targets}
        for r in size_rows:
            values_by_target[r["target"]][r["metric"]] = r["value"]

        fig, ax = plt.subplots(figsize=(5.5, 4.5), facecolor=SURFACE)
        bar_group(ax, PERCENTILE_METRICS, values_by_target, targets)
        style_axes(ax)
        ax.set_xticklabels(PERCENTILE_LABELS)
        ax.set_xlabel("Percentile")
        ax.set_ylabel("Latency (ns)")

        add_legend(fig, [ax], targets, 1.06)
        fig.suptitle(f"TC3 burst latency percentiles — N={qsize}",
                     color=INK_PRIMARY, fontsize=13, y=1.16)
        fig.tight_layout()
        out_path = out_dir / f"TC3_N{qsize}_Percentiles.png"
        fig.savefig(out_path, dpi=150, bbox_inches="tight", facecolor=SURFACE)
        plt.close(fig)
        written.append(out_path)
    return written


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--csv", type=pathlib.Path, default=DEFAULT_CSV,
                         help="input CSV (default: %(default)s)")
    parser.add_argument("--out-dir", type=pathlib.Path, default=SCRIPT_DIR,
                         help="directory to write PNGs into (default: script directory)")
    args = parser.parse_args()

    if not args.csv.exists():
        raise SystemExit(f"CSV not found: {args.csv} (run spsc_head_to_head.sh first)")

    rows = load_rows(args.csv)
    args.out_dir.mkdir(parents=True, exist_ok=True)

    perf_metrics = sorted({r["metric"] for r in rows if r["metric"] not in PERCENTILE_METRICS})
    written = []
    for m in perf_metrics:
        written.extend(plot_perf_metric(rows, m, args.out_dir))
    written.extend(plot_percentiles(rows, args.out_dir))

    if not written:
        raise SystemExit("No known metrics found in CSV — nothing plotted.")
    for p in written:
        print(f"wrote {p}")


if __name__ == "__main__":
    main()

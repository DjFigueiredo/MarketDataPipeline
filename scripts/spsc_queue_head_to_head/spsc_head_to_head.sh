#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BENCH="$REPO_ROOT/build/bench/spsc_head_to_head"
OUT="$SCRIPT_DIR/spsc_head_to_head.md"
RAW_CSV="$SCRIPT_DIR/spsc_head_to_head.raw.csv"
CSV_OUT="$SCRIPT_DIR/spsc_head_to_head.csv"

TARGETS=("mine" "rigtorp" "moodycamel" "folly" "boost")
QUEUE_SIZES=(64 128 256 512 1024)
# All test cases now run under perf stat. TC3's own stdout (P25/P50/P75/P99
# histogram lines) and perf's counter lines share the same captured blob —
# each awk pass below just picks out the lines it cares about.
TEST_CASES=(1 2 3)
# Each (target, queue_size, test_case) is measured this many times; the
# final CSV holds the per-metric median across those runs, not a single
# noisy sample. Raw per-run values are kept in RAW_CSV for audit.
REPEATS=3

mkdir -p "$(dirname "$OUT")"
echo "target,queue_size,test_case,run,metric,value" > "$RAW_CSV"

# Parses one captured perf-stat(+binary-stdout) blob into raw CSV rows and
# appends them to RAW_CSV. Field-based awk matching, not regex-fragile.
emit_csv_rows() {
    local target="$1" qsize="$2" tc="$3" run="$4" text="$5"

    # Also derives ipc (instructions/cycle) and llc_misses_per_kinstr
    # (LLC-load-misses per 1K instructions) from the raw counters —
    # raw miss counts alone aren't comparable across targets that do
    # different amounts of total work per op.
    awk -v t="$target" -v q="$qsize" -v c="$tc" -v r="$run" '
        /[[:space:]]cycles([[:space:]]|$)/           { gsub(",", "", $1); cycles=$1; print t","q","c","r",cycles,"$1 }
        /[[:space:]]instructions([[:space:]]|$)/     { gsub(",", "", $1); instr=$1; print t","q","c","r",instructions,"$1 }
        /[[:space:]]LLC-load-misses([[:space:]]|$)/  { gsub(",", "", $1); misses=$1; print t","q","c","r",llc_load_misses,"$1 }
        /seconds time elapsed/                       { print t","q","c","r",elapsed_sec,"$1 }
        END {
            if (cycles > 0) printf "%s,%s,%s,%s,ipc,%.6f\n", t, q, c, r, instr / cycles
            if (instr > 0)  printf "%s,%s,%s,%s,llc_misses_per_kinstr,%.6f\n", t, q, c, r, (misses / instr) * 1000
        }
    ' <<< "$text" >> "$RAW_CSV"

    # Only present for TC3; no-op (matches nothing) for TC1/TC2.
    awk -v t="$target" -v q="$qsize" -v c="$tc" -v r="$run" '
        /^P25:/ { print t","q","c","r",p25_ns,"$2 }
        /^P50:/ { print t","q","c","r",p50_ns,"$2 }
        /^P75:/ { print t","q","c","r",p75_ns,"$2 }
        /^P99:/ { print t","q","c","r",p99_ns,"$2 }
    ' <<< "$text" >> "$RAW_CSV"
}

# Collapses RAW_CSV (one row per run) into CSV_OUT (one row per metric),
# taking the median across REPEATS runs. Grouping order is preserved by
# first-appearance so the output stays readable, not hash-shuffled.
compute_medians() {
    awk -F, '
        NR == 1 { next }
        {
            key = $1","$2","$3","$5
            if (!(key in seen)) { order[++n_keys] = key; seen[key] = 1 }
            count[key]++
            values[key SUBSEP count[key]] = $6 + 0
        }
        END {
            print "target,queue_size,test_case,metric,value"
            for (i = 1; i <= n_keys; i++) {
                key = order[i]
                cnt = count[key]
                for (j = 1; j <= cnt; j++) v[j] = values[key SUBSEP j]
                for (j = 2; j <= cnt; j++) {
                    x = v[j]; k = j - 1
                    while (k >= 1 && v[k] > x) { v[k+1] = v[k]; k-- }
                    v[k+1] = x
                }
                if (cnt % 2 == 1) median = v[(cnt+1)/2]
                else median = (v[cnt/2] + v[cnt/2 + 1]) / 2
                print key "," median
            }
        }
    ' "$RAW_CSV" > "$CSV_OUT"
}

{
    echo "# SPSC Head-to-Head Results"
    echo ""
    echo "Generated: $(date)"
    echo "Host:      $(uname -n)"
    echo "Kernel:    $(uname -r)"
    echo "Repeats:   $REPEATS (median reported in CSV)"
    echo ""

    for target in "${TARGETS[@]}"; do
        echo "---"
        echo ""
        echo "## $target"
        echo ""

        for qsize in "${QUEUE_SIZES[@]}"; do
            echo "### N=$qsize"
            echo ""

            for tc in "${TEST_CASES[@]}"; do
                echo "#### TC$tc"
                echo ""

                for run in $(seq 1 "$REPEATS"); do
                    echo "Run $run:"
                    echo '```'

                    result=$(perf stat -e cycles,instructions,LLC-load-misses \
                        "$BENCH" "$target" "$tc" "$qsize" 2>&1)

                    echo "$result"
                    emit_csv_rows "$target" "$qsize" "$tc" "$run" "$result"

                    echo '```'
                    echo ""
                done
            done
        done
    done
} | tee "$OUT"

compute_medians

echo ""
echo "Results written to $OUT"
echo "Raw per-run CSV written to $RAW_CSV"
echo "Median CSV written to $CSV_OUT"

PYTHON="$SCRIPT_DIR/.venv/bin/python"
[[ -x "$PYTHON" ]] || PYTHON="python3"
"$PYTHON" "$SCRIPT_DIR/plot_results.py" --csv "$CSV_OUT" --out-dir "$SCRIPT_DIR"

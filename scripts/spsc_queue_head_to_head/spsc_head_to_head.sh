#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BENCH="$REPO_ROOT/build/bench/spsc_head_to_head"
OUT="$SCRIPT_DIR/spsc_head_to_head.md"
CSV_OUT="$SCRIPT_DIR/spsc_head_to_head.csv"

TARGETS=("mine" "rigtorp" "moodycamel" "folly" "boost")
QUEUE_SIZES=(64 128 256 512 1024)
# TC1/TC2: measured via perf stat — output captured directly
# TC3:     internal rdtscp histogram — output captured from binary stdout
TEST_CASES=(1 2 3)

mkdir -p "$(dirname "$OUT")"
echo "target,queue_size,test_case,metric,value" > "$CSV_OUT"

# Parses one captured perf-stat/binary-stdout blob into CSV rows and
# appends them to CSV_OUT. Field-based awk matching, not regex-fragile.
emit_csv_rows() {
    local target="$1" qsize="$2" tc="$3" text="$4"
    if [[ $tc -le 2 ]]; then
        # Also derives ipc (instructions/cycle) and llc_misses_per_kinstr
        # (LLC-load-misses per 1K instructions) from the raw counters —
        # raw miss counts alone aren't comparable across targets that do
        # different amounts of total work per op.
        awk -v t="$target" -v q="$qsize" -v c="$tc" '
            /[[:space:]]cycles([[:space:]]|$)/           { gsub(",", "", $1); cycles=$1; print t","q","c",cycles,"$1 }
            /[[:space:]]instructions([[:space:]]|$)/     { gsub(",", "", $1); instr=$1; print t","q","c",instructions,"$1 }
            /[[:space:]]LLC-load-misses([[:space:]]|$)/  { gsub(",", "", $1); misses=$1; print t","q","c",llc_load_misses,"$1 }
            /seconds time elapsed/                       { print t","q","c",elapsed_sec,"$1 }
            END {
                if (cycles > 0) printf "%s,%s,%s,ipc,%.6f\n", t, q, c, instr / cycles
                if (instr > 0)  printf "%s,%s,%s,llc_misses_per_kinstr,%.6f\n", t, q, c, (misses / instr) * 1000
            }
        ' <<< "$text" >> "$CSV_OUT"
    else
        awk -v t="$target" -v q="$qsize" -v c="$tc" '
            /^P25:/ { print t","q","c",p25_ns,"$2 }
            /^P50:/ { print t","q","c",p50_ns,"$2 }
            /^P75:/ { print t","q","c",p75_ns,"$2 }
            /^P99:/ { print t","q","c",p99_ns,"$2 }
        ' <<< "$text" >> "$CSV_OUT"
    fi
}

{
    echo "# SPSC Head-to-Head Results"
    echo ""
    echo "Generated: $(date)"
    echo "Host:      $(uname -n)"
    echo "Kernel:    $(uname -r)"
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
                echo '```'

                if [[ $tc -le 2 ]]; then
                    # TC1/TC2 — wrap with perf stat
                    result=$(perf stat -e cycles,instructions,LLC-load-misses \
                        "$BENCH" "$target" "$tc" "$qsize" 2>&1)
                else
                    # TC3 — internal histogram, just capture stdout
                    result=$("$BENCH" "$target" "$tc" "$qsize" 2>&1)
                fi

                echo "$result"
                emit_csv_rows "$target" "$qsize" "$tc" "$result"

                echo '```'
                echo ""
            done
        done
    done
} | tee "$OUT"

echo ""
echo "Results written to $OUT"
echo "CSV written to $CSV_OUT"

PYTHON="$SCRIPT_DIR/.venv/bin/python"
[[ -x "$PYTHON" ]] || PYTHON="python3"
"$PYTHON" "$SCRIPT_DIR/plot_results.py" --csv "$CSV_OUT" --out-dir "$SCRIPT_DIR"

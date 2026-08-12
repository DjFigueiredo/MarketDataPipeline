#!/usr/bin/env bash
set -e

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BENCH="$REPO_ROOT/build/bench/spsc_head_to_head"
OUT="$REPO_ROOT/notes/external/timing/spsc_head_to_head.md"

TARGETS=("mine" "rigtorp" "moodycamel" "folly" "boost")
# TC1/TC2: measured via perf stat — output captured directly
# TC3:     internal rdtscp histogram — output captured from binary stdout
TEST_CASES=(1 2 3 4 5 6)

mkdir -p "$(dirname "$OUT")"

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

        for tc in "${TEST_CASES[@]}"; do
            echo "### TC$tc"
            echo ""
            echo '```'

            if [[ $tc -le 4 ]]; then
                # TC1/TC2 — wrap with perf stat
                perf stat -e cycles,instructions,LLC-load-misses \
                    "$BENCH" "$target" "$tc" 2>&1
            else
                # TC3 — internal histogram, just capture stdout
                "$BENCH" "$target" "$tc" 2>&1
            fi

            echo '```'
            echo ""
        done
    done
} | tee "$OUT"

echo ""
echo "Results written to $OUT"

// counter_bench.cpp — DJ writes this. Spec: notes/THREADING_WARMUP.md
// Variants: 1 mutex | 2 single atomic (relaxed) | 3 sharded unpadded | 4 sharded alignas(64)
// Rules: time only the increment loop, best of several runs, print totals so the
// compiler can't delete the work. Release build (-O2/-O3) is set at project level.
// Stub exists only so the build configures before tonight's code lands. Replace entirely.
int main() { return 0; }

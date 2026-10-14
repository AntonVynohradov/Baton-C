#!/usr/bin/env sh
# Run the simulation test with many seeds and report only failing ones.
# Usage: tests/run_seeds.sh [build_dir] [count]
#   e.g. tests/run_seeds.sh "build/INTERCOM[SimTests]" 100
BUILD_DIR=${1:-build}
COUNT=${2:-100}
EXE=""
for c in "$BUILD_DIR/intercom_sim_test" "$BUILD_DIR/intercom_sim_test.exe" "$BUILD_DIR/tests/intercom_sim_test" "$BUILD_DIR/tests/intercom_sim_test.exe"; do
    if [ -x "$c" ]; then EXE="$c"; break; fi
done
if [ -z "$EXE" ]; then
    echo "test executable not found in $BUILD_DIR (build with -DINTERCOM_BUILD_TESTS=ON)"
    exit 2
fi

fails=0
i=1
while [ "$i" -le "$COUNT" ]; do
    seed=$((i * 7919))
    out=$("$EXE" "$seed" 2>&1)
    if ! printf '%s\n' "$out" | grep -q "ALL TESTS PASSED"; then
        fails=$((fails + 1))
        echo "=== seed $seed ==="
        printf '%s\n' "$out" | grep -B4 -E "FAIL|ERROR|runtime error|AddressSanitizer" | head -40
    fi
    i=$((i + 1))
done
echo "failed seeds: $fails/$COUNT"
[ "$fails" -eq 0 ]

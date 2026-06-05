#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage:
  scripts/perf_stat.sh [BIN] [CPU] [OUT_FILE]

Arguments:
  BIN       Benchmark binary to run under perf stat.
            Default: ./parse_throughput

  CPU       CPU core used by taskset.
            Default: 2

  OUT_FILE  Output file intended for perf stat results.
            Default: perf-stat.txt

Examples:
  scripts/perf_stat.sh
  scripts/perf_stat.sh ./parse_throughput 4
  scripts/perf_stat.sh ./parse_throughput 4 perf-stat-run.txt

Help:
  scripts/perf_stat.sh -h
  scripts/perf_stat.sh --help
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
    usage
    exit 0
fi

BIN="${1:-./parse_throughput}"
RUNS="${2:-2}"
OUT_FILE="${3:-perf-stat.txt}"
CPU="${4:-2}"

PROJECT_DIR="${BENCHMARK_PROJECT_DIR:-$(pwd)}"

PRESET="${BENCHMARK_CMAKE_PRESET:-release}"
BUILD_DIR="$PROJECT_DIR/cmake-build-$PRESET"

OUT_DIR="$PROJECT_DIR/benchmarks/saved/$OUT_FILE"

build_benchmark_binary() {
    local bin="$1"
    local target

    target="$(basename "$bin")"

    if [[ -f "$PROJECT_DIR/CMakePresets.json" && "$bin" == "$BUILD_DIR/"* ]]; then
        echo "Configuring preset: $PRESET"
        cmake --preset "$PRESET" -S "$PROJECT_DIR"

        echo "Building benchmark target: $target"
        cmake --build "$BUILD_DIR" --target "$target"
    else
        echo "Building benchmark target from current directory: $target"
        cmake --build . --target "$target"
    fi
}

build_benchmark_binary "$BIN"

CTL_FIFO="/tmp/mdp_perf_stat_ctl.$$"

rm -f "$CTL_FIFO"
mkfifo "$CTL_FIFO"

cleanup() {
    rm -f "$CTL_FIFO"
}
trap cleanup EXIT

echo "Running perf stat with FIFO control..."
echo "Binary: $BIN"
echo "CPU: $CPU"
echo "Control FIFO: $CTL_FIFO"
echo "Output: $OUT_FILE"

PERF_CTL_FIFO="$CTL_FIFO" \
perf stat \
    -d -d -d \
    --control=fifo:"$CTL_FIFO" \
    --delay=-1 \
    -- taskset -c "$CPU" "$BIN" "$RUNS" \
    2>&1 | tee "$OUT_DIR"
    #-M tma_frontend_bound,tma_backend_bound,tma_bad_speculation,tma_retiring \
        #-I 10000 -d -d -d\
        #-e cycles,instructions,branches,branch-misses \
    #-- taskset -c "$CPU"  #\

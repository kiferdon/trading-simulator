#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage:
  scripts/perf_record.sh [BIN] [RUNS]

Arguments:
  BIN              Benchmark binary to record.
                   Default: ./cmake-build-release-perf/parse_throughput

  RUNS             Number of benchmark runs passed to the binary.
                   Default: 1

Environment:
  MDP_CMAKE_PRESET CMake preset used when building the default benchmark binary.
                   Default: release-perf

Examples:
  scripts/perf_record.sh
  scripts/perf_record.sh ./cmake-build-release-perf/parse_throughput 5
  MDP_CMAKE_PRESET=release-perf scripts/perf_record.sh

Help:
  scripts/perf_record.sh -h
  scripts/perf_record.sh --help
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
    usage
    exit 0
fi

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd -- "$SCRIPT_DIR/.." && pwd)"

PRESET="${MDP_CMAKE_PRESET:-release-perf}"
BUILD_DIR="$PROJECT_DIR/cmake-build-release-perf"

if [[ $# -ge 1 ]]; then
    BIN="$1"
else
    BIN="$BUILD_DIR/parse_throughput"
fi

RUNS="${2:-1}"

build_benchmark_binary() {
    local bin="$1"
    local target

    target="$(basename "$bin")"

    if [[ -f "$PROJECT_DIR/CMakePresets.json" && "$bin" == "$BUILD_DIR/"* ]]; then
        echo "Configuring preset: $PRESET"
        cmake --preset "$PRESET" -S "$PROJECT_DIR"

        echo "Building benchmark target with preset $PRESET: $target"
        cmake --build --preset "$PRESET" --target "$target"
    else
        echo "Building benchmark target from current build directory: $target"
        cmake --build . --target "$target"
    fi
}

build_benchmark_binary "$BIN"

CTL_FIFO="/tmp/mdp_perf_ctl"

rm -f "$CTL_FIFO"
mkfifo "$CTL_FIFO"

cleanup() {
    rm -f "$CTL_FIFO"
}
trap cleanup EXIT

echo "Using perf control FIFO: $CTL_FIFO"
echo "Binary: $BIN"
echo "Runs: $RUNS"

PERF_CTL_FIFO="$CTL_FIFO" \
perf record \
    -F 999 \
    --call-graph fp \
    --control=fifo:"$CTL_FIFO" \
    --delay=-1 \
    -- taskset -c 2 "$BIN" "$RUNS"

perf report
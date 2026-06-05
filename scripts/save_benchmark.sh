#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage:
  scripts/save_benchmark.sh [NAME] [RUNS] [BENCHMARK_BIN]

Arguments:
  NAME
      Benchmark result directory name.
      Default: benchmark-YYYYMMDD-HHMMSS

  RUNS
      Number of independent benchmark runs.
      Default: 5

  BENCHMARK_BIN
      Benchmark executable path.
      Default: ./cmake-build-release/benchmark

Environment:
  BENCHMARK_CMAKE_PRESET
      CMake preset used for builds.
      Default: release

Examples:
  scripts/save_benchmark.sh
  scripts/save_benchmark.sh parser-throughput
  scripts/save_benchmark.sh parser-throughput 10
  scripts/save_benchmark.sh parser-throughput 10 ./cmake-build-release/parser_throughput

Help:
  scripts/save_benchmark.sh --help
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
    usage
    exit 0
fi

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="${BENCHMARK_PROJECT_DIR:-$(pwd)}"

PRESET="${BENCHMARK_CMAKE_PRESET:-release}"
BUILD_DIR="$PROJECT_DIR/cmake-build-$PRESET"

NAME="${1:-benchmark-$(date +%Y%m%d-%H%M%S)}"
RUNS="${2:-5}"
BENCHMARK_BIN="$(realpath "${3:-$BUILD_DIR/benchmark}")"

OUT_DIR="$PROJECT_DIR/benchmarks/saved/$NAME"
RUNS_DIR="$OUT_DIR/runs"

validate_positive_integer() {
    local name="$1"
    local value="$2"

    if ! [[ "$value" =~ ^[0-9]+$ ]] || [[ "$value" -lt 1 ]]; then
        echo "$name must be a positive integer, got: $value" >&2
        exit 1
    fi
}

validate_positive_integer "RUNS" "$RUNS"

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

build_benchmark_binary "$BENCHMARK_BIN"

mkdir -p "$RUNS_DIR"

echo "Saving benchmark results to: $OUT_DIR"
echo "Runs: $RUNS"

{
    echo "Date: $(date -Iseconds)"
    echo "Git commit: $(git rev-parse HEAD 2>/dev/null || echo unknown)"
    echo "Benchmark binary: $(realpath "$BENCHMARK_BIN")"
    echo "Runs: $RUNS"
    echo

    echo "Git status:"
    git status --short 2>/dev/null || true
    echo

    echo "Compiler:"
    g++ --version | head -n 1
    echo

    echo "CPU:"
    lscpu | grep -E 'Model name|CPU\(s\)|Thread|Core|Socket|MHz|NUMA' || true

} > "$OUT_DIR/metadata.txt"

extract_metric() {
    local file="$1"
    local metric="$2"

    awk -F= -v metric="$metric" '
        $1 == metric {
            print $2
        }
    ' "$file"
}

median_value() {
    sort -g | awk '
        {
            values[NR] = $1
        }

        END {
            if (NR == 0) {
                exit 1
            }

            if (NR % 2 == 1) {
                print values[(NR + 1) / 2]
            } else {
                print (values[NR / 2] + values[NR / 2 + 1]) / 2
            }
        }
    '
}

write_summary() {
    local output_file="$1"
    shift

    local files=("$@")

    {
        echo "benchmark_name=$NAME"
        echo "runs=${#files[@]}"
        echo

        metrics="$(
            cat "${files[@]}" |
            cut -d= -f1 |
            sort -u
        )"

        for metric in $metrics; do
            values="$(
                for file in "${files[@]}"; do
                    extract_metric "$file" "$metric" || true
                done
            )"

            if [[ -n "$values" ]]; then
                median="$(printf '%s\n' "$values" | median_value)"
                echo "median_$metric=$median"
            fi
        done

        echo
        echo "run_files:"

        for file in "${files[@]}"; do
            echo "$file"
        done

    } > "$output_file"
}

run_files=()

RUN_FILE="$RUNS_DIR/output.txt"

echo "Running benchmark [$RUNS runs]..."

"$BENCHMARK_BIN" "$RUNS" "../market-data/12302019.NASDAQ_ITCH50" "$OUT_DIR/hist.txt" | tee "$RUN_FILE"

run_files+=("$RUN_FILE")

write_summary \
    "$OUT_DIR/summary.txt" \
    "${run_files[@]}"

echo "Done."
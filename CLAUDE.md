# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

This project is a high-performance trading engine simulator. Prioritize performance and correctness.

## Build Commands

```bash
# Build with preset
cmake --preset <debug|release|release-perf>
cmake --build cmake-build-<preset>

# Build specific target
cmake --build cmake-build-release --target <target>
```

**CMake Presets:**

- `debug` - No optimization, no LTO, native arch disabled
- `release` - O3, LTO enabled, march=native
- `release-perf` - O3, frame pointers for perf, march=native

**Available targets:** `parser_throughput`, `parser_latency`, `replay_itch`, `book_benchmark`, `book_microbenchmark`,
`simulator_throughput`, `simulator_latency`, `itch_parser_test`, `order_book_test`

## Tests

Run tests after **any** change to verify correctness:

```bash
cmake --build cmake-build-release --target <test_target>
./cmake-build-release/<test_target>
```

**Existing test targets:**

- `itch_parser_test` - Parsing, big-endian helpers, cursor handling
- `order_book_test` - Order book operations

When adding new tests, ensure they are added to `CMakeLists.txt` and run them after making changes.

## Workflow with Claude (Claude Code)

- **Plan mode** for implementation tasks: Present a plan for approval before implementing. Iterate on plan until
  approved, then implement.
- **Direct conversation** for questions/clarifications: No need to enter plan mode for discussions or clarifications.
- **Run tests** after changes to verify correctness.

**Plans:** When using plan mode, include full implementation details (exact file changes, class layouts, function
signatures, data structures, test approach) so the user can review and correct before implementation begins. Save
approved plans to `<project-root>/.claude/plans/` for future reference.

## Architecture

```
market_data    Parses NASDAQ ITCH50 binary protocol
order_book     Order management (bids/asks)
simulator      Market state and event routing
benchmark      Throughput/latency measurement
```

## Workflow

- **Plan mode** for implementation tasks: `I want to add X...`
- **Direct conversation** for questions/clarifications
- **Run tests** after changes to verify

## Data

Market data file (not in repository): `market-data/12302019.NASDAQ_ITCH50`

## Scripts

- `run_benchmark_pipeline.py` - Orchestrates benchmark runs with perf stat
- `plot.py` - Generate plots from histogram data

## Notes

- Benchmark output paths are relative from `cmake-build-*/` (e.g., `../market-data/`)
- `-march=native` is enabled - builds are not portable
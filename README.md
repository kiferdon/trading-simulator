# HFT Simulator

A Linux C++23 trading engine simulator built around NASDAQ ITCH 5.0 market data, with a focus on low-latency parsing,
order book correctness, and measurable performance work.

This is a systems/performance project: it parses binary market data, routes order events into multiple order book
implementations, runs replay simulation, and records throughput/latency benchmark reports.

## What This Shows

- Binary protocol parsing for NASDAQ ITCH-style fixed-width messages.
- Data-structure tradeoffs for order book design.
- Correctness-first benchmarking: order book behavior is tested before benchmark output is treated as meaningful.
- C++23 code organized with CMake targets, release/perf build presets, and repeatable benchmark entry points.
- Practical performance engineering: hot-path allocation awareness, cached best bid/ask handling, and latency histogram
  reporting.

## Highlights

The project contains four order book implementations:

- `OrderBookV1`: baseline `std::map<Price, std::list<Order>>` implementation.
- `OrderBookV2_PageTable`: two-level price page table for sparse direct indexing.
- `OrderBookV2_Vector`: dense normalized price range with contiguous level storage.
- `OrderBookV2_Hybrid`: sparse hash-map implementation with direct order-location metadata.

The V2 work targets the common pain points in a naive book:

- Avoid full-book scans for remove/cancel/replace.
- Store enough metadata to preserve side and locate levels directly.
- Keep cached best bid/ask correct when top levels empty.
- Reduce pointer-heavy access patterns where the price range makes that practical.

## Correctness Status

`tests/order_book_test.cpp` runs the same snapshot-based checks against V1 and all V2 implementations.

Covered behavior includes:

- Add, delete, cancel, replace, modify, and ITCH handler routing.
- Aggregate volume at price levels.
- Best bid/ask recomputation after full removal.
- Same-page price separation for the page-table implementation.
- Side preservation during replace.
- Missing-order operations as no-ops.

Current expected result:

```text
OrderBookV1
OrderBookV2_PageTable
OrderBookV2_Vector
OrderBookV2_Hybrid
288 checks, 0 failures
```

## Benchmark Results

Saved benchmark reports are committed under `benchmarks/saved`. The numbers below are from Linux release builds on an
Intel Core i9-14900HX with GCC 15.2.0. They are not presented as universal performance claims; they are the saved results
for this machine and dataset.

Parser throughput on the full ITCH file:

| Benchmark | Messages | Throughput | Data Rate |
| --- | ---: | ---: | ---: |
| ITCH parser | 263.2M parsed | 148.1M msg/s | 4.32 GiB/s |

ITCH-backed order book replay, filtered to QQQ order events:

| Implementation | Throughput | p50 | p99 | p999 |
| --- | ---: | ---: | ---: | ---: |
| `OrderBookV1` | 1.05M msg/s | 143 cycles | 450 cycles | 786 cycles |
| `OrderBookV2_Hybrid` | 0.96M msg/s | 120 cycles | 17,010 cycles | 20,385 cycles |
| `OrderBookV2_PageTable` | 0.87M msg/s | 132 cycles | 18,711 cycles | 35,176 cycles |
| `OrderBookV2_Vector` | 1.10M msg/s | 65 cycles | 153 cycles | 302 cycles |

The most successful result is `OrderBookV2_Vector`: about 1.05x V1 throughput on this replay benchmark, with much lower
tail latency in the saved run.

Synthetic operation microbenchmark, median throughput:

| Implementation | Add | Delete | Modify |
| --- | ---: | ---: | ---: |
| `OrderBookV1` | 6.0M ops/s | 5.8M ops/s | 4.4M ops/s |
| `OrderBookV2_Hybrid` | 15.5M ops/s | 12.2M ops/s | 10.0M ops/s |
| `OrderBookV2_PageTable` | 17.7M ops/s | 20.9M ops/s | 2.0M ops/s |
| `OrderBookV2_Vector` | 22.2M ops/s | 35.8M ops/s | 16.5M ops/s |

These numbers are useful mainly as directional evidence for the data-structure work. The correctness suite above is the
first gate; benchmark comparisons come after that.

## Benchmarking

Benchmark outputs are intentionally saved in the repository under:

```text
benchmarks/saved
```

There are separate targets for throughput and latency:

- Parser: `parser_throughput`, `parser_latency`
- ITCH-backed order book benchmark: `book_throughput`, `book_latency`
- Synthetic order book microbenchmark: `book_microbenchmark_throughput`, `book_microbenchmark_latency`
- Simulator: `simulator_throughput`, `simulator_latency`

Order book benchmark implementation selection is explicit in code:

- `include/benchmark/order_book/order_book_benchmark.hpp`: `OB_impl`
- `include/benchmark/order_book/order_book_microbenchmark.hpp`: `Book_impl`

## Architecture

```text
market_data   -> NASDAQ ITCH parser and dispatch
order_book    -> V1/V2 book implementations and ITCH order-event handling
simulator     -> market state, event routing, strategy hooks, replay loop
benchmark     -> throughput and latency measurement utilities
apps          -> replay and benchmark executables
tests         -> parser and order book correctness tests
```

## Platform

This project is intended for Linux.

The current build uses Unix Makefiles, `-march=native` in release-oriented presets, and perf-friendly build options for
Linux profiling workflows.

## Build And Run

Requirements:

- Linux
- CMake 3.20+
- C++23 compiler

Configure a release build:

```bash
cmake --preset release
```

Run the order book correctness suite:

```bash
cmake --build cmake-build-release --target order_book_test
./cmake-build-release/order_book_test
```

Run parser tests:

```bash
cmake --build cmake-build-release --target itch_parser_test
./cmake-build-release/itch_parser_test
```

Build benchmark targets:

```bash
cmake --build cmake-build-release --target book_throughput
cmake --build cmake-build-release --target book_latency
cmake --build cmake-build-release --target book_microbenchmark_throughput
cmake --build cmake-build-release --target book_microbenchmark_latency
```

## Market Data

The replay and ITCH-backed benchmarks expect local market data at:

```text
market-data/12302019.NASDAQ_ITCH50
```

The data file is not committed.

Run replay:

```bash
cmake --build cmake-build-release --target replay_itch
./cmake-build-release/replay_itch market-data/12302019.NASDAQ_ITCH50
```

## Notes For Reviewers

The most interesting files to inspect first are:

- `include/order_book/order_book_v2_page_table.hpp`
- `src/order_book/order_book_v2_page_table.cpp`
- `include/order_book/order_book_v2_vector.hpp`
- `src/order_book/order_book_v2_vector.cpp`
- `tests/order_book_test.cpp`
- `include/benchmark/order_book/order_book_microbenchmark.hpp`

The project is intentionally not a trading strategy showcase. It is a C++ systems project focused on correctness,
market-data plumbing, order book internals, and performance measurement.

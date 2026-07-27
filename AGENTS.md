# AGENTS.md

This file provides durable project context for coding agents working in this repository.
Do not treat benchmark results as meaningful unless correctness tests for the same order book behavior pass first.

## Project

This is a C++23 high-performance trading engine simulator focused on NASDAQ ITCH50 market data.
Prioritize performance and correctness, especially in parser hot paths, order book operations, simulator event routing,
and benchmark measurement.

## Build

Use CMake presets:

```bash
cmake --preset debug
cmake --preset release
cmake --preset release-perf
cmake --build cmake-build-release --target <target>
```

Presets:

- `debug`: no optimization, no LTO, native arch disabled.
- `release`: `-O3`, LTO enabled, `-march=native`.
- `release-perf`: `-O3`, frame pointers for perf, `-march=native`.

Common targets:

- `itch_parser_test`
- `order_book_test`
- `parser_throughput`
- `parser_latency`
- `book_throughput`
- `book_latency`
- `book_microbenchmark_throughput`
- `book_microbenchmark_latency`
- `simulator_throughput`
- `simulator_latency`
- `replay_itch`

## Test Rules

After code changes, build and run the relevant tests:

```bash
cmake --build cmake-build-release --target order_book_test
./cmake-build-release/order_book_test
```

For parser changes, also run `itch_parser_test`. For benchmark or simulator changes, build the touched benchmark target.

`order_book_test` currently runs assertion-style snapshot checks against V1 and all three V2 implementations. Coverage
includes aggregate volumes, partial/full cancels, deletes, side preservation on replace, same-page price separation,
best bid/ask invalidation, ITCH order handlers, and missing-order no-ops. Extend these tests before relying on benchmark
comparisons for any new order book behavior.

## Architecture

- `market_data`: NASDAQ ITCH50 parsing and message dispatch.
- `order_book`: bid/ask order management and ITCH order event handlers.
- `simulator`: market state and event routing.
- `benchmark`: throughput and latency measurement tools.

Market data is expected locally at:

```text
market-data/12302019.NASDAQ_ITCH50
```

## Order Book State

The V1 order book is the baseline:

- `std::map<Price, std::list<Order>>` for bid and ask levels.
- `std::unordered_map<OrderId, OrderLocation>` for order lookup.
- Simple but cache-unfriendly due to tree traversal, list node allocation, and pointer chasing.

The V2 goals, based on `ORDER_BOOK_ANALYSIS.md` and `ORDER_BOOK_V2_REPORT.md`, are:

- Avoid per-order heap allocation in hot paths.
- Use cache-friendly price-level storage.
- Keep O(1) order lookup by ID.
- Store enough order metadata to remove, cancel, modify, and replace without scanning.
- Cache best bid/ask, but update or invalidate them correctly when best levels empty.

Current V2 implementation status:

- `OrderBookV2_Vector`: dense vector indexed by normalized configured price range. It stores side, level index, order
  index, and volume in `orders_by_id_`, uses swap-pop removal, tracks active levels with bitsets, and refreshes cached
  best bid/ask when the best level empties. It avoids hot-path resize by raw price, but only accepts prices inside its
  configured range.
- `OrderBookV2_PageTable`: two-level page table using `price >> 16` and `price & 0xFFFF`. Pages are allocated
  on-demand, order lookup stores pointers to intrusive nodes, same-page prices map to distinct levels, and cached best
  bid/ask are recomputed when the best level empties.
- `OrderBookV2_Hybrid`: sparse hash-map implementation with O(1)-average lookup and side/iterator metadata for direct
  removal and replacement. It keeps cached best bid/ask correct when levels empty, but still uses `std::list` and does
  not meet the no-per-order-heap-allocation goal.

## Known Current Risks

- The old V2 correctness risks above are fixed in the current source: the stray Vector TODO is gone, Vector no longer
  resizes to raw prices, Vector lookup includes side/location metadata, PageTable uses `page_offset`, and all V2
  implementations refresh cached best bid/ask when the best level empties.
- `OrderBookV2_Vector` has a default `VectorPriceRange{0, 1'000'000, 1}`. Real ITCH prices outside that range are
  rejected, and debug builds assert on out-of-range or unaligned prices. Configure the range explicitly before using it
  on broad ITCH input.
- `OrderBookV2_Vector` uses swap-pop removal, so FIFO order within a price level is not preserved.
- `OrderBookV2_PageTable` allocates each touched page as `65536 * sizeof(PriceLevel)` entries per side. It avoids dense
  full-range allocation but can still have coarse page-level memory growth.
- `OrderBookV2_Hybrid` is intentionally sparse and simple, but `std::list` means per-order heap allocation remains.
- Order book benchmarks select the implementation through `OB_impl` in
  `include/benchmark/order_book/order_book_benchmark.hpp`; microbenchmarks select through `Book_impl` in
  `include/benchmark/order_book/order_book_microbenchmark.hpp`. Both currently default to `OrderBookV1`.

## Performance Guidance

- Measure release or release-perf builds, not debug builds.
- Prefer data-oriented changes with clear memory layout and hot-path effects.
- Avoid introducing allocations, virtual dispatch, logging, streams, exceptions, or branch-heavy generic code into hot
  loops unless there is a measured reason.
- Keep benchmark setup outside measured regions.
- Compare correctness first, then throughput, then latency percentiles.
- When changing data structures, document expected operation costs and memory tradeoffs in the relevant header.

## Workflow

- For implementation work, make a concrete plan before broad refactors.
- Keep edits narrowly scoped to the requested behavior.
- Do not edit `CLAUDE.md` unless explicitly asked.
- Do not revert unrelated worktree changes.
- Use existing project style and CMake structure.

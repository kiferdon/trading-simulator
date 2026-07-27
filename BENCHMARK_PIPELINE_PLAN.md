# OrderBook Benchmark Pipeline Plan

## Status

1. **Fix Compilation Errors** - ✅ DONE
2. **Common Benchmark Infrastructure** - ✅ DONE
    - `compute_latency_stats<>()` in `histogram.hpp`
    - `BenchmarkOutput` class in `output.hpp`
3. **Split order book benchmark entry points** - ✅ DONE
4. **Switch Implementation Type** - ⏳ READY TO START

## Current Benchmark Infrastructure

### Core Framework

| File | Contents |
|------|----------|
| `benchmark/common/benchmark.hpp` | `run_throughput`, `run_latency`, `BenchmarkSessionConfig` |
| `benchmark/common/histogram.hpp` | `compute_latency_stats<>()`, `save_histogram<>()`, `histogram_percentile<>()` |
| `benchmark/common/output.hpp` | `BenchmarkOutput` class, `derive_output_dir()` |
| `benchmark/common/json_utilities.hpp` | `write_json_result<>()` |

### Command-Line Interface

**Format:** `[runs] [market_file] [json_path]`

```bash
./book_microbenchmark_throughput 1 /tmp/market.dat /tmp/results/output.jsonl
```

**Produces:**
- `output.jsonl` - one compact JSON per line
- `{name}_latency.hist` - histograms in same directory as JSONL

## Design: Compile-Time Implementation Switching

Change the type alias at the top of the source file:

```cpp
// book_microbenchmark_* or book_* benchmark target
using BookType = ob::OrderBookV1;  // change this line

// Other options:
// using BookType = ob::OrderBookV2_PageTable;
// using BookType = ob::OrderBookV2_Vector;
// using BookType = ob::OrderBookV2_Hybrid;
```

## Implementation Plan

### Step 1: Template Microbenchmark Functions

Template `src/benchmark/order_book/order_book_microbenchmark.cpp` functions to accept any OrderBook type.

**`include/benchmark/order_book/order_book_microbenchmark.hpp`:**
```cpp
template<typename OrderBookT>
OrderBookOperationResult run_add_orders(
    OrderBookT &order_book,
    const OrderBookSyntheticConfig &config = OrderBookSyntheticConfig{}
);

template<typename OrderBookT>
OrderBookOperationLatencyResult run_add_orders_latency(
    OrderBookT &order_book,
    const OrderBookSyntheticConfig &config = OrderBookSyntheticConfig{}
);
// ... 8 more functions
```

**`src/benchmark/order_book/order_book_microbenchmark.cpp`:**
- Template all 10 function definitions
- Use existing `compute_latency_stats(result.histogram)` call

### Step 2: Update Benchmark Runners

**Order book benchmark targets:**

- `book_throughput`: full ITCH replay throughput only
- `book_latency`: full ITCH replay latency only
- `book_microbenchmark_throughput`: synthetic operation throughput only
- `book_microbenchmark_latency`: synthetic operation latency only

This keeps `scripts/run_benchmark_pipeline.py` simple: each target still accepts one run count, but throughput and
latency can now be scheduled independently.

**Microbenchmark implementation switching:**
```cpp
#include "order_book/order_book_v1.hpp"
#include "order_book/order_book_v2_page_table.hpp"
#include "order_book/order_book_v2_vector.hpp"
#include "order_book/order_book_v2_hybrid.hpp"

// ========== CHANGE THIS LINE TO SWITCH IMPLEMENTATIONS ==========
using BookType = ob::OrderBookV1;
// ===============================================================

// Then replace all ob::OrderBookV1 with BookType throughout
BookType book_for_add;
BookType book_for_add_latency;
// etc.
```

## File Changes Summary

| File | Change |
|------|--------|
| `include/benchmark/order_book/order_book_microbenchmark.hpp` | Template all 10 function declarations |
| `src/benchmark/order_book/order_book_microbenchmark.cpp` | Template all 10 function definitions |
| `apps/benchmarks/book_throughput.cpp` | Full ITCH replay throughput entry point |
| `apps/benchmarks/book_latency.cpp` | Full ITCH replay latency entry point |
| `apps/benchmarks/book_microbenchmark_throughput.cpp` | Synthetic operation throughput entry point |
| `apps/benchmarks/book_microbenchmark_latency.cpp` | Synthetic operation latency entry point |

## Workflow

```bash
# 1. Edit source - change the type alias line
#    benchmark header or app source, depending on target
using BookType = ob::OrderBookV2_PageTable;

# 2. Build
cmake --build cmake-build-release --target book_microbenchmark_throughput

# 3. Run
./cmake-build-release/book_microbenchmark_throughput 1 /tmp/market.dat benchmarks/saved/V2_PageTable/output.jsonl

# 4. Repeat for each implementation needed
```

## Expected Output Structure

```
benchmarks/saved/{run_name}/book_microbenchmark_latency/{timestamp}/
├── output.jsonl          # 8 records
├── add_latency.hist
├── cancel_latency.hist
├── delete_latency.hist
└── modify_latency.hist
```

## Build & Test

```bash
# Build
cmake --build cmake-build-release --target book_microbenchmark_throughput book_microbenchmark_latency

# Run
./cmake-build-release/book_microbenchmark_throughput 1 /tmp/market.dat benchmarks/saved/test/output.jsonl

# Verify
python3 -c "import json; [json.loads(line) for line in open('benchmarks/saved/test/output.jsonl')]"
wc -l benchmarks/saved/test/add_latency.hist
```

## Notes

- Pipeline command format unchanged
- Each implementation requires a source edit + rebuild
- `BenchmarkOutput::hist_path(name)` derives histogram paths from JSONL location
- All 4 histograms created in same directory as output.jsonl

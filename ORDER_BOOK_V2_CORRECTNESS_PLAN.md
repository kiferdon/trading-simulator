# OrderBook V2 Correctness Plan

## Goal

Make the order book implementations correct enough to benchmark. Benchmark results are not meaningful until
`order_book_test` passes for the behavior being compared.

The current single correctness gate is:

```bash
cmake --build cmake-build-release --target order_book_test
./cmake-build-release/order_book_test
```

Latest known result from `test-reports/order_book_test_report.txt`:

- `288` checks
- `0` failures
- `OrderBookV1`, `OrderBookV2_Vector`, `OrderBookV2_PageTable`, and `OrderBookV2_Hybrid` pass the unified correctness
  suite

## Current Test Status

The separate `order_book_v2_test` target was removed because it duplicated the broader suite. All V2 regression cases now
belong in `tests/order_book_test.cpp`, which runs:

- `OrderBookV1`
- `OrderBookV2_PageTable`
- `OrderBookV2_Vector`
- `OrderBookV2_Hybrid`

The test suite now covers:

- add bid/ask gives correct best bid/ask and quantities
- multiple orders at one level aggregate volume
- snapshot quantities
- partial cancel reduces volume
- full cancel removes best bid and recomputes next best bid
- full cancel removes best ask and recomputes next best ask
- over-cancel removes the order
- delete preserves the opposite side
- replace preserves side
- replace preserves ask side
- modify same price changes volume
- modify new price moves level
- same-page prices stay distinct
- missing order operations are no-ops
- ITCH add/cancel/replace handlers
- ITCH delete and executed-with-price handlers

## Implementation Order

### 1. Fix V1 Baseline Failure - Done

File:

- `include/order_book/order_book_v1.hpp`

Known failure:

- `modify_same_price_changes_volume`: best bid quantity remains `100`, expected `150`

Fix:

- In `modify_order`, when `new_price == location.price`, update `location.iterator->volume`.
- When `new_price != location.price`, erase from the old level, clean up the old empty level, insert into the new level,
  and update both `location.price` and `location.iterator`.

Reason:

- V1 should be the correctness baseline before judging V2 behavior.

### 2. Rebuild `OrderBookV2_Vector` First - Done

Files:

- `include/order_book/order_book_v2_vector.hpp`
- `src/order_book/order_book_v2_vector.cpp`

Vector is the main target. The current implementation is not a reliable base because it:

- resizes vectors directly to raw price values
- stores only `Order` by ID, without side or level location
- scans both sides on removal
- cannot preserve side on replace
- does not update best bid/ask after best levels empty
- returns best prices without best quantities

Required data model:

```cpp
struct VectorPriceRange {
  dt::Price min_price;
  dt::Price max_price;
  dt::Price tick_size;
};

struct VectorOrderLocation {
  dt::Price price;
  dt::Quantity volume;
  Side side;
  uint32_t level_index;
  uint32_t order_index;
};
```

`OrderBookV2_Vector` should store:

```cpp
VectorPriceRange range_;
std::vector<VectorPriceLevel> bid_levels_;
std::vector<VectorPriceLevel> ask_levels_;
std::vector<uint64_t> active_bid_words_;
std::vector<uint64_t> active_ask_words_;
std::unordered_map<dt::OrderId, VectorOrderLocation> orders_by_id_;
uint32_t best_bid_index_;
uint32_t best_ask_index_;
```

Use fixed normalized indexing:

```cpp
index = (price - min_price) / tick_size;
```

Validation:

- price is inside `[min_price, max_price]`
- `(price - min_price) % tick_size == 0`

Default range:

- keep tests and current benchmarks working with `[0, 1'000'000]`, `tick_size = 1`

Out-of-range behavior:

- use `assert` in debug builds
- return without mutation in release builds, because the public API has no error channel

Level removal:

- use swap-pop on `VectorPriceLevel::orders`
- update the moved order's `order_index`
- accept that FIFO is not preserved in this aggregate book implementation

Best bid/ask:

- maintain active bitmaps
- set bit on empty -> non-empty level transition
- clear bit on non-empty -> empty level transition
- update cached best on add
- rescan bitmap only when the current best level becomes empty

Operations to implement correctly:

- `add_order`: ignore duplicate IDs
- `remove_order`: O(1), update volume, bitmap, and best cache
- `subtract_order`: reduce volume or fully remove
- `modify_order`: update same-price volume or move to new price with same side
- `replace_order`: preserve original side, remove old ID, add new ID
- `get_snapshot`: return best prices and best level quantities
- `print_top_levels`: bids high-to-low, asks low-to-high

### 3. Fix `OrderBookV2_PageTable` - Done

Files:

- `include/order_book/order_book_v2_page_table.hpp`
- `src/order_book/order_book_v2_page_table.cpp`

Known failure classes:

- snapshot quantities are zero
- prices in the same page collapse together
- best bid/ask caches are stale after removal

Required fixes:

- allocate one array of `PriceLevel` per page, not one level per page
- use the computed page offset:

```cpp
PriceLevel* page = new PriceLevel[PAGE_SIZE];
return page[page_offset];
```

- use `delete[]` in destruction
- store order side and precise level location in `orders_by_id_`
- update best bid/ask when current best levels empty
- return best level aggregate quantities in `get_snapshot`

### 4. Fix `OrderBookV2_Hybrid` - Done

Files:

- `include/order_book/order_book_v2_hybrid.hpp`
- `src/order_book/order_book_v2_hybrid.cpp`

Known failure classes:

- snapshot quantities are zero
- removal does not recompute best prices
- remove/modify/replace behavior lacks enough order location metadata

Required location model:

```cpp
struct HybridOrderLocation {
  Side side;
  dt::Price price;
  dt::Quantity volume;
  std::list<HybridOrderNode>::iterator iterator;
};
```

Required fixes:

- remove by iterator instead of scanning a whole price level
- update aggregate level volume on add, subtract, remove, modify, and replace
- preserve side on replace
- update or invalidate best bid/ask when a best level becomes empty
- return best level aggregate quantities in `get_snapshot`

### 5. Keep Tests Unified

Do not reintroduce `order_book_v2_test.cpp` unless there is a genuinely separate test domain. V2-specific cases should
be added to `tests/order_book_test.cpp` with clear test names.

After every implementation change:

```bash
cmake --build cmake-build-release --target order_book_test
./cmake-build-release/order_book_test
```

Update:

- `test-reports/order_book_test_report.txt`

### 6. Benchmark Only After Correctness - Ready

Do not compare V2 benchmarks until the relevant correctness tests pass. Once correctness passes, use release or
release-perf builds and compare:

- `OrderBookV1`
- `OrderBookV2_Vector`
- `OrderBookV2_PageTable`
- `OrderBookV2_Hybrid`

Benchmark implementation switching still belongs in the benchmark pipeline work, but it is blocked by this plan.

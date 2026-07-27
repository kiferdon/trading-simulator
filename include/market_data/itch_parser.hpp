//
// Created by silay on 4/30/26.
//

#ifndef MARKET_DATA_PARSER_ITCH_PARSER_HPP
#define MARKET_DATA_PARSER_ITCH_PARSER_HPP
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iosfwd>
#include <optional>
#include <unordered_map>
#include <vector>

#include "common/common.hpp"

namespace md {
  constexpr size_t SIZE_A = 36;
  constexpr size_t SIZE_F = 40;
  constexpr size_t SIZE_E = 31;
  constexpr size_t SIZE_C = 36;
  constexpr size_t SIZE_X = 23;
  constexpr size_t SIZE_D = 19;
  constexpr size_t SIZE_U = 35;

  constexpr size_t SIZE_INVALID = 0;
  constexpr size_t FRAME_OFFSET_SIZE = 1;
  constexpr size_t FRAME_LENGTH_SIZE = 1;
  constexpr size_t FRAME_HEADER_SIZE = FRAME_OFFSET_SIZE + FRAME_LENGTH_SIZE;

  constexpr auto build_message_sizes() {
    std::array<size_t, 256> arr{};
    arr['A'] = SIZE_A;
    arr['F'] = SIZE_F;
    arr['E'] = SIZE_E;
    arr['C'] = SIZE_C;
    arr['X'] = SIZE_X;
    arr['D'] = SIZE_D;
    arr['U'] = SIZE_U;

    return arr;
  }

  alignas(64) static constexpr std::array<size_t, 256> MESSAGE_SIZES =
      build_message_sizes();

  static inline uint16_t be16(const char *p) {
    uint16_t v;
    std::memcpy(&v, p, 2);
    return __builtin_bswap16(v);
  }

  static inline uint32_t be32(const char *p) {
    uint32_t v;
    std::memcpy(&v, p, 4);
    return __builtin_bswap32(v);
  }

  static inline uint64_t be48(const char *p) {
    return (static_cast<uint64_t>(static_cast<unsigned char>(p[0])) << 40) |
           (static_cast<uint64_t>(static_cast<unsigned char>(p[1])) << 32) |
           (static_cast<uint64_t>(static_cast<unsigned char>(p[2])) << 24) |
           (static_cast<uint64_t>(static_cast<unsigned char>(p[3])) << 16) |
           (static_cast<uint64_t>(static_cast<unsigned char>(p[4])) << 8) |
           static_cast<uint64_t>(static_cast<unsigned char>(p[5]));
  }

  static inline uint64_t be64(const char *p) {
    uint64_t v;
    std::memcpy(&v, p, 8);
    return __builtin_bswap64(v);
  }

  static inline uint64_t identifier64(const char *p) {
    uint64_t v;
    std::memcpy(&v, p, 8);
    return v;
  }

  static inline uint32_t identifier32(const char *p) {
    uint32_t v;
    std::memcpy(&v, p, 4);
    return v;
  }

  struct Cursor {
    const char *ptr;
    const char *start;
    const char *end;
    uint32_t currentOffset = 0;

    Cursor(const char *data, size_t size)
      : ptr(data), start(data), end(data + size) {
    }

    bool is_in_bounds(size_t offset) const { return ptr + offset <= end; }

    template<typename T>
    T read() {
      T val;
      std::memcpy(&val, ptr, sizeof(T));
      ptr += sizeof(T);
      currentOffset += sizeof(T);
      return val;
    }

    const char *read_bytes(size_t n) {
      const char *p = ptr;
      ptr += n;
      currentOffset += n;
      return p;
    }

    const char *read_bytes_const(size_t n) const {
      const char *p = ptr;
      return p;
    }

    void reset() { ptr = start; }
  };

  // TOP 16 stocks by frequency
  constexpr dt::StockLocate STOCK_LOCATE_QQQ = 6556;
  constexpr dt::StockLocate STOCK_LOCATE_SPY = 7451;
  constexpr dt::StockLocate STOCK_LOCATE_IWM = 4315;
  constexpr dt::StockLocate STOCK_LOCATE_TQQQ = 7931;
  constexpr dt::StockLocate STOCK_LOCATE_AAPL = 13;
  constexpr dt::StockLocate STOCK_LOCATE_AMD = 344;
  constexpr dt::StockLocate STOCK_LOCATE_MSFT = 5291;
  constexpr dt::StockLocate STOCK_LOCATE_URTY = 8197;
  constexpr dt::StockLocate STOCK_LOCATE_TNA = 7879;
  constexpr dt::StockLocate STOCK_LOCATE_XLK = 8748;
  constexpr dt::StockLocate STOCK_LOCATE_DIA = 2014;
  constexpr dt::StockLocate STOCK_LOCATE_XLY = 8756;
  constexpr dt::StockLocate STOCK_LOCATE_NUGT = 5686;
  constexpr dt::StockLocate STOCK_LOCATE_TVIX = 8028;
  constexpr dt::StockLocate STOCK_LOCATE_MU = 5332;
  constexpr dt::StockLocate STOCK_LOCATE_SLB = 7233;

  template<typename Identifier>
  std::string get_identifier_string(Identifier id) {
    constexpr std::size_t identifier_length = sizeof(Identifier);

    char identifier_bytes[identifier_length];
    std::memcpy(identifier_bytes, &id, identifier_length);

    std::string result{identifier_bytes, identifier_length};
    std::erase(result, ' ');

    return result;
  }

  struct ITCHHandler {
    // A: Add Order - No MPID Attribution
    virtual inline void on_add_order(uint16_t locate, uint16_t tracking,
                                     uint64_t timestamp, uint64_t order_ref,
                                     char side, uint32_t shares, uint64_t stock,
                                     uint32_t price) {
      checksum += timestamp;
      // stock_locate_to_id[locate] = stock;
      // stock_events_count[locate]++;
    }

    // F: Add Order with MPID Attribution
    virtual inline void on_add_order_with_mpid(uint16_t locate, uint16_t tracking,
                                               uint64_t timestamp,
                                               uint64_t order_ref, char side,
                                               uint32_t shares, uint64_t stock,
                                               uint32_t price, uint32_t mpid) {
      // Note: MPID is usually 4 alpha chars, often handled as uint32_t
      checksum += timestamp;
      // stock_locate_to_id[locate] = stock;
      // stock_events_count[locate]++;
    }

    // E: Order Executed
    virtual inline void on_order_executed(uint16_t locate, uint16_t tracking,
                                          uint64_t timestamp, uint64_t order_ref,
                                          uint32_t executed_shares,
                                          uint64_t match_id) {
      checksum += timestamp;
      // stock_events_count[locate]++;
    }

    // C: Order Executed With Price
    virtual inline void
    on_order_executed_with_price(uint16_t locate, uint16_t tracking,
                                 uint64_t timestamp, uint64_t order_ref,
                                 uint32_t executed_shares, uint64_t match_id,
                                 char printable, uint32_t price) {
      checksum += timestamp;
      // stock_events_count[locate]++;
    }

    // X: Order Cancel
    virtual inline void on_order_cancel(uint16_t locate, uint16_t tracking,
                                        uint64_t timestamp, uint64_t order_ref,
                                        uint32_t canceled_shares) {
      checksum += timestamp;
      // stock_events_count[locate]++;
    }

    // D: Order Delete
    virtual inline void on_order_delete(uint16_t locate, uint16_t tracking,
                                        uint64_t timestamp, uint64_t order_ref) {
      checksum += timestamp;
      // stock_events_count[locate]++;
    }

    // U: Order Replace
    virtual inline void on_order_replace(uint16_t locate, uint16_t tracking,
                                         uint64_t timestamp,
                                         uint64_t original_order_ref,
                                         uint64_t new_order_ref, uint32_t shares,
                                         uint32_t price) {
      checksum += timestamp;
      // stock_events_count[locate]++;
    }

    uint64_t checksum = 0;
    std::unordered_map<uint16_t, dt::StockId> stock_locate_to_id;
    std::unordered_map<uint16_t, uint32_t> stock_events_count;
  };

  struct MessageHeader {
    char message_type;
    uint16_t stock_locate;
    uint16_t tracking_number;
    dt::Timestamp timestamp;
  };

  struct AddOrderMessage {
    MessageHeader header;

    dt::OrderId order_reference_number;
    char buy_sell_indicator;
    dt::Quantity shares;
    dt::StockId stock;
    dt::Price price;
  };

  struct AddOrderWithMPIDMessage {
    MessageHeader header;

    dt::OrderId order_reference_number;
    char buy_sell_indicator;
    dt::Quantity shares;
    dt::StockId stock;
    dt::Price price;
    dt::Attribution attribution;
  };

  struct OrderExecutedMessage {
    MessageHeader header;

    dt::OrderId order_reference_number;
    dt::Quantity executed_shares;
    uint64_t match_number;
  };

  struct OrderExecutedWithPriceMessage {
    MessageHeader header;

    dt::OrderId order_reference_number;
    dt::Quantity executed_shares;
    uint64_t match_number;
    bool printable;
    dt::Price execution_price;
  };

  struct OrderCancelMessage {
    MessageHeader header;

    dt::OrderId order_reference_number;
    dt::Quantity canceled_shares;
  };

  struct OrderDeleteMessage {
    MessageHeader header;

    dt::OrderId order_reference_number;
  };

  struct OrderReplaceMessage {
    MessageHeader header;

    dt::OrderId original_order_reference_number;
    dt::OrderId new_order_reference_number;
    dt::Quantity shares;
    dt::Price price;
  };

  inline MessageHeader parse_message_header(const char *msg) {
    MessageHeader header;

    header.message_type = msg[0];
    const char *p = msg + 1;

    header.stock_locate = be16(p);
    p += 2;

    header.tracking_number = be16(p);
    p += 2;

    header.timestamp = be48(p);

    return header;
  }

  inline AddOrderMessage parse_add_order_message(const char *msg) {
    AddOrderMessage message;
    message.header = parse_message_header(msg);

    const char *p = msg + 11;

    message.order_reference_number = be64(p);
    p += 8;

    message.buy_sell_indicator = *p;
    p += 1;

    message.shares = be32(p);
    p += 4;

    message.stock = identifier64(p);
    p += 8;

    message.price = be32(p);

    return message;
  }

  inline AddOrderWithMPIDMessage
  parse_add_order_with_mpid_message(const char *msg) {
    AddOrderWithMPIDMessage message;
    message.header = parse_message_header(msg);

    const char *p = msg + 11;

    message.order_reference_number = be64(p);
    p += 8;

    message.buy_sell_indicator = *p;
    p += 1;

    message.shares = be32(p);
    p += 4;

    message.stock = identifier64(p);
    p += 8;

    message.price = be32(p);
    p += 4;

    message.attribution = identifier32(p);

    return message;
  }

  inline OrderExecutedMessage parse_order_executed_message(const char *msg) {
    OrderExecutedMessage message;
    message.header = parse_message_header(msg);

    const char *p = msg + 11;

    message.order_reference_number = be64(p);
    p += 8;

    message.executed_shares = be32(p);
    p += 4;

    message.match_number = be64(p);

    return message;
  }

  inline OrderExecutedWithPriceMessage
  parse_order_executed_with_price_message(const char *msg) {
    OrderExecutedWithPriceMessage message;
    message.header = parse_message_header(msg);

    const char *p = msg + 11;

    message.order_reference_number = be64(p);
    p += 8;

    message.executed_shares = be32(p);
    p += 4;

    message.match_number = be64(p);
    p += 8;

    message.printable = *p == 'Y';
    p += 1;

    message.execution_price = be32(p);

    return message;
  }

  inline OrderCancelMessage parse_order_cancel_message(const char *msg) {
    OrderCancelMessage message;
    message.header = parse_message_header(msg);

    const char *p = msg + 11;

    message.order_reference_number = be64(p);
    p += 8;

    message.canceled_shares = be32(p);

    return message;
  }

  inline OrderDeleteMessage parse_order_delete_message(const char *msg) {
    OrderDeleteMessage message;
    message.header = parse_message_header(msg);

    const char *p = msg + 11;

    message.order_reference_number = be64(p);

    return message;
  }

  inline OrderReplaceMessage parse_order_replace_message(const char *msg) {
    OrderReplaceMessage message;
    message.header = parse_message_header(msg);

    const char *p = msg + 11;

    message.original_order_reference_number = be64(p);
    p += 8;

    message.new_order_reference_number = be64(p);
    p += 8;

    message.shares = be32(p);
    p += 4;

    message.price = be32(p);

    return message;
  }

  enum class ParseError : uint8_t {
    None,
    InvalidSize,
    TruncatedMessage,
    UnsupportedMessageType
  };

  struct ParseResult {
    size_t messages = 0;
    size_t skipped = 0;
    size_t consumed_bytes = 0;
    std::unordered_map<char, uint32_t> message_types;
    bool needs_more_data = false;
  };

  struct ParseOneResult {
    size_t consumed_bytes = 0;
    bool parsed = false;
    bool skipped = false;
    bool needs_more_data = false;
    char message_type = '\0';
  };

  class ITCHParser {
  public:
    ITCHParser() = default;

    ITCHParser(std::initializer_list<dt::StockLocate> tracked);

    template<typename Handler>
    ParseOneResult parse_one(Cursor &cursor, Handler &&handler);

    template<typename Handler>
    ParseResult parse(const char *data, size_t size, Handler &&handler);

    void reset();

    size_t pending_bytes() const;

    static constexpr std::size_t MaxTrackedStocks = 16;

    bool is_tracked(dt::StockLocate stock_locate) const;

    void set_track_all(bool value) { track_all = value; }

    bool add_tracked_stock(dt::StockLocate stock_locate);

    void remove_tracked_stock(dt::StockLocate stock_locate);

  private:
    template<typename Handler>
    ParseOneResult parse_payload(const char *msg, size_t message_size,
                                 Handler &&handler);

    std::vector<char> pending_message;

    bool track_all = true;
    std::array<uint16_t, MaxTrackedStocks> tracked_stocks{};
    std::size_t tracked_count = 0;
  };

  inline ITCHParser::ITCHParser(std::initializer_list<dt::StockLocate> tracked) {
    track_all = false;

    for (dt::StockLocate stock: tracked) {
      add_tracked_stock(stock);
    }
  }

  inline void ITCHParser::reset() { pending_message.clear(); }

  inline size_t ITCHParser::pending_bytes() const {
    return pending_message.size();
  }

  inline bool ITCHParser::is_tracked(dt::StockLocate stock_locate) const {
    if (track_all)
      return true;

    for (std::size_t index = 0; index < tracked_count; ++index) {
      if (tracked_stocks[index] == stock_locate)
        return true;
    }

    return false;
  }

  inline bool ITCHParser::add_tracked_stock(dt::StockLocate stock_locate) {
    track_all = false;

    if (tracked_count >= MaxTrackedStocks)
      return false;

    if (is_tracked(stock_locate))
      return false;

    tracked_stocks[tracked_count++] = stock_locate;
    return true;
  }

  inline void ITCHParser::remove_tracked_stock(dt::StockLocate stock_locate) {
    for (std::size_t index = 0; index < tracked_count; ++index) {
      if (tracked_stocks[index] == stock_locate) {
        tracked_stocks[index] = tracked_stocks[tracked_count - 1];
        --tracked_count;
        return;
      }
    }
  }

  template<typename Handler>
  ParseOneResult ITCHParser::parse_payload(const char *msg, size_t message_size,
                                           Handler &&handler) {
#define HANDLE(TYPE, NAME, SIZE, ...)                                          \
  case #TYPE[0]: {                                                             \
    if (message_size != SIZE) [[unlikely]]                                     \
      return {.parsed = false, .skipped = true, .message_type = type};         \
                                                                               \
    const dt::StockLocate stock_locate = be16(msg + 1);                        \
                                                                               \
    if (!is_tracked(stock_locate))                                             \
      return {.parsed = false, .skipped = true, .message_type = type};         \
                                                                               \
    handler.on_##NAME(__VA_ARGS__);                                            \
                                                                               \
    return {.parsed = true, .skipped = false, .message_type = type};           \
  }

    if (message_size == 0) [[unlikely]] {
      return {.parsed = false, .skipped = true, .message_type = 0};
    }

    switch (const char type = msg[0]) {
        // sorted by occurrences
      HANDLE(A, add_order, SIZE_A, stock_locate, be16(msg + 3), be48(msg + 5),
               be64(msg + 11), msg[19], be32(msg + 20), identifier64(msg + 24),
               be32(msg + 32));
      HANDLE(D, order_delete, SIZE_D, stock_locate, be16(msg + 3), be48(msg + 5),
               be64(msg + 11));
      HANDLE(U, order_replace, SIZE_U, stock_locate, be16(msg + 3), be48(msg + 5),
               be64(msg + 11), be64(msg + 19), be32(msg + 27), be32(msg + 31));
      HANDLE(E, order_executed, SIZE_E, stock_locate, be16(msg + 3),
               be48(msg + 5), be64(msg + 11), be32(msg + 19), be64(msg + 23));
      HANDLE(F, add_order_with_mpid, SIZE_F, stock_locate, be16(msg + 3),
               be48(msg + 5), be64(msg + 11), msg[19], be32(msg + 20),
               identifier64(msg + 24), be32(msg + 32), identifier32(msg + 36));
      HANDLE(X, order_cancel, SIZE_X, stock_locate, be16(msg + 3), be48(msg + 5),
               be64(msg + 11), be32(msg + 19));
      HANDLE(C, order_executed_with_price, SIZE_C, stock_locate, be16(msg + 3),
               be48(msg + 5), be64(msg + 11), be32(msg + 19), be64(msg + 23),
               *(msg + 31) == 'Y', be32(msg + 32));
      default:
        return {.parsed = false, .skipped = true, .message_type = type};
    }
#undef HANDLE
  }

  template<typename Handler>
  ParseOneResult ITCHParser::parse_one(Cursor &cursor, Handler &&handler) {
    ParseOneResult result;

    if (!cursor.is_in_bounds(FRAME_HEADER_SIZE)) {
      result.needs_more_data = true;
      return result;
    }

    const char *frame_start = cursor.ptr;

    const auto message_size = static_cast<size_t>(
      static_cast<unsigned char>(frame_start[FRAME_OFFSET_SIZE]));
    const size_t frame_size = FRAME_HEADER_SIZE + message_size;

    if (message_size == SIZE_INVALID) [[unlikely]] {
      cursor.read_bytes(FRAME_HEADER_SIZE);
      result.consumed_bytes = FRAME_HEADER_SIZE;
      result.skipped = true;
      return result;
    }

    if (!cursor.is_in_bounds(frame_size)) {
      result.needs_more_data = true;
      return result;
    }

    const char *msg = frame_start + FRAME_HEADER_SIZE;

    result = parse_payload(msg, message_size, std::forward<Handler>(handler));
    cursor.read_bytes(frame_size);
    result.consumed_bytes = frame_size;

    return result;
  }

  template<typename Handler>
  ParseResult ITCHParser::parse(const char *data, size_t size,
                                Handler &&handler) {
    ParseResult result;

    const char *ptr = data;
    const char *end = data + size;

    if (!pending_message.empty()) {
      if (pending_message.size() < FRAME_HEADER_SIZE) {
        const size_t missing_header = FRAME_HEADER_SIZE - pending_message.size();
        const size_t available = static_cast<size_t>(end - ptr);
        const size_t to_copy =
            missing_header < available ? missing_header : available;

        if (to_copy != 0) {
          const size_t old_size = pending_message.size();
          pending_message.resize(old_size + to_copy);
          std::memcpy(pending_message.data() + old_size, ptr, to_copy);
          ptr += to_copy;
          result.consumed_bytes += to_copy;
        }

        if (pending_message.size() < FRAME_HEADER_SIZE) {
          result.needs_more_data = true;
          return result;
        }
      }

      const auto expected_payload_size = static_cast<size_t>(
        static_cast<unsigned char>(pending_message[FRAME_OFFSET_SIZE]));
      const size_t expected_total_size =
          FRAME_HEADER_SIZE + expected_payload_size;
      const size_t missing = expected_total_size - pending_message.size();
      const size_t available = static_cast<size_t>(end - ptr);
      const size_t to_copy = missing < available ? missing : available;

      pending_message.insert(pending_message.end(), ptr, ptr + to_copy);
      ptr += to_copy;
      result.consumed_bytes += to_copy;

      if (pending_message.size() < expected_total_size) {
        result.needs_more_data = true;
        return result;
      }

      ParseOneResult one =
          parse_payload(pending_message.data() + FRAME_HEADER_SIZE,
                        expected_payload_size, std::forward<Handler>(handler));

      if (one.parsed) {
        ++result.messages;
        // ++result.message_types[one.message_type];
      } else if (one.skipped) {
        ++result.skipped;
      }

      pending_message.clear();
    }

    Cursor cursor(ptr, static_cast<size_t>(end - ptr));

    while (cursor.is_in_bounds(1)) {
      const size_t total_offset_pre_parse = cursor.currentOffset;

      ParseOneResult one = parse_one(cursor, std::forward<Handler>(handler));

      if (one.needs_more_data) {
        const char *incomplete_start = cursor.start + total_offset_pre_parse;
        pending_message.assign(incomplete_start, end);

        result.consumed_bytes += static_cast<size_t>(incomplete_start - ptr);
        result.needs_more_data = true;
        break;
      }

      if (one.parsed) {
        ++result.messages;
        //++result.message_types[one.message_type];
      } else if (one.skipped) {
        ++result.skipped;
      }

      result.consumed_bytes += one.consumed_bytes;
    }

    return result;
  }
} // namespace md

#endif // MARKET_DATA_PARSER_ITCH_PARSER_HPP

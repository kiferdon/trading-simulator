//
// Created by silay on 5/4/26.
//

#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "market_data/itch_parser.hpp"

using namespace md;

namespace {
    struct CapturingITCHHandler : ITCHHandler {
        size_t parsed_messages = 0;
        uint64_t parsed_order_id = 0;

        inline void on_add_order(uint16_t locate, uint16_t tracking, uint64_t timestamp,
                                 uint64_t order_ref, char side, uint32_t shares,
                                 uint64_t stock, uint32_t price) {
            ++parsed_messages;
            parsed_order_id = order_ref;
            ITCHHandler::on_add_order(locate, tracking, timestamp, order_ref, side, shares, stock, price);
        }
    };

    void write_be16(char *p, uint16_t value) {
        p[0] = static_cast<char>((value >> 8) & 0xff);
        p[1] = static_cast<char>(value & 0xff);
    }

    void write_be32(char *p, uint32_t value) {
        p[0] = static_cast<char>((value >> 24) & 0xff);
        p[1] = static_cast<char>((value >> 16) & 0xff);
        p[2] = static_cast<char>((value >> 8) & 0xff);
        p[3] = static_cast<char>(value & 0xff);
    }

    void write_be64(char *p, uint64_t value) {
        p[0] = static_cast<char>((value >> 56) & 0xff);
        p[1] = static_cast<char>((value >> 48) & 0xff);
        p[2] = static_cast<char>((value >> 40) & 0xff);
        p[3] = static_cast<char>((value >> 32) & 0xff);
        p[4] = static_cast<char>((value >> 24) & 0xff);
        p[5] = static_cast<char>((value >> 16) & 0xff);
        p[6] = static_cast<char>((value >> 8) & 0xff);
        p[7] = static_cast<char>(value & 0xff);
    }

    std::vector<char> make_add_order_payload() {
        std::vector<char> msg(SIZE_A);

        msg[0] = 'A';
        write_be16(msg.data() + 1, 7);
        write_be16(msg.data() + 3, 11);

        // Timestamp: 6 bytes at offsets 5..10.
        msg[5] = 1;
        msg[6] = 2;
        msg[7] = 3;
        msg[8] = 4;
        msg[9] = 5;
        msg[10] = 6;

        write_be64(msg.data() + 11, 0x0102030405060708ULL);
        msg[19] = 'B';
        write_be32(msg.data() + 20, 100);

        std::memcpy(msg.data() + 24, "ABCDEFGH", 8);

        write_be32(msg.data() + 32, 1234500);

        return msg;
    }

    void test_big_endian_helpers() {
        char b16[] = {static_cast<char>(0x12), static_cast<char>(0x34)};
        char b32[] = {
            static_cast<char>(0x12),
            static_cast<char>(0x34),
            static_cast<char>(0x56),
            static_cast<char>(0x78),
        };
        char b48[] = {
            static_cast<char>(0x01),
            static_cast<char>(0x02),
            static_cast<char>(0x03),
            static_cast<char>(0x04),
            static_cast<char>(0x05),
            static_cast<char>(0x06),
        };
        char b64[] = {
            static_cast<char>(0x01),
            static_cast<char>(0x02),
            static_cast<char>(0x03),
            static_cast<char>(0x04),
            static_cast<char>(0x05),
            static_cast<char>(0x06),
            static_cast<char>(0x07),
            static_cast<char>(0x08),
        };

        assert(be16(b16) == 0x1234);
        assert(be32(b32) == 0x12345678);
        assert(be48(b48) == 0x010203040506ULL);
        assert(be64(b64) == 0x0102030405060708ULL);
    }

    void test_parse_message_header() {
        auto msg = make_add_order_payload();

        MessageHeader header = parse_message_header(msg.data());

        assert(header.message_type == 'A');
        assert(header.stock_locate == 7);
        assert(header.tracking_number == 11);
        assert(header.timestamp == 0x010203040506ULL);
    }

    void test_parse_add_order_message() {
        auto msg = make_add_order_payload();

        AddOrderMessage message = parse_add_order_message(msg.data());

        assert(message.header.message_type == 'A');
        assert(message.header.stock_locate == 7);
        assert(message.header.tracking_number == 11);
        assert(message.order_reference_number == 0x0102030405060708ULL);
        assert(message.buy_sell_indicator == 'B');
        assert(message.shares == 100);
        assert(get_identifier_string(message.stock) == std::string("ABCDEFGH"));
        assert(message.price == 1234500);
    }

    void test_cursor_bounds() {
        char data[] = {'a', 'b', 'c'};
        Cursor cursor(data, sizeof(data));

        assert(cursor.is_in_bounds(0));
        assert(cursor.is_in_bounds(3));
        assert(!cursor.is_in_bounds(4));

        assert(*cursor.read_bytes(1) == 'a');
        assert(*cursor.read_bytes(1) == 'b');

        cursor.reset();

        assert(*cursor.read_bytes(1) == 'a');
    }

    std::vector<char> make_framed_add_order_message() {
        auto payload = make_add_order_payload();

        std::vector<char> framed;
        framed.reserve(FRAME_HEADER_SIZE + payload.size());

        framed.push_back(0);
        framed.push_back(static_cast<char>(payload.size()));
        framed.insert(framed.end(), payload.begin(), payload.end());

        return framed;
    }

    void test_parse_one_framed_add_order_message() {
        auto framed = make_framed_add_order_message();

        Cursor cursor(framed.data(), framed.size());
        ITCHParser parser;
        CapturingITCHHandler handler;

        ParseOneResult result = parser.parse_one(cursor, handler);

        assert(result.parsed);
        assert(!result.skipped);
        assert(!result.needs_more_data);
        assert(result.consumed_bytes == framed.size());
        assert(cursor.currentOffset == framed.size());
        assert(handler.parsed_messages == 1);
        assert(handler.parsed_order_id == 0x0102030405060708ULL);
    }

    void test_parse_split_message() {
        auto framed = make_framed_add_order_message();

        ITCHParser parser;
        CapturingITCHHandler handler;

        const size_t split_at = 10;

        ParseResult first = parser.parse(framed.data(), split_at, handler);

        assert(first.messages == 0);
        assert(first.needs_more_data);
        assert(parser.pending_bytes() == split_at);

        ParseResult second = parser.parse(framed.data() + split_at, framed.size() - split_at, handler);

        assert(second.messages == 1);
        assert(!second.needs_more_data);
        assert(handler.parsed_messages == 1);
        assert(handler.parsed_order_id == 0x0102030405060708ULL);
        assert(parser.pending_bytes() == 0);
    }

    void test_parse_split_before_length_byte() {
        auto framed = make_framed_add_order_message();

        ITCHParser parser;
        CapturingITCHHandler handler;

        ParseResult first = parser.parse(framed.data(), 1, handler);

        assert(first.messages == 0);
        assert(first.needs_more_data);
        assert(parser.pending_bytes() == 1);

        ParseResult second = parser.parse(framed.data() + 1, framed.size() - 1, handler);

        assert(second.messages == 1);
        assert(!second.needs_more_data);
        assert(handler.parsed_messages == 1);
        assert(parser.pending_bytes() == 0);
    }
} // namespace

int main() {
    std::cout << "Tests started" << std::endl;
    test_big_endian_helpers();
    test_parse_message_header();
    test_parse_add_order_message();
    test_cursor_bounds();
    test_parse_one_framed_add_order_message();
    test_parse_split_message();
    test_parse_split_before_length_byte();
    std::cout << "Tests finished" << std::endl;

    return 0;
}
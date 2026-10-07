#pragma once

#include <algorithm>
#include <vector>

#include "input/input_source.hpp"
#include "market_data/itch_parser.hpp"

namespace simulator_test_data {
    inline std::vector<char> add_order(uint16_t locate, char side = 'B',
                                       uint32_t price = 1000, uint64_t id = 1) {
        std::vector<char> frame(md::FRAME_HEADER_SIZE + md::SIZE_A, 0);
        frame[1] = md::SIZE_A;
        char *message = frame.data() + md::FRAME_HEADER_SIZE;
        const auto write = [](char *out, uint64_t value, unsigned bytes) {
            for (unsigned i = 0; i < bytes; ++i) {
                out[bytes - i - 1] = static_cast<char>(value >> (8 * i));
            }
        };
        message[0] = 'A';
        write(message + 1, locate, 2);
        write(message + 5, id, 6);
        write(message + 11, id, 8);
        message[19] = side;
        write(message + 20, 100, 4);
        write(message + 32, price, 4);
        return frame;
    }

    class ChunkedInput final : public sim::IInputSource {
    public:
        ChunkedInput(const std::vector<char> &data, std::size_t chunk_size)
            : data_(data), chunk_size_(chunk_size) {}

        bool read_chunk(const char *&data, std::size_t &size) override {
            ++reads;
            if (offset_ == data_.size()) return false;
            size = std::min(chunk_size_, data_.size() - offset_);
            data = data_.data() + offset_;
            offset_ += size;
            return true;
        }

        std::size_t reads = 0;

    private:
        const std::vector<char> &data_;
        std::size_t chunk_size_;
        std::size_t offset_ = 0;
    };
}

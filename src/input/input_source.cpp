//
// Created by Claude on 2026-05-29.
//

#include "input/input_source.hpp"

namespace sim {
    FileInputSource::FileInputSource(const std::string& path, Mode mode, size_t chunk_size, double file_portion)
        : mode_(mode), chunk_size_(chunk_size), max_read_size_(std::numeric_limits<size_t>::max()) {
        file_.open(path, std::ios::binary | std::ios::ate);
        if (!file_.is_open()) {
            throw std::runtime_error("Failed to open file: " + path);
        }

        auto file_size = static_cast<size_t>(file_.tellg());
        max_read_size_ = static_cast<size_t>(std::min(1.0, std::max(0.0, file_portion)) * file_size);

        if (mode == Mode::WholeFile) {
            file_.seekg(0, std::ios::beg);
            buffer_.resize(max_read_size_);
            file_.read(buffer_.data(), max_read_size_);
            eof_ = file_.gcount() != static_cast<std::streamsize>(buffer_.size());
        }
    }

    bool FileInputSource::read_chunk(const char*& out_data, size_t& out_size) {
        if (mode_ == Mode::WholeFile) {
            if (buffer_.empty() || eof_) {
                return false;
            }
            out_data = buffer_.data();
            out_size = buffer_.size();
            eof_ = true;
            return true;
        }

        size_t bytes_to_read = std::min(chunk_size_, max_read_size_ - bytes_read_);
        if (bytes_to_read == 0) {
            eof_ = true;
            return false;
        }

        buffer_.resize(bytes_to_read);
        file_.read(buffer_.data(), bytes_to_read);
        auto gcount = file_.gcount();

        if (gcount <= 0) {
            return false;
        }

        buffer_.resize(static_cast<size_t>(gcount));
        bytes_read_ += static_cast<size_t>(gcount);
        out_data = buffer_.data();
        out_size = buffer_.size();

        eof_ = file_.eof() || bytes_read_ >= max_read_size_;
        return true;
    }

    bool FileInputSource::eof() const {
        return eof_;
    }
}
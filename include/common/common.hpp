//
// Created by silay on 5/26/26.
//

#ifndef HFT_SIMULATOR_COMMON_HPP
#define HFT_SIMULATOR_COMMON_HPP
#include <fstream>
#include <ios>
#include <iosfwd>
#include <vector>

namespace common {
    static std::vector<char> read_file(const std::string &path) {
        std::ifstream file(path, std::ios::binary);

        if (!file) {
            throw std::runtime_error("Failed to open input file: " + path);
        }

        file.seekg(0, std::ios::end);
        const auto end_position = file.tellg();

        if (end_position < 0) {
            throw std::runtime_error("Failed to determine input file size: " + path);
        }

        const auto size = static_cast<std::size_t>(end_position);
        file.seekg(0, std::ios::beg);

        std::vector<char> buffer(size);

        if (!buffer.empty()) {
            file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));

            if (!file) {
                throw std::runtime_error("Failed to read complete input file: " + path);
            }
        }

        return buffer;
    }
}

#endif //HFT_SIMULATOR_COMMON_HPP

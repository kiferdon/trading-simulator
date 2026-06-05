//
// Created by Claude on 2026-05-29.
//

#ifndef HFT_SIMULATOR_INPUT_SOURCE_HPP
#define HFT_SIMULATOR_INPUT_SOURCE_HPP

#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace sim {
    class IInputSource {
    public:
        virtual ~IInputSource() = default;

        virtual bool read_chunk(const char *&out_data, size_t &out_size) = 0;
    };

    class FileInputSource : public IInputSource {
    public:
        enum class Mode { WholeFile, Chunked };

        FileInputSource(const std::string &path, Mode mode = Mode::WholeFile,
                        size_t chunk_size = std::numeric_limits<size_t>::max(),
                        double file_portion = 1.0);

        bool read_chunk(const char *&out_data, size_t &out_size) override;

        bool eof() const;

    private:
        std::ifstream file_;
        Mode mode_;
        size_t chunk_size_;
        size_t max_read_size_;
        size_t bytes_read_ = 0;
        std::vector<char> buffer_;
        bool eof_ = false;
    };
}

#endif //HFT_SIMULATOR_INPUT_SOURCE_HPP

// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Stanley
#pragma once
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <vector>
namespace recomp_image {
// One open read-only source. Offsets and length are physical source bytes.
class ByteSource {
public:
    explicit ByteSource(const std::filesystem::path &path)
        : file_(path, std::ios::binary | std::ios::ate) {
        if (!file_ || file_.tellg() < 0)
            throw std::runtime_error("Cannot read selected game file");
        size_ = uint64_t(file_.tellg());
    }
    uint64_t size() const { return size_; }
    // Physical byte reads into caller storage; no sector buffer or allocation.
    // A failed/truncated read may modify a prefix. The next read clears stream
    // state and seeks explicitly, so independent valid reads remain possible.
    void read_into(uint64_t at, void *destination, uint64_t bytes) {
        if (at > size_ || bytes > size_ - at ||
            bytes > uint64_t(std::numeric_limits<std::streamsize>::max()) ||
            at > uint64_t(std::numeric_limits<std::streamoff>::max()) ||
            (bytes && !destination))
            throw std::runtime_error("Image extent is truncated or invalid");
        file_.clear();
        file_.seekg(std::streamoff(at));
        if (!file_ || (bytes && !file_.read(static_cast<char *>(destination), std::streamsize(bytes))))
            throw std::runtime_error("Image is truncated");
    }
    std::vector<unsigned char> read(uint64_t at, uint64_t bytes,
                                   uint64_t budget = 256u * 1024 * 1024) {
        if (at > size_ || bytes > size_ - at || bytes > budget ||
            bytes > std::numeric_limits<size_t>::max() ||
            bytes > uint64_t(std::numeric_limits<std::streamsize>::max()))
            throw std::runtime_error("Image extent is truncated or invalid");
        std::vector<unsigned char> result(static_cast<size_t>(bytes));
        read_into(at, result.data(), bytes);
        return result;
    }
private:
    std::ifstream file_;
    uint64_t size_ = 0;
};
inline std::vector<unsigned char> read_bounded_file(const char *path,
                                                   uint64_t *source_size = nullptr) {
    ByteSource file(std::filesystem::u8path(path ? path : ""));
    if (source_size) *source_size = file.size();
    return file.read(0, file.size());
}
}

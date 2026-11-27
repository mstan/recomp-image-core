// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Stanley. See PROVENANCE.md for donor history.
#pragma once
#include "chd_cd.hpp"
#include <functional>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace recomp_image {
// Read-only native File Folio storage boundary. Reads original sectors through
// the shared verified media backend; it never extracts or edits the image.
class OperaVolume {
public:
  using Sector = std::array<uint8_t, 2048>;
  using Reader = std::function<Sector(uint32_t)>;
  using FileId = uint32_t;
  struct Entry {
    std::string path;
    uint32_t bytes, block, blocks, type, flags;
    bool directory;
  };
  explicit OperaVolume(std::shared_ptr<ChdCd>);
  OperaVolume(uint32_t available_blocks, Reader);
  const std::string &label() const { return label_; }
  const std::array<uint8_t, 20> &content_identity() const { return content_identity_; }
  std::span<const Entry> entries() const { return entries_; }
  std::optional<FileId> find(std::string_view path) const;
  size_t read(FileId, uint64_t offset, std::span<uint8_t> destination) const;
  size_t read_blocks(FileId, uint32_t first_block, std::span<uint8_t> destination) const;

private:
  Reader reader_;
  uint32_t blocks_ = 0;
  std::string label_;
  std::array<uint8_t, 20> content_identity_{};
  std::vector<Entry> entries_;
  std::unordered_map<std::string, FileId> names_;
  void mount(uint32_t available_blocks);
  void directory(uint32_t first, uint32_t count, const std::string &path,
                 std::vector<uint32_t> &ancestors);
  static std::string key(std::string_view);
};
} // namespace recomp_image

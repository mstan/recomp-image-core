// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Stanley. See PROVENANCE.md for donor history.
#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <string>

namespace recomp_image {
struct ChdCdInfo {
  uint64_t logical_bytes = 0;
  uint32_t hunk_bytes = 0, hunk_count = 0, frames = 0;
  std::array<uint8_t, 20> raw_sha1{}, combined_sha1{};
};
// Read-only storage backend. This supplies sectors to the modeled CD drive;
// it does not load executables or replace guest filesystem/Portfolio services.
class ChdCd {
public:
  explicit ChdCd(const std::string &path);
  ~ChdCd();
  ChdCd(const ChdCd &) = delete;
  ChdCd &operator=(const ChdCd &) = delete;
  const ChdCdInfo &info() const;
  std::span<const uint8_t> read_hunk(uint32_t index);
  std::array<uint8_t, 2448> read_frame(uint32_t lba);
  std::array<uint8_t, 2048> read_mode1(uint32_t lba);

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
} // namespace recomp_image

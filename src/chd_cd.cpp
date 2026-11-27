// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Stanley. See PROVENANCE.md for donor history.
#include "recomp_image/chd_cd.hpp"
#include "libchdr/chd.h"
#include <algorithm>
#include <charconv>
#include <map>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace recomp_image {
namespace {
void require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void checked(chd_error value) {
  if (value != CHDERR_NONE)
    throw std::runtime_error(std::string("CHD: ") + chd_error_string(value));
}
} // namespace
struct ChdCd::Impl {
  chd_file *file = nullptr;
  ChdCdInfo info;
  std::vector<uint8_t> hunk;
  uint32_t cached_index = UINT32_MAX;
  ~Impl() {
    if (file)
      chd_close(file);
  }
};
ChdCd::ChdCd(const std::string &path) : impl_(std::make_unique<Impl>()) {
  checked(chd_open(path.c_str(), CHD_OPEN_READ, nullptr, &impl_->file));
  const auto &header = *chd_get_header(impl_->file);
  require(header.version == 5 && header.unitbytes == 2448,
          "only raw CD CHD v5 inputs are implemented");
  require(header.hunkbytes && header.hunkbytes <= 16 * 1024 * 1024 &&
              header.hunkbytes % 2448 == 0 && header.logicalbytes &&
              header.logicalbytes % 2448 == 0 &&
              header.logicalbytes < (1ull << 32) &&
              header.totalhunks ==
                  (header.logicalbytes + header.hunkbytes - 1) /
                      header.hunkbytes,
          "invalid or excessive CD geometry");
  require(std::all_of(std::begin(header.parentsha1),
                      std::end(header.parentsha1),
                      [](uint8_t v) { return v == 0; }),
          "parent CHDs are not implemented");
  std::array<char, 1024> metadata{};
  uint32_t length = 0;
  checked(chd_get_metadata(impl_->file, CDROM_TRACK_METADATA2_TAG, 0,
                           metadata.data(), uint32_t(metadata.size()), &length,
                           nullptr, nullptr));
  require(length && length < metadata.size(), "invalid track metadata length");
  std::map<std::string, std::string> fields;
  std::istringstream stream(std::string(metadata.data(), length));
  std::string token;
  while (stream >> token) {
    const auto split = token.find(':');
    require(split != std::string::npos, "invalid CD track metadata token");
    auto value = token.substr(split + 1);
    if (!value.empty() && value.back() == '\0')
      value.pop_back();
    require(fields.emplace(token.substr(0, split), value).second,
            "duplicate CD track metadata field");
  }
  require(fields["TRACK"] == "1" && fields["TYPE"] == "MODE1_RAW" &&
              fields["SUBTYPE"] == "NONE" && fields["PREGAP"] == "0" &&
              fields["POSTGAP"] == "0",
          "only one plain MODE1_RAW track is implemented");
  const auto &frames = fields["FRAMES"];
  uint32_t count = 0;
  const auto parsed =
      std::from_chars(frames.data(), frames.data() + frames.size(), count);
  require(parsed.ec == std::errc() &&
              parsed.ptr == frames.data() + frames.size() && count,
          "invalid CD frame count");
  require(count <= header.logicalbytes / 2448 &&
              header.logicalbytes / 2448 - count < 4,
          "CD track and CHD padding disagree");
  require(chd_get_metadata(impl_->file, CDROM_TRACK_METADATA2_TAG, 1,
                           metadata.data(), uint32_t(metadata.size()), nullptr,
                           nullptr, nullptr) == CHDERR_METADATA_NOT_FOUND,
          "multiple CD tracks are not implemented");
  impl_->info.logical_bytes = header.logicalbytes;
  impl_->info.hunk_bytes = header.hunkbytes;
  impl_->info.hunk_count = header.totalhunks;
  impl_->info.frames = count;
  std::copy_n(header.rawsha1, 20, impl_->info.raw_sha1.begin());
  std::copy_n(header.sha1, 20, impl_->info.combined_sha1.begin());
  impl_->hunk.resize(header.hunkbytes);
}
ChdCd::~ChdCd() = default;
const ChdCdInfo &ChdCd::info() const { return impl_->info; }
std::span<const uint8_t> ChdCd::read_hunk(uint32_t index) {
  require(index < info().hunk_count, "CD hunk index out of range");
  if (impl_->cached_index != index) {
    impl_->cached_index = UINT32_MAX;
    checked(chd_read(impl_->file, index, impl_->hunk.data()));
    impl_->cached_index = index;
  }
  return impl_->hunk;
}
std::array<uint8_t, 2448> ChdCd::read_frame(uint32_t lba) {
  require(lba < info().frames, "CD sector index out of range");
  const uint64_t offset = uint64_t(lba) * 2448;
  const auto data = read_hunk(uint32_t(offset / info().hunk_bytes));
  std::array<uint8_t, 2448> result;
  std::copy_n(data.begin() + (offset % info().hunk_bytes), result.size(),
              result.begin());
  return result;
}
std::array<uint8_t, 2048> ChdCd::read_mode1(uint32_t lba) {
  const auto raw = read_frame(lba);
  require(raw[0] == 0 && raw[11] == 0 && raw[15] == 1 &&
              std::all_of(raw.begin() + 1, raw.begin() + 11,
                          [](uint8_t v) { return v == 255; }),
          "invalid MODE1_RAW sync/mode header");
  std::array<uint8_t, 2048> result;
  std::copy_n(raw.begin() + 16, result.size(), result.begin());
  return result;
}
} // namespace recomp_image

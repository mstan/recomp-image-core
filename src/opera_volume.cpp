// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Stanley. See PROVENANCE.md for donor history.
#include "recomp_image/opera_volume.hpp"
#include <algorithm>
#include <cstring>
#include <set>
#include <stdexcept>

namespace recomp_image {
namespace {
uint32_t be32(const uint8_t *p) {
  return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
}
std::string field(const uint8_t *p, size_t n) {
  const auto end = std::find(p, p + n, uint8_t(0));
  return {reinterpret_cast<const char *>(p), size_t(end - p)};
}
[[noreturn]] void malformed(const char *reason) {
  throw std::runtime_error(std::string("Opera filesystem: ") + reason);
}
} // namespace

OperaVolume::OperaVolume(std::shared_ptr<ChdCd> disc) {
  if (!disc)
    throw std::invalid_argument("Opera filesystem requires mounted media");
  const auto available = disc->info().frames;
  content_identity_ = disc->info().raw_sha1;
  reader_ = [disc = std::move(disc)](uint32_t lba) { return disc->read_mode1(lba); };
  mount(available);
}
OperaVolume::OperaVolume(uint32_t available, Reader reader)
    : reader_(std::move(reader)) {
  if (!reader_)
    throw std::invalid_argument("Opera filesystem requires a sector reader");
  mount(available);
}

std::string OperaVolume::key(std::string_view path) {
  std::string result, component;
  auto append = [&] {
    if (component == "..")
      throw std::invalid_argument("Opera path cannot escape its volume");
    if (!component.empty() && component != ".") {
      if (!result.empty())
        result += '/';
      result += component;
    }
    component.clear();
  };
  for (unsigned char c : path) {
    if (!c)
      throw std::invalid_argument("Opera path contains NUL");
    if (c == '/' || c == '\\')
      append();
    else
      component += char(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c);
  }
  append();
  return result;
}

void OperaVolume::mount(uint32_t available) {
  if (!available)
    malformed("empty medium");
  const auto header = reader_(0);
  if (header[0] != 1 || std::memcmp(header.data() + 1, "ZZZZZ", 5) ||
      header[6] != 1)
    malformed("unsupported volume signature/version");
  if (be32(header.data() + 76) != 2048 || be32(header.data() + 92) != 2048)
    malformed("unsupported volume/directory block geometry");
  blocks_ = be32(header.data() + 80);
  if (!blocks_ || blocks_ > available)
    malformed("volume exceeds mounted medium");
  label_ = field(header.data() + 40, 32);
  if (be32(header.data() + 96) > 255)
    malformed("root avatar resource bound");
  std::vector<uint32_t> ancestors;
  directory(be32(header.data() + 100), be32(header.data() + 88), "", ancestors);
}

void OperaVolume::directory(uint32_t first, uint32_t count,
                            const std::string &path,
                            std::vector<uint32_t> &ancestors) {
  if (!first || !count || uint64_t(first) + count > blocks_)
    malformed("directory extent outside volume");
  if (ancestors.size() >= 64 ||
      std::find(ancestors.begin(), ancestors.end(), first) != ancestors.end())
    malformed("directory ancestry cycle/depth bound");
  ancestors.push_back(first);
  std::set<uint32_t> seen;
  uint32_t relative = 0;
  for (;;) {
    if (relative >= count || !seen.insert(relative).second)
      malformed("directory block link outside extent or cyclic");
    const auto sector = reader_(first + relative);
    const auto next = be32(sector.data());
    const auto end = be32(sector.data() + 12);
    auto offset = be32(sector.data() + 16);
    if (offset < 20 || end < offset || end > sector.size())
      malformed("invalid directory entry interval");
    bool done = false;
    while (offset < end) {
      if (end - offset < 68)
        malformed("truncated directory entry");
      const auto *p = sector.data() + offset;
      const auto flags = be32(p), last_avatar = be32(p + 64);
      if (last_avatar > 255 || uint64_t(offset) + 72 + last_avatar * 4 > end)
        malformed("truncated/excessive file avatars");
      if (be32(p + 12) != 2048)
        malformed("unsupported file block geometry");
      const auto name = field(p + 32, 32);
      if (name.empty() || name == "." || name == ".." ||
          name.find_first_of("/\\") != std::string::npos || path.size() + name.size() > 4096)
        malformed("invalid directory entry name/path bound");
      Entry entry{path.empty() ? name : path + '/' + name,
                  be32(p + 16), be32(p + 68), be32(p + 20),
                  be32(p + 8), flags, (flags & 255) == 7};
      if (uint64_t(entry.bytes) > uint64_t(entry.blocks) * 2048 ||
          uint64_t(entry.block) + entry.blocks > blocks_)
        malformed("file extent outside volume");
      if (entries_.size() >= 100000)
        malformed("directory entry resource bound");
      const auto id = FileId(entries_.size());
      if (!names_.emplace(key(entry.path), id).second)
        malformed("duplicate case-insensitive path");
      entries_.push_back(entry);
      if (entry.directory)
        directory(entry.block, entry.blocks, entry.path, ancestors);
      offset += 72 + last_avatar * 4;
      if (flags & 0x80000000u)
        done = true;
      if (flags & 0xc0000000u)
        break;
    }
    if (done || next == UINT32_MAX)
      break;
    relative = next;
  }
  ancestors.pop_back();
}

std::optional<OperaVolume::FileId> OperaVolume::find(std::string_view path) const {
  const auto it = names_.find(key(path));
  return it == names_.end() ? std::nullopt : std::optional<FileId>(it->second);
}
size_t OperaVolume::read(FileId id, uint64_t offset,
                         std::span<uint8_t> destination) const {
  if (id >= entries_.size() || entries_[id].directory)
    throw std::invalid_argument("Opera file ID does not name a regular file");
  const auto &entry = entries_[id];
  if (offset >= entry.bytes)
    return 0;
  const auto amount = size_t(std::min<uint64_t>(destination.size(), entry.bytes - offset));
  size_t copied = 0;
  while (copied < amount) {
    const auto sector = reader_(entry.block + uint32_t(offset / 2048));
    const auto in_sector = size_t(offset % 2048);
    const auto count = std::min(amount - copied, sector.size() - in_sector);
    std::memcpy(destination.data() + copied, sector.data() + in_sector, count);
    copied += count;
    offset += count;
  }
  return copied;
}
size_t OperaVolume::read_blocks(FileId id, uint32_t first,
                                std::span<uint8_t> destination) const {
  if (id >= entries_.size() || entries_[id].directory || destination.size() % 2048)
    throw std::invalid_argument("Opera block read requires a regular file and whole sectors");
  const auto &entry = entries_[id];
  if (first >= entry.blocks) return 0;
  const auto count = std::min<size_t>(destination.size() / 2048, entry.blocks - first);
  for (size_t n = 0; n < count; ++n) {
    const auto sector = reader_(entry.block + first + uint32_t(n));
    std::copy(sector.begin(), sector.end(), destination.begin() + n * 2048);
  }
  return count * 2048;
}
} // namespace recomp_image

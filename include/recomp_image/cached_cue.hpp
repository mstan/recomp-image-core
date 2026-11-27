// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Stanley. See PROVENANCE.md for donor history.
#pragma once



#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace recomp_image {

enum class TrackType { mode1, audio };

struct Track {
    TrackType type;
    uint32_t pregap_lba, start_lba, end_lba;
};

struct DiscFile {
    std::string name;
    uint32_t lba, size;
    uint8_t attributes = 0, unit = 0, gap = 0, number = 0;
};

using RawSector = std::array<uint8_t, 2352>;
using DataSector = std::array<uint8_t, 2048>;

// Supports a single session of one BINARY file per track and embedded pregaps.
class CachedCueDisc {
public:
    explicit CachedCueDisc(const std::filesystem::path &cue);
    ~CachedCueDisc();
    CachedCueDisc(CachedCueDisc &&) noexcept;
    CachedCueDisc &operator=(CachedCueDisc &&) noexcept;

    const std::vector<Track> &tracks() const { return tracks_; }
    void cache_tracks();
    RawSector read_raw(uint32_t lba);
    void read_raw(uint32_t lba, uint8_t sector[2352]);
    const uint8_t *view_raw(uint32_t lba) const;
    DataSector read_mode1(uint32_t lba);
    DiscFile root_directory();
    std::vector<DiscFile> read_directory(uint32_t lba, uint32_t size, unsigned first, unsigned limit);

protected:
    bool in_data_track(uint32_t lba, uint32_t bytes);

private:
    const Track &track_at(uint32_t lba) const;
    std::vector<Track> tracks_;
    struct TrackFile;
    std::vector<std::unique_ptr<TrackFile>> files_;
};

} // namespace recomp_image

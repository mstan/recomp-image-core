// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Stanley. See PROVENANCE.md for donor history.
#include "recomp_image/cached_cue.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>

namespace recomp_image {
namespace {

std::optional<uint32_t> read_msf(const std::string &text)
{
    if (text.size() != 8 || text[2] != ':' || text[5] != ':')
        return std::nullopt;
    unsigned groups[3] = {};
    for (unsigned i = 0; i < 3; ++i) {
        const char tens = text[3 * i], units = text[3 * i + 1];
        if (tens < '0' || tens > '9' || units < '0' || units > '9')
            return std::nullopt;
        groups[i] = 10 * (tens - '0') + units - '0';
    }
    if (groups[1] >= 60 || groups[2] >= 75)
        return std::nullopt;
    return (groups[0] * 60 + groups[1]) * 75 + groups[2];
}

std::string field(const DataSector &sector, size_t offset, size_t length)
{
    std::string value(sector.begin() + offset, sector.begin() + offset + length);
    const auto last = value.find_last_not_of(' ');
    value.resize(last == std::string::npos ? 0 : last + 1);
    if (std::any_of(value.begin(), value.end(), [](unsigned char c) {
            return c < 0x20 || c > 0x7e;
        }))
        throw std::runtime_error("Invalid text in ISO volume descriptor");
    return value;
}

uint32_t iso_number(const uint8_t *data)
{
    uint32_t little = 0, big = 0;
    for (unsigned i = 0; i < 4; ++i) {
        little |= static_cast<uint32_t>(data[i]) << (8 * i);
        big = (big << 8) | data[i + 4];
    }
    if (little != big)
        throw std::runtime_error("Inconsistent ISO 9660 number");
    return big;
}

DiscFile directory_entry(const uint8_t *entry, size_t available)
{
    if (available < 34 || entry[0] < 34 || entry[0] > available || entry[1] ||
        !entry[32] || entry[32] > entry[0] - 33)
        throw std::runtime_error("Malformed ISO 9660 directory entry");
    DiscFile file{std::string(entry + 33, entry + 33 + entry[32]), iso_number(entry + 2),
                  iso_number(entry + 10), entry[25], entry[26], entry[27]};
    const unsigned system = 33u + (entry[32] | 1u);
    if (system + 14 <= entry[0] && entry[system + 6] == 'X' && entry[system + 7] == 'A') {
        file.attributes = (file.attributes & 2) | (entry[system + 4] & 0xf8);
        file.number = entry[system + 8];
    }
    return file;
}

} // namespace

struct CachedCueDisc::TrackFile {
    explicit TrackFile(const std::filesystem::path &path)
        : stream(path, std::ios::binary | std::ios::ate)
    {
        const auto length = stream.tellg();
        if (length <= 0)
            throw std::runtime_error("Cannot open CUE track file");
        size = static_cast<uint64_t>(length);
    }

    void cache()
    {
        if (data)
            return;
        if (size > std::numeric_limits<size_t>::max() || size > std::numeric_limits<std::streamsize>::max())
            throw std::runtime_error("CUE track file is too large to read ahead");
        std::unique_ptr<uint8_t[]> cached(new uint8_t[static_cast<size_t>(size)]);
        stream.clear();
        stream.seekg(0);
        if (!stream.read(reinterpret_cast<char *>(cached.get()), static_cast<std::streamsize>(size)))
            throw std::runtime_error("Short read from CUE track");
        stream.close();
        data = std::move(cached);
    }

    const uint8_t *view(uint64_t offset) const { return data ? data.get() + offset : nullptr; }
    void read(uint64_t offset, uint8_t *sector)
    {
        if (const uint8_t *cached = view(offset)) {
            std::memcpy(sector, cached, 2352);
            return;
        }
        stream.clear();
        stream.seekg(static_cast<std::streamoff>(offset));
        if (!stream.read(reinterpret_cast<char *>(sector), 2352))
            throw std::runtime_error("Short read from CUE track");
    }

    uint64_t size = 0;
    std::unique_ptr<uint8_t[]> data;
    std::ifstream stream;
};

CachedCueDisc::~CachedCueDisc() = default;
CachedCueDisc::CachedCueDisc(CachedCueDisc &&) noexcept = default;
CachedCueDisc &CachedCueDisc::operator=(CachedCueDisc &&) noexcept = default;

CachedCueDisc::CachedCueDisc(const std::filesystem::path &cue)
{
    std::ifstream input(cue);
    if (!input)
        throw std::runtime_error("Cannot open CUE sheet");

    std::filesystem::path bin;
    std::optional<TrackType> track_type;
    bool index_zero = false;
    uint32_t next_lba = 0;
    size_t line_number = 0;
    for (std::string line; std::getline(input, line);) {
        ++line_number;
        const auto invalid = [line_number]() {
            return std::runtime_error("Invalid or unsupported CUE line " + std::to_string(line_number));
        };
        std::istringstream tokens(line);
        std::string key;
        tokens >> key;
        if (key.empty() || key == "REM")
            continue;
        if (key == "FILE") {
            std::string name, format;
            if (!bin.empty() || !(tokens >> std::quoted(name) >> format) ||
                name.empty() || format != "BINARY")
                throw invalid();
            bin = cue.parent_path() / std::filesystem::u8path(name);
        } else if (key == "TRACK") {
            unsigned number;
            std::string type;
            if (bin.empty() || track_type || !(tokens >> number >> type) ||
                number != tracks_.size() + 1 || number > 99)
                throw invalid();
            if (type == "AUDIO")
                track_type = TrackType::audio;
            else if (type == "MODE1/2352")
                track_type = TrackType::mode1;
            else
                throw invalid();
        } else if (key == "INDEX") {
            unsigned index;
            std::string time;
            if (!track_type || !(tokens >> index >> time))
                throw invalid();
            const auto start = read_msf(time);
            if (!start)
                throw invalid();
            if (index == 0 && !index_zero && *start == 0) {
                index_zero = true;
            } else if (index == 1 && (index_zero || *start == 0)) {
                auto track = std::make_unique<TrackFile>(bin);
                if (track->size % 2352 != 0)
                    throw invalid();
                const auto sectors = track->size / 2352;
                if (sectors <= *start || sectors > std::numeric_limits<uint32_t>::max() - next_lba ||
                    (tracks_.empty() && *start != 0))
                    throw invalid();
                const auto end = next_lba + static_cast<uint32_t>(sectors);
                tracks_.push_back({*track_type, next_lba, next_lba + *start, end});
                files_.push_back(std::move(track));
                next_lba = end;
                track_type.reset();
                index_zero = false;
                bin.clear();
            } else {
                throw invalid();
            }
        } else {
            throw invalid();
        }
        std::string trailing;
        if (tokens >> trailing)
            throw invalid();
    }
    if (input.bad() || tracks_.empty() || !bin.empty())
        throw std::runtime_error("Incomplete CUE sheet");
}

const Track &CachedCueDisc::track_at(uint32_t lba) const
{
    const auto track = std::upper_bound(tracks_.begin(), tracks_.end(), lba,
        [](uint32_t address, const Track &entry) { return address < entry.end_lba; });
    if (track == tracks_.end())
        throw std::out_of_range("Sector is past the final track");
    return *track;
}

void CachedCueDisc::cache_tracks()
{
    /* The CUE addresses use a two-digit minute component. */
    constexpr uint32_t max_cd_sectors = 100 * 60 * 75;
    if (tracks_.back().end_lba > max_cd_sectors)
        throw std::runtime_error("CUE exceeds the cached CD address range");
    for (const auto &file : files_)
        file->cache();
}

RawSector CachedCueDisc::read_raw(uint32_t lba)
{
    RawSector sector;
    read_raw(lba, sector.data());
    return sector;
}

void CachedCueDisc::read_raw(uint32_t lba, uint8_t sector[2352])
{
    const auto &track = track_at(lba);
    const auto offset = static_cast<uint64_t>(lba - track.pregap_lba) * 2352;
    files_[&track - tracks_.data()]->read(offset, sector);
}

const uint8_t *CachedCueDisc::view_raw(uint32_t lba) const
{
    const auto &track = track_at(lba);
    const auto offset = static_cast<uint64_t>(lba - track.pregap_lba) * 2352;
    return files_[&track - tracks_.data()]->view(offset);
}

DataSector CachedCueDisc::read_mode1(uint32_t lba)
{
    const auto &track = track_at(lba);
    if (track.type != TrackType::mode1 || lba < track.start_lba)
        throw std::out_of_range("Sector is outside a MODE1 data track");
    const auto offset = static_cast<uint64_t>(lba - track.pregap_lba) * 2352;
    const auto &file = files_[&track - tracks_.data()];
    RawSector storage;
    const uint8_t *sector = file->view(offset);
    if (!sector) {
        file->read(offset, storage.data());
        sector = storage.data();
    }
    constexpr std::array<uint8_t, 12> sync = {0, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 0};
    if (!std::equal(sync.begin(), sync.end(), sector) || sector[15] != 1)
        throw std::runtime_error("Invalid raw MODE1 sector");
    DataSector data;
    std::copy_n(sector + 16, data.size(), data.begin());
    return data;
}

bool CachedCueDisc::in_data_track(uint32_t lba, uint32_t bytes)
{
    constexpr size_t sector_size = DataSector{}.size();
    const auto &track = track_at(lba);
    return track.type == TrackType::mode1 && lba >= track.start_lba &&
           (uint64_t(bytes) + sector_size - 1) / sector_size <= track.end_lba - lba;
}

DiscFile CachedCueDisc::root_directory()
{
    const auto pvd = read_mode1(16);
    if (pvd[0] != 1 || field(pvd, 1, 5) != "CD001" || pvd[6] != 1 ||
        pvd[128] != 0 || pvd[129] != 8 || pvd[130] != 8 || pvd[131] != 0)
        throw std::runtime_error("Missing 2048-byte ISO 9660 primary volume");
    auto root = directory_entry(&pvd[156], pvd.size() - 156);
    if (!(root.attributes & 2) || !root.size || !in_data_track(root.lba, root.size))
        throw std::runtime_error("Invalid ISO 9660 root directory");
    return root;
}

std::vector<DiscFile> CachedCueDisc::read_directory(uint32_t lba, uint32_t size, unsigned first, unsigned limit)
{
    constexpr size_t sector_size = DataSector{}.size();
    if (first < 2 || limit < 2 || !size || !in_data_track(lba, size))
        throw std::runtime_error("Invalid ISO 9660 directory extent");
    std::vector<DiscFile> entries;
    unsigned identifier = 0;
    for (uint64_t relative = 0; relative < size; relative += sector_size) {
        const auto page = read_mode1(lba + static_cast<uint32_t>(relative / sector_size));
        const auto used = std::min<uint64_t>(sector_size, size - relative);
        for (size_t start = 0; start < used && page[start]; start += page[start]) {
            auto file = directory_entry(&page[start], static_cast<size_t>(used) - start);
            if (identifier < 2 && (file.name.size() != 1 || (unsigned char)file.name[0] != identifier ||
                                   !(file.attributes & 2)))
                throw std::runtime_error("Missing ISO 9660 self or parent entry");
            if (identifier < 2 || identifier >= first)
                entries.push_back(std::move(file));
            ++identifier;
            if (entries.size() == limit)
                return entries;
        }
    }
    if (entries.size() < 2)
        throw std::runtime_error("Incomplete ISO 9660 directory");
    return entries;
}


} // namespace recomp_image

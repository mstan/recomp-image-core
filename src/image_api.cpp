#include "recomp_image/image.h"
#include "recomp_image/image_reader.hpp"
#include <algorithm>
#include <memory>
struct recomp_image_handle { recomp_image::ImageReader reader; };
extern "C" {
recomp_image_handle* recomp_image_open(const char* path) {
    if (!path) return nullptr;
    try {
        auto value = std::make_unique<recomp_image_handle>();
        if (!value->reader.Open(path)) return nullptr;
        return value.release();
    } catch (...) { return nullptr; }
}
void recomp_image_close(recomp_image_handle* image) { delete image; }
uint32_t recomp_image_sector_count(const recomp_image_handle* image) {
    return image ? const_cast<recomp_image_handle*>(image)->reader.GetSectorCount() : 0;
}
int recomp_image_track_count(const recomp_image_handle* image) { return image ? image->reader.TrackCount() : 0; }
int recomp_image_track(const recomp_image_handle* image, int number, uint32_t* start, uint32_t* pregap, int* audio) {
    if (!image || number < 1 || number > image->reader.TrackCount() || !start || !pregap || !audio) return 0;
    *start = image->reader.TrackStartLBA(number);
    *pregap = image->reader.TrackPregapLBA(number);
    *audio = image->reader.TrackIsAudio(number) ? 1 : 0;
    return 1;
}
int recomp_image_read(recomp_image_handle* image, uint32_t lba, recomp_image_view view, uint8_t* bytes, size_t capacity) {
    try { return image && image->reader.ReadView(lba, view, bytes, capacity); } catch (...) { return 0; }
}
int recomp_image_identity(recomp_image_handle* image, uint8_t out[32]) {
    try { return image && image->reader.Identity(out); } catch (...) { return 0; }
}
}

#ifndef RECOMP_IMAGE_H
#define RECOMP_IMAGE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct recomp_image_handle recomp_image_handle;
typedef enum recomp_image_view {
    RECOMP_IMAGE_USER_2048 = 0,
    RECOMP_IMAGE_RAW_2352 = 1,
    RECOMP_IMAGE_MODE2_BODY_2336 = 2,
    RECOMP_IMAGE_MODE2_FORM2_2324 = 3
} recomp_image_view;
/* One handle per owner/thread; no process globals. Files are always read-only.
 * All failures return zero/null, never propagate C++ exceptions into C. */
recomp_image_handle* recomp_image_open(const char* path);
void recomp_image_close(recomp_image_handle* image);
uint32_t recomp_image_sector_count(const recomp_image_handle* image);
int recomp_image_track_count(const recomp_image_handle* image);
int recomp_image_track(const recomp_image_handle* image, int number,
                       uint32_t* start, uint32_t* pregap, int* audio);
int recomp_image_read(recomp_image_handle* image, uint32_t lba,
                      recomp_image_view view, uint8_t* bytes, size_t capacity);
/* Explicit full-image computation; see README for canonical identity-v1.
 * Success writes 32 bytes. Failure leaves output unchanged. */
int recomp_image_identity(recomp_image_handle* image, uint8_t out[32]);
#ifdef __cplusplus
}
#endif
#endif

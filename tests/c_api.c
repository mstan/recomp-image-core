#include "recomp_image/image.h"
int main(void) {
    unsigned char bytes[2352] = {0};
    if (recomp_image_open(0) || recomp_image_open("nonexistent-image.iso")) return 1;
    if (recomp_image_sector_count(0) || recomp_image_track_count(0)) return 2;
    if (recomp_image_read(0, 0, RECOMP_IMAGE_RAW_2352, bytes, sizeof(bytes))) return 3;
    if (recomp_image_identity(0, bytes)) return 4;
    recomp_image_close(0);
    return 0;
}

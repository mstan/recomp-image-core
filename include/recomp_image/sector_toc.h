#pragma once
#include <stdint.h>
typedef struct {int32_t index00,start,end;uint8_t data;} RecompImageTrack;
typedef struct {RecompImageTrack track[99];unsigned count;int32_t leadout;} RecompImageTOC;

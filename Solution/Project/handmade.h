// Platform Independent Layer

// only define the header file if not called before - like singleton
#if !defined HANDMADE_H

#include<stdint.h>


#define global_variable static

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

struct BitmapState {
	void* memory;
	int byte_per_pixel = 4;
	int width;
	int height;
};

global_variable void PlatformGameRender(BitmapState bitmap_state, int x_offset, int y_offset);

// will define this when called first time and then future calls to header will skip all code.
#define HANDMADE_H
#endif

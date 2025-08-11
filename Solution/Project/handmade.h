// Platform Independent Layer

// only define the header file if not called before - like singleton
#if !defined HANDMADE_H

#include<stdint.h>


#define global_variable static

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

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

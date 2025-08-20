#include<handmade.h>

global_variable void animateCheckerPattern(BitmapState bitmap_state, int x_offset, int y_offset) {
	uint8* row = (uint8*)bitmap_state.memory;
	for (int y = 0; y < bitmap_state.height; y++) {
		uint32* pixel = (uint32*)row;
		for (int x = 0; x < bitmap_state.width; x++) {
			uint8 green_offset = x + x_offset;
			uint8 blue_offset = y + y_offset;
			*(pixel++) = (green_offset << 8) | (blue_offset);
		}
		row = row + bitmap_state.width * bitmap_state.byte_per_pixel;
	}
}

global_variable void PlatformGameRender(BitmapState bitmap_state, int x_offset, int y_offset) {
	animateCheckerPattern(bitmap_state, x_offset, y_offset);
}
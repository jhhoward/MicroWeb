#pragma once

#include <stdint.h>

// Images are 1bpp
struct Image
{
	uint16_t width;
	uint16_t height;
	uint8_t* data;
};

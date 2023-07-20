#ifndef _IMAGE_H_
#define _IMAGE_H_

#include <stdint.h>

// Images are 1bpp
struct Image
{
	uint16_t width;
	uint16_t height;
	uint16_t pitch;
	uint8_t data[1];
};

#endif

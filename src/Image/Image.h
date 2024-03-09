#ifndef _IMAGE_H_
#define _IMAGE_H_

#include <stdint.h>

struct ImageMetadata
{
	uint16_t width;
	uint16_t height;
	uint16_t pitch;
	uint8_t bpp;
};

struct Image : ImageMetadata
{
	Image() : lines(nullptr) 
	{
		width = height = pitch = bpp = 0;
		isLoaded = false;
	}
	struct MemBlockHandle* lines;
	bool isLoaded;
};

#endif

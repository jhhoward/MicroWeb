#ifndef _IMAGE_H_
#define _IMAGE_H_

#include <stdint.h>

#include "../Memory/MemBlock.h"

struct ImageMetadata
{
	uint16_t width;
	uint16_t height;
	uint16_t pitch;
	uint8_t bpp;
};

struct Image : ImageMetadata
{
	Image() 
	{
		width = height = pitch = bpp = sourceWidth = sourceHeight = 0;
	}
	MemBlockHandle lines;
	uint16_t sourceWidth, sourceHeight;
};

#endif

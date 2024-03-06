#ifndef _VIDMODES_H_
#define _VIDMODES_H_

#include <stdint.h>
#include "Datapack.h"

#define HERCULES_MODE 0

struct VideoModeInfo
{
	const char* name;
	int biosVideoMode;
	int screenWidth;
	int screenHeight;
	uint8_t bpp;
	float aspectRatio;
	DataPack::Preset dataPackIndex : 8;
	uint16_t vramPage1;
	uint16_t vramPage2;
	uint16_t vramPage3;
	uint16_t vramPage4;
};

VideoModeInfo* ShowVideoModePicker(int defaultSelection);

#endif
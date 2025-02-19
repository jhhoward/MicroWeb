#ifndef _VIDMODES_H_
#define _VIDMODES_H_

#include <stdint.h>
#include "Datapack.h"
#include "Draw/Surface.h"

#define HERCULES_MODE 0
#define CGA_COMPOSITE_MODE 4

struct VideoModeInfo
{
	const char* name;
	int biosVideoMode;
	int screenWidth;
	int screenHeight;
	DrawSurface::Format surfaceFormat;
	int aspectRatio;
	int zoom;
	DataPack::Preset dataPackIndex : 8;
	uint16_t vramPage1;
	uint16_t vramPage2;
	uint16_t vramPage3;
	uint16_t vramPage4;
};

VideoModeInfo* ShowVideoModePicker(int defaultSelection);

extern VideoModeInfo VideoModeList[];
int GetNumVideoModes();

#endif

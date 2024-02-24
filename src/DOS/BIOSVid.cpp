//
// Copyright (C) 2021 James Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//

#include <dos.h>
#include <stdio.h>
#include <i86.h>
#include <conio.h>
#include <memory.h>
#include <stdint.h>
#include "../Image/Image.h"
#include "BIOSVid.h"
#include "../Interface.h"
#include "../DataPack.h"
#include "../Draw/Surf1bpp.h"
#include "../Draw/Surf4bpp.h"
#include "../Draw/Surf8bpp.h"
#include "../VidModes.h"

BIOSVideoDriver::BIOSVideoDriver()
{
}

void BIOSVideoDriver::Init(VideoModeInfo* inVideoModeInfo)
{
	videoModeInfo = inVideoModeInfo;
	startingScreenMode = GetScreenMode();

	screenWidth = videoModeInfo->screenWidth;
	screenHeight = videoModeInfo->screenHeight;

	SetScreenMode(videoModeInfo->biosVideoMode);

	Assets.LoadPreset(videoModeInfo->dataPackIndex);

	int screenPitch = 0;

	if (videoModeInfo->bpp == 1)
	{
		drawSurface = new DrawSurface_1BPP(screenWidth, screenHeight);
		screenPitch = screenWidth / 8;
		colourScheme = monochromeColourScheme;
		paletteLUT = nullptr;
	}
	else if (videoModeInfo->bpp == 4)
	{
		drawSurface = new DrawSurface_4BPP(screenWidth, screenHeight);
		screenPitch = screenWidth / 8;
		colourScheme = egaColourScheme;
		paletteLUT = cgaPaletteLUT;
	}
	else if (videoModeInfo->bpp == 8)
	{
		drawSurface = new DrawSurface_8BPP(screenWidth, screenHeight);
		screenPitch = screenWidth;
		colourScheme = egaColourScheme;
		paletteLUT = cgaPaletteLUT;
	}

	if (videoModeInfo->vramPage3)
	{
		// 4 page interlaced memory layout
		int offset = 0;
		for (int y = 0; y < screenHeight; y += 4)
		{
			drawSurface->lines[y] = (uint8_t*) MK_FP(videoModeInfo->vramPage1, offset);
			drawSurface->lines[y + 1] = (uint8_t*) MK_FP(videoModeInfo->vramPage2, offset);
			drawSurface->lines[y + 2] = (uint8_t*) MK_FP(videoModeInfo->vramPage3, offset);
			drawSurface->lines[y + 3] = (uint8_t*) MK_FP(videoModeInfo->vramPage4, offset);
			offset += screenPitch;
		}
	}
	else if (videoModeInfo->vramPage2)
	{
		// 2 page interlaced memory layout
		int offset = 0;
		for (int y = 0; y < screenHeight; y += 2)
		{
			drawSurface->lines[y] = (uint8_t*)MK_FP(videoModeInfo->vramPage1, offset);
			drawSurface->lines[y + 1] = (uint8_t*)MK_FP(videoModeInfo->vramPage2, offset);
			offset += screenPitch;
		}
	}
	else
	{
		// No interlacing
		int offset = 0;
		for (int y = 0; y < screenHeight; y++)
		{
			drawSurface->lines[y] = (uint8_t*)MK_FP(videoModeInfo->vramPage1, offset);
			offset += screenPitch;
		}
	}

	//DrawSurface_1BPP* drawSurface1BPP = new DrawSurface_1BPP(screenWidth, screenHeight);
	//for (int y = 0; y < screenHeight; y += 2)
	//{
	//	drawSurface1BPP->lines[y] = (CGA_BASE_VRAM_ADDRESS) + (40 * y);
	//	drawSurface1BPP->lines[y + 1] = (CGA_PAGE2_VRAM_ADDRESS) + (40 * y);
	//}
	//drawSurface = drawSurface1BPP;
	//
	//colourScheme.pageColour = 1;
	//colourScheme.linkColour = 0;
	//colourScheme.textColour = 0;
	//colourScheme.buttonColour = 1;

}

void BIOSVideoDriver::Shutdown()
{
	SetScreenMode(startingScreenMode);
}

int BIOSVideoDriver::GetScreenMode()
{
	union REGS inreg, outreg;
	inreg.h.ah = 0xf;

	int86(0x10, &inreg, &outreg);

	return (int)outreg.h.al;
}

void BIOSVideoDriver::SetScreenMode(int screenMode)
{
	union REGS inreg, outreg;
	inreg.h.ah = 0;
	inreg.h.al = (unsigned char)screenMode;

	int86(0x10, &inreg, &outreg);
}


void BIOSVideoDriver::ScaleImageDimensions(int& width, int& height)
{
	height /= videoModeInfo->aspectRatio;
	// Scale to 4:3
	//height = (height * 5) / 12;
}

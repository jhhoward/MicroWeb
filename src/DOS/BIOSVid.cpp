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
#include "../Draw/Surf2bpp.h"
#include "../Draw/Surf4bpp.h"
#include "../Draw/Surf8bpp.h"
#include "Surf1512.h"
#include "../VidModes.h"

BIOSVideoDriver::BIOSVideoDriver()
{
	startingScreenMode = -1;
}

void BIOSVideoDriver::Init(VideoModeInfo* inVideoModeInfo)
{
	videoMode = inVideoModeInfo;
	startingScreenMode = GetScreenMode();

	screenWidth = videoMode->screenWidth;
	screenHeight = videoMode->screenHeight;

	if (!SetScreenMode(videoMode->biosVideoMode))
	{
		Platform::FatalError("Could not set video mode: %d", videoMode->biosVideoMode);
		return;
	}

	if (videoMode->biosVideoMode == CGA_COMPOSITE_MODE)
	{
		SetScreenMode(6);
		outp(0x3D8, 0x1a);
		Assets.LoadPreset(videoMode->dataPackIndex);
	}
	else
	{
		Assets.LoadPreset(videoMode->dataPackIndex);
	}

	int screenPitch = 0;

	switch(videoMode->surfaceFormat)
	{
	case DrawSurface::Format_1BPP:
		drawSurface = new DrawSurface_1BPP(screenWidth, screenHeight);
		screenPitch = screenWidth / 8;
		colourScheme = monochromeColourScheme;
		paletteLUT = nullptr;
		break;
	case DrawSurface::Format_2BPP:
		drawSurface = new DrawSurface_2BPP(screenWidth, screenHeight);
		screenPitch = screenWidth / 4;

		if (videoMode->biosVideoMode == CGA_COMPOSITE_MODE)
		{
			paletteLUT = compositeCgaPaletteLUT;
			colourScheme = compositeCgaColourScheme;
		}
		else
		{
			paletteLUT = cgaPaletteLUT;
			colourScheme = cgaColourScheme;
		}
		break;
	case DrawSurface::Format_4BPP_EGA:
		drawSurface = new DrawSurface_4BPP(screenWidth, screenHeight);
		screenPitch = screenWidth / 8;
		colourScheme = egaColourScheme;
		paletteLUT = egaPaletteLUT;
		break;
	case DrawSurface::Format_8BPP:
		drawSurface = new DrawSurface_8BPP(screenWidth, screenHeight);
		screenPitch = screenWidth;
		colourScheme = colourScheme666;
		paletteLUT = new uint8_t[256];
		if (!paletteLUT)
		{
			Platform::FatalError("Could not allocate memory for paletteLUT");
		}

		for (int n = 0; n < 256; n++)
		{
			int r = (n & 0xe0);
			int g = (n & 0x1c) << 3;
			int b = (n & 3) << 6;

			int rgbBlue = ((long)b * 255) / 0xc0;
			int rgbGreen = ((long)g * 255) / 0xe0;
			int rgbRed = ((long)r * 255) / 0xe0;

			paletteLUT[n] = RGB666(rgbRed, rgbGreen, rgbBlue);
		}

		// Set palette to RGB666
		outp(0x03C6, 0xff);
		outp(0x03C8, 16);

		for (int r = 0; r < 6; r++)
		{
			for (int g = 0; g < 6; g++)
			{
				for (int b = 0; b < 6; b++)
				{
					outp(0x03C9, (r * 63) / 5);
					outp(0x03C9, (g * 63) / 5);
					outp(0x03C9, (b * 63) / 5);
				}
			}
		}
		break;

	case DrawSurface::Format_4BPP_PC1512:
		drawSurface = new DrawSurface_4BPP_PC1512(screenWidth, screenHeight);
		screenPitch = screenWidth / 8;
		colourScheme = egaColourScheme;
		paletteLUT = egaPaletteLUT;
		break;

	default:
		Platform::FatalError("Unsupported video format");
		break;
	}

	if (!drawSurface || !drawSurface->lines)
	{
		Platform::FatalError("Could not allocate memory for draw surface");
	}

	if (videoMode->vramPage3)
	{
		// 4 page interlaced memory layout
		int offset = 0;
		for (int y = 0; y < screenHeight; y += 4)
		{
			drawSurface->lines[y] = (uint8_t*) MK_FP(videoMode->vramPage1, offset);
			drawSurface->lines[y + 1] = (uint8_t*) MK_FP(videoMode->vramPage2, offset);
			drawSurface->lines[y + 2] = (uint8_t*) MK_FP(videoMode->vramPage3, offset);
			drawSurface->lines[y + 3] = (uint8_t*) MK_FP(videoMode->vramPage4, offset);
			offset += screenPitch;
		}
	}
	else if (videoMode->vramPage2)
	{
		// 2 page interlaced memory layout
		int offset = 0;
		for (int y = 0; y < screenHeight; y += 2)
		{
			drawSurface->lines[y] = (uint8_t*)MK_FP(videoMode->vramPage1, offset);
			drawSurface->lines[y + 1] = (uint8_t*)MK_FP(videoMode->vramPage2, offset);
			offset += screenPitch;
		}
	}
	else
	{
		// No interlacing
		int offset = 0;
		for (int y = 0; y < screenHeight; y++)
		{
			drawSurface->lines[y] = (uint8_t*)MK_FP(videoMode->vramPage1, offset);
			offset += screenPitch;
		}
	}
}

void BIOSVideoDriver::Shutdown()
{
	if (startingScreenMode != -1)
	{
		SetScreenMode(startingScreenMode);
	}
}

int BIOSVideoDriver::GetScreenMode()
{
	union REGS inreg, outreg;
	inreg.h.ah = 0xf;

	int86(0x10, &inreg, &outreg);

	return (int)outreg.h.al;
}

bool BIOSVideoDriver::SetScreenMode(int screenMode)
{
	union REGS inreg, outreg;
	inreg.h.ah = 0;
	inreg.h.al = (unsigned char)screenMode;

	int86(0x10, &inreg, &outreg);

	return GetScreenMode() == screenMode;
}



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
#include "../Image.h"
#include "EGA.h"
#include "../DataPack.h"
#include "../Interface.h"
#include "../Draw/Surf1bpp.h"
#include "../Draw/Surf4bpp.h"

#define EGA_BASE_VRAM_ADDRESS (uint8_t*) MK_FP(0xA000, 0)
#define BYTES_PER_LINE 80

EGADriver::EGADriver()
{
	SetupVars("EGA.DAT", 0x10, 350);
}

void EGADriver::SetupVars(const char* inAssetPackToUse, int inScreenMode, int inScreenHeight)
{
	assetPackToUse = inAssetPackToUse;
	screenModeToUse = inScreenMode;
	screenWidth = 640;
	screenHeight = inScreenHeight;
}

void EGADriver::Init()
{
	startingScreenMode = GetScreenMode();
	SetScreenMode(screenModeToUse);

	Assets.Load(assetPackToUse);

	/*
	DrawSurface_1BPP* drawSurface1BPP = new DrawSurface_1BPP(screenWidth, screenHeight);
	for (int y = 0; y < screenHeight; y++)
	{
		drawSurface1BPP->lines[y] = (EGA_BASE_VRAM_ADDRESS)+(BYTES_PER_LINE * y);
	}
	drawSurface = drawSurface1BPP;
	*/
	DrawSurface_4BPP* drawSurface4BPP = new DrawSurface_4BPP(screenWidth, screenHeight);
	for (int y = 0; y < screenHeight; y++)
	{
		drawSurface4BPP->lines[y] = (EGA_BASE_VRAM_ADDRESS)+(BYTES_PER_LINE * y);
	}
	drawSurface = drawSurface4BPP;
}

void EGADriver::Shutdown()
{
	SetScreenMode(startingScreenMode);
}

int EGADriver::GetScreenMode()
{
	union REGS inreg, outreg;
	inreg.h.ah = 0xf;

	int86(0x10, &inreg, &outreg);

	return (int)outreg.h.al;
}

void EGADriver::SetScreenMode(int screenMode)
{
	union REGS inreg, outreg;
	inreg.h.ah = 0;
	inreg.h.al = (unsigned char)screenMode;

	int86(0x10, &inreg, &outreg);
}

static void FastMemSet(void far* mem, uint8_t value, unsigned int count);
#pragma aux FastMemSet = \
	"rep stosb" \
	modify [di cx] \	
	parm[es di][al][cx];

void EGADriver::ScaleImageDimensions(int& width, int& height)
{
	// Scale to 4:3
	height = (height * 35) / 48;
}

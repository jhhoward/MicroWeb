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
#include "HP95LX.h"
#include "../Interface.h"
#include "../DataPack.h"
#include "../Draw/Surf1bpp.h"

#define USE_EGA_PREVIS 0

#if USE_EGA_PREVIS
#define BASE_VRAM_ADDRESS (uint8_t*) MK_FP(0xA000, 0)
#define BYTES_PER_LINE 80
#else
#define BASE_VRAM_ADDRESS (uint8_t*) MK_FP(0xB000, 0)
#define BYTES_PER_LINE 30
#endif

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 128

HP95LXVideoDriver::HP95LXVideoDriver()
{
	screenWidth = 240;
	screenHeight = 128;
}

void HP95LXVideoDriver::Init()
{
	startingScreenMode = GetScreenMode();
#if USE_EGA_PREVIS
	SetScreenMode(0x10);
#else
	SetScreenMode(0x20);
#endif

	Assets.Load("LOWRES.DAT");

	DrawSurface_1BPP* drawSurface1BPP = new DrawSurface_1BPP(screenWidth, screenHeight);
	for (int y = 0; y < screenHeight; y++)
	{
		drawSurface1BPP->lines[y] = (BASE_VRAM_ADDRESS)+(BYTES_PER_LINE * y);
	}
	drawSurface = drawSurface1BPP;
}

void HP95LXVideoDriver::Shutdown()
{
	SetScreenMode(startingScreenMode);
}

int HP95LXVideoDriver::GetScreenMode()
{
	union REGS inreg, outreg;
	inreg.h.ah = 0xf;

	int86(0x10, &inreg, &outreg);

	return (int)outreg.h.al;
}

void HP95LXVideoDriver::SetScreenMode(int screenMode)
{
	union REGS inreg, outreg;
	inreg.h.ah = 0;
	inreg.h.al = (unsigned char)screenMode;

	int86(0x10, &inreg, &outreg);
}

void HP95LXVideoDriver::ScaleImageDimensions(int& width, int& height)
{
}

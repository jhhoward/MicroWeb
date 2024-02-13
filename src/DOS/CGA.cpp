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
#include "CGA.h"
#include "../Interface.h"
#include "../DataPack.h"
#include "../Draw/Surf1bpp.h"

#define CGA_BASE_VRAM_ADDRESS (uint8_t*) MK_FP(0xB800, 0)
#define CGA_PAGE2_VRAM_ADDRESS (uint8_t*) MK_FP(0xBA00, 0)

#define BYTES_PER_LINE 80

CGADriver::CGADriver()
{
	screenWidth = 640;
	screenHeight = 200;
}

void CGADriver::Init()
{
	startingScreenMode = GetScreenMode();
	SetScreenMode(6);

	Assets.Load("CGA.DAT");

	DrawSurface_1BPP* drawSurface1BPP = new DrawSurface_1BPP(screenWidth, screenHeight);
	for (int y = 0; y < screenHeight; y += 2)
	{
		drawSurface1BPP->lines[y] = (CGA_BASE_VRAM_ADDRESS) + (40 * y);
		drawSurface1BPP->lines[y + 1] = (CGA_PAGE2_VRAM_ADDRESS) + (40 * y);
	}
	drawSurface = drawSurface1BPP;
}

void CGADriver::Shutdown()
{
	SetScreenMode(startingScreenMode);
}

int CGADriver::GetScreenMode()
{
	union REGS inreg, outreg;
	inreg.h.ah = 0xf;

	int86(0x10, &inreg, &outreg);

	return (int)outreg.h.al;
}

void CGADriver::SetScreenMode(int screenMode)
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

void CGADriver::ScaleImageDimensions(int& width, int& height)
{
	// Scale to 4:3
	height = (height * 5) / 12;
}

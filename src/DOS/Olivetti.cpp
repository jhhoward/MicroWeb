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
#include "Olivetti.h"
#include "../Interface.h"
#include "../DataPack.h"
#include "../Draw/Surf1bpp.h"

#define BASE_VRAM_ADDRESS (uint8_t*) MK_FP(0xB800, 0)

#define BYTES_PER_LINE 80

OlivettiDriver::OlivettiDriver(int inScreenModeToUse)
{
	screenModeToUse = inScreenModeToUse;
	screenWidth = 640;
	screenHeight = 400;
}

void OlivettiDriver::Init()
{
	startingScreenMode = GetScreenMode();
	SetScreenMode(screenModeToUse);

	Assets.Load("DEFAULT.DAT");

	DrawSurface_1BPP* drawSurface1BPP = new DrawSurface_1BPP(screenWidth, screenHeight);
	for (int y = 0; y < screenHeight; y += 4)
	{
		int offset = BYTES_PER_LINE * (y / 4);
		drawSurface1BPP->lines[y] = (BASE_VRAM_ADDRESS)+offset;
		drawSurface1BPP->lines[y + 1] = (BASE_VRAM_ADDRESS)+offset + 0x2000;
		drawSurface1BPP->lines[y + 2] = (BASE_VRAM_ADDRESS)+offset + 0x4000;
		drawSurface1BPP->lines[y + 3] = (BASE_VRAM_ADDRESS)+offset + 0x6000;
	}
	drawSurface = drawSurface1BPP;
}

void OlivettiDriver::Shutdown()
{
	SetScreenMode(startingScreenMode);
}

int OlivettiDriver::GetScreenMode()
{
	union REGS inreg, outreg;
	inreg.h.ah = 0xf;

	int86(0x10, &inreg, &outreg);

	return (int)outreg.h.al;
}

void OlivettiDriver::SetScreenMode(int screenMode)
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

void OlivettiDriver::ScaleImageDimensions(int& width, int& height)
{
	// Scale to 4:3
	height = (height * 5) / 12;
}

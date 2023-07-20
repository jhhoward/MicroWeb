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
#include "Hercules.h"
#include "../DataPack.h"
#include "../Interface.h"
#include "../Draw/Surf1bpp.h"

#define BASE_VRAM_ADDRESS (uint8_t*) MK_FP(0xB000, 0)

#define BYTES_PER_LINE 90

HerculesDriver::HerculesDriver()
{
	screenWidth = 720;
	screenHeight = 348;
}

// graphics mode CRTC register values
static uint8_t graphicsModeCRTC[] = { 0x35, 0x2d, 0x2e, 0x07, 0x5b, 0x02, 0x57, 0x57, 0x02, 0x03, 0x00, 0x00 };

// text mode CRTC register values
static uint8_t textModeCRTC[] = { 0x61, 0x50, 0x52, 0x0f, 0x19, 0x06, 0x19, 0x19, 0x02, 0x0d, 0x0b, 0x0c };


void HerculesDriver::Init()
{
	SetGraphicsMode();

	Assets.Load("EGA.DAT");

	DrawSurface_1BPP* drawSurface1BPP = new DrawSurface_1BPP(screenWidth, screenHeight);
	for (int y = 0; y < screenHeight; y += 4)
	{
		int offset = BYTES_PER_LINE * (y / 4);
		drawSurface1BPP->lines[y] = (BASE_VRAM_ADDRESS) + offset;
		drawSurface1BPP->lines[y + 1] = (BASE_VRAM_ADDRESS) + offset + 0x2000;
		drawSurface1BPP->lines[y + 2] = (BASE_VRAM_ADDRESS) + offset + 0x4000;
		drawSurface1BPP->lines[y + 3] = (BASE_VRAM_ADDRESS) + offset + 0x6000;
	}
	drawSurface = drawSurface1BPP;
}

void HerculesDriver::Shutdown()
{
	SetTextMode();
}

void HerculesDriver::SetGraphicsMode()
{
	outp(0x03BF, 0x03);

	for (int i = 0; i < sizeof(graphicsModeCRTC); i++)
	{
		outp(0x03B4, i);
		outp(0x03B5, graphicsModeCRTC[i]);
	}

	outp(0x03B8, 0x0a);
}

static void FastMemSet(void far* mem, uint8_t value, unsigned int count);
#pragma aux FastMemSet = \
	"rep stosb" \
	modify [di cx] \	
	parm[es di][al][cx];

void HerculesDriver::SetTextMode()
{
	outp(0x03BF, 0x3);

	for (int i = 0; i < sizeof(textModeCRTC); i++)
	{
		outp(0x03B4, i);
		outp(0x03B5, textModeCRTC[i]);
	}

	outp(0x03B8, 0x08);
	FastMemSet(BASE_VRAM_ADDRESS, 0, 0x4000);
}

void HerculesDriver::ScaleImageDimensions(int& width, int& height)
{
	// Scale to 4:3
	height = (height * 29) / 45;
}

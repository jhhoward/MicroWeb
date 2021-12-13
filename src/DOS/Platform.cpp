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

#include <i86.h>
#include "../Platform.h"
#include "CGA.h"
#include "EGA.h"
#include "Hercules.h"
#include "TextMode.h"
#include "DOSInput.h"
#include "DOSNet.h"

#include "../Image.h"
#include "../Cursor.h"
#include "../Font.h"
#include "DefData.inc"

static DOSInputDriver DOSinput;
static DOSNetworkDriver DOSNet;

VideoDriver* Platform::video = NULL;
InputDriver* Platform::input = &DOSinput;
NetworkDriver* Platform::network = &DOSNet;

/*
	Find6845
	
	This routine detects the presence of the CRTC on a MDA, CGA or HGC.
	The technique is to write and read register 0Fh of the chip (cursor
	low). If the same value is read as written, assume the chip is
	present at the specified port addr.
*/
bool Find6845(int port);
#pragma aux Find6845 = \
	"mov al, 0xF" \
	"out dx, al" \
	"inc dx" \
	"in al, dx" \
	"mov ah, al" \
	"mov al, 0x66" \
	"out dx, al" \
	"mov cx, 0x100" \
	"W1:" \
	"loop W1" \
	"in al, dx" \
	"xchg ah, al" \
	"out dx, al" \
	"mov al, 1" \
	"cmp ah, 0x66" \
	"je found" \
	"mov al, 0" \
	"found:" \
	parm [dx] \
	modify [ax dx cx] \
	value [al]

/*
	Distinguishes between an MDA
	and a Hercules adapter by monitoring bit 7 of the CRT Status byte.
	This bit changes on Hercules adapters but does not change on an MDA.
*/
bool DetectHercules();
#pragma aux DetectHercules = \
	"mov dl, 0xba" \
	"in al, dx" \
	"and al, 0x80" \
	"mov ah, al" \
	"mov cx, 0x8000" \
	"L1:" \
	"in al, dx" \
	"and al, 0x80" \
	"cmp ah, al" \
	"loope L1" \
	"mov al, 1" \
	"jne found" \
	"mov al, 0" \
	"found:" \
	modify [ax cx dx] \
	value [al]

static void AutoDetectVideoDriver()
{
	union REGS inreg, outreg;

	// First try detect presence of VGA card
	inreg.x.ax = 0x1200;		
	inreg.h.bl = 0x32;
	int86(0x10, &inreg, &outreg);

	if (outreg.h.al == 0x12)
	{
		Platform::video = new VGADriver();
		return;
	}

	// Attempt to detect EGA card
	inreg.x.ax = 0x1200;
	inreg.h.bl = 0x10;
	int86(0x10, &inreg, &outreg);

	if (outreg.h.bl < 4)
	{
		Platform::video = new EGADriver();
		return;
	}

	bool isMono = Find6845(0x3b4);
	// Attempt to detect Hercules
	if (isMono && DetectHercules())
	{
		Platform::video = new HerculesDriver();
		return;
	}

	// Attempt to detect CGA
	if (Find6845(0x3d4))
	{
		Platform::video = new CGADriver();
		return;
	}

	if (isMono)
	{
		Platform::video = new MDATextModeDriver();
		return;
		//fprintf(stderr, "MDA cards not supported!\n");
	}
	else
	{
		fprintf(stderr, "Could not detect video card!\n");
	}
	exit(1);
}

void Platform::Init(int argc, char* argv[])
{
	bool inverse = false;

	for (int n = 1; n < argc; n++)
	{
		if (!stricmp(argv[n], "-v") && !video)
		{
			video = new VGADriver();
		}
		if (!stricmp(argv[n], "-e") && !video)
		{
			video = new EGADriver();
		}
		if (!stricmp(argv[n], "-c") && !video)
		{
			video = new CGADriver();
		}
		if (!stricmp(argv[n], "-h") && !video)
		{
			video = new HerculesDriver();
		}
		if (!stricmp(argv[n], "-t") && !video)
		{
			video = new CGATextModeDriver();
		}
		if (!stricmp(argv[n], "-m") && !video)
		{
			video = new MDATextModeDriver();
		}
		if (!stricmp(argv[n], "-i"))
		{
			inverse = true;
		}
	}

	if (!video)
	{
		AutoDetectVideoDriver();
	}

	network->Init();
	video->Init();
	video->ClearScreen();
	input->Init();

	if (inverse)
	{
		video->InvertScreen();
	}
}

void Platform::Shutdown()
{
	input->Shutdown();
	video->Shutdown();
	network->Shutdown();

	delete video;
}

void Platform::Update()
{
	network->Update();
	input->Update();
}

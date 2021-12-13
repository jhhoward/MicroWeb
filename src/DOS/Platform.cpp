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
#ifdef HP95LX
#include "HP95LX.h"
#else
#include "CGA.h"
#include "EGA.h"
#include "Hercules.h"
#include "TextMode.h"
#endif
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
#ifdef HP95LX
	inreg.x.ax = 0x4dd4;
	int86(0x15, &inreg, &outreg);
	if (outreg.x.bx == 0x4850)
	{
		Platform::video = new HP95LXVideoDriver();
		printf("Detected HP 95LX\n");
		return;
	}

	fprintf(stderr, "This build only works with the HP 95LX, 100LX and 200LX\n");
	exit(1);
#else
	// First try detect presence of VGA card
	inreg.x.ax = 0x1200;		
	inreg.h.bl = 0x32;
	int86(0x10, &inreg, &outreg);

	if (outreg.h.al == 0x12)
	{
		Platform::video = new VGADriver();
		printf("Detected VGA\n");
		return;
	}

	// Attempt to detect EGA card
	inreg.x.ax = 0x1200;
	inreg.h.bl = 0x10;
	int86(0x10, &inreg, &outreg);

	if (outreg.h.bl < 4)
	{
		Platform::video = new EGADriver();
		printf("Detected EGA\n");
		return;
	}

	// Attempt to detect CGA
	// Note we are preferring CGA over Hercules because some CGA devices are errorneously reporting Hercules support
	if (Find6845(0x3d4))
	{
		Platform::video = new CGADriver();
		printf("Detected CGA\n");
		return;
	}

	bool isMono = Find6845(0x3b4);
	
	// Attempt to detect Hercules
	if (isMono && DetectHercules())
	{
		Platform::video = new HerculesDriver();
		printf("Detected Hercules\n");
		return;
	}

	if (isMono)
	{
		Platform::video = new MDATextModeDriver();
		printf("Detected MDA\n");
		return;
		//fprintf(stderr, "MDA cards not supported!\n");
	}
	else
	{
		fprintf(stderr, "Could not detect video card!\n");
	}
	exit(1);
#endif
}

void Platform::Init(int argc, char* argv[])
{
	bool inverse = false;
	video = NULL;

	for (int n = 1; n < argc; n++)
	{
#ifndef HP95LX
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
#endif
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

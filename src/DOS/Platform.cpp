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
#include <stdarg.h>
#include "../Platform.h"
#include "Hercules.h"
#include "../VidModes.h"
#include "BIOSVid.h"
#include "DOSInput.h"
#include "DOSNet.h"

#include "../Image/Image.h"
#include "../Cursor.h"
#include "../Font.h"
#include "../Draw/Surface.h"
#include "../Memory/Memory.h"
#include "../App.h"

VideoDriver* Platform::video = nullptr;
InputDriver* Platform::input = nullptr;
NetworkDriver* Platform::network = nullptr;

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

static const int HP95LX = 13;
static const int Hercules = 10;
static const int CGA = 0;
static const int CGAPalmtop = 1;
static const int EGA = 6;
static const int VGA = 8;

static int AutoDetectVideoMode()
{
	union REGS inreg, outreg;

	// Look for HP 95LX
	inreg.x.ax = 0x4dd4;
	int86(0x15, &inreg, &outreg);
	if (outreg.x.bx == 0x4850)
	{
		if (outreg.x.cx == 0x0101)
		{
			return HP95LX;
		}
		else if (outreg.x.cx == 0x0102)
		{
			return CGAPalmtop;
		}
	}

	// First try detect presence of VGA card
	inreg.x.ax = 0x1200;		
	inreg.h.bl = 0x32;
	int86(0x10, &inreg, &outreg);
	if (outreg.h.al == 0x12)
	{
		return VGA;
	}

	// Attempt to detect EGA card
	inreg.x.ax = 0x1200;
	inreg.h.bl = 0x10;
	int86(0x10, &inreg, &outreg);

	if (outreg.h.bl < 4)
	{
		return EGA;
	}

	// Attempt to detect CGA
	// Note we are preferring CGA over Hercules because some CGA devices are errorneously reporting Hercules support
	if (Find6845(0x3d4))
	{
		return CGA;
	}

	bool isMono = Find6845(0x3b4);
	
	// Attempt to detect Hercules
	if (isMono && DetectHercules())
	{
		return Hercules;
	}

	return CGA;
}

#include "../Node.h"
#include <stdio.h>
#include <ctype.h>

bool Platform::Init(int argc, char* argv[])
{
	network = new DOSNetworkDriver();
	if (network)
	{
		network->Init();
	}
	else FatalError("Could not create network driver");

	VideoModeInfo* videoMode = nullptr;

	// Check for video mode argument
	for (int n = 1; n < argc; n++)
	{
		if (strstr(argv[n], "-video=") == argv[n])
		{
			int chosenMode = tolower(argv[n][7]) - 'a';
			if (chosenMode >= 0 && chosenMode < GetNumVideoModes())
			{
				videoMode = &VideoModeList[chosenMode];
			}
			break;
		}
	}

	if (!videoMode)
	{
		int suggestedMode = AutoDetectVideoMode();
		videoMode = ShowVideoModePicker(suggestedMode);
	}

	if (!videoMode)
	{
		return false;
	}

	if (videoMode == &VideoModeList[HP95LX] || videoMode == &VideoModeList[CGAPalmtop])
	{
		App::config.invertScreen = true;
	}

	if (videoMode->biosVideoMode == HERCULES_MODE)
	{
		video = new HerculesDriver();
	}
	else
	{
		video = new BIOSVideoDriver();
	}

	if (!video)
	{
		FatalError("Could not create video driver");
	}

	video->Init(videoMode);

	input = new DOSInputDriver();
	if (input)
	{
		input->Init();
	}
	else FatalError("Could not create input driver");

	return true;
}

void Platform::Shutdown()
{
	MemoryManager::pageBlockAllocator.Shutdown();
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

void Platform::FatalError(const char* message, ...)
{
	va_list args;

	if (video)
	{
		video->Shutdown();
	}

	va_start(args, message);
	vfprintf(stderr, message, args);
	printf("\n");
	va_end(args);

	MemoryManager::pageBlockAllocator.Shutdown();

	exit(1);
}


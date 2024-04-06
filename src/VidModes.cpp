#include <stdio.h>
#include <ctype.h>
#include "VidModes.h"

VideoModeInfo VideoModeList[] =
{
	// name												mode			width		height		surfaceFormat						aspect %	zoom %		data pack			vram1		vram2		vram3		vram4
	{ "640x200 monochrome (CGA)",						6,				640,		200,		DrawSurface::Format_1BPP,			240,		100,		DataPack::CGA,		0xb800,		0xba00							 },
	{ "640x200 inverse monochrome (Palmtop CGA)",		6,				640,		200,		DrawSurface::Format_1BPP,			100,		100,		DataPack::Default,	0xb800,		0xba00							 },
	{ "320x200 4 colours (CGA)",						5,				320,		200,		DrawSurface::Format_2BPP,			120,		70,			DataPack::Lowres,	0xb800,		0xba00							 },
	{ "320x200 16 colours (Composite CGA)",				4,				320,		200,		DrawSurface::Format_2BPP,			120,		70,			DataPack::CGA,		0xb800,		0xba00							 },
	{ "640x200 16 colours (EGA)",						0xe,			640,		200,		DrawSurface::Format_4BPP_EGA,		240,		100,		DataPack::CGA,		0xa000,										 },
	{ "640x350 monochrome (EGA)",						0xf,			640,		350,		DrawSurface::Format_1BPP,			137,		100,		DataPack::EGA,		0xa000,										 },
	{ "640x350 16 colours (EGA)",						0x10,			640,		350,		DrawSurface::Format_4BPP_EGA,		137,		100,		DataPack::EGA,		0xa000,										 },
	{ "640x480 monochrome (VGA)",						0x11,			640,		480,		DrawSurface::Format_1BPP,			100,		100,		DataPack::Default,	0xa000,										 },
	{ "640x480 16 colours (VGA)",						0x12,			640,		480,		DrawSurface::Format_4BPP_EGA,		100,		100,		DataPack::Default,	0xa000,										 },
	{ "320x200 256 colours (VGA)",						0x13,			320,		200,		DrawSurface::Format_8BPP,			120,		70,			DataPack::Lowres,	0xa000,										 },
	{ "720x348 monochrome (Hercules)",					HERCULES_MODE,	720,		348,		DrawSurface::Format_1BPP,			155,		100,		DataPack::EGA,		0xb000,		0xb200,		0xb400,		0xb600   },
	{ "640x400 monochrome (Olivetti M24)",				0x40,			640,		400,		DrawSurface::Format_1BPP,			100,		100,		DataPack::Default,	0xb800,		0xba00,		0xbc00,		0xbe00   },
	{ "640x400 monochrome (Toshiba T3100)",				0x74,			640,		400,		DrawSurface::Format_1BPP,			100,		100,		DataPack::Default,	0xb800,		0xba00,		0xbc00,		0xbe00   },
	{ "240x128 monochrome (HP 95LX)",					0x20,			240,		128,		DrawSurface::Format_1BPP,			100,		50,			DataPack::Lowres,	0xb000,										 },
	{ "640x200 16 colours (Amstrad PC1512)",			6,				640,		200,		DrawSurface::Format_4BPP_PC1512,	240,		100,		DataPack::CGA,		0xb800,		0xba00							 },
	{ nullptr }
};

VideoModeInfo* ShowVideoModePicker(int defaultSelection)
{
	int n = 0;

	printf("Pick a video mode:\n");
	while (VideoModeList[n].name)
	{
		printf("(%c) %s\n", 'a' + n, VideoModeList[n].name);
		n++;
	}

	printf("? %c\b", 'a' + defaultSelection);
	
	int numModes = n;
	char selection = getchar();

	if (selection == 13 || selection == 10)
	{
		return &VideoModeList[defaultSelection];
	}

	selection = tolower(selection);
	int selectionIndex = selection - 'a';

	if (selectionIndex >= 0 && selectionIndex < numModes)
	{
		return &VideoModeList[selectionIndex];
	}

	return nullptr;
}

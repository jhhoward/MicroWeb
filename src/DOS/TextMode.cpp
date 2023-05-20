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
#include "TextMode.h"
#include "TextData.inc"
#include "../Interface.h"

#define NAVIGATION_BUTTON_WIDTH 3
#define NAVIGATION_BUTTON_HEIGHT 1

#define BACK_BUTTON_X 4
#define FORWARD_BUTTON_X 6

#define ADDRESS_BAR_X 7
#define ADDRESS_BAR_Y 1
#define ADDRESS_BAR_WIDTH 64
#define ADDRESS_BAR_HEIGHT 1
#define TITLE_BAR_HEIGHT 1
#define STATUS_BAR_HEIGHT 1
#define STATUS_BAR_Y (screenHeight - STATUS_BAR_HEIGHT)

#define WINDOW_TOP 3
#define WINDOW_HEIGHT (screenHeight - WINDOW_TOP - STATUS_BAR_HEIGHT)
#define WINDOW_BOTTOM (WINDOW_TOP + WINDOW_HEIGHT)

#define SCROLL_BAR_WIDTH 1

//#define WINDOW_VRAM_TOP_EVEN (BYTES_PER_LINE * (WINDOW_TOP / 2))
//#define WINDOW_VRAM_TOP_ODD (0x2000 + BYTES_PER_LINE * (WINDOW_TOP / 2))
//#define WINDOW_VRAM_BOTTOM_EVEN (BYTES_PER_LINE * (WINDOW_BOTTOM / 2))
//#define WINDOW_VRAM_BOTTOM_ODD (0x2000 + BYTES_PER_LINE * (WINDOW_BOTTOM / 2))
#define BYTES_PER_LINE 160

static Image dummyImage =
{
	0, 0, NULL
};

TextModeDriver::TextModeDriver(int inScreenMode, uint8_t* inVideoBaseAddress, uint8_t* inTextAttributeMap)
{
	screenWidth = 80;
	screenHeight = 25;
	windowWidth = 79;
	windowHeight = WINDOW_HEIGHT;
	windowX = 0;
	windowY = WINDOW_TOP;
	scissorX1 = 0;
	scissorY1 = 0;
	scissorX2 = screenWidth - SCROLL_BAR_WIDTH;
	scissorY2 = screenHeight;
	invertScreen = false;
	clearMask = invertScreen ? 0 : 0xffff;

	videoBaseAddress = inVideoBaseAddress;
	textAttributeMap = inTextAttributeMap;
	screenMode = inScreenMode;

	imageIcon = &dummyImage;
	bulletImage = &dummyImage;
	isTextMode = true;
	//imageIcon = &CGA_ImageIcon;
	//bulletImage = &CGA_Bullet;
}

void TextModeDriver::Init()
{
	startingScreenMode = GetScreenMode();
	SetScreenMode(screenMode);

	bool disableBlinking = true;
	if (disableBlinking)
	{
		// For CGA cards
		outp(0x3D8, 9);

		// For EGA and above
		union REGS inreg, outreg;
		inreg.h.ah = 0x10;
		inreg.h.al = 0x3;
		inreg.h.bl = 0x0;
		int86(0x10, &inreg, &outreg);
	}
}

void TextModeDriver::Shutdown()
{
	SetScreenMode(startingScreenMode);
}

int TextModeDriver::GetScreenMode()
{
	union REGS inreg, outreg;
	inreg.h.ah = 0xf;

	int86(0x10, &inreg, &outreg);

	return (int)outreg.h.al;
}

void TextModeDriver::SetScreenMode(int screenMode)
{
	union REGS inreg, outreg;
	inreg.h.ah = 0;
	inreg.h.al = (unsigned char)screenMode;

	int86(0x10, &inreg, &outreg);
}

static void FastMemSet(void far* mem, uint16_t value, unsigned int count);
#pragma aux FastMemSet = \
	"rep stosw" \
	modify [di cx] \	
	parm[es di][ax][cx];

void TextModeDriver::InvertScreen()
{
	int count = 0x1000;
	unsigned char far* VRAM = videoBaseAddress;
	while (count--)
	{
		*VRAM ^= 0xff;
		VRAM += 2;
	}

	invertScreen = !invertScreen;
	clearMask = invertScreen ? 0 : 0xffff;
}

void TextModeDriver::ClearScreen()
{
	uint16_t clearValue = textAttributeMap[FontStyle::Regular] << 8;
	FastMemSet(videoBaseAddress, clearValue, 0x1000);
	// White out main page
	//FastMemSet(CGA_BASE_VRAM_ADDRESS + BYTES_PER_LINE * TITLE_BAR_HEIGHT / 2, 0xff, BYTES_PER_LINE * (SCREEN_HEIGHT - TITLE_BAR_HEIGHT - STATUS_BAR_HEIGHT) / 2);
	//FastMemSet(CGA_BASE_VRAM_ADDRESS + 0x2000 + BYTES_PER_LINE * TITLE_BAR_HEIGHT / 2, 0xff, BYTES_PER_LINE * (SCREEN_HEIGHT - TITLE_BAR_HEIGHT - STATUS_BAR_HEIGHT) / 2);

	FastMemSet(videoBaseAddress + (BYTES_PER_LINE * 2), clearValue | 0xc4, 80);
	FastMemSet(videoBaseAddress + (BYTES_PER_LINE * 24), 0x0f00, 80);
}

void TextModeDriver::DrawImage(Image* image, int x, int y)
{
}

void TextModeDriver::DrawString(const char* text, int x, int y, int size, FontStyle::Type style)
{
	if (y < scissorY1 || y >= scissorY2)
		return;

	unsigned char far* VRAM = videoBaseAddress;
	uint8_t attribute = textAttributeMap[style];

	VRAM += (y * screenWidth + x) * 2;

	while (*text && x < screenWidth)
	{
		*VRAM++ = *text;
		*VRAM++ = attribute;
		text++;
		x++;
	}
}

Font* TextModeDriver::GetFont(int fontSize, FontStyle::Type style)
{
	return &TextMode_Font;
}

void TextModeDriver::HLine(int x, int y, int count)
{
}

void TextModeDriver::ClearRect(int x, int y, int width, int height)
{
	if (!ApplyScissor(y, height))
		return;

	uint8_t* VRAM = videoBaseAddress + (y * screenWidth + x) * 2;
	uint16_t clearValue = textAttributeMap[FontStyle::Regular] << 8;

	while (height)
	{
		FastMemSet(VRAM, clearValue, width);
		VRAM += BYTES_PER_LINE;
		height--;
	}
}

bool TextModeDriver::ApplyScissor(int& y, int& height)
{
	if (y + height < scissorY1)
		return false;
	if (y >= scissorY2)
		return false;
	if (y < scissorY1)
	{
		height -= (scissorY1 - y);
		y = scissorY1;
	}
	if (y + height >= scissorY2)
	{
		height = scissorY2 - y;
	}
	return true;
}

void TextModeDriver::InvertRect(int x, int y, int width, int height)
{
	if (!ApplyScissor(y, height))
		return;
}

void TextModeDriver::FillRect(int x, int y, int width, int height)
{
}

void TextModeDriver::VLine(int x, int y, int count)
{
}

MouseCursorData* TextModeDriver::GetCursorGraphic(MouseCursor::Type type)
{
	return NULL;
}

int TextModeDriver::GetGlyphWidth(char c, int fontSize, FontStyle::Type style)
{
	return 1;
}

int TextModeDriver::GetLineHeight(int fontSize, FontStyle::Type style)
{
	return 1;
}

void TextModeDriver::DrawScrollBar(int position, int size)
{
	uint8_t* VRAM = videoBaseAddress;
	VRAM += (BYTES_PER_LINE * WINDOW_TOP) + (screenWidth - 1) * 2;

	for (int y = 0; y < WINDOW_HEIGHT; y++)
	{
		if (y >= position && y <= position + size)
		{
			*VRAM = '\xdb';
		}
		else
		{
			*VRAM = '\xb1';
		}		
		VRAM += BYTES_PER_LINE;
	}
}

void TextModeDriver::DrawRect(int x, int y, int width, int height)
{
}

void TextModeDriver::DrawButtonRect(int x, int y, int width, int height)
{
}

void TextModeDriver::ScrollWindow(int amount)
{
}

void TextModeDriver::ClearWindow()
{
	uint8_t* VRAM = videoBaseAddress;
	VRAM += (BYTES_PER_LINE * WINDOW_TOP);
	uint16_t clearValue = textAttributeMap[FontStyle::Regular] << 8;

	for (int y = 0; y < WINDOW_HEIGHT; y++)
	{
		FastMemSet(VRAM, clearValue, BYTES_PER_LINE - 2);
		VRAM += BYTES_PER_LINE;
	}
}

void TextModeDriver::SetScissorRegion(int y1, int y2)
{
	scissorY1 = y1;
	scissorY2 = y2;
}

void TextModeDriver::ClearScissorRegion()
{
	scissorY1 = 0;
	scissorY2 = screenHeight;
}

void TextModeDriver::ArrangeAppInterfaceWidgets(AppInterface& app)
{
	app.addressBar.x = ADDRESS_BAR_X;
	app.addressBar.y = ADDRESS_BAR_Y;
	app.addressBar.width = ADDRESS_BAR_WIDTH;
	app.addressBar.height = ADDRESS_BAR_HEIGHT;

	app.scrollBar.x = screenWidth - SCROLL_BAR_WIDTH;
	app.scrollBar.y = WINDOW_TOP;
	app.scrollBar.width = SCROLL_BAR_WIDTH;
	app.scrollBar.height = WINDOW_HEIGHT;

	app.backButton.x = BACK_BUTTON_X;
	app.backButton.y = ADDRESS_BAR_Y;
	app.backButton.width = NAVIGATION_BUTTON_WIDTH;
	app.backButton.height = NAVIGATION_BUTTON_HEIGHT;

	app.forwardButton.x = FORWARD_BUTTON_X;
	app.forwardButton.y = ADDRESS_BAR_Y;
	app.forwardButton.width = NAVIGATION_BUTTON_WIDTH;
	app.forwardButton.height = NAVIGATION_BUTTON_HEIGHT;

	app.statusBar.x = 0;
	app.statusBar.y = screenHeight - STATUS_BAR_HEIGHT;
	app.statusBar.width = screenWidth;
	app.statusBar.height = STATUS_BAR_HEIGHT;

	app.titleBar.x = 0;
	app.titleBar.y = 0;
	app.titleBar.width = screenWidth;
	app.titleBar.height = TITLE_BAR_HEIGHT;
}

void TextModeDriver::ScaleImageDimensions(int& width, int& height)
{
	width >>= 3;
	height >>= 4;
}

#define MDA_BASE_VRAM_ADDRESS (uint8_t*) MK_FP(0xB000, 0)

uint8_t MDATextModeDriver::mdaAttributeMap[16] =
{
	0x7, 0xa, 0xa, 0xa, 0x1, 0x9, 0x9, 0x9, 
	0x7, 0xa, 0xa, 0xa, 0x1, 0x9, 0x9, 0x9,
};

MDATextModeDriver::MDATextModeDriver() : TextModeDriver(7, MDA_BASE_VRAM_ADDRESS, mdaAttributeMap)
{
}

#define CGA_BASE_VRAM_ADDRESS (uint8_t*) MK_FP(0xB800, 0)

uint8_t CGATextModeDriver::cgaAttributeMap[16] =
{
	0x07, 0x0f, 0x0e, 0x07, 0x09, 0x07, 0x07, 0x07,
	0x07, 0x0f, 0x0e, 0x07, 0x09, 0x07, 0x07, 0x07,
};

CGATextModeDriver::CGATextModeDriver() : TextModeDriver(3, CGA_BASE_VRAM_ADDRESS, cgaAttributeMap)
{
}

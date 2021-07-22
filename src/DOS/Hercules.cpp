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
#include "DefData.h"
#include "../Interface.h"

#define BASE_VRAM_ADDRESS (uint8_t*) MK_FP(0xB000, 0)

#define SCREEN_WIDTH 720
#define SCREEN_HEIGHT 348

#define BACK_BUTTON_X 4
#define FORWARD_BUTTON_X 32

#define ADDRESS_BAR_X 60
#define ADDRESS_BAR_Y 12
#define ADDRESS_BAR_WIDTH (SCREEN_WIDTH - 64)
#define ADDRESS_BAR_HEIGHT 14
#define TITLE_BAR_HEIGHT 11
#define STATUS_BAR_HEIGHT 12
#define STATUS_BAR_Y (SCREEN_HEIGHT - STATUS_BAR_HEIGHT)

#define NAVIGATION_BUTTON_WIDTH 24
#define NAVIGATION_BUTTON_HEIGHT ADDRESS_BAR_HEIGHT

#define WINDOW_TOP 28
#define WINDOW_HEIGHT (SCREEN_HEIGHT - WINDOW_TOP - STATUS_BAR_HEIGHT)
#define WINDOW_BOTTOM (WINDOW_TOP + WINDOW_HEIGHT)

#define SCROLL_BAR_WIDTH 16

#define WINDOW_VRAM_TOP_PAGE1 (BYTES_PER_LINE * (WINDOW_TOP / 4))
#define WINDOW_VRAM_TOP_PAGE2 (0x2000 + BYTES_PER_LINE * (WINDOW_TOP / 4))
#define WINDOW_VRAM_TOP_PAGE3 (0x4000 + BYTES_PER_LINE * (WINDOW_TOP / 4))
#define WINDOW_VRAM_TOP_PAGE4 (0x6000 + BYTES_PER_LINE * (WINDOW_TOP / 4))

#define WINDOW_VRAM_BOTTOM_PAGE1 (BYTES_PER_LINE * (WINDOW_BOTTOM / 4))
#define WINDOW_VRAM_BOTTOM_PAGE2 (0x2000 + BYTES_PER_LINE * (WINDOW_BOTTOM / 4))
#define WINDOW_VRAM_BOTTOM_PAGE3 (0x4000 + BYTES_PER_LINE * (WINDOW_BOTTOM / 4))
#define WINDOW_VRAM_BOTTOM_PAGE4 (0x6000 + BYTES_PER_LINE * (WINDOW_BOTTOM / 4))

#define WINDOW_VRAM_TOP_EVEN (BYTES_PER_LINE * (WINDOW_TOP / 2))
#define WINDOW_VRAM_TOP_ODD (0x2000 + BYTES_PER_LINE * (WINDOW_TOP / 2))
#define WINDOW_VRAM_BOTTOM_EVEN (BYTES_PER_LINE * (WINDOW_BOTTOM / 2))
#define WINDOW_VRAM_BOTTOM_ODD (0x2000 + BYTES_PER_LINE * (WINDOW_BOTTOM / 2))
#define BYTES_PER_LINE 90

HerculesDriver::HerculesDriver()
{
	screenWidth = SCREEN_WIDTH;
	screenHeight = SCREEN_HEIGHT;
	windowWidth = screenWidth - 16;
	windowHeight = WINDOW_HEIGHT;
	windowX = 0;
	windowY = WINDOW_TOP;
	scissorX1 = 0;
	scissorY1 = 0;
	scissorX2 = SCREEN_WIDTH - SCROLL_BAR_WIDTH;
	scissorY2 = SCREEN_HEIGHT;
	invertScreen = false;
	clearMask = invertScreen ? 0 : 0xffff;
	imageIcon = &Default_ImageIcon;
	bulletImage = &Default_Bullet;
}

// graphics mode CRTC register values
static uint8_t graphicsModeCRTC[] = { 0x35, 0x2d, 0x2e, 0x07, 0x5b, 0x02, 0x57, 0x57, 0x02, 0x03, 0x00, 0x00 };

// text mode CRTC register values
static uint8_t textModeCRTC[] = { 0x61, 0x50, 0x52, 0x0f, 0x19, 0x06, 0x19, 0x19, 0x02, 0x0d, 0x0b, 0x0c };


void HerculesDriver::Init()
{
	SetGraphicsMode();
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

void HerculesDriver::InvertScreen()
{
	int count = 0x8000;
	unsigned char far* VRAM = BASE_VRAM_ADDRESS;
	while (count--)
	{
		*VRAM ^= 0xff;
		VRAM++;
	}

	invertScreen = !invertScreen;
	clearMask = invertScreen ? 0 : 0xffff;
}

void HerculesDriver::ClearScreen()
{
	uint8_t clearValue = (uint8_t)(clearMask & 0xff);
	FastMemSet(BASE_VRAM_ADDRESS, clearValue, 0x8000);
	// White out main page
	//FastMemSet(CGA_BASE_VRAM_ADDRESS + BYTES_PER_LINE * TITLE_BAR_HEIGHT / 2, 0xff, BYTES_PER_LINE * (SCREEN_HEIGHT - TITLE_BAR_HEIGHT - STATUS_BAR_HEIGHT) / 2);
	//FastMemSet(CGA_BASE_VRAM_ADDRESS + 0x2000 + BYTES_PER_LINE * TITLE_BAR_HEIGHT / 2, 0xff, BYTES_PER_LINE * (SCREEN_HEIGHT - TITLE_BAR_HEIGHT - STATUS_BAR_HEIGHT) / 2);
}

void HerculesDriver::DrawImage(Image* image, int x, int y)
{
	int imageHeight = image->height;

	if (x >= scissorX2)
	{
		return;
	}
	if (y >= scissorY2)
	{
		return;
	}
	if (y + imageHeight < scissorY1)
	{
		return;
	}
	if (y + imageHeight > scissorY2)
	{
		imageHeight = (scissorY2 - y);
	}

	uint16_t firstLine = 0;
	if (y < scissorY1)
	{
		firstLine += scissorY1 - y;
		y += firstLine;
	}

	uint8_t far* VRAM = (uint8_t far*) BASE_VRAM_ADDRESS;
	VRAM += (y >> 2) * BYTES_PER_LINE;

	uint16_t imageWidthBytes = image->width >> 3;

	// Check if rounds to a byte, pad if necessary
	if ((imageWidthBytes << 3) != image->width)
	{
		imageWidthBytes++;
	}

	uint8_t* imageData = image->data + firstLine * imageWidthBytes;
	uint8_t far* VRAMptr = VRAM + (x >> 3);

	int interlace = (y & 3);
	VRAMptr += 0x2000 * interlace;

	for (uint8_t j = firstLine; j < imageHeight; j++)
	{
		uint8_t writeOffset = (uint8_t)(x) & 0x7;

		for (uint8_t i = 0; i < imageWidthBytes; i++)
		{
			uint8_t glyphPixels = *imageData++;

			VRAMptr[i] ^= (glyphPixels >> writeOffset);
			VRAMptr[i + 1] ^= (glyphPixels << (8 - writeOffset));
		}


		interlace++;
		if (interlace == 4)
		{
			VRAMptr -= (0x6000 - BYTES_PER_LINE);
			interlace = 0;
		}
		else
		{
			VRAMptr += 0x2000;
		}
	}
}

void HerculesDriver::DrawString(const char* text, int x, int y, int size, FontStyle::Type style)
{
	Font* font = GetFont(size, style);

	int startX = x;
	uint8_t glyphHeight = font->glyphHeight;
	if (x >= scissorX2)
	{
		return;
	}
	if (y >= scissorY2)
	{
		return;
	}
	if (y + glyphHeight > scissorY2)
	{
		glyphHeight = (uint8_t)(scissorY2 - y);
	}
	if (y + glyphHeight < scissorY1)
	{
		return;
	}

	uint8_t firstLine = 0;
	if (y < scissorY1)
	{
		firstLine += scissorY1 - y;
		y += firstLine;
	}

	uint8_t far* VRAM = (uint8_t far*) BASE_VRAM_ADDRESS;

	VRAM += (y >> 2) * BYTES_PER_LINE;
	int interlace = (y & 3);
	VRAM += 0x2000 * interlace;

	while (*text)
	{
		char c = *text++;
		if (c < 32 || c >= 128)
		{
			continue;
		}

		char index = c - 32;
		uint8_t glyphWidth = font->glyphWidth[index];

		if (glyphWidth == 0)
		{
			continue;
		}

		uint8_t* glyphData = font->glyphData + (font->glyphDataStride * index);
		
		glyphData += (firstLine * font->glyphWidthBytes);

		interlace = (y & 3);
		uint8_t far* VRAMptr = VRAM + (x >> 3);

		for (uint8_t j = firstLine; j < glyphHeight; j++)
		{
			uint8_t writeOffset = (uint8_t)(x) & 0x7;

			if ((style & FontStyle::Italic) && j < (font->glyphHeight >> 1))
			{
				writeOffset++;
			}

			for (uint8_t i = 0; i < font->glyphWidthBytes; i++)
			{
				uint8_t glyphPixels = *glyphData++;

				if (style & FontStyle::Bold)
				{
					glyphPixels |= (glyphPixels >> 1);
				}

				VRAMptr[i] ^= (glyphPixels >> writeOffset);
				VRAMptr[i + 1] ^= (glyphPixels << (8 - writeOffset));
			}

			interlace++;

			if (interlace == 4)
			{
				VRAMptr -= (0x6000 - BYTES_PER_LINE);
				interlace = 0;
			}
			else
			{
				VRAMptr += 0x2000;
			}
		}

		x += glyphWidth;
		if (style & FontStyle::Bold)
		{
			x++;
		}

		if (x >= scissorX2)
		{
			break;
		}
	}

	if ((style & FontStyle::Underline) && y - firstLine + font->glyphHeight - 1 < scissorY2)
	{
		HLine(startX, y - firstLine + font->glyphHeight - 1, x - startX);
	}
}

Font* HerculesDriver::GetFont(int fontSize, FontStyle::Type style)
{
	if (style & FontStyle::Monospace)
	{
		switch (fontSize)
		{
		case 0:
			return &Default_SmallFont_Monospace;
		case 2:
		case 3:
		case 4:
			return &Default_LargeFont_Monospace;
		default:
			return &Default_RegularFont_Monospace;
		}
	}

	switch (fontSize)
	{
	case 0:
		return &Default_SmallFont;
	case 2:
	case 3:
	case 4:
		return &Default_LargeFont;
	default:
		return &Default_RegularFont;
	}
}

void HerculesDriver::HLine(int x, int y, int count)
{
	if (invertScreen)
	{
		ClearHLine(x, y, count);
	}
	else
	{
		HLineInternal(x, y, count);
	}
}

void HerculesDriver::HLineInternal(int x, int y, int count)
{
	if (y < scissorY1 || y >= scissorY2)
		return;

	uint8_t far* VRAMptr = (uint8_t far*) BASE_VRAM_ADDRESS;

	VRAMptr += (y >> 2) * BYTES_PER_LINE;
	VRAMptr += (x >> 3);

	int interlace = y & 3;
	VRAMptr += interlace * 0x2000;

	uint8_t data = *VRAMptr;
	uint8_t mask = ~(0x80 >> (x & 7));

	while (count--)
	{
		data &= mask;
		x++;
		mask = (mask >> 1) | 0x80;
		if ((x & 7) == 0)
		{
			*VRAMptr++ = data;
			while (count > 8)
			{
				*VRAMptr++ = 0;
				count -= 8;
			}
			mask = ~0x80;
			data = *VRAMptr;
		}
	}

	*VRAMptr = data;
}

void HerculesDriver::ClearHLine(int x, int y, int count)
{
	uint8_t far* VRAMptr = (uint8_t far*) BASE_VRAM_ADDRESS;

	VRAMptr += (y >> 2) * BYTES_PER_LINE;
	VRAMptr += (x >> 3);

	int interlace = y & 3;
	VRAMptr += interlace * 0x2000;

	uint8_t data = *VRAMptr;
	uint8_t mask = (0x80 >> (x & 7));

	while (count--)
	{
		data |= mask;
		x++;
		mask >>= 1;
		if ((x & 7) == 0)
		{
			*VRAMptr++ = data;
			while (count > 8)
			{
				*VRAMptr++ = 0xff;
				count -= 8;
			}
			mask = 0x80;
			data = *VRAMptr;
		}
	}

	*VRAMptr = data;
}

void HerculesDriver::ClearRect(int x, int y, int width, int height)
{
	if (!ApplyScissor(y, height))
		return;

	if (invertScreen)
	{
		for (int j = 0; j < height; j++)
		{
			HLineInternal(x, y + j, width);
		}
	}
	else
	{
		for (int j = 0; j < height; j++)
		{
			ClearHLine(x, y + j, width);
		}
	}
}

void HerculesDriver::InvertLine(int x, int y, int count)
{
	uint8_t far* VRAMptr = (uint8_t far*) BASE_VRAM_ADDRESS;

	VRAMptr += (y >> 2) * BYTES_PER_LINE;
	VRAMptr += (x >> 3);

	int interlace = y & 3;
	VRAMptr += interlace * 0x2000;

	uint8_t data = *VRAMptr;
	uint8_t mask = (0x80 >> (x & 7));

	while (count--)
	{
		data ^= mask;
		x++;
		mask >>= 1;
		if ((x & 7) == 0)
		{
			*VRAMptr++ = data;
			while (count > 8)
			{
				*VRAMptr++ ^= 0xff;
				count -= 8;
			}
			mask = 0x80;
			data = *VRAMptr;
		}
	}

	*VRAMptr = data;
}

bool HerculesDriver::ApplyScissor(int& y, int& height)
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

void HerculesDriver::InvertRect(int x, int y, int width, int height)
{
	if (!ApplyScissor(y, height))
		return;

	for (int j = 0; j < height; j++)
	{
		InvertLine(x, y + j, width);
	}
}

void HerculesDriver::FillRect(int x, int y, int width, int height)
{
	if (invertScreen)
	{
		for (int j = 0; j < height; j++)
		{
			ClearHLine(x, y + j, width);
		}
	}
	else
	{
		for (int j = 0; j < height; j++)
		{
			HLineInternal(x, y + j, width);
		}
	}
}

void HerculesDriver::VLine(int x, int y, int count)
{
	if (y < scissorY1)
	{
		count -= (scissorY1 - y);
		y = scissorY1;
	}
	if (y >= scissorY2)
	{
		return;
	}
	if (y + count >= scissorY2)
	{
		count = scissorY2 - y;
	}
	if (count <= 0)
	{
		return;
	}

	uint8_t far* VRAMptr = (uint8_t far*) BASE_VRAM_ADDRESS;
	uint8_t mask = ~(0x80 >> (x & 7));

	VRAMptr += (y >> 2) * BYTES_PER_LINE;
	VRAMptr += (x >> 3);

	int interlace = (y & 3);
	VRAMptr += 0x2000 * interlace;

	if (invertScreen)
	{
		mask ^= 0xff;

		while (count--)
		{
			*VRAMptr |= mask;

			interlace++;
			if (interlace == 4)
			{
				VRAMptr -= (0x6000 - BYTES_PER_LINE);
				interlace = 0;
			}
			else
			{
				VRAMptr += 0x2000;
			}
		}
	}
	else
	{
		while (count--)
		{
			*VRAMptr &= mask;

			interlace++;
			if (interlace == 4)
			{
				VRAMptr -= (0x6000 - BYTES_PER_LINE);
				interlace = 0;
			}
			else
			{
				VRAMptr += 0x2000;
			}
		}
	}
}

MouseCursorData* HerculesDriver::GetCursorGraphic(MouseCursor::Type type)
{
	switch (type)
	{
	default:
	case MouseCursor::Pointer:
		return &Default_MouseCursor;
	case MouseCursor::Hand:
		return &Default_MouseCursorHand;
	case MouseCursor::TextSelect:
		return &Default_MouseCursorTextSelect;
	}
}

int HerculesDriver::GetGlyphWidth(char c, int fontSize, FontStyle::Type style)
{
	Font* font = GetFont(fontSize, style);
	if (c >= 32 && c < 128)
	{
		int width = font->glyphWidth[c - 32];
		if (style & FontStyle::Bold)
		{
			width++;
		}
		return width;
	}
	return 0;
}

int HerculesDriver::GetLineHeight(int fontSize, FontStyle::Type style)
{
	return GetFont(fontSize, style)->glyphHeight + 1;
}

void DrawScrollBarBlock(uint8_t far* ptr, int top, int middle, int bottom);
#pragma aux DrawScrollBarBlock = \
	"cmp bx, 0" \
	"je _startMiddle" \	
	"mov ax, 0xfe7f" \
	"_loopTop:" \
	"stosw" \
	"add di, 88" \
	"dec bx"\
	"jnz _loopTop" \
	"_startMiddle:" \
	"mov ax, 0x0660" \
	"_loopMiddle:" \
	"stosw" \
	"add di, 88" \
	"dec cx"\
	"jnz _loopMiddle" \
	"mov ax, 0xfe7f" \
	"cmp dx, 0" \
	"je _end" \
	"_loopBottom:" \
	"stosw" \
	"add di, 88" \
	"dec dx"\
	"jnz _loopBottom" \
	"_end:" \
	modify [di ax bx cx dx] \	
parm[es di][bx][cx][dx];

void DrawScrollBarBlockInverted(uint8_t far* ptr, int top, int middle, int bottom);
#pragma aux DrawScrollBarBlockInverted = \
	"cmp bx, 0" \
	"je _startMiddle" \	
	"mov ax, 0x0180" \
	"_loopTop:" \
	"stosw" \
	"add di, 88" \
	"dec bx"\
	"jnz _loopTop" \
	"_startMiddle:" \
	"mov ax, 0xf99f" \
	"_loopMiddle:" \
	"stosw" \
	"add di, 88" \
	"dec cx"\
	"jnz _loopMiddle" \
	"mov ax, 0x0180" \
	"cmp dx, 0" \
	"je _end" \
	"_loopBottom:" \
	"stosw" \
	"add di, 88" \
	"dec dx"\
	"jnz _loopBottom" \
	"_end:" \
	modify [di ax bx cx dx] \	
parm[es di][bx][cx][dx];


void HerculesDriver::DrawScrollBar(int position, int size)
{
	position >>= 2;
	size >>= 2;

	uint8_t* VRAM = BASE_VRAM_ADDRESS + WINDOW_TOP / 4 * BYTES_PER_LINE + (BYTES_PER_LINE - 2);

	if (invertScreen)
	{
		DrawScrollBarBlockInverted(VRAM, position, size, (WINDOW_HEIGHT / 4) - position - size);
		DrawScrollBarBlockInverted(VRAM + 0x2000, position, size, (WINDOW_HEIGHT / 4) - position - size);
		DrawScrollBarBlockInverted(VRAM + 0x4000, position, size, (WINDOW_HEIGHT / 4) - position - size);
		DrawScrollBarBlockInverted(VRAM + 0x6000, position, size, (WINDOW_HEIGHT / 4) - position - size);
	}
	else
	{
		DrawScrollBarBlock(VRAM, position, size, (WINDOW_HEIGHT / 4) - position - size);
		DrawScrollBarBlock(VRAM + 0x2000, position, size, (WINDOW_HEIGHT / 4) - position - size);
		DrawScrollBarBlock(VRAM + 0x4000, position, size, (WINDOW_HEIGHT / 4) - position - size);
		DrawScrollBarBlock(VRAM + 0x6000, position, size, (WINDOW_HEIGHT / 4) - position - size);
	}
}

void HerculesDriver::DrawRect(int x, int y, int width, int height)
{
	HLine(x, y, width);
	HLine(x, y + height - 1, width);
	VLine(x, y + 1, height - 2);
	VLine(x + width - 1, y + 1, height - 2);
}

void HerculesDriver::DrawButtonRect(int x, int y, int width, int height)
{
	HLine(x + 1, y, width - 2);
	HLine(x + 1, y + height - 1, width - 2);
	VLine(x, y + 1, height - 2);
	VLine(x + width - 1, y + 1, height - 2);
}

void ScrollRegionUp(int dest, int src, int count);
#pragma aux ScrollRegionUp = \
	"push ds" \
	"push es" \
	"mov ax, 0xb000" \
	"mov ds, ax" \
	"mov es, ax" \
	"_loopLine:" \
	"mov cx, 44" \
	"rep movsw" \
	"add di, 2" \
	"add si, 2" \
	"dec dx" \
	"jnz _loopLine" \
	"pop es" \
	"pop ds" \
	modify [ax cx dx di si] \
	parm [di][si][dx]

void ScrollRegionDown(int dest, int src, int count);
#pragma aux ScrollRegionDown = \
	"push ds" \
	"push es" \
	"mov ax, 0xb000" \
	"mov ds, ax" \
	"mov es, ax" \
	"_loopLine:" \
	"mov cx, 44" \
	"rep movsw" \
	"sub di, 178" \
	"sub si, 178" \
	"dec dx" \
	"jnz _loopLine" \
	"pop es" \
	"pop ds" \
	modify [ax cx dx di si] \
	parm [di][si][dx]

void ClearRegion(int offset, int count, uint16_t clearMask);
#pragma aux ClearRegion = \
	"push es" \
	"mov ax, 0xb000" \
	"mov es, ax" \
	"mov ax, bx" \
	"_loopLine:" \
	"mov cx, 44" \
	"rep stosw" \
	"add di, 2" \
	"dec dx" \
	"jnz _loopLine" \
	"pop es" \
	modify [cx di ax cx dx] \
	parm [di] [dx] [bx]

void HerculesDriver::ScrollWindow(int amount)
{
	amount &= ~3;

	if (amount > 0)
	{
		int lines = (WINDOW_HEIGHT - amount) >> 2;
		//int offset = amount * (BYTES_PER_LINE >> 1);
		int offset = (amount * BYTES_PER_LINE) >> 2;
		ScrollRegionUp(WINDOW_VRAM_TOP_PAGE1, WINDOW_VRAM_TOP_PAGE1 + offset, lines);
		ScrollRegionUp(WINDOW_VRAM_TOP_PAGE2, WINDOW_VRAM_TOP_PAGE2 + offset, lines);
		ScrollRegionUp(WINDOW_VRAM_TOP_PAGE3, WINDOW_VRAM_TOP_PAGE3 + offset, lines);
		ScrollRegionUp(WINDOW_VRAM_TOP_PAGE4, WINDOW_VRAM_TOP_PAGE4 + offset, lines);

		ClearRegion(WINDOW_VRAM_BOTTOM_PAGE1 - offset, (WINDOW_HEIGHT / 4) - lines, clearMask);
		ClearRegion(WINDOW_VRAM_BOTTOM_PAGE2 - offset, (WINDOW_HEIGHT / 4) - lines, clearMask);
		ClearRegion(WINDOW_VRAM_BOTTOM_PAGE3 - offset, (WINDOW_HEIGHT / 4) - lines, clearMask);
		ClearRegion(WINDOW_VRAM_BOTTOM_PAGE4 - offset, (WINDOW_HEIGHT / 4) - lines, clearMask);
	}
	else if (amount < 0)
	{
		int lines = (WINDOW_HEIGHT + amount) >> 2;
		int offset = (amount * BYTES_PER_LINE) >> 2;
		ScrollRegionDown(WINDOW_VRAM_BOTTOM_PAGE1 - BYTES_PER_LINE, WINDOW_VRAM_BOTTOM_PAGE1 - BYTES_PER_LINE + offset, lines);
		ScrollRegionDown(WINDOW_VRAM_BOTTOM_PAGE2 - BYTES_PER_LINE, WINDOW_VRAM_BOTTOM_PAGE2 - BYTES_PER_LINE + offset, lines);
		ScrollRegionDown(WINDOW_VRAM_BOTTOM_PAGE3 - BYTES_PER_LINE, WINDOW_VRAM_BOTTOM_PAGE3 - BYTES_PER_LINE + offset, lines);
		ScrollRegionDown(WINDOW_VRAM_BOTTOM_PAGE4 - BYTES_PER_LINE, WINDOW_VRAM_BOTTOM_PAGE4 - BYTES_PER_LINE + offset, lines);
		//
		ClearRegion(WINDOW_VRAM_TOP_PAGE1, (WINDOW_HEIGHT / 4) - lines, clearMask);
		ClearRegion(WINDOW_VRAM_TOP_PAGE2, (WINDOW_HEIGHT / 4) - lines, clearMask);
		ClearRegion(WINDOW_VRAM_TOP_PAGE3, (WINDOW_HEIGHT / 4) - lines, clearMask);
		ClearRegion(WINDOW_VRAM_TOP_PAGE4, (WINDOW_HEIGHT / 4) - lines, clearMask);
	}
}

void HerculesDriver::ClearWindow()
{
	ClearRegion(WINDOW_VRAM_TOP_PAGE1, (WINDOW_HEIGHT / 4), clearMask);
	ClearRegion(WINDOW_VRAM_TOP_PAGE2, (WINDOW_HEIGHT / 4), clearMask);
	ClearRegion(WINDOW_VRAM_TOP_PAGE3, (WINDOW_HEIGHT / 4), clearMask);
	ClearRegion(WINDOW_VRAM_TOP_PAGE4, (WINDOW_HEIGHT / 4), clearMask);
}

void HerculesDriver::SetScissorRegion(int y1, int y2)
{
	scissorY1 = y1;
	scissorY2 = y2;
}

void HerculesDriver::ClearScissorRegion()
{
	scissorY1 = 0;
	scissorY2 = screenHeight;
}

void HerculesDriver::ArrangeAppInterfaceWidgets(AppInterface& app)
{
	app.addressBar.x = ADDRESS_BAR_X;
	app.addressBar.y = ADDRESS_BAR_Y;
	app.addressBar.width = ADDRESS_BAR_WIDTH;
	app.addressBar.height = ADDRESS_BAR_HEIGHT;

	app.scrollBar.x = SCREEN_WIDTH - SCROLL_BAR_WIDTH;
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
	app.statusBar.y = SCREEN_HEIGHT - STATUS_BAR_HEIGHT;
	app.statusBar.width = SCREEN_WIDTH;
	app.statusBar.height = STATUS_BAR_HEIGHT;

	app.titleBar.x = 0;
	app.titleBar.y = 1;
	app.titleBar.width = SCREEN_WIDTH;
	app.titleBar.height = TITLE_BAR_HEIGHT;
}

void HerculesDriver::ScaleImageDimensions(int& width, int& height)
{
	// Scale to 4:3
	height = (height * 29) / 45;
}

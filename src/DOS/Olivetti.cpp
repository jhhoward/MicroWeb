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
#include "CGA.h"
#include "DefData.h"
#include "../GUI.h"

#define CGA_BASE_VRAM_ADDRESS (uint8_t*) MK_FP(0xB800, 0)

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 400

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

#define WINDOW_VRAM_TOP_0 (BYTES_PER_LINE * (WINDOW_TOP / 4))
#define WINDOW_VRAM_TOP_1 (0x2000 + BYTES_PER_LINE * (WINDOW_TOP / 4))
#define WINDOW_VRAM_TOP_2 (0x4000 + BYTES_PER_LINE * (WINDOW_TOP / 4))
#define WINDOW_VRAM_TOP_3 (0x6000 + BYTES_PER_LINE * (WINDOW_TOP / 4))

#define WINDOW_VRAM_BOTTOM_0 (BYTES_PER_LINE * (WINDOW_BOTTOM / 4))
#define WINDOW_VRAM_BOTTOM_1 (0x2000 + BYTES_PER_LINE * (WINDOW_BOTTOM / 4))
#define WINDOW_VRAM_BOTTOM_2 (0x4000 + BYTES_PER_LINE * (WINDOW_BOTTOM / 4))
#define WINDOW_VRAM_BOTTOM_3 (0x6000 + BYTES_PER_LINE * (WINDOW_BOTTOM / 4))

#define BYTES_PER_LINE 80

OlivettiDriver::OlivettiDriver(int _videoMode)
	: videoMode(_videoMode)
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
	isTextMode = false;
}

void OlivettiDriver::Init()
{
	startingScreenMode = GetScreenMode();
	SetScreenMode(videoMode);
}

void OlivettiDriver::InvertScreen()
{
	int count = 0x8000;
	unsigned char far* VRAM = CGA_BASE_VRAM_ADDRESS;
	while (count--)
	{
		*VRAM ^= 0xff;
		VRAM++;
	}

	invertScreen = !invertScreen;
	clearMask = invertScreen ? 0 : 0xffff;
}

void OlivettiDriver::ClearScreen()
{
	uint8_t clearValue = (uint8_t)(clearMask & 0xff);
	FastMemSet(CGA_BASE_VRAM_ADDRESS, clearValue, 0x8000);
}

void OlivettiDriver::DrawImage(Image* image, int x, int y)
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

	uint8_t far* VRAM = (uint8_t far*) CGA_BASE_VRAM_ADDRESS;
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

		if (interlace == 3)
		{
			VRAMptr -= (0x6000 - BYTES_PER_LINE);
		}
		else
		{
			VRAMptr += 0x2000;
		}
		interlace = (interlace + 1) & 3;
	}
}

void OlivettiDriver::DrawString(const char* text, int x, int y, int size, FontStyle::Type style)
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

	uint8_t far* VRAM = (uint8_t far*) CGA_BASE_VRAM_ADDRESS;
	VRAM += (y >> 2) * BYTES_PER_LINE;
	uint8_t interlace = y & 3;
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

		uint8_t far* VRAMptr = VRAM + (x >> 3);
		interlace = y & 3;

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

			if (interlace == 3)
			{
				VRAMptr -= (0x6000 - BYTES_PER_LINE);
			}
			else
			{
				VRAMptr += 0x2000;
			}
			interlace = (interlace + 1) & 3;
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

Font* OlivettiDriver::GetFont(int fontSize, FontStyle::Type style)
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

void OlivettiDriver::HLineInternal(int x, int y, int count)
{
	if (y < scissorY1 || y >= scissorY2)
		return;

	uint8_t far* VRAMptr = (uint8_t far*) CGA_BASE_VRAM_ADDRESS;

	VRAMptr += (y >> 2) * BYTES_PER_LINE;
	VRAMptr += (y & 3) * 0x2000;
	VRAMptr += (x >> 3);

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

void OlivettiDriver::ClearHLine(int x, int y, int count)
{
	uint8_t far* VRAMptr = (uint8_t far*) CGA_BASE_VRAM_ADDRESS;

	VRAMptr += (y >> 2) * BYTES_PER_LINE;
	VRAMptr += (y & 3) * 0x2000;
	VRAMptr += (x >> 3);

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

void OlivettiDriver::InvertLine(int x, int y, int count)
{
	uint8_t far* VRAMptr = (uint8_t far*) CGA_BASE_VRAM_ADDRESS;

	VRAMptr += (y >> 2) * BYTES_PER_LINE;
	VRAMptr += (y & 3) * 0x2000;
	VRAMptr += (x >> 3);

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

void OlivettiDriver::FillRect(int x, int y, int width, int height)
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

void OlivettiDriver::VLine(int x, int y, int count)
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

	uint8_t far* VRAMptr = (uint8_t far*) CGA_BASE_VRAM_ADDRESS;
	uint8_t mask = ~(0x80 >> (x & 7));

	VRAMptr += (y >> 2) * BYTES_PER_LINE;
	VRAMptr += (x >> 3);

	uint8_t interlace = y & 3;
	VRAMptr += 0x2000 * interlace;

	if (invertScreen)
	{
		mask ^= 0xff;

		while (count--)
		{
			*VRAMptr |= mask;
			if (interlace == 3)
			{
				VRAMptr -= (0x6000 - BYTES_PER_LINE);
			}
			else
			{
				VRAMptr += 0x2000;
			}
			interlace = (interlace + 1) & 3;
		}
	}
	else
	{
		while (count--)
		{
			*VRAMptr &= mask;
			if (interlace == 3)
			{
				VRAMptr -= (0x6000 - BYTES_PER_LINE);
			}
			else
			{
				VRAMptr += 0x2000;
			}
			interlace = (interlace + 1) & 3;
		}
	}
}

MouseCursorData* OlivettiDriver::GetCursorGraphic(MouseCursor::Type type)
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

void OlivettiDriver::DrawScrollBar(int position, int size)
{
	position >>= 2;
	size >>= 2;

	uint8_t* VRAM = CGA_BASE_VRAM_ADDRESS + WINDOW_TOP / 4 * BYTES_PER_LINE + (BYTES_PER_LINE - 2);

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

void OlivettiDriver::ScrollWindow(int amount)
{
	amount &= ~3;

	if (amount > 0)
	{
		int lines = (WINDOW_HEIGHT - amount) >> 2;
		int offset = (amount * BYTES_PER_LINE) >> 2;
		ScrollRegionUp(WINDOW_VRAM_TOP_0, WINDOW_VRAM_TOP_0 + offset, lines);
		ScrollRegionUp(WINDOW_VRAM_TOP_1, WINDOW_VRAM_TOP_1 + offset, lines);
		ScrollRegionUp(WINDOW_VRAM_TOP_2, WINDOW_VRAM_TOP_2 + offset, lines);
		ScrollRegionUp(WINDOW_VRAM_TOP_3, WINDOW_VRAM_TOP_3 + offset, lines);

		ClearRegion(WINDOW_VRAM_BOTTOM_0 - offset, (WINDOW_HEIGHT / 4) - lines, clearMask);
		ClearRegion(WINDOW_VRAM_BOTTOM_1 - offset, (WINDOW_HEIGHT / 4) - lines, clearMask);
		ClearRegion(WINDOW_VRAM_BOTTOM_2 - offset, (WINDOW_HEIGHT / 4) - lines, clearMask);
		ClearRegion(WINDOW_VRAM_BOTTOM_3 - offset, (WINDOW_HEIGHT / 4) - lines, clearMask);
	}
	else if (amount < 0)
	{
		int lines = (WINDOW_HEIGHT + amount) >> 2;
		int offset = (amount * BYTES_PER_LINE) >> 2;
		ScrollRegionDown(WINDOW_VRAM_BOTTOM_0 - BYTES_PER_LINE, WINDOW_VRAM_BOTTOM_0 - BYTES_PER_LINE + offset, lines);
		ScrollRegionDown(WINDOW_VRAM_BOTTOM_1 - BYTES_PER_LINE, WINDOW_VRAM_BOTTOM_1 - BYTES_PER_LINE + offset, lines);
		ScrollRegionDown(WINDOW_VRAM_BOTTOM_2 - BYTES_PER_LINE, WINDOW_VRAM_BOTTOM_2 - BYTES_PER_LINE + offset, lines);
		ScrollRegionDown(WINDOW_VRAM_BOTTOM_3 - BYTES_PER_LINE, WINDOW_VRAM_BOTTOM_3 - BYTES_PER_LINE + offset, lines);

		ClearRegion(WINDOW_VRAM_TOP_0, (WINDOW_HEIGHT / 4) - lines, clearMask);
		ClearRegion(WINDOW_VRAM_TOP_1, (WINDOW_HEIGHT / 4) - lines, clearMask);
		ClearRegion(WINDOW_VRAM_TOP_2, (WINDOW_HEIGHT / 4) - lines, clearMask);
		ClearRegion(WINDOW_VRAM_TOP_3, (WINDOW_HEIGHT / 4) - lines, clearMask);
	}
}

void OlivettiDriver::ClearWindow()
{
	ClearRegion(WINDOW_VRAM_TOP_0, (WINDOW_HEIGHT / 4), clearMask);
	ClearRegion(WINDOW_VRAM_TOP_1, (WINDOW_HEIGHT / 4), clearMask);
	ClearRegion(WINDOW_VRAM_TOP_2, (WINDOW_HEIGHT / 4), clearMask);
	ClearRegion(WINDOW_VRAM_TOP_3, (WINDOW_HEIGHT / 4), clearMask);
}

void OlivettiDriver::ScaleImageDimensions(int& width, int& height)
{
	// Scale to 4:3
	height = (height * 5) / 6;
}

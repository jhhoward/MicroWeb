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

#include <stdio.h>
#include <windows.h>
#include "WinVid.h"
#include "../Image.h"
//#include "../DOS/CGAData.inc"
#include "../Interface.h"
#include "../DataPack.h"
#include "../Draw/Surf1bpp.h"

#define WINDOW_TOP 24
#define WINDOW_HEIGHT 168
#define WINDOW_BOTTOM (WINDOW_TOP + WINDOW_HEIGHT)

//#define SCREEN_WIDTH 800
//#define SCREEN_HEIGHT 600
#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 200

#define NAVIGATION_BUTTON_WIDTH 24
#define NAVIGATION_BUTTON_HEIGHT 12

#define BACK_BUTTON_X 4
#define FORWARD_BUTTON_X 32

#define ADDRESS_BAR_X 60
#define ADDRESS_BAR_Y 10
#define ADDRESS_BAR_WIDTH 576
#define ADDRESS_BAR_HEIGHT 12

#define TITLE_BAR_HEIGHT 8
#define STATUS_BAR_HEIGHT 8
#define STATUS_BAR_Y (SCREEN_HEIGHT - STATUS_BAR_HEIGHT)

#define SCROLL_BAR_WIDTH 16

extern HWND hWnd;

WindowsVideoDriver::WindowsVideoDriver()
{
	screenWidth = SCREEN_WIDTH;
	screenHeight = SCREEN_HEIGHT;
	windowWidth = screenWidth - 16;
	windowHeight = WINDOW_HEIGHT;
	windowX = 0;
	windowY = WINDOW_TOP;

	foregroundColour = RGB(0, 0, 0);
	backgroundColour = RGB(255, 255, 255);
	verticalScale = 1;

	scissorX1 = 0;
	scissorY1 = 0;
	scissorX2 = screenWidth;
	scissorY2 = screenHeight;



	//imageIcon = &CGA_ImageIcon;
	//bulletImage = &CGA_Bullet;

	Assets.Load("Default.dat");
//	Assets.Load("Lowres.dat");
//	Assets.Load("CGA.dat");

	isTextMode = false;
}

void WindowsVideoDriver::Init()
{
	HDC hDC = GetDC(hWnd);
	HDC hDCMem = CreateCompatibleDC(hDC);

	bitmapInfo = (BITMAPINFO*) malloc(sizeof(BITMAPINFO) + sizeof(RGBQUAD) * 2);
	ZeroMemory(bitmapInfo, sizeof(BITMAPINFO));
	bitmapInfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bitmapInfo->bmiHeader.biWidth = screenWidth;
	bitmapInfo->bmiHeader.biHeight = screenHeight * verticalScale;
	bitmapInfo->bmiHeader.biPlanes = 1;
//	bitmapInfo->bmiHeader.biBitCount = 32;
	bitmapInfo->bmiHeader.biBitCount = 1;
	bitmapInfo->bmiHeader.biCompression = BI_RGB;
	bitmapInfo->bmiHeader.biClrUsed = 0;

	bitmapInfo->bmiColors[0].rgbRed = 0;
	bitmapInfo->bmiColors[0].rgbGreen = 0;
	bitmapInfo->bmiColors[0].rgbBlue = 0;
	bitmapInfo->bmiColors[1].rgbRed = 0xff;
	bitmapInfo->bmiColors[1].rgbGreen = 0xff;
	bitmapInfo->bmiColors[1].rgbBlue = 0xff;

	screenBitmap = CreateDIBSection(hDCMem, bitmapInfo, DIB_RGB_COLORS, (VOID**)&lpBitmapBits, NULL, 0);

	{
		DrawSurface_1BPP* surface = new DrawSurface_1BPP(screenWidth, screenHeight);
		uint8_t* buffer = (uint8_t*)(lpBitmapBits);
		
		int pitch = (screenWidth + 7) / 8;		// How many bytes needed for 1bpp
		if (pitch & 3)							// Round to nearest 32-bit 
		{
			pitch += 4 - (pitch & 3);
		}

		for (int y = 0; y < screenHeight; y++)
		{
			int bufferY = (screenHeight - 1 - y);
			surface->lines[y] = &buffer[bufferY * pitch];
		}
		drawSurface = surface;
	}

	if (bitmapInfo->bmiHeader.biBitCount == 1)
	{
		uint8_t* ptr = (uint8_t*)(lpBitmapBits);
		for (int n = 0; n < screenWidth * screenHeight / 8; n++)
		{
			ptr[n] = 0xff;
		}
	}
	else
	{
		for (int n = 0; n < screenWidth * screenHeight * verticalScale; n++)
		{
			lpBitmapBits[n] = backgroundColour;
		}
	}
}

void WindowsVideoDriver::Shutdown()
{
}

void WindowsVideoDriver::ClearScreen()
{
	//FillRect(0, 0, screenWidth, TITLE_BAR_HEIGHT, foregroundColour);
	//FillRect(0, TITLE_BAR_HEIGHT, screenWidth, screenHeight - STATUS_BAR_HEIGHT - TITLE_BAR_HEIGHT, backgroundColour);
	//FillRect(0, screenHeight - STATUS_BAR_HEIGHT, screenWidth, STATUS_BAR_HEIGHT, foregroundColour);
}

void WindowsVideoDriver::FillRect(int x, int y, int width, int height)
{
	DrawContext context(drawSurface, 0, 0, screenWidth, screenHeight);
	//drawSurface->FillRect(context, x, y, width, height, 0);

	//	FillRect(x, y, width, height, foregroundColour);
}

void WindowsVideoDriver::FillRect(int x, int y, int width, int height, uint32_t colour)
{
	DrawContext context(drawSurface, 0, 0, screenWidth, screenHeight);
	drawSurface->FillRect(context, x, y, width, height, colour ? 1 : 0);

//	for (int j = 0; j < height; j++)
//	{
//		for (int i = 0; i < width; i++)
//		{
//			SetPixel(x + i, y + j, colour);
//		}
//	}
}

void WindowsVideoDriver::SetPixel(int x, int y, uint32_t colour)
{
	if (x >= 0 && y >= 0 && x < screenWidth && y < screenHeight)
	{
		int outY = screenHeight - y - 1;
		uint8_t mask = 0x80 >> ((uint8_t)(x & 7));
		uint8_t* ptr = (uint8_t*)(lpBitmapBits);
		int index = (outY * (screenWidth / 8) + (x / 8));

		if (colour)
		{
			ptr[index] |= mask;
		}
		else
		{
			ptr[index] &= ~mask;
		}
	}
	/*
	if (x >= 0 && y >= 0 && x < screenWidth && y < screenHeight)
	{
		int line = ((screenHeight - y - 1) * verticalScale);
		for (int j = 0; j < verticalScale; j++)
		{
			lpBitmapBits[(line + j) * screenWidth + x] = colour;
		}
	}
	*/
}

void WindowsVideoDriver::InvertPixel(int x, int y, uint32_t colour)
{
	if (x >= 0 && y >= 0 && x < screenWidth && y < screenHeight)
	{
		int outY = screenHeight - y - 1;
		uint8_t mask = 0x80 >> ((uint8_t)(x & 7));
		uint8_t* ptr = (uint8_t*)(lpBitmapBits);
		int index = (outY * (screenWidth / 8) + (x / 8));

		ptr[index] ^= mask;
	}

	/*if (x >= 0 && y >= 0 && x < screenWidth && y < screenHeight)
	{
		int line = ((screenHeight - y - 1) * verticalScale);
		for (int j = 0; j < verticalScale; j++)
		{
			lpBitmapBits[(line + j) * screenWidth + x] ^= colour;
		}
	}*/
}

void WindowsVideoDriver::ClearWindow()
{
	//FillRect(0, WINDOW_TOP, SCREEN_WIDTH - SCROLL_BAR_WIDTH, WINDOW_HEIGHT, backgroundColour);
}

void WindowsVideoDriver::ClearRect(int x, int y, int width, int height)
{
	//FillRect(x, y, width, height, backgroundColour);
}

void WindowsVideoDriver::ScrollWindow(int delta)
{

}
void WindowsVideoDriver::SetScissorRegion(int y1, int y2)
{
	scissorY1 = y1;
	scissorY2 = y2;
}

void WindowsVideoDriver::ClearScissorRegion()
{
	scissorY1 = 0;
	scissorY2 = screenHeight;
}

void WindowsVideoDriver::DrawString(const char* text, int x, int y, int size, FontStyle::Type style)
{
//	printf("%s\n", text);
	Font* font = GetFont(size, style);

	DrawContext context(drawSurface, 0, 0, screenWidth, screenHeight);
	drawSurface->DrawString(context, font, text, x, y, 0, style);
	return;
	

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

		for (uint8_t j = firstLine; j < glyphHeight; j++)
		{
			uint8_t writeOffset = (uint8_t)(x) & 0x7;

			if ((style & FontStyle::Italic) && j < (glyphHeight >> 1))
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

				for (int k = 0; k < 8; k++)
				{
					if (glyphPixels & (0x80 >> k))
					{
						InvertPixel(x + k + i * 8, y + j, 0xffffff);
					}
				}
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

void WindowsVideoDriver::HLine(int x, int y, int count)
{
	DrawContext context(drawSurface, 0, 0, screenWidth, screenHeight);
	drawSurface->HLine(context, x, y, count, 0);
	//for (int n = 0; n < count; n++)
	//{
	//	SetPixel(x + n, y, foregroundColour);
	//}
}

void WindowsVideoDriver::VLine(int x, int y, int count)
{
	DrawContext context(drawSurface, 0, 0, screenWidth, screenHeight);
	drawSurface->VLine(context, x, y, count, 0);
	//for (int n = 0; n < count; n++)
	//{
	//	SetPixel(x, y + n, foregroundColour);
	//}
}

void WindowsVideoDriver::DrawScrollBar(int position, int size)
{
}

MouseCursorData* WindowsVideoDriver::GetCursorGraphic(MouseCursor::Type type)
{
	return NULL;
}

Font* WindowsVideoDriver::GetFont(int fontSize, FontStyle::Type style)
{
	if (style & FontStyle::Monospace)
	{
		switch (fontSize)
		{
		case 0:
			return &Assets.monoFonts[0];
		case 2:
		case 3:
		case 4:
			return &Assets.monoFonts[2];
		default:
			return &Assets.monoFonts[1];
		}
	}

	switch (fontSize)
	{
	case 0:
		return &Assets.fonts[0];
	case 2:
		return &Assets.fonts[2];
	default:
		return &Assets.fonts[1];
	}

}
int WindowsVideoDriver::GetGlyphWidth(char c, int fontSize, FontStyle::Type style)
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
int WindowsVideoDriver::GetLineHeight(int fontSize, FontStyle::Type style)
{
	return GetFont(fontSize, style)->glyphHeight + 1;
}

void WindowsVideoDriver::Paint(HWND hwnd)
{
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(hwnd, &ps);
	HDC hdcMem = CreateCompatibleDC(hdc);
	HGDIOBJ oldBitmap = SelectObject(hdcMem, screenBitmap);
	BITMAP bitmap;

	GetObject(screenBitmap, sizeof(BITMAP), &bitmap);
	BitBlt(hdc, 0, 0, bitmap.bmWidth, bitmap.bmHeight,
		hdcMem, 0, 0, SRCCOPY);

	SelectObject(hdcMem, oldBitmap);
	DeleteDC(hdcMem);

	EndPaint(hwnd, &ps);

/*	PAINTSTRUCT ps;
	RECT r;

	GetClientRect(hwnd, &r);

	if (r.bottom == 0) {

		return;
	}

	HDC hdc = BeginPaint(hwnd, &ps);

	for (int y = 0; y < screenHeight; y++)
	{
		for (int x = 0; x < screenWidth; x++)
		{
			uint8_t value = screenPixels[y * screenWidth + x];
			SetPixel(hdc, x, y * 2, value ? RGB(255, 255, 255) : RGB(0, 0, 0));
			SetPixel(hdc, x, y * 2 + 1, value ? RGB(255, 255, 255) : RGB(0, 0, 0));
		}
	}

	EndPaint(hwnd, &ps);*/


}

void WindowsVideoDriver::ArrangeAppInterfaceWidgets(AppInterface& app)
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
	app.titleBar.y = 0;
	app.titleBar.width = SCREEN_WIDTH;
	app.titleBar.height = TITLE_BAR_HEIGHT;
}

void WindowsVideoDriver::InvertRect(int x, int y, int width, int height)
{
	if (y + height < scissorY1)
		return;
	if (y >= scissorY2)
		return;
	if (y < scissorY1)
	{
		height -= (scissorY1 - y);
		y = scissorY1;
	}
	if (y + height >= scissorY2)
	{
		height = scissorY2 - y - 1;
	}

	for (int j = 0; j < height; j++)
	{
		for (int i = 0; i < width; i++)
		{
			InvertPixel(x + i, y + j, 0xffffffff);
		}
	}
}

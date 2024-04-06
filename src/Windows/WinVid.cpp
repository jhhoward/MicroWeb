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
#include "../Image/Image.h"
#include "../Interface.h"
#include "../DataPack.h"
#include "../Draw/Surf1bpp.h"
#include "../Draw/Surf8bpp.h"
#include "../VidModes.h"

//#define SCREEN_WIDTH 800
//#define SCREEN_HEIGHT 600
#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480
#define USE_COLOUR 0

extern HWND hWnd;
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

WindowsVideoDriver::WindowsVideoDriver()
{
}

const RGBQUAD monoPalette[] =
{
	{ 0, 0, 0, 0 },
	{ 0xff, 0xff, 0xff, 0 }
};

const RGBQUAD cgaPalette[] =
{
	{ 0x00, 0x00, 0x00 }, // Entry 0 - Black
	{ 0xFF, 0xFF, 0x55 }, // Entry 11 - Light Cyan
	{ 0x55, 0x55, 0xFF }, // Entry 12 - Light Red
	{ 0xFF, 0xFF, 0xFF }, // Entry 15 - White
};

const RGBQUAD cgaCompositePalette[] =
{
	{ 0x00, 0x00, 0x00 },
	{ 0x31, 0x6e, 0x00 },
	{ 0xff, 0x09, 0x31 },
	{ 0xff, 0x8a, 0x00 },
	{ 0x31, 0x00, 0xa7 },
	{ 0x76, 0x76, 0x76 },
	{ 0xff, 0x11, 0xec },
	{ 0xff, 0x92, 0xbb },
	{ 0x00, 0x5a, 0x31 },
	{ 0x00, 0xdb, 0x00 },
	{ 0x76, 0x76, 0x76 },
	{ 0xbb, 0xf7, 0x45 },
	{ 0x00, 0x63, 0xec },
	{ 0x00, 0xe4, 0xbb },
	{ 0xbb, 0x7f, 0xff },
	{ 0xff, 0xff, 0xff },
};


const RGBQUAD egaPalette[] =
{
	{ 0x00, 0x00, 0x00 }, // Entry 0 - Black
	{ 0xAA, 0x00, 0x00 }, // Entry 1 - Blue
	{ 0x00, 0xAA, 0x00 }, // Entry 2 - Green
	{ 0xAA, 0xAA, 0x00 }, // Entry 3 - Cyan
	{ 0x00, 0x00, 0xAA }, // Entry 4 - Red
	{ 0xAA, 0x00, 0xAA }, // Entry 5 - Magenta
	{ 0x00, 0x55, 0xAA }, // Entry 6 - Brown
	{ 0xAA, 0xAA, 0xAA }, // Entry 7 - Light Gray
	{ 0x55, 0x55, 0x55 }, // Entry 8 - Dark Gray
	{ 0xFF, 0x55, 0x55 }, // Entry 9 - Light Blue
	{ 0x55, 0xFF, 0x55 }, // Entry 10 - Light Green
	{ 0xFF, 0xFF, 0x55 }, // Entry 11 - Light Cyan
	{ 0x55, 0x55, 0xFF }, // Entry 12 - Light Red
	{ 0xFF, 0x55, 0xFF }, // Entry 13 - Light Magenta
	{ 0x55, 0xFF, 0xFF }, // Entry 14 - Yellow
	{ 0xFF, 0xFF, 0xFF }, // Entry 15 - White
};

void WindowsVideoDriver::Init(VideoModeInfo* inVideoMode)
{
	videoMode = inVideoMode;

	screenWidth = videoMode->screenWidth;
	screenHeight = videoMode->screenHeight;
	verticalScale = videoMode->aspectRatio / 100.0f; // ((screenWidth * 3.0f) / 4.0f) / screenHeight;
	//verticalScale = 1.0f;
	Assets.LoadPreset((DataPack::Preset) videoMode->dataPackIndex);

	// Create window
	WNDCLASSW wc = { 0 };
	HINSTANCE hInstance = GetModuleHandle(NULL);

	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpszClassName = L"Pixels";
	wc.hInstance = hInstance;
	wc.hbrBackground = GetSysColorBrush(COLOR_3DFACE);
	wc.lpfnWndProc = WndProc;
	wc.hCursor = LoadCursor(0, IDC_ARROW);

	RECT wr = { 0, 0, screenWidth, (int)(screenHeight * verticalScale) };
	AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

	RegisterClassW(&wc);
	hWnd = CreateWindowW(wc.lpszClassName, L"MicroWeb",
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		100, 100, wr.right - wr.left, wr.bottom - wr.top, NULL, NULL, hInstance, NULL);

	// Create bitmap for window contents
	HDC hDC = GetDC(hWnd);
	HDC hDCMem = CreateCompatibleDC(hDC);

	bool useColour = videoMode->surfaceFormat != DrawSurface::Format_1BPP;
	int paletteSize = useColour ? 256 : 2;

	bitmapInfo = (BITMAPINFO*)malloc(sizeof(BITMAPINFO) + sizeof(RGBQUAD) * paletteSize);
	ZeroMemory(bitmapInfo, sizeof(BITMAPINFO) + sizeof(RGBQUAD) * paletteSize);
	bitmapInfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bitmapInfo->bmiHeader.biWidth = screenWidth;
	bitmapInfo->bmiHeader.biHeight = screenHeight;
	bitmapInfo->bmiHeader.biPlanes = 1;
//	bitmapInfo->bmiHeader.biBitCount = 32;
	bitmapInfo->bmiHeader.biBitCount = useColour ? 8 : 1;
	bitmapInfo->bmiHeader.biCompression = BI_RGB;
	bitmapInfo->bmiHeader.biClrUsed = 0;

	if (useColour)
	{
		memcpy(bitmapInfo->bmiColors, egaPalette, sizeof(RGBQUAD) * 16);

		if (videoMode->surfaceFormat == DrawSurface::Format_8BPP)
		{
			int index = 16;
			for (int r = 0; r < 6; r++)
			{
				for (int g = 0; g < 6; g++)
				{
					for (int b = 0; b < 6; b++)
					{
						bitmapInfo->bmiColors[index].rgbRed = (r * 255) / 5;
						bitmapInfo->bmiColors[index].rgbGreen = (g * 255) / 5;
						bitmapInfo->bmiColors[index].rgbBlue = (b * 255) / 5;
						index++;
					}
				}
			}
		}
		else if (videoMode->surfaceFormat == DrawSurface::Format_2BPP)
		{
			if (videoMode->biosVideoMode == CGA_COMPOSITE_MODE)
			{
				for (int n = 0; n < 256; n += 16)
				{
					memcpy(bitmapInfo->bmiColors + n, cgaCompositePalette, sizeof(RGBQUAD) * 16);
				}
			}
			else
			{
				for (int n = 0; n < 256; n += 4)
				{
					memcpy(bitmapInfo->bmiColors + n, cgaPalette, sizeof(RGBQUAD) * 4);
				}
			}
		}
	}
	else
	{
		memcpy(bitmapInfo->bmiColors, monoPalette, sizeof(RGBQUAD) * 2);
	}

	screenBitmap = CreateDIBSection(hDCMem, bitmapInfo, DIB_RGB_COLORS, (VOID**)&lpBitmapBits, NULL, 0);

	if (useColour)
	{
		DrawSurface_8BPP* surface = new DrawSurface_8BPP(screenWidth, screenHeight);
		uint8_t* buffer = (uint8_t*)(lpBitmapBits);

		int pitch = screenWidth;

		for (int y = 0; y < screenHeight; y++)
		{
			int bufferY = (screenHeight - 1 - y);
			surface->lines[y] = &buffer[bufferY * pitch];
		}
		drawSurface = surface;

		for (int n = 0; n < screenWidth * screenHeight; n++)
		{
			buffer[n] = 0xf;
		}


		if (videoMode->surfaceFormat == DrawSurface::Format_8BPP)
		{
			colourScheme = colourScheme666;
			//paletteLUT = cgaPaletteLUT;

			paletteLUT = new uint8_t[256];
			for (int n = 0; n < 256; n++)
			{
				int r = (n & 0xe0);
				int g = (n & 0x1c) << 3;
				int b = (n & 3) << 6;

				int rgbBlue = (b * 255) / 0xc0;
				int rgbGreen = (g * 255) / 0xe0;
				int rgbRed = (r * 255) / 0xe0;

				paletteLUT[n] = RGB666(rgbRed, rgbGreen, rgbBlue);
			}
		}
		else if (videoMode->surfaceFormat == DrawSurface::Format_2BPP)
		{
			if (videoMode->biosVideoMode == CGA_COMPOSITE_MODE)
			{
				colourScheme = compositeCgaColourScheme;
				paletteLUT = compositeCgaPaletteLUT;
			}
			else
			{
				colourScheme = cgaColourScheme;
				paletteLUT = cgaPaletteLUT;
			}
		}
		else
		{
			colourScheme = egaColourScheme;
			paletteLUT = egaPaletteLUT;
		}
	}
	else
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

		for (int n = 0; n < screenWidth * screenHeight / 8; n++)
		{
			buffer[n] = 0xff;
		}

		colourScheme = monochromeColourScheme;

		paletteLUT = nullptr;
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

void WindowsVideoDriver::Paint(HWND hwnd)
{
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(hwnd, &ps);
	HDC hdcMem = CreateCompatibleDC(hdc);
	HGDIOBJ oldBitmap = SelectObject(hdcMem, screenBitmap);
	BITMAP bitmap;

	GetObject(screenBitmap, sizeof(BITMAP), &bitmap);

	// Calculate the destination rectangle to stretch the bitmap to fit the window
	RECT destRect;
	GetClientRect(hwnd, &destRect);

	// Stretch the bitmap to fit the window size
	StretchBlt(hdc, 0, 0, destRect.right - destRect.left, destRect.bottom - destRect.top,
		hdcMem, 0, 0, bitmap.bmWidth, bitmap.bmHeight, SRCCOPY);

	SelectObject(hdcMem, oldBitmap);
	DeleteDC(hdcMem);

	EndPaint(hwnd, &ps);
}


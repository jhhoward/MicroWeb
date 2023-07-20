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
#include "../Interface.h"
#include "../DataPack.h"
#include "../Draw/Surf1bpp.h"

//#define SCREEN_WIDTH 800
//#define SCREEN_HEIGHT 600
#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480

extern HWND hWnd;

WindowsVideoDriver::WindowsVideoDriver()
{
	screenWidth = SCREEN_WIDTH;
	screenHeight = SCREEN_HEIGHT;
	foregroundColour = RGB(0, 0, 0);
	backgroundColour = RGB(255, 255, 255);
	verticalScale = 1;

	Assets.Load("Default.dat");
//	Assets.Load("Lowres.dat");
//	Assets.Load("CGA.dat");
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


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

#pragma once

#include "../Platform.h"
#include <Windows.h>

class DrawSurface;

class WindowsVideoDriver : public VideoDriver
{
public:
	WindowsVideoDriver();

	virtual void Init(VideoModeInfo* videoMode);
	virtual void Shutdown();

	virtual void ClearScreen();
	virtual void ScaleImageDimensions(int& width, int& height) override;


	void Paint(HWND hwnd);

	float verticalScale;

private:
	void SetPixel(int x, int y, uint32_t colour);
	void InvertPixel(int x, int y, uint32_t colour);

	BITMAPINFO* bitmapInfo;
	uint32_t* lpBitmapBits;
	HBITMAP screenBitmap;

};

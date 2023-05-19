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

struct Image;

class CGADriver : public VideoDriver
{
public:
	CGADriver();

	virtual void Init();
	virtual void Shutdown();
	virtual void ClearScreen();
	virtual void InvertScreen();

	virtual void ArrangeAppInterfaceWidgets(class AppInterface& app);
	

	virtual void ClearWindow();
	virtual void ScrollWindow(int delta);
	virtual void SetScissorRegion(int y1, int y2);
	virtual void ClearScissorRegion();

	virtual void DrawString(const char* text, int x, int y, int size = 1, FontStyle::Type style = FontStyle::Regular);
	virtual void DrawRect(int x, int y, int width, int height);
	virtual void DrawButtonRect(int x, int y, int width, int height);
	virtual void DrawImage(Image* image, int x, int y);
	virtual void ClearRect(int x, int y, int width, int height);
	virtual void FillRect(int x, int y, int width, int height);
	virtual void InvertRect(int x, int y, int width, int height);

	virtual void HLine(int x, int y, int count);
	virtual void VLine(int x, int y, int count);
	virtual void ScaleImageDimensions(int& width, int& height);

	virtual int GetGlyphWidth(char c, int fontSize = 1, FontStyle::Type style = FontStyle::Regular);
	virtual int GetLineHeight(int fontSize = 1, FontStyle::Type style = FontStyle::Regular);
	virtual Font* GetFont(int fontSize, FontStyle::Type style = FontStyle::Regular);

	virtual void DrawScrollBar(int position, int size);

	virtual MouseCursorData* GetCursorGraphic(MouseCursor::Type type);

protected:
	int GetScreenMode();
	void SetScreenMode(int screenMode);
	bool ApplyScissor(int& y, int& height);

	bool invertScreen;
	uint16_t clearMask;
	int startingScreenMode;
	int scissorX1, scissorY1, scissorX2, scissorY2;

private:
	void ClearHLine(int x, int y, int count);
	void HLineInternal(int x, int y, int count);
	void InvertLine(int x, int y, int count);
};

class OlivettiDriver : public CGADriver
{
public:
	OlivettiDriver(int _videoMode);

	virtual void Init();

	virtual void ClearScreen();
	virtual void InvertScreen();

	virtual void DrawImage(Image* image, int x, int y);
	virtual void DrawString(const char* text, int x, int y, int size = 1, FontStyle::Type style = FontStyle::Regular);
	virtual void FillRect(int x, int y, int width, int height);
	virtual void VLine(int x, int y, int count);
	virtual void DrawScrollBar(int position, int size);

	virtual Font* GetFont(int fontSize, FontStyle::Type style = FontStyle::Regular);
	virtual MouseCursorData* GetCursorGraphic(MouseCursor::Type type);

	virtual void ClearWindow();
	virtual void ScrollWindow(int delta);

	virtual void ScaleImageDimensions(int& width, int& height);

private:
	void ClearHLine(int x, int y, int count);
	void HLineInternal(int x, int y, int count);
	void InvertLine(int x, int y, int count);

	int videoMode;
};

void FastMemSet(void far* mem, uint8_t value, unsigned int count);
#pragma aux FastMemSet = \
	"rep stosb" \
	modify [di cx] \	
	parm[es di][al][cx];

void DrawScrollBarBlock(uint8_t far* ptr, int top, int middle, int bottom);
#pragma aux DrawScrollBarBlock = \
	"cmp bx, 0" \
	"je _startMiddle" \	
	"mov ax, 0xfe7f" \
	"_loopTop:" \
	"stosw" \
	"add di, 78" \
	"dec bx"\
	"jnz _loopTop" \
	"_startMiddle:" \
	"mov ax, 0x0660" \
	"_loopMiddle:" \
	"stosw" \
	"add di, 78" \
	"dec cx"\
	"jnz _loopMiddle" \
	"mov ax, 0xfe7f" \
	"cmp dx, 0" \
	"je _end" \
	"_loopBottom:" \
	"stosw" \
	"add di, 78" \
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
	"add di, 78" \
	"dec bx"\
	"jnz _loopTop" \
	"_startMiddle:" \
	"mov ax, 0xf99f" \
	"_loopMiddle:" \
	"stosw" \
	"add di, 78" \
	"dec cx"\
	"jnz _loopMiddle" \
	"mov ax, 0x0180" \
	"cmp dx, 0" \
	"je _end" \
	"_loopBottom:" \
	"stosw" \
	"add di, 78" \
	"dec dx"\
	"jnz _loopBottom" \
	"_end:" \
	modify [di ax bx cx dx] \	
parm[es di][bx][cx][dx];

void ScrollRegionUp(int dest, int src, int count);
#pragma aux ScrollRegionUp = \
	"push ds" \
	"push es" \
	"mov ax, 0xb800" \
	"mov ds, ax" \
	"mov es, ax" \
	"_loopLine:" \
	"mov cx, 39" \
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
	"mov ax, 0xb800" \
	"mov ds, ax" \
	"mov es, ax" \
	"_loopLine:" \
	"mov cx, 39" \
	"rep movsw" \
	"sub di, 158" \
	"sub si, 158" \
	"dec dx" \
	"jnz _loopLine" \
	"pop es" \
	"pop ds" \
	modify [ax cx dx di si] \
	parm [di][si][dx]

void ClearRegion(int offset, int count, uint16_t clearMask);
#pragma aux ClearRegion = \
	"push es" \
	"mov ax, 0xb800" \
	"mov es, ax" \
	"mov ax, bx" \
	"_loopLine:" \
	"mov cx, 39" \
	"rep stosw" \
	"add di, 2" \
	"dec dx" \
	"jnz _loopLine" \
	"pop es" \
	modify [cx di ax cx dx] \
	parm [di] [dx] [bx]

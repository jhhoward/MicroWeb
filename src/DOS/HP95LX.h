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

class HP95LXVideoDriver : public VideoDriver
{
public:
	HP95LXVideoDriver();

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

private:
	int GetScreenMode();
	void SetScreenMode(int screenMode);

	void ClearHLine(int x, int y, int count);
	void HLineInternal(int x, int y, int count);
	void InvertLine(int x, int y, int count);
	bool ApplyScissor(int& y, int& height);

	bool invertScreen;
	uint16_t clearMask;
	int startingScreenMode;
	int scissorX1, scissorY1, scissorX2, scissorY2;

};


#pragma once

#include "../Platform.h"

class WindowsVideoDriver : public VideoDriver
{
public:
	WindowsVideoDriver();

	virtual void Init();
	virtual void Shutdown();

	virtual void ClearScreen();

	virtual void ClearWindow();
	virtual void ClearRect(int x, int y, int width, int height);
	virtual void ScrollWindow(int delta);
	virtual void SetScissorRegion(int y1, int y2);

	virtual void DrawString(const char* text, int x, int y, int size = 1, FontStyle::Type style = FontStyle::Regular);
	virtual void DrawScrollBar(int position, int size);
	virtual void DrawTitle(const char* text);
	virtual void DrawStatus(const char* text);

	virtual MouseCursorData* GetCursorGraphic(MouseCursor::Type type);

	virtual Font* GetFont(int fontSize);
	virtual int GetGlyphWidth(char c, int fontSize = 1, FontStyle::Type style = FontStyle::Regular);
	virtual int GetLineHeight(int fontSize = 1);

};
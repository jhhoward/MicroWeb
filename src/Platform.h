#pragma once

#include "Font.h"
#include "Cursor.h"
#include "Widget.h"

class VideoDriver
{
public:
	virtual void Init() = 0;
	virtual void Shutdown() = 0;

	virtual void ClearScreen() = 0;

	virtual void DrawString(const char* text, int x, int y, int size = 1, FontStyle::Type style = FontStyle::Regular) = 0;
	virtual void DrawScrollBar(int position, int size) = 0;
	virtual void DrawTitle(const char* text) = 0;
	virtual void DrawStatus(const char* text) = 0;

	virtual MouseCursorData* GetCursorGraphic(MouseCursor::Type type) = 0;

	virtual Font* GetFont(int fontSize) = 0;
	virtual int GetGlyphWidth(char c, int fontSize = 1, FontStyle::Type style = FontStyle::Regular) = 0;
	virtual int GetLineHeight(int fontSize = 1) = 0;

	int screenWidth;
	int screenHeight;

	int windowWidth;
	int windowHeight;
	int windowX, windowY;

	Widget scrollBar;
	Widget addressBar;
};

class NetworkDriver
{
public:
	virtual void Init() = 0;
	virtual void Shutdown() = 0;
};

class MouseDriver
{
public:
	virtual void Init() = 0;
	virtual void Shutdown() = 0;

	virtual void Hide() = 0;
	virtual void Show() = 0;
	virtual void SetCursor(MouseCursor::Type type) = 0;
	virtual void GetMouseState(int& buttons, int& x, int& y) = 0;
};

class Platform
{
public:
	static void Init();
	static void Shutdown();

	static VideoDriver* video;
	static NetworkDriver* network;
	static MouseDriver* mouse;
};

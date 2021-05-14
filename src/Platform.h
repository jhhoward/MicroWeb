#pragma once

#include <stdlib.h>
#include "Font.h"
#include "Cursor.h"
#include "Widget.h"

class VideoDriver
{
public:
	virtual void Init() = 0;
	virtual void Shutdown() = 0;

	virtual void ClearScreen() = 0;

	virtual void ClearWindow() = 0;
	virtual void ScrollWindow(int delta) = 0;
	virtual void SetScissorRegion(int y1, int y2) = 0;

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

class HTTPRequest
{
public:
	enum Status
	{
		Stopped,
		Connecting,
		Downloading,
		Finished,
		Error
	};

	virtual Status GetStatus() = 0;
	virtual size_t ReadData(char* buffer, size_t count) = 0;
	virtual void Stop() = 0;
};

class NetworkDriver
{
public:
	virtual void Init() = 0;
	virtual void Shutdown() = 0;
	virtual void Update() = 0;

	virtual bool IsConnected() = 0;

	virtual HTTPRequest* CreateRequest(char* url) = 0;
	virtual void DestroyRequest(HTTPRequest* request) = 0;
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

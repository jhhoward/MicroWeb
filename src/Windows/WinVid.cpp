#include <stdio.h>
#include "WinVid.h"
#include "../DOS/CGAData.inc"

#define WINDOW_TOP 24
#define WINDOW_HEIGHT 168
#define WINDOW_BOTTOM (WINDOW_TOP + WINDOW_HEIGHT)

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 200

#define ADDRESS_BAR_X 80
#define ADDRESS_BAR_Y 10
#define ADDRESS_BAR_WIDTH 480
#define ADDRESS_BAR_HEIGHT 12
#define TITLE_BAR_HEIGHT 8
#define STATUS_BAR_HEIGHT 8
#define STATUS_BAR_Y (SCREEN_HEIGHT - STATUS_BAR_HEIGHT)

#define SCROLL_BAR_WIDTH 16

WindowsVideoDriver::WindowsVideoDriver()
{
	screenWidth = 640;
	screenHeight = 200;
	windowWidth = screenWidth - 16;
	windowHeight = WINDOW_HEIGHT;
	windowX = 0;
	windowY = WINDOW_TOP;

	addressBar.x = ADDRESS_BAR_X;
	addressBar.y = ADDRESS_BAR_Y;
	addressBar.width = ADDRESS_BAR_WIDTH;
	addressBar.height = ADDRESS_BAR_HEIGHT;

	scrollBar.x = SCREEN_WIDTH - SCROLL_BAR_WIDTH;
	scrollBar.y = WINDOW_TOP;
	scrollBar.width = SCROLL_BAR_WIDTH;
	scrollBar.height = WINDOW_HEIGHT;

}

void WindowsVideoDriver::Init()
{

}

void WindowsVideoDriver::Shutdown()
{

}

void WindowsVideoDriver::ClearScreen()
{

}

void WindowsVideoDriver::ClearWindow()
{

}
void WindowsVideoDriver::ClearRect(int x, int y, int width, int height)
{

}
void WindowsVideoDriver::ScrollWindow(int delta)
{

}
void WindowsVideoDriver::SetScissorRegion(int y1, int y2)
{

}

void WindowsVideoDriver::DrawString(const char* text, int x, int y, int size, FontStyle::Type style)
{
	printf("%s\n", text);
}
void WindowsVideoDriver::DrawScrollBar(int position, int size)
{

}
void WindowsVideoDriver::DrawTitle(const char* text)
{

}
void WindowsVideoDriver::DrawStatus(const char* text)
{
}

MouseCursorData* WindowsVideoDriver::GetCursorGraphic(MouseCursor::Type type)
{
	return NULL;
}

Font* WindowsVideoDriver::GetFont(int fontSize)
{
	switch (fontSize)
	{
	case 0:
		return &CGA_SmallFont;
	case 2:
		return &CGA_LargeFont;
	default:
		return &CGA_RegularFont;
	}

}
int WindowsVideoDriver::GetGlyphWidth(char c, int fontSize, FontStyle::Type style)
{
	Font* font = GetFont(fontSize);
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
int WindowsVideoDriver::GetLineHeight(int fontSize)
{
	return GetFont(fontSize)->glyphHeight + 1;
}

#pragma once

#include "../Platform.h"
#include <Windows.h>

class WindowsVideoDriver : public VideoDriver
{
public:
	WindowsVideoDriver();

	virtual void Init();
	virtual void Shutdown();

	virtual void ArrangeAppInterfaceWidgets(class AppInterface& app);
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

	virtual void HLine(int x, int y, int count);
	virtual void VLine(int x, int y, int count);

	void Paint(HWND hwnd);

	int verticalScale;

private:
	void SetPixel(int x, int y, uint32_t colour);
	void InvertPixel(int x, int y, uint32_t colour);
	void FillRect(int x, int y, int width, int height, uint32_t colour);

	uint32_t* lpBitmapBits;
	HBITMAP screenBitmap;

	uint32_t foregroundColour, backgroundColour;
	int scissorX1, scissorY1, scissorX2, scissorY2;
};

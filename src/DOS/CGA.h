#pragma once

#include "../Platform.h"

class CGADriver : public VideoDriver
{
public:
	CGADriver();

	virtual void Init();
	virtual void Shutdown();
	virtual void ClearScreen();

	virtual void ArrangeAppInterfaceWidgets(class AppInterface& app);

	virtual void ClearWindow();
	virtual void ScrollWindow(int delta);
	virtual void SetScissorRegion(int y1, int y2);

	virtual void DrawString(const char* text, int x, int y, int size = 1, FontStyle::Type style = FontStyle::Regular);
	virtual void DrawTitle(const char* text);
	virtual void DrawStatus(const char* text);
	virtual void DrawRect(int x, int y, int width, int height);
	virtual void DrawButtonRect(int x, int y, int width, int height);
	virtual void ClearRect(int x, int y, int width, int height);

	virtual void HLine(int x, int y, int count);
	virtual void VLine(int x, int y, int count);

	virtual int GetGlyphWidth(char c, int fontSize = 1, FontStyle::Type style = FontStyle::Regular);
	virtual int GetLineHeight(int fontSize = 1);
	virtual Font* GetFont(int fontSize);

	virtual void DrawScrollBar(int position, int size);

	virtual MouseCursorData* GetCursorGraphic(MouseCursor::Type type);

private:
	int GetScreenMode();
	void SetScreenMode(int screenMode);

	void ClearHLine(int x, int y, int count);

	int startingScreenMode;
};
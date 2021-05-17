#pragma once

#include "Widget.h"
#include "LinAlloc.h"
#include "URL.h"

#define MAX_PAGE_WIDGETS 2000
#define MAX_PAGE_STYLE_STACK_SIZE 32
#define MAX_TEXT_BUFFER_SIZE 128

class App;

class Page
{
public:
	Page(App& inApp);

	void Reset();

	void AppendText(const char* text);
	void BreakLine(int padding = 0);
	void FlagLeadingWhiteSpace() { needLeadingWhiteSpace = true; }
	void SetTitle(const char* text);
	void FinishSection();

	void SetWidgetURL(const char* url);
	void ClearWidgetURL();

	Widget* GetWidget(int x, int y);

	WidgetStyle& GetStyleStackTop();
	void PushStyle(const WidgetStyle& style);
	void PopStyle();

	LinearAllocator allocator;

	int GetPageHeight() { return pageHeight; }

	URL pageURL;

private:
	friend class Renderer;

	Widget* CreateWidget();
	void FinishCurrentWidget();
	void FinishCurrentLine();

	App& app;

	char* title;

	int currentLineStartWidgetIndex;
	int currentWidgetIndex;
	int submittedWidgetIndex;

	int pageWidth, pageHeight;
	int cursorX, cursorY;
	int pendingVerticalPadding;

	int leftMarginPadding;

	bool needLeadingWhiteSpace;

	Widget widgets[MAX_PAGE_WIDGETS];
	int numWidgets;

	WidgetStyle styleStack[MAX_PAGE_STYLE_STACK_SIZE];
	uint8_t styleStackSize;

	char textBuffer[MAX_TEXT_BUFFER_SIZE];
	int textBufferSize;

	char* widgetURL;
};

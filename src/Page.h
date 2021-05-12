#pragma once

#include "Widget.h"

#define MAX_PAGE_WIDGETS 100
#define MAX_PAGE_STYLE_STACK_SIZE 32
#define MAX_TEXT_BUFFER_SIZE 128

class Page
{
public:
	Page();

	void AppendText(const char* text);
	void BreakLine(int padding = 0);
	void FlagLeadingWhiteSpace() { needLeadingWhiteSpace = true; }
	void SetTitle(const char* text);

	void Render();

	WidgetStyle& GetStyleStackTop();
	void PushStyle(const WidgetStyle& style);
	void PopStyle();

private:
	Widget* CreateWidget();
	void FinishCurrentWidget();
	void FinishCurrentLine();

	char* title;

	int currentLineStartWidgetIndex;
	int currentWidgetIndex;
	int submittedWidgetIndex;

	int pageWidth, pageHeight;
	int cursorX, cursorY;
	int pendingVerticalPadding;

	bool needLeadingWhiteSpace;

	Widget widgets[MAX_PAGE_WIDGETS];
	int numWidgets;

	WidgetStyle styleStack[MAX_PAGE_STYLE_STACK_SIZE];
	uint8_t styleStackSize;

	char textBuffer[MAX_TEXT_BUFFER_SIZE];
	int textBufferSize;
};

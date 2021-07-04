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

	void AddHorizontalRule();
	void AddButton(char* text);
	void AddTextField(char* text, int bufferLength, char* name);
	void AppendText(const char* text);
	void AddImage(char* altText, int width, int height);
	void BreakLine(int padding = 0);
	void BreakTextLine();
	void FlagLeadingWhiteSpace() { needLeadingWhiteSpace = true; }
	void SetTitle(const char* text);
	void FinishSection();
	void SetFormData(WidgetFormData* formData);

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
	friend class AppInterface;

	Widget* CreateWidget(Widget::Type type);
	Widget* CreateTextWidget();
	void FinishCurrentWidget();
	void FinishCurrentLine();

	App& app;

	char* title;

	int currentLineStartWidgetIndex;
	int currentWidgetIndex;
	int numFinishedWidgets;

	int pageWidth, pageHeight;
	int cursorX, cursorY;
	int pendingVerticalPadding;

	int leftMarginPadding;

	bool needLeadingWhiteSpace;

	Widget widgets[MAX_PAGE_WIDGETS];
	int numWidgets;

	WidgetStyle styleStack[MAX_PAGE_STYLE_STACK_SIZE];
	uint8_t styleStackSize;
	WidgetFormData* formData;

	char textBuffer[MAX_TEXT_BUFFER_SIZE];
	int textBufferSize;

	char* widgetURL;
};

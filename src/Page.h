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

#ifndef _PAGE_H_
#define _PAGE_H_

#include "Widget.h"
#include "LinAlloc.h"
#include "URL.h"
#include "Layout.h"

#define MAX_PAGE_WIDGETS 2000
#define MAX_PAGE_STYLE_STACK_SIZE 32
#define MAX_TEXT_BUFFER_SIZE 128

#define MAX_WIDGET_CHUNKS 256
#define WIDGETS_PER_CHUNK 64
#define WIDGET_INDEX_TO_CHUNK_INDEX(index) ((index) >> 6)
#define WIDGET_INDEX_TO_INDEX_IN_CHUNK(index) ((index) & 0x3f)

class App;
class Node;

class WidgetContainer
{
public:
	WidgetContainer(LinearAllocator& inAllocator);

	bool Allocate();
	void Clear();

	int Count() const { return numAllocated; }

	Widget& operator[](int index);

private:
	LinearAllocator& allocator;
	int numAllocated;

	struct Chunk
	{
		Widget widgets[WIDGETS_PER_CHUNK];
	};

	Chunk* chunks[MAX_WIDGET_CHUNKS];
};

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
	void AddBulletPoint();
	void BreakLine(int padding = 0);
	void BreakTextLine();
	void FlagLeadingWhiteSpace() { needLeadingWhiteSpace = true; }
	void SetTitle(const char* text);
	void FinishSection();
	void SetFormData(WidgetFormData* formData);
	void AdjustLeftMargin(int delta);

	void SetWidgetURL(const char* url);
	void ClearWidgetURL();

	Node* GetRootNode() { return rootNode; }

	Widget* GetWidget(int x, int y);

	WidgetStyle& GetStyleStackTop();
	void PushStyle(const WidgetStyle& style);
	void PopStyle();

	LinearAllocator allocator;
	Layout layout;

	int GetPageWidth();
	int GetPageHeight() { return pageHeight; }

	void DebugDraw(DrawContext& context, Node* node);
	void DebugDumpNodeGraph(Node* node, int depth = 0);

	URL pageURL;

private:
	friend class Renderer;
	friend class AppInterface;

	Widget* CreateWidget(Widget::Type type);
	Widget* CreateTextWidget();
	void FinishCurrentWidget();
	void FinishCurrentLine(bool includeCurrentWidget = true);

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

	Node* rootNode;

	WidgetContainer widgets;

	WidgetStyle styleStack[MAX_PAGE_STYLE_STACK_SIZE];
	uint8_t styleStackSize;
	WidgetFormData* formData;

	char textBuffer[MAX_TEXT_BUFFER_SIZE];
	int textBufferSize;

	char* widgetURL;
};

#endif

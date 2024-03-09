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

#include "URL.h"
#include "Layout.h"

#define MAX_PAGE_STYLE_STACK_SIZE 32
#define MAX_TEXT_BUFFER_SIZE 128

class App;
class Node;

class Page
{
public:
	Page(App& inApp);

	void Reset();

	void SetTitle(const char* text);

	Node* GetRootNode() { return rootNode; }

	Layout layout;

	int GetPageWidth();
	int GetPageHeight() { return pageHeight; }

	void DebugDumpNodeGraph(Node* node, int depth = 0);

	URL pageURL;

	App& GetApp() { return app; }

	Node* ProcessNextLoadTask(Node* lastNode, struct LoadTask& loadTask);

private:
	friend class AppInterface;

	App& app;

	char* title;

	int pageWidth, pageHeight;
	int cursorX, cursorY;
	int pendingVerticalPadding;

	int leftMarginPadding;

	Node* rootNode;

	char textBuffer[MAX_TEXT_BUFFER_SIZE];
	int textBufferSize;
};

#endif

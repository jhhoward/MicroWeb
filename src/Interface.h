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

#ifndef _INTERFACE_H_
#define _INTERFACE_H_
#include "Platform.h"
#include "URL.h"
#include "LinAlloc.h"
#include "Node.h"

#define MAX_TITLE_LENGTH 80

class App;
class Node;
struct DrawContext;

class AppInterface
{
public:
	AppInterface(App& inApp);

	void Init();
	void Reset();
	void Update();

	void DrawInterfaceNodes(DrawContext& context);
	void UpdateAddressBar(const URL& url);
	void SetStatusMessage(const char* message);

	void UpdatePageScrollBar();

	void SetTitle(const char* title);

	void FocusNode(Node* node);
	Node* GetFocusedNode() { return focusedNode; }
	Node* GetHoverNode() { return hoverNode; }
	Node* GetRootInterfaceNode() { return rootInterfaceNode; }
	bool IsInterfaceNode(Node* node);

	int GetScrollPositionY() { return scrollPositionY; }
	void ScrollRelative(int delta);
	void ScrollAbsolute(int position);


	URL addressBarURL;

	Rect windowRect;

private:
	void GenerateInterfaceNodes();

	Node* PickNode(int x, int y);

	void HandleClick(int mouseX, int mouseY);
	void HandleRelease();

	bool IsOverNode(Node* node, int x, int y);

	static void OnBackButtonPressed(Node* node);
	static void OnForwardButtonPressed(Node* node);
	static void OnAddressBarSubmit(Node* node);


	App& app;

	Node* focusedNode;
	Node* hoverNode;
	int oldButtons;
	int oldMouseX, oldMouseY;
	int oldPageHeight;

	LinearAllocator allocator;

	Node* rootInterfaceNode;
	Node* titleNode;
	Node* backButtonNode;
	Node* forwardButtonNode;
	Node* addressBarNode;
	Node* statusBarNode;
	Node* scrollBarNode;

	int scrollPositionY;

	char titleBuffer[MAX_TITLE_LENGTH];
};

#endif

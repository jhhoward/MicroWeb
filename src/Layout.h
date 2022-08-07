#pragma once

#include "Node.h"

#define MAX_LAYOUT_PARAMS_STACK_SIZE 32

class Page;
class Node;

struct LayoutParams
{
	int marginLeft, marginRight;
};

class Layout
{
public:

	Layout(Page& page);

	void Reset();

	Page& page;

	void BreakNewLine();

	void PushLayout();
	void PopLayout();

	void ProgressCursor(Node* nodeContext, int width, int lineHeight);

	Coord GetCursor(int lineHeight = 0) 
	{ 
		Coord result = cursor;
		result.y += currentLineHeight - lineHeight;
		return result; 
	}
	LayoutParams& GetParams() { return paramStack[paramStackSize]; }
	int AvailableWidth() { return GetParams().marginRight - cursor.x; }
	int MaxAvailableWidth() { return GetParams().marginRight - GetParams().marginLeft; }

	Node* lineStartNode;

	Coord cursor;
	int currentLineHeight;

	LayoutParams paramStack[MAX_LAYOUT_PARAMS_STACK_SIZE];
	int paramStackSize;

	void TranslateNodes(Node* node, int deltaX, int deltaY);
};

/*



*/

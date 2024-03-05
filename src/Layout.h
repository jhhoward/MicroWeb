#pragma once

#include "Node.h"

#define MAX_LAYOUT_PARAMS_STACK_SIZE 32
#define MAX_CURSOR_STACK_SIZE 16

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
	
	void PadHorizontal(int left, int right);
	void PadVertical(int down);
	void RestrictHorizontal(int maxWidth);

	void OnNodeEmitted(Node* node);
	void ProgressCursor(Node* nodeContext, int width, int lineHeight);

	void RecalculateLayout();
	void RecalculateLayoutForNode(Node* node);

	Coord GetCursor(int lineHeight = 0) 
	{ 
		Coord result = Cursor();
		result.y += currentLineHeight - lineHeight;
		return result; 
	}
	LayoutParams& GetParams() { return paramStack[paramStackSize]; }
	int AvailableWidth() { return GetParams().marginRight - Cursor().x; }
	int MaxAvailableWidth() { return GetParams().marginRight - GetParams().marginLeft; }

	Node* lineStartNode;
	Node* lastNodeContext;

	Coord& Cursor()
	{
		return cursorStack[cursorStackSize];
	}
	void PushCursor();
	void PopCursor();

	Coord cursorStack[MAX_CURSOR_STACK_SIZE];
	int cursorStackSize;

	int currentLineHeight;

	LayoutParams paramStack[MAX_LAYOUT_PARAMS_STACK_SIZE];
	int paramStackSize;

	void TranslateNodes(Node* start, Node* end, int deltaX, int deltaY);
};

/*



*/

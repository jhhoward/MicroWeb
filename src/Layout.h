#pragma once

#include "Node.h"
#include "Stack.h"

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

	void PushLayout() { paramStack.Push(); }
	void PopLayout() { paramStack.Pop(); }
	
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
	LayoutParams& GetParams() { return paramStack.Top(); }
	int AvailableWidth() { return GetParams().marginRight - Cursor().x; }
	int MaxAvailableWidth() { return GetParams().marginRight - GetParams().marginLeft; }

	Node* lineStartNode;
	Node* lastNodeContext;

	Coord& Cursor()
	{
		return cursorStack.Top();
	}
	void PushCursor() { cursorStack.Push(); }
	void PopCursor() { cursorStack.Pop(); }

	Stack<Coord> cursorStack;

	int currentLineHeight;
	int tableDepth;

	Stack<LayoutParams> paramStack;

	void TranslateNodes(Node* start, Node* end, int deltaX, int deltaY);
};

/*



*/

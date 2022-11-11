#include "Layout.h"
#include "Page.h"

Layout::Layout(Page& inPage)
	: page(inPage)
{
	Reset();
}

void Layout::Reset()
{
	cursor.Clear();
	paramStackSize = 0;
	currentLineHeight = 0;

	LayoutParams& params = GetParams();
	params.marginLeft = 0;
	params.marginRight = 600;
}

void Layout::PushLayout()
{
	if (paramStackSize < MAX_LAYOUT_PARAMS_STACK_SIZE - 1)
	{
		paramStack[paramStackSize + 1] = paramStack[paramStackSize];
		paramStackSize++;
	}
	else
	{
		// TODO error
	}
}

void Layout::PopLayout()
{
	if (paramStackSize > 0)
	{
		paramStackSize--;
	}
	else
	{
		// TODO error
	}
}

void Layout::BreakNewLine()
{
	// Recenter items if required
	if (lineStartNode && lineStartNode->style.alignment == ElementAlignment::Center)
	{
		int shift = AvailableWidth() / 2;
		TranslateNodes(lineStartNode, shift, 0);
	}

	cursor.x = GetParams().marginLeft;
	cursor.y += currentLineHeight;
	currentLineHeight = 0;
	lineStartNode = nullptr;
}

void Layout::ProgressCursor(Node* nodeContext, int width, int lineHeight)
{
	if (!lineStartNode)
	{
		lineStartNode = nodeContext;
	}

	if (lineHeight > currentLineHeight)
	{
		// Line height has increased so move everything down accordingly
		int deltaY = lineHeight - currentLineHeight;
		TranslateNodes(lineStartNode, 0, deltaY);
		currentLineHeight = lineHeight;
	}

	cursor.x += width;
}

void Layout::TranslateNodes(Node* node, int deltaX, int deltaY)
{
	while (node)
	{
		node->anchor.x += deltaX;
		node->anchor.y += deltaY;

		TranslateNodes(node->firstChild, deltaX, deltaY);

		node = node->next;
	}
}

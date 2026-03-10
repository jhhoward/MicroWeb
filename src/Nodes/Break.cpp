#include <stdio.h>
#include "../Layout.h"
#include "../Memory/LinAlloc.h"
#include "../Draw/Surface.h"
#include "../App.h"
#include "Break.h"

BreakNode::Data* BreakNode::Construct(Allocator& allocator, int breakPadding, bool displayBreakLine, bool onlyPadEmptyLines)
{
	return allocator.Alloc<BreakNode::Data>(breakPadding, displayBreakLine, onlyPadEmptyLines);
}

void BreakNode::Draw(DrawContext& context, Node* node)
{
	BreakNode::Data* data = static_cast<BreakNode::Data*>(node);

	if (data->displayBreakLine)
	{
		uint8_t outlineColour = App::Get().page.colourScheme.textColour;
		context.surface->HLine(context, node->anchor.x, node->anchor.y + node->size.y / 2, node->size.x, outlineColour);
	}
}


void BreakNode::GenerateLayout(Layout& layout, Node* node)
{
	BreakNode::Data* data = static_cast<BreakNode::Data*>(node);

	int lineHeight = layout.currentLineHeight;
	layout.BreakNewLine();

	int breakPadding = data->breakPadding;

	if (data->onlyPadEmptyLines && lineHeight != 0)
	{
		breakPadding = 0;
	}

	if (breakPadding)
	{
		layout.PadVertical(breakPadding);
	}

	if (data->displayBreakLine)
	{
		node->anchor = layout.GetCursor();
		node->anchor.x += 8;
		node->anchor.y -= breakPadding;
		node->size.x = layout.AvailableWidth() - 16;
		node->size.y = breakPadding;
		if (!breakPadding)
		{
			node->size.y = 1;
		}
	}
}

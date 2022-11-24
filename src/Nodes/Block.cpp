#include <stdio.h>
#include "../Layout.h"
#include "../LinAlloc.h"
#include "Block.h"

Node* BlockNode::Construct(Allocator& allocator, int horizontalPadding, int verticalPadding)
{
	BlockNode::Data* data = allocator.Alloc<BlockNode::Data>(horizontalPadding, verticalPadding);
	if (data)
	{
		return allocator.Alloc<Node>(Node::Block, data);
	}
	return nullptr;
}

void BlockNode::BeginLayoutContext(Layout& layout, Node* node)
{
	BlockNode::Data* data = static_cast<BlockNode::Data*>(node->data);

	layout.BreakNewLine();
	layout.PadVertical(data->verticalPadding);
	layout.PushLayout();
	layout.PadHorizontal(data->horizontalPadding, data->horizontalPadding);
}

void BlockNode::EndLayoutContext(Layout& layout, Node* node)
{
	NodeHandler::EndLayoutContext(layout, node);

	BlockNode::Data* data = static_cast<BlockNode::Data*>(node->data);

	layout.PopLayout();
	layout.BreakNewLine();
	layout.PadVertical(data->verticalPadding);
}

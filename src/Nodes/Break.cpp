#include <stdio.h>
#include "../Layout.h"
#include "../LinAlloc.h"
#include "Break.h"

Node* BreakNode::Construct(Allocator& allocator, int breakPadding, bool displayBreakLine)
{
	BreakNode::Data* data = allocator.Alloc<BreakNode::Data>(breakPadding, displayBreakLine);
	if (data)
	{
		return allocator.Alloc<Node>(Node::Break, data);
	}
	return nullptr;
}

void BreakNode::GenerateLayout(Layout& layout, Node* node)
{
	layout.BreakNewLine();
}

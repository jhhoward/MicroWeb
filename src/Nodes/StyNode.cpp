#include "StyNode.h"
#include "../LinAlloc.h"

Node* StyleNode::Construct(Allocator& allocator)
{
	StyleNode::Data* data = allocator.Alloc<StyleNode::Data>();
	if (data)
	{
		return allocator.Alloc<Node>(Node::Style, data);
	}
	return nullptr;
}

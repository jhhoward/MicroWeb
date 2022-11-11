#include "LinkNode.h"
#include "../LinAlloc.h"

void LinkNode::ApplyStyle(Node* node)
{
	node->style.fontStyle = (FontStyle::Type)(node->style.fontStyle | FontStyle::Underline);
}

Node* LinkNode::Construct(Allocator& allocator)
{
	LinkNode::Data* data = allocator.Alloc<LinkNode::Data>();
	if (data)
	{
		return allocator.Alloc<Node>(Node::Link, data);
	}
	return nullptr;
}

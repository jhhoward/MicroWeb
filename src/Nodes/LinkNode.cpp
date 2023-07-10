#include "LinkNode.h"
#include "../LinAlloc.h"
#include "../Event.h"
#include "../App.h"

void LinkNode::ApplyStyle(Node* node)
{
	node->style.fontStyle = (FontStyle::Type)(node->style.fontStyle | FontStyle::Underline);
}

Node* LinkNode::Construct(Allocator& allocator, char* url)
{
	LinkNode::Data* data = allocator.Alloc<LinkNode::Data>(url);
	if (data)
	{
		return allocator.Alloc<Node>(Node::Link, data);
	}
	return nullptr;
}

bool LinkNode::HandleEvent(Node* node, const Event& event)
{
	switch (event.type)
	{
		case Event::MouseRelease:
		{
			LinkNode::Data* data = static_cast<LinkNode::Data*>(node->data);
			if (data->url)
			{
				event.app.OpenURL(data->url);
			}
			return true;
		}
		default:
			break;
	}

	return false;
}

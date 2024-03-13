#include "LinkNode.h"
#include "../Memory/Memory.h"
#include "../Event.h"
#include "../App.h"

void LinkNode::ApplyStyle(Node* node)
{
	node->style.fontStyle = (FontStyle::Type)(node->style.fontStyle | FontStyle::Underline);
	node->style.fontColour = Platform::video->colourScheme.linkColour;
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
		case Event::Focus:
		{
			HighlightChildren(node);
		}
		return true;
		case Event::Unfocus:
		{
			HighlightChildren(node);
		}
		return true;
		case Event::MouseClick:
		{
			LinkNode::Data* data = static_cast<LinkNode::Data*>(node->data);
			if (data->url)
			{
				App::Get().OpenURL(URL::GenerateFromRelative(App::Get().page.pageURL.url, data->url).url);
			}
			return true;
		}
		default:
			break;
	}

	return false;
}

void LinkNode::HighlightChildren(Node* node)
{
	DrawContext context;
	App::Get().pageRenderer.GenerateDrawContext(context, node);

	Node* child = node->firstChild;
	if (!child)
		return;

	bool isDescendingTree = true;

	Platform::input->HideMouse();

	while (child != node)
	{
		if (isDescendingTree)
		{
			bool shouldHighlight = false;

			switch (child->type)
			{
			case Node::Text:
				shouldHighlight = child->firstChild == nullptr;
				break;
			case Node::Image:
			case Node::SubText:
				shouldHighlight = true;
				break;
			}

			if (shouldHighlight)
			{
				context.surface->InvertRect(context, child->anchor.x, child->anchor.y, child->size.x, child->size.y);
			}

			if (child->firstChild)
			{
				child = child->firstChild;
			}
			else if(child->next)
			{
				child = child->next;
			}
			else
			{
				isDescendingTree = false;
				child = child->parent;
			}
		}
		else
		{
			if (child->next)
			{
				child = child->next;
				isDescendingTree = true;
			}
			else
			{
				child = child->parent;
			}
		}
	}

	Platform::input->ShowMouse();

}

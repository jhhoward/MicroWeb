#include "LinkNode.h"
#include "../Memory/Memory.h"
#include "../Event.h"
#include "../App.h"
#include "../KeyCodes.h"

void LinkNode::ApplyStyle(Node* node)
{
	ElementStyle style = node->GetStyle();
	style.fontStyle = (FontStyle::Type)(style.fontStyle | FontStyle::Underline);
	style.fontColour = Platform::video->colourScheme.linkColour;
	node->SetStyle(style);
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
	LinkNode::Data* data = static_cast<LinkNode::Data*>(node->data);

	switch (event.type)
	{
		case Event::Focus:
		{
			HighlightChildren(node);
			if (data->url)
			{
				App::Get().ui.SetStatusMessage(URL::GenerateFromRelative(App::Get().page.pageURL.url, data->url).url, StatusBarNode::HoverStatus);
			}
		}
		return true;
		case Event::Unfocus:
		{
			HighlightChildren(node);
			if (data->url)
			{
				App::Get().ui.ClearStatusMessage(StatusBarNode::HoverStatus);
			}
		}
		return true;
		case Event::MouseClick:
		{
			if (data->url)
			{
				App::Get().OpenURL(URL::GenerateFromRelative(App::Get().page.pageURL.url, data->url).url);
			}
			return true;
		}
		case Event::KeyPress:
		{
			if (event.key == KEYCODE_ENTER)
			{
				if (data->url)
				{
					App::Get().OpenURL(URL::GenerateFromRelative(App::Get().page.pageURL.url, data->url).url);
				}
				return true;
			}
			return false;
		}
		default:
			break;
	}

	return false;
}

void LinkNode::HighlightChildren(Node* node)
{
	Node* child = node->firstChild;
	if (!child)
		return;

	bool isDescendingTree = true;

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
				App::Get().pageRenderer.InvertNode(child);
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

}

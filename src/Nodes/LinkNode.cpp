#include "LinkNode.h"
#include "../Memory/Memory.h"
#include "../Event.h"
#include "../App.h"
#include "../KeyCodes.h"
#include "ImgNode.h"

void LinkNode::ApplyStyle(Node* node)
{
	ElementStyle style = node->GetStyle();
	style.fontStyle = (FontStyle::Type)(style.fontStyle | FontStyle::Underline);
	style.fontColour = App::Get().page.colourScheme.linkColour;
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
				Node* leafNode = PickLeafChild(node, event.x, event.y);

				if (leafNode && leafNode->type == Node::Image && (static_cast<ImageNode::Data*>(leafNode->data))->isMap)
				{
					ImageNode::Data* imageData = static_cast<ImageNode::Data*>(leafNode->data);
					int x = event.x - leafNode->anchor.x;
					int y = event.y - leafNode->anchor.y;

					x = ((long)x * imageData->image.sourceWidth) / leafNode->size.x;
					y = ((long)y * imageData->image.sourceHeight) / leafNode->size.y;

					char* buffer = (char*) URL::GenerateFromRelative(App::Get().page.pageURL.url, data->url).url;
					size_t bufferSpace = MAX_URL_LENGTH - strlen(buffer) - 1;
					snprintf(buffer + strlen(buffer), bufferSpace, "?%d,%d", x, y);
					App::Get().OpenURL(HTTPRequest::Get, buffer);
				}
				else
				{
					App::Get().OpenURL(HTTPRequest::Get, URL::GenerateFromRelative(App::Get().page.pageURL.url, data->url).url);
				}
			}
			return true;
		}
		case Event::KeyPress:
		{
			if (event.key == KEYCODE_ENTER)
			{
				if (data->url)
				{
					App::Get().OpenURL(HTTPRequest::Get, URL::GenerateFromRelative(App::Get().page.pageURL.url, data->url).url);
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

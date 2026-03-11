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

LinkNode::Data* LinkNode::Construct(Allocator& allocator, char* url)
{
	MemBlockHandle urlHandle = MemoryManager::pageBlockAllocator.AllocString(url);
	return allocator.Alloc<LinkNode::Data>(urlHandle);
}

bool LinkNode::HandleEvent(Node* node, const Event& event)
{
	LinkNode::Data* data = static_cast<LinkNode::Data*>(node);

	switch (event.type)
	{
		case Event::Focus:
		{
			HighlightChildren(node);
			if (data->GetURL())
			{
				App::Get().ui.SetStatusMessage(URL::GenerateFromRelative(App::Get().page.pageURL.url, data->GetURL()).url, StatusBarNode::HoverStatus);
			}
		}
		return true;
		case Event::Unfocus:
		{
			HighlightChildren(node);
			if (data->GetURL())
			{
				App::Get().ui.ClearStatusMessage(StatusBarNode::HoverStatus);
			}
		}
		return true;
		case Event::MouseClick:
		{
			if (data->GetURL())
			{
				Node* leafNode = PickLeafChild(node, event.x, event.y);

				if (leafNode && leafNode->type == Node::Image && (static_cast<ImageNode::Data*>(leafNode))->isMap)
				{
					ImageNode::Data* imageData = static_cast<ImageNode::Data*>(leafNode);
					int x = event.x - leafNode->anchor.x;
					int y = event.y - leafNode->anchor.y;

					x = ((long)x * imageData->image.sourceWidth) / leafNode->size.x;
					y = ((long)y * imageData->image.sourceHeight) / leafNode->size.y;

					char* buffer = (char*) URL::GenerateFromRelative(App::Get().page.pageURL.url, data->GetURL()).url;
					size_t bufferSpace = MAX_URL_LENGTH - strlen(buffer) - 1;
					snprintf(buffer + strlen(buffer), bufferSpace, "?%d,%d", x, y);
					App::Get().OpenURL(HTTPRequest::Get, buffer);
				}
				else
				{
					App::Get().OpenURL(HTTPRequest::Get, URL::GenerateFromRelative(App::Get().page.pageURL.url, data->GetURL()).url);
				}
			}
			return true;
		}
		case Event::KeyPress:
		{
			if (event.key == KEYCODE_ENTER)
			{
				if (data->GetURL())
				{
					App::Get().OpenURL(HTTPRequest::Get, URL::GenerateFromRelative(App::Get().page.pageURL.url, data->GetURL()).url);
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
	Node* child = node->firstChild.Get();
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
				shouldHighlight = child->firstChild.Get() == nullptr;
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
				child = child->firstChild.Get();
			}
			else if(child->next)
			{
				child = child->next.Get();
			}
			else
			{
				isDescendingTree = false;
				child = child->parent.Get();
			}
		}
		else
		{
			if (child->next)
			{
				child = child->next.Get();
				isDescendingTree = true;
			}
			else
			{
				child = child->parent.Get();
			}
		}
	}

}

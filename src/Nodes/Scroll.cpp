#include "Scroll.h"
#include "../Memory/Memory.h"
#include "../DataPack.h"
#include "../App.h"
#include "../Interface.h"
#include "../Draw/Surface.h"

Node* ScrollBarNode::Construct(Allocator& allocator, int scrollPosition, int maxScroll, NodeCallbackFunction onScroll)
{
	ScrollBarNode::Data* data = allocator.Alloc<ScrollBarNode::Data>(scrollPosition, maxScroll, onScroll);
	if (data)
	{
		return allocator.Alloc<Node>(Node::ScrollBar, data);
	}

	return nullptr;
}

void ScrollBarNode::CalculateWidgetParams(Node* node, int& outPosition, int& outSize)
{
	ScrollBarNode::Data* data = static_cast<ScrollBarNode::Data*>(node->data);
	const int minWidgetSize = 15;
	const int maxWidgetSize = node->size.y;
	int scrollPosition = data->scrollPosition;

	if (node == App::Get().ui.GetFocusedNode())
	{
		scrollPosition = draggingScrollPosition;
	}

	if (data->maxScroll == 0)
	{
		outPosition = 0;
		outSize = maxWidgetSize;
	}
	else
	{
		outSize = (int)(((int32_t)node->size.y * node->size.y) / (data->maxScroll + node->size.y));
		if (outSize < minWidgetSize)
		{
			outSize = minWidgetSize;
		}
		if (outSize > maxWidgetSize)
		{
			outSize = maxWidgetSize;
		}
		int maxWidgetPosition = node->size.y - outSize;
		outPosition = (int32_t)maxWidgetPosition * scrollPosition / data->maxScroll;
		if (outPosition < 0)
			outPosition = 0;
		if (outPosition > maxWidgetPosition)
			outPosition = maxWidgetPosition;
	}
}

void ScrollBarNode::Draw(DrawContext& context, Node* node)
{
	int widgetPosition, widgetSize;
	CalculateWidgetParams(node, widgetPosition, widgetSize);
	context.surface->VerticalScrollBar(context, node->anchor.x, node->anchor.y, node->size.y, widgetPosition, widgetSize);
}

bool ScrollBarNode::HandleEvent(Node* node, const Event& event)
{
	AppInterface& ui = App::Get().ui;
	ScrollBarNode::Data* data = static_cast<ScrollBarNode::Data*>(node->data);
	int widgetPosition, widgetSize;
	CalculateWidgetParams(node, widgetPosition, widgetSize);
	int maxWidgetPosition = node->size.y - widgetSize;

	switch (event.type)
	{
		case Event::MouseClick:
		{
			ui.FocusNode(node);
			startDragOffset = (event.y - node->anchor.y) - widgetPosition;
			draggingScrollPosition = data->scrollPosition;
		}
		return true;
		case Event::MouseRelease:
		{
			ui.FocusNode(nullptr);

			data->scrollPosition = draggingScrollPosition;
			if (data->onScroll)
			{
				data->onScroll(node);
			}
		}
		return true;
		case Event::MouseDrag:
		{
			if (maxWidgetPosition > 0)
			{
				int dragPosition = (event.y - node->anchor.y) - startDragOffset;
				int newScrollPosition = ((long)dragPosition * data->maxScroll) / maxWidgetPosition;
				if (newScrollPosition < 0)
					newScrollPosition = 0;
				else if (newScrollPosition > data->maxScroll)
					newScrollPosition = data->maxScroll;

				if (newScrollPosition != draggingScrollPosition)
				{
					draggingScrollPosition = newScrollPosition;
					node->Redraw();
				}
			}
		}
		return true;
	}
	return false;
}


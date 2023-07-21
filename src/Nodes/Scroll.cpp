#include "Scroll.h"
#include "../LinAlloc.h"
#include "../DataPack.h"
#include "../App.h"
#include "../Interface.h"
#include "../Draw/Surface.h"

Node* ScrollBarNode::Construct(Allocator& allocator, int scrollPosition, int maxScroll)
{
	ScrollBarNode::Data* data = allocator.Alloc<ScrollBarNode::Data>(scrollPosition, maxScroll);
	if (data)
	{
		return allocator.Alloc<Node>(Node::ScrollBar, data);
	}

	return nullptr;
}

void ScrollBarNode::Draw(DrawContext& context, Node* node)
{
	ScrollBarNode::Data* data = static_cast<ScrollBarNode::Data*>(node->data);

	const int minWidgetSize = 15;
	const int maxWidgetSize = node->size.y;
	if (data->maxScroll == 0)
	{
		context.surface->VerticalScrollBar(context, node->anchor.x, node->anchor.y, node->size.y, 0, maxWidgetSize);
	}
	else
	{
		int widgetSize = (int)(((int32_t)node->size.y * node->size.y) / data->maxScroll);
		if (widgetSize < minWidgetSize)
		{
			widgetSize = minWidgetSize;
		}
		if (widgetSize > maxWidgetSize)
		{
			widgetSize = maxWidgetSize;
		}
		int maxWidgetPosition = node->size.y - widgetSize;
		int widgetPosition = (int32_t)maxWidgetPosition * data->scrollPosition / data->maxScroll;
		if (widgetPosition < 0)
			widgetPosition = 0;
		if (widgetPosition > maxWidgetPosition)
			widgetPosition = maxWidgetPosition;

		context.surface->VerticalScrollBar(context, node->anchor.x, node->anchor.y, node->size.y, widgetPosition, widgetSize);
	}

	
//	context.surface->FillRect(context, node->anchor.x, node->anchor.y, node->size.x, node->size.y, 0);

}



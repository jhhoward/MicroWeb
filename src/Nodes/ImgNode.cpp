#include "../Platform.h"
#include "../Page.h"
#include "ImgNode.h"
#include "../LinAlloc.h"
#include "../Layout.h"
#include "../Draw/Surface.h"

void ImageNode::Draw(DrawContext& context, Node* node)
{
	ImageNode::Data* data = static_cast<ImageNode::Data*>(node->data);
	//printf("--IMG [%d, %d]\n", node->anchor.x, node->anchor.y);
	uint8_t outlineColour = 0;

	context.surface->HLine(context, node->anchor.x, node->anchor.y, node->size.x, outlineColour);
	context.surface->HLine(context, node->anchor.x, node->anchor.y + node->size.y - 1, node->size.x, outlineColour);
	context.surface->VLine(context, node->anchor.x, node->anchor.y + 1, node->size.y - 2, outlineColour);
	context.surface->VLine(context, node->anchor.x + node->size.x - 1, node->anchor.y + 1, node->size.y - 2, outlineColour);
}


Node* ImageNode::Construct(Allocator& allocator, void* imageData)
{
	ImageNode::Data* data = allocator.Alloc<ImageNode::Data>(imageData);
	if (data)
	{
		return allocator.Alloc<Node>(Node::Image, data);
	}

	return nullptr;
}

void ImageNode::GenerateLayout(Layout& layout, Node* node)
{
	int imageWidth = node->size.x;
	int imageHeight = node->size.y;
	if (layout.AvailableWidth() < imageWidth)
	{
		layout.BreakNewLine();
	}

	node->anchor = layout.GetCursor(imageHeight);
	layout.ProgressCursor(node, imageWidth, imageHeight);
}
